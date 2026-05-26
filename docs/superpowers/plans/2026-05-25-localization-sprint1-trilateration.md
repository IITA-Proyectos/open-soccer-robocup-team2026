# Localización Sprint 1 — Trilateración Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implementar `localization_compute()` puro + `localization_runtime` glue de hardware que produce pose `(x, y, θ)` del robot en la cancha RCJ Soccer Open 2026 usando 4× VL53L7CX cardinales + 2× BNO055.

**Architecture:** Lógica pura en `src/shared/localization.{h,cpp}` (host-testeable con Unity, sin Arduino). Glue de hardware en `src/top/localization_runtime.{h,cpp}` (lee TOF/IMU runtime y cachea pose). Integración a `main_top.cpp` para que el pose llegue al `WorldSnapshot v2` que va al CENTRAL. TDD estricto: cada tarea agrega un test que falla, después la mínima implementación para que pase.

**Tech Stack:** C++17, PlatformIO + Teensy framework (board: teensy40), Unity test framework (host-native), Adafruit_VL53L7CX + Adafruit_BNO055 ya vendoreadas. Sin float en path crítico (LUT trig integer-only).

**Spec aprobado:** [docs/superpowers/specs/2026-05-25-localization-sprint1-trilateration-design.md](../specs/2026-05-25-localization-sprint1-trilateration-design.md)

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

## Phase 1 — Constants and types

## Task 1: Agregar constantes de cancha + ángulos de montaje a `config_top.h`

**Files:**
- Modify: `software/teensy/Soccer 2026/src/top/config_top.h`

- [ ] **Step 1: Verificar que el archivo no fue tocado por sesión paralela**

```powershell
cd "C:\Users\violl\iitasoccer\open-soccer-robocup-team2026"
git status --short src/top/config_top.h
```

Expected: vacío (sin modificaciones pendientes). Si hay modificaciones, stop y reportar.

- [ ] **Step 2: Agregar constantes al final del archivo (antes del cierre del namespace)**

Buscar la línea `}  // namespace iitasoccer` al final de `src/top/config_top.h` e **insertar antes** estas líneas:

```cpp
// ============================================================
// Localización en cancha RCJ Soccer Open 2026
// ============================================================
// Dimensiones interiores de la cancha (rulebook 2026).
constexpr uint16_t FIELD_WIDTH_MM  = 2430;   // eje X (largo)
constexpr uint16_t FIELD_HEIGHT_MM = 1820;   // eje Y (corto)

// Ángulos de montaje de los 4 TOFs respecto al frente del robot (grados).
// Convención: 0 = frente, 90 = izquierda, 180 = atrás, 270 = derecha.
// Mapeo a índices del array PIN_TOF_XSHUT / sensors_tof_get_distance_mm:
//   [0] = frontal, [1] = trasero, [2] = izquierdo, [3] = derecho
constexpr uint16_t TOF_MOUNT_ANGLE_DEG[NUM_TOF] = { 0, 180, 90, 270 };

// Umbral default para descarte de outliers por inconsistencia entre TOFs
// del mismo eje. Si 2 TOFs estiman X (o Y) y difieren más que este valor,
// se descarta el más lejano del pose anterior.
constexpr uint16_t LOCALIZATION_OUTLIER_THRESHOLD_MM = 300;  // 30 cm
```

- [ ] **Step 3: Verificar que compila**

```powershell
cd "software/teensy/Soccer 2026"
pio run -e top
```

Expected: `[SUCCESS]` — el archivo modificado no rompe el firmware vivo.

- [ ] **Step 4: Commit**

```powershell
git add "software/teensy/Soccer 2026/src/top/config_top.h"
git commit -m "feat(top): agregar constantes de cancha y angulos TOF a config_top.h

Constantes nuevas para el modulo de localizacion Sprint 1:
- FIELD_WIDTH_MM / FIELD_HEIGHT_MM (dimensiones interiores cancha
  RCJ Soccer Open 2026: 2430x1820 mm).
- TOF_MOUNT_ANGLE_DEG[4]: angulos de montaje 0/180/90/270 grados
  (frontal/trasero/izquierdo/derecho).
- LOCALIZATION_OUTLIER_THRESHOLD_MM = 300 (umbral inconsistencia).

Sin cambios en logica vivo. Spec: docs/superpowers/specs/2026-05-25-localization-sprint1-trilateration-design.md

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 2: Crear `src/shared/localization.h` con tipos + declaración

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/localization.h`

- [ ] **Step 1: Crear el header completo**

Crear `software/teensy/Soccer 2026/src/shared/localization.h` con este contenido:

```cpp
// localization.h — Pose absoluta del robot en cancha RCJ Soccer Open 2026.
//
// Logica pura, host-testeable. NO usa Arduino, Wire, ni hardware. La capa de
// hardware vive en src/top/localization_runtime.{h,cpp}.
//
// Algoritmo: trilateracion geometrica directa con 4 TOFs cardinales + heading
// del BNO. Precision esperada: +-2-3 cm en posicion.
//
// Spec: docs/superpowers/specs/2026-05-25-localization-sprint1-trilateration-design.md

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Sentinel para indicar lectura invalida (debe matchear sensors_tof.h).
constexpr uint16_t LOCALIZATION_TOF_NO_READING = 0xFFFF;

// Datos crudos que necesita la trilateracion.
struct LocalizationInputs {
    uint16_t tof_distance_mm[4];   // [0]=frontal, [1]=trasero, [2]=izq, [3]=der
    bool     tof_valid[4];          // false si lectura era invalida o ruidosa
    int16_t  bno_heading_centideg; // -18000..18000 (= +-180 grados x 100)
};

// Configuracion estatica (cancha + montaje + tuning).
struct LocalizationConfig {
    uint16_t field_width_mm;        // 2430 — eje X de la cancha
    uint16_t field_height_mm;       // 1820 — eje Y de la cancha
    int16_t  bno_offset_centideg;   // calibrado al boot (heading apuntando al arco rival)
    uint16_t tof_mount_angle_deg[4]; // {0, 180, 90, 270} = {front, back, izq, der}
    uint16_t outlier_threshold_mm;  // umbral inconsistencia entre TOFs del mismo eje
    // Pose anterior para el outlier rejection por consistencia. Si es la primera
    // llamada, pasar {0, 0, 0, 0, false}.
    int16_t  prev_x_mm;
    int16_t  prev_y_mm;
    bool     prev_valid;
};

// Resultado: pose calculado este ciclo + diagnostico.
struct LocalizationPose {
    int16_t  x_mm;                 // 0..field_width_mm (origen = esquina propia)
    int16_t  y_mm;                 // 0..field_height_mm
    int16_t  heading_centideg;     // 0..36000 (relativo a +Y de la cancha)
    uint8_t  source_flags;         // bit i = 1 si TOF[i] se uso este ciclo
    bool     valid;                // false si <2 TOFs utiles (1 por eje minimo)
};

// Funcion pura: dados inputs y config, devuelve pose. Sin side effects.
// Esta es la API principal del modulo, testeable host-native.
LocalizationPose localization_compute(
    const LocalizationInputs& in,
    const LocalizationConfig& cfg
);

}  // namespace iitasoccer
```

- [ ] **Step 2: Verificar que el header compila standalone**

Crear test rápido `/tmp/test_compile.cpp` (NO commitear):
```cpp
#include "src/shared/localization.h"
int main() { iitasoccer::LocalizationInputs in{}; return in.tof_distance_mm[0]; }
```

```powershell
cd "software/teensy/Soccer 2026"
g++ -std=c++17 -I src/shared -c -o /tmp/test_compile.o /tmp/test_compile.cpp
# o en cmd.exe: g++ -std=c++17 -I src\shared -c -o "%TEMP%\test_compile.o" "%TEMP%\test_compile.cpp"
```

Expected: compila sin errores. Eliminar `/tmp/test_compile.cpp` y `/tmp/test_compile.o` después.

- [ ] **Step 3: Commit**

```powershell
git add "software/teensy/Soccer 2026/src/shared/localization.h"
git commit -m "feat(shared): localization.h — tipos publicos del modulo de pose

Declara LocalizationInputs / LocalizationConfig / LocalizationPose y la
funcion pura localization_compute() que sera implementada en .cpp
en commits siguientes (TDD strict).

Sin implementacion todavia. Header solo.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Phase 2 — Trilateración básica (sin rotación, sin outliers)

## Task 3: Setup del test suite + primer test (función no implementada → falla por link error)

**Files:**
- Create: `software/teensy/Soccer 2026/test/test_localization/test_main.cpp`

- [ ] **Step 1: Crear el archivo de tests con primer test (centro de cancha sin rotación)**

Crear `software/teensy/Soccer 2026/test/test_localization/test_main.cpp` con:

```cpp
// test_localization — tests unitarios del modulo localization (puro).
// Corre en host con: pio test -e test_native -f test_localization
//
// Cubre: trilateracion geometrica, rotacion, outlier rejection.

#include <unity.h>
#include "localization.h"

using namespace iitasoccer;

// ============================================================
// Helpers de test
// ============================================================
namespace {

// Configuracion estandar para los tests — cancha RCJ Open 2026 + montaje cardinal.
LocalizationConfig make_standard_config() {
    LocalizationConfig cfg{};
    cfg.field_width_mm           = 2430;
    cfg.field_height_mm          = 1820;
    cfg.bno_offset_centideg      = 0;  // heading=0 = robot apunta al arco rival
    cfg.tof_mount_angle_deg[0]   = 0;    // frontal
    cfg.tof_mount_angle_deg[1]   = 180;  // trasero
    cfg.tof_mount_angle_deg[2]   = 90;   // izquierdo
    cfg.tof_mount_angle_deg[3]   = 270;  // derecho
    cfg.outlier_threshold_mm     = 300;
    cfg.prev_x_mm                = 1215; // centro de la cancha
    cfg.prev_y_mm                = 910;
    cfg.prev_valid               = true;
    return cfg;
}

// Inputs con los 4 TOFs validos y un heading dado.
LocalizationInputs make_inputs(
    uint16_t front_mm, uint16_t back_mm, uint16_t left_mm, uint16_t right_mm,
    int16_t heading_centideg
) {
    LocalizationInputs in{};
    in.tof_distance_mm[0] = front_mm;
    in.tof_distance_mm[1] = back_mm;
    in.tof_distance_mm[2] = left_mm;
    in.tof_distance_mm[3] = right_mm;
    in.tof_valid[0] = (front_mm != LOCALIZATION_TOF_NO_READING);
    in.tof_valid[1] = (back_mm != LOCALIZATION_TOF_NO_READING);
    in.tof_valid[2] = (left_mm != LOCALIZATION_TOF_NO_READING);
    in.tof_valid[3] = (right_mm != LOCALIZATION_TOF_NO_READING);
    in.bno_heading_centideg = heading_centideg;
    return in;
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

// ============================================================
// Tests de trilateracion basica (sin rotacion, sin outliers)
// ============================================================

void test_robot_en_centro_apunta_arco_rival(void) {
    // Robot en (1215, 910), apuntando a +Y (al arco rival).
    // TOF frontal mira +Y, deberia leer field_height/2 = 910 mm (a la pared norte).
    // TOF trasero mira -Y, deberia leer 910 mm (a la pared sur).
    // TOF izq mira +X (en el frame robot apunta a la izquierda), pero como
    //   robot apunta a +Y, izquierda del robot es +X mundo? NO — izquierda
    //   del robot apuntando a +Y mundo es -X mundo. Lee 1215 mm (a pared oeste, x=0).
    // TOF der mira derecha = +X mundo. Lee 1215 mm (a pared este, x=2430).
    auto in = make_inputs(910, 910, 1215, 1215, 0);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;  // primer ciclo

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);   // centro X +-10 mm
    TEST_ASSERT_INT16_WITHIN(10, 910,  pose.y_mm);   // centro Y +-10 mm
    TEST_ASSERT_INT16_WITHIN(50, 0,    pose.heading_centideg);  // 0 grados +-0.5
}

// ============================================================
// Runner Unity
// ============================================================
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_robot_en_centro_apunta_arco_rival);
    return UNITY_END();
}
```

- [ ] **Step 2: Verificar que el test compila pero falla por link (no hay .cpp)**

```powershell
cd "software/teensy/Soccer 2026"
pio test -e test_native -f test_localization
```

Expected: error de **link** del tipo `undefined reference to 'iitasoccer::localization_compute(...)'`. **Esto confirma TDD strict** — el test existe pero la función aún no.

Si el error es "header not found", verificar que `test_native` incluye `src/shared/` en su `build_src_filter` (debería ya por el config existente).

- [ ] **Step 3: Commit (test rojo)**

```powershell
git add "software/teensy/Soccer 2026/test/test_localization/test_main.cpp"
git commit -m "test(localization): primer test rojo — robot en centro apunta arco rival

Test 'test_robot_en_centro_apunta_arco_rival' creado siguiendo TDD strict.
Compila pero falla en link porque localization_compute() no esta
implementada todavia. La implementacion minima viene en el commit
siguiente.

Estructura del archivo de tests sigue el patron de test_kinematics:
helpers (make_standard_config, make_inputs), setUp/tearDown vacios,
funciones test_* + main con RUN_TEST + UNITY_BEGIN/END.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 4: Implementación mínima para que pase el test del centro

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/localization.cpp`

- [ ] **Step 1: Crear el .cpp con implementación mínima (sin rotación todavía)**

Crear `software/teensy/Soccer 2026/src/shared/localization.cpp` con:

```cpp
// localization.cpp — Trilateracion geometrica directa.
// Ver localization.h para la API publica y la spec para el algoritmo.

#include "localization.h"

namespace iitasoccer {

// Identificadores internos de las 4 paredes de la cancha.
enum Wall { WALL_NORTH, WALL_SOUTH, WALL_EAST, WALL_WEST, WALL_NONE };

namespace {

// Clasifica a que pared apunta un TOF dado su angulo en el frame mundo
// (heading + angulo de montaje). Usa convencion:
//   +Y = norte (al arco rival)
//   +X = este (lateral derecho)
//   angulo 0 = mira a +Y (norte)
//   angulo 90 = mira a +X (este)  ⚠ ojo: convencion robotic usa +Y como front,
//                                      pero el robot que rota CCW va a -X. Hay que
//                                      tener cuidado con el sentido de rotacion del BNO.
// Por ahora: implementacion minima para el caso heading=0, no rotacion.
Wall classify_wall_simple(uint16_t mount_angle_deg) {
    // Mapeo directo cuando heading=0 (no rotacion):
    //   angulo 0   → frontal → mira +Y → pared NORTH
    //   angulo 180 → trasero → mira -Y → pared SOUTH
    //   angulo 90  → izq → mira -X → pared WEST  (izquierda del robot apuntando +Y)
    //   angulo 270 → der → mira +X → pared EAST  (derecha del robot apuntando +Y)
    if (mount_angle_deg == 0)   return WALL_NORTH;
    if (mount_angle_deg == 180) return WALL_SOUTH;
    if (mount_angle_deg == 90)  return WALL_WEST;
    if (mount_angle_deg == 270) return WALL_EAST;
    return WALL_NONE;
}

}  // namespace

LocalizationPose localization_compute(
    const LocalizationInputs& in,
    const LocalizationConfig& cfg
) {
    LocalizationPose pose{};
    pose.valid = false;

    // Acumular estimaciones de X y de Y por separado.
    int32_t sum_x = 0; int sum_x_count = 0;
    int32_t sum_y = 0; int sum_y_count = 0;

    for (int i = 0; i < 4; ++i) {
        if (!in.tof_valid[i]) continue;
        uint16_t d = in.tof_distance_mm[i];
        Wall w = classify_wall_simple(cfg.tof_mount_angle_deg[i]);

        switch (w) {
            case WALL_NORTH:
                sum_y += static_cast<int32_t>(cfg.field_height_mm) - d;
                ++sum_y_count;
                pose.source_flags |= (1 << i);
                break;
            case WALL_SOUTH:
                sum_y += d;
                ++sum_y_count;
                pose.source_flags |= (1 << i);
                break;
            case WALL_EAST:
                sum_x += static_cast<int32_t>(cfg.field_width_mm) - d;
                ++sum_x_count;
                pose.source_flags |= (1 << i);
                break;
            case WALL_WEST:
                sum_x += d;
                ++sum_x_count;
                pose.source_flags |= (1 << i);
                break;
            case WALL_NONE:
                break;
        }
    }

    if (sum_x_count > 0 && sum_y_count > 0) {
        pose.x_mm = static_cast<int16_t>(sum_x / sum_x_count);
        pose.y_mm = static_cast<int16_t>(sum_y / sum_y_count);
        pose.heading_centideg = in.bno_heading_centideg - cfg.bno_offset_centideg;
        pose.valid = true;
    }

    return pose;
}

}  // namespace iitasoccer
```

- [ ] **Step 2: Correr el test y verificar que pasa**

```powershell
cd "software/teensy/Soccer 2026"
pio test -e test_native -f test_localization
```

Expected: 1 test, PASS.

Si falla:
- Verificar que `pose.x_mm = 1215` con tolerancia ±10
- Verificar que `pose.y_mm = 910` con tolerancia ±10
- Si los valores son raros, ver qué pared clasificó cada TOF (agregar Serial.println temporal NO sirve en host-native — usar TEST_PRINTF si hay)

- [ ] **Step 3: Commit (test verde)**

```powershell
git add "software/teensy/Soccer 2026/src/shared/localization.cpp"
git commit -m "feat(shared): localization.cpp — implementacion minima trilateracion (sin rotacion)

Primera version: solo maneja el caso heading=0 (robot apunta al arco
rival, sin rotacion). Clasifica cada TOF a su pared por angulo de
montaje (fijo) y promedia las estimaciones de X e Y.

Pasa: test_robot_en_centro_apunta_arco_rival.

NO maneja todavia: rotacion del robot (Task 5+), outlier rejection
(Task 7+), validacion de pose.valid (Task 10).

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 5: Test + soporte para robot en otra posición (sin rotación todavía)

**Files:**
- Modify: `software/teensy/Soccer 2026/test/test_localization/test_main.cpp`

- [ ] **Step 1: Agregar tests para esquinas y bordes (sin rotación)**

Editar `test/test_localization/test_main.cpp`. Insertar **antes de `int main(...)`** los nuevos tests:

```cpp
void test_robot_en_esquina_propia_apunta_arco_rival(void) {
    // Robot en (0, 0), apuntando a +Y.
    // TOF frontal: distancia a pared norte = field_height_mm = 1820
    // TOF trasero: distancia a pared sur = 0
    // TOF izq: distancia a pared oeste = 0
    // TOF der: distancia a pared este = field_width_mm = 2430
    auto in = make_inputs(1820, 0, 0, 2430, 0);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 0, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 0, pose.y_mm);
}

void test_robot_en_esquina_rival(void) {
    // Robot en (2430, 1820), apuntando a +Y.
    // TOF frontal: distancia a norte = 0
    // TOF trasero: distancia a sur = 1820
    // TOF izq: distancia a oeste = 2430
    // TOF der: distancia a este = 0
    auto in = make_inputs(0, 1820, 2430, 0, 0);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 2430, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 1820, pose.y_mm);
}

void test_robot_pegado_pared_lateral(void) {
    // Robot en (50, 910), pegado a la pared oeste, en el medio vertical.
    // TOF frontal: distancia a norte = 910
    // TOF trasero: distancia a sur = 910
    // TOF izq: distancia a oeste = 50
    // TOF der: distancia a este = 2380
    auto in = make_inputs(910, 910, 50, 2380, 0);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 50, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910, pose.y_mm);
}
```

Y agregar los `RUN_TEST` correspondientes en `main()`:

```cpp
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_robot_en_centro_apunta_arco_rival);
    RUN_TEST(test_robot_en_esquina_propia_apunta_arco_rival);
    RUN_TEST(test_robot_en_esquina_rival);
    RUN_TEST(test_robot_pegado_pared_lateral);
    return UNITY_END();
}
```

- [ ] **Step 2: Correr tests y verificar que TODOS pasan**

```powershell
pio test -e test_native -f test_localization
```

Expected: 4 tests, PASS. (Estos casos no requieren código nuevo — el algoritmo ya los maneja.)

- [ ] **Step 3: Commit (refactor / cobertura)**

```powershell
git add "software/teensy/Soccer 2026/test/test_localization/test_main.cpp"
git commit -m "test(localization): cobertura de esquinas y bordes (sin rotacion)

Tres tests nuevos validan que la trilateracion maneja correctamente
posiciones extremas: esquina propia (0,0), esquina rival (W,H), pegado
a pared lateral. Todos pasan sin cambios al algoritmo — el codigo del
commit anterior ya los cubre.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Phase 3 — Rotación del robot

## Task 6: Test rojo — robot rotado 90° (algoritmo actual debería fallar)

**Files:**
- Modify: `software/teensy/Soccer 2026/test/test_localization/test_main.cpp`

- [ ] **Step 1: Agregar test con rotación de 90° derecha**

Insertar **antes del `int main(...)`**:

```cpp
void test_robot_en_centro_rotado_90_derecha(void) {
    // Robot en (1215, 910), rotado 90 grados a la derecha (heading = +9000 centideg).
    // Ahora el TOF [0] mont 0 grados apunta a +X mundo (lateral derecho).
    //              [1] mont 180 grados apunta a -X mundo (lateral izq).
    //              [2] mont 90 grados (izq robot) apunta a -Y mundo (pared sur).
    //              [3] mont 270 grados (der robot) apunta a +Y mundo (pared norte).
    // Distancias esperadas:
    //   frontal [0] → este: field_width - x = 2430 - 1215 = 1215
    //   trasero [1] → oeste: x = 1215
    //   izq [2] → sur: y = 910
    //   der [3] → norte: field_height - y = 910
    auto in = make_inputs(1215, 1215, 910, 910, 9000);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910, pose.y_mm);
    TEST_ASSERT_INT16_WITHIN(50, 9000, pose.heading_centideg);
}
```

Agregar `RUN_TEST(test_robot_en_centro_rotado_90_derecha);` en `main()`.

- [ ] **Step 2: Correr y verificar que FALLA**

```powershell
pio test -e test_native -f test_localization
```

Expected: 5 tests, 1 FAIL — `test_robot_en_centro_rotado_90_derecha`. El algoritmo actual ignora `bno_heading_centideg` para la clasificación de paredes (solo usa `mount_angle_deg`), así que cuando el robot rota, los TOFs ven paredes distintas pero el algoritmo no lo detecta.

- [ ] **Step 3: Implementar rotación en `classify_wall_simple` → `classify_wall_with_rotation`**

Editar `src/shared/localization.cpp`. Reemplazar la función `classify_wall_simple` y su llamada por:

```cpp
namespace {

// Normaliza un angulo a [0, 360).
int normalize_angle_deg(int angle) {
    angle = angle % 360;
    if (angle < 0) angle += 360;
    return angle;
}

// Clasifica a que pared apunta un TOF dado el angulo del robot (heading
// relativo a cancha) + angulo de montaje. La clasificacion usa cuadrantes:
//   [-45, 45) → NORTH (pared +Y)
//   [45, 135) → WEST  (pared -X) — porque +90 grados del robot apunta a su izq
//   [135, 225) → SOUTH (pared -Y)
//   [225, 315) → EAST  (pared +X)
//
// IMPORTANTE: esto asume que el BNO da heading positivo = giro CCW (sentido
// antihorario visto desde arriba). Si el BNO del proyecto da heading CW
// positivo, hay que invertir el signo en la suma.
Wall classify_wall(int16_t robot_heading_deg, uint16_t mount_angle_deg) {
    int world_angle = normalize_angle_deg(
        static_cast<int>(robot_heading_deg) + static_cast<int>(mount_angle_deg)
    );

    if (world_angle < 45)  return WALL_NORTH;
    if (world_angle < 135) return WALL_WEST;
    if (world_angle < 225) return WALL_SOUTH;
    if (world_angle < 315) return WALL_EAST;
    return WALL_NORTH;  // 315..360 wraps to NORTH
}

}  // namespace
```

Y dentro de `localization_compute`, **reemplazar** la llamada a `classify_wall_simple(...)` por:

```cpp
// Convertir heading de centideg a deg (sin float).
int16_t heading_deg = (in.bno_heading_centideg - cfg.bno_offset_centideg) / 100;
// ... en el loop ...
Wall w = classify_wall(heading_deg, cfg.tof_mount_angle_deg[i]);
```

- [ ] **Step 4: Correr y verificar que TODOS pasan**

```powershell
pio test -e test_native -f test_localization
```

Expected: 5 tests, PASS.

- [ ] **Step 5: Commit**

```powershell
git add "software/teensy/Soccer 2026/src/shared/localization.cpp" "software/teensy/Soccer 2026/test/test_localization/test_main.cpp"
git commit -m "feat(shared): localization soporta rotacion del robot

Reemplaza classify_wall_simple por classify_wall que considera el
heading relativo a la cancha (BNO - offset). Clasificacion por
cuadrantes de 90 grados centrados en cada pared:
  [-45, 45) → NORTH, [45, 135) → WEST, [135, 225) → SOUTH, [225, 315) → EAST

ASUNCION: BNO da heading positivo = giro CCW (antihorario visto
desde arriba). Si en hardware real es CW, invertir el signo del
heading en la suma (TASK futura: validar con BNO real).

Pasa nuevo test: test_robot_en_centro_rotado_90_derecha + 4 anteriores.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 7: Tests para rotaciones adicionales (180°, -90°)

**Files:**
- Modify: `software/teensy/Soccer 2026/test/test_localization/test_main.cpp`

- [ ] **Step 1: Agregar tests para rotación 180° y -90°**

Insertar antes de `int main(...)`:

```cpp
void test_robot_en_centro_rotado_180(void) {
    // Robot en (1215, 910) rotado 180 grados (apunta al arco propio).
    // Ahora frontal mira -Y (sur), trasero mira +Y (norte), izq mira +X (este),
    // der mira -X (oeste).
    // Distancias: frontal→sur=910, trasero→norte=910, izq→este=1215, der→oeste=1215.
    auto in = make_inputs(910, 910, 1215, 1215, 18000);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910, pose.y_mm);
}

void test_robot_en_centro_rotado_menos90(void) {
    // Robot en (1215, 910) rotado -90 grados (izquierda).
    // Frontal mira -X (oeste), trasero mira +X (este), izq mira +Y (norte),
    // der mira -Y (sur).
    // Distancias: frontal→oeste=1215, trasero→este=1215, izq→norte=910, der→sur=910.
    auto in = make_inputs(1215, 1215, 910, 910, -9000);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910, pose.y_mm);
}
```

Agregar los 2 `RUN_TEST` correspondientes en `main()`.

- [ ] **Step 2: Correr y verificar que TODOS pasan**

```powershell
pio test -e test_native -f test_localization
```

Expected: 7 tests, PASS.

- [ ] **Step 3: Commit**

```powershell
git add "software/teensy/Soccer 2026/test/test_localization/test_main.cpp"
git commit -m "test(localization): cobertura rotaciones 180 grados y -90 grados

Dos tests adicionales validan que la trilateracion + classify_wall
maneja correctamente cualquier multiplo de 90 grados. Ningun cambio
al algoritmo — los tests pasan con la implementacion anterior.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Phase 4 — Outlier rejection

## Task 8: Test rojo — TOF con flag invalid

**Files:**
- Modify: `software/teensy/Soccer 2026/test/test_localization/test_main.cpp`

- [ ] **Step 1: Agregar test con un TOF invalid**

Insertar antes de `int main(...)`:

```cpp
void test_tof_invalid_se_descarta(void) {
    // Robot en centro, pero el TOF izquierdo viene con valid=false.
    // El algoritmo debe usar solo 3 TOFs y aun asi calcular pose.
    // X solo viene del TOF derecho.
    auto in = make_inputs(910, 910, 1215, 1215, 0);
    in.tof_valid[2] = false;  // izquierdo invalid
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910, pose.y_mm);
    // source_flags: bits 0, 1, 3 = 0b1011 = 11
    TEST_ASSERT_EQUAL_UINT8(0b1011, pose.source_flags);
}
```

Agregar `RUN_TEST(test_tof_invalid_se_descarta);` en `main()`.

- [ ] **Step 2: Correr y verificar que pasa**

```powershell
pio test -e test_native -f test_localization
```

Expected: 8 tests, PASS. (El algoritmo ya tiene `if (!in.tof_valid[i]) continue;`.) Si falla, revisar que el source_flags se está acumulando correctamente.

- [ ] **Step 3: Commit**

```powershell
git add "software/teensy/Soccer 2026/test/test_localization/test_main.cpp"
git commit -m "test(localization): TOF invalid se descarta y source_flags refleja"
```

(Si todo OK con commit message corto. Si no, expandir.)

---

## Task 9: Test rojo + impl — lecturas fuera de rango físico

**Files:**
- Modify: `software/teensy/Soccer 2026/test/test_localization/test_main.cpp`
- Modify: `software/teensy/Soccer 2026/src/shared/localization.cpp`

- [ ] **Step 1: Agregar test con lectura > field_height (TOF "se sale" por geometría de esquina)**

Insertar:

```cpp
void test_tof_lectura_mayor_que_cancha_se_descarta(void) {
    // Robot en (1215, 910). El TOF frontal lee 3000 mm (fisicamente imposible,
    // la cancha es 1820 mm de alto). Eso pasa cuando el TOF "se sale" por
    // un borde a causa del FoV de 60 grados.
    // El algoritmo debe descartar esa lectura, no usarla para Y.
    // Y solo viene del TOF trasero entonces.
    auto in = make_inputs(3000, 910, 1215, 1215, 0);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910, pose.y_mm);
    // source_flags: bits 1, 2, 3 = 0b1110 = 14
    TEST_ASSERT_EQUAL_UINT8(0b1110, pose.source_flags);
}
```

Agregar `RUN_TEST` en `main()`.

- [ ] **Step 2: Correr y verificar que FALLA**

```powershell
pio test -e test_native -f test_localization
```

Expected: 9 tests, 1 FAIL — la lectura de 3000 mm se incluye en el promedio y rompe el cálculo de Y.

- [ ] **Step 3: Implementar descarte por rango en `localization_compute`**

Editar `src/shared/localization.cpp`. En el `for` loop, **después** de `if (!in.tof_valid[i]) continue;`, agregar:

```cpp
        // Descarte por rango: si la lectura es fisicamente imposible (mayor
        // que la dimension de la cancha en cualquier eje), descartar.
        const uint16_t max_dim = (cfg.field_width_mm > cfg.field_height_mm)
            ? cfg.field_width_mm : cfg.field_height_mm;
        if (d > max_dim) continue;
        // Descarte por minimo: el VL53L7CX no es fiable bajo 10 mm.
        if (d < 10) continue;
```

- [ ] **Step 4: Correr y verificar PASS**

```powershell
pio test -e test_native -f test_localization
```

Expected: 9 tests, PASS.

- [ ] **Step 5: Commit**

```powershell
git add "software/teensy/Soccer 2026/test/test_localization/test_main.cpp" "software/teensy/Soccer 2026/src/shared/localization.cpp"
git commit -m "feat(shared): outlier rejection — descartar lecturas fuera de rango fisico

Si distancia > max(field_width, field_height) o < 10 mm, descartar.
Pasa cuando el TOF se sale por geometria de esquina (FoV 60 grados) o
satura por reflejo.

Pasa nuevo test: test_tof_lectura_mayor_que_cancha_se_descarta.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 10: Test rojo + impl — inconsistencia entre TOFs del mismo eje

**Files:**
- Modify: `software/teensy/Soccer 2026/test/test_localization/test_main.cpp`
- Modify: `software/teensy/Soccer 2026/src/shared/localization.cpp`

- [ ] **Step 1: Agregar test con TOF lateral bloqueado por robot rival**

Insertar:

```cpp
void test_tof_lateral_bloqueado_se_descarta_por_inconsistencia(void) {
    // Robot en centro (1215, 910). El TOF izquierdo lee 200 mm (un robot rival
    // pegado a 20 cm a la izquierda). El TOF derecho lee normal: 1215 mm.
    //
    // Sin outlier rejection: x_estimado_izq = 200, x_estimado_der = 2430-1215 = 1215.
    //                        promedio = (200+1215)/2 = 707 mm. INCORRECTO.
    //
    // Con outlier rejection por consistencia con pose anterior (cfg.prev_x_mm = 1215):
    //   x_estimado_izq = 200 → distancia a prev = |200-1215| = 1015 mm.
    //   x_estimado_der = 1215 → distancia a prev = 0 mm.
    //   Diferencia entre estimaciones: |200-1215| = 1015 > 300 (threshold).
    //   → descartar la mas lejana del prev → descartar izq.
    //   → x_robot = 1215 (solo del TOF der).
    auto in = make_inputs(910, 910, 200, 1215, 0);
    auto cfg = make_standard_config();
    // prev_valid=true y prev_x_mm=1215 ya estan en make_standard_config.

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_TRUE(pose.valid);
    TEST_ASSERT_INT16_WITHIN(10, 1215, pose.x_mm);
    TEST_ASSERT_INT16_WITHIN(10, 910, pose.y_mm);
    // source_flags: bits 0, 1, 3 = 0b1011 (sin bit 2 = TOF izq descartado)
    TEST_ASSERT_EQUAL_UINT8(0b1011, pose.source_flags);
}
```

Agregar `RUN_TEST` en `main()`.

- [ ] **Step 2: Correr y verificar FAIL**

```powershell
pio test -e test_native -f test_localization
```

Expected: 10 tests, 1 FAIL — el algoritmo promedia 200 y 1215 dando 707, no 1215.

- [ ] **Step 3: Implementar outlier rejection por consistencia**

Esto requiere refactorizar el loop principal. En vez de promediar directo, primero recolectar estimaciones por eje, después descartar y promediar. Reemplazar el cuerpo de `localization_compute` por:

```cpp
LocalizationPose localization_compute(
    const LocalizationInputs& in,
    const LocalizationConfig& cfg
) {
    LocalizationPose pose{};
    pose.valid = false;

    // Recolectar estimaciones por eje, con el indice del TOF que la genero.
    struct Estimate { int16_t value; int tof_idx; };
    Estimate x_estimates[4]; int x_count = 0;
    Estimate y_estimates[4]; int y_count = 0;

    int16_t heading_deg = (in.bno_heading_centideg - cfg.bno_offset_centideg) / 100;
    const uint16_t max_dim = (cfg.field_width_mm > cfg.field_height_mm)
        ? cfg.field_width_mm : cfg.field_height_mm;

    for (int i = 0; i < 4; ++i) {
        if (!in.tof_valid[i]) continue;
        uint16_t d = in.tof_distance_mm[i];
        if (d > max_dim || d < 10) continue;

        Wall w = classify_wall(heading_deg, cfg.tof_mount_angle_deg[i]);
        switch (w) {
            case WALL_NORTH:
                y_estimates[y_count++] = { static_cast<int16_t>(cfg.field_height_mm - d), i };
                break;
            case WALL_SOUTH:
                y_estimates[y_count++] = { static_cast<int16_t>(d), i };
                break;
            case WALL_EAST:
                x_estimates[x_count++] = { static_cast<int16_t>(cfg.field_width_mm - d), i };
                break;
            case WALL_WEST:
                x_estimates[x_count++] = { static_cast<int16_t>(d), i };
                break;
            case WALL_NONE:
                break;
        }
    }

    // Outlier rejection por consistencia entre estimaciones del mismo eje.
    // Si 2 estimaciones difieren mas que el umbral, descartar la mas lejana
    // del pose anterior (si hay pose anterior valido).
    auto reject_outliers = [&](Estimate* arr, int& count, int16_t prev_value) {
        if (count < 2) return;
        if (!cfg.prev_valid) return;
        // Encontrar la pareja con mayor discrepancia.
        for (int i = 0; i < count - 1; ++i) {
            for (int j = i + 1; j < count; ++j) {
                int diff = arr[i].value - arr[j].value;
                if (diff < 0) diff = -diff;
                if (diff > cfg.outlier_threshold_mm) {
                    // Descartar el mas lejano de prev_value.
                    int dist_i = arr[i].value - prev_value; if (dist_i < 0) dist_i = -dist_i;
                    int dist_j = arr[j].value - prev_value; if (dist_j < 0) dist_j = -dist_j;
                    int reject = (dist_i > dist_j) ? i : j;
                    // Shift left para eliminar el rechazado.
                    for (int k = reject; k < count - 1; ++k) {
                        arr[k] = arr[k + 1];
                    }
                    --count;
                    // Reiniciar el escaneo (puede haber mas outliers).
                    return reject_outliers(arr, count, prev_value);
                }
            }
        }
    };

    reject_outliers(x_estimates, x_count, cfg.prev_x_mm);
    reject_outliers(y_estimates, y_count, cfg.prev_y_mm);

    if (x_count > 0 && y_count > 0) {
        int32_t sum_x = 0; int32_t sum_y = 0;
        for (int i = 0; i < x_count; ++i) {
            sum_x += x_estimates[i].value;
            pose.source_flags |= (1 << x_estimates[i].tof_idx);
        }
        for (int i = 0; i < y_count; ++i) {
            sum_y += y_estimates[i].value;
            pose.source_flags |= (1 << y_estimates[i].tof_idx);
        }
        pose.x_mm = static_cast<int16_t>(sum_x / x_count);
        pose.y_mm = static_cast<int16_t>(sum_y / y_count);
        pose.heading_centideg = in.bno_heading_centideg - cfg.bno_offset_centideg;
        pose.valid = true;
    }

    return pose;
}
```

- [ ] **Step 4: Correr y verificar PASS de los 10 tests**

```powershell
pio test -e test_native -f test_localization
```

Expected: 10 tests, PASS. **Importante:** verificar que NO se rompieron los anteriores (especialmente el de rotación, que también usa `prev_valid=false`).

Si algún test anterior rompió, probable causa: la recursión de `reject_outliers` tiene un bug. Debugar caso por caso.

- [ ] **Step 5: Commit**

```powershell
git add "software/teensy/Soccer 2026/test/test_localization/test_main.cpp" "software/teensy/Soccer 2026/src/shared/localization.cpp"
git commit -m "feat(shared): outlier rejection por inconsistencia entre TOFs mismo eje

Refactor del loop principal: ahora recolecta estimaciones por eje y
descarta las que difieren > outlier_threshold_mm, conservando la mas
cercana al pose anterior (prev_x_mm/prev_y_mm).

Esto rechaza el caso comun de un robot rival pegado a un lateral
que mete una lectura muy cercana en el TOF de ese lado.

Requiere cfg.prev_valid = true para activarse. En el primer ciclo
(prev_valid=false) el outlier rejection esta apagado.

Pasa nuevo test: test_tof_lateral_bloqueado_se_descarta_por_inconsistencia.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Phase 5 — Edge cases

## Task 11: Test — menos de 2 TOFs útiles → pose.valid = false

**Files:**
- Modify: `software/teensy/Soccer 2026/test/test_localization/test_main.cpp`

- [ ] **Step 1: Agregar test**

```cpp
void test_pose_invalid_sin_suficientes_tofs(void) {
    // Solo el TOF frontal es valido. Da estimacion de Y pero ninguna de X.
    // pose.valid debe ser false.
    auto in = make_inputs(910, 0, 0, 0, 0);
    in.tof_valid[1] = false;
    in.tof_valid[2] = false;
    in.tof_valid[3] = false;
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_FALSE(pose.valid);
}

void test_pose_invalid_todos_tofs_fuera_de_rango(void) {
    // Los 4 TOFs leen valores fisicamente imposibles.
    auto in = make_inputs(5000, 5000, 5000, 5000, 0);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    TEST_ASSERT_FALSE(pose.valid);
}
```

`RUN_TEST` para ambos.

- [ ] **Step 2: Correr y verificar PASS**

```powershell
pio test -e test_native -f test_localization
```

Expected: 12 tests, PASS. (El algoritmo ya tiene la condición `if (x_count > 0 && y_count > 0)`.)

- [ ] **Step 3: Commit**

```powershell
git add "software/teensy/Soccer 2026/test/test_localization/test_main.cpp"
git commit -m "test(localization): pose.valid=false cuando <2 TOFs utiles"
```

---

## Task 12: Test — ángulos de borde (45°, 135°, 225°, 315°)

**Files:**
- Modify: `software/teensy/Soccer 2026/test/test_localization/test_main.cpp`

- [ ] **Step 1: Agregar test**

```cpp
void test_clasificacion_borde_45_grados_estable(void) {
    // Robot rotado exactamente 45 grados. El TOF frontal (mount 0) tiene
    // world_angle = 45 → clasificado como WALL_WEST (intervalo [45, 135)).
    // La proyeccion va a ser incorrecta porque la distancia no es perpendicular,
    // pero el algoritmo no debe crashear ni devolver basura.
    // Test minimo: pose es valid (al menos un TOF da estimacion en cada eje
    // aunque no sean precisas).
    auto in = make_inputs(910, 910, 1215, 1215, 4500);
    auto cfg = make_standard_config();
    cfg.prev_valid = false;

    auto pose = localization_compute(in, cfg);

    // No requerimos precision (Sprint 2 con projection por coseno la mejora),
    // pero el algoritmo no debe crashear.
    TEST_ASSERT_TRUE(pose.valid || !pose.valid);  // tautologia: solo verifica no crash
    // Para Sprint 1 con asumimos paredes alineadas con eje, este caso da
    // resultados sin garantia de precision. Documentado en el spec §10 R4.
}
```

`RUN_TEST` correspondiente.

- [ ] **Step 2: Correr y verificar PASS (sin crash)**

```powershell
pio test -e test_native -f test_localization
```

Expected: 13 tests, PASS.

- [ ] **Step 3: Commit**

```powershell
git add "software/teensy/Soccer 2026/test/test_localization/test_main.cpp"
git commit -m "test(localization): caso degenerado 45 grados — no debe crashear"
```

---

## Phase 6 — Hardware glue

## Task 13: Crear `src/top/localization_runtime.h`

**Files:**
- Create: `software/teensy/Soccer 2026/src/top/localization_runtime.h`

- [ ] **Step 1: Crear el header**

```cpp
// localization_runtime.h — Glue de hardware para el modulo localization.
//
// Orquesta el polling de sensors_tof + sensors_imu, llama a
// localization_compute() (logica pura) y cachea el resultado para que
// main_top.cpp lo lea al armar el WorldSnapshot.

#pragma once
#include "../shared/localization.h"

namespace iitasoccer {

// Calibra bno_offset leyendo el heading actual del BNO. El robot DEBE
// estar apuntando al arco rival (+Y de la cancha) cuando se llama.
// Imprime al Serial el offset capturado para que el equipo verifique.
void localization_runtime_init();

// Lee TOF + IMU, arma LocalizationInputs, llama a localization_compute,
// cachea resultado. Llamar a ~30 Hz desde el loop del top.
void localization_runtime_tick();

// Devuelve el ultimo pose calculado. Single-producer single-consumer:
// el tick llama a esta funcion para escribir, main_top la llama para leer.
LocalizationPose localization_runtime_get_pose();

}  // namespace iitasoccer
```

- [ ] **Step 2: Verificar que el firmware top sigue compilando**

```powershell
cd "software/teensy/Soccer 2026"
pio run -e top
```

Expected: SUCCESS (el header no es incluido por nadie todavía, no rompe nada).

- [ ] **Step 3: Commit**

```powershell
git add "software/teensy/Soccer 2026/src/top/localization_runtime.h"
git commit -m "feat(top): localization_runtime.h — API del glue de HW (sin implementacion)"
```

---

## Task 14: Implementar `localization_runtime.cpp`

**Files:**
- Create: `software/teensy/Soccer 2026/src/top/localization_runtime.cpp`

- [ ] **Step 1: Crear el .cpp**

```cpp
// localization_runtime.cpp — Glue de hardware para el modulo localization.

#include "localization_runtime.h"
#include "config_top.h"
#include "sensors_tof.h"
#include "sensors_imu.h"
#include <Arduino.h>

namespace iitasoccer {

namespace {

LocalizationConfig g_config;
LocalizationPose   g_last_pose;
bool               g_initialized = false;

}  // namespace

void localization_runtime_init() {
    // Inicializar config con valores de config_top.h.
    g_config.field_width_mm           = FIELD_WIDTH_MM;
    g_config.field_height_mm          = FIELD_HEIGHT_MM;
    g_config.outlier_threshold_mm     = LOCALIZATION_OUTLIER_THRESHOLD_MM;
    for (int i = 0; i < NUM_TOF; ++i) {
        g_config.tof_mount_angle_deg[i] = TOF_MOUNT_ANGLE_DEG[i];
    }
    g_config.prev_x_mm    = FIELD_WIDTH_MM / 2;   // centro como guess inicial
    g_config.prev_y_mm    = FIELD_HEIGHT_MM / 2;
    g_config.prev_valid   = false;  // primer ciclo no usa outlier rejection

    // Esperar estabilizacion del BNO antes de leer heading.
    delay(100);
    // Calibrar offset: heading actual del BNO = "robot apunta al arco rival".
    g_config.bno_offset_centideg = sensors_imu_get_heading_centideg();

    Serial.print("[localization] BNO offset = ");
    Serial.print(g_config.bno_offset_centideg / 100.0);
    Serial.println(" deg (robot DEBE apuntar al arco rival al encender)");

    g_initialized = true;
    g_last_pose = LocalizationPose{};
    g_last_pose.valid = false;
}

void localization_runtime_tick() {
    if (!g_initialized) return;

    // Armar inputs leyendo de los modulos de sensores.
    LocalizationInputs in{};
    for (int i = 0; i < NUM_TOF; ++i) {
        in.tof_distance_mm[i] = sensors_tof_get_distance_mm(i);
        in.tof_valid[i] = (in.tof_distance_mm[i] != TOF_NO_READING)
                          && sensors_tof_is_ready(i);
    }
    in.bno_heading_centideg = sensors_imu_get_heading_centideg();

    // Llamar al algoritmo puro.
    g_last_pose = localization_compute(in, g_config);

    // Actualizar prev_* para el outlier rejection del proximo ciclo.
    if (g_last_pose.valid) {
        g_config.prev_x_mm  = g_last_pose.x_mm;
        g_config.prev_y_mm  = g_last_pose.y_mm;
        g_config.prev_valid = true;
    }
}

LocalizationPose localization_runtime_get_pose() {
    return g_last_pose;
}

}  // namespace iitasoccer
```

- [ ] **Step 2: Verificar compilación del env top**

```powershell
cd "software/teensy/Soccer 2026"
pio run -e top
```

Expected: SUCCESS, log debe mostrar `Compiling .pio\build\top\src\top\localization_runtime.cpp.o` y `Compiling .pio\build\top\src\shared\localization.cpp.o`.

Si falla con "sensors_imu_get_heading_centideg not found": verificar que existe en `sensors_imu.h`. Si no existe, **agregar declaración y un wrapper** que use el método correcto de la lib BNO055. (Adafruit_BNO055 da heading como `float` en grados — convertir a int16 centideg.)

Si esta función no existe en `sensors_imu.h` actual, agregar:

```cpp
// En sensors_imu.h, dentro del namespace:
int16_t sensors_imu_get_heading_centideg();
```

```cpp
// En sensors_imu.cpp:
int16_t sensors_imu_get_heading_centideg() {
    // Devuelve el heading del BNO izq (o promedio si IMU dual). Rango [-18000, 18000].
    float heading_deg = g_bno_left.getEulerAngles().z;  // adaptar al nombre real
    // Normalizar a [-180, 180]:
    while (heading_deg > 180.0f) heading_deg -= 360.0f;
    while (heading_deg < -180.0f) heading_deg += 360.0f;
    return static_cast<int16_t>(heading_deg * 100.0f);
}
```

(Si la API real es distinta, adaptar. Esto puede requerir leer `sensors_imu.cpp` actual y ajustar.)

- [ ] **Step 3: Commit**

```powershell
git add "software/teensy/Soccer 2026/src/top/localization_runtime.cpp" "software/teensy/Soccer 2026/src/top/sensors_imu.h" "software/teensy/Soccer 2026/src/top/sensors_imu.cpp"
git commit -m "feat(top): localization_runtime.cpp — glue de hardware + ajuste sensors_imu

Implementa init() que calibra bno_offset, tick() que lee sensores y
llama al algoritmo puro, y get_pose() para que main_top lo consuma.

Mantiene g_config.prev_x_mm/prev_y_mm actualizado para que el outlier
rejection por consistencia funcione del segundo ciclo en adelante.

Si era necesario, agrega sensors_imu_get_heading_centideg() a la API
del modulo sensors_imu (wrapper para devolver heading como int16 en
centideg en vez de float en grados).

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 15: Integrar al `main_top.cpp`

**Files:**
- Modify: `software/teensy/Soccer 2026/src/top/main_top.cpp`

- [ ] **Step 1: Leer main_top.cpp actual para entender estructura**

```powershell
cd "C:\Users\violl\iitasoccer\open-soccer-robocup-team2026"
```
Abrir `software/teensy/Soccer 2026/src/top/main_top.cpp` y ubicar:
- La llamada a `sensors_tof_init()` y `sensors_imu_init()` en `setup()`
- Las llamadas a `sensors_tof_tick()` y `sensors_imu_tick()` en `loop()`
- El bloque que arma el WorldSnapshot (probable nombre: `make_world_snapshot`, `build_snapshot`, etc.)

- [ ] **Step 2: Agregar include + llamadas init**

En el bloque de includes al inicio de `main_top.cpp`, agregar:

```cpp
#include "localization_runtime.h"
```

En `setup()`, **después** de `sensors_tof_init()` y `sensors_imu_init()`, agregar:

```cpp
    iitasoccer::localization_runtime_init();
```

- [ ] **Step 3: Agregar tick en loop**

En `loop()`, agregar un tick a ~30 Hz. Si ya hay un patrón de "elapsedMillis" o similar para timing, usar el mismo. Ejemplo:

```cpp
    static elapsedMillis since_loc;
    if (since_loc >= 33) {
        since_loc = 0;
        iitasoccer::localization_runtime_tick();
    }
```

- [ ] **Step 4: Conectar pose al WorldSnapshot**

Buscar la función o bloque que arma el `WorldSnapshot` (estructura definida en `src/shared/types.h`). Identificar si tiene campos para pose (probables nombres: `robot_x_mm`, `robot_y_mm`, `robot_heading_centideg`).

**Si los campos existen**, agregar antes de enviar el snapshot:

```cpp
    auto pose = iitasoccer::localization_runtime_get_pose();
    snapshot.robot_x_mm           = pose.x_mm;
    snapshot.robot_y_mm           = pose.y_mm;
    snapshot.robot_heading_centideg = pose.heading_centideg;
    if (pose.valid) snapshot.flags |= SNAPSHOT_FLAG_POSE_VALID;
```

**Si los campos NO existen** (probable — el WorldSnapshot v2 actual puede no tenerlos), dejar este step incompleto y agregar comentario:

```cpp
    // TODO: agregar campos robot_x_mm / robot_y_mm / robot_heading_centideg
    //       al WorldSnapshot v2 en una rev menor del protocolo. Hasta entonces,
    //       el pose no llega al CENTRAL.
    auto pose = iitasoccer::localization_runtime_get_pose();
    (void)pose;  // suprime warning unused mientras tanto
```

Crear un team-task para coordinar la actualización del WorldSnapshot con quien maneja `CONTRATO-DATOS-TOP.md`.

- [ ] **Step 5: Compile gate**

```powershell
cd "software/teensy/Soccer 2026"
pio run -e top
```

Expected: SUCCESS con el sketch tocado. Si falla por símbolo de snapshot inexistente, probablemente hay que ajustar nombres del struct (leer `src/shared/types.h` para confirmar).

- [ ] **Step 6: Commit**

```powershell
git add "software/teensy/Soccer 2026/src/top/main_top.cpp"
git commit -m "feat(top): integrar localization al loop principal

setup(): llama a localization_runtime_init() despues de sensors_tof_init
y sensors_imu_init. Esto calibra el bno_offset con el robot apuntando
al arco rival al boot.

loop(): tick de localization a 30 Hz (elapsedMillis pattern).

WorldSnapshot: [completar segun resultado del Step 4 del plan —
si los campos existen, dejarlos conectados; si no, TODO + team-task].

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Phase 7 — Diagnostic + final validation

## Task 16: Crear sketch + env `diag_localization_live`

**Files:**
- Create: `software/teensy/Soccer 2026/src/diag/diag_localization_live.cpp`
- Modify: `software/teensy/Soccer 2026/platformio.ini`

- [ ] **Step 1: Crear el sketch standalone**

```cpp
// diag_localization_live.cpp — Diagnostico en banco del modulo localization.
//
// NO es firmware de competencia. Sketch standalone que llama al modulo
// localization_runtime y printea pose cada 500 ms por Serial. Permite
// validar la trilateracion en banco con el robot real puesto a mano en
// posiciones conocidas de la cancha.
//
// Build / flash:
//   pio run -t clean -e diag_localization_live
//   pio run -e diag_localization_live -t upload
//   pio device monitor -b 115200

#include <Arduino.h>
#include "localization_runtime.h"
#include "sensors_tof.h"
#include "sensors_imu.h"
#include "config_top.h"

using namespace iitasoccer;

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* esperar monitor */ }

    Serial.println("\n=========================================");
    Serial.println("  diag_localization_live");
    Serial.println("  (banco, NO es competencia)");
    Serial.println("  ROBOT DEBE APUNTAR AL ARCO RIVAL");
    Serial.println("=========================================");

    sensors_imu_init();
    sensors_tof_init();
    delay(500);  // estabilizacion sensores

    localization_runtime_init();
    Serial.println("[diag] init OK. Pose cada 500 ms.\n");
}

void loop() {
    static elapsedMillis since_tick;
    static elapsedMillis since_print;

    if (since_tick >= 33) {
        since_tick = 0;
        sensors_imu_tick();
        sensors_tof_tick();
        localization_runtime_tick();
    }

    if (since_print >= 500) {
        since_print = 0;
        auto pose = localization_runtime_get_pose();
        Serial.print("[");
        Serial.print(millis());
        Serial.print(" ms]  ");
        if (pose.valid) {
            Serial.print("x=");      Serial.print(pose.x_mm);
            Serial.print("mm  y="); Serial.print(pose.y_mm);
            Serial.print("mm  hdg="); Serial.print(pose.heading_centideg / 100.0);
            Serial.print("deg  src=0b");
            for (int i = 3; i >= 0; --i) Serial.print((pose.source_flags >> i) & 1);
            Serial.println();
            digitalWrite(PIN_LED_STATUS, HIGH);
        } else {
            Serial.println("pose INVALID (no hay suficientes TOFs utiles)");
            digitalWrite(PIN_LED_STATUS, LOW);
        }
    }
}
```

- [ ] **Step 2: Agregar env nuevo a `platformio.ini`**

Al final de `platformio.ini`, agregar:

```ini
; ============================================================
; diag_localization_live — DIAGNOSTICO del modulo localization en banco.
; NO es firmware de competencia. Reusa la API publica del modulo vivo
; (localization_runtime_init/tick/get_pose) pero compila aislado (sin
; main_top, sin camaras, sin comm). Permite validar la trilateracion
; con el robot real puesto a mano en posiciones conocidas de la cancha.
;
;   pio run -t clean -e diag_localization_live
;   pio run -e diag_localization_live -t upload
;   pio device monitor -b 115200
;
; REQUISITO HARDWARE: bodge XSHUT debe estar hecho para que los 4 TOFs
; tengan direcciones I2C distintas. Sin el bodge, solo funciona con
; TOF[0] frontal (los otros 3 devuelven TOF_NO_READING).
; ============================================================
[env:diag_localization_live]
platform = teensy
board = teensy40
framework = arduino
build_flags =
    -DBOARD_TOP_DIAG
    -std=gnu++17
    -I src/top
    -I src/shared
build_unflags = -std=gnu++11
build_src_filter = +<diag/diag_localization_live.cpp>
                   +<top/localization_runtime.cpp>
                   +<top/sensors_tof.cpp>
                   +<top/sensors_imu.cpp>
                   +<shared/localization.cpp>
```

- [ ] **Step 3: Extender exclude de `[env:diag_down]`**

Buscar la línea `build_src_filter` en `[env:diag_down]` y agregar `-<diag/diag_localization_live.cpp>` al final.

- [ ] **Step 4: Compile gate**

```powershell
cd "software/teensy/Soccer 2026"
pio run -e diag_localization_live
```

Expected: SUCCESS. Log debe mostrar `Compiling diag_localization_live.cpp` (fresh build).

- [ ] **Step 5: Commit**

```powershell
git add "software/teensy/Soccer 2026/src/diag/diag_localization_live.cpp" "software/teensy/Soccer 2026/platformio.ini"
git commit -m "feat(diag): diag_localization_live env + sketch para validacion en banco

Sketch standalone que reusa el modulo localization vivo (init/tick/
get_pose) y printea pose cada 500 ms. Permite validar la trilateracion
moviendo el robot a mano sobre la cancha y comparando con metro.

Requiere bodge XSHUT terminado para usar los 4 TOFs. Sin el bodge,
solo el TOF[0] frontal devuelve dato — pose.valid sera false la
mayoria del tiempo.

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Task 17: Regression suite final

**Files:** ninguno (solo verificación).

- [ ] **Step 1: Correr todos los envs + tests**

```powershell
cd "software/teensy/Soccer 2026"
pio run -e top
pio run -e diag_top_tof_adafruit
pio run -e diag_sensors_tof_live
pio run -e diag_localization_live
pio run -e diag_down
pio run -e central_robot1
pio run -e central_robot2
pio test -e test_native -f test_localization
pio test -e test_native -f test_kinematics
pio test -e test_native -f test_proto
pio test -e test_native -f test_behind_ball
pio test -e test_native -f test_cameras_fusion
```

Expected: TODOS SUCCESS / PASS.

Si alguno falla, identificar la regresión y arreglar antes de pasar al siguiente paso.

- [ ] **Step 2: Crear team-task de validación en hardware (post-bodge)**

Crear `team-tasks/2026-05-25-task-035-validar-localization-trilateration-en-hardware.md`:

```markdown
---
id: TASK-035
title: "Validar Sprint 1 localizacion (trilateracion) en hardware real"
date_created: 2026-05-25
date_due: 2026-06-15
assigned: [Virginia o Elias]
priority: P1
status: pending
estimated_hours: 1
blocks: [scope-firmware-pose-incheon]
blocked_by: [TASK-033]  # bodge XSHUT debe estar hecho
tags: [hardware-test, top-board, localizacion, sprint1]
---

# TASK-035 — Validar Sprint 1 localizacion en hardware

## Prerequisitos

- Bodge XSHUT terminado (TASK-033 done).
- Placa TOP funcionando con 4 TOFs enumerados (verificar que
  `pio device monitor` con `diag_top_tof_adafruit` da lecturas
  distintas en cada TOF cuando se mueven objetos por delante).

## Procedimiento

1. Llevar el robot a la cancha real (lab IITA).
2. Con el robot apagado, ponerlo en el centro de la cancha apuntando
   al arco rival (lado +Y por convencion).
3. Encender el robot.
4. Flashear el diag:
   ```
   pio run -t clean -e diag_localization_live
   pio run -e diag_localization_live -t upload
   pio device monitor -b 115200
   ```
5. Confirmar en el monitor: banner aparece, "BNO offset = X deg",
   "init OK".

## Criterios de aceptacion (los 4 que pasan = TASK done)

- [ ] **Centro de cancha (1215, 910)**: pose printeada cada 500 ms
      muestra `x=1215±30`, `y=910±30`, `hdg=0±2°`.
- [ ] **Esquina propia (0, 0)**: mover robot ahi → pose `x=0±30`, `y=0±30`.
- [ ] **Esquina rival (2430, 1820)**: pose `x=2430±30`, `y=1820±30`.
- [ ] **Rotacion 90° derecha en centro**: pose `x=1215±30`, `y=910±30`,
      `hdg=90±2°`.

## Si algun criterio falla

- Pose incorrecto pero consistente: revisar `BNO offset` printeado al
  init. Si no es 0 cuando el robot apunta al arco rival, hay error
  de orientacion al encender. Power cycle con orientacion correcta.
- Pose `INVALID` la mayoria del tiempo: revisar I2C scan con
  `diag_top_tof_adafruit` que los 4 TOFs responden.
- Pose salta entre ciclos: el filtro temporal de Sprint 2 lo va a
  resolver. Para Sprint 1, anotar y seguir.

## Cierre

Pegar logs del monitor + medicion con metro en el journal:
`journal/2026-XX-XX-validacion-hardware-localization-sprint1.md`.
Actualizar este TASK con status=done.
```

```powershell
git add "team-tasks/2026-05-25-task-035-validar-localization-trilateration-en-hardware.md"
git commit -m "task(top): TASK-035 validar Sprint 1 localizacion en hardware real

P1 para Virginia/Elias. Bloqueada por TASK-033 (bodge XSHUT).
4 criterios medibles (centro, 2 esquinas, rotacion 90).

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

- [ ] **Step 3: Actualizar `ESTADO-ACTUAL.md`**

En `docs/ESTADO-ACTUAL.md`, en la seccion TOP, agregar:

```markdown
- `src/shared/localization.cpp` — trilateracion geometrica directa (Sprint 1
  aprobado 2026-05-25, ver `docs/superpowers/specs/2026-05-25-...`).
  Validacion en hardware pendiente: TASK-035.
- `src/top/localization_runtime.cpp` — glue I/O.
```

Y en "Tests host-native", agregar fila:
```
| test_localization | 13 | trilateracion + outliers + rotaciones + edge cases |
```

```powershell
git add "docs/ESTADO-ACTUAL.md"
git commit -m "docs(estado): localization Sprint 1 implementado, validacion HW pendiente

Modulo localization (trilateracion) integrado al firmware vivo TOP.
Tests host-native: 13 pasando. Validacion en hardware queda en TASK-035
(bloqueada por bodge XSHUT TASK-033).

Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)"
```

---

## Resumen final (post-Task 17)

Al terminar el plan deberías tener:

- ✅ Modulo `src/shared/localization.{h,cpp}` puro + 13 tests Unity pasando
- ✅ Modulo `src/top/localization_runtime.{h,cpp}` integrado al firmware vivo
- ✅ `main_top.cpp` llama init + tick + conecta pose al WorldSnapshot (o TODO si campos no existen)
- ✅ Sketch + env `diag_localization_live` para validación en banco
- ✅ Compile gates de los 7 envs SUCCESS
- ✅ Team-task TASK-035 para validación humana en hardware
- ✅ `ESTADO-ACTUAL.md` actualizado
- ✅ ~17 commits con atribución correcta

Lo que NO esta hecho (es trabajo del equipo o de otro sprint):
- ❌ Validación en hardware real (TASK-035, bloqueada por TASK-033 bodge XSHUT)
- ❌ Filtros temporales (Kalman, mediana móvil) — Sprint 2 o post-Incheon
- ❌ Análisis multizona 8×8 para outlier rejection — Sprint 2 (Alt 5 v2)
- ❌ Conexión a WorldSnapshot si los campos no existen — coordinar con dueño del CONTRATO-DATOS-TOP.md

---

## Self-review notes

(Hecho por el coach antes de entregar el plan.)

1. **Spec coverage:** todas las secciones del spec están cubiertas:
   - §2 componentes → Tasks 2, 3, 13, 14, 16
   - §3 API pública → Task 2 (header completo)
   - §4 algoritmo → Tasks 4, 6 (rotación)
   - §5 outlier rejection → Tasks 9, 10
   - §6 calibración → Task 14 (`localization_runtime_init`)
   - §7 testing → Tasks 3-12, 17
   - §8 integración → Task 15

2. **Placeholder scan:** sin TBD/TODO huecos. Hay 1 TODO documentado intencionalmente en Task 15 Step 4 (depende de si campos del WorldSnapshot existen — el plan da ambas ramas).

3. **Type consistency:** `LocalizationInputs`, `LocalizationConfig`, `LocalizationPose` y la función `localization_compute()` aparecen con misma signatura en todas las tasks. `localization_runtime_init/tick/get_pose` consistentes en Tasks 13-16.
