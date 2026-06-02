#include "down_tx.h"
#include "proto.h"
#include <Arduino.h>
#include <string.h>

namespace iitasoccer {
namespace {

struct DownLink {
    HardwareSerial* uart;
    uint8_t  seq;
    uint32_t sent;
    uint32_t dropped;
};

// Enlace 0 = CENTRAL (Serial1), enlace 1 = TOP (Serial5).
DownLink g_links[2] = {
    { &Serial1, 0, 0, 0 },
    { &Serial5, 0, 0, 0 },
};

void send_on_link(DownLink& lk, MsgType type, const void* payload, size_t len) {
    Frame f{};
    f.type = type;
    f.seq = lk.seq++;                       // SEQ propio del enlace
    f.payload_len = static_cast<uint8_t>(len);
    memcpy(f.payload, payload, len);

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n == 0) return;
    if (lk.uart->availableForWrite() >= static_cast<int>(n)) {
        lk.uart->write(buf, n);
        lk.sent++;
    } else {
        lk.dropped++;                       // backpressure: dropear, no bloquear
    }
}

void broadcast(MsgType type, const void* payload, size_t len) {
    send_on_link(g_links[0], type, payload, len);   // CENTRAL
    send_on_link(g_links[1], type, payload, len);   // TOP
}

}  // namespace

void down_tx_broadcast_line(const LineStatusV2& s) { broadcast(MsgType::LINE_URGENT,    &s, sizeof(s)); }
void down_tx_broadcast_pose(const Pose2D& p)       { broadcast(MsgType::DOWN_OTOS_POSE, &p, sizeof(p)); }
void down_tx_broadcast_vel(const Velocity2D& v)    { broadcast(MsgType::DOWN_OTOS_VEL,  &v, sizeof(v)); }

uint32_t down_tx_get_sent(uint8_t link)    { return link < 2 ? g_links[link].sent    : 0; }
uint32_t down_tx_get_dropped(uint8_t link) { return link < 2 ? g_links[link].dropped : 0; }

}  // namespace iitasoccer
