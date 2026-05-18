---
title: "Contrato de datos de la placa TOP — qué emite, qué recibe, con precisión y gaps (v1)"
date: 2026-05-18
author: "Claude (Anthropic - Claude Sonnet 4.6)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Sonnet 4.6, Anthropic)"
status: final
tags: [comunicacion, firmware, protocolo, contrato, top-board, ambos]
robot: ambos
area: comunicacion
tipo: protocolo
contract-schema: 1
related: [software/teensy/Soccer 2026/src/shared/proto.h, software/teensy/Soccer 2026/src/shared/types.h, software/teensy/Soccer 2026/src/top/main_top.cpp, docs/firmware/CONTRATO-DATOS-DOWN.md, docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md]
---

# Contrato de datos TOP — referencia única para programar TOP, CENTRAL y COMM

> **Propósito.** Definir SIN AMBIGÜEDAD qué datos emite y recibe la placa TOP,
> con qué formato exacto, unidades, rangos, convenciones, estado real del código
> (real vs stub/hardcodeado) y gaps pendientes. Quien programe TOP implementa
> exactamente esto; quien programe CENTRAL consume exactamente esto. Si el código
> y este documento difieren, **se corrige el que esté mal y se versiona el
> contrato** (`contract-schema`).
>
> **Fuentes directas.** Todo lo documentado aquí fue leído de:
> `src/top/main_top.cpp`, `src/top/config_top.h`, `src/top/cameras.cpp`,
> `src/top/cameras_runtime.cpp`, `src/top/cameras_fusion.h`,
> `src/top/sensors_imu.cpp`, `src/top/sensors_tof.cpp`,
> `src/top/comm_central.cpp`, `src/top/comm_arbiter.cpp`,
> `src/top/comm_down.cpp`, `src/shared/types.h`, `src/shared/proto.h`.
> No se inventa nada; "NO implementado" donde el código lo confirma.

---

## 0. Frontera de responsabilidad (leer primero)

TOP es el **cerebro sensorial** del robot. Su trabajo: percibir, fusionar y
publicar. **NO decide estrategia. NO controla motores.**

### Lo que TOP hace

| Tarea | Estado real |
|-------|-------------|
| Heading del robot (IMU dual BNO055) | **REAL** — promedio/fallback de dos BNO055 en modo IMUPLUS |
| Detección de pelota (fusión front+back) | **REAL** — parser + fusión dual testeada |
| Detección de arcos (ángulo + distancia) | **REAL** para ángulo; distancia usa `CAMERA_UNIT_TO_MM=10.0` **sin calibrar** |
| Evasión de obstáculos (ToF) | **STUB** — todos los ToF I2C comentados; solo HC-SR04 bloqueante activo |
| Pose absoluta (x, y) en cancha | **HARDCODEADO a 0** — fusión cámara+IMU+OTOS ausente |
| Recepción odometría OTOS desde DOWN | Frame se recibe, pero `my_x_mm/my_y_mm` NO se populan con esa data |
| Clasificación del rol (arquero/delantero) | `PIN_ROLE_DIPSWITCH` declarado (`config_top.h:87`) pero NO hay `digitalRead` en el código |
| Polaridad de arco (opp vs own) | **HARDCODEADO** `yellow=opp, blue=own` (`main_top.cpp:65`, TODO presente) |
| Bridge árbitros / partner ESP-NOW | **REAL** — `comm_arbiter` recibe `COMM_REFEREE_CMD` y partner data |

### Lo que TOP NO hace (y por qué importa documentarlo)

- NO clasifica si la línea detectada es "de fondo" o "lateral": esa clasificación
  requiere heading y es responsabilidad de CENTRAL, usando el heading que le llega
  en `WORLD_SNAPSHOT`. TOP sí tiene heading — fue una decisión de diseño poner la
  clasificación en quien tiene todo el contexto (CENTRAL). Ver contrato DOWN §0.
- NO aplica lógica de estrategia: ni approach, ni kick, ni GK. CENTRAL consume el
  snapshot y decide.
- NO recibe `LINE_URGENT` para control: ese mensaje viaja por bus de emergencia
  DOWN→CENTRAL directo. TOP lo acepta si llega (código de transición en
  `comm_down.cpp:45-50`) pero no lo expone en el snapshot.

---

## 1. Capa de transporte (proto.h) — idéntica a todos los enlaces

Frame idéntico para todos los enlaces (`src/shared/proto.h:6-16`):

```
┌──────┬─────┬──────┬─────┬────────────┬──────────┬──────┐
│ 0xAA │ LEN │ TYPE │ SEQ │  PAYLOAD   │ CRC16 BE │ 0x55 │
│  1B  │ 1B  │  1B  │  1B │  LEN bytes │    2B    │  1B  │
└──────┴─────┴──────┴─────┴────────────┴──────────┴──────┘
```

| Elemento | Valor / regla |
|---|---|
| START | `0xAA` |
| LEN | longitud del PAYLOAD en bytes (= `sizeof(struct)`) |
| TYPE | `MsgType` (enum en `proto.h:35-69`) |
| SEQ | contador 0–255 que envuelve; incrementa el emisor por frame |
| CRC16 | CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`, sin reflexión, xorout `0x0000`) sobre **LEN+TYPE+SEQ+PAYLOAD** (NO incluye START ni END). Transmitido **big-endian** (byte alto primero) |
| END | `0x55` |
| Overhead | **7 bytes**; payload máximo **32 bytes** (`PROTO_MAX_PAYLOAD`) |

**Endianness del payload (CRÍTICO):** los structs se serializan con `memcpy`
crudo desde un Teensy 4.0 (ARM Cortex-M7, little-endian). Por lo tanto **todo
entero multibyte del payload es little-endian** (byte menos significativo
primero). El CRC viaja big-endian. No confundir.

---

## 2. Catálogo de mensajes que TOP emite y recibe

| Dir | TYPE hex | Nombre enum | Payload | Frec | Propósito |
|-----|----------|-------------|---------|------|-----------|
| TOP → CENTRAL | `0x60` | `WORLD_SNAPSHOT` | `WorldSnapshot` (23 B) | 100 Hz | Snapshot completo del mundo percibido (§3) |
| TOP ← CENTRAL | `0x61` | `CENTRAL_RESET_TOP` | `uint8` (0 B usado) | evento | Reset del world model / recalibrar cámaras — **recibido pero no procesado** (`comm_central.cpp:19-23`) |
| TOP ← CENTRAL | `0x62` | `CENTRAL_TOP_CMD` | genérico | evento | Comandos admin — **no implementado** |
| TOP → COMM | `0x32` | `TOP_STATUS_REPLY` | `StatusReply` (5 B) | pedido | Responde a `COMM_STATUS_REQ` con rol, errores, batería (§4) |
| TOP ← COMM | `0x30` | `COMM_REFEREE_CMD` | `uint8` (1 B) | evento | Comando del árbitro start/stop/halftime/reset (§4) |
| TOP ← COMM | `0x31` | `COMM_STATUS_REQ` | vacío | pedido | COMM pide status — recibido, handler devuelve sin responder automáticamente (`comm_arbiter.cpp:44-48`) |
| TOP ← COMM | `0x40` | `COMM_PARTNER_DATA` | `PartnerSnapshot` (12 B) | ~10 Hz | Snapshot del robot partner vía ESP-NOW (§4) |
| TOP → COMM | `0x41` | `TOP_PARTNER_DATA` | `PartnerSnapshot` (12 B) | app | Mi snapshot al partner vía COMM/ESP-NOW (§4) |
| TOP ← DOWN | `0x11` | `DOWN_OTOS_POSE` | `Pose2D` (7 B) | 100 Hz | Odometría OTOS — recibida pero NO fusionada en snapshot (§5) |
| TOP ← DOWN | `0x12` | `DOWN_OTOS_VEL` | `Velocity2D` (7 B) | 100 Hz | Velocidad OTOS — recibida, no expuesta en snapshot actual |

**UARTs físicos** (según `config_top.h:35-55`, estado NOT CONFIRMED para algunos):

| UART | Pines | Baud | Enlace | Confirmado |
|------|-------|------|--------|-----------|
| Serial1 | RX=0, TX=1 | 230400 | ← DOWN (odometría OTOS) | Inferido del schematic |
| Serial2 | RX=7, TX=8 | 230400 | → CENTRAL (snapshot) | **NO CONFIRMADO** (`config_top.h:40-42`) |
| Serial3 | RX=15, TX=14 | 19200 | ← Cámara frontal (OpenMV) | Schematic U8 |
| Serial4 | RX=16, TX=17 | 115200 | ↔ COMM (árbitros + ESP-NOW) | Schematic U15 |
| Serial5 | RX=21, TX=20 | 19200 | ← Cámara trasera (OpenMV) | Schematic U9 |

> **Crítico:** el mapeo de Serial2 (→CENTRAL) no está confirmado en hardware.
> Si el cableado físico cruza Serial1 y Serial2, CENTRAL nunca recibe el snapshot
> → motores parados siempre. Verificar con osciloscopio antes de confiar (TASK-008/014).

---

## 3. `WORLD_SNAPSHOT` — TOP → CENTRAL (TYPE `0x60`), 23 bytes

### 3.1 Definición del struct (`src/shared/types.h:92-123`)

```cpp
struct WorldSnapshot {
    int16_t my_x_mm;                  // off 0
    int16_t my_y_mm;                  // off 2
    int16_t my_heading_centideg;      // off 4
    uint8_t my_pose_confidence;       // off 6
    int16_t ball_x_mm;                // off 7
    int16_t ball_y_mm;                // off 9
    uint8_t ball_visible;             // off 11
    uint8_t ball_confidence;          // off 12
    int16_t goal_opp_angle_centideg;  // off 13
    int16_t goal_opp_distance_mm;     // off 15
    uint8_t goal_opp_visible;         // off 17
    uint8_t goal_own_visible;         // off 18
    uint16_t min_obstacle_mm;         // off 19
    uint8_t referee_cmd;              // off 21
    uint8_t flags;                    // off 22
} __attribute__((packed));
// sizeof(WorldSnapshot) == 23 bytes
```

`static_assert(sizeof(WorldSnapshot)==23)` **no existe en el código actual**
(gap: debe agregarse para proteger el contrato).

### 3.2 Layout exacto byte-a-byte (offsets, little-endian)

| Off | Campo | Tipo | Unidad | Rango / Sentinela | Estado real | Significado |
|----:|-------|------|--------|------|-------------|-------------|
| 0 | `my_x_mm` | i16 | mm | −32768..+32767; **0 = N/A hoy** | **HARDCODEADO a 0** (`main_top.cpp:49`) | Posición X del robot en cancha. NO implementada: fusión cámara+OTOS ausente. |
| 2 | `my_y_mm` | i16 | mm | id. | **HARDCODEADO a 0** (`main_top.cpp:50`) | Posición Y del robot en cancha. Mismo estado. |
| 4 | `my_heading_centideg` | i16 | centideg | −18000..+18000 | **REAL** — IMU dual BNO055 (`main_top.cpp:51`) | Heading del robot (ángulo de orientación, marco cancha). Cero = orientación al boot. |
| 6 | `my_pose_confidence` | u8 | — | 0..100; **0 = pose inválida** | **PARCIAL** — vale 60 si ≥1 IMU OK, 0 si ambos fallaron (`main_top.cpp:51-52`); no refleja calidad de x/y | Confianza de la pose completa. Hoy solo cubre heading; x/y siempre inválidos. CENTRAL debe tratar x/y como N/A independientemente de este campo. |
| 7 | `ball_x_mm` | i16 | mm | −32768..+32767 | **REAL** pero escala sin calibrar (`CAMERA_UNIT_TO_MM=10.0`, `cameras_runtime.cpp:25`) | Posición X de la pelota relativa al robot. Marco robot: +x = derecha, +y = frente. |
| 9 | `ball_y_mm` | i16 | mm | id. | **REAL** pero escala sin calibrar | Posición Y de la pelota relativa al robot. |
| 11 | `ball_visible` | u8 | — | 0 / 1 | **REAL** — fusión dual front+back (`cameras_runtime.cpp:127`) | 1 = al menos una cámara viva detectó la pelota. |
| 12 | `ball_confidence` | u8 | — | 0..100 | **REAL** — ponderado por consenso dual (`cameras_fusion.cpp`) | Calidad de la detección de la pelota. 100 = ambas cámaras concuerdan. |
| 13 | `goal_opp_angle_centideg` | i16 | centideg | −18000..+18000 | **REAL** pero escala sin calibrar | Ángulo del arco rival respecto al frente del robot. Convención: atan2(x_mm, y_mm), 0° = frente, +90° = izquierda. |
| 15 | `goal_opp_distance_mm` | i16 | mm | 0..32767 | **REAL** pero escala sin calibrar | Distancia estimada al arco rival. Precision baja hasta calibrar `CAMERA_UNIT_TO_MM`. |
| 17 | `goal_opp_visible` | u8 | — | 0 / 1 | **REAL**; polaridad **HARDCODEADA** `yellow=opp` (`main_top.cpp:65`) | 1 = el arco rival está visible. Polaridad incorrecta en ~50% de partidos hasta leer el comando de árbitro. |
| 18 | `goal_own_visible` | u8 | — | 0 / 1 | **REAL**; polaridad **HARDCODEADA** `blue=own` (`main_top.cpp:69`) | 1 = el arco propio está visible. Mismo problema de polaridad. |
| 19 | `min_obstacle_mm` | u16 | mm | 0..65534; **65535 (`0xFFFF`) = sin lectura** | **PARCIAL** — los 4 ToF I2C son STUB (siempre `0xFFFF`); solo HC-SR04 activo pero bloqueante (`sensors_tof.cpp:23-35`) | Distancia al obstáculo más cercano. Sin los ToF, cubre solo sector frontal con HC-SR04. |
| 21 | `referee_cmd` | u8 | — | 0=STOP, 1=START, 2=HALFTIME, 3=RESET, 0xFF=UNKNOWN | **REAL** — llega vía COMM (`comm_arbiter.cpp:39-43`) | Último comando del árbitro. Arranca en `UNKNOWN=0xFF`. |
| 22 | `flags` | u8 | bitfield | ver §3.3 | **PARCIAL** — solo bits 1 y 3 se ponen (`main_top.cpp:76-80`) | Flags útiles para strategy (ver tabla §3.3). |

`sizeof(WorldSnapshot) == 23` bytes (calculado de `types.h:92-123` con `__attribute__((packed))`).
Frame completo TOP→CENTRAL: 23 + 7 overhead = **30 bytes**.

### 3.3 `flags` (bitfield, offset 22)

| Bit | Máscara | Nombre | Estado real | Significado |
|----:|---------|--------|-------------|-------------|
| 0 | `0x01` | `in_own_penalty_area` | **NO implementado** (`main_top.cpp:78`: "requiere pose absoluta — Nivel 2") | El robot está dentro de su propia área. Necesita fusión cámara+pose. |
| 1 | `0x02` | `partner_alive` | **REAL** — `comm_arbiter_partner_is_fresh()` con timeout 500 ms (`comm_arbiter.cpp:107`) | El robot partner respondió hace menos de 500 ms. |
| 2 | `0x04` | `partner_sees_ball` | **NO implementado** (`main_top.cpp:79`: "requiere parseo del partner snapshot — futuro") | El partner detectó la pelota. |
| 3 | `0x08` | `match_running` | **REAL** — `comm_arbiter_is_match_running()` (`comm_arbiter.cpp:80`) | El árbitro dio START y no dio STOP/HALFTIME/RESET. |
| 4-7 | `0xF0` | reservados | — | Escribir 0; receptor ignora. |

### 3.4 Convención de ángulos (sin ambigüedad)

```
            +Y  (FRENTE del robot)
             │   heading = 0°, goal_opp_angle = 0°
             │
   −90.00° ──┼── +90.00°   (vista desde ARRIBA)
  (izquierda)│(derecha)
             │
            −Y  (ATRÁS)  = ±180.00° (±18000 centideg)
```

- **Heading** (`my_heading_centideg`): ángulo de orientación del robot en el
  marco cancha. 0 = orientación al momento del boot (no necesariamente norte).
  Positivo = horario visto desde arriba.
- **Ball / goal angles**: ángulo en marco ROBOT, `atan2(x_mm, y_mm)`.
  0° = frente del robot, +90° = izquierda del robot.
- **`ball_x_mm`, `ball_y_mm`**: vector pelota en marco robot (mm).
  `+y` = frente, `+x` = lateral derecho.
- **Unidades**: todos los ángulos en **centidegrees** (grados × 100).
  `+9000 = +90.00°`, `−18000 = −180.00°`.

### 3.5 Frecuencia y timing

- Frecuencia de emisión: **100 Hz** (`main_top.cpp:129`: `if (g_since_snapshot >= 10)`).
- Enlace: Serial2 a **230400 baud, 8N1** (`comm_central.cpp:16`).
- Tiempo de transmisión de un frame de 30 bytes a 230400 baud:
  `30 × 10 bits / 230400 bps ≈ 1.3 ms`.
- SEQ: contador 0–255, envuelve; se puede usar para detectar pérdidas.

### 3.6 Ejemplo byte-a-byte (frame completo)

Escenario: heading=+45.00°, pelota visible a (+120, +200) mm, arco opp
visible a +15.00° y ~300 mm, arco own no visible, min_obstacle=0xFFFF (sin
ToF), referee_cmd=1 (START), flags=0x0A (partner_alive + match_running).

```
my_x_mm=0         → 00 00
my_y_mm=0         → 00 00
my_heading=4500   → 94 11  (LE: 0x1194 = 4500 = +45.00°)
my_pose_conf=60   → 3C
ball_x_mm=120     → 78 00  (LE: 0x0078 = 120)
ball_y_mm=200     → C8 00  (LE: 0x00C8 = 200)
ball_visible=1    → 01
ball_conf=85      → 55
goal_opp_angle=1500→ DC 05 (LE: 0x05DC = 1500 = +15.00°)
goal_opp_dist=300 → 2C 01  (LE: 0x012C = 300)
goal_opp_vis=1    → 01
goal_own_vis=0    → 00
min_obstacle=65535→ FF FF  (sentinel sin ToF)
referee_cmd=1     → 01
flags=0x0A        → 0A

Payload completo (23 bytes):
00 00 00 00 94 11 3C 78 00 C8 00 01 55 DC 05 2C 01 01 00 FF FF 01 0A

Frame completo (= 0xAA LEN TYPE SEQ PAYLOAD CRC16 0x55):
AA 17 60 XX 00 00 00 00 94 11 3C 78 00 C8 00 01 55 DC 05 2C 01 01 00 FF FF 01 0A [CRC_H] [CRC_L] 55
```
(LEN=0x17=23; TYPE=0x60; SEQ=XX=depende del contador en curso; CRC calculado
sobre `LEN+TYPE+SEQ+PAYLOAD`. El receptor debe calcularlo para validar; no se
provee valor numérico aquí porque SEQ varía.)

---

## 4. Mensajes con COMM (placa árbitros + partner) — Serial4

### 4.1 `COMM_REFEREE_CMD` — COMM → TOP (TYPE `0x30`)

| Payload | Tipo | Bytes | Valores válidos |
|---------|------|-------|-----------------|
| byte 0 | `uint8` | 1 | 0=STOP, 1=START, 2=HALFTIME, 3=RESET |

Implementado en `comm_arbiter.cpp:38-43`. El campo `referee_cmd` del
`WORLD_SNAPSHOT` refleja el último valor recibido. Arranca en `0xFF=UNKNOWN`.

**Sin COMM operativa:** la FSM de CENTRAL queda en `WAIT_START` indefinidamente
(bloqueante). Es un P0 para Incheon (TASK-006/TASK-024: fallback manual o COMM
operativa obligatoria antes de la primera competencia).

### 4.2 `TOP_STATUS_REPLY` — TOP → COMM (TYPE `0x32`), 5 bytes

Definido inline en `comm_arbiter.cpp:83-88` (struct local, no en `types.h`):

| Off | Campo | Tipo | Significado |
|----:|-------|------|-------------|
| 0 | `role` | u8 | Rol del robot (0=arquero, 1=delantero). Hoy valor hardcodeado/sin leer |
| 1 | `error_flags` | u8 | Bitfield de errores de hardware. Definición no formalizada aún |
| 2 | `battery_mv` | u16 LE | Tensión de batería en mV. Fuente no conectada (retorna 0) |
| 4 | `match_running` | u8 | 0/1 — espejo del estado interno de COMM |

Enviado en respuesta a `COMM_STATUS_REQ`. El handler actual recibe el request
pero **no responde automáticamente** (`comm_arbiter.cpp:44-48`); el caller
(`main_top.cpp`) tampoco invoca `comm_arbiter_send_status()` periódicamente.
Resultado: COMM nunca recibe status del robot. Gap a cerrar.

### 4.3 `COMM_PARTNER_DATA` — COMM → TOP (TYPE `0x40`), 12 bytes

Struct `PartnerSnapshot` definida en `comm_arbiter.h:48-56`:

| Off | Campo | Tipo | Significado |
|----:|-------|------|-------------|
| 0 | `x_mm` | i16 LE | Posición X del partner en cancha (mm) |
| 2 | `y_mm` | i16 LE | Posición Y del partner |
| 4 | `heading_centideg` | i16 LE | Heading del partner (centideg) |
| 6 | `ball_x_mm` | i16 LE | Pelota detectada por el partner (marco robot del partner) |
| 8 | `ball_y_mm` | i16 LE | id. |
| 10 | `ball_visible` | u8 | 0/1 — el partner ve la pelota |
| 11 | `partner_state` | u8 | Estado FSM del partner (encoding no formalizado) |

Recibido y almacenado en `g_partner`. Accesible vía
`comm_arbiter_get_partner()`. El flag `partner_alive` en `WorldSnapshot.flags`
bit 1 se pone si llegó un frame hace menos de 500 ms (`comm_arbiter.cpp:107`).
El flag `partner_sees_ball` (bit 2) **NO se extrae** (`main_top.cpp:79`).

### 4.4 `TOP_PARTNER_DATA` — TOP → COMM (TYPE `0x41`), 12 bytes

Mismo `PartnerSnapshot` (12 bytes). TOP envía su propio snapshot al COMM para
retransmisión ESP-NOW al partner. **El llamado a `comm_arbiter_send_partner()`
NO aparece en el loop de `main_top.cpp`** — la transmisión de datos propios al
partner no está conectada al loop principal. Gap.

---

## 5. Datos recibidos desde DOWN — Serial1 (`0x11` / `0x12`)

Referencia completa: `docs/firmware/CONTRATO-DATOS-DOWN.md` §4.

TOP recibe `Pose2D` (TYPE `0x11`, 7 B) y `Velocity2D` (TYPE `0x12`, 7 B) de
DOWN a 100 Hz. Comportamiento actual:

- El frame es parseado y almacenado en `comm_down.cpp` (`g_pose`, `g_vel`).
- Frescos con `is_*_fresh()` usando `DOWN_HEARTBEAT_TIMEOUT_MS = 500 ms`
  (`config_top.h:99`).
- **`my_x_mm` y `my_y_mm` del snapshot NO se populan con esta data.**
  `build_snapshot()` (`main_top.cpp:48-51`) escribe `0` directamente,
  ignorando `comm_down_get_pose()`.

### Regla de interpretación de `Pose2D` (contrato DOWN §4)

- `confidence == 0` → pose INVÁLIDA (no hay OTOS o no listo). TOP **no debe**
  mostrar (0,0) como posición real ni fusionarla con peso alguno.
- `confidence 1..100` → calidad de la estimación de DOWN.
- `slip_estimate` de `Velocity2D`: 0 = sin patinaje; >50 = anomalía (patada/choque).

El `comm_down.h` expone `comm_down_is_pose_fresh()` / `comm_down_get_pose()`.
El consumo real en `build_snapshot()` ignora estas funciones completamente.
Hasta que se implemente la fusión, todo lo que CENTRAL recibe sobre la pose
del robot son los dos ceros.

---

## 6. Reglas de interpretación obligatorias para CENTRAL al consumir `WORLD_SNAPSHOT`

### 6.1 Qué campos confiar HOY

| Campo | Confianza hoy | Condición para usar |
|-------|--------------|---------------------|
| `my_heading_centideg` | ALTA si `my_pose_confidence > 0` | Usar si `my_pose_confidence != 0` |
| `ball_visible`, `ball_x_mm/y_mm` | MEDIA | Usar; escala sin calibrar (factor ~10×); decisiones de approach OK, distancias exactas NO |
| `ball_confidence` | MEDIA | Ponderar acciones según valor |
| `goal_opp_angle_centideg` | MEDIA | Usar para dirección de tiro; distancia NO confiable |
| `goal_opp_visible`, `goal_own_visible` | MEDIA PERO polaridad incorrecta | Solo confiable luego de implementar lectura de `referee_cmd` para corrección de polaridad (TASK-024) |
| `min_obstacle_mm` | BAJA — solo HC-SR04 frontal | Usar con precaución; 0xFFFF = sin datos (ignorar para decisiones críticas) |
| `referee_cmd` | ALTA si COMM operativa | Confiable si COMM está viva (`partner_alive` en flags) |
| `flags.match_running` (bit 3) | ALTA si COMM operativa | Depende de COMM |
| `flags.partner_alive` (bit 1) | ALTA | Confiable |

### 6.2 Qué campos son N/A hoy (NUNCA usar para decisiones)

| Campo | Razón | Nota |
|-------|-------|------|
| `my_x_mm` | Hardcodeado a 0 | Nunca representa posición real |
| `my_y_mm` | Hardcodeado a 0 | id. |
| `my_pose_confidence` | Cubre solo heading, no x/y | Incluso si > 0, x/y siguen siendo 0 |
| `flags.in_own_penalty_area` (bit 0) | Nunca se pone (requiere pose) | Siempre 0; ignorar |
| `flags.partner_sees_ball` (bit 2) | Nunca se pone | Siempre 0; ignorar |
| `goal_opp_distance_mm`, `goal_own_distance_mm` | Factor 10× no calibrado | Usar para lógica cualitativa (cercano/lejano), no para PID de distancia exacta |

### 6.3 Valor de `min_obstacle_mm == 0xFFFF`

`0xFFFF` es el sentinel `TOF_NO_READING` (`sensors_tof.h:26`). Significa "no
hay lectura disponible" (los 4 ToF I2C son stub). CENTRAL **no debe
interpretar 0xFFFF como "sin obstáculos"**: debe tratarlo como "estado de
evasión desconocido". Si solo el HC-SR04 aporta datos, la cobertura es solo
frontal (~60°).

### 6.4 Versionado del contrato

No existe campo `schema_version` en `WorldSnapshot` v1 (a diferencia de
`LineStatusV2`). Si se requiere versionado (cambio de layout), se recomienda
ocupar uno de los bits reservados de `flags` (bits 4-7) o incrementar
`contract-schema` en este documento y anunciar cambio de layout a CENTRAL.
**Hoy `contract-schema: 1` = este layout.**

---

## 7. Diseño de módulos del programa TOP

### 7.1 Arquitectura del loop principal (`main_top.cpp`)

```
setup():
  sensors_imu_init()      ← BLOQUEANTE ~3-5 s (calibración BNO055)
  sensors_tof_init()      ← no bloqueante (stub)
  cameras_init()          ← abre Serial3 + Serial5
  comm_down_init()        ← abre Serial1
  comm_arbiter_init()     ← abre Serial4
  comm_central_init()     ← abre Serial2

loop() (no tiene period fijo — corre tan rápido como puede):
  comm_down_tick()        ← drena Serial1, OTOS de DOWN
  comm_arbiter_tick()     ← drena Serial4, COMM/árbitros/partner
  comm_central_tick()     ← drena Serial2, comandos de CENTRAL
  cameras_tick()          ← drena Serial3 + Serial5, parsers OpenMV
  if (every 10 ms) sensors_imu_tick()
  if (every 30 ms) sensors_tof_tick()
  if (every 10 ms) build_snapshot() + comm_central_send_snapshot()
  if (every 500 ms) Serial.print debug  ← SIN gate de competición (P0)
```

### 7.2 Clasificación de módulos: lógica pura testeable vs HW glue

| Módulo | Archivo(s) | Tipo | Testeable host-native | Notas |
|--------|------------|------|----------------------|-------|
| Parser cámara OpenMV | `src/top/cameras.cpp` | **PURA** | Sí (solo `uint8_t feed()`) | Parser state machine sin dependencia Arduino. Los 3 headers 201/202/203 + heurística sentinel. |
| Fusión dual de cámaras | `src/shared/cameras_fusion.{h,cpp}` | **PURA** | **SÍ — ya testeada** (16 tests `test_cameras_fusion`) | Rotación 180° cámara trasera, promedio ponderado, watchdog externo. |
| Proto encoder/decoder | `src/shared/proto.{h,cpp}` | **PURA** | **SÍ — ya testeada** (`test_proto`) | FrameDecoder con stats. |
| CRC16 | `src/shared/crc16.{h,cpp}` | **PURA** | SÍ | CRC-16/CCITT-FALSE. |
| Fusión pose (cámara+IMU+OTOS) | NO EXISTE | **PURA (a crear)** | Debería vivir en `src/shared/` | EKF o filtro complementario. Ausente es el mayor gap de TOP. |
| World snapshot builder | `main_top.cpp:43-83` | **HW-BOUND** | No (llama a getters de HW) | Candidato a refactorizar: extraer lógica de construcción a función pura que reciba structs. |
| Watchdog de cámaras | `cameras_runtime.cpp` | **HW-BOUND** | No (usa `millis()`) | El `last_ms==0` guard está implementado (`cameras_runtime.cpp:47-48`). |
| Wiring cámaras UART | `cameras_runtime.cpp` | **HW-BOUND** | No | `Serial3` / `Serial5`; cota 64 bytes/tick implementada. |
| IMU BNO055 dual | `sensors_imu.cpp` | **HW-BOUND** | No (usa Wire + Adafruit_BNO055) | Setup bloqueante (~5s); tick no bloqueante. |
| ToF + HC-SR04 | `sensors_tof.cpp` | **HW-BOUND** | No | HC-SR04 usa `pulseIn` bloqueante (P0: TASK-014); 4 ToF I2C son stub. |
| COMM arbiter | `comm_arbiter.cpp` | **HW-BOUND** | No (usa Serial4) | `guard last_ms==0` ausente en `comm_arbiter_partner_is_fresh()` (`comm_arbiter.cpp:107`): `millis()-0 < 500` → true al boot antes de recibir cualquier dato. |
| Comm DOWN | `comm_down.cpp` | **HW-BOUND** | No (usa Serial1) | Mismo bug de guard: `fresh(0)` → true al boot. |
| Comm CENTRAL | `comm_central.cpp` | **HW-BOUND** | No (usa Serial2) | Handler de `CENTRAL_RESET_TOP` recibido pero no procesado. |

**Módulos puros candidatos a mover a `src/shared/`** (hoy viven en `src/top/`):
- `cameras.h/cpp` — `CameraParser` es 100% pura; sin `#include <Arduino.h>`.
  Moverla a `src/shared/` permite unit tests nativos del parser (gap TASK-023).

---

## 8. Gaps auditados — qué falta para "candidato real, no esqueleto"

### P0 — Bloqueantes para Incheon

| ID | Gap | Archivo:línea | Risk-no-fix | Risk-fix | Tiempo est. |
|----|-----|--------------|-------------|----------|-------------|
| G-TOP-01 | **Serial2 (→CENTRAL) no confirmado en HW** | `config_top.h:40-42` | Snapshot nunca llega → CENTRAL en `motors_stop()` permanente → robot inerte | Medir con osciloscopio, posible swap de constantes | 2h |
| G-TOP-02 | **Polaridad de arco hardcodeada** `yellow=opp` | `main_top.cpp:65` | En ~50% de partidos el robot ataca su propio arco | Leer `referee_cmd` para aplicar inversión; fácil una vez que COMM anda | 3h |
| G-TOP-03 | **Rol no leído** (`PIN_ROLE_DIPSWITCH` sin `digitalRead`) | `config_top.h:87` + `main_top.cpp` (ausente) | Arquero y delantero son indistinguibles al boot; `TOP_STATUS_REPLY` manda rol=0 siempre | Agregar 1 línea en `setup()`, propagar rol | 1h |
| G-TOP-04 | **`pulseIn` bloqueante en HC-SR04** en el loop | `sensors_tof.cpp:32` | Loop puede tardar hasta 25 ms en cada lectura → violación de loop timing → snapshot retrasado | Convertir a lectura no bloqueante con ISR o timer | 4h |
| G-TOP-05 | **`Serial.print` debug sin gate de competición** | `main_top.cpp:136-167` | En partido puede agregar latencia perceptible al loop | Agregar flag `competition_mode` o macro | 1h |
| G-TOP-06 | **Guard `last_ms==0` ausente** en `comm_arbiter_partner_is_fresh()` y `comm_down.cpp:fresh()` | `comm_arbiter.cpp:107`, `comm_down.cpp:25-27` | Al boot, antes de recibir datos, ambos reportan "fresh=true" → false positives → decisiones con datos vacíos | Agregar `if (last_ms == 0) return false;` | 30 min |

### P1 — Impacto alto en partidos

| ID | Gap | Archivo:línea | Risk-no-fix | Risk-fix | Tiempo est. |
|----|-----|--------------|-------------|----------|-------------|
| G-TOP-07 | **Pose absoluta (x,y) hardcodeada a 0** | `main_top.cpp:49-50` | Sin posición: in_own_penalty_area siempre false; estrategia GK sin ancla posicional | EKF o fusión simple cámara+OTOS; ~2 semanas de trabajo | 2 semanas |
| G-TOP-08 | **`CAMERA_UNIT_TO_MM=10.0` sin calibrar** | `cameras_runtime.cpp:25` | Distancias a pelota/arco erróneas en factor desconocido → approach/kick con timing incorrecto | Calibrar contra cancha real (30/50/80/100 cm); reemplazar placeholder | 3h |
| G-TOP-09 | **4 ToF I2C completamente stub** | `sensors_tof.cpp:51-67` | `min_obstacle_mm=0xFFFF` siempre → sin evasión de robots | Elegir lib según modelo (VL53L7CX vs VL53L5CX), enumeración I2C, TASK-012 | 1-2 días |
| G-TOP-10 | **`TOP_PARTNER_DATA` no se envía en el loop** | `main_top.cpp` (ausencia) | Partner nunca recibe datos propios vía ESP-NOW → coordinación inter-robot nula | Llamar `comm_arbiter_send_partner()` en el loop con datos del snapshot | 1h |
| G-TOP-11 | **`COMM_STATUS_REQ` no se responde automáticamente** | `comm_arbiter.cpp:44-48` | COMM display/OLED no puede mostrar estado del robot | Callback o response inline al recibir el request | 2h |
| G-TOP-12 | **`static_assert(sizeof(WorldSnapshot)==23)` ausente** | `types.h` (ausente) | Cambio accidental de tamaño no detecta hasta runtime → protocolo silenciosamente roto | Una línea | 5 min |
| G-TOP-13 | **`CENTRAL_RESET_TOP` recibido pero no procesado** | `comm_central.cpp:19-23` | CENTRAL no puede recuperar TOP por comando (Nivel 2 de la escalera de reset) | Definir y ejecutar acción de reset del world model | 2h |

### P2 — Capitalizable a 2027

| ID | Gap | Notas |
|----|-----|-------|
| G-TOP-14 | **Fusión EKF pose** (cámara+IMU+OTOS) | No existe. Módulo puro en `src/shared/`. Mayor inversión de arquitectura. |
| G-TOP-15 | **Migrar `CameraParser` a `src/shared/`** para habilitar unit tests nativos | Mover sin cambiar código; agregar test. TASK-023. |
| G-TOP-16 | **`partner_sees_ball` (flags bit 2) no se extrae** de `PartnerSnapshot` | Fácil: leer `g_partner.ball_visible` en `build_snapshot()`. |
| G-TOP-17 | **IMU bloqueante en `setup()`** (~3-5 s) | Aceptable para Incheon; refactorizar para 2027 si el boot time importa. |
| G-TOP-18 | **`PartnerSnapshot.partner_state` encoding no formalizado** | Definir en `proto.h` o `types.h`. |
| G-TOP-19 | **Heartbeat explícito TOP→CENTRAL ausente** | Solo se envía snapshot; no hay `LINK_HEARTBEAT` si no hay dato nuevo. Según diseño de comunicaciones §4. |

---

## 9. Checklist para quien programe cada placa

**TOP (emisor del snapshot):**
- Implementar `digitalRead(PIN_ROLE_DIPSWITCH)` en `setup()` y propagar rol.
- Corregir polaridad de arco usando `referee_cmd` una vez recibido.
- Agregar guard `last_ms == 0` en todos los `is_*_fresh()`.
- Convertir HC-SR04 a no-bloqueante.
- Gatear `Serial.print` debug con flag de competición.
- Agregar `static_assert(sizeof(WorldSnapshot)==23)`.
- Confirmar mapeo físico de Serial2→CENTRAL con osciloscopio.
- Calibrar `CAMERA_UNIT_TO_MM` contra cancha real.

**CENTRAL (receptor):**
- Usar `my_x_mm / my_y_mm` **SOLO** si en el futuro `my_pose_confidence > 0`
  **Y** se sabe que TOP ya implementa fusión de pose. Hoy = siempre N/A.
- `min_obstacle_mm == 0xFFFF` → no interpretar como "sin obstáculos".
- `goal_opp_visible` / `goal_own_visible` → confiar en polaridad solo tras
  recibir `referee_cmd != UNKNOWN` y aplicar la corrección correspondiente.
- Arranca en LOST; esperar primer `WORLD_SNAPSHOT` válido antes de mover motores.

---

## 10. Fuentes

- `software/teensy/Soccer 2026/src/top/main_top.cpp`
- `software/teensy/Soccer 2026/src/top/config_top.h`
- `software/teensy/Soccer 2026/src/top/cameras.cpp` / `cameras_runtime.cpp`
- `software/teensy/Soccer 2026/src/top/sensors_imu.cpp` / `sensors_tof.cpp`
- `software/teensy/Soccer 2026/src/top/comm_central.cpp` / `comm_arbiter.cpp` / `comm_down.cpp`
- `software/teensy/Soccer 2026/src/shared/types.h` / `proto.h` / `cameras_fusion.h`
- `docs/firmware/CONTRATO-DATOS-DOWN.md` (contrato DOWN, referencia para Pose2D/Velocity2D)
- `docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md`
- `research/completed/2026-05-18-estado-firmware-robot-evaluacion-critica.md`
- Cálculo de offsets/tamaños: verificado por script Python sobre la definición de `types.h`.
