---
task: TASK-217
title: "Subir el firmware COMPLETO (RT) al TOP de ROBOT1 cuando esté a mano"
fecha: 2026-06-16
asignado: equipo (Gustavo / Elías) — REQUIERE R1 a mano
prioridad: P1
estado: pendiente (bloqueado por: R1 no disponible al 2026-06-16)
robot: robot1
related: [journal/2026-06-16-top-competencia-firmware-completo-rt.md, platformio.ini, src/top/sensors_imu.cpp]
---

# TASK-217 — Firmware completo (RT) al TOP de ROBOT1

> 🔄 **ACTUALIZACIÓN 2026-06-17 (Gustavo):** R1 ya tiene BNO sano (los 2 reconectados) → los flags
> de BNO (`TOP_BNO_FAST`, freeze-detect, centinela) ahora **SON válidos para R1**. Falta flashear
> el firmware completo RT y validar en banco. Sigue **ABIERTA**.

## Contexto
El 2026-06-16 el TOP de **ROBOT2** pasó a correr el firmware COMPLETO/rápido (env
`top_robot2_pri` ahora trae todas las mejoras RT; `top_robot2_pri_anterior` = fallback). R1
quedó **pendiente** porque no estaba a mano. Esta task es hacer el equivalente para R1.

## ⚠️ NO es copiar los flags — R1 es DISTINTO
- R1 hoy **NO tiene un BNO sano** (heading N/A; corre OTOS-based). Por eso los flags que dependen
  del BNO **NO aplican igual**: `TOP_BNO_FAST`, `TOP_ENABLE_BNO_SENTINEL`, `TOP_ENABLE_BNO_FREEZE_DETECT`.
  De hecho `TOP_BNO_FAST` exige `TOP_BNO_PRIMARY_ONLY` (#error) y un primario aislado; revisar el
  estado real del BNO de R1 antes.
- Lo que SÍ aplica a R1 (independiente del BNO): el **emisor @100Hz desacoplado**
  (`TOP_ENABLE_SNAPSHOT_TIMER`), el **HC-SR04 no-bloqueante** (`TOP_ENABLE_HCSR04_ASYNC`), el
  **round-robin con skip** (`TOP_ENABLE_TOF_SCHED`).
- La familia de R1 es `top_robot1_pri*` (env propio, `-DROBOT1`), NO `top_robot2_*`.

## Qué hacer (con R1 a mano)
1. Confirmar el estado del BNO de R1 (¿hay primario sano aislado en Wire2?). Si sí → incluir los
   flags de BNO; si no → solo emisor/HC-SR04 async/round-robin.
2. Definir/actualizar el env de competencia de R1 (`top_robot1_pri`) con los flags que correspondan
   + dejar `top_robot1_pri_anterior` como fallback (espejo de lo hecho en R2).
3. `pio run -e top_robot1_pri` SUCCESS.
4. Banco: el mismo plan T1-T7 + (si lleva freeze-detector) el make-or-break de falso-CONGELADO.

## Criterio de cierre
- R1 corre el firmware completo que le corresponde, validado en banco; `top_robot1_pri_anterior`
  disponible como escape; documentado en el journal. **Lo cierra el equipo con R1 real** (regla #1).
