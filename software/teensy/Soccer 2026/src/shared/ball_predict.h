// ball_predict.h — Predicción de la posición futura de la pelota (marco robot).
//
// Por qué existe
// -------------
// El arquero (strategy CENTRAL, estado GK_INTERCEPT) hoy persigue la posición
// ACTUAL de la pelota (ball_x). Con la velocidad de la pelota ya disponible en el
// WorldSnapshot (ball_vx/vy mm/s, marco robot — ball_velocity.{h,cpp}), podemos
// ANTICIPAR: proyectar dónde estará la pelota dentro de `lookahead_s` y apuntar
// ahí, en vez de ir siempre un paso atrás.
//
// Diseño (módulo PURO, host-testeable, sin Arduino)
// -------------------------------------------------
//   p = pos + clamp(v * lookahead_s, -max_lead_mm, +max_lead_mm)
//
// ⚠️ FALLBACK AUTOMÁTICO (clave del wiring): si vx=vy=0 — la pelota está quieta
// O la velocidad es N/A (el getter del world_model devuelve 0 en ese caso) — el
// lead es 0 → px=ball_x, py=ball_y = exactamente la conducta ACTUAL. Por eso el
// wiring en strategy NO necesita gating extra: con dato de velocidad anticipa,
// sin él se comporta igual que hoy.
//
// Convención de ejes: posición y velocidad vienen en marco robot
// (+y = frente, +x = derecha); la predicción sale en el mismo marco.

#pragma once
#include <stdint.h>

namespace iitasoccer {

struct BallPredictParams {
    float lookahead_s;   // horizonte de anticipación (s)
    float max_lead_mm;   // tope del adelanto por eje (mm), simétrico ±
};

struct BallPredictOut {
    float px_mm;   // posición X predicha (marco robot)
    float py_mm;   // posición Y predicha (marco robot)
};

// Parámetros por defecto recomendados: lookahead_s=0.2, max_lead_mm=400.
// ⚠️ A TUNEAR EN BANCO (cambia la conducta del arquero — anticipa).
BallPredictParams ball_predict_default_params();

// Predice la posición futura de la pelota en marco robot.
//   ball_x_mm,  ball_y_mm  : posición actual (de world_model).
//   ball_vx_mm_s, ball_vy_mm_s : velocidad (mm/s, marco robot); 0 si quieta/N/A.
// Con vx=vy=0 el lead es 0 → px=ball_x, py=ball_y (fallback automático).
BallPredictOut ball_predict(int16_t ball_x_mm, int16_t ball_y_mm,
                            int16_t ball_vx_mm_s, int16_t ball_vy_mm_s,
                            const BallPredictParams& p);

}  // namespace iitasoccer
