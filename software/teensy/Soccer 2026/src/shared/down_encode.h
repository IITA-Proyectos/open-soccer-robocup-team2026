#pragma once
#include <stdint.h>
#include <stddef.h>
#include "types.h"
namespace iitasoccer {
// Serializa LineStatusV2 como frame proto.h (TYPE=LINE_URGENT=0x10).
// Devuelve bytes escritos, 0 si error. Conforme a docs/firmware/CONTRATO-DATOS-DOWN.md.
size_t down_encode_line(const LineStatusV2& s, uint8_t seq,
                         uint8_t* out, size_t out_size);

// Edad (ms) de la muestra física para el campo LineStatusV2.sample_age_ms
// (contrato §3.5, detección de datos stale en el receptor).
//   now_us    = micros() actual.
//   sample_us = micros() del ÚLTIMO muestreo (timestamp, NO la duración del tick).
// Resta unsigned (maneja el wrap de micros() a ~71.6 min) y clampea a 0..255.
// PURA: sin Arduino, host-testeable. El glue (comm_central) la llama con
// (micros(), line_ring_get_last_sample_us()). Ver audit 2026-06-03 #2.
uint8_t lsv2_sample_age_ms(uint32_t now_us, uint32_t sample_us);
}  // namespace iitasoccer
