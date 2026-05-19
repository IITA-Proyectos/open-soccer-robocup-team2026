#pragma once
#include <stdint.h>
#include "types.h"
namespace iitasoccer {
struct FieldSafety {
    bool    preempt;
    bool    conservative;
    int16_t escape_angle_centideg;
};
FieldSafety fs_eval(const LineStatusV2& s);
}  // namespace iitasoccer
