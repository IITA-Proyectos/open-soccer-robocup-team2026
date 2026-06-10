---
date: 2026-06-04
status: vivo
tipo: auditoria-compatibilidad
autor: editor-tecnico (consolidacion)
fuente: auditoria por-archivo de 9 grupos (A1-diag-tof, A2-diag-bno-loc, A3-diag-central, A5-firmware-central-top, A6-firmware-down-shared, A7-programas-anteriores, A8-legacy-scripts, A9-packs)
---

# Auditoria de compatibilidad de TODOS los programas del repo (2026-06-04)

Consolidacion por archivo del estado de compatibilidad de cada programa del repo contra el
**contrato vigente** (3 placas TOP/CENTRAL/DOWN, WorldSnapshot v3 = 31 B, camara->TOP v2 = 11 B,
sin kicker, arbitro por GPIO 5/6, BNO 100 kHz / 20 Hz, 4 ToF Adafruit 0x2A-0x2D, ejes
X=1820 corto / Y=2430 largo, MOTOR_INVERT M2/U17 invertido).

---

## 1. Resumen para el coach

Se auditaron **~110 unidades de programa** del repo (firmware vivo, diags, scripts host,
programas 2025, snapshots de pack y scripts de camara). La **buena noticia**: todo el firmware
vivo que se flashea a competencia (`src/central/`, `src/top/`, `src/down/`, `src/shared/`, las
2 camaras N6 `cam-*-n6.py`) es **compatible con el contrato vigente**; lo que falla son cosas
que NO entran al build de competencia. Los problemas se concentran en tres bolsas: (1) **diags
de bring-up ya cumplidos** (identificacion del chip ToF, descubrimiento de pines LP) que siguen
ocupando un `[env]` en `platformio.ini` aunque su pregunta ya esta resuelta; (2) **programas
2025** (`robot-delantero/`, `robot-arquero/`, `zirconLib`, vision v1) que son **incompatibles
de raiz** (kicker, parser camara 9B v1, BNO local, sin framing proto.h) y deben ir a un cajon
historico; y (3) **comentarios stale** en firmware vivo que mienten sobre el cableado/version
aunque el codigo corre bien (p.ej. `diag_central_drive_straight.cpp` dice "Serial1" cuando el
runtime ya usa Serial7). El riesgo mas serio para confundir al equipo son los **pack-snapshots
del 2026-05-24** (`hardware/electronics/*-pack/firmware/`): tienen `WorldSnapshot==27` (v2),
`kicker_fire` y el UART intercambiado, y NO llevan banner de "NO ES BUILD PATH" — alguien podria
creer que son el firmware vivo. Accion neta: mover ~20 archivos a deprecated, poner banner en
~16 snapshots, corregir ~22 comentarios stale y arreglar 2 diags para que compilen/corran.

---

## 2. Conteo por veredicto

| Veredicto | Cantidad | Que significa |
|---|---:|---|
| compatible | 55 | corre/usable hoy contra el contrato vigente (incluye 8 "dejar" con nota menor) |
| comentarios-stale | 22 | el codigo es correcto pero un comentario/banner miente (version, cableado, env) |
| incompatible-fixable | 12 | no corre tal cual pero la logica util existe o se arregla con poco |
| incompatible-deprecar | 15 | incompatible de raiz (kicker / parser v1 / HW Zircon ausente / snapshot v2) -> al cajon |
| no-ejecutable | 7 | sin valor operativo hoy (diag cumplido, stub de 1 linea, README vacio) |

**Nota de mapeo:** los grupos usaron etiquetas mixtas. Aqui `no-ejecutable` agrupa "diag
cumplido sin proposito" + stubs/READMEs vacios; `incompatible-deprecar` agrupa los
`incompatible-deprecar` de programas 2025 y los snapshots v2+kicker marcados como tales.
Por **accion**, que es lo accionable, el desglose es: **deprecar-mover ~16**, **agregar-warning
~17**, **corregir-comentarios ~22**, **hacer-compatible 2**, **dejar ~53**.

### Temas mas importantes (orden de impacto)

1. **Pack-snapshots = trampa silenciosa.** `hardware/electronics/{central,top,down}-board-pack/
   firmware/` son congelados del 2026-05-24: `WorldSnapshot==27` (v2), `MotorCommand.kicker_fire`,
   `ZirconStatus.kicker_ready`, UART TOP<->CENTRAL intercambiado (Serial1/Serial2 en vez de
   Serial7/Serial4), ToF XSHUT {2,3,4,5}. **No son build path** pero no lo gritan. (`proto.{cpp,h}`
   de los packs SI es byte-identico al vivo: el transporte no cambio, cambio el payload+cableado.)
2. **Programas 2025 incompatibles de raiz.** `definitivo-delantero.cpp`, `delantero-sin-zirconLib.cpp`,
   `definitivo-arquero_6-9-2026`, `zirconLib.{cpp,h}` y la vision v1 `enviar coordenadas...`:
   kicker fisico, parser camara 9B v1 (sin CRC/END, X cruda), BNO local en el Teensy de motores,
   sin MOTOR_INVERT, HW Zircon Mark1/Naveen1 ausente. `zirconLib.cpp` ni siquiera compila (llave
   colgante L355).
3. **Diags de bring-up cumplidos con env vivo.** Toda la saga de identificacion del ToF
   (`_as_l5cx`, `_as_l8cx`, `_adafruit`, `_lp_discover`, `_enumerate`, `_census`) y el `diag_top_tof`
   con la lib ST abandonada: la pregunta ("es L7CX, LP en 9/10/11/12") ya esta baked en el firmware.
   Ocupan envs en `platformio.ini` y confunden.
4. **400 kHz en diags BNO-solo es SEGURO, no bug.** Varios diags ponen `setClock(400000)` pero
   duermen los ToF -> bus limpio. El congelamiento de yaw solo aplica a coexistencia BNO+ToF.
   La excepcion real es `diag_top_bno.cpp` y `diag_pose_live.cpp`, que NO duermen los ToF y a la
   vez corren a 400k: ahi la lectura de heading NO es representativa del firmware vivo (100k).
5. **`diag_sensors_tof_live` env base no compila** (falta `-DROBOT1/-DROBOT2` -> `#error` en
   `hardware_profile.h`). Solo el `_robot1/_robot2` flashea: el env pelado es una trampa de build.
6. **Ejes invertidos en comentarios de `diag_pose_live.cpp`** (dice X=largo/Y=corto, convencion
   pre 2026-06-03) aunque sus constantes ya son las correctas (X=1820, Y=2430). El print en
   pantalla miente sobre que eje es cual.

---

## 3. Tabla por archivo (agrupada por accion)

### 3a. accion = DEPRECAR-MOVER (al cajon historico)

| Archivo | tipo | veredicto | Que tiene mal (resumen) | tiene_env |
|---|---|---|---|---|
| `src/diag/diag_top_tof.cpp` | diag | comentarios-stale | lib ST VL53L7CX abandonada (err=255); single-sensor 0x29; sketch de bring-up cerrado | si `[env:diag_top_tof]` (+ `_no_xshut`) |
| `src/diag/diag_top_tof_as_l5cx.cpp` | diag | no-ejecutable | experimento "es L5CX?" ya respondido NO; sin proposito | si `[env:diag_top_tof_as_l5cx]` |
| `src/diag/diag_top_tof_as_l8cx.cpp` | diag | no-ejecutable | experimento "es L8CX?" ya respondido NO | si `[env:diag_top_tof_as_l8cx]` |
| `src/diag/diag_top_tof_adafruit.cpp` | diag | no-ejecutable | test de control que gano; ya promovido a sensors_tof.cpp | si `[env:diag_top_tof_adafruit]` |
| `src/diag/diag_top_tof_lp_discover.cpp` | diag | no-ejecutable | descubridor de pines LP; resuelto 9/10/11/12 | si `[env:diag_top_tof_lp_discover]` |
| `src/diag/diag_top_tof_enumerate.cpp` | diag | no-ejecutable | hipotesis LP con pin 22 (erroneo); superado por _census | si `[env:diag_top_tof_enumerate]` |
| `src/diag/diag_top_tof_census.cpp` | diag | no-ejecutable | censo que RESOLVIO la incognita LP; util solo como re-diagnostico | si `[env:diag_top_tof_census]` |
| `software/robot-delantero/definitivo-delantero.cpp` | programa-anterior | incompatible-deprecar | zirconLib (no compila), BNO local, camara 9B v1, KICKER, sin proto.h, ROBOT2 sin MOTOR_INVERT | NO |
| `software/robot-delantero/delantero-sin-zirconLib.cpp` | programa-anterior | incompatible-deprecar | readLine A8/A9... mono-placa 2025, BNO local, camara 9B v1, KICKER | NO |
| `software/robot-arquero/definitivo-arquero_6-9-2026` | programa-anterior | incompatible-deprecar | sin extension, zirconLib, BNO local, 9B v1, KICKER | NO |
| `software/libraries/zirconLib/zirconLib.cpp` | legacy | incompatible-deprecar | NO COMPILA (llave colgante L355); HW Zircon ausente; sin MOTOR_INVERT | NO |
| `software/libraries/zirconLib/zirconLib.h` | legacy | incompatible-deprecar | API Zircon 2025, BNO en header, mono-placa | NO |
| `software/vision/enviar coordenadas 2 arcos y pelota` | programa-anterior | incompatible-deprecar | camara 9B v1 (sin CRC/END, X cruda), pyb.LED (crash N6), LAB 2025 | N/A (.py) |
| `hardware/electronics/cameraFront-pack/.../target-cam-frontal-template.py` | template | comentarios-stale | contrato v1 (9B, X sin offset); banner ya dice "SUPERADO -> flashear n6" | N/A (.py) |
| `hardware/electronics/cameraBack-pack/.../target-cam-trasera-template.py` | template | comentarios-stale | idem v1; banner ya redirige al n6 | N/A (.py) |
| `software/teensy/Soccer 2026/src/main.cpp` | programa-anterior | compatible (mover) | self-contained robot 2025 ROBOT2; pinout no-CENTRAL; confunde en src/ raiz | si `[env:teensy41_legacy]` (NO TOCAR) |

### 3b. accion = AGREGAR-WARNING (se quedan, pero con banner)

| Archivo | tipo | veredicto | Que tiene mal (resumen) | tiene_env |
|---|---|---|---|---|
| `src/diag/diag_top_tof_quad_live.cpp` | diag | compatible | VIGENTE; solo 400k vs 100k (sin BNO en loop -> aceptable) | si `[env:diag_top_tof_quad_live]` |
| `src/diag/diag_top_tof_zonemap.cpp` | diag | compatible | VIGENTE; mismo matiz 400k | si `[env:diag_top_tof_zonemap]` |
| `src/diag/diag_central_recv1.cpp` | diag | compatible | Serial2/pin7 a proposito; NO es el cableado de produccion (prod=Serial1/pin0) | si `[env:diag_central_recv1]` |
| `src/diag/diag_central_line_sweep.cpp` | diag | compatible | usa imu_zircon (BNO local) -> degrada si ausente; arquero que SI anduvo | si robot1/2 + _nosafety |
| `software/staging/shared/test-motores-lateral-simple/...ino` | test | incompatible-fixable | BNO local, NDOF, delay() bloqueante; logica ya en diag_central_strafe/line_sweep | NO (staging) |
| `software/staging/shared/test-4-movimientos/...ino` | test | incompatible-fixable | BNO local NDOF; constrain mata rueda; usar diag_central_drive | NO (staging) |
| `software/staging/shared/test-circulo/...ino` | test | incompatible-fixable | BNO local NDOF; cinematica ad-hoc; usar kinematics/pids | NO (staging) |
| `software/staging/shared/test-gyro-movimiento-basico/...ino` | test | incompatible-fixable | BNO local NDOF; logica ya en diag_central_drive | NO (staging) |
| `software/staging/shared/test-gyro-movimiento-lateral(No probar...)/...ino` | test | incompatible-fixable | el propio nombre dice "no probar"; BNO local NDOF | NO (staging) |
| `software/staging/shared/test-bno055-imuplus/...ino` | test | incompatible-fixable | el mejor del lote (IMUPLUS) pero falta 100k/20Hz; = diag_bno_left | NO (staging) |
| `software/staging/shared/test-movimiento-omnidireccional/...ino` | test | incompatible-fixable | depende de zirconLib (HW ausente); angulos rueda 30/150/270 divergentes | NO (staging) |
| `software/staging/down_board/light_sensors/prueba-leer-S1.cpp` | diag | incompatible-fixable | comentario pines vs codigo contradictorios; solo canal 0; = diag_down | NO (staging) |
| `hardware/electronics/*-pack/.../firmware/teensy/config_top.h, cameras_fusion, cameras_runtime` | pack-snapshot | comentarios-stale | config monolitico viejo: Serial4->COMM, ToF XSHUT {2,3,4,5}, BNO sin nota 0x29 | N/A (no build path) |
| `hardware/electronics/{central,top,down}-pack/.../shared/types.h` | pack-snapshot | incompatible-deprecar | `WorldSnapshot==27` (v2), `kicker_fire`, `kicker_ready`; falta EV_SENSOR_NOISY | N/A (no build path) |
| `hardware/electronics/central-pack/.../config_central.h` | pack-snapshot | incompatible-deprecar | UART_TOP en Serial1 (vivo=Serial7); KICKER; sin MOTOR_INVERT | N/A (no build path) |
| `hardware/electronics/central-pack/.../main_central.cpp` (+ comm/motors/strategy/world_model/shared) | pack-snapshot | incompatible-deprecar | UART intercambiado; arrastra types v2+kicker; kicker_init() | N/A (no build path) |
| `hardware/electronics/top-pack/.../main_top.cpp, config_top.h` (+ cameras/comm_arbiter/...) | pack-snapshot | incompatible-deprecar | snapshot a CENTRAL por Serial2 (vivo=Serial4); arbitro UART (vivo=GPIO 5/6); cameras quiza v1 9B | N/A (no build path) |
| `hardware/electronics/down-pack/.../config_down.h` (+ main_down/comm/line/...) | pack-snapshot | compatible/stale | el menos divergente; OTOS nomenclatura bus a verificar; hereda types v2+kicker | N/A (no build path) |

### 3c. accion = CORREGIR-COMENTARIOS (firmware vivo, codigo OK)

| Archivo | tipo | veredicto | Que comentario miente | tiene_env |
|---|---|---|---|---|
| `src/diag/diag_top_i2c_scan.cpp` | diag | compatible | omite 0x2D; "otro BNO en Wire2 (24/25)" optimista (0x29 fallado; bus de 24/25 = `Wire2`/LPI2C4, no `Wire1` — corrección 2026-06-09) | si `[env:diag_top_i2c_scan]` |
| `src/diag/diag_pose_live.cpp` | diag | comentarios-stale | L21/L178 ejes invertidos (X=largo) vs constantes correctas; 400k con ToF activos | si `[env:diag_pose_live]` |
| `src/diag/diag_central_atras_adelante.cpp` | programa-anterior | compatible | "conflicto Serial2 7/8" stale (link a DOWN se movio a Serial1) | si `[env:diag_central_atras_adelante]` |
| `src/diag/diag_central_drive_straight.cpp` | diag | comentarios-stale | dice "WorldSnapshot por Serial1"; runtime ya usa Serial7; "v2" stale (hoy v3) | si drive_robot1/robot2 |
| `src/diag/diag_central_rx_all.cpp` | diag | comentarios-stale | dice "WorldSnapshot 27B" (v2); runtime auto-valida 31B v3 | si `[env:diag_central_rx_all]` |
| `src/central/main_central.cpp` | firmware-vivo | comentarios-stale | `apply_role_from_dipswitch()` misnomer (rol por -DROBOT1/2); L9 "BNO respaldo" enganoso | central_robot1/2 |
| `src/central/strategy.h` | firmware-vivo | comentarios-stale | "rol por dipswitch en config_top.h" doblemente falso; "stub Hito 4/6" desactualizado | via central_robot1/2 |
| `src/top/main_top.cpp` | firmware-vivo | comentarios-stale | L17 "pio run -e top" ya no compila (exige -DROBOT1/2) | top_robot1/2 |
| `src/top/sensors_tof.cpp` | firmware-vivo | comentarios-stale | L24-29/52-56 "solo ToF frontal soldado (2026-05-24)"; vivo enumera 4 | via top_robot1/2 |
| `src/down/comm_top.h` | firmware-vivo | comentarios-stale | L8 handler "calibrar-linea desde TOP" ya eliminado | n/a |
| `src/shared/cameras_fusion.{h,cpp}` | firmware-vivo | comentarios-stale | protocolo OpenMV viejo en comentario; funcion agnostica al wire | n/a |
| `src/shared/ball_velocity.{h,cpp}` | firmware-vivo | comentarios-stale | dice "WorldSnapshot v2"; campos ball_vx/vy siguen en v3 | n/a |
| `software/staging/README.md` | script/doc | comentarios-stale | cuerpo lista tests "a probar" y apunta a definitivo-* 2025 / vision v1 | N/A (doc) |
| `software/vision/README.md` | script/doc | comentarios-stale | "OpenMV H7/H7 Plus" (vivo=N6); apunta al script v1 | N/A (doc) |
| `scripts/visualize_down_sensors.py` | script | compatible | ruta de comentario apunta al pack-snapshot, no a src/diag/main_diag_down.cpp | N/A (host) |
| `hardware/electronics/cameraFront-pack/.../cam-frontal-n6.py` | firmware-vivo (camara) | compatible | L219 "9 bytes" residual; el packet real es 11B v2 | N/A (.py) |
| `hardware/electronics/cameraBack-pack/.../cam-trasera-n6.py` | firmware-vivo (camara) | compatible | posible "9 bytes" residual analogo al frontal | N/A (.py) |
| `hardware/electronics/cameraFront-pack/.../current-generic.py` | legacy ref | comentarios-stale | v1 + pyb.LED; banner v1 ya presente; NO flashear | N/A (.py) |
| `hardware/electronics/cameraBack-pack/.../current-generic.py` | legacy ref | comentarios-stale | idem v1; banner ya presente | N/A (.py) |

(Total corregir-comentarios incluye tambien `diag_top_i2c_scan` y los .py de camara que estan
marcados "compatible" pero con un comentario a tocar.)

### 3d. accion = HACER-COMPATIBLE (arreglar para que corra)

| Archivo | tipo | veredicto | Que arreglar | tiene_env |
|---|---|---|---|---|
| `src/diag/diag_sensors_tof_live.cpp` | diag | incompatible-fixable | env base NO compila (falta -DROBOT); banner stale 0x29/HC-SR04 6/7 (vivo 4/3); imprime solo tof[0] | parcial: base no compila; `_robot1/_robot2` si |
| `src/diag/diag_top_bno.cpp` | diag | comentarios-stale | RIGHT en **Wire2 (24/25)** (bus de 24/25 = `Wire2`/LPI2C4, no `Wire1` — corrección 2026-06-09); ese bus-aparte vuelve a ser el fix canónico (TASK-207, ROBOT2 BNO2 0x28 en Wire2); 400k con ToF activos | si `[env:diag_top_bno]` |

### 3e. accion = DEJAR (compatible, sin tocar) — resumen

55 archivos quedan tal cual. Bloque grueso:
- **Firmware CENTRAL vivo:** `config_central.h`, `comm_top.{h,cpp}`, `comm_down.{h,cpp}`,
  `world_model.{h,cpp}`, `strategy.cpp`, `motors_zircon.{h,cpp}`, `imu_zircon.{h,cpp}`.
- **Firmware TOP vivo:** `config_top.h`, `pinout_common.h`, `pinout_robot1.h`, `pinout_robot2.h`,
  `hardware_profile.h`, `cameras.{h,cpp}`, `cameras_runtime.{h,cpp}`, `comm_central.{h,cpp}`,
  `comm_arbiter.{h,cpp}`, `comm_down.{h,cpp}`, `localization_runtime.{h,cpp}`,
  `sensors_imu.{h,cpp}`, `sensors_tof.h`.
- **DOWN + shared:** `src/down/` resto (8) + `src/shared/` (37) compatibles (LineStatusV2 16B,
  OTOS dual-bus 0x17, sin kicker, WorldSnapshot v3 31B, ejes X1820/Y2430).
- **Diags vigentes:** `diag_bno_addr_check`, `diag_bno_dual_live`, `diag_bno_left`, `diag_bno_tof`
  (+`_slow`), `diag_top_all`, `diag_localization_live`, `diag_top_ultrasonic`,
  `diag_central_motors`, `diag_central_strafe`, `diag_central_arbitro_strafe`,
  `diag_central_comm_down`, `diag_cam_acceptance`.
- **Scripts host:** `diag_capture.py`, `diag_position_sweep.py`, `diag_otos_move_test.py`,
  `extract_pinout_from_schematic.py`.
- **Camara/calib:** `calib-lab-n6.py` (front/back), mirrors `cameras.{h,cpp}` v2 de los packs,
  `proto.{cpp,h}` de los 3 packs (byte-identicos al vivo).
- **Docs/README vigentes:** `staging/up_board/00-LEER-PRIMERO`, `communication/README.md`,
  `libraries/README.md`, `robot-delantero/README.md`, `robot-arquero/README.md`,
  `legacy/2025-season/README.md` (+misc/mechanical), `robot-v2/*` stubs (no-ejecutable, dejar).

---

## 4. Plan de accion ejecutable (en orden de prioridad)

### (a) DEPRECAR-MOVER

**Estructura de carpetas propuesta:**

```
software/teensy/Soccer 2026/_deprecated/        <- diags de bring-up cumplidos (.cpp con [env])
software/_deprecated-2025/                       <- programas 2025 completos del robot nacional
    robot-delantero/ robot-arquero/ libraries/zirconLib/ vision/ (los v1)
```

**Mover a `software/teensy/Soccer 2026/_deprecated/` (diags cumplidos):**
`diag_top_tof.cpp`, `diag_top_tof_as_l5cx.cpp`, `diag_top_tof_as_l8cx.cpp`,
`diag_top_tof_adafruit.cpp`, `diag_top_tof_lp_discover.cpp`, `diag_top_tof_enumerate.cpp`,
`diag_top_tof_census.cpp`.

> **REGLA DURA:** cada uno de estos tiene un `[env]` activo en `platformio.ini`. Mover el `.cpp`
> sin quitar su `[env]` (y su `build_src_filter +<diag/...>`) **ROMPE EL BUILD** del env (no
> encuentra la fuente). Por cada archivo movido: **borrar/comentar el `[env]` correspondiente**
> en `platformio.ini` (lineas indicadas: diag_top_tof:234, _no_xshut:255, as_l5cx:272,
> as_l8cx:294, adafruit:316, lp_discover:352, enumerate:371, census:390). Estos envs NO se
> reemplazan: la herramienta de re-diagnostico que se conserva es `diag_top_tof_census` SOLO si
> se decide dejarlo como utilidad — en ese caso NO mover ese y mantener su env. Recomendado:
> mover los 7 y conservar `diag_top_tof_quad_live` (3a/3b) como el re-test vivo de los 4 ToF.

**Mover a `software/_deprecated-2025/` (programas 2025):**
`robot-delantero/definitivo-delantero.cpp`, `robot-delantero/delantero-sin-zirconLib.cpp`,
`robot-arquero/definitivo-arquero_6-9-2026`, `libraries/zirconLib/zirconLib.{cpp,h}`,
`vision/enviar coordenadas 2 arcos y pelota`,
`cameraFront-pack/.../target-cam-frontal-template.py`,
`cameraBack-pack/.../target-cam-trasera-template.py`.

> **NINGUNO de estos debe tener `[env]` activo tras moverlo.** Verificar: hoy ya NO estan en
> ningun `build_src_filter` (los `.cpp` 2025 estan fuera del build path; `zirconLib` no la
> referencia ningun env; los `.py` no van por platformio). El unico con env es
> `src/main.cpp` (`[env:teensy41_legacy]`) — ver nota especial abajo.

**Caso especial `src/main.cpp`:** compila y flashea via `[env:teensy41_legacy]` (marcado
"NO TOCAR"). Recomendacion: **mover a `legacy/` o `archive/` y actualizar el `build_src_filter`
del env** (`+<main.cpp>` -> nueva ruta) para que no confunda en `src/` raiz. Si no se quiere
tocar el env, dejarlo en su lugar con banner (es la opcion conservadora). NO borrar el env.

### (b) AGREGAR-WARNING (banner inicial, se quedan donde estan)

**Banner "NO ES BUILD PATH — SNAPSHOT CONGELADO 2026-05-24, contrato v2+kicker, NO FLASHEAR"
en todos los pack-snapshots** de `hardware/electronics/{central,top,down}-board-pack/firmware/`:
`types.h`, `config_central.h`, `config_top.h`, `main_central.cpp`, `main_top.cpp`,
`comm_*.{cpp,h}`, `motors_zircon.*`, `cameras*.{cpp,h}`, `sensors_*`, `cameras_fusion.*`.
(Excepcion: `proto.{cpp,h}` de los packs es identico al vivo — banner suave "mirror, sin cambios".)

**Banner "VIGENTE pero 400 kHz (solo ToF en bus, sin BNO en loop) — NO es la velocidad de
produccion 100 kHz":** `diag_top_tof_quad_live.cpp`, `diag_top_tof_zonemap.cpp`.

**Banner "Serial2/pin7 = cable de prueba puntual, NO el cableado de produccion (prod = DOWN->
CENTRAL por Serial1/pin0)":** `diag_central_recv1.cpp`.

**Banner "usa BNO local (imu_zircon) — la CENTRAL no lleva BNO; degrada a patrulla sin
correccion si ausente":** `diag_central_line_sweep.cpp`.

**Banner "STAGING CONGELADO — NO flashear; equivalente vivo = <env>":** los 8 sketches de
`software/staging/shared/*` y `software/staging/down_board/light_sensors/prueba-leer-S1.cpp`.
(El `staging/README.md` ya tiene el header congelado; falta propagar a cada `.ino`.)

### (c) CORREGIR-COMENTARIOS (firmware vivo, codigo intacto)

Edicion puntual de comentario/banner, sin tocar logica:
- `src/top/sensors_tof.cpp` L24-29/52-56: cambiar "solo ToF frontal soldado (2026-05-24)" por
  "4 ToF activos 0x2A-0x2D via LP 9/10/11/12 (banco 2026-05-30)".
- `src/top/main_top.cpp` L17: "Build: pio run -e top_robot1 / top_robot2" (no `-e top`).
- `src/central/main_central.cpp`: renombrar `apply_role_from_dipswitch()` -> `apply_role_from_build_flag()`
  (o nota inline); corregir L9 "BNO respaldo" -> "gated por -DCENTRAL_HAS_LOCAL_BNO, default OFF".
- `src/central/strategy.h` L3-7/11-13: borrar "rol por dipswitch en config_top.h" y "stub Hito 4/6".
- `src/diag/diag_pose_live.cpp` L21/L178: invertir el texto de ejes a X=1820 corto / Y=2430 largo.
- `src/diag/diag_central_drive_straight.cpp`: "WorldSnapshot por Serial1" -> "Serial7 (pin28)";
  quitar "v2".
- `src/diag/diag_central_rx_all.cpp` L24: "WorldSnapshot 27B" -> "31B (v3)".
- `src/diag/diag_central_atras_adelante.cpp`: quitar nota "conflicto Serial2 7/8".
- `src/diag/diag_top_i2c_scan.cpp`: agregar 0x2D, suavizar "otro BNO en Wire1" → debe decir **Wire2 (24/25)** (el bus de 24/25 es `Wire2`/LPI2C4, no `Wire1` — corrección 2026-06-09).
- `src/down/comm_top.h` L8; `src/shared/cameras_fusion.{h,cpp}`; `src/shared/ball_velocity.{h,cpp}`
  (v2 -> v3).
- `scripts/visualize_down_sensors.py` ~L31: ruta -> `src/diag/main_diag_down.cpp`.
- `cam-frontal-n6.py` L219 / `cam-trasera-n6.py`: "9 bytes" -> "11 bytes (v2)".
- `software/staging/README.md`, `software/vision/README.md`: actualizar plan y "OpenMV H7"->N6.
- `current-generic.py` (front/back): ya tienen banner v1 — solo confirmar, sin cambio.

### (d) HACER-COMPATIBLE (que corran)

1. **`src/diag/diag_sensors_tof_live.cpp`** — (i) el env base `[env:diag_sensors_tof_live]`
   (platformio.ini:455) NO compila por falta de `-DROBOT1/-DROBOT2`: **eliminar el env base** y
   dejar solo `_robot1`/`_robot2` (774/778), o agregarle un `-DROBOT1` por defecto. (ii) Corregir
   banner stale: "0x29 / 1 ToF frontal" -> "4 ToF 0x2A-0x2D"; HC-SR04 "TRIG=6/ECHO=7" -> "4/3".
   (iii) opcional: imprimir tof[0..3], no solo tof[0].
2. **`src/diag/diag_top_bno.cpp`** — el modelo de HW asume RIGHT en **Wire2 (24/25)** (el bus de
   24/25 es `Wire2`/LPI2C4, no `Wire1` — corrección 2026-06-09).
   > ⚠️ **Recomendación REVERTIDA (2026-06-09, TASK-207).** Esta acción decía "mover RIGHT a
   > `Wire` 0x29 (ambos BNO en el mismo bus)". Eso es justo lo que CAUSÓ la contención/freeze.
   > La decisión actual es la inversa: el 2º BNO va a un **bus APARTE (`Wire2` 24/25)** como
   > PRIMARIO (confirmado en ROBOT2: BNO2 0x28 vivo en `Wire2`). O sea: la variante "BNO en el
   > bus de 24/25" NO se deprecía — es el diseño bueno. Mantener `diag_top_bno` con RIGHT en
   > `Wire2` (o usar `diag_bno_dual_live` si soporta el 2º bus); a 100 kHz si se dejan ToF.

---

## 5. Notas finales

- **Lo que SI se flashea a competencia esta sano.** `central_robot1/2`, `top_robot1/2`, `down`,
  `down_debug` y las 2 camaras `cam-*-n6.py` son todos compatibles con v3 / camara v2 / sin
  kicker / arbitro GPIO. El riesgo es de **higiene de repo**, no de competencia.
- **Verificacion de build tras mover diags:** correr `pio run` (o el subset de envs afectados) y
  `scripts/run-host-tests.sh` despues de tocar `platformio.ini`, para confirmar que ningun
  `build_src_filter` quedo apuntando a una fuente movida.
- **No homogeneizar legacy.** `legacy/2025-season/README.md` (UART 6B, solenoide) describe el
  contrato 2025 a proposito; es correcto historicamente y NO debe alinearse a v2/v3.
