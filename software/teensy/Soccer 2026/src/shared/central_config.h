// central_config.h — Calibración persistente de la CENTRAL (sin reflashear).
// Módulo PURO (solo <stdint.h>, sin Arduino): serializa/valida un blob con CRC y
// parsea los comandos de calibración (SET/GET/SAVE). El glue con la EEPROM física
// vive en src/central/central_eeprom_config.cpp (Arduino-only) → este es host-testeable.
//
// Hermano de top_config.h (TOP) y calib_storage (DOWN): mismo patrón de 3 capas
// (puro acá + glue EEPROM + carga al boot). Spec: docs/firmware/PLAN-MONITOR-Y-
// CALIBRACION-CENTRAL.md (Fase 2).
//
// QUÉ guarda (calibrable EN BANCO desde el monitor USB, sin reflashear):
//   • min_pwm[3]  — piso de PWM de arranque POR MOTOR. La potencia mínima para que la
//                   rueda rompa la inercia. Distinta por rueda (las delanteras van
//                   oblicuas = más fricción). 0=del-izq · 1=del-der · 2=trasera.
//   • eff[3]      — eficiencia (×100) por rueda. Corrige la relación de velocidad en el
//                   STRAFE lateral (la trasera alineada rinde más por PWM).
//   • fwd_pwm_l/r — PWM de la rueda izquierda y derecha en AVANCE RECTO. Sirve para que
//                   el robot avance derecho cuando un motor anda distinto al otro.
//   • gyro_kp/ki/kd — ganancias del PID de rumbo con GIROSCOPIO (×1000). El robot deja
//                   de oscilar/serpentear ajustando estas.
//
// Los valores compilados (config_central.h / strategy.cpp, POR ROBOT) son los DEFAULTS;
// la EEPROM es un OVERRIDE opcional. EEPROM en blanco/corrupta → CRC falla → el firmware
// usa los constexpr (competencia byte-idéntica).
//
// ⚠️ Lo que NO va acá (es LÓGICA, se cambia en el programa, no es "un valor"):
//   la cinemática (WHEEL_ANGLES_DEG), el sentido de los motores (MOTOR_INVERT), el signo
//   del giro (OMEGA_SIGN), la estructura de la FSM. Esos no son calibrables por EEPROM.
//
// Formato del blob v1 (26 bytes), byte-a-byte little-endian (NO memcpy del struct):
//   off  bytes  campo
//   ---  -----  --------------------------------------------------
//     0      1  magic = CENTRAL_CONFIG_MAGIC (0x7A)
//     1      1  version = CENTRAL_CONFIG_VERSION (1)
//     2      6  min_pwm[3]  i16 LE                              ← payload (2..23)
//     8      6  eff[3]      i16 LE
//    14      2  fwd_pwm_l   i16 LE
//    16      2  fwd_pwm_r   i16 LE
//    18      2  gyro_kp     i16 LE (×1000)
//    20      2  gyro_ki     i16 LE (×1000)
//    22      2  gyro_kd     i16 LE (×1000)
//    24      2  CRC16-CCITT del payload (offsets 2..23)

#pragma once
#include <stdint.h>

namespace iitasoccer {

constexpr uint8_t CENTRAL_CONFIG_MAGIC   = 0x7A;
constexpr uint8_t CENTRAL_CONFIG_VERSION = 1;

// Rangos seguros (espejo de GUIA-DE-TUNING-CENTRAL): PWM NUNCA > 149 (motores brushed
// 5V a 7.4V se queman > ~70%); eff (×100) en [50,200]; ganancia PID (×1000) en [0,32000].
constexpr int16_t CENTRAL_PWM_MIN  = 0;
constexpr int16_t CENTRAL_PWM_MAX  = 149;
constexpr int16_t CENTRAL_EFF_MIN  = 50;
constexpr int16_t CENTRAL_EFF_MAX  = 200;
constexpr int16_t CENTRAL_GAIN_MIN = 0;
constexpr int16_t CENTRAL_GAIN_MAX = 32000;

struct CentralConfig {
    int16_t min_pwm[3];   // piso PWM por motor: 0=del-izq · 1=del-der · 2=trasera
    int16_t eff[3];       // eficiencia ×100 por rueda (corrige strafe lateral)
    int16_t fwd_pwm_l;    // PWM rueda IZQUIERDA en avance recto
    int16_t fwd_pwm_r;    // PWM rueda DERECHA en avance recto
    int16_t gyro_kp;      // PID rumbo con giroscopio: kp ×1000
    int16_t gyro_ki;      // ki ×1000
    int16_t gyro_kd;      // kd ×1000
};

constexpr int CENTRAL_CONFIG_PAYLOAD   = 11 * 2;                    // 11 int16 = 22
constexpr int CENTRAL_CONFIG_BLOB_SIZE = 2 + CENTRAL_CONFIG_PAYLOAD + 2;  // 26

// Serializa cfg → buf (LE byte a byte + CRC). Retorna BLOB_SIZE o -1 si buf chico / args inválidos.
int central_config_serialize(uint8_t* buf, int cap, const CentralConfig& cfg);

// Deserializa buf → out. Valida magic + version (EXACTO) + CRC. Si TODO OK llena out y
// retorna true; si cualquier validación falla NO toca out y retorna false. EEPROM en
// blanco (0xFF) → CRC falla → false.
bool central_config_deserialize(const uint8_t* buf, int len, CentralConfig& out);

// CRC-16/CCITT-FALSE (0x1021, init 0xFFFF). Igual que top_config_crc16 / cs_crc16_ccitt.
uint16_t central_config_crc16(const uint8_t* data, int len);

// Clamps de seguridad por tipo de parámetro.
int16_t central_config_clamp_pwm(int v);    // [0,149]   (min_pwm, fwd_pwm)
int16_t central_config_clamp_eff(int v);    // [50,200]  (eff ×100)
int16_t central_config_clamp_gain(int v);   // [0,32000] (PID ×1000)

// ── Comandos de calibración ───────────────────────────────────────────────────
// Indexados (idx 0..2):  "SET MINPWM <idx> <val>" · "SET EFF <idx> <val>"
// Escalares (sin idx):   "SET FWDL <val>" · "SET FWDR <val>" ·
//                        "SET GKP <val>" · "SET GKI <val>" · "SET GKD <val>"
// Sin valor:             "GET" · "SAVE"
enum class CcCmd   : uint8_t { NONE, SET, GET, SAVE };
enum class CcParam : uint8_t { NONE, MINPWM, EFF, FWDL, FWDR, GKP, GKI, GKD };

struct CcCommand {
    CcCmd   cmd;
    CcParam param;   // sólo relevante para SET
    int     idx;     // 0..2 para MINPWM/EFF; -1 para los escalares
    int     value;   // YA CLAMPEADO según el parámetro, para SET
    bool    valid;   // true sólo si el comando está bien formado
};

// Parsea una línea (case-insensitive). idx fuera de [0,2], param desconocido, o cantidad
// de tokens incorrecta → valid=false. Texto no reconocido (PING/STREAM/basura) → NONE/invalid.
CcCommand cc_parse_command(const char* s, int len);

}  // namespace iitasoccer
