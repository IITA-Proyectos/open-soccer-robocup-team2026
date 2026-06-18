---
title: "Arquero: patrulla SIMPLE izquierda↔derecha que ANDA (banco Gustavo) + fix clasificación marco-cancha + análisis de arquitectura del lazo"
date: 2026-06-18
author: "Claude (sesión coach — Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: ANDA en banco (validado Gustavo 2026-06-18, env central_robot2_arquero_pingpong); 2 mejoras PENDIENTES para mañana
tipo: implementacion + diagnostico
---

# Resumen

El arquero ahora **patrulla de lado a lado (izq↔derecha) de forma confiable** — validado en
banco por Gustavo el 2026-06-18 con `central_robot2_arquero_pingpong`. Se llegó después de
varias iteraciones de banco y un análisis de arquitecto del lazo de control. Todo detrás de
flags, apagado por defecto → los binarios de competencia (`central_robot2_arquero`) quedan
byte-idénticos. **Quedan 2 mejoras para mañana** (heading-hold por giróscopo + posición XY).

# Camino (qué falló y por qué)

1. Primer intento (`central_robot2_arquero_strafe_pid`): strafe continuo + heading-PID por la
   rueda trasera + escape de 2 s. **No andaba**: se detenía antes de la línea y retrocedía.
2. Análisis de arquitecto (workflow): se **descartaron** las hipótesis de bloqueo de serie —
   el lazo es un superloop libre (decenas de kHz), el RX es NO bloqueante (la ISR del UART
   buffea los bytes en un ring de 576 B; el loop solo drena + parsea, barato), control a 100 Hz.
   **El serie NO es el cuello.** (Ver §Hallazgos para no re-investigar.)
3. **Root cause REAL (confirmado adversarialmente):** la clasificación "¿la línea está atrás o
   al costado?" se hacía en **MARCO ROBOT**, sin corregir por el rumbo (`strategy.cpp` rebote
   del MOVE). Con el robot rotado, su propia línea de FONDO se confundía con una lateral →
   escapaba para el lado equivocado / contra la pared. Más el **churn de estados de la v3.3**
   (PULSO→STOP→REACQ): ante rumbo fuera de banda se frenaba a reorientar y RETROCEDÍA.
4. **Solución que ANDA:** FSM **SIMPLE ping-pong** (`-DGK_PINGPONG`) que reemplaza la patrulla
   v3.3: strafe continuo izq↔der, clasificación de línea en **MARCO CANCHA** (la + heading),
   y escape al opuesto **HASTA SALIR** de la línea (mín 0,7 s, tope 4 s). Sin pelota, sin
   retroceso, sin pararse a media patrulla.

# Qué se hizo (host-tested + compila; banco lo cerró Gustavo)

- **`src/shared/gk_strafe_hold.h`** (PURO, host 7/7): PI de rumbo → trim de la rueda TRASERA,
  con zona muerta + anti-windup + flag "fuera de banda". Es la idea de Gustavo de modular con
  pequeñas correcciones la trasera (que tiene holgura sobre su piso, a diferencia de las
  delanteras que se desbordan con ω — banco 2026-06-12).
- **`motors_set_rear_trim()`** en `motors_zircon` (gateado `-DCENTRAL_REAR_TRIM`): suma la
  corrección a la trasera (idx 2) post-floor-scale, cap térmico 150.
- **`strategy.cpp`**: `gk_pingpong_tick()` (FSM simple) + bypass en el router; `GK_STRAFE_PID`
  (versión continua previa); clasificación **marco cancha** (reusa el patrón ya validado de
  `GK_SIMPLE_STRAFE`, banco María 2026-06-14); escape **hasta-salir** (mín/máx).
- **`central_telemetry_serial.cpp`**: `Serial.write` del monitor gateado con `availableForWrite`
  → un host lento (la app va a ~9 Hz) ya NO puede colgar el control de motores. (= el A2 del
  análisis de arquitecto; en partido no hay host → no-op.)
- **Envs banco** (`platformio.ini`): `central_robot2_arquero_pulse` (debug pulsado),
  `central_robot2_arquero_strafe_pid` (continuo+PID), **`central_robot2_arquero_pingpong`**
  (el que ANDA). Competencia byte-idéntica.
- Doc: `docs/firmware/FSM-ARQUERO-ESTADOS.md` (los estados de la FSM del arquero).

# Validación de banco (Gustavo, 2026-06-18)

`central_robot2_arquero_pingpong`: el arquero hace izq↔derecha, toca la línea y escapa al
opuesto a máxima potencia hasta salir. **"Ahora sí anda."** Empezó en strafe PURO (sin trim de
rumbo), que es lo más confiable (banco previo: strafe puro derecho a 200 mm/s). Dato de planta
confirmado: la potencia del escape **ya está al máximo** (a ~590 mm/s la trasera llega al cap
de 150 PWM = límite de quemado de los motores 5 V @ 7,4 V); más mm/s no da más empuje.

# PENDIENTE para mañana (pedido Gustavo)

1. **Heading-hold con giróscopo modulando la rueda trasera.** El módulo (`gk_strafe_hold.h`) y
   el actuador (`motors_set_rear_trim`) YA están y compilan; falta **prenderlo** (agregar
   `-DCENTRAL_REAR_TRIM` al env pingpong) y **confirmar el signo** `GK_REAR_TRIM_SIGN` en banco
   (si corrige al revés, −1) + tunear `GK_STRAFE_KP/KI`. Objetivo: que el robot no derive de
   paralelo durante el strafe. ⚠️ Ojo: el heading del TOP llega a ~4 Hz / ~250 ms viejo (cuello
   real) → ver la rama `feat/top-heading-predict-gateado` de la otra sesión.
2. **Control de posición XY en cancha** con la pose del TOP (`world_model_get_my_x/y_mm` +
   `my_pose_confidence`): que el arquero se ubique respecto de su arco por XY (no solo por tocar
   la línea). Encaja con el spec `docs/superpowers/specs/2026-06-17-localizacion-tof-pose-xy-design.md`.

# Hallazgos de arquitectura (para NO re-investigar)

- El serie **no bloquea**: la ISR del UART buffea los bytes (ring 576 B); el parseo (~400
  frames/s) es trivial en el M7 a 600 MHz. El cuello NO es el serie.
- La latencia que importa es el **heading del TOP (~4 Hz)** — es el loop del TOP frenado por
  I²C de ToF, no el de la CENTRAL. Otra sesión lo ataca (`feat/top-heading-predict-gateado`).
- Mover el parseo a ISR/RTOS = optimización prematura para este robot (no lo necesita).

# Archivos
- `src/shared/gk_strafe_hold.h` + `test/test_gk_strafe_hold/`
- `src/central/{motors_zircon.h,motors_zircon.cpp,strategy.cpp,central_telemetry_serial.cpp}`
- `platformio.ini` (3 envs de banco del arquero)
- `docs/firmware/FSM-ARQUERO-ESTADOS.md`, este journal.
