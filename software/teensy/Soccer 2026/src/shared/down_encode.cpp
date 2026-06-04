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

uint8_t lsv2_sample_age_ms(uint32_t now_us, uint32_t sample_us){
    // Resta unsigned: si now_us < sample_us por wrap de micros(), el módulo de
    // 2^32 da el delta real (siempre que el tick sea < ~71.6 min, garantizado).
    const uint32_t age_ms = (now_us - sample_us) / 1000u;
    return (age_ms > 255u) ? 255u : static_cast<uint8_t>(age_ms);
}
}  // namespace iitasoccer
