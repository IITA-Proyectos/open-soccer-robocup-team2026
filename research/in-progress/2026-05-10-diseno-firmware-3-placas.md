---
title: "Diseño preliminar de firmware para las 3 placas — Roboliga 2026"
date: 2026-05-10
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: draft
tags: [firmware, arquitectura, top-board, down-board, comm, protocolo-uart, decision]
robot: ambos
area: software
tipo: decision
---

# Diseño preliminar de firmware — 3 placas Roboliga 2026

## Contexto

Las 3 placas físicas (TOP, DOWN, COMM) llegaron de China el 2026-05-10. Greenfield total: 0 firmware existente para Teensy 4.0. Este documento propone:

1. **Decisiones meta** (master/slave, framework, lenguaje).
2. **Protocolo UART entre placas** (con CRC, sincronización robusta — lección del análisis del delantero).
3. **Estructura modular del firmware TOP**.
4. **Estructura modular del firmware DOWN**.

Aplica las lecciones de `docs/internal/analisis-definitivo-delantero.md` y `analisis-definitivo-arquero.md` (marzo 2026) — los bugs identificados en marzo NO se repiten acá.

---

## 1. Decisiones meta

### 1.1 Arquitectura master/slave

**Propuesta: TOP = master, DOWN = slave de sensores.**

| Rol | Placa | Responsabilidad |
|-----|-------|------------------|
| **Master** | TOP | Estrategia (FSM), fusión sensorial (world model), comm con árbitros y partner, control de motores, decisiones tácticas |
| **Slave sensores** | DOWN | Lectura anillo 32 sensores línea + 2 OTOS, **entrega datos procesados** (no crudos) por UART al master |
| **Adaptador árbitros** | COMM | Bridge ESP32 entre TOP y árbitros RCJ + ESP-NOW inter-robot |

**Por qué TOP master:**
- Tiene cámaras (la fuente de datos más complejos).
- Tiene IMU dual y ToF (la información crítica para estrategia).
- Tiene 4 UARTs en uso — es el hub natural.
- Cualquier estrategia que reaccione a "veo pelota" tiene su epicentro acá.

**Por qué DOWN slave:**
- Sus 32 sensores de línea son muy frecuentes pero **trivialmente procesables** (ángulo + distancia).
- Los OTOS dan posición pre-procesada.
- Si el DOWN solo entrega datos procesados (no crudos), el ancho de banda UART es bajo (~50 bytes/ciclo a 100Hz).

### 1.2 Framework

**Propuesta: PlatformIO + Arduino framework.**

Razones:
- Ya existe el proyecto `software/teensy/Soccer 2026/` con `platformio.ini`. Mantener consistencia.
- PlatformIO maneja dependencias mejor que Arduino IDE (libs versionadas en `platformio.ini`).
- Soporta debugging por GDB (útil para Teensy 4.0).
- Sigue siendo C++ Arduino-style (familiar para Virginia y Elías).

**Cambio necesario en `platformio.ini`:**
```ini
; ANTES (incorrecto para placas nuevas):
[env:teensy41]
board = teensy41

; PROPUESTA (proyecto multi-board):
[env:top]
platform = teensy
board = teensy40
framework = arduino
build_flags = -DBOARD_TOP
src_filter = +<top/*> +<shared/*>
lib_deps =
    adafruit/Adafruit BNO055@^1.6.4
    sparkfun/SparkFun Qwiic OTOS Arduino Library
    pololu/VL53L1X@^1.3.1 ; o el ToF correcto

[env:down]
platform = teensy
board = teensy40
framework = arduino
build_flags = -DBOARD_DOWN
src_filter = +<down/*> +<shared/*>
lib_deps =
    sparkfun/SparkFun Qwiic OTOS Arduino Library
```

Un solo proyecto, dos environments. Código compartido en `src/shared/`.

### 1.3 Lenguaje y estilo

- **C++17** (Teensy 4.0 lo soporta).
- **Sin clases pesadas en hot path** (loop principal usa structs + funciones — overhead de clases mínimo pero predecible).
- **Headers `.h` + impl `.cpp`** (no `.ino`), para que PlatformIO compile separado y sea fácil para múltiples alumnos en paralelo.
- **`config.h` por placa** con todas las constantes con nombre (no magic numbers — lección del análisis viejo).

---

## 2. Protocolo UART entre placas

### 2.1 Lecciones del protocolo viejo (NO repetir)

Del `analisis-definitivo-delantero.md` BUG 1 y BUG R6:
- Sin sincronización robusta (si se pierde un byte, se cae todo).
- Sin checksum.
- Headers (201/202/203) pueden colisionar con valores de datos.
- Si el OpenMV manda X=201, el Teensy lo lee como header.

**Acá NO repetimos esto.**

### 2.2 Diseño del frame

Frame estándar de 16 bytes para TODO UART entre placas. Estructura:

```
┌────┬────┬────┬────┬────────────┬────┬────┐
│ 0xAA│ LEN│ TYP│ SEQ│  PAYLOAD   │ CRC│ 0x55│
│ 1B │ 1B │ 1B │ 1B │   N bytes  │ 2B │ 1B │
└────┴────┴────┴────┴────────────┴────┴────┘
```

| Campo | Bytes | Valor / propósito |
|-------|-------|-------------------|
| START | 1 | **0xAA fijo** — sync byte |
| LEN | 1 | Longitud del payload (1-32) |
| TYPE | 1 | Tipo de mensaje (ver tabla abajo) |
| SEQ | 1 | Número de secuencia (0-255, wrap) — detecta pérdida |
| PAYLOAD | N | Datos según TYPE |
| CRC | 2 | CRC-16/CCITT sobre LEN+TYPE+SEQ+PAYLOAD |
| END | 1 | **0x55 fijo** — sync byte de fin |

**Cómo se sincroniza el receptor:**
1. Loop continuo: `while (Serial.available())` consume bytes.
2. Estado interno: buscando START (0xAA).
3. Si llega 0xAA, lee LEN, TYPE, SEQ, payload completo, CRC, END.
4. Si END no es 0x55 o CRC no coincide → descarta todo y vuelve a buscar START.
5. Si frame válido → procesa según TYPE.

**Garantías:**
- Un byte basura no contamina el siguiente frame (vuelve a buscar 0xAA).
- CRC detecta corrupción.
- SEQ detecta pérdida (si recibimos SEQ 5 y el anterior fue 3, sabemos que perdimos uno).
- START + END distintos (0xAA vs 0x55) reducen falsos positivos de sincronización.

### 2.3 Tipos de mensaje (TYPE)

#### Mensajes DOWN → TOP (frecuencia 100Hz, ~10ms)

| TYPE | Nombre | Payload | Bytes |
|------|--------|---------|-------|
| 0x10 | DOWN_LINE_STATUS | ángulo línea (int16, ±180°×100), distancia al borde (uint8 mm), flag_salida_inminente (uint8) | 4 |
| 0x11 | DOWN_OTOS_POSE | x (int16 mm), y (int16 mm), heading (int16 ×100), calidad (uint8) | 7 |
| 0x12 | DOWN_OTOS_VEL | vx (int16 mm/s), vy (int16 mm/s), omega (int16 ×100) | 6 |

#### Mensajes TOP → DOWN (baja frecuencia, ~10Hz)

| TYPE | Nombre | Payload | Bytes |
|------|--------|---------|-------|
| 0x20 | TOP_RESET_OTOS | flag (uint8: 1 = reset position to 0) | 1 |
| 0x21 | TOP_CALIB_LINE | umbral (uint16) | 2 |

#### Mensajes TOP ↔ COMM (frecuencia variable)

| TYPE | Nombre | Payload | Sentido |
|------|--------|---------|---------|
| 0x30 | COMM_REFEREE_CMD | cmd (uint8: 0=stop, 1=start, 2=halftime, …) | COMM → TOP |
| 0x31 | COMM_STATUS_REQ | — | COMM → TOP |
| 0x32 | TOP_STATUS_REPLY | estado robot + batería + errores | TOP → COMM |
| 0x40 | COMM_PARTNER_DATA | x, y, heading, ball_x, ball_y, ball_conf, state del partner | COMM → TOP |
| 0x41 | TOP_PARTNER_DATA | mismo formato, hacia el partner | TOP → COMM |

### 2.4 CRC-16/CCITT

Implementación estándar, polinomio 0x1021, init 0xFFFF. Tabla precalculada en `shared/crc16.h`. Costo: ~30µs para 16 bytes en Teensy 4.0. Despreciable.

### 2.5 Diseño defensivo

- **Timeout de recepción:** si el receptor no recibe frame válido en 500ms, marca `partner_alive = false` y entra en degradación elegante.
- **Buffer circular** de bytes para no perder datos en bursts.
- **Logging de packet loss** acumulado para diagnóstico.
- **Reset automático del UART** si packet loss > 50% en 5s (sospecha de buffer corrupto).

---

## 3. Estructura modular firmware TOP

```
software/teensy/Soccer 2026/
├── platformio.ini                   ← multi-environment (top + down)
├── src/
│   ├── top/
│   │   ├── main_top.cpp             ← setup() + loop()
│   │   ├── strategy.h / .cpp        ← FSM principal (separada delantero vs arquero)
│   │   ├── world_model.h / .cpp     ← estado del mundo, fusión sensorial
│   │   ├── motors.h / .cpp          ← control de motores via conector U1 (PWM directos)
│   │   ├── sensors_imu.h / .cpp     ← BNO055 dual (Wire + Wire1, IMUPLUS modo)
│   │   ├── sensors_tof.h / .cpp     ← 4 ToF + HC-SR04
│   │   ├── cameras.h / .cpp         ← parse OpenMV via Serial3 + Serial5
│   │   ├── comm_down.h / .cpp       ← bridge con placa DOWN
│   │   ├── comm_arbiter.h / .cpp    ← bridge con placa COMM
│   │   └── config_top.h             ← pines, constantes, tolerancias
│   ├── down/
│   │   ├── main_down.cpp
│   │   ├── line_ring.h / .cpp       ← 32 sensores via 4 muxes
│   │   ├── otos.h / .cpp            ← 2 SparkFun OTOS fusion
│   │   ├── comm_top.h / .cpp        ← bridge con placa TOP
│   │   └── config_down.h
│   └── shared/
│       ├── proto.h / .cpp           ← protocolo UART (encode/decode frame)
│       ├── crc16.h / .cpp
│       └── types.h                  ← struct compartidas (Pose2D, BallPos, etc.)
└── test/
    └── (tests por módulo, futuros)
```

### 3.1 `main_top.cpp` — esqueleto

```cpp
#include <Arduino.h>
#include "strategy.h"
#include "world_model.h"
#include "sensors_imu.h"
#include "sensors_tof.h"
#include "cameras.h"
#include "comm_down.h"
#include "comm_arbiter.h"
#include "motors.h"
#include "config_top.h"

WorldModel world;

void setup() {
    Serial.begin(115200);
    motors_init();
    sensors_imu_init();      // BNO055 dual, modo IMUPLUS
    sensors_tof_init();      // 4 ToF + HC-SR04
    cameras_init();          // Serial3 + Serial5 a 230400
    comm_down_init();        // Serial1 a 230400
    comm_arbiter_init();     // Serial4 a 115200
    strategy_init();
}

void loop() {
    // 1. Leer todos los sensores (orden: rápidos primero)
    sensors_imu_tick();      // ~100µs
    cameras_tick();          // drena buffers UART
    comm_down_tick();        // drena UART desde DOWN
    sensors_tof_tick();      // ~1ms (4 ToF I2C)
    comm_arbiter_tick();     // drena UART desde COMM

    // 2. Actualizar modelo del mundo
    world.update();          // fusiona todo

    // 3. Decidir y actuar
    strategy_tick(world);    // FSM decide qué hacer
    motors_apply();           // aplica los comandos al hardware
}
```

### 3.2 Garantías del loop

- **Loop budget: < 10ms** (100Hz). Si pasa, alarma por LED.
- **Cada `tick()` no bloquea** — usa state machines internas.
- **Si BNO055 falla, se sigue sin él** (degradación elegante, lección del análisis BNO055).
- **Si DOWN no responde, FSM sigue con datos viejos** + flag de degradación.

### 3.3 `strategy.h` — FSM dual (delantero o arquero)

```cpp
#pragma once
#include "world_model.h"

enum class RobotRole { ATTACKER, GOALKEEPER };

void strategy_init();
void strategy_tick(const WorldModel& world);
void strategy_set_role(RobotRole role);

// Configuración del lado de cancha (resuelve T1 de la auditoría: polaridad)
void strategy_set_attack_color(uint8_t color);  // 0 = cyan, 1 = magenta
```

**El rol (delantero vs arquero) se selecciona por dipswitch o serial al setup, NO por `#define`** — un solo firmware para los 2 robots, configurado en runtime.

### 3.4 `world_model.h` — Pose y entidades

```cpp
struct Pose2D { float x; float y; float heading; uint32_t timestamp_ms; float confidence; };
struct Velocity2D { float vx; float vy; float omega; };
struct BallObservation { float x; float y; float confidence; uint32_t timestamp_ms; bool visible; };

class WorldModel {
public:
    Pose2D self;              // posición propia (de OTOS + heading IMU)
    Velocity2D self_vel;
    BallObservation ball;
    Pose2D partner;
    bool partner_alive;
    // ...

    void update();           // fusiona datos de sensors_imu, comm_down, cameras, comm_arbiter
};
```

---

## 4. Estructura modular firmware DOWN

### 4.1 `main_down.cpp` — esqueleto

```cpp
#include <Arduino.h>
#include "line_ring.h"
#include "otos.h"
#include "comm_top.h"
#include "config_down.h"

void setup() {
    Serial.begin(115200);
    line_ring_init();        // 4 muxes + 32 ADC reads paralelos
    otos_init();             // 2 OTOS en Wire + Wire1
    comm_top_init();         // Serial5 a 230400
}

void loop() {
    // Loop a 1kHz para sensores; comm a 100Hz
    line_ring_tick();        // ~40µs — actualiza ángulo + distancia
    otos_tick();             // ~200µs — actualiza pose fusionada

    // Send a TOP a 100Hz
    static uint32_t last_send = 0;
    if (millis() - last_send >= 10) {
        comm_top_send_line_status();
        comm_top_send_otos_pose();
        comm_top_send_otos_vel();
        last_send = millis();
    }

    comm_top_tick();         // drena comandos desde TOP
}
```

### 4.2 `line_ring.h` — anillo de 32 sensores

```cpp
#pragma once

void line_ring_init();
void line_ring_tick();              // muestrea los 32 sensores

// Datos procesados:
float line_ring_get_angle();        // ángulo de la línea más fuerte detectada, 0° = frente
float line_ring_get_depth();        // mm hacia adentro de la línea
bool  line_ring_get_imminent_exit(); // ≥ N sensores en blanco → robot saliendo

// Para debug y calibración:
uint16_t line_ring_get_raw(uint8_t sensor_idx);  // 0-31
void line_ring_calibrate_white();   // captura umbral por sensor
void line_ring_calibrate_carpet();
```

**Algoritmo del ángulo:** los 32 sensores forman un círculo. Cada sensor `i` tiene un ángulo `θ_i = i × 360° / 32 = i × 11.25°`. Cuando un grupo de sensores adyacentes ven blanco, el centroide del grupo da el ángulo de la línea. Implementación simple: weighted average de cosenos y senos.

### 4.3 `otos.h` — fusion de los 2 OTOS

```cpp
#pragma once

void otos_init();
void otos_tick();

// Pose fusionada de los 2 sensores:
float otos_get_x();        // mm
float otos_get_y();        // mm
float otos_get_heading();  // grados
float otos_get_vx();
float otos_get_vy();
float otos_get_omega();
uint8_t otos_get_quality(); // 0-100, baja si los 2 OTOS divergen

void otos_reset();         // pone (x, y, heading) en 0
```

**Estrategia de fusión inicial (simple):**
- Si ambos OTOS reportan dentro de tolerancia (ej. 50mm): promediar.
- Si divergen: usar el de mayor calidad reportada por SparkFun + flag quality bajo.
- Si uno reporta NaN/error: usar solo el otro + quality muy bajo.

Estrategia avanzada (post-mundial): Kalman 2D que fusiona OTOS + IMU heading + Cámara (cuando ve arcos para anchor).

---

## 5. Decisiones resueltas (sesión coach 2026-05-10)

| ID | Pregunta original | Decisión |
|----|------------------|----------|
| Q1 | Manejo de motores | **Zircon Rev v15 + Teensy 4.1 sigue activo** como **motor server**. TOP envía `MotorCommand{vx, vy, omega, kicker}` por UART al Zircon; el Zircon aplica cinemática inversa y PWM a los 3 omni. Fail-safe natural: si TOP se cuelga, Zircon se detiene. |
| Q2 | Placa COMM | Copia 100% del módulo oficial RCJ (ESP32 + OLED + acelerómetro + 2 botones). **Firmware oficial RCJ a cargar tal cual.** ESP-NOW inter-robot: evaluar firmware modificado o app secundaria. |
| Q4 | Manufacturer ToF | **VL53L5CX (pendiente arribo) + VL53L7CX (posible stock).** ToF de matriz 8×8 SPAD. Firmware ToF queda como **hito tardío**, Hito 3 arranca sin ToF (BNO055 + cámaras + HC-SR04 alcanzan). |
| Q5 | Montaje OTOS | **Uno a cada costado del robot.** Análisis diferencial: si `vx_left ≠ vx_right` → robot girando → TOP comanda `omega` correctiva al Zircon. Especialmente útil para "avanzar derecho al patear". |
| Q6 | Protocolo OpenMV | **Mantener el viejo (9 bytes 201/202/203)** inicialmente. TOP implementa parser robusto del análisis: `while (peek != 201) discard`, valida 3 headers, maneja edge case `Xp=0`. Migración a protocolo nuevo si hay tiempo post-Hito 5. |
| Q7 | Selección rol arquero/delantero | **Dipswitch físico en TOP**, leído al setup (pin GPIO). Decide `RobotRole::ATTACKER` o `RobotRole::GOALKEEPER` al boot. Cambio dinámico de rol es post-mundial (objetivo Nacional Nov 2026). |

## 5b. Q3 resuelta (inferencia desde PCB EasyEDA)

Análisis del archivo `1-PCB_PCB_Roboliga2026_TOP.json` (proyecto EasyEDA dentro de `BackupProjects_*.zip` doble-zip):

- Los nets **SCL1, SDA1, RX4, TX4** tienen **tracks separados ruteados** en el PCB fabricado (5+5+11+9 tracks respectivamente).
- 4 nets físicos distintos → **no es conflicto, es remapeo intencional** de Wire1 a pines 24/25.
- **Firmware TOP debe llamar `Wire1.setSCL(24); Wire1.setSDA(25);` antes de `Wire1.begin()`.**
- Serial4 queda en pines 16/17 default.

Confirmación final pendiente: abrir proyecto en EasyEDA o medir con multímetro en placa fabricada. Detalles en [`hardware/electronics/mapa-pines-placas-nuevas.md`](../../hardware/electronics/mapa-pines-placas-nuevas.md#q3-resuelta-con-análisis-del-pcb-easyeda-2026-05-10).

**Implicancia para Hito 3:** módulo `sensors_imu.h` arranca con el remap. No bloquea Hito 1 ni Hito 2.

## 5c. Arquitectura confirmada del robot 2026

```
                       ┌────────────────────────────┐
                       │  PLACA COMM (módulo RCJ)    │
                       │  ESP32 + OLED + IMU         │
                       │  Firmware RCJ oficial       │
                       └─────────────┬──────────────┘
                                     │ UART (Serial4 del TOP)
                                     │
 ┌─────────────────┐  UART      ┌───▼──────────────────┐   UART       ┌──────────────────────┐
 │  PLACA DOWN     │◄──────────►│  PLACA TOP           │◄────────────►│  ZIRCON Rev v15      │
 │  Teensy 4.0     │  línea +   │  Teensy 4.0 MASTER   │   MotorCmd   │  Teensy 4.1          │
 │  SLAVE sensores │  pose +    │  - 2× BNO055         │   {vx, vy,   │  MOTOR SERVER        │
 │  - 32 sensores  │  velocidad │  - 4× ToF (futuro)   │    omega,    │  - 3× drivers omni   │
 │    luz (4 mux)  │            │  - 1× HC-SR04        │    kicker}   │  - Cinemática        │
 │  - 2× OTOS      │            │  - 2× UART → OpenMV  │              │    inversa           │
 │    (1 c/lado)   │            │  - FSM estrategia    │              │  - Solenoide kicker  │
 │                 │            │  - World model       │              │                      │
 │  Entrega proc.: │            │                      │              │  Recibe MotorCommand │
 │  - ángulo línea │            │  Procesa visión,     │              │  por UART, aplica    │
 │  - flag salida  │            │  fusiona sensores,   │              │  PWM. Fail-safe si   │
 │  - pose (x,y,h) │            │  decide estrategia,  │              │  TOP se cuelga.      │
 │  - velocidad    │            │  comanda motores.    │              │                      │
 └─────────────────┘            └──────────────────────┘              └──────────────────────┘
                                          ▲
                                          │  UART (Serial3, Serial5)
                                          │
                                ┌─────────┴──────────┐
                                │  2× OpenMV H7/H7+   │
                                │  Cámaras (proto.    │
                                │  viejo 9 bytes)     │
                                └─────────────────────┘
```

**Implicancias clave:**

- **Zircon como motor server simplifica la migración.** El firmware del Zircon es mínimo: recibe `MotorCommand` → cinemática inversa → PWM. La complejidad (FSM, decisión, fusión) vive en el TOP. Esto desacopla bien.
- **DOWN entrega procesado, no crudo.** "Ángulo de línea + flag salida + pose" en vez de 32 lecturas analógicas. Ancho de banda UART baja drásticamente.
- **OTOS dual a los costados** habilita corrección de trayectoria de alto nivel en TOP (no en Zircon).
- **El protocolo UART nuevo (con CRC) se usa TOP↔DOWN y TOP↔Zircon y TOP↔COMM.** Las cámaras OpenMV mantienen el protocolo viejo (parser robusto en TOP).

---

## 6. Plan de implementación incremental

**No implementar todo de golpe.** Orden propuesto (cada hito termina con test en hardware real):

### Hito 1 — Cimientos (semana 1, mayo 11-17)
1. Reorganizar `software/teensy/Soccer 2026/` con multi-environment PlatformIO + carpetas `top/` `down/` `shared/`.
2. Implementar protocolo UART (frame + CRC + buffer + tests unitarios).
3. Validar comunicación TOP↔DOWN básica (loopback test: TOP envía, DOWN responde echo).

### Hito 2 — DOWN funcional (semana 2, mayo 18-24)
1. `line_ring`: leer 32 sensores via 4 muxes, calibración blanco/carpet, ángulo + distancia.
2. `otos`: leer 2 OTOS, fusión simple, reset.
3. DOWN entrega `LINE_STATUS` + `OTOS_POSE` + `OTOS_VEL` a TOP a 100Hz.
4. **Test hardware:** robot encendido, mover sobre cancha, verificar que TOP recibe pose y línea coherentes (log serial).

### Hito 3 — TOP sensores (semana 3, mayo 25-31)
1. `sensors_imu`: BNO055 dual modo IMUPLUS, calibración robusta, degradación elegante (lección del análisis BNO055).
2. `sensors_tof`: 4 ToF + HC-SR04, lectura no bloqueante.
3. `cameras`: parser para 2 OpenMV via UART.
4. `world_model`: integra todo.
5. **Test hardware:** robot estático, mover pelota y robots vecinos, verificar que world model refleja realidad.

### Hito 4 — Estrategia básica (semana 4, jun 1-7)
1. `strategy`: FSM mínima delantero (3 estados: SEARCH/APPROACH/PUSH).
2. `motors`: control de motores (depende de Q1).
3. **Test hardware:** robot delantero juega solo en cancha con pelota.

### Hito 5 — COMM + partner (semana 5, jun 8-14)
1. `comm_arbiter`: integración con placa COMM.
2. Start/stop de árbitros funciona.
3. Partner data básica vía COMM.

### Hito 6 — Arquero + integración (semana 6, jun 15-21)
1. `strategy` arquero: FSM con polaridad de campo configurable.
2. Test 2v0 (dos robots vs nada, comunicación entre ellos).

### Hito 7 — Scrimmage + ajustes (semana 7, jun 22-28)
1. Test 1v1 contra robot rival simulado (otro alumno controla manualmente).
2. Calibración fina, journaling intensivo (skill `engineering-journal`).

### Hito 8 — Viaje (jun 29 - jul 6)
Incheon.

**A 7 semanas de Incheon, este plan es ambicioso.** Si se atrasa: **bajar el techo** — entregar lo que funcione (Hitos 1-4 mínimo) y dejar partner + estrategia compleja como "lo que aprendemos en Incheon". El frame coach es claro: aprender > podio.

---

## 7. Riesgos identificados

| Riesgo | Severidad | Mitigación |
|--------|-----------|-----------|
| Conflicto pines 16/17 TOP no resuelto | Alto | Verificar con multímetro antes de mayo 17. Si hay conflicto, decidir cuál sacrificar. |
| Sin firmware funcionando a 4 semanas de Incheon | Alto | Plan B: usar el robot viejo (Zircon Rev v15 + Teensy 4.1 + código nacional 2025) en Incheon. Las placas nuevas se debuggean post-mundial. |
| ToF no funciona como esperamos | Medio | Tener HC-SR04 como fallback (ya está en TOP). |
| BNO055 dual no aporta sobre single | Bajo | No bloquea — uno solo ya funcionaba en 2025. |
| OTOS desconocido para el equipo | Medio | Empezar Hito 2 con tutorial OpenLab de SparkFun. Plan B: posicionamiento solo por arcos detectados por cámara. |
| Comm placa COMM no programable | Alto | Si es módulo RCJ cerrado, definir el protocolo exacto en Q2 ya. |

---

## 8. Próximos pasos inmediatos

1. **Coach confirma las 7 decisiones bloqueantes** (Q1-Q7).
2. Crear `journal/2026-05-11-kickoff-firmware-3-placas.md` con plan de Hito 1.
3. Refactorear `software/teensy/Soccer 2026/platformio.ini` a multi-environment.
4. Crear esqueleto de carpetas `src/top/` `src/down/` `src/shared/`.
5. Implementar `shared/proto.{h,cpp}` y `shared/crc16.{h,cpp}` con tests unitarios (en `test/`).
