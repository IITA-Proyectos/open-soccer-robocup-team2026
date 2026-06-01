---
title: "Analisis profundo - comunicacion DOWN<->CENTRAL (+TOP)"
date: 2026-05-31
status: vivo
audiencia: "equipo IITA + sesiones Claude"
author: "Claude Opus 4.8 (Anthropic) - via workflow multi-agente"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
metodologia: "56 agentes: 8 lectores en paralelo (protocolo/CRC, buffers/recuperacion, linea, OTOS, link DOWN->CENTRAL, link TOP->CENTRAL, contratos, tests) + verificacion adversarial por hallazgo (confirmado/refutado/incierto) + sintesis. Refutados descartados; inciertos marcados (a verificar). NO validado en hardware."
tags: [analisis, comunicacion, down, central, top, protocolo, buffers, timing, incheon]
---
# Analisis profundo — comunicacion DOWN<->CENTRAL (+TOP)

> **Actualización 2026-05-31 (posterior al análisis, commit `bb298d4` / TASK-204):**
> la cámara trasera quedó soldada en **Serial5**, así que el link **TOP→CENTRAL se
> movió de Serial5 a Serial7** (ya implementado por el agente TOP). Donde más abajo
> diga *"Serial5 → CENTRAL"* (p. ej. en "Cómo funciona hoy"), leer **Serial7 → CENTRAL**;
> los hallazgos **TOP-CEN-01 y TOP-CEN-04** (Serial2/Serial5 stale) quedaron
> **resueltos**. El **DOWN→TOP sí sigue en Serial5** (odometría OTOS) — eso no cambia.
> El CENTRAL tampoco cambió (recibe el snapshot en Serial1 / pin 0).

## TL;DR (lo mas importante)

- **El link DOWN→CENTRAL funciona en codigo y esta bien diseñado en su nucleo.** Framing (proto.h/cpp), CRC-16/CCITT-FALSE, baudios (230400 ambos lados) y backpressure del emisor estan solidos y testeados host-native. El problema NO es el protocolo: es (a) un bloqueante de hardware sin cerrar, (b) bugs de "frescura" que cargan trampas para mas adelante, y (c) el firmware de competencia que compila degradado.
- **P0 real y unico bloqueante: el conflicto de pines 7/8 (Serial2 RX vs motor del driver U17, TASK-036) NO esta aislado.** Si esos pines son del motor, `motors_init()` los pone OUTPUT y pisa el UART → CENTRAL nunca recibe a DOWN cuando los motores estan activos. Esto hay que resolverlo en banco ANTES de cualquier otra cosa. (a verificar en hardware)
- **El firmware de competencia `[env:down]` compila con 1 mux (8 sensores) y 1 OTOS, no con los 32 sensores validados.** Una linea de build flag. Si se flashea para competir, el arquero ve un cuarto del anillo. P1, fix de 15 min.
- **`sample_age_ms` esta roto en origen** (mide duracion de tick, no timestamp → pegado en 255). Hoy CENTRAL no lo lee, asi que no rompe nada, pero es una trampa cargada: el contrato OBLIGA a usarlo y quien lo implemente va a descartar todos los frames buenos. P1, fix ~1 h.
- **Cero deteccion de perdida de frames en produccion.** El SEQ viaja pero CENTRAL lo ignora; la deteccion de huecos solo vive en el diag de banco. Para un equipo en modo "aprendizaje Incheon" esto es exactamente la telemetria que falta. P1, fix ~2-3 h.
- **Drift mayor codigo↔doc:** los 2 contratos canonicos dicen "CENTRAL/DOWN usan LineStatus v1 (5B)" cuando el codigo ya migro 100% a LineStatusV2 (16B), con un `GAP-005` marcado P0 que ya no existe. Desinforma justo en el modulo que el coach quiere destrabar. P1, solo docs.
- **Reconciliacion diags↔produccion: consistente.** Mismo proto, mismo baud, mismo par de pines (Serial1 emisor / Serial2 receptor) en produccion y en los 3 diags. Lo unico stale son comentarios de cabecera.

---

## Implementado en esta sesión (2026-05-31, `agente/central`)

✅ **#4 schema gate** (`line_view.h`) · ✅ **#3 telemetría de SEQ / pérdida de frames** (`comm_down.cpp`) · ✅ **#8 observabilidad** (`data_valid`/`event_flags` + telemetría en el print de `main_central`). Verificados (compila `central_robot1` + diag; harness g++ del schema gate 7/7 PASS). Detalle: `journal/2026-05-31-quick-wins-link-down-central.md`.

**Pendiente:** #1 `[env:down]` con 32 sensores y #2 `sample_age_ms` → **scope agente DOWN** (tocan código/config de DOWN; #1 además tiene un comentario deliberado "1 mux para placa 04-12"). #5 watchdog de emergencia (diseño) y contratos v1→v2 (docs) → próximas sesiones. El **P0 de pines 7/8 (TASK-036)** sigue siendo el único bloqueante duro, y es de hardware.

---

## Como funciona hoy (con timing/frecuencias)

**Linea (DOWN, lectura fisica) — 1 kHz.** `line_ring_tick()` (`main_down.cpp:87-90`) lee 32 sensores ALS-PT19 via 4 muxes CD4051 cada 1 ms. El scrambling de Enzo (`MUX_CH_FOR_SENSOR`) deja `g_raw[]` en orden logico S0..S31.

**Linea (DOWN, decision + envio) — 200 Hz.** `comm_central_send_line_urgent()` (`comm_central.cpp:100-137`) corre cada 5 ms: toma SOLO el raw del ring, lo procesa con `dm_update()` → `LineStatusV2` (16 B), lo serializa con `down_encode_line` → frame de 23 B (7 overhead + 16 payload, TYPE=LINE_URGENT=0x10) y escribe Serial1 con backpressure. **Punto clave de latencia: la deteccion de `EV_IMMINENT_EXIT` se reevalua a 200 Hz (cada 5 ms), NO a 1 kHz** — el comentario "linea es lo mas urgente / 1 kHz" da falsa sensacion.

**Tracking OTOS (DOWN) — 100 Hz, va al TOP, NO a CENTRAL.** `otos_tick()` (cada 10 ms) lee 2 OTOS por buses I2C separados (U5=Wire, U6=Wire1, ambos 0x17), fusiona pose/heading/slip y la manda por **Serial5 → TOP** (`DOWN_OTOS_POSE`/`VEL`). La pose NO viaja por el link DOWN→CENTRAL (ese lleva solo `LineStatusV2`). Llega a CENTRAL recien dentro del `WorldSnapshot` que arma el TOP.

**Link DOWN→CENTRAL — recepcion.** `comm_down_tick()` (`main_central.cpp:89`, cada loop) drena Serial2 byte-a-byte → `FrameDecoder` (state machine con resync por CRC/END) → frame valido → `lsv2_from_frame` (valida `type==LINE_URGENT` y `payload_len==16`) → `world_model_apply_line` sella `g_line_last_ms=millis()`.

**Freno de emergencia (CENTRAL).** `main_central.cpp:95`, en CADA iteracion antes del gate de 100 Hz: si `world_model_imminent_exit()` (gateado `data_valid && EV_IMMINENT_EXIT && !lifted`) Y `world_model_line_is_fresh()` (`millis()-g_line_last_ms < 500`) → `motors_brake()`. Presupuesto deteccion→freno ~6-7 ms peor caso (5 ms cadencia + ~1 ms UART + <1 ms drain), cumple el objetivo de <15 ms.

**Link TOP→CENTRAL — 100 Hz.** TOP arma `WorldSnapshot` (27 B) y lo manda por **Serial5 → CENTRAL Serial1**, 34 B/frame, ~15% del bus. Watchdog `snapshot_is_fresh()` = 500 ms; si stale → `motors_stop()` + LED (SAFE_NO_TOP). Recuperacion automatica.

**Throughput global:** LINE_URGENT 23 B @ 200 Hz = ~4.6 kB/s (~20% del link). Snapshot 34 B @ 100 Hz = ~3.4 kB/s (~15%). Holgura comoda; el riesgo es recuperacion/stalls, no regimen.

---

## Hallazgos P0 (bloquean o rompen el link)

**Conflicto de pines 7/8: Serial2 RX (link de DOWN) vs motor del driver U17** — evidencia `ESTADO-ACTUAL.md:245-248` + `comm_down.cpp:29-31` + TASK-036 — **riesgo-no-fix:** si los pines 7/8 son del motor 2, `motors_init()` (`motors_zircon.cpp:101-105`) los pone OUTPUT y pisa el UART; TODO firmware CENTRAL llama `motors_init()` → ninguno puede recibir a DOWN con motores activos → el link de partido (no el diag) no funciona. **recomendacion:** correr `diag_central_motors` en banco (TASK-036) y decidir: si se confirma el conflicto, migrar Serial2 → Serial7 (pines 28/29) en `comm_down.cpp` y en el diag, en el mismo commit. Esto es lo PRIMERO a resolver antes de cualquier otra mejora del link. **confianza/veredicto:** la cita es confirmada; el conflicto fisico es **(a verificar en hardware)** — el repo lo tiene explicitamente como PENDIENTE de aislar.

> Nota de sintesis: este P0 NO surgio como "finding" de codigo (el codigo esta bien); surge de las preguntas abiertas de 4 dimensiones que convergen en TASK-036. Es el unico bloqueante duro del objetivo del coach. **TOP-CEN-01 fue propuesto como P0 pero el verificador lo bajo a P1** (ver abajo): un comentario stale no rompe el link en codigo.

---

## Hallazgos P1 (impacto alto)

**1. `sample_age_ms` se calcula sobre una DURACION, no un timestamp → pegado en 255** — evidencia `comm_central.cpp:118` usa `line_ring_get_last_tick_us()`, que en `line_ring.cpp:128` guarda `micros()-t_start` (duracion del tick ~200-400 µs), no el instante del muestreo; `(micros()-300)/1000 ≈ millis()` → clamp a 255 permanente pasados ~256 ms de uptime — **riesgo-no-fix:** el campo de frescura viaja como basura constante. Hoy CENTRAL no lo lee, pero el contrato (`CONTRATO-DATOS-DOWN.md:106,168`, §3.5 regla 5) OBLIGA a usarlo; quien implemente el gate de "enlace degradado" va a descartar TODOS los frames buenos. Trampa cargada para la proxima sesion. **recomendacion:** agregar `line_ring_get_last_sample_micros()` (guardar `t_start` en variable aparte) y usarlo en `comm_central.cpp:118`; dejar `get_last_tick_us()` solo para diagnostico. Test host con timestamps sinteticos. No toca el wire. ~1 h, risk-fix minimo. **confianza/veredicto:** confirmado (P1). *(Reportado por 3 dimensiones: COMM-BUF-01, LINE-DOWN-01, y referido por LINK-01.)*

**2. Cero deteccion de perdida de frames en produccion (SEQ se transmite pero CENTRAL lo ignora)** — evidencia `comm_down.cpp:19-25` (`handle_frame` solo hace `g_frames_received++`, nunca lee `f.seq`); la unica logica de huecos vive en `diag_central_comm_down.cpp:157-161`; DOWN incrementa `g_send_seq++` (`comm_central.cpp:123`) y del otro lado se descarta — **riesgo-no-fix:** una degradacion gradual (cable, conector, EMI) es invisible; CENTRAL no distingue "llegan todos" de "llega 1 de cada 5", y como `snapshot/line_is_fresh` siguen true con que llegue 1 frame cada <500 ms, el robot parece sano corriendo con datos entrecortados. Diagnostico a ciegas el dia del partido. **recomendacion:** en `comm_down.cpp` mantener `last_seq` y `frames_lost += (uint8_t)(f.seq - last_seq - 1)` ante discontinuidad (mide MAGNITUD del hueco, mejor que el diag que solo cuenta eventos); exponer getter y mostrarlo en el print de `main_central.cpp:120` junto con `crc_errors` y `frames_received` (que tampoco se imprimen hoy). Nombrarlo `frames_lost`, NO `frames_dropped` (ese ya existe TX-side). ~2-3 h, codigo aditivo de telemetria. **confianza/veredicto:** confirmado (P1). *(Reportado por 4 dimensiones: PROTO-02, COMM-BUF-04, TOP-CEN-03 para el link gemelo, TEST-P2-1.)*

**3. `[env:down]` de competencia compila con 1 mux (8 sensores) y 1 OTOS, no 32/2** — evidencia `config_down.h:29-31` (`DOWN_NUM_MUXES_CONNECTED` default 1 → `NUM_LINE_SENSORS=8`); `platformio.ini` `[env:down]` (61-77) NO pasa el flag; solo `down_debug` (94-95) y `diag_down` (145-146) lo hacen; `ESTADO-ACTUAL.md:252` llama `[env:down]` "competencia". Con n=8, `dm_update` cae al fallback de centroide angular uniforme (`down_model.cpp:73-78`), enciende `EV_DEGRADED_GEOMETRY` siempre y el MuxWatchdog vigila 1 mux — **riesgo-no-fix:** si se flashea `pio run -e down -t upload` para competir, el robot juega con 1/4 del anillo y geometria degradada; cobertura pesima lateral/trasera, justo donde el arquero la necesita. Y `data_valid` sigue en 1 → degradacion SILENCIOSA. **recomendacion:** agregar `-DDOWN_NUM_MUXES_CONNECTED=4` y `-DDOWN_NUM_OTOS_CONNECTED=2` al `[env:down]` (igual que ya tiene `down_debug`). ~15 min, HW ya validado con 4 muxes (journal 2026-05-24). **confianza/veredicto:** confirmado (P1). *(Reportado por LINE-DOWN-04 y, para OTOS, OTOS-06.)*

**4. Primer arranque sin EEPROM: el umbral por sensor se deriva de un BLANCO inventado (800)** — evidencia `main_down.cpp:63` solo llama `line_ring_calibrate_carpet()`, nunca `calibrate_white()` en boot; `g_white_avg[i]` queda en el default 800 (`line_ring.cpp:86`); el lazy-init `derive_calib_from_line_ring()` setea umbral=(carpet_real+800)/2 — **riesgo-no-fix:** en placa fresca o tras `ec_erase_calibration()`, el modo de falla DOMINANTE es umbral sesgado alto → sub-deteccion ("nunca ve blanco"). Escenario exacto de Incheon: iluminacion distinta a Salta + posible reflash. **Ademas `EV_CALIB_SUSPECT` NO cubre este caso** (white=800 vs carpet~200 → margen 600 >> 120 → no marca suspect, `data_valid=1`): el frame se acepta como valido. **recomendacion:** (a) disciplina de encendido: SIEMPRE calib carpet+white desde CENTRAL antes de jugar y verificar `EV_CALIB_SUSPECT` (~1 h, doc); (b) forzar `EV_CALIB_SUSPECT` mientras `white` siga en el default (~2 h, cierra el gap de raiz). **confianza/veredicto:** confirmado (P1). *(LINE-DOWN-03.)*

**5. Drift mayor doc↔codigo: los 2 contratos canonicos describen LineStatus v1 (5B), el codigo ya esta 100% en v2 (16B)** — evidencia codigo en v2 (`comm_down.cpp:21-23`, `world_model.cpp:10`, `down_encode.cpp:10-11`); docs en v1: `CONTRATO-DATOS-CENTRAL.md:244-253,666` (GAP-005 P0 sin tachar), `:814`, `CONTRATO-DATOS-DOWN.md:254-257,277`; los 4 docs son del 2026-05-18, la migracion (commit e2ca3af, 2026-05-29) no los toco — **riesgo-no-fix:** quien haga el bring-up leyendo los contratos canonicos (CLAUDE.md ordena tratarlos como verdad) va a buscar un bug v1↔v2 que ya no existe, o peor, "arreglar" el codigo hacia v1 y romper lo que funciona. **recomendacion:** actualizar AMBOS contratos en un commit: marcar GAP-005 RESUELTO, reescribir las secciones de CENTRAL (§2.2/§4.2/§8) y DOWN (§5/§7) a v2. `FUENTES-DE-VERDAD.md:36` ya lista v2, no hay que tocarlo. ~1.5-2 h, solo docs. **confianza/veredicto:** confirmado (P1). *(CONTRACT-02.)*

**6. Validacion de schema_version ausente en el receptor (fail-safe mas debil de lo que el comentario promete)** — evidencia `line_view.h:24-29`: `lsv2_from_frame` valida solo `type` y `payload_len==16`, NO compara `out.schema_version` contra `LSV2_SCHEMA=2`; el comentario `line_view.h:21-23` afirma que es el fail-safe ante cambio de schema, pero solo cubre TAMAÑO — **riesgo-no-fix:** un schema v3 de 16 B (reordenar campos, reusar `reserved`) pasa el filtro, pasa CRC y se reinterpreta como v2 → angulo/penetracion basura silenciosos. Plausible dado que CENTRAL y DOWN se flashean en sesiones/worktrees distintas; tipo de bug que sobrevive al hand-off 2027. **recomendacion:** 1 linea tras el memcpy: `if (out.schema_version != LSV2_SCHEMA) return false;` + 1 test host (frame de 16 B con schema=99 → rechazado). ~30-60 min, risk-fix nulo. **confianza/veredicto:** confirmado (P1). *(Reportado por 3 dimensiones: PROTO-03, LINK-02, CONTRACT-01.)*

**7. Watchdog de freshness (500 ms) es 33x el objetivo de latencia de freno (15 ms)** — evidencia `world_model.cpp:15` `LINE_TIMEOUT_MS=500`, el freno (`main_central.cpp:95`) exige `line_is_fresh()` que es true mientras `millis()-g_line_last_ms < 500`; objetivo `<15 ms` (`main_central.cpp:93`) — **riesgo-no-fix:** en corte momentaneo (vibracion al patear, conector flojo), si el ultimo frame decia "sin borde", CENTRAL maniobra a ciegas hasta 500 ms sobre un dato de hasta medio segundo (~50 cm a 1 m/s) → puede salirse de cancha. El bus de emergencia falla justo cuando debia proteger. **recomendacion:** separar dos umbrales — enlace-vivo puede quedar 200-300 ms; agregar umbral accionable-para-emergencia chico (20-30 ms = 4-6 periodos) que gatee `imminent_exit()`. Aprovechar `sample_age_ms` (despues de arreglar el bug #1). Test de banco: cortar cable y medir cuanto tarda en soltar el freno. **confianza/veredicto:** confirmado (P1). *(LINK-01.)*

**8. Con enlace vivo pero `data_valid=0` (mux muerto/saturacion/lifted), el freno queda DESACTIVADO sin aviso** — evidencia `imminent_exit()` exige `data_valid!=0` (`line_view.h:38-42`); `down_model.cpp:103` baja `data_valid` si `lifted||suspect||any_mux_dead||saturated`, pero DOWN sigue mandando frames frescos → `line_is_fresh()=true` pero `imminent_exit()=false` → el freno nunca dispara y strategy corre normal; el debug print (`main_central.cpp:120-136`) no muestra `event_flags` ni `data_valid` — **riesgo-no-fix:** si un mux muere a mitad de partido (punto fragil: la placa DOWN llego con nets sin rutear, TASK-002), el robot pierde la proteccion de borde en silencio. El operador no entiende por que: "el enlace anda pero la proteccion esta muerta". **recomendacion:** es decision de estrategia, no solo codigo. Minimo: exponer `world_model_line_data_invalid()` y loguear `g_line.event_flags` en el print para ver `EV_MUX_DEAD`/`EV_CALIB_SUSPECT` en vivo. Solo leer el dato que ya viaja. Test de banco: desconectar un mux y confirmar que `data_valid` baja y el evento aparece. **confianza/veredicto:** confirmado (P1). *(LINK-03.)*

**9. Lecturas I2C de OTOS ignoran el codigo de error → pose falsa viaja como "fresca" a TOP** — evidencia `otos.cpp:89-90` descarta el retorno `sfTkError_t` de `getPosition`/`getVelocity`; en error la lib deja la pose SIN modificar y `otos_tick` re-inicializa los structs a `{}` cada tick → la pose COLAPSA a (0,0,0) y se propaga con `confidence=100` — **riesgo-no-fix:** un glitch I2C intermitente (vibracion, cable Qwiic flojo) genera un salto al origen de cancha indistinguible de "robot realmente en el centro", sin contador ni sintoma. No hay metrica de errores I2C (los UART si la tienen). **recomendacion:** capturar el retorno (`if (...getPosition(pl) != ksfTkErrOk) { io_err++; }`), exponer contador por OTOS, y bajar `confidence`/`ready` tras N errores para que la fusion deje de promediar el lado caido. ~2-3 h. **confianza/veredicto:** confirmado (P1). *No bloquea DOWN→CENTRAL* (es la ruta OTOS→TOP). *(OTOS-01.)*

**10. OTOS: flags `ready` se fijan UNA vez en init → sin recuperacion del brownout YA observado** — evidencia `otos.cpp:54-77` setea `g_*_ready` solo en `otos_init()`; `otos_tick()` nunca re-evalua ni reintenta `begin()`; brownout reproducible en ESTA placa (journals 2026-05-24 y 2026-05-29: OTOS aparece en 0x64 o desaparece cuando la bateria no entrega corriente; el 3.3V del MP1584 nunca se midio, P0.3 del audit) — **riesgo-no-fix:** bateria que decae a mitad de partido → OTOS brownout → DOWN sigue jurando pose buena (confidence 60/100) → TOP/CENTRAL navegan con odometria muerta, sin aviso, sin recuperacion salvo power-cycle manual (imposible en partido). **recomendacion:** (1) `ready` dinamico: tras N lecturas con error, marcar NOT ready y bajar confidence; (2) reintento no-bloqueante de `begin()` con backoff (sin recalibrar IMU, que bloquea ~0.5s); (3) medir el 3.3V del MP1584 (HW del equipo). Para primera instancia, el minimo es forzar `confidence=0` ante error acumulado. ~4-6 h firmware + medicion HW. **confianza/veredicto:** confirmado (P1). *Ruta OTOS→TOP, no bloquea DOWN→CENTRAL.* *(OTOS-02.)*

**11. OTOS: `getStatus()` (warnOpticalTracking/tilt/error fatal) nunca se lee → pose mala viaja como valida en superficie de baja textura** — evidencia `otos.cpp` no llama `getStatus()` en ningun lado (grep: cero matches); journal 2026-05-24:101-109 documenta el sintoma (sobre hoja A4 la pose salta y se congela, 28.6 mm para 300 mm reales = `warnOpticalTracking`); `confidence` se calcula solo con `ready` flags — **riesgo-no-fix:** la cancha verde puede tener zonas de baja textura; el chip sabe que el tracking es malo y el firmware lo ignora; diagnostico = cero. **recomendacion:** leer `getStatus()` a ~10 Hz, mapear `warnOpticalTracking`/tilt a `confidence` (bajar a <=30) y loguearlo en `down_debug`. ~2-3 h. **confianza/veredicto:** confirmado (P1). *La inferencia "es warnOpticalTracking" es solida pero **(a verificar en hardware)** — nadie observo el bit porque el codigo nunca lo lee. Ruta OTOS→TOP.* *(OTOS-03.)*

**12. Falta cobertura de test sobre el path de produccion del glue (comm_*_tick, backpressure, handle_frame, watchdog)** — evidencia `platformio.ini:180` `[env:test_native]` usa `build_src_filter = +<shared/>` → `comm_central.cpp` (src/down/) y `comm_down.cpp` (src/central/) NO se compilan en NINGUN test; `world_model.cpp` tampoco. El chain testeado (`down_encode→FrameDecoder→line_view`) es un PROXY confeso (comentario en `test_central_line_ingest:18`) — **riesgo-no-fix:** la logica que de verdad corre en el robot (cuando se dropea un frame, que hace `handle_frame` con un calib de payload_len 0/1/2, el watchdog `is_fresh` con `g_line_last_ms=0` o wrap de millis) nunca se ejecuta fuera del hardware; una regresion pasa todos los tests verdes. **recomendacion:** extraer la logica pura a funciones testeables (`bool should_send(int avail, size_t nb)`, `bool is_fresh(uint32_t last, uint32_t now, uint32_t timeout)`) y testear casos de borde; o linkear los `.cpp` con un shim de Serial fake. Minimo viable: drop-counter + handler de calib + freshness wrap-safe (el patron ya existe en `test_sensor_health:41`). ~0.5-1 dia. **confianza/veredicto:** confirmado, **el verificador bajo P0→P1** (ausencia de test no rompe el link; es gap de cobertura + riesgo de regresion sobre un path P0). *(TEST-P0-1, TEST-P1-1.)*

**13. Doc de hardware del TOP desincronizada con el fix Serial5 (header dice Serial2, el .cpp usa Serial5)** — evidencia `top/comm_central.cpp:34,59` ya en Serial5; `top/comm_central.h:3` dice "Serial2 (pines 7/8)"; `top/main_top.cpp:14,119` idem — **riesgo-no-fix:** quien suelde el cable lee el header/main, conecta a pines 7/8 (Serial2, no inicializado) → CENTRAL nunca recibe snapshot → `snapshot_is_fresh()=false` permanente → robot en SAFE_NO_TOP, y se debuggea el firmware cuando el bug es un comentario mentiroso. **recomendacion:** sincronizar `comm_central.h:3` y `main_top.cpp:14,119` a Serial5 en un commit; verificar continuidad con multimetro ANTES de soldar (el comentario del fix razono los pines "leyendo el diagrama", no por medicion). ~15 min doc + 20 min continuidad. **confianza/veredicto:** confirmado, **el verificador bajo P0→P1** (el link TOP→CENTRAL funciona en codigo; el dano es riesgo de cableado, y es TOP→CENTRAL, no el foco DOWN→CENTRAL). *(TOP-CEN-01.)*

**14. TX del snapshot TOP→CENTRAL sin backpressure (Serial5.write a ciegas)** — evidencia `top/comm_central.cpp:58-61` hace `Serial5.write(buf,n)` sin chequear `availableForWrite()` ni contador de drop; DOWN ya resolvio esto (audit P1.6: `down/comm_central.cpp:131-136`, `down/comm_top.cpp:55-60`) — **riesgo-no-fix:** en operacion normal el FIFO no se llena (~15% del bus), pero si CENTRAL deja de drenar Serial1 o el cable se desconecta, el `write()` pasa a busy-wait y roba ms al loop de TOP (IMU/TOF/localizacion), degradando su 100 Hz. Sin contador, en banco no se ve. **recomendacion:** copiar el patron de DOWN (`if availableForWrite()>=n ... else dropped++`) + getter `comm_central_get_frames_dropped()`. ~30 min. **confianza/veredicto:** confirmado (P1). *Link TOP→CENTRAL.* *(TOP-CEN-02.)*

---

## Hallazgos P2 (mejoras)

**Resync consume el byte de error: una frontera corrupta puede perder DOS frames** — `proto.cpp:54-59,94-99`: ante `END!=0x55` o `LEN>32`, el byte se consume y vuelve a WAIT_START sin re-procesar; si ese byte era el `0xAA` START del frame siguiente, se traga y se pierde el frame N+1 tambien — **riesgo-no-fix:** bajo ruido (motores, EMI) un byte corrompido degrada 2 frames, no 1; la tasa efectiva cae mas de lo que sugiere `crc_errors_`. **recomendacion:** en el rechazo de READ_END/READ_LEN, si el byte == `PROTO_START`, transicionar a READ_LEN en vez de descartarlo (~4 lineas + 1 test "END corrupto + frame valido pegado"). El CRC sigue siendo la red final, no bloquea. — confirmado (P1→**se mantiene P1 segun el verificador**, pero por priorizacion del coach lo ubico aca como mejora de robustez no bloqueante para la primera instancia). *(PROTO-01.)*

**FrameDecoder sin timeout inter-byte ni reset al reconectar** — `proto.cpp:45-118` avanza solo con bytes; `comm_down.cpp` nunca llama `g_decoder.reset()` salvo construccion — **riesgo-no-fix:** tras un corte a mitad de frame, los primeros bytes nuevos se consumen como cola del viejo; cuesta 1-2 frames extra resincronizar. **recomendacion:** en `comm_down_tick`, si pasaron >timeout sin byte, `g_decoder.reset()` antes del proximo `feed()`. **confianza/veredicto:** **REFUTADO como riesgo de seguridad** (PROTO-05): el watchdog de 500 ms (`world_model_line_is_fresh`) ya cubre completamente la consecuencia (CENTRAL no actua sobre dato stale); el residual es solo ~1-2 frames de resync, benigno. Se incluye como mejora cosmetica opcional, no accionable como bug.

**Frame stale + decoder Frankenstein al reconectar (g_line retiene el ultimo valor)** — `comm_down.cpp` no resetea el decoder ni limpia `g_line` en un corte — **riesgo-no-fix:** ventana donde un frame a medio decodificar podria validar CRC por casualidad (~2⁻¹⁶) y aplicarse como fresco. **recomendacion:** limpiar `g_line` a `data_valid=0` al expirar el timeout (defensa en profundidad) + `reset()` del decoder. **confianza/veredicto:** **INCIERTO, bajado a P2** (COMM-BUF-03): la retencion de `g_line` es real pero su consecuencia operativa (frenazo espurio) esta neutralizada — todos los consumidores de produccion estan gateados por `line_is_fresh()`; el frame Frankenstein requiere una colision de CRC improbable. Fix barato igual.

**Constantes de deteccion DIVERGEN entre las 2 cadenas de DOWN** — `IMMINENT_EXIT_DEPTH=3` (line_ring) vs `imminent_depth=6` (dm_update); lifted delta 50 vs 80 — **riesgo-no-fix:** el equipo tunea mirando el diag (line_ring, depth>=3) pero el robot actua con dm_update (depth>=6); persiguen un comportamiento que el firmware de competencia no tiene. **recomendacion:** resolver junto con la deuda de 2 cadenas (abajo); si line_ring queda "solo raw", estas constantes desaparecen de esa cadena. **confianza/veredicto:** confirmado (P2). *(LINE-DOWN-05.)*

**Deuda de 2 cadenas: el pipeline de line_ring (1 kHz) es computo MUERTO en produccion** — `line_ring_get_angle_deg/depth/imminent_exit` solo los consume el diag; el dato real lo arma `dm_update` a 200 Hz con OTRO estado, OTRA geometria (line_ring usa centroide uniforme, dm_update usa `lg_compute_xy` con geometria real del PCB) — **riesgo-no-fix:** confusion grave en bring-up (el equipo debuggea la cadena equivocada); ~75% del presupuesto de CPU de linea quemado en calculo descartado. **recomendacion:** degradar line_ring a SOLO muestreo+raw, dm_update como unica cadena, actualizar el diag a `LineStatusV2`. Los tests `test_line_filters` siguen validos. ~3-4 h. **confianza/veredicto:** confirmado (P1 por el verificador; lo ubico en P2/mejora porque NO bloquea la primera instancia del link — pero es la deuda arquitectonica #1 a saldar post-bring-up). *(LINE-DOWN-02.)*

**Saturacion todo-blanco puede disparar un `EV_LINE_END` falso** — `down_model.cpp:50-52,91,118`: un tick saturado tras linea sostenida pasa `present=0` al LineTracker, que lo lee como fin-de-linea — **riesgo-no-fix:** hoy INERTE (ningun consumidor de estrategia reacciona a `EV_LINE_END`; `data_valid` ya va a 0). **recomendacion:** guard `if(!saturated)` alrededor de `lt_update` (congelar el tracker, no resetear). ~1 h + test host. **confianza/veredicto:** confirmado (P2). *(LINE-DOWN-06.)*

**TX sin backpressure en CENTRAL→DOWN (comandos esporadicos)** — `central/comm_down.cpp:53,64` `Serial2.write` directo — riesgo bajo (reset/calib on-demand). **recomendacion:** mismo patron de DOWN por consistencia. confirmado (P2). *(COMM-BUF-05.)*

**Buffers RX/TX en el default ~64 B (sin `addMemoryForRead`)** — grep sin matches en todo el repo; ~2.7 frames de tolerancia a stall del loop de CENTRAL — **riesgo-no-fix:** en partido normal drena de sobra, pero un bloqueo accidental (I2C del BNO055, print pesado) >umbral genera overflow silencioso. **recomendacion:** `Serial2.addMemoryForRead(buf, 256)` en `comm_*_init`; 5-10x mas tolerancia, RAM sobra en Teensy 4.x. ~1 h, defensa en profundidad. confirmado (P2). *(COMM-BUF-06.)*

**Observabilidad: frames CRC-OK de TYPE desconocido se cuentan como recibidos y caen al default silencioso** — `comm_down.cpp:19-25` cuenta `frames_received` siempre, no separa los aplicados; DOWN `comm_central.cpp:74` tiene default sin contador — **riesgo-no-fix:** si por error se cablea el UART del TOP al RX de CENTRAL-DOWN, llegan frames CRC-OK de otro tipo, `frames_received` sube pero la linea nunca se aplica → dificil de debuggear. Justo el escenario de los pines 7/8. **recomendacion:** separar `frames_received` (todo CRC-OK) de `frames_applied`; contar descartes por tipo (el patron ya existe en `diag_central_comm_down.cpp:75` `g_other_frames`). ~1 h. confirmado (P2). *(PROTO-06.)*

**Heading dual-OTOS y `slip_estimate` mal definidos (geometria + unidades sin cerrar)** — `otos.cpp:105-106,118`: heading usa solo `dy`, slip es `|right_x-left_x|` (diferencia de POSICION acumulada, no velocidad menos rotacion como promete `otos.h:44-46`); satura el `uint8` enseguida — **riesgo-no-fix:** latente (solo corre con 2 OTOS activos); cuando se active, heading/slip ininterpretables. NOTA: el argumento del hallazgo de que "mide el eje equivocado" esta REFUTADO geometricamente (con separacion en X, la rotacion SI se refleja en `dy`); el problema real es la contradiccion interna del doc + unidades de slip. **recomendacion:** cerrar montaje (TASK-004), promediar `left_h/right_h`, redefinir slip como diff de velocidad menos omega*brazo. confirmado parcialmente (P2). *(OTOS-04.)*

**`calibrateImu()` bloqueante en setup (~0.5s x2) sobre riel posiblemente marginal** — `otos.cpp:65,74` con defaults (255 samples, `waitUntilDone=true`), retorno ignorado — **riesgo-no-fix:** arranque lento (~1.2-1.5s) y, en riel marginal, calibracion IMU silenciosamente mala → deriva de heading. No bloquea DOWN→CENTRAL. **recomendacion:** bajar numSamples o `waitUntilDone=false` + poll de `getImuCalibrationProgress` con timeout. confirmado (P2). *(OTOS-05.)*

**Default 1 OTOS + separacion 200mm sin medir + comentario de bus stale** — `config_down.h:75-76` dice "U5→Wire1/U6→Wire2" (Wire2 no existe en esta placa); el codigo correcto es U5→Wire, U6→Wire1 — **riesgo-no-fix:** flashear competencia con 1 OTOS pierde el diferencial; el comentario stable puede llevar a cablear el bus equivocado. **recomendacion:** corregir `config_down.h:75-76` Y `otos.h:53`, medir separacion (TASK-004), activar `=2` cuando ambos validados. confirmado (P2). *(OTOS-06.)*

**Semantica de `penetration_mm`/`cross_track_mm`: doc promete mm reales + PID lateral, codigo manda conteo + N/A permanente** — `down_model.cpp:109-110`: `penetration_mm = sensors_on_line` (PROXY), `cross_track_mm = LSV2_NA_I16` (siempre N/A); el contrato §3.1/§3.4 describe un PID lateral del arquero sobre `cross_track_mm` con setpoint=0. El codigo real corre el PID sobre `penetration` (conteo) con setpoint en conteo (`strategy.cpp:357,386`, `pids.h:69`) — divergencia doble — **riesgo-no-fix:** quien calibre el arquero contra el doc sintoniza contra la señal y el setpoint equivocados; el "PID lateral fino" no tiene su entrada. **recomendacion:** alinear §3.4 entero (PID corre sobre penetration-proxy/conteo, cross_track N/A hasta Plan 3). ~30-45 min doc. confirmado (P2). *(CONTRACT-04.)*

**`EV_CALIB_SUSPECT` sobrecargado para saturacion + tabla §3.2 sin `EV_SENSOR_NOISY`** — `down_model.cpp:120` (`suspect||saturated` → mismo bit); `types.h:156` gasta bit 7 en `EV_SENSOR_NOISY` que el contrato marca "reservado" — **riesgo-no-fix:** baja granularidad de diagnostico en banco; tabla del contrato desactualizada. **recomendacion:** documentar el overload + agregar `EV_SENSOR_NOISY` a §3.2. ~30 min doc. confirmado (P2). *(CONTRACT-05.)*

**Faltan `static_assert` de tamaño en Pose2D/Velocity2D** — `types.h` solo asserta WorldSnapshot==27 y LineStatusV2==16; Pose2D/Velocity2D (7 B documentados) no — **riesgo-no-fix:** acotado a DOWN→TOP (OTOS); un campo agregado sin querer cambia el LEN de cable y el peer viejo descarta por size, sin error. NOTA: MotorCommand no viaja por ningun enlace ni esta documentado a tamaño de cable → su assert seria higiene interna, no enforcement. **recomendacion:** `static_assert(sizeof(Pose2D)==7)` y `sizeof(Velocity2D)==7`. ~15 min. confirmado (P2). *(CONTRACT-06.)*

**Asimetria endianness payload (LE) vs CRC (BE)** — real y verificado byte-a-byte, pero la recomendacion principal ("documentar en CONTRATO-DATOS-DOWN.md") esta **REFUTADA**: el doc canonico YA lo documenta de forma prominente (`CONTRATO-DATOS-DOWN.md:63-66` callout "Endianness del payload (CRITICO)"). Residual minimo: `proto.h` no lo menciona en sus comentarios. **veredicto:** incierto/sobreestimado (PROTO-04) — solo queda un cross-reference trivial en `proto.h`, no la "deuda fragil indocumentada" que pinta el hallazgo.

**Drenado de RX sin tope de bytes / TOP todavia decodifica LINE_URGENT como v1 / HC-SR04 pin 7 doc stale / latencia de emergencia es 200 Hz no 1 kHz / SEQ wrap sin test** — un grupo de P2 menores confirmados, todos de bajo riesgo y no bloqueantes: TOP-CEN-05 (agregar tope al `while` de drain por consistencia con camaras; recalcular el cap, los 64 B propuestos sub-drenan a 230400), CONTRACT-03 (codigo muerto: TOP espera 5 B en LINE_URGENT, DOWN ya no se la manda), TOP-CEN-04 (comentario stale en `sensors_tof.cpp` post-fix Serial5), LINK-04 (documentar que la latencia la domina `LINE_URGENT_INTERVAL_MS=5`, no el tick de 1 kHz), TEST-P2-1 (test de wrap de SEQ + contador en produccion).

**Drop del frame con `EV_IMMINENT_EXIT` por backpressure** — `comm_central.cpp:131-136` dropea cualquier frame si el TX esta lleno, sin priorizar el de emergencia — **riesgo-no-fix:** baja probabilidad hoy (a 230400 el TX de Serial1 se vacia cada ciclo), pero el frame de emergencia merece mas garantia. NOTA: el texto del hallazgo implica que Serial5 (TOP) llena el buffer de Serial1 — son UARTs independientes, solo comparten CPU. **recomendacion:** no dropear el frame con imminent (write bloqueante solo en ese caso o reintentar) + contador separado. confirmado (P2). *(LINK-05.)*

---

## Buffers, backpressure y recuperacion ante cortes (analisis dedicado)

**Backpressure — asimetrico pero el sentido critico esta cubierto.** DOWN protege sus dos TX con `availableForWrite()` (audit P1.6: `down/comm_central.cpp:131`, `down/comm_top.cpp:55`) y dropea con contador en vez de busy-wait — decision CORRECTA para un sensor de tasa alta donde solo importa el dato mas reciente. El sentido contrario NO esta protegido (CENTRAL→DOWN comandos, TOP→CENTRAL snapshot), pero el link DOWN→CENTRAL —el foco del coach— ya tiene su TX blindado. Para la primera instancia esto NO bloquea.

**Buffers UART — todos en el default ~64 B.** Confirmado por grep: ningun `addMemoryForRead/Write` en el repo. Tolerancia ~2.7 frames (~2.8 ms de bytes continuos a 230400, no los 14 ms optimistas si DOWN hace burst). Suficiente en partido normal (loop >>1 kHz); el riesgo es un bloqueo accidental del loop. Defensa en profundidad barata: subir el RX de Serial2 a 256 B.

**Recuperacion ante cortes — el watchdog de 500 ms es la red real y FUNCIONA.** Verificado adversarialmente: tras un corte, a los 500 ms `line_is_fresh()=false` y TODOS los consumidores de produccion dejan de leer `g_line` (`main_central.cpp:95`, `strategy.cpp:141,326,356,385`). El "frenazo espurio por dato retenido" NO puede ocurrir — esta neutralizado. Lo que NO esta cubierto:
- **El decoder no se resetea por inactividad** (cuesta 1-2 frames de resync tras reconexion — benigno, COMM-BUF-03 incierto/P2).
- **`g_line` no se limpia a `data_valid=0` al expirar** (cosmetico, ya cubierto por el gate — defensa en profundidad).
- **El watchdog de 500 ms es demasiado grueso para emergencias** (LINK-01, P1 real): vivo ≠ accionable-para-frenar.
- **No hay deteccion de perdida parcial** (frames_lost, P1): el enlace puede degradar gradualmente sin alarma.

**Recuperacion OTOS — el gap mas serio del lado tracking.** `ready` se congela en init; un brownout post-setup deja la pose falsa a confidence alta para siempre. No bloquea DOWN→CENTRAL pero es el riesgo #1 de la odometria (OTOS-01/02/03).

---

## Timing y cuellos de botella

- **No hay cuello de botella de ancho de banda.** DOWN→CENTRAL ~20% del link, TOP→CENTRAL ~15%. Holgura comoda.
- **Latencia de emergencia: la domina la cadencia de envio (5 ms / 200 Hz), NO el muestreo de 1 kHz** (LINK-04, confirmado). Presupuesto ~6-7 ms peor caso, cumple <15 ms con margen. Pero NO hay medicion end-to-end real (GPIO+osciloscopio) — el "<1 ms drain en CENTRAL" asume loop rapido sin medir el periodo real del loop de CENTRAL.
- **`calibrateImu()` bloquea ~1.2-1.5s en el setup de DOWN** (OTOS-05). No afecta el link (otra ruta/tick) pero ralentiza el arranque.
- **El loop de CENTRAL no tiene bloqueos en el path caliente** (delay solo en setup, strategy sin while/delay) — bueno. El riesgo de overflow del RX de 64 B solo aparece si una operacion accidental (I2C colgado, print pesado) traba el loop >~3 ms.
- **Pregunta abierta de timing:** ¿cual es el peor caso REAL de duracion del loop de CENTRAL en partido? Si algun tick puede bloquear >umbral, COMM-BUF-06 sube de P2 a P1. Medir `loop_count/s` y delta maximo entre iteraciones en banco.

---

## Tests: que cubre, que falta, reconciliacion diags vs produccion

**Que cubre (solido):**
- `test_proto` (13 tests): vector CRC estandar 0x29B1, loopback, basura previa, CRC/END corruptos, back-to-back x10, recuperacion tras corrupcion, LEN oversize. La suite mas robusta del link.
- `test_central_line_ingest` (8): chain `encode→FrameDecoder→lsv2_from_frame→helpers` + guard anti-regresion del P0 v1↔v2 (payload de 5 B rechazado).
- `test_down_encode` (3): golden vector byte-a-byte del Ejemplo B con CRC 0xDF 0xBF.
- `test_line_filters` (39), `test_down_model` (7): pipeline de linea, MuxWatchdog, lifted, saturacion, wrap de millis().
- Total: 262 tests / 20 envs / 0 fallos (al 2026-05-29).

**Que falta:**
- **El path de produccion del glue (comm_*_tick, backpressure, handle_frame) NO se compila en ningun test** (TEST-P0-1→P1). El binario de test solo incluye `shared/`.
- **El watchdog de freshness (`line_is_fresh`, 500 ms) y el corte+recuperacion no tienen test directo** (TEST-P1-1→P1). El unico test relacionado usa el FLAG `line_fresh`, no la logica temporal real.
- **FrameDecoder sin fuzz adversario con `0xAA` embebido ni cortes a mitad de frame** (TEST-P1-2 → **bajado a P2**: gap de cobertura real, pero el CRC ya protege la integridad; el valor del test es diagnostico, no arreglar un defecto vivo).
- **SEQ wrap (255→0) y deteccion de gap sin test, y produccion ni lo mira** (TEST-P2-1, duplica el P1 de frames_lost).

**Reconciliacion diags vs produccion — CONSISTENTE.** Verificado: produccion (`comm_down.cpp` CENTRAL) y diag (`diag_central_comm_down.cpp`) ambos en Serial2/pin7 a 230400; DOWN transmite por Serial1/pin1 en produccion y en `diag_down_send1`. Mismo baud, mismo protocolo, mismo `FrameDecoder`, mismo `line_view`. La escalera de diags esta bien pensada (cable crudo `send1/recv1` → protocolo+panel `diag_central_comm_down`). El commit 9cd18f7 unifico el discurso a Serial2/pin7. Lo unico stale son comentarios de cabecera (no el cableado). El `diag_central_comm_down` ya implementa STALE detection y `seq_gaps` que sirven de test manual hoy.

---

## Inconsistencias codigo<->documentacion

| # | Doc dice | Codigo hace | Severidad | Veredicto |
|---|---|---|---|---|
| 1 | Contratos: "CENTRAL/DOWN usan LineStatus v1 (5B)", GAP-005 P0 abierto | 100% LineStatusV2 (16B), GAP-005 ya resuelto | **P1** | confirmado (CONTRACT-02) |
| 2 | `line_view.h:21-23`: "fail-safe ante cambio de schema" | Solo valida tamaño, no schema_version | **P1** | confirmado (CONTRACT-01) |
| 3 | `comm_central.h`/`main_top.cpp` (TOP): "Serial2 pines 7/8" | Codigo usa Serial5 | **P1** | confirmado (TOP-CEN-01) |
| 4 | `CONTRATO §3.1/§3.4`: penetration en mm reales + PID sobre cross_track | Conteo (proxy) + cross_track N/A; PID sobre conteo | P2 | confirmado (CONTRACT-04) |
| 5 | `CONTRATO §3.2`: EV_CALIB_SUSPECT = solo calib; bit 7 reservado | Tambien señaliza saturacion; bit 7 = EV_SENSOR_NOISY | P2 | confirmado (CONTRACT-05) |
| 6 | `config_down.h:75-76` + `otos.h:53`: "U5→Wire1 / U6→Wire2" | U5→Wire, U6→Wire1 (Wire2 no existe en la placa) | P2 | confirmado (OTOS-06) |
| 7 | `proto.h:12,18`: "SEQ detecta perdida" | Produccion nunca lee f.seq | **P1** | confirmado (PROTO-02) |
| 8 | `sensors_tof.cpp:57-66` (TOP): "pin 7 = Serial2 RX a CENTRAL" | UART migro a Serial5; pin 7 libre | P2 | confirmado (TOP-CEN-04) |
| — | "asimetria endianness indocumentada" | **El doc YA lo documenta** (CONTRATO §3.1/63-66) | — | **REFUTADO** (PROTO-04) |
| — | `central/comm_down.h:3`: "Serial2 UART secundario rapido" (sin mencionar conflicto motor) | Correcto, pero omite TASK-036/plan-B | P2 | confirmado (LINK-06) |

---

## Checklist "primera instancia": pasos minimos para DOWN→CENTRAL andando y confiable

Ordenado, accionable. Los pasos de hardware NO los puede cerrar Claude (regla no-negociable #1 del CLAUDE.md).

**FASE 0 — Desbloquear hardware (lo unico P0):**
1. **[HW, TASK-036] Aislar el conflicto de pines 7/8.** Correr `diag_central_motors` en banco con los 3 motores activos. ¿El pin 7 (Serial2 RX) lo usa el driver U17? — Si SI: migrar `comm_down.cpp` y `diag_central_comm_down` de Serial2 a Serial7 (pines 28/29) en un commit. Si NO: confirmar Serial2/pin7 y documentarlo. **Sin esto, el link de partido no funciona.**
2. **[HW] Verificar continuidad del cable** DOWN-pin1 (TX1) → CENTRAL-pin7 (RX2, o 28 si se migro), GND comun. Con multimetro, antes de confiar en cualquier resultado de software.

**FASE 1 — Levantar el link aislado:**
3. **[HW] Correr `diag_down_send1` ↔ `diag_central_recv1`** (cable crudo, sin protocolo). Confirmar que llega el byte.
4. **[HW] Correr `diag_central_comm_down`** (protocolo completo + panel). Criterio: a 200 Hz durante 60 s, `frames_received ≈ 12000`, `crc_errors = 0`, `seq_gaps = 0`, sin `[STALE!]`. Registrar como baseline de salud del enlace.
5. **[Firmware, 15 min] Corregir `[env:down]`:** agregar `-DDOWN_NUM_MUXES_CONNECTED=4` y `-DDOWN_NUM_OTOS_CONNECTED=2` para que competencia compile con los 32 sensores validados. Reconfirmar en banco que la linea se detecta (igual que `down_debug`).

**FASE 2 — Calibracion y dato confiable:**
6. **[HW + disciplina] Protocolo de encendido:** SIEMPRE calib carpet+white desde CENTRAL antes de jugar; verificar que NO sube `EV_CALIB_SUSPECT`. Medir carpet y blanco reales, comparar contra el default 800.
7. **[Firmware, ~1h] Arreglar `sample_age_ms`** (getter de timestamp separado de la duracion del tick). Test host. No toca el wire.
8. **[Firmware, ~30min] Agregar el gate de schema_version** en `lsv2_from_frame` (1 linea + 1 test).

**FASE 3 — Observabilidad (telemetria de "aprendizaje Incheon"):**
9. **[Firmware, ~2-3h] Llevar la deteccion de SEQ a produccion:** `frames_lost` en `comm_down.cpp` + getter; imprimir `frames_received`/`crc_errors`/`frames_lost`/`data_valid`/`event_flags` en `main_central.cpp:120`.
10. **[HW] Test "tiron de cable":** desconectar Serial2 10 s, reconectar; verificar que NO hay frenazo espurio, que `line_fresh` recupera limpio, y medir cuanto tarda en soltar el freno.

**FASE 4 — Docs (en los mismos commits, regla de sesion):**
11. Actualizar los 2 contratos a LineStatusV2 (marcar GAP-005 RESUELTO).
12. Sincronizar comentarios de `comm_down.h` (CENTRAL, agregar nota TASK-036/plan-B) y la doc del TOP (Serial5).

---

## Preguntas abiertas / pruebas de hardware pendientes

1. **[P0] ¿Los pines 7/8 son del motor del driver U17?** (TASK-036, sin aislar). Decide si TODO el link de produccion hay que migrarlo a Serial7. Riesgo #1 para poner el link a andar.
2. **¿La migracion a LineStatusV2 se probo con DOWN+CENTRAL fisicos enlazados** (no solo host-native)? El contrato marca el mapeo fisico de UART como pendiente de osciloscopio (TASK-008/014). Sin esa medicion no se cierra el link como `done`.
3. **¿Latencia end-to-end real deteccion→freno?** Medir con GPIO+osciloscopio (no estimar). Confirma el presupuesto ~6-7 ms.
4. **¿Peor caso de duracion del loop de CENTRAL en partido?** Si algun tick bloquea >~3 ms, el RX de 64 B desborda → COMM-BUF-06 sube a P1.
5. **¿Que hace strategy con `line_is_fresh() && !data_valid`** (mux muerto/saturacion de luz de Incheon)? Decision de coach, no solo firmware (LINK-03).
6. **¿Se acepta levantar DOWN→CENTRAL con la cadena Serial5→TOP desactivada** (menos variables) para la primera instancia? Define si hace falta un flag `-DDOWN_LINK_CENTRAL_ONLY`.
7. **[OTOS] ¿Montaje real (eje, separacion)?** (TASK-004). Y ¿se midio el 3.3V del MP1584? (P0.3 del audit, nunca medido — causa raiz del brownout).
8. **¿Se corrio `pio test -e test_native` recientemente** y pasan las 262? El repo estuvo parado 7 semanas; confirmar verde antes de agregar tests nuevos.

> **Honestidad sobre lo no verificado en hardware:** todo lo marcado "(a verificar en hardware)" — el conflicto de pines 7/8, el brownout de OTOS como `warnOpticalTracking`, la latencia real, el peor caso del loop — son afirmaciones de codigo/journal que Claude NO puede cerrar. El codigo del link esta solido; el bring-up depende del equipo con la placa.