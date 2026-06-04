---
title: "Handoff CENTRAL 2026-06-03 — qué se hizo y lista priorizada para continuar"
date: 2026-06-03
author: "Claude Opus 4.8 (Anthropic)"
ai-assisted: true
status: vivo
tipo: handoff-sesion
robot: CENTRAL (Teensy 4.1 / Zircon Rev v15)
related:
  - research/in-progress/2026-06-03-auditoria-firmware-central.md
  - team-tasks/2026-06-03-task-101-banco-mitad-inferior-cinematica-y-fork-arquero.md
  - team-tasks/2026-06-03-task-102-validar-banco-tier1-fixes-central.md
  - journal/2026-06-03-etapa2-tier1-fixes-central-host-tested.md
  - journal/2026-06-03-etapa2-tanda2-robustez-y-contrato-central.md
---

# Handoff CENTRAL — 2026-06-03

## Qué se hizo hoy (todo en `agente/central`, GitHub)

1. **Banco:** árbitro mueve la CENTRAL (HITO) + diagnóstico "solo gira M1" (deadzone +
   geometría kiwi) + freeze del BNO del TOP. → journal `banco-resultados-arbitro-strafe`.
2. **Programa nuevo:** `diag_central_arbitro_strafe` (patrulla del arquero gatillada por árbitro).
3. **Auditoría multi-agente del firmware CENTRAL** (6 agentes + verificación adversarial):
   46 hallazgos confirmados / 6 descartados → `research/in-progress/2026-06-03-auditoria-firmware-central.md`.
4. **Etapa 2 — 12 fixes implementados, host-verde (322 tests) + compilan ambos robots:**
   - Tier-1 (host-tested): **#9** omega int16 anti sign-flip, **#29** anti-windup real,
     **#5/#13** gates `data_valid`, **#25** resync telemetry, **#15/#17** constantes.
   - Tanda 2 (compile-verified): **#21** OTOS vel freshness, **#16/#19/#41/#23** honestidad de contrato/telemetría.

## Lista priorizada para CONTINUAR

### 🔑 PRIMERO — el test que destraba más cosas (tu banco, ~1-2 h)
**Validar el signo de omega (#8)** — comandar `omega>0` conocido y filmar (¿CCW o CW?),
luego cerrar el lazo con setpoint +30° y confirmar que **converge** (no diverge). Esto:
- valida el lazo de heading de TODO estado rotante (POSITION/APPROACH/KICKOFF/CLEAR),
- cierra el **#9** que ya implementé (el clamp evita el overflow; falta confirmar el sentido),
- se hace en la **misma sesión** que el test de velocidad de **TASK-101** (`-DDIAG_ARB_SPEED_MM_S=600`)
  que decide si el "solo gira M1" es deadzone o M2.

### 🔧 TU BANCO — validar lo ya implementado (TASK-102)
Los fixes #9/#29/#5/#13/#21 compilan y pasan host, pero cambian conducta → **validar en banco**:
- #9: con error de rumbo >120° el robot gira al lado **corto**, NO se invierte.
- #29: vuelve al setpoint **sin sobrepasar**.
- #5/#13: con `data_valid=0` el arquero NO strafea y LINE_AVOID no retrocede a basura.
- #21: kickoff/approach con OTOS sigue andando.

### 🔧 TU BANCO/DECISIÓN — Tier-2 de la auditoría (yo preparo, vos validás)
Orden sugerido por impacto:
1. **#10 deadzone / PWM mínimo** — ligado a TASK-101 (el "solo gira M1"). Medís el PWM de arranque por motor.
2. **#7 watchdog de pérdida de DOWN** — vos elegís cuánto capar la velocidad al perder la línea.
3. **#6 fuente de heading** (BNO congelado vs OTOS) — decisión + medir drift del OTOS.
4. **#11 brake vs coast** — identificar el chip driver del Zircon (Enzo) + medir frenado real.
5. **#2 derivative kick** (re-tunear kd con visión), **#4 debounce de `ball_visible`** (ver flicker real).
6. **#35 reset de PATROL**, **#26/#27/#28** (control), **#34/#36** (FSM arquero/delantero).

### 🤝 CROSS-BOARD — necesita al agente TOP
- **#1 `schema_version` en WorldSnapshot** — cambia el formato de wire; hay que tocar TOP y
  CENTRAL juntos o el link se rompe. Coordinar con el agente TOP.
- **Freeze del BNO** (del banco de hoy) — fix de fondo en el TOP (BNO a bus aparte / watchdog de yaw).

### 🛠️ AUTÓNOMO RESTANTE (bajo valor — solo si querés el backlog más limpio)
Micro-cosméticos sin banco: #38 (wrap de `loop_count`), #42 (drenado USB), #46 (comentario doble-saturación).

### 📦 POST-INCHEON
- **#33** unificar la FSM con `strategy_transitions` (refactor del cerebro, con la red de tests).
- **#14** política de los firmware-packs duplicados.
- **Harness host para `world_model`** (stub de `millis()`) → habilitaría host-test de #21 y futuros.

## Qué hago yo en la próxima sesión (según tu decisión)
- Si traés resultados de banco (#8 + TASK-101/102) → cargo los hallazgos y arranco el **v2
  del arquero con heading-hold OTOS** (ahora con base sólida: #9/#29 listos).
- Si querés → coordino el **#1** con el agente TOP.
- Si no → cierro acá; el track autónomo dio lo que tenía (12 fixes, todo verde).

## Atribución
Claude Opus 4.8 (Anthropic), 2026-06-03. Requested-by: Gustavo Viollaz (@gviollaz).
Validación en hardware: equipo humano (regla 1 CLAUDE.md).
