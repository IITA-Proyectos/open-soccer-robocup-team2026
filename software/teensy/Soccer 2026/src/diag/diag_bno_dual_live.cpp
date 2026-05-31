// diag_bno_dual_live.cpp — Validación de los 2 BNO055 + fusión de heading.
// NO es firmware de competencia.
//
// Por qué existe
// -------------
// diag_top_bno (ya en el repo) probó UN BNO y midió el sentido de giro
// (resultado banco 2026-05-31: el chip da yaw CW-positivo; el firmware lo
// invierte con HEADING_SIGN=-1 en sensors_imu.cpp para que IZQUIERDA suba el
// heading = convención CCW de la cancha).
//
// ARQUITECTURA NUEVA (recableado 2026-05-31, confirmada en banco):
//   Los 2 BNO055 estan en el MISMO bus Wire (18/19) — NO en buses separados.
//   LEFT  = 0x28 (ADR a GND/flotante)
//   RIGHT = 0x29 (ADR puenteado a 3V3)   <-- verificado con diag_bno_addr_check
//   Esto LIBERA Wire1 (24/25) para la comunicacion con la placa DOWN.
//
// ⚠️ 0x29 lo comparten el BNO RIGHT y los 4 ToF (direccion de fabrica del
//    VL53L7CX). Para que el BNO en 0x29 no colisione con los ToF, este diag
//    DUERME los 4 ToF (LP pins 9/10/11/12 en LOW) antes de hablar el bus.
//
// Valida, SIN depender de localización:
//   1) Que ambos inicialicen (LEFT 0x28 + RIGHT 0x29, ambos en Wire).
//   2) Heading de cada uno + heading FUSIONADO (promedio circular, maneja el
//      wraparound -179/+179 con atan2).
//   3) DESACUERDO entre ambos (|L-R| circular). Sano: < ~5° en reposo.
//   4) Detección simple de IMPACTO: si el desacuerdo SALTA > 15° en un ciclo.
//   5) Drift en reposo (comando 'd', 20 s) por sensor y del fusionado.
//
// CONVENCIÓN DE GIRO (idéntica al resto del firmware, primera persona):
//   derecha = horario (CW)   |  izquierda = antihorario (CCW)
//   El heading que mostramos aquí YA está invertido a CCW (igual que el firmware):
//   girar a la IZQUIERDA SUBE el heading; a la DERECHA lo BAJA.
//   (El chip crudo es CW+, aplicamos HEADING_SIGN = -1 igual que sensors_imu.)
//
// Este diag instancia los BNO localmente (no usa sensors_imu.cpp) para poder
// mostrar el yaw + gyroZ crudos de cada sensor por separado y el fusionado —
// cosas que la API del módulo vivo no expone. La init replica la validada.
//
// Uso
// ---
//   pio run -e diag_bno_dual_live -t upload
//   pio device monitor -b 115200
// Comandos serial: z = recapturar cero de ambos | d = test de drift 20 s

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <math.h>

namespace {

// Arquitectura nueva: AMBOS BNO en el MISMO bus Wire, direcciones distintas.
constexpr uint8_t  BNO_ADDR_LEFT   = 0x28;   // ADR a GND/flotante
constexpr uint8_t  BNO_ADDR_RIGHT  = 0x29;   // ADR puenteado a 3V3

// LP de los 4 ToF — los dormimos para que no aparezcan en 0x29 y colisionen
// con el BNO RIGHT. Activo-alto: LOW = ToF dormido. (Pines confirmados banco.)
constexpr uint8_t  TOF_LP_PIN[4]   = {9, 10, 11, 12};
constexpr uint8_t  TOF_SLEEP_LVL   = LOW;

constexpr uint32_t INIT_TIMEOUT_MS = 3000;
constexpr uint32_t STABILIZE_MS    = 1000;
constexpr uint32_t GYRO_CALIB_MS   = 2000;
constexpr int      ZERO_SAMPLES    = 10;

// Signo medido en banco: chip CW+, firmware usa -1 para pasar a CCW+.
constexpr float    HEADING_SIGN    = -1.0f;

// Umbrales de diagnóstico.
constexpr float    DISAGREE_WARN_DEG  = 5.0f;    // desacuerdo sano en reposo
constexpr float    IMPACT_JUMP_DEG    = 15.0f;   // salto de desacuerdo = impacto

Adafruit_BNO055 g_left (55, BNO_ADDR_LEFT,  &Wire);
Adafruit_BNO055 g_right(56, BNO_ADDR_RIGHT, &Wire);  // mismo bus, 0x29

bool  g_left_ok = false, g_right_ok = false;
float g_left_zero = 0.0f, g_right_zero = 0.0f;
float g_prev_disagree = 0.0f;
bool  g_have_prev_disagree = false;

float normalize(float h) {
    while (h > 180.0f)  h -= 360.0f;
    while (h < -180.0f) h += 360.0f;
    return h;
}

// Diferencia circular a-b en [-180,180] (NO restar crudo: rompe en ±180).
float heading_diff(float a, float b) { return normalize(a - b); }

// Promedio circular de dos headings (maneja wraparound con atan2).
float fuse_circular(float a_deg, float b_deg) {
    const float ar = a_deg * (float)M_PI / 180.0f;
    const float br = b_deg * (float)M_PI / 180.0f;
    const float s = sinf(ar) + sinf(br);
    const float c = cosf(ar) + cosf(br);
    return atan2f(s, c) * 180.0f / (float)M_PI;
}

float raw_yaw(Adafruit_BNO055& bno) {
    return bno.getVector(Adafruit_BNO055::VECTOR_EULER).x();
}
float gyro_z(Adafruit_BNO055& bno) {
    return bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE).z();
}

// Heading en convención del firmware (CCW+, relativo al cero capturado).
float heading_of(Adafruit_BNO055& bno, float zero) {
    return normalize(HEADING_SIGN * (raw_yaw(bno) - zero));
}

float capture_zero(Adafruit_BNO055& bno) {
    float sum = 0.0f;
    for (int i = 0; i < ZERO_SAMPLES; ++i) { sum += raw_yaw(bno); delay(20); }
    return sum / ZERO_SAMPLES;
}

bool init_bno(Adafruit_BNO055& bno, const char* tag) {
    Serial.print("[BNO ");
    Serial.print(tag);
    Serial.print("] begin(IMUPLUS)... ");
    const uint32_t t0 = millis();
    while (!bno.begin(OPERATION_MODE_IMUPLUS)) {
        if (millis() - t0 > INIT_TIMEOUT_MS) { Serial.println("NO DETECTADO."); return false; }
        delay(100);
    }
    bno.setExtCrystalUse(true);
    delay(STABILIZE_MS);
    const uint32_t c0 = millis();
    uint8_t sys, gyro, accel, mag;
    while (millis() - c0 < GYRO_CALIB_MS) {
        bno.getCalibration(&sys, &gyro, &accel, &mag);
        if (gyro >= 3) break;
        delay(100);
    }
    bno.getCalibration(&sys, &gyro, &accel, &mag);
    Serial.print("OK (gyro_cal=");
    Serial.print(gyro);
    Serial.println(")");
    return true;
}

void drift_test() {
    if (!g_left_ok && !g_right_ok) return;
    Serial.println("\n=== DRIFT 20 s — NO MOVER el robot ===");
    const float l0 = g_left_ok  ? heading_of(g_left,  g_left_zero)  : 0;
    const float r0 = g_right_ok ? heading_of(g_right, g_right_zero) : 0;
    for (int s = 1; s <= 20; ++s) {
        delay(1000);
        Serial.print("  t=");
        Serial.print(s);
        Serial.print("s ");
        if (g_left_ok)  { Serial.print(" L_drift="); Serial.print(heading_diff(heading_of(g_left,  g_left_zero),  l0), 2); }
        if (g_right_ok) { Serial.print(" R_drift="); Serial.print(heading_diff(heading_of(g_right, g_right_zero), r0), 2); }
        Serial.println(" deg");
    }
    Serial.println("=== fin drift (IMUPLUS: ~0.5-5 deg/min es normal) ===\n");
}

void header() {
    Serial.println("\n============================================================");
    Serial.println("  TOP — BNO055 DUAL en vivo + fusion de heading");
    Serial.println("============================================================");
    Serial.println("Heading en convencion CCW (izquierda SUBE, derecha BAJA).");
    Serial.println("Sano en reposo: |disagree| < 5 deg.  impact=1 = salto brusco.");
    Serial.println("Girar 90 a la IZQUIERDA -> heading sube ~+90 en ambos.");
    Serial.println("Comandos: z=recapturar cero, d=drift 20s.");
    Serial.println("------------------------------------------------------------");
}

}  // namespace

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) {}
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // Dormir los 4 ToF ANTES de hablar el bus: comparten 0x29 con el BNO RIGHT.
    for (uint8_t i = 0; i < 4; ++i) {
        pinMode(TOF_LP_PIN[i], OUTPUT);
        digitalWrite(TOF_LP_PIN[i], TOF_SLEEP_LVL);
    }
    delay(60);

    Wire.begin();
    Wire.setClock(400000);

    Serial.println("\n### Init BNO LEFT (Wire / 18-19 @ 0x28) ###");
    g_left_ok = init_bno(g_left, "LEFT");
    Serial.println("\n### Init BNO RIGHT (Wire / 18-19 @ 0x29) ###");
    g_right_ok = init_bno(g_right, "RIGHT");

    if (g_left_ok)  g_left_zero  = capture_zero(g_left);
    if (g_right_ok) g_right_zero = capture_zero(g_right);

    Serial.print("\nResumen: LEFT=");
    Serial.print(g_left_ok ? "OK" : "FAIL");
    Serial.print("  RIGHT=");
    Serial.println(g_right_ok ? "OK" : "FAIL");
    if (g_left_ok && g_right_ok)
        Serial.println("-> 2 BNO (0x28 + 0x29): fusion + deteccion de impacto ACTIVAS.");
    else if (g_left_ok || g_right_ok)
        Serial.println("-> 1 BNO: degradado (sin fusion). Revisar el otro (0x28/0x29).");
    else
        Serial.println("-> NINGUN BNO. Revisar I2C/alimentacion (diag_top_i2c_scan).");
    header();
}

void loop() {
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'z' || c == 'Z') {
            if (g_left_ok)  g_left_zero  = capture_zero(g_left);
            if (g_right_ok) g_right_zero = capture_zero(g_right);
            g_have_prev_disagree = false;
            Serial.println(">> cero recapturado.");
        } else if (c == 'd' || c == 'D') {
            drift_test();
        }
    }

    static uint32_t last = 0;
    if (millis() - last < 100) return;   // 10 Hz
    last = millis();

    float lh = 0, rh = 0, lz = 0, rz = 0;
    uint8_t lcal = 0, rcal = 0, s = 0, a = 0, m = 0;
    if (g_left_ok)  { lh = heading_of(g_left,  g_left_zero);  lz = gyro_z(g_left);  g_left.getCalibration(&s, &lcal, &a, &m); }
    if (g_right_ok) { rh = heading_of(g_right, g_right_zero); rz = gyro_z(g_right); g_right.getCalibration(&s, &rcal, &a, &m); }

    // Línea por sensor.
    if (g_left_ok)  { Serial.print("L hdg="); if (lh>=0) Serial.print('+'); Serial.print(lh,1); Serial.print(" gZ="); Serial.print(lz,0); Serial.print(" c"); Serial.print(lcal); }
    else            { Serial.print("L --"); }
    if (g_right_ok) { Serial.print(" | R hdg="); if (rh>=0) Serial.print('+'); Serial.print(rh,1); Serial.print(" gZ="); Serial.print(rz,0); Serial.print(" c"); Serial.print(rcal); }
    else            { Serial.print(" | R --"); }

    // Fusión + diagnóstico (solo con los 2).
    if (g_left_ok && g_right_ok) {
        const float fused = fuse_circular(lh, rh);
        const float disagree = fabsf(heading_diff(lh, rh));
        Serial.print(" | FUSED="); if (fused>=0) Serial.print('+'); Serial.print(fused,1);
        Serial.print(" disagree="); Serial.print(disagree,1);
        if (disagree > DISAGREE_WARN_DEG) Serial.print(" [!]");

        bool impact = false;
        if (g_have_prev_disagree && fabsf(disagree - g_prev_disagree) > IMPACT_JUMP_DEG) impact = true;
        g_prev_disagree = disagree;
        g_have_prev_disagree = true;
        if (impact) Serial.print("  IMPACT=1");
    }
    Serial.println();

    digitalWrite(LED_BUILTIN, (g_left_ok || g_right_ok) ? HIGH : ((millis()/100)%2));
}
