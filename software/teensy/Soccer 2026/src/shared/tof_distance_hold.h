// tof_distance_hold.h — controlador P para ACERCARSE/MANTENER una distancia
// objetivo a una pared usando UNA lectura de ToF (mm).
//
// Backup de navegación para moverse SIN cámaras ni sensores de luz: con una
// sola distancia ToF basta para acercarse a la pared y mantener una separación.
//
// Módulo PURO host-testeable (solo <stdint.h>, SIN Arduino), vive en src/shared
// como pids.h / drive_straight.h. Sin estado: una sola función pura.
//
// ── Convención de signo ─────────────────────────────────────────────────────
//   v_mm_s > 0 = avanzar HACIA la pared (REDUCE la distancia medida).
//     error = cur_dist - target_dist
//       • cur > target (estoy lejos)  -> error > 0 -> v > 0 (avanzo, me acerco).
//       • cur < target (estoy cerca)  -> error < 0 -> v < 0 (retrocedo, me alejo).
//
// ── Ley de control ──────────────────────────────────────────────────────────
//   • !dist_valid && stop_if_invalid -> v=0, arrived=false, valid=false (seguro:
//     sin lectura confiable, el backup NO se mueve a ciegas).
//   • |error| <= deadband_mm         -> arrived=true,  v=0,  valid=true.
//   • si no                           -> v = clamp(-max, max, kp*error),
//                                         arrived=false, valid=true.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Ganancias/umbrales del lazo. Defaults pensados como punto de partida de banco.
struct TofHoldParams {
    float kp;               // mm/s de comando por mm de error de distancia.
    float max_speed_mm_s;   // saturación de la velocidad (ambos signos).
    float deadband_mm;      // |error| <= deadband -> se considera "llegó".
    bool  stop_if_invalid;  // sin lectura ToF: true -> frenar (backup seguro).
};

// Comando de salida.
struct TofHoldCmd {
    float v_mm_s;   // velocidad longitudinal (>0 = hacia la pared).
    bool  arrived;  // dentro del deadband de la distancia objetivo.
    bool  valid;    // el comando es confiable (hubo lectura usable).
};

// Defaults sanos: kp=2.0, max=400 mm/s, deadband=15 mm, stop_if_invalid=true.
TofHoldParams tof_hold_default_params();

// Computa el comando de mantenimiento de distancia (función PURA, sin estado).
//   cur_dist_mm:    distancia ToF actual a la pared (mm).
//   target_dist_mm: distancia deseada a la pared (mm).
//   dist_valid:     el ToF entregó una lectura usable.
TofHoldCmd tof_hold_update(float cur_dist_mm, float target_dist_mm,
                           bool dist_valid, const TofHoldParams& p);

}  // namespace iitasoccer
