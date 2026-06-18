// heading_predict.h — Predicción de rumbo por extrapolación lineal (predict step).
//
// POR QUÉ EXISTE (coach 2026-06-17, pedido Gustavo):
// En vez de transmitir a la CENTRAL el ÚLTIMO heading medido "quieto" (zero-order
// hold, que asume velocidad cero entre muestras), extrapolamos linealmente:
//
//     heading_estimado = heading_ancla + ω · Δt
//
// donde ω es la velocidad angular MEDIDA por el giroscopio (CCW+, °/s) y Δt es la
// edad del último heading fusionado. Es el PASO DE PREDICCIÓN de un estimador
// predict/correct (dead reckoning de 1er orden / filtro alpha-beta). Descuenta la
// LATENCIA/staleness entre muestras del BNO; NO crea información nueva (no reemplaza
// subir la tasa de sensado — ver research/2026-06-17-extrapolacion-predict-step.md).
//
// POR QUÉ es seguro acá y no en la pelota:
//   ω viene del GIROSCOPIO (sensor que mide la tasa DIRECTO) → derivada LIMPIA.
//   La velocidad de la pelota, en cambio, se DIFERENCIA de posiciones de cámara
//   ruidosas → extrapolar ESO amplifica ruido (por eso la pelota NO usa este módulo;
//   ya tiene ball_predict con su EMA). El heading es el mejor candidato del robot.
//
// LAS 3 REGLAS DURAS (sin ellas, extrapolar es PEOR que el hold):
//   (a) velocidad MEDIDA (ω del gyro) — la entrada, no diferenciada.
//   (b) ACOTAR Δt con max_extrap_ms (CAP) — el error crece con Δt; nunca extrapolar
//       lejos. CRÍTICO: el único guard de dato-muerto del firmware es
//       SNAPSHOT_TIMEOUT_MS=500 (world_model.cpp), DEMASIADO grueso para un
//       extrapolador (480 ms → posición absurda). Este cap propio (~50-80 ms) es
//       obligatorio, no opcional.
//   (c) RESET POR EVENTO — re-anclar al heading fusionado cada vez que cambia
//       (muestra absoluta fresca → borra la deriva); deadband cuando el robot está
//       quieto (no extrapolar el ruido del gyro).
//
// SINERGIA CON EL FREEZE DEL BNO: si el heading fusionado se CONGELA (deja de
// cambiar) pero el gyro sigue reportando giro, este módulo PUENTEA el rumbo con el
// gyro hasta el cap (band-aid acotado). Si además está TOP_ENABLE_BNO_FREEZE_DETECT,
// el detector baja heading_valid → on_sample resetea → degradamos a hold (el
// detector tiene precedencia). NO reemplaza el fix de fondo (BNO sin contención I²C).
//
// CONVENCIÓN: heading en CENTIGRADOS (centideg, int16), CCW-positivo, envuelto a
// [-18000, +18000] — igual que sensors_imu_get_heading_centideg() y el WorldSnapshot.
// ω en grados/s, CCW-positivo (igual que sensors_imu_get_gyro_z_dps()).
//
// PURO (sin Arduino) → host-testeable: bash scripts/run-host-tests.sh test_heading_predict

#pragma once
#include <stdint.h>

namespace iitasoccer {

struct HeadingPredictCfg {
    uint32_t max_extrap_ms;  // (b) CAP: nunca extrapolar más que esto hacia el futuro
    float    max_gyro_dps;   // clamp defensivo de ω (el robot físico no gira más rápido)
    float    deadband_dps;   // (c) por debajo de |ω| esto → NO extrapolar (robot quieto, ruido)
};

// Punto de partida (tunear en banco — ver TASK + research doc):
//   cap 60 ms (entre 50-80, muy por debajo de SNAPSHOT_TIMEOUT_MS=500);
//   clamp ±600°/s (igual que heading_rate.h);
//   deadband 2°/s (sobre el piso de ruido del gyro quieto, ~0,5-1°/s).
inline HeadingPredictCfg heading_predict_default_cfg() {
    return HeadingPredictCfg{60u, 600.0f, 2.0f};
}

struct HeadingPredictState {
    bool     primed;
    int16_t  anchor_cd;    // último heading fusionado (ancla absoluta, centideg)
    uint32_t anchor_ms;    // instante en que ese heading fue muestreado/cambió
    float    gyro_dps;     // última ω medida (CCW+, °/s)
};

inline void heading_predict_reset(HeadingPredictState& s) {
    s.primed    = false;
    s.anchor_cd = 0;
    s.anchor_ms = 0;
    s.gyro_dps  = 0.0f;
}

// Envuelve un valor en centideg a [-18000, +18000].
inline int16_t heading_predict_wrap_cd(long h) {
    while (h >  18000) h -= 36000;
    while (h < -18000) h += 36000;
    return static_cast<int16_t>(h);
}

// Ingesta de una MUESTRA (llamar en el tick del IMU, con el heading fusionado +
// la ω medida + la validez del heading + el reloj). Re-ancla cuando el heading
// fusionado cambia (muestra absoluta fresca = reset de deriva). Si el heading no
// es válido, des-ceba (no extrapolaremos sobre un valor muerto; el bit
// heading_valid del snapshot marca la invalidez aparte).
inline void heading_predict_on_sample(HeadingPredictState& s, int16_t heading_cd,
                                      float gyro_dps, bool heading_valid,
                                      uint32_t now_ms) {
    if (!heading_valid) {
        s.primed    = false;
        s.anchor_cd = heading_cd;
        s.gyro_dps  = 0.0f;
        return;
    }
    if (!s.primed) {
        s.primed    = true;
        s.anchor_cd = heading_cd;
        s.anchor_ms = now_ms;
        s.gyro_dps  = gyro_dps;
        return;
    }
    s.gyro_dps = gyro_dps;
    if (heading_cd != s.anchor_cd) {   // (c) muestra absoluta fresca → re-anclar
        s.anchor_cd = heading_cd;
        s.anchor_ms = now_ms;
    }
}

// Valor EXTRAPOLADO al instante now_ms (llamar en el sitio de transmisión del
// snapshot). Devuelve el heading estimado en centideg. Degrada a hold (el ancla)
// si: no cebado, robot quieto (deadband) o ω clampeada a 0.
inline int16_t heading_predict_value(const HeadingPredictState& s, uint32_t now_ms,
                                     const HeadingPredictCfg& cfg) {
    if (!s.primed) return s.anchor_cd;

    float w = s.gyro_dps;
    if (w >  cfg.max_gyro_dps) w =  cfg.max_gyro_dps;   // (a) clamp defensivo
    if (w < -cfg.max_gyro_dps) w = -cfg.max_gyro_dps;
    if (w < cfg.deadband_dps && w > -cfg.deadband_dps) return s.anchor_cd;  // quieto → hold

    uint32_t age = now_ms - s.anchor_ms;               // wrap-safe (unsigned)
    if (age > cfg.max_extrap_ms) age = cfg.max_extrap_ms;   // (b) CAP duro

    // Δ(centideg) = ω[°/s] · 100[cd/°] · age[s]
    const float delta_cd = w * 100.0f * (static_cast<float>(age) / 1000.0f);
    const long  rounded  = static_cast<long>(delta_cd >= 0.0f ? delta_cd + 0.5f
                                                              : delta_cd - 0.5f);
    return heading_predict_wrap_cd(static_cast<long>(s.anchor_cd) + rounded);
}

}  // namespace iitasoccer
