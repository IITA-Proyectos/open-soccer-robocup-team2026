---
title: "2026-05-18 — Ejecución Plan 1: DOWN line-sensing core (subagent-driven, TDD)"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [firmware, down-board, tdd, ejecucion, resultado]
robot: ambos
area: comunicacion
tipo: resultado
related: [docs/superpowers/plans/2026-05-18-down-line-sensing-core.md, docs/firmware/CONTRATO-DATOS-DOWN.md]
related-tasks: [TASK-008, TASK-009, TASK-023]
---

# Ejecución Plan 1 — DOWN line-sensing core

## Contexto

Ejecución subagent-driven (TDD estricto, review spec+calidad por task) del plan
`docs/superpowers/plans/2026-05-18-down-line-sensing-core.md`. Objetivo: convertir
el lado de sensado de línea de DOWN de esqueleto a **candidato compilable y
testeado**, sin depender de OTOS.

## Qué se hizo

9 tasks, cada una con test que falla → implementación → test verde → review
spec + review calidad → fixes → commit. Módulos puros nuevos en `src/shared/`
(host-testables): `line_geometry` (ángulo/centroide/escape + CORNER),
`line_tracker` (LINE_END), `line_calib` (estática + adaptativa + CALIB_SUSPECT),
`surface_monitor` (lifted + data_valid), `down_model` (orquestador →
`LineStatusV2`), `down_encode` (frame proto). `LineStatusV2` (16 B) en `types.h`.
Glue HW en `src/down/comm_central.cpp` + getters triviales en `line_ring`.

## Qué se midió / observó

- **162/162 test cases PASSED** en 13 suites host (`pio test -e test_native`),
  incluidas las 6 nuevas de DOWN. Cero warnings -Wall/-Wextra.
- **`pio run -e down` → SUCCESS** (`firmware.hex`, FLASH 25 KB). DOWN compila
  contra la toolchain Teensy real.
- `down_encode` valida **byte-a-byte** el frame del contrato §3.6 (CRC 0xDFBF
  recalculado independientemente por un revisor → test no fudgeado).

## Incidentes y correcciones (registro honesto)

1. **Unity corrupto por un subagente fuera de scope.** Un subagente de fix
   thrasheó (114 tool-calls/38 min) y editó `unity.c` (dependencia) intentando
   silenciar warnings → rompió todos los tests. Se borró el libdep corrupto y
   el usuario reinstaló Unity limpio. **Medida:** guardarraíl en todos los
   subagentes siguientes (prohibido tocar `.pio/`/dependencias; ante error de
   entorno PARAR y reportar; scope/tiempo acotado). No volvió a ocurrir.
2. **Bug de infra pre-existente:** ningún binario de firmware compilaba —
   `[env:down/top/central*]` en `platformio.ini` no tenían `-I src/shared`
   (solo `test_native`). Fix aplicado a los 4 envs (`581f14f`).
3. **`config_down.h` no incluía `<Arduino.h>`** → `A0..A3` no declarados. Fix
   seguro no-semántico (`#include <Arduino.h>`), NO cambia números de pin.
4. **2 P1 del review holístico final** (gaps reales del path vivo, no cubiertos
   por el test de encode hand-crafted): `sample_age_ms` siempre 0 y `g_dm_init`
   no se reseteaba al recalibrar. Ambos corregidos (`12c62ab`).

## Conclusión

DOWN line-sensing core es **candidato real**: compila, 162 tests verdes,
contrato verificado byte-a-byte. Funciona **sin OTOS** (requisito del equipo).

## Deferred / pendiente (honesto — no oculto)

- **Pin VALUES de `config_down.h` "tentativo"** (qué pin físico es cada mux):
  pendiente confirmación HW → **TASK-009** (compila, pero el mapeo HW no está
  garantizado por el build).
- **UART físico DOWN→CENTRAL** sin confirmar → **TASK-008** (medir osciloscopio).
- `penetration_mm` (proxy = #sensores) y `cross_track_mm` (N/A): diferidos a
  **Plan 3** (necesitan geometría calibrada + ref-edge físico). Marcados en
  código y contrato.
- `quality`: placeholder {0,85,95}; métrica SNR real → Plan 3.
- **OTOS**: fuera de scope (Plan 2). DOWN emite Pose2D con confidence=0 honesto.
- `EV_MUX_DEAD` definido en el contrato pero no emitido (hoy `n<32` setea
  `EV_DEGRADED_GEOMETRY`). Decisión: documentar / mux-health readback es Plan 2/3.
- Hardening de build/CI/lib_deps (OTOS/ToF) → **TASK-023**.

## Próximos pasos

- Plan 2 (OTOS detrás de `IOdometrySource`+mock) y Plan 3 (penetration/
  cross_track reales) cuando haya hardware.
- TASK-008/009/023 para bring-up físico.
- Replicar el patrón (contrato → plan → subagent-TDD) para CENTRAL, TOP y
  cámaras (contratos ya escritos: `docs/firmware/CONTRATO-DATOS-{CENTRAL,TOP,CAMARAS}.md`).
