---
title: "Estado actual del robot — vivo, 1 página"
date: 2026-05-29
last-updated-by: "Claude (sesión 2026-06-11 post-demo — práctica alumnos 2026-06-12: delantero R1 sin gyro por OTOS + arquero integral R2 con caja negra v1.2)"
status: vivo
tipo: indice-operacional
---

# Estado actual — Snapshot del repo (al 2026-05-29)

> **Toda sesión Claude que toca este repo: ESTA es la primera lectura
> obligatoria** (después de `git pull`). Si lo que estás por hacer contradice
> algo de acá, **parar y consultar al humano**. Si lo que vas a hacer hace
> cambiar algo de acá, **actualizá esta página en el mismo commit.**

> **🩺 MONITOR TOP "SALUD" + ZONAS DE ToF (sesión autónoma 2026-06-14 — vale sobre lo de abajo):**
> Nueva vista `python -m monitor_base --top-salud` = **tablero de salud por sensor**
> (semáforo OK/REVISAR/FALLA/SIN DATO) + **grilla de zonas de cada ToF** + per-cámara
> (pelota fantasma) + OTOS/escape + botones de config, **sobre `tools/monitor-base/`
> existente** (no desde cero; reusa transporte/parser/sim/golden). **+ el firmware
> ahora EXPONE las zonas crudas 4×4 de cada ToF** en la telemetría (campo `z` ADITIVO,
> schema sigue **2**; antes promediaba las 16 zonas a 1 distancia y las tiraba).
> Host-verificado: `test_telemetry_top` 20/20 (golden exacto), `pio run -e top_robot2_pri`
> SUCCESS, `pytest` monitor-base 114. **✅ VALIDADO EN BANCO 2026-06-14 (Gustavo):** el
> monitor anda en la placa TOP real **y el TOP→CENTRAL llega [OK]** (`diag_central_rx_all`:
> SNAPSHOT 66 Hz, 0 CRC, 0 seqGap). **TASK-209 CERRADA**; TASK-208 cumplida vía `--top-salud`.
> Se arregló un **parpadeo** de la GUI (la ventana se redimensionaba cada frame). **Hallazgo
> aparte (NO bloquea): el OTOS de la DOWN salía inválido** (pose `conf=0`, vel no difunde)
> **porque R2 NO tiene OTOS y estaba con el binario `down` (OTOS=2); fix = flashear
> `down_robot2` (OTOS=0)** → **TASK-308** (corregido: era binario equivocado, no batería).
> A2.2 enmascarado sigue sin aplicar (zonas = solo
> lectura). Contrato: `docs/firmware/TELEMETRIA-TOP.md` (campo `z`). Journals:
> `journal/2026-06-14-monitor-top-salud-y-zonas-telemetria.md` +
> `journal/2026-06-14-banco-monitor-top-validado-y-top-central-ok.md`.

> **📋 AUDITORÍA INTEGRAL 2026-06-11 (48 agentes, 122 hallazgos, 35 confirmados adversarialmente):**
> el backlog consolidado y priorizado vive en **[`docs/BACKLOG-INCHEON.md`](BACKLOG-INCHEON.md)**
> (P0: deliverables jueces + visión en sede + cap térmico de motores; P1: B1 freno-clavado,
> statics GK, delay 2s en competencia, heading sin detector de muerte, etc.). El mapa de envs
> vigentes/obsoletos ahora es **[`docs/pruebas-banco/QUE-FLASHEO-HOY.md`](pruebas-banco/QUE-FLASHEO-HOY.md)**.
> Fixes docs-only de esa noche ya aplicados (detector heading del analizador, default_envs,
> tutorial build, banner TOP.md, moratoria CLAUDE.md cerrada). Journal:
> `journal/2026-06-11-auditoria-integral-repo-y-backlog.md`.

> **🎓 PRÁCTICA CON ALUMNOS 2026-06-12 (preparada 2026-06-11 post-demo — vale sobre lo de abajo):**
> La demo del 2026-06-11 SE HIZO. Siguiente hito: práctica de 2 h en cancha, un robot por alumno.
> **Virginia + R2 = ARQUERO INTEGRAL** (FSM v3.3 existente + debut de INTERCEPT/CLEAR con pelota):
> envs nuevos `central_robot2_arquero_patrol_bb` / `central_robot2_arquero_bb` (solo agregan caja
> negra). Guion: [`docs/pruebas-banco/PRACTICA-2026-06-12-VIRGINIA-ARQUERO-R2.md`](pruebas-banco/PRACTICA-2026-06-12-VIRGINIA-ARQUERO-R2.md).
> **Elías + R1 = DELANTERO SIN GYRO por OTOS** (BNOs de R1 desconectados; sus 2 OTOS reemplazan
> al gyro): env `central_robot1_delantero_practica_bb` (+`_obst_bb` 2ª instancia anti-choque
> HC-SR04/ToF a 250 mm). Flags nuevos TODOS off-by-default (`ATK_OTOS_NOGYRO`,
> `ATK_SEARCH_SPIN_ONLY`, `ATK_OBSTACLE_STOP_MM`, `CENTRAL_FORCE_ROLE_ATTACKER`) → resto de envs
> byte-idénticos. Helpers puros en `src/shared/atk_nogyro.h` (host-tested). Guion:
> [`docs/pruebas-banco/PRACTICA-2026-06-12-ELIAS-DELANTERO-R1.md`](pruebas-banco/PRACTICA-2026-06-12-ELIAS-DELANTERO-R1.md).
> **Caja negra v1.2**: columnas `otos_*` + panel CENTRAL con `otos=` + detector "EMPUJE TORCIDO"
> en `tools/blackbox/analizar_corrida.py`. ⚠️ Riesgo #1 conocido: el SIGNO del yaw OTOS nunca se
> validó en banco → paso 2 del guion de Elías lo chequea A MANO antes de mover nada (bail-out 45°
> de red). Gate 58/798/0 + 8 envs pio SUCCESS (2026-06-11). Journal:
> `journal/2026-06-11-preparacion-practica-alumnos-delantero-otos-arquero-integral.md`.
> **+ OPCIONAL "cámara pegajosa"** (hallazgo P1: `fuse_ball_dual` PROMEDIABA las 2 cámaras en el
> caso ambas-ven — imposible para una pelota física → pelota fantasma en el punto medio): fix
> listo y gateado en env **`top_robot2_pri_sticky`** (`-DTOP_CAM_STICKY`, `ball_sticky.h` puro +
> test_ball_sticky; titular con memoria, conflicto = conf 60, panel `ball=(x,y)cNN`), **NO
> flasheado** — lo prueba el alumno que termine primero:
> `docs/pruebas-banco/PRACTICA-2026-06-12-OPCIONAL-CAMARA-PEGAJOSA.md`. Revertir = `top_robot2_pri`.

> **🥅 BANCO 2026-06-09/10 (arquero en ROBOT2 — vale sobre menciones más abajo):**
> (1) ✅ **Patrulla del arquero v3.2+v3.3 "PEGADA A LA LÍNEA"** (main `c11d770`): mantiene el
> sentido tramo a tramo hasta tocar la línea LATERAL que ve DOWN → rebota; línea ATRÁS = guía
> (avance corto ~3 cm para despegarse); sub-fase **RE-ENGANCHE** si la pierde + **guard
> anti-caminar-al-arco** (2 re-enganches vacíos → no retrocede más: requisito duro = nunca
> meterse al área). Pulsos de frente domesticados (35°, 40-80 ms, máx 2, settle 700 ms) →
> **giros violentos eliminados en banco**. Checklist de cierre (7 puntos) →
> `docs/pruebas-banco/ARQUERO-EN-ROBOT2-PLAN.md` FASE 4. LINE_AVOID quedó inalcanzable para el GK.
> (2) ✅ **RESUELTO EL MISMO DÍA — el loop del TOP estaba a ~6 Hz; ahora ~190.000/s.**
> Se midió (Δ`loop=` del panel `[TOP]`): el WorldSnapshot llegaba a ~4 Hz a la CENTRAL
> (heading 250-500 ms viejo — explica el ping-pong de pulsos, el J/U de reversa, y generaliza
> el punto (3) del banco 2026-06-03 de abajo). **Causa raíz: los 4 `getRangingData()` del
> VL53L7CX por pasada, cada uno trayendo el bloque COMPLETO de resultados por `Wire`@100 kHz
> (~60 ms/sensor).** Fix doble, validado en banco por Gustavo: **round-robin** (UN ToF por
> tick, `a6c0366`) + **payload recortado** (`-DVL53L7CX_DISABLE_*` de los bloques no usados,
> solo distance+status). Resultado: snapshot de vuelta a **100 Hz de diseño**, hdg trackea
> giro a mano sin congelarse, ToF dinámicos, `resync=0`. **TASK-014 baja P0→P2** (resta
> CENTRAL/DOWN). Los pulsos del arquero quedaron tuneados para 4 Hz → se pueden re-apretar
> en un próximo banco (35°→20°, settle 700→400). ⚠️ ROBOT1 hereda ambos fixes — A VERIFICAR.
> (3) ✅ **Juez desde la PC** (main `44b129b`, SOLO envs de banco del arquero): monitor serie de
> la CENTRAL = juez (`g`/ENTER=GO, `s`=STOP; STOP→WAIT_START→`g` re-corre todo). En competencia
> el flag no se define (GO/STOP real = app del juez por GPIO 5/6 del TOP).
> Journal completo: `journal/2026-06-10-banco-arquero-juez-pc-patrulla-v32-v33-top-lento.md`.
> (4) 🔧 **ESTADO HW DE ROBOT1 (banco escritorio 2026-06-10 noche, Gustavo — NO re-diagnosticar):**
> · **Cámaras ×2: ANDAN** — script v2 ya flasheado, `pkts_F/B` subiendo, `resync=0`, pelota
>   trackeada por ambas. NO tocar.
> · 🔧 **GYRO DE R1 — recableado HECHO (2026-06-11):** el "BNO
>   muerto por golpe" era FALSO diagnóstico — un BNO sano trasplantado congelaba IDÉNTICO →
>   el freeze era del BUS `Wire` compartido bajo carga (la conclusión 2026-06-08 del repo).
>   El equipo RECABLEÓ la TOP de robot1 a la arquitectura de robot2 (BNO primario
>   en bus propio Wire2 24/25 + secundario en Wire). **TERMINADO 2026-06-11: recableado
>   validado (scan I²C 0x28 en Wire2 ✓). Flashear `top_robot2_pri`** (los envs
>   `top_robot1*` quedan para el cableado viejo). **⚠️ BNOs de R1 hoy DESCONECTADOS**
>   (uno congelaba y el soft-resync arrastraba al sano) → **R1 corre SIN gyro**
>   (test del giro pendiente de reconectar BNO).
>   El BNO "muerto" original → re-test en bus propio (posible repuesto gratis). Cada CENTRAL
>   con su env per-robot; desde el recableado 2026-06-11 el M2 de R1 quedó DERECHO →
>   `MOTOR_INVERT={+1,+1,+1}` en ambos (`8d5fc90`, validado piso).
> · **BNO-R (0x29): MUERTO desde antes** (unidad quemada) — `imu_R=N` es lo esperado, no es noticia.
> · **TOP de R1 heredó los fixes**: loop ~220k/s, ToF 4/4 (`min_obst` ok) en el banco 2026-06-10.
> · **DEMO de R1 = `diag_central_arbitro_strafe_robot1`** (validado end-to-end con la app
>   2026-06-11); ToF derecho (LP pin 11) no enumera — pendiente cable. **CORRECCIÓN 2026-06-11
>   noche (Gustavo): la TOP de R1 SÍ se alimenta de la batería del robot, igual que R2** — la
>   nota de la madrugada "VIN sin soldar → TOP solo vive por USB" quedó superada (si la TOP
>   solo encendiera por USB, eso es un FALLO a reportar, no el estado normal).

> **🔧 ÚLTIMO (2026-06-02 — vale sobre cualquier mención más abajo):** mapa UART final.
> **TOP (Teensy 4.0):** S1←DOWN · **S2 (7/8)↔COMM** · S3←cam frontal · **S4 (16/17)→CENTRAL** · S5←cam trasera.
> ⚠️ **El 4.0 NO expone S7 (28/29) en el borde** (back-pads) → el enlace a CENTRAL va por **S4**, no S7 (fix 2026-06-02: antes estaba en S7 y el TOP nunca le llegaba a la CENTRAL).
> **CENTRAL (Teensy 4.1):** **S7 (pin 28)←TOP** (el cable sale del TOP pin 17/TX4) · **S1 (pin 0)←DOWN** · **pines 7/8 LIBRES para el motor 2** → **conflicto 7/8 (TASK-036) RESUELTO**. HC-SR04 en pines 4/3.
> CENTRAL **sin BNO** (los 2 BNO están en el TOP). **Los 4 ToF activos por default** en
> top_robot1/2 (`TOP_ENABLE_MULTI_TOF`; **I²C 100 kHz** — el BNO055 + ToF NO coexisten a 400 kHz, boot ~40 s). **`Zircon.pdf`** (esquemático
> del Zircon/CENTRAL, fuente Robomov) ya está en `hardware/electronics/`. Detalle único del
> cableado: `hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md`. Cualquier "conflicto 7/8
> abierto" o "Serial2 → CENTRAL" más abajo está **superado**.

> **🏁 BANCO 2026-06-03 (3 placas) — leer:** (1) ✅ **El árbitro mueve a la CENTRAL.**
> `diag_central_arbitro_strafe` validó en banco que el START/STOP del árbitro
> (COMM→GPIO 5/6→TOP→flag MATCH_RUNNING→Serial7→CENTRAL) dispara/frena la conducta.
> Primera vez que el árbitro mueve el robot end-to-end. (2) ✅ **RESUELTO 2026-06-08:**
> ese día solo giraba el motor 1 porque `inverse_kinematics({60,-60,180})` estaba en el
> eje equivocado (daba círculos) y subdimensionaba el par. Con la cinemática CALIBRADA
> `WHEEL_ANGLES_DEG={330,210,90}` (M1=del-IZQ · M2=del-DER · M3=trasera) un lateral puro
> da M1=M2=+0.5·vx (mismo lado) y M3=−vx (la trasera es la que más empuja). El piso de PWM
> pasó a ser POR RUEDA (`MOTOR_MIN_PWM[3]={70,70,42}`: delanteras oblicuas 70 > trasera
> paralela 42) para sacar a las ruedas del deadzone. ⚠️ **El `{70,70,42}` quedó SUPERADO
> 2026-06-09 → `{70,70,107}` + impulso inicial + freno anticipado de la trasera (banco R2;
> ver «Avance 2026-06-09» abajo).** **Pendiente de banco: SOLO el tuneo
> fino del lateral + confirmar el sentido.** Detalle → TASK-101 + journal
> `2026-06-03-banco-resultados-arbitro-strafe-y-bno-freeze.md`.
> (3) ⚠️ **El heading del BNO (TOP) se CONGELA en producción** (`top_robot1`): el snapshot
> llega sano por Serial7 (0 CRC, frames OK) y x/y cambian, pero `hdg` quedó clavado en
> −108.3°. Causa: contención BNO+ToF en `Wire` (`sensors_imu.cpp:167`), band-aid insuficiente.
> **Scope TOP.** Para el heading del arquero, la CENTRAL NO debe depender del BNO/TOP —
> usar el OTOS (llega vivo y local). (4) `seqGap` ~33% en el link DOWN = frames que DOWN
> dropea por backpressure (`down_tx.cpp:25` sube SEQ aunque descarte); dato fresco igual, no crítico.

## 📦 Para programar un subsistema: usar los packs

Hay 5 packs autocontenidos en `hardware/electronics/` (uno por subsistema
programable). Cada uno tiene TODO en un solo lugar: docs curados + snapshot
del firmware vivo + tests + ground-truth.

Punto de entrada: **[`hardware/electronics/PACKS-INDEX.md`](../hardware/electronics/PACKS-INDEX.md)**.

Lista rápida: `down-board-pack/`, `central-board-pack/`, `top-board-pack/`,
`cameraFront-pack/`, `cameraBack-pack/`. Cada uno tiene `README.md` con
"índice pregunta → doc".

> Los packs son snapshot del 2026-05-24. Si contradicen al código vivo del
> repo (`software/teensy/.../src/`), **gana el código vivo**.

## Calendario crítico

- **Incheon 2026** — 30-jun a 6-jul (**≈32 días** al 2026-05-29).
- **Estrategia** (CLAUDE.md): inversión en aprendizaje, no en podio. Robot
  honesto, partidos jugados, captura sistemática.
- **Estado realista hoy**: 50/50 que el robot compita "de verdad" (ver
  `journal/2026-05-19-analisis-coach-fabrica.md` cuando se cree).

## Módulos VIVOS (corren en binario hoy)

### CENTRAL (Teensy 4.1, Zircon Rev v15)
- `src/central/main_central.cpp` — entry
- `src/central/strategy.cpp` — FSM ATK + GK Nivel 2 (KICKOFF/SEARCH/POSITION/APPROACH + PATROL/INTERCEPT/CLEAR + LINE_AVOID). **El cerebro.**
- `src/central/motors_zircon.{h,cpp}` — PWM 3 motores omni (sin kicker físico: el robot empuja la pelota por inercia). **Motion lateral ESTÁNDAR (banco R2 2026-06-09, decisión Gustavo — vale para TODO movimiento lateral en TODOS los programas):** piso de PWM por rueda `MOTOR_MIN_PWM={70,70,107}` + impulso inicial fijo `{130,130,140}` PWM ×40 ms (gateado `-DCENTRAL_MOTOR_KICKSTART`) + freno anticipado de la trasera 66 ms (`motors_set_rear_cut()`, gateado `-DCENTRAL_REAR_BRAKE_LEAD`, hoy cableado solo en `diag_central_strafe.cpp`). **R2 VALIDADO en banco; R1 arranca de los mismos valores — A VERIFICAR EN BANCO R1.** Fila canónica: `FUENTES-DE-VERDAD.md` (CENTRAL — motores).
- `src/central/imu_zircon.{h,cpp}` — BNO055 (⚠️ ya NO se conecta en CENTRAL desde 2026-05-31; compat gateado por `-DCENTRAL_HAS_LOCAL_BNO`, off; el heading viene de ARRIBA)
- `src/central/world_model.{h,cpp}` — espejo del WorldSnapshot
- `src/central/comm_top.{h,cpp}` — recibe WorldSnapshot del TOP por **`Serial7` (RX7 = pin 28)** (reasignado 2026-05-31: antes Serial1, se movió a Serial7 cuando el link a DOWN tomó Serial1)
- `src/central/comm_down.{h,cpp}` — recibe LineStatusV2 + OTOS (Pose2D/Velocity2D) del DOWN por **`Serial1` (pin 0)**. ✅ Conflicto 7/8 **RESUELTO** (2026-05-31: UART movido a Serial1; los pines 7/8 quedan para el motor 2). Receiver de banco: `diag_central_comm_down` ([doc](firmware/DIAG-CENTRAL-COMM-DOWN.md)). 📊 **Análisis profundo del link** (protocolo/CRC, buffers, timing, recuperación ante cortes, P0/P1 + checklist "primera instancia") → [`docs/firmware/ANALISIS-COMM-DOWN-CENTRAL-2026-05-31.md`](firmware/ANALISIS-COMM-DOWN-CENTRAL-2026-05-31.md).

- **Diags de banco (CENTRAL, `src/diag/`):** `diag_central_motors` (motores + `MOTOR_DIR`), **`diag_central_strafe`** (patrulla lateral del arquero — **open-loop**, omega=0, sin BNO en CENTRAL; ahora que el OTOS llega a CENTRAL por broadcast se le puede sumar heading-hold = v2 — [doc](firmware/DIAG-CENTRAL-STRAFE.md)), `diag_central_drive_straight` (+Y con heading del TOP), `diag_central_comm_down` (link DOWN→CENTRAL), `diag_central_rx_all` (decodifica DOWN+TOP juntos).

### TOP (Teensy 4.0)
- `src/top/main_top.cpp` + `cameras_runtime`, `cameras`, `sensors_imu`, `sensors_tof` (4 ToF VL53L7CX en bus único `Wire`, lib `Adafruit_VL53L7CX`. ✅ Bodge de Enzo 2026-05-30: los 4 ToF con LP en pines {9,10,11,12} (activo-alto), enumeran a 0x2A..0x2D, confirmado en banco. El bus `Wire2` (24/25) del TOP quedó liberado (corrección 2026-06-09: el bus de 24/25 es `Wire2`/LPI2C4, no `Wire1`; es donde va el 2º BNO — TASK-207). ⚠️ Probar ToF SIEMPRE con power-cycle (las direcciones I²C persisten). El firmware vivo `sensors_tof.cpp` YA lee los 4 ToF (round-robin UN sensor por tick `a6c0366` + payload VL53L7CX recortado `bf8ddd4` — banco 2026-06-10, loop del TOP 6 Hz→190k/s). Plan: escalar a 6 ToF (4 fijos + 2 móviles para pelota). Ver journal 2026-05-30), `comm_*`
- `src/shared/localization.cpp` — trilateracion geometrica directa (Sprint 1
  aprobado 2026-05-25, ver `docs/superpowers/specs/2026-05-25-localization-sprint1-trilateration-design.md`).
  Validacion en hardware pendiente: TASK-035.
- `src/top/localization_runtime.cpp` — glue I/O.
- `src/top/hardware_profile.h` + `pinout_common.h` + `pinout_robot1.h`
  + `pinout_robot2.h` — HAL refactor Sprint A (2026-05-29). El código
  vivo del firmware usa `hardware_profile.h` (a través del wrapper
  legacy `config_top.h`). Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md

### DOWN (Teensy 4.0) — **deuda: 2 cadenas paralelas**
- `src/down/main_down.cpp` → llama `line_ring.{h,cpp}` (cadena vieja, lectura cruda 1 kHz)
- `src/down/comm_central.cpp` → llama cadena nueva: `down_model + line_geometry + line_tracker + line_calib + surface_monitor + down_encode` para armar `LineStatusV2` que va al CENTRAL
- **NO archivar ni una ni otra antes de Incheon.** Decisión binaria post-Incheon (ver `FUENTES-DE-VERDAD.md` deudas).
- **Broadcast simétrico (Capa 1, 2026-06-01):** DOWN ahora **difunde** los 3 frames
  a **ambas** placas — CENTRAL (`Serial1`) **y** TOP (`Serial5`) — vía el módulo
  nuevo `down_tx` (SEQ monótono por enlace), con estas **tasas** (no todas 100 Hz):
  `LineStatusV2` 0x10 **@200 Hz** + `Pose2D` 0x11 **@100 Hz** + `Velocity2D` 0x12 **@100 Hz**.
  ⚠️ TOP **recibe** la línea (0x10) pero **no la consume** (sólo necesita la odometría
  OTOS); la línea es el bus de emergencia que CENTRAL usa para frenar en el borde.
  Antes la línea iba solo a CENTRAL y el OTOS solo a TOP.
  CENTRAL ingiere el OTOS directo (storage/accessors en `world_model`). Contrato
  canónico de tasas/tamaños: `docs/firmware/CONTRATO-DATOS-DOWN.md` §2. Spec/plan:
  `docs/superpowers/specs/2026-06-01-down-broadcast-simetrico-design.md` +
  `docs/superpowers/plans/2026-06-01-down-broadcast-capa1.md`.
- **Capa 2 + 3 (2026-06-02, code-complete + pusheadas):** CENTRAL **consume** el OTOS para
  ir/patear derecho (`drive_straight` en ATK KICKOFF/APPROACH) y el arquero hace **strafe
  paralelo a la línea** por `cross_track_mm` real (centroide-Y) que calcula DOWN. **Fallback
  EXACTO** cuando OTOS/cross_track están en N/A (= hoy) → **no cambia la conducta actual**;
  las conductas nuevas se activan recién con OTOS fluyendo + DOWN con Capa 3. Falta **banco**
  (tunear gains + confirmar eje/signo del strafe del GK). Gate: 25 envs + 311 tests host.

### Shared (puro, testeado host-native)
- `pids`, `kinematics`, `behind_ball`, `cameras_fusion`, `ball_velocity` (velocidad pelota → enciende `bt_classify`; vivo en `build_snapshot`), `line_filters`, `crc16`, `proto`, `types`
- `strategy_transitions` (caracterización pura de `strategy.cpp` con 35 tests — no conectado, mantener como red)

## Tests host-native

| Suite | Tests | Cubre |
|---|---|---|
| `test_kinematics` | 11 | omni-3 |
| `test_pids` | 18 | heading + lateral + distancia |
| `test_proto` | 13 | CRC, frame, marker |
| `test_line_filters` | 39 | temporal + hysteresis + spatial + centroide + lifted + saturación todo-blanco |
| `test_cameras_fusion` | 16 | rot 180°, fuse front+back, watchdog |
| `test_behind_ball` | 16 | target detrás, alineación para empujar, attack-line, kickoff |
| `test_strategy_transitions` | 35 | árbol decisión ATK + GK (caracterización) |
| `test_localization` | 14 | trilateracion + outliers + rotaciones + edge cases |
| `test_calib_storage` | 19 | persistencia de calibración (`calib_storage`) |
| `test_sensor_health` | 12 | salud/watchdog de sensores (`sensor_health`) |
| `test_central_contract` | 2 | contrato CENTRAL |
| `test_central_line_ingest` | 8 | ingest `LineStatusV2` DOWN→CENTRAL (`line_view`) |
| `test_central_motion` | 9 | `motion_target` |
| `test_central_trajectory` | 7 | `ball_trajectory` |
| `test_down_calib` | 5 | cadena DOWN: calib |
| `test_down_encode` | 3 | cadena DOWN: encode |
| `test_down_geometry` | 20 | cadena DOWN: geometry |
| `test_down_model` | 7 | cadena DOWN: model |
| `test_down_surface` | 5 | cadena DOWN: surface |
| `test_down_tracker` | 3 | cadena DOWN: tracker |
| `test_ball_velocity` | 13 | velocidad pelota (EMA + reset al perder + clamp int16) |
| **Total (20 envs)** | **262** | **0 fallos** |

> ⚠️ La tabla de arriba es snapshot 2026-05-29. **Número vivo (2026-06-05):
> 624 tests / 44 suites (envs) / 0 fallos** vía `scripts/run-host-tests.sh` (la tabla no
> incluye los tests sumados después: broadcast, drive_straight, imu_fusion,
> tof_zone_orient, otos_ingest, gk_cross_track, ball_velocity, **cameras_parser**).

> **Historial del conteo de tests (no reescribir las líneas viejas):** 246/19 (2026-05-29)
> → 262/20 (2026-05-29 post-merge) → 324/26 (2026-06-03) → 354/29 (2026-06-03 pt.2)
> → 403/33 (2026-06-03 pt.3) → **624/44 (2026-06-05, vigente)**, todos 0 fallos.

**Estado (2026-05-29, post-merge 3 agentes):** ✅ **262 tests / 20 envs / 0 fallos** —
verificado con `pio test -e test_native` tras mergear central+top+down a `main`
(el +16 vs el snapshot previo de 246 = `test_central_line_ingest` nuevo + tests de DOWN).
También corren **offline** con `scripts/run-host-tests.sh` (compila cada test
con g++ contra Unity vendoreado + `src/shared`, salteando PlatformIO y el
registry que Avast bloqueaba). TASK-025 (excepción Avast) **deja de ser
bloqueante** para correr la suite — sigue siendo deseable para `pio test`
nativo, pero ya no es el único camino. Ver
`journal/2026-05-29-auditoria-top-pre-incheon-top.md`.

## TASKs activas — snapshot al 2026-05-19 (lista viva abajo + en `team-tasks/`)

> ⚠️ **Lista congelada al 2026-05-19.** El estado vivo está en las secciones
> «Avance YYYY-MM-DD» de arriba + los archivos de `team-tasks/` (incluye las
> series 100/200/300, que no figuran en esta lista). No tomar esto como estado
> actual de prioridades.

**P0 hardware (asignar HOY a humanos, no a Claude):**
- TASK-001 (Enzo): fix 10 nets DOWN PCB
- TASK-002 (Enzo): DRC+ERC ambas placas
- ~~TASK-006 (Virginia/Elías): flash firmware COMM ESP32-C6~~ → ✅ **FLASHEADA 2026-06-01** (Gustavo). E2E del árbitro **RESUELTO 2026-06-02 (TASK-039)**: el START/STOP del árbitro llega al TOP como **NIVEL GPIO en pines 5/6** (`OUT1`/`OUT2`), no por UART (fix 2026-06-02 / TASK-039: el árbitro es NIVEL GPIO en pines 5/6 del TOP, no UART). El `Serial2` (pines 7/8) del COMM queda SOLO para partner ESP-NOW / status.
- ~~TASK-011 (Enzo): confirmar PIN_KICKER_SOL en Zircon~~ → **CANCELADA**: el robot NO tiene kicker físico (el delantero empuja la pelota por inercia); no se cablea solenoide.
- TASK-013 (Enzo): recuperar BOM placa TOP
- TASK-025 (todos): excepción Avast en cada máquina → destraba PlatformIO

**P0 firmware (alguien tiene que hacerlo, pero solo después de tener placa que compile/flashee):**
- TASK-014 (Virginia/Elías): loop TOP no-bloqueante medido con osciloscopio
- TASK-015 (Virginia): CRC + fin de trama enlace cámara
- TASK-016 (Virginia/Elías): fail-safe borde (OR-latch + precedencia)
- TASK-022 (Virginia): cámara operativa (sentinel, exposición fija, recalib Incheon)
- TASK-023 (Virginia/Enzo): build/tooling CI
- TASK-024 (Virginia/Elías): arranque rol/polaridad arco

## Bloqueantes Incheon (los 2 que importan)

> ⚠️ **BNO heading — PROBLEMA ABIERTO, NO bloqueante (TASK-207, 2026-06-08).** El heading del BNO055
> NO anda en producción (`main_top`): `hdg=0.0`/`flags=0x0`; en `diag_top_all` SÍ trackea. **Causa:
> contención del BNO055 en el bus `Wire` compartido con los 4 ToF, bajo la carga de `main_top`.** El
> SOFTWARE está AGOTADO (100 kHz + 20 Hz + deconflict + noInterrupts → los 4 fallaron). **Fix = BNO a
> bus aparte (Wire2 24/25), como ROBOT2** (hardware + 1 línea fw). **NO bloquea Incheon:** el arquero
> degrada con gracia → navega por **línea (DOWN) + cámara (pelota/arcos) + heading del OTOS** (no del
> BNO); `central_gate_heading_omega` pone ω=0 si `heading_valid=0` (no orienta con rumbo falso). Solo
> pierde la orientación fina por giroscopio. Detalle: `journal/2026-06-08-bno-contencion-bus-debug-y-arquero-sin-bno.md`.

1. **COMM — firmware flasheado ✅ (2026-06-01); E2E del árbitro RESUELTO ✅ (2026-06-02, TASK-039).** El árbitro RCJ **NO viaja por UART**: señaliza como **NIVEL GPIO** hacia el TOP (Teensy 4.0) en **pin 5 = OUT1 (PLAY/STOP)** y **pin 6 = OUT2 (PLAY/STOP)** (en la práctica, en PLAY sube SOLO UNO de los dos —no son espejo—). Nivel: **0 = juego PARADO, 1 = juego EN CURSO (3.3 V)**. Firmware: `src/top/comm_arbiter.cpp::read_referee_gpio()` lee los pines 5/6 con `INPUT_PULLDOWN` y `match_running = (pin5 OR pin6)` (en PLAY sube SOLO UNO de los dos pines —el otro queda en 0— por eso AND nunca daba GO y OR sí; probado en banco 2026-06-02, Gustavo. Sigue siendo fail-safe: si se desconecta el cable del COMM, ambos pines leen 0 con `INPUT_PULLDOWN` → `match_running=false` → STOP). El probe temporal se removió de `main_top.cpp`. El **UART del módulo COMM (TOP `Serial2`, pines 7/8) queda SOLO para partner ESP-NOW / status** — el viejo `COMM_REFEREE_CMD` por UART quedó **obsoleto**. (fix 2026-06-02 / TASK-039: el árbitro es NIVEL GPIO en pines 5/6 del TOP, no UART). El robot ya recibe START/STOP por GPIO → homologa el árbitro. TASK-006/TASK-039.
2. **Cámaras — calibración de DISTANCIA ✅ HECHA (Elías, 2026-06-07)**, integrada a
   producción **v2** (homografía en el script de producción `camaras-openmv/main.py` @**VGA**;
   ⚠️ corrección 2026-06-15: el destino es `main.py`, NO `cam-*-n6.py` —deprecados—; misma H para las 4 cámaras,
   decisión provisoria). TASK-022 ya **no** es "sin calibrar". **Lo que queda (banco, lo hace
   el equipo):** (a) **deploy coordinado** — re-flashear las 2 cámaras (v2 @VGA con la H nueva)
   **+ el TOP** juntos (CRC OK / sin pelota fantasma); (b) medir y restar el **offset
   lente→centro del robot** (las distancias son desde el lente, no del centro); (c) **fps a
   VGA**; (d) **lock de exposición/WB/gain + LAB bajo luz de Incheon** + estabilidad; (e)
   distancias vs regla. Doc: [`CALIBRACION-HOMOGRAFIA-XY-N6.md`](firmware/CALIBRACION-HOMOGRAFIA-XY-N6.md)
   §Resultado + `journal/2026-06-07-calibracion-distancia-camara-frontal-elias.md`. (Migración
   H7→N6, bugs P0 y velocidad de pelota ya estaban resueltos — Avance 2026-06-03.)

### Avance 2026-06-15 — TASK-022 (cámaras): tooling host del solver de homografía (acelera el banco, sin tocar hardware)
- **El núcleo de TASK-022 es BANCO** (calibrar LAB/exposición bajo luz de Incheon, medir distancias con regla,
  fps@VGA) — Claude NO lo cierra. Un workflow (7 agentes + verificación) identificó lo **host-testeable/tooling**
  que sí acelera el banco. Journal: `journal/2026-06-15-task022-tooling-solver-homografia.md`.
- **`solve_homografia.py` endurecido** (PC, numpy): **`--csv FILE`** (antes había que editar el fuente a mano en
  cada recalibración — se corre ≥4 veces), **guardas anti-calibración-mala** (puntos colineales / <4 / H[2,2]~0 →
  mensaje claro en vez de una H inválida silenciosa), **modo `--validate`** (predice X/Y de píxeles de prueba con la
  H ANTES de flashear) + fix del emoji que crasheaba en consola Windows. Test nuevo `test_solve_homografia.py` **14/14**.
- **HI-6 (doc P1, de-risk Incheon):** 3 docs mandaban a pegar la H en `cam-*-n6.py` (**DEPRECADO** 2026-06-08); el
  destino real es `camaras-openmv/main.py`. Corregido el banner del doc de homografía + ESTADO + el `print_h_block`
  del solver. (El equipo iba a flashear el archivo equivocado en sede.)
- **Pendiente (banco, no Claude):** recalibrar LAB/exposición en sede, fps@VGA, validar distancia con regla. Opcional
  (decisión Gustavo): HI-5 alinear el baseline LAB/H de `robot2.h` al último-bueno de `main.py`.

### Avance 2026-06-15 — Cierre host-testeable de la TOP: estimador pose_fusion numéricamente correcto + clamp drive_straight
- Pedido de Gustavo: terminar TODOS los módulos/funcionalidades de la TOP. Un **workflow de inventario
  (10 agentes + verificación adversarial)** mapeó el TOP y separó lo terminable host-testeable del glue
  Arduino de banco. **Verdad central honesta:** el pipeline de competencia ya está vivo/byte-idéntico; el
  grueso del remanente es glue (ISR/DMA/timer/Wire) de banco. Journal: `journal/2026-06-15-cierre-top-estimador-correcto.md`.
- **Se cerraron los 3 agujeros NUMÉRICOS del estimador `pose_fusion`** (estaba cableado pero MAL al girar):
  **H1** módulo nuevo `rot_lut.h` (sin/cos Q12 círculo completo) + des-rotación del delta OTOS al marco de
  cancha (`net=bno−otos`; antes integraba crudo → deriva en dirección equivocada al girar); **H2** gate de
  seed por consenso (3 ToF consistentes antes de anclar — evita anclar contra un rival/pared al boot);
  **H3** campo `heading_valid` (no anclar/corregir con heading muerto). Cableado gateado en `main_top`.
- **H6** clamp de omega en `drive_straight` (float, ±327 °/s) — no desborda el centideg int16 del caller.
- **Honestidad:** estos fixes NO cambian el binario de competencia (byte-idéntico) — dejan el estimador
  correcto para el día de banco. Encender `TOP_ENABLE_POSE_FUSION` sigue siendo banco (TASK-210/211).
- Tests: `test_rot_lut` 8 + `test_pose_fusion` 11→16 + `test_drive_straight` 9→10.

### Avance 2026-06-15 — Confiabilidad del heading: cross-validación independiente + centinela dual-BNO @1Hz (módulos puros)
- Pedido de Gustavo: heading MÁS CONFIABLE con AUTO-RECUPERACIÓN (decidir qué BNO está sano con datos
  INDEPENDIENTES, no auto-referencia; centinela @1Hz del 2º BNO con ToF pausados; fusión cámara+OTOS;
  reseteo si deriva). **Construye sobre TASK-212** (análisis subido por otra sesión — el mismo enfoque) +
  aporta el **centinela dual-BNO @1Hz** (idea de Gustavo, no estaba en TASK-212). El **INC-1 (gyro-guard)
  de hoy ES la Fase 0** de TASK-212; esto es la **Fase 1**. Journal: `journal/2026-06-15-fusion-confiabilidad-heading-bno.md`.
- **Workflow de diseño (9 agentes) + verificación adversarial** reencuadró el alcance: (1) **FAILOVER
  físico RECHAZADO para Incheon** — `imu_fusion` NO hace failover idx0→idx1 transparente (promedia), y un
  failover a heading PLAUSIBLE-PERO-MALO es PEOR que `heading_valid=0` (el arquero rotaría fuera del arco).
  Jerarquía: primario-sano > heading_valid=0 > secundario (2027). (2) La cámara NO mide ω si el arquero
  hace strafe (traslación contamina). (3) **Anti-falso-veto**: con <2 refs INDEPENDIENTES, JAMÁS MALO.
- **Implementado PURO + host-tested (riesgo cero, gateado):** `src/shared/goal_rate_tracker.h` (w_cam del
  bearing del arco, resta angular envuelta — NO espejo de ball_velocity; `test_goal_rate_tracker` 7/7) +
  `src/shared/imu_cross_validate.h` (salud por mediana de refs + anti-falso-veto + consenso + scheduler
  del centinela con timeout; **NO failover**, solo veredicto/telemetría; `test_imu_cross_validate` 13/13).
- **Bloqueado a banco (glue Arduino, 3 blockers) → TASK-213:** init del secundario, centinela 1Hz inline
  en `sensors_tof_tick` (ToF-pausa), cableado cámara/OTOS. Failover físico = 2027.

### Avance 2026-06-15 — Optimización TOP no-bloqueante (workflow): INC-1 gyro-guard + INC-2 pose-age (gateados)
- Continuación del trabajo RT, foco **placa TOP**, con metodología superpowers + 2 workflows (plan de 11
  agentes + verificación adversarial de 5). Todo gateado off-by-default → binarios byte-idénticos.
  Journal: `journal/2026-06-15-top-optimizacion-no-bloqueante.md`.
- **Hallazgo del workflow:** el loop del TOP YA está muy afinado (round-robin ToF + payload recortado →
  ~190k/s, TX no-bloqueante, RX acotado + addMemoryForRead, I²C 1 MHz init). El pendiente de alto valor
  NO era la arquitectura ISR/DMA (eso es post-Incheon) sino re-habilitar el **freeze-detector del BNO**
  (apagado por falso-DEAD el 2026-06-08), que además BLOQUEA `pose_fusion` (lo exige por `#error`).
- **INC-1 (P0) — guarda de gyro en `imu_freeze`:** nueva variante pura `imu_freeze_update_g` (la vieja
  intacta) que solo declara congelado si el gyro probó rotación REAL mientras el heading quedó clavado →
  **robot quieto nunca muere** (mata el falso-DEAD) y no es inerte. Cableada en `sensors_imu.cpp` (gyro ya
  leído, 0 I²C extra). `test_imu_freeze` 13→30. Envs banco `top_robot1_bnofreeze` + `top_robot2_pri_bnofreeze`.
- **INC-2 (P1) — edad fina del OTOS:** `pose_age.h` puro (nunca-recibido → edad MÁX, no 0) + getter
  `comm_down_pose_age_ms()` + gating de `pose_fusion` a `otos_stale_ms`≈60 ms (vs booleano de 500 ms).
- **Verificación adversarial: 0 blockers / 0 majors de corrección.** Sus hallazgos de cobertura ya
  aplicados (blindaje anti-regresión del falso-DEAD, bordes de umbral, rotación negativa).
- **Diferido (post-Incheon, banco):** INC-3 (snapshot_assembler — choca con pose_fusion) e INC-4/5/6
  (sensor_slot ISR + RX-IRQ + emisor por timer — glue no host-testeable; el review halló un blocker de
  concurrencia real en el emisor por timer). Banco: **TASK-211** (freeze-detect) + **TASK-210** (pose_fusion + otos_stale_ms).

### Avance 2026-06-15 — Integración RT GATEADA: quick-wins CENTRAL + pose_fusion TOP + motor_slew (todo OFF-by-default)
- **Se CABLEÓ parte del análisis RT al firmware vivo, todo gateado off-by-default → binarios de
  competencia byte-idénticos.** Rama feature + PR (NO push directo). Gate host 937/67/0 sin cambios
  (los cambios son glue Arduino; los módulos puros ya tenían sus tests). Journal:
  `journal/2026-06-15-integracion-rt-gateada.md`.
- **A1 `CENTRAL_DEBUG_SERIAL`** — el bloque de ~30 `Serial.print` cada 500 ms del `loop()` de CENTRAL
  ahora está gateado. El flag se DEFINE en `central_robot1/2` (byte-idéntico HOY); para SACAR el pico de
  jitter, **borrar el flag del env de competencia en banco**.
- **A2 `CENTRAL_TOP_RX_BIGBUF`** — `Serial7.addMemoryForRead(buf,512)` en `comm_top.cpp` (hoy 64 B → el
  snapshot del TOP se descarta en silencio si una vuelta se alarga). Default OFF = 64 B = binario de hoy;
  el equipo agrega el flag en banco (chequear que `resync` del link TOP baja).
- **B `pose_fusion`+`pose_filter`** — CABLEADOS en `main_top.cpp::build_snapshot` tras `-DTOP_ENABLE_POSE_FUSION`
  (env nuevo `top_robot2_pri_posefusion`). **INTERLOCK DURO**: `#error` si se prende sin
  `-DTOP_ENABLE_BNO_FREEZE_DETECT` (el heading es la raíz). **Seguro por diseño**: hasta que el ToF ancle
  (hoy casi nunca: sólo eje Y) la fusión NO inicializa → cae al comportamiento de localization de hoy.
  ⚠️ `ball_sticky` (`TOP_CAM_STICKY`) e `imu_freeze` (`TOP_ENABLE_BNO_FREEZE_DETECT`) YA estaban cableados
  de antes — verificado, no se re-hizo.
- **C `motor_slew`** (Capa 2 lazo CENTRAL) — CABLEADO tras `-DCENTRAL_MOTOR_SLEW` (env nuevo
  `central_robot2_strafe_slew_bb`): rampa `{vx,vy,omega}` antes de la cinemática. Los **reflejos
  (freno de borde, STOP/SAFE_NO_TOP) BYPASEAN la rampa** (se aplican YA + `slew_reset`).
- **NO cableados a propósito** (decisión de coach): `state_timer` + `sensor_slot`/`snapshot_assembler`
  (son del rewrite de FSM/loop NUEVOS → cablearlos = reescribir `strategy.cpp`/`main_*.cpp`, prohibido)
  y `line_early_escape` (cambia la señal de borde safety-crítica → necesita banco para titular el trigger).
  Ver el journal para el razonamiento.

### Avance 2026-06-15 — Arquero strafe: tuning del control de rumbo + mitigaciones de latencia (banco María)
- **Banco María (2026-06-14/15):** el arquero strafe (R2) andaba "muy feo" con el PFM de rumbo.
  Hallazgo: **anda MEJOR sin BNO** (PFM apagado) → el problema es el **lazo de rumbo / latencia del
  BNO**, no la base. Hoy el env `central_robot2_arquero_strafe_cam_bb` lleva `-DGK_STRAFE_NO_PFM`
  **temporal** (borrar el flag para reactivar el PID). Gains titrados a kp1/ki0.2/db10/win160.
- **Deriva mecánica residual** del strafe a ω=0 ≈ 8°/s: el **piso** de la trasera es el lever
  EQUIVOCADO (asimetría por FLOOR_SCALE → peor, medido 18 vs 8); el simétrico es la **eficiencia** →
  `MOTOR_EFF_X100[2]` 131→115 (A/B, **pendiente de banco**). + gate anti-lecturas-saltarinas de cámara
  y signo de centrado de pelota +1 (anda: "queda buscándola").
- **2 mitigaciones MERGEADAS (gateadas OFF → binario de competencia byte-idéntico):** **P0 fast-BNO**
  (env `top_robot2_pri_fastbno`, BNO a 100 Hz vs 20 → menos latencia) y **P1 rate-damp/"D"** (env
  `central_robot2_arquero_strafe_cam_ratedamp`, `GK_PFM_KD_RATE` + `heading_rate.h`). Doc:
  `docs/firmware/CONTROL-ARQUERO-LATERAL-Y-LATENCIAS.md`. **Pendiente de banco → TASK-103** (4 casos con
  caja negra). ⚠️ Para no re-implementar: estos 2 envs YA existen.

### Avance 2026-06-09 — Motores: MOTION LATERAL ESTÁNDAR validado en banco ROBOT2 (3 técnicas)
- **Banco R2 (Gustavo, `diag_central_strafe_robot2_kick`): "anda bien".** Antes de esto, en el
  strafe de R2 la trasera movía pero las delanteras no rompían la inercia, y al frenar la
  inercia de la trasera desacomodaba el robot. Tres técnicas lo resolvieron y quedaron como
  **ESTÁNDAR para TODO movimiento lateral, en TODOS los programas** (decisión Gustavo 2026-06-09):
  1. **Piso de PWM por rueda `MOTOR_MIN_PWM={70,70,107}`** — la trasera se barrió
     42→50→70→85→95→100→105→**107**. Física aprendida: el PWM NO es proporcional a la
     velocidad y es DISTINTO por rueda; en el strafe la trasera debe girar al DOBLE que las
     delanteras (cinemática: fronts 0.5·vx, rear 1.0·vx) pero como va ALINEADA (menos fricción
     que las oblicuas) lo logra con ~1.5× el PWM (107 vs 70), no 2×.
  2. **Impulso inicial fijo por rueda `{130,130,140}` PWM ×40 ms** en la transición
     parado→comando (gateado `-DCENTRAL_MOTOR_KICKSTART`; factor ×9.9 + cap por rueda =
     impulso fijo; la trasera pide 140 porque "se quedaba").
  3. **Freno anticipado de la trasera** (gateado `-DCENTRAL_REAR_BRAKE_LEAD`):
     `motors_set_rear_cut()` corta la trasera (idx2) a 0 en los últimos **66 ms** del tramo
     (tunable `-DDIAG_STRAFE_REAR_LEAD_MS`) mientras las delanteras terminan.
     **HOY cableado solo en `diag_central_strafe.cpp`.**
- **Además (mismo banco): motores R2 calibrados** — pines **IGUALES a R1 (NO rotados**, la
  suposición "rotados" venía del delantero 2025) y `MOTOR_INVERT={+1,+1,+1}` (el U17 de esa
  placa NO está invertido por HW).
- **ROBOT1: arranca de los MISMOS valores** ({70,70,107} + {130,130,140} + 66 ms) por decisión
  de Gustavo — **✅ VALIDADOS en piso 2026-06-11** (su `{70,70,42}` viejo era del banco 2026-06-08:
  la trasera se bajó porque rotaba en el strafe). Además en la reparación el M2 de R1 quedó
  RECABLEADO DERECHO → `MOTOR_INVERT={+1,+1,+1}` en ambos robots (`8d5fc90`).
- **Tema-a-analizar (NO implementado):** llevar el freno anticipado al lateral de la FSM del
  arquero (patrol/intercept) — el corte necesita saber cuándo TERMINA el movimiento, y en el
  control continuo de la FSM no existe ese evento (es glue futuro; `strategy.cpp` no se toca).
- **✅ HECHO (053fd0a, 2026-06-10 — corregido en auditoría 2026-06-11):** los envs de
  producción `central_robot1`/`central_robot2` YA llevan `-DCENTRAL_MOTOR_KICKSTART` y
  `-DCENTRAL_REAR_BRAKE_LEAD` (igual que los diags de strafe de ambos robots). Lo ÚNICO que
  sigue pendiente del freno anticipado es el glue de FSM del bullet anterior (tema-a-analizar).
- Canónico: fila «CENTRAL — motores» de `FUENTES-DE-VERDAD.md` + `config_central.h` +
  `motors_zircon.cpp` + `MOTION-CONTROL-PLAN-2026.md`. Journal:
  `2026-06-09-banco-robot2-bringup-sensores-y-bno-wire2.md` (sección motores).

### Avance 2026-06-03 (pt.3) — Visión P1 [CÓDIGO]: tests del parser + robustez detección + kit calib + análisis eje X
- **Agente de visión**, ítems [CÓDIGO] que NO necesitan banco (los P0 de higiene ya estaban hechos).
- **Parser de cámara con red de seguridad (Gap 7):** nuevo `test/test_cameras_parser/` (10 tests)
  para `src/top/cameras.cpp` (sentinel, **caracterización del bug fantasma**, resync, header-como-dato,
  reset). Unity-build (`#include` del .cpp) → corre en `run-host-tests.sh` sin tocar harness ni mover a shared.
- **Robustez de detección (cam-frontal/trasera-n6.py):** filtro de **forma** solo para la pelota
  (`is_ball_like` por aspect/density, **fail-open**, flag `BALL_SHAPE_FILTER`), ROI opcional en `None`
  (verificar montaje en banco), fps en bring-up. Arcos sin filtro de forma. Contrato de 9 bytes INTACTO.
- **Kit `calib-lab-n6.py`** (×2, sincronizados): cicla 3 colores sin re-Run, `draw_string`, márgenes por
  canal, try/except anti-crash. Para recalibración <5 min en Incheon.
- **Tests borde:** `test_cameras_fusion` 16→19, `test_ball_velocity` 16→17.
- **Análisis eje X (sin tocar código/contrato):** `research/in-progress/2026-06-03-eje-x-codificacion-asimetrica-vision.md`
  — X se codifica SIN offset (Y sí lo tiene) y el clamp uint8 aplasta la izquierda a 0 → pelota a la
  izquierda se lee "al frente". Más profundo que TASK-202 (representabilidad). **Decisión pendiente +
  coordinar con agente TOP** (toca el contrato).
- **Gate:** host **403 tests / 33 envs / 0 fallos**; `py_compile` OK en los 4 scripts. **NO se cierra
  TASK-022 ni TASK-202** (son banco). Journal: `journal/2026-06-03-vision-p1-codigo-parser-tests-robustez.md`.

### Avance 2026-06-03 (pt.2) — arquero anticipa + robustez velocidad + OTOS heading/slip + merges de agentes
- **Merges a main:** se trajeron a `main` los trabajos de los agentes **down** (`comm_top` = Serial7 en
  ESTADO), **central** (prep banco mitad-inferior + review GK + TASK-101 + caveat cinemática del strafe)
  y **top** (solo el journal histórico del árbitro; sus ediciones a TASK-006/204 quedaron afuera por
  estar superadas por TASK-039).
- **`ball_velocity` expira por tiempo (M4):** `ball_velocity_update` recibe `now_ms`; si los packets se
  cortan pero la cámara sigue `visible`, invalida la velocidad (no sirve fantasmas). 16 tests.
- **Arquero ANTICIPA (nuevo módulo VIVO `src/shared/ball_predict`):** el GK INTERCEPT apunta a la **X
  predicha** de la pelota (`pos + v·lookahead`, clamp) en vez de la X actual. Getters `world_model_get_ball_vx/vy_mm_s`.
  **Fallback automático**: con pelota quieta o velocidad N/A (vx=vy=0) la conducta es **idéntica** a hoy.
  9 tests. ⚠️ **Cambio de conducta → validar en banco** (tunear `lookahead_s`/`max_lead_mm`).
- **OTOS heading/slip (M2/M3):** heading dual por **promedio circular** de los headings absolutos (cubre
  ±180, antes saturaba a ±90); slip por **diferencia de velocidad** menos rotación esperada (antes era
  diferencia de posición integrada, monótona). Sin cambio de conducta (nadie consume hoy; heading bueno = BNO).
  ⚠️ **Validar en banco con 2 OTOS** (signo/eje, `OTOS_SEPARATION_MM` real).
- **Gate:** host **354 tests / 29 envs / 0 fallos**; compilan `down`, `central_robot1/2`, `top_robot1/2`.
  Los cambios de conducta (arquero) y los de OTOS están **gateados/sin consumidor** → no rompen lo de hoy.

### Avance 2026-06-03 — Visión TASK-022: velocidad de pelota (firmware) + kit de calibración
- **Pieza A — estimador de velocidad de la pelota (DONE, host-testeado).** `build_snapshot()`
  llenaba `ball_x/y/confidence` pero dejaba `ball_vx/vy` en **0** → toda la cadena
  `bt_classify` (ball_trajectory, 7 tests) estaba dormida (siempre `BT_STILL`). Nuevo
  módulo puro `src/shared/ball_velocity.{h,cpp}`: deriva velocidad (mm/s, marco robot) por
  diferencias finitas sobre la posición fusionada, **sólo al llegar packet nuevo** (sample_ms),
  EMA (α=0.4), **reset al perder la pelota** (descarta el 1er frame al reaparecer), re-siembra
  si el gap > 200 ms, getter con clamp int16. Cableado en `cameras_runtime` (estado +
  `cameras_get_ball_vx/vy_mm_s`) + 2 líneas en `main_top.cpp::build_snapshot`.
- **Verificación:** TDD (13 tests nuevos `test_ball_velocity`, RED→GREEN). Suite host
  completa **324 tests / 26 envs / 0 fallos** (`run-host-tests.sh`). Compilan **top_robot1,
  top_robot2 y diag_top_all** (SUCCESS). El cableado de runtime es glue Arduino (compile-only,
  como el resto del runtime); la lógica real está 100% bajo test.
- **Pieza B — kit de calibración (para banco/Incheon).** `calib-lab-n6.py` (standalone, en
  ambos packs de cámara): muestra una sonda central, imprime el LAB real del objeto + un tuple
  sugerido y dibuja los blobs que agarra el threshold actual. NO toca el script de competencia.
  Procedimiento de 1 página: [`docs/firmware/CALIBRACION-VISION-N6.md`](firmware/CALIBRACION-VISION-N6.md).
- **Pendiente humano (Virginia):** la calibración de banco en sí (LAB + UART + exposición + H)
  — Claude no cierra TASK-022 (es hardware). ⚠️ El `calib-lab-n6.py` usa API OpenMV estándar
  (`get_statistics`/`draw_*`) pero **no pude probarlo en la N6**: confirmar en banco.

### Resuelto 2026-05-24
- ~~**DOWN — pinout Teensy↔mux NO confirmado**~~ → **VALIDADO EMPÍRICAMENTE.**
  Gustavo + Claude (ejecución directa) aplicaron el mapeo del doc canónico
  (`hardware/electronics/down-board-pack/01-pinout-y-posiciones.md`) al
  firmware (config_down.h + line_ring.cpp). Verdict del diag_capture: 0
  muertos, los 32 sensores responden. TASK-026 bajó de P0 a P2. Ver
  `journal/2026-05-24-hardware-up-down-anillo-linea.md`.

- ~~**DOWN OTOS — lib SparkFun en stub**~~ → **LIB ACTIVADA + OTOS RESPONDEN**.
  Misma sesión 2026-05-24. `src/down/otos.cpp` reescrito con API real
  (`getPosition`, no `getPose`; mismo tipo para position y velocity).
  Ambos chips U5 y U6 responden I²C en 0x17, pose se actualiza con
  movimiento. TASK-012 bajó de P0 a P1 (queda parte ToF en stub).
  Validación cuantitativa pendiente: TASK-029.

  > ⚠️ **CÓMO ENCENDER LOS OTOS — leer ANTES de debuggear si dan `L=-- R=--`.**
  > Los OTOS se alimentan del **3.3 V del MP1584, que viene de la BATERÍA**
  > (el USB **NO** los alimenta — el USB solo da el Teensy). Si ves `L=-- R=--`,
  > bus I²C vacío, o una dirección rara tipo **`0x64`** (eso es **brownout**, no
  > es otro chip), **NO es firmware — es alimentación.** Receta:
  > 1. Batería **cargada y entregando corriente de verdad** (switch ON, Dean XP1
  >    bien puesto). *"Conectada pero sin pasar corriente" = OTOS muertos.*
  > 2. **Power cycle completo**: desconectar batería **+** USB, esperar 10 s,
  >    reconectar.
  > 3. Verificar el scan I²C del arranque → ambos OTOS deben dar **`0x17`**.
  >
  > Confirmado 2026-05-24 **y otra vez 2026-05-29** (TASK-028). Detalle: los 32
  > sensores de luz pueden seguir leyendo con el riel flojo, pero los OTOS no.
  > Ver `journal/2026-05-29-otos-revividos-power-bateria.md` y
  > `journal/2026-05-24-otos-lib-activada-y-power-cycle-bug.md`.

- ~~**TOP VL53L7CX frontal en stub TODO_TOF_LIB**~~ → **VL53L7CX U2 FRONTAL VIVO**.
  Misma sesión 2026-05-24. Debug de 3 horas: 3 libs ST (L5/L7/L8) fallaron
  todas en init. Bug raiz identificado en `STM32duino_VL53L7CX/src/vl53l7cx_platform.h:49-60`
  (`DEFAULT_I2C_BUFFER_LEN = BUFFER_LENGTH` desborda en 2 bytes el buffer
  de `Wire` en Teensy 4.0 al cargar el firmware blob). Lib Adafruit_VL53L7CX
  funciona out of the box. `src/top/sensors_tof.cpp` migrado del stub a
  Adafruit (solo U2 instalado fisicamente; U3/U5/U17 quedan retornando
  `TOF_NO_READING`). Nuevo `[env:diag_sensors_tof_live]` permite probar el
  modulo migrado en aislamiento en banco. Libs ST marcadas DEPRECATED en
  sus READMEs. Ver `journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md`.

### Resuelto 2026-05-27
- 2026-05-27: Sprint 1 localizacion (trilateracion geometrica) implementado.
  Modulo localization.cpp puro + tests host-native 14 PASS + integrado a
  main_top.cpp (pose llega al WorldSnapshot v2). Validacion HW pendiente
  (TASK-035, bloqueada por bodge XSHUT TASK-033).

### Avance 2026-05-28 — diag_central_motors (CENTRAL, banco)
- Nuevo sketch standalone para validar los 3 H-bridges del Zircon Rev v15
  uno a la vez, con onda PWM controlada por botón (pin 9). Sirve para
  mapear "motor N firmware → rueda física" y para **resolver
  empíricamente el conflicto pines 7/8** (motor 2 vs Serial2 hacia DOWN):
  si el motor 2 del driver U17 NO gira → pines 7/8 son Serial2; si gira →
  son motor y hay que migrar Serial2 a otro UART (Serial7 28/29 libre).
- Archivos: [`src/diag/diag_central_motors.cpp`](../software/teensy/Soccer%202026/src/diag/diag_central_motors.cpp)
  + `[env:diag_central_motors]` en `platformio.ini` + doc operativo
  [`docs/firmware/DIAG-CENTRAL-MOTORS.md`](firmware/DIAG-CENTRAL-MOTORS.md).
- Sesión Claude: scope acotado a producir el sketch + docs (excepción
  explícita a la moratoria aprobada por Gustavo, alineada con el espíritu
  de "desbloqueo de hardware"). El brainstorm de "desarrollo completo de
  software CENTRAL" quedó pausado para una próxima sesión, post-ensayo
  del diag_central_motors en banco.
- **Pendiente humano**: Virginia/Elías/Enzo correr el test en banco
  (procedimiento en el doc). Cierra empíricamente el conflicto pines 7/8.

### Resuelto 2026-05-29
- HAL Sprint A (TOP): 4 archivos nuevos (hardware_profile.h, pinout_common.h,
  pinout_robot1.h, pinout_robot2.h) + config_top.h migrado a wrapper +
  envs por robot en platformio.ini. Backwards compat preservada (Sprint 1
  localización intacto). 14/14 tests host-native siguen pasando. Pendiente:
  TASK-038 (pines XSHUT reales) para arrancar Sprint B.

### Avance 2026-05-29 — DOWN robustez (P0.2 + P1.5 + P1.6 del audit)
- Implementados en firmware los 3 hallazgos in-scope del audit
  `research/in-progress/2026-05-29-auditoria-exhaustiva-placa-down.md`:
  - **P0.2 — persistencia de calib en EEPROM**: `comm_central_load_persisted_calib()`
    (nueva, en `comm_central.cpp`) carga la calib al boot y bloquea el lazy-init;
    el SAVE se dispara al completar el paso "blanco" del comando `CENTRAL_CALIB_LINE`.
    La calib sobrevive al power cycle (objetivo: no perder calib entre matches).
  - **P1.5 — rechazo de saturación "todo blanco"**: `lf_all_white()` (nuevo, puro)
    + uso en `dm_update` (invalida sensores, saltea adaptación, marca `data_valid=0`,
    señaliza por `EV_CALIB_SUSPECT`). Mitiga luz extrema (Modo 5 del audit).
  - **P1.6 — backpressure UART**: guard `availableForWrite()` en `comm_central.cpp`
    (Serial1) **y** `comm_top.cpp` (Serial5) + contadores `..._get_frames_dropped()`.
    Evita que `Serial.write()` con buffer lleno robe ciclos al `line_ring` de 1 kHz.
- **Verificación host**: `pio run -e down` compila y linkea OK (FLASH 33 KB).
  Tests host-native: `test_line_filters` 39/39, `test_down_model` 7/7 (g++ fallback,
  TASK-025). Los cambios Arduino-only (comm_*, main_down) son **compile-only**.
- **Pendiente humano**: validación en hardware real → **TASK-301** (3 criterios:
  power-cycle calib, all-white con luz real, frames_dropped bajo carga). Claude NO
  cierra tasks de hardware (regla 1 CLAUDE.md).
- **Nota infra (RESUELTO 2026-05-29 — TASK-302)**: `[env:down]`/`[env:diag_down]`
  eran los únicos firmware que NO compilaban offline. Causa: al activar OTOS
  (TASK-012, 2026-05-24) quedó como única lib de firmware sin vendorear, con
  `lib_deps` de registry → build roto bajo Avast (TASK-025). **Solución**: OTOS +
  `SparkFun_Toolkit` vendoreadas en `lib/` (podadas, sin blobs) y `lib_deps`
  quitado de ambos envs. Verificado en esta máquina (Avast): borrando
  `.pio/libdeps/down` y con `lib_deps` vacío, `pio run -t clean -e down` y
  `-e diag_down` dan **SUCCESS 100% offline** (FLASH 33416 / 21960 B); `.pio/libdeps/down`
  NO se recreó (cero registry). Las 4 placas vuelven a compilar sin red.
  TASK-302 cerrada (build-verificada, no es HW).

### Avance 2026-05-29 — DOWN↔CENTRAL bring-up (hallazgos verificados + tooling down_debug)
- Sesión con María (banco, sin placa TOP). Verificado en código:
  - **DOWN→CENTRAL (Serial1→Serial2) lleva SOLO la línea** (`LineStatusV2`), no OTOS;
    los OTOS van DOWN→TOP (`main_down.cpp:101/107`). ⚠️ **SUPERSEDED 2026-06-01
    (broadcast simétrico Capa 1):** DOWN ahora difunde línea **+** OTOS a **ambas**
    placas (CENTRAL `Serial1` + TOP `Serial5`) vía `down_tx`; CENTRAL ingiere el OTOS
    directo. Ver la sección «DOWN» de Módulos VIVOS arriba.
  - **El "ir derecho" del sketch de manejo usa heading IMU/TOF, no OTOS**:
    `main_top.cpp::build_snapshot` toma `localization_runtime_get_pose()` (BNO+TOF);
    el pose OTOS llega al TOP pero NO entra al snapshot. "Manejar con OTOS" no está
    cableado (post-Incheon, `TODO_DIFFERENTIAL_OTOS`).
  - **Conflicto pines 7/8 BLOQUEA DOWN→CENTRAL**: Serial2 (UART desde DOWN) = pines
    7/8 = mismos de un motor (`config_central.h`); `motors_init()` los pone OUTPUT
    (`motors_zircon.cpp:101-105`) y pisa el UART. Todo firmware CENTRAL llama
    `motors_init()` → **ninguno puede recibir a DOWN** → **TASK-036**.
- **Tooling**: `[env:down_debug]` (= `[env:down]` + `-DDOWN_DEBUG_SERIAL`) imprime por
  USB, 4 Hz, lo que DOWN manda por Serial1 (data_valid, line_present, ángulo,
  imm_exit, flags, tx_ok/tx_drop). Valida la transmisión DOWN **sin** placa CENTRAL.
  `[env:down]` competencia queda IDÉNTICO (FLASH 33416). HW pendiente (María).

### Avance 2026-05-29 — diag_central_drive_straight (CENTRAL + TOP, banco)
- Nuevo sketch que valida end-to-end la cadena de control de movimiento:
  `WorldSnapshot (Serial1 desde TOP) → world_model → HeadingPID →
  kinematics inversa omni-3 → motors_zircon`. Botón pin 9 controla
  FORWARD 3 s → PAUSED 1 s → REVERSE 3 s → DONE.
- Archivos: [`src/diag/diag_central_drive_straight.cpp`](../software/teensy/Soccer%202026/src/diag/diag_central_drive_straight.cpp)
  + envs `[env:diag_central_drive_robot1]` / `[env:diag_central_drive_robot2]`
  en `platformio.ini` + doc operativo
  [`docs/firmware/DIAG-CENTRAL-DRIVE.md`](firmware/DIAG-CENTRAL-DRIVE.md).
- **Diferencia con la propuesta original del usuario**: pidió "PID
  diferencial por diferencia de las 2 OTOS". CENTRAL hoy recibe SOLO la
  pose fusionada del TOP, no las 2 OTOS crudas — para el diferencial hay
  que ampliar el contrato a v3 o agregar bypass DOWN→CENTRAL (ambos
  post-Incheon). Este sketch usa HeadingPID clásico con `my_heading_centideg`
  del WorldSnapshot, que para "ir derecho" es suficiente.
- **Pendiente humano**: TASK-037 — Virginia/Elías/Enzo correr el test en
  banco con cadena TOP→CENTRAL operativa. Pre-requisito: TASK-036 cerrada
  (motores validados) + TOP con firmware mandando snapshots.

### Avance 2026-05-29 — fix P0 contrato de línea DOWN→CENTRAL (firmware)
- `comm_down.cpp` decodificaba `LineStatus` viejo (5 B) y descartaba **todos**
  los frames de DOWN, que manda `LineStatusV2` (16 B): `payload_len==5` daba
  false para los 16 B reales. CENTRAL quedaba **ciego a la línea** (sin frenado
  de borde, sin datos para el GK, `LINE_AVOID` sin entrada). **Corregido**:
  nuevo `src/shared/line_view.h` (helpers puros) + `comm_down`/`world_model`
  migrados a `LineStatusV2`. `strategy.cpp` SIN cambios (firmas de accessors
  intactas). Ahora `world_model_imminent_exit()` filtra `lifted` (honra el
  contrato de `strategy.cpp:17`, que el código viejo ignoraba).
- Verificación: harness g++ offline 8/8 PASS (chain real encode→decode→interpret)
  + `-fsyntax-only` limpio en los 2 archivos de `src/central`. El test Unity
  `test/test_central_line_ingest` quedó escrito pero `pio test -e test_native`
  NO corrió acá (sandbox sin red → no baja Unity; `test_down_encode` también
  ERRORÓ igual). El equipo lo corre con red para el verde oficial.
- **Pendiente humano**: TASK-100 — validar ingest + frenado en banco
  (blocked_by TASK-036, por el conflicto pines 7/8 / Serial2).
- Journal: `journal/2026-05-29-fix-contrato-linea-central.md`.

### Avance 2026-05-30 — TOP: ✅ los 4 ToF enumeran en bus único (bodge LP OK)
- Enzo recableó la placa TOP: los **4 ToF al bus principal `Wire` (18/19)** +
  bodge de la pata **LP** de cada ToF a un pin del Teensy (reusando la traza de
  INT), para **liberar el bus `Wire2` (24/25)** del TOP (corrección 2026-06-09: el bus de
  los pines 24/25 es `Wire2`/LPI2C4, no `Wire1`; es donde va el 2º BNO — TASK-207).
- Diagnóstico de banco (5 sketches nuevos: `diag_top_i2c_scan`,
  `diag_top_tof_lp_discover`, `diag_top_tof_enumerate`, `diag_top_tof_census`,
  `diag_top_tof_quad_live`). **Veredicto (tras power-cycle): los 4 LP
  funcionan** → pines **9,10,11,12 activo-ALTO**, enumerados a
  0x2A/0x2B/0x2C/0x2D. ✅
- **Lección clave**: las direcciones I2C de los VL53L7CX **persisten entre
  resets**; hay que **power-ciclar** (cortar/reponer energía) tras flashear o el
  bus arranca sucio. Un primer diagnóstico sin power-cycle dio un FALSO
  NEGATIVO ("ningún LP funciona", commit `096108a`) que quedó refutado y
  corregido.
- **Habilita**: bus `Wire2` (24/25) del TOP libre (corrección 2026-06-09: 24/25 = `Wire2`,
  no `Wire1`; bus del 2º BNO — TASK-207) + **localización 2D por trilateración**
  con 4 ToF reales.
- **TASK-201** (multímetro de continuidad LP) **degradada**: ya confirmado por
  banco, queda opcional.
- **Pendiente firmware (HAL Sprint B)**: correr `diag_top_tof_quad_live` para
  confirmar ranging + mapear dirección→posición (tapar cada sensor), luego
  actualizar `pinout_robot1.h` (`PIN_TOF_XSHUT={9,10,11,12}`, `NUM_TOF_ACTIVE=4`,
  flags) y extender `sensors_tof.cpp` para enumerar los 4 al boot. Ver
  `journal/2026-05-30-top-tof-4-en-bus-unico-enumeracion-ok.md`.

### Avance 2026-05-29 — auditoria independiente TOP pre-Incheon + 3 fixes (TOP)
- **Suite host-native corrida punta a punta por primera vez**: 246 tests /
  19 envs / **0 fallos**, 100% offline via nuevo `scripts/run-host-tests.sh`
  (saltea PlatformIO + el registry que Avast bloqueaba). Esto destraba la
  verificacion que TASK-025 mantenia bloqueada.
- **3 fixes de firmware aplicados** (compilan limpio ambos robots, sin warnings):
  1. **`main_top.cpp` — heading al CENTRAL ahora viene del IMU.** Antes
     `build_snapshot()` mandaba `pose.heading_centideg` de localization, que
     es SIEMPRE 0 con el hardware actual (TOFs solo en eje Y, pose nunca
     `valid`, heading nunca se escribe). CENTRAL navegaba con heading=0 fijo.
     Ahora `s.my_heading_centideg = sensors_imu_get_heading_centideg()`
     (los 2 BNO055 si dan orientacion). **Mejora la validez de TASK-037**
     (diag_central_drive usa ese heading).
  2. **`sensors_tof.cpp` — HC-SR04 gateado tras `#ifdef TOP_ENABLE_HCSR04`
     (OFF por default).** El `pulseIn(PIN_HCSR04_ECHO=7, ..., 25000UL)` corria
     sobre el pin 7 = Serial2 RX2 (uplink WorldSnapshot), causando stall de
     25 ms + `min_obstacle_mm` basura. Resuelve el lado firmware de TASK-014.
     El VL53L7CX frontal U2 cubre la distancia frontal de forma redundante.
  3. **`comm_down.cpp` — removida funcion muerta `send_empty()`** (sin
     callers, generaba warning).
- **Pendiente humano**: **TASK-200** — validar en hardware que (a) el heading
  llega al CENTRAL y el robot orienta, y (b) el loop ya no se cuelga 25 ms.
  Ver `journal/2026-05-29-auditoria-top-pre-incheon-top.md` (auditoria completa
  + temas-a-analizar en formato coach para los hallazgos dependientes de HW).

### Avance 2026-05-29 — corrección UART TOP→CENTRAL = Serial5 (no Serial2/7-8)
> ⚠️ **SUPERSEDED 2026-05-31 (TASK-204):** la cámara trasera quedó **soldada en Serial5
> (pin 21)** (confirmado en banco con `diag_top_cameras`), así que **TOP→CENTRAL se movió
> a Serial7 (pin 29)** y la trasera se lee en Serial5. Además el **HC-SR04 quedó en pines
> 4/3** (TRIG/ECHO), no 6/7. **En la CENTRAL** los UART se reasignaron: recibe el snapshot
> en **Serial7 (pin 28)** y la línea del DOWN en **Serial1 (pin 0)** → **Serial2 (7/8) libre
> para el motor 2; conflicto 7/8 (TASK-036) RESUELTO**. Lo de abajo es el registro histórico
> del 2026-05-29 (donde diga "conflicto 7/8 abierto" o "Serial2 → CENTRAL", está superado).
- **Hallazgo (Gustavo, en banco):** el diagrama del Teensy tiene doble numeración
  (externa + interna); vale la **interna (GPIO)**. El conector del TOP hacia CENTRAL
  cae en los **pines 20/21 = Serial5**, NO en 7/8 (Serial2). Era mala lectura del diagrama.
- **Impacto:** (1) destraba el riesgo G-TOP-01 ("Serial2→CENTRAL NO CONFIRMADO →
  el snapshot nunca llega"); (2) **resuelve el conflicto del pin 7 del TOP** (HC-SR04
  ECHO vs Serial2 RX2): Serial2 ya no se usa. ⚠️ **NO confundir con el conflicto 7/8
  del CENTRAL** (Motor U17 vs Serial2 hacia DOWN) — ese sigue ABIERTO (lo resuelve
  `diag_central_motors` en banco).
- **Firmware corregido (compila OK, top_robot1 SUCCESS):** `src/top/comm_central.cpp`
  → `Serial5`; cámara trasera movida de Serial5 a **Serial7** (28/29) en `cameras_runtime.cpp`.
- **Docs actualizados:** `top-board-pack/01`, `CONTRATO-DATOS-TOP`, `CONTRATO-DATOS-CAMARAS`,
  `ARQUITECTURA-3-PLACAS`, `FIRMWARE-PLACA-ARRIBA`, `cameraBack-pack`, `FUENTES-DE-VERDAD`.
  De paso se corrigió un RX/TX cruzado de Serial5 (20=TX5, 21=RX5) en `config_down.h` + ARQUITECTURA.
- **Pendiente humano:** confirmar con Enzo a qué pines del Teensy llega el conector
  **U9** (cámara trasera, hoy provisional en Serial7).

### 🏁 HITO 2026-05-29 — TOP ARMADA: las 3 placas físicas existen (robot casi completo)
- Se terminó de armar la **placa/carcaza TOP**. Por primera vez las **3 placas están
  físicamente montadas** (CENTRAL + DOWN ya estaban). Robot casi completo a nivel
  mecánico/electrónico.
- **Falta para cerrar la integración**: las 2 conexiones inter-placa hacia TOP —
  (1) **DOWN↔TOP** (DOWN Serial5 → TOP Serial1: odometría OTOS + LINE_STATUS),
  (2) **CENTRAL↔TOP** (TOP Serial7 pin 29 → CENTRAL Serial7 pin 28: WorldSnapshot). Firmware listo
  en ambas puntas; falta cablear + validar el stream por protocolo.
- **Se DESTRABAN** (ya no bloqueadas por "TOP sin armar"): TASK-022 (cámara operativa),
  TASK-024 (rol/polaridad), TASK-032 (ToF U2 en HW), TASK-035 (localización),
  TASK-200 (heading IMU→CENTRAL + loop), TASK-037 (drive-straight), TASK-003 (remap del bus de 24/25 en TOP — ese bus es `Wire2`/LPI2C4, no `Wire1`; corrección 2026-06-09, ver TASK-207).

> ✅ SUPERADO (2026-06-08): el sentido de los 3 motores ROBOT1 ya está validado (MOTOR_INVERT={+1,-1,+1}, M2/U17 invertido, banco 2026-06-01 re-confirmado 2026-06-06) y el conflicto 7/8 está resuelto (2026-05-31). La GEOMETRÍA quedó CALIBRADA 2026-06-08: WHEEL_ANGLES_DEG={330,210,90} (M1=del-IZQ · M2=del-DER · M3=trasera) + piso de PWM POR RUEDA MOTOR_MIN_PWM={70,70,42}. Fila canónica: FUENTES-DE-VERDAD.md:38. Tabla de disposición: docs/firmware/DIAG-CENTRAL-MOTORS.md. Lo único de banco que queda es el TUNEO FINO del lateral (que no rote) + confirmar el SENTIDO de la traslación, y ROBOT2.
> ⚠️ Actualización 2026-06-09: el `{70,70,42}` de arriba quedó SUPERADO como valor final → `MOTOR_MIN_PWM={70,70,107}` + impulso inicial `{130,130,140}`×40 ms + freno anticipado trasera 66 ms (banco R2; R1 mismos valores A VERIFICAR). ROBOT2 ya quedó validado (pines NO rotados, MOTOR_INVERT={+1,+1,+1}). Ver «Avance 2026-06-09».
> ⚠️ Actualización 2026-06-11: en la reparación de R1 el M2/U17 quedó **RECABLEADO DERECHO** → `MOTOR_INVERT={+1,+1,+1}` en AMBOS robots (`8d5fc90`, validado en piso); pisos {70,70,107} + kickstart de R1 también validados en piso. El `{+1,-1,+1}` de arriba era la compensación del cableado PRE-reparación (si se recablea como estaba, volver al `-1`).

### 🏁 Avance 2026-05-29 — BANCO: motores CENTRAL + enlace físico DOWN↔CENTRAL
- **Motores del CENTRAL andan** (`diag_central_motors` en banco): identificados
  motor 1/2/3 + definida la orientación (horario/antihorario) de cada uno. Los
  valores específicos (mapeo motor↔rueda, sentidos) y el **veredicto del conflicto
  7/8** (¿giró el motor del driver U17 en pines 7/8?) → cargar de las notas de banco.
- **Enlace físico DOWN→CENTRAL validado** (test mínimo "mandar un 1":
  `diag_down_send1`/`diag_central_recv1`): el cable + UART transmiten (DOWN Serial1
  TX1 pin 1 → CENTRAL Serial2 RX2 pin 7 + GND, 230400). **Falta:** la lectura por
  **protocolo** end-to-end. La herramienta ya existe: `diag_central_comm_down`
  decodifica `LineStatusV2` (proto.h `FrameDecoder` + `line_view.h`, campo por
  campo) en **Serial7**. Falta que DOWN emita frames reales + decidir el Serial
  definitivo del link en CENTRAL (Serial2 7/8 vs Serial7, según veredicto 7/8).
- Tools en ramas `agente/central` (diag_central_comm_down + motors antirebote) y
  `agente/down` (send1/recv1) — pendiente decidir merge a `main`.
- Detalle completo: `journal/2026-05-29-sesion-banco-motores-y-enlace-down-central.md`.

### Resuelto 2026-05-25 → SUPERADO por bodge 2026-05-30
- **Verificado forensicamente que los XSHUT/LPn de los 4 TOFs NO estaban
  ruteados en TOP rev 1.0 de fábrica** (NC flags explícitos en SCH, 0 nets en
  PCB netlist). Implicancia de entonces: máximo 2 ToFs sin rework.
  ⚠️ **SUPERADO el 2026-05-30**: el bodge manual de Enzo cableó los 4 LP a
  pines del Teensy {9,10,11,12} y movió los 4 ToF a `Wire` — los 4 enumeran
  (confirmado en banco). Ver el "Avance 2026-05-30" arriba. El wishlist de TOP
  rev 1.1 (post-Incheon) sigue válido (rutear el XSHUT en el PCB elimina el
  bodge frágil). Ver `journal/2026-05-30-top-tof-4-en-bus-unico-enumeracion-ok.md`.

### Deudas conocidas (resumen — la canónica está en `FUENTES-DE-VERDAD.md`)

- ~~**TOP rev 1.0 — XSHUT/LPn de los 4 slots ToF no ruteados**~~ → **RESUELTO
  por bodge de Enzo (2026-05-30)**: los 4 ToF enumeran en `Wire` (LP pines
  {9,10,11,12}). TASK-033 (cuántos ToF) decidida por los hechos: 4. Solución de
  fondo (rutear en PCB) queda para TOP rev 1.1 post-Incheon.
- ~~**HAL Sprint B**: extender `sensors_tof.cpp` a los 4 ToF~~ → ✅ **RESUELTO**:
  enumeración de los 4 activa por default desde 2026-06-01 (`TOP_ENABLE_MULTI_TOF`)
  y lectura validada en banco 2026-06-10 (round-robin `a6c0366` + payload recortado
  `bf8ddd4`). Queda solo: mapear dirección→posición física tapando cada sensor.
- **Conflicto pin 10**: el bodge usa el pin 10 como LP de ToF, pero ese pin era
  el dipswitch de rol → reubicar la lectura de rol (anotado en pinout_robot1.h).
- HAL para CENTRAL (replicar el patrón en src/central/config_central.h).
  Sprint futuro.

### 🏁 HITO 2026-05-24 — Subsistema DOWN/BOTTOM operacional en banco + OTOS validado cuantitativamente
La placa DOWN (también llamada "BOTTOM") pasó tests de banco con éxito:
anillo de 32 sensores leyendo + 2 OTOS reportando pose con precisión
cuantitativa validada (280.4 mm sobre 300 mm reales = 6.5% error, pasa
tolerancia 8% de TASK-029).

**Cerradas hoy:**
- ✅ **TASK-030**: lámina protectora sacada (en la misma sesión, sin
  esperar tapa).
- ✅ **TASK-029**: validación cuantitativa OTOS confirmada sobre cartón
  corrugado (300 mm reales → 280 mm reportados).

**Sigue pendiente:**
- **TASK-031**: verificar comunicación UART real DOWN→TOP (Serial5) y
  DOWN→CENTRAL (Serial1). Requiere las otras placas disponibles.

Ver journal `journal/2026-05-24-down-board-passing-tests-cierre.md`
(con sección "Test final post-lámina" agregada al final).

## Regla operativa (CLAUDE.md actualizado 2026-05-19)

- Claude **planifica, documenta y programa firmware host-testeable**.
- Claude **NO cierra TASKs de hardware** — eso solo lo puede hacer el equipo humano que tiene la placa en la mano.
- Moratoria temporal de nuevos docs/specs/plans hasta primera hardware-up (robot encendido + COMM flasheada + DOWN reportando línea por UART real).

## Cómo actualizar esta página

Modificarla en cada commit que cambie:
- Qué módulo es VIVO (entra/sale del binario)
- Qué TASK es bloqueante (cambia prioridad o se cierra)
- Qué deuda apareció o se resolvió

Sin actualización = sesión inválida.
