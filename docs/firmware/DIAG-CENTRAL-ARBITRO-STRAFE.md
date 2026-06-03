---
title: "Patrulla lateral del arquero gatillada por el ÁRBITRO (diag_central_arbitro_strafe)"
date: 2026-06-03
status: vivo
audiencia: "Virginia / Elías / Enzo — operativos en el banco"
firmware-source: software/teensy/Soccer 2026/src/diag/diag_central_arbitro_strafe.cpp
environment: "pio run -e diag_central_arbitro_strafe_robot1 (o _robot2)"
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
---

# `diag_central_arbitro_strafe` — Patrulla del arquero con START/STOP del árbitro

> **🔬 Resultado de banco 2026-06-03:** ✅ **el gate del árbitro ANDA** (START/STOP
> mueve/frena la CENTRAL). ⚠️ **Al moverse solo gira el motor 1**: por la cinemática
> `{60,-60,180}` un lateral puro da **M3=0 (esperado**, rueda trasera kiwi) y
> M1=M2=±0.866·vx, pero a `vx=150`/`MAX_SPEED=1000` el PWM es ~13% → M1 raspa, **M2
> stalled (deadzone)**. **Primer test: subir velocidad** `-DDIAG_ARB_SPEED_MM_S=600`.
> Detalle: journal `2026-06-03-banco-resultados-arbitro-strafe-y-bno-freeze.md` + TASK-101.

## Qué hace (lo pedido)

- **Espera el START** del árbitro.
- Al recibir **START**: ~30 cm a la **IZQUIERDA** → para **3 s** → ~30 cm a la
  **DERECHA** → para **3 s** → y así (patrulla lateral del arquero).
- Al recibir **STOP**: **frena** y queda esperando (el freno es inmediato, no
  espera a terminar el tramo).
- **START de nuevo** → retoma la patrulla desde la izquierda.

Es el hermano de [`diag_central_strafe`](DIAG-CENTRAL-STRAFE.md), pero **gatillado
por el árbitro** en vez del botón, y con **pausa de 3 s**.

## De dónde sale el START/STOP (leer — no es obvio)

La CENTRAL **NO recibe el árbitro directo**. El árbitro entra como **nivel GPIO en
los pines 5/6 del TOP**; el TOP arma `match_running` y lo manda en el flag
**MATCH_RUNNING** del `WorldSnapshot`. La CENTRAL lo lee por **Serial7 (pin 28)**
(`comm_top` → `world_model_match_running()`).

```
COMM --(GPIO pines 5/6)--> TOP --(WorldSnapshot flag MATCH_RUNNING, Serial7)--> CENTRAL
```

**Fail-safe:** el árbitro real solo cuenta si el snapshot del TOP está **fresco**
(`world_model_snapshot_is_fresh()`). Si el TOP se cae → STOP automático.

## Override de banco (probar el MOVIMIENTO sin COMM/TOP)

Como la cadena del árbitro necesita COMM **y** TOP, hay un override por el Serial
Monitor para probar el motor solo:

| Tecla | Efecto |
|---|---|
| **`s`** | START manual (corre la patrulla aunque no haya árbitro real) |
| **`x`** (o `p`) | STOP manual (vuelve a depender del árbitro real) |

El robot corre si **(árbitro real = RUN)** **O** **(override manual = START)**. Así:
- Para probar el **movimiento** en banco sin nada más: mandá **`s`**.
- Para probar la **cadena real** del árbitro: dejá el manual en STOP y que el
  START llegue del COMM/TOP (mirá `arbitro=RUN(arbitro real)` en la telemetría).

## Cableado / pre-requisitos

- **Para la cadena real del árbitro:** TOP corriendo y emitiendo `WorldSnapshot`
  (cable TOP TX4/pin 17 → CENTRAL **pin 28**, GND común) + COMM señalando los
  pines 5/6 del TOP. Confirmar con `diag_central_rx_all` que `flags` trae
  `MATCH_RUNNING` cuando el árbitro manda START.
- **Para el override manual:** solo la CENTRAL + motores + batería.
- **Batería cargada** (los H-bridges NO van por USB). **SUJETAR el robot** o
  ruedas al aire en el primer run.

## Cómo correr

```bash
cd "software/teensy/Soccer 2026"
pio run -e diag_central_arbitro_strafe_robot1 -t upload     # arquero
pio device monitor -b 115200
#   mandar 's' para probar el movimiento (START manual), 'x' para frenar.
#   o mandar START desde el arbitro real y mirar arbitro=RUN(arbitro real).
```

Telemetría cada 250 ms: estado | árbitro (real/manual/STOP) | snap_fresh | match_flag | salud del link TOP.

LED: parpadeo lento = esperando START · fijo = moviéndose · parpadeo rápido = pausa.

## ⚠️ Caveats (mismos que diag_central_strafe)

1. **OPEN-LOOP, sin heading** (la CENTRAL no tiene BNO): `omega=0`. Cualquier
   rotación residual es **deriva** y no se corrige. Distancia **por tiempo**
   (nominal — calibrar con regla y `-DDIAG_ARB_SPEED_MM_S`/`-DDIAG_ARB_DISTANCE_MM`).
2. **Cinemática TENTATIVA**: usa `inverse_kinematics(WHEEL_ANGLES_DEG={60,-60,180})`
   sin calibrar. El banco 2026-06-01 vio **círculos** + **M2 con polaridad invertida
   por HW**. El "lateral" puede salir diagonal/rotando por la cinemática, no solo
   por deriva. Reconciliar el substrato de movimiento → **TASK-101**.

## Plan de prueba en hardware real

**Subsistema:** motor + CENTRAL + (árbitro vía TOP/COMM) · **Robot:** arquero (ROBOT1).

**Pasos:**
1. **Movimiento solo (override):** flashear, ruedas al aire, mandar `s`.
   - ¿Se mueve **lateral** (no adelante/atrás)? ¿IZQUIERDA primero? (si va al revés
     → recompilar con `-DDIAG_ARB_INVERT_LR`).
   - ¿Cada tramo ≈ 30 cm? ¿Pausa ≈ 3 s? Mandar `x` → ¿frena en el acto?
2. **Cadena real del árbitro:** con TOP + COMM, mandar START del árbitro.
   - ¿La telemetría muestra `arbitro=RUN(arbitro real)` y arranca la patrulla?
   - Mandar STOP del árbitro → ¿frena? Desconectar el cable del TOP → ¿frena por
     fail-safe (snap_fresh=N)?

**Criterio de aceptación (medible):**
- Override `s`/`x` arranca/frena la patrulla de forma confiable.
- Con la cadena real: START del árbitro arranca < 200 ms; STOP frena < 200 ms.
- Fail-safe: TOP caído (snap no fresco) → robot detenido.
- Tramo 30 cm ± tolerancia con la velocidad calibrada; pausa 3 s.

**Regression check:** `central_robot1` sigue compilando; el árbitro real (pines
5/6 → TOP → snapshot) sigue leyéndose en `diag_central_rx_all`.

**Documentación esperada:** journal `journal/YYYY-MM-DD-arbitro-strafe-<desc>.md`
con video + qué disparó el movimiento (manual vs árbitro real) + deriva observada.

## Flags

| Flag | Efecto |
|---|---|
| `-DDIAG_ARB_SPEED_MM_S=200` | velocidad lateral (default 150) |
| `-DDIAG_ARB_DISTANCE_MM=400` | distancia por tramo (default 300 = 30 cm) |
| `-DDIAG_ARB_PAUSE_MS=3000` | pausa entre tramos (default 3000 = 3 s) |
| `-DDIAG_ARB_INVERT_LR` | invierte izquierda/derecha |

## Referencias

- Sketch: [`src/diag/diag_central_arbitro_strafe.cpp`](../../software/teensy/Soccer%202026/src/diag/diag_central_arbitro_strafe.cpp)
- Hermano por botón: [`DIAG-CENTRAL-STRAFE.md`](DIAG-CENTRAL-STRAFE.md)
- Ver el árbitro llegar a la CENTRAL: [`DIAG-CENTRAL-RX-ALL.md`](DIAG-CENTRAL-RX-ALL.md)
- Caveats de cinemática / fork del arquero: `team-tasks/2026-06-03-task-101-banco-mitad-inferior-cinematica-y-fork-arquero.md`

## Cambios

- 2026-06-03 — creación. Sketch + envs `diag_central_arbitro_strafe_robot1/2` + doc.
  Gate por `world_model_match_running()` (snapshot del TOP) + override manual Serial
  `s`/`x`. Compila robot1 + robot2 SUCCESS. NO validado en hardware (lo corre el
  equipo). Author: Claude Opus 4.8 (Anthropic). Requested-by: Viollaz.
