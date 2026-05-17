---
title: "Team Tasks — IITA Soccer Open RoboCup 2026"
date: 2026-05-10
status: active
---

# Team Tasks

Sistema de tareas asignadas a miembros del equipo para Incheon 2026.

## Cómo funciona

- Cada tarea es un archivo `YYYY-MM-DD-task-NNN-nombre-descriptivo.md` en este directorio.
- El archivo tiene frontmatter con `assigned`, `priority`, `status`, `estimated_hours`, `blocks`.
- **El responsable actualiza el archivo** cuando arranca (`status: in-progress`) y cuando termina (`status: done`).
- **Las decisiones tomadas durante la tarea se documentan en la sección "Notas" del propio archivo** — no en otro lugar.
- Tareas bloqueadas (`status: blocked`) deben anotar por quién o por qué.

## Prioridades

- **P0** — Bloqueante. Sin esto otros hitos no avanzan o el robot no compite.
- **P1** — Impacto alto en el calendario o la performance del robot.
- **P2** — Mejora deseable / capitalizable para 2027.

## Miembros del equipo

| Nombre | Handle | Rol | Tareas típicas |
|--------|--------|-----|----------------|
| Virginia Viollaz | @mariaviollaz | Competidora — visión / estrategia | Firmware TOP, parser cámara, FSM |
| Elías Cordero | (sin handle) | Competidor — mecánica / electrónica | Hardware setup, montaje OTOS, integración |
| Enzo | enzzo195 | Diseñador de placas / hardware | PCB design, DRC/ERC, schematics, Gerbers |
| Gustavo Viollaz | @gviollaz | Coach / coordinador | Revisión, priorización, decisiones |

## Tareas activas (al 2026-05-15)

| ID | Título | Asignado | Prio | Estado |
|----|--------|----------|------|--------|
| [TASK-001](2026-05-10-task-001-pcb-down-unrouted-nets-fix.md) | PCB DOWN — fix 10 nets sin rutear | Enzo | P0 | pending |
| [TASK-002](2026-05-10-task-002-drc-erc-pcb-completo.md) | DRC + ERC sobre ambas placas | Enzo | P0 | pending |
| [TASK-003](2026-05-10-task-003-confirmar-wire1-remap-top.md) | Confirmar Wire1 remap a pines 24/25 en TOP | Enzo | P0 | pending |
| [TASK-004](2026-05-10-task-004-confirmar-montaje-otos.md) | Confirmar montaje físico de los 2 OTOS | Elías + Enzo | P1 | pending |
| [TASK-005](2026-05-10-task-005-exportar-gerbers.md) | Exportar Gerbers de TOP y DOWN | Enzo | P1 | pending |
| [TASK-006](2026-05-10-task-006-cargar-firmware-rcj-comm.md) | Cargar firmware oficial RCJ en placa COMM | Virginia o Elías | **P0** | pending |
| [TASK-007](2026-05-10-task-007-verificar-bugs-p0-codigo-viejo.md) | Verificar si bugs P0 del código viejo se fixearon | Virginia + Elías | P1 | pending |
| [TASK-008](2026-05-10-task-008-rewiring-fisico-uart-robot.md) | Rewiring UART: OpenMV → TOP, TOP → Zircon | Enzo + Elías | P1 | pending |
| [TASK-009](2026-05-10-task-009-pcb-json-04-20.md) | Subir PCB JSON + Schematic JSON del 2026-04-20 | Enzo | P0 | pending |
| [TASK-010](2026-05-10-task-010-verificar-firmware-rcj-esp32c6.md) | Verificar compat firmware oficial RCJ con ESP32-C6 | Virginia + Elías | P0 | **done** ✅ |
| [TASK-011](2026-05-15-task-011-confirmar-pin-kicker-solenoide-zircon.md) | Confirmar PIN_KICKER_SOL del solenoide en Zircon (ROBOT2) | Enzo | P1 | pending |
| [TASK-012](2026-05-15-task-012-activar-libs-otos-tof.md) | Activar libs reales OTOS (DOWN) + ToF (TOP) — salir de stub | Enzo + Elías | P0 | pending |

## Convenciones de archivo

```yaml
---
id: TASK-NNN
title: "Título corto"
date_created: YYYY-MM-DD
date_due: YYYY-MM-DD  # opcional
assigned: [handle1, handle2]
priority: P0|P1|P2
status: pending|in-progress|done|blocked
estimated_hours: N
blocks: [task-id-1, task-id-2]   # opcional — qué bloquea esta tarea
blocked_by: [task-id-1]           # opcional — qué la bloquea
tags: [hardware, firmware, ...]
---
```

Cuerpo:

1. **Resumen** — 1 línea, qué hay que hacer.
2. **Contexto** — por qué importa, qué pasa si no se hace.
3. **Pasos concretos** — numerados.
4. **Criterio de cierre** — checkboxes `- [ ]`.
5. **Notas / decisiones** — durante el trabajo.
6. **Cambios de estado** — log al pie.

## Anti-patterns

- ❌ Tareas sin asignado (no se hace).
- ❌ Tareas sin criterio de cierre (no se cierra).
- ❌ "Investigar X" sin acción concreta. Mejor: "Reportar resultados de X en `research/in-progress/`".
- ❌ Marcar `done` sin actualizar las "Notas" con qué se encontró/decidió.
- ❌ Crear tarea P0 que en realidad es P2 (inflación de prioridad).
