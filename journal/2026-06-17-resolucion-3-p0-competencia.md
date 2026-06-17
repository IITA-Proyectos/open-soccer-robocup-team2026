---
title: "Resolución de los 3 P0 de competencia: FLOOR_SCALE arquero R1 + watchdog CENTRAL + pose XY"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: firmware listo (compila); cierre = banco (TASK-110)
tipo: journal
---

# Resumen

Del estado de situación de las 3 placas (validado contra código) salieron 3 P0 que afectan
el binario de competencia. Gustavo pidió resolverlos. Se aplicó **criterio diferenciado por
riesgo** (no se metió a ciegas todo al binario default), respetando la regla de oro: el
cierre final es banco.

# Los 3 P0 y cómo se resolvió cada uno

## P0.2 — FLOOR_SCALE en el arquero R1 → CORRECCIÓN aplicada al default

**Hallazgo (código):** `apply_role_from_dipswitch` (main_central.cpp:146-147): `-DROBOT1`
→ rol GOALKEEPER. Y `[env:central_robot1]` NO tenía `-DCENTRAL_FLOOR_SCALE`. El arquero
v3.3 se validó CON FLOOR_SCALE (en R2); sin el flag, el clamp por-rueda se come las
correcciones de gyro → "pierde el frente".

**Resolución:** se agregó `-DCENTRAL_FLOOR_SCALE` a `[env:central_robot1]`. Es una
corrección hacia el estado validado, no una feature nueva → bajo riesgo, va al default.
Banco R1 pendiente (los pisos {70,70,107} de R1 están "a verificar").

## P0.1 — Watchdog CENTRAL → env candidato + paridad, NO al default

**Hallazgo:** `-DCENTRAL_ENABLE_WDT` existía con `central_robot1_wdt`, pero NINGÚN env de
competencia lo define. El timeout de snapshot (500 ms) no cubre un cuelgue del propio loop.

**Resolución:** se creó `central_robot2_wdt` (paridad; antes solo R1). **NO se metió el
flag al default**: un reset espurio en partido es catastrófico. El WDOG1 se alimenta cada
loop (riesgo bajo), pero se valida primero (30 min sin reset + hang test) y luego se
promueve. Pushback razonado: la prudencia del repo (gateado hasta validar) es correcta.

## P0.3 — Pose XY (pose_fusion) → env candidato limpio + paridad, NO al default

**Hallazgo:** `pose_fusion` GATEADO-OFF en el TOP de competencia. Existían
`top_robot2_pri_posefusion` (R2) y `top_robot1_pri_xval` (R1 + extras), pero no un R1 limpio.

**Resolución:** se creó `top_robot1_pri_posefusion` (R1 limpio: solo POSE_FUSION +
FREEZE_DETECT). **NO se metió al default**: una pose XY mal anclada en partido es PEOR que
sin pose (navegaría a una posición fantasma). Requiere banco TASK-210/211 (signo OTOS +
ruido ToF + freeze-detector). Pushback razonado explícito.

# Por qué NO se metió todo al binario default

La consigna era "resolver los 3 P0". El criterio coach: de los 3, solo P0.2 es una
corrección hacia lo validado (seguro para el default). P0.1 y P0.3 AÑADEN comportamiento
no validado → meterlos al binario de competencia sin banco sería imprudente:
- WDT mal → reset en pleno partido.
- pose_fusion con signo OTOS invertido → robot navega a posición fantasma.
La arquitectura del repo ya los tenía gateados en envs aparte por esta razón. Se completó
la paridad R1/R2 y se dejó el checklist; la promoción al default la decide el equipo tras
banco.

# Verificación

- `central_robot1` (con FLOOR_SCALE), `central_robot2`, `central_robot1_wdt`,
  `central_robot2_wdt`, `top_robot1_pri_posefusion`, `top_robot2_pri_posefusion`: compilan
  SUCCESS (ver TASK-110). "Compila" no es "funciona" — el banco lo cierra el equipo.

# Archivos tocados

- `software/teensy/Soccer 2026/platformio.ini`: FLOOR_SCALE a central_robot1; +env
  central_robot2_wdt; +env top_robot1_pri_posefusion (todos con comentario trazable).
- `team-tasks/2026-06-17-task-110-cierre-banco-3-p0-competencia.md` (checklists de banco).
- `docs/ESTADO-ACTUAL.md` (banner).
- `journal/2026-06-17-resolucion-3-p0-competencia.md` (este).

# Pendiente equipo (banco) → TASK-110

Los 3 checklists de TASK-110. En particular: confirmar arquero R1 con FLOOR_SCALE mantiene
el frente; validar watchdog (30 min + hang test) y promover; validar pose XY (signo OTOS) y
decidir promoción.
