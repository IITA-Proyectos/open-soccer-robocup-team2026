#include "comm_down.h"
#include "world_model.h"
#include "proto.h"
#include "types.h"
#include "line_view.h"
#include "pose_view.h"

#include <Arduino.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;
uint8_t  g_send_seq = 0;

// Deteccion de perdida de frames (el SEQ del protocolo viaja 0..255 con wrap).
bool     g_have_last_seq = false;
uint8_t  g_last_seq      = 0;
uint32_t g_frames_lost   = 0;
// ⚠️ g_frames_lost cuenta HUECOS DE SEQ: incluye tanto pérdida de cable como frames que el
// FrameDecoder descartó (CRC malo, LEN>32/resync, END malo) — esos NO llegan a handle_frame y su
// SEQ queda como hueco. Para distinguir, mirar crc_errors() + resync_events() (este último aún sin
// exponer en la telemetría). El "lost enorme con crc=0" de la memoria = resync, NO pérdida de cable.

constexpr long UART_BAUD = 230400;

void handle_frame(const Frame& f) {
    g_frames_received++;
    // Perdida de frames: (uint8_t)(seq - last - 1) = cuantos faltaron entre este
    // y el anterior (0 si consecutivo; el cast maneja el wrap 255->0). Mide la
    // MAGNITUD del hueco, no solo el evento — telemetria para "aprendizaje Incheon".
    if (g_have_last_seq) {
        g_frames_lost += static_cast<uint8_t>(f.seq - g_last_seq - 1);
    }
    g_last_seq = f.seq;
    g_have_last_seq = true;

    LineStatusV2 ls{};
    if (lsv2_from_frame(f, ls)) { world_model_apply_line(ls); return; }
    Pose2D pose{};
    if (pose_from_frame(f, pose)) { world_model_apply_otos_pose(pose); return; }
    Velocity2D vel{};
    if (vel_from_frame(f, vel)) { world_model_apply_otos_vel(vel); return; }
}

}  // namespace

// UART hacia DOWN = Serial1 (RX1 = pin 0, TX1 = pin 1) del Teensy 4.1.
// Reasignado 2026-05-31: antes Serial2 (7/8) — pero esos pines son del driver del
// motor 2 (U17). Movido a Serial1 (0/1, conector libre del Zircon) → CONFLICTO 7/8
// RESUELTO (F8/TASK-036): el motor 2 ya tiene los pines 7/8 para sí solo.
void comm_down_init() {
    Serial1.begin(UART_BAUD);
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

uint32_t comm_down_get_frames_received() { return g_frames_received; }
uint32_t comm_down_get_crc_errors()      { return g_decoder.crc_errors(); }
uint32_t comm_down_get_frames_lost()     { return g_frames_lost; }

}  // namespace iitasoccer
