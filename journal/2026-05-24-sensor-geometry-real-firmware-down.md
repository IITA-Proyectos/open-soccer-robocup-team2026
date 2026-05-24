---
title: "2026-05-24 — Geometría REAL del PCB en firmware DOWN: SENSOR_POS[32] + lg_compute_xy"
date: 2026-05-24
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [firmware, down-board, sensor-geometry, line-geometry, tests, hito]
robot: ambos
area: electronica
tipo: firmware-change
related-journals: [2026-05-24-down-board-passing-tests-cierre.md]
related-tasks: [TASK-029, TASK-027]
---

# Geometría REAL del PCB en firmware DOWN

> **TL;DR.** Gap del subsistema DOWN cerrado: el firmware ahora tiene
> `SENSOR_POS[32]` con coordenadas (x, y) reales del PCB extraídas del
> doc canónico §5b, y una versión cartesiana de `lg_compute_xy()` que
> usa esas posiciones para el centroide de línea. La función vieja
> `lg_compute()` queda intacta — backward-compat total. **20/20 tests
> host-native pasan**, `pio run -e down` y `pio run -e diag_down` compilan
> limpios.

## Contexto

El usuario pidió "elaborar el programa completo para la placa DOWN,
tomando como base lo que ya está, actualizando con pines correctos
(ya hecho hoy) y con **ubicación REAL de sensores**". El gap principal
era que `line_geometry` aceptaba un array de ángulos pero nadie le
pasaba los ángulos REALES del PCB — `lg_sensor_angle_deg(i, n)` calcula
un anillo "ideal" equidistante que NO refleja la geometría del PCB DOWN
(que tiene 3 anillos con R ≈ 37, 54, 80-87 mm + asimetrías).

## Qué se hizo

### Archivos nuevos
- **`src/shared/sensor_geometry.h`** (~75 líneas) — declara `SENSOR_POS[32]`,
  constantes de anillos lógicos (externo frontal/izq/der + interno), y
  helpers `sg_angle_deg(i)`, `sg_radius_mm(i)`, `sg_fill_angles_deg(out, n)`.
- **`src/shared/sensor_geometry.cpp`** (~75 líneas) — LUT con coords
  (x, y) en mm de los 32 sensores, copiada literalmente del doc canónico
  `hardware/electronics/down-board-pack/01-pinout-y-posiciones.md` §5b.
  Sistema de referencia: origen = centro del PCB, +X = derecha del robot,
  +Y = adelante.

### Archivos modificados
- **`src/shared/line_geometry.h`**: forward-decl de `SensorPos2D` + nueva
  declaración `lg_compute_xy(white, pos, n)`. La declaración vieja
  `lg_compute(white, ang, n)` queda intacta.
- **`src/shared/line_geometry.cpp`**: implementación de `lg_compute_xy()`
  que computa centroide **cartesiano** (suma de vectores xy, atan2 al
  final). Más correcto que el angular cuando los sensores no son
  equidistantes del centro, porque ahora cada sensor pesa según su
  vector real, no como un unitario en su dirección.
- **`test/test_down_geometry/test_main.cpp`**: agregados 10 tests nuevos
  cubriendo `sg_angle_deg`, `sg_radius_mm`, `sg_fill_angles_deg`,
  defensa contra índice fuera de rango, y 4 escenarios de `lg_compute_xy`
  (sin blancos, frontales, mux U3 entero blanco, todos blancos).
- **`platformio.ini`**: fix incidental — el filter de `[env:diag_down]`
  era `+<diag/>` que ahora agarra también `diag_top_tof.cpp` (agregado
  por otra sesión hoy) y daba multiple-definition de `setup()`/`loop()`.
  Cambiado a `+<diag/main_diag_down.cpp>` explícito.

### Snapshots del pack sincronizados
- `down-board-pack/firmware/shared/sensor_geometry.{h,cpp}` (nuevos).
- `down-board-pack/firmware/shared/line_geometry.{h,cpp}` (actualizados).

## Qué NO se tocó (intencional)

- ❌ La deuda viva `line_ring` + `DownModel` en paralelo (regla CLAUDE.md:
  no archivar antes de Incheon).
- ❌ Llamadores de `lg_compute()` actuales (no migrados a `lg_compute_xy()`).
  Decisión: mantener backward-compat. El llamador que quiera la
  geometría real ahora tiene la opción de elegir entre:
  - `lg_compute(white, sg_fill_angles_deg(out), n)` — ángulos reales,
    centroide angular (igual API que antes pero mejor data).
  - `lg_compute_xy(white, SENSOR_POS, n)` — centroide cartesiano
    completo, geometría 100% real.
- ❌ Detección de corner en `lg_compute_xy()`: NO implementada todavía.
  Para corner detection, el llamador puede usar `lg_compute()` con
  `sg_fill_angles_deg()` en paralelo. Si se necesita corner cartesiano
  100%, se puede agregar en sesión futura.

## Validación

```
$ pio run -e down
  FLASH: code:30004, data:6328, headers:8720   free for files:1986564
  RAM1:  variables:8096, code:27824, padding:4944  free for local variables:483424
  ========================= [SUCCESS] Took 15.40 seconds =========================

$ pio run -e diag_down
  ========================= [SUCCESS] Took 10.17 seconds =========================

$ pio test -e test_native -f test_down_geometry
  test_sg_angle_front_sensor       PASSED
  test_sg_angle_right_sensor       PASSED
  test_sg_radius_outer             PASSED
  test_sg_radius_inner             PASSED
  test_sg_fill_angles              PASSED
  test_sg_out_of_range_safe        PASSED
  test_lg_compute_xy_no_white      PASSED
  test_lg_compute_xy_front_sensors PASSED
  test_lg_compute_xy_right_back_sensors  PASSED
  test_lg_compute_xy_all_white     PASSED
  (+ 10 tests existentes — todos siguen pasando)
  ============= 20 test cases: 20 succeeded in 00:00:02.976 ==================
```

## Próximos pasos (post-Incheon o cuando sea seguro tocar el cerebro)

1. **Migrar `DownModel::dm_update()`** a usar `lg_compute_xy()` con
   `SENSOR_POS[]` para que el `LineStatusV2` que va al CENTRAL tenga
   ángulo de línea calculado con geometría real. Hoy NO se hizo para
   no cambiar comportamiento del fail-safe sin test en cancha.
2. Implementar corner detection cartesiano en `lg_compute_xy()` si se
   detecta que la versión angular tiene falsos negativos en cancha.

## Atribución

- Pedido + decisiones de alcance — Gustavo Viollaz (@gviollaz).
- Implementación, tests, build verification — Claude Opus 4.7 (Anthropic),
  sesión 2026-05-24.
