#include "comm_central.h"
#include "config_down.h"
#include "line_ring.h"
#include "otos.h"
#include "proto.h"
#include "types.h"
#include "down_model.h"
#include "down_encode.h"
#include "down_tx.h"
#include "eeprom_calib.h"
// (sensor_geometry.h ya no se incluye: el bloque debug que usaba SENSOR_POS para
//  recalcular cross_track era código muerto y se borró — audit 2026-06-03 #9/#11.)

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;

#ifdef DOWN_DEBUG_TELEMETRY
// Cache del último LineStatusV2 difundido a CENTRAL (exacto: el que viaja por el
// cable, con sample_age_ms ya seteado). Lo lee la telemetría USB del modo DEBUG.
LineStatusV2 g_last_lsv2{};
bool         g_last_lsv2_valid = false;
#endif

// Value-init explícito ({}): garantiza por CÓDIGO el estado-limpio de TODA la
// DownModel (watchdogs mw/sh, histéresis was_white[], tracker, surface, filtros,
// calib) en vez de depender del zero-init de .bss. Sobrevive si alguien mueve
// g_dm a miembro/heap/stack o agrega un campo no-cero. Audit 2026-06-03 #10.
DownModel g_dm{};
DownModelCfg g_dmcfg = {
    /* imminent_depth        */ 6,
    /* adapt_alpha           */ 0.02f,
    // calib_min_margin: separación verde/blanco mínima (counts) para confiar en el
    // frame (data_valid). Banco 2026-06-06 (María, ROBOT1): márgenes estáticos reales
    // 82–339 (5 flacos: S09=82, S16=88, S32=108, S15=110, S30=116). 120 era inalcanzable.
    // CLAVE: con 60 arrancaba valid=1 pero a ~1-2 s la auto-adaptación del carpet
    // (adapt_alpha → lc_adapt_carpet lleva el baseline 'verde' al valor en vivo) erosiona
    // el margen del sensor más flaco (S09) por debajo de 60 → data_valid volvía a 0 (CALIB?).
    // Bajado a 40 = 2× la banda de histéresis (±20), piso razonable que aguanta la erosión.
    // Si reaparece CALIB? estable, S09 es el límite real (óptica/altura del sensor) o hay
    // que subir su señal / reducir adapt_alpha. Ver docs/pruebas-banco/DOWN.md.
    /* calib_min_margin      */ 40,
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
                    //
                    // BLOQUEO (audit 2026-06-03 #18): line_ring_calibrate_white()
                    // bloquea ~320ms (32 muestras × delay(10)) y ec_save_calibration
                    // hace un write a EEPROM emulada en flash (worst case decenas de
                    // ms con IRQ deshabilitada si toca borrado de sector). Durante
                    // ese lapso NO corre el muestreo de 1kHz ni los envíos. Es
                    // ACEPTABLE porque la calibración es operación de BANCO/ADMIN con
                    // el robot QUIETO: NUNCA se calibra en vivo durante un partido
                    // (nadie emite 0x21 en juego). No mover este write al hot-loop.
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

    // DOBLE CADENA DE LÍNEA (deuda conocida, NO archivar antes de Incheon): acá
    // RE-leemos los valores crudos del anillo y los reprocesamos con DownModel
    // para armar el LineStatusV2 que va a CENTRAL. NO usamos la salida de
    // line_ring_tick() (la "cadena vieja" que corre en paralelo a 1 kHz); solo
    // tomamos su lectura cruda (line_ring_get_raw). Ver FUENTES-DE-VERDAD §deudas.
    uint16_t raw[DM_MAX_SENSORS];
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        raw[i] = line_ring_get_raw(static_cast<uint8_t>(i));
    }

    // Procesar con DownModel → LineStatusV2.
    LineStatusV2 s = dm_update(g_dm, g_dmcfg, raw, NUM_LINE_SENSORS, millis());

    // Edad real de la muestra física (raw del line_ring) para que CENTRAL
    // detecte enlace/datos stale (contrato §3.5). Clamp a 0..255 ms.
    // Audit 2026-06-03 #2: usar el TIMESTAMP del último muestreo
    // (line_ring_get_last_sample_us), NO la duración del tick — antes el campo
    // viajaba saturado en 255 porque restaba una duración chica de micros().
    s.sample_age_ms = lsv2_sample_age_ms(micros(), line_ring_get_last_sample_us());

    // Difundir la línea a AMBAS placas (CENTRAL + TOP) con SEQ por enlace.
    down_tx_broadcast_line(s);

#ifdef DOWN_DEBUG_TELEMETRY
    // Snapshot del LineStatusV2 EXACTO que se difundió, para la telemetría USB.
    g_last_lsv2       = s;
    g_last_lsv2_valid = true;
#endif

#ifdef DOWN_DEBUG_SERIAL
    // Debug de bring-up (TASK-301): imprime por el USB, a ~4 Hz, lo esencial que
    // DOWN tiene listo para CENTRAL: ¿HAY LINEA? (line_present del DownModel —
    // logica de linea ya probada) + cross_track + la pose de los 2 OTOS + TX.
    // Se activa SOLO con -DDOWN_DEBUG_SERIAL (ver [env:down_debug]); el firmware
    // de competencia no lo trae. Reemplaza el volcado de 32 sensores crudos (eso
    // vive en diag_down).
    // NOTA: line_present y cross_track YA se transmiten a CENTRAL en LineStatusV2
    // (arriba, down_tx_broadcast_line). cross_track_mm sale del DownModel
    // (dm_update -> dm_line_metrics, geometria real, host-testeado); + adelante /
    // - atras. La pose OTOS TAMBIEN se difunde por down_tx a CENTRAL (Serial1) y
    // a TOP (Serial5). Aca solo lo MOSTRAMOS por USB (leyendo el `s` ya difundido,
    // sin recalcular nada). Audit 2026-06-03 #9: se borro un re-calculo de
    // cross_track que corria DESPUES del broadcast (codigo muerto, nunca se enviaba)
    // y daba un centroide-Y crudo distinto al de produccion.
    static elapsedMillis dbg_since;
    if (dbg_since >= 250) {
        dbg_since = 0;
        Serial.print("[DOWN] LINEA=");
        Serial.print(s.line_present ? "SI" : "NO");
        if (s.line_present && s.line_angle_centideg != LSV2_NA_I16) {
            Serial.print(" ang=");
            Serial.print(s.line_angle_centideg / 100.0f, 1);
        }
        if (s.cross_track_mm != LSV2_NA_I16) {
            Serial.print(" cross=");
            Serial.print(s.cross_track_mm);
            Serial.print("mm");
        }
        Serial.print("  | OTOS x=");  Serial.print(otos_get_x_mm(), 1);
        Serial.print(" y=");          Serial.print(otos_get_y_mm(), 1);
        Serial.print(" hdg=");        Serial.print(otos_get_heading_deg(), 1);
        Serial.print(" [L=");         Serial.print(otos_is_left_ready()  ? "ok" : "--");
        Serial.print(" R=");          Serial.print(otos_is_right_ready() ? "ok" : "--");
        Serial.print("]  | tx_ok=");  Serial.print(down_tx_get_sent(0));
        Serial.print(" drop=");       Serial.print(down_tx_get_dropped(0));
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
uint32_t comm_central_get_frames_sent()    { return down_tx_get_sent(0); }
uint32_t comm_central_get_frames_dropped() { return down_tx_get_dropped(0); }
uint32_t comm_central_get_crc_errors()      { return g_decoder.crc_errors(); }

#ifdef DOWN_DEBUG_TELEMETRY
bool comm_central_get_last_line_status(LineStatusV2& out) {
    if (!g_last_lsv2_valid) return false;
    out = g_last_lsv2;
    return true;
}
#endif

}  // namespace iitasoccer
