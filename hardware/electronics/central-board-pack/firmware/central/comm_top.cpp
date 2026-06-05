// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#include "comm_top.h"
#include "world_model.h"
#include "proto.h"
#include "types.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;

constexpr long UART_BAUD = 230400;

void handle_frame(const Frame& f) {
    g_frames_received++;
    if (f.type == MsgType::WORLD_SNAPSHOT && f.payload_len == sizeof(WorldSnapshot)) {
        WorldSnapshot snap{};
        memcpy(&snap, f.payload, sizeof(WorldSnapshot));
        world_model_apply_snapshot(snap);
    }
}

}  // namespace

void comm_top_init() {
    Serial1.begin(UART_BAUD);
}

int comm_top_tick() {
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

uint32_t comm_top_get_frames_received() { return g_frames_received; }
uint32_t comm_top_get_crc_errors()      { return g_decoder.crc_errors(); }

}  // namespace iitasoccer
