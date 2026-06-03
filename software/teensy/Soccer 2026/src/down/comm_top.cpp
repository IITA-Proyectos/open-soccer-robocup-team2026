#include "comm_top.h"
#include "config_down.h"
#include "line_ring.h"
#include "otos.h"
#include "proto.h"
#include "types.h"
#include "down_tx.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;

void handle_frame(const Frame& f) {
    switch (f.type) {
        case MsgType::CENTRAL_RESET_OTOS:
            otos_reset();
            break;
        case MsgType::CENTRAL_CALIB_LINE:
            // El primer byte del payload indica qué calibrar:
            //   0 = carpet, 1 = white.
            if (f.payload_len >= 1) {
                if (f.payload[0] == 0) line_ring_calibrate_carpet();
                else if (f.payload[0] == 1) line_ring_calibrate_white();
            }
            break;
        default:
            // Frames no relevantes para DOWN — ignorar silenciosamente.
            break;
    }
}

}  // namespace

void comm_top_init() {
    Serial5.begin(UART_TOP_BAUD);
}

int comm_top_tick() {
    int processed = 0;
    while (Serial5.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial5.read());
        if (g_decoder.feed(b)) {
            handle_frame(g_decoder.get_frame());
            g_frames_received++;
            processed++;
        }
    }
    return processed;
}

void comm_top_send_status() {
    // NOTA: en la arquitectura nueva, LineStatus YA NO se envía a ARRIBA.
    // ARRIBA solo necesita la odometría OTOS para fusión sensorial.
    // La línea (LINE_URGENT) viaja por el bus de emergencia DOWN → CENTRAL
    // (ver src/down/comm_central.{h,cpp}).

    // OTOS POSE — pose central del robot (fusión de los 2 OTOS, o el único disponible).
    Pose2D pose{};
    pose.x_mm = static_cast<int16_t>(otos_get_x_mm());
    pose.y_mm = static_cast<int16_t>(otos_get_y_mm());
    pose.heading_centideg = static_cast<int16_t>(otos_get_heading_deg() * 100.0f);
    pose.confidence = (otos_is_left_ready() && otos_is_right_ready()) ? 100
                    : (otos_is_left_ready() || otos_is_right_ready()) ? 60
                    : 0;
    down_tx_broadcast_pose(pose);

    // OTOS VEL — velocidades + slip_estimate (DOWN-specific).
    Velocity2D vel{};
    vel.vx_mm_s = static_cast<int16_t>(otos_get_vx_mm_s());
    vel.vy_mm_s = static_cast<int16_t>(otos_get_vy_mm_s());
    // omega: el getter da rad/s; Velocity2D usa centideg/s. Convertimos.
    vel.omega_centideg_s = static_cast<int16_t>(
        otos_get_omega_rad_s() * (18000.0f / 3.14159265f));
    vel.slip_estimate = static_cast<uint8_t>(
        otos_get_slip_estimate() > 255.0f ? 255 : otos_get_slip_estimate());
    down_tx_broadcast_vel(vel);
}

uint32_t comm_top_get_frames_received() { return g_frames_received; }
uint32_t comm_top_get_frames_sent()    { return down_tx_get_sent(1); }
uint32_t comm_top_get_frames_dropped() { return down_tx_get_dropped(1); }
uint32_t comm_top_get_crc_errors()      { return g_decoder.crc_errors(); }

}  // namespace iitasoccer
