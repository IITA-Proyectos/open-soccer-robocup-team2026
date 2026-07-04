---
title: "arqueromix: 9º checkpoint — pateo recto (trim) + corte de retroceso por pelota (gateados)"
date: 2026-07-04
author: "Claude (sesión coach, pedido María)"
status: hecho-sin-banco
---

# arqueromix — 9º checkpoint `_recto_cortaretro`

## Contexto de banco (María, 2026-07-04, con el #8)

- El +6% de potencia del #8 **"anda bastante bien"** (anotado en TASK-122 como banco parcial).
- Hallazgo: **el despeje se desvía SIEMPRE a la izquierda.**
- Pedido: que durante el retroceso post-pateo también mire la pelota y, si la ve, **corte el
  retroceso** y vuelva a buscarla/posicionarse.

## Qué se hizo (todo gateado, regla de oro de la familia)

**(1) `-DARQMIX_KICK_TRIM` — pateo recto.** El golpe es simétrico en PWM (M1=+vel/M2=−vel) pero
el robot curva a la izquierda → asimetría física. Mismo remedio que el delantero centralmix
(`MIX_KICK_FWD_TRIM`, banco 2026-06-22): PWM extra en la delantera IZQUIERDA, `AMIX_KICK_TRIM_PWM`=15
nominal en el pico, escalado con la rampa (`(vel × trim) / VEL_FINAL`) para corrección pareja en todo
el golpe. En el pico: M1 = 191+15 = 206 < 255. Titración en TASK-123.

**(2) `-DARQMIX_RETRO_CUT_BALL` — corte de retroceso por pelota.** Guard al tope de `PATEANDO_atras`
(cubre las 3 variantes: por línea / por tiempo / base): `ball_visible` + distancia ≤
`AMIX_RETRO_CUT_DIST_MM` (default 9999 = cualquiera, pedido literal) → `parar()` + volver a
`esperar_quieto`. **Decisión de diseño:** NO se aplicó al homing del GO (`inicio_retroceder`) — al
arrancar el partido la pelota del centro SIEMPRE se ve, el corte habría matado el homing y el arquero
nunca llegaría a su arco. El re-homing (que reusa ese estado) tampoco lo necesita: se dispara tras
15 s SIN pelota.

**Env nuevo `central_robot2_arqueromix_recto_cortaretro`** (#9) = flags del #8 + los 2 nuevos.

## Riesgos a banco (TASK-123)

1. Con pelota lejana visible el arquero corta y queda ADELANTADO sin volver a su línea → bajar el knob
   de distancia (~800-1000 mm; la escala mm está sin calibrar — titrar en banco).
2. Loop de despeje rápido si la pelota no se va (pared); el caso rival lo cubre el anti-choque heredado.
3. El trim se pensó sobre el golpe de 191 (+6%); si cambia la potencia global, re-mirar el trim.

## Verificación (Claude — compila ≠ anda)

- 9 envs `pio run` SUCCESS; checkpoints #1–#8 **byte-idénticos** (md5 contra la tabla del contexto).
- md5 del #9 anotado en `docs/pruebas-banco/CONTEXTO-ARQUERO-CHECKPOINTS-2026-07-03.md`.
- **SIN banco** → TASK-123 (el cierre es del equipo). Nada se promueve a competencia.
