---
title: "Suspensión de la traducción automática ES→EN (GitHub Action)"
date: 2026-08-02
author: "Claude (Anthropic - Claude Opus 5)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 5, Anthropic)"
status: final
tags: [journal, ci, github-actions, documentacion, traduccion, costos]
---

# Journal — suspensión de la traducción automática ES→EN

## Qué se hizo

Gustavo pidió apagar la traducción automática de documentación (ES→EN) en este
repo y en el de RCJ Rescue Line, porque **no hay documentación en preparación
para campeonatos internacionales** (Incheon terminó; el próximo hito es el
Nacional de noviembre 2026, que es en español).

Cambio aplicado en `.github/workflows/translate-docs.yml`: se sacó el trigger
`push` y quedó **solo `workflow_dispatch`** (disparo manual). El workflow, el
script `scripts/translate_docs.py` (con su allowlist `DELIVERABLES`) y el secret
`OPENAI_API_KEY` **NO se borraron**. Las instrucciones completas de reactivación
quedaron como comentario en la cabecera del propio YAML — que es donde uno va a
buscarlas dentro de seis meses.

## Estado medido antes del cambio (no supuesto)

| | Este repo (soccer) | RCJ Rescue Line |
|---|---|---|
| Workflow | `Auto Translate Deliverables (ES -> EN)` (ID 248963381) | `Auto Translate Spanish Docs to English (L2)` (ID 237403828) |
| Estado | `active` | `active` |
| Trigger | push a `main`, paths `docs/competencia/**` + `docs/official/**` | push en **cualquier rama**, paths `docs/es/**` |
| Alcance | allowlist de 7 deliverables | **todo** `docs/es/` (`rglob`, sin allowlist) |
| Última corrida real | **2026-06-24** | **2026-08-02 00:01 UTC** (rama `Bugs-Prioritarios`), 16m54s |
| Duración típica | 10–22 min | 15–34 min |

O sea: **acá ya estaba dormido de hecho** (nadie toca `docs/competencia/` desde
el 24-jun), pero seguía armado y hubiera revivido solo con el primer push a un
deliverable. El que estaba gastando de verdad era el de RCJ.

## Dos correcciones a la premisa del pedido

1. **No gasta tokens de Claude.** El script usa `OPENAI_API_KEY` con
   `gpt-4o-mini` (OpenAI). Es gasto de OpenAI, no de Anthropic.
2. **No frenaba el push.** El workflow corre asíncrono, después del push. Lo que
   sí costaba era (a) minutos de GitHub Actions —repos privados consumen cuota—
   y (b) un PR `bot/translate-docs` a triar. El ahorro real está ahí.

## Consecuencia a tener presente

`docs/competencia/en/**` queda **CONGELADA** en su estado del 2026-06-24. Sigue
sin editarse a mano, pero ahora además **está desactualizada** respecto del
español. Si alguien necesita el inglés al día, hay dos caminos: correr el
workflow a mano (`gh workflow run "Auto Translate Deliverables (ES -> EN)"
-R IITA-Proyectos/open-soccer-robocup-team2026 --ref main`) o reactivar el
trigger automático siguiendo la cabecera del YAML.

## Docs actualizados en el mismo commit

- `docs/coach-system/HANDOFF-NUEVA-SESION.md` — decía que el push de un
  deliverable ES dispara el PR `bot/translate-docs`. Ya no es cierto (3 lugares).

## Pendiente / no hecho

- `HANDOFF-NUEVA-SESION.md` tiene **otra** desactualización que NO toqué porque
  está fuera del alcance de este pedido: la línea de arranque apunta a
  `C:\Users\violl\iitasoccer\soccer-main`, ruta que dejó de existir con la
  consolidación de repos del 2026-07-26 (ver `CLAUDE.md` § "Ubicación del repo").
  Tema a analizar aparte.
