// ⚠️⚠️ DEPRECADO — NO USAR EN BANCO HOY (arquitectura I2C vieja). ⚠️⚠️
// Este sketch asume el BNO derecho (U11) en Wire1 con REMAP de pines 24/25. Eso
// YA NO ES EL HARDWARE ACTUAL: tras el recableado (2026-05-31) AMBOS BNO viven en
// el bus Wire (I2C0, 18/19) y ademas el BNO derecho (0x29) es una UNIDAD FALLADA
// (el robot corre con 1 solo BNO 0x28). Para verificar los BNO en banco usá el
// VIGENTE: [env:diag_bno_dual_live]. Se conserva solo como referencia historica.
// (Ver memoria hardware_uart_pin_map / FUENTES-DE-VERDAD.)
//
// diag_top_bno.cpp — Verificacion de los 2 BNO055 de la placa TOP.
// NO es firmware de competencia.
//
// Que verifica
// ------------
//   1) Que AMBOS BNO055 respondan e inicialicen:
//        - LEFT  (U10) en Wire  (I2C0, pines 18/19)         addr 0x28
//        - RIGHT (U11) en Wire1 (I2C1, REMAP pines 24/25)   addr 0x28
//      El test viejo (staging/test-bno055-imuplus) solo probaba UNO en Wire.
//      Este prueba los dos buses, que es lo que tiene la placa real.
//   2) ORIENTACION: heading Euler (yaw) en vivo de cada sensor.
//   3) SENTIDO DE GIRO (lo critico): muestra el gyro-Z (velocidad angular en
//      el eje vertical, °/s) Y el delta de heading, para resolver SIN
//      AMBIGUEDAD el signo del giro contra la convencion del firmware.
//   4) DRIFT estatico: cuanto se va el heading con el robot QUIETO (problema
//      historico #2 del analisis BNO).
//   5) DESACUERDO entre los 2 sensores (sanity: deberian coincidir).
//
// Init: replica EXACTAMENTE la del modulo vivo src/top/sensors_imu.cpp
// (IMUPLUS, cristal externo, estabilizacion 1000ms, espera calib gyro 2000ms,
// promedio de 10 lecturas para el cero). Esa init ya esta validada como
// correcta contra docs/internal/giroscopo-bno055-analisis-tecnico.md. Aca se
// instancia local (no se usa sensors_imu.cpp) porque el modulo vivo NO expone
// el gyro-Z crudo, que es justo lo que necesitamos para el sentido de giro.
//
// ============================================================================
// CONVENCION DE GIRO — leer antes de interpretar el output
// ============================================================================
// Nos paramos EN EL LUGAR del robot, mirando al frente (primera persona, como
// un videojuego FPS). "Derecha" = la derecha del robot. "Izquierda" = su izq.
//   • Girar a la DERECHA  = giro HORARIO       visto desde arriba (CW).
//   • Girar a la IZQUIERDA = giro ANTIHORARIO  visto desde arriba (CCW).
//
// Convencion de cancha del firmware (CLAUDE.md / world_model):
//   • heading 0 = robot apunta al ARCO RIVAL (+Y).
//   • localization.cpp (classify_wall) ASUME heading positivo = giro CCW =
//     girar a la IZQUIERDA. Es decir: girar a la izquierda DEBERIA SUBIR el
//     heading. Si este test mide lo contrario, hay que invertir el signo en
//     sensors_imu (o en localization) y se documenta en la convencion canonica.
//
// El BNO055 Euler crudo suele aumentar en sentido HORARIO, y ademas el signo
// depende de como quedo montado el chip (boca arriba/abajo). Por eso NO se
// asume: se MIDE aca y se reporta en estos terminos.
//
// Uso
// ---
//   pio run -e diag_top_bno -t upload
//   pio device monitor -b 115200
// Comandos por serial (enter):
//   z = recapturar el cero (heading offset) de ambos sensores
//   d = correr test de DRIFT (10 s, robot QUIETO)

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <cmath>

namespace {

// Mismas direcciones/buses que el modulo vivo (pinout_common.h).
constexpr uint8_t BNO_ADDR        = 0x28;
constexpr int     WIRE1_SCL       = 24;
constexpr int     WIRE1_SDA       = 25;

constexpr uint32_t INIT_TIMEOUT_MS = 3000;
constexpr uint32_t STABILIZE_MS    = 1000;
constexpr uint32_t GYRO_CALIB_MS   = 2000;
constexpr int      HEADING_SAMPLES = 10;

Adafruit_BNO055 g_left (55, BNO_ADDR, &Wire);
Adafruit_BNO055 g_right(56, BNO_ADDR, &Wire1);

bool  g_left_ok = false, g_right_ok = false;
float g_left_zero = 0.0f, g_right_zero = 0.0f;

float normalize(float h) {
    while (h > 180.0f)  h -= 360.0f;
    while (h < -180.0f) h += 360.0f;
    return h;
}

float raw_yaw(Adafruit_BNO055& bno) {
    // VECTOR_EULER[0] = heading/yaw, identico a event.orientation.x.
    return bno.getVector(Adafruit_BNO055::VECTOR_EULER).x();
}

float gyro_z(Adafruit_BNO055& bno) {
    // VECTOR_GYROSCOPE en dps; eje Z = rotacion alrededor del eje vertical.
    return bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE).z();
}

float capture_zero(Adafruit_BNO055& bno) {
    float sum = 0.0f;
    for (int i = 0; i < HEADING_SAMPLES; ++i) { sum += raw_yaw(bno); delay(20); }
    return sum / HEADING_SAMPLES;
}

// Init que replica sensors_imu.cpp. Devuelve true si el sensor respondio.
bool init_bno(Adafruit_BNO055& bno, const char* tag) {
    Serial.print("[BNO ");
    Serial.print(tag);
    Serial.print("] begin(IMUPLUS)... ");
    const uint32_t t0 = millis();
    while (!bno.begin(OPERATION_MODE_IMUPLUS)) {
        if (millis() - t0 > INIT_TIMEOUT_MS) {
            Serial.println("NO DETECTADO (timeout).");
            return false;
        }
        delay(100);
    }
    bno.setExtCrystalUse(true);
    Serial.print("OK. Estabilizando ");
    Serial.print(STABILIZE_MS);
    Serial.println(" ms (no mover)...");
    delay(STABILIZE_MS);

    Serial.print("[BNO ");
    Serial.print(tag);
    Serial.print("] calibrando gyro (no mover)... ");
    const uint32_t c0 = millis();
    uint8_t sys, gyro, accel, mag;
    while (millis() - c0 < GYRO_CALIB_MS) {
        bno.getCalibration(&sys, &gyro, &accel, &mag);
        if (gyro >= 3) break;
        delay(100);
    }
    bno.getCalibration(&sys, &gyro, &accel, &mag);
    Serial.print("gyro_cal=");
    Serial.println(gyro);
    return true;
}

void drift_test() {
    if (!g_left_ok && !g_right_ok) return;
    Serial.println("\n=== TEST DRIFT (10 s, NO MOVER el robot) ===");
    const float l0 = g_left_ok  ? normalize(raw_yaw(g_left)  - g_left_zero)  : 0;
    const float r0 = g_right_ok ? normalize(raw_yaw(g_right) - g_right_zero) : 0;
    for (int s = 1; s <= 10; ++s) {
        delay(1000);
        Serial.print("  t=");
        Serial.print(s);
        Serial.print("s  ");
        if (g_left_ok) {
            Serial.print("L drift=");
            Serial.print(normalize(raw_yaw(g_left) - g_left_zero) - l0, 2);
            Serial.print("deg  ");
        }
        if (g_right_ok) {
            Serial.print("R drift=");
            Serial.print(normalize(raw_yaw(g_right) - g_right_zero) - r0, 2);
            Serial.print("deg");
        }
        Serial.println();
    }
    Serial.println("=== fin drift (referencia: IMUPLUS ~0.5-5 deg/min es normal) ===\n");
}

void print_header() {
    Serial.println("\n============================================================");
    Serial.println("  TOP — Verificacion BNO055 dual (orientacion + sentido giro)");
    Serial.println("============================================================");
    Serial.println("CONVENCION (primera persona, parado COMO el robot):");
    Serial.println("  derecha = giro HORARIO (CW)   |  izquierda = giro ANTIHORARIO (CCW)");
    Serial.println("  El firmware (localization) espera: IZQUIERDA sube el heading (+).");
    Serial.println("");
    Serial.println("QUE HACER:");
    Serial.println("  1) Dejar quieto: heading ~0 y gyroZ ~0.");
    Serial.println("  2) Girar el robot ~90 a la DERECHA y observar:");
    Serial.println("       - signo de gyroZ MIENTRAS girás a la derecha");
    Serial.println("       - si el heading SUBE o BAJA");
    Serial.println("  3) Idem a la IZQUIERDA.");
    Serial.println("  Reportá esos signos: con eso fijamos la convencion del firmware.");
    Serial.println("Comandos: 'z'=recapturar cero, 'd'=test drift 10s.");
    Serial.println("------------------------------------------------------------");
}

}  // namespace

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) {}

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    Wire.begin();
    Wire.setClock(400000);
    Wire1.setSCL(WIRE1_SCL);
    Wire1.setSDA(WIRE1_SDA);
    Wire1.begin();
    Wire1.setClock(400000);

    Serial.println("\n### Init BNO055 LEFT (Wire / I2C0 / 18-19) ###");
    g_left_ok = init_bno(g_left, "LEFT");
    Serial.println(g_left_ok ? "  -> LEFT OK" : "  -> LEFT FAIL (sigo)");

    Serial.println("\n### Init BNO055 RIGHT (Wire1 / I2C1 / 24-25 remap) ###");
    g_right_ok = init_bno(g_right, "RIGHT");
    Serial.println(g_right_ok ? "  -> RIGHT OK" : "  -> RIGHT FAIL (sigo)");

    if (g_left_ok)  g_left_zero  = capture_zero(g_left);
    if (g_right_ok) g_right_zero = capture_zero(g_right);

    Serial.print("\nResumen init:  LEFT=");
    Serial.print(g_left_ok ? "OK" : "FAIL");
    Serial.print("   RIGHT=");
    Serial.println(g_right_ok ? "OK" : "FAIL");
    if (!g_left_ok && !g_right_ok) {
        Serial.println("ATENCION: ningun BNO respondio. Revisar I2C/alimentacion.");
        Serial.println("  - LEFT  usa Wire  (18/19). RIGHT usa Wire1 (24/25 remap).");
        Serial.println("  - Ambos en 0x28; verificar con diag_top_i2c_scan.");
    }
    print_header();
}

void loop() {
    // Comandos por serial.
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'z' || c == 'Z') {
            if (g_left_ok)  g_left_zero  = capture_zero(g_left);
            if (g_right_ok) g_right_zero = capture_zero(g_right);
            Serial.println(">> cero recapturado en ambos sensores.");
        } else if (c == 'd' || c == 'D') {
            drift_test();
        }
    }

    static uint32_t last = 0;
    if (millis() - last < 100) return;  // 10 Hz
    last = millis();

    float lh = 0, lz = 0, rh = 0, rz = 0;
    uint8_t ls = 0, lg = 0, la = 0, lm = 0, rs = 0, rg = 0, ra = 0, rm = 0;
    if (g_left_ok) {
        lh = normalize(raw_yaw(g_left) - g_left_zero);
        lz = gyro_z(g_left);
        g_left.getCalibration(&ls, &lg, &la, &lm);
    }
    if (g_right_ok) {
        rh = normalize(raw_yaw(g_right) - g_right_zero);
        rz = gyro_z(g_right);
        g_right.getCalibration(&rs, &rg, &ra, &rm);
    }

    // Linea compacta. gyroZ con signo: el usuario correlaciona con el giro fisico.
    if (g_left_ok) {
        Serial.print("L hdg=");
        if (lh >= 0) Serial.print('+');
        Serial.print(lh, 1);
        Serial.print(" gyroZ=");
        if (lz >= 0) Serial.print('+');
        Serial.print(lz, 1);
        Serial.print("/s cal(g)=");
        Serial.print(lg);
        Serial.print("  ");
    } else {
        Serial.print("L --  ");
    }
    if (g_right_ok) {
        Serial.print("| R hdg=");
        if (rh >= 0) Serial.print('+');
        Serial.print(rh, 1);
        Serial.print(" gyroZ=");
        if (rz >= 0) Serial.print('+');
        Serial.print(rz, 1);
        Serial.print("/s cal(g)=");
        Serial.print(rg);
    } else {
        Serial.print("| R --");
    }
    if (g_left_ok && g_right_ok) {
        Serial.print("  | dif=");
        Serial.print(std::abs(normalize(lh - rh)), 1);
    }

    // Pista de sentido de giro en vivo: si gira fuerte, decir hacia donde sube.
    if (g_left_ok && std::abs(lz) > 20.0f) {
        Serial.print(lz > 0 ? "  <<gyroZ+ >>" : "  <<gyroZ- >>");
    }
    Serial.println();

    digitalWrite(LED_BUILTIN, (g_left_ok || g_right_ok) ? HIGH : ((millis() / 100) % 2));
}
