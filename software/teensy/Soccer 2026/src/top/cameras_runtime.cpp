#include "cameras_runtime.h"
#include "cameras.h"
#include "cameras_fusion.h"
#include "ball_velocity.h"
#include "config_top.h"

#include <Arduino.h>

namespace iitasoccer {

namespace {

// === Constantes ===

// Si una cámara no manda un packet completo en este lapso, la consideramos caída.
// Cota holgada: a 30 Hz un packet llega cada ~33 ms → 1 s = ~30 packets perdidos.
constexpr uint32_t CAMERA_TIMEOUT_MS = 1000;

// Conversión cruda → mm. El protocolo viejo OpenMV manda Xp ∈ [0..200] y Yp ∈
// [-100..100] (después del decode del offset). Sin calibración fina, asumimos
// "1 unidad ≈ 1 cm" — alcanza para que la FSM produzca ángulos correctos y
// distancias razonables al perfil de approach_velocity (close=50, far=500).
//
// TODO: calibrar contra cancha real con pelota a 30/50/80/100 cm desde el robot,
// reemplazar este valor por el factor medido. Apuntar al journal cuando se haga.
constexpr float CAMERA_UNIT_TO_MM = 10.0f;

// Cota máxima de bytes a drenar por tick para no monopolizar el loop. A 19200
// baud llegan ~2 bytes/ms — con 64 bytes/tick (10 ms) tenemos margen 3×.
constexpr int MAX_BYTES_PER_TICK = 64;

// === Estado ===

CameraParser g_parser_front;    // Serial3 (RX pin 15) → conector U8 (cámara frontal)
CameraParser g_parser_back;     // Serial5 (RX pin 21) → cámara trasera (soldada acá)
// ⚠️ SWAP 2026-05-31 (TASK-204): la trasera quedó SOLDADA en Serial5 (pin 21),
// confirmado en banco con diag_top_cameras. Se revierte la movida del 2026-05-29
// (que la había puesto en Serial7 asumiendo la placa sin armar). Serial7 (pines
// 28/29) pasó a ser el link → CENTRAL (ver comm_central.cpp).

uint32_t g_last_packet_ms_front = 0;
uint32_t g_last_packet_ms_back  = 0;

BallFused  g_ball{};
GoalFused  g_goal_yellow{};
GoalFused  g_goal_blue{};

// Estimador de velocidad de la pelota (módulo puro host-testeado).
BallVelocityState        g_ball_vel{};
const BallVelocityParams g_ball_vel_params = ball_velocity_default_params();

// === Helpers ===

inline bool camera_alive(uint32_t last_ms, uint32_t now_ms) {
    // Convención: last_ms == 0 → todavía no recibimos ningún packet.
    if (last_ms == 0) return false;
    return (now_ms - last_ms) <= CAMERA_TIMEOUT_MS;
}

void recompute_fused(uint32_t now_ms) {
    const bool front_alive = camera_alive(g_last_packet_ms_front, now_ms);
    const bool back_alive  = camera_alive(g_last_packet_ms_back,  now_ms);

    const CameraPacket& pf = g_parser_front.get_packet();
    const CameraPacket& pb = g_parser_back.get_packet();

    // Pelota
    const CamObs ball_f = cam_obs_to_robot_frame(
        pf.ball_x, pf.ball_y, pf.ball_visible, /*cam_id=*/0, CAMERA_UNIT_TO_MM);
    const CamObs ball_b = cam_obs_to_robot_frame(
        pb.ball_x, pb.ball_y, pb.ball_visible, /*cam_id=*/1, CAMERA_UNIT_TO_MM);
    g_ball = fuse_ball_dual(ball_f, ball_b, front_alive, back_alive);

    // Arco amarillo
    const CamObs yellow_f = cam_obs_to_robot_frame(
        pf.goal_yellow_x, pf.goal_yellow_y, pf.goal_yellow_visible, 0, CAMERA_UNIT_TO_MM);
    const CamObs yellow_b = cam_obs_to_robot_frame(
        pb.goal_yellow_x, pb.goal_yellow_y, pb.goal_yellow_visible, 1, CAMERA_UNIT_TO_MM);
    g_goal_yellow = fuse_goal_dual(yellow_f, yellow_b, front_alive, back_alive);

    // Arco azul
    const CamObs blue_f = cam_obs_to_robot_frame(
        pf.goal_blue_x, pf.goal_blue_y, pf.goal_blue_visible, 0, CAMERA_UNIT_TO_MM);
    const CamObs blue_b = cam_obs_to_robot_frame(
        pb.goal_blue_x, pb.goal_blue_y, pb.goal_blue_visible, 1, CAMERA_UNIT_TO_MM);
    g_goal_blue = fuse_goal_dual(blue_f, blue_b, front_alive, back_alive);
}

}  // namespace

// ============================================================================
// API pública
// ============================================================================

void cameras_init() {
    Serial3.begin(UART_CAMERA1_BAUD);
    Serial5.begin(UART_CAMERA2_BAUD);
    g_parser_front.reset();
    g_parser_back.reset();
    g_last_packet_ms_front = 0;
    g_last_packet_ms_back  = 0;
    g_ball = BallFused{};
    g_goal_yellow = GoalFused{};
    g_goal_blue = GoalFused{};
    ball_velocity_reset(g_ball_vel);
}

void cameras_tick() {
    const uint32_t now_ms = millis();

    // Drenar Serial3 (cámara frontal). Cota por iteración para no bloquear.
    int drained = 0;
    while (Serial3.available() && drained < MAX_BYTES_PER_TICK) {
        const uint8_t byte = static_cast<uint8_t>(Serial3.read());
        if (g_parser_front.feed(byte)) {
            g_last_packet_ms_front = (now_ms == 0) ? 1 : now_ms;
        }
        ++drained;
    }

    // Drenar Serial5 (cámara trasera, soldada en pin 21).
    drained = 0;
    while (Serial5.available() && drained < MAX_BYTES_PER_TICK) {
        const uint8_t byte = static_cast<uint8_t>(Serial5.read());
        if (g_parser_back.feed(byte)) {
            g_last_packet_ms_back = (now_ms == 0) ? 1 : now_ms;
        }
        ++drained;
    }

    // Recalcular el snapshot fusionado en cada tick — barato (~µs).
    recompute_fused(now_ms);

    // Derivar velocidad de la pelota. Le pasamos como `sample_ms` el timestamp
    // del packet más nuevo (cualquiera de las 2 cámaras): el estimador sólo
    // deriva cuando ese timestamp avanza, así que correr esto cada tick (~100 Hz)
    // no diluye la velocidad pese a que los datos llegan a ~30 Hz. Además le
    // pasamos `now_ms` (millis()) para que EXPIRE la velocidad por tiempo si los
    // packets se cortan aunque la cámara siga reportando la pelota visible.
    const uint32_t sample_ms = (g_last_packet_ms_front > g_last_packet_ms_back)
                                   ? g_last_packet_ms_front
                                   : g_last_packet_ms_back;
    ball_velocity_update(g_ball_vel, g_ball_vel_params,
                         g_ball.x_mm, g_ball.y_mm, g_ball.visible, sample_ms, now_ms);
}

// === Getters ===

bool    cameras_ball_visible()        { return g_ball.visible; }
int16_t cameras_get_ball_x_mm()       { return g_ball.x_mm; }
int16_t cameras_get_ball_y_mm()       { return g_ball.y_mm; }
uint8_t cameras_get_ball_confidence() { return g_ball.confidence; }
int16_t cameras_get_ball_vx_mm_s()    { return ball_velocity_vx_mm_s(g_ball_vel); }
int16_t cameras_get_ball_vy_mm_s()    { return ball_velocity_vy_mm_s(g_ball_vel); }

bool    cameras_goal_yellow_visible()              { return g_goal_yellow.visible; }
int16_t cameras_get_goal_yellow_angle_centideg()   { return g_goal_yellow.angle_centideg; }
int16_t cameras_get_goal_yellow_distance_mm()      { return g_goal_yellow.distance_mm; }

bool    cameras_goal_blue_visible()                { return g_goal_blue.visible; }
int16_t cameras_get_goal_blue_angle_centideg()     { return g_goal_blue.angle_centideg; }
int16_t cameras_get_goal_blue_distance_mm()        { return g_goal_blue.distance_mm; }

bool cameras_front_alive() {
    return camera_alive(g_last_packet_ms_front, millis());
}
bool cameras_back_alive() {
    return camera_alive(g_last_packet_ms_back, millis());
}
bool cameras_any_alive() {
    const uint32_t now_ms = millis();
    return camera_alive(g_last_packet_ms_front, now_ms)
        || camera_alive(g_last_packet_ms_back,  now_ms);
}
bool cameras_both_dead() { return !cameras_any_alive(); }

uint32_t cameras_packets_front() { return g_parser_front.packets_decoded(); }
uint32_t cameras_packets_back()  { return g_parser_back.packets_decoded(); }
uint32_t cameras_resyncs_total() {
    return g_parser_front.resync_events() + g_parser_back.resync_events();
}
// CRC errors AGREGADOS (front + back). El parser v2 (cameras.cpp) cuenta un
// crc_error por cada frame de 11 bytes que llegó completo pero con CRC8 malo
// (bit-flip en el enlace cámara→TOP). Es la métrica de integridad de enlace que
// pedía P0-CAM-CRC / TASK-015: si crece en banco, hay ruido/bit-flips en el cable.
// Solo LEE los getters del parser (cam_crc8 / crc_errors()); NO toca cameras.cpp/.h
// ni el contrato de wire. resync_events() (ya expuesto en cameras_resyncs_total)
// cuenta el otro modo de falla: pérdida de framing (header/END fuera de lugar).
uint32_t cameras_get_crc_errors_total() {
    return g_parser_front.crc_errors() + g_parser_back.crc_errors();
}
// Alias de nombre explícito para el agregado de resyncs (mismo valor que
// cameras_resyncs_total(), que ya consumen diag_top_all y main_top). Lo agrego
// para cerrar el par crc/resync pedido por TASK-015 sin renombrar el getter viejo.
uint32_t cameras_resync_total() { return cameras_resyncs_total(); }

}  // namespace iitasoccer
