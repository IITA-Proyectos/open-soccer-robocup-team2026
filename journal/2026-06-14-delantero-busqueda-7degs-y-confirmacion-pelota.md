---
title: "Delantero R1: giro de búsqueda a 7 deg/s (omega) + confirmación temporal de pelota"
date: 2026-06-14
author: Claude (Opus 4.8)
requested-by: Elías Cordero
status: SIN VALIDAR EN BANCO — solo compila
tipo: cambio-firmware
env: central_robot1_delantero_practica_bb
---

# Delantero R1 — giro de búsqueda lento + anti falso-naranja

## Qué cambió (pedido Elías 2026-06-14)

En `src/central/strategy.cpp` (FSM delantero, `attacker_tick` / SEARCH):

1. **Giro de búsqueda vuelve a OMEGA (deg/s) a 7 deg/s.** Se revirtió el giro por
   PWM crudo (`cmd.spin_pwm`, introducido más temprano el 2026-06-14) al comando por
   `omega_centideg_s`. `ATK_SEARCH_OMEGA_DEG_S = 30 → 7`. `ATK_SEARCH_SPIN_PWM` queda
   sin uso (se deja por compat).

2. **Confirmación temporal de pelota (anti falso naranja).** Antes, en SEARCH, un solo
   frame de `world_model_ball_visible()` disparaba la persecución → perseguía falsos
   naranjas de 1-2 frames. Ahora la pelota tiene que verse **continuo ≥ `ATK_BALL_CONFIRM_MS`
   (200 ms)** antes de salir de SEARCH. Variable de estado `g_atk_ball_seen_since_ms`;
   se resetea cuando la pelota deja de verse y al (re)entrar a SEARCH (en `transition_atk`,
   para no arrastrar un timer viejo tras LINE_AVOID/APPROACH → evita confirmación instantánea).

## Riesgo conocido (a validar en banco)

⚠️ **El piso de PWM por rueda es 70.** A 7 deg/s las velocidades de rueda caen muy por
debajo del piso → el piso puede LEVANTAR el PWM y hacer que el robot gire **más rápido que
7 deg/s o a tirones**. Esto es exactamente lo que motivó el cambio previo a PWM crudo. Si en
banco no gira lento de verdad, el camino correcto es **giro pulsado (PFM)** o **lazo cerrado
con el yaw del OTOS** (R1 tiene OTOS). Queda anotado para la próxima sesión.

## Verificación hecha

- `pio run -e central_robot1_delantero_practica_bb` → **SUCCESS** (solo compila).
- **NO validado en hardware.** El comportamiento del giro (¿realmente lento?) y el filtro
  de confirmación se cierran en banco (regla no negociable: lo cierra el equipo humano).

## Plan de prueba (banco — pendiente)

1. Flashear `central_robot1_delantero_practica_bb` + DOWN con `down` (OTOS).
2. **Giro:** soltar en SEARCH (sin pelota). Medir si gira lento/parejo a ~7 deg/s o si el
   piso de PWM lo acelera / lo deja a tirones. Anotar.
3. **Confirmación:** mostrar un naranja falso por <200 ms → NO debe perseguir. Mostrar la
   pelota real >200 ms continuo → debe salir a buscarla.
4. Si el giro sale rápido/tironeado → abrir tarea de giro pulsado u OTOS-loop.

## Tuning expuesto

- `ATK_SEARCH_OMEGA_DEG_S` (strategy.cpp) — velocidad de giro de búsqueda.
- `ATK_BALL_CONFIRM_MS` (strategy.cpp) — ms de pelota continua para confirmar (200 def).
