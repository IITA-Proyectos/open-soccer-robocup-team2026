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
