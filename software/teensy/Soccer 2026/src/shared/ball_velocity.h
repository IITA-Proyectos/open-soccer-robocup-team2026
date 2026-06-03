// ball_velocity.h — Estimador de velocidad de la pelota (mm/s, marco robot).
//
// Por qué existe
// -------------
// El WorldSnapshot v2 ya tiene `ball_vx_mm_s` / `ball_vy_mm_s` y la cadena
// `bt_classify` (ball_trajectory.{h,cpp}, 7 tests verdes) los consume para
// decidir si la pelota va al arco propio/rival. Antes `build_snapshot()` en el
// TOP los dejaba en 0 (siempre BT_STILL). Este módulo deriva la velocidad de la
// posición fusionada y la mete en el snapshot, dejándola lista para esa cadena.
// ⚠️ Falta el último eslabón: CENTRAL aún NO consume ball_vx/vy (no hay getter
// en world_model ni llamada a bt_classify en strategy) — pendiente de cablear.
// Ver journal/2026-05-31-analisis-vision-n6-deteccion-protocolo.md §4.
//
// Diseño (MVP, host-testeable, sin Arduino)
// -----------------------------------------
//   • Estima por diferencias finitas sobre la posición fusionada (cameras_fusion).
//   • Sólo deriva al llegar una muestra NUEVA: el caller pasa `sample_ms` = el
//     timestamp del packet más nuevo que produjo la muestra. El loop corre a
//     ~100 Hz pero los datos de cámara llegan a ~30 Hz; si `sample_ms` no avanzó
//     no hay dato nuevo → no se recalcula (evita "diluir" la velocidad a 0).
//   • Necesita 2 muestras: la primera sólo siembra el punto de referencia
//     (valid=false, v=0).
//   • Resetea al perder visibilidad → al reaparecer descarta el primer frame
//     (no clava un pico por el salto de posición).
//   • Re-siembra si el gap entre muestras supera `max_gap_ms` (un stall dentro
//     del watchdog de cámara no debe generar una velocidad falsa enorme).
//   • Suaviza derivadas sucesivas con un EMA (`ema_alpha`).
//
// Convención de ejes: la posición fusionada viene en marco robot (+y = frente,
// +x = derecha), así que la velocidad sale en el mismo marco.

#pragma once
#include <stdint.h>

namespace iitasoccer {

struct BallVelocityParams {
    float    ema_alpha;     // peso de la muestra nueva en el EMA, 0..1
    uint32_t max_gap_ms;    // dt mayor a esto → re-siembra (no deriva)
};

struct BallVelocityState {
    bool     has_point;     // tenemos un punto de referencia
    bool     valid;         // vx/vy son una estimación usable (≥2 muestras seguidas)
    float    last_x_mm;
    float    last_y_mm;
    uint32_t last_ms;       // timestamp de la muestra de referencia
    float    vx_mm_s;       // velocidad suavizada (interna, float)
    float    vy_mm_s;
};

// Parámetros por defecto recomendados (alpha ~0.4, gap 200 ms).
BallVelocityParams ball_velocity_default_params();

// Deja el estado en cero: sin punto, sin velocidad válida.
void ball_velocity_reset(BallVelocityState& s);

// Alimenta una nueva muestra de posición fusionada de la pelota.
//   x_mm,y_mm : posición de la pelota en marco robot (de cameras_fusion).
//   visible   : la pelota está visible este frame (fusión).
//   sample_ms : timestamp del packet más nuevo que produjo esta muestra.
void ball_velocity_update(BallVelocityState& s,
                          const BallVelocityParams& p,
                          int16_t x_mm, int16_t y_mm,
                          bool visible, uint32_t sample_ms);

// Velocidad lista para el WorldSnapshot: 0 si no es válida, clampeada a int16.
int16_t ball_velocity_vx_mm_s(const BallVelocityState& s);
int16_t ball_velocity_vy_mm_s(const BallVelocityState& s);

}  // namespace iitasoccer
