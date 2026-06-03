// diag_central_comm_down.cpp — Receiver de banco: valida el link DOWN -> CENTRAL
//
// Para qué sirve:
//   Primer paso del bring-up de la comunicación DOWN → CENTRAL (el hito que
//   destraba la moratoria). CENTRAL escucha el UART, corre el FrameDecoder del
//   protocolo (proto.h), decodifica el LineStatusV2 que DOWN ya manda, y lo
//   muestra como un PANEL legible para humanos (estado, dirección con flecha,
//   barras) + salud del enlace.
//
//   Filosofía: probar que los bytes fluyen con CRC OK usando el payload que
//   DOWN ya emite, ANTES de sumar mensajes nuevos (los 32 sensores crudos son
//   un 2do paso — necesitan un mensaje nuevo que DOWN tiene que mandar).
//
// ⚠️ UART — Serial1 (RX1 = pin 0, TX1 = pin 1) del Teensy 4.1.
//    REASIGNADO 2026-05-31: el link DOWN→CENTRAL se movió de Serial2 (7/8) a
//    Serial1 (0/1). Motivo: los pines 7/8 son del driver del motor 2 (U17); al
//    moverlo a Serial1 (0/1, conector libre del Zircon) el CONFLICTO 7/8 queda
//    RESUELTO (F8/TASK-036) y el motor 2 se queda con 7/8 para sí solo. El
//    comm_down de produccion también usa Serial1, así que coincide.
//    Cableado:
//        DOWN  TX1 (pin 1)   ->   CENTRAL  RX1 (pin 0)
//        DOWN  GND           <->  CENTRAL  GND
//    Baud: 230400 (igual que DOWN — config_down.h / comm_central.cpp).
//
// Uso:
//   pio run -e diag_central_comm_down -t upload
//   pio device monitor -b 115200
//
// Atribución:
//   Author: Claude Opus 4.7 (Anthropic)
//   Requested-by: Gustavo Viollaz (@gviollaz)

#include <Arduino.h>

#include "proto.h"
#include "types.h"
#include "line_view.h"

using namespace iitasoccer;

namespace {

constexpr long DOWN_LINK_BAUD = 230400;   // mismo baud que DOWN

// UART hacia DOWN. Serial1 = RX1 pin 0 / TX1 pin 1 (Teensy 4.1). Reasignado
// 2026-05-31 desde Serial2 (7/8) para liberar los pines del motor 2 (U17) —
// conflicto 7/8 RESUELTO. Mismo UART que comm_down de produccion.
#define DOWN_UART Serial1

// Cada cuánto se redibuja el panel (ms). 500 ms = legible, no satura.
constexpr uint32_t PANEL_INTERVAL_MS = 500;

// Escala de la barra de penetración (mm a fondo de escala).
constexpr int PENETRATION_FULL_SCALE_MM = 50;

FrameDecoder g_decoder;

// Tracking de SEQ — cuenta discontinuidades (frames perdidos / fuera de orden).
bool     g_have_last_seq = false;
uint8_t  g_last_seq      = 0;
uint32_t g_seq_gaps      = 0;

// Último LineStatusV2 recibido.
bool         g_have_lsv2     = false;
LineStatusV2 g_lsv2{};
uint32_t     g_lsv2_count    = 0;
uint32_t     g_last_lsv2_ms  = 0;

// Frames válidos que NO son LineStatusV2 (para ver si llega "otra cosa").
uint32_t g_other_frames = 0;

elapsedMillis g_since_panel;

// ---- Helpers de presentación ----------------------------------------------

// Barra de progreso ASCII: [####------]
void print_bar(int value, int max_value, int width) {
    int filled = (max_value > 0) ? (value * width) / max_value : 0;
    if (filled > width) filled = width;
    if (filled < 0)     filled = 0;
    Serial.print('[');
    for (int i = 0; i < width; ++i) Serial.print(i < filled ? '#' : '-');
    Serial.print(']');
}

// Decodifica event_flags a una cadena corta legible.
void print_event_flags(uint8_t ev) {
    if (ev == 0) { Serial.print("(ninguno)"); return; }
    if (ev & EV_IMMINENT_EXIT)     Serial.print("SALIDA-INMINENTE ");
    if (ev & EV_CORNER)            Serial.print("ESQUINA ");
    if (ev & EV_LINE_END)          Serial.print("FIN-LINEA ");
    if (ev & EV_LIFTED)            Serial.print("LEVANTADO ");
    if (ev & EV_CALIB_SUSPECT)     Serial.print("CALIB? ");
    if (ev & EV_MUX_DEAD)          Serial.print("MUX-MUERTO ");
    if (ev & EV_DEGRADED_GEOMETRY) Serial.print("GEOMETRIA ");
    if (ev & EV_SENSOR_NOISY)      Serial.print("RUIDO ");
}

// Dirección de la línea como flecha + palabra (visto desde el robot).
// Convención: 0° = frente. Signo izq/der a confirmar en banco.
void print_direction(float deg) {
    float a = deg;
    while (a > 180.0f)  a -= 360.0f;
    while (a < -180.0f) a += 360.0f;

    char arrow;
    const char* label;
    if      (a >= -45.0f && a <= 45.0f) { arrow = '^'; label = "FRENTE"; }
    else if (a > 45.0f   && a < 135.0f) { arrow = '>'; label = "DERECHA"; }
    else if (a <= -45.0f && a > -135.0f){ arrow = '<'; label = "IZQUIERDA"; }
    else                                { arrow = 'v'; label = "ATRAS"; }

    Serial.print(arrow);
    Serial.print(' ');
    Serial.print(label);
    Serial.print("  (");
    Serial.print(deg, 0);
    Serial.print(" grados)");
}

// Vuelca TODOS los campos del LineStatusV2 ya decodificados (con N/A donde el
// contrato usa un sentinel). Es la vista "cruda decodificada": exactamente lo
// que sale del decoder, campo por campo, para no dejar duda de la decodificación.
void print_decoded_fields(const LineStatusV2& s) {
    Serial.print(F(" CAMPOS   : schema="));
    Serial.print(s.schema_version);
    Serial.print(F(" valid="));    Serial.print(s.data_valid);
    Serial.print(F(" present="));  Serial.print(s.line_present);
    Serial.print(F(" on_line="));  Serial.print(s.sensors_on_line);
    Serial.print(F(" q="));        Serial.print(s.quality);
    Serial.print(F(" age="));      Serial.print(s.sample_age_ms);
    Serial.print(F("ms ev=0x"));   Serial.println(s.event_flags, HEX);

    Serial.print(F("            angle="));
    if (s.line_angle_centideg == LSV2_NA_I16) Serial.print(F("N/A"));
    else { Serial.print(s.line_angle_centideg / 100.0f, 2); Serial.print(F("deg")); }
    Serial.print(F(" escape="));
    if (s.escape_angle_centideg == LSV2_NA_I16) Serial.print(F("N/A"));
    else { Serial.print(s.escape_angle_centideg / 100.0f, 2); Serial.print(F("deg")); }
    Serial.print(F(" pen="));
    if (s.penetration_mm == LSV2_NA_U16) Serial.print(F("N/A"));
    else { Serial.print(s.penetration_mm); Serial.print(F("mm")); }
    Serial.print(F(" cross="));
    if (s.cross_track_mm == LSV2_NA_I16) Serial.print(F("N/A"));
    else { Serial.print(s.cross_track_mm); Serial.print(F("mm")); }
    Serial.println();
}

// ---- RX --------------------------------------------------------------------

void on_frame(const Frame& f) {
    if (g_have_last_seq && f.seq != static_cast<uint8_t>(g_last_seq + 1)) {
        g_seq_gaps++;
    }
    g_last_seq = f.seq;
    g_have_last_seq = true;

    LineStatusV2 ls{};
    if (lsv2_from_frame(f, ls)) {
        g_lsv2 = ls;
        g_have_lsv2 = true;
        g_lsv2_count++;
        g_last_lsv2_ms = millis();
    } else {
        g_other_frames++;
    }
}

// ---- Panel -----------------------------------------------------------------

void print_panel() {
    Serial.println();
    Serial.println(F("+=========== LINEA  DOWN -> CENTRAL ===========+"));

    if (!g_have_lsv2) {
        Serial.print(F(" ESPERANDO datos de DOWN...   frames="));
        Serial.print(g_decoder.frames_decoded());
        Serial.print(F("  bytes="));
        Serial.println(g_decoder.bytes_received());
        if (g_decoder.bytes_received() == 0) {
            Serial.println(F(" -> No llega NADA. Revisar:"));
            Serial.println(F("    cable DOWN TX1(pin 1) -> CENTRAL RX1(pin 0),"));
            Serial.println(F("    GND comun, DOWN flasheado y enviando, baud 230400."));
        } else {
            Serial.println(F(" -> Llegan bytes pero ningun frame valido todavia."));
            Serial.println(F("    Revisar baud / TX-RX cruzados / masa."));
        }
        Serial.println(F("+==============================================+"));
        return;
    }

    const LineStatusV2& s = g_lsv2;
    const bool stale = (millis() - g_last_lsv2_ms) > 500;

    // --- Estado grande ---
    Serial.print(F(" ESTADO   : "));
    if      (lsv2_lifted(s))         Serial.println(F("[ ROBOT LEVANTADO - datos ignorados ]"));
    else if (!s.data_valid)          Serial.println(F("[ DATOS NO VALIDOS (data_valid=0) ]"));
    else if (lsv2_imminent_exit(s))  Serial.println(F("*** SALIDA INMINENTE - FRENAR ***"));
    else if (lsv2_line_present(s))   Serial.println(F(">>> SOBRE LA LINEA <<<"));
    else                             Serial.println(F("... fuera de linea ..."));

    // --- Direccion ---
    Serial.print(F(" DIRECCION: "));
    print_direction(lsv2_line_angle_deg(s));
    Serial.println();

    // --- Sensores en linea (CUENTA, no posiciones) ---
    const uint8_t on_line = lsv2_sensors_on_line(s);
    Serial.print(F(" SENSORES : "));
    if (on_line < 10) Serial.print('0');
    Serial.print(on_line);
    Serial.print(F("/32 "));
    print_bar(on_line, 32, 24);
    Serial.println();

    // --- Penetracion ---
    const uint8_t pen = lsv2_penetration_u8(s);
    Serial.print(F(" PENETRA. : "));
    if (pen < 100) Serial.print(' ');
    if (pen < 10)  Serial.print(' ');
    Serial.print(pen);
    Serial.print(F(" mm "));
    print_bar(pen, PENETRATION_FULL_SCALE_MM, 16);
    Serial.println();

    // --- Calidad ---
    Serial.print(F(" CALIDAD  : "));
    Serial.print(s.quality);
    Serial.println(F("/100"));

    // --- Eventos ---
    Serial.print(F(" EVENTOS  : "));
    print_event_flags(s.event_flags);
    Serial.println();

    // --- Campos crudos ya decodificados (vista "decodificada" completa) ---
    print_decoded_fields(s);

    // --- Salud del enlace ---
    Serial.println(F(" ----------------------------------------------"));
    Serial.print(F(" ENLACE   : "));
    Serial.print(g_decoder.frames_decoded());
    Serial.print(F(" frames | "));
    Serial.print(g_decoder.crc_errors());
    Serial.print(F(" CRC err | "));
    Serial.print(g_seq_gaps);
    Serial.print(F(" perdidos | hace "));
    Serial.print(millis() - g_last_lsv2_ms);
    Serial.print(F(" ms"));
    if (stale) Serial.print(F("  [STALE!]"));
    Serial.println();
    Serial.println(F("+==============================================+"));
}

}  // namespace

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }

    DOWN_UART.begin(DOWN_LINK_BAUD);

    Serial.println();
    Serial.println(F("=================================================="));
    Serial.println(F("  diag_central_comm_down  -  link DOWN -> CENTRAL"));
    Serial.println(F("=================================================="));
    Serial.println(F(" UART: Serial1 (RX1 = pin 0) @ 230400  (reasignado 2026-05-31 desde Serial2/7-8)"));
    Serial.println(F(" Cablear: DOWN TX1(pin 1) -> CENTRAL RX1(pin 0), GND comun."));
    Serial.println(F(" (Serial1/pin 0 -> sin conflicto con motores; 7/8 quedan para el motor 2.)"));
    Serial.println(F(" Panel cada 500 ms abajo:"));
}

void loop() {
    // Drenar el UART de DOWN y alimentar el decoder byte por byte.
    while (DOWN_UART.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(DOWN_UART.read());
        if (g_decoder.feed(b)) {
            on_frame(g_decoder.get_frame());
        }
    }

    // LED: ON fijo si recibimos un LSV2 fresco (<500 ms), si no parpadea.
    const bool fresh = g_have_lsv2 && (millis() - g_last_lsv2_ms) < 500;
    if (fresh) digitalWrite(LED_BUILTIN, HIGH);
    else       digitalWrite(LED_BUILTIN, (millis() / 250) % 2);

    if (g_since_panel >= PANEL_INTERVAL_MS) {
        g_since_panel = 0;
        print_panel();
    }
}
