#include "comm_central.h"
#include "proto.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;
uint32_t g_frames_sent = 0;
uint8_t  g_send_seq = 0;

constexpr long UART_BAUD = 230400;

void handle_frame(const Frame& f) {
    g_frames_received++;
    // Comandos administrativos del CENTRAL hacia TOP irán acá (reset, calib).
    // Por ahora solo contamos para diagnóstico.
    (void)f;
}

}  // namespace

// UART hacia CENTRAL = Serial7 (Teensy 4.0: TX7 = pin 29, RX7 = pin 28).
// SWAP 2026-05-31 (TASK-204): la cámara trasera quedó SOLDADA en Serial5 (pin 21),
// confirmado en banco con diag_top_cameras, así que el link a CENTRAL se movió de
// Serial5 a Serial7 (pines 28/29, el UART libre). El Serial2 del TOP (pin 7) queda sin
// uso (el HC-SR04 está en pines 4/3, no en 7). Lado CENTRAL: recibe en su Serial7
// (RX7 = pin 28); el cable del TOP sale por pin 29 (TX7). El Serial1/pin 0 del CENTRAL
// quedó para la línea del DOWN.
void comm_central_init() {
    Serial7.begin(UART_BAUD);
}

int comm_central_tick() {
    int processed = 0;
    while (Serial7.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial7.read());
        if (g_decoder.feed(b)) {
            handle_frame(g_decoder.get_frame());
            processed++;
        }
    }
    return processed;
}

void comm_central_send_snapshot(const WorldSnapshot& snap) {
    Frame f{};
    f.type = MsgType::WORLD_SNAPSHOT;
    f.seq = g_send_seq++;
    f.payload_len = sizeof(WorldSnapshot);
    memcpy(f.payload, &snap, sizeof(WorldSnapshot));

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n > 0) {
        Serial7.write(buf, n);
        g_frames_sent++;
    }
}

uint32_t comm_central_get_frames_sent()     { return g_frames_sent; }
uint32_t comm_central_get_frames_received() { return g_frames_received; }

}  // namespace iitasoccer
