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
| [TASK-006](2026-05-10-task-006-cargar-firmware-rcj-comm.md) | Cargar firmware oficial RCJ en placa COMM | Virginia o Elías | **P0** | done ✅ (2026-06-01/02; árbitro queda en TASK-039) |
| [TASK-007](2026-05-10-task-007-verificar-bugs-p0-codigo-viejo.md) | Verificar si bugs P0 del código viejo se fixearon | Virginia + Elías | P1 | pending |
| [TASK-008](2026-05-10-task-008-rewiring-fisico-uart-robot.md) | Rewiring UART: OpenMV → TOP, TOP → Zircon | Enzo + Elías | P1 | pending |
| [TASK-009](2026-05-10-task-009-pcb-json-04-20.md) | Subir PCB JSON + Schematic JSON del 2026-04-20 | Enzo | P0 | pending |
| [TASK-010](2026-05-10-task-010-verificar-firmware-rcj-esp32c6.md) | Verificar compat firmware oficial RCJ con ESP32-C6 | Virginia + Elías | P0 | **done** ✅ |
| [TASK-011](2026-05-15-task-011-confirmar-pin-kicker-solenoide-zircon.md) | ~~Confirmar PIN_KICKER_SOL del solenoide en Zircon (ROBOT2)~~ — robot sin kicker físico (empuja por inercia) | Enzo | P1 | **cancelled** |
| [TASK-012](2026-05-15-task-012-activar-libs-otos-tof.md) | Activar libs reales OTOS (DOWN) + ToF (TOP) — salir de stub | Enzo + Elías | P0 | pending |
| [TASK-013](2026-05-17-task-013-recuperar-bom-placa-top.md) | Recuperar BOM + P&P + serigrafía de la placa TOP | Enzo | P1 | pending |
| [TASK-014](2026-05-18-task-014-loop-top-no-bloqueante-medir-periodo.md) | Loop de TOP no-bloqueante + medir período real (fundacional) | Virginia + Elías | **P0** | pending |
| [TASK-015](2026-05-18-task-015-crc-fin-trama-enlace-camara.md) | CRC + fin de trama en enlace de cámara OpenMV→TOP | Virginia | **P0** | pending |
| [TASK-016](2026-05-18-task-016-failsafe-borde-orlatch-precedencia.md) | Fail-safe de borde: OR-latch + precedencia ante LOST conjunto | Virginia + Elías | **P0** | pending |
| [TASK-017](2026-05-18-task-017-heartbeat-histeresis-seq.md) | Heartbeat + máquina OK/STALE/LOST con histéresis + SEQ | Virginia + Elías | P1 | pending |
| [TASK-018](2026-05-18-task-018-cota-drain-sin-serialclear.md) | Cotar drenado RX + NO introducir Serial.clear() | Virginia | P1 | pending |
| [TASK-019](2026-05-18-task-019-robustez-arranque-debug-handlers.md) | Robustez de arranque (guard last_ms>0 ×3, gate debug, handlers) | Virginia | P1 | pending |
| [TASK-020](2026-05-18-task-020-refactor-link-staticassert-config.md) | Refactor a módulo Link único + static_assert + config | Virginia + Enzo | P2 | pending |
| [TASK-021](2026-05-18-task-021-recuperacion-activa-wdt-reset.md) | Recuperación activa: WDT por placa + reset por comando + reset HW | Virginia + Elías | P1 | pending |
| [TASK-022](2026-05-18-task-022-camara-operativa.md) | Cámara operativa: sentinel, crash, exposición fija, calib mm+LAB, 1 script/cámara | Virginia | **P0** | pending |
| [TASK-023](2026-05-18-task-023-build-tooling-ci.md) | Build/tooling: doc compilar+flashear, CI, pinear platform, lib_deps | Virginia + Enzo | **P0** | pending |
| [TASK-024](2026-05-18-task-024-arranque-rol-polaridad-arco.md) | Arranque: leer rol, polaridad de arco, fallback START | Virginia + Elías | **P0** | pending |
| [TASK-025](2026-05-18-task-025-avast-bloquea-platformio-ssl.md) | Avast (SSL MITM) bloquea PlatformIO — excepción por máquina | Gustavo + Virginia + Elías | P1 | pending |
| [TASK-026](2026-05-19-task-026-confirmar-pinout-mux-down.md) | Confirmar pinout Teensy↔CD4051 en placa DOWN (bloquea cualquier test del anillo) | Enzo | **P0** | pending |
| [TASK-033](2026-05-25-task-033-decidir-cuantos-tofs-incheon.md) | Decidir cuántos TOFs llevar a Incheon (2 sin rework vs 4 con bodge) | Gustavo | P1 | pending |
| [TASK-034](2026-05-25-task-034-decidir-arquitectura-localizacion-incheon.md) | Decidir arquitectura de localización XY+heading para Incheon (5 alternativas) | Gustavo | P1 | pending |
| [TASK-200](2026-05-29-task-200-validar-fixes-heading-y-loop-top.md) | Validar en HW los 2 fixes del TOP (heading al CENTRAL + loop sin stall HC-SR04) | Virginia + Elías | P1 | pending |
| [TASK-301](2026-05-29-task-301-validar-hw-robustez-down.md) | Validar HW robustez DOWN (P0.2 calib EEPROM + P1.5 all-white + P1.6 backpressure) | Virginia + Elías | P1 | pending |
| [TASK-302](2026-05-29-task-302-vendorear-otos-offline.md) | Vendorear OTOS+Toolkit en lib/ para compilar [env:down] offline (Avast) | Gustavo / agente DOWN | P2 | **done** ✅ |
| [TASK-303](2026-06-03-task-303-validar-banco-correcciones-audit-down.md) | Validar en banco las 24 correcciones del audit DOWN 2026-06-03 + lista de pendientes (#1/#3/#5/#23) | Virginia + Elías + Gustavo | P1 | pending |
| [TASK-304](2026-06-06-task-304-down-firmware-debug-telemetria-calibracion.md) | **Modo DEBUG/telemetría + calibración en firmware DOWN (stream USB)** ⭐ próximo desarrollo | Virginia + Gustavo | **P0** | in-progress — v1 (módulo+protocolo+app+test host verde; glue Arduino pio-pendiente) |
| [TASK-305](2026-06-06-task-305-app-pc-monitoreo-calibracion-base.md) | **App PC monitoreo + calibración de la BASE (anillo de 32 + línea + LineStatusV2)** ⭐ | Virginia + Gustavo | **P0** | in-progress — v1 app Python (sim+pytest verde; cierre en banco) |
| [TASK-205](2026-06-06-task-205-top-debug-telemetria-y-app-monitoreo.md) | Modo DEBUG/telemetría TOP + app PC de monitoreo superior | Virginia | P1 | pending |

> **Nota 2026-06-06:** abierto el sistema de **monitoreo/telemetría USB + apps PC** (próximo desarrollo PRIORITARIO). Diseño en `research/in-progress/2026-06-06-diseno-monitoreo-telemetria-usb-y-apps-pc.md`. Orden: **TASK-304 (firmware base) → TASK-305 (app base)** [P0] ; luego TASK-205 (superior). Es la versión USB de hoy del roadmap de telemetría "estilo F1" (CANbus + ESP32, `MEJORAS-PENDIENTES.md` (e) E4).

> **Nota:** esta tabla quedó parcialmente desactualizada (faltan TASK-027 a 032
> y 035-038, que existen como archivos en este dir). El índice vivo está en
> `docs/ESTADO-ACTUAL.md`. Rango de TASK por placa (multi-agente): CENTRAL
> 100-199, TOP 200-299, DOWN 300-399; las viejas 001-099 no se renumeran.

> **Nota 2026-05-19 (cleanup quirúrgico):** análisis crítico determinó que el
> 100% del trabajo desde 2026-05-10 fue generado por sesiones Claude
> descoordinadas (69 commits, 49 docs, 25 tasks) sin que el equipo humano
> ejecutara ninguna TASK de hardware. **Moratoria de nuevos docs/specs/plans
> hasta primer hardware-up** (CLAUDE.md regla 8). Foco inmediato del equipo
> humano: TASK-001/002/006/011/022/025 (las P0 que requieren placa en mano).
> Ver índice canónico de fuentes en `docs/FUENTES-DE-VERDAD.md` y estado vivo
> en `docs/ESTADO-ACTUAL.md`.

> **Nota 2026-05-18:** TASK-014..020 abiertas tras la verificación independiente
> del protocolo de comms. **Orden de ejecución:** TASK-014 es fundacional
> (bloquea a TASK-017 y al rediseño de ventanas). P0 = 014/015/016. NO bajar el
> timeout de motores a 150 ms hasta cerrar TASK-014. Diseño definitivo:
> `docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md`.

> **Nota 2026-05-17:** TASK-006 corregida — el firmware correcto es el branch
> `esp32-c6` (no `master`/C5). Pin map y procedimiento nuevos en
> `hardware/electronics/comm-board/`. Ver journal `2026-05-17-analisis-3-placas-*`.

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
