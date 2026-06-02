// diag_bno_addr_check.cpp — Verifica que los 2 BNO055 esten en direcciones
// distintas (0x28 y 0x29) en el MISMO bus Wire. NO es firmware de competencia.
//
// Contexto (2026-05-31)
// ---------------------
// El recableado dejo TODO en el bus principal Wire (18/19) para liberar Wire1.
// El 2do BNO se soldó y se conecta a Wire tambien. PROBLEMA: el BNO055 NO se
// redirecciona por software (no tiene setAddress; solo 0x28 o 0x29 segun el pin
// ADR fisico: ADR->GND = 0x28, ADR->3.3V = 0x29). Para que los 2 convivan, uno
// queda en 0x28 (default) y al otro se le puentea ADR a 3.3V -> 0x29.
//
// Este diag confirma el resultado del puente, SIN ambiguedad:
//   - Pone los 4 ToF a DORMIR (LP pins 9/10/11/12 en LOW) para que NO aparezcan
//     en 0x29 (los ToF salen de fabrica en 0x29 y contaminarian la lectura).
//   - Escanea el bus Wire e informa que hay en 0x28 y en 0x29.
//   - Para cada direccion que responde, confirma que es un BNO055 leyendo el
//     registro CHIP_ID (0x00), que en el BNO055 vale 0xA0. Asi distingue un BNO
//     real de un ToF que se haya quedado despierto.
//
// Interpretacion:
//   0x28=BNO(A0) Y 0x29=BNO(A0)  -> PERFECTO: 2 BNO en direcciones distintas.
//   solo 0x28=BNO                 -> el 2do no aparece: puente ADR no hizo 0x29,
//                                    o 2do BNO sin alimentar / PS1 en alto / mal SDA-SCL.
//   0x28 y 0x29 pero uno NO es A0 -> en 0x29 hay un ToF despierto, no el BNO.
//
// Uso
// ---
//   pio run -e diag_bno_addr_check -t upload
//   pio device monitor -b 115200

#include <Arduino.h>
#include <Wire.h>

namespace {

// LP de los 4 ToF (confirmados en banco). Los dormimos para que no aparezcan.
constexpr uint8_t TOF_LP_PIN[4] = {9, 10, 11, 12};
constexpr uint8_t TOF_SLEEP_LVL = LOW;   // activo-ALTO => LOW = dormido

constexpr uint8_t BNO_CHIP_ID_REG = 0x00;
constexpr uint8_t BNO_CHIP_ID_VAL = 0xA0;  // ID fijo del BNO055

bool acks(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

// Lee 1 byte de un registro. Devuelve true si pudo leer.
bool read_reg(uint8_t addr, uint8_t reg, uint8_t& out) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;   // repeated start
    if (Wire.requestFrom((int)addr, 1) != 1) return false;
    out = Wire.read();
    return true;
}

// Reporta una direccion: presente? es BNO (chip id 0xA0)?
void report_addr(uint8_t addr) {
    Serial.print("  0x");
    Serial.print(addr, HEX);
    Serial.print(": ");
    if (!acks(addr)) { Serial.println("ausente"); return; }
    uint8_t id = 0;
    if (read_reg(addr, BNO_CHIP_ID_REG, id) && id == BNO_CHIP_ID_VAL) {
        Serial.println("PRESENTE — es BNO055 (chip_id=0xA0) OK");
    } else {
        Serial.print("PRESENTE pero chip_id=0x");
        Serial.print(id, HEX);
        Serial.println(" (NO es BNO055 — probable ToF despierto u otro chip)");
    }
}

void run_once() {
    Serial.println("\n============================================================");
    Serial.println("  TOP — Check de direcciones de los 2 BNO055 (bus Wire)");
    Serial.println("============================================================");
    Serial.println("(ToF dormidos para no contaminar 0x29)");

    report_addr(0x28);
    report_addr(0x29);

    const bool a = acks(0x28), b = acks(0x29);
    Serial.println("------------------------------------------------------------");
    if (a && b) {
        Serial.println("VEREDICTO: 2 dispositivos en 0x28 y 0x29.");
        Serial.println("  Si AMBOS dicen 'es BNO055' -> LISTO: 2 BNO en direcciones");
        Serial.println("  distintas. Ya podemos leer los dos y fusionar heading.");
    } else if (a && !b) {
        Serial.println("VEREDICTO: solo 0x28. El 2do BNO NO aparecio en 0x29.");
        Serial.println("  Revisar: (1) puente ADR -> 3.3V bien hecho; (2) BNO nuevo");
        Serial.println("  alimentado (3vo/Vin + GND); (3) PS0 y PS1 a GND (modo I2C);");
        Serial.println("  (4) SDA->18 y SCL->19 continuos.");
    } else if (!a && b) {
        Serial.println("VEREDICTO: solo 0x29. Falta el BNO de 0x28 (el que andaba).");
        Serial.println("  Revisar conexion/alimentacion del primer BNO.");
    } else {
        Serial.println("VEREDICTO: NADA en 0x28 ni 0x29. Revisar el bus Wire (18/19),");
        Serial.println("  alimentacion y GND comun. Correr diag_top_i2c_scan.");
    }
    Serial.println("============================================================\n");
}

}  // namespace

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) {}
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // Dormir los 4 ToF para que no aparezcan en 0x29.
    for (uint8_t i = 0; i < 4; ++i) {
        pinMode(TOF_LP_PIN[i], OUTPUT);
        digitalWrite(TOF_LP_PIN[i], TOF_SLEEP_LVL);
    }

    Wire.begin();
    Wire.setClock(400000);

    run_once();
}

void loop() {
    // Re-escaneo cada ~3 s para poder ajustar el puente y ver el cambio en vivo.
    for (int i = 0; i < 6; ++i) {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        delay(500);
    }
    run_once();
}
