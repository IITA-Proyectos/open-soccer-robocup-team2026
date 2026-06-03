---
title: "Estado actual del robot — vivo, 1 página"
date: 2026-05-29
last-updated-by: "Claude (sesión 2026-06-02 — DOWN broadcast Capa 2+3 code-complete: drive-straight ATK + arquero por cross_track, fallback exacto)"
status: vivo
tipo: indice-operacional
---

# Estado actual — Snapshot del repo (al 2026-05-29)

> **Toda sesión Claude que toca este repo: ESTA es la primera lectura
> obligatoria** (después de `git pull`). Si lo que estás por hacer contradice
> algo de acá, **parar y consultar al humano**. Si lo que vas a hacer hace
> cambiar algo de acá, **actualizá esta página en el mismo commit.**

> **🔧 ÚLTIMO (2026-06-02 — vale sobre cualquier mención más abajo):** mapa UART final.
> **TOP (Teensy 4.0):** S1←DOWN · **S2 (7/8)↔COMM** · S3←cam frontal · **S4 (16/17)→CENTRAL** · S5←cam trasera.
> ⚠️ **El 4.0 NO expone S7 (28/29) en el borde** (back-pads) → el enlace a CENTRAL va por **S4**, no S7 (fix 2026-06-02: antes estaba en S7 y el TOP nunca le llegaba a la CENTRAL).
> **CENTRAL (Teensy 4.1):** **S7 (pin 28)←TOP** (el cable sale del TOP pin 17/TX4) · **S1 (pin 0)←DOWN** · **pines 7/8 LIBRES para el motor 2** → **conflicto 7/8 (TASK-036) RESUELTO**. HC-SR04 en pines 4/3.
> CENTRAL **sin BNO** (los 2 BNO están en el TOP). **Los 4 ToF activos por default** en
> top_robot1/2 (`TOP_ENABLE_MULTI_TOF`; **I²C 100 kHz** — el BNO055 + ToF NO coexisten a 400 kHz, boot ~40 s). **`Zircon.pdf`** (esquemático
> del Zircon/CENTRAL, fuente Robomov) ya está en `hardware/electronics/`. Detalle único del
> cableado: `hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md`. Cualquier "conflicto 7/8
> abierto" o "Serial2 → CENTRAL" más abajo está **superado**.

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
- `src/central/motors_zircon.{h,cpp}` — PWM 3 motores omni (sin kicker físico: el robot empuja la pelota por inercia)
- `src/central/imu_zircon.{h,cpp}` — BNO055 (⚠️ ya NO se conecta en CENTRAL desde 2026-05-31; compat gateado por `-DCENTRAL_HAS_LOCAL_BNO`, off; el heading viene de ARRIBA)
- `src/central/world_model.{h,cpp}` — espejo del WorldSnapshot
- `src/central/comm_top.{h,cpp}` — recibe WorldSnapshot del TOP por **`Serial7` (RX7 = pin 28)** (reasignado 2026-05-31: antes Serial1, se movió a Serial7 cuando el link a DOWN tomó Serial1)
- `src/central/comm_down.{h,cpp}` — recibe LineStatusV2 + OTOS (Pose2D/Velocity2D) del DOWN por **`Serial1` (pin 0)**. ✅ Conflicto 7/8 **RESUELTO** (2026-05-31: UART movido a Serial1; los pines 7/8 quedan para el motor 2). Receiver de banco: `diag_central_comm_down` ([doc](firmware/DIAG-CENTRAL-COMM-DOWN.md)). 📊 **Análisis profundo del link** (protocolo/CRC, buffers, timing, recuperación ante cortes, P0/P1 + checklist "primera instancia") → [`docs/firmware/ANALISIS-COMM-DOWN-CENTRAL-2026-05-31.md`](firmware/ANALISIS-COMM-DOWN-CENTRAL-2026-05-31.md).

- **Diags de banco (CENTRAL, `src/diag/`):** `diag_central_motors` (motores + `MOTOR_DIR`), **`diag_central_strafe`** (patrulla lateral del arquero — **open-loop**, omega=0, sin BNO en CENTRAL; ahora que el OTOS llega a CENTRAL por broadcast se le puede sumar heading-hold = v2 — [doc](firmware/DIAG-CENTRAL-STRAFE.md)), `diag_central_drive_straight` (+Y con heading del TOP), `diag_central_comm_down` (link DOWN→CENTRAL), `diag_central_rx_all` (decodifica DOWN+TOP juntos).

### TOP (Teensy 4.0)
- `src/top/main_top.cpp` + `cameras_runtime`, `cameras`, `sensors_imu`, `sensors_tof` (4 ToF VL53L7CX en bus único `Wire`, lib `Adafruit_VL53L7CX`. ✅ Bodge de Enzo 2026-05-30: los 4 ToF con LP en pines {9,10,11,12} (activo-alto), enumeran a 0x2A..0x2D, confirmado en banco. `Wire1` liberado para DOWN. ⚠️ Probar ToF SIEMPRE con power-cycle (las direcciones I²C persisten). El firmware vivo `sensors_tof.cpp` todavía lee 1 ToF — extender a los 4 es HAL Sprint B. Plan: escalar a 6 ToF (4 fijos + 2 móviles para pelota). Ver journal 2026-05-30), `comm_*`
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

> ⚠️ La tabla de arriba es snapshot 2026-05-29. **Número vivo (2026-06-03):
> 324 tests / 26 envs / 0 fallos** vía `scripts/run-host-tests.sh` (la tabla no
> incluye los tests sumados después: broadcast, drive_straight, imu_fusion,
> tof_zone_orient, otos_ingest, gk_cross_track, ball_velocity).

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

1. **COMM — firmware flasheado ✅ (2026-06-01); E2E del árbitro RESUELTO ✅ (2026-06-02, TASK-039).** El árbitro RCJ **NO viaja por UART**: señaliza como **NIVEL GPIO** hacia el TOP (Teensy 4.0) en **pin 5 = OUT1 (PLAY/STOP)** y **pin 6 = OUT2 (PLAY/STOP)** (en la práctica, en PLAY sube SOLO UNO de los dos —no son espejo—). Nivel: **0 = juego PARADO, 1 = juego EN CURSO (3.3 V)**. Firmware: `src/top/comm_arbiter.cpp::read_referee_gpio()` lee los pines 5/6 con `INPUT_PULLDOWN` y `match_running = (pin5 OR pin6)` (en PLAY sube SOLO UNO de los dos pines —el otro queda en 0— por eso AND nunca daba GO y OR sí; probado en banco 2026-06-02, Gustavo. Sigue siendo fail-safe: si se desconecta el cable del COMM, ambos pines leen 0 con `INPUT_PULLDOWN` → `match_running=false` → STOP). El probe temporal se removió de `main_top.cpp`. El **UART del módulo COMM (TOP `Serial2`, pines 7/8) queda SOLO para partner ESP-NOW / status** — el viejo `COMM_REFEREE_CMD` por UART quedó **obsoleto**. (fix 2026-06-02 / TASK-039: el árbitro es NIVEL GPIO en pines 5/6 del TOP, no UART). El robot ya recibe START/STOP por GPIO → homologa el árbitro. TASK-006/TASK-039.
2. **Cámaras sin recalibrar para iluminación Incheon** → no ve la pelota. TASK-022.
   La migración H7→N6 y los bugs P0 ya están resueltos; **lo único que falta es
   calibración de banco** (LAB + UART + exposición + H). Kit + procedimiento listos
   (2026-06-03): `calib-lab-n6.py` en ambos packs + [`docs/firmware/CALIBRACION-VISION-N6.md`](firmware/CALIBRACION-VISION-N6.md).
   El item #6 del análisis (velocidad de pelota en el TOP) **ya está hecho** (ver Avance 2026-06-03).

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
  INT), para **liberar `Wire1` (24/25)** hacia la placa DOWN.
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
- **Habilita**: `Wire1` libre para DOWN + **localización 2D por trilateración**
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
  TASK-200 (heading IMU→CENTRAL + loop), TASK-037 (drive-straight), TASK-003 (Wire1 remap).

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
- **HAL Sprint B**: extender `sensors_tof.cpp` para enumerar los 4 ToF al boot
  (hoy lee 1). Ya NO bloqueado por TASK-038 (pines confirmados en banco). Falta:
  el código de enumeración + confirmar ranging (`diag_top_tof_quad_live`) +
  mapear dirección→posición física.
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
