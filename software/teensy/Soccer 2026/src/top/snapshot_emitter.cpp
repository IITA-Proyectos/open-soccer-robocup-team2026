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
// PERO NO cubre todavia la muerte de UN sensor con el loop vivo (el getter sigue
// devolviendo su ultimo valor cacheado, que aca se re-publica como fresco). Esa
// frescura POR-SENSOR es FASE 2: cada read real del sensor (sensors_tof_tick,
// sensors_imu_tick, cameras_tick) debe llamar slot_publish CON SU read-timestamp
// (o el publish debe usar el age que el sensor ya expone, p.ej. comm_down_pose_age_ms).
// Los modulos de frescura (snapshot_from_slots/freshness_policy/assembler) ya estan
// listos y host-testeados; falta SOLO cablear el read-time por sensor (banco).
// ----------------------------------------------------------------------------
void snapshot_emitter_publish() {
    const uint32_t now = millis();

    // POSE propia (trilateracion localization; el estimador gateado pose_fusion no se
    // duplica aca — esta es la pose cruda, igual que el camino #else de build_snapshot).
    const auto pose = localization_runtime_get_pose();
    PoseObs po{};
    po.x = pose.x_mm; po.y = pose.y_mm;
    po.valid_src = pose.valid; po.conf = pose.valid ? 70 : 0;
    slot_publish(g_bb.pose, po, now);

    // HEADING (SIEMPRE del IMU; el bit de validez lo decide assemble desde valid_src).
    HeadingObs ho{};
    ho.centideg  = static_cast<int16_t>(sensors_imu_get_heading_centideg());
    ho.valid_src = sensors_imu_get_heading_valid();
    slot_publish(g_bb.heading, ho, now);

    // PELOTA (fusion front+back).
    BallObs bo{};
    bo.visible = cameras_ball_visible();
    bo.x  = cameras_get_ball_x_mm();      bo.y  = cameras_get_ball_y_mm();
    bo.conf = cameras_get_ball_confidence();
    bo.vx = cameras_get_ball_vx_mm_s();   bo.vy = cameras_get_ball_vy_mm_s();
    slot_publish(g_bb.ball, bo, now);

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
    slot_publish(g_bb.goal_opp, opp, now);
    slot_publish(g_bb.goal_own, own, now);

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
