---
title: "Fase 0 — Setup del sistema de trabajo: frame coach + skills locales"
date: 2026-05-10
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: aprobado
tags: [organizacion, coaching, infraestructura, decision]
area: organizacion
tipo: decision
---

# Fase 0 — Setup del sistema de trabajo

## Contexto

A 7 semanas de RoboCup 2026 Incheon (jun 30 – jul 6 2026), el equipo IITA Soccer Open opera con un repo bien organizado pero técnicamente disperso:

- 11 playbooks en `skills/` sin frontmatter Claude (no son skills auto-invocables; son knowledge base).
- Research pipeline activo en febrero 2026 pero estancado desde abril (last journal: 2026-03-20).
- Código del robot mezclando "estado actual" con "propuestas no probadas" en los playbooks.

Se acuerda con el coach del equipo (Gustavo Viollaz) un plan de 4 fases para llegar a Incheon con un sistema de trabajo capitalizable hacia Nacional Nov 2026 y RoboCup Mundial 2027 (con Virginia Viollaz transicionando de competidora a coach).

## Frame estratégico

- **Incheon 2026 (jun 30 – jul 6)** — inversión en aprendizaje. No se busca podio mundial; se busca robot honesto, partidos jugados, captura sistemática de aprendizajes.
- **Nacional Argentina noviembre 2026** — cosecha doméstica con lo aprendido.
- **Mundial 2027** — Virginia coach + nuevos alumnos. **El repo y las skills tienen que sobrevivirla.**

Cualquier propuesta técnica se evalúa por tres criterios:
1. Qué aprendizaje deja al equipo actual.
2. Qué tan reusable queda para 2027.
3. Qué tan documentada queda.

Una mejora corta y bien documentada > una mejora ambiciosa y opaca.

## Plan en 4 fases

### Fase 0 — Setup del sistema de trabajo (esta fase)

Output:
1. `CLAUDE.md` en raíz del repo — frame coach senior, multi-temporada, reglas de feedback. Distinto de `AI-INSTRUCTIONS.md` (ese es para alumnos y todos los AIs en general).
2. `.claude/skills/` con 8 skills Claude reales (frontmatter válido, autoinvocables):

   **Frame del coach:**
   - `rcj-soccer-coach` — formato de feedback (P0/P1/P2, tema-a-analizar, plan en hardware real).

   **Aceleradores con IA:**
   - `vibe-mechanical-design` — pipeline IA → STL/STEP verificable.
   - `vibe-pcb-design` — pipeline IA → KiCad/Gerber verificable.
   - `vibe-robotics-coding` — firmware embedded acelerado con IA.

   **Disciplina operativa:**
   - `hardware-test-protocol` — diseño y ejecución de tests en hardware real. Referenciada por las vibe-* y rcj-soccer-coach.
   - `engineering-journal` — disciplina del journal + research pipeline. Reactiva el flujo que el equipo dejó de usar desde 2026-03-20.

   **Técnica específica:**
   - `openmv-vision-tuning` — calibración de cámaras OpenMV para distintas iluminaciones. Crítica para Incheon.

   **Entregables:**
   - `rcj-judging-package` — BOM, poster A1, video, portfolio, entrevista.
3. Decisión sobre `skills/` raíz: **NO renombrar** por ahora. `CLAUDE.md` aclara la distinción entre `skills/` (playbooks técnicos del dominio soccer, legacy del nombre) y `.claude/skills/` (skills Claude auto-invocables).

### Fase 1 — Auditoría de playbooks existentes (~1 semana)

- Cada playbook en `skills/` se contrasta contra el código real en `software/`.
- Sale tema-a-analizar con `risk-no-fix` / `risk-fix` / `tiempo`. Pasan a `research/in-progress/`.
- Marcar inequívocamente "comportamiento actual del robot" vs "propuesta no probada".
- Reactivar journal: una entrada por playbook auditado.
- Empezar por los marcados ⭐⭐⭐ que dicen reflejar comportamiento actual: `striker-strategy`, `goalkeeper-strategy`, `multi-camera-world-model`, `soccer-match-fsm`.

### Fase 2 — Optimización rumbo a Incheon (4-5 semanas)

- Plan semanal mayo-junio basado en playbooks ya validados.
- Foco en lo que más capitaliza experiencia para 2027 (mecánica reproducible, código comprensible, documentación viva).
- Skills `vibe-*` aceleran iteraciones mecánica/PCB/firmware.

### Fase 3 — Deliverables de jueces (2 semanas, en paralelo a final de Fase 2)

- BOM, poster A1, video técnico, portfolio digital, entrevista.
- `rcj-judging-package` skill guía cada deliverable contra rúbrica.

## Decisiones meta

- **No renombrar `skills/` → `playbooks/`** por ahora. Cero churn en docs existentes; el `CLAUDE.md` aclara la distinción cognitiva.
- **Skills locales primero, globales después.** Lo que demuestre ser reusable a Rescue Line / WRO se promueve a `~/.claude/skills/` o a un plugin propio.
- **Testing en hardware real es no negociable.** Cualquier cambio en código del robot que se proponga debe venir con plan de prueba en hardware real (calibración, criterio de aceptación, regresión).
- **El frame coach NO es autoridad inventada.** Activa el lente de equipos top de RoboCup Soccer Open / Middle Size League para evaluar propuestas, pero cuando una afirmación requiere experiencia personal de un mundial, marcarla y verificar con fuentes públicas (papers, repos públicos de equipos, foros RCJ).

## Próximos pasos inmediatos

1. **Validación del setup** — Gustavo revisa árbol resultante y confirma o pide ajustes.
2. **Iteración del contenido de cada skill** — los SKILL.md de Fase 0 son esqueletos; cada uno se desarrolla con contexto real (Zircon pinout, código del robot, restricciones del equipo).
3. **Fase 1: auditoría playbooks** — empezar por `striker-strategy.md` y `multi-camera-world-model.md` (donde ya identifiqué bugs sutiles en el muestreo inicial).
4. **Cronograma actualizado en `competition/timeline.md`** reflejando este plan de 4 fases.

## Bugs ya detectados en muestreo inicial (entran a Fase 1)

- `skills/striker-strategy.md` — el "behind-the-ball" asume `goal_y = FIELD_LENGTH` (arco rival fijo); no hay polaridad de campo según qué arco defendemos. Si los robots cambian de lado, todo se invierte.
- `skills/multi-camera-world-model.md` — `correct_position_from_goal()` mezcla coords globales con vector relativo en su álgebra; conceptualmente devuelve ruido en vez de estimación de posición.
- General — magic numbers (`dist < 250`, `behind 130mm`, `alignment < 25°`, `drive(700, …)`) sin trazabilidad a calibración o test.

Estos pasan a Fase 1 con tema-a-analizar formal.
