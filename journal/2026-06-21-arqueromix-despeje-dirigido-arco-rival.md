---
title: "arqueromix — despeje DIRIGIDO al arco rival + arcos por ROL (la CENTRAL deja de mirar color)"
date: 2026-06-21
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
tipo: firmware-feature
toca-competencia: NO (solo src/arqueromix/ + env aislado; build separado)
status: COMPILA · NO validado en banco
---

# Sesión 2026-06-21 — el arquero apunta el despeje al arco rival, sin mirar color

## Qué se pidió (Gustavo)

Que el programa del arquero (`src/arqueromix/`):
1. Deje la **determinación de arco propio vs rival en la placa TOP** (ya estaba así: `goal_polarity`).
2. **Nunca pregunte por color** en la placa CENTRAL: que use el ROL ya resuelto.
3. Al despejar, **apunte al ARCO RIVAL (`goal_opp`) y patee alineado hacia ahí**, sin importar el color.

Antecedente: relevamiento previo de la sesión (cómo cada programa —mix, 2025, central no-mix, cámaras—
decide a qué arco atacar). Documentos:
- [`research/in-progress/2026-06-21-deteccion-arco-ataque-mix-delantero-arquero.md`](../research/in-progress/2026-06-21-deteccion-arco-ataque-mix-delantero-arquero.md) (relevamiento largo)
- [`research/in-progress/2026-06-21-deteccion-arco-propuesta-concreta.md`](../research/in-progress/2026-06-21-deteccion-arco-propuesta-concreta.md) (propuesta corta)

## Hallazgo que enmarcó el cambio

`goal_polarity` (en el TOP, flash de competencia `top_robot2_pri` con `-DTOP_ENABLE_SNAPSHOT_TIMER`) **ya
autodetecta** la polaridad ("el arco al frente del robot es el rival", con latch que la fija al arranque) y
**ya entrega** `goal_opp`/`goal_own` resueltos en el `WorldSnapshot`. El arquero ya recibía ese dato pero
lo IGNORABA (lo copiaba a campos de color que la FSM nunca leía). La feature, entonces, no es construir
detección — es que el arquero **consuma** lo que ya llega.

## Cambios (solo `src/arqueromix/` + `platformio.ini`)

- **`amix_io.h` / `amix_comm.cpp` — fuera el color.** Se eliminan los campos `goal_yellow_*/goal_blue_*` y
  el `#ifdef ARQMIX_ATTACK_BLUE`; el snapshot se copia DIRECTO a `goal_opp_*` (rival) / `goal_own_*`
  (propio), tal como vienen del TOP. La CENTRAL ya no nombra ni invierte colores.
- **`amix_motors.{h,cpp}` — primitiva `girar()`** (rotación pura del omni-3: 3 ruedas al mismo sentido)
  para apuntar el frente al arco.
- **`amix_fsm.{h,cpp}` — estado nuevo `ALINEAR_arco_opp`** entre `PATEANDO_pausa_inicial` y
  `PATEANDO_adelante`: gira hasta `|goal_opp_angle| ≤ AMIX_TOL_ARCO_OPP_DEG`, acotado por
  `AMIX_T_ALINEAR_OPP`. **Fallback robusto:** si no ve el arco rival / ya está alineado / se acaba el
  tiempo → patea recto (= comportamiento arquero 2025). Sin cuelgues (3 salidas en OR).
- **`amix_config.h`** — `AMIX_TOL_ARCO_OPP_DEG`(12°) / `AMIX_GIRO_ALINEAR_PWM`(90) /
  `AMIX_T_ALINEAR_OPP`(300 ms, conservador) + perilla `AMIX_GIRO_ALINEAR_SIGN` (`-DARQMIX_FLIP_GIRO_ALINEAR`).
- **`platformio.ini`** — env `central_robot2_arqueromix_giroflip` (invertir el sentido del giro en banco).
- **`DOCUMENTACION.md`** — §18 + plan de banco; conteo de estados 10→11.

## Verificación hecha

- **Compila:** `pio run -e central_robot2_arqueromix` y `_giroflip` → SUCCESS (FLASH ~16 KB).
- **Revisión adversarial** (subagente): sin bloqueantes. Confirmó color erradicado del código vivo,
  sentinela `visible=0` bien manejado, FSM sin cuelgues, rampa del golpe arranca limpia tras girar,
  `girar()` correcto y precedente (igual patrón que `centralmix/mix_motors girar()`). Aplicadas sus dos
  mejoras: timeout conservador (600→300 ms) y el env `_giroflip`.
- **Merge con trabajo concurrente de Virginia** (`f872a8a`, avance del homing): limpio, recompilado OK.

## ⚠️ NO validado en hardware — a probar en banco (Virginia)

1. Al despejar, ¿el arquero **gira hacia el arco rival**? Si gira al lado contrario → flashear
   `central_robot2_arqueromix_giroflip` (es esperable que el sentido salga invertido la 1ª vez).
2. `AMIX_T_ALINEAR_OPP` arranca en **300 ms** (conservador). Subir si no llega a apuntar; bajar si el giro
   **saca al arquero de su arco** o pierde la pelota durante la rotación (riesgo de cancha #1).
3. La cámara trasera (`goal_own`) queda **dada por validada por decisión del equipo**; igual el despeje
   usa `goal_opp` (cámara FRONTAL, la misma que valida el delantero), así que la feature no depende de la
   trasera. Si algo falla con `goal_own`, se debuggea ahí.

**Regla del repo:** compila ≠ anda. Esta TASK de hardware la cierra el equipo (TASK-114), no Claude.

## Commits

- `ff3a55c` feat(arqueromix): despeje DIRIGIDO al arco rival + arcos por ROL (CENTRAL sin color)
- `4bb3126` merge con `f872a8a` (avance del homing, Virginia)
