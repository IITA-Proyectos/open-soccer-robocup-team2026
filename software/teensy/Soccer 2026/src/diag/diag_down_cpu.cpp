// diag_down_cpu.cpp — ¿el anillo de línea sostiene 1 kHz? + carga por tick (banco)
//
// Para qué sirve:
//   El muestreo del anillo de 32 sensores es el path de SEGURIDAD de borde y se
//   asume a ~1 kHz. Este sketch lo MIDE en vivo: corre line_ring_tick() lo más
//   rápido posible y cada 500 ms reporta la TASA real (Hz) y la DURACIÓN del
//   último tick (us = carga por muestreo). Sirve para:
//     • confirmar que hay headroom para 1 kHz (tasa >> 1000),
//     • cuantificar cuánto cuesta un tick,
//     • comparar el robo de CPU del OTOS: flashealo build con OTOS y sin OTOS
//       (env down vs down_robot2 → DOWN_NUM_OTOS_CONNECTED=2 vs 0) y mirá si la
//       tasa baja. (Este diag NO tickea OTOS; mide el anillo puro. El robo real
//       en operación se ve en el firmware vivo, que interleavea otos_tick.)
//
//   ⚠️ Herramienta de BANCO. Reusa la API pública de line_ring.h. NO es el
//   firmware de competencia (ese es src/down/main_down.cpp).
//
// Build / flash:
//   pio run -e diag_down_cpu -t upload      (anillo puro)
//   pio device monitor -b 115200
//
// Atribución:
//   Author: Claude Opus 4.8 (1M context) (Anthropic)
//   Requested-by: Gustavo Viollaz (@gviollaz)

#include <Arduino.h>
#include "config_down.h"
#include "line_ring.h"

using namespace iitasoccer;

namespace {
elapsedMillis g_since_print;
uint32_t      g_last_tick_count = 0;
uint32_t      g_window_start_ms = 0;
}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* esperar al monitor, máx 3 s */ }

    Serial.println("\n==================================================");
    Serial.println("  DIAG DOWN CPU — tasa de muestreo del anillo (1 kHz?)");
    Serial.println("  (herramienta de banco, NO es competencia)");
    Serial.println("==================================================");
    Serial.print("Sensores de luz : "); Serial.println(NUM_LINE_SENSORS);
    Serial.print("OTOS compilados : "); Serial.println(NUM_OTOS);
    Serial.println("Corriendo line_ring_tick() a full. Reporte cada 500 ms:");
    Serial.println("  rate_Hz = muestreos/seg (objetivo: >> 1000)  |  last_tick_us = costo del ultimo tick");
    Serial.println();

    line_ring_init();
    Serial.println("[diag] line_ring init OK\n");

    g_last_tick_count = line_ring_get_tick_count();
    g_window_start_ms = millis();
    g_since_print = 0;
}

void loop() {
    // Muestreo a full: cada vuelta hace un tick del anillo (incrementa tick_count).
    line_ring_tick();

    if (g_since_print >= 500) {
        const uint32_t now      = millis();
        const uint32_t count    = line_ring_get_tick_count();
        const uint32_t dcount   = count - g_last_tick_count;
        const uint32_t dms      = now - g_window_start_ms;
        const float    rate_hz  = (dms > 0) ? (dcount * 1000.0f / dms) : 0.0f;
        const uint32_t last_us  = line_ring_get_last_tick_us();

        Serial.print("[DOWN-CPU] rate_Hz=");
        Serial.print(rate_hz, 0);
        Serial.print("  last_tick_us=");
        Serial.print(last_us);
        Serial.print("  (ticks=");
        Serial.print(dcount);
        Serial.print(" en ");
        Serial.print(dms);
        Serial.println(" ms)");

        digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));  // heartbeat
        g_last_tick_count = count;
        g_window_start_ms = now;
        g_since_print = 0;
    }
}
