---
title: "Estado actual del robot — vivo, 1 página"
date: 2026-05-29
last-updated-by: "Claude (coach, sesión 2026-05-29 — sync de frontmatter/calendario)"
status: vivo
tipo: indice-operacional
---

# Estado actual — Snapshot del repo (al 2026-05-29)

> **Toda sesión Claude que toca este repo: ESTA es la primera lectura
> obligatoria** (después de `git pull`). Si lo que estás por hacer contradice
> algo de acá, **parar y consultar al humano**. Si lo que vas a hacer hace
> cambiar algo de acá, **actualizá esta página en el mismo commit.**

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
- `src/central/motors_zircon.{h,cpp}` — PWM 3 motores omni + kicker (ROBOT2)
- `src/central/imu_zircon.{h,cpp}` — BNO055 respaldo
- `src/central/world_model.{h,cpp}` — espejo del WorldSnapshot
- `src/central/comm_top.{h,cpp}` — recibe WorldSnapshot del TOP (Serial1)
- `src/central/comm_down.{h,cpp}` — recibe LineStatusV2 del DOWN (hoy `Serial2`). ⚠️ **Veredicto del conflicto 7/8 PENDIENTE de aislar** (Gustavo 2026-05-29: NO se aisló en banco si el motor del driver U17 usa 7/8). SI se confirma que 7/8 son del motor → migrar `Serial2` → `Serial7` acá. Por las dudas, el receiver de banco del link ya se hizo en **Serial7**: env `diag_central_comm_down` + [`docs/firmware/DIAG-CENTRAL-COMM-DOWN.md`](firmware/DIAG-CENTRAL-COMM-DOWN.md).

### TOP (Teensy 4.0)
- `src/top/main_top.cpp` + `cameras_runtime`, `cameras`, `sensors_imu`, `sensors_tof` (HC-SR04 + 1 VL53L7CX frontal U2 vivo, lib `Adafruit_VL53L7CX`. ⚠️ TOP rev 1.0 NO permite >1 ToF por bus sin rework — XSHUT/LPn de los 4 slots NO ruteados en el PCB, verificación forense 2026-05-25. Máximo soportado: 2 ToFs total. Ver journal 2026-05-24 + 2026-05-25), `comm_*`
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

### Shared (puro, testeado host-native)
- `pids`, `kinematics`, `behind_ball`, `cameras_fusion`, `line_filters`, `crc16`, `proto`, `types`
- `strategy_transitions` (caracterización pura de `strategy.cpp` con 35 tests — no conectado, mantener como red)

## Tests host-native

| Suite | Tests | Cubre |
|---|---|---|
| `test_kinematics` | 11 | omni-3 |
| `test_pids` | 18 | heading + lateral + distancia |
| `test_proto` | 13 | CRC, frame, marker |
| `test_line_filters` | 39 | temporal + hysteresis + spatial + centroide + lifted + saturación todo-blanco |
| `test_cameras_fusion` | 16 | rot 180°, fuse front+back, watchdog |
| `test_behind_ball` | 16 | target detrás, aligned-to-shoot, attack-line, kickoff |
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
| **Total (20 envs)** | **262** | **0 fallos** |

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
- TASK-006 (Virginia/Elías): **flash firmware COMM ESP32-C6** (procedure del 17-may, NO el del 15-may que tiene banner)
- TASK-011 (Enzo): confirmar PIN_KICKER_SOL en Zircon
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

1. **COMM no flasheada** → robot no homologa (no recibe START/STOP árbitro). TASK-006.
2. **Cámaras sin recalibrar para iluminación Incheon** → no ve la pelota. TASK-022.

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
    los OTOS van DOWN→TOP (`main_down.cpp:101/107`).
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
  **U9** (cámara trasera, hoy provisional en Serial7). La placa TOP aún no está armada.

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

### Resuelto 2026-05-25
- **Verificado forensicamente que los XSHUT/LPn de los 4 TOFs NO están
  ruteados en TOP rev 1.0** (NC flags explícitos en SCH, 0 nets en PCB
  netlist). Implicancia: máximo 2 ToFs sin rework (1 por bus I²C).
  `config_top.h:68` con `PIN_TOF_XSHUT[4] = {2,3,4,5}` documentado como
  ficción heredada (banner agregado, código vivo no lo usa). Wishlist
  de TOP rev 1.1 (post-Incheon) capturado con 7 items
  (XSHUT + agujeros cámaras + reguladores fuera del borde + conectores
  keyed + LEDs OK + voltímetro + STM32 integrado). Decisión Incheon
  (2 ToFs vs 4 con bodge) escalada a TASK-033. Ver journal
  `2026-05-25-top-xshut-no-routed-hallazgo-forense.md` + research
  `research/in-progress/2026-05-25-top-board-rev-1.1-wishlist.md`.

### Deudas conocidas (resumen — la canónica está en `FUENTES-DE-VERDAD.md`)

- **TOP rev 1.0 — XSHUT/LPn de los 4 slots ToF no ruteados.** Sin rework
  hardware el máximo soportado es 2 ToFs (1 por bus I²C). Decisión
  pendiente para Incheon: ver TASK-033 (2 ToFs sin rework vs 4 con bodge
  de Enzo). Solución de fondo: TOP rev 1.1 post-Incheon
  (`research/in-progress/2026-05-25-top-board-rev-1.1-wishlist.md`).
- HAL Sprint B (extender sensors_tof.cpp para enumerar NUM_TOF_ACTIVE
  TOFs con XSHUT secuencial al boot). Bloqueado por TASK-038
  (confirmar pines reales del bodge).
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
