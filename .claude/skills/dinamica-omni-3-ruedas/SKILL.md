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
- **Deriva parásita del strafe: ~80°/s de yaw CRUDO** (lazo abierto; es la perturbación que el
  integrador/PI debe cancelar). Tras `{70,70,107}` + `MOTOR_EFF_X100` el RESIDUAL baja a ~8°/s
  (banco 2026-06-14). Es SISTEMÁTICA → feedforward/integrador, NO con P puro capado bajo.
- **Rotación mínima física con pisos: ~300°/s** — el robot no sabe girar despacio
  parado; los giros finos van por pulsos cortos (40-80 ms) + asentamiento.
- **Kickstart** `{145,145,150}` PWM ×40 ms (factor ×9.9) en cada arranque parado→movimiento
  (detrás de `-DCENTRAL_MOTOR_KICKSTART`, activo en producción): el primer golpe es FUERTE.
  Historia: 2025 factor 1.8 → 2026-06-09 `{130,130,140}` → 2026-06-14 `{145,145,150}` (el CÓDIGO
  manda). Para más impulso subir la VENTANA (ms), no el cap (térmico ~150).
- Motores brushed 5V alimentados a 7,4 V: **burn cap 150 PWM** (freno térmico, NUNCA subir).
- **`MOTOR_PWM_NOISE_THRESH = 5`** (|PWM|≤5 → manda 0, no zumba parado). **`MOTOR_EFF_X100`**
  `{100,100,131}` R1 / `{100,100,115}` R2 (eficiencia por rueda; palanca preferida sobre el piso
  para enderezar el strafe — no mete yaw parásita).

> **⚠️ ANTES DE TUNEAR motor/arquero: NO re-derivar — ya está medido y documentado.** Doc-prosa
> canónico de TODAS las perillas (rango + qué pasa al subir/bajar + qué NO hacer):
> **`docs/firmware/GUIA-DE-TUNING-CENTRAL.md`** y **`docs/firmware/MOTION-CONTROL-ACTUAL.md`**.
> Lazo de rumbo + latencias del arquero: **`docs/firmware/CONTROL-ARQUERO-LATERAL-Y-LATENCIAS.md`**.
> FSM/estrategia del arquero: **`docs/firmware/FSM-ARQUERO-ESTADOS.md`**. Fuente de verdad de los
> VALORES concretos = el código (`config_central.h`, `motors_zircon.cpp`, `strategy.cpp`).

## Hechos validados en banco (no re-descubrir)

- Strafe puro (ω=0) a 200 mm/s con mezcla {70,70,107}: **derecho y estable** ✅.
- Strafe + ω continuo capado a 40°/s: **runaway** (pierde contra los 80°/s) ❌ (2026-06-12).
- Strafe + ω continuo P=3 capado a 120°/s: **oscila ±140° y trompea** (cuantización) ❌ (2026-06-12).
- Heading hold continuo del delantero a 400-700 mm/s: **funciona** ✅ (régimen proporcional).
- Pulsos parados 35°→ corte en vivo + settle 700 ms: **estables** ✅ (patrulla v3.3).
- **Escape/strafe lateral: velocidad TOPE ~470 mm/s.** Arriba la trasera satura → la huida sale
  DIAGONAL (no recta). Para más distancia/despegue se sube el TIEMPO, NO la velocidad (banco María:
  600→900→1300→1700 ms). El "transitorio de arranque" de la trasera ES esta misma diagonal de saturación.
- **Trim traslacional (vy de corrección): tope físico ±19 mm/s.** Arriba la trasera se dispara a su
  piso 107 = patada lateral brusca. Vale para el heading-hold lateral y para el control de profundidad Y.
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
