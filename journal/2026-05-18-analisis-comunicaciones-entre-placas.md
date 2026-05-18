---
title: "2026-05-18 — Análisis profundo de comunicaciones entre placas"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [comunicacion, electronica, analisis, ambos]
robot: ambos
area: comunicacion
tipo: analisis
related-tasks: [TASK-003, TASK-006, TASK-008]
related: [docs/decisions/2026-05-18-protocolo-comunicaciones-entre-placas.md]
---

# Análisis profundo de comunicaciones entre placas

## Contexto

Gustavo pidió un análisis a fondo de TODOS los enlaces de comunicación del robot
(TOP↔CENTRAL motores, DOWN↔TOP, DOWN↔CENTRAL línea, cámaras↔TOP) con foco en
confiabilidad: heartbeat, recuperación ante fallas, limpieza de buffer, velocidad,
reset. Objetivo: que el repo tenga un documento maestro.

## Qué se hizo

3 procesos paralelos leyendo el código real: (1) enlaces inter-placa
`proto.h`/`comm_*`, (2) enlace cámaras OpenMV, (3) docs de diseño previos +
convenciones del repo. Síntesis en el decision record
`docs/decisions/2026-05-18-protocolo-comunicaciones-entre-placas.md`.

## Qué se observó (hallazgos clave, citados en el decision record)

- `proto.h` (CRC16-CCITT + SEQ + resync, bien testeado) es sólido → es la base
  del diseño homogéneo objetivo.
- **Heartbeat: NO existe explícito** — "vivo" = "hubo datos" (decisión previa de
  los docs de firmware; el coach pide explícito).
- **Cámaras: único enlace SIN CRC ni fin de trama** (framing legacy 9 B, 19200) →
  el más frágil; P0.
- **Fail-safe de borde se pierde en silencio** si cae DOWN→CENTRAL
  (`main_central.cpp:13,95`) → riesgo de salir de cancha; P0.
- SEQ se transmite pero **nadie lo verifica**; sin observabilidad de salud en
  cancha; bug de frescura al boot del TOP (sin guard `last_ms>0`); ventanas de
  freshness 500 ms = 50× el período de emisión.
- Diseño NO homogéneo: 5 `comm_*.cpp` casi idénticos, baud hardcodeado 3 veces.
- **Contradicción crítica C1:** `config_central.h:1-15` aún describe el modelo
  viejo "motor server TOP-master"; el código real ya es CENTRAL-master con
  motores locales. Más contradicciones C2–C6 (pines, baud DOWN↔CENTRAL,
  frecuencia LINE_URGENT 100 vs 200 Hz, Wire1, frame 16 B vs variable) listadas
  en el decision record.

## Conclusión

Capa de framing fuerte, **capa de enlace incompleta**. Se definió un diseño
objetivo confiable/simple/homogéneo (un solo `proto.h` en todo, heartbeat
explícito, máquina OK/STALE/LOST, acciones de seguridad por LOST, SEQ verificado,
buffer limpio, refactor a módulo `Link` único). Priorización honesta: P0 = CRC
cámara + fail-safe de borde (sin esto se pierden partidos / se sale de cancha);
P1 = heartbeat/SEQ/boot; P2 = homogeneización (inversión 2027).

## Próximos pasos

- Convertir cada tema P0/P1 en TASK con plan de prueba en hardware real (no se
  cierra sin test en robot).
- Corregir la doc de config obsoleta de CENTRAL (C1) — no asumir CENTRAL-master
  implementado en `config_central.h`.
- Decisión de priorización a validar por el coach con el equipo.
