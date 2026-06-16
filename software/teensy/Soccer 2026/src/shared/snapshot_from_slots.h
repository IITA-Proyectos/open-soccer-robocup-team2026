// snapshot_from_slots.h — LA COSTURA entre la PIZARRA (sensor_slot.h) y el
// ENSAMBLADOR (snapshot_assembler.h). Pieza PURA, host-testeable.
//
// Qué resuelve (el hueco que faltaba)
// -----------------------------------
// sensor_slot.h da la PIZARRA: cada sensor publica su último dato en un slot con
// doble-buffer + timestamp, y el lector se lo lleva ENTERO sin bloquear. El
// productor (lectura por interrupt/DMA) sólo sabe escribir su slot; no sabe nada
// del WorldSnapshot. snapshot_assembler.h da el ENSAMBLADOR: dado un SnapshotInputs
// (valores ya leídos + un bool *_fresh por slot), arma el WorldSnapshot aplicando
// el FAIL-SAFE por frescura (un slot viejo colapsa a su sentinela). Pero el
// ensamblador NO sabe de relojes ni de slots: recibe los *_fresh ya decididos.
//
// Faltaba quien LEA la pizarra y DECIDA la frescura: tomar cada SensorSlot<T>,
// sacarle el último valor con slot_read_latest(), preguntarle a slot_is_fresh()
// si sigue vivo contra `now` y el umbral de ESE slot, y volcar todo a un
// SnapshotInputs. Eso es este módulo. Es el ÚNICO lugar donde se cruzan la
// pizarra (que tiene timestamps) y el ensamblador (que sólo quiere bools).
//
// El flujo de un tick del loop del snapshot queda así:
//   uint32_t now = millis();                              // glue Arduino
//   SnapshotInputs in = inputs_from_slots(blackboard, now, thresholds);
//   WorldSnapshot snap = assemble_snapshot(in);           // snapshot_assembler.h
//   send_to_central(snap);                                // glue Arduino
//
// FAIL-SAFE (por qué importa el "ahora" único)
// --------------------------------------------
//   • `now` se lee UNA SOLA VEZ (parámetro) y se usa para EVALUAR LA FRESCURA DE
//     TODOS LOS SLOTS. Si cada slot se midiera contra su propio millis() leído por
//     separado, dos slots podrían quedar juzgados con relojes distintos (uno justo
//     antes y otro justo después de un tick), una incoherencia temporal. Un solo
//     `now` ⇒ una sola foto del tiempo ⇒ el snapshot es internamente consistente.
//   • Por cada slot: si slot_is_fresh(slot, now, umbral) == false, el *_fresh del
//     input queda en false → el ensamblador colapsa ESE campo a su sentinela
//     (pelota invisible, arco no-visible, heading sin bit válido, pose sin
//     confidence, obstáculo = NONE). Nunca se propaga dato muerto.
//   • Restas de tiempo UNSIGNED (las hace slot_is_fresh) → wrap-safe: el wrap de
//     millis() (~49,7 días) no dispara un "viejo" espurio.
//   • Slot NUNCA publicado (published=false) → slot_is_fresh devuelve false →
//     sentinela. Arranque en frío = todo fail-safe, sin datos basura.
//   • NO toca el bus, NO bloquea, NO duerme. Sólo lee la pizarra y arma POD.
//
// Campos COMM (árbitro / partner): NO son slots sensoriales con frescura propia.
// Su frescura se resuelve AGUAS ARRIBA (comm_arbiter_*). Acá pasan TAL CUAL desde
// el blackboard al input. No los gateamos por tiempo en este módulo.
//
// PURO: sólo <stdint.h> + sensor_slot.h + snapshot_assembler.h (que trae types.h).
// CERO Arduino (sin Wire/Serial/millis/digitalRead). El tiempo entra como uint32
// por parámetro. Compila con g++ host (bash scripts/run-host-tests.sh).
//
// ALCANCE: diseño + andamiaje, ADITIVO. NO se cablea a main_top.cpp todavía (la
// integración del doble-buffer por interrupts es post-Incheon, en banco). El glue
// (declarar el SensorBlackboard global, publicar desde las ISR, leer millis()) se
// agrega en la integración; la LÓGICA de composición es la que se testea acá.

#pragma once
#include <stdint.h>
#include "sensor_slot.h"
#include "snapshot_assembler.h"

namespace iitasoccer {

// ----------------------------------------------------------------------------
// Observaciones POD por slot — lo que el PRODUCTOR (lectura por interrupt/DMA)
// publica en cada SensorSlot<T>. Son structs triviales (números) para que el
// doble-buffer del slot las copie enteras sin torn-read.
//
// El campo de validez intrínseca (visible / valid_src) viaja DENTRO de la
// observación: "la cámara está fresca PERO no ve la pelota" es distinto de "la
// cámara se colgó". La frescura (¿el slot se actualizó a tiempo?) la decide ESTE
// módulo con slot_is_fresh(); la validez intrínseca la trae el productor.
// ----------------------------------------------------------------------------

// Avistaje de pelota (cámaras front+back ya fusionadas por el productor).
struct BallObs {
    int16_t x;            // mm, relativo al robot
    int16_t y;            // mm
    uint8_t conf;         // 0-100
    int16_t vx;           // mm/s (marco robot); 0 si N/A
    int16_t vy;           // mm/s
    bool    visible;      // ¿la cámara ve la pelota AHORA? (≠ frescura del slot)
};

// Avistaje de un arco (rival o propio).
struct GoalObs {
    int16_t ang;          // ángulo en centideg
    int16_t dist;         // distancia en mm
    bool    visible;      // ¿el arco está a la vista? (≠ frescura del slot)
};

// Rumbo (BNO055).
struct HeadingObs {
    int16_t centideg;     // -18000..+18000
    bool    valid_src;    // validez intrínseca del BNO (fused_valid en vivo);
                          // false = el BNO se congeló/cayó aunque el slot esté fresco
};

// Pose propia (OTOS / trilateración).
struct PoseObs {
    int16_t x;            // mm
    int16_t y;            // mm
    uint8_t conf;         // 0-100 (ya calculada por el productor)
    bool    valid_src;    // validez intrínseca de la fusión (p.ej. ≥2 ToFs)
};

// Distancia al obstáculo más cercano (min de ToFs + HC-SR04).
// El productor ya pone WS_OBSTACLE_NONE (0xFFFF) si no hay obstáculo; un slot
// VIEJO lo colapsa igual a NONE en el ensamblador (no pared fantasma).
struct ObstObs {
    uint16_t mm;          // distancia en mm, o WS_OBSTACLE_NONE = libre
};

// ----------------------------------------------------------------------------
// Umbrales de frescura por slot (ms). Defaults HEREDADOS de los timeouts ya
// titulados en el firmware vivo:
//   • obstacle (ToF)        = TOF_STALE_TIMEOUT_MS = 250 ms (sensors_tof.h)
//   • ball / goal (cámara)  = CAMERA_TIMEOUT_MS   = 1000 ms (cameras_runtime.cpp)
//   • heading (BNO @ ~20 Hz) = 250 ms  → ~5 muestras de margen (el BNO se lee a
//     20 Hz por el contention del bus Wire; 250 ms ⇒ un cuelgue real se detecta
//     rápido sin matar por un tick perdido)
//   • pose (OTOS)            = 250 ms  → la odometría debe ser reciente para
//     confiar en la posición; mismo orden que el obstáculo
//
// (Estas constantes se RE-DECLARAN acá como literales, NO se #incluyen los .h de
// top/ que arrastran Arduino — este header es PURO. Si el firmware vivo cambia un
// timeout, este default se actualiza a mano; es un valor de arranque, el caller
// puede pasar otros umbrales por banco.)
// ----------------------------------------------------------------------------
struct SlotThresholds {
    uint32_t ball_ms     = 1000;   // CAMERA_TIMEOUT_MS
    uint32_t goal_ms     = 1000;   // CAMERA_TIMEOUT_MS (rival y propio)
    uint32_t heading_ms  = 250;    // BNO @ ~20 Hz, ~5 muestras de margen
    uint32_t pose_ms     = 250;    // OTOS — odometría reciente
    uint32_t obstacle_ms = 250;    // TOF_STALE_TIMEOUT_MS
};

// ----------------------------------------------------------------------------
// SensorBlackboard — LA PIZARRA COMPLETA de la placa TOP: un slot por dato
// sensorial + los campos COMM (árbitro/partner) que llegan ya resueltos.
//
// Cada SensorSlot<T> lo publica su productor (lectura por interrupt/DMA). Este
// módulo SOLO lee. Zero-init (.bss) ⇒ todos los slots vacíos (published=false) ⇒
// inputs_from_slots() saca un SnapshotInputs todo-no-fresco ⇒ snapshot 100%
// sentinela. Arranque en frío seguro, sin glue.
// ----------------------------------------------------------------------------
struct SensorBlackboard {
    SensorSlot<BallObs>    ball;
    SensorSlot<GoalObs>    goal_opp;
    SensorSlot<GoalObs>    goal_own;
    SensorSlot<HeadingObs> heading;
    SensorSlot<PoseObs>    pose;
    SensorSlot<ObstObs>    obstacle;

    // --- COMM (árbitro/partner): frescura resuelta aguas arriba, pasan tal cual ---
    uint8_t referee_cmd        = 0;       // 0=stop,1=start,2=halftime,3=reset
    bool    match_running      = false;
    bool    partner_alive      = false;
    bool    partner_sees_ball  = false;
    bool    in_own_penalty_area= false;
};

// ----------------------------------------------------------------------------
// inputs_from_slots — lee la pizarra y arma el SnapshotInputs para el ensamblador.
//
// `now` se lee UNA vez (parámetro) y juzga la frescura de TODOS los slots, para
// consistencia temporal. Por cada slot: saca el valor con slot_read_latest_capped()
// (lector ACOTADO — esta función corre EN LA ISR del emisor, que no puede girar para
// siempre si un productor dejó seq impar) y decide *_fresh con slot_is_fresh(slot,
// now, umbral_de_ese_slot). Si el lector acotado se rinde (slot atascado) ⇒ *_fresh
// = false y el valor queda en su sentinela. Un slot viejo, nunca-publicado o atascado
// ⇒ *_fresh=false ⇒ el ensamblador lo colapsa a su sentinela. DEFAULT-TO-SAFE.
//
// Los campos COMM pasan tal cual del blackboard (frescura aguas arriba).
//
// NO bloquea, NO toca hardware, NO usa millis() (el caller lo pasa en `now`).
// ----------------------------------------------------------------------------
inline SnapshotInputs inputs_from_slots(const SensorBlackboard& bb,
                                        uint32_t now,
                                        const SlotThresholds& th) {
    SnapshotInputs in{};   // value-init: todo 0/false (base segura)

    // Reintentos máximos del lector ACOTADO. Esta función corre en la ISR del emisor
    // (snapshot_emitter), que NO puede girar para siempre: si un productor quedó a medio
    // publicar (seq IMPAR atascado por un torn-write o un cuelgue), slot_read_latest_capped
    // se RINDE tras unos intentos, deja `v` en su value-init (= sentinela) y marcamos
    // *_fresh=false → el ensamblador colapsa ese campo. DEFAULT-TO-SAFE: ante la duda,
    // sentinela; NUNCA un dato partido NI un cuelgue de la ISR (la trampa que el reader
    // for(;;) sin cota tenía). Pequeño a propósito: una publicación sana dura unas pocas
    // instrucciones → 1-2 reintentos bastan.
    constexpr uint32_t kMaxRetries = 2u;

    // --- PELOTA ---
    {
        BallObs v{};
        const bool ok = slot_read_latest_capped(bb.ball, v, kMaxRetries);
        in.ball_fresh      = ok && slot_is_fresh(bb.ball, now, th.ball_ms);
        in.ball_visible    = v.visible;
        in.ball_x_mm       = v.x;
        in.ball_y_mm       = v.y;
        in.ball_confidence = v.conf;
        in.ball_vx_mm_s    = v.vx;
        in.ball_vy_mm_s    = v.vy;
    }

    // --- ARCO RIVAL ---
    {
        GoalObs v{};
        const bool ok = slot_read_latest_capped(bb.goal_opp, v, kMaxRetries);
        in.goal_opp_fresh          = ok && slot_is_fresh(bb.goal_opp, now, th.goal_ms);
        in.goal_opp_visible        = v.visible;
        in.goal_opp_angle_centideg = v.ang;
        in.goal_opp_distance_mm    = v.dist;
    }

    // --- ARCO PROPIO ---
    {
        GoalObs v{};
        const bool ok = slot_read_latest_capped(bb.goal_own, v, kMaxRetries);
        in.goal_own_fresh          = ok && slot_is_fresh(bb.goal_own, now, th.goal_ms);
        in.goal_own_visible        = v.visible;
        in.goal_own_angle_centideg = v.ang;
        in.goal_own_distance_mm    = v.dist;
    }

    // --- RUMBO (heading) ---
    {
        HeadingObs v{};
        const bool ok = slot_read_latest_capped(bb.heading, v, kMaxRetries);
        in.heading_fresh     = ok && slot_is_fresh(bb.heading, now, th.heading_ms);
        in.heading_valid_src = v.valid_src;
        in.heading_centideg  = v.centideg;
    }

    // --- POSE ---
    {
        PoseObs v{};
        const bool ok = slot_read_latest_capped(bb.pose, v, kMaxRetries);
        in.pose_fresh     = ok && slot_is_fresh(bb.pose, now, th.pose_ms);
        in.pose_valid_src = v.valid_src;
        in.pose_x_mm      = v.x;
        in.pose_y_mm      = v.y;
        in.pose_confidence= v.conf;
    }

    // --- OBSTÁCULO ---
    {
        ObstObs v{};
        const bool ok = slot_read_latest_capped(bb.obstacle, v, kMaxRetries);
        in.obstacle_fresh  = ok && slot_is_fresh(bb.obstacle, now, th.obstacle_ms);
        in.min_obstacle_mm = v.mm;
    }

    // --- COMM (árbitro/partner): tal cual, frescura resuelta aguas arriba ---
    in.referee_cmd        = bb.referee_cmd;
    in.match_running      = bb.match_running;
    in.partner_alive      = bb.partner_alive;
    in.partner_sees_ball  = bb.partner_sees_ball;
    in.in_own_penalty_area= bb.in_own_penalty_area;

    return in;
}

}  // namespace iitasoccer
