---
title: "arqueromix — revert del re-homing por línea: vuelta al checkpoint (pedido Virginia)"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco)"
status: COMPILA · = versión checkpoint arqueromix-ok-patrulla-arco-2026-06-21
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: revert
---

# arqueromix — vuelta a la versión anterior (sin re-homing por línea)

## Por qué

Virginia: "volvé a la versión anterior". Se revierte el último cambio (re-homing al detectar la línea
del área chica durante la patrulla) y se vuelve al checkpoint que se había guardado para esto.

## Qué se hizo

- `git revert --no-commit 96d15d6` (el commit del re-homing).
- Verificado: `git diff --cached 965af1f` en los archivos de arqueromix = **0 líneas** (idéntico al
  checkpoint `arqueromix-ok-patrulla-arco-2026-06-21`). `AMIX_REHOME_ON_LINE` = 0 referencias.
- `pio run -e central_robot2_arqueromix` → **SUCCESS**.

## Estado resultante

arqueromix queda en la versión de la **patrulla por arco propio** (homing por línea + seguimiento de
pelota por ángulo + despeje con rampa dirigido al arco rival + patrulla que rebota por ángulo del arco
propio). SIN el re-homing por línea del área chica. = exactamente el checkpoint guardado.

## Archivos

- Revertidos a estado checkpoint: `amix_config.h`, `amix_fsm.cpp`, `DOCUMENTACION.md`.
- Borrado: el journal del re-homing (`2026-06-21-arqueromix-rehome-linea-area-chica.md`).
- NO tocado: binario de competencia (build aislado) ni el trabajo de centralmix del compañero.
