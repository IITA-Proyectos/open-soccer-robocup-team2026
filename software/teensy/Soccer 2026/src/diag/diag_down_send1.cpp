// diag_down_send1.cpp — Test MINIMO de enlace DOWN -> CENTRAL.
//
// Que hace: manda el caracter '1' por Serial1 (el UART hacia la placa CENTRAL)
// una vez por segundo, prende el LED 13 en cada envio, y lo avisa por el USB.
// NO usa los muxes ni los OTOS — por eso aca el pin 13 SI esta libre como LED
// (en el firmware de competencia, el pin 13 es SEL_A del mux U1, ver config_down.h).
//
// Para que sirve: aislar el cable DOWN->CENTRAL de TODO el protocolo (CRC, frames,
// LineStatusV2). Si CENTRAL recibe el '1', el cable + el UART funcionan. Es el
// "hola mundo" del enlace entre placas.
//
// CABLEADO (cruzado + GND comun):
//   DOWN  Serial1  TX = pin 1   ->   CENTRAL Serial2 RX = pin 7
//   DOWN  GND                   ->   CENTRAL GND
//   (opcional, para comandos al reves: CENTRAL pin 8 TX -> DOWN pin 0 RX)
//   Baud 230400. Ver el cuadrito de UARTs entre placas.
//
// BUILD (placa DOWN, Teensy 4.0):
//   pio run -e diag_down_send1 -t upload
//   pio device monitor -b 115200      ; deberias ver "[DOWN] enviado: 1 (#N)"
//
// Companion: en la placa CENTRAL cargar [env:diag_central_recv1] (diag_central_recv1.cpp).
//
// Atribucion:
//   Author: Claude Opus 4.7 (Anthropic)
//   Requested-by: Maria Viollaz (en la compu de Gustavo @gviollaz)

#include <Arduino.h>

namespace {
constexpr int      PIN_LED        = 13;      // LED onboard (libre aca: no hay mux)
constexpr long     UART_BAUD      = 230400;  // mismo baud que el enlace real
constexpr uint32_t SEND_PERIOD_MS = 1000;    // 1 Hz: lento para verlo facil
constexpr uint32_t LED_PULSE_MS   = 80;      // duracion del destello del LED

elapsedMillis g_since_send;
uint32_t      g_count = 0;
}  // namespace

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    Serial.begin(115200);       // USB (debug)
    Serial1.begin(UART_BAUD);   // -> CENTRAL (pin 1 = TX)

    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 1500) { /* esperar USB un toque */ }

    Serial.println();
    Serial.println("[DOWN] diag_down_send1: mando '1' por Serial1 (-> CENTRAL) a 1 Hz.");
    Serial.println("[DOWN] Cable: DOWN pin 1 (TX) -> CENTRAL pin 7 (RX) + GND comun.");
}

void loop() {
    if (g_since_send >= SEND_PERIOD_MS) {
        g_since_send = 0;
        Serial1.write('1');             // <-- el '1' que viaja a CENTRAL
        g_count++;
        digitalWrite(PIN_LED, HIGH);    // LED ON al enviar
        Serial.print("[DOWN] enviado: 1  (#");
        Serial.print(g_count);
        Serial.println(")");
    }

    // Apagar el LED LED_PULSE_MS despues del envio -> destello visible por envio.
    if (g_since_send >= LED_PULSE_MS) {
        digitalWrite(PIN_LED, LOW);
    }
}
