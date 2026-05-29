#include "comm_central.h"
#include "config_down.h"
#include "line_ring.h"
#include "otos.h"
#include "proto.h"
#include "types.h"
#include "down_model.h"
#include "down_encode.h"
#include "eeprom_calib.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;
uint32_t g_frames_sent = 0;
uint32_t g_frames_dropped = 0;   // P1.6: frames descartados por TX buffer lleno
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

// Deriva la calib por-sensor de g_dm desde los promedios actuales del line_ring
// y la marca como inicializada. Usada por el lazy-init del send y por el handler
// de recalibración. (DRY: una sola fuente de la derivación.)
void derive_calib_from_line_ring() {
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        lc_set_static(g_dm.calib[i],
                      line_ring_get_carpet_avg(static_cast<uint8_t>(i)),
                      line_ring_get_white_avg(static_cast<uint8_t>(i)));
    }
    g_dm_init = true;
}

void handle_frame(const Frame& f) {
    switch (f.type) {
        case MsgType::CENTRAL_RESET_OTOS:
            otos_reset();
            break;
        case MsgType::CENTRAL_CALIB_LINE:
            if (f.payload_len >= 1) {
                if (f.payload[0] == 0) {
                    // Paso 1 (carpet): re-derivar en el proximo send. Aun falta
                    // el blanco — todavia no persistimos (calib incompleta).
                    line_ring_calibrate_carpet();
                    g_dm_init = false;
                } else if (f.payload[0] == 1) {
                    // Paso 2 (white): calib completa (carpet previo + blanco
                    // ahora). Derivar ya mismo y PERSISTIR en EEPROM para que
                    // sobreviva al power cycle (audit P0.2 — 2026-05-29).
                    line_ring_calibrate_white();
                    derive_calib_from_line_ring();
                    if (ec_save_calibration(g_dm.calib, NUM_LINE_SENSORS)) {
                        Serial.println("[DOWN] calib persistida en EEPROM");
                    } else {
                        Serial.println("[DOWN] WARN: fallo guardar calib en EEPROM");
                    }
                }
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
    // Inicialización lazy: derivar calib desde line_ring la primera vez (salvo
    // que ya esté cargada de EEPROM por comm_central_load_persisted_calib()).
    if (!g_dm_init) {
        derive_calib_from_line_ring();
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
        // Backpressure (audit P1.6 — 2026-05-29): escribir solo si hay espacio
        // en el TX buffer del UART. Serial.write() del core Teensy hace
        // busy-wait cuando el buffer está lleno — a 200 Hz eso le robaría
        // ciclos al line_ring de 1 kHz. Si no hay espacio dropeamos el frame:
        // CENTRAL tolera huecos (siempre actúa sobre el measurement más
        // reciente, no acumula). El contador permite diagnosticar saturación.
        if (Serial1.availableForWrite() >= (int)nb) {
            Serial1.write(buf, nb);
            g_frames_sent++;
        } else {
            g_frames_dropped++;
        }
    }

#ifdef DOWN_DEBUG_SERIAL
    // Debug de bring-up (TASK-301): imprime por el USB, a ~4 Hz, lo esencial que
    // DOWN tiene listo para CENTRAL: ¿HAY LINEA? (line_present del DownModel —
    // logica de linea ya probada) + la pose de los 2 OTOS + contadores de TX.
    // Se activa SOLO con -DDOWN_DEBUG_SERIAL (ver [env:down_debug]); el firmware
    // de competencia no lo trae. Reemplaza el volcado de 32 sensores crudos (eso
    // vive en diag_down).
    // NOTA: line_present YA se transmite a CENTRAL en LineStatusV2 (arriba). La
    // pose OTOS por ahora NO se transmite a CENTRAL (va al TOP por Serial5);
    // mandarla a CENTRAL requiere destrabar los pines 7/8 (TASK-036). Aca se
    // muestra por USB para validar que el dato existe y responde.
    static elapsedMillis dbg_since;
    if (dbg_since >= 250) {
        dbg_since = 0;
        Serial.print("[DOWN] LINEA=");
        Serial.print(s.line_present ? "SI" : "NO");
        if (s.line_present && s.line_angle_centideg != LSV2_NA_I16) {
            Serial.print(" ang=");
            Serial.print(s.line_angle_centideg / 100.0f, 1);
        }
        Serial.print("  | OTOS x=");  Serial.print(otos_get_x_mm(), 1);
        Serial.print(" y=");          Serial.print(otos_get_y_mm(), 1);
        Serial.print(" hdg=");        Serial.print(otos_get_heading_deg(), 1);
        Serial.print(" [L=");         Serial.print(otos_is_left_ready()  ? "ok" : "--");
        Serial.print(" R=");          Serial.print(otos_is_right_ready() ? "ok" : "--");
        Serial.print("]  | tx_ok=");  Serial.print(g_frames_sent);
        Serial.print(" drop=");       Serial.print(g_frames_dropped);
        Serial.println();
    }
#endif
}

bool comm_central_load_persisted_calib() {
    // EEPROM gana sobre la derivación boot-time del line_ring: trae una
    // referencia de BLANCO real (medida en una calibración manual previa), que
    // el boot no tiene (solo capturó carpet). El carpet ligeramente stale se
    // auto-corrige por lc_adapt_carpet en los primeros segundos de operación.
    if (ec_load_calibration(g_dm.calib, NUM_LINE_SENSORS)) {
        g_dm_init = true;   // bloquea la calib: no re-derivar desde line_ring
        return true;
    }
    return false;           // EEPROM vacía/inválida → lazy-init fallback
}

uint32_t comm_central_get_frames_received() { return g_frames_received; }
uint32_t comm_central_get_frames_sent()     { return g_frames_sent; }
uint32_t comm_central_get_frames_dropped()  { return g_frames_dropped; }
uint32_t comm_central_get_crc_errors()      { return g_decoder.crc_errors(); }

}  // namespace iitasoccer
