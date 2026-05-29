---
id: TASK-037
title: "Correr diag_central_drive_straight en banco — validar cadena TOP→CENTRAL→motores con PID heading"
date_created: 2026-05-29
assigned: [virginia-viollaz, elias, enzo]
priority: P1
status: pending
estimated_hours: 2
blocks: ["definición de movimientos vectoriales (próxima sesión)", FSM táctica con corrección de heading real]
blocked_by: [TASK-036 (motores validados), TOP corriendo + mandando WorldSnapshot por Serial1, cable UART TOP→CENTRAL conectado, conflicto pines 7/8 resuelto si se usa -DDIAG_DRIVE_WITH_LINE]
tags: [hardware, central-board, motion, pid, heading, integracion-3-placas]
related: [team-tasks/2026-05-29-task-036-correr-diag-central-motors-en-banco.md, team-tasks/2026-05-24-task-031-verificar-uart-down-top-central.md, hardware/electronics/central-board-pack/01-pinout-y-hardware.md]
---

# TASK-037 — Correr `diag_central_drive_straight` en banco

## Resumen

Sketch nuevo listo en el repo el 2026-05-29 (sesión Claude, requested-by
Gustavo). Hace falta que el equipo humano lo ejecute en banco con:

1. Placa CENTRAL flasheada con este sketch.
2. Placa TOP corriendo y enviando `WorldSnapshot` por Serial1 a 230400 baud.
3. Robot SUJETO al banco / ruedas al aire / cancha despejada (avanza ~900 mm
   en 3 s a 300 mm/s).

El sketch hace una secuencia simple controlada por botón pin 9:

```
WAITING -> [boton] -> FORWARD 3s (PID heading) -> PAUSED 1s -> REVERSE 3s -> DONE
```

Sirve para:
- Validar end-to-end la cadena TOP→CENTRAL→motores.
- Tunear las ganancias `Kp/Ki/Kd` del HeadingPID.
- Confirmar que `WHEEL_ANGLES_DEG` y `WHEEL_RADIUS_MM` en `config_central.h`
  matchean el robot armado físicamente.
- Identificar deriva del sensor de orientación fusionado por el TOP.

## Pre-requisitos (ORDEN ESTRICTO)

1. **TASK-036 cerrada** — `diag_central_motors` corrió, los 3 motores
   validados, mapeo motor↔rueda físico conocido, conflicto pines 7/8
   resuelto (Caso A o B según el doc). **Sin esto los datos del PID son
   basura** porque un motor caído impide compensar.
2. **Placa TOP operativa** — firmware `pio run -e top -t upload` corriendo,
   `pio device monitor` muestra que arma y emite `WorldSnapshot`.
3. **Cable UART** TOP↔CENTRAL conectado (pin 0 RX1 / pin 1 TX1 del CENTRAL).
4. **Batería cargada** — los H-bridges NO se alimentan por USB.
5. **Espacio físico** o robot sujeto.
6. **Excepción Avast** (TASK-025) aplicada si PIO se traba.

## Procedimiento

Detalles completos en
[`docs/firmware/DIAG-CENTRAL-DRIVE.md`](../docs/firmware/DIAG-CENTRAL-DRIVE.md).
Resumen:

```bash
cd "software/teensy/Soccer 2026"
pio run -t clean -e diag_central_drive_robot1     # o robot2
pio run -e diag_central_drive_robot1 -t upload
pio device monitor -b 115200
```

Apretás el botón (o `<enter>` por Serial Monitor):
- Captura heading actual como setpoint.
- Arranca FORWARD: avanza +Y con PID heading.
- A los 3 s (o segundo apretón): pausa 1 s.
- REVERSE: vuelve -Y manteniendo mismo setpoint.
- 3 s (o tercer apretón): FIN.

## Criterio de cierre

1. El sketch corrió de punta a punta (FORWARD + PAUSED + REVERSE) sin que
   se dispare el watchdog de snapshot.
2. Entrada de journal `journal/2026-05-29-diag-central-drive-<descripcion>.md`
   con:
   - Foto/video del trayecto.
   - Captura del Serial Monitor (mostrar evolución del `err` durante FW + RV).
   - Veredicto: ¿anduvo recto? Cuánto se desvió. Qué ganancias quedaron.
   - **Calibración resultante** del `Kp/Ki/Kd` final (si se modificó).
3. Si el robot NO anduvo recto:
   - Identificar la causa (motor caído / cinemática mal / PID flojo / drift
     de sensor / snapshot lento) usando la tabla de diagnóstico del doc.
   - Abrir TASKs nuevas según corresponda.
4. Si se cambian ganancias del PID, **portar al `pids.h` vivo** (TASK aparte).

## Por qué es P1 (no P0)

Bloquea:
- La próxima sesión Claude de "definición de movimientos vectoriales" —
  sin saber si la cadena anda y la cinemática es correcta, definir
  movimientos diagonales o curvas es papel.
- FSM táctica realista con corrección de heading (post-Incheon mejoras).

No es P0 porque NO bloquea la homologación de Incheon (eso es COMM +
cámara). Pero es alta prioridad para que el robot juegue con calidad.

Tiempo estimado: **2 h** incluyendo tuning iterativo de ganancias del PID.

## Atribución del sketch + docs

- Sketch + envs + docs: Claude Opus 4.7 (Anthropic), sesión 2026-05-29
  (requested-by Gustavo Viollaz @gviollaz).
- **La ejecución del test la hace el equipo humano que tiene la placa** —
  Claude NO cierra TASKs de hardware (regla 1 de CLAUDE.md).
