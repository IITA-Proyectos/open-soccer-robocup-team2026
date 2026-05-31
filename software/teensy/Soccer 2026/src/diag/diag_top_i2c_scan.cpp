// diag_top_i2c_scan.cpp — Escaner I2C de los DOS buses de la placa TOP.
//
// NO es firmware de competencia. Es el PRIMER paso de bring-up de los ToF:
// antes de enumerar (reasignar direcciones), hay que ver QUE responde hoy en
// cada bus, sin tocar ningun pin LPn/XSHUT.
//
// Que hace
// --------
// Escanea direcciones 0x08..0x77 en:
//   • Wire  (I2C0) = pines 18 (SDA0) / 19 (SCL0)   -> aca viven U2 (frontal) + U3 (trasero) + 1 BNO
//   • Wire1 (I2C1) = pines 25 (SDA1) / 24 (SCL1)   -> aca viven U5 (izq) + U17 (der) + (otro BNO si esta)
//   (topologia confirmada por extraccion forense del schematic 2026-04-12)
//
// Como leer la salida
// -------------------
//   0x28 = BNO055 (IMU) en su direccion default.
//   0x29 = VL53L7CX (ToF) en su direccion DEFAULT de fabrica.
//          OJO: los 4 ToF salen de fabrica en 0x29. Si hay 2 ToF en el MISMO
//          bus, ambos contestan a 0x29 -> el escaner ve UN ACK en 0x29 pero
//          NO puede distinguir cuantos sensores hay detras. Esa ambiguedad es
//          EXACTAMENTE la razon por la que hay que enumerar (dar a cada uno
//          una direccion distinta con el pin LPn). Este escaner confirma
//          "hay al menos 1 ToF vivo en este bus", no "hay 2".
//   0x2A/0x2B/0x2C = ToF YA reasignados (si corres esto despues de enumerar).
//
// Uso
// ---
//   pio run -e diag_top_i2c_scan -t upload
//   pio device monitor -b 115200
//
// No depende del HAL ni de -DROBOT: usa los numeros de pin crudos del bus,
// asi que compila igual para arquero y delantero (los buses son iguales por
// hardware).

#include <Arduino.h>
#include <Wire.h>

namespace {

// Pines del remap de Wire1 en la placa TOP (pinout_common.h: SCL1=24, SDA1=25).
constexpr uint8_t WIRE1_SDA = 25;
constexpr uint8_t WIRE1_SCL = 24;

// Devuelve una etiqueta humana para las direcciones que esperamos ver.
const char* addr_hint(uint8_t addr) {
    switch (addr) {
        case 0x28: return "BNO055 (IMU) default";
        case 0x29: return "VL53L7CX (ToF) default  <-- 1 o MAS ToF aca";
        case 0x2A: return "ToF reasignado (post-enumeracion)";
        case 0x2B: return "ToF reasignado (post-enumeracion)";
        case 0x2C: return "ToF reasignado (post-enumeracion)";
        default:   return "desconocido";
    }
}

// Escanea un bus y reporta cada direccion que hace ACK.
int scan_bus(TwoWire& bus, const char* name) {
    Serial.print("\n--- Escaneando ");
    Serial.print(name);
    Serial.println(" (0x08..0x77) ---");
    int found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
        bus.beginTransmission(addr);
        const uint8_t err = bus.endTransmission();  // 0 = ACK (dispositivo presente)
        if (err == 0) {
            ++found;
            Serial.print("  0x");
            if (addr < 0x10) Serial.print('0');
            Serial.print(addr, HEX);
            Serial.print("  ACK  -> ");
            Serial.println(addr_hint(addr));
        }
    }
    if (found == 0) {
        Serial.println("  (nada respondio en este bus)");
    } else {
        Serial.print("  Total dispositivos que hacen ACK en ");
        Serial.print(name);
        Serial.print(": ");
        Serial.println(found);
    }
    return found;
}

}  // namespace

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) { /* esperar USB hasta 3s */ }

    Serial.println("\n=========================================");
    Serial.println("  TOP — Escaner I2C de los 2 buses");
    Serial.println("  Wire (18/19)  +  Wire1 (25/24 remap)");
    Serial.println("=========================================");

    // Bus 0 (Wire) — pines 18/19 por hardware, no hace falta remap.
    Wire.begin();
    Wire.setClock(400000);

    // Bus 1 (Wire1) — remap a 25/24. ORDEN CRITICO en Teensy: setSCL/setSDA
    // DEBEN ir ANTES de begin(), si no el remap no toma efecto y Wire1 usa los
    // pines default (16/17) -> el escaneo de Wire1 no encontraria nada. Mismo
    // orden que sensors_imu.cpp:83-85 y la nota de src/shared/types.h:10-12.
    Wire1.setSCL(WIRE1_SCL);
    Wire1.setSDA(WIRE1_SDA);
    Wire1.begin();
    Wire1.setClock(400000);
}

void loop() {
    const int n0 = scan_bus(Wire,  "Wire  (I2C0, U2/U3 frontal/trasero)");
    const int n1 = scan_bus(Wire1, "Wire1 (I2C1, U5/U17 izq/der)");

    Serial.print("\n[resumen] Wire=");
    Serial.print(n0);
    Serial.print(" dispositivos, Wire1=");
    Serial.print(n1);
    Serial.println(" dispositivos.");
    Serial.println("Recordatorio: 0x29 = al menos 1 ToF (no distingue 2 en el mismo bus).");
    Serial.println("Re-escaneo en 3 s...\n");

    // Parpadeo de vida + espera.
    for (int i = 0; i < 6; ++i) {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        delay(500);
    }
}
