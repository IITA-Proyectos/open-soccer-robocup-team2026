# 2026-06-16 — Corrección global: los 2 BNO de cada robot están en 0x28 (no 0x29). Unificación de robot1.

## Disparador (banco en vivo con Gustavo)

Gustavo estaba por testear el centinela dual-BNO de robot2 (TASK-213). Como pre-flight le
mandé flashear `diag_bno_dual_live`. La salida dio:

```
L hdg=-7.6 gZ=0 c3 | R --
```

`L` (0x28) leía perfecto; `R --` = el 2º BNO "no aparece". **Falsa alarma:** el diag estaba
escrito para el cableado viejo (los 2 BNO en el mismo bus Wire, uno puenteado a **0x29**).
Robot2 hacía rato (banco 2026-06-09) que tenía sus 2 BNO en **0x28 en buses separados**
(primario Wire2, secundario Wire). El diag buscaba un 0x29 que en robot2 ya no existe.

Gustavo lo confirmó y corrigió el dato de raíz:

> "en los dos robots los BNO quedaron en 0x28, tanto el que está en el I2C de los ToF como el
> que está solo en el otro puerto I2C. Actualizá en todos los programas y documentos."

## La realidad canónica (ambos robots, ya corregido en hardware)

- 2× BNO055, **ambos en I2C 0x28**, en **buses separados**:
  - **PRIMARIO** → Wire2 (24/25), solo, sin ToF → sin contención.
  - **SECUNDARIO** → Wire (18/19), junto a los 4 ToF.
- **NO hay ningún BNO en 0x29.** El esquema viejo (2º BNO con ADR puenteado a 3V3 → 0x29, o
  "los 2 en el mismo bus Wire") fue un ERROR. 0x29 queda solo como dir de fábrica de los ToF
  VL53L7CX (se reasignan a 0x2A..0x2D al enumerar).

## Lo que hice (y verifiqué compilando — pio SÍ está en este entorno)

**Mito derribado:** mi nota de memoria decía que no podía compilar firmware Teensy. Falso —
`pio` (Core 6.1.19) está instalado. Compilé todo yo. (Antes en esta misma sesión, compilando,
ya había cazado un bug real: `TOP_BNO_TOF_GAP_MS` no declarado en el env del centinela.)

### Firmware (gateado donde corresponde, compila):
- `src/top/sensors_imu.cpp` — **robot1 unificado a la arquitectura de robot2**: se eliminó la
  rama `#else` con el sondeo 0x29 (+ `i2c_present`/`read_reg`); robot1 usa el mismo path
  probado (PRIMARIO Wire2 @0x28 / SECUNDARIO Wire @0x28, guarda de ACK por bus). `g_bno_left/right`
  → `g_bno_primary/secondary`.
- `src/top/pinout_common.h` — `BNO055_RIGHT_I2C_ADDR` ahora alias de 0x28 (ya no 0x29) + comentario.
- `src/top/sensors_imu.h`, `main_top.cpp`, `sensors_tof.{h,cpp}` — comentarios de arquitectura corregidos.
- `src/diag/diag_bno_dual_live.cpp` — ahora lee **Wire + Wire2** (ambos 0x28): el diag que dio el
  falso `R --` queda útil de verdad para el HW actual.
- `src/diag/diag_bno_addr_check.cpp` — banner de obsolescencia (su premisa era el puente 0x29).
- `src/diag/diag_bno_left.cpp` — nota.

**Compilaciones verificadas (todas SUCCESS):** `top_robot1`, `top_robot2_pri` (competencia,
byte-identidad respaldada), `top_robot2_pri_xval` (banco), `top_robot2_pri_posefusion`,
`top_robot{1,2_pri}_bnofreeze`, `central_robot2_strafe_slew_bb`,
`central_robot2_arquero_strafe_cam_ratedamp`, `diag_bno_dual_live`, `diag_bno_addr_check`.

### Docs (barrido en workflow paralelo, sin tocar journals):
- ~28 docs de competencia/firmware/robot-variants/pruebas-banco/hardware/skills/research barridos
  para corregir afirmaciones BNO-0x29 (respetando el 0x29 de fábrica de los ToF). Ver resultado del
  workflow en el commit.

### Canónicos:
- `docs/ESTADO-ACTUAL.md` — corregido "BNO-R (0x29) muerto" → secundario en 0x28; y "envs top_robot1*
  para cableado viejo" → unificados al cableado nuevo.
- `docs/FUENTES-DE-VERDAD.md` — entrada del BNO precisada + puntero canónico a `sensors_imu.h`/`pinout_common.h`.
- `team-tasks/2026-06-16-task-216-robot1-bno-unificado-banco.md` — banco de R1 (Claude NO lo cierra).
- Memoria `host-build-toolchain` corregida (pio compila; solo falta `-t upload` que es del equipo).

## Lo que NO puedo cerrar (regla 1)

**robot1** con firmware unificado **necesita banco** (reconectar sus BNO, flashear, confirmar que
el primario en Wire2 lee y sigue el giro). Lo escribí y compila, pero es un robot que compite →
**TASK-216** lo valida el equipo. robot2 no se ve afectado (su código ya era correcto).

## Pendiente flagged
- `hardware/electronics/top-board-pack/firmware/` tiene un **snapshot** de firmware (config_top.h,
  main_top.cpp, sensors_imu.cpp) con el dato viejo. No lo toqué (¿es snapshot congelado del pack o
  debe trackear?). Decisión del equipo (anotado en TASK-216).

## Atribución
Diagnóstico + corrección + unificación + esta entrada: Claude Opus 4.8 (Anthropic), requested-by
Gustavo Viollaz, 2026-06-15/16.
