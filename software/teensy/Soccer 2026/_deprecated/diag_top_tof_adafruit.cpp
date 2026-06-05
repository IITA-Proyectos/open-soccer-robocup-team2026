// diag_top_tof_adafruit.cpp — Diagnostico del VL53L7CX usando la lib ADAFRUIT
// (Adafruit_VL53L7CX). NO es la lib STMicroelectronics (STM32duino_VL53L7CX)
// que se usa en los otros 3 sketches diag_top_tof*. Este es un TEST DE CONTROL.
//
// NO es firmware de competencia. Sketch standalone que valida si el chip
// VL53L7CX responde y mide cuando lo manejamos con la lib Adafruit.
//
// Contexto (Hardware-up 2026-05-24):
//   Las 3 libs STMicroelectronics (L5/L7/L8) fallaron en la placa TOP:
//     - diag_top_tof (L7CX)            -> err=255 en init
//     - diag_top_tof_as_l5cx (L5CX)    -> hang en carga de firmware blob
//     - diag_top_tof_as_l8cx (L8CX)    -> falla / no es L8CX
//   Si este sketch (Adafruit_VL53L7CX) FUNCIONA donde las 3 ST fallan,
//   el problema esta en las libs ST con este hardware especifico.
//   Si tampoco funciona, el problema es mas profundo: hardware, alimentacion,
//   modulo Pololu defectuoso, o chip remarcado / no L7CX genuino.
//
// Hardware esperado:
//   Setup A — placa TOP del robot:
//     • Teensy 4.0 de la placa TOP
//     • VL53L7CX en posicion U2 del schematic (mismo sensor que estuvo fallando)
//     • Bus Wire (I2C0): SDA=18, SCL=19
//     • XSHUT/LPn no usado por este sketch (igual que el hermano _as_l5cx).
//   Setup B — banco con Teensy 4.0 + VL53L7CX nuevo en protoboard:
//     • Cableado: VIN -> 3.3V Teensy, GND -> GND Teensy
//     •           SDA -> pin 18 Teensy, SCL -> pin 19 Teensy
//     •           Resto (LPn/INT/I2C_RST) al aire.
//     • El mismo binario sirve para ambos setups — el sketch es standalone,
//       no depende de config_top.h ni de otros modulos del firmware.
//
// Build / flash:
//   pio run -t clean -e diag_top_tof_adafruit
//   pio run -e diag_top_tof_adafruit -t upload
//   pio device monitor -b 115200
//
// Flags opcionales:
//   -DDIAG_TOF_SLOW_I2C    ; baja el clock a 100 kHz (default 400 kHz)
//
// IMPORTANTE — diferencias de API vs libs STMicroelectronics:
//   La lib Adafruit tiene una API mas simple, mas Arduino-style:
//     - Adafruit_VL53L7CX vl53l7cx;                       ctor sin args
//     - vl53l7cx.begin(addr, &Wire, 400000)               init completo
//     - vl53l7cx.setResolution(64)                        64 = 8x8, 16 = 4x4
//     - vl53l7cx.setRangingFrequency(15)                  Hz
//     - vl53l7cx.startRanging()
//     - vl53l7cx.isDataReady()                            devuelve bool
//     - vl53l7cx.getRangingData(&results)                 devuelve bool
//   Todos los setters devuelven `bool` (true = ok). El error pattern del LED
//   se dispara si CUALQUIER paso devuelve false.
//
// Creado 2026-05-24 para tener un test de control independiente de las libs ST.

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L7CX.h>

// ============================================================
// Pinout y constantes (todo local — sketch standalone)
// ============================================================
constexpr int PIN_LED_STATUS = 13;    // LED_BUILTIN.

// I2C clock: 400 kHz default (fast-mode standard, recomendado por el upstream
// Adafruit). Compilar con -DDIAG_TOF_SLOW_I2C para bajar a 100 kHz si se
// sospecha que la carga del firmware blob (~85 KB) falla por capacitancia
// o ruido en el bus (caso visto con las libs ST en este mismo hardware).
#ifdef DIAG_TOF_SLOW_I2C
constexpr uint32_t I2C_CLOCK_HZ = 100000;  // slow mode
#else
constexpr uint32_t I2C_CLOCK_HZ = 400000;  // fast-mode standard
#endif

// ============================================================
// Globales
// ============================================================
namespace {
// Ctor sin args (a diferencia de STM32duino que recibe TwoWire* + lpn_pin
// en el ctor). El bus y el address se pasan en begin().
Adafruit_VL53L7CX g_sensor;
VL53L7CX_ResultsData g_results;
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

// ============================================================
// Formatters de salida del frame ToF.
// ============================================================
// Formato comun: cada celda son 4 caracteres + 1 espacio. Valores >9999 se
// muestran como "XXXX" (status invalido o fuera de rango). status==5/6/9
// son "valid" segun la convencion ST comun a las 3 familias L5/L7/L8.
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

// IMPORTANTE — indexing "espejado" del ejemplo Adafruit:
//   El ejemplo Adafruit 02_ascii_art.ino usa este doble loop:
//       for (int x = width - 1; x >= 0; x--)
//         for (int y = width * (width - 1); y >= 0; y -= width)
//           int idx = x + y;
//   Esto recorre la grilla en orden visualmente correcto desde el punto de
//   vista del operador mirando al sensor desde el frente (la grilla aparece
//   sin rotar/mirror respecto a la escena). Lo replicamos identico para que
//   la salida sea consistente con el ejemplo upstream.
//
//   Esto difiere de los sketches diag_top_tof.cpp / _as_l5cx / _as_l8cx que
//   imprimen row-major directo (sin espejado). Las ASCII art impresas por
//   este sketch y por los hermanos ST NO van a verse iguales aunque el
//   sensor sea el mismo — pero los datos (distance_mm + target_status) si
//   son identicos. Esto NO es bug; es que el ejemplo Adafruit eligio una
//   orientacion mas "operador-friendly" para visualizar.
void print_grid_8x8(const VL53L7CX_ResultsData& r) {
    constexpr uint8_t width = 8;
    constexpr uint8_t n = width * width;

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

    // doble loop "espejado" — replicado del ejemplo Adafruit 02_ascii_art.ino
    for (int x = width - 1; x >= 0; x--) {
        Serial.print("  ");
        for (int y = width * (width - 1); y >= 0; y -= width) {
            int idx = x + y;
            print_cell(r.distance_mm[idx], r.target_status[idx]);
        }
        Serial.println();
    }
}

// Helper: equivalente al halt() del ejemplo Adafruit, pero en vez de
// `while (1)` deja `g_init_ok = false` y devuelve, asi el loop entra al
// error pattern del LED y al re-dump periodico del diagnostico.
void abort_init(const char* msg) {
    Serial.print("[init] FAIL: ");
    Serial.println(msg);
    Serial.println("[diag] entrando a loop con LED en error pattern (3 blinks rapidos).");
    g_init_ok = false;
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
    Serial.println("  diag_top_tof_adafruit — VL53L7CX via Adafruit lib");
    Serial.println("  (Test de control — si funciona con esta lib pero NO con STM32duino,");
    Serial.println("   el problema es de las libs ST con este hardware especifico)");
    Serial.println("=========================================");
    Serial.print  ("Board       : Teensy 4.0\n");
    Serial.print  ("Bus         : Wire (I2C0)  SDA=18  SCL=19\n");
    Serial.print  ("XSHUT/LPn   : NO usado (manejo manual fuera de la lib si hace falta)\n");
    Serial.print  ("Resolution  : 8x8 (64 zonas)\n");
    Serial.print  ("Freq        : 15 Hz\n");
    Serial.print  ("I2C clock   : "); Serial.print(I2C_CLOCK_HZ / 1000); Serial.println(" kHz");
    Serial.print  ("Build       : "); Serial.print(__DATE__); Serial.print(" "); Serial.println(__TIME__);
    Serial.println("-----------------------------------------");

    // ------ Wire init + scan ANTES de tocar la lib ------
    // begin() de la lib Adafruit hace Wire.begin() internamente, pero lo
    // arrancamos nosotros primero para poder hacer el scan I2C y mostrar
    // que sensores ven en el bus ANTES de la carga del firmware blob.
    Wire.begin();
    Wire.setClock(I2C_CLOCK_HZ);
    delay(5);
    scan_i2c_bus(Wire, "Wire (I2C0)");
    // Si NO aparece 0x29: el sensor no responde por I2C. Stop, problema fisico.

    // ------ Init del sensor (Adafruit API) ------
    // begin() devuelve bool. Internamente:
    //   1) Crea Adafruit_I2CDevice en la address y bus indicados.
    //   2) Llama vl53l7cx_is_alive() (ping al chip).
    //   3) Llama vl53l7cx_init() — esto carga el firmware blob (~85 KB)
    //      por I2C, tarda hasta 10 s segun el upstream README.
    Serial.print("[init] vl53l7cx.begin(0x29, &Wire, ");
    Serial.print(I2C_CLOCK_HZ);
    Serial.println(") ... (esto puede tardar hasta 10s mientras carga firmware blob)");
    if (!g_sensor.begin(VL53L7CX_DEFAULT_ADDRESS, &Wire, I2C_CLOCK_HZ)) {
        abort_init("begin() devolvio false — sensor no inicializa con lib Adafruit");
        return;  // g_init_ok queda false → loop entra a error pattern
    }
    Serial.println("[init] begin() OK — sensor inicializado");

    // ------ setResolution(64) — 8x8 (64 zonas) ------
    // Default del ejemplo Adafruit upstream. 8x8 son 64 zonas a 15 Hz.
    if (!g_sensor.setResolution(64)) {
        abort_init("setResolution(64) devolvio false");
        return;
    }
    Serial.println("[init] resolution = 8x8 (64 zonas)");

    // ------ setRangingFrequency(15) — 15 Hz ------
    if (!g_sensor.setRangingFrequency(15)) {
        abort_init("setRangingFrequency(15) devolvio false");
        return;
    }
    Serial.println("[init] ranging frequency = 15 Hz");

    // ------ startRanging() ------
    if (!g_sensor.startRanging()) {
        abort_init("startRanging() devolvio false");
        return;
    }
    Serial.println("[init] startRanging() OK — listo. Esperando frames...");

    g_init_ok = true;
    Serial.println("*** FUNCIONA con lib Adafruit ***");
    Serial.println("    Si las 3 libs ST fallaron y esta funciona, el problema");
    Serial.println("    estaba en las libs ST con este hardware especifico.");
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
            Serial.println("[error-loop] Adafruit VL53L7CX init FALLO. Diagnostico fresco:");
            scan_i2c_bus(Wire, "Wire (I2C0)");
            Serial.println("  Si NO aparece 0x29  -> sensor no responde por I2C.");
            Serial.println("    Causas: 3V3 caido, soldadura del modulo, pull-ups");
            Serial.println("            I2C ausentes en SDA/SCL, cableado incorrecto.");
            Serial.println("  Si APARECE 0x29     -> sensor responde pero la lib Adafruit");
            Serial.println("    tampoco lo inicializa. Las 4 libs (Adafruit + 3 ST) fallaron.");
            Serial.println("    Problema mas profundo: hardware fisico, alimentacion,");
            Serial.println("    chip remarcado / no L7CX genuino, o modulo defectuoso.");
            Serial.println("=========================================");
        }
        return;
    }

    // Heartbeat: LED ON mientras hay frames llegando.
    digitalWrite(PIN_LED_STATUS, HIGH);

    // API Adafruit: isDataReady() y getRangingData() devuelven bool.
    if (!g_sensor.isDataReady()) return;
    if (!g_sensor.getRangingData(&g_results)) {
        Serial.println("[get_ranging_data devolvio false]");
        return;
    }

    print_grid_8x8(g_results);
}
