// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#include "comm_central.h"
#include "config_down.h"
#include "line_ring.h"
#include "otos.h"
#include "proto.h"
#include "types.h"
#include "down_model.h"
#include "down_encode.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;
uint32_t g_frames_sent = 0;
uint8_t  g_send_seq = 0;

DownModel g_dm;
DownModelCfg g_dmcfg = {
    /* imminent_depth        */ 6,
    /* adapt_alpha           */ 0.02f,
    /* calib_min_margin      */ 120,
    /* lifted_debounce_ms    */ 100,
    /* lifted_min_sensors    */ (NUM_LINE_SENSORS * 7) / 8,
    /* lifted_delta_below    */ 80,
    /* line_end_min_track_ms */ 200
};
bool g_dm_init = false;

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
            g_dm_init = false;  // recalibrar: forzar reload de calib en el proximo send
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
    // Inicialización lazy: cargar calibración por sensor desde line_ring la primera vez.
    if (!g_dm_init) {
        for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
            lc_set_static(g_dm.calib[i],
                          line_ring_get_carpet_avg(static_cast<uint8_t>(i)),
                          line_ring_get_white_avg(static_cast<uint8_t>(i)));
        }
        g_dm_init = true;
    }

    // Leer valores crudos del anillo de sensores.
    uint16_t raw[DM_MAX_SENSORS];
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        raw[i] = line_ring_get_raw(static_cast<uint8_t>(i));
    }

    // Procesar con DownModel → LineStatusV2.
    LineStatusV2 s = dm_update(g_dm, g_dmcfg, raw, NUM_LINE_SENSORS, millis());

    // Edad real de la muestra física (raw del line_ring) para que CENTRAL
    // detecte enlace/datos stale (contrato §3.5). Clamp a 0..255 ms.
    uint32_t age_ms = (micros() - line_ring_get_last_tick_us()) / 1000u;
    s.sample_age_ms = (age_ms > 255u) ? 255u : (uint8_t)age_ms;

    // Serializar y enviar.
    uint8_t buf[PROTO_MAX_FRAME];
    size_t nb = down_encode_line(s, g_send_seq++, buf, sizeof(buf));
    if (nb > 0) {
        Serial1.write(buf, nb);
        g_frames_sent++;
    }
}

uint32_t comm_central_get_frames_received() { return g_frames_received; }
uint32_t comm_central_get_frames_sent()     { return g_frames_sent; }
uint32_t comm_central_get_crc_errors()      { return g_decoder.crc_errors(); }

}  // namespace iitasoccer
