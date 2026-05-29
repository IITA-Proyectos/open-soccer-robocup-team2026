// diag_central_recv1.cpp — Test MINIMO de recepcion en CENTRAL desde DOWN.
//
// Que hace: lee Serial2 (el UART que viene de la placa DOWN, pines 7/8 del
// Teensy 4.1) y cada vez que recibe un '1' prende el LED 13 y lo avisa por USB.
// NO inicializa los motores -> por eso el pin 13 esta libre como LED y los pines
// 7/8 quedan como Serial2 (el conflicto motor de TASK-036 SOLO aparece cuando se
// manejan motores; en recepcion pura no molesta).
//
// Companion de diag_down_send1.cpp (placa DOWN). Juntos prueban el cable
// DOWN->CENTRAL sin protocolo: si aca aparece "recibi: 1", el enlace anda.
//
// CABLEADO (cruzado + GND comun):
//   CENTRAL Serial2 RX = pin 7  <-  DOWN Serial1 TX = pin 1
//   CENTRAL GND                 <-  DOWN GND
//   Baud 230400.
//
// BUILD (placa CENTRAL, Teensy 4.1):
//   pio run -e diag_central_recv1 -t upload
//   pio device monitor -b 115200      ; deberias ver "[CENTRAL] recibi: 1 (#N)"
//
// NOTA multi-agente: sketch de banco para la placa CENTRAL, agregado desde la
// sesion DOWN en coordinacion con Gustavo (su maquina). El merge a main lo hace
// el coach.
//
// Atribucion:
//   Author: Claude Opus 4.7 (Anthropic)
//   Requested-by: Maria Viollaz (en la compu de Gustavo @gviollaz)

#include <Arduino.h>

namespace {
constexpr int      PIN_LED      = 13;       // LED onboard (libre: no hay motores)
constexpr long     UART_BAUD    = 230400;
constexpr uint32_t LED_PULSE_MS = 100;

elapsedMillis g_led_since;
uint32_t      g_count   = 0;
uint32_t      g_otros   = 0;   // bytes recibidos que NO eran '1' (ruido/baud mal)
}  // namespace

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    Serial.begin(115200);       // USB (debug)
    Serial2.begin(UART_BAUD);   // <- desde DOWN (pin 7 = RX)

    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 1500) { /* esperar USB un toque */ }

    Serial.println();
    Serial.println("[CENTRAL] diag_central_recv1: esperando '1' de DOWN por Serial2 (pin 7).");
    Serial.println("[CENTRAL] Cable: DOWN pin 1 (TX) -> CENTRAL pin 7 (RX) + GND comun.");
}

void loop() {
    while (Serial2.available() > 0) {
        const char c = static_cast<char>(Serial2.read());
        if (c == '1') {
            g_count++;
            digitalWrite(PIN_LED, HIGH);
            g_led_since = 0;
            Serial.print("[CENTRAL] recibi: 1  (#");
            Serial.print(g_count);
            Serial.println(")");
        } else {
            // Si llegan bytes pero NO son '1', casi seguro el baud no coincide
            // o hay ruido. Lo contamos para diagnosticar.
            g_otros++;
            if ((g_otros % 20) == 1) {
                Serial.print("[CENTRAL] OJO: llegan bytes que NO son '1' (revisar baud/cable). total=");
                Serial.println(g_otros);
            }
        }
    }

    if (g_led_since >= LED_PULSE_MS) {
        digitalWrite(PIN_LED, LOW);
    }
}
