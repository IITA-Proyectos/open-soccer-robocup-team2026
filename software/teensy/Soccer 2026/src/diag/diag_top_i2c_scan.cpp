// diag_top_i2c_scan.cpp — Escaner I2C de los DOS buses de la placa TOP.
//
// NO es firmware de competencia. Es el PRIMER paso de bring-up de los ToF:
// antes de enumerar (reasignar direcciones), hay que ver QUE responde hoy en
// cada bus, sin tocar ningun pin LPn/XSHUT.
//
// Que hace
// --------
// Escanea direcciones 0x08..0x77 en los TRES buses i2c del Teensy 4.0 (pines NATIVOS):
//   • Wire  (LPI2C1) = 18(SDA)/19(SCL)  -> BNO1 (0x28) + los 4 ToF (0x29 de fabrica)
//   • Wire1 (LPI2C3) = 17(SDA)/16(SCL)
//   • Wire2 (LPI2C4) = 25(SDA)/24(SCL)  -> pines del BACK-PAD, "DEBAJO" del Teensy
//
// ⚠️ CORRECCION 2026-06-09 (ROBOT2): la version vieja remapeaba Wire1 a 24/25 — PERO en
//   el Teensy 4.0 los pines 24/25 son de Wire2 (LPI2C4), NO de Wire1 (LPI2C3). Wire1 no
//   puede manejar esos pines -> el "escaneo de 24/25" via Wire1 nunca funciono. El 2do BNO
//   de ROBOT2 (soldado a 24/25 "debajo" del Teensy) hay que buscarlo en Wire2.
//   Si sale un BNO (0x28) en Wire2 -> ese es el 2do BNO. Si NO sale en ningun bus salvo
//   Wire -> revisar soldadura / alimentacion / pull-ups del 2do BNO.
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

// Teensy 4.0: cada bus usa sus pines NATIVOS, no se remapea nada.
//   Wire 18/19 · Wire1 16/17 · Wire2 24/25. (24/25 = Wire2, NO Wire1 — ver header.)

// Devuelve una etiqueta humana para las direcciones que esperamos ver.
const char* addr_hint(uint8_t addr) {
    switch (addr) {
        case 0x28: return "BNO055 (IMU) default";
        case 0x29: return "VL53L7CX (ToF) default  <-- 1 o MAS ToF aca";
        case 0x2A: return "ToF reasignado (post-enumeracion)";
        case 0x2B: return "ToF reasignado (post-enumeracion)";
        case 0x2C: return "ToF reasignado (post-enumeracion)";
        case 0x2D: return "ToF reasignado (post-enumeracion)";
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
    Serial.println("  TOP — Escaner I2C de los 3 buses");
    Serial.println("  Wire 18/19 + Wire1 16/17 + Wire2 24/25");
    Serial.println("  (2do BNO de robot2 = Wire2 24/25, 'debajo' del Teensy)");
    Serial.println("=========================================");

    // Los 3 buses con sus pines NATIVOS del Teensy 4.0 (sin remap).
    Wire.begin();      // 18/19
    Wire.setClock(400000);
    Wire1.begin();     // 16/17
    Wire1.setClock(400000);
    Wire2.begin();     // 24/25  <- aca va el 2do BNO de robot2 (back-pad)
    Wire2.setClock(400000);
}

void loop() {
    const int n0 = scan_bus(Wire,  "Wire  (18/19) -> BNO1 + 4 ToF");
    const int n1 = scan_bus(Wire1, "Wire1 (16/17)");
    const int n2 = scan_bus(Wire2, "Wire2 (24/25) -> 2do BNO de robot2 (back-pad)");

    Serial.print("\n[resumen] Wire=");
    Serial.print(n0);
    Serial.print("  Wire1=");
    Serial.print(n1);
    Serial.print("  Wire2=");
    Serial.print(n2);
    Serial.println(" dispositivos.");
    Serial.println("Recordatorio: 0x29 = al menos 1 ToF (no distingue 2 en el mismo bus).");
    Serial.println("2do BNO de robot2 -> deberia salir 0x28 en Wire2 (24/25).");
    Serial.println("Re-escaneo en 3 s...\n");

    // Parpadeo de vida + espera.
    for (int i = 0; i < 6; ++i) {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        delay(500);
    }
}
