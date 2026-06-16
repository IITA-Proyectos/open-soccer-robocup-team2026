// sensors_tof.h — 4 ToF (VL53L7CX) + 1 HC-SR04 ultrasonido.
//
// Hardware (estado vivo, banco 2026-05-30 — supera el esquema viejo de abajo):
//   • Los 4 ToF VL53L7CX cuelgan del bus ÚNICO Wire (I2C0, 18/19) y enumeran a
//     0x2A..0x2D vía las patas LP {9,10,11,12} (bodge de Enzo; TOP_ENABLE_MULTI_TOF).
//   • Wire2 (24/25, LPI2C4) — en R1 libre; en R2 es el bus del 2º BNO PRIMARIO.
//     (Corrección 2026-06-09: 24/25 = Wire2, NO "Wire1"; Wire1 real = 16/17.)
//   • HC-SR04 → TRIG=pin 4, ECHO=pin 3 (NO 6/7; ver pinout_common.h).
//   ⚠️ Probar SIEMPRE con power-cycle (las direcciones I2C de los VL53L7CX persisten).
//   Lib usada: Adafruit_VL53L7CX (la STM32duino tiene bug en Teensy 4.0).
//
// (Histórico previo al bodge 2026-05-30: U2/U3 en Wire; U5/U17 en Wire1 24/25;
//  HC-SR04 en TRIG6/ECHO7; solo U2 instalado. Superado — no usar.)
//
// API publica estable: no cambio con la migracion stub -> Adafruit.

#pragma once
#include <stdint.h>
// config_top.h arrastra Arduino (config de pines/HAL). El test host de la lógica
// PURA de frescura (P1-TOF-STALE) define TOF_PURE_HOST_TEST para incluir solo la
// función pura sin Arduino. El firmware real NUNCA define ese macro -> incluye
// config_top.h como siempre.
#ifndef TOF_PURE_HOST_TEST
#include "config_top.h"
#endif

namespace iitasoccer {

constexpr uint16_t TOF_NO_READING = 0xFFFF;  // sentinel "no leído"
constexpr uint16_t TOF_MAX_RANGE_MM = 4000;  // ~4m según datasheet VL53L7CX

// ----------------------------------------------------------------------------
// STALE TIMEOUT (P1-TOF-STALE, 2026-06-03)
// ----------------------------------------------------------------------------
// Antes: si getRangingData() fallaba, sensors_tof_tick mantenía el último valor
// SIN límite de tiempo. Un ToF colgado en 80 mm se propagaba para siempre como
// min_obstacle -> CENTRAL frenaba/evadía contra un fantasma toda la partida
// (falla silenciosa y persistente). Ahora cada sensor lleva una marca de
// frescura (last_ok_ms); tras TOF_STALE_TIMEOUT_MS sin lectura buena, el getter
// devuelve TOF_NO_READING (igual que un sensor ausente) en vez del valor viejo.
//
// El timeout es HOLGADO a propósito: el ranging corre a 15 Hz (~66 ms/frame) y
// un frame perdido aislado es normal, así que un timeout corto borraría datos
// buenos. ~250 ms = ~3-4 frames perdidos seguidos = sensor realmente colgado,
// no jitter. Mantiene la preferencia "último dato bueno" para 1 frame perdido.
constexpr uint32_t TOF_STALE_TIMEOUT_MS = 250;

// ----------------------------------------------------------------------------
// LÓGICA PURA host-testeable (sin Arduino): decide si un valor cacheado sigue
// fresco. El glue (millis(), el array de sensores) queda en sensors_tof.cpp.
//
//   cached       = último valor mm cacheado (o TOF_NO_READING si nunca leyó OK)
//   last_ok_ms   = millis() de la última lectura BUENA (sólo válido si ever_ok)
//   ever_ok      = ¿hubo alguna vez una lectura buena? (sin esto, no hay base)
//   now_ms       = millis() actual
//   timeout_ms   = ventana de frescura
//
// Devuelve `cached` si fresco; TOF_NO_READING si vencido o nunca leyó OK.
// WRAP-SAFE: usa resta unsigned (now - last_ok) — el patrón estándar de millis();
// si now retrocede (wrap a ~49.7 días) la resta unsigned sigue dando el elapsed
// correcto y no dispara stale espurio. (Mismo criterio que sensor_health.)
inline uint16_t tof_fresh_or_no_reading(uint16_t cached,
                                        uint32_t last_ok_ms,
                                        bool     ever_ok,
                                        uint32_t now_ms,
                                        uint32_t timeout_ms) {
    if (!ever_ok) return TOF_NO_READING;             // nunca tuvo dato bueno
    if (cached == TOF_NO_READING) return TOF_NO_READING;
    const uint32_t elapsed = now_ms - last_ok_ms;    // unsigned -> wrap-safe
    if (elapsed > timeout_ms) return TOF_NO_READING;  // vencido -> sentinel
    return cached;                                    // fresco -> dato bueno
}

bool sensors_tof_init();

// Duerme los 4 ToF (LP low) para dejar el bus I2C limpio ANTES de iniciar el BNO
// (los VL53L7CX arrancan en su dir de FÁBRICA 0x29 y ensucian el bus si se enumeran a
// destiempo). Llamar desde setup() ANTES de sensors_imu_init(). No-op si no esta
// TOP_ENABLE_MULTI_TOF.
void sensors_tof_predim_lp();

// DIAG (bring-up): escanea el bus Wire (0x08..0x77) e imprime las direcciones que
// responden. Llamar con los ToF DORMIDOS (post predim, pre enumeracion) para ver SOLO
// el BNO secundario de este bus (debe responder en 0x28; ya NO hay ningún BNO en 0x29).
void sensors_tof_scan_wire();

// Lee ToFs disponibles (no bloqueante — algunas lecturas son asincrónicas).
// Llamar a ~30 Hz desde el loop.
void sensors_tof_tick();

// Distancia en mm del ToF idx (0..NUM_TOF-1), o TOF_NO_READING si no listo.
uint16_t sensors_tof_get_distance_mm(uint8_t idx);

// Zona cruda (0..15, grilla 4x4) del ToF idx en mm, o TOF_NO_READING si la zona
// no tiene lectura válida / el sensor está deshabilitado o vencido. Para exponer
// la grilla de zonas por telemetría (campo "z" del monitor TOP).
uint16_t sensors_tof_get_zone_mm(uint8_t idx, uint8_t zone);

// Distancia del HC-SR04 frontal en mm.
uint16_t sensors_hcsr04_get_distance_mm();

// Distancia mínima entre los 4 ToF + HC-SR04 (útil para evasión genérica).
uint16_t sensors_tof_get_min_distance_mm();

// Diagnóstico:
bool sensors_tof_is_ready(uint8_t idx);
uint32_t sensors_tof_get_tick_count();

}  // namespace iitasoccer
