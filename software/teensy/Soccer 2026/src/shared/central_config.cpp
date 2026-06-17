// central_config.cpp — implementación. Ver central_config.h para el contrato.
// Espejo de top_config.cpp / calib_storage.cpp (CRC16-CCITT + serialización LE byte
// a byte) + parser de comandos de calibración (SET/GET/SAVE).

#include "central_config.h"
#include <cstring>
#include <cctype>
#include <cstdlib>

namespace iitasoccer {

uint16_t central_config_crc16(const uint8_t* data, int len) {
    if (data == nullptr || len <= 0) return 0xFFFF;
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; ++i) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
            else              crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

namespace {

inline void write_u16_le(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}
inline uint16_t read_u16_le(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0]) | (((uint16_t)p[1]) << 8));
}
inline int16_t clampi16(int v, int lo, int hi) {
    if (v < lo) return (int16_t)lo;
    if (v > hi) return (int16_t)hi;
    return (int16_t)v;
}
bool tok_is_int(const char* t) {
    if (t == nullptr || *t == '\0') return false;
    int i = 0;
    if (t[0] == '-' || t[0] == '+') i = 1;
    if (t[i] == '\0') return false;
    for (; t[i] != '\0'; ++i)
        if (!std::isdigit((unsigned char)t[i])) return false;
    return true;
}

}  // namespace

int16_t central_config_clamp_pwm(int v)  { return clampi16(v, CENTRAL_PWM_MIN,  CENTRAL_PWM_MAX);  }
int16_t central_config_clamp_eff(int v)  { return clampi16(v, CENTRAL_EFF_MIN,  CENTRAL_EFF_MAX);  }
int16_t central_config_clamp_gain(int v) { return clampi16(v, CENTRAL_GAIN_MIN, CENTRAL_GAIN_MAX); }

int central_config_serialize(uint8_t* buf, int cap, const CentralConfig& cfg) {
    if (buf == nullptr || cap < CENTRAL_CONFIG_BLOB_SIZE) return -1;

    std::memset(buf, 0, CENTRAL_CONFIG_BLOB_SIZE);
    buf[0] = CENTRAL_CONFIG_MAGIC;
    buf[1] = CENTRAL_CONFIG_VERSION;

    // Payload (offsets 2..23).
    for (int i = 0; i < 3; ++i) write_u16_le(buf + 2  + i * 2, (uint16_t)cfg.min_pwm[i]);
    for (int i = 0; i < 3; ++i) write_u16_le(buf + 8  + i * 2, (uint16_t)cfg.eff[i]);
    write_u16_le(buf + 14, (uint16_t)cfg.fwd_pwm_l);
    write_u16_le(buf + 16, (uint16_t)cfg.fwd_pwm_r);
    write_u16_le(buf + 18, (uint16_t)cfg.gyro_kp);
    write_u16_le(buf + 20, (uint16_t)cfg.gyro_ki);
    write_u16_le(buf + 22, (uint16_t)cfg.gyro_kd);

    const uint16_t crc = central_config_crc16(buf + 2, CENTRAL_CONFIG_PAYLOAD);
    write_u16_le(buf + 2 + CENTRAL_CONFIG_PAYLOAD, crc);
    return CENTRAL_CONFIG_BLOB_SIZE;
}

bool central_config_deserialize(const uint8_t* buf, int len, CentralConfig& out) {
    if (buf == nullptr || len < CENTRAL_CONFIG_BLOB_SIZE) return false;
    if (buf[0] != CENTRAL_CONFIG_MAGIC)   return false;
    if (buf[1] != CENTRAL_CONFIG_VERSION) return false;

    const uint16_t stored   = read_u16_le(buf + 2 + CENTRAL_CONFIG_PAYLOAD);
    const uint16_t computed = central_config_crc16(buf + 2, CENTRAL_CONFIG_PAYLOAD);
    if (stored != computed) return false;

    for (int i = 0; i < 3; ++i) out.min_pwm[i] = (int16_t)read_u16_le(buf + 2 + i * 2);
    for (int i = 0; i < 3; ++i) out.eff[i]     = (int16_t)read_u16_le(buf + 8 + i * 2);
    out.fwd_pwm_l = (int16_t)read_u16_le(buf + 14);
    out.fwd_pwm_r = (int16_t)read_u16_le(buf + 16);
    out.gyro_kp   = (int16_t)read_u16_le(buf + 18);
    out.gyro_ki   = (int16_t)read_u16_le(buf + 20);
    out.gyro_kd   = (int16_t)read_u16_le(buf + 22);
    return true;
}

// ── Parser de comandos ────────────────────────────────────────────────────────
CcCommand cc_parse_command(const char* s, int len) {
    CcCommand out{CcCmd::NONE, CcParam::NONE, -1, 0, false};
    if (s == nullptr || len <= 0) return out;

    constexpr int MAXT = 5, TLEN = 12;
    char tok[MAXT][TLEN];
    int nt = 0, i = 0;
    while (i < len && nt < MAXT) {
        while (i < len && (std::isspace((unsigned char)s[i]) || s[i] == ',')) ++i;
        if (i >= len) break;
        int k = 0;
        while (i < len && !std::isspace((unsigned char)s[i]) && s[i] != ',') {
            if (k < TLEN - 1) tok[nt][k++] = (char)std::toupper((unsigned char)s[i]);
            ++i;
        }
        tok[nt][k] = '\0';
        ++nt;
    }
    if (nt == 0) return out;

    if (nt == 1 && std::strcmp(tok[0], "GET")  == 0) { out.cmd = CcCmd::GET;  out.valid = true; return out; }
    if (nt == 1 && std::strcmp(tok[0], "SAVE") == 0) { out.cmd = CcCmd::SAVE; out.valid = true; return out; }
    if (std::strcmp(tok[0], "SET") != 0) return out;

    // Indexados: SET <MINPWM|EFF> <idx 0..2> <val>  (4 tokens)
    if (nt == 4) {
        CcParam param = CcParam::NONE;
        if      (std::strcmp(tok[1], "MINPWM") == 0) param = CcParam::MINPWM;
        else if (std::strcmp(tok[1], "EFF")    == 0) param = CcParam::EFF;
        if (param == CcParam::NONE) return out;
        if (!tok_is_int(tok[2]) || !tok_is_int(tok[3])) return out;
        const int idx = std::atoi(tok[2]);
        if (idx < 0 || idx > 2) return out;
        const int raw = std::atoi(tok[3]);
        out.cmd   = CcCmd::SET;
        out.param = param;
        out.idx   = idx;
        out.value = (param == CcParam::MINPWM) ? central_config_clamp_pwm(raw)
                                               : central_config_clamp_eff(raw);
        out.valid = true;
        return out;
    }

    // Escalares: SET <FWDL|FWDR|GKP|GKI|GKD> <val>  (3 tokens)
    if (nt == 3) {
        CcParam param = CcParam::NONE;
        bool is_pwm = false;
        if      (std::strcmp(tok[1], "FWDL") == 0) { param = CcParam::FWDL; is_pwm = true; }
        else if (std::strcmp(tok[1], "FWDR") == 0) { param = CcParam::FWDR; is_pwm = true; }
        else if (std::strcmp(tok[1], "GKP")  == 0) { param = CcParam::GKP; }
        else if (std::strcmp(tok[1], "GKI")  == 0) { param = CcParam::GKI; }
        else if (std::strcmp(tok[1], "GKD")  == 0) { param = CcParam::GKD; }
        if (param == CcParam::NONE) return out;
        if (!tok_is_int(tok[2])) return out;
        const int raw = std::atoi(tok[2]);
        out.cmd   = CcCmd::SET;
        out.param = param;
        out.idx   = -1;
        out.value = is_pwm ? central_config_clamp_pwm(raw) : central_config_clamp_gain(raw);
        out.valid = true;
        return out;
    }

    return out;   // SET mal formado → NONE/invalid
}

}  // namespace iitasoccer
