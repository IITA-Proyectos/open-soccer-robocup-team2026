// main_down.cpp — Firmware de la placa DOWN (Teensy 4.0)
//
// Responsabilidad: leer 32 sensores de luz multiplexados + 2 SparkFun OTOS,
// procesar línea y pose, y enviar ese estado al TOP a 100 Hz por Serial5.
//
// Frecuencias de los ticks:
//   • line_ring_tick(): 1 kHz (línea es lo más urgente — el robot la cruza
//     en pocos ms a velocidades altas).
//   • otos_tick(): 100 Hz (suficiente para tracking de posición).
//   • comm_top_send_status(): 100 Hz.
//   • comm_top_tick() (rx): cada loop, no bloquea.
//
// Build:
//   pio run -e down
//
// Flags opcionales (para soportar la placa 04-12 vs fixeada):
//   -DDOWN_NUM_MUXES_CONNECTED=4 → placa fixeada con todos los muxes
//   -DDOWN_NUM_OTOS_CONNECTED=2  → ambos OTOS conectados
//   (defaults conservadores: 1 mux + 1 OTOS — ver config_down.h)

#include <Arduino.h>
#include "config_down.h"
#include "line_ring.h"
#include "line_calib.h"
#include "eeprom_calib.h"
#include "otos.h"
#include "comm_top.h"
#include "comm_central.h"
#ifdef DOWN_DEBUG_TELEMETRY
#include "down_telemetry_serial.h"
#endif

using namespace iitasoccer;

namespace {

elapsedMicros g_since_line_tick;
elapsedMillis g_since_otos_tick;
elapsedMillis g_since_top_send;        // 100 Hz odometría a ARRIBA
elapsedMillis g_since_central_send;    // 100-200 Hz línea a CENTRAL

constexpr uint32_t LINE_URGENT_INTERVAL_MS = 5;  // 200 Hz hacia CENTRAL

// ============================================================
// R2 — WATCHDOG DE HARDWARE (WDOG1), GATEADO (default OFF)
// ============================================================
// Portado del TOP (src/top/main_top.cpp ~l.79-97). Cinturón de seguridad ante
// un cuelgue del bus I2C de los OTOS: otos_tick() hace I2C BLOQUEANTE (~3-4 ms
// los 2 OTOS) cada 10 ms, y el muestreo de línea (path de freno de borde) corre
// a 1 kHz. Si el bus I2C de un OTOS se cuelga (cuelgue indefinido), el loop se
// traba y se CORTA el LINE_URGENT a CENTRAL → el robot deja de frenar en el
// borde. El WDOG1 resetea la placa tras 1 s sin feed, recuperando la cadena.
//
// GATEADO con -DDOWN_ENABLE_WDT (DEFAULT OFF): el binario de competencia NO
// cambia de comportamiento hasta validar en banco que no hay resets espurios.
// needs_bench (igual que el TOP): (1) 0 resets en 30 min de marcha normal con
// los 4 muxes + 2 OTOS activos; (2) auto-reset al colgar el I2C (desconectar un
// OTOS en caliente).
//
// NOTA Teensy 4.0: no existe Wire.setWireTimeout() en el core IMXRT (API de
// AVR/ESP); las transacciones del Wire tienen timeout HW interno, pero un
// cuelgue del bus igual traba el loop. El WATCHDOG es la protección real.
//
// ⚠️ TIMING DE BOOT: el watchdog se arma al FINAL de setup(), DESPUÉS de
// otos_init/calibración (~0.5 s) y line_ring_calibrate_carpet (~0.32 s). Esas
// operaciones lentas de boot NO deben auto-resetear la placa.
#ifdef DOWN_ENABLE_WDT
constexpr uint8_t WDT_WT_FIELD = 1;  // (1+1)*0.5 s = 1.0 s

void watchdog_init_1s() {
    // Una sola escritura al WCR (WT y bits de control solo se fijan una vez tras
    // el reset). WT(1) → 1.0 s; WDE habilita; WDZST suspende en low-power/debug.
    WDOG1_WCR = WDOG_WCR_WT(WDT_WT_FIELD) | WDOG_WCR_WDE | WDOG_WCR_WDZST |
                WDOG_WCR_SRS | WDOG_WCR_WDA;
    asm volatile("dsb");
}

inline void watchdog_feed() {
    // Refresh ("kick"): la secuencia mágica 0x5555 → 0xAAAA reinicia el contador.
    WDOG1_WSR = 0x5555;
    WDOG1_WSR = 0xAAAA;
}
#endif  // DOWN_ENABLE_WDT

}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);  // USB debug
    delay(50);

    line_ring_init();
    Serial.println("[DOWN] line_ring init OK");

    const bool otos_ok = otos_init();
    Serial.print("[DOWN] OTOS: ");
    Serial.print(otos_is_left_ready() ? "L=ok " : "L=- ");
    Serial.print(otos_is_right_ready() ? "R=ok " : "R=- ");
    Serial.println(otos_ok ? "(al menos uno OK)" : "(NINGUNO — degradacion total)");

    comm_top_init();          // Serial5 → ARRIBA (odometría)
    comm_central_init();      // Serial1 → CENTRAL (bus emergencia: línea)
    Serial.println("[DOWN] comm_top + comm_central init OK");

    // Calibración rápida: capturar carpet (asumir robot sobre carpet al encender).
    Serial.println("[DOWN] calibrando carpet... no mover el robot");
    line_ring_calibrate_carpet();
    Serial.println("[DOWN] calibracion carpet OK");

    // Cargar calibración persistida en EEPROM (audit P0.2 — 2026-05-29).
    // Si existe una calib manual previa (carpet+blanco), gana sobre la
    // derivación boot-time porque trae una referencia de BLANCO real. El
    // carpet recién medido arriba se usa solo como fallback si la EEPROM
    // está vacía/inválida (primer arranque o tras ec_erase_calibration()).
    if (comm_central_load_persisted_calib()) {
        Serial.println("[DOWN] calib cargada de EEPROM (persistida)");
        // IMPORTANTE: la linea anterior carga la calib en el DownModel, pero el
        // line_ring (usado por line_ring_get_white y por el centroide
        // cross_track_mm) tiene su PROPIA calib, que arriba quedo solo con el
        // carpet recien medido y el blanco DEFAULT. Cargamos tambien el blanco
        // real de EEPROM en el line_ring para que detecte bien (fix 2026-06).
        SensorCalib cal[NUM_LINE_SENSORS];
        if (ec_load_calibration(cal, NUM_LINE_SENSORS)) {
            uint16_t carpet[NUM_LINE_SENSORS], white[NUM_LINE_SENSORS];
            for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
                carpet[i] = cal[i].carpet;
                white[i]  = cal[i].white;
            }
            line_ring_set_calibration(carpet, white, NUM_LINE_SENSORS);
            Serial.println("[DOWN] line_ring calibrado con blanco de EEPROM");
        }
    } else {
        Serial.println("[DOWN] EEPROM sin calib valida — usando carpet recien medido");
    }

    digitalWrite(PIN_LED_STATUS, HIGH);
    Serial.println("[DOWN] listo: odometria a ARRIBA + linea urgente a CENTRAL");

#ifdef DOWN_ENABLE_WDT
    // R2: armar el watchdog AL FINAL de setup(), DESPUÉS de otos_init (~0.5 s) y
    // line_ring_calibrate_carpet (~0.32 s) — esas operaciones lentas de boot NO
    // deben auto-resetear. A partir de acá, loop() debe alimentarlo cada ciclo
    // (watchdog_feed, primera línea de loop) o la placa se resetea a 1 s.
    watchdog_init_1s();
    Serial.println("[DOWN] watchdog (WDOG1, 1 s) armado");
#endif

#ifdef DOWN_DEBUG_TELEMETRY
    down_telemetry_init();
#endif
}

void loop() {
#ifdef DOWN_ENABLE_WDT
    // R2: alimentar el watchdog como PRIMERA línea del loop. Si el loop se traba
    // (p.ej. cuelgue del bus I2C de un OTOS), no se vuelve a alimentar y el
    // WDOG1 resetea la placa a 1 s, recuperando el LINE_URGENT a CENTRAL.
    watchdog_feed();
#endif

    // RX: drenar ambos UARTs cada loop (no bloquea).
    comm_top_tick();          // comandos desde ARRIBA (legacy, low priority)
    comm_central_tick();      // comandos desde CENTRAL (calib línea, reset OTOS)

    // line_ring: 1 kHz (lectura urgente).
    if (g_since_line_tick >= LINE_TICK_INTERVAL_US) {
        g_since_line_tick = 0;
        line_ring_tick();
    }

    // Send línea urgente a CENTRAL: 200 Hz (Serial1, bus emergencia).
    // Audit 2026-06-03 #24: va ANTES de otos_tick. otos_tick hace I2C bloqueante
    // (~3-4 ms los 2 OTOS); si corriera primero, le agregaría esa latencia al
    // frame de línea, que es el path de SEGURIDAD de borde (lo más sensible al
    // tiempo). El send es no-bloqueante (backpressure en down_tx), así sale ya.
    if (g_since_central_send >= LINE_URGENT_INTERVAL_MS) {
        g_since_central_send = 0;
        comm_central_send_line_urgent();
    }

    // OTOS: 100 Hz. Lectura I2C BLOQUEANTE (~3-4 ms) — por eso va DESPUÉS del
    // send de línea (audit #24). El bloqueo igual le roba ticks al muestreo de
    // 1 kHz del line_ring (mitigación completa = bajar las tx I2C del OTOS, otro
    // bucket); este reorden al menos protege la latencia del frame de línea.
    if (g_since_otos_tick >= OTOS_TICK_INTERVAL_MS) {
        g_since_otos_tick = 0;
        otos_tick();
    }

    // Send odometría a ARRIBA: 100 Hz (Serial5).
    if (g_since_top_send >= COMM_SEND_INTERVAL_MS) {
        g_since_top_send = 0;
        comm_top_send_status();
    }

#ifdef DOWN_DEBUG_TELEMETRY
    down_telemetry_tick();
#endif
}
