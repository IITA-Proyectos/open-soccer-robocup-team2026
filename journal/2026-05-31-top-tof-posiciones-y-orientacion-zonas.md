---
title: "TOP — Mapeo de posición de los 4 ToF + fix de ángulos + plan de orientación de zonas (lidar-360)"
date: 2026-05-31
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8, Anthropic)"
status: final
tags: [top-board, tof, vl53l7cx, localizacion, zonas, lidar, mapeo, hardware-test]
robot: top (R1 montado)
area: percepcion
tipo: hardware-test + fix
---

# TOP — Posición de los 4 ToF, fix de ángulos, y orientación de zonas

> **TL;DR.** Gustavo confirmó en banco la posición física de los 4 ToF:
> **TOF0=FRENTE, TOF1=ATRÁS, TOF2=DERECHA, TOF3=IZQUIERDA** (LP 9/10/11/12 →
> dir 0x2A/0x2B/0x2C/0x2D). Al documentarlo encontré un **bug**: el array
> `TOF_MOUNT_ANGLE_DEG` era `{0,180,90,270}`, que en `localization.cpp` asigna
> el índice 2 a IZQUIERDA y el 3 a DERECHA — **cruzado** respecto al hardware.
> Lo corregí a `{0,180,270,90}`. Queda un tema nuevo a verificar: la
> **orientación interna de las zonas** de cada sensor (el izquierdo es de otro
> fabricante y se montó mirando hacia abajo → puede tener arriba/abajo o
> izq/der de su grilla invertidos). Para eso hice `diag_top_tof_zonemap`. El
> objetivo final es un **barrido tipo lidar-360** combinando los 4 sensores.

## 1. Posición física confirmada (banco, Gustavo)

| Índice firmware | LP pin | Dir I²C | Posición | Ángulo de montaje |
|---|---|---|---|---|
| TOF[0] | 9  | 0x2A | **FRENTE**     | 0°   (mira +Y, arco rival) |
| TOF[1] | 10 | 0x2B | **ATRÁS**      | 180° (mira -Y) |
| TOF[2] | 11 | 0x2C | **DERECHA**    | 270° (mira +X, pared EAST) |
| TOF[3] | 12 | 0x2D | **IZQUIERDA**  | 90°  (mira -X, pared WEST) |

Documentado en `pinout_robot1.h`/`pinout_robot2.h` (mapeo pin→posición) y
`pinout_common.h` (`TOF_MOUNT_ANGLE_DEG`).

## 2. Bug encontrado y corregido — ángulos cruzados (P1)

**Categoría:** percepción / localización · **Robot afectado:** ambos · **Prioridad: P1**

**Qué observé.** `pinout_common.h` tenía:
```cpp
TOF_MOUNT_ANGLE_DEG[4] = { 0, 180, 90, 270 };   // [TOF0,TOF1,TOF2,TOF3]
```
La función `classify_wall()` de `localization.cpp` computa
`world_angle = heading + mount_angle` (heading 0 = robot mira al arco rival +Y)
y clasifica: `[45,135)→WEST(-X)`, `[225,315)→EAST(+X)`. O sea **90° = izquierda
(WEST), 270° = derecha (EAST)**.

Con el array viejo, el firmware creía: índice 2 → 90° → izquierda, índice 3 →
270° → derecha. Pero el hardware real es **TOF2 = derecha, TOF3 = izquierda**.
Resultado: la localización tomaba la distancia del sensor **derecho** y la
atribuía a la pared **izquierda** (y viceversa). Con el robot descentrado en X,
la pose X salía espejada.

**Fix aplicado.** `TOF_MOUNT_ANGLE_DEG = { 0, 180, 270, 90 }` — ahora el índice
2 (derecha) mapea a 270°/EAST y el índice 3 (izquierda) a 90°/WEST, coincidiendo
con el hardware. Compila limpio ambos robots. Tests host-native: sin cambio (el
array es config, no lógica).

**Risk-no-fix.** Localización con X espejada → el robot cree que está del lado
opuesto de la cancha en el eje X. **Risk-fix.** Nulo (alinea config con HW real;
si acaso, expone que faltaba validar). **Tiempo:** 0 (aplicado).

**Plan de prueba en hardware.** Con `diag_top_tof_quad_live` + el robot a una
distancia conocida de la pared derecha, confirmar que la lectura de TOF2 (0x2C)
baja al acercarse a la **derecha**, no a la izquierda. (Validación de pose
completa: TASK-035.)

## 3. Tema a verificar — orientación interna de las zonas (P1 para lidar-360)

**Categoría:** percepción · **Robot afectado:** ambos (sobre todo R1) · **Prioridad: P1** (para lidar-360; P2 para el promedio actual)

**Qué observo.** Cada VL53L7CX entrega una grilla 8×8 (64 zonas). El orden de
esas zonas en la memoria depende de **cómo está montado el chip** (rotación,
espejado). El **ToF IZQUIERDO (TOF3) es de otro fabricante** y se montó
**mirando hacia abajo** — es muy posible que tenga invertido arriba/abajo y/o
izquierda/derecha de su grilla respecto a los otros 3.

**Por qué importa.**
- Para el **promedio de zonas actual** (`mean_valid_zones`) NO importa: se
  promedian todas las zonas válidas, el orden da igual.
- Para un **barrido tipo lidar-360** SÍ importa: ahí se mapea cada
  columna/zona de cada sensor a un ángulo real alrededor del robot. Si un
  sensor tiene las columnas espejadas, su sector del "lidar" queda invertido y
  el mapa 360° sale roto en ese cuadrante.

**Herramienta creada.** `diag_top_tof_zonemap` (nuevo): imprime la grilla 8×8
**cruda** (sin reordenar) de cada ToF, marca la zona más cercana con `[###]`, y
acepta comandos serie (`0/1/2/3` para un sensor, `a` para los 4). Procedimiento:
poner un objeto en una posición conocida del campo de visión de un sensor
(arriba/abajo/izq/der) y anotar qué fila/columna se enciende. Repetir por
sensor → se deduce la transformación (flip-H / flip-V / rotación) de cada uno.

**Plan de prueba en hardware (pendiente, equipo).**
1. Flashear `diag_top_tof_zonemap`, power-cycle, abrir monitor.
2. Por cada sensor: mover la mano arriba → ver qué fila se marca; abajo, izq,
   der → idem. Anotar la tabla fila/col → posición real para los 4.
3. Comparar TOF3 (izquierdo) contra los otros 3: identificar si está
   flippeado/rotado.
4. Con esas 4 transformaciones, definir el remapeo de zonas → ángulo para el
   barrido lidar-360 (Sprint futuro).

**Risk-no-fix.** El barrido lidar-360 quedaría con un cuadrante espejado
(el izquierdo). Para Incheon, si solo se usa el promedio por sensor, no
bloquea. **Tiempo estimado.** 1-2 h de banco + el remapeo en firmware cuando se
implemente el lidar-360.

## 4. Estado y próximos pasos

- ✅ Posición de los 4 ToF documentada (firmware + docs).
- ✅ Bug de ángulos cruzados corregido.
- ⏳ **Equipo**: correr `diag_top_tof_zonemap` y anotar la orientación de zonas
  de los 4 (especialmente el izquierdo).
- ⏳ **Firmware (HAL Sprint B)**: `sensors_tof.cpp` todavía lee 1 ToF; extender
  a los 4 con enumeración al boot. Recién ahí el lidar-360 tiene sentido.

## Archivos

- `src/top/pinout_common.h` — `TOF_MOUNT_ANGLE_DEG` corregido + convención.
- `src/top/pinout_robot1.h` / `pinout_robot2.h` — mapeo pin→posición confirmado.
- `src/diag/diag_top_tof_zonemap.cpp` — **nuevo**, mapeo de orientación de zonas.
- `platformio.ini` — env `[env:diag_top_tof_zonemap]`.
- Specs `2026-05-24-diag-top-tof` y `2026-05-25-localization-sprint1` — banners.
