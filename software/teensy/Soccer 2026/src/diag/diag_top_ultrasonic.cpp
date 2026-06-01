// diag_top_ultrasonic.cpp — Verifica el HC-SR04 (ultrasonido frontal) de la placa TOP.
//
// NO es firmware de competencia. Standalone: sólo pulsa TRIG, mide ECHO con pulseIn,
// e imprime distancia + salud del enlace por USB. Sirve para confirmar EN BANCO que
// el HC-SR04 mide bien antes de integrarlo al firmware vivo.
//
// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ ⚠️  HARDWARE — LEER ANTES DE ALIMENTAR                                      ║
// ║ El HC-SR04 alimentado a 5 V saca el pulso ECHO a 5 V. Los pines del Teensy ║
// ║ 4.0 NO toleran 5 V (máx 3.3 V) → conectar ECHO 5 V directo al pin 7 puede   ║
// ║ DAÑAR el micro. Antes de cablear ECHO al pin 7, asegurá UNO de estos:       ║
// ║   • divisor de nivel en ECHO (ej. 1 kΩ + 2 kΩ → 3.3 V), o                   ║
// ║   • alimentar el HC-SR04 a 3.3 V (ECHO sale ~3.3 V, seguro; pierde alcance). ║
// ╚══════════════════════════════════════════════════════════════════════════╝
//
// Pines (pinout_common.h §2.4): TRIG = pin 6, ECHO = pin 7. + VCC y GND comunes.
//
// Uso:
//   pio run -e diag_top_ultrasonic -t upload
//   pio device monitor -b 115200
//
// Atribución: Claude Opus (Anthropic), vía Claude Code. Pedido: Gustavo Viollaz.

#include <Arduino.h>

constexpr int      PIN_TRIG        = 6;          // = PIN_HCSR04_TRIG (pinout_common.h)
constexpr int      PIN_ECHO        = 7;          // = PIN_HCSR04_ECHO (pinout_common.h)
constexpr long     USB_BAUD        = 115200;
constexpr uint32_t PERIOD_MS       = 100;        // ~10 Hz (HC-SR04 quiere >60 ms entre pulsos)
constexpr uint32_t ECHO_TIMEOUT_US = 30000UL;    // ~5 m ida+vuelta (el HC-SR04 llega ~4 m)
constexpr float    US_PER_CM       = 58.0f;      // 343 m/s → ~58 µs por cm (ida+vuelta)

uint32_t g_lecturas = 0;
uint32_t g_validas  = 0;
uint32_t g_timeouts = 0;

// Dispara un pulso TRIG de 10 µs y mide el ancho del ECHO. Devuelve µs, o 0 si timeout.
uint32_t medir_eco_us() {
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(3);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);
    return pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
}

void setup() {
    Serial.begin(USB_BAUD);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* esperar host hasta 2 s */ }

    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    digitalWrite(PIN_TRIG, LOW);

    Serial.println();
    Serial.println("=============================================================");
    Serial.println(" diag_top_ultrasonic — HC-SR04 frontal (TRIG=pin 6, ECHO=pin 7)");
    Serial.print  (" build: "); Serial.print(__DATE__); Serial.print(' '); Serial.println(__TIME__);
    Serial.println(" Baud 115200. ~10 Hz. dist_cm = eco_us / 58.");
    Serial.println(" [sin eco] = timeout (fuera de rango / cableado / sin energia).");
    Serial.println(" ATENCION: ECHO a 5V dana el pin 7 del Teensy — ver cabecera del .cpp.");
    Serial.println("=============================================================");
}

void loop() {
    static uint32_t t_last = 0;
    const uint32_t now = millis();
    if (now - t_last < PERIOD_MS) return;
    t_last = now;

    const uint32_t eco = medir_eco_us();
    g_lecturas++;

    if (eco == 0) {
        g_timeouts++;
        Serial.print("[sin eco]  validas=");  Serial.print(g_validas);
        Serial.print(" timeouts=");           Serial.print(g_timeouts);
        Serial.print("/");                    Serial.println(g_lecturas);
        return;
    }

    g_validas++;
    const float cm = eco / US_PER_CM;
    Serial.print("[OK] eco=");  Serial.print(eco);    Serial.print(" us");
    Serial.print("  dist=");    Serial.print(cm, 1);  Serial.print(" cm   ");

    // Barra visual: 1 char por cada 2 cm, cap a 50 (≈100 cm). Acercá/alejá la mano y mirá.
    int barras = static_cast<int>(cm / 2.0f);
    if (barras > 50) barras = 50;
    for (int i = 0; i < barras; i++) Serial.print('#');
    Serial.println();
}
