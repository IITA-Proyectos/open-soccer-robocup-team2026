---
title: "OTOS revividos en banco: el bug de power-cycle del 2026-05-24 se repitió (batería sin entregar corriente) + lección de proceso"
date: 2026-05-29
author: "Claude (Anthropic - Claude Opus 4.7)"
requested-by: "María Viollaz (en la compu de Gustavo @gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7, Anthropic)"
status: final
tags: [down, otos, hardware-up, power-cycle, bateria, mp1584, i2c, leccion-proceso]
robot: ambos
area: electronica
tipo: hardware-up
related-tasks: [TASK-028, TASK-029, TASK-004, TASK-012]
related-journals: [2026-05-24-otos-lib-activada-y-power-cycle-bug.md]
---

# OTOS revividos: recurrencia del power-cycle bug + lección de proceso

## Contexto

María, en banco (compu de Gustavo), corriendo `diag_down`. Los 32 sensores de
luz leían bien, pero los OTOS daban `[L=-- R=--]` con `x=0 y=0 hdg=0`. El scan
I²C mostró: **Wire (U5) vacío** y **Wire1 (U6) respondiendo en `0x64`** (la
dirección del OTOS es `0x17`, confirmado en la lib: `kDefaultAddress = 0x17`).

## Qué pasó (y la causa real)

Diagnostiqué desde cero (scan I²C, direcciones, cables) hasta que **María
recordó que estos OTOS ya se habían validado en ESTA misma placa** (los movió
~30 cm y reportaron movimiento). Eso cambió todo: si antes andaban, no es un
problema de diseño, es algo que se desconectó/degradó.

Buscando en el repo apareció el journal `2026-05-24-otos-lib-activada-y-power-cycle-bug.md`,
que documentaba **exactamente este síntoma** y su fix. La causa real de hoy,
confirmada con María: la **batería estaba conectada pero sin entregar corriente**.
Los OTOS se alimentan del **3.3 V del MP1584, que viene de la batería** (el USB
solo alimenta el Teensy — schematic DOWN, `+3.3V net` alimenta CD4051 **y** OTOS).
Con la batería sin entregar, el riel 3.3 V quedó hambriento → OTOS muertos / a
media máquina (el `0x64` es **brownout**, no otro chip).

**Fix:** batería entregando corriente de verdad + **power cycle completo**
(batería + USB desconectados 10 s, reconectar). Resultado: **ambos OTOS en
`0x17`, funcionando** (confirmado por María en HW).

## Qué aprendimos (refinamiento del bug conocido)

- El disparador del bug del 2026-05-24 no es solo "batería en caliente sobre USB":
  **"batería conectada pero sin pasar corriente" lo reproduce igual.**
- El síntoma puede ser una **dirección I²C rara (`0x64`)**, no solo bus vacío. Es
  brownout del MCU del OTOS por 3.3 V marginal.
- **Los 32 sensores de luz pueden seguir leyendo con el riel flojo, pero los OTOS
  no** → "los sensores andan" NO prueba que el 3.3 V esté sano.

## Lección de proceso (lo más importante — lo marcó María)

La regla **ya estaba documentada** en tres lados: `docs/ESTADO-ACTUAL.md` (línea
126, el doc que el protocolo de sesión dice leer PRIMERO), `TASK-028`, y el
journal del 2026-05-24. **No la consulté cuando los OTOS fallaron** — hice
diagnóstico desde cero y le hice perder tiempo a María. Esto es justo lo que el
protocolo de sesión y la disciplina de journal existen para evitar ("el problema
de antes": redescubrir lo ya resuelto).

**Regla para mí y para futuras sesiones:** ante una falla de hardware o de
comportamiento, **lo PRIMERO es grepear `journal/`, `docs/ESTADO-ACTUAL.md` y
`team-tasks/` para ver si ya pasó**, antes de diagnosticar desde cero.

## Qué se hizo en el repo (para que no se vuelva a enterrar)

- `docs/ESTADO-ACTUAL.md`: la nota de OTOS se elevó a un **checklist visible**
  ("CÓMO ENCENDER LOS OTOS — leer ANTES de debuggear si dan `L=-- R=--`") con el
  detalle nuevo (batería debe entregar corriente; `0x64` = brownout).
- `TASK-028`: actualizada con la recurrencia 2026-05-29 + el refinamiento.

## Estado / próximos pasos

- **OTOS funcionando** (ambos en `0x17`, validado en HW por María). Claude no
  cierra tasks de HW: la validación cuantitativa de precisión sigue siendo
  **TASK-029** (sobre superficie texturada, no hoja A4).
- **TASK-028 sigue pending**: falta el doc operativo formal + medir el 3.3 V del
  MP1584 con multímetro (P0.3 del audit — nunca se midió; explicaría el brownout).
- Objetivo original de María (que DOWN **mande** los OTOS a CENTRAL): ahora que
  los OTOS leen, es viable del lado DOWN — pero recordar que la recepción en
  CENTRAL está bloqueada por el conflicto de pines 7/8 (**TASK-036**), ver
  `journal/2026-05-29-down-central-bringup-debug-serial.md`.
