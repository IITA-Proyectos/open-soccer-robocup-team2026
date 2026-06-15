// pose_age.h — Edad (ms) de un dato con sello de recepción. PURO, header-only.
//
// Por qué existe (INC-2 RT 2026-06-15)
// ------------------------------------
// El TOP recibe la pose OTOS de DOWN y guarda el millis() de la última recepción
// (g_pose_last_rx_ms en comm_down.cpp). El booleano comm_down_is_pose_fresh() ya
// existe, pero gatea contra un umbral GRUESO (DOWN_HEARTBEAT_TIMEOUT_MS=500 ms).
// La fusión de pose (pose_fusion) quiere gatear la PREDICCIÓN con granularidad FINA
// (PoseFusionConfig.otos_stale_ms=60 ms): a 100 Hz un OTOS sano llega cada ~10 ms,
// y predecir con un delta de 200 ms ya es basura. Para eso hace falta la EDAD, no
// solo fresco/stale.
//
// EL CONTRATO QUE IMPORTA (y el bug que previene): si NUNCA llegó un frame
// (ever_received=false, o last_rx_ms==0 como centinela), la edad es MÁXIMA
// (0xFFFFFFFF = "infinitamente viejo"), NO 0. Devolver 0 haría creer al gate que el
// dato es fresquísimo cuando en realidad nunca existió → la fusión integraría un
// delta contra un OTOS cero-inicializado. Mismo espíritu que el guard last_ms!=0 de
// comm_down.cpp::fresh() y world_model.cpp.
//
// WRAP-SAFE: la resta unsigned (now - last) da el intervalo real aunque millis()
// haya dado la vuelta (~49,7 días), mientras el intervalo real sea < 49,7 días
// (siempre cierto para una edad de pose de ms/segundos). Mismo patrón que
// state_timer.h::st_elapsed y sensor_slot.h::slot_age_ms.
//
// PURO: sin Arduino. El caller (glue) pasa millis() como now_ms; los tests pasan un
// reloj falso. Header-only para linkear sin .cpp.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Centinela de "infinitamente viejo / nunca recibido".
constexpr uint32_t POSE_AGE_NEVER = 0xFFFFFFFFu;

// Edad en ms del último dato. ever_received=false (o last_rx_ms==0) => POSE_AGE_NEVER.
inline uint32_t pose_age_ms_pure(uint32_t last_rx_ms, uint32_t now_ms, bool ever_received) {
    if (!ever_received || last_rx_ms == 0u) return POSE_AGE_NEVER;
    return now_ms - last_rx_ms;   // unsigned: wrap-safe
}

// ¿El dato es más nuevo que max_age_ms? Nunca-recibido => false (POSE_AGE_NEVER no es
// < ningún umbral razonable). Útil para gating fino sin exponer el centinela al caller.
// Corte ESTRICTO (<): edad == max_age_ms ya NO es fresco. Caso degenerado max_age_ms==0
// => nada es fresco nunca (ningún age es < 0) — intencionado, no es bug.
inline bool pose_age_is_fresh(uint32_t age_ms, uint32_t max_age_ms) {
    return age_ms < max_age_ms;
}

}  // namespace iitasoccer
