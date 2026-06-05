// diag_top_tof_as_l8cx.cpp — Diagnostico del sensor ToF DESCONOCIDO en U2 (placa TOP),
// probando si es un VL53L8CX. Ultimo candidato del trio L5/L7/L8.
//
// NO es firmware de competencia. Sketch standalone que prueba si el chip soldado
// en U2 responde y se inicializa con la lib L8CX. Hermano de diag_top_tof.cpp
// (L7CX) y diag_top_tof_as_l5cx.cpp (L5CX). El equipo compro VL53L5/L7/L8
// mezclados sin trazabilidad y los carriers Pololu son fisicamente identicos
// (mismo PCB irs18a).
//
// Decision tree (Hardware-up 2026-05-24):
//   - diag_top_tof (lib L7CX)            -> init falla err=255        -> NO es L7CX
//   - diag_top_tof_as_l5cx (lib L5CX)    -> se cuelga en firmware load -> NO es L5CX
//   - diag_top_tof_as_l8cx (este sketch) -> init OK                    -> ES L8CX
//   - diag_top_tof_as_l8cx               -> init falla                 -> NO es ninguno
//                                                                          de los 3 -> revisar chip
//                                                                          fisicamente o reemplazar
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
//   pio run -e diag_top_tof_as_l8cx -t upload
//
// Modos (cambiar SOLO uno via build_flags):
//   -DDIAG_TOF_MODE_SINGLE   ; 1 numero promedio
//   -DDIAG_TOF_MODE_4X4      ; (default) grilla 4x4 ASCII
//   -DDIAG_TOF_MODE_8X8      ; grilla 8x8 ASCII
//
// IMPORTANTE — diferencias de API vs L5CX/L7CX:
//   El wrapper L8CX expone los metodos del ULD SIN PREFIJO (init, set_resolution,
//   check_data_ready, get_ranging_data, start_ranging, set_ranging_frequency_hz,
//   set_ranging_mode). En L5CX/L7CX vienen con prefijo (vl53l5cx_set_resolution,
//   vl53l7cx_check_data_ready, etc.). Ver lib/STM32duino_VL53L8CX/README.md.
//   Tambien el header se llama vl53l8cx.h (no vl53l8cx_class.h) y el metodo de
//   carga del firmware blob es init() (no init_sensor()).
//
// Creado 2026-05-24 para identificar el chip desconocido en U2 — tercer y
// ultimo intento del trio.

#include <Arduino.h>
#include <Wire.h>
#include <vl53l8cx.h>

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

// I2C clock: 400 kHz default (fast-mode standard). Si init() falla
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
// Constructor I2C: (TwoWire*, lpn_pin, i2c_rst_pin=-1). lpn_pin=-1 baked
// in porque el carrier Pololu no tiene XSHUT ruteado al Teensy (ya validado
// en diag_top_tof: el toggle XSHUT no cambia el resultado). La lib chequea
// `if (lpn_pin >= 0)` antes de manipular el pin — pasar -1 hace que init
// no lo toque. i2c_rst_pin tambien queda en default -1 (no usado).
//
// El wrapper L8CX tambien expone un constructor SPI, pero el modulo Pololu
// que tenemos esta cableado I2C (SPI no esta ruteado), asi que usamos el
// constructor I2C exclusivamente.
VL53L8CX g_sensor(&Wire, -1);
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
// int16_t. El status==5/6/9 son "valid" segun la convencion ST comun a las
// 3 familias L5/L7/L8.
void print_cell(int16_t mm, uint8_t status) {
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

void print_grid(const VL53L8CX_ResultsData& r, uint8_t side) {
    // VL53L8CX devuelve los datos en row-major desde la esquina top-left de la
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

void print_single(const VL53L8CX_ResultsData& r, uint8_t side) {
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
    Serial.println("  diag_top_tof_as_l8cx — probando U2 como VL53L8CX");
    Serial.println("  (sensor desconocido: L5/L7/L8 mezclados sin trazabilidad)");
    Serial.println("  Ultimo candidato del trio. Si init OK -> ES L8CX.");
    Serial.println("  Si falla -> NINGUNO de los 3 sirve, revisar chip fisicamente.");
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
    // NOTA: el wrapper L8CX usa metodos plain (sin prefix vl53l8cx_).
    // begin() prepara el objeto + arranca Wire.begin() si hace falta.
    // init() carga el firmware blob (~3 s). Distinto a L5CX/L7CX donde el
    // metodo se llama init_sensor().
    Serial.print("[init] g_sensor.begin() ... ");
    int err_begin = g_sensor.begin();
    if (err_begin != 0) {
        Serial.print("FAILED begin err="); Serial.println(err_begin);
        Serial.println("[diag] begin() L8CX fallo — el chip no responde basico.");
        return;
    }
    Serial.println("ok");

    Serial.print("[init] loading firmware ULD L8CX (~3 s) ... ");
    uint8_t err = g_sensor.init();
    if (err != 0) {
        Serial.print("FAILED err="); Serial.println(err);
        Serial.println("[diag] init L8CX fallo — el chip NO es un VL53L8CX.");
        Serial.println("       Tampoco era L5CX ni L7CX (probados en commits previos).");
        Serial.println("       Posibles causas:");
        Serial.println("       - el chip es de otra familia (raro: Pololu solo vende L5/L7/L8");
        Serial.println("         en ese carrier irs18a)");
        Serial.println("       - alimentacion 3V3 baja / soldadura del modulo");
        Serial.println("       - silicon dead-on-arrival (raro pero posible)");
        Serial.println("       - lote falsificado / chip remarcado");
        Serial.println("       Proximo paso fuera de software: lupa al marking del chip");
        Serial.println("       o reemplazar el modulo Pololu por uno con trazabilidad.");
        Serial.println("[diag] entrando a loop con LED en error pattern (3 blinks rapidos).");
        return;  // g_init_ok queda false → loop entra a error pattern
    }
    Serial.println("OK");
    Serial.println("*** ES VL53L8CX *** init() devolvio 0 con la lib L8CX.");

    // ------ Configurar resolucion + frecuencia ------
    // Nota: metodos plain (sin prefix). Ver lib/STM32duino_VL53L8CX/src/vl53l8cx.h:73,75,83.
#if defined(DIAG_TOF_MODE_8X8)
    g_sensor.set_resolution(VL53L8CX_RESOLUTION_8X8);
    Serial.println("[init] resolution = 8x8");
#else
    g_sensor.set_resolution(VL53L8CX_RESOLUTION_4X4);
    Serial.println("[init] resolution = 4x4");
#endif
    g_sensor.set_ranging_frequency_hz(15);
    g_sensor.set_ranging_mode(VL53L8CX_RANGING_MODE_CONTINUOUS);

    g_sensor.start_ranging();
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
            Serial.println("[error-loop] VL53L8CX init FALLO. Diagnostico fresco:");
            scan_i2c_bus(Wire, "Wire (I2C0)");
            Serial.println("  Si NO aparece 0x29  -> sensor no responde por I2C.");
            Serial.println("    Causas: 3V3 caido, soldadura del modulo Pololu,");
            Serial.println("            pull-ups I2C ausentes en SDA/SCL.");
            Serial.println("  Si APARECE 0x29     -> sensor responde pero NO es L8CX.");
            Serial.println("    Tampoco era L5CX ni L7CX (probados en commits previos).");
            Serial.println("    Proximo paso: lupa al marking del chip o reemplazar modulo.");
            Serial.println("=========================================");
        }
        return;
    }

    // Heartbeat: LED ON mientras hay frames llegando.
    digitalWrite(PIN_LED_STATUS, HIGH);

    // NOTA: metodos plain (sin prefix vl53l8cx_). Ver vl53l8cx.h:70,71.
    uint8_t ready = 0;
    uint8_t err = g_sensor.check_data_ready(&ready);
    if (err != 0 || !ready) return;

    VL53L8CX_ResultsData results;
    err = g_sensor.get_ranging_data(&results);
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
