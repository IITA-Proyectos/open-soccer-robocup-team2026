# diag_top_tof Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Crear sketch standalone `diag_top_tof` que valida en hardware real que el VL53L7CX frontal (posición U2) de la placa TOP responde y mide, sin tocar el firmware de competencia.

**Architecture:** Lib `STM32duino_VL53L7CX` vendoreada en `lib/` + sketch único en `src/diag/diag_top_tof.cpp` + nuevo env `[env:diag_top_tof]` en `platformio.ini`. Sigue patrón ya establecido por `[env:diag_down]`. Modos seleccionables por `#define` (SINGLE / 4×4 / 8×8). Sin tests host-native (sketch standalone que habla I²C con sensor físico — la validación real es por flasheo + Serial Monitor + equipo humano siguiendo §4 del spec).

**Tech Stack:** Teensy 4.0 + PlatformIO + Arduino framework + Wire (I²C0) + STM32duino_VL53L7CX (BSD-3-Clause).

**Spec aprobado:** [docs/superpowers/specs/2026-05-24-diag-top-tof-vl53l7cx-design.md](../specs/2026-05-24-diag-top-tof-vl53l7cx-design.md)

**Atribución (TODOS los commits):**
```
Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)
```

---

## Task 1: Vendorear lib STM32duino_VL53L7CX

**Files:**
- Create: `software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/` (directorio + contenido podado)
- Create: `software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/README.md`
- Modify: `software/teensy/Soccer 2026/lib/README.md` (agregar fila a tabla de contenido)

- [ ] **Step 1: Verificar que git + acceso a internet funcionan**

```bash
git --version
git ls-remote https://github.com/stm32duino/VL53L7CX.git HEAD
```

Expected: muestra versión de git + un commit hash + `HEAD`. Si el `ls-remote` falla por SSL/Avast, aplicar excepción de TASK-025 antes de seguir.

- [ ] **Step 2: Clonar upstream a directorio temporal**

```bash
git clone --depth 1 https://github.com/stm32duino/VL53L7CX.git /tmp/vl53l7cx-vendor
```

(En Windows si `/tmp` no existe usar `$env:TEMP\vl53l7cx-vendor` desde PowerShell.)

Expected: `Cloning into '/tmp/vl53l7cx-vendor'... done.`

- [ ] **Step 3: Capturar commit hash y tag para README de vendoreo**

```bash
cd /tmp/vl53l7cx-vendor
git rev-parse HEAD                     # commit hash
git describe --tags --abbrev=0 2>&1    # último tag (puede no existir)
ls                                     # ver estructura
```

Guardar los outputs — se usan en Step 7.

- [ ] **Step 4: Crear directorio destino**

```bash
mkdir -p "software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX"
```

- [ ] **Step 5: Copiar SOLO lo necesario (siguiendo política `lib/README.md`)**

Política del repo (`lib/README.md` línea 57-59): "copiar SOLO los `.cpp`, `.h`, `utility/`, `src/`, `library.properties`/`library.json`, `LICENSE`. NO copiar `examples/`, `docs/`".

```bash
cp -r /tmp/vl53l7cx-vendor/src "software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/"
cp /tmp/vl53l7cx-vendor/library.json "software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/" 2>/dev/null || true
cp /tmp/vl53l7cx-vendor/library.properties "software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/" 2>/dev/null || true
cp /tmp/vl53l7cx-vendor/LICENSE "software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/" 2>/dev/null || true
cp /tmp/vl53l7cx-vendor/LICENSE.md "software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/" 2>/dev/null || true
```

(`2>/dev/null || true` porque library.json/library.properties/LICENSE pueden no existir — al menos uno de `library.{json,properties}` debe estar.)

- [ ] **Step 6: Verificar contenido vendoreado**

```bash
ls "software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/"
ls "software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/src/"
du -sh "software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/"
```

Expected: 
- En root: `src/`, `library.{json,properties}`, `LICENSE` (al menos uno de cada).
- En `src/`: archivos `.h`/`.cpp` del wrapper (típicamente `vl53l7cx_class.{h,cpp}`) + subdirectorio `core/` o `platform/` con el ULD driver C de ST.
- Tamaño total: 200-400 KB (incluye firmware blob ~85 KB del L7CX).

Si `src/` está vacío o no contiene `.cpp/.h`: stop, investigar layout del repo upstream antes de seguir.

- [ ] **Step 7: Crear README de vendoreo**

Crear `software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/README.md`:

```markdown
# STM32duino_VL53L7CX — librería vendoreada

Driver Arduino oficial para el sensor ToF multizona **VL53L7CX** de
STMicroelectronics. Wrapper C++ sobre el ULD ("Ultra Lite Driver") C nativo
de ST.

## Origen

- **Upstream:** https://github.com/stm32duino/VL53L7CX
- **Commit vendoreado:** `<COMMIT_HASH_DEL_STEP_3>`
- **Tag más cercano:** `<TAG_DEL_STEP_3 o "ninguno">`
- **License:** BSD-3-Clause (ver `LICENSE`)
- **Fecha de vendoreo:** 2026-05-24
- **Vendoreado por:** Claude Opus 4.7 (Anthropic) — Requested-by Gustavo Viollaz (@gviollaz)

## Por qué está vendoreada

Política del repo (`lib/README.md`): todas las dependencias de firmware se
vendorean para compilar 100% offline (Avast/SSL roto, Incheon sin wifi
garantizado). Igual que Adafruit_BNO055, Adafruit_BusIO, etc.

## Quién la usa

- `[env:diag_top_tof]` — sketch standalone para validar el VL53L7CX frontal U2
  de la placa TOP.

Cuando los TOFs se integren al firmware vivo (`src/top/sensors_tof.cpp`),
esta lib también la usará el `[env:top]`.

## Contenido podado

Se removieron `examples/`, `.git/`, `.github/`, `docs/`, `extras/`,
`keywords.txt` (siguiendo `lib/README.md`). Se conservaron: `src/`,
`library.{json,properties}`, `LICENSE`.

## Cómo actualizar

Procedimiento estándar — ver `lib/README.md` sección "Actualizar una librería
vendoreada".

## Referencias

- Spec del primer uso: `docs/superpowers/specs/2026-05-24-diag-top-tof-vl53l7cx-design.md`
- Datasheet sensor: https://www.st.com/resource/en/datasheet/vl53l7cx.pdf
```

Reemplazar `<COMMIT_HASH_DEL_STEP_3>` y `<TAG_DEL_STEP_3>` con los valores reales del Step 3.

- [ ] **Step 8: Actualizar tabla en `lib/README.md`**

Editar `software/teensy/Soccer 2026/lib/README.md` línea 16-20. Agregar fila a la tabla de "Contenido":

```diff
| `Adafruit_BusIO/` | I2C/SPI helper (dep. de BNO055) | idem | adafruit/Adafruit BusIO |
| `Adafruit_Unified_Sensor/` | Interfaz de sensor (dep. de BNO055) | idem | adafruit/Adafruit Unified Sensor |
+| `STM32duino_VL53L7CX/` | Driver ToF multizona VL53L7CX | `diag_top_tof` (futuro `top` cuando se integre) | stm32duino/VL53L7CX |
```

Y actualizar la línea 11 con el total nuevo:
```diff
-> Total: ~475 KB.
+> Total: ~750 KB (incluye VL53L7CX vendoreada 2026-05-24 con firmware blob ~85 KB).
```

- [ ] **Step 9: Borrar clone temporal**

```bash
rm -rf /tmp/vl53l7cx-vendor
```

- [ ] **Step 10: Commit**

```bash
git add "software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/" "software/teensy/Soccer 2026/lib/README.md"
git commit -m "$(cat <<'EOF'
feat(lib): vendorear STM32duino_VL53L7CX para diag_top_tof

Driver Arduino oficial del sensor ToF multizona VL53L7CX (wrapper C++
sobre ULD C nativo de ST). License BSD-3-Clause. Origen:
github.com/stm32duino/VL53L7CX commit <COMMIT_HASH>.

Vendoreada siguiendo politica del repo (lib/README.md): podada de
examples/docs/.git, solo .cpp/.h/library.json/LICENSE. Compilable
100% offline (Avast/Incheon sin wifi).

Primer uso: [env:diag_top_tof] que se agrega en commit siguiente.
Cuando se integre al firmware vivo (sensors_tof.cpp) la va a usar [env:top].

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)
EOF
)"
```

(Reemplazar `<COMMIT_HASH>` con el valor real.)

---

## Task 2: Skeleton del sketch + env compilable (compile gate)

**Files:**
- Create: `software/teensy/Soccer 2026/src/diag/diag_top_tof.cpp` (skeleton mínimo)
- Modify: `software/teensy/Soccer 2026/platformio.ini` (agregar `[env:diag_top_tof]`)

- [ ] **Step 1: Verificar header de la lib vendoreada**

```bash
ls "software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/src/" | grep -i "\.h$"
```

Anotar el nombre del header principal (típicamente `vl53l7cx_class.h` o `vl53l7cx.h`). Se usa en Step 2.

- [ ] **Step 2: Crear skeleton del sketch**

Crear `software/teensy/Soccer 2026/src/diag/diag_top_tof.cpp`:

```cpp
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
#include <vl53l7cx_class.h>   // ⚠ ajustar si Step 1 mostro otro header

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
}  // namespace

// ============================================================
// setup() / loop() — skeleton: solo banner + LED. Se llenan en Tasks 3-4.
// ============================================================
void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* esperar al monitor, max 3 s */ }

    Serial.println("\n=== diag_top_tof (skeleton) ===");
    Serial.println("Build OK. Se completa en Tasks 3-4 del plan.");
}

void loop() {
    // Heartbeat visible: LED parpadea a 1 Hz.
    static uint32_t last = 0;
    if (millis() - last >= 500) {
        last = millis();
        digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
    }
}
```

- [ ] **Step 3: Agregar env `[env:diag_top_tof]` al `platformio.ini`**

Editar `software/teensy/Soccer 2026/platformio.ini`. Agregar al final del archivo:

```ini

; ============================================================
; diag_top_tof — DIAGNOSTICO de hardware del VL53L7CX frontal (placa TOP)
; NO es firmware de competencia. Carga esto, abri Serial Monitor a 115200,
; y verificas que el TOF mide.
;   pio run -e diag_top_tof -t upload
;
; Modos (cambiar SOLO uno):
;   -DDIAG_TOF_MODE_SINGLE   ; 1 numero promedio
;   -DDIAG_TOF_MODE_4X4      ; (default) grilla 4x4 ASCII
;   -DDIAG_TOF_MODE_8X8      ; grilla 8x8 ASCII
;
; Fallback si XSHUT pin 2 no esta ruteado:
;   -DDIAG_TOF_SKIP_XSHUT    ; asume sensor siempre alimentado
;
; Spec: docs/superpowers/specs/2026-05-24-diag-top-tof-vl53l7cx-design.md
; ============================================================
[env:diag_top_tof]
platform = teensy
board = teensy40
framework = arduino
build_flags =
    -DBOARD_TOP_DIAG
    -DDIAG_TOF_MODE_4X4
    -std=gnu++17
build_unflags = -std=gnu++11
build_src_filter = +<diag/diag_top_tof.cpp>
; lib_deps: STM32duino_VL53L7CX vendoreada en lib/ (igual que BNO055/BusIO).
; PIO LDF la detecta sola; NO se baja del registry. 100% offline.
```

- [ ] **Step 4: Compilar (compile gate del skeleton)**

```bash
cd "software/teensy/Soccer 2026"
pio run -e diag_top_tof
```

Expected: termina en `========= [SUCCESS] =========` con tamaño RAM/Flash (~50-100 KB flash para el skeleton).

**Si falla:**
- "fatal error: vl53l7cx_class.h: No such file" → el header tiene otro nombre. Ver Step 1 y ajustar `#include` en el sketch.
- "undefined reference to VL53L7CX::..." → la lib no se está linkeando. Verificar que `lib/STM32duino_VL53L7CX/src/` tiene `.cpp` además de `.h`.
- Cualquier otro error: fix antes de seguir.

- [ ] **Step 5: Commit**

```bash
git add "software/teensy/Soccer 2026/src/diag/diag_top_tof.cpp" "software/teensy/Soccer 2026/platformio.ini"
git commit -m "$(cat <<'EOF'
feat(diag): skeleton diag_top_tof + env [env:diag_top_tof] compilable

Sketch standalone para validar VL53L7CX frontal U2 de la placa TOP.
Skeleton minimo (banner + LED heartbeat) que solo prueba que la lib
vendoreada VL53L7CX se linkea OK con Teensy 4.0. Sigue patron de
main_diag_down.cpp / [env:diag_down].

Modos por define (mutuamente excluyentes con #error):
- DIAG_TOF_MODE_SINGLE
- DIAG_TOF_MODE_4X4 (default)
- DIAG_TOF_MODE_8X8

NO toca firmware TOP vivo (sensors_tof.cpp sigue siendo stub).

Setup() / loop() reales se completan en commits siguientes.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)
EOF
)"
```

---

## Task 3: Implementar setup() — I²C scan + init del sensor

**Files:**
- Modify: `software/teensy/Soccer 2026/src/diag/diag_top_tof.cpp` (reemplazar `setup()`)

- [ ] **Step 1: Agregar utilidad `scan_i2c_bus()` (mismo patrón que diag_down)**

Insertar en `diag_top_tof.cpp` ANTES de `setup()`, dentro del namespace anónimo:

```cpp
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
```

- [ ] **Step 2: Reemplazar `setup()` con la implementación real**

Reemplazar el `setup()` completo por:

```cpp
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
#if defined(DIAG_TOF_MODE_8X8)
    g_sensor.set_resolution(VL53L7CX_RESOLUTION_8X8);
    Serial.println("[init] resolution = 8x8");
#else
    g_sensor.set_resolution(VL53L7CX_RESOLUTION_4X4);
    Serial.println("[init] resolution = 4x4");
#endif
    g_sensor.set_ranging_frequency_hz(15);
    g_sensor.set_ranging_mode(VL53L7CX_RANGING_MODE_CONTINUOUS);

    g_sensor.start_ranging();
    g_init_ok = true;
    Serial.println("[init] start_ranging — listo. Esperando frames...");
    Serial.println();
}
```

- [ ] **Step 3: Compilar (compile gate)**

```bash
cd "software/teensy/Soccer 2026"
pio run -e diag_top_tof
```

Expected: `[SUCCESS]`.

**Si falla con "no member named 'init_sensor'"** o similar: el API de la lib STM32duino puede usar `begin()` (no `init_sensor`) o nombres ligeramente distintos. Ver `lib/STM32duino_VL53L7CX/src/vl53l7cx_class.h` y ajustar las llamadas. Hacer el ajuste mínimo, recompilar.

**Si falla con "VL53L7CX_RESOLUTION_4X4 undeclared"**: incluir el header del API ULD, típicamente `#include <vl53l7cx_api.h>` además del wrapper.

- [ ] **Step 4: Commit**

```bash
git add "software/teensy/Soccer 2026/src/diag/diag_top_tof.cpp"
git commit -m "$(cat <<'EOF'
feat(diag): implementar setup() del diag_top_tof — I2C scan + init sensor

Banner completo + I2C scan en bus Wire ANTES de tocar la lib (asi se
distingue "no responde I2C" vs "lib falla en init"). XSHUT pin 2 HIGH
con #ifndef DIAG_TOF_SKIP_XSHUT (fallback documentado). Init de la lib
con manejo de error que printea causas posibles. Configura resolucion
segun MODE define y arranca ranging continuo @ 15 Hz.

Errores de init imprimen diagnostico humano-leible y dejan g_init_ok
en false; loop() (proximo commit) entra a patron LED de error.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)
EOF
)"
```

---

## Task 4: Implementar loop() + formatters según MODE

**Files:**
- Modify: `software/teensy/Soccer 2026/src/diag/diag_top_tof.cpp` (reemplazar `loop()` + agregar formatters)

- [ ] **Step 1: Agregar formatters dentro del namespace anónimo (ANTES de setup)**

Insertar después de `mode_name()`, antes de `setup()`:

```cpp
// Formato comun: cada celda son 4 caracteres + 1 espacio. Valores >9999 se
// muestran como "XXXX" (status invalido o fuera de rango).
void print_cell(uint16_t mm, uint8_t status) {
    // status==5 = "range valid". 6/9 = "range valid + reflectance/sigma warning".
    // Otros = invalido.
    bool valid = (status == 5 || status == 6 || status == 9);
    if (!valid || mm > 9999) {
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
            sum += r.distance_mm[i];
            ++valid_count;
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
```

- [ ] **Step 2: Reemplazar `loop()` con la implementación real**

Reemplazar el `loop()` completo por:

```cpp
void loop() {
    // Error pattern si init fallo: 3 blinks rapidos cada 1 s.
    if (!g_init_ok) {
        static uint32_t t0 = 0;
        uint32_t phase = (millis() - t0) % 1000;
        bool on = (phase < 100) || (phase >= 200 && phase < 300) || (phase >= 400 && phase < 500);
        digitalWrite(PIN_LED_STATUS, on ? HIGH : LOW);
        return;
    }

    // Heartbeat: LED ON mientras hay frames llegando.
    digitalWrite(PIN_LED_STATUS, HIGH);

    uint8_t ready = 0;
    int err = g_sensor.check_data_ready(&ready);
    if (err != 0 || !ready) return;

    VL53L7CX_ResultsData results;
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
```

- [ ] **Step 3: Compilar default (4x4)**

```bash
cd "software/teensy/Soccer 2026"
pio run -e diag_top_tof
```

Expected: `[SUCCESS]`.

**Si falla** con "VL53L7CX_ResultsData no es POD" o "campo distance_mm no existe": el wrapper STM32duino puede exponer la estructura con otro nombre (típicamente `VL53L7CX_ResultsData` sí es el oficial, pero verificar en `lib/STM32duino_VL53L7CX/src/`). Buscar el header con el struct y ajustar.

- [ ] **Step 4: Validar que los OTROS modos compilan (editar env + recompilar + revertir)**

Para cada combinación, editar `[env:diag_top_tof]` en `platformio.ini`, compilar, y **revertir el archivo a su estado original** antes de pasar a la siguiente combinación.

**4a — modo SINGLE:** cambiar `-DDIAG_TOF_MODE_4X4` por `-DDIAG_TOF_MODE_SINGLE`:
```bash
pio run -e diag_top_tof
# Expected: [SUCCESS]
git checkout "software/teensy/Soccer 2026/platformio.ini"   # revertir
```

**4b — modo 8X8:** cambiar `-DDIAG_TOF_MODE_4X4` por `-DDIAG_TOF_MODE_8X8`:
```bash
pio run -e diag_top_tof
# Expected: [SUCCESS]
git checkout "software/teensy/Soccer 2026/platformio.ini"   # revertir
```

**4c — fallback SKIP_XSHUT:** agregar `-DDIAG_TOF_SKIP_XSHUT` al `build_flags` (en una línea nueva, manteniendo el resto):
```bash
pio run -e diag_top_tof
# Expected: [SUCCESS] — verifica que el #ifndef del setup compila las dos ramas.
git checkout "software/teensy/Soccer 2026/platformio.ini"   # revertir
```

**Después de los 3 cambios**, verificar que `platformio.ini` quedó EXACTAMENTE como lo dejó Task 2 (default `-DDIAG_TOF_MODE_4X4`, sin SKIP_XSHUT):
```bash
git diff "software/teensy/Soccer 2026/platformio.ini"
# Expected: sin diff (archivo limpio).
```

- [ ] **Step 5: Compilar default una vez más para garantizar estado limpio**

```bash
pio run -e diag_top_tof
```

Expected: `[SUCCESS]` con `-DDIAG_TOF_MODE_4X4`.

- [ ] **Step 6: Compilar que el env `[env:top]` NO se rompió**

```bash
pio run -e top
```

Expected: `[SUCCESS]` — el firmware vivo TOP NO debería haberse afectado (no cambiamos ninguno de sus archivos).

- [ ] **Step 7: Commit**

```bash
git add "software/teensy/Soccer 2026/src/diag/diag_top_tof.cpp"
git commit -m "$(cat <<'EOF'
feat(diag): implementar loop() + formatters del diag_top_tof

loop() polla check_data_ready, get_ranging_data y dispatchea al
formatter segun MODE compilado:
- MODE_SINGLE: mean de zonas validas + valid_count
- MODE_4X4:    grilla 4x4 ASCII de distancias en mm
- MODE_8X8:    grilla 8x8 ASCII

print_cell maneja status 5/6/9 como validos, resto como "XXXX".
Error pattern LED (3 blinks/s) si init fallo.

Validado: compila clean con los 3 modos + DIAG_TOF_SKIP_XSHUT.
[env:top] sigue compilando OK (no se rompio nada del firmware vivo).

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)
EOF
)"
```

---

## Task 5: Crear team-task de validación en hardware real

**Files:**
- Create: `team-tasks/2026-05-24-task-NNN-validar-vl53l7cx-frontal-u2.md` (NNN = próximo número libre)

- [ ] **Step 1: Encontrar próximo número de TASK libre**

```bash
ls team-tasks/ | grep -E "^2026-.*-task-[0-9]+" | sed -E 's/.*task-([0-9]+).*/\1/' | sort -n | tail -3
```

Tomar el número más alto + 1. Ej: si el último es 030, usar 031.

- [ ] **Step 2: Leer un team-task reciente para copiar formato**

```bash
ls -t team-tasks/2026-*.md | head -1
```

Leerlo para imitar frontmatter y estructura.

- [ ] **Step 3: Crear el team-task**

Crear `team-tasks/2026-05-24-task-NNN-validar-vl53l7cx-frontal-u2.md` (reemplazar NNN):

```markdown
---
task-id: TASK-NNN
fecha-creada: 2026-05-24
asignado: <Virginia | Elias | Enzo | a-asignar>
prioridad: P2
estado: pending
estima: 30 min
bloquea: integracion-vl53l7cx-firmware-top
relacionado:
  - docs/superpowers/specs/2026-05-24-diag-top-tof-vl53l7cx-design.md
  - docs/superpowers/plans/2026-05-24-diag-top-tof-vl53l7cx.md
---

# TASK-NNN — Validar VL53L7CX frontal U2 en hardware real (placa TOP)

## Que hay que hacer

Flashear el sketch de diagnostico `diag_top_tof` en la Teensy 4.0 de la
placa TOP, abrir Serial Monitor, y verificar los 7 criterios de aceptacion
del spec.

## Prerequisitos

- Placa TOP montada, Teensy 4.0 alimentada, LED de power ON.
- VL53L7CX soldado en posicion U2 del schematic.
- Excepcion Avast aplicada (TASK-025).

## Pasos

```bash
cd "software/teensy/Soccer 2026"
pio run -e diag_top_tof -t upload
```

Abrir Serial Monitor (PlatformIO o `pio device monitor -b 115200`).

## Criterio de cierre (los 7 del spec §4.3)

- [ ] (1) Banner `=== diag_top_tof ===` visible.
- [ ] (2) Linea `loading FW... OK` (NO `init FAILED`).
- [ ] (3) Grilla nueva cada ~67 ms durante 30 s sin freeze.
- [ ] (4) Apuntar a pared a 1 m: zona central lee 950-1050 mm.
- [ ] (5) Mano pegada al sensor: zona central baja a 30-50 mm.
- [ ] (6) Cielo abierto (>4 m): valores ~4000 mm o status XXXX (OOR).
- [ ] (7) Mover obstaculo solo al cuadrante sup-izq a 50 cm: solo esas
      4 zonas (modo 4x4) bajan, las otras 12 quedan en lectura de fondo.

## Si algun criterio falla

1. Si init falla: probar con `-DDIAG_TOF_SKIP_XSHUT` agregado al env.
2. Si I2C scan no muestra `0x29`: problema fisico (alimentacion, soldadura,
   pull-ups). Avisar a Enzo.
3. Si la grilla aparece pero todas las zonas leen 0 o XXXX: probar
   `Wire.setClock(100000)` (slow mode) en el sketch.
4. Documentar todos los hallazgos en `journal/2026-05-2X-validacion-tof-frontal.md`.

## Que NO hacer

- NO modificar `src/top/sensors_tof.cpp` con codigo de la lib hasta que
  este sketch pase los 7 criterios.
- NO marcar este TASK como done sin pasar los 7 criterios.

## Referencias

- Spec: `docs/superpowers/specs/2026-05-24-diag-top-tof-vl53l7cx-design.md`
- Plan: `docs/superpowers/plans/2026-05-24-diag-top-tof-vl53l7cx.md`
- Pack TOP: `hardware/electronics/top-board-pack/`
```

- [ ] **Step 4: Commit**

```bash
git add "team-tasks/2026-05-24-task-NNN-validar-vl53l7cx-frontal-u2.md"
git commit -m "$(cat <<'EOF'
task(top): TASK-NNN validar VL53L7CX frontal U2 en hardware real

Tarea humana de banco — flashear diag_top_tof, abrir Serial Monitor,
verificar los 7 criterios de aceptacion del spec. P2 (los TOFs son
Nivel 3+, no bloquean Incheon, pero validan que el sensor que compraron
funciona).

Criterio de cierre: los 7 checkboxes pasan + journal entry con verdict.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)
EOF
)"
```

(Reemplazar `NNN` con el número real del Step 1 en el subject y body.)

---

## Resumen final (post-Task 5)

Al terminar el plan deberías tener:

- ✅ Lib `STM32duino_VL53L7CX` vendoreada en `lib/` (~300 KB).
- ✅ Sketch `src/diag/diag_top_tof.cpp` (~250 LOC) con 3 modos + fallback XSHUT.
- ✅ Nuevo env `[env:diag_top_tof]` en `platformio.ini` (no rompe los otros).
- ✅ `[env:top]` sigue compilando OK (el firmware vivo no se tocó).
- ✅ Team task creada para que el equipo valide en hardware.
- ✅ 5 commits con mensajes descriptivos y atribución correcta.

Lo que **NO** está hecho (es trabajo del equipo humano, no de Claude):
- ❌ Flashear el Teensy y observar Serial Monitor.
- ❌ Cerrar la team-task como done.
- ❌ Migrar el código a `src/top/sensors_tof.cpp` (esa es otra spec, post-validación).
