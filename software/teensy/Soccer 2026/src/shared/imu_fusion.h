// imu_fusion.h — Fusión inteligente de 2 BNO055 para heading robusto.
//
// Módulo PURO host-testeable (sin Arduino.h). Toda la I/O real (leer los BNO,
// resetearlos, EEPROM) vive en la capa hardware sensors_imu.cpp, que le pasa
// muestras crudas a este módulo y ejecuta las acciones que el módulo pide.
//
// QUÉ HACE (Capa 1 — aprovechar 2 sensores):
//   1. Clasifica la SALUD de cada sensor: OK / DEGRADED / DEAD.
//   2. Rechaza GLITCHES/IMPACTOS: si el heading salta más de lo que su propio
//      giroscopio justifica (golpe mecánico, glitch I2C), descarta ese ciclo.
//   3. FUSIÓN ponderada por calidad: promedio CIRCULAR (maneja wraparound ±180)
//      pesado por calibración y salud. 2 sanos => menos ruido.
//   4. FAILOVER: si uno muere, usa el otro de forma transparente.
//   5. DETECCIÓN de drift en reposo: con el robot quieto el heading NO debería
//      cambiar; lo que cambie es drift. Si un sensor driftea feo y sostenido,
//      pide RESET de ESE sensor y da el heading del sano para re-sembrarlo.
//   6. CONFIG POR SENSOR (offset de montaje, calib mínima, peso) — un BNO puede
//      estar montado rotado o ser de otra calidad que el otro.
//
// QUÉ NO HACE (eso es Capa 3, necesita referencia ABSOLUTA externa):
//   No elimina el drift de yaw de fondo. Dos giroscopios que derivan, promediados,
//   siguen derivando. Para anular el drift de yaw hace falta una referencia
//   absoluta (paredes vía ToF, arcos por cámara) — complementary filter futuro.
//   Este módulo DETECTA drift y da redundancia; no inventa un norte que no existe.
//
// CONVENCIÓN: heading en grados, CCW-positivo (IZQUIERDA sube), rango [-180,180].
// Idéntica al resto del firmware (localization.cpp, TOF_MOUNT_ANGLE_DEG).

#pragma once
#include <stdint.h>

namespace iitasoccer {

constexpr int IMU_FUSION_N = 2;   // LEFT(0) + RIGHT(1)

enum class ImuHealth : uint8_t { DEAD = 0, DEGRADED = 1, OK = 2 };

// Config PERSISTENTE por sensor (la capa hardware la guarda/lee de EEPROM).
struct ImuSensorCfg {
    bool    enabled;            // false = ignorar este sensor por completo
    float   mount_offset_deg;   // corrección si el chip está montado rotado
    uint8_t min_calib_gyro;     // calib gyro mínima (0..3) para considerarlo OK
    float   base_weight;        // peso relativo base en la fusión (default 1.0)
};

// Parámetros globales de la fusión.
struct ImuFusionCfg {
    float    disagree_max_deg;   // > esto entre los 2 => NO promediar (impacto/falla)
    float    rest_gyro_dps;      // |gyroZ| de ambos < esto => robot quieto
    float    glitch_factor;      // salto > factor*|gyro*dt| + margen => glitch
    float    glitch_margin_deg;  // margen fijo del test de glitch
    uint8_t  glitch_max_streak;  // si glitchea esto seguido, ACEPTA (era real)
    float    drift_reset_dps;    // drift en reposo > esto (sostenido) => reset
    uint32_t drift_reset_ms;     // tiempo sostenido de drift feo => request_reset
    uint16_t dead_after_misses;  // ciclos sin 'present' => DEAD
    float    degraded_weight;    // factor de peso de un sensor DEGRADED (ej 0.25)
};

// Defaults sensatos (la capa hardware los puede pisar desde EEPROM).
ImuFusionCfg imu_fusion_default_cfg();
ImuSensorCfg imu_fusion_default_sensor_cfg();

// Muestra cruda de un sensor en un tick (la arma la capa hardware).
struct ImuSample {
    bool    present;       // ACK en el bus este ciclo (false = desconectado)
    float   heading_deg;   // yaw CCW+, ya relativo al cero, SIN mount_offset
    float   gyro_z_dps;    // velocidad yaw deg/s, CCW+
    uint8_t calib_gyro;    // 0..3 (registro de calibración del BNO)
};

// Estado por sensor (persiste entre ticks; lo maneja el módulo).
struct ImuSensorState {
    ImuHealth health;
    float     heading_deg;       // último heading bueno (con mount_offset aplicado)
    float     drift_dps;         // EMA del wander en reposo (°/s) — vs SÍ MISMO
    uint32_t  drift_accum_ms;    // cuánto tiempo lleva driftando feo
    uint16_t  miss_count;        // ciclos consecutivos sin present
    uint8_t   glitch_streak;     // glitches consecutivos
    bool      glitched;          // hubo glitch en el ciclo ACTUAL (excluir de fusión)
    bool      request_reset;     // la capa HW debe re-inicializar este sensor
    float     reseed_heading;    // al re-init, sembrar este heading (del más estable)
    bool      seen;              // alguna vez estuvo present (ceba el glitch-test)
    float     rest_prev_heading; // heading del ciclo anterior estando en reposo
    bool      rest_tracking;     // venía en reposo el ciclo anterior
};

struct ImuFusion {
    ImuSensorState s[IMU_FUSION_N];
    float fused_heading_deg;    // SALIDA principal (CCW+, [-180,180])
    bool  fused_valid;          // false si ningún sensor utilizable
    float disagreement_deg;     // |L-R| circular (0 si no hay 2 comparables)
    bool  impact_detected;      // este ciclo el desacuerdo superó disagree_max_deg
};

// Inicializa el estado (todos DEAD hasta el primer present).
void imu_fusion_init(ImuFusion& f);

// Un paso de fusión. dt_s = tiempo desde el tick anterior (s).
void imu_fusion_update(ImuFusion& f,
                       const ImuFusionCfg& cfg,
                       const ImuSensorCfg sensor_cfg[IMU_FUSION_N],
                       const ImuSample in[IMU_FUSION_N],
                       float dt_s);

// La capa HW llama esto DESPUÉS de re-inicializar físicamente el sensor i
// (re-init + sembrar su cero con reseed_heading). Limpia el pedido de reset.
void imu_fusion_clear_reset(ImuFusion& f, int i);

// ---- Helpers puros (expuestos para test) ----
float imu_norm180(float deg);                          // normaliza a [-180,180]
float imu_diff(float a_deg, float b_deg);              // a-b circular en [-180,180]
float imu_circular_mean(float a, float b, float wa, float wb);  // promedio circular pesado

}  // namespace iitasoccer
