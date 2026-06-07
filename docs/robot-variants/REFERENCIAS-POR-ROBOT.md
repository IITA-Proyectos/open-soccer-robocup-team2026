# REFERENCIAS-POR-ROBOT — Auditoría read-only de lo que varía (o debería variar) por robot

**Fecha:** 2026-06-05 · **Tipo:** auditoría READ-ONLY (no se editó firmware) · **Autor:** agente B-AUDIT
**Repo:** `C:/Users/violl/iitasoccer/soccer-main` · **Firmware:** `software/teensy/Soccer 2026`

Este documento mapea TODA referencia del firmware (TOP / CENTRAL / DOWN / cámaras / platformio)
que depende —o que según los datos del usuario DEBERÍA depender— del robot físico (ROBOT1
arquero vs ROBOT2 delantero). Lo usa el agente de diseño para decidir qué meter en un futuro
"robot definition" único.

**Convenciones de marcado:**
- **PER-ROBOT (HW)** = ya está parametrizado por robot en el firmware (vía `#if ROBOT1/ROBOT2` o pinout_robotN.h).
- **HARDCODED-COMÚN** = un solo valor para ambos robots; si R2 difiere, hoy NO hay forma de seleccionarlo.
- **PER-CÁMARA** = vive en el script `.py` de cada cámara (front/back), no en el Teensy.
- **A-CONFIRMAR-EN-BANCO** = valor tentativo / placeholder, hay que medirlo en R2.
- **⚠️ MISMATCH R2** = el firmware HOY contradice lo que el usuario dijo de ROBOT2 (acción requerida).

---

## 0. Índice de subsistemas

1. Mecanismo de selección por robot (envs / macros)
2. Motores (CENTRAL)
3. IMU / BNO (TOP)
4. ToF (TOP)
5. OTOS (DOWN)
6. Cámaras: distancia (homografía) + color (LAB)
7. Geometría del robot
8. Cancha (común, no per-robot)
9. Puertos de comunicación entre placas y cámaras
10. Placa COMM (ESP32-C6) — dónde vive el firmware
11. Resumen ejecutivo: conteo + qué cambia en R2

---

## 1. Mecanismo de selección por robot (estado actual)

Hay **DOS ejes de selección independientes** y NO unificados:

### Eje A — Macro `ROBOT1` / `ROBOT2` (TOP + CENTRAL)
Se pasa por `build_flags` del env de PlatformIO. NO hay dipswitch ni autodetección: el rol/robot
se fija en COMPILACIÓN.

| Placa | Env de competencia | Macro | Qué selecciona |
|---|---|---|---|
| TOP | `top_robot1` / `top_robot2` | `-DROBOT1` / `-DROBOT2` | `hardware_profile.h` incluye `pinout_robot1.h` o `pinout_robot2.h` |
| CENTRAL | `central_robot1` / `central_robot2` | `-DROBOT1` / `-DROBOT2` | bloque `#if defined(ROBOT1)/#elif ROBOT2` en `config_central.h` (pines motor + invert) + rol FSM en `main_central.cpp` / `strategy` |

- `platformio.ini:573-581` — `[env:top_robot1]` añade `-DROBOT1 -DTOP_ENABLE_MULTI_TOF -DTOP_ENABLE_HCSR04`; `[env:top_robot2]` igual con `-DROBOT2`.
- `platformio.ini:137-164` — `[env:central_robot1]` (`-DROBOT1`) y `[env:central_robot2]` (`-DROBOT2`).
- Dispatcher: `src/top/hardware_profile.h:19-25` — `#if defined(ROBOT1) ... #elif defined(ROBOT2) ... #else #error`. Compilar sin la macro es error de compilación a propósito.
- `src/top/config_top.h:29` es un wrapper legacy que sólo hace `#include "hardware_profile.h"`.

### Eje B — Flags `-DDOWN_NUM_*` (DOWN)
La placa DOWN **NO usa `ROBOT1/ROBOT2`**. Su variación de hardware se selecciona con flags numéricos
de cantidad de periféricos, en el env (no por robot):

| Flag | Archivo:línea | Default (fallback) | En `[env:down]` |
|---|---|---|---|
| `DOWN_NUM_MUXES_CONNECTED` | `config_down.h:32-35` (`#ifndef`→1) | 1 (8 sensores) | `platformio.ini:101` → **4** (32 sensores) |
| `DOWN_NUM_OTOS_CONNECTED` | `config_down.h:37-40` (`#ifndef`→1) | 1 | `platformio.ini:102` → **2** |

- **HAY UN SOLO `[env:down]`** (`platformio.ini:92-113`). No existe `down_robot1` / `down_robot2`.
  → Hoy DOWN compila idéntico para ambos robots; para R2 (sin OTOS) hay que crear un env nuevo
  o cambiar el flag. Ver §5.

**Conclusión del mecanismo:** la selección por robot está **fragmentada en 3 lugares**
(macro TOP, macro CENTRAL, flags DOWN) y NO existe un único "robot definition". Ese es el objetivo
de diseño. Las cámaras (§6) son un 4º lugar, fuera del Teensy.

---

## 2. MOTORES (placa CENTRAL — Teensy 4.1, Zircon Rev v15)

Archivo: `src/central/config_central.h`. **Ya es PER-ROBOT (HW)** vía `#if defined(ROBOT1)/#elif ROBOT2`.

| Referencia | archivo:línea | valor ROBOT1 (arquero) | per-robot? | cómo se selecciona | nota ROBOT2 (delantero) |
|---|---|---|---|---|---|
| M1 pines (INA/INB/PWM) | `config_central.h:25-27` | 2 / 5 / 3 (driver U5) | **PER-ROBOT (HW)** | `#if ROBOT1` | R2 `:43-45` = 8/7/6 (driver U17). **Pines ROTADOS** vs R1. |
| M2 pines | `config_central.h:29-31` | 8 / 7 / 6 (driver U17) | **PER-ROBOT (HW)** | `#if ROBOT1` | R2 `:47-49` = 11/12/4 (driver U7). |
| M3 pines | `config_central.h:33-35` | 11 / 12 / 4 (driver U7) | **PER-ROBOT (HW)** | `#if ROBOT1` | R2 `:51-53` = 2/5/3 (driver U5). |
| `MOTOR_INVERT[3]` | `config_central.h:41` (R1) / `:62` (R2) | `{+1, -1, +1}` (R1 **VALIDADO en banco**) | **PER-ROBOT (HW)**; R1 validado, R2 sin validar | `#if ROBOT1/ROBOT2` | **ROBOT1 `{+1,-1,+1}` = VALIDADO en banco** (María/Elías 2026-06-01 con `diag_central_line_sweep_robot1`; re-confirmado 2026-06-06 commit 8956d10 tras rearmar el robot: "se mueve derecho" + "esquiva la línea"). El M2=U17 está INVERTIDO por HW (INA/INB cruzados) → `{+1,-1,+1}` es el correcto y NO está en duda. El "sin validar / A-CONFIRMAR-EN-BANCO" aplica **SOLO a ROBOT2**: R2 copia `{+1,-1,+1}` de R1 sin probar. ⚠️ **A-CONFIRMAR-EN-BANCO (solo R2)**: el comentario `:58-61` advierte que como los pines están rotados, el índice 1 en R2 es U7 (no U17). Si la inversión real es del driver U17 (índice 0 en R2), el correcto sería `{-1,+1,+1}`. El usuario confirma: motores R2 posiblemente soldados en dirección y/o pines distintos → **validar con `diag_central_motors_robot2`**. |
| `WHEEL_ANGLES_DEG[3]` | `config_central.h:87` | `{60, -60, 180}` | **HARDCODED-COMÚN** | ninguno (fuera del `#if`) | TENTATIVO ambos robots ("da círculos", ver memoria eval). No está dentro del `#if ROBOT` → hoy es igual para los dos. Si las ruedas de R2 montan distinto, esto debería volverse per-robot. **A-CONFIRMAR-EN-BANCO.** |
| `WHEEL_RADIUS_MM` | `config_central.h:83` | 100.0 | **HARDCODED-COMÚN** | ninguno | tentativo; per-robot sólo si los chasis difieren. |
| `MAX_SPEED_MM_S` | `config_central.h:93` | 1000.0 | **HARDCODED-COMÚN** | ninguno | estimado, igual para ambos. |
| `MAX_PWM` | `config_central.h:92` | 255 | **HARDCODED-COMÚN** | ninguno | constante del core. |
| `MOTOR_MIN_PWM` / `MOTOR_PWM_NOISE_THRESH` | `config_central.h:105-106` | 0 / 0 (deadzone OFF) | **HARDCODED-COMÚN** | ninguno | per-robot potencial (cada motor arranca distinto), hoy default 0. |

**Fuente del mapeo:** `hardware/electronics/mapa-pines-teensy-ambos-robots.md` + journal
`2026-03-20-diferencias-pines-motores-arquero-delantero.md` (citados en `config_central.h:13-15`).
El driver INVERTIDO por HW en R1 es U17 (validado banco María/Elías).

---

## 3. IMU / BNO (placa TOP — Teensy 4.0)

Archivos: `src/top/sensors_imu.cpp/.h`, `src/top/pinout_common.h`. **HARDCODED-COMÚN** (ningún `#if ROBOT`).

| Referencia | archivo:línea | valor actual (ROBOT1) | per-robot? | cómo se selecciona | nota ROBOT2 |
|---|---|---|---|---|---|
| Nº de BNO | `sensors_imu.cpp:43-46`, `imu_fusion.h` `IMU_FUSION_N` | 2 slots (LEFT+RIGHT), hoy 1 sano | **HARDCODED-COMÚN** | ninguno | R2 tiene 2 BNO **funcionales** (uno por bus). |
| **Bus de cada BNO** | `sensors_imu.cpp:43-44` | **AMBOS en `&Wire`** (18/19) | **HARDCODED-COMÚN** | ninguno | ⚠️ **MISMATCH R2**: el usuario dice que R2 tiene el 2º BNO en **`Wire1`** (soldó cables por debajo del Teensy). El firmware hoy pone los 2 en `Wire`. Para R2 el 2º BNO debe leerse de `Wire1`. **Cambio de código requerido** (no sólo config). |
| Direcciones BNO | `pinout_common.h:26-27` | LEFT 0x28, RIGHT 0x29 (ADR puenteado) | **HARDCODED-COMÚN** | ninguno | ⚠️ **MISMATCH R2**: el usuario dice que los 2 BNO de R2 comparten la **MISMA base 0x28** (no se distinguen por dirección; se distinguen por BUS). El esquema R1 (0x28 vs 0x29 mismo bus) NO aplica a R2. |
| Detección / readiness | `sensors_imu.cpp:48` `g_ready[]`, `:222-238` sondeo 0x29 con chip-id 0xA0 | sondea 0x29 en `Wire`, init sólo si es BNO real | **HARDCODED-COMÚN** | ninguno | La heurística "0x29 = 2º BNO" es específica de R1. En R2 el 2º BNO está en `Wire1@0x28` → la detección actual NO lo encontraría. **A-CONFIRMAR / rediseñar para R2.** |
| `HEADING_SIGN` | `sensors_imu.cpp:83` | `-1.0f` (chip CW+ → firmware CCW+) | **HARDCODED-COMÚN** | ninguno | A-CONFIRMAR-EN-BANCO en R2: si el BNO se montó con otra orientación, el signo puede diferir (medido en banco R1 2026-05-31). |
| `BNO_READ_INTERVAL_MS` | `sensors_imu.cpp:78` | 50 (≈20 Hz, band-aid BNO+ToF en `Wire`) | **HARDCODED-COMÚN** | ninguno | En R2, con el 2º BNO en `Wire1` (bus aparte), la contención BNO+ToF podría no existir → se podría leer más rápido. Oportunidad, no bloqueante. |
| Clock I²C | `sensors_imu.cpp:188` `Wire.setClock(100000)` | 100 kHz (coexistencia BNO+ToF) | **HARDCODED-COMÚN** | ninguno | idem: R2 con BNO en `Wire1` podría volver a 400 kHz en ese bus. |
| `HEADING_SAMPLES`, timeouts init | `sensors_imu.cpp:72-75` | 10 / 3000 / 1000 / 2000 ms | **HARDCODED-COMÚN** | ninguno | iguales. |
| Detector BNO congelado | `sensors_imu.cpp` `#ifdef TOP_ENABLE_BNO_FREEZE_DETECT` | OFF en competencia | flag de env (`top_robot1_bnofreeze`) | `-DTOP_ENABLE_BNO_FREEZE_DETECT` | no per-robot; gateado por flag. |

> **Nota CENTRAL:** el CENTRAL **ya NO conecta BNO** (`config_central.h:67-77`): las constantes
> `BNO055_*` y el módulo `imu_zircon` sólo se compilan con `-DCENTRAL_HAS_LOCAL_BNO` (default OFF).
> El heading le llega por WorldSnapshot del TOP. No es per-robot.

---

## 4. ToF (placa TOP — Teensy 4.0)

Archivos: `src/top/pinout_robot1.h`, `pinout_robot2.h`, `pinout_common.h`, `sensors_tof.cpp/.h`.
Estructura **PER-ROBOT** (arrays en `pinout_robotN.h`) pero los valores R2 son **copia sin validar** de R1.

| Referencia | archivo:línea | valor ROBOT1 | per-robot? | cómo se selecciona | nota ROBOT2 |
|---|---|---|---|---|---|
| Modelo de sensor | lib `Adafruit_VL53L7CX`, usado en `sensors_tof.cpp:34,54,70` | VL53L7CX | **HARDCODED-COMÚN** | ninguno (incluido en `.cpp`) | ⚠️ usuario: R2 usa **modelo DISTINTO** de ToF ("mismo circuito, dispuesto distinto, ~rotado 90°"). Si el modelo difiere (p.ej. VL53L5CX/L8CX — ambas libs están en `lib/`), el `.cpp` instancia `Adafruit_VL53L7CX` fijo → **A-CONFIRMAR-EN-BANCO / posible cambio de código per-robot.** |
| Nº de ToF (`NUM_TOF`) | `pinout_common.h:137` + `static_assert :141` | 4 | **HARDCODED-COMÚN** | ninguno (común) | static_assert ata a 4; arrays de `localization.h` son `[4]`. |
| `NUM_TOF_ACTIVE` | `pinout_robot1.h:86` / `pinout_robot2.h:53` | 4 | **PER-ROBOT** | pinout_robotN.h | R2 = 4 (asumido). |
| `PIN_TOF_XSHUT[4]` (pines LP) | `pinout_robot1.h:45-50` / `pinout_robot2.h:26-31` | `{9,10,11,12}` | **PER-ROBOT** | pinout_robotN.h | R2 = `{9,10,11,12}` **copiado de R1, SIN validar en banco** (el header `:18-23` lo dice explícito). A-CONFIRMAR-EN-BANCO con `diag_top_tof_quad_live`. |
| `TOF_I2C_ADDR_ASSIGNED[4]` | `pinout_robot1.h:64-69` / `pinout_robot2.h:38-40` | `{0x2A,0x2B,0x2C,0x2D}` | **PER-ROBOT** | pinout_robotN.h | R2 igual (asumido). |
| **Posición física por índice** | `pinout_robot1.h:32-37` (comentario) + `TOF_MOUNT_ANGLE_DEG` | [0]=FRENTE [1]=ATRÁS [2]=DERECHA [3]=IZQUIERDA | mezcla | índice mapea posición | ⚠️ usuario: en R2 los ToF están **"rotados ~90°"** → el mapeo índice→posición y/o los mount angles **DIFIEREN**. Hoy `TOF_MOUNT_ANGLE_DEG` es común (ver abajo). **A-CONFIRMAR / volver per-robot.** |
| `TOF_MOUNT_ANGLE_DEG[4]` | `pinout_common.h:104` | `{0,180,270,90}` | **HARDCODED-COMÚN** ⚠️ | ninguno | ⚠️ **debería ser PER-ROBOT** si R2 tiene los ToF rotados 90°. Hoy está en `pinout_common.h` (común a ambos). Mover a `pinout_robotN.h` para R2. **A-CONFIRMAR-EN-BANCO.** |
| **FOV / apertura por sensor** | (no existe constante) | implícito 4x4 zonas, ~60° (VL53L7CX) | **HARDCODED-COMÚN / AUSENTE** | ninguno | ⚠️ usuario: **un ToF de R2 tiene ~40° de FOV** (vs ~60°). HOY NO existe ninguna constante de FOV/apertura por sensor en el firmware. La resolución (`TOF_RESOLUTION_ZONES=16`, `sensors_tof.cpp:107`) y freq (`:108`) son comunes. **Falta modelar FOV per-sensor/per-robot.** |
| `TOF_RESOLUTION_ZONES` / `TOF_RANGING_FREQ_HZ` | `sensors_tof.cpp:107-108` | 16 (4x4) / 15 Hz | **HARDCODED-COMÚN** | ninguno | iguales; revisar si el modelo distinto de R2 soporta misma config. |
| `TOF_OFFSET_MM` | `pinout_common.h:115` | 95 (placeholder, plano-sensor→centro) | **HARDCODED-COMÚN** | ninguno | A-CONFIRMAR-EN-BANCO ambos robots; per-robot si difiere el chasis/posición. |
| `TOF_STALE_TIMEOUT_MS` | `sensors_tof.h:45` | 250 | **HARDCODED-COMÚN** | ninguno | igual. |
| `LOCALIZATION_OUTLIER_THRESHOLD_MM` | `pinout_common.h:107` | 300 | **HARDCODED-COMÚN** | ninguno | igual. |
| Polaridad LP, mux/enumeración | `sensors_tof.cpp:71-73` (`LP_WAKE_LEVEL=HIGH`) | activo-ALTO, enumeración por LP | **HARDCODED-COMÚN** | gateado por `TOP_ENABLE_MULTI_TOF` | igual (mismo bodge asumido en R2). |
| `PIN_HCSR04_TRIG/ECHO` | `pinout_common.h:64-65` | 4 / 3 | **HARDCODED-COMÚN** | ninguno; activo con `-DTOP_ENABLE_HCSR04` | iguales (común). |

---

## 5. OTOS (placa DOWN — Teensy 4.0)

Archivos: `src/down/config_down.h`, `src/down/otos.cpp`, `src/down/main_down.cpp`.
Selección por **flag numérico de env**, NO por `ROBOT1/ROBOT2`.

| Referencia | archivo:línea | valor actual | per-robot? | cómo se selecciona | nota ROBOT2 |
|---|---|---|---|---|---|
| Presencia / cantidad | `config_down.h:37-44` (`NUM_OTOS = DOWN_NUM_OTOS_CONNECTED`) | 2 (vía `[env:down]`) | **por-env (no por robot)** | `-DDOWN_NUM_OTOS_CONNECTED` en `platformio.ini:102` | ⚠️ **MISMATCH R2**: R2 **NO tiene OTOS** (no llegaron). Hay que compilarlo con `-DDOWN_NUM_OTOS_CONNECTED=0`. **Hoy NO existe un env `down_robot2`** → hay que crearlo (o bajar el flag). El código YA soporta 0: `otos.cpp:81,86` usan `if (NUM_OTOS >= 1)` / `>= 2`, así que con 0 saltea ambos `begin()` y `otos_init()` retorna false (degradado). |
| `otos_init()` levanta Wire+Wire1 | `otos.cpp:72-73` | `Wire.begin(); Wire1.begin();` siempre | **HARDCODED-COMÚN** | ninguno | con NUM_OTOS=0 igual levanta los 2 buses (inocuo, pero a revisar si en R2 esos pines se usan para otra cosa). |
| Bus de cada OTOS | `otos.cpp:32-33` | U5→`Wire` (18/19), U6→`Wire1` (16/17) | **HARDCODED-COMÚN** | ninguno | N/A en R2 (sin OTOS). |
| `OTOS_I2C_ADDR` | `config_down.h:111` | 0x17 | **HARDCODED-COMÚN** | ninguno | N/A R2. |
| `OTOS_SEPARATION_MM` | `config_down.h:122` | 200.0 (tentativo) | **HARDCODED-COMÚN** | ninguno | A-CONFIRMAR-EN-BANCO; afecta sólo slip (no heading). N/A R2 (sin OTOS). |
| Calibración IMU del OTOS (bloqueante ~0.5 s) | `otos.cpp:62-66`, `main_down.cpp:62,94` | se corre en init | per-presencia | gated por `begin()==true` | con 0 OTOS no corre → boot R2 más rápido. |
| **Consumidores que asumen OTOS** | `shared/otos_position.*`, `shared/pose_fusion.*`, `shared/otos_fusion.h`, `down_tx` (pose/vel) | computan pose/vel del centro | degradan solos | en runtime (alive=false) | el firmware ya degrada: sin OTOS la pose/vel quedan N/A y los consumidores (drive_straight OTOS, GK paralelo, cross_track) caen al fallback exacto (ver memoria broadcast_simetrico_down). **Verificar que R2 corre estable con NUM_OTOS=0.** |

> **DOWN NO tiene BNO.** El heading de DOWN (cuando hay OTOS) sale de los OTOS; sin OTOS, DOWN no
> aporta heading. (memoria: CENTRAL no tiene BNO; heading viene del TOP).

---

## 6. CÁMARAS — distancia (homografía) + color (LAB)

**Las cámaras son OpenMV N6 (×2). Su calibración NO vive en el firmware Teensy: vive en los scripts
`.py` de CADA cámara**, fuera de `software/teensy`. Hoy es **PER-CÁMARA** (front vs back), NO per-robot
(los archivos no están duplicados por robot → un único juego de scripts para ambos robots, lo cual es
un problema si R1 y R2 tienen cámaras/montajes distintos).

### 6a. Lo que vive en el FIRMWARE Teensy (común a ambos robots, escala cruda→mm)
| Referencia | archivo:línea | valor | per-robot? | nota |
|---|---|---|---|---|
| `CAMERA_UNIT_TO_MM` | `src/top/cameras_runtime.cpp:27` | 10.0 (1 unidad ≈ 1 cm, **placeholder**) | **HARDCODED-COMÚN** | TODO `:25-26`: calibrar contra cancha (30/50/80/100 cm). El TOP sólo escala; la geometría real está en la homografía del `.py`. |
| `CAMERA_TIMEOUT_MS` | `cameras_runtime.cpp:17` | 1000 | **HARDCODED-COMÚN** | watchdog por cámara. |
| Parser / contrato wire v2 | `src/top/cameras.h:64-71`, `cameras.cpp` | 11 B, offset 100, CRC8, END 254 | **HARDCODED-COMÚN** | igual ambos robots (contrato del enlace). |
| Fusión dual (back = 180°) | `src/shared/cameras_fusion.*` | rota la trasera 180° | **HARDCODED-COMÚN** | igual. |

### 6b. Lo que vive en los SCRIPTS `.py` (per-cámara — y HOY no per-robot)
Front: `hardware/electronics/cameraFront-pack/firmware/openmv/cam-frontal-n6.py`
Back:  `hardware/electronics/cameraBack-pack/firmware/openmv/cam-trasera-n6.py`

| Referencia (DISTANCIA / homografía) | front archivo:línea | back archivo:línea | valor | per-? | nota |
|---|---|---|---|---|---|
| `H_MATRIX` (homografía 3×3) | `cam-frontal-n6.py:54-58` | `cam-trasera-n6.py:52-56` | matriz placeholder (idéntica en ambas hoy) | **PER-CÁMARA** (debe diferir front/back) | ⚠️ **placeholder en ambas** — recalibrar con 4 puntos en el suelo (la trasera con puntos DETRÁS). TASK-022. Si R1 y R2 montan la cámara a distinta altura/ángulo, **la H debería ser PER-ROBOT-Y-PER-CÁMARA** (4 matrices), hoy hay 2 (y son iguales). |
| `CAM_HEIGHT_CM` | `cam-frontal-n6.py:59` | `cam-trasera-n6.py:57` | 18.7 (⚠️ MEDIR) | **PER-CÁMARA** | A-CONFIRMAR; puede diferir entre robots y entre front/back. |
| `BALL_RADIUS_CM` | `cam-frontal-n6.py:60` | `:58` | 13.5/(2π) (circunferencia pelota) | común | constante de la pelota RCJ. |
| corrección perspectiva | `cam-frontal-n6.py:152-153` | `:147-148` | usa height/radius | per-cámara | deriva de los dos de arriba. |

| Referencia (COLOR / LAB) | front archivo:línea | back archivo:línea | valor | per-? | nota |
|---|---|---|---|---|---|
| `NARANJA_THRESHOLD` (pelota) | `cam-frontal-n6.py:63` | `cam-trasera-n6.py:61` | `(21,67,18,79,-32,127)` | **PER-CÁMARA** | ⚠️ RECALIBRAR en cada N6 (sensor PAG7936). HOY front==back. TASK-022 = bloqueante real #1. |
| `AMARILLO_THRESHOLD` (arco) | `cam-frontal-n6.py:64` | `:62` | `(17,70,-27,14,38,111)` | **PER-CÁMARA** | idem. |
| `AZUL_THRESHOLD` (arco) | `cam-frontal-n6.py:65` | `:63` | `(4,36,-13,57,-64,-4)` | **PER-CÁMARA** | idem. |
| `*_PIXELS_MIN` | `cam-frontal-n6.py:67-69` | (back) | 20 / 600 / 300 | per-cámara | umbral de área. |
| `HMIRROR` / `VFLIP` | `cam-frontal-n6.py:50-51` | `:47-48` | True / True (montaje 180°) | **PER-CÁMARA** | depende del montaje físico → potencialmente per-robot. |
| `EXPOSURE_US` | `cam-frontal-n6.py:47` | `:43` | 37000 (⚠️ RE-MEDIR) | per-cámara | sólo si BRING_UP=False. |
| `BRING_UP` | `cam-frontal-n6.py:41` | (back) | True (calibración) → False competencia | per-cámara | flag operativo. |
| Filtros de forma pelota (`BALL_*`) | `cam-frontal-n6.py:76-85` | (back) | density/aspect/roundness | per-cámara | tuneables banco. |

**Para el "robot definition" único** (objetivo del usuario): la calibración de **DISTANCIA (homografía)**
y la de **COLOR (LAB)** hoy NO están versionadas por robot. Si R2 lleva otras cámaras o montaje, hace
falta o bien duplicar los packs por robot, o bien parametrizar los `.py` con un archivo de config por
robot. **Decisión de diseño abierta:** ¿la calibración de COLOR/LAB también va en el robot-definition?
(la de distancia casi seguro sí). Templates existentes: `target-cam-frontal-template.py` /
`target-cam-trasera-template.py` en cada pack.

> **Nota wire-breaking:** el contrato cámara→TOP v2 y el snapshot TOP→CENTRAL v3 son wire-breaking;
> cambiar la calibración NO toca el wire, pero re-flashear cámaras requiere coordinar (memoria
> wire_contracts). La homografía debe alinearse a la convención simétrica [-100,100] (memoria vision_openmv_n6).

---

## 7. GEOMETRÍA del robot

| Referencia | archivo:línea | valor | per-robot? | nota |
|---|---|---|---|---|
| Posiciones de los 32 sensores de línea | `src/shared/sensor_geometry.h:32` + `.cpp` (`SENSOR_POS[32]`) | LUT del PCB DOWN 2026-04-12 | **HARDCODED-COMÚN** | extraído del schematic; igual si ambos robots usan la misma placa DOWN. A-CONFIRMAR montaje (TASK-027, `:8-9`). |
| Origen = centro robot | `sensor_geometry.h:8-9` | asumido centro PCB = centro robot | común | validación de montaje pendiente. |
| Radio del robot (ToF→centro) | `pinout_common.h:115` `TOF_OFFSET_MM`=95 | placeholder | común | ver §4. |
| Radio a las ruedas | `config_central.h:83` `WHEEL_RADIUS_MM`=100 | tentativo | común | ver §2. |
| Dimensiones de cancha | `pinout_common.h:82-83` | 1820 × 2430 | común (no es del robot) | ver §8. |

No hay constantes de "diámetro/dimensión del chasis" per-robot en el firmware más allá de los radios
de arriba. Si los chasis difieren, `TOF_OFFSET_MM` / `WHEEL_RADIUS_MM` / `WHEEL_ANGLES_DEG` son los
candidatos a volverse per-robot.

---

## 8. CANCHA (común — NO per-robot, listado para descartar)

| Referencia | archivo:línea | valor | nota |
|---|---|---|---|
| `FIELD_WIDTH_MM` | `pinout_common.h:82` | 1820 (eje X, lado corto) | convención de ejes (memoria field_axis_convention). |
| `FIELD_HEIGHT_MM` | `pinout_common.h:83` | 2430 (eje Y, arco-a-arco) | NO re-invertir. |
| `LINE_DEFAULT_THRESHOLD` | `config_down.h:148` | 500 | calibrable por EEPROM; no per-robot estructural. |

---

## 9. PUERTOS DE COMUNICACIÓN entre placas y cámaras — **IGUALES en ambos robots (confirmado)**

El usuario afirma que los puertos de comunicación entre placas y con las cámaras son iguales en ambos
robots. La auditoría lo **confirma**: están en `pinout_common.h` / `config_central.h` / `config_down.h`
SIN ningún `#if ROBOT` → son comunes por construcción.

| Enlace | archivo:línea | Serial / pines | baud | per-robot? |
|---|---|---|---|---|
| DOWN → CENTRAL (LINE_URGENT) | `config_central.h:125-126` | Serial1 (RX0/TX1) | 230400 | **COMÚN** |
| TOP → CENTRAL (WorldSnapshot) | `config_central.h:123-124`; `pinout_common.h:50-54` | Serial4 en TOP (TX17) → Serial7 en CENTRAL (RX28) | 230400 | **COMÚN** |
| TOP ↔ COMM (status/partner) | `pinout_common.h:56` | Serial2 (RX7/TX8) | 115200 | **COMÚN** |
| DOWN → TOP | `config_down.h:127-128` | Serial5 (TX20/RX21) | 230400 | **COMÚN** |
| Cámara frontal → TOP | `cameras_runtime.cpp:35,97`; `pinout_common.h:55` | Serial3 (RX15) | 19200 | **COMÚN** |
| Cámara trasera → TOP | `cameras_runtime.cpp:36,98`; `pinout_common.h:57` | Serial5 (RX21) | 19200 | **COMÚN** |
| Árbitro → TOP (start/stop) | `comm_arbiter.cpp:40-41` | **GPIO** pin5=OUT1 / pin6=OUT2 (NO UART), match = pin5 OR pin6, INPUT_PULLDOWN | nivel | **COMÚN** (memoria UART↔pin map) |

**Baudios comunes** (`pinout_common.h:49-57`): `UART_FROM_DOWN_BAUD`/`UART_TO_ZIRCON_BAUD`/
`UART_TO_COMM_BAUD`/`UART_CAMERA1_BAUD`/`UART_CAMERA2_BAUD`. Ninguno bajo `#if ROBOT`.

---

## 10. Placa COMM (ESP32-C6) — ¿dónde vive su firmware?

**El firmware de la placa COMM NO está en este repo como código fuente.**

- `software/communication/` contiene **sólo un `README.md`** (`software/communication/README.md`),
  no código.
- El README (`:9-19`) indica que la placa COMM es un **fork IITA del módulo oficial RCJ**, MCU
  **ESP32-C6-MINI-1-N4**, y que el firmware es el **oficial branch `esp32-c6`** (externo), que se
  **flashea** siguiendo `hardware/electronics/comm-board/2026-05-17-procedimiento-flash-firmware-c6.md`.
- Documentación de la placa (componentes/circuito/pinout): `hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md` y `RECURSOS-Y-ENLACES.md`.
- Journals: `journal/2026-05-15-firmware-comm-c6-flash-procedure.md`, `journal/2026-05-17-analisis-3-placas-y-correccion-firmware-c6.md`; research: `research/completed/2026-05-17-firmware-comm-rcj-branch-esp32-c6.md`.
- ESP-NOW entre robots: **NO implementado** en el firmware oficial v0.91 (README `:21-24`).

**Conclusión:** firmware COMM = AUSENTE del repo (es upstream RCJ, sólo docs de flasheo). No es
per-robot (mismo firmware oficial para los 2). El start/stop del árbitro entra al TOP por GPIO 5/6.

---

## 11. RESUMEN EJECUTIVO

### Mecanismo de selección actual (fragmentado en 4 lugares)
1. **TOP** → macro `-DROBOT1/-DROBOT2` (envs `top_robot1/2`) → `hardware_profile.h` → `pinout_robotN.h`.
2. **CENTRAL** → macro `-DROBOT1/-DROBOT2` (envs `central_robot1/2`) → `#if` en `config_central.h`.
3. **DOWN** → flags `-DDOWN_NUM_MUXES_CONNECTED` / `-DDOWN_NUM_OTOS_CONNECTED` (un solo `[env:down]`, **NO** hay `down_robot1/2`).
4. **CÁMARAS** → scripts `.py` por cámara (front/back), **fuera del Teensy, no versionados por robot**.

NO existe un único "robot definition". Es exactamente lo que el agente de diseño debe crear.

### Referencias que VARÍAN o DEBERÍAN variar por robot (conteo)
- **Ya PER-ROBOT y bien (HW):** motores (pines M1/M2/M3 + MOTOR_INVERT en `config_central.h`); arrays ToF (`PIN_TOF_XSHUT`, `TOF_I2C_ADDR_ASSIGNED`, `NUM_TOF_ACTIVE`, `ROBOT_HAS_*` en `pinout_robotN.h`). **≈ 8 referencias.**
- **PER-ROBOT pero R2 = copia SIN validar (A-CONFIRMAR-EN-BANCO):** `MOTOR_INVERT` R2, `PIN_TOF_XSHUT` R2, mapeo posición ToF R2. **≈ 3.**
- **HARDCODED-COMÚN que DEBERÍAN ser per-robot según datos del usuario:** **≈ 7 críticas**
  1. Bus del 2º BNO (`Wire` vs **`Wire1`** en R2) — `sensors_imu.cpp:43-44` ⚠️ **MISMATCH + cambio de código**.
  2. Direcciones BNO (0x28/0x29 mismo bus vs **2×0x28 en buses distintos** en R2) — `pinout_common.h:26-27` ⚠️ **MISMATCH**.
  3. Detección del 2º BNO (heurística 0x29 chip-id) — `sensors_imu.cpp:222-238` ⚠️ no sirve para R2.
  4. `TOF_MOUNT_ANGLE_DEG` (ToF de R2 rotados ~90°) — `pinout_common.h:104` (está en common, debería ir a pinout_robotN.h).
  5. Modelo de ToF (R2 ≠ VL53L7CX) — `sensors_tof.cpp` instancia `Adafruit_VL53L7CX` fijo.
  6. FOV/apertura por sensor (un ToF de R2 ~40°) — **constante AUSENTE en todo el firmware**.
  7. OTOS en DOWN (R2 = 0) — hay que `-DDOWN_NUM_OTOS_CONNECTED=0` + crear env (no hay `down_robot2`).
- **PER-CÁMARA y NO per-robot (cámaras):** homografía `H_MATRIX`×2, `CAM_HEIGHT_CM`×2, 3 LAB×2, HMIRROR/VFLIP, EXPOSURE — todos placeholders/sin recalibrar (TASK-022). **≈ 12 referencias** (×2 cámaras), candidatas a entrar al robot-definition (distancia sí; color = decisión abierta).
- **HARDCODED-COMÚN tentativos (per-robot si difieren chasis):** `WHEEL_ANGLES_DEG`, `WHEEL_RADIUS_MM`, `TOF_OFFSET_MM`, `OTOS_SEPARATION_MM`, `HEADING_SIGN`. **≈ 5.**

### Diferencias confirmadas de ROBOT2 vs el firmware actual (lo accionable)
| # | Subsistema | Lo que dice el usuario de R2 | Estado del firmware HOY | Acción |
|---|---|---|---|---|
| 1 | OTOS | R2 **sin OTOS** | `[env:down]` compila con 2 OTOS; no hay `down_robot2` | env nuevo con `-DDOWN_NUM_OTOS_CONNECTED=0` (código ya soporta 0). `pinout_robot2.h:51` además declara `ROBOT_HAS_OTOS 1` (TOP) — inconsistente con R2. |
| 2 | BNO | 2 BNO, uno en `Wire` y otro en **`Wire1`**, ambos **0x28** | firmware: 2 BNO en `Wire`, 0x28/0x29 | cambio de código en `sensors_imu.cpp` (leer 2º BNO de Wire1) — per-robot. |
| 3 | ToF | modelo distinto, rotados ~90°, uno ~40° FOV | modelo/ángulos/FOV comunes | mover `TOF_MOUNT_ANGLE_DEG` a per-robot; parametrizar modelo+FOV; validar pines en banco. |
| 4 | Motores | mismo modelo, posiblemente dirección/pines distintos por error de armado | pines ya per-robot; `MOTOR_INVERT` R2 = copia de R1 sin validar | `diag_central_motors_robot2` en banco → fijar pines+invert reales. |
| 5 | Cámaras | calibración distancia (homografía) y quizá color, en un robot-definition | en `.py` por cámara, no por robot, placeholders | diseñar config por robot; recalibrar (TASK-022). |
| 6 | Puertos comm | iguales en ambos | **confirmado común** | ninguna. |

### Notas para el agente de diseño
- `pinout_robot2.h` y la mitad CENTRAL ya dan el patrón "archivo por robot": el robot-definition puede
  extender ese patrón a IMU-bus, ToF-mount/modelo/FOV, OTOS-count y un puntero a la calibración de cámara.
- DOWN rompe el patrón (usa flags numéricos, no `ROBOT1/2`): unificar criterio (crear `down_robot1/2`
  que internamente seteen `DOWN_NUM_OTOS_CONNECTED`).
- `pinout_robot2.h:51 ROBOT_HAS_OTOS 1` contradice "R2 sin OTOS" — pero ese flag es del TOP (no usado para
  los OTOS reales, que viven en DOWN); aun así conviene corregirlo a 0 para coherencia del robot-definition.
