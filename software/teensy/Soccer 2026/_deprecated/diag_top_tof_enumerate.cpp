// diag_top_tof_enumerate.cpp — Enumeracion ROBUSTA de los 4 ToF en un solo bus,
// tolerante a UN LP roto. NO es firmware de competencia.
//
// Contexto (recableado 2026-05-30, Enzo)
// --------------------------------------
// Los 4 ToF VL53L7CX quedaron TODOS colgados del I2C principal (Wire, 18/19),
// junto con el BNO (0x28). Cada ToF tiene su pata LP cableada por bodge a un
// pin del Teensy (los ex-INT). El bodge es fisico, no esta en el PCB del repo.
//
// El descubridor previo (diag_top_tof_lp_discover) mostro que con TODOS los
// candidatos en "dormir" SIEMPRE queda >=1 ToF contestando en 0x29 -> al menos
// un LP NO se puede controlar (cable en INT en vez de LP, flojo, o NC).
//
// Estrategia (idea de Gustavo) — funciona si >=3 de los 4 LP andan
// ----------------------------------------------------------------
// Los 4 ToF salen de fabrica en 0x29 (7-bit). La direccion se cambia con un
// write a page-0 + registro 0x0004 = nueva_dir_7bit (secuencia ST, confirmada
// en Adafruit_VL53L7CX::setAddress y vl53l7cx_set_i2c_address). NO requiere
// init de la lib -> sirve para enumerar antes de cargar el firmware blob.
//
//   1) Dormir los 4 LP. El/los ToF cuyo LP NO se controla quedan en 0x29 (su
//      LP flota HIGH por el pull-up del modulo). Lo APARTO: 0x29 -> PARK_ADDR.
//   2) Peino los controlables de a uno: despierto LP[i] (HIGH), el ToF habilita
//      su I2C y aparece en 0x29 SOLO (los demas dormidos, el pegado ya en
//      PARK). Lo muevo a TARGET_ADDR[i] y lo dejo despierto (retiene la dir).
//   3) Scan final del bus: el veredicto sale de CUANTOS dispositivos ToF quedan
//      leibles (total en bus menos el BNO 0x28). Objetivo: 4.
//
// Idempotente: las direcciones persisten mientras el sensor tenga 3V3 (un reset
// del Teensy NO las borra; solo un power-cycle del modulo). El programa re-corre
// cada 4 s y reconoce sensores ya reasignados, asi que el output es estable.
// Para volver al estado de fabrica (todos en 0x29): power-cycle de la placa.
//
// Polaridad
// ---------
// Asume LPn activo-ALTO (HIGH = I2C habilitado / ToF visible; LOW = I2C
// deshabilitado), que es la convencion de los carriers Pololu/Adafruit y la
// consistente con el descubridor. Si NINGUN pin despierta un ToF, invertir
// WAKE_LEVEL/SLEEP_LEVEL abajo y recompilar.
//
// Uso
// ---
//   pio run -e diag_top_tof_enumerate -t upload
//   pio device monitor -b 115200

#include <Arduino.h>
#include <Wire.h>

namespace {

// --- Hipotesis de pines LP (editable). Gustavo propuso {9, 11, 12, 22}. ---
// El indice aca es el "ToF logico" #0..#3. El mapeo a posicion fisica
// (frontal/tras/izq/der) se hace despues tapando cada sensor en el ranging.
const uint8_t LP_PIN[] = {9, 11, 12, 22};
constexpr uint8_t N_LP = sizeof(LP_PIN) / sizeof(LP_PIN[0]);

// --- Polaridad del LP (ver nota arriba). Activo-alto por default. ---
constexpr uint8_t WAKE_LEVEL  = HIGH;  // ToF visible / I2C habilitado
constexpr uint8_t SLEEP_LEVEL = LOW;   // ToF oculto / I2C deshabilitado

// --- Direcciones 7-bit. Evitar 0x28 (BNO) y 0x29 (default ToF). ---
constexpr uint8_t TOF_DEFAULT_ADDR = 0x29;
constexpr uint8_t BNO_ADDR         = 0x28;
const uint8_t     TARGET_ADDR[]    = {0x2A, 0x2B, 0x2C, 0x2D};
constexpr uint8_t PARK_ADDR        = 0x60;  // donde aparco al/los "pegado(s)"

constexpr uint32_t LP_SETTLE_MS = 120;  // habilitacion del I2C tras tocar LP

// ¿Contesta 'addr'? (ACK = presente)
bool acks(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

// Cambia la dir del ToF que ESTE contestando en 'cur' -> 'next' (7-bit).
// Secuencia ST: page-select 0 (reg 0x7FFF=0x00), luego reg 0x0004 = next.
// Devuelve true si el write hizo ACK Y 'next' responde despues.
bool change_addr(uint8_t cur, uint8_t next) {
    // page 0
    Wire.beginTransmission(cur);
    Wire.write(0x7F); Wire.write(0xFF); Wire.write(0x00);
    if (Wire.endTransmission() != 0) return false;
    // reg 0x0004 = nueva dir 7-bit (NO shift: el registro guarda el 7-bit)
    Wire.beginTransmission(cur);
    Wire.write(0x00); Wire.write(0x04); Wire.write(next);
    if (Wire.endTransmission() != 0) return false;
    delay(5);
    return acks(next);
}

void sleep_all_lp() {
    for (uint8_t i = 0; i < N_LP; ++i) digitalWrite(LP_PIN[i], SLEEP_LEVEL);
}

void print_hex(uint8_t v) {
    Serial.print("0x");
    if (v < 16) Serial.print('0');
    Serial.print(v, HEX);
}

// Escanea el bus y cuenta dispositivos ToF (todo menos el BNO). Imprime la lista.
int scan_and_count_tof(const char* label) {
    Serial.print(label);
    int tof = 0;
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
        if (acks(a)) {
            print_hex(a);
            if (a == BNO_ADDR) Serial.print("(BNO) ");
            else { Serial.print(' '); ++tof; }
        }
    }
    Serial.println();
    return tof;
}

void run_once() {
    Serial.println("\n============================================================");
    Serial.println("  TOP — Enumeracion de 4 ToF en bus unico (tolera 1 LP roto)");
    Serial.println("============================================================");
    Serial.print("LP pins (hipotesis): ");
    for (uint8_t i = 0; i < N_LP; ++i) { Serial.print(LP_PIN[i]); Serial.print(' '); }
    Serial.print(" | wake=");
    Serial.println(WAKE_LEVEL == HIGH ? "HIGH" : "LOW");

    scan_and_count_tof("Scan inicial   -> ");
    Serial.print("BNO055 (0x28): ");
    Serial.println(acks(BNO_ADDR) ? "PRESENTE (bus OK)" : "AUSENTE (revisar bus!)");

    // -------- Paso 1: dormir todos; apartar el/los pegado(s) --------
    sleep_all_lp();
    delay(LP_SETTLE_MS);
    Serial.print("\n[Paso 1] todos LP dormidos. 0x29: ");
    if (acks(TOF_DEFAULT_ADDR)) {
        Serial.println("PRESENTE (hay ToF con LP no controlable) -> aparto a PARK");
        bool ok = change_addr(TOF_DEFAULT_ADDR, PARK_ADDR);
        Serial.print("  0x29 -> ");
        print_hex(PARK_ADDR);
        Serial.println(ok ? " : OK" : " : FALLO el write");
        delay(LP_SETTLE_MS);
        if (acks(TOF_DEFAULT_ADDR)) {
            Serial.println("  OJO: 0x29 SIGUE presente -> probablemente >=2 ToF con");
            Serial.println("       LP roto (comparten 0x29 y se movieron juntos). El");
            Serial.println("       scan final dira cuantos quedan realmente leibles.");
        }
    } else {
        Serial.println("ausente (ningun ToF pegado en 0x29, o ya estaba apartado)");
    }

    // -------- Paso 2: peinar los controlables de a uno --------
    Serial.println("\n[Paso 2] enumerando controlables:");
    bool pin_is_lp[N_LP];
    for (uint8_t i = 0; i < N_LP; ++i) pin_is_lp[i] = false;

    for (uint8_t i = 0; i < N_LP; ++i) {
        digitalWrite(LP_PIN[i], WAKE_LEVEL);
        delay(LP_SETTLE_MS);

        Serial.print("  pin ");
        if (LP_PIN[i] < 10) Serial.print(' ');
        Serial.print(LP_PIN[i]);
        Serial.print(" (ToF#");
        Serial.print(i);
        Serial.print("): ");

        if (acks(TARGET_ADDR[i])) {
            // Ya estaba en su target (persistio de una corrida previa).
            pin_is_lp[i] = true;
            Serial.print("ya en ");
            print_hex(TARGET_ADDR[i]);
            Serial.println(" (persistio) -> LP OK");
        } else if (acks(TOF_DEFAULT_ADDR)) {
            // Aparecio fresco en 0x29 al despertarlo -> su LP funciona.
            if (change_addr(TOF_DEFAULT_ADDR, TARGET_ADDR[i])) {
                pin_is_lp[i] = true;
                Serial.print("LP OK -> movido a ");
                print_hex(TARGET_ADDR[i]);
                Serial.println();
            } else {
                Serial.println("aparecio en 0x29 pero FALLO el cambio de address.");
            }
        } else {
            Serial.println("sin efecto (pin NO es LP real, o su ToF es el pegado).");
            digitalWrite(LP_PIN[i], SLEEP_LEVEL);
        }
    }

    // -------- Paso 3: scan final + veredicto (fuente de verdad) --------
    Serial.println("\n[Paso 3] scan final del bus:");
    int tof_readable = scan_and_count_tof("  ToF leibles   -> ");

    int lp_ok = 0;
    for (uint8_t i = 0; i < N_LP; ++i) if (pin_is_lp[i]) ++lp_ok;

    Serial.println("------------------------------------------------------------");
    Serial.print("LP controlables confirmados: ");
    Serial.print(lp_ok);
    Serial.print(" de ");
    Serial.print(N_LP);
    Serial.print("  { ");
    for (uint8_t i = 0; i < N_LP; ++i) if (pin_is_lp[i]) { Serial.print(LP_PIN[i]); Serial.print(' '); }
    Serial.println("}");

    Serial.print("VEREDICTO: ");
    if (tof_readable >= 4) {
        Serial.println("4 ToF LEIBLES a la vez. Enumeracion lograda.");
        if (lp_ok < 4)
            Serial.println("  (1 ToF quedo en PARK por LP no controlable, pero se lee.)");
        Serial.println("  Siguiente: tapar cada sensor para mapear address -> posicion");
        Serial.println("  fisica (frontal/tras/izq/der), y fijar PIN_TOF_XSHUT reales.");
    } else if (tof_readable == 3) {
        Serial.println("solo 3 ToF leibles.");
        Serial.println("  Probable: 2 LP rotos (no separables) o 1 pin LP mal");
        Serial.println("  adivinado. Mira que pin dio 'sin efecto' en Paso 2.");
        Serial.println("  Si fue el 22, probar 23 (A9) en LP_PIN y recompilar.");
    } else {
        Serial.print(tof_readable);
        Serial.println(" ToF leibles. Enumeracion incompleta.");
        Serial.println("  Si NINGUN pin dio 'LP OK' -> probar polaridad invertida");
        Serial.println("  (WAKE_LEVEL=LOW) o revisar que LP_PIN sean los reales.");
        Serial.println("  Tip: power-cycla la placa para volver todos a 0x29 y reintentar.");
    }
    Serial.println("============================================================\n");
}

}  // namespace

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) {}

    for (uint8_t i = 0; i < N_LP; ++i) {
        pinMode(LP_PIN[i], OUTPUT);
        digitalWrite(LP_PIN[i], SLEEP_LEVEL);
    }

    Wire.begin();
    Wire.setClock(400000);

    run_once();
}

void loop() {
    delay(4000);
    run_once();
}
