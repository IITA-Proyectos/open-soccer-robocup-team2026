// diag_top_ultrasonic.cpp — Verifica el HC-SR04 (ultrasonido frontal) de la placa TOP.
//
// NO es firmware de competencia. Standalone: pulsa TRIG, mide ECHO con pulseIn,
// imprime distancia + salud por USB. Para confirmar EN BANCO que el HC-SR04 mide.
//
// v2 (2026-05-31): MODO DIAGNÓSTICO. Como el sensor daba "eco≈3µs" (sin eco real),
//   este diag ahora prueba LAS DOS orientaciones de pin cada ciclo y muestra el
//   estado idle de cada pin, para distinguir 3 causas:
//     • pines swapeados → una orientación mide y la otra no.
//     • sensor no emite (alimentación 3.3V / sin GND) → NINGUNA orientación mide.
//     • idle de ECHO pegado en HIGH → cableado raro / no es el pin del ECHO.
//
// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ ⚠️  HARDWARE — LEER                                                         ║
// ║ 1) El HC-SR04 CLÁSICO necesita Vcc = 5 V para emitir el ping ultrasónico.   ║
// ║    A 3.3 V muchas veces NO dispara → no hay eco (síntoma "eco≈3µs/0 cm").    ║
// ║    Excepción: HC-SR04+ / RCWL-1601 sí andan a 3.3 V.                        ║
// ║ 2) El pin ECHO a 5 V DAÑA el Teensy 4.0 (máx 3.3 V). Si alimentás a 5 V,    ║
// ║    poné divisor en ECHO (1kΩ + 2kΩ → 3.3 V). NO bajes Vcc a 3.3 V para      ║
// ║    "arreglar" lo del nivel — eso apaga el sensor. Vcc=5V + divisor en ECHO. ║
// ╚══════════════════════════════════════════════════════════════════════════╝
//
// Pines (pinout_common.h): TRIG = pin 4, ECHO = pin 3. + VCC y GND comunes.
// Este diag igual prueba la orientación invertida (T3/E4) por si están al revés.
//
// Uso:
//   pio run -e diag_top_ultrasonic -t upload
//   pio device monitor -b 115200
//
// Atribución: Claude Opus (Anthropic), vía Claude Code. Pedido: Gustavo Viollaz.

#include <Arduino.h>

constexpr int      PIN_A           = 4;          // "TRIG" según pinout_common.h
constexpr int      PIN_B           = 3;          // "ECHO" según pinout_common.h
constexpr long     USB_BAUD        = 115200;
constexpr uint32_t PERIOD_MS       = 160;        // ~6 Hz (2 pulsos/ciclo, gap >60 ms c/u)
constexpr uint32_t ECHO_TIMEOUT_US = 30000UL;    // ~5 m ida+vuelta
constexpr float    US_PER_CM       = 58.0f;      // 343 m/s → ~58 µs por cm (ida+vuelta)
constexpr uint32_t ECO_MIN_REAL_US = 100UL;      // <100 µs (~1.7 cm) lo tratamos como glitch/no-eco

uint32_t g_ciclos = 0;
uint32_t g_real_A = 0;   // lecturas plausibles con orientación T4/E3
uint32_t g_real_B = 0;   // lecturas plausibles con orientación T3/E4

// Dispara TRIG (10 µs) en `trig` y mide el ancho del pulso ECHO en `echo`. µs, o 0 si timeout.
uint32_t probe(int trig, int echo) {
    pinMode(trig, OUTPUT);
    pinMode(echo, INPUT);
    digitalWrite(trig, LOW);
    delayMicroseconds(3);
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);
    return pulseIn(echo, HIGH, ECHO_TIMEOUT_US);
}

void print_col(const char* tag, uint32_t eco) {
    Serial.print(tag);
    if (eco == 0) {
        Serial.print("sin eco       ");
    } else if (eco < ECO_MIN_REAL_US) {
        Serial.print("glitch ");  Serial.print(eco);  Serial.print("us     ");
    } else {
        const float cm = eco / US_PER_CM;
        Serial.print(eco);  Serial.print("us=");  Serial.print(cm, 1);  Serial.print("cm  ");
    }
}

void setup() {
    Serial.begin(USB_BAUD);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* esperar host hasta 2 s */ }

    Serial.println();
    Serial.println("=============================================================");
    Serial.println(" diag_top_ultrasonic v2 — HC-SR04 (prueba T4/E3 y T3/E4)");
    Serial.print  (" build: "); Serial.print(__DATE__); Serial.print(' '); Serial.println(__TIME__);
    Serial.println(" dist_cm = eco_us / 58. <100us = glitch (no es eco real).");
    Serial.println(" Mové la mano frente al sensor (5-50 cm) y mirá QUE COLUMNA varía:");
    Serial.println("   - varía T4/E3  -> pinout OK (TRIG=4, ECHO=3).");
    Serial.println("   - varía T3/E4  -> pines INVERTIDOS (swapealos o avisame).");
    Serial.println("   - NINGUNA varía -> el sensor no emite: Vcc=5V? GND comun? (ver cabecera).");
    Serial.println(" idle[p4 p3] = nivel en reposo de cada pin (ECHO sano idlea en 0).");
    Serial.println("=============================================================");
}

void loop() {
    static uint32_t t_last = 0;
    const uint32_t now = millis();
    if (now - t_last < PERIOD_MS) return;
    t_last = now;

    // Estado idle de ambos pines (los dos como entrada, sin disparar nada).
    pinMode(PIN_A, INPUT);
    pinMode(PIN_B, INPUT);
    delayMicroseconds(50);
    const int idle_a = digitalRead(PIN_A);
    const int idle_b = digitalRead(PIN_B);

    const uint32_t eco_A = probe(PIN_A, PIN_B);   // TRIG=4 / ECHO=3 (pinout actual)
    delay(60);                                    // HC-SR04 necesita gap entre pulsos
    const uint32_t eco_B = probe(PIN_B, PIN_A);   // TRIG=3 / ECHO=4 (invertido)

    g_ciclos++;
    if (eco_A >= ECO_MIN_REAL_US) g_real_A++;
    if (eco_B >= ECO_MIN_REAL_US) g_real_B++;

    Serial.print("idle[p4="); Serial.print(idle_a);
    Serial.print(" p3=");     Serial.print(idle_b);  Serial.print("]  ");
    print_col("T4/E3: ", eco_A);
    Serial.print("| ");
    print_col("T3/E4: ", eco_B);
    Serial.print(" reales A/B=");  Serial.print(g_real_A);
    Serial.print("/");             Serial.print(g_real_B);
    Serial.print(" de ");          Serial.print(g_ciclos);
    Serial.println();
}
