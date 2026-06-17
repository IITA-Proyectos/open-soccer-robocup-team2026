---
title: "CENTRAL RT — auditoría + cierre + reflex.h PURO (sin cambio de binario de competencia)"
date: 2026-06-16
author: "Claude (Anthropic - Claude Opus 4.7 1M) — coach"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M)"
status: final
tags: [control, tiempo-real, central, monitor, reflex, fuentes-de-verdad]
robot: ambos
area: control
tipo: estado-y-modulo-puro
---

# CENTRAL RT — cierre del 2026-06-16

## Contexto

Gustavo pidió analizar la CENTRAL como ingeniero senior de sistemas embedded RT,
evaluar demoras / no-bloqueo / monitor dormido, producir un doc 2-páginas, y
después "avanzar con la programación de los temas pendientes para tener la placa
operativa sin errores conocidos". Contratos TOP/DOWN intactos (alcance fijo).

## Qué se hizo

### 1. Auditoría 2-páginas — [`docs/firmware/CENTRAL-RT-ESTADO-Y-CIERRE-2026-06-16.md`](../docs/firmware/CENTRAL-RT-ESTADO-Y-CIERRE-2026-06-16.md)

Verificación contra `main_central.cpp` HEAD `8d284ae` (no de memoria). Hallazgo
load-bearing que corrigió el pedido inicial: **el RX YA es no-bloqueante hoy**.
El `IRQHandler` del core de Teensy llena el ring; `comm_*_tick()` solo VACIA con
`while(available()) read()`, nunca espera. Lo que faltaba no era "sacar el RX
del loop" sino capacidad (TOP en 64 B) y blindaje futuro (seqlock para cuando se
migre a RX async).

Página 1 = estado verificado + diseños nuevos + mejoras esperadas + 7 problemas
con severidad. Página 2 = plan A-H ordenado con quien cierra cada item (Claude
programa / equipo valida).

### 2. Sprint B del monitor — VERIFICADO COMPLETO (no era pendiente)

La auditoría sorpresa de `tools/monitor-base/`:

- ✅ `protocol_central.py` con `CentralFrame` + `parse_line_central` + validador `v=1`
- ✅ `sources.py:118-119` rutea `"central"` a `parse_line_central`
- ✅ `gui_shell.py:44-45` `board_of()` reconoce `CentralFrame`; `BOARD_LONG`/`BOARD_SHORT`/`last_by_board`/`metrics_by` con `central`
- ✅ `panel_central.py` + `panel_central_health.py` registrados en `_registry()`
- ✅ `simulator_central.py` + `test_simulator_central.py` (8 tests)
- ✅ `pytest`: **235/236 passed** — el único fail es `test_tooltip.py` por
  `tk.tcl` mal instalado en el Python local (`C:/Python314/tcl/tk8.6/ttk/combobox.tcl`
  no existe). Es entorno, NO regresión de código.

Conclusión: la FASE 1 del [`PLAN-MONITOR-Y-CALIBRACION-CENTRAL.md`](../docs/firmware/PLAN-MONITOR-Y-CALIBRACION-CENTRAL.md)
está completa firmware-side Y app-side. **Solo falta T1-T7 en banco (TASK-106).**

### 3. `src/shared/reflex.h` PURO + `test/test_reflex/` — CREADO

Por qué: la Capa 1 del lazo RT vivía hardcodeada en `main_central.cpp:388-415`
(freno de borde con anti-latch 350 ms, fix María 2026-06-14). El doc de
arquitectura la marca para formalizar como módulo puro junto a la prioridad
STOP del árbitro. Hoy el STOP corre por el camino LENTO (dentro del tick de
strategy a 100 Hz) — un reflejo formal lo preempta.

**Lo que devuelve el módulo** (POD + 1 función pura):

| Acción | Cuándo | Qué hace el caller |
|---|---|---|
| `STOP_MOTORS` | `!match_running` (P-ALTA, gana sobre todo) | `motors_stop()` + `return` |
| `BRAKE_EDGE` | `edge_now` y `< 350 ms` desde el primer edge (P-MEDIA) | `motors_brake()` + `return` |
| `RELEASE_TO_FSM` | `edge_now` pero `>= 350 ms` (anti-latch vencido) | dejar correr `strategy_tick` (la FSM ESCAPE despega al robot) |
| `NONE` | `edge_now=false` (resetea estado del borde) | FSM normal |

**Espejo EXACTO** del freno de borde vivo en `main_central.cpp:388-415` —
mismas semánticas, misma constante 350 ms, misma máquina (`edge_since_ms` /
`edge_released`), mismo re-arme cuando `edge_now` baja. La única diferencia es
P-ALTA (STOP preempta el borde), que es el cambio de conducta declarado como
riesgo R5 a validar en banco.

**Tests host** (13/13 verde, `bash scripts/run-host-tests.sh test_reflex`):

1. STOP gana sin borde
2. **STOP preempta borde** (cambio de conducta vs hoy)
3. STOP resetea estado del borde (al volver a RUN, anti-latch arranca limpio)
4. Borde reciente → `BRAKE_EDGE`
5. Borde dentro del anti-latch → `BRAKE_EDGE`
6. Borde en el borde exacto (>=350 ms) → `RELEASE_TO_FSM`
7. Borde sostenido bien pasado → `RELEASE_TO_FSM` (sostenido, NO re-frena)
8. Sin borde → `NONE`
9. Borde baja y vuelve → re-arma anti-latch desde cero
10. Wrap-safe del anti-latch (overflow de `millis()`)
11. Override de `edge_max_ms` (testing/titración)
12. **REGRESIÓN: deadlock 5+ s de María** — simula 5 s de loop @100 Hz con
    edge_now sostenido: BRAKE solo cubre los primeros ~35 ticks, RELEASE
    sostiene los restantes ~465. Si el módulo re-cayera en el bug, brake_count
    sería 501 y release_count = 0.
13. REGRESIÓN: RELEASE no es pegamento — si la línea se va y vuelve, BRAKE
    arranca de nuevo (no salta directo a RELEASE).

**Estado:** **PURO + host-tested + NO cableado**. Gate previsto
`-DCENTRAL_REFLEX_LAYER` (default OFF, se cabela cuando el equipo reescriba el
loop post-Incheon con caja negra + CSV de María como regresión).

### 4. NO se creó `gk_tuning.h`/`atk_tuning.h` — decisión honesta

El doc de arquitectura marca este módulo como F2 (tabla de jugadas separada
del mecanismo). Después de leer `strategy.cpp:142-461` (~80 constexpr):

- Muchos tienen `#ifdef` build-time: `ATK_OTOS_NOGYRO`, `ATK_SEARCH_SPIN_ONLY`,
  `CENTRAL_FLOOR_SCALE`. Extraerlos naive a un struct rompe builds por-env.
- Es el riesgo #7 explícito del doc de arquitectura: "Sobre-ingeniería a 2
  semanas de Incheon. Tocar el cerebro arriesga el binario de competencia por
  una mejora que rinde post-mundial."
- No aporta a "placa operativa sin errores conocidos" hoy — es pre-trabajo
  post-Incheon que rompería builds si se hace mal.

Defer post-Incheon. La fila CENTRAL RT de `FUENTES-DE-VERDAD.md` lo dice
explícito ahora.

### 5. NO se tocaron envs de competencia

Items B (`CENTRAL_DEBUG_SERIAL`) y C (`CENTRAL_TOP_RX_BIGBUF`) cambian el
binario de `central_robot1/central_robot2`. La regla dura del repo
(CLAUDE.md punto 1) prohíbe cambiar binarios de competencia sin banco. Esos
flips quedan como tema-a-analizar prioridad **P0** para el próximo banco,
documentados en la pág. 2 del doc de cierre (items A-C).

## Verificación

- **`bash scripts/run-host-tests.sh test_reflex`** → 13/13 verde.
- **`bash scripts/run-host-tests.sh`** (suite completa) → **91 suites,
  1214 tests, 0 fails, 0 skips**. test_reflex incluido.
- **`pytest` monitor-base** → 235/236 verde (`test_tooltip` = tk.tcl
  entorno, no regresión).
- **NO se compilaron envs Teensy** (sin toolchain en PATH); `reflex.h` es PURO,
  compila host con g++. El equipo verifica el `pio run -e central_robot1/2` en
  el próximo banco — debe ser **byte-idéntico** (gate `CENTRAL_REFLEX_LAYER`
  no se define en ningún env todavía).

## Pendiente del equipo (Claude NO cierra TASKs de HW)

Por orden de prioridad — todo documentado en pág. 2 del doc de cierre:

1. **A — TASK-106 banco T1-T7 del monitor USB DORMIDO** (P0, make-or-break:
   Test 2 = la 'S' de STREAM no frena el robot).
2. **B — flip `CENTRAL_DEBUG_SERIAL` en competencia** (P0, único bloqueo
   activo conocido del loop).
3. **C — flip `CENTRAL_TOP_RX_BIGBUF` en competencia** (P1, elimina descartes
   silenciosos del TOP).
4. **D — titrar `motor_slew`** con caja negra (env `central_robot2_strafe_slew_bb`).
5. **E — confirmar `motors_brake()` = freno activo (no COAST)** del chip Zircon
   (Enzo + datasheet + medición).

## Próximos pasos (post-Incheon)

- Cablear `reflex.h` con `-DCENTRAL_REFLEX_LAYER` + regresión CSV María.
- Migrar RX a ISR + cablear `sensor_slot.h` seqlock JUNTOS (F5 del doc).
- Crear `gk_tuning.h`/`atk_tuning.h` con cuidado de los `#ifdef` build-time.
- Refactorizar `strategy.cpp` con "un estado = una función" + `state_timer.h`
  cableado (F2-F3 del doc).
- Fase 2 monitor: EEPROM + comandos `SET` para calibrar sin reflashear.

## Archivos tocados (commit)

- **Nuevo:** `software/teensy/Soccer 2026/src/shared/reflex.h`
- **Nuevo:** `software/teensy/Soccer 2026/test/test_reflex/test_main.cpp`
- **Nuevo:** `docs/firmware/CENTRAL-RT-ESTADO-Y-CIERRE-2026-06-16.md`
- **Modificado:** `docs/FUENTES-DE-VERDAD.md` (fila CENTRAL RT + fila nueva del
  doc de cierre)
- **Modificado:** `docs/ESTADO-ACTUAL.md` (banner del día)
- **Nuevo:** este journal
