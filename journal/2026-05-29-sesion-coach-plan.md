---
title: "Sesión de coach 2026-05-29 — análisis estratégico + plan de prioridades (T-32 a Incheon)"
date: 2026-05-29
author: "Claude Opus 4 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus, Anthropic)"
status: final
tags: [coach, planificacion, multi-agente, incheon, prioridades, documentacion]
robot: ambos
area: control
tipo: analisis
---

# Sesión de coach 2026-05-29 — análisis estratégico + plan (T-32 a Incheon)

> **TL;DR.** Los 3 agentes (central/top/down) están trabajando pero **nada
> commiteado** — el trabajo vive como cambios sin commit en sus worktrees,
> frágil e inmergeable. Convergen en una sola feature coherente (pipeline de
> línea DOWN→CENTRAL) sin forkear el contrato `LineStatusV2`. Mientras tanto,
> los **dos bloqueantes de homologación/partido** (TASK-006 flashear COMM,
> TASK-022 cámara operativa) llevan semanas sin moverse porque necesitan
> **humano + placa**, no más firmware. El plan de mañana prioriza esos
> bloqueantes humanos/hardware por encima de seguir puliendo firmware.

## 1. Estado de los 3 agentes (hallazgo principal)

Las 4 worktrees siguen byte-idénticas a `9ef90e3` en el historial git, pero
hay trabajo **en curso sin commitear**:

| Agente | Archivos (sin commit) | Tema |
|---|---|---|
| **central** | `comm_down.cpp`, `world_model.{cpp,h}`, +nuevo `src/shared/line_view.h`, +nuevo test `test_central_line_ingest/` | CENTRAL **consume** la línea de DOWN |
| **down** | `src/shared/down_model.cpp`, `src/shared/line_filters.{cpp,h}`, `comm_central.cpp`, +2 tests | DOWN **refina y produce** el contrato de línea |
| **top** | `sensors_tof.cpp` (+28), `main_top.cpp`, `comm_down.cpp` (−10) | TOP endurece ToF (coherente con el hallazgo XSHUT del 05-25) |

- **Bueno**: convergen en una feature coherente (pipeline de línea DOWN→CENTRAL).
  Ni central ni down forkearon `types.h`/`proto.h` — el struct `LineStatusV2`
  (16 bytes) está intacto en ambos. `line_view.h` valida
  `payload_len == sizeof(LineStatusV2)` como compuerta fail-safe. Son archivos
  distintos en `src/shared/` → no se anticipa conflicto textual de git.
- **Riesgo real**: trabajo sin commitear = frágil (un accidente de worktree lo
  borra), invisible al historial, e **inmergeable** (no se puede `git merge
  --no-ff` lo que no está commiteado).

## 2. Análisis estratégico: hecho vs. falta

**Hecho / hardware-up (verificado en journal + código vivo):**
- DOWN: anillo de 32 sensores de línea arriba (05-24) + 2 OTOS activados con
  lib SparkFun real (confirmado en `otos.cpp`, no stub) + tests host verdes.
- TOP: 1 ToF VL53L7CX frontal arriba con lib Adafruit (05-24).
- COMM: firmware RCJ verificado compatible con ESP32-C6 (TASK-010 ✅) — **aún
  no flasheado**.
- Firmware base: 180 tests host verdes; HAL Sprint A hecho en TOP.

**Falta — la tensión estratégica:** los agentes pulen el pipeline de línea
mientras lo que decide "¿compite o no?" no se mueve porque necesita humano + placa:
- **TASK-006** (flashear COMM ESP32-C6) → `pending`. Sin START/STOP de árbitro,
  el robot no se puede arrancar en cancha = homologación imposible.
- **TASK-022** (cámara operativa) → `pending`. Sin visión, no encuentra la
  pelota = no juega.
- **TASK-024** (rol + polaridad de arco al arranque) → `pending`. Sin esto puede
  jugar hacia su propio arco.
- Decisiones de coach abiertas que bloquean a los agentes: **TASK-033** (2 vs 4
  ToFs) y **TASK-034** (arquitectura de localización).

## 3. Plan de prioridades para mañana

> Formato coach: *tema a ejecutar* con riesgo-si-no / riesgo-si-sí / tiempo /
> plan de prueba HW. Las TASKs de hardware las cierra quien las prueba en placa.

### 🔴 P0 — destraban "¿compite o no?"

**P0-1 — Consolidar el trabajo de los 3 agentes (commit → merge → gate).**
- Riesgo si no: las 3 worktrees pierden el trabajo ante cualquier accidente; no
  hay baseline limpia.
- Riesgo si sí: bajo. Tocan `src/shared/` pero archivos distintos; revisar 5 min
  que el `LineStatusV2` que produce down == el que consume `line_view.h`
  (16 bytes, flags `EV_*`, sentinels `LSV2_NA_*`).
- Tiempo: 30–45 min.
- Plan: (a) cada agente commitea en su rama con atribución; (b) Gustavo
  `git merge --no-ff agente/down`, luego `central`, luego `top`; (c) **gate tras
  cada merge**: `pio run` + `pio test -e native` (180 tests) deben quedar verdes.

**P0-2 — TASK-006: flashear COMM (ESP32-C6).** *(Virginia o Elías)*
- Riesgo si no: homologación imposible → no compite.
- Riesgo si sí: bajo (procedimiento documentado, branch `esp32-c6`).
- Tiempo: 1–2 h.
- Plan: flashear → botón START de árbitro → confirmar que el robot recibe START
  por el link COMM y que STOP lo frena.

**P0-3 — TASK-022: cámara operativa.** *(Virginia)*
- Riesgo si no: robot ciego → no hay partido real.
- Riesgo si sí: medio (exposición fija, calib mm+LAB, sentinel/anti-crash).
- Tiempo: medio día.
- Plan: pelota a distancias/ángulos conocidos → reporta mm + ángulo estables,
  sin crash durante N minutos, sentinel ante frame perdido.

### 🟠 P1 — impacto de partido / desbloquean a los agentes

- **P1-1 — TASK-024: rol + polaridad de arco + fallback START.** *(Virginia/Elías)*
  El firmware de mayor valor de partido. Tiempo 2–3 h. Plan: bootear cada robot,
  confirmar por debug rol leído + polaridad correcta; simular caída del link de
  árbitro → fallback START.
- **P1-2 — Decisiones de coach TASK-033 + TASK-034.** *(Gustavo)* 30 min de
  decisión que destraban al agente top y a localización. Recomendación de coach
  registrada: 2 ToFs sin rework para Incheon.
- **P1-3 — TASK-036 / TASK-037: bench-test motores + drive-straight de CENTRAL.**
  Sketches diag ya compilados (commit `868852e`). Tiempo 1–2 h en banco. Plan:
  cada motor gira en el sentido correcto + drive-straight va derecho.

### 🟡 P2 / estratégico
- **Entregables de competencia (TDP, poster, video de homologación, registración
  Incheon) sin dueño ni fecha.** Tienen deadlines duros de RoboCup. Asignar dueño
  + fecha esta semana aunque la ejecución sea más adelante.

## 4. Documentación: inconsistencias

**Corregidas en esta sesión** (este commit):
- `README.md` — diagrama de estructura a la realidad (3 placas, firmware vivo,
  `docs/`, `team-tasks/`, `skills/`).
- `AI-INSTRUCTIONS.md` — hardware a la arquitectura 3-placas + ESTADO-ACTUAL /
  FUENTES-DE-VERDAD como primeras lecturas obligatorias.
- `docs/README.md` — callout de índices vivos + estructura al día.
- `CONTRIBUTING.md` — modelo `develop/feature/*` obsoleto → modelo de worktrees.
- `docs/ESTADO-ACTUAL.md` — frontmatter + título + calendario a 2026-05-29 (≈32 días).
- `CHANGELOG.md` — banner: no es fuente de verdad; redirige a ESTADO-ACTUAL + journal.
- `REFERENCE.md` — tabla de hardware completa (3× Teensy, ESP32-C6, OTOS, VL53L7CX, anillo).

**Flag — NO tocadas a propósito** (requieren estado humano-propietario / son
contradicciones intencionales):
- `team-tasks/README.md` — tabla "al 2026-05-15", le faltan ~14 task files
  (027–032, 035–038) y lista TASK-012/026 como `pending` aunque el hardware ya
  está arriba. Reconciliar con los archivos individuales.
- Docs marcados como superados en `FUENTES-DE-VERDAD.md` (contradicciones
  intencionales: WorldSnapshot 24B vs v2, ejes invertidos en
  FIRMWARE-PLACA-ARRIBA, SEEK/DRIVE vs SEARCH/POSITION).

## 5. Atribución

- **Análisis estratégico, plan de prioridades, auditoría de worktrees y
  correcciones de documentación general** — Claude Opus 4 (Anthropic), vía
  Claude Code, sesión 2026-05-29.
- **Encuadre de la sesión de coach, decisiones de scope y dirección estratégica**
  — Gustavo Viollaz (@gviollaz).

## 6. Referencias

- Estado vivo: `docs/ESTADO-ACTUAL.md`
- Doc canónico por tema: `docs/FUENTES-DE-VERDAD.md`
- Hallazgo XSHUT TOP: `journal/2026-05-25-top-xshut-no-routed-hallazgo-forense.md`
- Hardware-up DOWN: `journal/2026-05-24-hardware-up-down-anillo-linea.md`
- Procedimiento flash COMM: `journal/2026-05-15-firmware-comm-c6-flash-procedure.md`
- Tasks clave: `team-tasks/2026-05-10-task-006-cargar-firmware-rcj-comm.md`,
  `team-tasks/2026-05-18-task-022-camara-operativa.md`,
  `team-tasks/2026-05-18-task-024-arranque-rol-polaridad-arco.md`
