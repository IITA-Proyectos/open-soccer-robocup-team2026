---
title: "Lazo de control del arquero: análisis de latencias + 2 mejoras gateadas (fast-BNO + amortiguación PD)"
date: 2026-06-14
author: "Claude (Anthropic - Claude Opus 4.8 1M) — coach"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M)"
status: final
tags: [control, analisis, arquero, alta]
robot: arquero
area: control
tipo: analisis
---

# Control del arquero moviéndose de lado: latencias + 2 mejoras gateadas

## Contexto

El arquero oscila ("medialuna") cuando hace strafe lateral tapando el arco.
Gustavo pidió: (1) trazar el lazo primario de control con el PID, frecuencias,
lectura del BNO, fórmulas y detección de línea; (2) sospechaba demoras en el lazo;
(3) proponer e implementar mejoras de código; (4) re-analizar tras dos
correcciones suyas.

## Qué se hizo

1. **Análisis end-to-end del lazo** (3 placas) con `archivo:línea`, en
   [`docs/firmware/CONTROL-ARQUERO-LATERAL-Y-LATENCIAS.md`](../docs/firmware/CONTROL-ARQUERO-LATERAL-Y-LATENCIAS.md):
   loop de la CENTRAL (`strategy_tick` 100 Hz), fórmulas del HeadingPID/PFM/LateralPID,
   lectura del BNO, detección de línea (32 sensores 1 kHz, imminent_exit ≥6), y el
   presupuesto de latencia.
2. **2 mejoras de código, gateadas (OFF por default → binario de competencia intacto):**
   - **P0 latencia — `TOP_BNO_FAST`** (`sensors_imu.cpp`): lee el BNO primario
     aislado (Wire2, sin ToF) a 100 Hz en vez de 20 → atraso del rumbo de ~35-70 ms
     a ~15-30 ms. `#error` exige `TOP_BNO_PRIMARY_ONLY`. Env `top_robot2_pri_fastbno`.
   - **P1 amortiguación — `GK_PFM_RATE_DAMP`**: módulo puro nuevo `heading_rate.h`
     (velocidad de giro de muestras frescas, sin ruido de cuantización) + término
     `kd_rate` en `pfm_heading.h` (la "D" de un PD), cableado en el strafe del
     arquero (`strategy.cpp`, `GK_PFM_KD_RATE=0,30`). Env
     `central_robot2_arquero_strafe_cam_ratedamp`.

## Qué se midió/observó

- **Latencia del rumbo (de código):** lazo 100 Hz pero BNO leído a 20 Hz (tope
  `BNO_READ_INTERVAL_MS=50` GLOBAL en `sensors_imu.cpp:365`) → 4 de 5 ticks usan
  rumbo repetido. Atraso ~35-70 ms hoy.
- **Hallazgo clave (corrección de Gustavo #1):** el BNO que usa el robot
  (`top_robot2_pri`, `-DTOP_BNO_PRIMARY_ONLY`) está SOLO en Wire2 sin ToF → el tope
  de 20 Hz es innecesario para ese sensor. Bajarlo es software, no hardware.
- **Corrección de Gustavo #2:** el actuador NO es "bang-bang / cuantizado". El PWM
  es continuo y proporcional (`wheel_speed_to_pwm`), con **zona muerta** abajo;
  `CENTRAL_FLOOR_SCALE` conserva "las correcciones finas… sin bang-bang"
  (`motors_zircon.cpp:154-174`). El documento se corrigió: **control imperfecto de
  potencia (zona muerta), NO sin control**. La latencia es la causa dominante de la
  oscilación; la zona muerta es secundaria (afina lo sub-piso).
- **Gate host:** `scripts/run-host-tests.sh` → **869 tests / 62 envs / 0 fallos**
  (incluye `test_heading_rate` 8 + 3 nuevos en `test_pfm_heading`), con el g++ de
  Webots (no hay g++/pio en PATH).

## Conclusión

La oscilación es, sobre todo, **latencia**: el lazo corrige más rápido de lo que
puede "ver" el rumbo. Las dos mejoras atacan las dos puntas (menos atraso + más
amortiguación), gateadas para validar en banco antes de entrar a competencia. El
actuador tiene control continuo de potencia con zona muerta — no es "sin control".

## Próximos pasos (banco — Claude NO cierra tareas de hardware)

1. Flashear `top_robot2_pri_fastbno` (TOP) y `central_robot2_arquero_strafe_cam_ratedamp`
   (CENTRAL); grabar rumbo/omega/tiempo con la caja negra en 3 casos (hoy, solo P0,
   P0+P1).
2. Confirmar que el loop del TOP sigue holgado con el BNO a 100 Hz.
3. Titrar `GK_PFM_KD_RATE` (de 0,30) hasta `|error de rumbo| < 10°` sin medialuna.
