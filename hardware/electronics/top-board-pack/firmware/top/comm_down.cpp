// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#include "comm_down.h"
#include "config_top.h"
#include "proto.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;
uint8_t  g_send_seq = 0;

// Estado más reciente.
LineStatus  g_line{};
Pose2D      g_pose{};
Velocity2D  g_vel{};

uint32_t g_line_last_rx_ms = 0;
uint32_t g_pose_last_rx_ms = 0;
uint32_t g_vel_last_rx_ms  = 0;

bool fresh(uint32_t last_ms) {
    return (millis() - last_ms) < DOWN_HEARTBEAT_TIMEOUT_MS;
}

void send_empty(MsgType type) {
    Frame f{};
    f.type = type;
    f.seq = g_send_seq++;
    f.payload_len = 0;
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n > 0) Serial1.write(buf, n);
}

void handle_frame(const Frame& f) {
    g_frames_received++;
    switch (f.type) {
        // En la arquitectura nueva, LINE_URGENT (antes DOWN_LINE_STATUS) NO
        // viene a ARRIBA — viaja por bus de emergencia directo DOWN → CENTRAL.
        // Lo procesamos acá solo si llegó por error (no romper si por ahora
        // DOWN sigue mandándolo en la transición).
        case MsgType::LINE_URGENT:
            if (f.payload_len == sizeof(LineStatus)) {
                memcpy(&g_line, f.payload, sizeof(LineStatus));
                g_line_last_rx_ms = millis();
            }
            break;
        case MsgType::DOWN_OTOS_POSE:
            if (f.payload_len == sizeof(Pose2D)) {
                memcpy(&g_pose, f.payload, sizeof(Pose2D));
                g_pose_last_rx_ms = millis();
            }
            break;
        case MsgType::DOWN_OTOS_VEL:
            if (f.payload_len == sizeof(Velocity2D)) {
                memcpy(&g_vel, f.payload, sizeof(Velocity2D));
                g_vel_last_rx_ms = millis();
            }
            break;
        default:
            // Frames no esperados desde DOWN — ignorar.
            break;
    }
}

}  // namespace

void comm_down_init() {
    Serial1.begin(UART_FROM_DOWN_BAUD);
}

int comm_down_tick() {
    int processed = 0;
    while (Serial1.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial1.read());
        if (g_decoder.feed(b)) {
            handle_frame(g_decoder.get_frame());
            processed++;
        }
    }
    return processed;
}

void comm_down_send_reset_otos() {
    Frame f{};
    f.type = MsgType::CENTRAL_RESET_OTOS;
    f.seq = g_send_seq++;
    f.payload_len = 1;
    f.payload[0] = 1;
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n > 0) Serial1.write(buf, n);
}

void comm_down_send_calib_line(bool white) {
    Frame f{};
    f.type = MsgType::CENTRAL_CALIB_LINE;
    f.seq = g_send_seq++;
    f.payload_len = 1;
    f.payload[0] = white ? 1 : 0;
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n > 0) Serial1.write(buf, n);
}

bool              comm_down_is_line_fresh() { return fresh(g_line_last_rx_ms); }
const LineStatus& comm_down_get_line_status() { return g_line; }

bool              comm_down_is_pose_fresh() { return fresh(g_pose_last_rx_ms); }
const Pose2D&     comm_down_get_pose() { return g_pose; }

bool              comm_down_is_vel_fresh() { return fresh(g_vel_last_rx_ms); }
const Velocity2D& comm_down_get_velocity() { return g_vel; }

uint32_t comm_down_get_frames_received() { return g_frames_received; }
uint32_t comm_down_get_crc_errors()      { return g_decoder.crc_errors(); }

}  // namespace iitasoccer
