---
title: "Placa CENTRAL — Especificación funcional (qué hace y cómo)"
date: 2026-05-24
status: vigente
parte-de: central-board-pack
basado-en: docs/firmware/FIRMWARE-PLACA-CENTRAL.md (curado, sin pseudo-código Nivel 3 obsoleto)
---

# Placa CENTRAL — Especificación funcional

> Para el pinout del Zircon Rev v15 / Teensy 4.1 ver
> [`01-pinout-y-hardware.md`](01-pinout-y-hardware.md).
> Para el contrato binario de las tramas ver [`03-contrato-datos.md`](03-contrato-datos.md).
> Para el diseño general de las comunicaciones de las 3 placas ver
> [`04-protocolo-comunicaciones.md`](04-protocolo-comunicaciones.md).
> Para la arquitectura general de las 3 placas ver
> [`05-arquitectura-3-placas.md`](05-arquitectura-3-placas.md).

## 1. Qué es la placa CENTRAL

**La placa CENTRAL es la que juega al fútbol.** Recibe percepción ya digerida
(`WORLD_SNAPSHOT` desde TOP @ 100 Hz + `LINE_URGENT` desde DOWN @ 200 Hz),
decide qué hacer y mueve los motores. Si DOWN reporta `imminent_exit=1`, bypassa
la estrategia y frena los motores en < 15 ms.

## 2. Tres capas funcionales

```
┌─────────────────────────────────────────────────────────┐
│  CAPA ALTA — Estrategia táctica (FSM)                    │
│  "¿Qué jugada hago?"                                      │
│  ATK_KICKOFF/SEARCH/POSITION/APPROACH/LINE_AVOID          │
│  GK_PATROL/INTERCEPT/CLEAR/LINE_AVOID                     │
└────────────────────┬────────────────────────────────────┘
                     │ produce: (target_pose | target_vector)
                     ▼
┌─────────────────────────────────────────────────────────┐
│  CAPA MEDIA — Lazos de control (PIDs)                    │
│  "¿Qué (vx, vy, omega) necesito para llegar al target?"   │
│  PID heading + PID lateral arquero + PID approach pelota  │
└────────────────────┬────────────────────────────────────┘
                     │ produce: (vx_mm_s, vy_mm_s, omega_rad_s)
                     ▼
┌─────────────────────────────────────────────────────────┐
│  CAPA BAJA — Control de motores                          │
│  Cinemática inversa omni-3 + saturación + PWM a H-bridge  │
└─────────────────────────────────────────────────────────┘
```

**Beneficio**: si se cambia la cinemática (4 ruedas en vez de 3), solo cambia la
capa BAJA. Si se mejora la estrategia, solo cambia la ALTA. Los PIDs no se
enteran.

| Capa | Frecuencia | Input | Output | Acoplamiento |
|---|---|---|---|---|
| **ALTA — Estrategia (FSM)** | 100 Hz | `WorldSnapshot` + `LineStatus` | "ir a (target_x, target_y) a V" o "empujar la pelota" | Solo lee la capa media |
| **MEDIA — PIDs** | 100 Hz | target del FSM + observación actual | `(vx, vy, omega)` deseados | Solo llama a la capa baja |
| **BAJA — Motores** | 100 Hz (o 1 kHz con encoders) | `(vx, vy, omega)` | PWM por motor | No conoce nada arriba |

## 3. Responsabilidades funcionales

| # | Responsabilidad | Capa | Frecuencia |
|---|---|---|---|
| R1 | Recibir `WORLD_SNAPSHOT` de TOP | comm | 100 Hz |
| R2 | Recibir `LINE_URGENT` de DOWN | comm | 200 Hz |
| R3 | Mantener `WorldModel` espejo del snapshot | comm | 100 Hz |
| R4 | Watchdog TOP — si > 500 ms sin snapshot, modo seguro | comm | continuo |
| R5 | FSM del rol (delantero o arquero, fijado en compile-time) | ALTA | 100 Hz |
| R6 | Decisión de jugada táctica dentro del rol | ALTA | 100 Hz |
| R7 | Coordinación con partner (Nivel 3+) | ALTA | 100 Hz |
| R8 | Construir `target_pose` o `target_vector` para los PIDs | ALTA | 100 Hz |
| R9 | PID heading (mantener orientación deseada) | MEDIA | 100 Hz |
| R10 | PID lateral arquero (cuando rol=GK + match running) | MEDIA | 100 Hz |
| R11 | PID approach a la pelota | MEDIA | 100 Hz |
| R12 | Sumar contribuciones de PIDs activos | MEDIA | 100 Hz |
| R13 | Cinemática inversa omni-3 → velocidad de cada rueda | BAJA | 100 Hz |
| R14 | Saturación proporcional | BAJA | 100 Hz |
| R15 | PID por motor con encoders (FUTURO) | BAJA | 1 kHz |
| R16 | Aplicar PWM al H-bridge | BAJA | 100 Hz |
| R17 | ~~Control de kicker~~ — eliminado (sin kicker físico; el delantero empuja por inercia) | — | — |
| R18 | **Bypass FSM al recibir `imminent_exit=1`** | EMERGENCIA | latencia < 15 ms |
| R19 | Watchdog motor: 200 ms sin MotorCommand → motores stop | hardware | continuo |
| R20 | Diagnóstico + LED + USB debug | meta | 1 Hz |

## 4. Modos de operación

| Modo | Cuándo se activa | Comportamiento |
|---|---|---|
| `BOOT` | Al encender | Inicializa motores en stop, abre UARTs, lee rol (compile-time) |
| `WAITING_REFEREE` | Default tras BOOT | Motores stop. Espera `referee_cmd = START` |
| `PLAYING` | Cuando `match_running = true` | FSM activa, PIDs activos |
| `HALFTIME` | `referee_cmd = HALFTIME` | Motores stop |
| `PENALTY` | (futuro) `referee_cmd = PENALTY` | Modo especial — solo arquero en cancha |
| `EMERGENCY_LINE` | `imminent_exit = 1` desde DOWN | **Bypass FSM, frenar motores**. Sale a PLAYING cuando depth=0 |
| `SAFE_NO_TOP` | TOP timeout > 500 ms | Motores stop, parpadeo LED. Sale solo cuando vuelve snapshot |
| `LOST` | Múltiples watchdogs disparados | Stop total + LED rojo + reset suave |

**El rol del robot** (`GOALKEEPER` o `ATTACKER`) **NO es un modo** — es una
**propiedad de compilación** (`#define ROBOT1` o `ROBOT2`). Permanece fija
durante todo el partido.

## 5. CAPA BAJA — Control de motores

### 5.1 Cinemática inversa omni-3

3 ruedas omnidireccionales a 120°. La cinemática inversa traduce
`(vx, vy, ω)` del robot a velocidad de cada rueda:

```
v_i = -vx · sin(θ_i) + vy · cos(θ_i) + ω · R
```

- `θ_i` = ángulo físico de la rueda i respecto al frente.
- `R` = distancia centro→rueda (mm).

Convención IITA (⚠️ tentativa, ver `01-pinout` §6): θ₁=+60°, θ₂=−60°, θ₃=+180°,
R=100 mm.

**Implementación**: [`firmware/shared/kinematics.{h,cpp}`](firmware/shared/kinematics.h).
**Tests**: [`tests/test_kinematics.cpp`](tests/test_kinematics.cpp) (11 tests
unitarios validando avanzar / lateral / rotación / diagonal / saturación).

### 5.2 Velocidad rueda → PWM

```cpp
int wheel_speed_to_pwm(float speed_mm_s, float max_speed, int max_pwm) {
    float pwm_f = (speed_mm_s / max_speed) * max_pwm;
    return clamp(pwm_f, -max_pwm, +max_pwm);
}
```

`MAX_SPEED_MM_S = 1000` (estimación TT @ 7.4 V, calibrable).
`MAX_PWM = 255` (rango `analogWrite` de Arduino).

### 5.3 Saturación proporcional

Si una rueda excede el máximo, **escalar las 3 por el mismo factor** (NO clipping
individual, que rompería la dirección del vector):

```cpp
void saturate_wheels(WheelSpeeds& ws, float max_speed) {
    float max_abs = max(abs(ws.v1), abs(ws.v2), abs(ws.v3));
    if (max_abs > max_speed) {
        float scale = max_speed / max_abs;
        ws.v1 *= scale; ws.v2 *= scale; ws.v3 *= scale;
    }
}
```

Preserva la dirección del movimiento a la velocidad máxima alcanzable.
Estándar en robótica omni.

### 5.4 Aplicación a hardware

```cpp
void apply_motor_pwm(int motor_idx, int pwm_signed) {
    const MotorPins& p = MOTOR_PINS[motor_idx];
    if (pwm_signed > 0) {
        digitalWrite(p.ina, HIGH); digitalWrite(p.inb, LOW);
        analogWrite(p.pwm, pwm_signed);
    } else if (pwm_signed < 0) {
        digitalWrite(p.ina, LOW); digitalWrite(p.inb, HIGH);
        analogWrite(p.pwm, -pwm_signed);
    } else {
        digitalWrite(p.ina, LOW); digitalWrite(p.inb, LOW);
        analogWrite(p.pwm, 0);  // libre (no frena activamente)
    }
}
```

Implementación: [`firmware/central/motors_zircon.{h,cpp}`](firmware/central/motors_zircon.h).

### 5.5 Stop modes

| Tipo | Cuándo | Comportamiento físico |
|---|---|---|
| `motors_stop()` | Default, entre estados, watchdog | PWM 0, INA=INB=0 (motor libre, frena por fricción) |
| `motors_brake()` | **EMERGENCY_LINE** | PWM 0, INA=INB=1 (corto en H-bridge, **freno activo**) |
| `motors_coast()` | Final de partido | Igual que stop |

### 5.6 Kicker — NO EXISTE

El robot **NO tiene kicker físico**: no hay solenoide ni MOSFET. El delantero
empuja la pelota por inercia avanzando hacia el arco rival. El pin
`PIN_KICKER_SOL` y las constantes `KICKER_*` fueron eliminados del firmware
(2026-06-03). **TASK-011 cancelada.**

## 6. CAPA MEDIA — Lazos de control (PIDs)

Implementación canónica: [`firmware/shared/pids.{h,cpp}`](firmware/shared/pids.h).
**Tests**: [`tests/test_pids.cpp`](tests/test_pids.cpp) (17 tests).

### 6.1 PID heading

Mantiene la orientación deseada del robot. Setpoint del FSM (por ej, mirar a
la pelota o mirar al arco). Salida: `omega_rad_s`.

```cpp
float pid_heading_compute(float setpoint_deg, float measured_deg, float dt) {
    float error = wrap_angle_signed(setpoint_deg - measured_deg);  // -180..+180
    // PI controller con anti-windup
    integral += error * dt;
    integral = clamp(integral, -INT_MAX, +INT_MAX);
    return Kp * error + Ki * integral;
}
```

### 6.2 PID lateral del arquero

Activo solo cuando rol = GK + match_running. **Measurement viene de
`LINE_URGENT` del DOWN** (no de pose absoluta — no la tenemos en Nivel 2).
El PID intenta mantener al arquero centrado sobre su línea de arco.

### 6.3 PID approach a la pelota

Activo en estados ATK_APPROACH y POSITION (delantero) o GK_INTERCEPT/CLEAR
(arquero). Toma `ball_x_mm`, `ball_y_mm` relativos al robot y produce
`(vx, vy)` para acercarse de forma controlada (no a velocidad full directo).

### 6.4 Combinación de PIDs

Cada estado del FSM activa un subset de PIDs y suma sus contribuciones a
`MotorCommand`:

```cpp
MotorCommand cmd = {0};
if (pid_heading_active)  cmd.omega    += pid_heading_compute(...);
if (pid_lateral_active)  cmd.vx       += pid_lateral_compute(...);
if (pid_approach_active) cmd.vx, cmd.vy += pid_approach_compute(...);
```

### 6.5 Anti-windup y reset selectivo

Al cambiar de estado del FSM, los integradores de los PIDs **se resetean
selectivamente** — los que estaban acumulando para el target viejo no deben
arrastrar bias al nuevo target. Cada PID expone `reset_integral()`.

## 7. CAPA ALTA — Estrategia táctica (FSM)

> **Estado de implementación**: el código vivo en
> [`firmware/central/strategy.cpp`](firmware/central/strategy.cpp) implementa
> **Niveles 1 + 2** con coordenadas RELATIVAS al robot (sin EKF de pose
> absoluta todavía). Caracterización pura con 35 tests en
> [`firmware/shared/strategy_transitions.{h,cpp}`](firmware/shared/strategy_transitions.h)
> + [`tests/test_strategy_transitions.cpp`](tests/test_strategy_transitions.cpp).
>
> ⚠️ Límite honesto: `strategy.cpp` todavía NO llama a
> `strategy_transitions` directamente — es una red de caracterización
> testeable, no prueba el binario línea-por-línea. Conectarlas es deuda P1
> post-Incheon.

| Nivel | Qué | Estado |
|---|---|---|
| **1** | ATK: WAIT_START/SEARCH/APPROACH. GK: WAIT_START/PATROL/INTERCEPT | ✅ implementado |
| **2** | ATK: + KICKOFF + POSITION (behind-the-ball relativo) + empuje alineado en APPROACH (sin kicker físico). GK: + CLEAR con histéresis. LINE_AVOID explícito | ✅ implementado |
| **3+** | Pose absoluta (EKF), orbit suave continuo, KICKOFF_OWN/ADV, coordinación partner, modelo del rival | ⏳ futuro |

### 7.1 FSM del DELANTERO (ATTACKER)

```
              ┌──────────────────────┐
              │  ATK_WAIT_START      │  match_running == false → motores stop
              └──────────┬───────────┘
                         │ flanco STOP→RUN
                         ▼
              ┌──────────────────────┐
              │  ATK_KICKOFF         │  boost recto al frente
              │  vy=500, 250 ms      │  (set play de saque)
              └──────────┬───────────┘
                         │ timer 250 ms vencido
                         ▼
              ┌──────────────────────┐
              │  ATK_SEARCH          │  recorre cancha, gira lento
              │  vy=200, omega=60°/s │
              └──────────┬───────────┘
                         │ ball_visible
                ┌────────┴─────────┐
   pelota NO    │                  │  pelota alineada con arco
   alineada o   ▼                  ▼  (o sin arco visible)
   sin arco  ┌─────────────┐   ┌──────────────────────┐
             │ ATK_POSITION│   │  ATK_APPROACH        │
             │ behind-the- │──▶│  va a la pelota y la │
             │ ball: target│   │  empuja por inercia  │
             │ detrás de   │◀──│  (sin kicker físico) │
             │ la pelota   │   │  (histéresis ±10°)   │
             └─────────────┘   └──────────┬───────────┘
                                          │ ball_lost / dist<1mm
                                          ▼
                                   → vuelve a SEARCH

   Transición global prioritaria (cualquier estado), orden:
     1. match_running == false       → ATK_WAIT_START
     2. imminent_exit && line_fresh  → ATK_LINE_AVOID
     3. flanco STOP→RUN              → ATK_KICKOFF
```

| Transición | Condición real |
|---|---|
| WAIT_START → KICKOFF | flanco STOP→RUN de `match_running` |
| KICKOFF → SEARCH | `now - kickoff_start ≥ 250 ms` |
| SEARCH → APPROACH | `ball_visible` y (`!goal_visible` o pelota dentro de ±30° de la línea al arco) |
| SEARCH → POSITION | `ball_visible` y `goal_visible` y pelota fuera de ±30° |
| POSITION → APPROACH | llegó al target detrás de la pelota (<80 mm) y alineada; o se perdió el arco |
| POSITION → SEARCH | `!ball_visible` |
| APPROACH → POSITION | `goal_visible` y pelota fuera de ±40° (histéresis +10°) |
| APPROACH → SEARCH | `!ball_visible` o `ball_dist < 1 mm` |
| APPROACH: alineado para empujar | `goal_visible` y alineada y `ball_dist ≤ 80 mm` y `|ángulo_arco| ≤ 12°` (sin kicker físico: solo geometría; el robot sigue empujando) |
| cualquiera → WAIT_START | `!match_running` |
| cualquiera → LINE_AVOID | `imminent_exit && line_fresh` |

El split SEARCH→POSITION/APPROACH es por **ÁNGULO** (pelota alineada con la
línea robot→arco), NO por distancia. Constantes de tuning en
`strategy.cpp` líneas ~76-83.

### 7.2 FSM del ARQUERO (GOALKEEPER)

```
              ┌──────────────────────┐
              │  GK_WAIT_START       │  match_running == false
              └──────────┬───────────┘
                         │ match_running == true
                         ▼
              ┌──────────────────────┐
              │  GK_PATROL           │  pid_lateral activo (pisa línea)
              │  oscilación lateral  │  + busca pelota
              └──────────┬───────────┘
                         │ ball_visible
                         ▼
              ┌──────────────────────┐
              │  GK_INTERCEPT        │  vx = ball_x · Kp + pid_lateral
              │  alinear con ball.x  │
              └──────────┬───────────┘
                         │ ball_dist < 250 mm (GK_CLEAR_TRIGGER)
                         ▼
              ┌──────────────────────┐
              │  GK_CLEAR            │  va DERECHO a la pelota, 500 mm/s
              │  empuja por inercia  │  (ROBOT1 NO tiene kicker físico)
              └──────────┬───────────┘
              dist>400 mm │ (histéresis GK_CLEAR_RELEASE)  o  !ball_visible
                          ▼
              → INTERCEPT (si se alejó) / PATROL (si perdió la pelota)
```

| Transición | Condición real |
|---|---|
| WAIT_START → PATROL | `match_running` |
| PATROL → INTERCEPT | `ball_visible` |
| INTERCEPT → CLEAR | `ball_dist < 250 mm` (gana sobre `!ball_visible`) |
| INTERCEPT → PATROL | `ball_dist ≥ 250 mm` y `!ball_visible` |
| CLEAR → PATROL | `!ball_visible` (gana sobre el chequeo de release) |
| CLEAR → INTERCEPT | `ball_visible` y `ball_dist > 400 mm` (histéresis) |
| cualquiera → LINE_AVOID | `imminent_exit && line_fresh`; vuelve a PATROL |

`GK_PATROL` aprovecha el PID lateral arquero (§6.2); el measurement viene de
DOWN por el bus de emergencia, respuesta lateral muy fluida. La histéresis
250/400 mm en INTERCEPT↔CLEAR evita el ping-pong de estado.

### 7.3 Behind-the-ball (técnica clave del ataque, Nivel 2)

El delantero NO debe correr derecho a la pelota — debe ubicarse del lado
**opuesto al arco rival**, para que cuando empuje, la pelota vaya al arco.

Implementado en versión RELATIVA (sin pose absoluta):

```cpp
// compute_behind_ball_target() en firmware/shared/behind_ball.cpp
target_x = ball_x_mm - gap_mm * sin(goal_angle_rad);
target_y = ball_y_mm - gap_mm * cos(goal_angle_rad);
// gap_mm = 120 mm (offset detrás de la pelota)
```

**Tests**: [`tests/test_behind_ball.cpp`](tests/test_behind_ball.cpp) (16 tests).

La versión con pose absoluta (`OPP_GOAL_X/Y`) + orbit tangencial continuo es
Nivel 3 — requiere EKF, no implementado.

## 8. Modo EMERGENCY — línea imminent_exit

Cuando DOWN reporta `imminent_exit = 1`, CENTRAL **debe frenar inmediatamente**
— antes del próximo tick de FSM.

```cpp
// handler de alta prioridad invocado desde comm_down_tick()
void enter_emergency_mode() {
    g_state = EMERGENCY_LINE;
    motors_brake();                          // freno ACTIVO (corto H-bridge)
    digitalWrite(PIN_LED_STATUS, HIGH);      // LED alerta
}

void exit_emergency_mode() {
    if (!world_model_imminent_exit() &&
         world_model_get_line_depth() == 0) {
        g_state = (world_model_match_running()) ? PLAYING : WAITING_REFEREE;
    }
}
```

**Latencia real (medida):**

| Etapa | Tiempo |
|---|---|
| DOWN detecta blanco (1 kHz sampling) | < 1 ms |
| TX UART hacia CENTRAL (5 bytes a 230400) | ~700 µs |
| CENTRAL recibe + handler | ~50 µs |
| `motors_brake()` aplica | instantáneo |
| **Total** | **~2 ms desde detección a freno activo** ✅ |

7× más rápido que el objetivo (< 15 ms).

**Recuperación**: cuando el depth vuelve a 0 por > 100 ms, el flag baja en
DOWN y CENTRAL sale de EMERGENCY_LINE al estado anterior automáticamente.

**Importante**: EMERGENCY_LINE **bypassa la FSM completamente**. No hay
decisión táctica en este modo — solo frenar. LINE_AVOID (en la FSM) es el
comportamiento de recuperación POST-brake (retrocede en dirección opuesta a
la línea).

## 9. Comunicaciones (resumen)

Detalles byte-a-byte: [`03-contrato-datos.md`](03-contrato-datos.md).
Diseño general: [`04-protocolo-comunicaciones.md`](04-protocolo-comunicaciones.md).

### 9.1 Recepción desde TOP (Serial7) — `WORLD_SNAPSHOT`

- **Frame**: `WorldSnapshot` v2 (27 bytes payload con `ball_vx/vy`).
- **Frecuencia**: 100 Hz.
- **Acción**: `world_model_apply_snapshot()` actualiza el espejo interno.

Implementación: [`firmware/central/comm_top.{h,cpp}`](firmware/central/comm_top.h).

### 9.2 Recepción desde DOWN (Serial1) — `LINE_URGENT` (bus de emergencia)

- **Frame**: `LineStatus` (5 bytes payload + 7 overhead = 12 bytes/frame).
- **Frecuencia**: 200 Hz.
- **Acción**: `world_model_apply_line()` + chequeo de `imminent_exit` con
  bypass a `enter_emergency_mode()` si está activo.

Implementación: [`firmware/central/comm_down.{h,cpp}`](firmware/central/comm_down.h).

### 9.3 Envío de comandos administrativos

CENTRAL puede enviar comandos puntuales (no streams) a las otras placas:

| Comando | Hacia | Cuándo |
|---|---|---|
| `CENTRAL_RESET_OTOS` | DOWN (Serial1) | Al inicio de cada partido (post `START`) |
| `CENTRAL_CALIB_LINE` | DOWN (Serial1) | Antes del partido, recalibrar línea |
| `CENTRAL_RESET_TOP` | TOP (Serial7) | Si pose del WorldModel se vuelve inconsistente |
| `CENTRAL_TOP_CMD` | TOP (Serial7) | Comandos genéricos (recalibrar cámaras, reset IMU) |

### 9.4 Heartbeat

**No hay heartbeat explícito** (igual que DOWN y TOP). El stream continuo de
snapshots/línea ES el heartbeat. CENTRAL detecta caída con timeouts (§10).

## 10. Detección de fallos y watchdogs

| Watchdog | Trigger | Acción |
|---|---|---|
| **TOP timeout** | 500 ms sin `WORLD_SNAPSHOT` | `motors_stop()`, modo `SAFE_NO_TOP` |
| **DOWN timeout** | 500 ms sin `LINE_URGENT` | Marca `line_fresh=false`, FSM ignora línea |
| **Motor watchdog** | 200 ms sin `MotorCommand` | Motores stop |
| Battery low (futuro) | `battery_mv < 6800` | Modo seguro, parpadeo SOS |
| Motor stall (con encoders, FUTURO) | velocidad esperada > 0 pero medida = 0 por > 200 ms | Marcar motor muerto, reducir velocidad global |
| FSM stuck | Mismo estado por > 30 s | Reset suave (vuelve a SEARCH/PATROL) |

**Watchdog de hardware**: usar el `WDT` del Teensy 4.1 configurado a 1 segundo.
Si el loop principal no llama `wdt_feed()`, el Teensy se resetea
automáticamente. Protege contra cuelgues por bugs en la FSM.

## 11. Timing y latencias

### 11.1 Loop principal

```
loop():
    comm_top_tick()              # ~50 µs (drena Serial1)
    comm_down_tick()             # ~50 µs (drena Serial1)
                                 # — bus emergencia procesado AQUÍ alta prioridad

    if since_strategy_tick >= 10 ms:
        strategy_tick()           # ~500 µs (FSM + decisión)
        update_pids()             # ~200 µs (3 PIDs)
        apply_motor_command()    # ~100 µs (kinematics + saturate + PWM)

    if HAS_ENCODERS && since_encoder_tick >= 1 ms:
        encoder_tick()            # ~150 µs (3 lecturas I²C)
        motor_pid_per_motor()     # ~300 µs (3 PIDs internos)

    if since_debug_print >= 1000 ms:
        debug_print()             # ~500 µs
```

**Loop budget**: típico ~500 µs, peor caso ~2 ms. A 100 Hz hay margen amplio
(10 ms).

### 11.2 Latencia decisión → motor

| Etapa | Tiempo |
|---|---|
| TOP detecta evento | (depende del sensor) |
| TX UART hacia CENTRAL | ~1.5 ms |
| Próximo tick del FSM (peor caso) | < 10 ms |
| Strategy decide | < 1 ms |
| PIDs + kinematics + PWM | < 0.5 ms |
| **Total TOP → motores** | **~13 ms** |

### 11.3 Latencia EMERGENCIA (más crítica, mejor)

| Etapa | Tiempo |
|---|---|
| DOWN detecta blanco (1 kHz) | < 1 ms |
| TX UART (5 bytes a 230400) | ~700 µs |
| CENTRAL handler | ~50 µs |
| `motors_brake()` aplica | instantáneo |
| **Total** | **~2 ms** ✅ (7× mejor que objetivo de 15 ms) |

## 12. Diagnóstico y debug

### 12.1 LED de estado (pin 13 = LED_BUILTIN)

| Patrón | Significado |
|---|---|
| Apagado | Firmware no inició / en `setup()` |
| Encendido fijo | NORMAL / PLAYING |
| Parpadeo lento (1 Hz) | SAFE_NO_TOP (esperando snapshot) |
| Parpadeo rápido (5 Hz) | EMERGENCY_LINE activo |
| Patrón SOS | LOST (múltiples watchdogs) |

### 12.2 USB Serial (debug humano)

A 115200 baud por el puerto USB del Teensy 4.1, el firmware imprime:

- En `setup()`: estado de cada subsistema (BNO055 OK, UARTs abiertos, rol).
- Cada 1 segundo en PLAYING: contadores (frames TX/RX TOP, frames TX/RX DOWN,
  errores CRC, estado del FSM, velocidad de cada motor, modo actual).
- En cada cambio de estado del FSM: la transición + razón.
- Si se dispara watchdog: print del evento.

Permite a Virginia/Elías conectar USB durante development y ver qué pasa sin
osciloscopio.

### 12.3 Comandos USB

Vía USB Serial el operario puede mandar texto para probar:
- `stop` / `start` — simular referee.
- `set_role gk` / `set_role atk` — forzar rol en runtime (debug).
- `dump_world` — imprime el WorldModel.
- `dump_pids` — imprime estado de cada PID.
- `stats` — contadores.

## 13. Encoders magnéticos (FUTURO, no implementado)

| Estado actual | Decisión |
|---|---|
| Sin encoders | Open-loop: PWM directo desde kinematics → H-bridge |
| Con encoders (futuro) | Closed-loop por motor: PID adicional 1 kHz que ajusta PWM según velocidad real medida |

Tipos candidatos:
- **AS5600** (I²C) — magnético, no requiere modificación mecánica grande
  (imán pegado al eje del motor).
- **Quadrature** — más estándar pero requiere modificación física.

Implementación: capa BAJA adicional, sin afectar capas MEDIA/ALTA.

## 14. Lo que NO hace (límite de scope)

La placa CENTRAL **NO** hace:

- **Visión / detección de pelota**: vive en TOP.
- **Fusión de cámaras**: vive en TOP.
- **Lectura de los 32 sensores de línea**: vive en DOWN.
- **Lectura de OTOS**: vive en DOWN.
- **Comunicación con árbitros (BLE)**: vive en TOP vía placa COMM (ESP32-C6).
- **Coordinación con partner robot vía ESP-NOW**: vive en TOP.
- **Almacenamiento en SD card**: no implementado (el Teensy 4.1 sí tiene
  slot SD, podría usarse a futuro para loguear telemetría).

CENTRAL es **el cerebro motor**: percepción digerida entra, motores se mueven.
Nada más.

## 15. Referencias dentro del pack

- Pinout completo: [`01-pinout-y-hardware.md`](01-pinout-y-hardware.md)
- Contrato byte-a-byte: [`03-contrato-datos.md`](03-contrato-datos.md)
- Protocolo general de comunicaciones: [`04-protocolo-comunicaciones.md`](04-protocolo-comunicaciones.md)
- Arquitectura general 3 placas: [`05-arquitectura-3-placas.md`](05-arquitectura-3-placas.md)
- Firmware vivo: [`firmware/central/`](firmware/central/) + [`firmware/shared/`](firmware/shared/)
- Tests host-native: [`tests/`](tests/)

## 16. Plan de trabajo factible (referencia)

| Nivel | Qué cubre | Plazo razonable |
|---|---|---|
| **Nivel 1 — Incheon MÍNIMO** | ATK/GK con SEARCH/APPROACH/PATROL/INTERCEPT | ✅ implementado |
| **Nivel 2 — Incheon IDEAL** | + KICKOFF + POSITION (behind-the-ball relativo) + CLEAR con histéresis + LINE_AVOID + empuje alineado en delantero (sin kicker físico) | ✅ implementado |
| **Nivel 3 — Roboliga Nov 2026** | EKF pose absoluta, orbit suave, KICKOFF_OWN/ADV, coordinación partner | 4–6 semanas post-Incheon |
| **Nivel 4 — Mundial 2027** | Modelo del rival, set plays avanzadas, encoders magnéticos | 4–8 semanas pre-Mundial |
| **Nivel 5 — largo plazo** | Estrategia adaptativa / ML simple sobre observaciones | 2027+ |
