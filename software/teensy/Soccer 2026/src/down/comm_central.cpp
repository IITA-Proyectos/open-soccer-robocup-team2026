#include "comm_central.h"
#include "config_down.h"
#include "line_ring.h"
#include "otos.h"
#include "proto.h"
#include "types.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;
uint32_t g_frames_sent = 0;
uint8_t  g_send_seq = 0;

void handle_frame(const Frame& f) {
    switch (f.type) {
        case MsgType::CENTRAL_RESET_OTOS:
            otos_reset();
            break;
        case MsgType::CENTRAL_CALIB_LINE:
            if (f.payload_len >= 1) {
                if (f.payload[0] == 0) line_ring_calibrate_carpet();
                else if (f.payload[0] == 1) line_ring_calibrate_white();
            }
            break;
        default:
            // Comandos no esperados se ignoran.
            break;
    }
}

}  // namespace

void comm_central_init() {
    // Serial1 → conector U11 del schematic DOWN → CENTRAL.
    Serial1.begin(UART_TOP_BAUD);   // mismo baud que el otro UART, 230400.
}

int comm_central_tick() {
    int processed = 0;
    while (Serial1.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial1.read());
        if (g_decoder.feed(b)) {
            handle_frame(g_decoder.get_frame());
            g_frames_received++;
            processed++;
        }
    }
    return processed;
}

void comm_central_send_line_urgent() {
    LineStatus ls{};
    ls.angle_centideg = static_cast<int16_t>(line_ring_get_angle_deg() * 100.0f);
    // depth_mm: cantidad de sensores en blanco como proxy de profundidad.
    // Cuando tengamos calibración geométrica del anillo, mapear a mm reales.
    ls.depth_mm = line_ring_get_depth();
    ls.imminent_exit_flag = line_ring_get_imminent_exit() ? 1 : 0;

    Frame f{};
    f.type = MsgType::LINE_URGENT;
    f.seq = g_send_seq++;
    f.payload_len = sizeof(LineStatus);
    memcpy(f.payload, &ls, sizeof(LineStatus));

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n > 0) {
        Serial1.write(buf, n);
        g_frames_sent++;
    }
}

uint32_t comm_central_get_frames_received() { return g_frames_received; }
uint32_t comm_central_get_frames_sent()     { return g_frames_sent; }
uint32_t comm_central_get_crc_errors()      { return g_decoder.crc_errors(); }

}  // namespace iitasoccer
