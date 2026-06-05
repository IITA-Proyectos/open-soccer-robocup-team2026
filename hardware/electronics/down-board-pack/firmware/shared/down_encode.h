// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "types.h"
namespace iitasoccer {
// Serializa LineStatusV2 como frame proto.h (TYPE=LINE_URGENT=0x10).
// Devuelve bytes escritos, 0 si error. Conforme a docs/firmware/CONTRATO-DATOS-DOWN.md.
size_t down_encode_line(const LineStatusV2& s, uint8_t seq,
                         uint8_t* out, size_t out_size);
}  // namespace iitasoccer
