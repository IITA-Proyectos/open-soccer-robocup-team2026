---
id: TASK-213
title: "Banco: cablear+validar el centinela dual-BNO @1Hz (ToF-pausa) + la cross-validación de salud del heading"
date_created: 2026-06-15
date_closed: 2026-06-16
assigned: [equipo (firmware TOP)]
priority: P2  # sube a P1 si el primario único se muestra poco confiable en banco
pedido-por: Gustavo Viollaz (2026-06-15)
status: VERIFICADO-EN-BANCO-2026-06-16  # ROBOT2: Gustavo validó en banco — primario Wire2 trackea sin freeze, 2º BNO inicializa como centinela y se ve "centinela @1Hz" en el monitor, scan I²C confirma 0x28 único (sin 0x29). Ver banner abajo. R1: mismo programa listo (envs top_robot1_pri*), boot-check de R1 en TASK-216.
relacionada: TASK-212 (análisis), TASK-211 (freeze-detector INC-1 = Fase 0), TASK-022 (cámaras), TASK-014 (pulseIn), TASK-207
tags: [firmware, top, bno055, imu, fusion-sensorial, centinela, otos, camaras, robustez]
depends_on: []
---

# TASK-213 — Centinela dual-BNO @1Hz + cross-validación (cablear + banco)

> ✅ **VERIFICADO EN BANCO (ROBOT2, Gustavo 2026-06-16).** Cerrado por el humano con la placa
> (regla 1). Evidencia del banco con `top_robot2_pri_xval` flasheado:
> - **Primario (Wire2 24/25 @ 0x28):** boot `PRIMARIO OK`; el heading TRACKEA el giro suave
>   (0 → -22 → +47 → ~1) y se clava al frenar, SIN congelarse → el bus propio Wire2 cumple.
> - **Centinela (2º BNO, Wire @ 0x28):** `CENTINELA init OK`; el monitor (monitor-base, vista
>   Salud) ahora lo muestra como **"BNO secundario (centinela) @1Hz: X°"** en verde (no "falla").
> - **Arquitectura confirmada:** `[i2c-scan Wire, ToF dormidos] ACK en: 0x28` → único 0x28, **sin
>   ningún BNO en 0x29** (la corrección del cableado, verificada físicamente).
> - **Feature del monitor** (mostrar centinela + heading) PROGRAMADO + verificado: firmware
>   `test_telemetry_top` 22/22 + monitor 236/236. Commit a785858 + el de cierre.
>
> **Lo que NO se hizo (queda como banco fino OPCIONAL, post-Incheon, NO bloqueante):** analizador
> lógico en Wire durante la ventana de 1 Hz + medir piso de ruido del ω para fijar `tol_base`/
> `consensus_k` con dato real. El centinela hoy aporta DIAGNÓSTICO (telemetría), sin failover.
>
> **ROBOT1:** el MISMO programa quedó listo (envs `top_robot1_pri*`, mismo path, sin deconflict);
> falta el boot-check de R1 (sus BNO estaban desconectados) → **TASK-216**. Riesgo bajo: HW idéntico.

## Por qué existe

La Fase 1 de TASK-212 (cross-validación de salud del heading con datos independientes) +
la idea de Gustavo del **centinela dual-BNO @1Hz con ToF-pausa** se DISEÑARON y se
implementó su **núcleo PURO host-testeado**. Falta el **glue Arduino** (no host-testeable)
y la **validación en banco** — regla 1 (CLAUDE.md): Claude NO lo cierra.

## YA HECHO (host-tested, gateado, en el repo)

- `src/shared/goal_rate_tracker.h` (+ `test_goal_rate_tracker` 7/7): w_cam = −Δbearing/Δt
  con resta angular envuelta + gate de salto + visibilidad continua.
- `src/shared/imu_cross_validate.h` (+ `test_imu_cross_validate` 13/13): w_truth = mediana
  de refs, tolerancia adaptativa, saturación OTOS, **anti-falso-veto por n_refs-indep**,
  consenso K-ventanas, latch+cooldown, scheduler del centinela con TIMEOUT. Veredicto
  {SANO/SOSPECHA/MALO} + acción {NINGUNA/FLAG_DRIFT/RECOMMEND_SETTLE}. **NO failover.**
- Fase 0 (modo congelado): `imu_freeze_update_g` con guarda de gyro — ver TASK-211.

## Qué falta cablear (glue Arduino — los 3 BLOCKERS del review viven acá)

1. **Init del secundario al boot** bajo `-DTOP_ENABLE_BNO_SENTINEL`: `init_one_bno`
   completo del 2º BNO (Wire) — begin IMUPLUS + cristal + calib — aunque `g_ready[1]`
   siga `false` para la fusión (separar 'inicializado-para-centinela' de 'activo-en-fusión'
   → byte-idéntico con el flag OFF). **Sin begin, leerlo crudo da basura** (no está en modo
   fusión) y podría confirmar un freeze FALSO.
2. **Centinela @1Hz con lectura INLINE dentro de `sensors_tof_tick()`** (NO handshake de 2
   módulos — la carrera reintroduce el freeze): cuando toca la ventana 1Hz, ese tick SALTA
   el round-robin Y lee el secundario en el mismo cuerpo (bus Wire garantizado limpio),
   publica a un buffer que `sensors_imu_tick` consume. Usar el scheduler+timeout PURO de
   `imu_cross_validate.h` (`xval_sentinel_due/arm/done/timed_out`). Rotar QUÉ índice de ToF
   se sacrifica (no siempre el mismo) para no atrasar un solo sensor.
3. **`GoalRateTracker` + omega OTOS** cableados a `cameras_runtime.cpp` / `comm_down.cpp`;
   alimentar `xval_feed_*`; exponer el veredicto/score por telemetría (`top_telemetry_serial`).
   La cámara: `valid=true` SOLO con arco visible continuo ≥2 frames **Y |vel_lateral|≈0**
   (anti-sesgo-strafe) **Y** no-stale.

## Pre-requisitos de banco (MEDIR antes de confiar en umbrales)

1. **Piso de ruido del ω** del primario y del centinela, robot quieto → fijar `tol_base`
   (hoy 8°/s ESTIMADO) y `consensus_k` (2 o 3).
2. **Tiempo real** de la lectura del secundario (varios `getVector` a 100 kHz, podría ser
   >10 ms) con analizador lógico → confirmar bus Wire LIMPIO durante la pausa + que 1 tick
   de ToF alcanza para la ventana.
3. **Staleness del peor ToF** con el centinela activo (loguear 5 min) → confirmar que NO
   cruza ~180 ms (margen contra los 250 ms de P1-TOF-STALE).
4. Confirmar que el secundario en Wire, inicializado y con ToF dormidos durante la lectura
   1Hz, **da un yaw que sigue el giro** (que la idea funciona de verdad).

## Qué validar (criterios medibles)

1. **Diagnóstico, sin actuar (telemetría):** con un BNO tapado/derivando, el score/veredicto
   distingue SANO / SOSPECHA / MALO. Robot quieto 3-5 min apuntando al arco → el sano NUNCA
   se declara MALO (falso-veto = el riesgo #1). Giro rápido en SEARCH → tampoco (saturación ω).
2. **Anti-falso-veto en R2 real:** con la cámara como única ref externa (intermitente, sin
   homografía), el sistema **NO veta nada** (por su propia regla con <2 refs indep) → el
   aporte real en R2 es INC-1 + centinela-diagnóstico.
3. **Recuperación (si se decide):** RECOMMEND_SETTLE solo se actúa OPORTUNISTA en ventanas de
   quietud que YA ocurren (WAIT_START), **NUNCA en defensa** (B.4 de TASK-212) — el reset
   activo en el arquero es superficie de gol.

## Criterio de cierre

1. Pre-requisitos medidos + umbrales fijados con dato real (no estimación).
2. Diagnóstico validado (distingue los 3 modos; 0 falso-veto del sano). Veredicto en journal.
3. Decisión: ¿`TOP_ENABLE_BNO_SENTINEL` (telemetría) para Incheon o 2027? El **failover
   físico** queda explícitamente para 2027 (requiere camino idx0→idx1 REAL en `imu_fusion`).

## Riesgos (formato coach)

- `risk-no-fix`: el primario único puede derivar en cancha sin diagnóstico (INC-1 cubre
  congelado, NO deriva). El centinela-diagnóstico lo detecta — pero solo aporta donde haya
  refs externas, que en R2 es la cámara (frágil) + el 2º BNO (no independiente).
- `risk-fix`: el glue del centinela mal cableado puede **reintroducir el freeze** del
  secundario (blocker timing) o dar un veredicto falso si el secundario no se inicializa.
  Por eso los módulos puros van primero (riesgo cero) y el glue se valida en banco con
  analizador lógico. **El failover NO entra (un failover errado es peor que heading_valid=0).**
- `tiempo`: glue ~1,5-2 días + 2-3 sesiones de banco. **Costo de oportunidad: TASK-022
  (cámaras) es el bloqueante #1 de Incheon y además la única pata externa del cross-check en R2.**

## Atribución

Diseño + módulos puros + esta TASK: Claude Opus 4.8 (Anthropic), 2026-06-15 (requested-by
Gustavo Viollaz). Construye sobre TASK-212 (análisis). Validación en banco = equipo humano.
