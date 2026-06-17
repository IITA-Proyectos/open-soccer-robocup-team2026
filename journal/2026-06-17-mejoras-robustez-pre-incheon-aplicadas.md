---
title: "Mejoras de robustez pre-Incheon aplicadas (3 de la auditoría): delay arquero + BIGBUF R1 + paridad watchdog"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: código aplicado + compila; cierre = banco (TASK-110/112)
tipo: implementacion
origen: "research/completed/2026-06-17-auditoria-robustez-programas-competencia.md"
---

# Mejoras de robustez pre-Incheon (las 3 del ranking del auditor)

De la auditoría de robustez (ciclo detección→multi-ángulo→auditor independiente), se aplicaron
las 3 mejoras "antes de Incheon", en el orden pedido por Gustavo (#2 → #3 → #1). Ninguna cierra
en código — el cierre es banco.

## Corrección importante (verificar antes de afirmar)
Antes de aplicar #3, verifiqué contra la fuente de verdad `QUE-FLASHEO-HOY.md:23`: **R1 TOP
flashea `top_robot2_pri`** (igual que R2; los `top_robot1*` son cableado VIEJO, NO se flashean).
→ El hallazgo del agente "R1 corre HC-SR04 BLOQUEANTE (top_robot1_pri_fastbno)" era **INCORRECTO**:
ese env no se usa en R1. El TOP de R1 ya tiene TODOS los flags RT (async US, snapshot timer,
sched, freeze-detect). Por eso #3 se redujo a UN flag de CENTRAL, no a tocar el TOP.

## #2 — Delay de arranque del arquero (P2, el más barato)
- `strategy.cpp:260`: `GK_START_DELAY_MS` 2000 → **200**. Con 2000 el arco quedaba descubierto 2 s
  en cada saque. NO se puso 0 estricto (recomendación del auditor): ~200 ms da margen a que el
  primer snapshot tras el GO traiga `heading_valid` para el gyro-hold del retroceso.
- Afecta a todos los envs del arquero (constante de la FSM). Host: `test_strategy_transitions` 39/39.
- Banco: confirmar que el arquero arranca ~al instante del GO y el retroceso sale derecho.

## #3 — R1 subconjunto seguro (P1) → se redujo a 1 flag
- `central_robot1`: AGREGADO `-DCENTRAL_TOP_RX_BIGBUF` (R2 ya lo tenía). El arquero R1 es el que
  más retiene el loop (freno de borde) → con el ring RX de 64 B se descartaban snapshots del TOP
  en silencio bajo jitter ("se congela a tirones"). Sube el ring de Serial7 a 512 B (~13 frames).
  **Fail-safe puro** (no cambia lógica; el auditor lo clasificó "aceptar sin banco").
- El TOP de R1 NO se tocó (ya corre top_robot2_pri con todo). Los flags RIESGOSOS de R1 que el
  agente sugería (freeze-detect/snapshot-timer/bno-fast en top_robot1*) NO aplican y, de todos
  modos, NO van a competencia sin banco — quedan post-Incheon.

## #1 — Watchdog: paridad (P0) — SIN promover al binario base
- Existían `central_robot1_wdt`, `central_robot2_wdt`, `down_wdt`. CREADO `down_robot2_wdt`
  (faltaba la paridad de R2). Ahora los 4 binarios de competencia tienen su env candidato `*_wdt`.
- **NO se promovió `*_ENABLE_WDT` al binario de partido** (central_robot1/2, down/down_robot2):
  el auditor fue claro — banco PRIMERO (30 min sin reset espurio + hang-test que confirme
  `WDOG1_WRSR`), recién ahí mover el flag al base. Promoverlo sin validar arriesga un reset en
  pleno partido. Claude prepara los envs, el equipo valida y promueve (TASK-110 P0.1 / TASK-112).

## Lo que NO se hizo (decisión del ciclo, no olvido)
- **CLEAR direccional (clear_aim):** "si sobra banco". Requiere validar el `goal_own_angle` de la
  cámara trasera primero (deuda abierta). No se cableó.
- **Modo conservador de DOWN (P-D):** SOLO-DOCUMENTAR (el auditor lo frenó: parche en el cerebro
  sin pose absoluta arriesga más de lo que arregla). Queda como limitación conocida.
- **Flags RT riesgosos de R1, freeze-detector umbral, motors_brake COAST:** post-Incheon / banco.

## Verificación
- `test_strategy_transitions` 39/39 (delay no rompe la FSM).
- 8 envs compilan (ver siguiente; central_robot1/2 + 4 wdt + down/down_robot2).
- "Compila" no es "funciona": las 3 cierran en banco.

## Archivos
- `src/central/strategy.cpp` (delay).
- `platformio.ini` (BIGBUF en central_robot1; +env down_robot2_wdt).
- Docs: este journal + research de la auditoría (origen).

## Banco (lo cierra el equipo)
1. **#2:** arranque del arquero ~instantáneo + retroceso derecho.
2. **#3:** R1 sin "congelarse a tirones" (loop_us/resync del link TOP estables).
3. **#1:** los 4 `*_wdt` → 30 min sin reset + hang-test → promover el flag al binario de partido.
