// tof_zone_mask.h — Aplicación de la MÁSCARA de zonas del ToF (A2.2), PURO host-testeable.
//
// Por qué existe (A2.2 — pedido de Gustavo, anular zonas superiores)
// ----------------------------------------------------------------------------------
// El VL53L7CX entrega una grilla de zonas (4×4 = 16 hoy). Algunas zonas "ven" cosas
// que NO son la pared del piso de juego (la estructura de arriba, el techo, un borde
// del campo por encima) y ensucian la distancia → el robot cree que hay un obstáculo
// o se le corrompe la trilateración. La solución acordada (spec 2026-06-13) es una
// MÁSCARA por-sensor persistida en EEPROM (TopConfig::tof[i].zone_mask): 1 bit por
// zona, 1 = USAR, 0 = ANULAR. El monitor PC pinta las zonas a anular y manda el
// comando; la Teensy lo aplica al leer y lo guarda (CFG SAVE).
//
// Este módulo es SOLO la aritmética de "tomar las zonas crudas + la máscara y reducir
// a una distancia", separada del driver Arduino (sensors_tof.cpp) para poder testearla
// host-native. El glue (leer el VL53L7CX, llenar el array de zonas, pasar la máscara de
// g_top_cfg) vive en sensors_tof.cpp.
//
// FAIL-SAFE / no-op por defecto:
//   • mask = ~0 (todas las zonas activas) → idéntico a promediar todas las zonas válidas
//     (comportamiento actual de mean_valid_zones). Con la EEPROM en blanco el default es
//     ~0 → competencia byte-idéntica.
//   • Si tras enmascarar NO queda ninguna zona válida → devuelve `no_reading` (el sensor
//     no aporta este tick; el wrapper de frescura lo trata como "sin lectura"). Nunca
//     inventa una distancia.
//
// PURO: solo <stdint.h>. El sentinela (no_reading) entra por parámetro para no arrastrar
// Arduino/config_top.h (el caller pasa TOF_NO_READING = 0xFFFF). Compila g++ host.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ¿La zona i está habilitada en la máscara? (bit i = 1 → usar). i fuera de [0,63] → false.
inline bool tof_zone_enabled(uint64_t mask, uint8_t i) {
    if (i >= 64u) return false;
    return ((mask >> i) & 1ull) != 0ull;
}

// tof_zone_masked_mean — promedio de las zonas HABILITADAS (bit de máscara 1) y con
// lectura válida (!= no_reading). Reemplaza al promedio de "todas las válidas" sumándole
// el filtro de la máscara. Devuelve no_reading si no queda ninguna zona habilitada+válida.
//
//   zones_mm   : array de n distancias por zona (cada una ya = mm válido, o no_reading)
//   n          : cantidad de zonas (16 hoy; <=64)
//   mask       : 1 bit por zona, 1 = usar, 0 = anular
//   no_reading : sentinela de "sin lectura" (el caller pasa TOF_NO_READING)
//
// Promedio entero (igual que mean_valid_zones) — suma en uint32 (16 zonas × ~4 m = sin
// overflow). Con mask = ~0 es BYTE-IDÉNTICO a promediar las válidas.
inline uint16_t tof_zone_masked_mean(const uint16_t* zones_mm, uint8_t n,
                                     uint64_t mask, uint16_t no_reading) {
    if (zones_mm == nullptr) return no_reading;
    uint32_t sum = 0;
    uint16_t count = 0;
    const uint8_t lim = (n < 64u) ? n : 64u;
    for (uint8_t i = 0; i < lim; ++i) {
        if (!((mask >> i) & 1ull)) continue;       // zona anulada por máscara
        const uint16_t d = zones_mm[i];
        if (d == no_reading) continue;             // zona sin lectura válida
        sum += d;
        ++count;
    }
    if (count == 0u) return no_reading;            // nada útil tras enmascarar → sin lectura
    return static_cast<uint16_t>(sum / count);
}

// tof_zone_mask_set — helper para construir la máscara desde comandos puntuales
// (TOF n ZONE ON/OFF idx): enciende/apaga el bit `idx`. idx fuera de rango → no-op.
inline uint64_t tof_zone_mask_set(uint64_t mask, uint8_t idx, bool enabled) {
    if (idx >= 64u) return mask;
    const uint64_t bit = (1ull << idx);
    return enabled ? (mask | bit) : (mask & ~bit);
}

}  // namespace iitasoccer
