// sensors_imu.cpp — Capa HARDWARE de los 2 BNO055 del TOP.
//
// Lee los 2 chips y delega TODA la inteligencia de fusión al módulo PURO
// src/shared/imu_fusion.{h,cpp} (host-testeable). Esta capa solo hace I/O:
//   • leer yaw + gyroZ + calib de cada BNO por I2C,
//   • detectar presencia (ACK en el bus),
//   • ejecutar lo que el módulo pide (soft-resync de un sensor que driftó),
//   • persistir/restaurar la calibración de cada chip en EEPROM (Capa 2).
//
// Arquitectura del hardware — POR ROBOT (gateado, ver #if defined(ROBOT2)):
//   ROBOT1 (recableado 2026-05-31, banco): AMBOS BNO en el bus Wire (18/19).
//     LEFT=0x28 (ADR flotante), RIGHT=0x29 (ADR a 3V3; unidad FALLADA, se saltea).
//   ROBOT2 (banco 2026-06-09): 2 BNO en BUSES SEPARADOS, ambos 0x28:
//     PRIMARIO   (idx 0) = Wire2 (24/25 nativos, LPI2C4) — SOLO en su bus, sin
//                          ToF -> sin contención i2c -> NO se congela. Manda.
//     SECUNDARIO (idx 1) = Wire (18/19) — comparte bus con los 4 ToF. Respaldo.
//     (24/25 = Wire2, NO "Wire1"; corrección 2026-06-09, commit 9da8e9e.)
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
#ifdef TOP_ENABLE_BNO_FREEZE_DETECT
#include "imu_freeze.h"   // detector de BNO congelado (GATED OFF por default)
#endif

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <EEPROM.h>
#include <cmath>

namespace iitasoccer {

namespace {

#if defined(ROBOT2)
// --- ROBOT2: 2 BNO en BUSES SEPARADOS (banco 2026-06-09; tabla en robot2.h:
// IMU_BNO_BUS={2,0}, IMU_BNO_ADDR={0x28,0x28}) ---
// El PRIMARIO va en el ÍNDICE 0 a propósito: imu_fusion prioriza idx0 y cae al
// que esté `present` (failover gratis — cubierto en test_imu_fusion: idx0-manda
// en empate + caída a idx1 si el primario muere/está ausente).
Adafruit_BNO055 g_bno_primary  (55, BNO055_LEFT_I2C_ADDR, &Wire2);  // PRIM: Wire2 (24/25) @0x28, solo
Adafruit_BNO055 g_bno_secondary(56, BNO055_LEFT_I2C_ADDR, &Wire);   // SEC:  Wire (18/19) @0x28, con ToF
Adafruit_BNO055* const g_bno[IMU_FUSION_N]  = { &g_bno_primary, &g_bno_secondary };
const uint8_t          g_addr[IMU_FUSION_N] = { BNO055_LEFT_I2C_ADDR, BNO055_LEFT_I2C_ADDR };
#else
// --- Los 2 BNO en el mismo bus Wire (ver cabecera) ---
Adafruit_BNO055 g_bno_left (55, BNO055_LEFT_I2C_ADDR,  &Wire);
Adafruit_BNO055 g_bno_right(56, BNO055_RIGHT_I2C_ADDR, &Wire);
Adafruit_BNO055* const g_bno[IMU_FUSION_N]  = { &g_bno_left, &g_bno_right };
const uint8_t          g_addr[IMU_FUSION_N] = { BNO055_LEFT_I2C_ADDR, BNO055_RIGHT_I2C_ADDR };
#endif

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

constexpr uint32_t INIT_TIMEOUT_MS = 3000;
constexpr uint32_t STABILIZE_MS    = 1000;
constexpr uint32_t GYRO_CALIB_MS   = 2000;
constexpr int      HEADING_SAMPLES = 10;

// Band-aid contención BNO+ToF (2026-06-02): leer el BNO a ~20 Hz (50 ms), no a 100 Hz.
constexpr uint32_t BNO_READ_INTERVAL_MS = 50;

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
#if defined(ROBOT2)
// Presencia en un bus ARBITRARIO: en ROBOT2 el primario vive en Wire2, no en
// Wire. (En R2, i2c_present()/read_reg() de R1 no se compilan — eran solo del
// sondeo 0x29 del RIGHT — para no dejar funciones sin uso con -Wall.)
bool i2c_present_on(TwoWire& bus, uint8_t addr) {
    bus.beginTransmission(addr);
    return bus.endTransmission() == 0;
}
#else
bool i2c_present(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}
#endif

#if !defined(ROBOT2)
// Lee 1 byte de un registro (repeated-start). true si pudo leer. Sirve para
// distinguir un BNO (CHIP_ID reg 0x00 = 0xA0) de un ToF u otro chip en la misma dir.
// (Solo R1: lo usa el sondeo 0x29 del RIGHT; en R2 no hay nada en 0x29.)
bool read_reg(uint8_t addr, uint8_t reg, uint8_t& out) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)addr, 1) != 1) return false;
    out = Wire.read();
    return true;
}
#endif

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
    // OSCILADOR INTERNO (banco 2026-06-10, robot1): el BNO-L de R1 quedó con el
    // yaw CONGELADO tras un golpe (ackea I²C pero hdg clavado en -160.2 girándolo
    // a mano). Modo de falla conocido del BNO055: el golpe daña el cristal externo
    // de 32 kHz → el reloj de la fusión muere → salidas congeladas con chip vivo.
    // Con este flag NO se conmuta al cristal externo (queda el oscilador interno,
    // levemente más drift) — si el cristal era el problema, el sensor revive.
    // Solo en envs *_oscint de banco hasta validar.
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
    // de esta llamada, asi que 0x28 y 0x29 quedan limpios para los 2 BNO (receta
    // validada en diag_pose_live: dim ToF -> init BNO -> enumerar ToF). (2026-06-02)
    // Wire1 ya NO se usa aca (recableado 2026-05-31).
    Wire.begin();
    Wire.setClock(100000);  // 100 kHz: el BNO055 y los VL53L7CX NO coexisten a 400 kHz — con
                            // los ToF rangeando, el read multi-byte del BNO se corrompe y el
                            // yaw queda CONGELADO (banco 2026-06-02). A 100 kHz coexisten OK
                            // (diag_bno_tof_slow: yaw sigue el giro con los 4 ToF activos).
                            // 400 kHz solo servia con ToF-solo (quad_live) o BNO-solo
                            // (diag_bno_left). Costo: boot ~40 s (carga firmware de los 4 ToF).
#if defined(ROBOT2)
    // ROBOT2: bus del BNO PRIMARIO — Wire2 (pines 24/25 NATIVOS del Teensy 4.0,
    // LPI2C4; SIN setSCL/setSDA — 24/25 son los default de Wire2, corrección
    // 2026-06-09). Ahí vive SOLO el 2º BNO (cero ToF -> cero contención; el
    // motivo del freeze de R1 no existe en este bus). Mismo clock que robot2.h
    // (I2C_CLOCK_HZ=100k, conservador; banco puede probar 400k al estar solo).
    Wire2.begin();
    Wire2.setClock(100000);
#endif

    imu_fusion_init(g_fusion);
    g_fcfg = imu_fusion_default_cfg();
    for (int i = 0; i < IMU_FUSION_N; ++i) g_scfg[i] = imu_fusion_default_sensor_cfg();

#ifdef TOP_ENABLE_BNO_FREEZE_DETECT
    // Detector de congelamiento: umbral derivado del intervalo de lectura real
    // (BNO_READ_INTERVAL_MS) para que N corresponda a un tiempo conocido.
    g_freeze_cfg = imu_freeze_cfg_from_rate(BNO_READ_INTERVAL_MS, IMU_FREEZE_MIN_MS);
    for (int i = 0; i < IMU_FUSION_N; ++i) imu_freeze_reset(g_freeze[i]);
#endif

#if defined(ROBOT2)
    // ROBOT2: PRIMARIO primero (Wire2 24/25, idx 0), SECUNDARIO después (Wire, idx 1).
    // Guard anti-cuelgue en AMBOS (lección del RIGHT de R1: un begin() sobre una
    // dirección fantasma CUELGA el bus entero): solo init si hace ACK en SU bus.
    // En Wire el 0x28 queda limpio porque los ToF ya fueron dormidos por predim.
    if (i2c_present_on(Wire2, g_addr[0])) {
        Serial.println("[IMU] Init BNO PRIMARIO (Wire2 24/25 @ 0x28, bus propio sin ToF)...");
        g_ready[0] = init_one_bno(g_bno_primary);
        Serial.println(g_ready[0] ? "[IMU] PRIMARIO OK" : "[IMU] PRIMARIO FAIL (continuando)");
    } else {
        g_ready[0] = false;
        Serial.println("[IMU] PRIMARIO (Wire2 0x28) sin ACK -> SALTEO (bus sano)."
                       " Revisar soldadura/alim del BNO de abajo del Teensy.");
    }
    if (i2c_present_on(Wire, g_addr[1])) {
        Serial.println("[IMU] Init BNO SECUNDARIO (Wire 18/19 @ 0x28, comparte con ToF)...");
        g_ready[1] = init_one_bno(g_bno_secondary);
        Serial.println(g_ready[1] ? "[IMU] SECUNDARIO OK" : "[IMU] SECUNDARIO FAIL (continuando)");
    } else {
        g_ready[1] = false;
        Serial.println("[IMU] SECUNDARIO (Wire 0x28) sin ACK -> SALTEO (bus sano).");
    }
#else
    Serial.println("[IMU] Init BNO055 LEFT (Wire @ 0x28)...");
    g_ready[0] = init_one_bno(g_bno_left);
    Serial.println(g_ready[0] ? "[IMU] LEFT OK" : "[IMU] LEFT FAIL (continuando)");

    // GUARD anti-cuelgue (fix 2026-06-02): el begin() del RIGHT reintenta 3 s; si 0x29
    // esta fantasma/marginal (puente ADR->3V3 flojo, 2do BNO sin alim), ese begin CUELGA
    // el bus I2C entero y se lleva puesto al LEFT (deja de leerse, hdg clavado) y a los
    // ToF (enumeracion 0/4). diag_pose_live nunca tuvo esto porque solo usa el LEFT.
    // Por eso: solo iniciamos el RIGHT si 0x29 HACE ACK (con los ToF ya dormidos por
    // predim, en 0x29 solo puede estar el 2do BNO). Si no contesta, lo salteamos y el
    // bus queda sano -> LEFT + ToF funcionan (degradado a 1 BNO). Cuando se arregle el
    // puente, 0x29 ACKea y el RIGHT entra solo.
    // 0x29 puede ser el 2do BNO (puente ADR) O un ToF que no se durmio (default 0x29,
    // p.ej. por el conflicto pin 10 = LP ToF[1] + dipswitch de rol). Leemos CHIP_ID:
    // BNO055 = 0xA0. SOLO iniciamos el RIGHT si es un BNO real; si no, salteamos (un
    // begin() de BNO sobre un ToF/chip raro CUELGA el bus y mata LEFT + ToF).
    uint8_t id29 = 0;
    const bool ack29  = i2c_present(BNO055_RIGHT_I2C_ADDR);
    const bool read29 = ack29 && read_reg(BNO055_RIGHT_I2C_ADDR, 0x00, id29);
    Serial.print("[IMU] Sondeo 0x29 -> ACK=");
    Serial.print(ack29 ? "SI" : "NO");
    if (ack29) { Serial.print(" chip_id=0x"); Serial.print(id29, HEX); }
    Serial.println();
    if (read29 && id29 == 0xA0) {
        Serial.println("[IMU] 0x29 es BNO real (0xA0). Init BNO055 RIGHT...");
        g_ready[1] = init_one_bno(g_bno_right);
        Serial.println(g_ready[1] ? "[IMU] RIGHT OK" : "[IMU] RIGHT FAIL (continuando)");
    } else {
        g_ready[1] = false;
        Serial.println("[IMU] 0x29 NO es un BNO sano -> SALTEO (no cuelgo el bus).");
        Serial.println("      -> chip_id != 0xA0: hay un ToF SIN DORMIR en 0x29 (revisar"
                       " LP del ToF[1] / dipswitch en pin 10). ACK=NO: 2do BNO ausente/puente.");
    }
#endif  // ROBOT2 / ROBOT1 (init por-BNO)

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
        if (imu_freeze_update(g_freeze[i], hcdeg, now, g_freeze_cfg)) {
            in[i].present = false;   // congelado → fusion lo verá ausente → DEAD
        }
    }
#endif

    imu_fusion_update(g_fusion, g_fcfg, g_scfg, in, dt_s);

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

float sensors_imu_get_disagreement_deg() { return g_fusion.disagreement_deg; }

void sensors_imu_recalibrate_zero() {
    for (int i = 0; i < IMU_FUSION_N; ++i) {
        if (g_ready[i]) g_offset[i] = capture_offset(*g_bno[i]);
    }
    imu_fusion_init(g_fusion);  // limpia estado de fusión (drift, glitch, etc)
#ifdef TOP_ENABLE_BNO_FREEZE_DETECT
    for (int i = 0; i < IMU_FUSION_N; ++i) imu_freeze_reset(g_freeze[i]);
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

}  // namespace iitasoccer
