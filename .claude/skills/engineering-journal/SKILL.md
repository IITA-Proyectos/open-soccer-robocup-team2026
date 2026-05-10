---
name: engineering-journal
description: Use when documenting engineering work in the repo — journal entries, research backlog grooming, in-progress analysis, completed conclusions, and decision records. Enforces the YYYY-MM-DD-descripcion.md convention, attribution rules, and the discipline that the team set up but stopped using since 2026-03-20. This skill is what keeps the repo alive between sessions and turns Incheon into a documented learning experience instead of folklore.
---

# Engineering Journal — Disciplina del Diario y Research Pipeline

> **Status: outline only — content pending iteration.**

## Why this skill exists

El repo tiene una infraestructura de journal + research **excelente, diseñada por el equipo**, pero **inactiva desde 2026-03-20** (last journal entry). Sin entradas frescas, el conocimiento no se acumula y cada sesión empieza de cero. Esta skill formaliza la disciplina para que **cada sesión de trabajo deje algo en el repo** — y para que Incheon sea aprendizaje documentado, no folklore.

## When to use

- Después de cada sesión de trabajo (lab o remota) — siempre.
- Al iniciar una nueva línea de investigación (item a `research/backlog/`).
- Al empezar a analizar un tema del backlog (mover a `research/in-progress/`).
- Al cerrar un análisis con conclusión accionable (mover a `research/completed/`).
- Al tomar una decisión técnica con trade-offs (decision record).
- Durante Incheon — protocolo de captura especial (ver sección abajo).

## When NOT to use

- Notas personales del coach (eso va en `~/.claude/projects/.../memory/`, no en repo).
- Información sensible de sponsors o financiamiento.
- Conversaciones de chat / Discord (links sí, contenido literal no necesariamente).

## Journal entry format

Archivo: `journal/YYYY-MM-DD-descripcion-corta.md`

**Frontmatter (siguiendo AI-INSTRUCTIONS.md):**
```yaml
---
title: "..."
date: YYYY-MM-DD
author: "..."  # nombre + (proveedor si es IA)
requested-by: "..."  # nombre humano que pidió el trabajo
ai-assisted: true|false
ai-tool: "..."  # si aplica
status: draft|final
tags: [area, tipo, robot, prioridad]
robot: arquero|delantero|ambos
area: vision|movilidad|control|electronica|...
tipo: analisis|protocolo|resultado|decision|...
---
```

**Cuerpo (estructura mínima):**
1. **Contexto** — qué se vino a hacer y por qué.
2. **Qué se hizo** — acciones concretas, sin floritura.
3. **Qué se midió/observó** — datos crudos (foto, log, número). Si no se midió nada, decirlo.
4. **Conclusión** — qué se aprendió (puede ser "no concluyente").
5. **Próximos pasos** — qué queda pendiente (puede generar items en `research/backlog/`).

## Research pipeline: backlog → in-progress → completed

### Backlog
- Archivo en `research/backlog/descripcion.md` (puede ir sin fecha si es muy nuevo).
- Contenido mínimo:
  - Pregunta concreta a responder.
  - Por qué importa (impacto en Incheon / Nacional Nov / 2027).
  - Recursos posibles (links, papers, repos hermanos).

### In-progress
- Mover el archivo de backlog a `research/in-progress/`.
- Renombrar con fecha de inicio: `YYYY-MM-DD-tema.md`.
- Documentar avance incremental dentro del mismo archivo (no archivos separados).
- **Regla:** si está más de 2 semanas en in-progress sin avance, escalar o cerrar (mover a `completed/` con conclusión "abandonado por X").

### Completed
- Mover a `research/completed/YYYY-MM-DD-tema.md` cuando hay conclusión.
- Debe incluir:
  - Resumen ejecutivo (3-5 líneas, leíble en 30 segundos).
  - Conclusión clara.
  - Recomendaciones accionables.
  - Decisión tomada (si aplica) — o link a la decision record.
  - Links a journal entries que ejecutaron sobre esta investigación.

## Decision records (light ADR)

Para decisiones técnicas con trade-offs (ej. "elegimos OpenMV H7+ vs H7 plain por X"):
- Archivo: `docs/decisions/YYYY-MM-DD-titulo.md`.
- Estructura mínima:
  - **Contexto** — qué problema queríamos resolver.
  - **Opciones consideradas** — mínimo 2, con pros/contras de cada una.
  - **Decisión** — qué elegimos.
  - **Consecuencias** — incluye lo que sacrificamos.
  - **Quién decidió y cuándo** — coach + equipo.

## Anti-patterns (qué NO hacer)

- ❌ "Hicimos varias cosas hoy" sin detalle medible.
- ❌ Journal entry sin frontmatter (no se indexa, no se filtra).
- ❌ Items en `research/in-progress/` por meses sin tocar (esto es lo que pasó desde abril 2026).
- ❌ Completed sin conclusión accionable (análisis paralizado).
- ❌ Mezclar journal personal (preocupaciones, dudas) con journal técnico (datos, mediciones).
- ❌ Borrar entradas. **Errores documentados son patrimonio del equipo.** Las correcciones van en entradas nuevas que linkean a las viejas.
- ❌ Esperar a "tener algo lindo que contar" para escribir. Mejor entrada honesta de "no funcionó X y no entendemos por qué" que silencio.

## Disciplina mínima del equipo

- **Cada sesión de trabajo** (lab IITA o remota) deja **1 entrada en `journal/`**. Aunque sea de 5 líneas.
- **Cada lunes** se revisa `research/in-progress/` para mover lo que se cierra o ajustar lo que se atascó.
- **Antes de cada partido / práctica oficial**, journal entry con "estado del robot, qué esperamos probar".
- **Después de cada partido / práctica oficial**, journal entry con "qué pasó, qué aprendimos, qué quedó por arreglar".

## Protocolo Incheon (planned)

[TODO: cerrar antes del viaje]

- **Antes del torneo:** entrada con baseline esperado, predicciones para cada partido, configuración de cámaras.
- **Durante el torneo:** una entrada por día como mínimo (5 días de competencia).
- **Después de cada partido:** mini-journal de 15 min — qué falló, qué decisión técnica tomamos para el siguiente partido. Foto + breve nota.
- **Después del torneo:** post-mortem completo en `research/completed/2026-07-XX-postmortem-incheon.md`. Una sección por subsistema + una sección de "lo que llevamos al Nacional Nov 2026".

**Este registro es el activo más valioso de Incheon.** Sin él, la "experiencia" se diluye y el equipo 2027 empieza de cero. Con él, Virginia tiene material concreto para coachear.

## Plantillas (planned)

[TODO: agregar plantillas listas en `journal/templates/`]
- `journal-session.md`
- `journal-pre-match.md`
- `journal-post-match.md`
- `research-backlog.md`
- `research-completed.md`
- `decision-record.md`

## References

- `AI-INSTRUCTIONS.md` secciones 2, 3, 4 — convenciones de archivos y journal.
- `journal/README.md` (si existe).
- `research/backlog/README.md`, `research/in-progress/README.md`, `research/completed/README.md`.
- `CONTRIBUTING.md` — reglas de atribución.
- `CLAUDE.md` (raíz) — frame general del repo.
