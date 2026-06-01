---
title: "Contrato de datos de la placa CENTRAL — qué recibe, qué decide, qué emite (v1)"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Sonnet 4.6, Anthropic)"
status: final
tags: [comunicacion, firmware, protocolo, contrato, central-board, ambos]
robot: ambos
area: comunicacion
tipo: protocolo
contract-schema: 2
related:
  - software/teensy/Soccer 2026/src/shared/proto.h
  - software/teensy/Soccer 2026/src/shared/types.h
  - software/teensy/Soccer 2026/src/central/main_central.cpp
  - software/teensy/Soccer 2026/src/central/motors_zircon.cpp
  - software/teensy/Soccer 2026/src/central/strategy.cpp
  - software/teensy/Soccer 2026/src/central/world_model.cpp
  - software/teensy/Soccer 2026/src/central/comm_top.cpp
  - software/teensy/Soccer 2026/src/central/comm_down.cpp
  - docs/firmware/CONTRATO-DATOS-DOWN.md
  - docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md
---

# Contrato de datos CENTRAL — referencia única para programar CENTRAL, DOWN y TOP

> **Propósito.** Definir SIN AMBIGÜEDAD qué datos recibe la placa CENTRAL, qué
> decide, y qué emite — hacia los motores (PWM local), hacia DOWN (comandos de
> reinicio/calibración) y hacia TOP (resets/comandos administrativos). Quien
> programe TOP sabe exactamente qué mandar; quien programe DOWN sabe qué esperar;
> quien audite CENTRAL sabe exactamente qué debe hacer. Si el código y este
> documento difieren, **se corrige el que esté mal y se versiona el contrato**
> (`contract-schema`).

---

## 0. Frontera de responsabilidad de CENTRAL (leer primero)

### 0.1 Qué ES CENTRAL

CENTRAL es el **master decisor del robot**. Corre en el **Teensy 4.1 montado
sobre la placa Zircon Rev v15**. Es la única placa que toca los motores y el
kicker directamente.

CENTRAL **recibe** datos del mundo (vía TOP) y datos de emergencia de línea
(vía DOWN), **ejecuta** la FSM + PIDs, y **actúa** sobre los actuadores
(motores PWM, kicker GPIO) sin intermediario.

**Diagrama de posición en el sistema:**

```
     [TOP]  ←──── WORLD_SNAPSHOT (STREAM, 100 Hz)  ────►  [CENTRAL]
                   (via Serial1 de CENTRAL)

     [DOWN] ←──── LINE_URGENT (EVENTO+STREAM, 200 Hz) ──►  [CENTRAL]
                   (via Serial2 de CENTRAL)

     [CENTRAL] ──► motors PWM + kicker GPIO  (LOCAL, no UART)

     [CENTRAL] ──► CENTRAL_RESET_OTOS / CENTRAL_CALIB_LINE  (COMANDO, via Serial2 → DOWN)

     [CENTRAL] ──► CENTRAL_RESET_TOP / CENTRAL_TOP_CMD       (COMANDO, via Serial? → TOP)
                   (anticipado en proto.h:51-52, NO implementado)
```

### 0.2 Qué NO ES CENTRAL (corrección de `config_central.h`)

`config_central.h` (línea 1–14) describe CENTRAL como **"motor server"** que
"recibe `MotorCommand` por Serial1 desde la placa TOP". **Esto es incorrecto
en la arquitectura actual.** El modelo "motor server" es el **rol legacy**
(`MsgType::MOTOR_COMMAND = 0x50`, marcado `LEGACY` en `proto.h:60-65`).

**La arquitectura vigente** (`main_central.cpp:1-10`) es:
- CENTRAL **recibe `WORLD_SNAPSHOT`** (no `MotorCommand`) de TOP.
- CENTRAL **corre la FSM/estrategia localmente**.
- CENTRAL **calcula sus propios `MotorCommand`** internamente vía `strategy_tick()`.
- CENTRAL **aplica PWM directamente** a los H-bridges del Zircon sin pasarle los
  comandos de motor a nadie.

**Consecuencia documental:** cualquier nuevo desarrollo que lea `config_central.h`
sin leer `main_central.cpp` entenderá el sistema al revés. Este documento tiene
precedencia. `config_central.h` debe ser corregido (ver §6, GAP-004).

### 0.3 División de responsabilidad entre CENTRAL y TOP

| Responsabilidad | Quién la tiene |
|---|---|
| Fusión sensorial (IMU + OTOS + cámaras + ToF) | **TOP** |
| Construcción de `WorldSnapshot` | **TOP** |
| Envío del snapshot a 100 Hz | **TOP** |
| Clasificación de línea (lateral/fondo/frente en marco-cancha) | **CENTRAL** (con heading del snapshot) |
| FSM táctica (SEARCH/APPROACH/PATROL/INTERCEPT…) | **CENTRAL** |
| PIDs de heading, lateral (arquero) y aproximación | **CENTRAL** |
| Cinemática inversa omni-3 | **CENTRAL** |
| Aplicación de PWM a motores y kicker | **CENTRAL** (hardware directo) |
| Protección de borde (EMERGENCY_LINE) | **CENTRAL** (bypass de FSM) |
| Pose absoluta en cancha | **TOP** (dentro del snapshot) |
| Odometría OTOS | **DOWN → TOP** |

---

## 1. Capa de transporte (referencia a proto.h)

El transporte de TODOS los enlaces es **idéntico al definido en
`docs/firmware/CONTRATO-DATOS-DOWN.md` §1**. Se refrencia aquí sin duplicar:

```
┌──────┬─────┬──────┬─────┬────────────┬──────────┬──────┐
│ 0xAA │ LEN │ TYPE │ SEQ │  PAYLOAD   │ CRC16 BE │ 0x55 │
│  1B  │ 1B  │  1B  │  1B │  LEN bytes │   2B     │  1B  │
└──────┴─────┴──────┴─────┴────────────┴──────────┴──────┘
```

- Framing: `proto.h` (START=`0xAA`, END=`0x55`, CRC-16/CCITT-FALSE).
- Payload little-endian (ARM Teensy). CRC big-endian.
- UART 230400 baud, 8N1, sin control de flujo en todos los enlaces.
- `FrameDecoder` con state machine, byte-a-byte, resync automático.

**Asignación de UARTs en CENTRAL** (`main_central.cpp:76-78`,
`comm_top.cpp:30`, `comm_down.cpp:30`):

| Enlace | Serial | Pines Teensy | Dirección |
|---|---|---|---|
| TOP → CENTRAL | `Serial1` | RX1=0, TX1=1 | recibe `WORLD_SNAPSHOT` |
| DOWN → CENTRAL | `Serial2` | RX2=7, TX2=8 | recibe `LINE_URGENT`, envía comandos |

> **ADVERTENCIA CRÍTICA (P0):** Los comentarios de `config_central.h:79-82`
> dicen "Serial1 RX1" para el enlace con TOP. Pero `comm_top.cpp:30` abre
> `Serial1` y `comm_down.cpp:30` abre `Serial2`. Si el cableado físico TOP→
> CENTRAL llega a los pines 7/8 (Serial2) en lugar de 0/1 (Serial1), **CENTRAL
> nunca recibe snapshots y los motores quedan parados permanentemente**. El
> mapeo físico está PENDIENTE DE VERIFICACIÓN CON OSCILOSCOPIO (TASK-008/014).

---

## 2. Qué RECIBE CENTRAL y de quién

### 2.1 De TOP: `WORLD_SNAPSHOT` (TYPE `0x60`)

**Clase de mensaje:** STREAM (coalesce — drenar buffer y quedarse con el
último frame válido). `docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md` §3.

**Frecuencia:** 100 Hz (TOP genera a 100 Hz).

**Transporte:** Serial1 de CENTRAL (`comm_top.cpp:30`). Recibido en
`comm_top_tick()` (`comm_top.cpp:33-43`), decodificado con `FrameDecoder`,
entregado a `world_model_apply_snapshot()` (`comm_top.cpp:22-24`).

**Payload:** `struct WorldSnapshot` de `types.h:92-123`. 27 bytes (WorldSnapshot schema v2: +ball_vx/vy).
`static_assert(sizeof(WorldSnapshot)==27)` presente en el código (RESUELTO 2026-05-18, commit 2a9064e).

#### 2.1.1 Layout exacto de `WorldSnapshot` (`types.h:92-123`)

| Off | Campo | Tipo | Unidad | Rango / Sentinela | Significado |
|----:|-------|------|--------|-------------------|-------------|
| 0 | `my_x_mm` | i16 | mm | −32767..+32767 | Pose X fusionada en cancha. **Hardcodeado a 0** (GAP-002). |
| 2 | `my_y_mm` | i16 | mm | −32767..+32767 | Pose Y fusionada en cancha. **Hardcodeado a 0** (GAP-002). |
| 4 | `my_heading_centideg` | i16 | centideg | −18000..+18000 | Heading fusionado IMU dual. **Funcional.** |
| 6 | `my_pose_confidence` | u8 | — | 0..100 | Confianza en la pose (0 = pose inválida). |
| 7 | `ball_x_mm` | i16 | mm | −32767..+32767 | Posición X de la pelota relativa al robot. |
| 9 | `ball_y_mm` | i16 | mm | −32767..+32767 | Posición Y de la pelota relativa al robot. |
| 11 | `ball_visible` | u8 | — | 0 / 1 | 1 = pelota detectada por cámara(s). |
| 12 | `ball_confidence` | u8 | — | 0..100 | Confianza en la detección (0 = inválida). |
| 13 | `ball_vx_mm_s` | i16 | mm/s | −32767..+32767; **0 = N/A** | Velocidad de la pelota eje X en marco robot (mm/s). TOP la llena en su plan; hasta entonces 0. CENTRAL la usa para clasificar trayectoria (dejar circular / interceptar / desviar). |
| 15 | `ball_vy_mm_s` | i16 | mm/s | −32767..+32767; **0 = N/A** | Velocidad de la pelota eje Y en marco robot (mm/s). Mismo criterio que `ball_vx_mm_s`; el par `(0,0)` significa N/A. |
| 17 | `goal_opp_angle_centideg` | i16 | centideg | −18000..+18000 | Ángulo al arco rival relativo al frente del robot. |
| 19 | `goal_opp_distance_mm` | i16 | mm | 0..32767 | Distancia estimada al arco rival. |
| 21 | `goal_opp_visible` | u8 | — | 0 / 1 | 1 = arco rival visible. |
| 22 | `goal_own_visible` | u8 | — | 0 / 1 | 1 = arco propio visible. |
| 23 | `min_obstacle_mm` | u16 | mm | 0..65535 | Obstáculo más cercano (ToF + HC-SR04). **Stub — siempre alto** (GAP-003). |
| 25 | `referee_cmd` | u8 | — | 0=stop,1=start,2=halftime,3=reset | Comando árbitro vigente. |
| 26 | `flags` | u8 | bitfield | ver abajo | Flags tácticos. |

`sizeof(WorldSnapshot) == 27` (schema v2, +ball_vx/vy). `static_assert(sizeof==27)` presente (RESUELTO 2026-05-18, commit 2a9064e: GAP-001 cerrado).

#### 2.1.2 `flags` de `WorldSnapshot` (`types.h:118-122`)

| Bit | Máscara | Nombre | Interpretación en CENTRAL |
|----:|---------|--------|--------------------------|
| 0 | `0x01` | `in_own_penalty_area` | El robot está en su propia área de penales. |
| 1 | `0x02` | `partner_alive` | El compañero envió heartbeat reciente (ESP-NOW). |
| 2 | `0x04` | `partner_sees_ball` | El compañero reportó pelota visible. |
| 3 | `0x08` | `match_running` | Partido activo (árbitro dijo START). **Gate maestro de la FSM.** |
| 4-7 | `0xF0` | reservados | CENTRAL los ignora; TOP escribe 0. |

**Interpretación en `world_model.cpp`** (líneas 66-70):
- `match_running` = `flag_set(g_snap.flags, 3)` → bit 3 = máscara `0x08`.
- `in_own_penalty_area` = `flag_set(g_snap.flags, 0)` → bit 0 = `0x01`.
- `partner_alive` = bit 1 = `0x02`.
- `partner_sees_ball` = bit 2 = `0x04`.

#### 2.1.3 Convención de ángulos en `WorldSnapshot`

Heredada del marco del robot (igual que en DOWN):
- 0° = **frente del robot** (+Y).
- Positivo = sentido horario visto desde arriba (hacia la derecha).
- Rango `(-18000, +18000]` centidegrees.

`goal_opp_angle_centideg` es **relativo al frente del robot** (no absoluto de
cancha). CENTRAL lo usa directamente en la FSM para alinear el kicker.

`my_heading_centideg` es **absoluto en cancha** (referencia: orientación al
inicio del partido). CENTRAL lo usa para los PIDs de heading y para clasificar
la línea detectada en marco-cancha.

#### 2.1.4 Freshness y timeout de `WorldSnapshot`

`world_model_snapshot_is_fresh()` (`world_model.cpp:39-41`):
```cpp
return g_snap_last_ms > 0 && (millis() - g_snap_last_ms) < SNAPSHOT_TIMEOUT_MS;
```
`SNAPSHOT_TIMEOUT_MS = 500` ms (`world_model.cpp:13`).

**Guard `last_ms > 0`:** presente (correcto — evita falso "fresco" al arranque).

Si `!snapshot_is_fresh()` → `motors_stop()` + LED parpadea (`main_central.cpp:108-111`).

> **Nota de diseño:** el timeout de 500 ms es **un número provisional**. El
> diseño definitivo de comunicaciones exige que se mida el período del loop en
> hardware (TASK-014) y que los thresholds se calculen a partir de esa medición.
> 500 ms es conservador y seguro para la etapa actual, pero puede causar paradas
> espurias si el loop de TOP tiene latencia variable alta.

---

### 2.2 De DOWN: `LINE_URGENT` (TYPE `0x10`)

**Clase de mensaje:** EVENTO (bits `event_flags`) + STREAM (geometría de
línea). Política: latch OR para eventos; coalesce para geometría.
`docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md` §3.

**Frecuencia:** 100–200 Hz (DOWN genera a 200 Hz según diseño).

**Transporte:** Serial2 de CENTRAL (`comm_down.cpp:30`). Recibido en
`comm_down_tick()` (`comm_down.cpp:34-43`).

**Payload en protocolo objetivo (v2):** `struct LineStatusV2` de `types.h:126-140`.
16 bytes. Ver **`docs/firmware/CONTRATO-DATOS-DOWN.md` §3** para el layout
completo, convención de ángulos, ejemplos byte-a-byte y reglas de
interpretación. **Este documento NO redefine `LineStatusV2` — lo referencia.**

**Payload en código actual (v1):** `struct LineStatus` de `types.h:43-51`. 5
bytes. El firmware DOWN actual (`comm_down.cpp:21`) todavía decodifica
`LineStatus` v1. `world_model.cpp` almacena un `LineStatus g_line{}` (v1, línea 9).

> **DISCREPANCIA v1 vs v2:** El contrato DOWN define `LineStatusV2` (16 B,
> schema_version, data_valid, escape_angle, cross_track_mm, event_flags,
> quality…). El código de CENTRAL recibe y almacena `LineStatus` v1 (5 B, sin
> data_valid, sin escape_angle, sin quality). Hasta que DOWN migre a v2 y CENTRAL
> actualice `comm_down.cpp` y `world_model.cpp`, **CENTRAL opera con la
> geometría de línea reducida de v1**. Ver GAP-005.

#### 2.2.1 Campos de `LineStatus` v1 que CENTRAL usa hoy

| Campo | Tipo | Uso en CENTRAL |
|---|---|---|
| `angle_centideg` | i16 | `world_model_get_line_angle_deg()` → LINE_AVOID retreat vector |
| `depth_mm` | u8 | `world_model_get_line_depth()` → PID lateral arquero |
| `imminent_exit_flag` | u8 | `world_model_imminent_exit()` → bypass FSM → `motors_brake()` |
| `flags` | u8 | bit 0 = LIFTED (ignorado actualmente — GAP-006) |

**Campos de `LineStatusV2` que CENTRAL NO usa todavía** (requieren migración):
`data_valid`, `escape_angle_centideg`, `penetration_mm`, `cross_track_mm`,
`sensors_on_line`, `event_flags` completo (CORNER, LINE_END, CALIB_SUSPECT,
MUX_DEAD, DEGRADED_GEOMETRY), `quality`, `sample_age_ms`.

#### 2.2.2 Freshness y timeout de LINE_URGENT

`world_model_line_is_fresh()` (`world_model.cpp:43-45`):
```cpp
return g_line_last_ms > 0 && (millis() - g_line_last_ms) < LINE_TIMEOUT_MS;
```
`LINE_TIMEOUT_MS = 500` ms (`world_model.cpp:14`).

**Guard `last_ms > 0`:** presente (correcto).

Si `!line_is_fresh()` → CENTRAL ignora datos de línea pero NO para motores
(continúa con snapshot). Ver §4.2 para la política de fail-safe completa.

---

## 3. Qué EMITE CENTRAL y a quién

### 3.1 Hacia DOWN: comandos administrativos

#### 3.1.1 `CENTRAL_RESET_OTOS` (TYPE `0x20`) — COMANDO

**Clase:** COMANDO (procesar todos, idempotente).

**Payload:** 1 byte (`uint8`, valor `1` en la implementación actual).

**Cuándo se emite:** cuando strategy o algún módulo de CENTRAL decide
reiniciar la odometría de DOWN. **No hay llamador implementado hoy en
strategy.cpp** — la función existe (`comm_down_send_reset_otos()`,
`comm_down.cpp:46-54`) pero nadie la llama. Es un gancho pendiente.

**Frame completo** (SEQ=0x00, payload=1):
```
START = AA
LEN   = 01
TYPE  = 20
SEQ   = 00
PAYLOAD = 01
CRC16 = sobre LEN+TYPE+SEQ+PAYLOAD = sobre [01 20 00 01]
END   = 55
```
(CRC numérico: calcular con CRC-16/CCITT-FALSE sobre bytes `01 20 00 01`.)

**Transporte:** `Serial2.write()` de CENTRAL (`comm_down.cpp:54`).

#### 3.1.2 `CENTRAL_CALIB_LINE` (TYPE `0x21`) — COMANDO

**Clase:** COMANDO.

**Payload:** 1 byte. `0x00` = calibrar contra carpet (piso), `0x01` = calibrar
contra white (línea blanca), `0x02` = auto (definido en el contrato DOWN pero
no en el emisor actual).

**Cuándo se emite:** antes del partido, después de posicionar el robot. Llamado
vía `comm_down_send_calib_line(bool white)` (`comm_down.cpp:56-64`). **No hay
llamador implementado en strategy.cpp todavía.**

**Frame completo** (SEQ=0x01, calibrar white):
```
START = AA
LEN   = 01
TYPE  = 21
SEQ   = 01
PAYLOAD = 01    (true = white)
CRC16 = sobre [01 21 01 01]
END   = 55
```

**Transporte:** `Serial2.write()` de CENTRAL (`comm_down.cpp:63`).

---

### 3.2 Hacia TOP: comandos administrativos

#### 3.2.1 `CENTRAL_RESET_TOP` (TYPE `0x61`) — COMANDO

**Clase:** COMANDO.

**Estado:** **NO implementado.** `proto.h:51` define el TYPE pero no hay
`comm_top_send_reset()` ni llamador en el código actual. El diseño de
comunicaciones (`docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md`
§5.bis) lo anticipa como reset-por-comando del peer (Nivel 2 de la escalera de
recuperación). Ver GAP-007.

#### 3.2.2 `CENTRAL_TOP_CMD` (TYPE `0x62`) — COMANDO

**Clase:** COMANDO.

**Estado:** **NO implementado.** `proto.h:52` define el TYPE. Propósito:
comandos administrativos genéricos (p.ej. "recalibrar cámaras"). Ver GAP-007.

---

### 3.3 Salida a motores — interfaz lógica `MotorCommand → PWM` (LOCAL, no UART)

La salida principal de CENTRAL **no es UART**. Es PWM + GPIO directos sobre
los H-bridges del Zircon. No hay frame de protocolo aquí.

#### 3.3.1 Flujo completo `strategy_tick()` → PWM

```
strategy_tick()           (strategy.cpp:455-457)
     │
     └── retorna MotorCommand {vx_mm_s, vy_mm_s, omega_centideg_s, kicker_fire, dribbler_pwm}
                              (types.h:59-65)
     │
     ▼
motors_apply_command(cmd) (motors_zircon.cpp:113-138)
     │
     ├── omega_rad_s = cmd.omega_centideg_s × (π / 18000)   [centideg/s → rad/s]
     │
     ├── WheelSpeeds ws = inverse_kinematics(vx, vy, omega_rad_s, WHEELS)
     │       v_i = -vx·sin(θ_i) + vy·cos(θ_i) + omega·R   (kinematics.h:33-38)
     │       WHEELS[3] = {60°, -60°, 180°} × 100 mm radio  (motors_zircon.cpp:20-24)
     │       ⚠️ WHEEL_ANGLES_DEG y WHEEL_RADIUS_MM son TENTATIVOS (config_central.h:65-68)
     │
     ├── saturate_wheels(ws, MAX_SPEED_MM_S=1000.0f)         [proporcional, preserva dirección]
     │
     ├── for i in 0..2:
     │       pwm_signed[i] = wheel_speed_to_pwm(ws.wheel[i], 1000.0f, 255)
     │       apply_pwm_to_motor(i, pwm_signed)
     │           pwm > 0: INA=1, INB=0, analogWrite(PWM, pwm)
     │           pwm < 0: INA=0, INB=1, analogWrite(PWM, -pwm)
     │           pwm = 0: INA=0, INB=0, analogWrite(PWM, 0)  [libre, no frena]
     │
     └── kicker_update(cmd.kicker_fire)    [solo ROBOT2 — delantero]
             fire=1 + cooldown_ok → GPIO PIN_KICKER_SOL HIGH durante 80 ms
             luego LOW + cooldown 1500 ms antes del próximo disparo
             (motors_zircon.cpp:48-69, config_central.h:102-104)
```

#### 3.3.2 Estructura `MotorCommand` (`types.h:59-65`)

| Campo | Tipo | Unidad | Rango | Significado |
|---|---|---|---|---|
| `vx_mm_s` | i16 | mm/s | −32767..+32767 | Velocidad lateral (+ = derecha del robot) |
| `vy_mm_s` | i16 | mm/s | −32767..+32767 | Velocidad longitudinal (+ = frente del robot) |
| `omega_centideg_s` | i16 | centideg/s | −36000..+36000 | Vel. angular (+ = CCW visto desde arriba) |
| `kicker_fire` | u8 | — | 0 / 1 | 1 = disparar kicker. Solo delantero (ROBOT2). |
| `dribbler_pwm` | u8 | — | 0..255 | **Futuro** — no implementado. Siempre 0. |

**Saturation real:** `MAX_SPEED_MM_S = 1000.0f` mm/s (`config_central.h:75`).
`MAX_PWM = 255` (`config_central.h:74`). Saturación proporcional
(`kinematics.h:46` — preserva dirección del vector).

#### 3.3.3 Pinout de motores (dependiente de robot)

| Motor | ROBOT1 (arquero) | ROBOT2 (delantero) |
|---|---|---|
| Motor 0 | INA=2, INB=5, PWM=3 | INA=8, INB=7, PWM=6 |
| Motor 1 | INA=8, INB=7, PWM=6 | INA=11, INB=12, PWM=4 |
| Motor 2 | INA=11, INB=12, PWM=4 | INA=2, INB=5, PWM=3 |
| Kicker | — | PIN=23 (⚠️ A CONFIRMAR ENZO) |

Fuente: `config_central.h:24-35` (ROBOT1) y `:37-47` (ROBOT2).

#### 3.3.4 Modos especiales de actuación

| Modo | Función | Cuándo se activa | Efecto H-bridge |
|---|---|---|---|
| Normal | `motors_apply_command(cmd)` | Strategy tick OK | INA/INB por dirección + analogWrite |
| Free-stop | `motors_stop()` | Watchdog TOP (500 ms) | INA=0, INB=0, PWM=0 — frena por fricción |
| Brake activo | `motors_brake()` | `imminent_exit` detectado | INA=1, INB=1, PWM=0 — corto H-bridge |
| Kicker force-off | en `motors_stop()` y `motors_brake()` | Con cualquier parada | GPIO kicker → LOW inmediatamente |

**Precedencia de actuación en el loop** (`main_central.cpp:84-116`):
1. Si `imminent_exit && line_is_fresh` → `motors_brake()` + `return` (bypassa FSM).
2. Si `!snapshot_is_fresh` → `motors_stop()`.
3. Si snapshot fresco → `motors_apply_command(strategy_tick())`.

---

## 4. Reglas de interpretación obligatorias

### 4.1 Interpretación de `WorldSnapshot`

1. Si `snapshot_is_fresh() == false` → `motors_stop()` inmediato. No llamar a
   `strategy_tick()`. El robot NO "sigue jugando ciego".

2. Si `match_running == false` (flags bit 3 = 0) → todas las FSMs retornan al
   estado `WAIT_START` y emiten `MotorCommand{}` (ceros = stop). Cumple las
   reglas de partido: el robot NO se mueve fuera de juego.

3. `ball_visible = 0` → CENTRAL NO usa `ball_x_mm/ball_y_mm` para control.
   `world_model_ball_visible()` devuelve false; `strategy.cpp` transiciona a
   SEARCH en lugar de APPROACH (`strategy.cpp:193`).

4. `ball_confidence = 0` → igual que `ball_visible = 0` para efectos prácticos.
   **Bug actual:** `world_model.cpp:51` solo chequea `ball_visible`, no
   `ball_confidence`. Si TOP manda `ball_visible=1, ball_confidence=0`, CENTRAL
   lo trata como válido. Ver GAP-008.

5. `ball_vx_mm_s / ball_vy_mm_s` = velocidad de la pelota en marco robot (mm/s);
   el par `(0, 0)` significa N/A (TOP la llena en su plan de Nivel 2; hasta
   entonces ambos valen 0). CENTRAL los usa para clasificar la trayectoria de
   la pelota: dejar circular (si la pelota se aleja sola del arco propio),
   interceptar (si la trayectoria apunta al arco propio) o desviar (pelota
   cruzando el campo). Si ambos son 0, CENTRAL opera solo con posición.

6. `goal_opp_visible = 0` → CENTRAL no usa `goal_opp_angle/distance`. La FSM
   delantero degrada a APPROACH directo en lugar de POSITION+kick
   (`strategy.cpp:202-204`).

7. `my_pose_confidence` actualmente no se usa en ninguna decisión de CENTRAL
   (`world_model.cpp` no expone este campo). La pose `(my_x_mm, my_y_mm)` está
   hardcodeada a 0 en TOP (GAP-002); CENTRAL la lee pero la FSM no la usa para
   decisiones tácticas. No hay riesgo de crash, pero tampoco hay localización
   real.

8. `referee_cmd` relevante: `0x01` (start) → `match_running` flag viene ya
   procesado en TOP dentro del byte `flags`. CENTRAL no parsea `referee_cmd`
   directamente — usa el flag `match_running` derivado. Ver
   `world_model.cpp:66`.

### 4.2 Interpretación de LINE_URGENT (`LineStatus` v1) — retícula de fail-safe

La política de fail-safe de borde (`docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md`
§5.1) establece precedencia explícita:

| Situación | Acción de CENTRAL |
|---|---|
| `line_is_fresh() && imminent_exit_flag=1` | `motors_brake()` inmediato. **Prioridad máxima, bypass de FSM.** `main_central.cpp:95-102` |
| `!line_is_fresh()` | FSM corre SIN datos de línea. Strategy ignora `imminent_exit` y `LINE_AVOID`. **Bug de diseño:** modo ciego de borde activo. Ver GAP-009. |
| `!snapshot_is_fresh()` | `motors_stop()`. Independiente del estado de la línea. |
| Ambos LOST (snapshot + línea) | `motors_stop()` (gana la acción más conservadora). |

**Reglas de la v1 (actualmente implementadas):**

1. Si `imminent_exit_flag=1 && line_is_fresh()` → `motors_brake()`. No esperar
   al tick de strategy. Latencia objetivo: <15 ms desde detección en DOWN hasta
   freno activo (`main_central.cpp:95`).

2. `depth_mm > 0` equivale a "hay línea" en v1 (`world_model.cpp:61`). No hay
   `data_valid` en v1 — si DOWN está levantado, DOWN igual manda `depth_mm > 0`
   (o no, dependiendo del hardware). Con v2 habrá `data_valid` real.

3. El flag `LIFTED` de v1 (`LineStatus.flags` bit 0) **no se chequea en
   `world_model_imminent_exit()`** — solo se usa `imminent_exit_flag`
   directamente. Si el robot está levantado y DOWN manda `imminent_exit_flag=1`
   por error, CENTRAL frena igual. `strategy.cpp:19` documenta que esto está
   "filtrado en line_ring DOWN" — se confía en que DOWN lo filtra. Con v2, el
   filtro migra a `data_valid`.

**Reglas adicionales que SE DEBEN implementar al migrar a v2** (no en código
actual):

4. Validar `schema_version == 2` antes de usar cualquier dato.
5. Si `data_valid == 0` → **no usar geometría** (mismo principio que
   `ball_visible=0`). Solo leer `event_flags` (LIFTED, CALIB_SUSPECT).
6. Sentinelas `LSV2_NA_I16 = -32768` y `LSV2_NA_U16 = 0xFFFF` → N/A, no ceros.
7. Ponderar PID lateral arquero por `quality` (`0..100`).
8. Latch OR sobre `event_flags` en el drenado del buffer (EVENTO — no coalesce).

### 4.3 IMU local (BNO055 del Zircon) — ⚠️ YA NO SE CONECTA (2026-05-31)

**La CENTRAL ya NO lleva BNO.** Los 2 BNO055 están en el TOP; el heading absoluto del
robot llega por `WORLD_SNAPSHOT` de ARRIBA. El módulo `imu_zircon.cpp` queda como
**compat**: `main_central` solo llama `imu_init()` si se compila con
`-DCENTRAL_HAS_LOCAL_BNO` (default OFF), así que en el build normal no se toca el bus
I2C ni se pierden ~3 s buscando un sensor ausente. Aun con el flag, `imu_get_heading()`
no tiene consumidor en `strategy.cpp` / `world_model.cpp`. GAP-010 queda **obsoleto**.

---

## 5. Diseño de módulos del programa CENTRAL

### 5.1 Arquitectura en dos capas

```
┌─────────────────────────────────────────────────────────────────┐
│  LÓGICA PURA (testeable host-native, sin Arduino)               │
│                                                                  │
│  src/shared/kinematics.{h,cpp}  — cinemática inversa omni-3     │
│  src/shared/pids.{h,cpp}        — HeadingPID, LateralPID,       │
│                                   approach_velocity              │
│  src/shared/behind_ball.{h,cpp} — helpers posicionamiento táctico│
│                                                                  │
│  [FALTANTE: strategy_transitions.cpp — réplica de la FSM para   │
│   tests, pero NO es la FSM real (strategy.cpp). Ver GAP-011.]   │
└─────────────────────────────────────────────────────────────────┘
          ▲ consumida por
┌─────────────────────────────────────────────────────────────────┐
│  GLUE HW (HW-bound, solo en target Teensy)                      │
│                                                                  │
│  src/central/main_central.cpp   — setup/loop, watchdogs, LED    │
│  src/central/world_model.cpp    — estado del mundo (+ millis()) │
│  src/central/strategy.cpp       — FSM + PIDs (usa millis())     │
│  src/central/motors_zircon.cpp  — PWM + H-bridges + kicker GPIO │
│  src/central/comm_top.cpp       — Serial1, FrameDecoder         │
│  src/central/comm_down.cpp      — Serial2, FrameDecoder         │
│  src/central/imu_zircon.cpp     — I2C BNO055 + delay() en setup │
│  src/central/config_central.h   — pinout + constantes HW        │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Módulos y testeabilidad

| Módulo | Archivo | Puro / HW-bound | Tests existentes |
|---|---|---|---|
| Cinemática inversa | `src/shared/kinematics.cpp` | **Puro** | `test/test_kinematics/` (presunto) |
| PIDs | `src/shared/pids.cpp` | **Puro** | `test/test_pids/` (presunto) |
| Behind-ball | `src/shared/behind_ball.cpp` | **Puro** | `test/test_behind_ball/` (presunto) |
| FSM strategy | `src/central/strategy.cpp` | **HW-bound** (usa `millis()`) | **NO — hay `strategy_transitions.cpp` (réplica) con 35 tests, pero NO es el archivo real. GAP-011.** |
| World model | `src/central/world_model.cpp` | **HW-bound** (usa `millis()`) | NO |
| Motors Zircon | `src/central/motors_zircon.cpp` | **HW-bound** (Arduino GPIO) | NO |
| Comm TOP | `src/central/comm_top.cpp` | **HW-bound** (Serial1) | NO (proto.h sí testeado) |
| Comm DOWN | `src/central/comm_down.cpp` | **HW-bound** (Serial2) | NO (proto.h sí testeado) |
| IMU Zircon | `src/central/imu_zircon.cpp` | **HW-bound** (I2C + delay) | NO |

**Qué hace testeable:** separar lógica de `strategy.cpp` que solo usa
`world_model_*()` y retorna `MotorCommand` — esa parte es inyectable con
mocks de `world_model`. El uso de `millis()` es la única dependencia de HW en
la FSM; abstraerlo con una función `uint32_t get_time_ms()` inyectable haría
los tests de FSM reales (en lugar de la réplica).

### 5.3 FSM del delantero (estados y transiciones)

```
               ┌──────────────────────────────────┐
               │  match_running = false (global)  │
               ▼                                  │
         [WAIT_START] ──flanco STOP→RUN──► [KICKOFF] ──250ms──►[SEARCH]
                                                                  │
              ◄── ball visible + goal visible + NO alineado ──────┤
         [POSITION]                                               │
              │                                                   │
          reached                                                 │
          + aligned                                               │
              ▼                                                   │
         [APPROACH] ◄──── ball visible + (no goal OR alineado) ───┘
              │
          kicker_fire=1 cuando dist < 80mm + aligned < 12°
              │
       ball perdido──► [SEARCH]
       desalineado──► [POSITION]

         cualquier estado + imminent_exit + line_fresh
              ▼
         [LINE_AVOID] ──imminent_exit baja──► [SEARCH]
```

Fuente: `strategy.cpp:53-313`.

### 5.4 FSM del arquero (estados y transiciones)

```
               ┌─────────────────────────────┐
               │  match_running = false      │
               ▼                             │
         [WAIT_START] ──match_running──► [PATROL]
                                             │
                                       ball visible
                                             ▼
                                       [INTERCEPT]
                                             │
                                      dist < 250mm
                                             ▼
                                        [CLEAR]
                                             │
                                      dist > 400mm──► [INTERCEPT]
                                      ball perdido──► [PATROL]

         cualquier estado + imminent_exit + line_fresh
              ▼
         [LINE_AVOID] ──imminent_exit baja──► [PATROL]
```

Fuente: `strategy.cpp:319-441`.

### 5.5 Safety lattice (retícula de seguridad, precedencia de actuación)

```
Prioridad 1 (máxima): imminent_exit && line_is_fresh
    → motors_brake()  [corto H-bridge + kicker OFF]  (main_central.cpp:95-101)

Prioridad 2: !snapshot_is_fresh  (TOP LOST)
    → motors_stop()   [libre + kicker OFF]            (main_central.cpp:108-110)

Prioridad 3: match_running = false  (árbitro STOP)
    → FSM → WAIT_START → MotorCommand{} (ceros → motors_stop implícito)

Prioridad 4: Normal — strategy_tick() → motors_apply_command()
```

**Gap de diseño:** "Prioridad 2" dispara si DOWN cae (LOST) pero TOP sigue
fresco. No hay modo borde conservador: el robot sigue jugando sin datos de
línea → puede salir de cancha. Ver GAP-009.

---

## 6. Gaps actuales (auditoría) — qué falta para "candidato real"

| ID | Severidad | Descripción | Archivo:línea | Qué hace falta |
|---|---|---|---|---|
| GAP-001 | ~~P1~~ **RESUELTO** | `static_assert(sizeof(WorldSnapshot)==27)` — **RESUELTO 2026-05-18, commit 2a9064e**: `static_assert(sizeof==27)` agregado; WorldSnapshot bumpeado a v2 (27 B, +ball_vx/vy). | `types.h` | — |
| GAP-002 | P0 | `my_x_mm` y `my_y_mm` hardcodeados a 0 en TOP; CENTRAL los lee pero no tiene localización real | TOP (no en CENTRAL) | OTOS real en DOWN + fusión en TOP (TASK-012) |
| GAP-003 | P1 | `min_obstacle_mm` siempre alto (ToF stub en TOP); CENTRAL no evita obstáculos | TOP (no en CENTRAL) | ToF real (TASK-012) |
| GAP-004 | P1 | `config_central.h` describe el modelo "motor server" (LEGACY); confunde al lector | `config_central.h:1-14` | Reescribir el comentario del archivo para reflejar el rol master actual |
| GAP-005 | P0 | CENTRAL recibe `LineStatus` v1 (5 B); el contrato DOWN define `LineStatusV2` (16 B); `comm_down.cpp:21` usa `sizeof(LineStatus)` (5 B); cuando DOWN migre a v2 el payload será 16 B y el parser tirará todos los frames | `comm_down.cpp:21`, `world_model.cpp:9,34` | Migrar `comm_down.cpp` y `world_model.cpp` a `LineStatusV2` coordinado con DOWN |
| GAP-006 | P1 | Flag `LIFTED` de `LineStatus.flags` bit 0 no chequeado antes de usar datos de línea | `world_model.cpp:61-64` | Chequear `LINE_FLAG_LIFTED` antes de exponer `imminent_exit`, `line_angle`, etc. (se resuelve con migración a v2 via `data_valid`) |
| GAP-007 | P1 | `CENTRAL_RESET_TOP` (0x61) y `CENTRAL_TOP_CMD` (0x62) definidos en `proto.h:51-52` pero NO implementados | `proto.h:51-52` | Implementar `comm_top_send_reset()` para la escalera de recuperación (TASK-021 Nivel 2) |
| GAP-008 | P2 | `ball_confidence` no se valida en `world_model_ball_visible()` | `world_model.cpp:51` | `return g_snap.ball_visible != 0 && g_snap.ball_confidence > 0` |
| GAP-009 | P0 | Si DOWN cae (LOST, `!line_is_fresh()`), CENTRAL sigue jugando sin protección de borde → puede salir de cancha | `main_central.cpp:104-116`, diseño comms §5.1 | Implementar modo borde conservador (velocidad limitada + vector prohibido hacia afuera) cuando `!line_is_fresh()` (TASK-016) |
| ~~GAP-010~~ | — | ✅ **OBSOLETO 2026-05-31:** ya no hay BNO en CENTRAL (los 2 están en el TOP). `imu_zircon` gateado por `-DCENTRAL_HAS_LOCAL_BNO` (off); el heading viene del snapshot de ARRIBA. | — | — |
| GAP-011 | P1 | `strategy_transitions.cpp` es una réplica de la FSM usada para los 35 tests de FSM, pero no es `strategy.cpp` el que corre en el robot; divergencias posibles son bugs silenciosos | `src/shared/strategy_transitions.cpp` (test en `test/test_strategy_transitions/`) | Abstraer dependencia de `millis()` en `strategy.cpp` con función inyectable y testear `strategy.cpp` directamente |
| GAP-012 | P0 | `strategy_set_attack_color()` definida (`strategy.h:46`) pero nunca llamada al inicio — arquero propio hardcodeado (inicial: `g_attack_color = AttackColor::MAGENTA`) | `strategy.cpp:40` | Leer polaridad del árbitro y llamar `strategy_set_attack_color()` antes del partido (TASK-024) |
| GAP-013 | P0 | Rol del robot determinado por `#define ROBOT1/ROBOT2` en tiempo de compilación — no por dipswitch en runtime | `main_central.cpp:38-48`, `config_central.h:24` | Leer `PIN_ROLE_DIPSWITCH` en runtime para no requerir recompilación por rol (TASK-024) |
| GAP-014 | P1 | No hay WDT (watchdog de hardware) en CENTRAL — si el loop se cuelga, CENTRAL queda inerte sin autorecuperación | ausente | Habilitar WDT del Teensy 4.1 (TASK-021 Nivel 1) |
| GAP-015 | P1 | `comm_down_send_reset_otos()` y `comm_down_send_calib_line()` existen pero ningún módulo de CENTRAL las llama | `comm_down.cpp:46-64` | Integrar llamadas en el ciclo pre-partido o en respuesta a eventos de la FSM |

---

## 7. Ejemplos concretos: `WorldSnapshot` recibido → decisión esperada

Los siguientes ejemplos usan los campos reales de `WorldSnapshot` y trazan el
camino por el código hasta la salida en `MotorCommand`.

### Ejemplo A — Partido detenido, snap fresco

```
WorldSnapshot:
  my_heading_centideg = 0          (robot mirando al frente)
  ball_visible = 1
  ball_x_mm = 300, ball_y_mm = 400
  goal_opp_visible = 1
  goal_opp_angle_centideg = 500    (+5.00°)
  flags = 0x00                     (match_running = 0)
  referee_cmd = 0                  (stop)

Flujo CENTRAL:
  world_model_match_running() → false
  strategy_tick() → attacker_tick() → WAIT_START → MotorCommand{0,0,0,0,0}
  motors_apply_command({0,0,0,0,0})
  → motors_stop() implícito (PWM=0, libre)

Resultado: robot quieto. Correcto — fuera del partido.
```

### Ejemplo B — Partido activo, pelota visible, arco visible, no alineado

```
WorldSnapshot:
  my_heading_centideg = 0
  ball_x_mm = 200, ball_y_mm = 300  (pelota a la derecha y al frente)
  ball_visible = 1, ball_confidence = 90
  goal_opp_visible = 1
  goal_opp_angle_centideg = 2000     (+20.00°)
  flags = 0x08                       (match_running = 1)

Flujo CENTRAL:
  match_running = true
  ball_visible = true
  goal_opp_visible = true
  ball_angle_from_robot = atan2(200, 300) ≈ 33.7°
  ball_is_in_attack_line(200, 300, 20.0°, 30.0°):
      ángulo pelota ≈ 33.7°; diferencia con goal 20° ≈ 13.7° < 30° → alineado = true
  → transition_atk(APPROACH)

  En APPROACH:
    dist = sqrt(200² + 300²) ≈ 360 mm
    speed = approach_velocity(360, 50, 500, 600, 200) ≈ proporcional ≈ 380 mm/s
    vx = 200/360 × 380 ≈ 211 mm/s
    vy = 300/360 × 380 ≈ 317 mm/s
    is_aligned_to_shoot(200, 300, 20.0°, 80mm, 12°): dist=360 > 80 → false
    cmd.kicker_fire = 0

MotorCommand: {vx≈211, vy≈317, omega=PID(heading,0°), kicker_fire=0}
motors_apply_command → kinematics → PWM a 3 ruedas
```

### Ejemplo C — Inminent exit (línea bajo el robot)

```
LineStatus v1 recibida:
  angle_centideg = 9000  (+90.00° = línea a la derecha del robot)
  depth_mm = 3
  imminent_exit_flag = 1
  flags = 0x00

Flujo CENTRAL (antes del tick de strategy):
  world_model_apply_line(ls) → g_line = ls; g_line_last_ms = millis()
  world_model_imminent_exit() = true
  world_model_line_is_fresh() = true

  main_central.cpp:95:
    if (world_model_imminent_exit() && world_model_line_is_fresh())
        motors_brake()   ← freno activo (INA=INB=1, PWM=0)
        return           ← NO ejecuta strategy_tick()

  En el siguiente loop sin imminent_exit:
    strategy_tick() → LINE_AVOID
    line_angle_deg = 90.0°
    retreat = 90.0° + 180° = 270.0°  (hacia la izquierda del robot)
    vx = sin(270°) × 400 = -400 mm/s
    vy = cos(270°) × 400 = 0 mm/s
    → MotorCommand{vx=-400, vy=0, omega=0}
    → robot se mueve a la izquierda (alejándose de la línea derecha)
```

### Ejemplo D — TOP caído (snapshot stale)

```
Condición: millis() - g_snap_last_ms > 500 ms

Flujo CENTRAL:
  world_model_snapshot_is_fresh() = false
  main_central.cpp:108:
    motors_stop()  ← libre (PWM=0)
    LED parpadea cada 200 ms

Resultado: robot frenado y señalizando fallo.
```

### Ejemplo E — Arquero defendiendo (INTERCEPT con línea)

```
WorldSnapshot:
  ball_x_mm = 80, ball_y_mm = 150
  ball_visible = 1
  flags = 0x08 (match_running=1)

LineStatus v1:
  angle_centideg = -18000  (línea atrás = línea de fondo propia)
  depth_mm = 2
  imminent_exit_flag = 0

Flujo GK INTERCEPT:
  vx_intercept = 80 × 4.0 = 320 mm/s   (GK_INTERCEPT_KP_VS_BALL_X = 4.0)
  line_data_fresh() = true
  depth = 2.0f
  lateral_pid_set_target(pid, 1.0)
  vx_lateral_pid = lateral_pid_tick(pid, 2.0, now_ms)
      error = 1.0 - 2.0 = -1.0  → pid produce vx_lateral hacia adentro
  cmd.vx = 320 + vx_lateral_pid × 0.3   (mezcla intercept + PID lateral)
  dist = sqrt(80² + 150²) ≈ 170 mm > 250 mm (GK_CLEAR_TRIGGER) → sigue en INTERCEPT

MotorCommand: {vx≈intercept+pid_blend, vy=0, omega=0, kicker=0}
```

---

## 8. Catálogo de mensajes de CENTRAL (resumen)

| Dir | TYPE | Nombre | Payload | Clase | Frecuencia | Estado impl. |
|---|---|---|---|---|---|---|
| TOP → CENTRAL | `0x60` | `WORLD_SNAPSHOT` | `WorldSnapshot` (27 B, schema v2) | STREAM | 100 Hz | Implementado (`comm_top.cpp`) |
| DOWN → CENTRAL | `0x10` | `LINE_URGENT` | `LineStatus` v1 (5 B) / objetivo v2 (16 B) | EVENTO+STREAM | 200 Hz | Implementado v1 (`comm_down.cpp`) |
| CENTRAL → DOWN | `0x20` | `CENTRAL_RESET_OTOS` | `uint8` (1 B) | COMANDO | Evento | Implementado (sin llamador) |
| CENTRAL → DOWN | `0x21` | `CENTRAL_CALIB_LINE` | `uint8` (1 B, 0/1/2) | COMANDO | Evento | Implementado (sin llamador) |
| CENTRAL → TOP | `0x61` | `CENTRAL_RESET_TOP` | TBD | COMANDO | Evento | **NO implementado** |
| CENTRAL → TOP | `0x62` | `CENTRAL_TOP_CMD` | TBD | COMANDO | Evento | **NO implementado** |
| CENTRAL → Motores | — | PWM/GPIO | MotorCommand → 3×PWM + kicker | LOCAL | 100 Hz | Implementado |

---

## 9. Versionado del contrato

- `contract-schema: 2` en este documento (WorldSnapshot schema v2: +ball_vx/vy, 27 B, 2026-05-18).
- Cambios de layout de `WorldSnapshot` o de mensajes emitidos por CENTRAL
  incrementan `contract-schema` y se versionan este documento y el código.
- La migración a `LineStatusV2` (schema 2 en DOWN) requiere actualización
  coordinada de `comm_down.cpp`, `world_model.cpp`, y este documento.

---

## 10. Fuentes

- `software/teensy/Soccer 2026/src/central/main_central.cpp` (setup/loop, watchdogs)
- `software/teensy/Soccer 2026/src/central/strategy.cpp` (FSM + PIDs, líneas 1-465)
- `software/teensy/Soccer 2026/src/central/world_model.cpp` (estado del mundo, líneas 1-73)
- `software/teensy/Soccer 2026/src/central/motors_zircon.cpp` (PWM + kicker, líneas 1-170)
- `software/teensy/Soccer 2026/src/central/comm_top.cpp` (Serial1 receptor, líneas 1-48)
- `software/teensy/Soccer 2026/src/central/comm_down.cpp` (Serial2 emisor/receptor, líneas 1-71)
- `software/teensy/Soccer 2026/src/central/imu_zircon.cpp` (IMU fallback, líneas 1-89)
- `software/teensy/Soccer 2026/src/central/config_central.h` (pinout + constantes, líneas 1-112)
- `software/teensy/Soccer 2026/src/shared/proto.h` (framing, líneas 1-136)
- `software/teensy/Soccer 2026/src/shared/types.h` (structs, líneas 1-154)
- `software/teensy/Soccer 2026/src/shared/kinematics.h` (cinemática, líneas 1-48)
- `software/teensy/Soccer 2026/src/shared/pids.h` (PIDs, líneas 1-103)
- `software/teensy/Soccer 2026/src/shared/behind_ball.h` (tácticos, líneas 1-84)
- `docs/firmware/CONTRATO-DATOS-DOWN.md` (contrato LINE_URGENT v2, referenciado)
- `docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md` (arquitectura comms)
- `research/completed/2026-05-18-estado-firmware-robot-evaluacion-critica.md` (auditoría)
