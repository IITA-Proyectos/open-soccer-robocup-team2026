#include "down_tx.h"
#include "down_encode.h"
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

// Encoder genérico para pose/vel (frame proto.h con TYPE/SEQ/PAYLOAD). La LÍNEA
// usa down_encode_line() directamente (ver down_tx_broadcast_line) para que el
// camino VIVO sea EL MISMO que el testeado contra el golden del contrato
// (audit 2026-06-03 #11).
size_t encode_generic(MsgType type, uint8_t seq, const void* payload, size_t len,
                      uint8_t* out, size_t out_size) {
    Frame f{};
    f.type = type;
    f.seq = seq;
    f.payload_len = static_cast<uint8_t>(len);
    memcpy(f.payload, payload, len);
    return proto_encode(f, out, out_size);
}

// Escribe un frame ya codificado a un enlace con backpressure (no bloquea el
// line_ring de 1 kHz: si el buffer está lleno, dropea y cuenta).
void write_frame(DownLink& lk, const uint8_t* buf, size_t n) {
    if (n == 0) return;
    if (lk.uart->availableForWrite() >= static_cast<int>(n)) {
        lk.uart->write(buf, n);
        lk.sent++;
    } else {
        lk.dropped++;                       // backpressure: dropear, no bloquear
    }
}

}  // namespace

void down_tx_broadcast_line(const LineStatusV2& s) {
    // Camino VIVO == camino TESTEADO: down_encode_line() es el encoder pinneado
    // al golden de 23 bytes del contrato (test_down_encode + test_down_tx).
    for (auto& lk : g_links) {
        uint8_t buf[PROTO_MAX_FRAME];
        size_t n = down_encode_line(s, lk.seq++, buf, sizeof(buf));  // SEQ por enlace
        write_frame(lk, buf, n);
    }
}

void down_tx_broadcast_pose(const Pose2D& p) {
    for (auto& lk : g_links) {
        uint8_t buf[PROTO_MAX_FRAME];
        size_t n = encode_generic(MsgType::DOWN_OTOS_POSE, lk.seq++, &p, sizeof(p), buf, sizeof(buf));
        write_frame(lk, buf, n);
    }
}

void down_tx_broadcast_vel(const Velocity2D& v) {
    for (auto& lk : g_links) {
        uint8_t buf[PROTO_MAX_FRAME];
        size_t n = encode_generic(MsgType::DOWN_OTOS_VEL, lk.seq++, &v, sizeof(v), buf, sizeof(buf));
        write_frame(lk, buf, n);
    }
}

uint32_t down_tx_get_sent(uint8_t link)    { return link < 2 ? g_links[link].sent    : 0; }
uint32_t down_tx_get_dropped(uint8_t link) { return link < 2 ? g_links[link].dropped : 0; }

}  // namespace iitasoccer
