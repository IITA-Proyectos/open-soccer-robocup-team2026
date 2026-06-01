---
title: "Auditoría de referencias colgadas (archivos referenciados inexistentes)"
date: 2026-05-31
status: vigente
tipo: auditoria
generado-por: "workflow `auditoria-referencias-colgadas` (57 agentes: 10 finders por directorio + verificación adversarial por archivo único) orquestado por Claude (coach)"
alcance: "TODO el repo: 1858 referencias a archivos revisadas en .md + #include"
---

> Disparada porque `hardware/electronics/Zircon.pdf` estaba referenciado en ~11 docs pero
> el archivo no existía (se resolvió bajándolo de Robomov, commit 0764ae4). Esta auditoría
> barrió TODO el repo para encontrar cualquier otra referencia colgada.

# Auditoría de referencias colgadas — Soccer 2026 (2026-05-31)

- Referencias revisadas: **1858**
- Candidatas a faltante: **56** → archivos faltantes únicos: **47** → **confirmadas colgadas (verificadas): 15**
- Las 32 candidatas restantes eran falsos positivos (shorthands que sí resuelven, URLs, anchors, sufijos de línea, globs).

## Clasificación de las 15

**A) RUTAS MAL APUNTADAS — el archivo existe en otro lado → CORREGIDAS (commit que acompaña):**

| # | Ref colgada | Está realmente en | Doc(s) |
|---|-------------|-------------------|--------|
| 1 | `test/test_strategy/strategy_transitions.cpp` | `src/shared/strategy_transitions.cpp` | central-board-pack/03-contrato-datos.md |
| 2 | `legacy/2025-season/code/libraries/` | `software/libraries/` (zirconLib) | hardware/electronics/README.md |
| 7 | `legacy/2025-season/code/delantero/` | repo GitHub 2025 (no migrado local) | software/robot-delantero/README.md |
| 8 | `test/test_proto/` | `test/test_proto/` real bajo `software/teensy/Soccer 2026/` | central-board-pack/03-contrato-datos.md |
| 9 | `test/test_pids/` | `software/teensy/Soccer 2026/test/test_pids/` | central-board-pack/03-contrato-datos.md |
| 14 | `legacy/2025-season/code/vision-openmv/` | `software/vision/` (refactor commit 1474673) | software/communication/README.md, software/vision/README.md |
| 15 | `legacy/2025-season/code/arquero/` | `software/robot-arquero/definitivo-arquero_6-9-2026` | software/robot-arquero/README.md |

(#6 `src/shared/proto.h` en un research/in-progress: es shorthand de la ruta del proyecto PIO,
histórico — se deja; #12 `journal/2026-05-11-kickoff...` forward-ref en un doc de diseño histórico — se deja.)

**B) DELIVERABLES PENDIENTES — referencias a archivos que ESE task/research debe PRODUCIR. NO son bugs; no se falsean. Existirán cuando se haga el task:**

| # | Archivo (a crear) | Lo pide |
|---|-------------------|---------|
| 3 | `hardware/electrical/cableado-uart-robot-2026.md` | TASK-008 (rewiring UART) — pending |
| 4 | `hardware/electronics/down-board-fab-as-received.md` | TASK-001 (PCB DOWN) — pending, Enzo |
| 5 | `hardware/electronics/down-board-mods-fab1.md` | TASK-001 (Opción A) — pending, Enzo |
| 10 | `journal/2026-05-2X-decision-localizacion-incheon.md` | TASK-034 — checklist sin cerrar (decisión ya está inline) |
| 11 | `docs/internal/proceso-pcb.md` | TASK-002 (DRC/ERC) — deliverable |
| 12 | `journal/2026-05-11-kickoff-firmware-3-placas.md` | research diseño firmware — forward-ref |
| 13 | `competition/inspection-checklist.md` | research análisis 2026-02-21 — deliverable recomendado (⭐ útil para Incheon) |

## Detalle (verificado adversarialmente)

(Ver tabla del reporte original abajo; cada entrada fue confirmada con Glob/git/grep por un
agente verificador independiente, descartando falsos positivos. Los detalles por archivo —
por qué falta, dónde está realmente, git history del rename — quedaron en el run del workflow.)

| # | Sev | Intencional | Referenciado por |
|---|-----|-------------|------------------|
| 1 | P1 | no | central-board-pack/03 (GAP-011) |
| 2 | P1 | no | hardware/electronics/README.md (canónico) + 5 más |
| 3 | P1 | sí | team-tasks/TASK-008 |
| 4 | P1 | sí | team-tasks/TASK-001 |
| 5 | P1 | no | team-tasks/TASK-001 + research |
| 6 | P1 | no | research/in-progress (shorthand `src/`) — se deja |
| 7 | P1 | no | software/robot-delantero/README.md |
| 8 | P2 | no | central-board-pack/03 |
| 9 | P2 | no | central-board-pack/03 |
| 10 | P2 | sí | team-tasks/TASK-034 |
| 11 | P2 | sí | team-tasks/TASK-002 |
| 12 | P2 | no | research/in-progress diseño |
| 13 | P2 | sí | research/completed análisis |
| 14 | P2 | sí | software/communication + vision README |
| 15 | P2 | sí | software/robot-arquero/README.md |

## Recomendación
- Categoría A: corregidas en el commit que acompaña este journal.
- Categoría B: son trabajo humano (deliverables de tasks). El más valioso para crear pronto:
  **#13 `competition/inspection-checklist.md`** (checklist de inspección física para Incheon).
- El `Zircon.pdf` (disparador original) ya se resolvió aparte (commit 0764ae4, fuente Robomov).
