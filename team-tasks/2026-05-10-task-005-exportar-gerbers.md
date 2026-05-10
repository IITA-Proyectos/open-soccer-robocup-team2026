---
id: TASK-005
title: "Exportar Gerbers de las placas TOP y DOWN y archivar en el repo"
date_created: 2026-05-10
assigned: [enzzo195]
priority: P1
status: pending
estimated_hours: 1
blocks: []
tags: [hardware, pcb, gerber, reproducibilidad]
---

# TASK-005 — Exportar Gerbers y archivar

## Resumen

Exportar los archivos Gerber (los archivos finales que se mandaron a fabricar) desde EasyEDA y archivarlos en el repo para reproducibilidad futura. Sin esto, Virginia 2027 va a tener que re-derivarlos desde el schematic.

## Contexto

El repo tiene los proyectos EasyEDA en JSON pero **no tiene Gerbers** (`.gbr`, `.drl`, `.gtl`, etc). Los Gerbers son los archivos universales de PCB que permiten:
- Re-fabricar la placa en cualquier proveedor (JLCPCB, PCBWay, OSHPark, fab local).
- Auditar el ruteo sin EasyEDA.
- Documentar la versión exacta que se fabricó (sin depender de un commit de EasyEDA que puede cambiar).

**Importante:** si la placa DOWN se re-fabrica (TASK-001 Opción B), generar Gerbers v1.1 con el ruteo corregido y archivar las dos versiones (v1.0 con bugs, v1.1 corregida).

## Pasos concretos

### Placa TOP

1. Abrir proyecto TOP en EasyEDA.
2. Ir a **Manufacturing → Generate Gerber File** (o **Export → Gerber**).
3. Configurar para JLCPCB (capas: top copper, bottom copper, top silkscreen, bottom silkscreen, top solder mask, bottom solder mask, drill).
4. Guardar el ZIP de Gerber en `hardware/electronics/pcb_design/top_board/gerber-v1.0/`.
5. Exportar también el archivo `pick-and-place.csv` (posiciones de componentes SMD para assembly automático, si aplica).
6. Exportar el BOM consolidado en formato JLCPCB si todavía no está.

### Placa DOWN

7. Repetir pasos 1-6 para `hardware/electronics/pcb_design/down_board/gerber-v1.0/`.
8. **Si se re-fabrica con fixes** (TASK-001 Opción B): generar Gerbers en `gerber-v1.1/` con commit message claro indicando los nets corregidos.

### Verificación

9. Abrir uno de los Gerbers exportados con un viewer (https://www.pcbgogo.com/GerberViewer.html o similar). Confirmar visualmente que el board se ve como esperamos.
10. Crear `hardware/electronics/pcb_design/README.md` (si no existe) con:
    - Convención de versionado de Gerbers (`gerber-v1.0/`, `gerber-v1.1/`...).
    - Procedimiento para futuras revisiones (siempre DRC+ERC antes de exportar — referencia a TASK-002).

## Criterio de cierre

- [ ] Gerbers TOP v1.0 archivados en `hardware/electronics/pcb_design/top_board/gerber-v1.0/`.
- [ ] Gerbers DOWN v1.0 archivados en `hardware/electronics/pcb_design/down_board/gerber-v1.0/`.
- [ ] Si se re-fabrica DOWN: Gerbers v1.1 archivados en `gerber-v1.1/`.
- [ ] Visualizados con un viewer y confirmados visualmente.
- [ ] `hardware/electronics/pcb_design/README.md` con convención de versionado.

## Notas / decisiones

_(actualizar cuando se ejecute)_

## Cambios de estado

- 2026-05-10: creado por Claude bajo requerimiento de Gustavo Viollaz.
