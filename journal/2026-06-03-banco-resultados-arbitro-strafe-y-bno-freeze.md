---
title: "Banco 2026-06-03: árbitro mueve la CENTRAL (HITO) + solo gira M1 + BNO del TOP se congela"
date: 2026-06-03
author: "Claude Opus 4.8 (Anthropic), banco operado por Gustavo Viollaz"
ai-assisted: true
status: completado
tags: [banco, central, arbitro, match-running, strafe, cinematica, motores, bno, freeze, rx-all, comm]
robot: ambos (Zircon Rev v15 + Teensy 4.1 / TOP Teensy 4.0)
---

# Banco 2026-06-03 — árbitro → CENTRAL + solo M1 + freeze del BNO

Sesión de banco con las 3 placas. Dos programas nuevos de esta semana corridos
en hardware real. Resultados (lo que el equipo OBSERVÓ, no lo que "debería"):

## 🏁 HITO — el árbitro mueve a la CENTRAL

`diag_central_arbitro_strafe` corrió y **el gate del árbitro ANDA**: el START/STOP
llega a la CENTRAL y dispara/frena la conducta. Es la **primera vez** que la
señal del árbitro (COMM → GPIO pines 5/6 → TOP → flag MATCH_RUNNING del
WorldSnapshot → Serial7 → CENTRAL) **mueve el robot**. La cadena de control de
partido de punta a punta quedó demostrada.

## ⚠️ Hallazgo — al moverse, SOLO gira el motor 1 (M2 y M3 quietos)

Con el árbitro en START, la patrulla lateral comanda y **solo se mueve el motor 1;
el 2 y el 3 quedan quietos.** (Lo debugean mañana — esta entrada deja la pista.)

**Diagnóstico (fundamentado en el código, NO probado en banco todavía):**

El movimiento va por `motors_apply_command()` → `inverse_kinematics()`
(`src/shared/kinematics.cpp:12`): `wheel[i] = -vx·sin(θ) + vy·cos(θ) + ω·R`, con
`WHEEL_ANGLES_DEG = {60, -60, 180}` (`config_central.h:72`, TENTATIVO). Para un
**lateral puro** (vx, vy=0, ω=0):

| Motor | θ | wheel speed | PWM @ vx=150, MAX=1000 | Resultado |
|---|---|---|---|---|
| **M1** (idx 0) | 60° | −vx·0.866 | ~33/255 (13%) | gira (el más libre arranca) |
| **M2** (idx 1) | −60° | +vx·0.866 | ~33/255 (13%) | **stalled** (debajo del umbral) |
| **M3** (idx 2) | 180° | −vx·sin(180)=**0** | **0** | **quieto — CORRECTO** (rueda trasera kiwi no aporta a lateral puro) |

Dos efectos apilados:
1. **M3 = 0 es esperado**, no es bug: la rueda a 180° (trasera, layout kiwi)
   tiene proyección nula en el eje lateral. Para straferar puro, la trasera no
   trabaja.
2. **M1 y M2 reciben el MISMO comando** (±0.866·vx), pero a `vx=150` con
   `MAX_SPEED_MM_S=1000` el PWM es **~13% (33/255)** — debajo del arranque típico
   de los motores. M1 (menos fricción) raspa y gira; M2 se queda clavado.

Es el **mismo síntoma de "no straferea / da círculos" del 2026-06-01**, ahora
explicado a nivel de rueda: PWM demasiado bajo + geometría kiwi.

**Pin path descartado:** `config_central.h` ROBOT1 da M2 = INA8/INB7/PWM6,
**idéntico** a los pines hardcodeados que `diag_central_motors` validó (M2 gira en
ese test). Así que M2 NO es un problema de pin/cableado en el path de producción.

**Primer test de mañana (gatea el resto):**
- Subir velocidad: `pio run -e diag_central_arbitro_strafe_robot1 ... -DDIAG_ARB_SPEED_MM_S=600`
  (≈52% PWM en el par delantero).
  - Si M1 **y** M2 se mueven y M3 sigue casi quieto → confirmado (deadzone +
    geometría kiwi). El "straferar puro" con la trasera quieta es esperable.
  - Si M2 **sigue muerto** a PWM alto → ahí sí es M2 (polaridad INA/INB invertida
    por HW del 2026-06-01 / driver). Revisar con `diag_central_motors` aislando M2.
- Decisión de fondo (TASK-101): la cinemática genérica `{60,-60,180}` está sin
  calibrar (Enzo) y el arquero que SÍ anduvo usa **control directo** de motores.
  Reconciliar substrato antes de confiar en los 30 cm.

## ⚠️ Hallazgo — el heading del BNO (del TOP) se congela en producción

Corriendo `top_robot1` + `diag_central_rx_all` (con el veredicto de completitud),
el snapshot del TOP llega **sano** (Serial7: 0 CRC err, frames fluyendo, age
5–26 ms) y `x/y/min_obst` cambian, pero **`hdg` quedó clavado en −108.3°** en
todos los snapshots. El árbitro angle "venía y dejó de venir".

- **No es la CENTRAL ni el enlace** — es el **yaw del BNO congelado en el TOP**.
- **Causa raíz documentada en el propio firmware** (`src/top/sensors_imu.cpp:167-172,
  231-235`): el BNO055 y los VL53L7CX **no coexisten** bien en el bus `Wire`; con
  los ToF rangeando, el read multi-byte del BNO se corrompe y el yaw queda
  **congelado**. El band-aid (I²C 100 kHz + leer el BNO a 20 Hz) **baja** las
  colisiones pero **no las elimina** → bajo carga de producción eventualmente
  wedgea el bus y late-quea.
- **Es scope del agente TOP, no CENTRAL.** Fix de fondo (ya anotado en el código):
  **BNO en bus aparte (`Wire1`)**. Fix intermedio: watchdog de yaw congelado
  (re-init del bus si el heading no cambia mientras el gyroZ dice que rota).
- **Implicación para CENTRAL:** para el heading-hold del arquero, **no depender
  del BNO/TOP** (es el que se congela). El **OTOS de DOWN llega vivo y local** a
  la CENTRAL (en el rx_all el `OTOS hdg` sí se movía), aunque driftea con patinaje
  (`slip` llegó a 17). Evidencia a favor del v2-con-OTOS.

## Observación secundaria — seqGap ~33% en el link DOWN→CENTRAL

El `diag_central_rx_all` mostró ~33% de `seqGap` en el enlace de DOWN con **0 CRC
err**. No es corrupción: `down_tx.cpp:25` incrementa el SEQ **aunque DESCARTE** el
frame por backpressure (`availableForWrite`, `:32-37`) → **cada frame que DOWN
dropea = un seqGap que ve la CENTRAL**. Dato fresco igual (age 2–7 ms, ~100 Hz por
mensaje), así que **no es crítico**. Confirmar comparando `down_tx_get_dropped(0)`
de DOWN vs el seqGap de la CENTRAL (si coinciden → backpressure de DOWN; si DOWN
dropea ~0 → la CENTRAL no drena a tiempo). Scope DOWN/coordinado.

## Estado de los programas usados

- `diag_central_arbitro_strafe` (nuevo, commit `02d68e5`): gate del árbitro
  **validado en banco** ✓; movimiento con el defecto de arriba (solo M1).
- `diag_central_rx_all` con veredicto de completitud (commit `768ec0f`):
  ⚠️ el binario flasheado en esta sesión era el ANTERIOR al veredicto (el panel
  no mostró el bloque `[OK]/[STALE]/[FALTA]`). Reflashear para tenerlo.

## Próximos pasos

1. **(mañana, equipo)** subir velocidad del strafe y reconfirmar M2 (gatea TASK-101).
2. **(TOP)** rutear el freeze del BNO al agente TOP (bus aparte / watchdog de yaw).
3. **(CENTRAL, mío)** v2: heading-hold con OTOS en la patrulla del arquero, ahora
   con evidencia de que el OTOS es la fuente viable (el BNO se congela).
4. confirmar el seqGap del link DOWN (dropped counter de DOWN).

## Atribución

Análisis + sketches + docs: Claude Opus 4.8 (Anthropic), requested-by Gustavo
Viollaz (@gviollaz). Banco operado por el equipo humano. Claude NO cierra TASKs de
hardware (regla 1 CLAUDE.md) — el "solo M1" y el freeze del BNO quedan abiertos
para el equipo.
