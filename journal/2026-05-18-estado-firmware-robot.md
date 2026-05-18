---
title: "2026-05-18 — Evaluación crítica del estado del firmware del robot"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [firmware, vision, analisis, ambos]
robot: ambos
area: control
tipo: analisis
related: [research/completed/2026-05-18-estado-firmware-robot-evaluacion-critica.md]
related-tasks: [TASK-022, TASK-023, TASK-024]
---

# Evaluación crítica del estado del firmware

## Contexto

Gustavo pidió un estudio profundo, crítico e independiente del estado de los
programas: si hay 3 candidatos completos (DOWN/CENTRAL/TOP) y 2 de cámara
(una por cámara), y qué falta para que el robot esté operativo (excluye armado
físico, que estima terminar esta semana).

## Qué se hizo

4 auditorías independientes en paralelo sobre el código real (no docs): DOWN,
CENTRAL, TOP, cámaras+build. Síntesis en
`research/completed/2026-05-18-estado-firmware-robot-evaluacion-critica.md`.

## Conclusión (sin endulzar)

- **NO hay 3 programas completos candidatos a funcionar.** Hay 3 esqueletos
  que compilan; cada uno con agujeros P0 que lo dejan no-funcional en cancha.
- **NO hay 2 programas de cámara.** Hay 1 script genérico de demo con 2 bugs
  que rompen el protocolo (sentinel `(0,100)` ≠ parser, crash bytearray) y
  auto-WB/gain ON.
- **El robot no funciona todavía y NO está "casi listo".** Los ~95 tests
  verdes son falsa seguridad (solo lógica pura; la FSM testeada es una réplica,
  no `strategy.cpp`).

P0 convergentes: UARTs cruzados (robot inerte), sentinel de cámara (pelota
fantasma), OTOS stub + pose hardcodeada 0 (sin mapa), ToF stub (sin evasión),
escala cámara sin calibrar, no arranca sin COMM, polaridad de arco hardcodeada
(autogoles), rol nunca leído, build sin doc/CI/lib_deps.

Lo que SÍ está bien: `proto.h`, fusión de cámaras (testeada), IMU dual real,
motores+cinemática+PIDs+FSM conectada (primitiva pero real), aislamiento de
builds PlatformIO.

Camino crítico realista: ~2–3 semanas de banco con hardware (destrabar →
percepción → jugar + transversal build). Coherente con "Incheon = aprendizaje".

## Próximos pasos

- Tasks nuevas: TASK-022 (cámara operativa), TASK-023 (build/tooling/CI),
  TASK-024 (arranque/rol/polaridad). Resto cubierto por TASK-006/008/012/014.
- El coach prioriza con el equipo. Atacar P0 en orden: destrabar primero.
