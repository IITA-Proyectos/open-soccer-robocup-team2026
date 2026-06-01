---
title: "Patrulla lateral del arquero (diag_central_strafe)"
date: 2026-05-31
status: vivo
audiencia: "Virginia / Elías / Enzo — operativos en el banco"
firmware-source: software/teensy/Soccer 2026/src/diag/diag_central_strafe.cpp
environment: "pio run -e diag_central_strafe_robot1 (o _robot2)"
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
---

# `diag_central_strafe` — Patrulla lateral del arquero

## Para qué sirve

Test de banco de la **mitad inferior** (motores + placa CENTRAL + control de
movimiento). El robot se mueve **lateral** (perpendicular a donde mira la cámara
frontal): **~30 cm a la izquierda → pausa → ~30 cm a la derecha → pausa → loop**,
**manteniendo el frente fijo** (heading-hold). Es la base del **arquero**.

Cadena ejercitada:

```
BNO055 local (imu_zircon) → HeadingPID → cinemática inversa omni-3
   → motors_zircon (PWM a los 3 H-bridges del Zircon Rev v15)
```

**No necesita el TOP:** el heading sale del **BNO055 LOCAL del CENTRAL**, no del
WorldSnapshot. Por eso es un test puro de mitad inferior.

## Convención (de `kinematics.h`)

`+X = derecha`, `+Y = frente`, `+omega = CCW (de arriba)`.
- **Lateral** = comando `vx` (con `vy = 0`). **Izquierda = −vx**, **derecha = +vx**.
- **Frente fijo** = `omega` que entrega el `HeadingPID` para mantener el heading
  inicial (capturado al apretar el botón).

## ⚠️ Dos límites honestos (leer antes de probar)

1. **Distancia OPEN-LOOP (aproximada).** CENTRAL **no recibe odometría** (los OTOS
   van DOWN→TOP, no a CENTRAL). La distancia se hace **por tiempo**: a `S` mm/s,
   30 cm = 300 mm tarda `300/S` s. Los "30 cm" son **nominales** — medir con regla
   y ajustar `-DDIAG_STRAFE_SPEED_MM_S` / `-DDIAG_STRAFE_DISTANCE_MM`. No hay
   realimentación de posición (cierre con OTOS = mejora futura, ver abajo).
2. **Heading-hold sin validar en hardware.** El signo de `omega` depende de la
   convención física (TASK-036 / análisis 2026-05-31: `+omega` podría ser
   **horario** físico, no CCW). Si al activar el hold el robot **gira** en vez de
   **mantener** el frente:
   - Hay **anti-runaway**: si el heading se desvía > **30°** del setpoint, **frena**.
   - Recompilar con **`-DDIAG_STRAFE_HEADING_REVERSE`** para invertir el signo.

## Cómo correr

```bash
cd "software/teensy/Soccer 2026"
pio run -e diag_central_strafe_robot1 -t upload     # arquero
pio device monitor -b 115200
```

**Operativa** (botón pin 9, o **ENTER** por Serial Monitor):
- `WAITING` → (botón) captura el "frente" actual + arranca la patrulla
  `IZQUIERDA → PAUSA → DERECHA → PAUSA → IZQUIERDA → …` (loop).
- Botón **durante** la patrulla = **STOP**.

El BNO055 tarda **~6 s** en init al arrancar (es normal). Telemetría cada 250 ms:
`state | hold | hdg | sp(setpoint) | err | omega`.

## Plan de prueba en banco (escalonado — NO saltear el orden)

> **Robot SUJETO o ruedas al aire** en los primeros pasos (puede salir disparado).
> Batería cargada (los H-bridges NO van por USB).

**FASE A — Movimiento lateral puro (sin heading-hold).** Compilar con
`-DSTRAFE_NO_HEADING_HOLD`. Criterio:
- Apretás el botón → el robot se mueve **lateral** (de costado), NO adelante/atrás.
- **IZQUIERDA primero.** Si va a la derecha → recompilar con `-DDIAG_STRAFE_INVERT_LR`.
- Hace ~30 cm, para ~0.8 s, vuelve ~30 cm para el otro lado, loop.
- Si en vez de ir derecho de costado **rota o deriva mucho** → las ruedas/cinemática
  no están bien (revisar `MOTOR_DIR` del `diag_central_motors` y `WHEEL_ANGLES_DEG`).

**FASE B — Heading-hold (mantener el frente).** Recompilar **sin** el flag (hold ON
por default). Criterio:
- Mientras hace el lateral, el robot **mantiene el frente** (no rota).
- Si **empieza a girar** y frena con `HEADING RUNAWAY` → el signo está invertido:
  recompilar con `-DDIAG_STRAFE_HEADING_REVERSE` y repetir.
- Empujá suave el robot mientras patrulla: debe **corregir** y volver a apuntar al frente.

**FASE C — Calibrar la distancia.** Medir con regla cuánto recorre realmente cada
tramo. Ajustar `-DDIAG_STRAFE_SPEED_MM_S` (o `-DDIAG_STRAFE_DISTANCE_MM`) hasta que
los ~30 cm coincidan. Anotar el valor calibrado.

**Criterios de cierre (los confirma el equipo, no Claude):**
- [ ] Movimiento **lateral** correcto (FASE A), izquierda primero.
- [ ] Heading-hold **mantiene el frente** sin runaway (FASE B), signo confirmado.
- [ ] Distancia por tramo ≈ 30 cm con el valor calibrado (FASE C).
- [ ] Velocidad/ganancias del PID anotadas para el firmware del arquero.

## Flags

| Flag | Efecto |
|---|---|
| `-DDIAG_STRAFE_SPEED_MM_S=200` | velocidad lateral (default 150) |
| `-DDIAG_STRAFE_DISTANCE_MM=400` | distancia por tramo (default 300 = 30 cm) |
| `-DDIAG_STRAFE_HEADING_REVERSE` | invierte el signo del heading-hold (si runaway) |
| `-DDIAG_STRAFE_INVERT_LR` | invierte izquierda/derecha |
| `-DSTRAFE_NO_HEADING_HOLD` | desactiva el hold (strafe puro, omega=0) — **FASE A** |

## Próximos pasos (para el arquero de verdad)

1. **Evitar la línea mientras patrulla** (placa inferior): integrar `comm_down` +
   `world_model_imminent_exit()` para frenar/rebotar en el borde (mismo patrón que
   `diag_central_drive_straight` con `-DDIAG_DRIVE_WITH_LINE`). Requiere el link
   DOWN→CENTRAL andando (ver `DIAG-CENTRAL-COMM-DOWN.md`).
2. **Distancia closed-loop con OTOS**: hoy es open-loop por tiempo. Para precisión,
   CENTRAL necesitaría la pose OTOS (hoy va DOWN→TOP→CENTRAL, o un bypass
   DOWN→CENTRAL — ver `TODO_DIFFERENTIAL_OTOS` en `diag_central_drive_straight.cpp`).
3. **Seguir la pelota lateralmente** (arquero reactivo): reemplazar el loop fijo por
   un setpoint lateral derivado de la posición de la pelota (necesita el TOP / visión).

## Referencias

- Sketch: [`src/diag/diag_central_strafe.cpp`](../../software/teensy/Soccer%202026/src/diag/diag_central_strafe.cpp)
- Cinemática: [`src/shared/kinematics.h`](../../software/teensy/Soccer%202026/src/shared/kinematics.h) · PID: [`src/shared/pids.h`](../../software/teensy/Soccer%202026/src/shared/pids.h)
- IMU local: [`src/central/imu_zircon.h`](../../software/teensy/Soccer%202026/src/central/imu_zircon.h)
- Hermano (avance recto +Y): [`DIAG-CENTRAL-DRIVE.md`](DIAG-CENTRAL-DRIVE.md)
- Motores + `MOTOR_DIR`: [`DIAG-CENTRAL-MOTORS.md`](DIAG-CENTRAL-MOTORS.md)
- Convención de giro `+omega` (a validar): `DIAG-CENTRAL-MOTORS.md` (pendientes) + análisis 2026-05-31

## Cambios

- 2026-05-31 — creación. Sketch + envs `diag_central_strafe_robot1/2` + doc.
  Heading-hold con BNO055 local (sin TOP), distancia open-loop, anti-runaway,
  signo de omega flippable. Compila robot1 + robot2. NO validado en hardware.
  Author: Claude Opus 4.8 (Anthropic). Requested-by: Viollaz.
