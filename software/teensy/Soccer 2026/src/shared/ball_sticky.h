// ball_sticky.h — Fusión de pelota "CÁMARA PEGAJOSA" (opcional práctica 2026-06-12).
//
// EL PROBLEMA QUE REEMPLAZA (hallazgo 2026-06-11, pregunta de Gustavo): la fusión
// clásica (fuse_ball_dual) PROMEDIA las posiciones cuando ambas cámaras reportan
// pelota. Pero la frontal y la trasera miran a lados OPUESTOS sin solape de FOV:
// una misma pelota física JAMÁS puede estar en ambas a la vez → ese caso es
// siempre un falso positivo en al menos una cámara, y el promedio inventa una
// pelota fantasma en el punto medio (pelota real adelante + buzo naranja atrás
// = "pelota" detrás del robot → el robot se da vuelta).
//
// LA REGLA NUEVA (idea de Gustavo: "acordarse de qué cámara estaba viendo la
// pelota y darle prioridad a ese lado"):
//   • Solo UNA cámara ve → esa manda (igual que siempre) y queda TITULAR.
//   • AMBAS ven (conflicto, físicamente imposible para una pelota):
//       - la TITULAR manda (memoria de qué lado veníamos viendo la pelota);
//       - sin titular (arranque en frío): la observación MÁS CERCANA al robot
//         (blob más cercano = más grande = más confiable; y es la táctica que
//         importa);
//       - la confianza BAJA a 60 (hay un objeto naranja espurio en escena) en
//         vez del 95 "consenso" viejo, que premiaba el caso imposible.
//   • Ninguna ve → invisible; la titularidad EXPIRA sola tras release_ms (una
//     pelota que reaparece después de mucho tiempo no hereda preferencia vieja).
//   • El TRASPASO legítimo (la pelota rodea al robot) es instantáneo: la titular
//     la pierde → el caso "solo una ve" corona a la otra al toque.
//
// LO QUE NO RESUELVE (documentado a propósito): si el falso naranja está en el
// campo visual de la cámara TITULAR y es más grande que la pelota real, gana
// igual — eso se decide DENTRO de la cámara (blob más grande, main.py) y solo se
// arregla con confianza por blob en el packet v3 (wire-breaking, P2).
//
// PURO (sin Arduino) → host-testeable: bash scripts/run-host-tests.sh test_ball_sticky
// Estado afuera (patrón ball_velocity): el caller guarda BallStickyState.
// Gateado en runtime por -DTOP_CAM_STICKY (env top_robot2_pri_sticky); sin el
// flag, el TOP sigue usando fuse_ball_dual byte-idéntico.

#pragma once
#include <stdint.h>

#include "cameras_fusion.h"   // CamObs / BallFused (mismos tipos que la fusión clásica)

namespace iitasoccer {

struct BallStickyParams {
    uint32_t release_ms;   // sin ver pelota este tiempo → soltar la titularidad
};

inline BallStickyParams ball_sticky_default_params() {
    // 700 ms ≈ 20 packets a 30 Hz: tolera parpadeos de detección sin soltar la
    // titularidad, pero una pelota perdida de verdad re-arranca de cero.
    return BallStickyParams{700};
}

// Quién "tiene" la pelota (la vio más recientemente estando elegida).
enum : uint8_t {
    BS_HOLDER_NONE  = 0,
    BS_HOLDER_FRONT = 1,
    BS_HOLDER_BACK  = 2,
};

struct BallStickyState {
    uint8_t  holder;         // BS_HOLDER_*
    uint32_t last_seen_ms;   // último instante con pelota elegida (0 = nunca)
};

inline void ball_sticky_reset(BallStickyState& s) {
    s.holder = BS_HOLDER_NONE;
    s.last_seen_ms = 0;
}

// Confianzas reportadas (0-100). La clásica daba 80 single / 95 "consenso";
// acá el conflicto REBAJA en vez de premiar: hay un naranja espurio en escena.
constexpr uint8_t BS_CONF_SINGLE   = 80;
constexpr uint8_t BS_CONF_CONFLICT = 60;

namespace ball_sticky_detail {
// Mismo truncamiento-con-saturación que usa cameras_fusion (SB-4): idéntico al
// cast crudo en el rango de hoy; solo satura los extremos.
inline int16_t clamp_i16_trunc(float v) {
    if (v >= 32767.0f)  return 32767;
    if (v <= -32768.0f) return -32768;
    return static_cast<int16_t>(v);
}
}  // namespace ball_sticky_detail

// Fusión pegajosa. Entradas idénticas a fuse_ball_dual + el reloj (now_ms) para
// la expiración de titularidad. front/back YA en marco robot (cam_obs_to_robot_frame).
inline BallFused ball_sticky_fuse(BallStickyState& s,
                                  const BallStickyParams& p,
                                  const CamObs& front,
                                  const CamObs& back,
                                  bool front_alive,
                                  bool back_alive,
                                  uint32_t now_ms) {
    const bool f_ok = front_alive && front.visible;
    const bool b_ok = back_alive  && back.visible;

    BallFused out{};
    const CamObs* chosen = nullptr;
    uint8_t conf = 0;

    if (f_ok && b_ok) {
        // CONFLICTO (una pelota no puede estar en ambas): titular manda;
        // sin titular, la más cercana al robot.
        if (s.holder == BS_HOLDER_FRONT) {
            chosen = &front;
        } else if (s.holder == BS_HOLDER_BACK) {
            chosen = &back;
        } else {
            const float df = front.x_mm * front.x_mm + front.y_mm * front.y_mm;
            const float db = back.x_mm  * back.x_mm  + back.y_mm  * back.y_mm;
            chosen = (db < df) ? &back : &front;
        }
        conf = BS_CONF_CONFLICT;
    } else if (f_ok) {
        chosen = &front;
        conf = BS_CONF_SINGLE;
    } else if (b_ok) {
        chosen = &back;
        conf = BS_CONF_SINGLE;
    }

    if (chosen != nullptr) {
        s.holder = (chosen->camera_id == 1) ? BS_HOLDER_BACK : BS_HOLDER_FRONT;
        s.last_seen_ms = (now_ms == 0) ? 1 : now_ms;   // 0 = "nunca" (sentinel)
        out.x_mm = ball_sticky_detail::clamp_i16_trunc(chosen->x_mm);
        out.y_mm = ball_sticky_detail::clamp_i16_trunc(chosen->y_mm);
        out.visible = true;
        out.confidence = conf;
    } else {
        // Nadie ve: la titularidad expira sola (no heredar preferencia vieja).
        if (s.holder != BS_HOLDER_NONE && s.last_seen_ms != 0 &&
            (now_ms - s.last_seen_ms) > p.release_ms) {
            s.holder = BS_HOLDER_NONE;
        }
        out.visible = false;
        out.confidence = 0;
        out.x_mm = 0;
        out.y_mm = 0;
    }
    return out;
}

}  // namespace iitasoccer
