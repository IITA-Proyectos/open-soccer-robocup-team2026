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
