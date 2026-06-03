---
title: "Placa CENTRAL — Pinout y hardware (Zircon Rev v15 + Teensy 4.1)"
date: 2026-05-24
status: vigente
parte-de: central-board-pack
fuentes:
  - firmware/central/config_central.h (código vivo)
  - hardware/electronics/mapa-pines-teensy-ambos-robots.md (2026-03-20)
  - hardware/electronics/Zircon.pdf (esquemático, no incluido en este pack)
---

# Placa CENTRAL — Pinout y hardware

> **Salvedad importante**: a diferencia del pack DOWN, **NO hay schematic JSON
> ni PCB JSON del Zircon disponibles en el repo**. La placa es la "Zircon Rev v15"
> que el equipo IITA ganó el nacional 2025; el equipo nuevo heredó solo el PDF
> del esquemático (`hardware/electronics/Zircon.pdf`, fuera de este pack).
> Este doc consolida el pinout que **vive en el firmware** (`config_central.h`) +
> el doc histórico del 2026-03-20 que coincide con él.

## 1. Hardware sobre el que corre

| Componente | Cantidad | Nota |
|---|---|---|
| MCU **Teensy 4.1** | 1 | Cortex-M7 a 600 MHz, 1 MB RAM, 8 MB flash, **8 UARTs hardware**, 480 KB PSRAM |
| Placa **Zircon Rev v15** | 1 | Shield del Teensy 4.1, PCB que ganó el nacional 2025 |
| H-bridges para motores omni-3 | 3 | Drivers Zircon U5, U7, U17. Cada uno: INA + INB + PWM |
| ~~**BNO055** IMU local~~ | 0 | ⚠️ **YA NO se conecta (2026-05-31)** — los 2 BNO están en el TOP; el heading viene del snapshot de ARRIBA. `imu_zircon` queda como compat (`-DCENTRAL_HAS_LOCAL_BNO`, off). |
| Solenoide / kicker (solo ROBOT2 delantero) | 1 | GPIO + MOSFET. ⚠️ pin a confirmar |
| Dribbler (opcional, ROBOT2) | 1 | PWM. Si está montado físicamente |
| Botones de programación | 2 | Pines 9, 10. Pull-up interno |
| LED de estado | 1 | LED_BUILTIN (pin 13) |
| **Encoders magnéticos** | 0 (FUTURO) | AS5600 o quadrature, no implementado todavía |

### Sensores legacy del Zircon 2025 — NO usados en arquitectura 3-placas

| Componente | Cantidad | Pines Teensy | Estado |
|---|---|---|---|
| Sensores IR pelota TSSP58038 (smoothedBall1..8) | 8 | 14, 15, 16, 17, 20, 21, 22, 23 | **LEGACY** — la pelota 2026 es óptica, no IR. Cableado físico sigue ahí pero firmware nuevo NO los lee |
| Sensores de línea analógicos (Line1, Line2, Line3) | 3 | A11 (25), A12 (26), A13 (27) | **LEGACY** — los 32 sensores de línea viven ahora en la placa DOWN. Redundantes |

Estos pines (8 IR + 3 línea = 11) están físicamente conectados a sus respectivos
sensores en el Zircon Rev v15, pero el firmware 2026 los ignora. Podrían
liberarse en una futura Rev del PCB.

## 2. Pinout compartido (idéntico en ROBOT1 y ROBOT2)

### 2.1 UARTs — conexiones con las otras placas

| Serial | Pin Arduino RX | Pin Arduino TX | Conectado a | Baud | Rol |
|---|---|---|---|---|---|
| **`Serial7`** | **28** | **29** | placa **TOP** (master de cámaras) | 230400 | Recibe `WORLD_SNAPSHOT` @ 100 Hz (reasignado 2026-05-31, antes Serial1) |
| **`Serial1`** | **0** | **1** | placa **DOWN** (sensores piso) | 230400 | **Recibe `LINE_URGENT` @ 200 Hz** (reasignado 2026-05-31 desde Serial2/7-8 → libera 7/8 para el motor 2) |

El Teensy 4.1 tiene 8 UARTs hardware (Serial1–Serial8); usamos solo 2.
Los demás están reservados para depuración / futuro (encoders por UART, etc).

### 2.2 I²C — BNO055 IMU

| Atributo | Valor |
|---|---|
| Bus | `Wire` (I²C0) |
| Pin Arduino SDA | **18** |
| Pin Arduino SCL | **19** |
| Dirección I²C | **0x28** |
| Timeout de init | 3000 ms |
| Estabilización post-init | 1000 ms |
| Calibración del giroscopio | 2000 ms |
| Modo de operación | OPERATION_MODE_IMUPLUS (gyro + accel, sin magnetómetro) |

### 2.3 Otros pines compartidos

| Pin Arduino | Función | Detalle |
|---|---|---|
| **13** | LED_BUILTIN | LED de estado del Teensy 4.1 |
| **9** | Botón 1 (PushButton) | Pull-up interno (legacy del 2025, opcional) |
| **10** | Botón 2 (PushButton1) | Pull-up interno (legacy del 2025, opcional) |

## 3. Pinout ROBOT1 — ARQUERO (#define ROBOT1)

Build: `pio run -e zircon_robot1`.

### Motores

| Motor | INA | INB | PWM | Driver Zircon |
|---|---|---|---|---|
| **Motor 1** | pin **2** | pin **5** | pin **3** | U5 |
| **Motor 2** | pin **8** | pin **7** | pin **6** | U17 |
| **Motor 3** | pin **11** | pin **12** | pin **4** | U7 |

### Kicker / dribbler

El arquero **NO tiene kicker** (regla: solo el delantero patea). El bloque
`PIN_KICKER_SOL` en `config_central.h` está envuelto en `#if defined(ROBOT2)`.

## 4. Pinout ROBOT2 — DELANTERO (#define ROBOT2)

Build: `pio run -e zircon_robot2`.

### Motores

| Motor | INA | INB | PWM | Driver Zircon |
|---|---|---|---|---|
| **Motor 1** | pin **8** | pin **7** | pin **6** | U17 |
| **Motor 2** | pin **11** | pin **12** | pin **4** | U7 |
| **Motor 3** | pin **2** | pin **5** | pin **3** | U5 |

### Kicker (solenoide)

| Atributo | Valor | Confianza |
|---|---|---|
| Pin de control | **23** (`PIN_KICKER_SOL`) | ⚠️ **TENTATIVO** — pendiente confirmar con Enzo qué GPIO del Zircon está cableado al MOSFET del solenoide |
| Duración del pulso | 80 ms (`KICKER_PULSE_MS`) | ✅ |
| Cooldown mínimo entre disparos | 1500 ms (`KICKER_COOLDOWN_MS`) | ✅ protege al solenoide de recargas seguidas que lo queman |

> **Ver TASK-011** en `team-tasks/2026-05-15-task-011-confirmar-pin-kicker-solenoide-zircon.md`.

## 5. Equivalencia de motores ARQUERO ↔ DELANTERO

Los dos robots usan el MISMO Zircon, pero los motores están físicamente
cableados distinto. La equivalencia eléctrica:

| Pin (INA/INB/PWM) | Driver | Motor en ARQUERO | Motor en DELANTERO |
|---|---|---|---|
| 2 / 5 / 3 | U5 | Motor 1 | Motor 3 |
| 8 / 7 / 6 | U17 | Motor 2 | Motor 1 |
| 11 / 12 / 4 | U7 | Motor 3 | Motor 2 |

Por eso la selección del pinout es `#define ROBOT1` vs `#define ROBOT2` en
compile-time. El firmware no puede deducir el rol del Zircon: lo hereda del
flag de compilación, que coincide físicamente con el robot al que se va a flashear.

## 6. Cinemática del robot

Las 3 ruedas omnidireccionales están distribuidas a 120° entre sí en el chasis.
La cinemática inversa traduce velocidad del robot `(vx, vy, ω)` a velocidad
de cada rueda. Implementación en `firmware/shared/kinematics.{h,cpp}`
(con 11 tests unitarios).

| Atributo | Valor | Confianza |
|---|---|---|
| Ángulos físicos de las ruedas | `θ₁ = +60°`, `θ₂ = −60°`, `θ₃ = +180°` | ⚠️ **TENTATIVO** — `WHEEL_ANGLES_DEG[3]` en config, confirmar con montaje físico |
| Distancia centro→rueda (R) | 100 mm (`WHEEL_RADIUS_MM`) | ⚠️ **TENTATIVO** — confirmar con regla en el robot armado |
| Convención frente del robot | +Y (igual que la placa DOWN para que `SENSOR_POS[]` no rote) | ✅ |
| Convención rotación positiva | counter-clockwise visto desde arriba | ✅ |
| Velocidad máxima estimada | 1000 mm/s (`MAX_SPEED_MM_S`) | ⚠️ estimación para motores TT @ 7.4 V — calibrar |
| Rango PWM (analogWrite) | 0–255 (`MAX_PWM`) | ✅ |

## 7. Watchdog del motor

| Atributo | Valor |
|---|---|
| `COMMAND_TIMEOUT_MS` | **200 ms** |

Si no llega un `MotorCommand` desde TOP en 200 ms, los motores se detienen
solos. Esto evita que el robot quede a velocidad fija si TOP se cuelga o si
se desconecta el cable UART. La implementación vive en
`firmware/central/main_central.cpp`.

## 8. Resumen del Pinout del Teensy 4.1 — uso por pin

| Pin Arduino | Función | ROBOT1 (Arquero) | ROBOT2 (Delantero) | Confianza |
|---|---|---|---|---|
| 0 | RX1 (Serial1) | UART desde **DOWN** (reasig. 2026-05-31) | idem | ✅ |
| 1 | TX1 (Serial1) | UART hacia **DOWN** | idem | ✅ |
| 2 | GPIO | Motor 1 INA | Motor 3 INA | ✅ |
| 3 | PWM | Motor 1 PWM | Motor 3 PWM | ✅ |
| 4 | PWM | Motor 3 PWM | Motor 2 PWM | ✅ |
| 5 | GPIO | Motor 1 INB | Motor 3 INB | ✅ |
| 6 | PWM | Motor 2 PWM | Motor 1 PWM | ✅ |
| 7 | GPIO (Motor) | Motor 2 INB | Motor 1 INB | ✅ (Serial2 liberado 2026-05-31) |
| 8 | GPIO (Motor) | Motor 2 INA | Motor 1 INA | ✅ (Serial2 liberado 2026-05-31) |
| 9 | GPIO | Botón 1 (legacy) | Botón 1 (legacy) | ✅ legacy |
| 10 | GPIO | Botón 2 (legacy) | Botón 2 (legacy) | ✅ legacy |
| 11 | GPIO | Motor 3 INA | Motor 2 INA | ✅ |
| 12 | GPIO | Motor 3 INB | Motor 2 INB | ✅ |
| 13 | LED_BUILTIN | LED estado | LED estado | ✅ |
| 14–17 | Analógicos | Legacy IR pelota (no usados) | Legacy IR pelota (no usados) | ✅ legacy |
| 18 | SDA0 (Wire) | I²C BNO055 | I²C BNO055 | ✅ |
| 19 | SCL0 (Wire) | I²C BNO055 | I²C BNO055 | ✅ |
| 20–23 | Analógicos | Legacy IR pelota (no usados) | Legacy IR pelota (no usados) | ✅ legacy |
| 23 | GPIO | — | Kicker solenoide | ⚠️ TENTATIVO |
| 25–27 | Analógicos | Legacy Line1/2/3 (no usados) | Legacy Line1/2/3 (no usados) | ✅ legacy |
| **28, 29** | RX7/TX7 (Serial7) | UART **desde/hacia TOP** (reasignado 2026-05-31) | idem | ✅ |
| 24, 30–41 | GPIO | Libres para expansión | Libres para expansión | ✅ |

> ✅ **Conflicto pines 7/8 RESUELTO (2026-05-31, Opción B):** se eligió mover los
> UART a otros pines. El link **DOWN→CENTRAL pasó a `Serial1` (0/1)** y **TOP→CENTRAL
> a `Serial7` (28/29)** (pines de expansión libres). Así los **pines 7/8 quedan solo
> para el driver del motor 2 (U17)** — sin UART encima. Firmware ya actualizado
> (`comm_down.cpp` → Serial1, `comm_top.cpp` → Serial7, `config_central.h`).

## 9. Pendientes humanos (NO bloquean uso del pack, pero hay que resolver)

| # | Pendiente | Asignado | Bloqueante para |
|---|---|---|---|
| 1 | Confirmar `PIN_KICKER_SOL` (¿es 23?) | Enzo | Patear pelota (TASK-011) |
| 2 | Confirmar `WHEEL_ANGLES_DEG` y `WHEEL_RADIUS_MM` con montaje físico | Enzo + Virginia | Cinemática correcta (omni-3) |
| 3 | ✅ RESUELTO 2026-05-31: conflicto 7/8 cerrado moviendo los UART (DOWN→Serial1 0/1, TOP→Serial7 28/29); 7/8 quedan para el motor 2. | ✅ | — |
| 4 | Confirmar que sensores legacy (8 IR + 3 línea) están físicamente en el PCB pero firmware no los lee | Enzo | Liberar esos pines en futura Rev del Zircon |
| 5 | Encoders magnéticos (AS5600) — futuro | Equipo | PID closed-loop por motor (ver §10 del 02-funcionalidad.md) |

## 10. Referencias

- Código vivo del firmware: [`firmware/central/`](firmware/central/) (especialmente `config_central.h`).
- Doc histórico del 2026-03-20: `hardware/electronics/mapa-pines-teensy-ambos-robots.md` (sigue siendo la única referencia textual del pinout del Zircon, coincide con `config_central.h` en motores y UART/I²C).
- Esquemático del Zircon: `hardware/electronics/Zircon.pdf` ✅ (en el repo desde 2026-05-31). **Fuente:** Robomov — https://robomov.net/pages/downloads (schematic público, KiCad/Eeschema; el Zircon es una placa comercial de Robomov). No se duplica dentro de este pack por ser binario pesado.
- Librería del equipo del 2025: `software/libraries/zirconLib/zirconLib.cpp` (legacy, no usada en firmware nuevo).
- Journal de motores arquero vs delantero: `journal/2026-03-20-diferencias-pines-motores-arquero-delantero.md` (historia).
