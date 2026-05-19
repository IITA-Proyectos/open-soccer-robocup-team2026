#include "field_safety.h"
namespace iitasoccer {
FieldSafety fs_eval(const LineStatusV2& s){
    FieldSafety f{}; f.preempt=false; f.conservative=false;
    f.escape_angle_centideg = LSV2_NA_I16;
    if (s.data_valid == 0){ f.conservative = true; return f; }
    bool imminent = (s.event_flags & EV_IMMINENT_EXIT) != 0;
    if ((s.line_present && s.escape_angle_centideg != LSV2_NA_I16) || imminent){
        f.preempt = true;
        f.escape_angle_centideg = s.escape_angle_centideg;
    }
    return f;
}
}  // namespace iitasoccer
