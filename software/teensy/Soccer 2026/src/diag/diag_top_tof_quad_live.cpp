// diag_top_tof_quad_live.cpp — Lee los 4 ToF VL53L7CX a la vez (bus unico),
// con la lib ADAFRUIT. NO es firmware de competencia.
//
// Contexto (banco 2026-05-30)
// ---------------------------
// El recableado de Enzo dejo los 4 ToF en el bus Wire (18/19) con la pata LP
// de cada uno cableada a un pin del Teensy. El censo (diag_top_tof_census)
// confirmo, tras POWER-CYCLE limpio, que los 4 LP se controlan:
//     LP pin 9 -> ToF#0 ,  10 -> #1 ,  11 -> #2 ,  12 -> #3   (ACTIVO-ALTO)
// Este programa usa esos pines para:
//   1) enumerar los 4 a direcciones distintas (0x2A..0x2D),
//   2) cargar el firmware de cada uno con la lib Adafruit (la BUENA),
//   3) leer ranging de los 4 SIMULTANEAMENTE e imprimir la distancia media.
// Sirve para (a) confirmar que los 4 MIDEN de verdad, y (b) MAPEAR que
// direccion corresponde a que posicion fisica: tapas un sensor con la mano y
// el que cae a ~50 mm es ese.
//
// PROCEDIMIENTO OBLIGATORIO (descubierto en banco)
// ------------------------------------------------
// Las direcciones I2C de los VL53L7CX PERSISTEN entre resets del Teensy: solo
// un corte real de energia las vuelve a 0x29 de fabrica. Por eso:
//   1) pio run -e diag_top_tof_quad_live -t upload
//   2) QUITAR energia de la placa y reponerla (no alcanza el reset).
//   3) pio device monitor -b 115200
// El programa igual hace un "recover" por software al arrancar (junta cualquier
// ToF disperso de vuelta a 0x29) para tolerar re-flasheos sin power-cycle, pero
// el power-cycle es lo mas confiable.
//
// Pinout: Wire = SDA 18 / SCL 19 (igual que el resto del bring-up TOP).

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L7CX.h>

namespace {

constexpr uint8_t LP_PIN[]   = {9, 10, 11, 12};   // confirmados en banco
constexpr uint8_t ADDR[]     = {0x2A, 0x2B, 0x2C, 0x2D};
constexpr uint8_t N_TOF      = 4;
constexpr uint8_t WAKE_LEVEL  = HIGH;             // ACTIVO-ALTO confirmado
constexpr uint8_t SLEEP_LEVEL = LOW;
constexpr uint8_t DEFAULT_ADDR = 0x29;
constexpr uint8_t BNO_ADDR     = 0x28;
constexpr uint8_t RES_ZONES    = 16;              // 4x4, liviano
constexpr uint8_t RANGE_HZ     = 15;
constexpr uint32_t LP_SETTLE_MS = 120;

Adafruit_VL53L7CX     g_tof[N_TOF];
VL53L7CX_ResultsData  g_res;
bool                  g_ready[N_TOF] = {false, false, false, false};
const char*           g_label[N_TOF] = {"ToF#0", "ToF#1", "ToF#2", "ToF#3"};

bool acks(uint8_t a) {
    Wire.beginTransmission(a);
    return Wire.endTransmission() == 0;
}

void set_all_lp(uint8_t level) {
    for (uint8_t i = 0; i < N_TOF; ++i) digitalWrite(LP_PIN[i], level);
}

// Cambio de dir de bajo nivel (no requiere firmware cargado) — para el recover.
bool raw_change_addr(uint8_t cur, uint8_t next) {
    Wire.beginTransmission(cur);
    Wire.write(0x7F); Wire.write(0xFF); Wire.write(0x00);
    if (Wire.endTransmission() != 0) return false;
    Wire.beginTransmission(cur);
    Wire.write(0x00); Wire.write(0x04); Wire.write(next);
    if (Wire.endTransmission() != 0) return false;
    delay(5);
    return acks(next);
}

// Junta cualquier ToF disperso (0x2A..0x2D, 0x60) de vuelta a 0x29.
void recover_to_default() {
    set_all_lp(WAKE_LEVEL);   // todos despiertos para poder hablarles
    delay(60);
    const uint8_t dispersed[] = {0x2A, 0x2B, 0x2C, 0x2D, 0x60};
    for (uint8_t k = 0; k < sizeof(dispersed); ++k) {
        for (uint8_t t = 0; t < 4 && acks(dispersed[k]); ++t) {
            raw_change_addr(dispersed[k], DEFAULT_ADDR);
        }
    }
}

uint16_t mean_valid(const VL53L7CX_ResultsData& r, uint8_t n, uint8_t& valid_out) {
    uint32_t sum = 0; uint16_t cnt = 0;
    for (uint8_t i = 0; i < n; ++i) {
        uint8_t s = r.target_status[i];
        int16_t mm = r.distance_mm[i];
        if ((s == 5 || s == 6 || s == 9) && mm >= 0) { sum += mm; ++cnt; }
    }
    valid_out = cnt;
    return cnt ? static_cast<uint16_t>(sum / cnt) : 0xFFFF;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) {}

    Serial.println("\n============================================================");
    Serial.println("  TOP — Lectura SIMULTANEA de los 4 ToF (lib Adafruit)");
    Serial.println("============================================================");
    Serial.println("LP pins: 9,10,11,12 (ACTIVO-ALTO) -> dir 0x2A,0x2B,0x2C,0x2D");
    Serial.print  ("Build: "); Serial.print(__DATE__); Serial.print(" "); Serial.println(__TIME__);

    for (uint8_t i = 0; i < N_TOF; ++i) {
        pinMode(LP_PIN[i], OUTPUT);
        digitalWrite(LP_PIN[i], SLEEP_LEVEL);
    }
    Wire.begin();
    Wire.setClock(400000);

    Serial.print("BNO055 (0x28): ");
    Serial.println(acks(BNO_ADDR) ? "PRESENTE (bus OK)" : "AUSENTE (revisar bus!)");

    Serial.println("Recuperando ToF dispersos -> 0x29 ...");
    recover_to_default();

    // Enumeracion: dormir todos; despertar de a uno; begin() carga firmware en
    // 0x29; mover a su direccion; configurar; rangear. Dejar despierto.
    set_all_lp(SLEEP_LEVEL);
    delay(LP_SETTLE_MS);

    for (uint8_t i = 0; i < N_TOF; ++i) {
        Serial.print("\n[init ");
        Serial.print(g_label[i]);
        Serial.print(" LP pin ");
        Serial.print(LP_PIN[i]);
        Serial.println("] despertando + cargando firmware (hasta ~10s)...");

        digitalWrite(LP_PIN[i], WAKE_LEVEL);
        delay(LP_SETTLE_MS);

        if (!acks(DEFAULT_ADDR)) {
            Serial.println("   ! no aparecio en 0x29 al despertar -> LP no controla este ToF. Salteo.");
            digitalWrite(LP_PIN[i], SLEEP_LEVEL);
            continue;
        }
        if (!g_tof[i].begin(DEFAULT_ADDR, &Wire, 400000)) {
            Serial.println("   ! begin() fallo (firmware no cargo). Salteo.");
            continue;
        }
        if (!g_tof[i].setAddress(ADDR[i])) {
            Serial.println("   ! setAddress() fallo. Salteo.");
            continue;
        }
        g_tof[i].setResolution(RES_ZONES);
        g_tof[i].setRangingFrequency(RANGE_HZ);
        if (!g_tof[i].startRanging()) {
            Serial.println("   ! startRanging() fallo. Salteo.");
            continue;
        }
        g_ready[i] = true;
        Serial.print("   OK -> midiendo en 0x");
        Serial.println(ADDR[i], HEX);
        // queda despierto a proposito (retiene direccion + sigue rangeando)
    }

    int ok = 0;
    for (uint8_t i = 0; i < N_TOF; ++i) if (g_ready[i]) ++ok;
    Serial.println("\n------------------------------------------------------------");
    Serial.print("ToF MIDIENDO: ");
    Serial.print(ok);
    Serial.println(" de 4.");
    if (ok == 4) Serial.println("Tapa cada sensor con la mano para mapear direccion -> posicion.");
    Serial.println("------------------------------------------------------------\n");
}

void loop() {
    static uint32_t last_print = 0;
    static uint16_t dist[N_TOF] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    static uint8_t  valid[N_TOF] = {0, 0, 0, 0};

    for (uint8_t i = 0; i < N_TOF; ++i) {
        if (!g_ready[i]) continue;
        if (g_tof[i].isDataReady() && g_tof[i].getRangingData(&g_res)) {
            dist[i] = mean_valid(g_res, RES_ZONES, valid[i]);
        }
    }

    if (millis() - last_print >= 400) {
        last_print = millis();
        for (uint8_t i = 0; i < N_TOF; ++i) {
            Serial.print(g_label[i]);
            Serial.print("@0x");
            Serial.print(ADDR[i], HEX);
            Serial.print("=");
            if (!g_ready[i])              Serial.print("(off)");
            else if (dist[i] == 0xFFFF)   Serial.print("----");
            else { Serial.print(dist[i]); Serial.print("mm"); }
            Serial.print("(v");
            Serial.print(valid[i]);
            Serial.print(")  ");
        }
        Serial.println();
    }
}
