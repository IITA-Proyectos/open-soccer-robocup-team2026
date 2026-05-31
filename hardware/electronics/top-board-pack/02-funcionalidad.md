---
title: "Placa TOP — Especificación funcional (qué hace y cómo)"
date: 2026-05-24
status: vigente
parte-de: top-board-pack
basado-en: docs/firmware/FIRMWARE-PLACA-ARRIBA.md (curado, separando Nivel 1+2 vivo de Nivel 3+ aspiracional)
---

# Placa TOP — Especificación funcional

> Para el pinout completo del Teensy 4.0 master ver [`01-pinout-y-hardware.md`](01-pinout-y-hardware.md).
> Para el contrato binario del WorldSnapshot que TOP envía a CENTRAL ver [`03-contrato-datos-top.md`](03-contrato-datos-top.md).
> Para el protocolo de las cámaras OpenMV ver [`04-contrato-datos-camaras.md`](04-contrato-datos-camaras.md).
> Para el diseño general de comunicaciones de las 3 placas ver [`05-protocolo-comunicaciones.md`](05-protocolo-comunicaciones.md).
> Para el contexto general de las 3 placas ver [`06-arquitectura-3-placas.md`](06-arquitectura-3-placas.md).

## 1. Qué es la placa TOP

La placa TOP es el **cerebro sensorial** del robot. No toma decisiones tácticas
ni controla motores — su trabajo es **ver, fusionar y entregar conocimiento**.

**Entrega al CENTRAL cada 10 ms (100 Hz)** un único struct `WorldSnapshot`
(27 B, v2) con:
- **Pose propia**: x, y, heading + vx, vy, omega + confianza 0–100.
- **Pelota**: posición + velocidad estimada + confianza. Si no es visible, se
  predice por inercia con confianza decayente.
- **Compañero** (futuro): pose recibida vía ESP-NOW.
- **Rivales** (futuro): hasta 2 oponentes estimados.
- **Arcos**: posiciones fijas conocidas, confirmadas cuando se ven.
- **Comando árbitro**: start/stop/halftime recibido por la placa COMM.
- **Flags**: match_running, in_own_penalty_area, partner_alive, partner_sees_ball, IMU degradado, etc.

## 2. ⚙️ Estado de implementación (lo vivo vs lo aspiracional)

**Esta es la separación más importante del doc**, porque la versión histórica
de `FIRMWARE-PLACA-ARRIBA.md` describe Nivel 3+ ambicioso que NO está
implementado todavía.

| Subsistema | Nivel | Estado | Dónde |
|---|---|---|---|
| Parser OpenMV (2 cámaras) | 1 | ✅ vivo | `firmware/top/cameras.{h,cpp}` |
| Fusión cámaras (front + back, rot 180°, watchdog) | 2 | ✅ vivo + 16 tests | `firmware/shared/cameras_fusion.{h,cpp}` + `firmware/top/cameras_runtime.{h,cpp}` |
| BNO055 dual (lectura + consistencia) | 2 | ✅ vivo | `firmware/top/sensors_imu.{h,cpp}` |
| HC-SR04 (frontal) | 1 | ✅ vivo (bloqueante 25 ms) | `firmware/top/sensors_tof.{h,cpp}` |
| **ToF VL53L7CX** | 4 (HW) | ⚠️ HW OK (enumeran 0x2A..0x2D), firmware lee 1 (Sprint B pendiente) | `firmware/top/sensors_tof.{h,cpp}` |
| Comm con COMM (Serial4) | 1 | ✅ vivo (arbiter) | `firmware/top/comm_arbiter.{h,cpp}` |
| Comm con DOWN (Serial1) | 1 | ✅ vivo | `firmware/top/comm_down.{h,cpp}` |
| Comm con CENTRAL (**Serial5**, ex-Serial2) | 1 | ✅ vivo | `firmware/top/comm_central.{h,cpp}` |
| **WorldSnapshot v2 con `ball_vx/vy`** | 2 | ⚠️ struct definido pero `cameras_runtime` no llena los campos (quedan en 0) | `firmware/shared/types.h` + tests `test_central_contract` |
| EKF de pose absoluta | 3 | ⏳ futuro | — |
| Kalman pelota (predicción cuando no se ve) | 3 | ⏳ futuro | — |
| Trilateración con ToF para pose XY | 3 | ⏳ futuro (requiere ToF activos) | — |
| Partner comm (ESP-NOW) | 3 | ⏳ **decisión: NO implementar para Incheon** | `docs/decisions/2026-05-17-comunicacion-inter-robot-superteam.md` |
| Estimación de rivales | 4 | ⏳ futuro (Mundial 2027) | — |

## 3. Responsabilidades funcionales

| # | Responsabilidad | Estado | Frecuencia |
|---|---|---|---|
| R1 | Inicializar y mantener los 2 BNO055 (modo IMUPLUS) | ✅ | 100 Hz |
| R2 | Detectar discrepancias entre los 2 BNO055 | ✅ | 100 Hz |
| R3 | Estrategia de reseteo del IMU ante impacto detectado | ⚠️ parcial | evento |
| R4 | Inicializar y leer los 4 ToF multizona | ❌ STUB | 15–30 Hz |
| R5 | Procesar matriz 8×8 de cada ToF para detectar paredes | ❌ futuro | 15–30 Hz |
| R6 | Trilateración con ToF → pose XY del robot | ❌ futuro | 15–30 Hz |
| R7 | Parser robusto del protocolo OpenMV (2 cámaras) | ✅ | ~30 Hz |
| R8 | Fusionar visión dual (frente + atrás) | ✅ | 30 Hz |
| R9 | Recibir ODOM_POSE/VEL del DOWN | ✅ | 100 Hz |
| R10 | Fusión sensorial EKF → pose propia con confianza | ❌ futuro | 100 Hz |
| R11 | Predicción Kalman de pelota | ❌ futuro | 100 Hz |
| R12 | Estimación de rivales | ❌ futuro | 30 Hz |
| R13 | Comm con placa COMM (recibir árbitros) | ✅ | 100 Hz |
| R14 | Recibir partner via COMM (ESP-NOW) | ❌ no para Incheon | 10 Hz |
| R15 | Construir `WorldSnapshot` y enviar a CENTRAL | ✅ | 100 Hz |
| R16 | Recibir comandos administrativos del CENTRAL | ✅ | eventos |
| R17 | Detección de IMU/cámara fallando | ✅ parcial | continuo |
| R18 | Recovery: degradación elegante si una fuente cae | ✅ | continuo |
| R19 | Diagnóstico USB + LED de estado | ✅ | 1 Hz |

## 4. Modos de operación

| Modo | Cuándo se activa | Comportamiento |
|---|---|---|
| `BOOT` | Al encender | Init de subsistemas en orden: I²C, IMU, ToF (stub), cámaras, UARTs |
| `CALIBRATING_IMU` | Comando al boot o `CENTRAL_RESET_TOP` con flag | Espera estabilización + calibración gyro de ambos BNO055 (~2-3 s) |
| `NORMAL` | Default tras `BOOT` | Loop completo: lectura + fusión + envío snapshot @ 100 Hz |
| `DEGRADED_IMU_SINGLE` | Si uno de los 2 BNO055 falla | Sigue con el otro. Flag en snapshot |
| `DEGRADED_NO_CAMERAS` | Si ambas cámaras caen | `ball_visible=0` siempre, confianza pose baja |
| `DEGRADED_NO_TOF` | Si los 4 ToF fallan | Pose XY queda en confianza 0 (solo heading IMU) — irrelevante hoy porque ToF es stub |
| `LOST` | Si todas las fuentes fallan | Envía snapshot vacío, parpadea LED. CENTRAL detecta y entra en modo seguro |

**TOP no conoce el rol del robot** (arquero/delantero) ni el estado del partido
(running/stop). Siempre reporta lo mismo. El estado del partido viene como
`referee_cmd` y `flags.match_running` en el snapshot.

## 5. Procesamiento del IMU dual (BNO055 × 2)

### 5.1 Lectura y frecuencia

Cada BNO055:
- **Modo IMUPLUS** (recomendado): fusión accel + gyro sin magnetómetro. Inmune a interferencia magnética de motores. Heading **relativo** (no Norte magnético).
- **Frecuencia interna**: 100 Hz.
- **Latencia I²C** (400 kHz): ~300 µs por lectura.

Polling del firmware: **100 Hz**.

### 5.2 Consistencia entre los 2 sensores

Los 2 BNO055 están físicamente a izquierda y derecha de la placa TOP. En
condiciones normales deberían reportar headings muy similares (diferencia < 2°).

Algoritmo (en `firmware/top/sensors_imu.cpp`):

```cpp
float diff = abs(wrap_diff(imu_left.heading, imu_right.heading));

if (diff < 2.0f) {
    fused_heading = (imu_left.heading + imu_right.heading) / 2.0f;
    imu_status = IMU_OK_BOTH;
} else if (diff < 10.0f) {
    // Discrepancia menor: usar el de mejor calibración o histórico
    fused_heading = (imu_left.calibration >= imu_right.calibration)
                    ? imu_left.heading : imu_right.heading;
    imu_status = IMU_DISAGREE_MINOR;
} else {
    // > 10°: uno saltó por impacto. Histórico de últimos 100 ms decide.
    fused_heading = resolve_imu_conflict_by_history();
    imu_status = IMU_DISAGREE_MAJOR;
}
```

### 5.3 Detección de impacto

`resolve_imu_conflict_by_history()` mira los últimos 10 readings de cada sensor
(últimos 100 ms a 100 Hz). El sensor que tuvo un salto súbito tiene `|omega|`
anómalamente alto vs el sensor estable. Se elige el más estable.

**Cuándo se considera roto/degradado un IMU**: `getCalibration()` reporta 0
durante > 5 segundos en NORMAL.

## 6. Procesamiento de cámaras OpenMV (2×)

### 6.1 Protocolo OpenMV (legacy del 2025)

Cada cámara envía packets de **9 bytes** por UART a 19200 baud (~2000
packets/s teóricos, en práctica ~30 Hz limitado por procesamiento OpenMV).

**Estructura del packet** (legacy, byte-a-byte):
- Byte 0: start marker.
- Bytes 1–2: ball_x (int16, mm relativos).
- Bytes 3–4: ball_y (int16, mm relativos).
- Byte 5: goal_color (cyan/magenta).
- Bytes 6–7: goal_x_relativo.
- Byte 8: checksum.

Implementación del parser: [`firmware/top/cameras.{h,cpp}`](firmware/top/cameras.h).
Wiring sobre los 2 UART: [`firmware/top/cameras_runtime.{h,cpp}`](firmware/top/cameras_runtime.h).

**Detalles completos del protocolo**: ver [`04-contrato-datos-camaras.md`](04-contrato-datos-camaras.md).

### 6.2 Fusión front + back

Las 2 cámaras OpenMV están montadas:
- **Cámara 1** (Serial3, U8) — frontal: mira hacia +Y del robot.
- **Cámara 2** (**Serial7**, U9, movida de Serial5 el 2026-05-29) — trasera: mira hacia −Y. **Sus coordenadas están rotadas 180°** y la fusión las corrige automáticamente.

Algoritmo de fusión (en `firmware/shared/cameras_fusion.{h,cpp}`):

```cpp
// Pseudocódigo de la fusión:
struct FusedView {
    bool ball_visible;
    float ball_x_mm, ball_y_mm;
    bool goal_visible;
    float goal_angle_deg;
};

FusedView fuse(CameraFrame front, CameraFrame back, uint32_t now_ms) {
    // 1. Aplicar rot 180° a back (porque la cámara mira hacia -Y)
    if (back.ball_visible) {
        back.ball_x_mm = -back.ball_x_mm;
        back.ball_y_mm = -back.ball_y_mm;
    }
    // 2. Watchdog: si una cámara no envió hace >500 ms, marcarla stale
    bool front_fresh = (now_ms - front.timestamp_ms) < 500;
    bool back_fresh  = (now_ms - back.timestamp_ms)  < 500;
    // 3. Pelota: si ambas la ven, usar la fresca + cercana. Si una sola, usar esa.
    // 4. Arco: idem.
    // ...
}
```

**Tests**: [`tests/test_cameras_fusion.cpp`](tests/test_cameras_fusion.cpp)
(16 tests cubren rot 180°, fuse front+back, watchdog stale, ambas ciegas).

### 6.3 Sentinel y watchdog (calibración Incheon)

⚠️ Pendiente para Incheon (TASK-022):
- **Sentinel**: detectar cámara con valores fijos (= no procesa) o ruido.
- **Exposición fija**: bloquear AGC/AWB para que la iluminación cambiante no descalibre.
- **Recalibración**: thresholds LAB se reajustan en el field de Incheon.

Detalles del workflow de calibración: ver skill `openmv-vision-tuning` del repo.

## 7. Procesamiento de ToF multizona — HW OK, firmware pendiente (HAL Sprint B)

### 7.1 Estado actual (2026-05-30)

| ToF | Estado | Razón |
|---|---|---|
| HC-SR04 (frontal) | ⚠️ gateado OFF | `pulseIn` colisionaba con el pin del uplink; deshabilitado por `#ifdef TOP_ENABLE_HCSR04`. El ToF frontal lo reemplaza. |
| 4× VL53L7CX | ✅ HW enumera / ⏳ firmware lee 1 | Hardware: los 4 enumeran a 0x2A..0x2D (banco 2026-05-30, bodge LP pines {9,10,11,12} activo-alto). Firmware: `sensors_tof.cpp` todavía lee solo 1 ToF — extenderlo a los 4 es HAL Sprint B. |

### 7.2 Lo que falta en firmware (HAL Sprint B)

1. **Enumeración al boot**: dormir todos los LP (pines **{9,10,11,12}**, LOW),
   despertar uno por uno (HIGH) y asignar **0x2A/0x2B/0x2C/0x2D** con
   `setAddress()`. ⚠️ Las direcciones I²C persisten con 3V3 → power-cycle al
   probar (no alcanza el reset). Pines/direcciones ya en `pinout_robot1.h`.
2. **Lectura a 15-30 Hz**: matriz 8×8 SPAD por chip = 64 distancias por chip,
   total 256 distancias.
3. **Procesamiento**: detectar pared más cercana en cada chip por mediana de
   las 8 columnas.
4. **Trilateración**: con 4 chips orientados a 90° entre sí (frente, atrás,
   izq, der), 4 distancias a pared → resolver pose XY del robot en la cancha
   (conociendo dimensiones del field).

El **hardware ya soporta los 4 ToF** (gran avance vs "1 frontal"); falta el
firmware que los explote. Diagnósticos de banco: `diag_top_tof_census` (enumera
+ verifica LP) y `diag_top_tof_quad_live` (lee los 4 + mapea posición). Plan de
escalado: 6 ToF (4 fijos + 2 móviles para pelota). Ver
`journal/2026-05-30-top-tof-4-en-bus-unico-enumeracion-ok.md`.

## 8. Fusión sensorial → WorldSnapshot

### 8.1 Lo que hay hoy (Nivel 1+2)

El firmware actual construye el `WorldSnapshot` con esta lógica simple:

```cpp
WorldSnapshot snap = {0};

// Pose propia (heading) → BNO055 fusionado
snap.heading_deg = fused_heading;
snap.heading_confidence = imu_status;

// Pose propia (xy) → recibida de DOWN (OTOS odometry)
snap.pose_x_mm = down_otos.x_mm;
snap.pose_y_mm = down_otos.y_mm;

// Pelota → cámaras fusionadas
FusedView fv = cameras_fuse(front, back, now);
snap.ball_visible = fv.ball_visible;
snap.ball_x_mm = fv.ball_x_mm;
snap.ball_y_mm = fv.ball_y_mm;
// snap.ball_vx_mm_s = 0;  // STUB — pendiente Kalman (cameras_runtime no llena estos)
// snap.ball_vy_mm_s = 0;

// Arco
snap.goal_visible = fv.goal_visible;
snap.goal_angle_centideg = fv.goal_angle_deg * 100;

// Árbitro
snap.referee_cmd = comm_arbiter_last_cmd();
snap.match_running = (snap.referee_cmd == REFEREE_START);

// Flags
snap.flags = (imu_degraded << 0) | (cameras_stale << 1) | ...;

// Envío
comm_central_send(snap);
```

### 8.2 Lo que falta (Nivel 3+)

- **EKF de pose XY**: fusionar OTOS + IMU + trilateración ToF + landmarks de
  cámaras (arcos vistos).
- **Kalman de pelota**: predecir posición futura cuando no se ve. Llenar
  `ball_vx_mm_s` y `ball_vy_mm_s` del `WorldSnapshot` v2 (campos definidos pero
  hoy quedan en 0 — el CENTRAL no puede intercep tar por velocidad todavía).
- **Estimación de rivales**: usar obstáculos ToF + cámaras filtrados (no son
  pelota ni partner).

## 9. Comunicación con la placa COMM (Serial4)

La placa COMM es un módulo ESP32-C6 separado que:
- Recibe comandos del árbitro por **BLE**.
- Mantiene **ESP-NOW** con el robot partner (futuro).
- Sirve de bridge a TOP por UART (Serial4, 115200 baud).

Implementación: [`firmware/top/comm_arbiter.{h,cpp}`](firmware/top/comm_arbiter.h).

| Comando árbitro | Acción en TOP |
|---|---|
| `START` | `match_running = true` en snapshot |
| `STOP` | `match_running = false` |
| `HALFTIME` | `match_running = false` + flag halftime |
| `PENALTY` (futuro) | `match_running = false` + flag penalty |

## 10. Comunicaciones (resumen)

Detalles byte-a-byte de las tramas TOP→CENTRAL: [`03-contrato-datos-top.md`](03-contrato-datos-top.md).
Detalles del protocolo OpenMV: [`04-contrato-datos-camaras.md`](04-contrato-datos-camaras.md).
Diseño general de las 3 placas: [`05-protocolo-comunicaciones.md`](05-protocolo-comunicaciones.md).

### 10.1 Resumen de los 5 UARTs (ver `01-pinout` §2.2)

| Serial | Pines RX/TX | A quién | Qué pasa |
|---|---|---|---|
| Serial1 | 0/1 | ← DOWN | RX: DOWN_OTOS_POSE + DOWN_OTOS_VEL + LINE_STATUS @ 100 Hz |
| **Serial5** | **20/21** | → CENTRAL | TX: **WORLD_SNAPSHOT @ 100 Hz** (✅ corregido 2026-05-29: era "Serial2 7/8") |
| Serial3 | 15/14 | ← cámara 1 | RX: parser OpenMV (9 bytes/packet) |
| Serial4 | 16/17 | ↔ COMM | RX: comando árbitro + partner. TX: comandos al árbitro (futuro) |
| **Serial7** | **28/29** | ← cámara 2 | RX: parser OpenMV (⚠️ provisional, movida de Serial5) |

### 10.2 Heartbeat / watchdogs

**No hay heartbeat explícito** (igual que DOWN y CENTRAL). Stream continuo =
heartbeat implícito.

| Watchdog | Trigger | Acción |
|---|---|---|
| DOWN timeout | 500 ms sin frames del DOWN | Pose XY → confianza 0, IMU heading sigue |
| COMM timeout | 500 ms sin frames del COMM | `referee_cmd = STOP` (failsafe) |
| Cámara 1 timeout | 500 ms sin packets | Watchdog del cameras_fusion marca cámara stale |
| Cámara 2 timeout | 500 ms sin packets | Idem |
| IMU dual roto | Ambos `getCalibration() = 0` por > 5 s | `imu_degraded = true` |

## 11. Timing y latencias

### 11.1 Loop principal

```
loop():
    comm_central_tick()           # drena RX desde CENTRAL (~10 µs)
    comm_down_tick()              # drena RX desde DOWN (~50 µs)
    comm_arbiter_tick()           # drena RX desde COMM (~10 µs)
    cameras_runtime_tick()        # parser de 2 cámaras (~100 µs)

    if since_imu_tick >= 10 ms:
        sensors_imu_tick()        # 2× I²C reads (~600 µs)

    if since_tof_tick >= 30 ms:   # HC-SR04 únicamente (los VL53 son stub)
        sensors_tof_hcsr04_tick() # ~25 ms BLOQUEANTE ⚠️

    if since_strategy_tick >= 10 ms:
        build_world_snapshot()    # fusión + construir struct (~200 µs)
        comm_central_send(snap)   # ~1.5 ms UART async

    if since_debug_print >= 1000 ms:
        debug_print()             # ~500 µs
```

> ⚠️ **El HC-SR04 es bloqueante (~25 ms)** — durante la espera del eco, el
> loop NO procesa cámaras ni IMU. Por eso se llama solo cada 30 ms y no más
> frecuente. Si se necesita más responsividad, usar interrupts. Plan B:
> reemplazar HC-SR04 con los ToF VL53 una vez que se active el código.

### 11.2 Latencia evento → WORLD_SNAPSHOT → CENTRAL

| Etapa | Tiempo |
|---|---|
| Sensor detecta (cámara, OTOS, etc.) | (variable) |
| Próximo tick del cameras_runtime / comm_down | < 10 ms |
| Construcción del snapshot | < 1 ms |
| TX UART Serial2 a 230400 (~31 bytes) | ~1.5 ms |
| Decode en CENTRAL | ~50 µs |
| **Total TOP → CENTRAL** | **~13 ms** |

## 12. Diagnóstico y debug

### 12.1 LED de estado (pin Arduino 13 = LED_BUILTIN)

| Patrón | Significado |
|---|---|
| Apagado | Firmware no inició o en `setup()` |
| Encendido fijo | NORMAL, todos los sensores OK |
| Parpadeo lento (1 Hz) | DEGRADED — alguna fuente caída |
| Parpadeo rápido (5 Hz) | LOST — todas las fuentes caídas |
| 3 parpadeos + pausa | Calibrando IMU |

### 12.2 USB Serial (debug humano)

A 115200 baud por el puerto USB del Teensy 4.0:
- En `setup()`: estado de cada subsistema (IMU OK, ToF stub, cámaras conectadas).
- Cada 1 s en NORMAL: contadores (frames TX/RX por cada UART, heading IMU,
  ball detected, calibración).
- En cada cambio de modo: la transición.
- Si se detecta fallo: print del evento.

### 12.3 Comandos USB de debug

Vía USB Serial:
- `dump_snapshot` — imprime el último `WorldSnapshot` armado.
- `dump_imu` — imprime headings de ambos BNO055 + status.
- `dump_cameras` — imprime últimos frames de cámara 1 + cámara 2.
- `dump_hcsr04` — imprime distancia HC-SR04 actual.
- `cal_imu` — fuerza recalibración de gyro.
- `stats` — contadores.

## 13. Lo que NO hace (límite de scope)

La placa TOP **NO** hace:

- **Control de motores**: vive en CENTRAL.
- **Cinemática inversa**: vive en CENTRAL.
- **Estrategia táctica (FSM)**: vive en CENTRAL.
- **PIDs**: viven en CENTRAL.
- **Lectura de los 32 sensores de línea**: vive en DOWN.
- **Lectura de los OTOS**: vive en DOWN.
- **Decisión de "estoy fuera de la cancha"**: vive en CENTRAL con info de DOWN
  (el bus de emergencia DOWN→CENTRAL bypassa TOP completamente).

TOP es el **cerebro sensorial**: percibe + fusiona + entrega `WorldSnapshot`.
Nada más.

## 14. Referencias dentro del pack

- Pinout completo: [`01-pinout-y-hardware.md`](01-pinout-y-hardware.md)
- Contrato byte-a-byte del WorldSnapshot: [`03-contrato-datos-top.md`](03-contrato-datos-top.md)
- Protocolo de las cámaras OpenMV: [`04-contrato-datos-camaras.md`](04-contrato-datos-camaras.md)
- Protocolo general de comunicaciones: [`05-protocolo-comunicaciones.md`](05-protocolo-comunicaciones.md)
- Arquitectura general 3 placas: [`06-arquitectura-3-placas.md`](06-arquitectura-3-placas.md)
- Firmware vivo: [`firmware/top/`](firmware/top/) + [`firmware/shared/`](firmware/shared/)
- Tests host-native: [`tests/`](tests/)
- Ground-truth (SCH, PCB, PDF, BOM): [`ground-truth/`](ground-truth/)

## 15. Plan de trabajo factible (referencia)

| Nivel | Qué cubre | Estado |
|---|---|---|
| **1** | IMU dual + cámaras + HC-SR04 + 3 UARTs + envío WORLD_SNAPSHOT básico | ✅ implementado |
| **2** | Fusión front+back de cámaras + consistencia IMU + watchdogs + WorldSnapshot v2 con campos `ball_vx/vy` definidos | ✅ implementado (campos `ball_vx/vy` quedan en 0 hasta Nivel 3) |
| **3 — Roboliga Nov 2026** | Activar ToF VL53 + trilateración + EKF de pose + Kalman pelota (llena `ball_vx/vy`) | ⏳ 4–6 semanas post-Incheon |
| **4 — Mundial 2027** | Partner comm (ESP-NOW) + estimación de rivales + set plays coordinados | ⏳ pre-Mundial |
| **5 — largo plazo** | Modelo del rival con observaciones de los primeros minutos | 2027+ |
