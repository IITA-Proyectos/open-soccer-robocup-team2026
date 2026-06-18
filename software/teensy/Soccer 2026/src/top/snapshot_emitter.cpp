// snapshot_emitter.cpp — implementacion del emisor @100 Hz (GATEADO -DTOP_ENABLE_SNAPSHOT_TIMER).
//
// TODO el cuerpo vive bajo el #if: con el flag OFF esta es una traduccion VACIA -> el
// binario de competencia es BYTE-IDENTICO (ni un static, ni un IntervalTimer, ni una
// llamada). Ver snapshot_emitter.h para el contrato + los invariantes load-bearing.

#include "snapshot_emitter.h"

#if defined(TOP_ENABLE_SNAPSHOT_TIMER)

#include <Arduino.h>

#include "snapshot_from_slots.h"   // SensorBlackboard, inputs_from_slots, obs types, SlotThresholds
#include "snapshot_assembler.h"    // assemble_snapshot (PURO, fail-safe por frescura)
#include "sensor_slot.h"           // slot_publish (single-writer seqlock)
#include "publish_decision.h"      // alive_now por slot (PURO) — FASE 2 read_stamp
#include "read_stamp.h"            // ReadStampState + read_stamp_tick (ratchet de vida por-sensor)
#include "config_top.h"
#include "sensors_imu.h"
#include "sensors_tof.h"
#include "cameras.h"
#include "cameras_runtime.h"
#include "comm_arbiter.h"
#include "comm_central.h"
#include "localization_runtime.h"
#include "goal_polarity.h"

namespace iitasoccer {
namespace {

// La PIZARRA. Solo existe con el flag ON -> .bss byte-neutro con el flag OFF.
SensorBlackboard  g_bb;
SlotThresholds    g_th;                 // defaults del header: obst/pose/heading 250ms, cam 1000ms
IntervalTimer     g_emit_timer;
GoalPolarityLatch g_goal_pol{};         // mirror del latch anti-rebote de build_snapshot

// Ratchet de vida POR-SENSOR (read_stamp.h). UNO por slot con frescura propia.
// Zero-init (.bss) == nunca vivo: en frío, read_stamp_tick fuerza stamp-vencido ->
// todos los slots arrancan STALE -> snapshot 100% sentinela hasta el 1er dato vivo.
// Solo existen con el flag ON (están bajo el #if) -> .bss byte-neutro en competencia.
// (El obstáculo NO lleva ratchet: su liveness vive en el VALOR; ver publish_decision.h.)
ReadStampState g_rs_heading{};
ReadStampState g_rs_ball{};
ReadStampState g_rs_goal_opp{};
ReadStampState g_rs_goal_own{};
ReadStampState g_rs_pose{};

volatile uint32_t g_frames  = 0;
volatile uint32_t g_wcet_us = 0;

// ISR del emisor @100 Hz. SOLO RAM (read_latest de cada slot) + assemble PURO + TX
// no-bloqueante. NO toca ningun bus, NO alimenta el WDT, NO hace Serial.print ni float.
void emit_isr() {
    const uint32_t t0 = micros();
    const SnapshotInputs in  = inputs_from_slots(g_bb, millis(), g_th);
    const WorldSnapshot  snap = assemble_snapshot(in);
    comm_central_send_snapshot(snap);   // no-bloqueante: dropea el frame si Serial4 esta lleno
    g_frames++;
    const uint32_t dt = micros() - t0;  // resta unsigned wrap-safe
    if (dt > g_wcet_us) g_wcet_us = dt;
}

}  // namespace

// ----------------------------------------------------------------------------
// snapshot_emitter_publish() — el LOOP vuelca cada getter cacheado a su slot.
// Espejo de los reads de build_snapshot(); SINGLE-WRITER (solo el loop publica).
//
// ⚠️ ALCANCE DE LA FRESCURA (honesto): aca se publica con `now`, asi que el timestamp
// del slot refleja la cadencia del LOOP, no la del SENSOR. Esto YA cubre el fail-safe
// mas grave —LOOP MUERTO—: si el loop se cuelga, deja de publicar -> todos los slots
// envejecen -> el emisor degrada TODO a sentinela -> CENTRAL frena (+ el WDT resetea).
// FASE 2 — CABLEADA (2026-06-16): la frescura POR-SENSOR ya esta conectada. Cada slot
// se estampa con read_stamp_tick() (publish_decision.h decide alive_now por sensor), asi
// que la muerte de UN sensor con el loop vivo (BNO clavado, camara muda) SI se detecta:
// su stamp se congela, el slot envejece y colapsa a su sentinela. El obstaculo es la
// unica excepcion (su liveness vive en el VALOR, no en el tiempo; ver publish_decision.h).
// Lo que aun cierra el banco: validar el comportamiento real (T3 fail-safe por slot) y
// afinar los umbrales por-sensor (SlotThresholds) contra las cadencias medidas.
// ----------------------------------------------------------------------------
void snapshot_emitter_publish() {
    const uint32_t now = millis();

    // FASE 2 — FRESCURA POR-SENSOR: cada slot se estampa con read_stamp_tick(), cuyo
    // timestamp SOLO avanza si el sensor dio señal de vida ESTE publish (publish_decision.h).
    // Un sensor congelado-pero-vivo (BNO clavado, cámara muda) deja de "estar vivo" -> su
    // stamp se congela -> el slot envejece a su ritmo -> tras su umbral colapsa a sentinela,
    // aunque el loop siga publicando. Cubre la muerte de UN sensor con el loop vivo (lo que
    // el `now` plano de Fase 1 NO cubría). El obstáculo NO lleva ratchet (su liveness vive
    // en el VALOR: sensors_tof_get_min_distance_mm ya colapsa a TOF_NO_READING).
    const bool cam_alive = cameras_any_alive();

    // POSE propia (trilateracion localization; el estimador gateado pose_fusion no se
    // duplica aca — esta es la pose cruda, igual que el camino #else de build_snapshot).
    const auto pose = localization_runtime_get_pose();
    PoseObs po{};
    po.x = pose.x_mm; po.y = pose.y_mm;
    po.valid_src = pose.valid; po.conf = pose.valid ? 70 : 0;
    slot_publish(g_bb.pose, po, read_stamp_tick(g_rs_pose, publish_alive_pose(pose.valid), now));

    // HEADING (SIEMPRE del IMU; el bit de validez lo decide assemble desde valid_src).
    HeadingObs ho{};
#ifdef TOP_ENABLE_HEADING_PREDICT
    // Predict step (GATED OFF): heading EXTRAPOLADO (crudo + ω·Δt, capado).
    ho.centideg  = sensors_imu_get_heading_centideg_predicted();
#else
    ho.centideg  = static_cast<int16_t>(sensors_imu_get_heading_centideg());
#endif
    ho.valid_src = sensors_imu_get_heading_valid();
    // Ratchet: mata el "heading congelado-pero-válido" — si el IMU deja de reportar
    // válido en vivo, el slot envejece y assemble limpia bit4 heading_valid.
    slot_publish(g_bb.heading, ho, read_stamp_tick(g_rs_heading, publish_alive_heading(ho.valid_src), now));

    // PELOTA (fusion front+back).
    BallObs bo{};
    bo.visible = cameras_ball_visible();
    bo.x  = cameras_get_ball_x_mm();      bo.y  = cameras_get_ball_y_mm();
    bo.conf = cameras_get_ball_confidence();
    bo.vx = cameras_get_ball_vx_mm_s();   bo.vy = cameras_get_ball_vy_mm_s();
    slot_publish(g_bb.ball, bo, read_stamp_tick(g_rs_ball, publish_alive_ball(cam_alive, bo.visible), now));

    // ARCOS con polaridad autodetectada (mirror de build_snapshot:228-254).
    const bool  yv = cameras_goal_yellow_visible();
    const bool  bv = cameras_goal_blue_visible();
    const float ya = cameras_get_goal_yellow_angle_centideg() / 100.0f;
    const float ba = cameras_get_goal_blue_angle_centideg() / 100.0f;
    goal_polarity_latch_update(g_goal_pol, goal_polarity_infer(yv, ya, bv, ba));
    const bool blue_is_opp =
        goal_polarity_effective(g_goal_pol, GoalPolarity::YELLOW_IS_OPP) == GoalPolarity::BLUE_IS_OPP;
    GoalObs opp{}, own{};
    if (blue_is_opp) {
        opp.visible = bv; opp.ang = cameras_get_goal_blue_angle_centideg();   opp.dist = cameras_get_goal_blue_distance_mm();
        own.visible = yv; own.ang = cameras_get_goal_yellow_angle_centideg(); own.dist = cameras_get_goal_yellow_distance_mm();
    } else {
        opp.visible = yv; opp.ang = cameras_get_goal_yellow_angle_centideg(); opp.dist = cameras_get_goal_yellow_distance_mm();
        own.visible = bv; own.ang = cameras_get_goal_blue_angle_centideg();   own.dist = cameras_get_goal_blue_distance_mm();
    }
    slot_publish(g_bb.goal_opp, opp, read_stamp_tick(g_rs_goal_opp, publish_alive_goal(cam_alive, opp.visible), now));
    slot_publish(g_bb.goal_own, own, read_stamp_tick(g_rs_goal_own, publish_alive_goal(cam_alive, own.visible), now));

    // OBSTACULO (min de ToFs + HC-SR04).
    ObstObs oo{}; oo.mm = sensors_tof_get_min_distance_mm();
    slot_publish(g_bb.obstacle, oo, now);

    // COMM (arbitro/partner): su frescura se resuelve aguas arriba en comm_arbiter ->
    // pasan directo (no llevan slot de frescura propio; ver SnapshotInputs).
    g_bb.referee_cmd         = static_cast<uint8_t>(comm_arbiter_get_last_command());
    g_bb.match_running       = comm_arbiter_is_match_running();
    g_bb.partner_alive       = comm_arbiter_partner_is_fresh();
    g_bb.partner_sees_ball   = false;   // futuro: parseo del partner snapshot
    g_bb.in_own_penalty_area = false;   // Nivel 2: requiere pose absoluta
}

void snapshot_emitter_init() {
    // IntervalTimer @100 Hz (10000 us). Prioridad por DEBAJO de los RX de UART (camaras
    // Serial3/5, OTOS Serial1) para no perder bytes: el emit puede esperar, el RX no.
    // (priority: 0=mas alta, 255=mas baja; default 128. BANCO ajusta el valor exacto.)
    g_emit_timer.begin(emit_isr, 10000);
    g_emit_timer.priority(200);
}

uint32_t snapshot_emitter_frames()      { return g_frames; }
uint32_t snapshot_emitter_isr_wcet_us() { return g_wcet_us; }

}  // namespace iitasoccer

#endif  // TOP_ENABLE_SNAPSHOT_TIMER
