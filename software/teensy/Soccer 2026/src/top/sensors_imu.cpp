// sensors_imu.cpp — Capa HARDWARE de los 2 BNO055 del TOP.
//
// Lee los 2 chips y delega TODA la inteligencia de fusión al módulo PURO
// src/shared/imu_fusion.{h,cpp} (host-testeable). Esta capa solo hace I/O:
//   • leer yaw + gyroZ + calib de cada BNO por I2C,
//   • detectar presencia (ACK en el bus),
//   • ejecutar lo que el módulo pide (soft-resync de un sensor que driftó),
//   • persistir/restaurar la calibración de cada chip en EEPROM (Capa 2).
//
// Arquitectura del hardware — UNIFICADA AMBOS ROBOTS (corrección 2026-06-15):
//   2 BNO055 en 0x28, en BUSES SEPARADOS (R2 desde 2026-06-09; R1 unificado 2026-06-15):
//     PRIMARIO   (idx 0) = Wire2 (24/25 nativos, LPI2C4) — SOLO en su bus, sin
//                          ToF -> sin contención i2c -> NO se congela. Manda.
//     SECUNDARIO (idx 1) = Wire (18/19) — comparte bus con los 4 ToF. Respaldo.
//   NO hay ningún BNO en 0x29: el viejo "RIGHT @ 0x29 (ADR a 3V3, unidad fallada)" de robot1
//   fue un ERROR, ya corregido en hardware. 0x29 = solo dir de fábrica de los ToF.
//   (24/25 = Wire2, NO "Wire1"; corrección 2026-06-09, commit 9da8e9e.)
//
// Tiempo de boot (DOC-FIX 2026-06-03): el init del BNO no es "~2 s" como decía
// la nota vieja. A 100 kHz (coexistencia con los ToF) el begin()+estabilización+
// calibración del/los BNO toma ~10 s; y el setup() COMPLETO del TOP ronda ~40 s
// porque cada VL53L7CX carga ~85 KB de firmware blob por I2C (init_one_bno aquí
// reintenta begin() hasta INIT_TIMEOUT_MS=3 s por chip). Ver sensors_tof.cpp.
//
// Convención de heading: CCW-positivo (IZQUIERDA sube), [-180,180]. El chip da
// yaw CW-positivo, lo invertimos con HEADING_SIGN. Mismo signo al gyroZ para que
// el test de glitch del módulo (cambio de heading vs gyro*dt) sea consistente.

#include "sensors_imu.h"
#include "config_top.h"
#include "imu_fusion.h"
#include "top_eeprom_config.h"   // g_top_cfg.bno_left_en/bno_right_en (A2.1 fail-safe)
#ifdef TOP_ENABLE_BNO_FREEZE_DETECT
#include "imu_freeze.h"   // detector de BNO congelado (GATED OFF por default)
#endif
#ifdef TOP_ENABLE_HEADING_PREDICT
#include "heading_predict.h"   // extrapolación de rumbo para transmitir (GATED OFF)
#endif

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <EEPROM.h>
#include <cmath>

namespace iitasoccer {

namespace {

// --- ARQUITECTURA UNIFICADA (corrección 2026-06-15, banco): AMBOS robots tienen
// 2 BNO055 en 0x28, en BUSES SEPARADOS. Antes robot1 figuraba con su 2º BNO en 0x29
// (ADR puenteado a 3V3) — ESO FUE UN ERROR, ya corregido en hardware. NO hay ningún
// BNO en 0x29 (0x29 es solo la dir de fábrica transitoria de los ToF VL53L7CX, que se
// reasignan a 0x2A..0x2D al enumerar). robot2 ya venía así (banco 2026-06-09); robot1
// se unificó a este mismo path → falta validación de banco de robot1 (TASK-216).
// El PRIMARIO va en el ÍNDICE 0 a propósito: imu_fusion prioriza idx0 y cae al que
// esté `present` (failover gratis — test_imu_fusion: idx0-manda en empate + caída a
// idx1 si el primario muere/está ausente).
//   idx 0 = PRIMARIO   → Wire2 (24/25 nativos, LPI2C4): SOLO en su bus, sin ToF →
//           sin contención i2c → no se congela. Fuente preferida.
//   idx 1 = SECUNDARIO → Wire (18/19): comparte bus con los 4 ToF. Respaldo.
Adafruit_BNO055 g_bno_primary  (55, BNO055_LEFT_I2C_ADDR, &Wire2);  // PRIM: Wire2 (24/25) @0x28, solo
Adafruit_BNO055 g_bno_secondary(56, BNO055_LEFT_I2C_ADDR, &Wire);   // SEC:  Wire (18/19) @0x28, con ToF
Adafruit_BNO055* const g_bno[IMU_FUSION_N]  = { &g_bno_primary, &g_bno_secondary };
const uint8_t          g_addr[IMU_FUSION_N] = { BNO055_LEFT_I2C_ADDR, BNO055_LEFT_I2C_ADDR };

bool  g_ready[IMU_FUSION_N]       = { false, false };
float g_offset[IMU_FUSION_N]      = { 0.0f, 0.0f };  // cero capturado (yaw crudo)
bool  g_calib_saved[IMU_FUSION_N] = { false, false };

// Módulo de fusión (estado + config).
ImuFusion    g_fusion;
ImuFusionCfg g_fcfg;
ImuSensorCfg g_scfg[IMU_FUSION_N];

uint32_t g_last_tick_ms   = 0;
uint32_t g_calib_check_ctr = 0;

#ifdef TOP_ENABLE_BNO_FREEZE_DETECT
// Detector de BNO CONGELADO (audit 2026-06-04, único HIGH). GATED OFF por
// default: con el flag sin definir, NADA de esto se compila y el binario de
// competencia es EXACTAMENTE el de hoy. Activarlo SOLO tras validar en banco que
// no da falsos-DEAD con el robot quieto (un BNO vivo jitterea ≥1 LSB de centideg;
// un valor clavado al centideg EXACTO muchos ticks = el (0,0,0) por fallo I²C /
// yaw congelado del banco 2026-06-02). NO agrega transacciones I²C: reusa el
// heading que ya se leyó este tick. Ver src/shared/imu_freeze.h.
ImuFreezeState g_freeze[IMU_FUSION_N];
ImuFreezeCfg   g_freeze_cfg;
#endif

#ifdef TOP_ENABLE_HEADING_PREDICT
// Estado del predictor de rumbo (extrapolación lineal). Lo alimenta el tick con el
// heading fusionado + la ω del primario; lo leen los sitios de transmisión. PURO →
// host-tested (test_heading_predict). Con el flag OFF nada se compila → byte-idéntico.
HeadingPredictState g_hpredict{};
HeadingPredictCfg   g_hpredict_cfg = heading_predict_default_cfg();
#endif

// ── Cross-validación de salud del heading (TASK-213, GATED OFF) ──────────────
// Decide la salud del BNO primario contra datos independientes (OTOS+cámara+centinela).
// Toda la decisión es PURA (imu_cross_validate.h, host-tested); acá solo se alimenta y
// se expone. Con el flag OFF nada de esto se compila → binario byte-idéntico.
#ifdef TOP_ENABLE_HEADING_XVAL
XvalState  g_xval{};
XvalParams g_xval_params = xval_default_params();
float      g_pri_gyro_z_dps = 0.0f;      // cache del gyro_z del primario (in[0], cero I2C extra)
float      g_pri_net_rot_deg_acc = 0.0f; // grados netos acumulados (gate de ventana del centinela)
#endif
#ifdef TOP_ENABLE_BNO_SENTINEL
bool     g_sentinel_init_ok = false;     // el 2º BNO (Wire) se inicializó para el centinela
float    g_sec_yaw_prev_deg = 0.0f;      // yaw del centinela de la ventana 1 Hz anterior
uint32_t g_sec_window_prev_ms = 0;       // ts de la ventana anterior (para el dt real)
bool     g_sec_seeded = false;
#endif

constexpr uint32_t INIT_TIMEOUT_MS = 3000;
constexpr uint32_t STABILIZE_MS    = 1000;
constexpr uint32_t GYRO_CALIB_MS   = 2000;
constexpr int      HEADING_SAMPLES = 10;

// Band-aid contención BNO+ToF (2026-06-02): leer el BNO a ~20 Hz (50 ms), no a 100 Hz.
// El tope de 20 Hz es NECESARIO para el BNO SECUNDARIO (Wire, comparte con los ToF):
// leerlo más seguido lo hace chocar con los reads de los ToF → yaw CONGELADO.
//
// TOP_BNO_FAST (coach 2026-06-14): cuando el ÚNICO BNO activo es el PRIMARIO, que vive
// SOLO en Wire2 (24/25, sin ToF → sin contención; eso lo garantiza TOP_BNO_PRIMARY_ONLY),
// ese tope es innecesario y AGREGA ~25-50 ms de latencia al lazo de rumbo del arquero
// (la mitad del atraso total — ver docs/firmware/CONTROL-ARQUERO-LATERAL-Y-LATENCIAS.md).
// Con el flag se lee a 100 Hz (10 ms) → dato de rumbo fresco cada tick del snapshot.
// El BNO055 en modo fusión entrega a 100 Hz, así que 10 ms es el punto óptimo (leer más
// seguido re-leería el mismo valor). 🔧 Validar en banco: que el loop del TOP siga holgado.
#if defined(TOP_BNO_FAST)
#  if !defined(TOP_BNO_PRIMARY_ONLY)
#    error "TOP_BNO_FAST solo es seguro con -DTOP_BNO_PRIMARY_ONLY (BNO primario SOLO en Wire2, sin ToF). Sin eso leería rápido el secundario del bus compartido y volvería el freeze por contención."
#  endif
constexpr uint32_t BNO_READ_INTERVAL_MS = 10;   // 100 Hz: primario aislado en Wire2, sin contención
#else
constexpr uint32_t BNO_READ_INTERVAL_MS = 50;   // 20 Hz: band-aid de contención (secundario en Wire)
#endif

// Signo del heading. MEDIDO EN BANCO 2026-05-31: el chip da yaw CRECIENTE al
// girar a la DERECHA (CW). La convención del firmware es CCW-positiva. Lo
// invertimos ACÁ, en la fuente, para TODO el firmware. Igual al gyroZ.
constexpr float HEADING_SIGN = -1.0f;

// --- EEPROM (Capa 2): perfil de calibración POR sensor ---
// Región propia (el DOWN usa su propia región en eeprom_calib.cpp; esta base
// 320 no la pisa). Layout: [magic][ver] + por sensor [valid][blob 22B].
constexpr int     EE_BASE         = 320;
constexpr uint8_t EE_MAGIC        = 0xB2;
constexpr uint8_t EE_VERSION      = 1;
constexpr int     BNO_CALIB_BYTES = 22;   // NUM_BNO055_OFFSET_REGISTERS
constexpr int     EE_SENSOR_STRIDE = 1 + BNO_CALIB_BYTES;
int ee_sensor_addr(int i) { return EE_BASE + 2 + i * EE_SENSOR_STRIDE; }

bool ee_header_ok() {
    return EEPROM.read(EE_BASE) == EE_MAGIC && EEPROM.read(EE_BASE + 1) == EE_VERSION;
}
void ee_write_header() {
    EEPROM.update(EE_BASE, EE_MAGIC);
    EEPROM.update(EE_BASE + 1, EE_VERSION);
}
bool ee_load_into_bno(int i, Adafruit_BNO055& bno) {
    if (!ee_header_ok()) return false;
    const int a = ee_sensor_addr(i);
    if (EEPROM.read(a) != 1) return false;
    uint8_t blob[BNO_CALIB_BYTES];
    for (int k = 0; k < BNO_CALIB_BYTES; ++k) blob[k] = EEPROM.read(a + 1 + k);
    bno.setSensorOffsets(blob);
    return true;
}
bool ee_save_from_bno(int i, Adafruit_BNO055& bno) {
    uint8_t blob[BNO_CALIB_BYTES];
    if (!bno.getSensorOffsets(blob)) return false;  // aún no calibrado
    ee_write_header();
    const int a = ee_sensor_addr(i);
    EEPROM.update(a, 1);
    for (int k = 0; k < BNO_CALIB_BYTES; ++k) EEPROM.update(a + 1 + k, blob[k]);
    return true;
}

// --- Lecturas crudas del chip ---
float read_raw_yaw(Adafruit_BNO055& bno) {
    return bno.getVector(Adafruit_BNO055::VECTOR_EULER).x();
}
float read_gyro_z(Adafruit_BNO055& bno) {
    return bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE).z();
}
uint8_t read_gyro_calib(Adafruit_BNO055& bno) {
    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);
    return gyro;
}
// Presencia en un bus ARBITRARIO (Wire o Wire2). Guard anti-cuelgue: solo se hace
// begin() de un BNO si su dirección ACKea en SU bus (un begin() sobre una dirección
// fantasma cuelga el bus I2C entero). Ambos BNO viven en 0x28 pero en buses distintos
// (primario en Wire2, secundario en Wire) → se sondea cada uno en el suyo.
// (El sondeo de chip-id en 0x29 — i2c_present()/read_reg() — se retiró con la corrección
//  2026-06-15: ya no hay ningún BNO en 0x29.)
bool i2c_present_on(TwoWire& bus, uint8_t addr) {
    bus.beginTransmission(addr);
    return bus.endTransmission() == 0;
}

float norm180(float h) {
    while (h > 180.0f) h -= 360.0f;
    while (h < -180.0f) h += 360.0f;
    return h;
}
float capture_offset(Adafruit_BNO055& bno) {
    float sum = 0.0f;
    for (int i = 0; i < HEADING_SAMPLES; ++i) { sum += read_raw_yaw(bno); delay(20); }
    return sum / HEADING_SAMPLES;
}
// Heading CCW+, relativo al cero, SIN mount_offset (eso lo aplica el módulo).
float heading_no_mount(int i, float raw_yaw) {
    return norm180(HEADING_SIGN * (raw_yaw - g_offset[i]));
}

bool init_one_bno(Adafruit_BNO055& bno) {
    const uint32_t start = millis();
    while (!bno.begin(OPERATION_MODE_IMUPLUS)) {
        if (millis() - start > INIT_TIMEOUT_MS) return false;
        delay(100);
    }
#ifndef TOP_BNO_INTERNAL_OSC
    bno.setExtCrystalUse(true);
    Serial.println("[IMU] reloj BNO: CRISTAL EXTERNO (default)");
#else
    Serial.println("[IMU] reloj BNO: OSCILADOR INTERNO (TOP_BNO_INTERNAL_OSC, banco)");
    // OSCILADOR INTERNO — HIPÓTESIS REFUTADA (banco 2026-06-10 → 2026-06-21, robot1).
    // En su momento se sospechó que un golpe dañó el cristal externo de 32 kHz (falla
    // conocida del BNO055: reloj de fusión muerto → salidas congeladas con chip vivo) y
    // este flag arranca con el OSCILADOR INTERNO para descartarlo. NO era el cristal: el
    // chip y el cristal están SANOS. El "heading=0.0" del PRIMARIO era un flag de config
    // (bno_left_en=0 en EEPROM) que lo excluía de la fusión — ver el FIX 2026-06-21 más
    // abajo y journal/2026-06-21-bno-heading-fix-config-flag-no-era-tof.md. El freeze del
    // SECUNDARIO sí era real, pero por contención I²C en el bus Wire compartido con los ToF
    // (no por el cristal). El flag se conserva solo como recurso por si apareciera una
    // falla de cristal REAL en otra unidad. Solo en envs *_oscint de banco.
#endif
    delay(STABILIZE_MS);
    const uint32_t calib_start = millis();
    while (millis() - calib_start < GYRO_CALIB_MS) {
        if (read_gyro_calib(bno) >= 3) break;
        delay(100);
    }
    return true;
}

}  // namespace

bool sensors_imu_init() {
    // Bus I2C. Los ToF ya fueron DORMIDOS (LP low) por sensors_tof_predim_lp() ANTES
    // de esta llamada, asi que el 0x28 del BNO secundario de Wire queda limpio (sin los
    // ToF en su 0x29 de fábrica); el primario vive en Wire2, aparte (receta validada en
    // diag_pose_live: dim ToF -> init BNO -> enumerar ToF). (2026-06-02)
    Wire.begin();
    Wire.setClock(100000);  // 100 kHz: el BNO055 y los VL53L7CX NO coexisten a 400 kHz — con
                            // los ToF rangeando, el read multi-byte del BNO se corrompe y el
                            // yaw queda CONGELADO (banco 2026-06-02). A 100 kHz coexisten OK
                            // (diag_bno_tof_slow: yaw sigue el giro con los 4 ToF activos).
                            // 400 kHz solo servia con ToF-solo (quad_live) o BNO-solo
                            // (diag_bno_left). Costo: boot ~40 s (carga firmware de los 4 ToF).
    // Bus Wire2 (pines 24/25 NATIVOS del Teensy 4.0, LPI2C4; SIN setSCL/setSDA — 24/25
    // son los default de Wire2, corrección 2026-06-09). Ahí vive SOLO el BNO PRIMARIO
    // (cero ToF -> cero contención). AMBOS robots usan Wire2 para el primario (arquitectura
    // unificada 2026-06-15) → el begin va SIEMPRE. Antes estaba gateado a ROBOT2/
    // TOP_BNO1_ON_WIRE2; sin esto, en `top_robot1` plano el primario de R1 en Wire2 no
    // arrancaría (i2c_present_on(Wire2,...) sin bus iniciado). Clock conservador 100k.
    Wire2.begin();
    Wire2.setClock(100000);

    imu_fusion_init(g_fusion);
    g_fcfg = imu_fusion_default_cfg();
    for (int i = 0; i < IMU_FUSION_N; ++i) g_scfg[i] = imu_fusion_default_sensor_cfg();
    // A2.1: un BNO deshabilitado por config queda EXCLUIDO de la fusión de heading
    // (imu_fusion lo marca DEAD, peso 0 — imu_fusion.cpp:102). bno_left = idx0
    // (primario), bno_right = idx1 (secundario). Apply mínimo: NO toca el begin()
    // ni el orden crítico de init; solo el peso en la fusión. Default = todo on.
    // FIX 2026-06-21: el EEPROM tenía bno_left_en=0 (primario DESHABILITADO, seguro por un
    // BNO_L_OFF de una sesión vieja) → la fusión lo trataba como DEAD → fused_heading=0.0
    // SIEMPRE (no era freeze del chip ni los ToF: el firmware ignoraba el primario). El
    // primario es la ÚNICA fuente de rumbo en primary-only: NUNCA debe quedar deshabilitado
    // por config. Se fuerza habilitado.
    g_scfg[0].enabled = true;   // era g_top_cfg.bno_left_en
    g_scfg[1].enabled = g_top_cfg.bno_right_en;

#ifdef TOP_ENABLE_BNO_FREEZE_DETECT
    // Detector de congelamiento: umbral derivado del intervalo de lectura real
    // (BNO_READ_INTERVAL_MS) para que N corresponda a un tiempo conocido.
    g_freeze_cfg = imu_freeze_cfg_from_rate(BNO_READ_INTERVAL_MS, IMU_FREEZE_MIN_MS);
    for (int i = 0; i < IMU_FUSION_N; ++i) imu_freeze_reset(g_freeze[i]);
#endif

    // AMBOS robots (arquitectura unificada 2026-06-15): PRIMARIO primero (Wire2 24/25,
    // idx 0), SECUNDARIO después (Wire, idx 1). Guard anti-cuelgue en AMBOS (un begin()
    // sobre una dirección fantasma CUELGA el bus entero): solo init si hace ACK en SU
    // bus. En Wire el 0x28 queda limpio porque los ToF ya fueron dormidos por predim.
    if (i2c_present_on(Wire2, g_addr[0])) {
        Serial.println("[IMU] Init BNO PRIMARIO (Wire2 24/25 @ 0x28, bus propio sin ToF)...");
        g_ready[0] = init_one_bno(g_bno_primary);
        Serial.println(g_ready[0] ? "[IMU] PRIMARIO OK" : "[IMU] PRIMARIO FAIL (continuando)");
    } else {
        g_ready[0] = false;
        Serial.println("[IMU] PRIMARIO (Wire2 0x28) sin ACK -> SALTEO (bus sano)."
                       " Revisar soldadura/alim del BNO de abajo del Teensy.");
    }
#ifdef TOP_BNO_PRIMARY_ONLY
    // PRIMARIO-SOLO (banco 2026-06-11, R1 recableado): el SECUNDARIO de Wire se
    // CONGELÓ en producción y el soft-resync de la fusión eligió al congelado
    // como referencia "estable" y ARRASTRÓ al primario sano (hdg 0.0 → -142.4
    // clavado, log de banco). Hasta arreglar ese árbitro (tema-a-analizar:
    // detectar freeze ANTES de arbitrar deriva), los envs *_pri corren SOLO el
    // primario de Wire2 (bus propio, nunca se congeló) — sin redundancia, sin
    // sensor muerto que pueda ganar nada.
    // (Nota 2026-06-21: el secundario NO es un chip muerto — es SANO pero vive en el bus
    //  compartido con los ToF, donde la contención lo congela; por eso queda de centinela,
    //  fuera de la fusión. La defensa real para volver al dual-BNO es el detector de freeze.)
    g_ready[1] = false;
    Serial.println("[IMU] SECUNDARIO DESHABILITADO (TOP_BNO_PRIMARY_ONLY - primario-solo)");
#else
    if (i2c_present_on(Wire, g_addr[1])) {
        Serial.println("[IMU] Init BNO SECUNDARIO (Wire 18/19 @ 0x28, comparte con ToF)...");
        g_ready[1] = init_one_bno(g_bno_secondary);
        Serial.println(g_ready[1] ? "[IMU] SECUNDARIO OK" : "[IMU] SECUNDARIO FAIL (continuando)");
    } else {
        g_ready[1] = false;
        Serial.println("[IMU] SECUNDARIO (Wire 0x28) sin ACK -> SALTEO (bus sano).");
    }
#endif
    // (Rama robot1 con sondeo 0x29 RETIRADA 2026-06-15: robot1 se unificó a la
    //  arquitectura de arriba — 2 BNO @ 0x28 en buses separados. Ya no hay BNO en 0x29.)

#if defined(TOP_ENABLE_BNO_SENTINEL)
    // CENTINELA (TASK-213): inicializar el 2º BNO (Wire) al boot para usarlo como
    // SEGUNDA OPINIÓN @1 Hz, AUNQUE g_ready[1] siga false (no entra a la fusión del
    // control → byte-idéntico). AMBOS robots (R1 unificado 2026-06-15) — antes ROBOT2-solo.
    // control → byte-idéntico). Sin begin(), leerlo crudo daría basura (no está en modo
    // fusión). Los ToF están dormidos (predim) en este punto del setup → el begin del
    // secundario en Wire no choca. ⚠️ NO tocar g_ready[1].
    if (i2c_present_on(Wire, g_addr[1])) {
        g_sentinel_init_ok = init_one_bno(g_bno_secondary);
        Serial.println(g_sentinel_init_ok
            ? "[IMU] CENTINELA init OK (2do BNO en Wire; g_ready[1] sigue false para fusion)"
            : "[IMU] CENTINELA init FAIL (sigo sin 2da opinion)");
    } else {
        Serial.println("[IMU] CENTINELA: 2do BNO (Wire 0x28) sin ACK -> sin 2da opinion.");
    }
#endif

    // EEPROM (Capa 2): restaurar perfil de calibración de cada chip si existe.
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (g_ready[i] && ee_load_into_bno(i, *g_bno[i])) {
            Serial.print("[IMU] calib restaurada de EEPROM para sensor ");
            Serial.println(i);
        }
    }

    // Cero de cada sensor (robot debe apuntar al arco rival al boot).
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (g_ready[i]) g_offset[i] = capture_offset(*g_bno[i]);
    }

    g_last_tick_ms = millis();
    return g_ready[0] || g_ready[1];
}

void sensors_imu_tick() {
    const uint32_t now = millis();
    // Band-aid contención BNO+ToF (2026-06-02): leer el BNO a ~20 Hz, NO a 100 Hz. Leerlo muy
    // seguido lo hace chocar con los reads de los ToF en `Wire` y el read multi-byte del BNO
    // se corrompe -> yaw CONGELADO. Bajando la frecuencia caen las colisiones. (Fix de fondo:
    // BNO en bus aparte = Wire2 24/25 — ROBOT2 ya lo tiene: su PRIMARIO vive solo en Wire2 y
    // no sufre la contención; el secundario de Wire mantiene este band-aid.)
    if (now - g_last_tick_ms < BNO_READ_INTERVAL_MS) return;
    float dt_s = (now - g_last_tick_ms) / 1000.0f;
    g_last_tick_ms = now;
    if (dt_s <= 0.0f || dt_s > 1.0f) dt_s = 0.01f;  // clamp arranque/saltos

    ImuSample in[IMU_FUSION_N];
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        const bool present = g_ready[i];  // band-aid: sin ping i2c_present (1 transaccion I2C menos por sensor)
        if (present) {
            in[i].present     = true;
            in[i].heading_deg = heading_no_mount(i, read_raw_yaw(*g_bno[i]));
            in[i].gyro_z_dps  = HEADING_SIGN * read_gyro_z(*g_bno[i]);
            in[i].calib_gyro  = read_gyro_calib(*g_bno[i]);
        } else {
            in[i].present = false; in[i].heading_deg = 0.0f;
            in[i].gyro_z_dps = 0.0f; in[i].calib_gyro = 0;
        }
    }

#ifdef TOP_ENABLE_BNO_FREEZE_DETECT
    // GATED OFF por default. Antes de fusionar: si un sensor que está present
    // tiene el heading clavado al centideg EXACTO por >= N lecturas y >= T ms,
    // lo tratamos como CONGELADO y bajamos su present → false, para que
    // imu_fusion lo marque DEAD (deja de contaminar la fusión / el failover).
    // Un BNO que muere por fallo I²C sigue ackeando (present=true) y reporta
    // (0,0,0) clavado: sin esto, su heading muerto pasaría como válido para
    // siempre (banco 2026-06-02: yaw congelado por contención BNO+ToF). Reusa el
    // heading ya leído este tick: CERO transacciones I²C extra (no reintroduce la
    // contención). Si el sensor revive (el valor cambia), imu_freeze se limpia
    // solo, pero present sigue gobernado por g_ready (re-habilitar de verdad
    // requiere re-init físico, igual que el latch de otos_health).
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (!in[i].present) continue;
        const int16_t hcdeg = static_cast<int16_t>(in[i].heading_deg * 100.0f);
        // VARIANTE CON GUARDA DE GYRO (2026-06-15): arregla el falso-DEAD del robot
        // QUIETO que hizo QUITAR este flag de los envs el 2026-06-08. Solo declara
        // congelado si ADEMÁS el gyro probó que el robot estaba GIRANDO mientras el
        // heading quedó clavado (in[i].gyro_z_dps ya leído este tick → CERO I²C extra).
        // Ver src/shared/imu_freeze.h::imu_freeze_update_g.
        if (imu_freeze_update_g(g_freeze[i], hcdeg, in[i].gyro_z_dps, now, g_freeze_cfg)) {
            in[i].present = false;   // congelado → fusion lo verá ausente → DEAD
        }
    }
#endif

    imu_fusion_update(g_fusion, g_fcfg, g_scfg, in, dt_s);
#ifdef TOP_DBG_BNO
    // DIAG (default OFF): ver dónde se congela — lectura cruda (in0_hdg) vs fusión (fused/s0).
    { static uint32_t s_dbg = 0; if (millis() - s_dbg > 300) { s_dbg = millis();
        Serial.print("[DBG] RAW_eul="); Serial.print(read_raw_yaw(*g_bno[0]), 1);
        Serial.print(" off="); Serial.print(g_offset[0], 1);
        Serial.print(" in0="); Serial.print(in[0].heading_deg, 1);
        Serial.print(" fused="); Serial.print(g_fusion.fused_heading_deg, 1);
        Serial.print(" pres="); Serial.print(in[0].present ? 1 : 0);
        Serial.print(" calg="); Serial.println(in[0].calib_gyro); } }
#endif

#ifdef TOP_ENABLE_HEADING_PREDICT
    // Extrapolación de rumbo (predict step): alimentar con el heading FUSIONADO recién
    // calculado + la ω del primario YA leída este tick (in[0].gyro_z_dps → CERO I²C
    // extra). El valor extrapolado lo consumen los sitios de TRANSMISIÓN del snapshot
    // (main_top / snapshot_emitter); el heading CRUDO sigue intacto para freeze/localiz.
    heading_predict_on_sample(g_hpredict, sensors_imu_get_heading_centideg(),
                              in[0].gyro_z_dps, sensors_imu_get_heading_valid(), now);
#endif

#ifdef TOP_ENABLE_HEADING_XVAL
    // Cross-validación (TASK-213): cachear el gyro_z del primario (in[0], YA leído →
    // CERO I²C extra), acumular la rotación NETA en GRADOS (gate de ventana del centinela),
    // alimentar el veredicto del primario y correr xval_update UNA sola vez por tick.
    g_pri_gyro_z_dps = in[0].gyro_z_dps;
    g_pri_net_rot_deg_acc += in[0].gyro_z_dps * dt_s;
#if defined(TOP_ENABLE_BNO_FREEZE_DETECT)
    // INC-1 ya bajó present→false si el primario está congelado (sano-pero-clavado).
    // Solo cuenta como "frozen" si ESTABA listo (no confundir con primario ausente al boot).
    const bool inc1_frozen = g_ready[0] && (in[0].present == false);
#else
    const bool inc1_frozen = false;
#endif
    xval_feed_primary(g_xval, g_pri_gyro_z_dps, inc1_frozen, now);
    xval_update(g_xval, g_xval_params, now);
#endif

    // Pedidos de RESET del módulo: SOFT-RESYNC no bloqueante (re-cero del sensor
    // que driftó, alineado al más estable). NO hace begin() en el loop.
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (g_fusion.s[i].request_reset && g_ready[i]) {
            const float reseed = g_fusion.s[i].reseed_heading;
            const float raw    = read_raw_yaw(*g_bno[i]);
            const float target_no_mount = reseed - g_scfg[i].mount_offset_deg;
            // heading = SIGN*(raw - offset) = target  =>  offset = raw - target/SIGN
            g_offset[i] = raw - target_no_mount / HEADING_SIGN;
            imu_fusion_clear_reset(g_fusion, i);
#ifdef TOP_ENABLE_BNO_FREEZE_DETECT
            // El re-cero cambia el heading a propósito: re-sembrar el detector
            // para no contar ese salto deliberado como (des)congelamiento espurio.
            imu_freeze_reset(g_freeze[i]);
#endif
            Serial.print("[IMU] soft-resync sensor ");
            Serial.print(i);
            Serial.println(" (drifto; re-alineado con el estable)");
        }
    }

    // Auto-guardar calib a EEPROM al llegar a fully-calibrated (chequeo barato
    // cada ~200 ticks para no saturar el bus).
    if (++g_calib_check_ctr >= 200) {
        g_calib_check_ctr = 0;
        for (int i = 0; i < IMU_FUSION_N; ++i) {
            if (g_ready[i] && !g_calib_saved[i] && ee_save_from_bno(i, *g_bno[i])) {
                g_calib_saved[i] = true;
                Serial.print("[IMU] calib guardada en EEPROM para sensor ");
                Serial.println(i);
            }
        }
    }
}

float sensors_imu_get_heading_deg()       { return g_fusion.fused_heading_deg; }
float sensors_imu_get_left_heading_deg()  { return g_fusion.s[0].heading_deg; }
float sensors_imu_get_right_heading_deg() { return g_fusion.s[1].heading_deg; }

int16_t sensors_imu_get_heading_centideg() {
    return static_cast<int16_t>(sensors_imu_get_heading_deg() * 100.0f);
}

#ifdef TOP_ENABLE_HEADING_PREDICT
int16_t sensors_imu_get_heading_centideg_predicted() {
    // Extrapola al instante ACTUAL (transmisión) usando la última ω medida y la edad
    // del ancla. Cap + deadband adentro (heading_predict.h). millis() aquí está OK
    // (contexto loop, no ISR).
    return heading_predict_value(g_hpredict, millis(), g_hpredict_cfg);
}
#endif

float sensors_imu_get_disagreement_deg() { return g_fusion.disagreement_deg; }

void sensors_imu_recalibrate_zero() {
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (g_ready[i]) g_offset[i] = capture_offset(*g_bno[i]);
    }
    imu_fusion_init(g_fusion);  // limpia estado de fusión (drift, glitch, etc)
#ifdef TOP_ENABLE_BNO_FREEZE_DETECT
    for (int i = 0; i < IMU_FUSION_N; ++i) imu_freeze_reset(g_freeze[i]);
#endif
#ifdef TOP_ENABLE_HEADING_PREDICT
    heading_predict_reset(g_hpredict);  // re-cero cambia el heading a propósito → no extrapolar el salto
#endif
    g_last_tick_ms = millis();
}

bool sensors_imu_save_calibration() {
    bool any = false;
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (g_ready[i] && ee_save_from_bno(i, *g_bno[i])) { g_calib_saved[i] = true; any = true; }
    }
    return any;
}

bool sensors_imu_left_ready()  { return g_ready[0]; }
bool sensors_imu_right_ready() { return g_ready[1]; }

// Validez del heading EN VIVO (no el readiness al boot). Refleja la fusión:
// false cuando NINGÚN sensor es utilizable en runtime (n_use==0 en imu_fusion).
// Hoy es byte-idéntico a (_left_ready || _right_ready) porque `present` arranca
// en g_ready y no baja por tick (band-aid de contención I2C). Cuando el detector
// de BNO congelado (TOP_ENABLE_BNO_FREEZE_DETECT) o un futuro miss-counter ponga
// present->false en vivo, fused_valid cae a false y el snapshot deja de marcar
// heading_valid -> CENTRAL deja de confiar en un heading muerto. (Audit 2026-06-05 R1.)
bool sensors_imu_get_heading_valid() { return g_fusion.fused_valid; }

// ── Cross-validación de salud del heading (TASK-213) — getters + paso del centinela ──
#ifdef TOP_ENABLE_HEADING_XVAL
uint8_t sensors_imu_xval_verdict()   { return static_cast<uint8_t>(xval_primary_verdict(g_xval)); }
uint8_t sensors_imu_xval_score()     { return static_cast<uint8_t>(xval_primary_score(g_xval)); }
uint8_t sensors_imu_xval_n_indep()   { return xval_n_indep_refs(g_xval); }
float   sensors_imu_get_gyro_z_dps() { return g_pri_gyro_z_dps; }
XvalState& sensors_imu_xval_state()  { return g_xval; }
float sensors_imu_take_pri_net_rotation_deg() {
    const float r = g_pri_net_rot_deg_acc; g_pri_net_rot_deg_acc = 0.0f; return r;
}
#else
uint8_t sensors_imu_xval_verdict()   { return 0; }
uint8_t sensors_imu_xval_score()     { return 0; }
uint8_t sensors_imu_xval_n_indep()   { return 0; }
float   sensors_imu_get_gyro_z_dps() { return 0.0f; }
#endif

#ifdef TOP_ENABLE_BNO_SENTINEL
bool sensors_imu_sentinel_ready() { return g_sentinel_init_ok; }
// Último yaw que leyó el centinela del 2º BNO (Wire), CCW+, crudo: SIN el offset de boot
// (es otro chip con otro mount → no comparte el cero del primario). Se actualiza @1 Hz en
// sensors_imu_sentinel_step. Sirve para que el monitor muestre que el 2º BNO da heading vivo.
float sensors_imu_sentinel_heading_deg() { return g_sec_yaw_prev_deg; }
#else
bool sensors_imu_sentinel_ready() { return false; }
float sensors_imu_sentinel_heading_deg() { return 0.0f; }
#endif

// Paso del CENTINELA @1Hz. Lo llama el LOOP en la ventana bus-quiet (aislada de los reads
// de ToF en Wire), para que el read del secundario NUNCA quede pegado a un getRangingData.
#if defined(TOP_ENABLE_BNO_SENTINEL) && defined(TOP_ENABLE_HEADING_XVAL)
void sensors_imu_sentinel_step(uint32_t now_ms) {
    if (!g_sentinel_init_ok) return;
    xval_sentinel_timed_out(g_xval, g_xval_params, now_ms);   // re-arma si quedó colgado
    if (!xval_sentinel_due(g_xval, now_ms)) return;           // todavía no toca la ventana 1 Hz
    xval_sentinel_arm(g_xval, g_xval_params, now_ms);
    // Lectura limpia del 2º BNO (Wire) — estamos en la ventana bus-quiet, sin ToF.
    const float yaw_now = HEADING_SIGN * read_raw_yaw(g_bno_secondary);   // CCW+ como gyro_z
    float w_sec_dps = 0.0f;
    if (g_sec_seeded) {
        float dt_s = (now_ms - g_sec_window_prev_ms) / 1000.0f;
        if (dt_s < 0.05f) dt_s = 1.0f;                        // guarda anti-división
        float d = yaw_now - g_sec_yaw_prev_deg;               // Δyaw envuelto a [-180,180]
        while (d >  180.0f) d -= 360.0f;
        while (d < -180.0f) d += 360.0f;
        w_sec_dps = d / dt_s;
    }
    const float net_pri_deg = sensors_imu_take_pri_net_rotation_deg();
    xval_feed_sentinel(g_xval, w_sec_dps, g_sentinel_init_ok, net_pri_deg, now_ms);
    g_sec_yaw_prev_deg   = yaw_now;
    g_sec_window_prev_ms = now_ms;
    g_sec_seeded         = true;
    xval_sentinel_done(g_xval, g_xval_params, now_ms);
}
#elif defined(TOP_ENABLE_BNO_SENTINEL)
// Centinela SIN XVAL (salud pura): lee el 2º BNO (Wire) @1Hz SOLO para diagnóstico/monitor.
// NO alimenta la fusión ni el heading — el robot sigue primary-only (g_ready[1]=false), así
// que un secundario congelado NO puede arrastrar al primario (a diferencia del bug histórico
// del soft-resync de la fusión, que era OTRO camino). El caller (main_top, rama R2) ya lo
// llama SOLO en la ventana bus-quiet (>=TOP_BNO_TOF_GAP_MS desde el último read de ToF), así
// que el read del secundario nunca queda pegado a un getRangingData del ToF en Wire. Throttle
// interno @1Hz. Expone el yaw vía sensors_imu_sentinel_heading_deg() (telemetría imu_sentinel_*):
// si NO cambia cuando el robot gira -> el secundario está congelado (el diagnóstico buscado).
void sensors_imu_sentinel_step(uint32_t now_ms) {
    if (!g_sentinel_init_ok) return;
    static uint32_t s_last_ms = 0;
    static bool     s_first   = true;
    if (!s_first && (now_ms - s_last_ms) < 1000u) return;   // @1Hz (resta unsigned -> wrap-safe)
    s_first   = false;
    s_last_ms = now_ms;
    // Lectura limpia del 2º BNO (Wire) en la ventana bus-quiet. CCW+ crudo: es otro chip con
    // otro mount, NO comparte el cero de boot del primario (es solo para ver "vivo y cambiando").
    g_sec_yaw_prev_deg = HEADING_SIGN * read_raw_yaw(g_bno_secondary);
}
#endif

}  // namespace iitasoccer
