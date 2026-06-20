// diag_bno_freeze_probe.cpp — (modo SCAN DUAL-BUS) test de aislamiento I2C Wire vs Wire2.
// NO es firmware de competencia.
//
// Hipótesis (Gustavo 2026-06-20): los buses I2C no están aislados — un puente físico
// SDA/SCL entre Wire (ToF) y Wire2 (BNO primario) haría que el tráfico/ruido de los ToF
// corrompa al BNO. Test directo: escanear AMBOS buses. Si una dirección de ToF (0x29
// default, o 0x2A-0x2D enumerado) aparece en Wire2, o el BNO (0x28) aparece en Wire,
// entonces los buses están PUENTEADOS (no aislados).
//
//   pio run -e diag_bno_freeze_probe -t upload ; pio device monitor -b 115200

#include <Arduino.h>
#include <Wire.h>

namespace {
constexpr uint8_t TOF_LP_PIN[4] = {9, 10, 11, 12};

void scan(TwoWire& bus, const char* name) {
    Serial.print(name); Serial.print(" ACK en:");
    bool any = false;
    for (uint8_t a = 0x08; a < 0x78; ++a) {
        bus.beginTransmission(a);
        if (bus.endTransmission() == 0) { Serial.print(" 0x"); Serial.print(a, HEX); any = true; }
    }
    if (!any) Serial.print(" (nada)");
    Serial.println();
}
}  // namespace

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis(); while (!Serial && (millis() - t0) < 3000) {}
    pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, HIGH);

    Serial.println("\n### DUAL-BUS I2C SCAN (Wire 18/19 vs Wire2 24/25) — test de aislamiento ###");
    for (int i = 0; i < 4; ++i) { pinMode(TOF_LP_PIN[i], OUTPUT); digitalWrite(TOF_LP_PIN[i], LOW); }
    Wire.begin();  Wire.setClock(100000);
    Wire2.begin(); Wire2.setClock(100000);
    delay(120);

    Serial.println("\n-- ToF DORMIDOS (LP low) --  esperado: BNO 0x28 solo donde esté cableado");
    scan(Wire,  "Wire  (18/19)");
    scan(Wire2, "Wire2 (24/25)");

    for (int i = 0; i < 4; ++i) digitalWrite(TOF_LP_PIN[i], HIGH);   // despertar los 4 ToF (van a 0x29 default)
    delay(150);
    Serial.println("\n-- ToF DESPIERTOS (LP high -> 0x29 default) --");
    scan(Wire,  "Wire  (18/19)");
    scan(Wire2, "Wire2 (24/25)");

    Serial.println("\nVEREDICTO:");
    Serial.println("  - 0x29 (ToF) aparece en Wire2  -> BUSES PUENTEADOS (no aislados).");
    Serial.println("  - 0x28 (BNO) aparece en Wire   -> idem (o el secundario en Wire).");
    Serial.println("  - Cada uno SOLO en su bus      -> buses AISLADOS (el freeze es por alim/EMI, no por el bus).");
}

void loop() { delay(1000); }
