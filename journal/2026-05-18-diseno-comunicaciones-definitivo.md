---
title: "2026-05-18 — Diseño definitivo de comunicaciones + TASK-014..020"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [comunicacion, electronica, decision, ambos]
robot: ambos
area: comunicacion
tipo: decision
related: [docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md]
---

# Diseño definitivo de comunicaciones + tareas

## Contexto

Tras la verificación independiente, Gustavo pidió: (1) armar las TASKs del orden
corregido, (2) un nuevo análisis global y un sistema robusto/confiable/a prueba
de fallas.

## Qué se hizo

- **7 tareas** creadas (TASK-014..020) con el plan de prueba corregido
  (inyección de stall + ruido EMI + medición de loop, no solo desconexión):
  - TASK-014 P0 — loop de TOP no-bloqueante + medir período real (fundacional).
  - TASK-015 P0 — CRC + fin de trama en enlace de cámara (con tests del parser).
  - TASK-016 P0 — fail-safe de borde: OR-latch + precedencia ante LOST conjunto.
  - TASK-017 P1 — heartbeat + FSM histéresis + SEQ sin falsos (depende de 014).
  - TASK-018 P1 — cota de drenado, sin Serial.clear().
  - TASK-019 P1 — robustez de arranque (guard ×3, gate debug, handlers).
  - TASK-020 P2 — refactor a módulo Link + static_assert + config (2027).
- Índice `team-tasks/README.md` actualizado con orden de ejecución y la regla
  dura (no bajar timeout hasta cerrar TASK-014).
- **Diseño definitivo** en
  `docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md`:
  arquitectura en 6 capas (defensa en profundidad), principio "fail-safe by
  default", taxonomía de mensajes STREAM/EVENTO/COMANDO (el corazón del
  arreglo), FSM de enlace con histéresis, retícula de precedencia de seguridad,
  umbrales derivados de medición (no inventados).

## Conclusión

El sistema queda especificado de forma que **toda falla degrada a un estado
seguro, observable y estable**. Lo "homogéneo" es un módulo `Link` único con 3
políticas claras, NO una sola política para todo (ese era el error original).
El orden de ejecución es obligatorio: TASK-014 es fundacional y bloquea el
rediseño de ventanas.

## Próximos pasos

- El coach valida prioridades con el equipo y asigna fechas.
- Ejecutar TASK-014 primero (medición en hardware) — sin eso, las ventanas no
  tienen base.
- Cada TASK se cierra solo con su prueba en hardware real.
