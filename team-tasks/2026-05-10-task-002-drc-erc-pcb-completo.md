---
id: TASK-002
title: "Correr DRC + ERC en EasyEDA sobre placas TOP y DOWN"
date_created: 2026-05-10
assigned: [enzzo195]
priority: P0
status: pending
estimated_hours: 2
blocks: [hito-2-firmware-down, hito-3-firmware-top]
tags: [hardware, pcb, drc, erc, validacion]
---

# TASK-002 — DRC + ERC sobre ambas placas

## Resumen

Correr **Design Rule Check (DRC)** y **Electrical Rule Check (ERC)** en EasyEDA sobre las placas TOP y DOWN. Documentar reportes y arreglar warnings antes de continuar.

## Contexto

La placa DOWN tiene 10 nets sin rutear que **deberían haber sido detectados por DRC/ERC antes de mandar a producir**. No sabemos si la TOP tiene problemas similares. Antes de comprometer firmware sobre esas placas, hay que correr la validación que faltó.

Esta tarea también establece el proceso "DRC + ERC antes de producir" como **regla del equipo** para evitar repetir el incidente.

## Pasos concretos

### En EasyEDA — placa DOWN

1. Abrir el proyecto: `hardware/electronics/pcb_design/down_board/` (importar el `.json` desde EasyEDA → File → Open).
2. Ir a **Design → Design Rule Check (DRC)**.
3. Configurar reglas (track width 0.2mm mín, clearance 0.2mm mín, drill 0.3mm mín — confirmar con coach).
4. Ejecutar DRC. Capturar screenshot del reporte.
5. Listar **todos** los errores y warnings encontrados (no solo los unrouted que ya conocemos).
6. Guardar reporte en `hardware/electronics/pcb_design/down_board/drc-report-2026-05-10.png` + log de texto.
7. Ir a **Design → Electrical Rule Check (ERC)** en el schematic.
8. Ejecutar ERC. Capturar screenshot.
9. Guardar reporte en `hardware/electronics/pcb_design/down_board/erc-report-2026-05-10.png` + log.

### En EasyEDA — placa TOP

10. Repetir pasos 1-9 para `hardware/electronics/pcb_design/top_board/`.
11. Guardar reportes en `hardware/electronics/pcb_design/top_board/`.

### Análisis y siguientes pasos

12. Crear (o actualizar) `research/in-progress/2026-05-10-auditoria-pcb-down-unrouted-nets.md` con sección "TOP — DRC/ERC findings": si la TOP tiene unrouted nets u otros warnings, listarlos.
13. Para cada warning encontrado, decidir: ¿es real bug? ¿es false positive? ¿requiere fix?
14. Si hay nuevos bugs P0, crear nuevas tasks en `team-tasks/` con prioridad P0.

## Criterio de cierre

- [ ] Reporte DRC + ERC de DOWN guardado en `hardware/electronics/pcb_design/down_board/`.
- [ ] Reporte DRC + ERC de TOP guardado en `hardware/electronics/pcb_design/top_board/`.
- [ ] Lista de warnings de cada placa registrada en `research/in-progress/`.
- [ ] Si aparecen bugs nuevos: tareas creadas en `team-tasks/`.
- [ ] Documento `docs/internal/proceso-pcb.md` creado con el procedimiento DRC+ERC obligatorio para futuras placas.

## Notas / decisiones

_(actualizar cuando se ejecute)_

## Cambios de estado

- 2026-05-10: creado por Claude bajo requerimiento de Gustavo Viollaz.
