---
title: "Diseño — Localización XY+θ Sprint 1: trilateración geométrica con 4 TOFs + BNO"
date: 2026-05-25
author: "Claude Opus 4.7 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: aprobado
tags: [firmware, top-board, localizacion, tof, bno055, control, diseno, sprint1]
robot: ambos
area: control
tipo: decision
related:
  - research/in-progress/2026-05-25-localizacion-tof-imu-analisis.md
  - journal/2026-05-25-top-xshut-no-routed-finding.md
  - journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md
  - software/teensy/Soccer 2026/src/top/sensors_tof.cpp
  - software/teensy/Soccer 2026/src/top/sensors_imu.cpp
  - docs/firmware/CONTRATO-DATOS-TOP.md
---

# Localización Sprint 1 — trilateración geométrica directa

> Aprobado por Gustavo (2026-05-25). Primer sprint de localización XY + heading
> del robot en la cancha de RCJ Soccer Open 2026, usando los 4 TOF VL53L7CX
> cardinales + heading del BNO055. **Baseline simple (±2-3 cm)** que se complementa
> en Sprint 2 con LUT matching multizona (±1 cm) si fuera necesario.

## 0. Contexto

### Hardware disponible (post-bodge XSHUT)

- **4× VL53L7CX** (Pololu carriers) montados en disposición **cardinal** (frontal,
  trasero, izquierdo, derecho del robot) a **14 cm de altura del piso**.
- **FoV de cada TOF: 60° × 60°** (datasheet), 8×8 zonas multizona.
- **2× BNO055** (IMU dual), provee heading absoluto al norte magnético.
- Bus I²C0 (Wire) compartido por TOF[0] y TOF[1] + BNO055 izq.
- Bus I²C1 (Wire1, pines 24/25 remap) compartido por TOF[2] y TOF[3] + BNO055 der.

### Cancha objetivo (RCJ Soccer Open 2026)

- **Dimensiones interiores: 2430 mm × 1820 mm** (rulebook 2026).
- **Barricadas perimetrales de 10-22 cm de alto** — los TOFs a 14 cm las ven
  como pared reflectiva continua (geometría favorable).
- **Pelota pasiva** de 7 cm de diámetro, centro a ~3.5 cm del piso →
  **NO interfiere con los TOFs** por geometría (pasa por debajo).
- **Robots oponentes/partner** típicamente 18-22 cm de alto → SÍ aparecen en
  lecturas TOF como obstáculos.

### Necesidad

Que el robot CENTRAL pueda recibir en el `WorldSnapshot v2` su pose absoluta
en la cancha (XY en mm + heading en centidegrees relativos al norte de la
cancha) para que la FSM táctica pueda tomar decisiones tipo "ir a posición
defensiva", "patear al arco rival" sin depender solo de visión.

### Dependencia bloqueante

- **Bodge XSHUT pendiente** (Enzo): el firmware completo requiere los 4 TOFs
  con direcciones I²C distintas. Hasta que se cablee, solo 1 TOF por bus
  funciona. **El diseño NO se bloquea** — los tests host-native se pueden
  correr ya, la validación hardware espera al bodge.

## 1. Decisiones de arquitectura

Tres decisiones tomadas durante el brainstorming (2026-05-25):

| Decisión | Elegido | Por qué |
|---|---|---|
| Algoritmo | **Trilateración geométrica directa** (Alt 1 del análisis) | Simple, sin filtros temporales, fácil de debuggear. Capitalizable para 2027. Sprint 2 (LUT matching) es el upgrade. |
| Calibración del norte cancha | **Boot apuntando al arco rival** | El robot lee heading del BNO al boot y lo guarda como `bno_offset`. Operativa: el equipo coloca el robot apuntando al arco rival antes de encender. Sin hardware extra. |
| Separación lógica/glue | **Lógica pura en `src/shared/`, glue HW en `src/top/`** | Lógica testeable host-native sin Arduino. Mismo patrón que `pids`, `kinematics`, `cameras_fusion`, etc. |

## 2. Componentes

### 2.A — `src/shared/localization.{h,cpp}` (puro, host-testeable)

**Path:** `software/teensy/Soccer 2026/src/shared/localization.h` + `localization.cpp`

**Responsabilidad:** dado un conjunto de mediciones (4 distancias TOF + heading
BNO), devolver la pose XY+θ del robot en la cancha. **No hace I/O, no usa
Arduino, no usa Wire.** Compilable y testeable host-native con Unity.

**Tamaño esperado:** ~80 LOC header + ~250 LOC source.

### 2.B — `src/top/localization_runtime.{h,cpp}` (glue hardware)

**Path:** `software/teensy/Soccer 2026/src/top/localization_runtime.h` + `.cpp`

**Responsabilidad:**
- En setup: calibrar `bno_offset` leyendo heading actual.
- En tick: leer datos crudos de `sensors_tof_*` y `sensors_imu_*`, armar el
  `LocalizationInputs`, llamar a `localization_compute()`, cachear el resultado.
- Exponer `localization_runtime_get_pose()` para que `main_top` lo lea al armar
  el `WorldSnapshot`.

**Tamaño esperado:** ~50 LOC header + ~120 LOC source.

### 2.C — `test/test_localization/test_main.cpp`

**Path:** `software/teensy/Soccer 2026/test/test_localization/test_main.cpp`

**Responsabilidad:** suite Unity con ~25-30 tests cubriendo:
- Robot en centro / esquinas / borde
- Robot rotado en 8 ángulos cardinales
- TOFs con lecturas válidas, inválidas, parcialmente bloqueadas
- Casos de borde: <2 TOFs útiles → pose.valid=false
- Edge cases: TOF apuntando exactamente a esquina (FoV se "sale")

**Tamaño esperado:** ~400 LOC.

### 2.D — Cambios menores en archivos existentes

- `src/top/config_top.h`: agregar constantes `FIELD_WIDTH_MM`, `FIELD_HEIGHT_MM`,
  ángulos de montaje de cada TOF.
- `src/top/main_top.cpp`: llamar a `localization_runtime_init()` en setup y
  `localization_runtime_tick()` en loop, después de sensors_tof + sensors_imu.
- Si el `WorldSnapshot v2` ya tiene campos `x_mm`/`y_mm`/`heading_centideg`,
  conectar nuestro pose. Si no, agregar (probable rev menor del protocolo).

## 3. API pública

### `src/shared/localization.h`

```cpp
#pragma once
#include <stdint.h>

namespace iitasoccer {

// Sentinel para indicar lectura inválida.
constexpr uint16_t TOF_NO_READING = 0xFFFF;

struct LocalizationInputs {
    uint16_t tof_distance_mm[4];   // [0]=frontal, [1]=trasero, [2]=izq, [3]=der
    bool     tof_valid[4];          // false si lectura era TOF_NO_READING o varianza alta
    int16_t  bno_heading_centideg; // -18000..18000 (= ±180° × 100)
};

struct LocalizationConfig {
    uint16_t field_width_mm;        // 2430 — eje X de la cancha
    uint16_t field_height_mm;       // 1820 — eje Y de la cancha
    int16_t  bno_offset_centideg;   // calibrado al boot
    // Ángulos de montaje (grados): 0=frente del robot, 90=izquierda, etc.
    uint16_t tof_mount_angle_deg[4]; // esperado: {0, 180, 90, 270}
    uint16_t outlier_threshold_mm;  // umbral para descarte por inconsistencia (default 300 mm)
};

struct LocalizationPose {
    int16_t  x_mm;                 // 0..field_width_mm
    int16_t  y_mm;                 // 0..field_height_mm
    int16_t  heading_centideg;     // 0..36000 (heading relativo a +Y de la cancha)
    uint8_t  source_flags;         // bit i = 1 si TOF[i] se usó este ciclo
    bool     valid;                // false si <2 TOFs útiles
};

// Función pura: dados inputs y config, devuelve pose. Sin side effects.
LocalizationPose localization_compute(
    const LocalizationInputs& in,
    const LocalizationConfig& cfg
);

}  // namespace iitasoccer
```

### `src/top/localization_runtime.h`

```cpp
#pragma once
#include "../shared/localization.h"

namespace iitasoccer {

// Calibra bno_offset leyendo el heading actual del BNO. El robot DEBE estar
// apuntando al arco rival (+Y de la cancha) cuando se llama.
void localization_runtime_init();

// Lee TOF + IMU, llama a localization_compute, cachea resultado. Llamar a
// ~30 Hz desde el loop del top.
void localization_runtime_tick();

// Devuelve el último pose calculado. Lock-free (single producer single consumer).
LocalizationPose localization_runtime_get_pose();

}  // namespace iitasoccer
```

## 4. Algoritmo de trilateración

### 4.1 Sistema de referencia

- **Origen**: esquina inferior-izquierda de la cancha (cuando se mira desde el
  arco propio hacia el rival).
- **Eje +X**: paralelo al lado largo de la cancha (243 cm), apuntando al lateral
  derecho.
- **Eje +Y**: paralelo al lado corto (182 cm), apuntando al arco rival.
- **θ = 0°**: robot apuntando a +Y (al arco rival).

### 4.2 Paso a paso

**Paso 1** — Obtener heading relativo a la cancha:
```
θ_world_centideg = in.bno_heading_centideg - cfg.bno_offset_centideg
θ_world = (θ_world_centideg / 100.0) mod 360
```

**Paso 2** — Para cada TOF `i` con `tof_valid[i]==true`, calcular su ángulo
de apuntamiento en el frame mundo:
```
θ_TOF_i = (θ_world + cfg.tof_mount_angle_deg[i]) mod 360
```

**Paso 3** — Clasificar a qué pared apunta cada TOF:
```
si θ_TOF_i ∈ [-45°, 45°)     → apunta a pared NORTE  (Y alta, y=field_height)
si θ_TOF_i ∈ [45°, 135°)     → apunta a pared OESTE  (X baja, x=0)
si θ_TOF_i ∈ [135°, 225°)    → apunta a pared SUR    (Y baja, y=0)
si θ_TOF_i ∈ [225°, 315°)    → apunta a pared ESTE   (X alta, x=field_width)
```

(Convención: +X derecha, +Y al frente. Por eso θ_TOF=90° apunta a la pared
izquierda del robot, que en frame mundo es la pared OESTE si el robot mira +Y.)

**Paso 4** — Para cada TOF que apunta a una pared, calcular la **componente
perpendicular** de la distancia (ya que el TOF no necesariamente apunta
perfectamente perpendicular a la pared cuando el robot está rotado un poco):

```
ángulo_relativo_a_pared = θ_TOF_i mod 90°  // distancia angular a "perpendicular"
si ángulo_relativo_a_pared > 45°:
    ángulo_relativo_a_pared = 90° - ángulo_relativo_a_pared
d_perpendicular_i = tof_distance_mm[i] * cos(ángulo_relativo_a_pared)
```

**Paso 5** — Estimar la coordenada del robot según pared:
```
si pared == NORTE: y_est_i = field_height_mm - d_perpendicular_i
si pared == SUR:   y_est_i = d_perpendicular_i
si pared == ESTE:  x_est_i = field_width_mm - d_perpendicular_i
si pared == OESTE: x_est_i = d_perpendicular_i
```

**Paso 6** — Promediar (después del outlier rejection del §5):
```
x_robot = mean(x_est de TOFs que apuntan a E o W, válidos y consistentes)
y_robot = mean(y_est de TOFs que apuntan a N o S, válidos y consistentes)
```

**Paso 7** — Armar el pose:
```
pose.x_mm = round(x_robot)
pose.y_mm = round(y_robot)
pose.heading_centideg = round(θ_world * 100)
pose.source_flags = bitmask de TOFs usados
pose.valid = (al menos 1 TOF útil para X) AND (al menos 1 TOF útil para Y)
```

### 4.3 Notas matemáticas

- `cos()` se computa en **lookup table** para evitar `<math.h>` pesado en
  Teensy. LUT de 91 entradas (0°-90° en pasos de 1°) en flash, ~360 B.
- Operaciones en `int32_t` para evitar overflow en la división por 100.
- Sin `float` en el path crítico (Teensy 4.0 tiene FPU, pero es buena
  costumbre).

## 5. Outlier rejection (Fase 2 implícita)

Implementado en `localization_compute` ANTES del promedio del Paso 6:

### 5.1 Descartes hechos en cada ciclo

1. **TOF inválido**: si `tof_valid[i]==false`, no se considera este ciclo.
2. **Lectura fuera de rango físico**: si `tof_distance_mm[i] > 4000` o
   `tof_distance_mm[i] < 10`, descartar. (Sensor satura o el objeto está
   más allá del rango fiable.)
3. **Lectura mayor que la cancha**: si `d_perpendicular > max(field_width, field_height)`,
   descartar. Esto pasa cuando un TOF "se sale" de la cancha por geometría
   (esquina + rotación + FoV de 60° → el cono apunta al aire del otro lado).
4. **Inconsistencia entre TOFs del mismo eje**: si 2 TOFs estiman X y difieren
   más que `cfg.outlier_threshold_mm` (default 300 mm = 30 cm), hay un
   obstáculo bloqueando uno. Se descarta el TOF cuya estimación sea más lejana
   del último pose válido cacheado en `localization_runtime`.

### 5.2 Qué NO se hace en Sprint 1

- **Filtros temporales** (Kalman, mediana móvil): tracking entre ciclos
  para suavizar ruido. → Sprint 2.
- **Análisis multizona 8×8** para distinguir "pared plana" vs "objeto":
  varianza de las 64 zonas indicaría obstáculo. → Sprint 2.
- **Recuperación de "kidnapped robot"** (pose perdida por mucho tiempo):
  reinicialización automática. → Sprint 2 o post-Incheon.

## 6. Calibración del norte de cancha (boot)

Proceso operativo:

1. **El equipo coloca el robot en cualquier posición de la cancha, pero
   apuntando al arco rival** (lado +Y según convención del §4.1).
2. **Se enciende el robot**. El setup del firmware llama a
   `localization_runtime_init()`.
3. `init()` espera 100 ms (estabilización del BNO), luego lee el heading
   actual y lo guarda como `bno_offset_centideg`.
4. A partir de ese momento, `θ_world = bno_heading - bno_offset` = 0° cuando
   el robot apunta al arco rival.

**Riesgo operativo**: si el equipo enciende el robot sin orientarlo bien, toda
la localización del partido estará desfasada. **Mitigación**: agregar print
muy visible en el banner del firmware indicando la operativa, y un LED de
estado que parpadee durante los primeros 5 segundos para que el equipo
verifique antes de soltar el robot.

**Mejora post-Incheon** (no Sprint 1): botón físico de "calibrar ahora" que
permite recalibrar sin power-cycle.

## 7. Plan de testing

### 7.1 Tests host-native (`pio test -e test_native -f test_localization`)

Suite Unity con escenarios sintéticos. NO requiere hardware. Se corre antes
de tocar el robot. Cobertura mínima:

**Escenarios estáticos** (robot quieto en posiciones conocidas):
- `test_robot_en_centro_apunta_arco_rival_devuelve_centro`
- `test_robot_en_esquina_propia_apunta_arco_rival`
- `test_robot_en_esquina_rival_apunta_arco_rival`
- `test_robot_pegado_a_pared_lateral_apunta_arco_rival`
- `test_robot_en_centro_apunta_arco_propio` (rotado 180°)
- `test_robot_en_centro_apunta_lateral_derecho` (rotado 90°)
- `test_robot_en_centro_apunta_lateral_izquierdo` (rotado -90°)

**Outlier rejection**:
- `test_un_tof_invalid_pose_calculada_con_3_restantes`
- `test_dos_tofs_invalid_misma_pared_pose_valid_falso_si_eje_sin_dato`
- `test_tof_bloqueado_por_robot_rival_descartado_por_inconsistencia`
- `test_tof_satura_descartado_por_rango`
- `test_tof_devuelve_mas_que_cancha_descartado`

**Casos límite**:
- `test_rotacion_exacta_en_45_grados_clasificacion_estable` (borde entre dos
  paredes)
- `test_todos_tofs_invalidos_pose_valid_falso`
- `test_inconsistencia_extrema_entre_tofs_X_descarta_el_mas_lejano_del_anterior`

### 7.2 Compile gate

Después de cada commit:
```powershell
pio run -e top                       # firmware vivo compila con localization integrado
pio run -e test_native               # tests compilan
pio test -e test_native -f test_localization  # tests pasan
pio run -e diag_top_tof_adafruit     # regresión, no se rompe
pio run -e diag_sensors_tof_live     # regresión
pio run -e diag_down                 # regresión
pio run -e central_robot1            # regresión
pio run -e central_robot2            # regresión
```

Todos SUCCESS / PASS.

### 7.3 Validación hardware (post-bodge XSHUT)

Nuevo env `[env:diag_localization_live]` (parte del Sprint 1, último commit):
- Sketch standalone que llama a `localization_runtime_*()` y printea pose
  cada 500 ms por Serial.
- Equipo humano: poner el robot en posiciones conocidas de la cancha y
  comparar el pose printeado con la posición real medida con metro.

**Criterios de aceptación medibles** (los probará el equipo, no Claude):
- Robot en centro de la cancha → pose printeada `(1215±30, 910±30, 0±2°)`.
- Robot en cada esquina (4 esquinas) → pose printeada matchea esquina ±30 mm.
- Robot rotado 90° en centro → heading printeado `9000±200 centideg`.
- Robot con obstáculo a 30 cm al frente (alguien pone la mano) → la pose X/Y
  no cambia (TOF frontal descartado por inconsistencia), `source_flags`
  refleja que solo se usan 3 TOFs.

## 8. Integración al firmware vivo

### 8.1 Cambios en `main_top.cpp`

En `setup()`:
```cpp
sensors_imu_init();
sensors_tof_init();
delay(100);              // estabilización del BNO
localization_runtime_init();   // ← NUEVO. Calibra bno_offset.
```

En `loop()`:
```cpp
// Existente:
sensors_imu_tick();
sensors_tof_tick();
// ...

// Nuevo: localización corre a 30 Hz, mismo que TOFs.
static uint32_t last_loc = 0;
if (millis() - last_loc >= 33) {
    last_loc = millis();
    localization_runtime_tick();
}
```

### 8.2 Cambios en armado del `WorldSnapshot v2`

Si el WorldSnapshot v2 ya tiene campos `x_mm`/`y_mm`/`heading_centideg`
(según el CONTRATO-DATOS-TOP.md), simplemente leemos:

```cpp
LocalizationPose pose = localization_runtime_get_pose();
snapshot.robot_x_mm = pose.x_mm;
snapshot.robot_y_mm = pose.y_mm;
snapshot.robot_heading_centideg = pose.heading_centideg;
snapshot.flags |= pose.valid ? SNAPSHOT_FLAG_POSE_VALID : 0;
```

Si NO tiene los campos: extender el WorldSnapshot (rev menor del protocolo).
Esto requiere coordinación con CENTRAL — **fuera de scope de este spec**.
Plan: si los campos faltan, **dejamos en TODO** y entregamos el módulo
funcional. CENTRAL no lo usa hasta que el contrato se actualice.

## 9. Boundary — qué NO se modifica

- ❌ `src/top/sensors_tof.cpp` no se toca (ya migrado a Adafruit en sesión
  previa).
- ❌ `src/top/sensors_imu.cpp` no se toca.
- ❌ `src/top/cameras_runtime.cpp` no se toca.
- ❌ `src/top/comm_central.cpp` no se toca **excepto** para conectar el pose
  al WorldSnapshot (cambio menor, si el contrato lo soporta).
- ❌ No se modifica nada del CENTRAL.
- ❌ No se modifica nada del DOWN.
- ❌ No se crea ningún sketch nuevo de competencia. El env `[env:diag_localization_live]`
  es solo diagnóstico de banco.
- ❌ No se marcan TASKs de hardware como `done` (CLAUDE.md regla 1).

## 10. Riesgos identificados

| # | Riesgo | Prob | Mitigación |
|---|---|---|---|
| R1 | Bodge XSHUT no se completa antes de Incheon → solo 1 TOF por bus → solo 2 TOFs útiles | Media | Lógica funciona con cualquier número ≥2 de TOFs útiles. Con 2 TOFs (uno por bus), pose.valid se calcula si uno mira a eje X y otro a eje Y. Precisión degradada (~±5 cm) pero usable. |
| R2 | BNO055 deriva durante el partido (gyro drift) → heading se desvía → toda la pose se desvía | Media | Recalibrar entre partidos (apagar/encender). Mejora post-Sprint 1: usar magnetómetro del BNO para corrección absoluta (requiere calibración previa del magnetómetro). |
| R3 | TOF lee la pelota (improbable por geometría, pero un robot rival podría tener la pelota encima) | Baja | El outlier rejection por inconsistencia (§5.1.4) lo agarra: si la lectura es mucho más cercana que la esperada, se descarta. |
| R4 | Robot en esquina rotado de cierta forma → 2 TOFs adyacentes apuntan al aire por FoV 60° | Media | `localization_compute` retorna `pose.valid=false`. El consumidor (FSM en CENTRAL) maneja esto manteniendo el último pose válido o pidiendo retreat. |
| R5 | Operativa: equipo enciende robot mal orientado → toda la sesión con offset incorrecto | Alta | LED parpadea 5 s al boot para que el equipo verifique antes de soltar. Documentado en operativa + journal post-Incheon. |
| R6 | Múltiples LUT de cos() en flash dispersos → desperdicio | Baja | Centralizar LUT en `src/shared/lut_trig.h` reusable por otros módulos. |

## 11. Tiempo y prioridad

- **Trabajo Claude (con subagent TDD):**
  - Spec + plan formal: 1 día (ya estamos)
  - Implementación shared/localization + tests: 2 días
  - Implementación top/localization_runtime + integración main_top: 1 día
  - Env diag_localization_live + ajustes: 0.5 día
  - **Total Claude: ~4-5 días.**
- **Trabajo equipo humano (post-bodge XSHUT):**
  - Validación en hardware con metro: 1-2 h.
- **Prioridad:** **P1** (alto impacto en juego, no bloqueante absoluto). La FSM
  ya juega con visión sin pose absoluta — agregar pose la mejora pero no
  desbloquea funcionalidad nueva por sí sola.

## 12. Próximos pasos post-Sprint 1

(Fuera del scope de este spec, pero documentado para no perderlo.)

1. **Sprint 2 — LUT matching multizona 8×8** (Alt 5 v2 del análisis). Mejora
   precisión a ±1 cm y robustez muy alta. Spec aparte cuando se decida arrancar.
2. **Filtros temporales** (Kalman simple o mediana móvil de 5 ciclos) para
   suavizar el pose entre ciclos. Spec aparte.
3. **Integración con visión** para fusión sensor: si TOFs y cámara dan pose
   distinto, decidir cuál creer. Probablemente fuera de Incheon.
4. **Botón físico de "calibrar ahora"** (requiere hardware nuevo). Post-Incheon.
5. **Calibración del magnetómetro BNO** para corrección absoluta de drift.
   Sprint independiente.

## 13. Atribución y referencias

- **Análisis previo:** `research/in-progress/2026-05-25-localizacion-tof-imu-analisis.md`
  (5 alternativas evaluadas, recomendación coach).
- **Hardware base:** `hardware/electronics/top-board-pack/`.
- **Sensores migrados a Adafruit:** `journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md`.
- **Hallazgo XSHUT (bloqueante para los 4 TOFs):** `journal/2026-05-25-top-xshut-no-routed-finding.md`.
- **Contrato de datos al CENTRAL:** `docs/firmware/CONTRATO-DATOS-TOP.md`.
- **Política de testing host-native:** `software/teensy/Soccer 2026/test/` + `lib/Unity/`.

## 14. Commits + atribución

Cada commit del Sprint 1 lleva:
```
Author: Claude Opus 4.7 (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)
```

(Convención del repo según `AI-INSTRUCTIONS.md`.)
