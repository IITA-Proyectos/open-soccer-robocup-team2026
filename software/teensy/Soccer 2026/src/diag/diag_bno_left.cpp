// diag_bno_left.cpp — Test DIRECTO de UN BNO055 en 0x28 (Wire) SOLO, en crudo.
// (Nota 2026-06-15: ambos BNO están en 0x28 en buses separados; este diag lee solo el
//  de Wire. Ya NO hay ningún BNO en 0x29 — el viejo "2do BNO @ 0x29" fue un error.)
//
// Lee el chip en CRUDO con la lib Adafruit (sin la fusion de sensors_imu) para diagnosticar
// por que el heading queda clavado: muestra en vivo el YAW (Euler), el GIROSCOPIO (dps) y el
// estado de CALIBRACION. Asi se distingue:
//   • yaw cambia al girar           -> el BNO anda; el problema esta en sensors_imu/fusion.
//   • yaw fijo pero gyro(z) cambia  -> la fusion IMUPLUS no integra (modo/lib).
//   • yaw fijo y gyro(z) ~0 girando -> el giroscopio no lee -> chip/lectura muerta.
//   • calib g (gyro) siempre 0      -> el gyro nunca calibro (hay que dejarlo QUIETO al boot).
//   • T (temp) absurda              -> la lectura I2C esta rota.
//
// Duerme los 4 ToF (LP low) para dejar el bus Wire limpio para el BNO.
//
//   pio run -e diag_bno_left -t upload
//   pio device monitor -b 115200
//   -> dejalo QUIETO ~5 s (para que calibre el gyro), DESPUES gira el robot y mira yaw/gyro.
//
// Atribucion: Claude Opus 4.8 (Anthropic), pedido de Gustavo Viollaz (@gviollaz).

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO055.h>

namespace {

constexpr uint8_t BNO_ADDR = 0x28;          // LEFT (ADR flotante)
Adafruit_BNO055 g_bno(55, BNO_ADDR, &Wire);
bool g_ok = false;
elapsedMillis g_since_print;

bool acks(uint8_t a) { Wire.beginTransmission(a); return Wire.endTransmission() == 0; }

}  // namespace

void setup() {
    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }

    Serial.println(F("\n==== diag_bno_left : BNO055 0x28 SOLO (lectura cruda) ===="));

    // Dormir los 4 ToF (LP low) para que no estorben en el bus Wire (18/19).
    const uint8_t lp[] = {9, 10, 11, 12};
    for (uint8_t p : lp) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
    delay(120);

    Wire.begin();
    Wire.setClock(400000);

    Serial.print(F("0x28 ACK: "));
    Serial.println(acks(BNO_ADDR) ? F("SI") : F("NO"));

    Serial.print(F("begin(IMUPLUS)... "));
    uint8_t tries = 0;
    while (!(g_ok = g_bno.begin(OPERATION_MODE_IMUPLUS)) && tries++ < 5) { delay(300); }
    Serial.println(g_ok ? F("OK") : F("FALLO"));
    if (g_ok) {
        g_bno.setExtCrystalUse(true);
        Serial.println(F("Dejalo QUIETO ~5 s (calibra el gyro), DESPUES gira el robot."));
        Serial.println(F("yaw/gyro(z) deberian cambiar al girar; calib g deberia llegar a 3."));
    }
}

void loop() {
    if (!g_ok) {
        if (g_since_print >= 1000) {
            g_since_print = 0;
            Serial.print(F("BNO no init. 0x28 ACK="));
            Serial.println(acks(BNO_ADDR) ? F("SI") : F("NO"));
        }
        return;
    }

    if (g_since_print >= 300) {
        g_since_print = 0;
        const imu::Vector<3> e = g_bno.getVector(Adafruit_BNO055::VECTOR_EULER);      // x=yaw
        const imu::Vector<3> g = g_bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);  // dps
        uint8_t sys = 0, gy = 0, ac = 0, mg = 0;
        g_bno.getCalibration(&sys, &gy, &ac, &mg);
        const int8_t temp = g_bno.getTemp();

        Serial.print(F("yaw="));   Serial.print(e.x(), 1);
        Serial.print(F(" roll=")); Serial.print(e.y(), 1);
        Serial.print(F(" pitch="));Serial.print(e.z(), 1);
        Serial.print(F(" | gyro(z)=")); Serial.print(g.z(), 2); Serial.print(F("dps"));
        Serial.print(F(" (x,y)="));     Serial.print(g.x(), 1); Serial.print(','); Serial.print(g.y(), 1);
        Serial.print(F(" | calib sys/g/a/m=")); Serial.print(sys); Serial.print('/');
        Serial.print(gy); Serial.print('/'); Serial.print(ac); Serial.print('/'); Serial.print(mg);
        Serial.print(F(" | T=")); Serial.print(temp); Serial.println(F("C"));
    }
}
