// diag_top_tof.cpp — Diagnostico del sensor ToF VL53L7CX frontal (placa TOP, U2).
//
// NO es firmware de competencia. Sketch standalone que valida que el sensor
// VL53L7CX soldado en U2 responde, se inicializa y mide. Sigue el patron de
// main_diag_down.cpp.
//
// Hardware esperado (segun pack 2026-05-24):
//   • Teensy 4.0 de la placa TOP
//   • VL53L7CX en posicion U2 del schematic
//   • Bus Wire (I2C0): SDA=18, SCL=19
//   • XSHUT del U2 conectado al pin 2 del Teensy
//
// Build / flash:
//   pio run -e diag_top_tof -t upload
//
// Modos (cambiar SOLO uno via build_flags):
//   -DDIAG_TOF_MODE_SINGLE   ; 1 numero promedio
//   -DDIAG_TOF_MODE_4X4      ; (default) grilla 4x4 ASCII
//   -DDIAG_TOF_MODE_8X8      ; grilla 8x8 ASCII
//
// Fallback si XSHUT pin 2 no esta ruteado:
//   -DDIAG_TOF_SKIP_XSHUT    ; asume sensor siempre alimentado
//
// Spec: docs/superpowers/specs/2026-05-24-diag-top-tof-vl53l7cx-design.md
// Creado 2026-05-24 para hardware-up del VL53L7CX frontal de la placa TOP.

#include <Arduino.h>
#include <Wire.h>
#include <vl53l7cx_class.h>

// ============================================================
// Configuracion de modo (mutuamente excluyente)
// ============================================================
#if defined(DIAG_TOF_MODE_SINGLE) + defined(DIAG_TOF_MODE_4X4) + defined(DIAG_TOF_MODE_8X8) > 1
#error "Solo UN modo a la vez: DIAG_TOF_MODE_SINGLE | DIAG_TOF_MODE_4X4 | DIAG_TOF_MODE_8X8"
#endif

#if !defined(DIAG_TOF_MODE_SINGLE) && !defined(DIAG_TOF_MODE_4X4) && !defined(DIAG_TOF_MODE_8X8)
#define DIAG_TOF_MODE_4X4   // default
#endif

// ============================================================
// Pinout y constantes
// ============================================================
constexpr int PIN_XSHUT_TOF_FRONT = 2;     // U2 del schematic — XSHUT al pin 2 del Teensy.
constexpr int PIN_LED_STATUS      = 13;    // LED_BUILTIN.
constexpr uint32_t I2C_CLOCK_HZ   = 400000;  // fast-mode standard; subir a 1 MHz si responde estable.

// ============================================================
// Globales
// ============================================================
namespace {
VL53L7CX g_sensor(&Wire, PIN_XSHUT_TOF_FRONT);   // constructor: (TwoWire*, lpn_pin/XSHUT)
bool g_init_ok = false;

// I2C scanner — recorre addresses 7-bit (1..127) en bus Wire y reporta los
// que responden con ACK. Util para distinguir "VL53L7CX no responde I2C"
// vs "responde pero la lib falla en init".
void scan_i2c_bus(TwoWire& bus, const char* bus_name) {
    Serial.print("[i2c-scan] ");
    Serial.print(bus_name);
    Serial.print(": ");
    int found = 0;
    for (uint8_t addr = 1; addr < 128; ++addr) {
        bus.beginTransmission(addr);
        if (bus.endTransmission() == 0) {
            if (found > 0) Serial.print(", ");
            Serial.print("0x");
            if (addr < 16) Serial.print('0');
            Serial.print(addr, HEX);
            ++found;
        }
    }
    if (found == 0) Serial.print("(sin dispositivos)");
    Serial.println();
}

const char* mode_name() {
#if defined(DIAG_TOF_MODE_SINGLE)
    return "SINGLE";
#elif defined(DIAG_TOF_MODE_4X4)
    return "4x4";
#elif defined(DIAG_TOF_MODE_8X8)
    return "8x8";
#endif
}

// ============================================================
// Formatters de salida del frame ToF.
// ============================================================
// Formato comun: cada celda son 4 caracteres + 1 espacio. Valores >9999 se
// muestran como "XXXX" (status invalido o fuera de rango). distance_mm es
// int16_t pero la lib clampea negativos a 0 (vl53l7cx_api.cpp:776), asi que
// el cast unsigned es seguro.
void print_cell(int16_t mm, uint8_t status) {
    // status==5 = "range valid". 6/9 = "range valid + reflectance/sigma warning".
    // Otros = invalido.
    bool valid = (status == 5 || status == 6 || status == 9);
    if (!valid || mm < 0 || mm > 9999) {
        Serial.print(" XXXX");
    } else {
        if (mm < 10)        Serial.print("    ");
        else if (mm < 100)  Serial.print("   ");
        else if (mm < 1000) Serial.print("  ");
        else                Serial.print(" ");
        Serial.print(mm);
    }
}

void print_grid(const VL53L7CX_ResultsData& r, uint8_t side) {
    // VL53L7CX devuelve los datos en row-major desde la esquina top-left de la
    // imagen. Para side=4 → 16 zonas. side=8 → 64 zonas.
    const uint8_t n = side * side;
    Serial.print("[");
    Serial.print(millis());
    Serial.print(" ms]  zones=");
    Serial.print(n);
    // contar zonas validas
    uint8_t valid_count = 0;
    for (uint8_t i = 0; i < n; ++i) {
        uint8_t s = r.target_status[i];
        if (s == 5 || s == 6 || s == 9) ++valid_count;
    }
    Serial.print("  valid=");
    Serial.print(valid_count);
    Serial.print("/");
    Serial.println(n);

    for (uint8_t row = 0; row < side; ++row) {
        Serial.print("  ");
        for (uint8_t col = 0; col < side; ++col) {
            uint8_t idx = row * side + col;
            print_cell(r.distance_mm[idx], r.target_status[idx]);
        }
        Serial.println();
    }
}

void print_single(const VL53L7CX_ResultsData& r, uint8_t side) {
    const uint8_t n = side * side;
    uint32_t sum = 0;
    uint8_t valid_count = 0;
    for (uint8_t i = 0; i < n; ++i) {
        uint8_t s = r.target_status[i];
        if (s == 5 || s == 6 || s == 9) {
            int16_t d = r.distance_mm[i];
            if (d >= 0) {
                sum += (uint32_t)d;
                ++valid_count;
            }
        }
    }
    Serial.print("[");
    Serial.print(millis());
    Serial.print(" ms] ");
    if (valid_count == 0) {
        Serial.print("mean=---  ");
    } else {
        Serial.print("mean=");
        Serial.print(sum / valid_count);
        Serial.print(" mm  ");
    }
    Serial.print("valid=");
    Serial.print(valid_count);
    Serial.print("/");
    Serial.println(n);
}
}  // namespace

// ============================================================
// setup() / loop()
// ============================================================
void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* esperar al monitor, max 3 s */ }

    // ------ Banner ------
    Serial.println("\n=========================================");
    Serial.println("  diag_top_tof — VL53L7CX frontal U2");
    Serial.println("  (herramienta de banco, NO es competencia)");
    Serial.println("=========================================");
    Serial.print  ("Board       : Teensy 4.0 (TOP)\n");
    Serial.print  ("Bus         : Wire (I2C0)  SDA=18  SCL=19\n");
#ifdef DIAG_TOF_SKIP_XSHUT
    Serial.print  ("XSHUT       : SKIPPED por define\n");
#else
    Serial.print  ("XSHUT       : pin 2 (sera HIGH)\n");
#endif
    Serial.print  ("Mode        : "); Serial.println(mode_name());
    Serial.print  ("I2C clock   : "); Serial.print(I2C_CLOCK_HZ / 1000); Serial.println(" kHz");
    Serial.print  ("Build       : "); Serial.print(__DATE__); Serial.print(" "); Serial.println(__TIME__);
    Serial.println("-----------------------------------------");

    // ------ XSHUT del U2 ------
#ifndef DIAG_TOF_SKIP_XSHUT
    pinMode(PIN_XSHUT_TOF_FRONT, OUTPUT);
    digitalWrite(PIN_XSHUT_TOF_FRONT, LOW);
    delay(10);
    digitalWrite(PIN_XSHUT_TOF_FRONT, HIGH);
    delay(10);
    Serial.println("[xshut] pin 2 HIGH — sensor power-on");
#endif

    // ------ Wire init + scan ANTES de tocar la lib ------
    Wire.begin();
    Wire.setClock(I2C_CLOCK_HZ);
    delay(5);
    scan_i2c_bus(Wire, "Wire (I2C0)");
    // Si NO aparece 0x29: el sensor no responde por I2C. Stop, problema fisico.

    // ------ Init del sensor ------
    Serial.print("[init] g_sensor.begin() ... ");
    g_sensor.begin();
    Serial.println("ok");

    Serial.print("[init] loading firmware ULD (~3 s) ... ");
    int err = g_sensor.init_sensor();
    if (err != 0) {
        Serial.print("FAILED err="); Serial.println(err);
        Serial.println("[diag] init fallo — sensor no se inicializa. Posibles causas:");
        Serial.println("       - alimentacion 3V3 baja en el VL53L7CX");
        Serial.println("       - XSHUT mal ruteado (probar -DDIAG_TOF_SKIP_XSHUT)");
        Serial.println("       - lib vendoreada incompatible con esta variante de L7CX");
        Serial.println("[diag] entrando a loop con LED en error pattern (3 blinks rapidos).");
        return;  // g_init_ok queda false → loop entra a error pattern
    }
    Serial.println("OK");

    // ------ Configurar resolucion + frecuencia ------
    // Nota: la lib STM32duino expone los metodos con prefix `vl53l7cx_`
    // (no `set_resolution` plain). Ver lib/STM32duino_VL53L7CX/src/vl53l7cx_class.h.
#if defined(DIAG_TOF_MODE_8X8)
    g_sensor.vl53l7cx_set_resolution(VL53L7CX_RESOLUTION_8X8);
    Serial.println("[init] resolution = 8x8");
#else
    g_sensor.vl53l7cx_set_resolution(VL53L7CX_RESOLUTION_4X4);
    Serial.println("[init] resolution = 4x4");
#endif
    g_sensor.vl53l7cx_set_ranging_frequency_hz(15);
    g_sensor.vl53l7cx_set_ranging_mode(VL53L7CX_RANGING_MODE_CONTINUOUS);

    g_sensor.vl53l7cx_start_ranging();
    g_init_ok = true;
    Serial.println("[init] start_ranging — listo. Esperando frames...");
    Serial.println();
}

void loop() {
    // Error pattern si init fallo: 3 blinks rapidos cada 1 s + re-dump
    // del diagnostico cada 2 s. El re-dump existe para garantizar que el
    // operador vea el motivo del fallo aunque el monitor Serial se haya
    // conectado DESPUES del setup() (caso comun: USB CDC del Teensy descarta
    // bytes si no hay host listo en los primeros segundos).
    if (!g_init_ok) {
        uint32_t phase = millis() % 1000;
        bool on = (phase < 100) || (phase >= 200 && phase < 300) || (phase >= 400 && phase < 500);
        digitalWrite(PIN_LED_STATUS, on ? HIGH : LOW);

        static uint32_t t_last_dump = 0;
        if (millis() - t_last_dump >= 2000) {
            t_last_dump = millis();
            Serial.println();
            Serial.println("=========================================");
            Serial.println("[error-loop] VL53L7CX init FALLO. Diagnostico fresco:");
            scan_i2c_bus(Wire, "Wire (I2C0)");
            Serial.println("  Si NO aparece 0x29  -> sensor no responde por I2C.");
            Serial.println("    Causas: 3V3 caido, soldadura L7CX, XSHUT pin 2 mal ruteado,");
            Serial.println("            pull-ups I2C ausentes en SDA/SCL.");
            Serial.println("  Si APARECE 0x29     -> sensor responde pero la lib no inicializa.");
            Serial.println("    Probar: -DDIAG_TOF_SKIP_XSHUT en build_flags del env.");
            Serial.println("=========================================");
        }
        return;
    }

    // Heartbeat: LED ON mientras hay frames llegando.
    digitalWrite(PIN_LED_STATUS, HIGH);

    // NOTA: la lib STM32duino expone estos metodos con prefix `vl53l7cx_`
    // (verificado en lib/STM32duino_VL53L7CX/src/vl53l7cx_class.h:261,271).
    uint8_t ready = 0;
    int err = g_sensor.vl53l7cx_check_data_ready(&ready);
    if (err != 0 || !ready) return;

    VL53L7CX_ResultsData results;
    err = g_sensor.vl53l7cx_get_ranging_data(&results);
    if (err != 0) {
        Serial.print("[get_ranging_data err=");
        Serial.print(err);
        Serial.println("]");
        return;
    }

#if defined(DIAG_TOF_MODE_8X8)
    print_grid(results, 8);
#elif defined(DIAG_TOF_MODE_4X4)
    print_grid(results, 4);
#elif defined(DIAG_TOF_MODE_SINGLE)
    print_single(results, 4);   // single internamente usa 4x4 para promediar
#endif
}
