// mix_comm.cpp — capa de comunicación de centralmix: TOP/DOWN → variables planas.
//
// ÚNICO punto que toca Serial. Replica EXACTAMENTE el patrón de comm_top.cpp y
// comm_down.cpp (Serial7=TOP, Serial1=DOWN, 230400, framing proto.h con CRC,
// FrameDecoder byte a byte), pero en vez de empujar a world_model POBLA g_io
// (mix_io.h) con variables planas estilo 2025. NO usa world_model.
//
// Fuentes verificadas (no de memoria):
//   - Serial/baud/IDs/framing: src/central/comm_top.cpp y comm_down.cpp.
//   - Decodificación de payloads: src/shared/proto.h, types.h, line_view.h, pose_view.h.
//   - BNO055 (lectura cruda, Wire @ 0x28, VECTOR_EULER.x()): src/diag/diag_bno_tof.cpp.
//   - Mapeo a g_io y convención de ángulo: mix_io.h / mix_comm.h (contrato).
//
// HEADING (tres modos excluyentes; ver mix_config.h):
//   - DEFAULT (BNO LOCAL): mix_comm inicializa un BNO055 en Wire @ MIX_BNO055_I2C_ADDR
//     (en la propia CENTRAL), captura heading_inicial en init y lee el yaw en cada tick.
//     ⚠️ R1 2026 NO tiene BNO local (los 2 BNO viven en el TOP) → este modo NO sirve
//     para R1: el begin() del BNO local falla y read_bno_heading() deja heading_valid=false.
//   - Con -DMIX_HEADING_SNAPSHOT: heading = BNO del TOP que viene en el WorldSnapshot
//     (my_heading_centideg + flag heading_valid bit4). NO se inicializa ni se lee ningún
//     BNO local; heading_inicial se sella con el PRIMER heading válido del snapshot.
//     ESTE es el modo correcto para R1 2026 (BNO del TOP arreglado + validado en banco
//     2026-06-21; ver src/top/sensors_imu.cpp:279 y journal 2026-06-21-bno-heading-fix).
//   - Con -DMIX_HEADING_OTOS: heading_deg = otos_heading_deg (del Pose2D de DOWN);
//     heading_inicial se captura del PRIMER Pose2D recibido. NO se toca el BNO.
//   En los tres casos otos_heading_deg queda SIEMPRE disponible (crudo OTOS) para A-B.
//
// heading_error_deg lo CALCULA mix_comm (= wrap180(heading_deg - heading_inicial)),
// como pide el contrato de mix_io.h (== 'error' del control de rumbo 2025). Se hace
// acá y no en la FSM porque heading_inicial pertenece al dominio de "fuente de
// heading" (BNO vs OTOS) que ya vive en esta capa; así la FSM consume el error listo.
//
// COLOR DEL ARCO RIVAL (goal_opp → yellow/blue):
//   El WorldSnapshot trae goal_opp (rival) y goal_own (propio) por ROL, no por color.
//   Hay que mapearlos a goal_yellow_* / goal_blue_*. Convención del repo
//   (src/central/strategy.h:41 "ARCO_CONTRINCANTE hardcoded a amarillo"): rival =
//   AMARILLO por default. Acá se respeta y se deja GATEADO con -DMIX_ATTACK_BLUE
//   (default OFF) para poder invertir la polaridad de campo sin tocar código.
//     - default            : goal_opp → yellow,  goal_own → blue
//     - con MIX_ATTACK_BLUE : goal_opp → blue,    goal_own → yellow
//
// ⚠️ NO TESTEADO EN HARDWARE.

#include "mix_comm.h"

#include "mix_io.h"
#include "mix_config.h"

#include "proto.h"        // FrameDecoder, Frame, MsgType, PROTO_*
#include "types.h"        // WorldSnapshot, LineStatusV2, Pose2D, Velocity2D
#include "line_view.h"    // lsv2_from_frame + interpretación pura de línea
#include "pose_view.h"    // pose_from_frame / vel_from_frame

#include <Arduino.h>
#include <string.h>       // memcpy
#include <math.h>         // atan2f, fmodf

// --- Resolución del modo de heading (EXACTAMENTE uno de tres) -----------------
//   -DMIX_HEADING_OTOS      → heading del OTOS (Pose2D de DOWN).
//   -DMIX_HEADING_SNAPSHOT  → heading del BNO del TOP vía WorldSnapshot (Serial7),
//                             SIN BNO local en la CENTRAL.
//   (ninguno de los dos)    → BNO LOCAL en Wire@0x28 (comportamiento 2025).
// Sólo en el modo por defecto se compila/inicializa/lee el BNO local.
#if defined(MIX_HEADING_OTOS) && defined(MIX_HEADING_SNAPSHOT)
#error "centralmix: MIX_HEADING_OTOS y MIX_HEADING_SNAPSHOT son mutuamente excluyentes (elegí UNA fuente de heading)"
#endif
#if !defined(MIX_HEADING_OTOS) && !defined(MIX_HEADING_SNAPSHOT)
#define MIX_HEADING_LOCAL_BNO 1
#endif

#ifdef MIX_HEADING_LOCAL_BNO
#include <Wire.h>
#include <Adafruit_BNO055.h>
#endif

namespace iitasoccer {
namespace mix {

// La instancia global única que define el contrato (extern en mix_io.h).
MixIO g_io;

namespace {

// ---- Enlaces (mismo patrón EXACTO que comm_top.cpp / comm_down.cpp) ----
// TOP  = Serial7 (RX7=28, TX7=29) — WORLD_SNAPSHOT 0x60.
// DOWN = Serial1 (RX1=0,  TX1=1)  — LINE_URGENT 0x10 + DOWN_OTOS_POSE 0x11 + _VEL 0x12.
FrameDecoder g_top_decoder;
FrameDecoder g_down_decoder;

// Ring RX por defecto de HardwareSerial = 64 B. El TOP manda ~37 B a 100 Hz y el
// DOWN es el tráfico más denso (línea 200 Hz + pose/vel 100 Hz). comm_top/comm_down
// amplían el ring con addMemoryForRead() (API Teensy, no host-testeable). Replico el
// mismo colchón de 512 B en cada enlace. Estáticos: deben sobrevivir todo el runtime.
#if defined(__IMXRT1062__) || defined(CORE_TEENSY)
uint8_t s_top_rx_buf[512];
uint8_t s_down_rx_buf[512];
#endif

// ---- Frescura de enlace (rx-watchdog liviano) ----
// Mismo umbral que el world_model del CENTRAL (world_model.cpp: *_TIMEOUT_MS = 500).
constexpr unsigned long MIX_LINK_TIMEOUT_MS = 500;

// ---- Heading inicial (se sella una sola vez; base del heading_error_deg) ----
float g_heading_inicial = 0.0f;
bool  g_heading_inicial_set = false;

#ifdef MIX_HEADING_LOCAL_BNO
// BNO055 LOCAL en Wire @ 0x28 (PRIMARIO del CENTRAL en 2025; igual que diag_bno_tof.cpp).
// IMUPLUS = giroscopio relativo sin fusión magnética (mismo modo que los diag).
Adafruit_BNO055 g_bno(55, MIX_BNO055_I2C_ADDR, &Wire);
bool g_bno_ok = false;
#endif

// Normaliza un ángulo a [-180, 180]. Equivale al wrap del 'error' 2025.
inline float wrap180(float deg) {
    deg = fmodf(deg, 360.0f);
    if (deg > 180.0f)  deg -= 360.0f;
    if (deg < -180.0f) deg += 360.0f;
    return deg;
}

// Sella heading_inicial la primera vez que tenemos un heading de la fuente activa.
// (BNO: en init tras leer el primer yaw. OTOS: con el primer Pose2D recibido.)
inline void seal_heading_inicial_if_needed(float heading_now) {
    if (!g_heading_inicial_set) {
        g_heading_inicial = heading_now;
        g_heading_inicial_set = true;
    }
}

// ============================================================
// TOP — WorldSnapshot 0x60 → pelota / arcos / heading(BNO) / referee.
// Patrón de comm_top.cpp::handle_frame(): validar tipo + tamaño exacto antes de
// memcpy (un snapshot de otro schema se descarta en vez de reinterpretarse).
// ============================================================
void apply_top_snapshot(const WorldSnapshot& s) {
    const unsigned long now = millis();

    // --- Pelota (marco robot: +X der, +Y adel; cm = dato CRUDO de la cámara) ---
    // La cámara manda cm; el TOP los escala x10 a mm en el snapshot (CAMERA_UNIT_TO_MM=10).
    // Acá DESHACEMOS ese x10 (÷10) para volver al cm crudo de la cámara (pedido de Elías).
    g_io.ball_x_cm   = static_cast<float>(s.ball_x_mm) / 10.0f;
    g_io.ball_y_cm   = static_cast<float>(s.ball_y_mm) / 10.0f;
    g_io.ball_visible = (s.ball_visible != 0);

    // angulo_pelota_deg = atan2(X, Y) (OJO: X primero) → 0=adelante, >0=derecha,
    // <0=izquierda. (El ángulo NO depende de la escala: cm o mm dan el mismo ángulo.)
    g_io.angulo_pelota_deg =
        atan2f(g_io.ball_x_cm, g_io.ball_y_cm) * 180.0f / static_cast<float>(M_PI);

    // Sella el timestamp de "última vez que se vio la pelota" (== millis_pelota 2025).
    if (g_io.ball_visible) {
        g_io.t_last_ball_seen_ms = now;
    }

    // --- Arcos: goal_opp (rival) / goal_own (propio) → yellow/blue por polaridad ---
    // Sentinela del schema: visible=0 ⇒ angle/dist NO válidos (no leerlos).
    const bool opp_vis = (s.goal_opp_visible != 0);
    const bool own_vis = (s.goal_own_visible != 0);
    const float opp_ang  = s.goal_opp_angle_centideg / 100.0f;
    const float opp_dist = static_cast<float>(s.goal_opp_distance_mm);
    const float own_ang  = s.goal_own_angle_centideg / 100.0f;
    const float own_dist = static_cast<float>(s.goal_own_distance_mm);

#ifdef MIX_ATTACK_BLUE
    // Rival = AZUL.
    g_io.goal_blue_visible   = opp_vis;
    g_io.goal_blue_angle     = opp_vis ? opp_ang  : 0.0f;
    g_io.goal_blue_dist      = opp_vis ? opp_dist : 0.0f;
    g_io.goal_yellow_visible = own_vis;
    g_io.goal_yellow_angle   = own_vis ? own_ang  : 0.0f;
    g_io.goal_yellow_dist    = own_vis ? own_dist : 0.0f;
#else
    // DEFAULT del repo: rival = AMARILLO (strategy.h:41).
    g_io.goal_yellow_visible = opp_vis;
    g_io.goal_yellow_angle   = opp_vis ? opp_ang  : 0.0f;
    g_io.goal_yellow_dist    = opp_vis ? opp_dist : 0.0f;
    g_io.goal_blue_visible   = own_vis;
    g_io.goal_blue_angle     = own_vis ? own_ang  : 0.0f;
    g_io.goal_blue_dist      = own_vis ? own_dist : 0.0f;
#endif

    // --- Heading desde el snapshot (BNO del TOP) — activo salvo en modo OTOS ---
    // El snapshot trae el heading fusionado del TOP + el flag de validez (bit4 de flags).
    //   · modo SNAPSHOT: ESTA es la ÚNICA fuente de heading (no hay BNO local en R1).
    //   · modo BNO LOCAL (default 2025): es el respaldo; el read del BNO local en
    //     mix_comm_tick() lo sobreescribe si ese chip local responde.
#ifndef MIX_HEADING_OTOS
    const bool snap_heading_valid = (s.flags & 0x10) != 0;  // bit4 = heading_valid
    const float snap_heading = s.my_heading_centideg / 100.0f;
    if (snap_heading_valid) {
        g_io.heading_deg   = snap_heading;
        g_io.heading_valid = true;
        seal_heading_inicial_if_needed(snap_heading);
        g_io.heading_error_deg = wrap180(g_io.heading_deg - g_heading_inicial);
    }
    // Si el snapshot no trae heading válido, NO se pisa heading_deg: el read directo
    // del BNO en mix_comm_tick() es el respaldo (mantiene el último válido).
#endif

    // --- Árbitro / partido: flags bit3 = match_running ---
    g_io.match_running = (s.flags & 0x08) != 0;

    g_io.t_last_top_frame_ms = now;
}

void handle_top_frame(const Frame& f) {
    if (f.type != MsgType::WORLD_SNAPSHOT) return;
    if (f.payload_len != sizeof(WorldSnapshot)) return;  // schema desfasado → descartar
    WorldSnapshot snap{};
    memcpy(&snap, f.payload, sizeof(WorldSnapshot));
    apply_top_snapshot(snap);
}

// ============================================================
// DOWN — LineStatusV2 0x10 (+ OTOS Pose2D 0x11 / Velocity2D 0x12).
// Patrón de comm_down.cpp::handle_frame(): probar cada decoder puro en orden.
// ============================================================
void apply_down_line(const LineStatusV2& ls) {
    // Helpers PUROS de line_view.h (honran la compuerta data_valid del contrato).
    g_io.line_present   = lsv2_line_present(ls);
    g_io.line_angle_deg = lsv2_line_angle_deg(ls);   // 0 = frente
    g_io.line_depth     = lsv2_sensors_on_line(ls);  // 0..32
    g_io.t_last_down_frame_ms = millis();
}

void apply_down_pose(const Pose2D& pose) {
    // otos_heading_deg = SIEMPRE disponible (crudo OTOS), para diagnóstico / A-B.
    g_io.otos_heading_deg = pose.heading_centideg / 100.0f;

#ifdef MIX_HEADING_OTOS
    // Fuente activa = OTOS: heading_deg = crudo OTOS; validez por confidence.
    g_io.heading_deg   = g_io.otos_heading_deg;
    g_io.heading_valid = (pose.confidence > 0);
    if (g_io.heading_valid) {
        seal_heading_inicial_if_needed(g_io.heading_deg);
        g_io.heading_error_deg = wrap180(g_io.heading_deg - g_heading_inicial);
    }
#endif

    g_io.t_last_down_frame_ms = millis();
}

void apply_down_vel(const Velocity2D& /*vel*/) {
    // centralmix (port FSM 2025) no usa velocidad OTOS hoy. Se decodifica para
    // mantener la frescura del enlace y por si la FSM la consume más adelante.
    g_io.t_last_down_frame_ms = millis();
}

void handle_down_frame(const Frame& f) {
    LineStatusV2 ls{};
    if (lsv2_from_frame(f, ls)) { apply_down_line(ls); return; }
    Pose2D pose{};
    if (pose_from_frame(f, pose)) { apply_down_pose(pose); return; }
    Velocity2D vel{};
    if (vel_from_frame(f, vel)) { apply_down_vel(vel); return; }
}

// Actualiza top_link_fresh / down_link_fresh (rx-watchdog). Resta unsigned =
// wrap-safe; el guard ms>0 evita marcar fresh antes del primer frame.
void update_link_freshness() {
    const unsigned long now = millis();
    g_io.top_link_fresh =
        (g_io.t_last_top_frame_ms > 0) &&
        ((now - g_io.t_last_top_frame_ms) < MIX_LINK_TIMEOUT_MS);
    g_io.down_link_fresh =
        (g_io.t_last_down_frame_ms > 0) &&
        ((now - g_io.t_last_down_frame_ms) < MIX_LINK_TIMEOUT_MS);
}

#ifdef MIX_HEADING_LOCAL_BNO
// Lee el BNO LOCAL crudo en cada tick (modo BNO local). Es el respaldo directo si el
// snapshot del TOP no trae heading válido. yaw del Euler = heading (igual diag).
void read_bno_heading() {
    if (!g_bno_ok) { g_io.heading_valid = false; return; }
    const float yaw = g_bno.getVector(Adafruit_BNO055::VECTOR_EULER).x();
    g_io.heading_deg   = yaw;
    g_io.heading_valid = true;
    seal_heading_inicial_if_needed(yaw);
    g_io.heading_error_deg = wrap180(g_io.heading_deg - g_heading_inicial);
}
#endif

}  // namespace

// ============================================================
// API pública.
// ============================================================
void mix_comm_init() {
    // --- UARTs (replica EXACTA de comm_top_init / comm_down_init) ---
    Serial7.begin(MIX_UART_BAUD);  // TOP
    Serial1.begin(MIX_UART_BAUD);  // DOWN
#if defined(__IMXRT1062__) || defined(CORE_TEENSY)
    Serial7.addMemoryForRead(s_top_rx_buf,  sizeof(s_top_rx_buf));
    Serial1.addMemoryForRead(s_down_rx_buf, sizeof(s_down_rx_buf));
#endif

    g_top_decoder.reset();
    g_down_decoder.reset();

#ifdef MIX_HEADING_LOCAL_BNO
    // --- BNO055 LOCAL (fuente de heading sólo en el modo por defecto) ---
    Wire.begin();
    Wire.setClock(400000);
    uint8_t tr = 0;
    while (!(g_bno_ok = g_bno.begin(OPERATION_MODE_IMUPLUS)) && tr++ < 5) {
        delay(300);
    }
    if (g_bno_ok) {
        g_bno.setExtCrystalUse(true);
        // Captura heading_inicial de arranque (== initialYaw 2025). Si el BNO aún no
        // dio una lectura estable, igual se sella en el primer tick válido.
        const float yaw0 = g_bno.getVector(Adafruit_BNO055::VECTOR_EULER).x();
        g_heading_inicial = yaw0;
        g_heading_inicial_set = true;
        g_io.heading_deg = yaw0;
        g_io.heading_valid = true;
        g_io.heading_error_deg = 0.0f;
    }
#endif
    // (En modo SNAPSHOT u OTOS no se toca ningún BNO local: heading_inicial se sella
    //  con el primer heading válido del snapshot del TOP / del primer Pose2D de DOWN.)
}

void mix_comm_tick() {
    // --- Drenar TOP (Serial7) ---
    while (Serial7.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial7.read());
        if (g_top_decoder.feed(b)) {
            handle_top_frame(g_top_decoder.get_frame());
        }
    }

    // --- Drenar DOWN (Serial1) ---
    while (Serial1.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial1.read());
        if (g_down_decoder.feed(b)) {
            handle_down_frame(g_down_decoder.get_frame());
        }
    }

#ifdef MIX_HEADING_LOCAL_BNO
    // Modo BNO local: leer el yaw en este tick (respaldo del snapshot).
    read_bno_heading();
#endif

    update_link_freshness();
}

}  // namespace mix
}  // namespace iitasoccer
