---
title: "Diseño — HAL por robot (config_robot1/robot2 + hardware_profile selector) para placa TOP"
date: 2026-05-29
author: "Claude Opus 4.7 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: aprobado
tags: [firmware, top-board, hal, refactor, multi-robot, config, sprint-a]
robot: ambos
area: arquitectura
tipo: decision
related:
  - software/teensy/Soccer 2026/src/top/config_top.h
  - software/teensy/Soccer 2026/src/central/config_central.h
  - docs/superpowers/specs/2026-05-25-localization-sprint1-trilateration-design.md
  - journal/2026-05-25-top-xshut-no-routed-finding.md
---

# HAL por robot — diseño aprobado

> Aprobado por Gustavo (2026-05-29). Primer sprint del Hardware Abstraction
> Layer (HAL) que permite compilar el mismo firmware para 2 robots (R1
> arquero, R2 delantero) con pinout, sensores opcionales y features
> distintos según el robot.

## 0. Contexto y motivación

El firmware del TOP actualmente tiene todas las constantes de hardware en
`src/top/config_top.h` como un solo archivo sin distinción de robot. Esto
genera 3 problemas:

1. **Bodge XSHUT distinto por robot**: Enzo va a cablear los XSHUT de los 4
   TOFs en R1 y R2 con pines del Teensy posiblemente distintos (cada robot
   tiene su propia construcción manual).
2. **Sensores opcionales**: el equipo puede tener un robot sin OTOS, o uno
   con solo 2 TOFs cardinales, o uno con sensores extra futuros. Sin HAL,
   cada combinación requiere modificar el firmware general.
3. **Inconsistencia con CENTRAL**: `src/central/config_central.h` ya usa
   `#if defined(ROBOT1) / #elif defined(ROBOT2)` pero todo en un mismo
   archivo. La estructura no escala bien y duplica lo común.

Esta spec define la estructura HAL para el TOP. CENTRAL queda como deuda
documentada para sprint futuro.

## 1. Decisiones de arquitectura

| Decisión | Elegido | Justificación |
|---|---|---|
| Granularidad archivos | **3 archivos**: `pinout_common.h` + `pinout_robot1.h` + `pinout_robot2.h` | Separa lo común de lo específico. Cada robot tiene su archivo limpio. |
| Selector | **Build flag `-DROBOT1` o `-DROBOT2` en envs de PIO**, dispatch en `hardware_profile.h` | Consistente con patrón existente en CENTRAL. Sin archivos editables al cambio de target. |
| Backwards compat | **`config_top.h` queda como thin wrapper** que solo incluye `hardware_profile.h` | El código del Sprint 1 (que hace `#include "config_top.h"`) no se rompe. |
| Scope | **Solo TOP** en este sprint. CENTRAL queda como sprint futuro. | Aislamiento de blast radius. Menor riesgo de bug por refactor amplio. |
| Sensores opcionales | **Feature flags `ROBOT_HAS_TOF_FRONT`/`BACK`/`LEFT`/`RIGHT`/`HAS_OTOS`** | Permite que el firmware compile y corra con cualquier combinación. |
| Diferenciación XSHUT | **2 bloques separados** (uno por robot) | El bodge puede ser distinto físicamente entre R1 y R2. |

## 2. Componentes

### 2.A — `src/top/hardware_profile.h` (selector central)

```cpp
#pragma once
// hardware_profile.h — Selector central de configuración de robot.
//
// Incluye primero lo común (cancha, protocolos, sentinels) y después
// el pinout específico del robot activo según -DROBOT1 o -DROBOT2 que
// el env de PIO debe definir.

#include "pinout_common.h"

#if defined(ROBOT1)
    #include "pinout_robot1.h"
#elif defined(ROBOT2)
    #include "pinout_robot2.h"
#else
    #error "Compilación requiere -DROBOT1 o -DROBOT2 en build_flags. Ver pinout_robot1.h / pinout_robot2.h para opciones."
#endif
```

**Tamaño esperado:** ~15 LOC.

### 2.B — `src/top/pinout_common.h`

Constantes idénticas en ambos robots. Mínimo esperado (puede crecer):

- Dimensiones cancha: `FIELD_WIDTH_MM`, `FIELD_HEIGHT_MM`
- Constantes I²C: `BNO055_LEFT_I2C_ADDR`, `BNO055_RIGHT_I2C_ADDR`, `VL53L7CX_DEFAULT_I2C_ADDR`
- LED status: `PIN_LED_STATUS` (es pin 13 en ambos robots por hardware)
- UART baudrates: `UART_FROM_DOWN_BAUD`, `UART_TO_CENTRAL_BAUD`, etc.
- Timing: `IMU_TICK_INTERVAL_MS`, `TOF_TICK_INTERVAL_MS`, etc.
- Sentinels: `TOF_NO_READING`
- Outlier threshold de localización: `LOCALIZATION_OUTLIER_THRESHOLD_MM`
- Wire1 remap: `WIRE1_SCL_PIN`, `WIRE1_SDA_PIN` (hardware-fijo en PCB rev 1.0)
- Mount angles default de los 4 TOFs: `TOF_MOUNT_ANGLE_DEG[4] = {0, 180, 90, 270}` — la disposición física es la misma en ambos robots.

**Tamaño esperado:** ~80 LOC.

### 2.C — `src/top/pinout_robot1.h` (arquero)

Constantes específicas. Mínimo esperado para Sprint A (solo TOFs por ahora):

```cpp
#pragma once
// pinout_robot1.h — Pinout específico del ROBOT 1 (arquero).
// Hardware: placa TOP rev 1.0 + bodge XSHUT manual de Enzo.

namespace iitasoccer {

// ============================================================
// XSHUT (LPn) de los 4 TOFs — bodge físico de Enzo
// ============================================================
// PLACEHOLDER hasta confirmar con Enzo después del bodge físico.
// Candidatos plausibles según pinout-y-hardware.md: pines 9, 11, 12, 22
// (libres en rev 1.0, sin conflicto con otros usos).
//
// REGLA: si Enzo cambia el cableado, actualizar solo este archivo
// y reflashear con `pio run -e top_robot1 -t upload`. No tocar
// pinout_common.h ni los .cpp del firmware.
constexpr int PIN_TOF_XSHUT[4] = {
    9,   // PLACEHOLDER — TOF[0] frontal U2
    11,  // PLACEHOLDER — TOF[1] trasero U3
    12,  // PLACEHOLDER — TOF[2] izquierdo U5
    22,  // PLACEHOLDER — TOF[3] derecho U17
};

// ============================================================
// Direcciones I²C asignadas a cada TOF después de la enumeración XSHUT
// ============================================================
// Default del L7CX es 0x29 (7-bit). Cada TOF se levanta uno por uno con
// su XSHUT y se le asigna dirección distinta.
constexpr uint8_t TOF_I2C_ADDR_ASSIGNED[4] = {
    0x29,  // TOF[0] frontal — mantiene default
    0x2A,  // TOF[1] trasero — reasignado
    0x2B,  // TOF[2] izquierdo — reasignado
    0x2C,  // TOF[3] derecho — reasignado
};

// ============================================================
// Feature flags — qué sensores están físicamente instalados
// ============================================================
// Actualizar cuando Enzo termine de soldar cada slot. Default (post-bodge
// 2026-05-29): solo frontal + trasero soldados. Laterales por agregar.
#define ROBOT_HAS_TOF_FRONT 1
#define ROBOT_HAS_TOF_BACK  1
#define ROBOT_HAS_TOF_LEFT  0
#define ROBOT_HAS_TOF_RIGHT 0
#define ROBOT_HAS_OTOS      0   // ROBOT1 (arquero) sin OTOS (deuda Sprint futuro)

// Cantidad de TOFs activos (calculada a mano según los HAS_* arriba)
constexpr int NUM_TOF_ACTIVE = 2;

// Dipswitch de rol (existía en config_top.h legacy)
constexpr int PIN_ROLE_DIPSWITCH = 10;

}  // namespace iitasoccer
```

**Tamaño esperado:** ~60 LOC.

### 2.D — `src/top/pinout_robot2.h` (delantero)

Mismo formato que R1 pero con pines XSHUT potencialmente distintos y
mismos feature flags por ahora:

```cpp
#pragma once
// pinout_robot2.h — Pinout específico del ROBOT 2 (delantero).

namespace iitasoccer {

constexpr int PIN_TOF_XSHUT[4] = {
    9,   // PLACEHOLDER — Enzo confirmar
    11,  // PLACEHOLDER
    12,  // PLACEHOLDER
    22,  // PLACEHOLDER
};

constexpr uint8_t TOF_I2C_ADDR_ASSIGNED[4] = {
    0x29, 0x2A, 0x2B, 0x2C,
};

#define ROBOT_HAS_TOF_FRONT 1
#define ROBOT_HAS_TOF_BACK  1
#define ROBOT_HAS_TOF_LEFT  0
#define ROBOT_HAS_TOF_RIGHT 0
#define ROBOT_HAS_OTOS      1   // ROBOT2 (delantero) con OTOS

constexpr int NUM_TOF_ACTIVE = 2;

constexpr int PIN_ROLE_DIPSWITCH = 10;

}  // namespace iitasoccer
```

**Tamaño esperado:** ~60 LOC.

### 2.E — `src/top/config_top.h` (legacy, migrado a wrapper)

```cpp
#pragma once
// config_top.h — DEPRECATED. Mantenido como wrapper para no romper
// `#include "config_top.h"` en código existente del Sprint 1.
//
// Nueva ubicación canónica del config: hardware_profile.h, que dispatcha
// a pinout_common.h + pinout_robot1.h o pinout_robot2.h según -DROBOT1/2.
//
// Código nuevo debería usar `#include "hardware_profile.h"` directamente.

#include "hardware_profile.h"
```

**Cambio respecto al actual:** todas las constantes que estaban acá migran
a `pinout_common.h` o `pinout_robotN.h`. El archivo queda solo con el
`#include`.

**Tamaño esperado:** ~10 LOC (era ~150 LOC).

## 3. Estructura de envs en `platformio.ini`

Agregar 4 envs nuevos (mantener los actuales para compatibilidad backwards
en lo que se pueda):

```ini
; ============================================================
; TOP — variantes por robot (HAL refactor 2026-05-29)
; ============================================================
[env:top_robot1]
extends = env:top
build_flags = ${env:top.build_flags} -DROBOT1

[env:top_robot2]
extends = env:top
build_flags = ${env:top.build_flags} -DROBOT2

; diag_localization_live variantes por robot
[env:diag_localization_live_robot1]
extends = env:diag_localization_live
build_flags = ${env:diag_localization_live.build_flags} -DROBOT1

[env:diag_localization_live_robot2]
extends = env:diag_localization_live
build_flags = ${env:diag_localization_live.build_flags} -DROBOT2
```

El env `[env:top]` original **falla a compilar** después del refactor (porque
ni ROBOT1 ni ROBOT2 está definido). Es comportamiento deseado: fuerza
al usuario a elegir un robot. Documentado en commit.

Para tests host-native: agregar `-DROBOT1` al `[env:test_native]` por
default (los tests del Sprint 1 asumieron 4 TOFs cardinales — usar R1 como
base, agregar `[env:test_native_robot2]` opcional si hace falta).

## 4. Backwards compatibility

| Archivo del Sprint 1 | Cambio necesario | Comentario |
|---|---|---|
| `src/top/main_top.cpp` | Ninguno (usa `config_top.h`) | El wrapper lo redirige a `hardware_profile.h` |
| `src/top/sensors_tof.cpp` | Ninguno por ahora | El cambio para enumerar N TOFs es Sprint B |
| `src/top/sensors_imu.cpp` | Ninguno | Usa direcciones I²C que ahora están en `pinout_common.h` |
| `src/top/localization_runtime.cpp` | Ninguno | Usa `FIELD_WIDTH_MM`/`HEIGHT_MM` que están en common |
| `test/test_localization/test_main.cpp` | Ninguno | Tests son puros, no dependen de config |
| `src/diag/diag_localization_live.cpp` | Ninguno | Usa `config_top.h` wrapper |

**El refactor es no-invasivo.** Lo único que rompe es el comando
`pio run -e top` sin flag de robot — pero ese caso debería fallar
con un mensaje claro de `#error`.

## 5. Roadmap fuera del scope de este sprint

Documentado para no perder, pero NO se implementa acá:

1. **Sprint B**: extender `sensors_tof.cpp` para enumerar `NUM_TOF_ACTIVE`
   TOFs con XSHUT secuencial al boot. Requiere los pines reales de Enzo.
2. **Sprint C**: TASK-035 con setup parcial (2 TOFs) en hardware real.
3. **Sprint futuro**: replicar el patrón en `src/central/`
   (`config_central.h` también pasa a `hardware_profile_central.h` +
   `pinout_central_common.h` + `pinout_central_robotN.h`).
4. **Sprint futuro**: replicar en `src/down/` si DOWN se vuelve robot-specific.

## 6. Plan de testing

### 6.1 Compile gates

Después del refactor, los siguientes envs deben SUCCESS:

```powershell
pio run -e top_robot1
pio run -e top_robot2
pio run -e diag_top_tof_adafruit
pio run -e diag_sensors_tof_live
pio run -e diag_localization_live_robot1
pio run -e diag_localization_live_robot2
pio run -e diag_down
pio run -e central_robot1
pio run -e central_robot2
pio test -e test_native -f test_localization
```

### 6.2 Verificación de compatibilidad

- Diff `pio run -e top_robot1` vs el binario pre-refactor (compilando con
  el mismo flag virtual): debe ser **idéntico** en tamaño y símbolos.
  Verificación posible con `git stash` + comparar tamaños.
- El `#error` salta correctamente si se intenta `pio run -e top` sin flag.

### 6.3 Tests host-native

Los 14 tests existentes deben seguir pasando (los tests son puros, no
deberían depender del flag de robot si todo está bien estructurado). Si
algún test asume `NUM_TOF=4` activo y R1 tiene `NUM_TOF_ACTIVE=2`,
adaptar el test para que use mock config independiente.

## 7. Boundary — qué NO se modifica

- ❌ `src/top/main_top.cpp` — no cambia (su única dependencia es `config_top.h`).
- ❌ `src/top/sensors_tof.cpp` — no cambia (Sprint B).
- ❌ `src/top/sensors_imu.cpp` — no cambia.
- ❌ `src/top/localization_runtime.cpp` — no cambia.
- ❌ `src/top/cameras_*.cpp` — no cambia.
- ❌ `src/central/*` — no cambia (Sprint futuro).
- ❌ `src/down/*` — no cambia.
- ❌ Tests host-native `.cpp` — no cambian (algoritmo puro independiente).
- ❌ `WorldSnapshot` y el protocolo CENTRAL ↔ TOP — sin cambios.

## 8. Riesgos identificados

| # | Riesgo | Prob | Mitigación |
|---|---|---|---|
| R1 | Constantes que olvido mover a común se duplican entre R1 y R2 → bug por desincronización | Alta | Self-review estricto del Plan + diff de las 2 últimas migraciones |
| R2 | El env `[env:top]` actual deja de compilar → rompe scripts | Cierta | Documentar en commit. No hay scripts CI que lo usen. |
| R3 | Pines XSHUT placeholder son los equivocados | Alta | Marcadores claros TODO + el comentario en pinout_robotN.h dice cómo cambiarlos |
| R4 | Macros `ROBOT_HAS_*` colisionan con otros símbolos del repo | Baja | Prefijo `ROBOT_HAS_` específico, grep antes |
| R5 | El refactor introduce regresión sutil en localization | Media | Tests host-native 14/14 deben seguir verdes después del refactor |
| R6 | Pines XSHUT para R1 vs R2 difieren pero los TOFs físicos están en mismas posiciones del PCB | Cierta | Ese es justo el caso que el HAL maneja. Cada robot tiene sus pines en su archivo. |

## 9. Tiempo estimado

- **Spec + plan + brainstorming**: 0.5 día (hoy).
- **Implementación con subagents TDD**: 0.5 día.
- **Compile gates + regression**: 0.5 hora.
- **Total Claude**: ~1 día.

Trabajo del equipo humano: ninguno hasta que Enzo confirme pines del bodge
real. Después del bodge: 5 minutos para actualizar `pinout_robotN.h` y
reflashear.

## 10. Próximos pasos post-Sprint A

Sprint B (próxima sesión): extender `sensors_tof.cpp` para enumerar
`NUM_TOF_ACTIVE` TOFs con XSHUT secuencial al boot. Requiere:
- Los pines reales del bodge de Enzo.
- Hardware con al menos 2 TOFs soldados (mañana 2026-05-30).
- Spec aparte porque introduce lógica nueva.

Sprint C: TASK-035 con setup parcial validado.

## 11. Atribución y referencias

- **Patrón existente** que inspiró el diseño: `src/central/config_central.h`
  (Gustavo + equipo, 2026-03).
- **Hallazgo XSHUT no ruteado**: `journal/2026-05-25-top-xshut-no-routed-finding.md`.
- **Sprint 1 base**: `docs/superpowers/specs/2026-05-25-localization-sprint1-trilateration-design.md`.
- **Política de testing**: `lib/Unity/` + tests host-native.

## 12. Commits + atribución

Cada commit del Sprint A lleva:
```
Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)
```
