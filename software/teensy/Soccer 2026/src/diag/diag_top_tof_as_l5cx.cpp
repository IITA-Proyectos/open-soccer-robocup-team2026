// diag_top_tof_as_l5cx.cpp — Diagnostico del sensor ToF DESCONOCIDO en U2 (placa TOP),
// probando si es un VL53L5CX.
//
// NO es firmware de competencia. Sketch standalone que prueba si el chip soldado
// en U2 responde y se inicializa con la lib L5CX. Hermano de diag_top_tof.cpp
// (que asume L7CX). El equipo compro VL53L5/L7/L8 mezclados sin trazabilidad y
// los carriers Pololu son fisicamente identicos (mismo PCB irs18a).
//
// Decision tree (Hardware-up 2026-05-24):
//   - diag_top_tof (lib L7CX)            -> init falla err=255  -> NO es L7CX
//   - diag_top_tof_as_l5cx (este sketch) -> init OK             -> ES L5CX
//   - diag_top_tof_as_l5cx               -> init falla err=255  -> NO es L5CX
//                                                                  proximo: L8CX
//
// Hardware esperado (segun pack 2026-05-24):
//   • Teensy 4.0 de la placa TOP
//   • Sensor desconocido (L5/L7/L8) en posicion U2 del schematic
//   • Bus Wire (I2C0): SDA=18, SCL=19
//   • XSHUT del U2 NO esta ruteado al pin 2 del Teensy (validado en
//     diag_top_tof: SKIP_XSHUT no cambia el resultado). Este sketch tiene
//     lpn_pin=-1 baked in — simpler this way.
//
// Build / flash:
//   pio run -e diag_top_tof_as_l5cx -t upload
//
// Modos (cambiar SOLO uno via build_flags):
//   -DDIAG_TOF_MODE_SINGLE   ; 1 numero promedio
//   -DDIAG_TOF_MODE_4X4      ; (default) grilla 4x4 ASCII
//   -DDIAG_TOF_MODE_8X8      ; grilla 8x8 ASCII
//
// Creado 2026-05-24 para identificar el chip desconocido en U2.

#include <Arduino.h>
#include <Wire.h>
#include <vl53l5cx_class.h>

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
constexpr int PIN_LED_STATUS      = 13;    // LED_BUILTIN.

// I2C clock: 400 kHz default (fast-mode standard). Si init_sensor() falla
// con err=255 aunque el scan I2C vea el sensor, probablemente la transferencia
// del firmware blob esta fallando por capacitancia/ruido en el bus.
// Compilar con -DDIAG_TOF_SLOW_I2C para bajar a 100 kHz (slow mode).
#ifdef DIAG_TOF_SLOW_I2C
constexpr uint32_t I2C_CLOCK_HZ   = 100000;  // slow mode
#else
constexpr uint32_t I2C_CLOCK_HZ   = 400000;  // fast-mode standard
#endif

// ============================================================
// Globales
// ============================================================
namespace {
// Constructor: (TwoWire*, lpn_pin/XSHUT). lpn_pin=-1 baked in porque el
// carrier Pololu no tiene XSHUT ruteado al Teensy (ya validado en
// diag_top_tof: el toggle XSHUT no cambia el resultado). La lib chequea
// `if (lpn_pin >= 0)` en vl53l5cx_class.h:82 — pasar -1 hace que el init
// no manipule el pin.
VL53L5CX g_sensor(&Wire, -1);
bool g_init_ok = false;

// I2C scanner — recorre addresses 7-bit (1..127) en bus Wire y reporta los
// que responden con ACK. Util para distinguir "sensor no responde I2C" vs
// "responde pero la lib falla en init".
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
// int16_t pero la lib clampea negativos a 0 (vl53l5cx_api.cpp:704), asi que
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

void print_grid(const VL53L5CX_ResultsData& r, uint8_t side) {
    // VL53L5CX devuelve los datos en row-major desde la esquina top-left de la
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

void print_single(const VL53L5CX_ResultsData& r, uint8_t side) {
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
    Serial.println("  diag_top_tof_as_l5cx — probando U2 como VL53L5CX");
    Serial.println("  (sensor desconocido: L5/L7/L8 mezclados sin trazabilidad)");
    Serial.println("  Si init OK -> ES L5CX. Si err=255 -> NO es L5CX, probar L8CX.");
    Serial.println("=========================================");
    Serial.print  ("Board       : Teensy 4.0 (TOP)\n");
    Serial.print  ("Bus         : Wire (I2C0)  SDA=18  SCL=19\n");
    Serial.print  ("XSHUT       : SKIPPED baked in (lpn_pin=-1)\n");
    Serial.print  ("Mode        : "); Serial.println(mode_name());
    Serial.print  ("I2C clock   : "); Serial.print(I2C_CLOCK_HZ / 1000); Serial.println(" kHz");
    Serial.print  ("Build       : "); Serial.print(__DATE__); Serial.print(" "); Serial.println(__TIME__);
    Serial.println("-----------------------------------------");

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

    Serial.print("[init] loading firmware ULD L5CX (~3 s) ... ");
    int err = g_sensor.init_sensor();
    if (err != 0) {
        Serial.print("FAILED err="); Serial.println(err);
        Serial.println("[diag] init L5CX fallo — el chip NO es un VL53L5CX (o algo fisico).");
        Serial.println("       Posibles causas:");
        Serial.println("       - el chip es L7CX (probar [env:diag_top_tof_no_xshut])");
        Serial.println("       - el chip es L8CX (proximo commit vendorea L8CX)");
        Serial.println("       - alimentacion 3V3 baja / soldadura del modulo");
        Serial.println("[diag] entrando a loop con LED en error pattern (3 blinks rapidos).");
        return;  // g_init_ok queda false → loop entra a error pattern
    }
    Serial.println("OK");
    Serial.println("*** ES VL53L5CX *** init_sensor() devolvio 0 con la lib L5CX.");

    // ------ Configurar resolucion + frecuencia ------
    // Nota: la lib STM32duino expone los metodos con prefix `vl53l5cx_`
    // (no `set_resolution` plain). Ver lib/STM32duino_VL53L5CX/src/vl53l5cx_class.h.
#if defined(DIAG_TOF_MODE_8X8)
    g_sensor.vl53l5cx_set_resolution(VL53L5CX_RESOLUTION_8X8);
    Serial.println("[init] resolution = 8x8");
#else
    g_sensor.vl53l5cx_set_resolution(VL53L5CX_RESOLUTION_4X4);
    Serial.println("[init] resolution = 4x4");
#endif
    g_sensor.vl53l5cx_set_ranging_frequency_hz(15);
    g_sensor.vl53l5cx_set_ranging_mode(VL53L5CX_RANGING_MODE_CONTINUOUS);

    g_sensor.vl53l5cx_start_ranging();
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
            Serial.println("[error-loop] VL53L5CX init FALLO. Diagnostico fresco:");
            scan_i2c_bus(Wire, "Wire (I2C0)");
            Serial.println("  Si NO aparece 0x29  -> sensor no responde por I2C.");
            Serial.println("    Causas: 3V3 caido, soldadura del modulo Pololu,");
            Serial.println("            pull-ups I2C ausentes en SDA/SCL.");
            Serial.println("  Si APARECE 0x29     -> sensor responde pero NO es L5CX.");
            Serial.println("    Proximo paso: vendorear lib L8CX y probar [env:diag_top_tof_as_l8cx].");
            Serial.println("=========================================");
        }
        return;
    }

    // Heartbeat: LED ON mientras hay frames llegando.
    digitalWrite(PIN_LED_STATUS, HIGH);

    // NOTA: la lib STM32duino expone estos metodos con prefix `vl53l5cx_`
    // (verificado en lib/STM32duino_VL53L5CX/src/vl53l5cx_class.h:262,272).
    uint8_t ready = 0;
    int err = g_sensor.vl53l5cx_check_data_ready(&ready);
    if (err != 0 || !ready) return;

    VL53L5CX_ResultsData results;
    err = g_sensor.vl53l5cx_get_ranging_data(&results);
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
