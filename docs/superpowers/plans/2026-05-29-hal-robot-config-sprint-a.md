# HAL por robot — Sprint A Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Crear estructura HAL del firmware TOP — `hardware_profile.h` + `pinout_common.h` + `pinout_robot1.h` + `pinout_robot2.h` con selector via `-DROBOT1`/`-DROBOT2`. Migrar `config_top.h` a thin wrapper. Backwards-compatible: ningún `.cpp` del firmware vivo cambia.

**Architecture:** Estructura de 4 archivos nuevos con cierre incremental. Primero se crean los 4 archivos en estado "no incluidos por nadie" (no rompen el build). Después en una migración atómica `config_top.h` pasa a wrapper + se agregan los envs `top_robot1` / `top_robot2` / `diag_localization_live_robot1` / `_robot2` + el `test_native` se actualiza para que compile con `-DROBOT1`.

**Tech Stack:** PlatformIO, Teensy 4.0 (TOP), Adafruit_VL53L7CX + BNO055 (vendoreadas), Unity host-native.

**Spec aprobado:** [docs/superpowers/specs/2026-05-29-hal-robot-config-design.md](../specs/2026-05-29-hal-robot-config-design.md)

**Atribución (TODOS los commits):**
```
Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)
```

**Working directory para todos los comandos:** `C:\Users\violl\iitasoccer\open-soccer-robocup-team2026\software\teensy\Soccer 2026`

**PowerShell setup antes de comandos pio:**
```powershell
$env:Path = "$env:USERPROFILE\.platformio\penv\Scripts;$env:Path"
```

---

## Task 1: Crear `src/top/pinout_common.h`

**Files:**
- Create: `software/teensy/Soccer 2026/src/top/pinout_common.h`

- [ ] **Step 1: Leer `config_top.h` actual para entender qué constantes existen**

```powershell
cd "C:\Users\violl\iitasoccer\open-soccer-robocup-team2026"
# Leer config_top.h completo con su Read tool, identificar constantes que son
# idénticas en ambos robots (cancha, I2C, UART, timing, sentinels)
```

- [ ] **Step 2: Crear `pinout_common.h` con las constantes comunes**

```cpp
// pinout_common.h — Constantes idénticas en ambos robots (R1 arquero, R2
// delantero). Incluido por hardware_profile.h antes del pinout específico
// del robot. NO incluir directamente desde código del firmware — usar
// hardware_profile.h o el wrapper legacy config_top.h.
//
// Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ============================================================
// I2C buses (TOP placa rev 1.0 — hardware fijo, ambos robots)
// ============================================================
constexpr int WIRE1_SCL_PIN = 24;
constexpr int WIRE1_SDA_PIN = 25;

constexpr uint8_t BNO055_LEFT_I2C_ADDR  = 0x28;
constexpr uint8_t BNO055_RIGHT_I2C_ADDR = 0x28;
constexpr uint8_t VL53L7CX_DEFAULT_I2C_ADDR = 0x29;

// ============================================================
// UARTs — pines fijos del Teensy 4.0
// ============================================================
constexpr long UART_FROM_DOWN_BAUD = 230400;   // Serial1
constexpr long UART_TO_ZIRCON_BAUD = 230400;   // Serial2 — TENTATIVO
constexpr long UART_CAMERA1_BAUD   = 19200;    // Serial3
constexpr long UART_TO_COMM_BAUD   = 115200;   // Serial4
constexpr long UART_CAMERA2_BAUD   = 19200;    // Serial5

// ============================================================
// HC-SR04 ultrasonido frontal (idéntico ambos robots)
// ============================================================
constexpr int PIN_HCSR04_TRIG = 6;
constexpr int PIN_HCSR04_ECHO = 7;

// ============================================================
// LED de estado (LED_BUILTIN del Teensy)
// ============================================================
constexpr int PIN_LED_STATUS = 13;

// ============================================================
// Cancha RCJ Soccer Open 2026 — convención de ejes canónica
// (X long axis derecha, Y short axis al arco rival)
// ============================================================
constexpr uint16_t FIELD_WIDTH_MM  = 2430;   // eje X (largo)
constexpr uint16_t FIELD_HEIGHT_MM = 1820;   // eje Y (corto)

// Ángulos de montaje físico de los 4 TOFs (mismo en ambos robots)
constexpr uint16_t TOF_MOUNT_ANGLE_DEG[4] = { 0, 180, 90, 270 };

// Umbral default para descarte de outliers en localización
constexpr uint16_t LOCALIZATION_OUTLIER_THRESHOLD_MM = 300;

// ============================================================
// Loop timing del TOP
// ============================================================
constexpr uint32_t IMU_TICK_INTERVAL_MS       = 10;
constexpr uint32_t TOF_TICK_INTERVAL_MS       = 30;
constexpr uint32_t STRATEGY_TICK_INTERVAL_MS  = 10;
constexpr uint32_t MOTORS_SEND_INTERVAL_MS    = 10;

// Watchdogs
constexpr uint32_t ZIRCON_HEARTBEAT_TIMEOUT_MS = 500;
constexpr uint32_t DOWN_HEARTBEAT_TIMEOUT_MS   = 500;

// ============================================================
// Cantidad de slots TOF físicos en la placa (hardware fijo)
// ============================================================
// NUM_TOF es el tamaño del array de slots TOF físicos. Cada slot puede
// estar populated o no según ROBOT_HAS_TOF_*. Para iterar solo los
// activos, usar NUM_TOF_ACTIVE del pinout_robotN.h.
constexpr int NUM_TOF = 4;

}  // namespace iitasoccer
```

- [ ] **Step 3: Verificar que el archivo es sintácticamente válido (no incluido por nadie todavía, no debería romper nada)**

```powershell
cd "software/teensy/Soccer 2026"
$env:Path = "$env:USERPROFILE\.platformio\penv\Scripts;$env:Path"
pio run -e top
```

Expected: SUCCESS (el build no usa el nuevo archivo todavía).

- [ ] **Step 4: Commit**

```powershell
git add "software/teensy/Soccer 2026/src/top/pinout_common.h"
git commit -m "feat(top): pinout_common.h — constantes idénticas R1+R2 (HAL Sprint A T1)

Primer archivo del HAL refactor. Mueve constantes que NO dependen del
robot desde config_top.h (legacy) a un archivo dedicado:
- Cancha (FIELD_WIDTH_MM, FIELD_HEIGHT_MM)
- I2C buses (Wire1 remap pins, BNO055 + VL53 addresses)
- UART baudrates
- HC-SR04 pins
- LED status pin
- TOF mount angles (idénticos en ambos robots)
- Loop timing intervals
- NUM_TOF (slots físicos del PCB)

NO está incluido por nadie todavía — el build vivo sigue usando las
constantes originales de config_top.h. La migración a wrapper viene en
T5 del plan.

Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md
Plan: docs/superpowers/plans/2026-05-29-hal-robot-config-sprint-a.md

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 2: Crear `src/top/pinout_robot1.h`

**Files:**
- Create: `software/teensy/Soccer 2026/src/top/pinout_robot1.h`

- [ ] **Step 1: Crear el archivo**

```cpp
// pinout_robot1.h — Pinout específico del ROBOT 1 (arquero).
//
// Hardware: placa TOP rev 1.0 + bodge XSHUT manual de Enzo.
// Sensores instalados al 2026-05-29 (post-bodge): TOF frontal + trasero.
// Laterales pendientes de soldar.
//
// REGLA: si Enzo cambia el cableado físico, actualizar SOLO este archivo
// y reflashear con `pio run -e top_robot1 -t upload`. No tocar
// pinout_common.h ni los .cpp del firmware.
//
// Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ============================================================
// XSHUT (LPn) de los 4 TOFs — bodge físico de Enzo
// ============================================================
// PLACEHOLDER hasta confirmar con Enzo después del bodge. Estos pines
// están libres en rev 1.0 según `01-pinout-y-hardware.md` (NC en el
// schematic, sin conflicto con otros usos). Si Enzo soldó otros pines,
// CAMBIAR ACÁ y reflashear.
//
// Mapeo a slots físicos del PCB:
//   [0] = TOF frontal  (slot U2 del schematic)
//   [1] = TOF trasero  (slot U3 del schematic)
//   [2] = TOF izquierdo (slot U5 del schematic)
//   [3] = TOF derecho  (slot U17 del schematic)
constexpr int PIN_TOF_XSHUT[4] = {
    9,   // PLACEHOLDER — TOF[0] frontal U2
    11,  // PLACEHOLDER — TOF[1] trasero U3
    12,  // PLACEHOLDER — TOF[2] izquierdo U5
    22,  // PLACEHOLDER — TOF[3] derecho U17
};

// ============================================================
// Direcciones I²C asignadas tras enumeración XSHUT
// ============================================================
// Default del L7CX es 0x29. Cada TOF se levanta uno por uno con XSHUT y
// se le asigna una dirección distinta. El módulo sensors_tof.cpp (Sprint B
// futuro) hace esa enumeración usando este array.
constexpr uint8_t TOF_I2C_ADDR_ASSIGNED[4] = {
    0x29,  // TOF[0] frontal — mantiene default
    0x2A,  // TOF[1] trasero — reasignado
    0x2B,  // TOF[2] izquierdo — reasignado
    0x2C,  // TOF[3] derecho — reasignado
};

// ============================================================
// Feature flags — qué sensores están físicamente instalados HOY en R1
// ============================================================
// Actualizar cuando Enzo termine de soldar cada slot. Cambiar de 0 a 1
// el flag correspondiente cuando el sensor esté soldado + verificado.
#define ROBOT_HAS_TOF_FRONT 1
#define ROBOT_HAS_TOF_BACK  1
#define ROBOT_HAS_TOF_LEFT  0
#define ROBOT_HAS_TOF_RIGHT 0
#define ROBOT_HAS_OTOS      0   // R1 (arquero) sin OTOS

// Cantidad de TOFs activos (calcular a mano según los HAS_* arriba).
// Si cambian los flags, recalcular este valor.
constexpr int NUM_TOF_ACTIVE = 2;

// ============================================================
// Dipswitch de rol (idéntico ambos robots por hardware)
// ============================================================
constexpr int PIN_ROLE_DIPSWITCH = 10;

}  // namespace iitasoccer
```

- [ ] **Step 2: Compile gate**

```powershell
cd "software/teensy/Soccer 2026"
pio run -e top
```
Expected: SUCCESS (archivo nuevo, sin uso todavía).

- [ ] **Step 3: Commit**

```powershell
git add "software/teensy/Soccer 2026/src/top/pinout_robot1.h"
git commit -m "feat(top): pinout_robot1.h — config específica del arquero (HAL Sprint A T2)

Pinout específico de ROBOT 1 (arquero). Contiene:
- PIN_TOF_XSHUT[4]: pines del Teensy para el bodge XSHUT de Enzo
  (PLACEHOLDER hasta confirmar — actualizar acá y reflashear)
- TOF_I2C_ADDR_ASSIGNED[4]: direcciones tras enumeración (0x29, 0x2A, 0x2B, 0x2C)
- ROBOT_HAS_TOF_FRONT/BACK/LEFT/RIGHT: flags (1/0/0/0 al 2026-05-29)
- ROBOT_HAS_OTOS: 0 (arquero sin OTOS)
- NUM_TOF_ACTIVE: 2 (frontal + trasero, post-bodge)
- PIN_ROLE_DIPSWITCH: 10

NO está incluido por nadie todavía — el dispatch viene en T4 + T5.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 3: Crear `src/top/pinout_robot2.h`

**Files:**
- Create: `software/teensy/Soccer 2026/src/top/pinout_robot2.h`

- [ ] **Step 1: Crear el archivo** (similar a R1 con diferencias documentadas)

```cpp
// pinout_robot2.h — Pinout específico del ROBOT 2 (delantero).
//
// Hardware: placa TOP rev 1.0 + bodge XSHUT manual de Enzo. Los pines
// del bodge pueden ser DISTINTOS a R1 — cada robot tiene su construcción
// manual. Confirmar con Enzo qué pines usó.
//
// Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ============================================================
// XSHUT (LPn) de los 4 TOFs — bodge físico de Enzo
// ============================================================
// PLACEHOLDER (mismos que R1 hasta confirmación). Si Enzo soldó otros
// pines en R2, CAMBIAR ACÁ y reflashear `pio run -e top_robot2 -t upload`.
constexpr int PIN_TOF_XSHUT[4] = {
    9,   // PLACEHOLDER — TOF[0] frontal U2
    11,  // PLACEHOLDER — TOF[1] trasero U3
    12,  // PLACEHOLDER — TOF[2] izquierdo U5
    22,  // PLACEHOLDER — TOF[3] derecho U17
};

// ============================================================
// Direcciones I²C asignadas tras enumeración XSHUT
// ============================================================
constexpr uint8_t TOF_I2C_ADDR_ASSIGNED[4] = {
    0x29, 0x2A, 0x2B, 0x2C,
};

// ============================================================
// Feature flags — qué sensores están físicamente instalados HOY en R2
// ============================================================
#define ROBOT_HAS_TOF_FRONT 1
#define ROBOT_HAS_TOF_BACK  1
#define ROBOT_HAS_TOF_LEFT  0
#define ROBOT_HAS_TOF_RIGHT 0
#define ROBOT_HAS_OTOS      1   // R2 (delantero) con OTOS

constexpr int NUM_TOF_ACTIVE = 2;

constexpr int PIN_ROLE_DIPSWITCH = 10;

}  // namespace iitasoccer
```

- [ ] **Step 2: Compile gate**

```powershell
pio run -e top
```
Expected: SUCCESS.

- [ ] **Step 3: Commit**

```powershell
git add "software/teensy/Soccer 2026/src/top/pinout_robot2.h"
git commit -m "feat(top): pinout_robot2.h — config específica del delantero (HAL Sprint A T3)

Pinout específico de ROBOT 2 (delantero). Mismo formato que R1 pero:
- ROBOT_HAS_OTOS = 1 (delantero con OTOS, R1 sin)
- PIN_TOF_XSHUT son PLACEHOLDER por ahora (pueden diferir de R1 cuando
  Enzo confirme)

NO está incluido por nadie todavía — el dispatch viene en T4.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 4: Crear `src/top/hardware_profile.h` (selector central)

**Files:**
- Create: `software/teensy/Soccer 2026/src/top/hardware_profile.h`

- [ ] **Step 1: Crear el archivo**

```cpp
// hardware_profile.h — Selector central de configuración de robot.
//
// Cualquier .cpp del firmware TOP debería incluir este archivo (NO
// config_top.h directamente — ese queda como wrapper legacy).
//
// Dispatcha al pinout específico según -DROBOT1 o -DROBOT2 que el env
// de PIO debe definir.
//
// Uso desde código:
//   #include "hardware_profile.h"
//   // ahora todo está disponible: FIELD_WIDTH_MM, PIN_TOF_XSHUT[], etc.
//
// Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md

#pragma once

#include "pinout_common.h"

#if defined(ROBOT1)
    #include "pinout_robot1.h"
#elif defined(ROBOT2)
    #include "pinout_robot2.h"
#else
    #error "Compilación requiere -DROBOT1 o -DROBOT2 en build_flags. Ver pinout_robot1.h / pinout_robot2.h para opciones."
#endif
```

- [ ] **Step 2: Compile gate**

```powershell
pio run -e top
```
Expected: SUCCESS todavía (hardware_profile.h aún no incluido por código vivo).

- [ ] **Step 3: Commit**

```powershell
git add "software/teensy/Soccer 2026/src/top/hardware_profile.h"
git commit -m "feat(top): hardware_profile.h — selector central HAL (Sprint A T4)

Selector central del HAL. Incluye pinout_common.h primero, después
dispatcha a pinout_robot1.h o pinout_robot2.h según -DROBOT1 o -DROBOT2.
Si ninguno está definido, #error con mensaje claro.

NO está incluido por nadie todavía — la migración del wrapper viene en T5.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 5: Migración atómica — `config_top.h` a wrapper + envs nuevos en `platformio.ini`

**Files:**
- Modify: `software/teensy/Soccer 2026/src/top/config_top.h` (transformación a wrapper)
- Modify: `software/teensy/Soccer 2026/platformio.ini` (agregar envs)

Esta es la task crítica. Tiene que ser atómica porque:
- Si solo migrás config_top.h, el env `[env:top]` actual (sin -DROBOT) falla con #error.
- Si solo agregás envs sin migrar config_top.h, los nuevos envs heredan constantes duplicadas.

Hacer las dos cosas en un solo commit.

- [ ] **Step 1: Backup mental — leer config_top.h actual completo**

Usar Read tool sobre `software/teensy/Soccer 2026/src/top/config_top.h` para tener el contenido entero antes de modificarlo. Identificar:
- El banner sobre XSHUT no ruteado (debe migrar a pinout_robot1.h / robot2.h o quedar como comentario en el wrapper).
- Las constantes Sprint 1 (`FIELD_WIDTH_MM`, etc. — ya están en common).
- Otras constantes existentes.

- [ ] **Step 2: Sobrescribir `config_top.h` con el wrapper minimal**

```cpp
// config_top.h — Wrapper legacy del HAL refactor (2026-05-29).
//
// Este archivo era el config monolítico original. Tras el HAL Sprint A,
// las constantes se movieron a:
//   - pinout_common.h: lo idéntico en ambos robots
//   - pinout_robot1.h / pinout_robot2.h: lo específico por robot
//   - hardware_profile.h: selector central que dispatcha
//
// Este wrapper se mantiene para que `#include "config_top.h"` en código
// existente (Sprint 1 localización, sensors_tof, main_top, etc.) siga
// funcionando sin cambios.
//
// **Código nuevo debería usar `#include "hardware_profile.h"` directamente.**
//
// Si querés agregar una constante nueva:
//   - ¿Idéntica en ambos robots? → pinout_common.h
//   - ¿Específica de un robot? → pinout_robot1.h o pinout_robot2.h
//   - ¿Nunca acá.

#pragma once
#include "hardware_profile.h"
```

- [ ] **Step 3: Agregar envs nuevos a `platformio.ini`**

Agregar al final del archivo (después del último env existente):

```ini
; ============================================================
; HAL refactor 2026-05-29 (Sprint A): envs por robot
; ============================================================
; El env [env:top] original ahora FALLA a compilar porque ni ROBOT1 ni
; ROBOT2 está definido en build_flags. Usar [env:top_robot1] o
; [env:top_robot2] explícitamente. Documentado por #error claro en
; hardware_profile.h.
;
; Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md

[env:top_robot1]
extends = env:top
build_flags = ${env:top.build_flags} -DROBOT1

[env:top_robot2]
extends = env:top
build_flags = ${env:top.build_flags} -DROBOT2

[env:diag_localization_live_robot1]
extends = env:diag_localization_live
build_flags = ${env:diag_localization_live.build_flags} -DROBOT1

[env:diag_localization_live_robot2]
extends = env:diag_localization_live
build_flags = ${env:diag_localization_live.build_flags} -DROBOT2
```

- [ ] **Step 4: Compile gates de los nuevos envs**

```powershell
cd "software/teensy/Soccer 2026"
$env:Path = "$env:USERPROFILE\.platformio\penv\Scripts;$env:Path"

pio run -e top_robot1
pio run -e top_robot2
pio run -e diag_localization_live_robot1
pio run -e diag_localization_live_robot2
```

Expected: TODOS SUCCESS. Si alguno falla por constante duplicada o not declared, ajustar (probablemente faltó mover una constante a common).

- [ ] **Step 5: Verificar que `[env:top]` antiguo FALLA con #error claro**

```powershell
pio run -e top 2>&1 | Select-String "ROBOT1.*ROBOT2"
```

Expected: el output muestra el `#error` con el mensaje sobre ROBOT1/ROBOT2.

- [ ] **Step 6: Compile gate de envs no migrados (que NO dependen de ROBOT1/2)**

```powershell
pio run -e diag_top_tof_adafruit
pio run -e diag_sensors_tof_live
pio run -e diag_down
pio run -e central_robot1
pio run -e central_robot2
```

Expected: TODOS SUCCESS (estos envs no usan config_top.h o no dependen de ROBOT flag).

- [ ] **Step 7: Commit ATÓMICO**

```powershell
git add "software/teensy/Soccer 2026/src/top/config_top.h" "software/teensy/Soccer 2026/platformio.ini"
git commit -m "feat(top): MIGRACIÓN ATÓMICA HAL — config_top.h a wrapper + envs por robot (Sprint A T5)

Cambios atómicos para activar el HAL:

1. config_top.h pasa de ~150 LOC monolítico a thin wrapper de ~20 LOC
   que solo incluye hardware_profile.h. Los #include de código existente
   siguen funcionando sin cambios — el wrapper preserva backwards compat.

2. platformio.ini gana 4 envs nuevos:
   - [env:top_robot1]: top + -DROBOT1 (arquero)
   - [env:top_robot2]: top + -DROBOT2 (delantero)
   - [env:diag_localization_live_robot1]: diag_localization_live + -DROBOT1
   - [env:diag_localization_live_robot2]: diag_localization_live + -DROBOT2

3. El env [env:top] viejo (sin -DROBOT) ahora FALLA a compilar con
   #error claro de hardware_profile.h. Comportamiento DESEADO — fuerza
   a elegir un robot. Documentado en el comentario del env.

Regresión verificada:
- top_robot1 / top_robot2 / diag_localization_live_robot1/2 SUCCESS
- diag_top_tof_adafruit / diag_sensors_tof_live / diag_down SUCCESS (no usan ROBOT flag)
- central_robot1 / central_robot2 SUCCESS (no afectados por refactor TOP)

Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 6: Ajustar `[env:test_native]` para que compile con `-DROBOT1`

**Files:**
- Modify: `software/teensy/Soccer 2026/platformio.ini` (agregar -DROBOT1 al test_native)

- [ ] **Step 1: Verificar primero si los tests host-native usan hardware_profile.h**

```powershell
cd "C:/Users/violl/iitasoccer/open-soccer-robocup-team2026"
grep -rE "hardware_profile|config_top|pinout_" "software/teensy/Soccer 2026/test/" 2>&1
```

**Caso A — los tests NO incluyen hardware_profile.h** (probable, los tests son puros y trabajan con LocalizationConfig pasado por argumento):

Los tests deberían seguir compilando con `pio test -e test_native -f test_localization` sin modificar nada. Verificar:

```powershell
pio test -e test_native -f test_localization
```

Si pasa, **NO modificar test_native** y saltar al Step 4 (commit vacío de NO-OP).

**Caso B — los tests SÍ incluyen hardware_profile.h** (improbable):

Modificar `[env:test_native]` agregando `-DROBOT1` a build_flags.

- [ ] **Step 2: Si Caso A, verificar que los 14 tests siguen pasando**

```powershell
pio test -e test_native -f test_localization
```

Expected: 14 tests PASSED.

- [ ] **Step 3: Si Caso B, después de modificar test_native, recompilar**

```powershell
pio test -e test_native -f test_localization
```

Expected: 14 tests PASSED.

- [ ] **Step 4: Commit**

**Si Caso A (no se modifica nada):** skip el commit (no hay cambios).

**Si Caso B:**

```powershell
git add "software/teensy/Soccer 2026/platformio.ini"
git commit -m "feat(test): test_native compila con -DROBOT1 default (HAL Sprint A T6)

Los tests host-native ahora dependen indirectamente de hardware_profile.h.
Agrega -DROBOT1 al build_flags del env test_native para que compilen.

Si se necesita testear contra config R2, usar pio test con build_flags
override o agregar [env:test_native_robot2] (no necesario hoy).

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 7: Regression suite final + TASK-036 + ESTADO-ACTUAL.md

**Files:**
- Create: `team-tasks/2026-05-29-task-036-confirmar-pines-xshut-bodge-enzo.md`
- Modify: `docs/ESTADO-ACTUAL.md`

- [ ] **Step 1: Regression suite completa**

```powershell
cd "software/teensy/Soccer 2026"
$env:Path = "$env:USERPROFILE\.platformio\penv\Scripts;$env:Path"

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

Expected: TODOS SUCCESS / PASS.

Si algo falla → BLOCKED, no avanzar con commits hasta arreglar.

- [ ] **Step 2: Crear TASK-036 para Enzo (confirmar pines XSHUT)**

Encontrar próximo número libre:
```bash
ls team-tasks/ 2>&1 | grep -E "^2026.*task-[0-9]+" | sed -E 's/.*task-0*([0-9]+).*/\1/' | sort -n | tail -3
```

Usar el próximo libre (probablemente 036).

Crear `team-tasks/2026-05-29-task-036-confirmar-pines-xshut-bodge-enzo.md`:

```markdown
---
id: TASK-036
title: "Confirmar pines del Teensy del bodge XSHUT (R1 y R2)"
date_created: 2026-05-29
date_due: 2026-05-31
assigned: [Enzo]
priority: P1
status: pending
estimated_hours: 0.5
blocks: [sprint-b-extender-sensors-tof, task-035-validacion-hardware]
blocked_by: []
tags: [hardware, top-board, bodge, xshut, hal]
---

# TASK-036 — Confirmar pines del Teensy del bodge XSHUT

## Por qué

El HAL Sprint A dejó en `src/top/pinout_robot1.h` y `pinout_robot2.h`
los pines del Teensy para XSHUT como **PLACEHOLDER** (9, 11, 12, 22).
El bodge físico que hiciste puede usar otros pines (vos dijiste que
ibas a usar los que originalmente iban a INT).

Sin confirmar los pines reales, el firmware no puede inicializar los 4
TOFs correctamente.

## Qué necesito

Para cada robot (R1 arquero y R2 delantero) decime los 4 números:

- TOF[0] frontal (slot U2 del schematic): pin ___
- TOF[1] trasero (slot U3): pin ___
- TOF[2] izquierdo (slot U5): pin ___
- TOF[3] derecho (slot U17): pin ___

Si los 2 robots tienen los MISMOS pines, decime una sola lista. Si son
distintos, una lista por robot.

## Cómo actualizo el firmware después

Editar `src/top/pinout_robot1.h` (y `pinout_robot2.h` si difiere) cambiando
el array `PIN_TOF_XSHUT[4] = {...}` con los pines reales. Después:

```
pio run -e top_robot1 -t upload  # o -e top_robot2 -t upload
```

Compilation falla si alguno de los pines elegidos colisiona con un uso
existente del Teensy (ej. si Enzo eligió pin 18 que es SDA0). En ese
caso, escribirle a Enzo + log en journal.

## Criterio de cierre

- TASK-036 closed con los 4 (u 8 si difieren) números de pines.
- pinout_robot1.h y/o pinout_robot2.h actualizados con los valores reales.
- Journal entry `journal/2026-05-XX-bodge-xshut-pines-confirmados.md`
  con la decisión.
```

- [ ] **Step 3: Actualizar `docs/ESTADO-ACTUAL.md`**

Leer el archivo, agregar:

En la sección "Módulos vivos" del TOP:
```
- `src/top/hardware_profile.h` + `pinout_common.h` + `pinout_robot1.h`
  + `pinout_robot2.h` — HAL refactor Sprint A (2026-05-29). El código
  vivo del firmware usa `hardware_profile.h` (a través del wrapper
  legacy `config_top.h`). Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md
```

En "Deudas conocidas":
```
- HAL Sprint B (extender sensors_tof.cpp para enumerar NUM_TOF_ACTIVE
  TOFs con XSHUT secuencial al boot). Bloqueado por TASK-036
  (confirmar pines del bodge).
- HAL para CENTRAL (replicar el patrón). Sprint futuro.
```

En "Resuelto 2026-05-29":
```
- HAL Sprint A (TOP): 4 archivos nuevos (hardware_profile.h, pinout_common.h,
  pinout_robot1.h, pinout_robot2.h) + config_top.h migrado a wrapper +
  envs por robot en platformio.ini. Backwards compat preservada (Sprint 1
  localización intacto). 14/14 tests host-native siguen pasando. Pendiente:
  TASK-036 (pines XSHUT reales) para arrancar Sprint B.
```

- [ ] **Step 4: Commit ambos**

```powershell
git add "team-tasks/2026-05-29-task-036-confirmar-pines-xshut-bodge-enzo.md" "docs/ESTADO-ACTUAL.md"
git commit -m "task(top): TASK-036 confirmar pines XSHUT + actualizar ESTADO (HAL Sprint A T7)

TASK-036: P1 para Enzo. Bloquea Sprint B (extender sensors_tof a N TOFs)
porque sin los pines reales del bodge, los placeholder del HAL no sirven
para inicializar los 4 TOFs.

ESTADO-ACTUAL.md: HAL Sprint A capturado como resuelto + deudas (Sprint B
y CENTRAL HAL) agregadas. Tests host-native siguen verdes.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Resumen final (post-Task 7)

Al terminar el plan deberías tener:

- ✅ 4 archivos nuevos del HAL: `pinout_common.h`, `pinout_robot1.h`, `pinout_robot2.h`, `hardware_profile.h`
- ✅ `config_top.h` migrado a thin wrapper (~10 LOC vs ~150 originales)
- ✅ 4 envs nuevos: `top_robot1`, `top_robot2`, `diag_localization_live_robot1`, `_robot2`
- ✅ `[env:top]` original falla a compilar con `#error` claro (deseado)
- ✅ 5 envs no migrados siguen SUCCESS (regresión OK)
- ✅ 14 tests host-native siguen PASSED
- ✅ TASK-036 abierta para Enzo
- ✅ `ESTADO-ACTUAL.md` actualizado
- ✅ ~7 commits con atribución correcta

Lo que NO está hecho:
- ❌ Pines XSHUT reales (bloqueado por TASK-036)
- ❌ Sprint B (extender sensors_tof a N TOFs)
- ❌ TASK-035 (validación HW del Sprint 1)
- ❌ HAL para CENTRAL (sprint futuro)
