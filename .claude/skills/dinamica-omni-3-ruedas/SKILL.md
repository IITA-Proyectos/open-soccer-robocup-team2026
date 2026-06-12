---
name: dinamica-omni-3-ruedas
description: Use when changing ANY motion control on the CENTRAL board (strafe, rotation, heading hold, PID gains, speeds) of the IITA omni-3 robots - or when the robot drifts, rotates parasitically while strafing, moves "aplastado" in diagonal, or behaves differently below/above certain speeds. Contains the measured plant model - wheel geometry, PWM floors, FLOOR_SCALE behavior, parasitic yaw, physical minimums, validated speed regimes from bench data.
---

# Dinámica del robot omni-3 IITA (planta MEDIDA — banco 2026-05/06)

## Principio central

Este robot NO es un omni ideal: tiene **pisos de PWM por rueda** que cuantizan el
actuador. TODO diseño de control debe elegir primero el **RÉGIMEN** en que opera:

| Régimen | Velocidad | Comportamiento | Control viable |
|---|---|---|---|
| **Cuantizado** | < ~420 mm/s | Los comandos se aplastan/escalan a los pisos; el giro es casi todo-o-nada | Strafe puro + correcciones por PULSOS o PFM (ver `control-pid-zona-muerta`) |
| **Proporcional** | ≥ ~420 mm/s | La rueda dominante queda SOBRE su piso → el mixer respeta proporciones | PID continuo clásico funciona (así corre el delantero a 400-700) |

## La planta (números medidos)

- **Geometría:** 3 ruedas, `WHEEL_ANGLES_DEG={330,210,90}` (M1 del-IZQ, M2 del-DER,
  M3 trasera). Strafe lateral puro: fronts a 0.5·vx, trasera a 1.0·vx.
- **Pisos de PWM:** `MOTOR_MIN_PWM={70,70,107}` (ambos robots; trasera alineada pide
  ~1.5× el PWM de las oblicuas, no 2×, por fricción). Bajo el piso una rueda NO gira.
- **`CENTRAL_FLOOR_SCALE`** (envs del arquero): escala UNIFORME el vector de PWMs para
  que la dominante alcance su piso → dirección fiel, pero **amplifica también la
  componente omega** del comando (una corrección chica sale multiplicada).
- **Deriva parásita del strafe: ~80°/s de yaw** (asimetrías de fricción/montaje). Es
  SISTEMÁTICA → cancelable con feedforward/integrador, NO con P puro capado bajo.
- **Rotación mínima física con pisos: ~300°/s** — el robot no sabe girar despacio
  parado; los giros finos van por pulsos cortos (40-80 ms) + asentamiento.
- **Kickstart** `{130,130,140}` PWM ×40 ms en cada arranque parado→movimiento
  (gateado, activo en producción): el primer golpe de cualquier comando es FUERTE.
- Motores brushed 5V alimentados a 7,4 V: **NO sostener >~150 PWM** (quemado).

## Hechos validados en banco (no re-descubrir)

- Strafe puro (ω=0) a 200 mm/s con mezcla {70,70,107}: **derecho y estable** ✅.
- Strafe + ω continuo capado a 40°/s: **runaway** (pierde contra los 80°/s) ❌ (2026-06-12).
- Strafe + ω continuo P=3 capado a 120°/s: **oscila ±140° y trompea** (cuantización) ❌ (2026-06-12).
- Heading hold continuo del delantero a 400-700 mm/s: **funciona** ✅ (régimen proporcional).
- Pulsos parados 35°→ corte en vivo + settle 700 ms: **estables** ✅ (patrulla v3.3).
- Recto-atrás con gyro-hold ≤10°/s: estable; trims traslacionales tope ±19 mm/s
  (más dispara la trasera por su piso).
- Convenciones: +Y=frente, +X=derecha, ω CCW+; `omega*100` viaja en int16 → clamp
  total ≤327°/s o desborda con signo invertido.

## Errores comunes

- Diseñar control "de libro" ignorando el régimen → falla con síntomas confusos.
- Mezclar ω con strafe lento sin PFM/pulsos → trompo u oscilación (medido 2×).
- Comandar < ~420 mm/s en diagonal: las componentes dispares se aplastan a pisos →
  dirección basura (la lección de los "círculos" del GOTO_LINE diagonal).
- Olvidar que FLOOR_SCALE amplifica el omega junto con el strafe.

**REQUIRED SUB-SKILL:** para diseñar el lazo sobre esta planta usar
`control-pid-zona-muerta` (PFM, deadband, PI-feedforward, titración de banco).
