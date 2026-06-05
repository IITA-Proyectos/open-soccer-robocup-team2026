// diag_pose_live.cpp — Pose (x,y,heading) en vivo: 4 ToF + BNO -> localization.
// NO es firmware de competencia.
//
// Qué hace
// --------
// Junta TODA la cadena de localización en banco, SIN el CENTRAL ni main_top:
//   1) Enumera los 4 ToF en bus único (LP {9,10,11,12}, dir 0x2A..0x2D) — mismo
//      patrón validado en diag_top_tof_quad_live.
//   2) Lee 1 BNO (LEFT, Wire 0x28) para el heading, en convención CCW del
//      firmware (HEADING_SIGN=-1, igual que sensors_imu.cpp).
//   3) Llama al ALGORITMO REAL del repo: localization_compute() de
//      src/shared/localization.cpp (no se reimplementa nada — se testea el
//      código que va a correr en competencia).
//   4) Imprime pose=(x,y) mm, heading, valid, y qué ToF aportó a cada eje.
//
// Sirve para el test de aceptación de la Capa 1 (XY con 4 ToF): poner el robot
// en posiciones conocidas de la cancha medidas con cinta y comparar.
//
// Mapeo físico (confirmado banco 2026-05-31, pinout_common.h):
//   ToF idx 0=FRENTE(+Y,0°)  1=ATRAS(-Y,180°)  2=DERECHA(+X,270°)  3=IZQ(-X,90°)
// Cancha: X (lateral, corto) = 1820 mm, Y (arco-a-arco, largo) = 2430 mm. heading 0 = mira arco rival (+Y).
//
// PROCEDIMIENTO (las direcciones I2C de los ToF PERSISTEN — power-cycle!):
//   1) pio run -e diag_pose_live -t upload
//   2) QUITAR energía de la placa y reponer (no alcanza reset).
//   3) Apuntar el robot al ARCO RIVAL (+Y) y mandar 'z' por serial (captura cero).
//   4) pio device monitor -b 115200
//
// Comandos serial: z = recapturar offset del BNO (robot debe mirar +Y).
//
// Uso:
//   pio run -e diag_pose_live -t upload ; pio device monitor -b 115200

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_VL53L7CX.h>
#include <math.h>

#include "localization.h"   // algoritmo REAL del repo (shared/, host-safe)

using namespace iitasoccer;

namespace {

// Sentinels locales (sensors_tof.h los define igual, pero ese header arrastra
// config_top.h/HAL que exige -DROBOT1/2; este diag es autonomo, no lo usa).
constexpr uint16_t TOF_NO_READING   = 0xFFFF;
constexpr uint16_t TOF_MAX_RANGE_MM = 4000;

// ---- Cancha / montaje (espejo de pinout_common.h; este diag no usa el HAL) ----
constexpr uint16_t FIELD_WIDTH_MM  = 1820;   // X (lateral, corto) — ver pinout_common.h
constexpr uint16_t FIELD_HEIGHT_MM = 2430;   // Y (arco-a-arco, largo) — corregido 2026-06-03
constexpr uint16_t MOUNT_ANGLE[4]  = {0, 180, 270, 90};  // frente,atras,der,izq
constexpr uint16_t OUTLIER_MM      = 300;
const char*        POS_LABEL[4]    = {"FRENTE", "ATRAS ", "DERECHA", "IZQ   "};

// ---- ToF (mismo patrón que diag_top_tof_quad_live) ----
constexpr uint8_t  LP_PIN[4]      = {9, 10, 11, 12};
constexpr uint8_t  TOF_ADDR[4]    = {0x2A, 0x2B, 0x2C, 0x2D};
constexpr uint8_t  TOF_DEFAULT    = 0x29;
constexpr uint8_t  RES_ZONES      = 16;   // 4x4 (liviano; suficiente para distancia media)
constexpr uint8_t  RANGE_HZ       = 15;
constexpr uint8_t  WAKE = HIGH, SLEEPL = LOW;
constexpr uint32_t LP_SETTLE_MS   = 120;

// ---- BNO ----
constexpr uint8_t  BNO_ADDR       = 0x28;
constexpr float    HEADING_SIGN   = -1.0f;  // chip CW+ -> firmware CCW+
constexpr uint32_t INIT_TIMEOUT_MS= 3000;
constexpr uint32_t STABILIZE_MS   = 1000;
constexpr uint32_t GYRO_CALIB_MS  = 2000;
constexpr int      ZERO_SAMPLES   = 10;

Adafruit_BNO055      g_bno(55, BNO_ADDR, &Wire);
bool                 g_bno_ok = false;
float                g_bno_zero = 0.0f;

Adafruit_VL53L7CX    g_tof[4];
VL53L7CX_ResultsData g_res;
bool                 g_tof_ok[4] = {false, false, false, false};
uint16_t             g_dist[4]   = {TOF_NO_READING, TOF_NO_READING, TOF_NO_READING, TOF_NO_READING};

LocalizationConfig   g_cfg;

bool acks(uint8_t a) { Wire.beginTransmission(a); return Wire.endTransmission() == 0; }

void set_all_lp(uint8_t lvl) { for (uint8_t i = 0; i < 4; ++i) digitalWrite(LP_PIN[i], lvl); }

bool raw_change_addr(uint8_t cur, uint8_t next) {
    Wire.beginTransmission(cur); Wire.write(0x7F); Wire.write(0xFF); Wire.write(0x00);
    if (Wire.endTransmission() != 0) return false;
    Wire.beginTransmission(cur); Wire.write(0x00); Wire.write(0x04); Wire.write(next);
    if (Wire.endTransmission() != 0) return false;
    delay(5);
    return acks(next);
}

void recover_to_default() {
    set_all_lp(WAKE);
    delay(60);
    const uint8_t dispersed[] = {0x2A, 0x2B, 0x2C, 0x2D, 0x60};
    for (uint8_t k = 0; k < sizeof(dispersed); ++k)
        for (uint8_t t = 0; t < 4 && acks(dispersed[k]); ++t)
            raw_change_addr(dispersed[k], TOF_DEFAULT);
}

uint16_t mean_valid(const VL53L7CX_ResultsData& r, uint8_t n) {
    uint32_t sum = 0; uint16_t cnt = 0;
    for (uint8_t i = 0; i < n; ++i) {
        uint8_t s = r.target_status[i];
        int16_t mm = r.distance_mm[i];
        if ((s == 5 || s == 6 || s == 9) && mm >= 0 && mm <= (int16_t)TOF_MAX_RANGE_MM) { sum += mm; ++cnt; }
    }
    return cnt ? (uint16_t)(sum / cnt) : TOF_NO_READING;
}

float normalize180(float h) { while (h > 180) h -= 360; while (h < -180) h += 360; return h; }
float raw_yaw() { return g_bno.getVector(Adafruit_BNO055::VECTOR_EULER).x(); }

// Heading firmware (CCW+) en centideg [0,36000) que espera localization.
int16_t heading_centideg() {
    float h = normalize180(HEADING_SIGN * (raw_yaw() - g_bno_zero));  // [-180,180]
    if (h < 0) h += 360.0f;                                          // [0,360)
    return (int16_t)(h * 100.0f);
}

float capture_zero() {
    float sum = 0; for (int i = 0; i < ZERO_SAMPLES; ++i) { sum += raw_yaw(); delay(20); }
    return sum / ZERO_SAMPLES;
}

bool init_bno() {
    Serial.print("[BNO LEFT] begin(IMUPLUS)... ");
    const uint32_t t0 = millis();
    while (!g_bno.begin(OPERATION_MODE_IMUPLUS)) {
        if (millis() - t0 > INIT_TIMEOUT_MS) { Serial.println("NO DETECTADO."); return false; }
        delay(100);
    }
    g_bno.setExtCrystalUse(true);
    delay(STABILIZE_MS);
    const uint32_t c0 = millis();
    uint8_t sys, gyro, accel, mag;
    while (millis() - c0 < GYRO_CALIB_MS) { g_bno.getCalibration(&sys, &gyro, &accel, &mag); if (gyro >= 3) break; delay(100); }
    Serial.println("OK");
    return true;
}

void enumerate_tofs() {
    Serial.println("Recuperando ToF dispersos -> 0x29 ...");
    recover_to_default();
    set_all_lp(SLEEPL);
    delay(LP_SETTLE_MS);
    for (uint8_t i = 0; i < 4; ++i) {
        Serial.print("[ToF ");
        Serial.print(POS_LABEL[i]);
        Serial.print(" LP");
        Serial.print(LP_PIN[i]);
        Serial.print("] ");
        digitalWrite(LP_PIN[i], WAKE);
        delay(LP_SETTLE_MS);
        if (!acks(TOF_DEFAULT)) { Serial.println("no aparecio en 0x29 -> salteo"); digitalWrite(LP_PIN[i], SLEEPL); continue; }
        if (!g_tof[i].begin(TOF_DEFAULT, &Wire, 400000)) { Serial.println("begin() fallo -> salteo"); continue; }
        if (!g_tof[i].setAddress(TOF_ADDR[i])) { Serial.println("setAddress() fallo -> salteo"); continue; }
        g_tof[i].setResolution(RES_ZONES);
        g_tof[i].setRangingFrequency(RANGE_HZ);
        if (!g_tof[i].startRanging()) { Serial.println("startRanging() fallo -> salteo"); continue; }
        g_tof_ok[i] = true;
        Serial.print("OK @0x"); Serial.println(TOF_ADDR[i], HEX);
    }
}

void header() {
    Serial.println("\n============================================================");
    Serial.println("  TOP — POSE EN VIVO (4 ToF + BNO -> localization_compute)");
    Serial.println("============================================================");
    Serial.println("Cancha: X(lateral,corto)=1820mm  Y(arco-a-arco,largo)=2430mm. Origen en una esquina.");
    Serial.println("heading 0 = mira arco rival (+Y), CCW+ (izquierda sube).");
    Serial.println("Centro de cancha ~ (910, 1215).");
    Serial.println("IMPORTANTE: apunta robot a +Y y manda 'z' para fijar el cero.");
    Serial.println("------------------------------------------------------------");
}

}  // namespace

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) {}
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    for (uint8_t i = 0; i < 4; ++i) { pinMode(LP_PIN[i], OUTPUT); digitalWrite(LP_PIN[i], SLEEPL); }
    Wire.begin();
    Wire.setClock(400000);

    Serial.println("\n=== diag_pose_live ===");
    Serial.print("BNO055 (0x28): ");
    Serial.println(acks(BNO_ADDR) ? "PRESENTE" : "AUSENTE (revisar bus!)");

    g_bno_ok = init_bno();
    if (g_bno_ok) g_bno_zero = capture_zero();
    enumerate_tofs();

    // Config de localization (espejo de localization_runtime_init).
    g_cfg.field_width_mm       = FIELD_WIDTH_MM;
    g_cfg.field_height_mm      = FIELD_HEIGHT_MM;
    g_cfg.outlier_threshold_mm = OUTLIER_MM;
    for (uint8_t i = 0; i < 4; ++i) g_cfg.tof_mount_angle_deg[i] = MOUNT_ANGLE[i];
    g_cfg.bno_offset_centideg  = 0;   // ya restamos el cero en heading_centideg()
    g_cfg.prev_x_mm = FIELD_WIDTH_MM / 2;
    g_cfg.prev_y_mm = FIELD_HEIGHT_MM / 2;
    g_cfg.prev_valid = false;

    int ok = 0; for (uint8_t i = 0; i < 4; ++i) if (g_tof_ok[i]) ++ok;
    Serial.print("\nToF midiendo: "); Serial.print(ok); Serial.println(" de 4.");
    Serial.println(g_bno_ok ? "BNO OK." : "BNO FAIL -> heading = 0.");
    header();
}

void loop() {
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'z' || c == 'Z') {
            if (g_bno_ok) g_bno_zero = capture_zero();
            g_cfg.prev_valid = false;
            Serial.println(">> cero del BNO recapturado (robot debe mirar +Y).");
        }
    }

    // Refrescar distancias de los ToF.
    for (uint8_t i = 0; i < 4; ++i) {
        if (!g_tof_ok[i]) continue;
        if (g_tof[i].isDataReady() && g_tof[i].getRangingData(&g_res))
            g_dist[i] = mean_valid(g_res, RES_ZONES);
    }

    static uint32_t last = 0;
    if (millis() - last < 200) return;   // 5 Hz para el monitor
    last = millis();

    // Armar inputs y llamar al algoritmo REAL.
    LocalizationInputs in{};
    for (uint8_t i = 0; i < 4; ++i) {
        in.tof_distance_mm[i] = g_dist[i];
        in.tof_valid[i] = g_tof_ok[i] && (g_dist[i] != TOF_NO_READING);
    }
    in.bno_heading_centideg = g_bno_ok ? heading_centideg() : 0;

    LocalizationPose pose = localization_compute(in, g_cfg);

    if (pose.valid) {
        g_cfg.prev_x_mm = pose.x_mm;
        g_cfg.prev_y_mm = pose.y_mm;
        g_cfg.prev_valid = true;
    }

    // Imprimir: distancias crudas + pose.
    Serial.print("d[F,A,D,I]=");
    for (uint8_t i = 0; i < 4; ++i) {
        if (in.tof_valid[i]) Serial.print(g_dist[i]); else Serial.print("----");
        Serial.print(i < 3 ? "," : "  ");
    }
    Serial.print("hdg=");
    Serial.print(in.bno_heading_centideg / 100.0f, 1);
    Serial.print("  POSE=");
    if (pose.valid) {
        Serial.print("("); Serial.print(pose.x_mm); Serial.print(","); Serial.print(pose.y_mm); Serial.print(")mm");
        Serial.print(" src=0b");
        for (int b = 3; b >= 0; --b) Serial.print((pose.source_flags >> b) & 1);
    } else {
        Serial.print("INVALIDA (falta TOF en algun eje)");
    }
    Serial.println();

    digitalWrite(LED_BUILTIN, pose.valid ? HIGH : ((millis()/250)%2));
}
