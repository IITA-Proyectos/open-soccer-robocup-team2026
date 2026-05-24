---
title: "Diseño — diag_top_tof: sketch standalone para verificar VL53L7CX frontal de la placa TOP"
date: 2026-05-24
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: aprobado
tags: [firmware, top-board, tof, vl53l7cx, diag, vendoring]
robot: ambos
area: sensores
tipo: decision
related:
  - software/teensy/Soccer 2026/src/top/sensors_tof.cpp
  - hardware/electronics/top-board-pack/01-pinout-y-hardware.md
  - hardware/electronics/top-board-pack/02-funcionalidad.md
  - software/teensy/Soccer 2026/platformio.ini
---

# diag_top_tof — diseño aprobado

> Aprobado por Gustavo (2026-05-24). Sketch de **diagnóstico de hardware**
> standalone para validar que el sensor VL53L7CX frontal de la placa TOP
> responde y mide. **No modifica el firmware de competencia** — sigue el
> patrón de `[env:diag_down]` que ya existe en el repo.

## 0. Contexto

El firmware vivo `src/top/sensors_tof.cpp` es un **stub**: el bloque
`TODO_TOF_LIB_BEGIN/END` marca dónde irá el código real, pero hoy todas las
lecturas ToF retornan `TOF_NO_READING` (solo el HC-SR04 funciona).

Hardware actualmente instalado en la placa TOP (confirmado por Gustavo
2026-05-24): **un (1) único sensor VL53L7CX** soldado en la posición **U2**
del schematic. Los otros 3 ToF (U3, U5, U17) no están físicamente montados.

Necesidad: validar que el sensor que el equipo compró funciona, antes de
Incheon, sin tocar el firmware de competencia (que tiene otros conflictos
abiertos — pin 7 HC-SR04 vs Serial2 RX, Wire1 remap pendiente).

## 1. Decisiones de arquitectura

Tres decisiones aprobadas durante el brainstorming (2026-05-24):

| Decisión | Elegido | Por qué |
|---|---|---|
| Posición física del TOF | **U2** del schematic → bus **Wire (I²C0)**, SDA=18, SCL=19, **XSHUT pin 2** | Confirmado por el equipo. Bus "limpio" (no depende del remap Wire1 pendiente de TASK-003). |
| Dónde vive el código | **Sketch standalone** en nuevo env `[env:diag_top_tof]` | Aislado. No rompe firmware TOP. Patrón ya usado por `[env:diag_down]`. |
| Qué imprime | **Modos seleccionables por `#define`**: SINGLE / 4×4 / 8×8 | Cubre los 3 casos (validar que mide, validar multizona, ver resolución máxima) con un solo sketch. Default = 4×4. |

## 2. Componentes

### 2.A — Lib vendoreada `lib/STM32duino_VL53L7CX/`

**Ubicación:** `software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/`

**Upstream:** [github.com/stm32duino/VL53L7CX](https://github.com/stm32duino/VL53L7CX)

Wrapper Arduino oficial de STMicroelectronics sobre el driver ULD
("Ultra Lite Driver") C nativo del VL53L7CX. Es la opción más usada en
proyectos Teensy + PIO porque:

- Usa `Wire` genérico (no es STM32-specific pese al nombre).
- Soporta resoluciones 4×4 y 8×8, frecuencias 1-60 Hz (4×4) o 1-15 Hz (8×8).
- Carga el firmware blob (~85 KB) del sensor al init automáticamente.
- License: BSD-3-Clause (compatible con el resto del repo).

**Política:** vendorear (no `lib_deps`), igual que el resto de libs del
proyecto. Esto está justificado en `lib/README.md`:
> "Hay copia de respaldo (...) Las libs vendoreadas se detectan solas vía
> LDF; NO se bajan del registry. Esto desbloquea a quien tenga Avast/SSL
> roto (TASK-025)."

Y consistente con `Adafruit_BNO055`, `Adafruit_BusIO`, `Adafruit_Unified_Sensor`,
`Unity` — todas ya vendoreadas.

**Procedimiento de vendoreo:**
1. `git clone https://github.com/stm32duino/VL53L7CX.git` en tmp.
2. Anotar `git rev-parse HEAD` (commit) y tag más reciente (versión).
3. Borrar: `.git/`, `examples/`, `.github/`, `extras/`, `keywords.txt` si no sirve a Teensy.
4. Mantener: `src/`, `library.json`, `library.properties`, `LICENSE`.
5. Copiar a `software/teensy/Soccer 2026/lib/STM32duino_VL53L7CX/`.
6. Escribir `README.md` con: origen URL, commit hash, tag/version, fecha de
   vendoreo, motivo (este spec).

**Tamaño esperado:** ~250-350 KB (incluye firmware blob ~85 KB).

### 2.B — Sketch standalone `src/diag/diag_top_tof.cpp`

**Ubicación:** `software/teensy/Soccer 2026/src/diag/diag_top_tof.cpp`

Carpeta `src/diag/` ya existe (creada por `[env:diag_down]`). Convención
respetada.

**Tamaño:** ~150 LOC.

**Modos seleccionables** vía macros pasadas en `build_flags`:

| Define | Resolución | Frame rate | Output |
|---|---|---|---|
| `DIAG_TOF_MODE_SINGLE` | 4×4 internamente, reporta valor único | 15 Hz | Una línea: `[ms] mean=XXX mm  valid=NN/16  status=OK` |
| `DIAG_TOF_MODE_4X4` *(default)* | 4×4 | 15 Hz | Grilla ASCII 4×4 cada ~67 ms |
| `DIAG_TOF_MODE_8X8` | 8×8 | 15 Hz | Grilla ASCII 8×8 cada ~67 ms |

Solo un `MODE_*` puede estar definido a la vez (verificado con `#error` si
hay conflicto).

**Flujo del sketch:**

```cpp
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { /* espera USB */ }
  print_banner();           // build flags, pinout, modo activo

  // XSHUT del U2 → ON (a menos que DIAG_TOF_SKIP_XSHUT esté definido).
  #ifndef DIAG_TOF_SKIP_XSHUT
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);
    delay(10);
  #endif

  Wire.begin();             // pines default 18/19, sin remap.
  Wire.setClock(1000000);   // I²C fast-mode plus (lib soporta hasta 1 MHz).

  Serial.print("loading FW... ");
  if (!sensor.begin()) { print_init_failed(); halt(); }
  if (sensor.init() != 0) { print_init_failed(); halt(); }
  Serial.println("OK");

  sensor.set_resolution(MODE_IS_8X8 ? 64 : 16);
  sensor.set_ranging_frequency_hz(15);
  sensor.set_ranging_mode(VL53L7CX_RANGING_MODE_CONTINUOUS);
  sensor.start_ranging();
}

void loop() {
  uint8_t ready = 0;
  sensor.check_data_ready(&ready);
  if (!ready) return;

  VL53L7CX_ResultsData r;
  sensor.get_ranging_data(&r);
  print_grid(r);          // según modo
}
```

**Banner ejemplo:**
```
=== diag_top_tof — VL53L7CX frontal U2 ===
Board:   Teensy 4.0 (TOP)
Bus:     Wire (I2C0)  SDA=18  SCL=19
XSHUT:   pin 2 (HIGH)
Mode:    4x4 @ 15 Hz
Build:   2026-05-24 14:32:01
============================================
```

**Output 4×4 ejemplo (cada 67 ms):**
```
[ 3210 ms]  valid=16/16
  320  315  311  308
  410  405  402  399
  510  503  498  493
  600  592  587  580
```

(Valores en mm. `XXXX` para zonas sin lectura válida.)

**No incluye:** ni `sensors_tof.h`, ni `cameras_*`, ni `comm_*`, ni
`config_top.h`. Sketch 100% autocontenido — define localmente `PIN_XSHUT_TOF_FRONT = 2`.

### 2.C — Nuevo env en `platformio.ini`

```ini
; ============================================================
; diag_top_tof — DIAGNÓSTICO de hardware del VL53L7CX frontal (placa TOP)
; NO es firmware de competencia. Cargá esto, abrí Serial Monitor a 115200,
; y verificás que el TOF mide.
;   pio run -e diag_top_tof -t upload
;
; Modos (cambiar SOLO uno):
;   -DDIAG_TOF_MODE_SINGLE   ; 1 número promedio
;   -DDIAG_TOF_MODE_4X4      ; (default) grilla 4x4 ASCII
;   -DDIAG_TOF_MODE_8X8      ; grilla 8x8 ASCII
;
; Fallback si XSHUT pin 2 no está ruteado:
;   -DDIAG_TOF_SKIP_XSHUT    ; asume sensor siempre alimentado
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
; lib_deps: VL53L7CX vendoreada en lib/ (igual que BNO055/BusIO/Unified Sensor).
; LDF la detecta sola; NO se baja del registry. Trabajamos 100% offline.
```

## 3. Boundary — qué NO se toca

- ❌ `src/top/sensors_tof.{h,cpp}` queda intacto (stub vivo).
- ❌ `src/top/config_top.h` no cambia.
- ❌ `[env:top]` no cambia (sigue compilando lo mismo y produciendo mismo binario).
- ❌ No se modifica ningún archivo de los packs (`hardware/electronics/*-pack/`).
- ❌ No se marca ninguna TASK de hardware como `done` (CLAUDE.md regla 1: eso lo hace el equipo humano).
- ❌ No se actualiza `ESTADO-ACTUAL.md` ni `FUENTES-DE-VERDAD.md` hasta que el sensor esté validado en hardware real. Este spec es un acelerador, no un cambio de fuente de verdad.

## 4. Plan de prueba en hardware real

> Esta sección la **ejecuta el equipo humano** (Virginia, Elías, Enzo, o
> Gustavo) con la placa TOP en la mano. Claude solo verifica que el código
> compila.

### 4.1 Setup
- Placa TOP montada con Teensy 4.0 funcional (LED de power ON).
- VL53L7CX soldado en posición **U2** (verificado visualmente o con multímetro:
  3.3 V en VDD del L7CX cuando XSHUT pin 2 está HIGH).
- USB del Teensy conectado a la PC con PIO instalado.
- Excepción Avast aplicada (TASK-025) si la máquina no compiló DOWN antes.

### 4.2 Pasos
1. `pio run -e diag_top_tof` desde `software/teensy/Soccer 2026/`.
   - **Criterio:** compila clean, sin warnings rojos.
2. `pio run -e diag_top_tof -t upload`.
   - **Criterio:** "SUCCESS" + LED del Teensy parpadea.
3. Abrir Serial Monitor a **115200 baud**, **Newline: LF**.
4. Esperar ~5 s (boot + carga firmware ULD).

### 4.3 Criterios de aceptación medibles

| # | Criterio | Cómo se verifica |
|---|---|---|
| 1 | Banner aparece | Línea con `=== diag_top_tof === ` visible |
| 2 | Init sin error | Línea `loading FW... OK` (NO `init FAILED`) |
| 3 | Lecturas continuas | Grilla nueva cada ~67 ms (15 Hz) durante 30 s |
| 4 | Rango realista a 1 m | Apuntar a pared a 1 m: zona central lee 950-1050 mm |
| 5 | Rango cercano | Mano pegada al sensor: zona central baja a 30-50 mm |
| 6 | Rango lejos | Cielo abierto (>4 m): valores ~4000 mm o status "OOR" |
| 7 | Multizona funciona | Mover obstáculo solo al cuadrante superior-izq a 50 cm: solo esas 4 zonas (modo 4×4) bajan, las otras 12 quedan en lectura de fondo |

Si **cualquier criterio falla**, el sensor NO está validado. Crear TASK
humana en `team-tasks/` con observaciones y dejar el spec en estado
`failed-validation` (banner arriba).

### 4.4 Regresión sobre subsistemas vecinos

**Ninguna**, por construcción:
- Wire (I²C0) — en este env no hay otros periféricos en el bus (BNO055 izq no se inicializa).
- Pin 2 (XSHUT) — pin único, no compartido.
- Serial USB — no compartido.
- Todos los demás pines del Teensy 4.0 — sin tocar.

Para volver al firmware TOP normal: `pio run -e top -t upload`.

## 5. Riesgos identificados

| # | Riesgo | Prob | Mitigación |
|---|---|---|---|
| R1 | Pin 2 XSHUT del U2 marcado "tentativo" en pack — puede no estar ruteado | Media | Si init falla, segundo intento con `build_flags += -DDIAG_TOF_SKIP_XSHUT` (asume sensor siempre alimentado). Si tampoco anda: problema físico (soldadura, capacitor, pull-up I²C). |
| R2 | VL53L7CX requiere cargar firmware blob ~85 KB en boot (~3-5 s) | Cierta | Sketch printea `loading FW...` y luego `OK` o `FAILED reason=N`. Documentado en el banner. |
| R3 | Variante hardware L7CH (sin DD bonding) tiene zonas reducidas | Baja | La lib soporta ambas. Si 8×8 sale rara, probar 4×4. |
| R4 | Bus Wire podría conflictar con BNO055 izq (U10) en firmware real (cuando se integre) | N/A en sketch | No aplica acá. Verificación se hace en futuro spec de integración a `sensors_tof.cpp`. |
| R5 | I²C fast-mode plus (1 MHz) puede fallar si las pistas del PCB son largas | Baja | Default es 1 MHz por performance. Si falla criterio 1-2, bajar a `Wire.setClock(400000)` (fast-mode standard). |

## 6. Próximos pasos post-validación (fuera del scope de este spec)

Cuando los 7 criterios pasen en hardware real:
1. Documentar verdict en `journal/2026-05-2X-hardware-up-top-tof-frontal.md`.
2. Bajar TASK-022 (calibración cámaras) y consideraciones similares a P2 — no acá.
3. **Nuevo spec** para migrar el sketch a `sensors_tof.cpp` integrado al
   firmware TOP. Esa migración incluye: coexistencia con BNO055 en bus Wire,
   read no-bloqueante en `sensors_tof_tick()`, integración al WorldSnapshot
   (campo nuevo `obstacles_min_mm[4]` si se decide exponer).
4. Cuando lleguen los otros 3 sensores (U3, U5, U17): extender enumeración con
   XSHUT secuencial y direcciones 0x52/54/56/58. **Pero solo si los TOFs entran
   en scope antes de Incheon** — hoy son Nivel 3+ aspiracional.

## 7. Atribución y referencias

- **Lib upstream:** [github.com/stm32duino/VL53L7CX](https://github.com/stm32duino/VL53L7CX) — STMicroelectronics, license BSD-3-Clause.
- **Stub vivo:** `src/top/sensors_tof.{h,cpp}` (Claude + equipo, sesiones previas).
- **Pinout TOP:** `hardware/electronics/top-board-pack/01-pinout-y-hardware.md`.
- **Funcionalidad TOP:** `hardware/electronics/top-board-pack/02-funcionalidad.md`.
- **Patrón env diag:** `[env:diag_down]` en `software/teensy/Soccer 2026/platformio.ini`.
- **Política vendoreo:** `software/teensy/Soccer 2026/lib/README.md` + CLAUDE.md regla 1.
- **CLAUDE.md regla 1:** Claude programa firmware host-testeable pero NO marca TASKs de hardware como `done`.

## 8. Commits y atribución

Este spec + implementación se commitean con:
```
Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)
```

(Convención del repo según `AI-INSTRUCTIONS.md`.)
