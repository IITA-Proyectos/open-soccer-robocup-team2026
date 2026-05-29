// diag_central_comm_down.cpp — Receiver de banco: valida el link DOWN -> CENTRAL
//
// Para qué sirve:
//   Primer paso del bring-up de la comunicación DOWN -> CENTRAL. Escucha el
//   UART donde DOWN reporta, corre el FrameDecoder del protocolo (proto.h), y
//   por cada frame válido imprime el LineStatusV2 decodificado + la SALUD del
//   enlace (bytes, frames OK, errores CRC, resyncs, huecos de SEQ).
//
//   Filosofía: probar que los BYTES FLUYEN con CRC OK usando el payload que
//   DOWN ya manda (LineStatusV2), ANTES de sumar mensajes nuevos (32 sensores
//   crudos). Si esto anda, el canal físico + el protocolo están validados.
//
// ⚠️ UART — Serial7 (RX7 = pin 28, TX7 = pin 29) del Teensy 4.1.
//    NO se usa Serial2 (pines 7/8): esos pines son el MOTOR 2 (driver U17),
//    confirmado en diag_central_motors el 2026-05-29 (resuelve TASK-036).
//    Cableado correcto:
//        DOWN  TX1 (pin 1)   ->   CENTRAL  RX7 (pin 28)
//        DOWN  GND           <->  CENTRAL  GND
//    Baud: 230400 (igual que DOWN — ver config_down.h / comm_central.cpp).
//
// DOWN manda hoy: LineStatusV2 (MsgType LINE_URGENT, 16 bytes) — resumen
//   procesado del anillo de 32 sensores (ángulo, penetración, CUENTA de
//   sensores en línea 0..32, flags). NO manda los 32 sensores individuales
//   (eso es un mensaje nuevo, 2do paso del bring-up).
//
// Uso:
//   pio run -e diag_central_comm_down -t upload
//   pio device monitor -b 115200
//
// Qué esperar:
//   - DOWN fuera de la línea (sobre carpet): frames suben sostenido, crc_err≈0,
//     present=0, on_line bajo.
//   - DOWN sobre la línea blanca: present=1, on_line sube, ang/pen coherentes.
//   - DOWN levantado en el aire: flags incluye LIFT.
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

// UART hacia DOWN. Serial7 = RX7 pin 28 / TX7 pin 29 (Teensy 4.1).
// NO Serial2 (pines 7/8 = motor 2). Si algún día se recablea, cambiar acá.
#define DOWN_UART Serial7

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

elapsedMillis g_since_print;

// Decodifica event_flags a una cadena corta legible.
void print_event_flags(uint8_t ev) {
    if (ev == 0) { Serial.print("-"); return; }
    if (ev & EV_IMMINENT_EXIT)     Serial.print("IMM ");
    if (ev & EV_CORNER)            Serial.print("COR ");
    if (ev & EV_LINE_END)          Serial.print("END ");
    if (ev & EV_LIFTED)            Serial.print("LIFT ");
    if (ev & EV_CALIB_SUSPECT)     Serial.print("CAL? ");
    if (ev & EV_MUX_DEAD)          Serial.print("MUX! ");
    if (ev & EV_DEGRADED_GEOMETRY) Serial.print("GEO ");
    if (ev & EV_SENSOR_NOISY)      Serial.print("NOISY ");
}

void on_frame(const Frame& f) {
    // SEQ: contar discontinuidades.
    if (g_have_last_seq && f.seq != static_cast<uint8_t>(g_last_seq + 1)) {
        g_seq_gaps++;
    }
    g_last_seq = f.seq;
    g_have_last_seq = true;

    // ¿Es el LineStatusV2 que DOWN manda?
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

void print_status() {
    Serial.print("[COMM-DOWN] bytes=");
    Serial.print(g_decoder.bytes_received());
    Serial.print(" frames=");
    Serial.print(g_decoder.frames_decoded());
    Serial.print(" crc_err=");
    Serial.print(g_decoder.crc_errors());
    Serial.print(" resync=");
    Serial.print(g_decoder.resync_events());
    Serial.print(" seq_gaps=");
    Serial.print(g_seq_gaps);
    Serial.print(" otros=");
    Serial.print(g_other_frames);

    if (!g_have_lsv2) {
        if (g_decoder.frames_decoded() == 0) {
            Serial.println(" | sin frames -> revisar: cable DOWN TX1(pin1)->CENTRAL RX7(pin28), GND comun, DOWN flasheado y enviando, baud 230400.");
        } else {
            Serial.println(" | frames OK pero ninguno es LineStatusV2 (16B). Revisar version de firmware DOWN.");
        }
        return;
    }

    const LineStatusV2& s = g_lsv2;
    Serial.print(" | LSV2 #");
    Serial.print(g_lsv2_count);
    Serial.print(" hace ");
    Serial.print(millis() - g_last_lsv2_ms);
    Serial.print("ms valid=");
    Serial.print(s.data_valid);
    Serial.print(" present=");
    Serial.print(lsv2_line_present(s) ? 1 : 0);
    Serial.print(" ang=");
    Serial.print(lsv2_line_angle_deg(s), 1);
    Serial.print(" pen=");
    Serial.print(lsv2_penetration_u8(s));
    Serial.print(" on_line=");
    Serial.print(lsv2_sensors_on_line(s));
    Serial.print("/32 q=");
    Serial.print(s.quality);
    Serial.print(" age=");
    Serial.print(s.sample_age_ms);
    Serial.print("ms flags=[");
    print_event_flags(s.event_flags);
    Serial.println("]");
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
    Serial.println("==================================================");
    Serial.println("  diag_central_comm_down  -  link DOWN -> CENTRAL");
    Serial.println("==================================================");
    Serial.println(" UART: Serial7  (RX7 = pin 28)  @ 230400");
    Serial.println(" Cablear: DOWN TX1(pin 1) -> CENTRAL RX7(pin 28), GND comun.");
    Serial.println(" NO usar Serial2 (pines 7/8 = motor 2).");
    Serial.println(" Decodifica el LineStatusV2 que DOWN ya manda + salud del enlace.");
    Serial.println(" Status cada 500 ms abajo:");
    Serial.println();
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

    if (g_since_print >= 500) {
        g_since_print = 0;
        print_status();
    }
}
