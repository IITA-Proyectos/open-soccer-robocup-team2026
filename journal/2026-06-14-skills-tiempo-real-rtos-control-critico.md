---
title: "Skills nuevas: tiempo real / RTOS / control embebido / sistemas críticos (4 skills + 4 referencias)"
date: 2026-06-14
author: "Claude Opus 4.8 (1M context) (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M, Anthropic)"
status: completado
tags: [skills, tiempo-real, rtos, control, sistemas-criticos, embebido, automotriz, aeroespacial, transversal]
area: control
robot: ambos
tipo: decision
---

# Skills de tiempo real, RTOS, control embebido y sistemas críticos

## Qué se pidió

Gustavo pidió "crear todos los skills que necesites" para convertirse en experto en:
programación de sistemas en tiempo real, RTOS, sistemas de control (inyección
electrónica de vehículos), sistemas electromecánicos con electrónica embebida para
cajas de cambio con accionamiento electrónico, y sistemas aeroespaciales de control.

## Decisión de alcance (lo importante — es OTRO dominio que el robot)

Estos temas son de **ingeniería embebida de tiempo real en general** — la inyección
electrónica, la caja por cable y el control aeroespacial NO son el robot de fútbol, y
**el robot hoy es un superloop bare-metal, sin RTOS**. Siguiendo la regla de CLAUDE.md
("si aparece algo fuera del dominio soccer, aclarar antes de aplicar el frame"), se
ofreció elegir alcance (anclado al robot / general / otro repo). Sin respuesta, se tomó
el default recomendado: **crearlas en este repo, ancladas a los problemas de tiempo
real REALES del robot, con los 3 dominios (auto/caja/aero) como casos de referencia.**

Esto respeta el frame coach (las skills aterrizan en el robot), entrega la profundidad
pedida (los 3 dominios safety-critical), y capitaliza a 2027 + sirve para entender el
oficio. Si Gustavo prefiere moverlas a otro repo, se reubican.

## Lo que se creó (4 skills transversales, par-a-par como dinamica+control-pid)

1. **`tiempo-real-determinismo`** — la lente raíz: hard/firm/soft, latencia/jitter/WCET
   (peor caso, no promedio), qué mata el determinismo, superloop vs cyclic executive vs
   RTOS. Anclada en el loop del TOP a 6 Hz (I/O I²C bloqueante) y el freeze del BNO.
   + `references/medir-y-presupuestar-tiempo.md`.
2. **`rtos-scheduling-embebido`** — RTOS, RMS/EDF, inversión de prioridad (Pathfinder),
   sincronización, ISR→tarea, stacks. Veredicto: el robot NO necesita RTOS hoy.
   + `references/patrones-rtos-y-trampas.md`.
3. **`control-embebido-tiempo-real`** — la realización en tiempo real del lazo
   (muestreo, discretización, punto fijo Q-format, jitter del dt, latencia, multi-rate).
   Cuidadosamente diferenciada de `control-pid-zona-muerta` (tuning) y `dinamica-omni-3-ruedas`
   (planta). + `references/discretizacion-y-punto-fijo.md`.
4. **`sistemas-criticos-tolerancia-fallas`** — confiabilidad: estado seguro, watchdog,
   redundancia/votación, FDIR, degradación con gracia, normas (DO-178C/ISO 26262/MISRA).
   + `references/casos-inyeccion-caja-aeroespacial.md` (los 3 dominios como casos profundos).

Las 4 se cruzan entre sí con cláusulas NOT-for para no colisionar al auto-invocarse, y
referencian las skills/convenciones existentes del robot.

## Anclaje en problemas reales del robot (no teoría suelta)

- Loop del TOP 6 Hz→190k/s = I/O bloqueante en el lazo (4× `getRangingData()` I²C).
- Freeze del BNO = contención de bus + acceso bloqueante (no "sensor roto").
- Fail-safe del árbitro = nivel GPIO con `INPUT_PULLDOWN` → desconectar el cable cae a
  STOP por física (ejemplo de estado seguro de manual).
- `omega*100` int16 (clamp ≤327°/s) = overflow de punto fijo.
- Degradar con gracia = el arquero sin BNO navega por línea+cámara+OTOS.

## Índice actualizado (mismo commit)

- `CLAUDE.md` — lista de skills 18 → 22, sección nueva "Tiempo real / sistemas críticos
  (transversal)" con la nota de alcance.
- No se agregó fila a `FUENTES-DE-VERDAD.md`: son conocimiento general de ingeniería, no
  un doc-canónico-por-tema-soccer con rivales.

## Verificación

- Solo docs (`.md`), no tocan firmware → sin gate de compilación. No se modificó código
  del robot. (En el working tree compartido había trabajo NO commiteado de otra sesión
  —heading_rate/pfm_heading/strategy.cpp/sensors_imu— que se dejó INTACTO; el commit
  staged solo los archivos de skills + CLAUDE.md + este journal.)

## Atribución

Author: Claude Opus 4.8 (1M context) (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)
