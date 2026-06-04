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
uint32_t g_bytes_received = 0;  // DIAG: bytes crudos leidos de Serial7 (link TOP->CENTRAL)
uint32_t g_snapshot_size_rejects = 0;  // CC-01: WorldSnapshot con tamano != schema (deploy v2/v3 desfasado)

constexpr long UART_BAUD = 230400;

void handle_frame(const Frame& f) {
    g_frames_received++;
    if (f.type == MsgType::WORLD_SNAPSHOT) {
        if (f.payload_len == sizeof(WorldSnapshot)) {
            WorldSnapshot snap{};
            memcpy(&snap, f.payload, sizeof(WorldSnapshot));
            world_model_apply_snapshot(snap);
        } else {
            // CC-01 (eval 2026-06-04): llego un WorldSnapshot con tamano distinto
            // al schema compilado -> una placa quedo con firmware viejo tras un
            // deploy wire-breaking (v2/v3). Sin este contador, en el pit se ve
            // IGUAL que "sin link". Se expone en la telemetria DIAG (badsz=).
            g_snapshot_size_rejects++;
        }
    }
}

}  // namespace

// UART hacia TOP = Serial7 (RX7 = pin 28, TX7 = pin 29) del Teensy 4.1.
// Reasignado 2026-05-31 (decisión Gustavo, cableado en banco): antes Serial1 (0/1);
// se movió a Serial7 (28/29, pines de expansión libres del Zircon) cuando el link
// DOWN→CENTRAL tomó Serial1 (0/1). Ver MAPA-CONEXIONES-3-PLACAS.md.
void comm_top_init() {
    Serial7.begin(UART_BAUD);
}

int comm_top_tick() {
    int processed = 0;
    while (Serial7.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial7.read());
        g_bytes_received++;  // DIAG: contar todo byte que entra por Serial7
        if (g_decoder.feed(b)) {
            handle_frame(g_decoder.get_frame());
            processed++;
        }
    }
    return processed;
}

uint32_t comm_top_get_frames_received() { return g_frames_received; }
uint32_t comm_top_get_crc_errors()      { return g_decoder.crc_errors(); }
uint32_t comm_top_get_bytes_received()  { return g_bytes_received; }
uint32_t comm_top_get_resync_events()   { return g_decoder.resync_events(); }
uint32_t comm_top_get_snapshot_size_rejects() { return g_snapshot_size_rejects; }

}  // namespace iitasoccer
