---
id: TASK-204
title: "[DUPLICADA → TASK-039] Verificar pines OUT1/OUT2 del árbitro COMM→TOP"
date_created: 2026-06-02
date_updated: 2026-06-02
assigned: [enzo]
priority: P0
status: duplicada
duplicada_por: TASK-039
tags: [comm-board, arbitros, hardware, top-board, multimetro, homologacion, duplicada]
---

# TASK-204 — DUPLICADA, usar TASK-039

> **⚠️ ESTA TASK ES DUPLICADA. La canónica es
> [`TASK-039`](2026-06-02-task-039-comm-arbitro-out1out2-no-llega-al-teensy.md).**
>
> Creada en `agente/top` el 2026-06-02 para el mismo problema del árbitro
> (OUT1/OUT2 no llegan al Teensy). En paralelo, otra sesión (CENTRAL, sobre
> `main`) creó **TASK-039** con el MISMO diagnóstico — los dos llegamos a la
> misma conclusión por caminos separados (la COMM entrega NIVEL en OUT1/OUT2,
> el TOP escucha UART). Al sincronizar branches se detectó el solape y se
> consolidó acá: **TASK-039 es la fuente de verdad.**
>
> **TASK-039 es más completa** — incluye el ground-truth del netlist del PCB
> (no estaba en esta): **OUT1 → pad 27 / GPIO 5**, **OUT2 → pad 26 / GPIO 6**,
> un probe ya commiteado en `main_top.cpp` (`refprobe[...]`), y datos medidos
> (pin 6 fijo en 1, pin 5 fijo en 0, no togglean con la app). Ir directo a esa.

## Qué se conserva de acá (ya está cubierto en TASK-039)

- El plan de firmware (leer el árbitro como pin digital en `comm_arbiter.cpp`)
  coincide con el §6 de TASK-039.
- El análisis del gap COMM(nivel) vs TOP(UART) está en mi journal
  `journal/2026-06-02-arbitro-gap-y-ultrasonido-top.md` (sigue válido como
  registro; no se duplica con TASK-039).
- La nota del HC-SR04 sin divisor (5 V al pin 3 del Teensy, riesgo P1) está en
  ese mismo journal — TASK-039 también menciona el HC-SR04 como nota secundaria.

## Cierre

No ejecutar esta TASK. Trabajar sobre **TASK-039**. Esta queda como registro
del solape para que no se pierda la trazabilidad de por qué hay dos números.

## Cambios de estado
- 2026-06-02: creada en agente/top.
- 2026-06-02: marcada **duplicada** de TASK-039 (consolidación al sincronizar
  con main), a pedido de Gustavo Viollaz.
