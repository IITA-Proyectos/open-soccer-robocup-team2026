// publish_decision.h — Decisión PURA "alive_now" por slot para el ratchet de vida
// (read_stamp.h) del emisor del WorldSnapshot (FASE 2 de la pizarra TOP).
//
// Por qué existe (separar POLÍTICA de GLUE)
// ----------------------------------------------------------------------------
// snapshot_emitter_publish() (glue Arduino, gateado -DTOP_ENABLE_SNAPSHOT_TIMER)
// vuelca cada getter cacheado a su slot de la pizarra. FASE 1 estampaba con `now`
// plano: la frescura del slot medía la cadencia del LOOP, no la del SENSOR -> un
// sensor congelado-pero-vivo (BNO clavado, cámara muda que sigue devolviendo su
// último packet) se re-estampaba como fresco para siempre. FASE 2 cierra ese hueco
// con el RATCHET de read_stamp.h: el stamp avanza SOLO si el sensor dio señal de
// vida ESTE publish (alive_now=true). El que DECIDE alive_now es el caller.
//
// Este header aísla esa DECISIÓN en funciones PURAS (sin Arduino, sin millis, sin
// hardware): reciben los bool/valores que cada fuente YA expone y devuelven el
// alive_now correcto. Así la política "qué cuenta como señal de vida" queda
// host-testeable de forma exhaustiva, separada del glue que llama a los getters
// reales (sensors_imu_get_heading_valid, cameras_*_alive/_visible, pose.valid).
//
// REGLA CLAVE (no marcar vivo algo inválido): alive_now NO es "el sensor existe",
// es "el sensor entregó un dato CONFIABLE este publish". Si la fuente reporta su
// dato como inválido/no-visible, NO está vivo para efectos del ratchet -> su slot
// envejece -> sentinela. La validez intrínseca (visible/valid_src) y el ratchet
// son CAPAS COMPLEMENTARIAS (ver read_stamp.h): acá las COMBINAMOS para alive_now.
//
// PURO: solo <stdint.h>. Header-only. Compila con g++ host
// (bash scripts/run-host-tests.sh test_publish_decision).

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ----------------------------------------------------------------------------
// HEADING (BNO055). Vivo SOLO si la fusión del IMU reporta heading válido EN VIVO
// (sensors_imu_get_heading_valid()). El caso load-bearing: si get_heading_valid()
// == false (ningún BNO utilizable en runtime, o el freeze-detector lo marcó),
// NO refrescamos el slot -> tras heading_ms (250) el slot vence -> assemble limpia
// bit4 heading_valid. NO miramos el centideg (un 0 puede ser un heading real).
// ----------------------------------------------------------------------------
inline bool publish_alive_heading(bool heading_valid_src) {
    return heading_valid_src;
}

// ----------------------------------------------------------------------------
// PELOTA (cámaras front+back fusionadas). Dos condiciones, ambas necesarias:
//   - al menos UNA cámara reportó en el último timeout (cam_any_alive): si ambas
//     ópticas están mudas, el dato fusionado es rancio -> NO vivo.
//   - la fusión DICE que ve la pelota AHORA (ball_visible): "cámara fresca pero no
//     ve la pelota" es un estado VÁLIDO y distinto de "cámara colgada"; en ese
//     caso el slot ya colapsa por ball_visible=false en el assembler (sentinela
//     ball_visible=0). Para el ratchet, "no veo la pelota" NO cuenta como señal de
//     vida del dato-pelota: si quedara vivo, un último avistaje viejo podría
//     re-estamparse fresco. Exigimos ver la pelota AHORA Y con cámara viva.
// ----------------------------------------------------------------------------
inline bool publish_alive_ball(bool cam_any_alive, bool ball_visible) {
    return cam_any_alive && ball_visible;
}

// ----------------------------------------------------------------------------
// ARCO (rival o propio). Mismo criterio que la pelota: cámara viva Y arco visible
// AHORA. "cámara fresca pero no veo el arco" -> alive=false -> el slot envejece y
// colapsa al sentinela (goal_*_visible=0), consistente con la validez intrínseca.
// El caller pasa el visible del arco YA resuelto por polaridad (opp/own).
// ----------------------------------------------------------------------------
inline bool publish_alive_goal(bool cam_any_alive, bool goal_visible) {
    return cam_any_alive && goal_visible;
}

// ----------------------------------------------------------------------------
// POSE propia (trilateración / OTOS). Vivo SOLO si la fuente reporta la pose
// válida EN VIVO (LocalizationPose.valid). pose.valid=false (p.ej. <2 ToFs)
// -> NO vivo -> el slot envejece -> assemble pone my_pose_confidence=0. No
// miramos x/y (un 0 puede ser una posición real).
// ----------------------------------------------------------------------------
inline bool publish_alive_pose(bool pose_valid_src) {
    return pose_valid_src;
}

// ----------------------------------------------------------------------------
// OBSTÁCULO (min de 4 ToF + HC-SR04). CASO ESPECIAL — leer con cuidado.
//
// TOP NO expone una liveness de SUB-SISTEMA ToF que separe "campo libre" (ningún
// obstáculo en rango -> min==TOF_NO_READING) de "todos los ToF muertos" (cada
// getter expira a TOF_NO_READING tras TOF_STALE_TIMEOUT_MS). El único agregado es
// sensors_tof_get_min_distance_mm(), cuyo TOF_NO_READING es AMBIGUO entre esos dos.
//
// La liveness por-sensor del obstáculo YA VIVE EN EL VALOR, no en el tiempo: el
// propio getter de min-distancia ya colapsa a TOF_NO_READING (== WS_OBSTACLE_NONE,
// el sentinela del snapshot) cuando todos los ToF vencen su frescura interna de
// 250 ms. O sea: un ToF muerto YA produce "sin obstáculo" por el camino del valor,
// idéntico a un campo libre.
//
// Aplicar el ratchet ingenuo alive=(min!=TOF_NO_READING) marcaría un campo
// legítimamente LIBRE como alive=false (confunde "libre" con "muerto") sin ganar
// detección. Decisión honesta: el obstáculo es ALWAYS-ALIVE en el ratchet y se
// deja que (a) el valor TOF_NO_READING y (b) el umbral obstacle_ms del slot hagan
// el fail-safe — EXACTAMENTE como en FASE 1, sin regresión. Cuando TOP exponga un
// bool "algún ToF dio read bueno este ciclo", esta función se reemplaza por ese
// bool (la firma ya lo permite). Se deja como función (no un literal) para
// documentar la decisión en UN solo lugar y testearla.
// ----------------------------------------------------------------------------
inline bool publish_alive_obstacle(bool /*tof_subsystem_alive_reserved*/) {
    return true;   // ver comentario: la liveness real del obstáculo vive en el VALOR
}

}  // namespace iitasoccer
