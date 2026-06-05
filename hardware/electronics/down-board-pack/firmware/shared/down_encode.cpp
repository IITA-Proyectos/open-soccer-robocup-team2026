// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#include "down_encode.h"
#include "proto.h"
#include <string.h>
namespace iitasoccer {
size_t down_encode_line(const LineStatusV2& s, uint8_t seq,
                         uint8_t* out, size_t out_size){
    Frame f{};
    f.type = MsgType::LINE_URGENT;
    f.seq = seq;
    f.payload_len = sizeof(LineStatusV2);   // 16
    memcpy(f.payload, &s, sizeof(LineStatusV2));
    return proto_encode(f, out, out_size);
}
}  // namespace iitasoccer
