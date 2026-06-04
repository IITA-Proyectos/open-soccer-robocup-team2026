#include "comm_central.h"
#include "comm_arbiter.h"   // LinkSeqTracker / link_seq_track (P1-SEQ-LINK-HEALTH)
#include "proto.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;
uint32_t g_frames_sent = 0;
uint8_t  g_send_seq = 0;
LinkSeqTracker g_seq{};   // gap de SEQ del enlace CENTRAL→TOP (RX) (P1-SEQ-LINK-HEALTH)

constexpr long UART_BAUD = 230400;

void handle_frame(const Frame& f) {
    g_frames_received++;
    // Comandos administrativos del CENTRAL hacia TOP irán acá (reset, calib).
    // Por ahora solo contamos para diagnóstico.
    (void)f;
}

}  // namespace

// UART hacia CENTRAL = Serial4 (Teensy 4.0: RX4 = pin 16, TX4 = pin 17).
// FIX 2026-06-02 (Gustavo, banco): mapeo UART del TOP corregido. El Teensy 4.0 NO expone
// Serial7 (28/29) en el borde (son pads SMD traseros) -> TASK-204 lo habia puesto en
// Serial7 por error y el TOP nunca le llegaba a la CENTRAL (snap_fresh=N). El cableado
// REAL del TOP es: Serial2 (7/8) = modulo COMM (arbitros), Serial4 (16/17) = enlace CENTRAL.
// CABLE: TOP pin 17 (TX4) -> CENTRAL pin 28 (RX7, Serial7 del Teensy 4.1, que SI es de
// borde) + GND comun. El lado CENTRAL (comm_top.cpp) recibe en su Serial7 (4.1, 28/29 OK).
void comm_central_init() {
    Serial4.begin(UART_BAUD);
}

int comm_central_tick() {
    int processed = 0;
    while (Serial4.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial4.read());
        if (g_decoder.feed(b)) {
            const Frame& f = g_decoder.get_frame();
            link_seq_track(g_seq, f.seq);   // salud del enlace CENTRAL→TOP por SEQ
            handle_frame(f);
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
        Serial4.write(buf, n);
        g_frames_sent++;
    }
}

uint32_t comm_central_get_frames_sent()     { return g_frames_sent; }
uint32_t comm_central_get_frames_received() { return g_frames_received; }
uint32_t comm_central_get_frames_lost()     { return g_seq.lost; }

}  // namespace iitasoccer
