---
title: "Confiabilidad + auto-recuperación del heading: cross-validación con datos independientes + centinela dual-BNO @1Hz (módulos puros gateados)"
date: 2026-06-15
author: "Claude (Anthropic - Claude Opus 4.8 1M) — coach"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M) — workflow de diseño (9 agentes) + verificación adversarial"
status: final
tags: [firmware, top, bno055, imu, fusion-sensorial, otos, camaras, robustez, tolerancia-fallas]
robot: ambos
area: control
tipo: implementacion
---

# Confiabilidad + auto-recuperación del heading (módulos puros gateados)

## Contexto

Gustavo pidió una estrategia COMBINADA para hacer el heading MÁS CONFIABLE con
AUTO-RECUPERACIÓN: (1) decidir qué BNO está sano con datos INDEPENDIENTES (no
auto-referencia, que un BNO congelado engaña); (2) usar el BNO de Wire2 como primario
@100Hz y el BNO del bus-ToF como **centinela @1Hz leído con los ToF pausados**; (3)
fusionar cámara + OTOS para validar el BNO; (4) plan de reseteo si deriva.

Minutos antes, otra sesión subió **TASK-212** — análisis profundo del MISMO enfoque
(cross-validación OTOS+cámaras + recuperación). Este trabajo **construye sobre TASK-212**
(no lo re-analiza) y aporta lo que faltaba: el **centinela dual-BNO @1Hz** (idea de
Gustavo, no estaba en TASK-212) + la **implementación host-testeable de la Fase 1**.

> **Conexión clave:** el **INC-1 (gyro-guard del freeze-detector)** que implementé hoy ES
> la **Fase 0** que TASK-212 marca como el 80/20. Esta entrega es la **Fase 1** (el módulo
> puro de cross-validación), con el centinela absorbido como una fuente más.

## Qué se diseñó (workflow + verificación adversarial)

Workflow de 9 agentes (entender firmware + diseñar + 3 lentes adversariales + plan final).
El review **reencuadró el alcance con 3 hallazgos duros y verificados**:

1. **FAILOVER FÍSICO RECHAZADO para Incheon.** La premisa "imu_fusion ya hace failover
   idx0→idx1 transparente" es **FALSA** (es un comentario; en realidad PROMEDIA y puede
   re-elegir al primario MALO). Y un failover a una fuente PLAUSIBLE-PERO-ERRADA es **PEOR**
   que `heading_valid=0`: el fail-safe ω=0 (`strategy.cpp:522,528,564`) cubre heading
   AUSENTE, NO heading PRESENTE-Y-MALO → el arquero ROTARÍA fuera del arco. **Jerarquía de
   degradación: primario-sano > heading_valid=0 > secundario-promovido (solo 2027).**
2. **La cámara NO mide ω si el arquero hace STRAFE** cerca del arco: d(bearing)/dt se
   contamina con la traslación → falso-veto. Solo vale con |vel_lateral|≈0.
3. **ANTI-FALSO-VETO es el riesgo #1.** Vetar un BNO SANO es peor que el bug original. Con
   <2 refs INDEPENDIENTES (no-BNO) válidas, JAMÁS se declara MALO. El centinela es un 2º
   BNO → confirma, pero NO cuenta como independiente (falla de modo común: un golpe al
   cristal de 32 kHz congela a los dos).

**Veredicto:** el centinela aporta **DIAGNÓSTICO de DERIVA + segunda opinión física +
telemetría**, NO failover. INC-1 ya cubre el modo CONGELADO a 100 Hz.

## Qué se implementó (PURO, host-testeado, gateado off-by-default)

Dos módulos puros (riesgo cero: no compilan para Teensy, no tocan I²C, no mueven el robot;
los 3 blockers viven todos en el glue Arduino, que queda BLOQUEADO a banco):

- **`src/shared/goal_rate_tracker.h`** — tasa de rotación inferida del bearing al arco
  (w_cam = −Δbearing/Δt). **NO es espejo de `ball_velocity`** (el review lo exigió): resta
  ANGULAR con envoltura `norm180` (el cruce ±180 no da un salto falso de 360°/s), gate de
  salto físico imposible (glitch/cambio de arco), visibilidad CONTINUA ≥2 frames. Test
  `test_goal_rate_tracker` 7/7.
- **`src/shared/imu_cross_validate.h`** — salud del primario por cross-validación: `w_truth`
  = mediana de refs válidas {centinela, OTOS, cámara}, tolerancia adaptativa
  (tol=8+0.15·|w_truth|), saturación OTOS (descarta ≥300°/s, B.3), **anti-falso-veto por
  n_refs-independientes** (0→jamás MALO, 1→solo baja score, ≥2→consenso K-ventanas), latch+
  cooldown, gate de ventana-evaluable (anti-aliasing del centinela @1Hz), y el **scheduler
  del centinela con TIMEOUT defensivo** (lógica pura; el read físico es glue). Veredicto
  {SANO/SOSPECHA/MALO} + acción {NINGUNA/FLAG_DRIFT/RECOMMEND_SETTLE}. **NO emite FAILOVER.**
  Test `test_imu_cross_validate` 13/13 (incluye los invariantes anti-falso-veto con 0 y 1 ref).

## Qué NO se implementó (glue Arduino → banco, TASK-213)

Los 3 blockers del review viven acá; NO son host-testeables ni los puede cerrar Claude:
- **Centinela @1Hz con lectura INLINE del secundario dentro de `sensors_tof_tick()`** (NO
  handshake de 2 módulos — la carrera reintroduciría el freeze) + pausa del round-robin de ToF.
- **Init del secundario al boot** bajo `TOP_ENABLE_BNO_SENTINEL` (begin IMUPLUS + cristal +
  calib) separando 'inicializado-para-centinela' de 'activo-en-fusión' (g_ready[1]=false →
  byte-idéntico con el flag OFF). Sin begin, leerlo crudo da basura (no está en modo fusión).
- **`GoalRateTracker` + omega OTOS cableados** a las cámaras/comm_down; el veredicto a telemetría.
- **Failover físico** = 2027 (requiere camino idx0→idx1 REAL en `imu_fusion` + re-anclaje
  del secundario contra el bearing de cámara, no contra el primario contaminado).

## Verificación

- Gate host verde (los 2 módulos puros + sus tests compilan g++; el glue Arduino lo valida
  el equipo en banco). `test_goal_rate_tracker` 7 + `test_imu_cross_validate` 13.
- Verificación adversarial del DISEÑO: 3 blockers (todos en el glue) + correcciones de
  matemática (goal_rate ≠ ball_velocity, init del secundario, integrated-angle del centinela)
  aplicadas en la implementación.

## Pendiente de banco (Claude NO cierra TASKs de HW) — TASK-213

- Medir el **piso de ruido del ω** del primario y del centinela ANTES de fijar `tol_base`
  (hoy 8°/s, ESTIMADO) y `consensus_k` (2 o 3).
- Medir el **tiempo real de la lectura del secundario** (read_raw_yaw+gyro, varios getVector)
  y confirmar con analizador lógico que el bus Wire queda LIMPIO durante la pausa de ToF.
- Confirmar que el **peor ToF** no cruza ~180 ms de staleness con el centinela activo.
- Decidir si se activa `TOP_ENABLE_BNO_SENTINEL` (solo telemetría/diagnóstico) para Incheon
  o queda 2027 — **costo de oportunidad: cada hora acá no calibra cámaras (TASK-022),
  bloqueante #1, y en R2 la cámara es la única pata externa del cross-check.** Prioridad P2.
