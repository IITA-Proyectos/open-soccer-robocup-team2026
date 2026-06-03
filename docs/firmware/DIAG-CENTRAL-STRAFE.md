---
title: "Patrulla lateral del arquero (diag_central_strafe) — OPEN-LOOP"
date: 2026-05-31
status: vivo
audiencia: "Virginia / Elías / Enzo — operativos en el banco"
firmware-source: software/teensy/Soccer 2026/src/diag/diag_central_strafe.cpp
environment: "pio run -e diag_central_strafe_robot1 (o _robot2)"
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
---

# `diag_central_strafe` — Patrulla lateral del arquero (OPEN-LOOP)

## Para qué sirve

Test de banco de la **mitad inferior** (motores + placa CENTRAL + cinemática). El
robot se mueve **lateral** (perpendicular a donde mira la cámara frontal):
**~30 cm a la izquierda → pausa → ~30 cm a la derecha → pausa → loop**. Es la base
del **arquero**.

```
vx lateral → cinemática inversa omni-3 → motors_zircon (PWM a los 3 H-bridges)
```

## ⚠️ Es OPEN-LOOP, sin feedback de heading

El CENTRAL **no tiene BNO055** (el heading viene del TOP, que acá **no se usa**).
El **OTOS de la base ahora SÍ llega a CENTRAL** (broadcast simétrico desde DOWN,
2026-06-01), pero **este sketch v1 todavía no lo usa** (open-loop a propósito, para
validar primero el movimiento lateral simple). Entonces comanda **`omega = 0`**:

- Para un omni-3 con **ruedas parejas y bien calibradas**, `omega=0` es
  **traslación pura SIN rotar** → el robot "mira al frente" mientras se mueve de
  costado.
- Cualquier rotación residual es **deriva**, y **NO se corrige** (no hay sensor de
  heading en CENTRAL). Cuánto deriva es justamente uno de los datos que este test
  mide (FASE B).

> **Heading-hold activo = v2** (ver "Próximos pasos"): ahora es **viable sin TOP**
> porque el **OTOS ya llega a CENTRAL** (`pose_view.h` + `world_model`, broadcast
> 2026-06-01). Es un cambio **local en CENTRAL** (sumar un `HeadingPID` sobre el
> heading del OTOS) — ya NO requiere mensaje nuevo ni trabajo cross-board.

## ⚠️ Distancia OPEN-LOOP (aproximada)

CENTRAL no recibe odometría → la distancia se hace **por tiempo**: a `S` mm/s,
30 cm = 300 mm tarda `300/S` s. Los "30 cm" son **nominales** — medir con regla y
ajustar `-DDIAG_STRAFE_SPEED_MM_S` / `-DDIAG_STRAFE_DISTANCE_MM`.

## Convención (de `kinematics.h`)

`+X = derecha`, `+Y = frente`. Lateral = `vx` (con `vy = 0`).
**Izquierda = −vx**, **derecha = +vx**. Invertible con `-DDIAG_STRAFE_INVERT_LR`.

## Cómo correr

```bash
cd "software/teensy/Soccer 2026"
pio run -e diag_central_strafe_robot1 -t upload     # arquero
pio device monitor -b 115200
```

**Operativa** (botón pin 9, o **ENTER** por Serial Monitor):
- `WAITING` → (botón) arranca la patrulla `IZQUIERDA → PAUSA → DERECHA → PAUSA → …`
- Botón **durante** la patrulla = **STOP**.

Telemetría cada 250 ms: `state | vx`.

## Plan de prueba en banco

> **Robot SUJETO o ruedas al aire** al principio (puede salir disparado). Batería
> cargada (los H-bridges NO van por USB).

> ⚠️⚠️ **LEER ANTES DE INTERPRETAR LA DERIVA (FASE B).** Este sketch mueve los
> motores vía `motors_apply_command()` → `inverse_kinematics()` con
> `WHEEL_ANGLES_DEG={60,-60,180}` (marcado **TENTATIVO**, sin medir — `config_central.h:72`).
> La sesión de banco del **2026-06-01** ([journal](../../journal/2026-06-01-arquero-seguidor-linea-y-calibracion.md))
> ya encontró que **la cinemática genérica da CÍRCULOS** y que en ese robot el
> **motor M2 tiene la polaridad INA/INB invertida por hardware** — y
> `motors_zircon.cpp` **no** tiene inversión por motor. Por eso el movimiento
> lateral puede salir **en diagonal / rotando por la cinemática**, no sólo por la
> deriva open-loop. **Si pasa eso, la "deriva" de FASE B está dominada por la
> cinemática, no por la falta de heading-hold** — y la conclusión "hace falta v2"
> sería un falso positivo. El arquero que **sí** anduvo en banco
> (`diag_central_line_sweep`) usa **control directo** de motores, no esta
> cinemática. **Reconciliar el substrato de movimiento antes de concluir sobre v2**
> (ver "Riesgos" en el reporte coach 2026-06-03 / journal del día).

**FASE A — Movimiento lateral + dirección.**
- Apretás el botón → el robot se mueve **de costado** (NO adelante/atrás), **izquierda primero**.
- Si va a la derecha → recompilar con `-DDIAG_STRAFE_INVERT_LR`.
- Si en vez de ir derecho de costado **se va en diagonal o rota fuerte** → la
  cinemática / el sentido de las ruedas no están bien: revisar `MOTOR_DIR`
  (`diag_central_motors`) y `WHEEL_ANGLES_DEG` (`config_central.h`).

**FASE B — Medir la DERIVA de heading (clave para el arquero).**
- Marcá la orientación inicial (una cinta/flecha en el piso). Dejá que patrulle
  varios ciclos izquierda↔derecha.
- **¿Cuánto rota el robot** respecto del frente después de, p. ej., 5 ciclos?
  - Deriva chica (pocos grados) → el open-loop alcanza para el arquero; seguimos así.
  - Deriva grande (gira notoriamente) → hace falta **heading-hold activo (v2 con OTOS)**.
- Anotar los grados de deriva (es el dato que decide si v2 es necesario).

**FASE C — Calibrar la distancia.**
- Medir con regla cuánto recorre cada tramo. Ajustar `-DDIAG_STRAFE_SPEED_MM_S`
  (o `-DDIAG_STRAFE_DISTANCE_MM`) hasta que los ~30 cm coincidan. Anotar el valor.

**Criterios de cierre (los confirma el equipo, no Claude):**
- [ ] Movimiento **lateral** correcto (FASE A), izquierda primero.
- [ ] **Deriva de heading medida** y anotada (FASE B) → decide si hace falta v2.
- [ ] Distancia por tramo ≈ 30 cm con el valor calibrado (FASE C).
- [ ] Velocidad anotada para el firmware del arquero.

## Flags

| Flag | Efecto |
|---|---|
| `-DDIAG_STRAFE_SPEED_MM_S=200` | velocidad lateral (default 150) |
| `-DDIAG_STRAFE_DISTANCE_MM=400` | distancia por tramo (default 300 = 30 cm) |
| `-DDIAG_STRAFE_INVERT_LR` | invierte izquierda/derecha |

## Próximos pasos (para el arquero "de verdad")

1. **Heading-hold con OTOS (v2)** — si la FASE B muestra deriva grande. **Ya
   viable**: el OTOS llega a CENTRAL (`pose_view.h` + `world_model`, broadcast
   2026-06-01). Es un cambio **local en CENTRAL**: agregar un `HeadingPID` que
   corrige `omega` con el heading del OTOS (igual que `diag_central_drive_straight`,
   pero con la pose OTOS en vez del WorldSnapshot del TOP). Ya NO es cross-board.
2. **Evitar la línea mientras patrulla** (placa inferior) — integrar `comm_down` +
   `world_model_imminent_exit()` para frenar/rebotar en el borde (mismo patrón que
   `diag_central_drive_straight` con `-DDIAG_DRIVE_WITH_LINE`). Necesita el link
   DOWN→CENTRAL andando (ver `DIAG-CENTRAL-COMM-DOWN.md`).
3. **Seguir la pelota lateralmente** (arquero reactivo) — setpoint lateral derivado
   de la posición de la pelota (necesita TOP / visión).

## Referencias

- Sketch: [`src/diag/diag_central_strafe.cpp`](../../software/teensy/Soccer%202026/src/diag/diag_central_strafe.cpp)
- Cinemática: [`src/shared/kinematics.h`](../../software/teensy/Soccer%202026/src/shared/kinematics.h)
- Motores + `MOTOR_DIR`: [`DIAG-CENTRAL-MOTORS.md`](DIAG-CENTRAL-MOTORS.md)
- Hermano (avance recto +Y, con heading del TOP): [`DIAG-CENTRAL-DRIVE.md`](DIAG-CENTRAL-DRIVE.md)
- OTOS → CENTRAL (camino v2): `TODO_DIFFERENTIAL_OTOS` en `diag_central_drive_straight.cpp`

## Cambios

- 2026-05-31 — creación. Sketch + envs `diag_central_strafe_robot1/2` + doc.
  **OPEN-LOOP** (omega=0): el CENTRAL no tiene BNO y los OTOS van DOWN→TOP, así que
  no hay feedback de heading. Heading-hold activo con OTOS queda como v2. Compila
  robot1 + robot2. NO validado en hardware.
  Author: Claude Opus 4.8 (Anthropic). Requested-by: Viollaz.
