---
title: "arqueromix: POTENCIA +6% proporcional sumada al checkpoint 8 (gateada)"
date: 2026-07-04
author: "Claude (sesión coach, pedido María)"
status: hecho-sin-banco
---

# arqueromix — potencia +6% proporcional en el checkpoint 8 (`_centrado_fino`)

## Pedido (María, 2026-07-04)

Tras ver los valores de potencia del #8 (tabla en TASK-122), María pidió subir **un 6% la potencia
general** — "esta clase de programa es necesario subir la velocidad de forma proporcional". Es decir:
todos los valores de potencia escalados por el mismo factor, conservando la relación entre ruedas y
entre movimientos.

## Qué se hizo

- **Flag nuevo `-DARQMIX_POWER_106`** (gateado, aditivo — regla de oro de la familia): factor único
  `AMIX_POWER_SCALE` en `amix_config.h` (1.06 con el flag, 1.0 sin él) + helper `constexpr amix_pow()`
  (redondeo al entero más cercano; con 1.0 devuelve el valor histórico EXACTO → sin flag, byte-idéntico).
- **Dónde se aplica (UNA vez por valor):** los PWM fijos de las primitivas (`AMIX_AVANZAR` 100→106,
  `AMIX_INICIO_RETRO_PWM` 100→106, `AMIX_INICIO_AVANCE_PWM` 75→80, `AMIX_KICK_VEL_FINAL` 180→191,
  `AMIX_ATRAS_QUIETO` 80→85, `AMIX_FRENO_PATADA_PWM` 200→212, `AMIX_GIRO_ALINEAR_PWM` 90→95,
  `AMIX_GIRO_FRENTE_PWM` 50→53) y los factores de strafe (`AMIX_PD_BASE` 0.85→0.901,
  `AMIX_PD_BALL` 1.5→1.59 → seguir pelota: delanteras 75→79, trasera 133→141).
- **Dónde NO se aplica (a propósito):** `AMIX_PROP_*` (ya los multiplica `pd` — sería escalar dos veces),
  `AMIX_ATRAS`=120 e impulso inicial (código muerto en el modo quieto), `FORWARD_BIAS` (off),
  `AMIX_ROT_MAX`/`AMIX_KP_RUMBO_OPP` (sin uso en el camino quieto).
- El flag se sumó al env del **#8** `central_robot2_arqueromix_centrado_fino` (queda
  `#7 + -DARQMIX_CENTRADO_FINO + -DARQMIX_POWER_106`).

## Riesgos que se llevan al banco (TASK-122 actualizada)

1. +6% en el seguimiento + banda de centrado recién angostada a 5° = más riesgo de sobrepaso/oscilación.
2. `AMIX_ATRAS_QUIETO` 80→85: el 80 se había bajado a propósito para NO cruzar la línea por inercia.
3. Golpe 191 + `KICK_FAR` 550 ms = más envión; el freno de patada (212) tiene que seguir conteniéndolo.

Para aislar el +6% en banco: quitar `-DARQMIX_POWER_106` del env (o flashear el tag del 2026-07-03,
que es el #8 con centrado fino solo).

## Verificación (Claude — compila ≠ anda)

- Los 8 envs `pio run` SUCCESS.
- Checkpoints #1–#7 **byte-idénticos** (md5 verificados contra la tabla del contexto).
- md5 nuevo del #8 anotado en `docs/pruebas-banco/CONTEXTO-ARQUERO-CHECKPOINTS-2026-07-03.md`.
- **SIN banco** → lo cierra el equipo (TASK-122). Nada se promueve a competencia.
