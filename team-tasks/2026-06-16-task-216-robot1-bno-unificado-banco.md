---
id: TASK-216
title: "Banco: validar robot1 con el firmware BNO UNIFICADO (2 BNO @0x28, buses separados) — antes top_robot1 buscaba el 2º en 0x29"
date_created: 2026-06-16
assigned: [equipo (firmware TOP, robot1)]
priority: P2  # robot1 hoy corre SIN gyro (BNOs desconectados); sube a P1 si R1 va a jugar con heading
pedido-por: Gustavo Viollaz (2026-06-15: "los BNO quedaron en 0x28 en los dos robots")
status: firmware-LISTO-2026-06-16  # unificado + envs R1 creados (top_robot1_pri/_fastbno/_xval, SIN deconflict → mismo path de centinela que R2) + TODOS compilan. R2 validó el diseño con HW idéntico (TASK-213). FALTA SOLO el boot-check físico de R1 (BNOs estaban desconectados). Claude NO cierra HW (regla 1).
relacionada: TASK-207 (BNO bus aparte Wire2), TASK-213 (centinela R2), TASK-042 (checklist R1 vuelta de reparación)
tags: [firmware, top, bno055, robot1, i2c, hardware-real]
depends_on: []
---

# TASK-216 — Validar robot1 con el firmware BNO unificado

> 🔄 **ACTUALIZACIÓN 2026-06-17 (Gustavo):** los 2 BNO de R1 están reconectados y funcionando.
> Falta el boot-check / validación de comportamiento con gyro en banco. Sigue **ABIERTA** para esa validación.

> 🟡 **ACTUALIZACIÓN 2026-06-16 — R1 firmware-LISTO, falta solo el boot-check físico.**
> Tras verificar el centinela en **R2** (TASK-213), Gustavo pidió dejar el MISMO programa para R1.
> Hecho en firmware (todo compila):
> - `Wire2.begin()` ahora es **incondicional** (antes gateado a ROBOT2/TOP_BNO1_ON_WIRE2): sin esto,
>   el primario de R1 en Wire2 no arrancaba con `top_robot1` plano.
> - El **centinela se des-gateó de `ROBOT2`** (R1 ya es idéntico): ahora compila/corre para ambos.
> - **Envs nuevos de R1** (espejo de R2, SIN `-DTOP_BNO_TOF_DECONFLICT` obsoleto → R1 toma el path del
>   centinela igual que R2): `top_robot1_pri`, `top_robot1_pri_fastbno` (⭐ competencia), `top_robot1_pri_xval` (banco). Los 3 compilan SUCCESS.
>
> **FALTA (equipo, regla 1 — Claude NO lo cierra):** reconectar los 2 BNO de R1 + flashear
> `top_robot1_pri_xval` + giro → confirmar `PRIMARIO OK` + `CENTINELA init OK` + heading que sigue
> el giro + monitor mostrando "centinela @1Hz". **Riesgo bajo: R2 ya validó el diseño con HW idéntico.**

## Por qué existe

Gustavo confirmó (2026-06-15, banco) que **en los DOS robots los 2 BNO055 quedaron en
0x28**, en **buses separados**: uno solo en Wire2 (24/25, sin ToF) y otro en Wire (18/19,
con los 4 ToF). El esquema viejo de robot1 — un 2º BNO en **0x29** por ADR puenteado a 3V3,
en el mismo bus Wire — **fue un ERROR, ya corregido en hardware**.

El firmware de **robot2** ya estaba así (banco 2026-06-09). El de **robot1** todavía buscaba
el 2º BNO en 0x29 (con sondeo de chip-id + guarda anti-cuelgue). El 2026-06-15 se **unificó
robot1 a la misma arquitectura de robot2** (mismo path probado, misma guarda anti-cuelgue
ahora en 0x28). **Compila** (`pio run -e top_robot1` → SUCCESS), pero es un cambio de
comportamiento en un robot que compite → **falta validación de banco** (regla 1: Claude no
la cierra).

## Qué cambió en el firmware (commit de la corrección 2026-06-15)

- `src/top/sensors_imu.cpp` — se eliminó la rama `#else` de robot1 (sondeo 0x29 +
  `i2c_present`/`read_reg`); robot1 usa ahora el mismo bloque que robot2: PRIMARIO en
  Wire2 @0x28 (idx0) + SECUNDARIO en Wire @0x28 (idx1), cada uno con guarda de ACK en SU bus.
- `src/top/pinout_common.h` — `BNO055_RIGHT_I2C_ADDR` ahora es alias de 0x28 (ya no 0x29).
- `src/top/sensors_imu.h`, `main_top.cpp`, `sensors_tof.{h,cpp}` — comentarios corregidos.
- Diags `diag_bno_dual_live` (lee Wire + **Wire2**, ambos 0x28) y `diag_bno_addr_check`
  (banner de obsolescencia) actualizados.
- El env `top_robot1*` ya NO es "cableado viejo": sirve para el R1 recableado (igual que
  `top_robot2_pri`). **Decisión pendiente del equipo:** ¿consolidar R1 en `top_robot2_pri`
  o seguir con `top_robot1`? (hoy ambos quedan equivalentes para el HW nuevo).

## Plan de prueba en hardware real (robot1)

1. **Reconectar los 2 BNO de R1** (hoy están desconectados — ver ESTADO-ACTUAL).
2. `pio run -e diag_bno_dual_live -t upload` + `pio device monitor` →
   - **LEFT** (Wire @0x28, secundario) debe leer y seguir el giro.
   - **RIGHT** (Wire2 @0x28, primario) debe leer y seguir el giro.
   - Si el 2º BNO de R1 sigue MUERTO (unidad quemada histórica), el primario solo debe andar.
3. `pio run -e top_robot1 -t upload` + monitor del boot → confirmar las líneas
   `[IMU] Init BNO PRIMARIO (Wire2 24/25 @ 0x28...)` y `[IMU] Init BNO SECUNDARIO (Wire ...)`.
4. Giro 90° a izquierda → el heading sube ~+90 (convención CCW). Heading estable en reposo.
5. ToF 4/4 enumeran igual (el cambio de BNO no debe afectar la enumeración).

## Criterio de cierre

- R1 booteando con el firmware unificado, primario (Wire2) leyendo y siguiendo el giro,
  sin colgar el bus, ToF 4/4. Resultado en journal. **Solo el equipo cierra esto.**

## Riesgos (formato coach)

- `risk-no-fix`: el firmware de R1 quedaría describiendo/buscando un 2º BNO en 0x29 que ya no
  existe → confusión (ya pasó: `diag_bno_dual_live` dio un falso `R --` el 2026-06-15) y, peor,
  init equivocado si alguien reactiva esa rama.
- `risk-fix`: el path unificado cambia dónde busca R1 sus BNO. Si el cableado físico de R1 NO
  fuera exactamente el de robot2 (primario en Wire2), el primario no aparecería. Mitigación:
  la guarda de ACK por bus evita cuelgues (degrada con gracia a 1 o 0 BNO), y el paso 2/3 del
  plan lo detecta antes de confiar.
- `tiempo`: ~30–45 min de banco (reconectar BNO + flashear diag + giro).

## Atribución

Unificación de firmware + esta TASK: Claude Opus 4.8 (Anthropic), 2026-06-15 (requested-by
Gustavo Viollaz). Validación en banco = equipo humano (regla 1).
