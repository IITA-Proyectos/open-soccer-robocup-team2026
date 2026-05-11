#include "comm_down.h"
#include "world_model.h"
#include "proto.h"
#include "types.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;
uint8_t  g_send_seq = 0;

constexpr long UART_BAUD = 230400;

void handle_frame(const Frame& f) {
    g_frames_received++;
    if (f.type == MsgType::LINE_URGENT && f.payload_len == sizeof(LineStatus)) {
        LineStatus ls{};
        memcpy(&ls, f.payload, sizeof(LineStatus));
        world_model_apply_line(ls);
    }
}

}  // namespace

void comm_down_init() {
    Serial2.begin(UART_BAUD);
}

int comm_down_tick() {
    int processed = 0;
    while (Serial2.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial2.read());
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
    if (n > 0) Serial2.write(buf, n);
}

void comm_down_send_calib_line(bool white) {
    Frame f{};
    f.type = MsgType::CENTRAL_CALIB_LINE;
    f.seq = g_send_seq++;
    f.payload_len = 1;
    f.payload[0] = white ? 1 : 0;
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n > 0) Serial2.write(buf, n);
}

uint32_t comm_down_get_frames_received() { return g_frames_received; }
uint32_t comm_down_get_crc_errors()      { return g_decoder.crc_errors(); }

}  // namespace iitasoccer
