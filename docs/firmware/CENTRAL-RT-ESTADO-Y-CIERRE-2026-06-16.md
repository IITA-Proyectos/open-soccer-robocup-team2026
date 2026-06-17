---
title: "CENTRAL RT — estado de situacion + cierre para software cargable y optimizado (contratos TOP/DOWN intactos)"
date: 2026-06-16
author: "Claude (Anthropic - Claude Opus 4.7 1M) — coach"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M)"
status: vivo
tipo: estado-y-cierre
area: control
scope: src/central
robot: ambos
tags: [control, tiempo-real, no-bloqueante, monitor, estado, cierre]
---

# CENTRAL RT — estado de situacion + cierre (2 paginas)

> **Alcance fijo (pedido por Gustavo).** **NO se tocan los contratos TOP->CENTRAL
> (Serial7 / WorldSnapshot v3, 31 B) ni DOWN->CENTRAL (Serial1 / LineStatusV2 16 B
> + Pose2D + Velocity2D).** Toda mejora vive DENTRO de la CENTRAL: cambia cómo
> drena, cómo decide y cómo se diagnostica — no cambia QUÉ recibe.
>
> **Regla dura no negociable:** Claude NO cierra TASKs de hardware. Cada item con
> "BANCO" en esta hoja lo valida el equipo (Virginia/Elias) con la placa enfrente.

---

# Pagina 1 — Estado actual, disenos nuevos, mejoras esperadas, problemas

## 1. Como esta hoy el lazo (verificado en codigo, no de memoria)

El `loop()` vivo (`main_central.cpp:232-550`) corre asi cada vuelta, **sin un solo
`delay()`**, en orden fijo de prioridad:

1. `watchdog_feed()` (si `CENTRAL_ENABLE_WDT`) — WDOG1 a 1 s.
2. `loop_monitor_update()` — supervisor de loop-time (vivo, sin flag).
3. `comm_top_tick()` + `comm_down_tick()` — drenan **rings del core** (`while(available()) read()`,
   nunca esperan bytes). El parseo byte-a-byte + CRC + resync corre en el loop, no en ISR.
4. Handler USB (gateado): juez PC + caja negra + **parser del monitor**.
5. **Freno de borde con anti-latch 350 ms** (`main_central.cpp:388-415`) — `return` y
   listo si la linea esta inminente.
6. **Strategy a 100 Hz** (gate `g_since_strategy_tick >= 10 ms`) -> `strategy_tick()`
   -> (opcional) `motor_slew` -> `motors_apply_command()`.
7. Debug print 500 ms (gateado) + `central_telemetry_tick()` (gateado).

**Lo que YA es no bloqueante (no hay que arreglarlo):**
- El RX **ya es por ISR del core de Teensy**: cada `HardwareSerial` tiene su
  `IRQHandler` que llena un ring; `comm_*_tick()` solo VACIA, nunca espera. Esto
  es lo que el pedido inicial pedia "leer los puertos independientemente del bucle
  principal" — **ya esta hecho** desde hace meses, lo que faltaba era proteger el
  ring de desbordes silenciosos (item 6.5 abajo).
- DOWN tiene **ring extendido a 512 B** (`comm_down.cpp:86-95`).
- La FSM (`strategy.cpp`) tiene **0 `delay()` verificados**, timeouts no-bloqueantes
  con tabla `constexpr`, gate de 100 Hz limpio.
- `motors_brake()` y `motors_stop()` bypasean cualquier rampa (escritura directa de
  registros del H-bridge).

## 2. Lo nuevo que YA esta cableado (todo gateado, default OFF = binario byte-identico)

Trabajo verificado contra `main_central.cpp`, `comm_top.cpp`, `comm_down.cpp` HEAD `8d284ae`:

| # | Pieza | Gate | Donde vive | Estado |
|---|---|---|---|---|
| 1 | `loop_monitor` (max_us + EMA) | sin flag, vivo | `main_central.cpp:83,263` | ✅ vivo, sin tocar binario |
| 2 | WDOG1 (i.MX RT1062, 1 s) | `CENTRAL_ENABLE_WDT` | `main_central.cpp:107-123,219-226,233-239` | ✅ andamiaje listo, default OFF |
| 3 | Freno de borde + anti-latch 350 ms | sin flag, vivo | `main_central.cpp:388-415` | ✅ vivo (fix Maria 2026-06-14) |
| 4 | DOWN RX ring 512 B | sin flag, vivo | `comm_down.cpp:83-95` | ✅ vivo |
| 5 | TOP RX ring 512 B | `CENTRAL_TOP_RX_BIGBUF` | `comm_top.cpp:44-65` | ⚠️ andamiaje, **NO en competencia** |
| 6 | `motor_slew` (Capa 2, rampa eje) + bypass reflejos | `CENTRAL_MOTOR_SLEW` | `main_central.cpp:34-76,399-451`, env `central_robot2_strafe_slew_bb` | ✅ cableado gateado; titrar `DVX/DVY/DW` |
| 7 | **Monitor USB DORMIDO + parser de lineas + trampa 'S' resuelta** | `CENTRAL_USB_MONITOR` | `src/shared/telemetry_central.{h,cpp}` (puro, host-testeable) + `src/central/central_telemetry_serial.{h,cpp}` (glue) + `main_central.cpp:159-167,212-217,269-364,542-549` | ✅ FASE 1 cableada (banco pendiente TASK-106) |

**Punto load-bearing del item 7 (lo que pediste): el sistema de diagnostico
dormido embebido YA EXISTE en el firmware.** Arranca en silencio; despierta solo
si la app habla por USB (`STREAM ON`/`PING`); auto-off a los 3 s del silencio del
host -> **en partido, sin USB, nunca emite nada** (match-safe). La 'S' de `STREAM`
ya no frena el robot: hay un buffer por lineas (`main_central.cpp:292-324`) que
las pasa al parser ANTES del handler char-por-char.

Lo unico que falta para "cargar y andar optimizado" es **validar en banco** y
**activar los 2 flips de competencia** (items A/B/C de la hoja 2).

## 3. Disenos nuevos (modulos PUROS host-testeables, no cableados)

Lo entregado por el doc `ARQUITECTURA-LAZO-CENTRAL-RT.md` (2026-06-14, vigente):

| Modulo | Estado | Que hace |
|---|---|---|
| `src/shared/state_timer.h` | ✅ existe puro | Timer no-bloqueante uniforme para la FSM (mata las ~15 copias hand-rolled de `strategy.cpp`). **NO cableado** — es del rewrite FSM. |
| `src/shared/sensor_slot.h` | ✅ existe puro | **Pizarra doble-buffer (seqlock)** — la blindaje para el dia que el parseo pase a contexto async/ISR. Single-writer + lector wait-free. **NO cableado**: hoy no hay race porque el parseo es cooperativo. |
| `reflex.h` (Capa 1 formal) | ❌ no existe | Maquina de prioridades: STOP arbitro **preempta** + freno de borde formalizado. Hoy el freno de borde cumple el espiritu pero el STOP corre por el camino lento dentro de `strategy_tick`. |
| `gk_tuning.h` / `atk_tuning.h` | ❌ no existen | Tabla de jugadas separada del mecanismo (los ~80 constexpr sueltos en `strategy.cpp:142-461` agrupados en UN struct comentado en espanol). |

## 4. Mejoras esperadas (cuanto rinde cada cosa)

Numeros de orden de magnitud, no medidos en banco (la regla del repo es medir):

- **Quita de `CENTRAL_DEBUG_SERIAL` (item A2 hoja 2):** elimina el pico de jitter
  periodico de ~30 `Serial.print` cada 500 ms en el loop. Es el unico bloqueo
  ACTIVO conocido del loop (esta documentado como tal en `comm_top.cpp:46-50` y
  `main_central.cpp:467-473`).
- **TOP RX 64->512 B (item C):** elimina los descartes silenciosos cuando una
  vuelta se alarga. Hoy un loop de >2.8 ms desborda el ring del TOP (64 B / 230400 baud).
  Se mide por `resync_events()` cayendo a ~0 bajo carga.
- **`motor_slew` titrado:** arranque parejo en el strafe del arquero sin matar el
  kickstart (orden = slew -> cinematica -> pisos -> kickstart). Default placeholder
  poco-limitante; titrar con caja negra.
- **Monitor USB dormido en envs `*_bb`:** acceso a FSM + PWM por rueda + salud
  enlaces + OTOS + `loop_us(max/ema)` sin re-flashear y sin tocar el binario de
  competencia. Habilita la Fase 2 (calibrar parametros sin reflashear).

## 5. Problemas actuales (cuello, latencia, riesgo) — verificados

| # | Problema | Donde | Severidad |
|---|---|---|---|
| 5.1 | `CENTRAL_DEBUG_SERIAL` definido en envs de competencia | `platformio.ini` envs `central_robot1`/`central_robot2` | **P0** — unico bloqueo activo del loop |
| 5.2 | TOP RX en 64 B en competencia | `comm_top.cpp:60-65` (sin `addMemoryForRead`) | **P1** — desbordes silenciosos bajo ratamiento |
| 5.3 | Pizarra single-buffer (`world_model.cpp`) | `world_model.cpp:9-19,42,55` | **P2 hoy** (no hay race, RX cooperativo) — **P0 el dia que se migre a RX-ISR async** |
| 5.4 | STOP del arbitro corre por camino lento (no es reflejo) | `strategy.cpp` dentro del tick @100Hz | **P2** — costo: 0-10 ms de demora ante STOP |
| 5.5 | `strategy.cpp` mezcla mecanismo y tabla; ~15 timers hand-rolled | `strategy.cpp:142-461,97-126` | **P2** — no es RT, es legibilidad/seguridad |
| 5.6 | `motors_brake()` posiblemente COAST y no freno activo | `motors_zircon.h:24-29` (chip Zircon sin confirmar) | **P1** — si es COAST, el freno de borde es decorativo |
| 5.7 | TASK-106: monitor USB CENTRAL **sin validar en banco** | `team-tasks/` + `central_telemetry_serial.cpp` | **P0** — sin banco no se puede prender en cancha |

**Lo que NO es problema (y se decia que si):** el RX **no es** bloqueante hoy —
ya corre por ISR del core (Teensy `HardwareSerial::IRQHandler`). El `comm_*_tick`
es un *drenado* no-bloqueante. Lo que falta es CAPACIDAD (item 5.2) y proteccion
contra torn-read futuro (item 5.3), no "sacar el RX del loop".

---

# Pagina 2 — Que queda por hacer para software cargable y optimizado

> Orden = lo que se ejecuta primero. Cada item dice **quien lo cierra** (Claude
> programa; el equipo valida en banco). Mantiene los contratos TOP/DOWN intactos.

## A. Hoy/manana — Banco T1-T7 del monitor USB DORMIDO (TASK-106) — P0

**Quien:** equipo (Virginia/Elias). **Claude:** no toca, el firmware esta listo
desde 2026-06-15. **Tiempo:** 1-2 h banco. **Donde:** flashear un env `*_bb`
(p.ej. `central_robot2_arquero_bb`).

- **T1 — Byte-neutralidad (gate previo).** `pio run -e central_robot1` y `-e central_robot2`
  (envs de competencia, flags OFF) -> `cmp` con binario de HEAD = 0 diferencias.
- **T2 — Trampa de la 'S' (make-or-break).** Conectar app. ✅ si el robot **NO**
  entra en STOP al conectar (`STREAM ON\n` no dispara la 'S'). **Si esto falla,
  desactivar el monitor y abrir issue.**
- **T3 — Dormido en partido.** Monitor flasheado, USB desconectado. ✅ si el robot
  juega normal (cero bytes salen por USB).
- **T4 — Stream estable + jitter.** App conectada. ✅ si llegan >=15 Hz sin drops
  Y `loop_us(max)` no sube >X% sobre baseline (medir baseline primero, T4a).
- **T5 — Hot-swap 3 placas.** Mover cable USB entre CENTRAL/TOP/DOWN. ✅ si la app
  enrutea por `auto_parse` sin reconfigurar.
- **T6 — Salud refleja realidad.** Desconectar TOP->CENTRAL. ✅ si el semaforo TOP
  pasa a stale/rojo y `fr` deja de subir.
- **T7 — Caja negra convive.** `'d'/'x'` siguen volcando/borrando CSV con el
  monitor activo.

**Salida T1-T7 OK -> el monitor dormido queda APROBADO para todos los envs `*_bb`.**

## B. Mismo banco — Quitar `CENTRAL_DEBUG_SERIAL` de competencia (item 5.1) — P0

**Que:** borrar `-DCENTRAL_DEBUG_SERIAL` de los envs `central_robot1` y
`central_robot2` en `platformio.ini`. **Por que:** unico bloqueo activo conocido
del loop (~30 prints/500 ms). **Quien:** Claude (edicion 2 lineas) + equipo
(banco).
**Validacion banco:** `loop_us(max)` del `loop_monitor` baja vs baseline.
**Riesgo:** ninguno en cancha (en partido no hay USB para ver esa telemetria; la
ven con el monitor USB DORMIDO de A cuando lo conectan).

## C. Mismo banco — Activar `CENTRAL_TOP_RX_BIGBUF` en competencia (item 5.2) — P1

**Que:** agregar `-DCENTRAL_TOP_RX_BIGBUF` a `central_robot1` y `central_robot2`.
**Por que:** colchon de 13 frames del TOP en vez de 1. **Quien:** Claude (3 lineas)
+ equipo (banco). **Validacion banco:** correr 5 min de partido, `comm_top_get_resync_events()`
debe bajar a ~0 (medirlo con el monitor USB del item A: campo `top.rsy`).
**Riesgo:** ninguno (mas buffer solo puede ayudar; costo 512 B de SRAM en una placa
con 512 KB).

## D. Banco con caja negra — Titrar `motor_slew` (item 6 cableado) — P1

**Que:** correr `central_robot2_strafe_slew_bb` y barrer `-DCENTRAL_SLEW_DVX`,
`DVY`, `DW`. **Quien:** equipo (banco) + Claude (analisis offline de CSV).
**Criterio:** ✅ si el periodo del cruce lateral del arquero se mantiene Y la
caja negra muestra arranque parejo (no patina). ❌ si el kickstart se aplana ->
bajar slew.
**Default actual** (`main_central.cpp:65-73`): `DVX=120 mm/s, DVY=120, DW=600
centideg/s` por tick — poco limitante a proposito.

## E. Banco corto — Confirmar `motors_brake()` = freno activo (item 5.6) — P1

**Que:** identificar el integrado del Zircon (Enzo + datasheet) y medir freno vs
stop con tester. **Quien:** equipo. **Sin esto el freno de borde queda como
"decorativo".** Tiempo: 1-2 h.

## F. Despues de A-E — Activar el monitor por default en envs `*_bb` y registrar la CENTRAL en `monitor-base` — P1

**Que:** completar Sprint B del plan de monitor (ya iniciado el firmware): rama
`central` en `auto_parse`, `board_of()`, `BOARD_LONG/SHORT`, `_registry()` paneles
`panel_central` (cerebro) + `panel_central_health` (semaforos enlaces + OTOS +
loop_us). **Quien:** Claude. **Validacion:** smoke tests + correr contra firmware
flasheado en banco.

## G. Post-Incheon (capitalizable 2027) — Rewrite del loop + FSM prolija — P2

> **NO antes del mundial** (regla del repo: mejora corta y bien documentada >
> ambiciosa y opaca a 2 semanas de Corea).

| Item | Que | Modulo |
|---|---|---|
| G1 | **F3 — `reflex.h`** maquina de prioridades: STOP preempta freno + freno borde formalizado | crear `src/shared/reflex.h` + tests host con CSV de Maria como regresion |
| G2 | **F5 — Migrar RX a ISR async + cablear `sensor_slot.h`** (seqlock con `__DMB()`) — JUNTOS, mismo paso. Antes no aporta. | `comm_top.cpp`/`comm_down.cpp` + `world_model.cpp` |
| G3 | **F2 — `state_timer.h` cableado** + crear `gk_tuning.h`/`atk_tuning.h` + refactor `strategy.cpp` con "un estado = una funcion chica" + tabla de jugadas separada | toca el cerebro -> banco extenso |
| G4 | **Fase 2 monitor** — EEPROM + comandos `SET` para calibrar `MOTOR_MIN_PWM`, `EFF_X100`, `PFM_KD_RATE`, `PUSH_SPEED` sin reflashear | `src/shared/central_config.{h,cpp}` + `src/central/central_eeprom_config.{h,cpp}` |
| G5 | Cota `MAX_BYTES_PER_TICK` por vuelta de cada `comm_*_tick` (defensa contra rafagas) | `comm_top.cpp`/`comm_down.cpp` |

## H. Definicion de "software optimizado cargable" (criterio de cierre)

Se considera CERRADO cuando, en este orden, se cumple **todo**:

1. ✅ A (T1-T7 banco monitor) PASS.
2. ✅ B aplicado + `loop_us(max)` bajo vs baseline.
3. ✅ C aplicado + `comm_top_get_resync_events()` ~0 en 5 min de partido.
4. ✅ D titrado en banco con caja negra.
5. ✅ E confirmado.
6. ✅ F: monitor-base muestra la CENTRAL como 3ra placa sin reconfigurar.
7. ✅ Build de competencia (`central_robot1`/`central_robot2`) verificado con
   `pio check` + `pio run` SUCCESS + tests host 937/937.

Con A-F cerrados, los `.hex` de competencia son **mas confiables, mas eficientes
(menos jitter, menos descartes) y mas simples de entender** (FSM intacta, plomeria
mejorada por debajo). G es la inversion 2027.

---

## Apendice — Contratos preservados (recordatorio)

- **TOP -> CENTRAL:** Serial7 @230400, `WorldSnapshot v3` 31 B (`docs/firmware/CONTRATO-DATOS-TOP.md`).
- **DOWN -> CENTRAL:** Serial1 @230400, `LineStatusV2` 16 B + `Pose2D` + `Velocity2D` (`docs/firmware/CONTRATO-DATOS-DOWN.md`).
- **Ningun cambio de wire en este track.** Todo es interno a CENTRAL.

## Docs hermanos

- `docs/firmware/ARQUITECTURA-LAZO-CENTRAL-RT.md` — diseno completo de las 4 capas (vigente).
- `docs/firmware/PLAN-MONITOR-Y-CALIBRACION-CENTRAL.md` — plan del monitor + Fase 2.
- `journal/2026-06-15-integracion-rt-gateada.md` — que se cableo gateado el 2026-06-15.
- `docs/FUENTES-DE-VERDAD.md` — fila CENTRAL RT (debe actualizarse en el mismo commit que este doc).
