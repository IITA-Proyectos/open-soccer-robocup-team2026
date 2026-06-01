---
title: "Firmware Placa CENTRAL — Especificación funcional completa"
date: 2026-05-11
status: especificación
audience: equipo IITA Soccer Open
tags: [firmware, placa-central, master, fsm, pid, cinematica, motores, estrategia, especificacion]
---

# Firmware Placa CENTRAL — Especificación funcional completa

> Documento de referencia integral del programa que corre en la placa CENTRAL
> (Teensy 4.1 sobre Zircon Rev v15). Define qué hace el **cerebro decisor**
> y ejecutor del robot: recibe percepción, decide jugada, controla motores.
>
> Es el cierre del set de tres documentos: ABAJO (sensor piso) + ARRIBA
> (cerebro sensorial) + **CENTRAL (cerebro decisor + ejecutor)**.

---

## Tabla de contenidos

1. [Resumen](#1-resumen)
2. [Hardware sobre el que corre](#2-hardware-sobre-el-que-corre)
3. [Tres capas del firmware](#3-tres-capas-del-firmware)
4. [Responsabilidades funcionales](#4-responsabilidades-funcionales)
5. [Modos de operación](#5-modos-de-operación)
6. [CAPA BAJA — Control de motores](#6-capa-baja--control-de-motores)
7. [CAPA MEDIA — Lazos de control (PIDs)](#7-capa-media--lazos-de-control-pids)
8. [CAPA ALTA — Estrategia táctica (FSM)](#8-capa-alta--estrategia-táctica-fsm)
9. [Predicción y estrategia avanzada (futuro)](#9-predicción-y-estrategia-avanzada-futuro)
10. [Lectura de encoders magnéticos (opcional)](#10-lectura-de-encoders-magnéticos-opcional)
11. [Comunicaciones](#11-comunicaciones)
12. [Modo EMERGENCIA — línea imminent_exit](#12-modo-emergencia--línea-imminent_exit)
13. [Detección de fallos y watchdogs](#13-detección-de-fallos-y-watchdogs)
14. [Timing y latencias](#14-timing-y-latencias)
15. [Estructuras de datos enviadas](#15-estructuras-de-datos-enviadas)
16. [Diagnóstico y debug](#16-diagnóstico-y-debug)
17. [Tabla resumen](#17-tabla-resumen)
18. [Plan de trabajo factible — Niveles 1 a 5](#18-plan-de-trabajo-factible)
19. [Compatibilidad con código del nacional 2025](#19-compatibilidad-con-código-del-nacional-2025)
20. [Lo que NO hace (límite de scope)](#20-lo-que-no-hace-límite-de-scope)
21. [Referencias](#21-referencias)

---

## 1. Resumen

La placa CENTRAL **es la que juega al fútbol**. Recibe percepción ya digerida (`WorldSnapshot` de ARRIBA + `LineStatus` urgente de ABAJO), decide qué hacer con esa información y mueve los motores para ejecutarlo.

**Tres capas funcionales claramente separadas**:

```
┌─────────────────────────────────────────────────────────┐
│  CAPA ALTA — Estrategia táctica (FSM)                    │
│  "¿Qué jugada hago?"                                      │
│  ATAQUE_BUSCAR / ATAQUE_PERSEGUIR / ATAQUE_PATEAR          │
│  GK_PATRULLA / GK_INTERCEPTAR / GK_DESPEJAR              │
└────────────────────┬────────────────────────────────────┘
                     │ produce: (target_pose | target_vector, kicker_fire)
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
│  "¿Qué PWM le doy a cada motor?"                         │
│  Cinemática inversa omni-3 + saturación + PID por motor   │
│  (si hay encoders)                                       │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼ PWM a los 3 H-bridges del Zircon
```

Esta separación permite **tunear y debuggear cada capa por separado**:
- Si el robot no llega al target → revisar capa media (ganancias PID).
- Si el motor no responde a PWM esperado → revisar capa baja (cableado, driver).
- Si el robot toma decisiones tontas → revisar capa alta (FSM).

**Adicionalmente**, CENTRAL atiende un **canal de emergencia** desde ABAJO (Serial2, 200 Hz). Cuando llega un flag `imminent_exit=1` el firmware **bypassa la capa alta** y frena los motores en < 15 ms — más rápido que esperar a que la FSM procese el snapshot completo.

---

## 2. Hardware sobre el que corre

| Componente | Conexión | Nota |
|-----------|----------|------|
| MCU Teensy 4.1 | — | Cortex-M7 a 600 MHz, 1 MB RAM, 8 MB flash, 7 UARTs hardware, 480 KB de PSRAM |
| Placa Zircon Rev v15 | shield del Teensy 4.1 | PCB que ganó el nacional 2025; mantenemos el cableado de motores |
| 3 H-bridges para motores omni | PWM + INA + INB por motor | Pinout según ROBOT1/ROBOT2 (ver `mapa-pines-teensy-ambos-robots.md`) |
| ~~BNO055 local del Zircon~~ | — | ⚠️ **YA NO se conecta (2026-05-31)** — los 2 BNO están en el TOP; el heading viene del snapshot de ARRIBA. `imu_zircon` queda como compat (gateado por `-DCENTRAL_HAS_LOCAL_BNO`, off). |
| Sensores de pelota IR (×8) | analógicos | Legacy del 2025 — opcional como respaldo de cámara |
| Sensores de línea (×3, legacy) | A11, A12, A13 | Legacy del 2025 — opcional, redundante con ABAJO |
| Solenoide / kicker (delantero) | GPIO + MOSFET | Si el robot delantero tiene kicker físico |
| Dribbler (delantero) | PWM | Si el robot delantero tiene dribbler |
| **Encoders magnéticos (opcional, FUTURO)** | I2C (AS5600) o quadrature | Para PID closed-loop por motor — ver §10 |
| Botones de programación | Pines 9, 10 | Iniciar / debug |
| LED de estado | LED_BUILTIN (pin 13) | Diagnóstico humano |

### UARTs

| Serial | TX | RX | Conectado a | Baud | Rol |
|--------|----|----|-------------|------|-----|
| Serial1 | 1 | 0 | desde ARRIBA | 230400 | Recibe `WORLD_SNAPSHOT` 100 Hz |
| Serial2 | 8 | 7 | desde ABAJO | 230400 | **Recibe `LINE_URGENT` 200 Hz (bus emergencia)** |
| Serial3+ | varios | varios | reservados | — | Para depuración + futuro |

El Teensy 4.1 tiene **8 UARTs hardware** (Serial1-Serial8), así que sobran. CENTRAL no necesita comm con cámaras (eso es de ARRIBA) ni con árbitros (eso es de ARRIBA vía COMM).

### Pinout de motores (compartido entre ROBOT1 y ROBOT2 con diferente mapeo)

```cpp
#if defined(ROBOT1)  // Arquero
    constexpr int PIN_INA1 = 2;  constexpr int PIN_INB1 = 5;  constexpr int PIN_PWM1 = 3;
    constexpr int PIN_INA2 = 8;  constexpr int PIN_INB2 = 7;  constexpr int PIN_PWM2 = 6;
    constexpr int PIN_INA3 = 11; constexpr int PIN_INB3 = 12; constexpr int PIN_PWM3 = 4;
#elif defined(ROBOT2)  // Delantero
    constexpr int PIN_INA1 = 8;  constexpr int PIN_INB1 = 7;  constexpr int PIN_PWM1 = 6;
    constexpr int PIN_INA2 = 11; constexpr int PIN_INB2 = 12; constexpr int PIN_PWM2 = 4;
    constexpr int PIN_INA3 = 2;  constexpr int PIN_INB3 = 5;  constexpr int PIN_PWM3 = 3;
#endif
```

Esto está documentado en `journal/2026-03-20-diferencias-pines-motores-arquero-delantero.md` — los robots usan el mismo Zircon pero los motores están físicamente cableados distinto entre arquero y delantero.

---

## 3. Tres capas del firmware

La arquitectura del firmware sigue el principio **"separar política de mecanismo"**. Cada capa solo conoce a la inmediatamente inferior:

| Capa | Frecuencia | Input | Output | Acoplamiento |
|------|------------|-------|--------|--------------|
| **ALTA** — Estrategia (FSM) | 100 Hz | `WorldSnapshot` + `LineStatus` | "ir a (target_x, target_y) a velocidad V" o "patear" | Solo lee la capa media |
| **MEDIA** — PIDs | 100 Hz | target del FSM + observación actual | `(vx, vy, omega)` deseados | Solo llama a la capa baja |
| **BAJA** — Motores | 100 Hz (o 1 kHz si hay encoders) | `(vx, vy, omega)` | PWM por motor | No conoce nada arriba |

Beneficio práctico: si se cambia la cinemática del robot (4 ruedas en vez de 3), solo cambia la capa baja. Si se mejora la estrategia, solo cambia la alta.

---

## 4. Responsabilidades funcionales

| # | Responsabilidad | Capa | Frecuencia |
|---|-----------------|------|------------|
| R1 | Recibir `WORLD_SNAPSHOT` de ARRIBA | comm | 100 Hz |
| R2 | Recibir `LINE_URGENT` de ABAJO | comm | 200 Hz |
| R3 | Mantener `WorldModel` espejo del snapshot | comm | 100 Hz |
| R4 | Watchdog ARRIBA — si > 500 ms sin snapshot, modo seguro | comm | continuo |
| R5 | FSM principal del rol (delantero o arquero) | ALTA | 100 Hz |
| R6 | Decisión de jugada táctica dentro del rol | ALTA | 100 Hz |
| R7 | Coordinación con partner (cuando llegue del snapshot) | ALTA | 100 Hz |
| R8 | Construir `target_pose` o `target_vector` para los PIDs | ALTA | 100 Hz |
| R9 | PID heading (mantener orientación deseada) | MEDIA | 100 Hz |
| R10 | PID lateral arquero (cuando role=GK + match running) | MEDIA | 100 Hz |
| R11 | PID approach a la pelota | MEDIA | 100 Hz |
| R12 | Sumar contribuciones de PIDs activos | MEDIA | 100 Hz |
| R13 | Cinemática inversa omni-3 → velocidad de cada rueda | BAJA | 100 Hz |
| R14 | Saturación proporcional (mantener dirección si excede max) | BAJA | 100 Hz |
| R15 | PID por motor con encoders (cuando hay) | BAJA | 1 kHz |
| R16 | Aplicar PWM al H-bridge | BAJA | 100 Hz / 1 kHz |
| R17 | Control de kicker y dribbler (delantero) | BAJA | evento |
| R18 | Bypass de FSM al recibir `imminent_exit=1` | EMERGENCIA | latencia < 15 ms |
| R19 | Watchdog motor: si CENTRAL se cuelga, motores se detienen solos | hardware | continuo |
| R20 | Diagnóstico + LED + USB debug | meta | 1 Hz |

---

## 5. Modos de operación

| Modo | Cuándo se activa | Comportamiento |
|------|------------------|----------------|
| `BOOT` | Al encender | Inicializar motores en stop, abrir UARTs, leer dipswitch para definir rol |
| `WAITING_REFEREE` | Default tras BOOT | Motores en stop. Espera `referee_cmd = START` en el snapshot |
| `PLAYING` | Cuando `match_running = true` | FSM activa, PIDs activos, motores en uso |
| `HALFTIME` | `referee_cmd = HALFTIME` | Motores en stop |
| `PENALTY` | (futuro) `referee_cmd = PENALTY` | Modo especial — solo el arquero en cancha |
| `EMERGENCY_LINE` | `imminent_exit = 1` desde ABAJO | Bypass FSM, frenar motores. Sale a `PLAYING` cuando depth baja |
| `SAFE_NO_TOP` | ARRIBA timeout > 500 ms | Frenar motores, parpadear LED. Sale solo cuando vuelve snapshot |
| `LOST` | Múltiples watchdogs disparados | Stop total + LED rojo + reset suave (esperar 1 s y reintentar) |

**El rol del robot (`GOALKEEPER` o `ATTACKER`) NO es un modo** — es una **propiedad** seteada al boot por dipswitch (Q7 del coach). Permanece fija durante todo el partido. Los modos tácticos dentro del rol están en la FSM (§8).

---

## 6. CAPA BAJA — Control de motores

### 6.1 Cinemática inversa omni-3

El robot tiene **3 ruedas omnidireccionales** distribuidas a 120° entre sí (configuración estándar). La cinemática inversa traduce velocidad del robot `(vx, vy, ω)` a velocidad de cada rueda `(v1, v2, v3)`:

```
v_i = -vx · sin(θ_i) + vy · cos(θ_i) + ω · R
```

Donde:
- `θ_i` = ángulo físico de la rueda i respecto al frente del robot (en radianes).
- `R` = distancia del centro del robot al centro de cada rueda (en mm).
- `v_i` = velocidad lineal deseada en la circunferencia de la rueda i (en mm/s).

**Convención IITA** (a confirmar con montaje físico real):
- `θ_1 = 60°` → rueda derecha-frente.
- `θ_2 = -60°` → rueda izquierda-frente.
- `θ_3 = 180°` → rueda atrás centro.
- `R = 100 mm` (radio del robot, tentativo).

Implementación en `src/shared/kinematics.h` (ya escrita, con 11 tests unitarios validando avanzar/lateral/rotación/diagonal/saturación).

### 6.2 Conversión velocidad rueda → PWM

Cada motor TT con su H-bridge acepta PWM 0-255 (signed = dirección):

```cpp
int wheel_speed_to_pwm(float speed_mm_s, float max_speed_mm_s, int max_pwm) {
    float pwm_f = (speed_mm_s / max_speed_mm_s) * max_pwm;
    return clamp(pwm_f, -max_pwm, +max_pwm);
}
```

`max_speed_mm_s` es la velocidad lineal máxima del robot (estimada en ~1000 mm/s para motores TT con batería 7.4V). Calibrable.

### 6.3 Saturación proporcional

Si alguna rueda excede el máximo, **NO** clipping individual (rompería la dirección del vector). En su lugar, **escalar las 3 ruedas** por el mismo factor:

```cpp
void saturate_wheels(WheelSpeeds& ws, float max_speed_mm_s) {
    float max_abs = max(abs(ws.v1), abs(ws.v2), abs(ws.v3));
    if (max_abs > max_speed_mm_s) {
        float scale = max_speed_mm_s / max_abs;
        ws.v1 *= scale; ws.v2 *= scale; ws.v3 *= scale;
    }
}
```

Esto preserva la **dirección** del movimiento pero a velocidad máxima alcanzable. Standard en robótica omni.

### 6.4 Aplicación a hardware

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

### 6.5 Stop modes (importante para seguridad)

| Tipo de stop | Cuándo se usa | Comportamiento físico |
|--------------|---------------|----------------------|
| `motors_stop()` | Default — entre estados, watchdog | PWM 0, INA=INB=0 (motor libre, frena por fricción) |
| `motors_brake()` | EMERGENCIA (línea inminente) | PWM 0, INA=INB=1 (corto en H-bridge, freno activo) |
| `motors_coast()` | Final de partido | Igual que stop |

`motors_brake()` para de inmediato pero estresa los drivers. Solo usarlo en emergencia real.

### 6.6 Kicker y dribbler (delantero)

```cpp
void kicker_fire() {
    // Solenoide controlado por MOSFET. Pulso corto (50-100 ms) carga
    // y dispara. Después delay de recarga (capacitor, ~1 s).
    digitalWrite(PIN_KICKER, HIGH);
    delay(50);  // solenoide energizado 50 ms
    digitalWrite(PIN_KICKER, LOW);
    g_kicker_recharge_until_ms = millis() + 1000;
}

bool kicker_ready() {
    return millis() > g_kicker_recharge_until_ms;
}

void dribbler_set_speed(uint8_t pwm) {
    analogWrite(PIN_DRIBBLER, pwm);
}
```

El kicker solo se activa cuando la FSM detecta que la pelota está alineada con el arco rival y a distancia óptima (calibrable, ~50-80 mm frontal).

---

## 7. CAPA MEDIA — Lazos de control (PIDs)

CENTRAL corre **todos los PIDs del robot** en un único lugar (decisión documentada en `docs/ARQUITECTURA-3-PLACAS-2026.md` §Decisiones de diseño justificadas). Hay 3 PIDs activos según contexto:

### 7.1 PID heading

Mantiene la orientación del robot en un setpoint deseado. La observación es `world_model_get_my_heading_deg()` (del IMU dual de ARRIBA, ya fusionado).

```cpp
struct HeadingPID {
    float kp = 3.0f, ki = 0.05f, kd = 0.5f;
    float setpoint_deg;
    float integral, prev_error;
    uint32_t last_tick_ms;
};

float heading_pid_tick(HeadingPID& pid, float current_heading_deg) {
    float error = wrap_diff(pid.setpoint_deg, current_heading_deg);  // ±180
    float dt = (millis() - pid.last_tick_ms) / 1000.0f;
    pid.last_tick_ms = millis();

    pid.integral += error * dt;
    pid.integral = clamp(pid.integral, -50.0f, 50.0f);

    float derivative = (error - pid.prev_error) / dt;
    pid.prev_error = error;

    return pid.kp * error + pid.ki * pid.integral + pid.kd * derivative;
}
```

Output: `omega_rad_s` que se suma al comando de movimiento.

**Ganancias iniciales** (sacadas del firmware `test-4-movimientos.ino` del staging del equipo IITA — están validadas):
- Kp = 3.0, Ki = 0.05, Kd = 0.5 para movimiento lateral.
- Kp = 3.0, Ki = 0.08, Kd = 0.8 para movimiento adelante/atrás.

El firmware nuevo puede usar dos juegos de ganancias y cambiar según el modo del movimiento.

### 7.2 PID lateral del arquero

Mantiene al robot pisando la línea de fondo a una profundidad deseada (típicamente "1-2 sensores en blanco" = pisar el borde sin cruzarlo).

**Observación**: `world_model_get_line_depth()` (cuántos sensores en blanco) recibido vía bus de emergencia desde ABAJO a 200 Hz.

**Setpoint**: `depth_target = 1` o `depth_target = 2` según calibración.

```cpp
float lateral_pid_tick_arquero(int current_depth) {
    static float integral = 0.0f, prev_error = 0.0f;
    static uint32_t last_ms = millis();

    if (!world_model_line_is_fresh()) return 0.0f;  // sin dato, no aplicar

    float error = depth_target - current_depth;  // positivo = retroceder
    float dt = (millis() - last_ms) / 1000.0f;
    last_ms = millis();

    integral += error * dt;
    integral = clamp(integral, -20.0f, 20.0f);

    float derivative = (error - prev_error) / dt;
    prev_error = error;

    // Output como velocidad lateral (mm/s). Positivo = alejarse de la línea (Y+).
    return 100.0f * error + 10.0f * integral + 30.0f * derivative;
}
```

**Solo activo cuando role=GK + match_running**. En otros casos su output se descarta. Se reactiva sin reset cuando vuelven las condiciones.

### 7.3 PID approach a la pelota

Cuando el robot ve la pelota a distancia, ajusta velocidad de avance proporcionalmente:

```cpp
float approach_pid_tick(float ball_distance_mm) {
    // Estrategia simple: velocidad proporcional a la distancia, saturada.
    if (ball_distance_mm < 50.0f) return 0.0f;        // ya estoy encima
    if (ball_distance_mm > 500.0f) return 600.0f;     // lejos, máxima velocidad
    return ball_distance_mm * 1.5f;                    // proporcional intermedio
}
```

Esto da un perfil de aproximación suave: rápido cuando estoy lejos, lento cerca para no patear sin alinear.

### 7.4 Combinación de PIDs

Los PIDs no se aplican aisladamente — se **suman**:

```cpp
MotorCommand build_motor_command(const WorldModel& wm, RobotRole role) {
    MotorCommand cmd{};

    // PID heading siempre activo
    cmd.omega_centideg_s = (int16_t)(heading_pid_tick(g_heading_pid,
                                                       wm.my_heading_deg) * 100.0f);

    // PID lateral SOLO en modo arquero + match running
    if (role == GOALKEEPER && wm.match_running) {
        float vx_lateral = lateral_pid_tick_arquero(wm.line_depth);
        cmd.vx_mm_s += (int16_t)vx_lateral;
    }

    // PID approach SOLO cuando atacante y ve pelota
    if (role == ATTACKER && wm.ball_visible) {
        float approach_speed = approach_pid_tick(ball_distance);
        // Dirección hacia la pelota
        cmd.vx_mm_s += (int16_t)(approach_speed * cos(ball_angle_rad));
        cmd.vy_mm_s += (int16_t)(approach_speed * sin(ball_angle_rad));
    }

    return cmd;
}
```

### 7.5 Anti-windup y reset selectivo

Cuando el robot entra en un estado nuevo (ej. de SEARCH a APPROACH), el integral acumulado del PID anterior puede ser irrelevante o incluso contraproducente. **Resetear el integral en cada transición de estado importante**.

```cpp
void strategy_transition_to(State new_state) {
    g_state = new_state;
    g_heading_pid.integral = 0.0f;
    // Otros PIDs también si aplica.
}
```

---

## 8. CAPA ALTA — Estrategia táctica (FSM)

> ### ⚙️ Estado de implementación (actualizado 2026-05-15, commit `b10c66d` + `c7affd5`)
>
> Esta sección originalmente describía un diseño *objetivo* con pose absoluta
> (Nivel 3). El **código real** en `src/central/strategy.cpp` implementa los
> **Niveles 1 + 2** con coordenadas RELATIVAS al robot (sin EKF de pose
> absoluta todavía). Las §8.1 y §8.2 abajo describen **lo que el código hace
> hoy**. El diseño con pose absoluta (orbit suave, OPP_GOAL_X/Y) queda como
> Nivel 3 — ver §8.4/8.5 con su nota.
>
> | Nivel | Qué | Estado |
> |-------|-----|--------|
> | **1** | ATK: WAIT_START/SEARCH/APPROACH. GK: WAIT_START/PATROL/INTERCEPT. | ✅ implementado |
> | **2** | ATK: + KICKOFF (set play inicial) + POSITION (behind-the-ball relativo) + kicker en APPROACH. GK: + CLEAR con histéresis. LINE_AVOID como estado explícito. | ✅ implementado |
> | **3+** | Pose absoluta (EKF), orbit suave continuo, set plays KICKOFF_OWN/ADV, coordinación partner, modelo del rival. | ⏳ futuro |
>
> **Caracterización testeable**: el árbol de transiciones está replicado fiel
> en `src/shared/strategy_transitions.{h,cpp}` con **35 tests host-native**
> (`test/test_strategy_transitions/`). ⚠️ Límite honesto: `strategy.cpp`
> todavía NO llama a ese módulo — es una red de caracterización, no prueba el
> binario línea-por-línea. Conectarlo es tema-a-analizar P1 (ver
> `journal/2026-05-15-analisis-firmware-y-fsm-testeable.md`).

### 8.1 FSM del DELANTERO (ATTACKER) — Nivel 1+2 implementado

```
              ┌──────────────────────┐
              │  ATK_WAIT_START      │  match_running == false → motores stop
              └──────────┬───────────┘
                         │ flanco STOP→RUN (match arranca)
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
             │ behind-the- │──▶│  va a la pelota +    │
             │ ball: target│   │  si alineado+cerca:  │
             │ detrás de   │◀──│  cmd.kicker_fire=1   │
             │ la pelota   │   │  (histéresis ±10°)   │
             └─────────────┘   └──────────┬───────────┘
                                          │ ball_lost / dist<1mm
                                          ▼
                                   → vuelve a SEARCH

   Transición global prioritaria (cualquier estado), en este orden:
     1. match_running == false              → ATK_WAIT_START
     2. imminent_exit && line_fresh         → ATK_LINE_AVOID
     3. flanco STOP→RUN                     → ATK_KICKOFF
   ATK_LINE_AVOID: retrocede opuesto a la línea; vuelve a SEARCH cuando
   !imminent_exit. (El brake activo lo hace main_central.cpp ANTES de la FSM
   — ver §12; LINE_AVOID es el comportamiento de recuperación post-brake.)
   snapshot stale > 500 ms → motores stop + SAFE_NO_TOP (fuera de la FSM).
```

**Transiciones reales** (en `strategy.cpp`, espejadas en
`strategy_transitions.cpp`). Clave: el split SEARCH→POSITION/APPROACH es por
**ÁNGULO** (pelota alineada con la línea robot→arco), NO por distancia:

| Transición | Condición real |
|------------|----------------|
| WAIT_START → KICKOFF | flanco STOP→RUN de `match_running` |
| KICKOFF → SEARCH | `now - kickoff_start ≥ 250 ms` |
| SEARCH → APPROACH | `ball_visible` y (`!goal_visible` **o** pelota dentro de ±30° de la línea al arco) |
| SEARCH → POSITION | `ball_visible` y `goal_visible` y pelota fuera de ±30° |
| POSITION → APPROACH | llegó al target detrás de la pelota (`<80 mm`) **y** alineada; o se perdió el arco |
| POSITION → SEARCH | `!ball_visible` |
| APPROACH → POSITION | `goal_visible` y pelota fuera de ±40° (histéresis +10°) |
| APPROACH → SEARCH | `!ball_visible` o `ball_dist < 1 mm` |
| APPROACH: `cmd.kicker_fire=1` | `goal_visible` y alineada y `ball_dist ≤ 80 mm` y `|ángulo_arco| ≤ 12°` |
| cualquiera → WAIT_START | `!match_running` |
| cualquiera → LINE_AVOID | `imminent_exit && line_fresh` |

> **Diferencia vs diseño original**: no hay estado `PUSH_KICK` separado — el
> disparo del kicker es una salida dentro de `APPROACH`. El split a `POSITION`
> es por ángulo (`ball_is_in_attack_line`), no por la distancia `<250 mm` del
> diseño viejo. Constantes de tuning en `strategy.cpp` líneas ~76-83.

### 8.2 FSM del ARQUERO (GOALKEEPER) — Nivel 1+2 implementado

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
              │  empuja por inercia  │  (ROBOT1 no tiene kicker físico)
              └──────────┬───────────┘
              dist>400 mm │ (histéresis GK_CLEAR_RELEASE)  o  !ball_visible
                          ▼
              → INTERCEPT (si se alejó) / PATROL (si perdió la pelota)

   Transición global prioritaria (igual que ATK, sin KICKOFF):
     1. match_running == false        → GK_WAIT_START
     2. imminent_exit && line_fresh   → GK_LINE_AVOID  (→ PATROL al despejarse)
```

**Transiciones reales** (`strategy.cpp`, espejadas en `strategy_transitions.cpp`):

| Transición | Condición real |
|------------|----------------|
| WAIT_START → PATROL | `match_running` |
| PATROL → INTERCEPT | `ball_visible` |
| INTERCEPT → CLEAR | `ball_dist < 250 mm` (gana sobre `!ball_visible`) |
| INTERCEPT → PATROL | `ball_dist ≥ 250 mm` y `!ball_visible` |
| CLEAR → PATROL | `!ball_visible` (gana sobre el chequeo de release) |
| CLEAR → INTERCEPT | `ball_visible` y `ball_dist > 400 mm` (histéresis) |
| cualquiera → LINE_AVOID | `imminent_exit && line_fresh`; vuelve a PATROL |

**Nota**: `GK_PATROL` aprovecha el PID lateral arquero (§7.2); el measurement
viene de ABAJO por el bus de emergencia, respuesta lateral muy fluida. La
histéresis 250/400 mm en INTERCEPT↔CLEAR evita el ping-pong de estado cuando
la pelota queda al borde del umbral.

### 8.3 Comportamientos adicionales (Nivel 3+, NO implementados)

> Nota: el `ATK_KICKOFF` simple (boost recto al detectar el flanco STOP→RUN)
> **YA está implementado** en Nivel 2 — ver §8.1. Lo de abajo es la versión
> avanzada con pose absoluta y coordinación, todavía futura.

| Estado adicional | Cuándo | Comportamiento |
|------------------|--------|----------------|
| `ATK_DEFEND` | Si partner es GK y nuestro arco está siendo atacado | Volver a defender, complementar al partner |
| `ATK_SUPPORT_KICK` | Si partner está pateando | Acompañar para rebote |
| `GK_RUSH` | Pelota se acerca rápido al arco | Salir del arco a interceptar |
| `KICKOFF_OWN` | Tras gol en contra o inicio | Posición fija + patear (requiere pose absoluta) |
| `KICKOFF_ADV` | Tras gol a favor o inicio adversario | Posición defensiva (requiere pose absoluta) |

### 8.4 Behind-the-ball (técnica clave de ataque)

> **⚠️ Implementado en versión RELATIVA (Nivel 2), no la de abajo.** El código
> real es `compute_behind_ball_target()` en `src/shared/behind_ball.cpp`
> (16 tests en `test/test_behind_ball/`). Usa coords **relativas al robot** +
> ángulo al arco (`goal_angle_deg`), porque NO hay pose absoluta todavía:
>
> ```cpp
> // behind_ball.cpp (real, Nivel 2): target relativo, gap 120 mm
> target_x = ball_x_mm - gap_mm * sin(goal_angle_rad);
> target_y = ball_y_mm - gap_mm * cos(goal_angle_rad);
> ```
>
> El pseudocódigo de abajo (con `OPP_GOAL_X/Y`, `my_x/y_mm`) es la versión
> **Nivel 3** para cuando exista el EKF de pose absoluta. Conservado como
> referencia de destino, no es lo que corre.

El robot delantero NO debe simplemente correr hacia la pelota — debe **ubicarse del lado opuesto al arco rival**, para que cuando empuje la pelota, vaya hacia el arco rival.

```cpp
void compute_behind_ball_target(WorldModel& wm, float& target_x, float& target_y) {
    float ball_x = wm.ball_x_mm;
    float ball_y = wm.ball_y_mm;
    float goal_x = OPP_GOAL_X;     // 910 (centro del arco rival)
    float goal_y = OPP_GOAL_Y;     // 2430 (fondo del arco rival)

    // Vector unitario desde la pelota hacia el arco
    float dx = goal_x - ball_x;
    float dy = goal_y - ball_y;
    float d = sqrt(dx*dx + dy*dy);
    if (d < 1) d = 1;
    dx /= d; dy /= d;

    // Target = pelota - 130mm en dirección opuesta al arco
    target_x = ball_x - dx * 130.0f;
    target_y = ball_y - dy * 130.0f;
}
```

Si el robot está **del lado equivocado** (entre la pelota y el arco rival), debe **orbitar** la pelota sin tocarla.

### 8.5 Orbit (rodear la pelota sin tocarla)

> **⚠️ NO implementado como orbit suave continuo.** En Nivel 2, el estado
> `POSITION` persigue el *target detrás de la pelota* recalculado cada tick
> (decisión documentada en `behind_ball.h`): más simple y robusto que el orbit
> tangencial de abajo, y no necesita pose absoluta. El `orbit_ball()` con
> `my_x/y_mm` es Nivel 3 (requiere EKF). Conservado como referencia de destino.

```cpp
void orbit_ball(WorldModel& wm, bool clockwise, MotorCommand& cmd) {
    float bx = wm.ball_x_mm, by = wm.ball_y_mm;
    float mx = wm.my_x_mm,   my = wm.my_y_mm;

    // Ángulo del robot al respecto de la pelota
    float angle_to_ball = atan2(by - my, bx - mx);
    // Tangente al círculo de orbit (perpendicular al radio)
    float tangent = angle_to_ball + (clockwise ? -90.0f : 90.0f) * (M_PI / 180.0f);

    float orbit_speed = 300.0f;  // mm/s
    cmd.vx_mm_s = orbit_speed * cos(tangent);
    cmd.vy_mm_s = orbit_speed * sin(tangent);
    // Heading mantiene mirando a la pelota
    g_heading_pid.setpoint_deg = angle_to_ball * (180.0f / M_PI);
}
```

---

## 9. Predicción y estrategia avanzada (futuro)

### 9.1 Anticipación de la pelota

El `WorldSnapshot` (Nivel 3+) puede incluir velocidad de la pelota. Con esa info, el robot puede **anticipar**:

```cpp
// Predecir dónde estará la pelota en T_anticipate segundos
float predicted_ball_x = wm.ball_x + wm.ball_vx * T_anticipate;
float predicted_ball_y = wm.ball_y + wm.ball_vy * T_anticipate;

// Target = posición predicha de la pelota
```

`T_anticipate` típico: 200-500 ms (cuánto tiempo va a tardar el robot en llegar).

### 9.2 Coordinación con partner

Cuando ambos robots ven la pelota, decidir **quién va a por ella**:

```cpp
// Distancia de cada robot a la pelota
float my_dist = distance(wm.my_x, wm.my_y, wm.ball_x, wm.ball_y);
float partner_dist = distance(wm.partner_x, wm.partner_y, wm.ball_x, wm.ball_y);

if (my_dist < partner_dist - 100) {
    // Yo voy. Partner se queda cubriendo.
} else if (partner_dist < my_dist - 100) {
    // Partner va. Yo cubro o me posiciono para rebote.
} else {
    // Empate aproximado — el ATTACKER va, GK se queda.
    if (role == ATTACKER) go_for_ball();
}
```

### 9.3 Modelo del rival (Nivel 4+)

Si tenemos historial de cómo se mueve el rival típico:
- ¿Sale del arco? Aprovechar para tirar centrado.
- ¿Es agresivo persiguiendo? Hacer pase rápido al partner.
- ¿Defiende mucho? Buscar ángulos laterales.

Esto es **machine learning** simple sobre observaciones de los primeros minutos. Razonable para Mundial 2027.

### 9.4 Set plays (jugadas ensayadas)

Para kickoff, lateral, esquina, hay patrones predefinidos:

```cpp
enum SetPlay { KICKOFF_OWN, KICKOFF_ADV, GOAL_KICK, LATERAL_LEFT, LATERAL_RIGHT };

void execute_set_play(SetPlay play, RobotRole role, MotorCommand& cmd) {
    // Tabla de comportamientos predefinidos por (play, role)
    // — ataque A: avanzar y disparar
    // — defensa B: posición frente al arco
    // — etc.
}
```

5 jugadas ensayadas según `docs/internal/.../game-strategy-playbook.md` (kickoff IITA con análisis detallado).

---

## 10. Lectura de encoders magnéticos (opcional)

### 10.1 Estado actual

Los motores TT del nacional 2025 **NO tienen encoders**. El control es open-loop: PWM aplicado → motor gira (idealmente) a velocidad proporcional. La variabilidad entre motores hace que el robot tienda a desviarse — compensado por el PID de heading que detecta el desvío.

**Limitación L2 del documento `limitaciones-robot-marzo-2026.md`**: sin encoders, no hay control closed-loop de velocidad por motor. La trayectoria es imprecisa.

### 10.2 Si se agregan encoders (HW-005 del roadmap)

**Opciones de hardware**:
- **AS5600 magnéticos** (I2C, $3 c/u): sensor magnético que detecta el ángulo de un imán pegado al eje del motor. Resolución 12-bit (4096 cuentas por revolución).
- **Encoders de cuadratura ópticos**: 2 pines digitales con interrupts. Más resolución pero requieren más pines.

**Recomendación**: AS5600 magnéticos. Pros: pocos pines (I2C compartido), pequeños, precios bajos. Contras: requiere imanes adheridos al eje (mecánica adicional).

### 10.3 Firmware con encoders

```cpp
struct EncoderState {
    uint16_t raw_angle;        // 0-4095 del AS5600
    int32_t total_counts;       // acumulado desde boot
    float velocity_rad_s;
    uint32_t last_read_ms;
};

EncoderState encoders[3];

void encoder_tick() {
    static uint32_t last_tick_ms = 0;
    uint32_t now = millis();
    float dt = (now - last_tick_ms) / 1000.0f;
    last_tick_ms = now;

    for (int m = 0; m < 3; m++) {
        uint16_t prev = encoders[m].raw_angle;
        uint16_t curr = as5600_read_angle(m);

        int16_t diff = curr - prev;
        // Wrap-around 4095 ↔ 0
        if (diff > 2048) diff -= 4096;
        else if (diff < -2048) diff += 4096;

        encoders[m].total_counts += diff;
        encoders[m].velocity_rad_s = diff * (2*M_PI / 4096.0f) / dt;
        encoders[m].raw_angle = curr;
    }
}
```

### 10.4 PID por motor (closed-loop)

Con encoders, cada motor tiene su PID propio que ajusta el PWM para alcanzar la velocidad deseada:

```cpp
struct MotorPID {
    float kp = 0.5f, ki = 0.1f, kd = 0.05f;
    float setpoint_rad_s;
    float integral, prev_error;
};

int motor_pid_tick(MotorPID& pid, float actual_rad_s) {
    float error = pid.setpoint_rad_s - actual_rad_s;
    pid.integral = clamp(pid.integral + error * 0.001f, -100.0f, 100.0f);  // 1 ms tick
    float derivative = (error - pid.prev_error) / 0.001f;
    pid.prev_error = error;

    int pwm_output = (int)(255 * (pid.kp * error + pid.ki * pid.integral + pid.kd * derivative));
    return clamp(pwm_output, -255, +255);
}
```

**Frecuencia**: el PID por motor corre a **1 kHz** (mucho más rápido que los PIDs de capa media), porque la dinámica del motor es rápida y conviene corregir desviaciones al instante.

### 10.5 Modo sin encoders (fallback)

Si no hay encoders (caso actual del nacional 2025):
- `wheel_speed_to_pwm()` mapea velocidad deseada → PWM directo (open-loop).
- No hay corrección de slip de rueda (lo detecta ABAJO via OTOS).
- El PID de heading (capa media) corrige el desvío global.

El firmware debe compilar con o sin encoders mediante `#define HAS_ENCODERS` o flag de configuración.

---

## 11. Comunicaciones

### 11.1 Recepción desde ARRIBA (Serial1)

**Frame esperado**: `WORLD_SNAPSHOT` (24 bytes payload + 7 overhead = 31 bytes/frame). Frecuencia: 100 Hz.

```cpp
void comm_top_tick() {
    while (Serial1.available()) {
        uint8_t b = Serial1.read();
        if (g_decoder_top.feed(b)) {
            const Frame& f = g_decoder_top.get_frame();
            if (f.type == MsgType::WORLD_SNAPSHOT) {
                WorldSnapshot snap;
                memcpy(&snap, f.payload, sizeof(snap));
                world_model_apply_snapshot(snap);
            }
        }
    }
}
```

### 11.2 Recepción desde ABAJO (Serial2) — bus de emergencia

**Frame esperado**: `LINE_URGENT` (5 bytes payload + 7 overhead = 12 bytes/frame). Frecuencia: 200 Hz.

```cpp
void comm_down_tick() {
    while (Serial2.available()) {
        uint8_t b = Serial2.read();
        if (g_decoder_down.feed(b)) {
            const Frame& f = g_decoder_down.get_frame();
            if (f.type == MsgType::LINE_URGENT) {
                LineStatus ls;
                memcpy(&ls, f.payload, sizeof(ls));
                world_model_apply_line(ls);

                // Procesamiento URGENTE: si imminent_exit, frenar inmediato
                if (ls.imminent_exit_flag) {
                    enter_emergency_mode();
                }
            }
        }
    }
}
```

### 11.3 Envío de comandos administrativos

CENTRAL puede enviar comandos a ARRIBA y ABAJO (eventos puntuales, no streams):

| Comando | Hacia | Cuándo |
|---------|-------|--------|
| `CENTRAL_RESET_OTOS` | ABAJO (Serial2) | Al inicio de cada partido (post `START`) |
| `CENTRAL_CALIB_LINE` | ABAJO (Serial2) | Antes del partido, recalibrar línea según iluminación |
| `CENTRAL_RESET_TOP` | ARRIBA (Serial1) | Si la pose del world model se vuelve inconsistente |
| `CENTRAL_TOP_CMD` | ARRIBA (Serial1) | Comandos genéricos (recalibrar cámaras, reset IMU, etc.) |

### 11.4 Heartbeat

**Igual que en las otras placas: NO hay heartbeat explícito**. El stream continuo de snapshots/línea ES el heartbeat. CENTRAL detecta caída con timeouts (§13).

---

## 12. Modo EMERGENCIA — línea imminent_exit

Cuando ABAJO reporta `imminent_exit=1` (≥ N sensores en blanco simultáneo), CENTRAL **debe frenar inmediatamente** — antes de procesar el siguiente snapshot, antes incluso de ejecutar la FSM normalmente.

**Implementación: handler de alta prioridad en el RX**:

```cpp
void enter_emergency_mode() {
    g_state = EMERGENCY_LINE;
    motors_brake();  // freno activo, no solo PWM=0
    digitalWrite(PIN_LED_STATUS, HIGH);  // LED prendido fijo (alerta)
}

void exit_emergency_mode() {
    // Sale solo cuando el flag está bajo Y la profundidad volvió a 0
    if (!world_model_imminent_exit() && world_model_get_line_depth() == 0) {
        g_state = (world_model_match_running()) ? PLAYING : WAITING_REFEREE;
    }
}
```

**Tiempos**:
- DOWN detecta blanco → flag al CENTRAL: ~5 ms (UART a 200 Hz).
- CENTRAL recibe + handler: ~1 ms.
- `motors_brake()` aplica: instantáneo.
- **Total: ~6-7 ms** desde detección → freno activo.

Esto es **mejor que los 15 ms objetivo** del documento de arquitectura.

**Recuperación**:
- ABAJO sigue muestreando a 1 kHz incluso en modo emergencia.
- Cuando el robot se aleja de la línea (depth = 0 durante > 100 ms), el flag baja.
- CENTRAL vuelve al estado anterior automáticamente.

**Importante**: el modo EMERGENCY_LINE **bypassa la FSM** — no hay "decisión táctica" en este modo. Solo frenar. La FSM se reactiva al salir.

---

## 13. Detección de fallos y watchdogs

| Watchdog | Trigger | Acción |
|----------|---------|--------|
| ARRIBA timeout | 500 ms sin `WORLD_SNAPSHOT` | `motors_stop()`, modo `SAFE_NO_TOP` |
| ABAJO timeout | 500 ms sin `LINE_URGENT` | Marca `line_fresh=false`, FSM ignora línea |
| Battery low (futuro) | `battery_mv < 6800` | Modo seguro, parpadeo SOS |
| Motor stall (con encoders) | velocidad esperada > 0 pero medida = 0 por > 200 ms | Marcar motor muerto, reducir velocidad global |
| FSM stuck | Mismo estado por > 30 s | Reset suave (vuelve a SEARCH/PATROL) |

**Watchdog de hardware (Teensy)**: usar `WDT` (Watchdog Timer) configurado a 1 segundo. Si el loop principal no llama `wdt_feed()`, el Teensy se resetea automáticamente. Protege contra cuelgues por bugs en la FSM.

---

## 14. Timing y latencias

### 14.1 Loop principal

```
loop():
    comm_top_tick()              # ~50 µs (drena Serial1)
    comm_down_tick()             # ~50 µs (drena Serial2)
                                 # — bus emergencia procesado AQUÍ con alta prioridad

    if since_strategy_tick >= 10 ms:
        strategy_tick()           # ~500 µs (FSM + decisión)
        update_pids()             # ~200 µs (3 PIDs)
        apply_motor_command()    # ~100 µs (kinematics + saturate + PWM)

    if HAS_ENCODERS && since_encoder_tick >= 1 ms:
        encoder_tick()            # ~150 µs (3 lecturas I2C)
        motor_pid_per_motor()     # ~300 µs (3 PIDs internos)

    if since_debug_print >= 1000 ms:
        debug_print()             # ~500 µs
```

**Loop budget**: típico ~500 µs, peor caso ~2 ms. A 100 Hz hay margen amplio (10 ms).

### 14.2 Latencia decisión → motor

| Etapa | Tiempo |
|-------|--------|
| ARRIBA detecta evento | (depende del sensor) |
| TX UART hacia CENTRAL | ~1.5 ms |
| Próximo tick del FSM (peor caso) | < 10 ms |
| Strategy decide | < 1 ms |
| PIDs + kinematics + PWM | < 0.5 ms |
| **Total ARRIBA → motores** | **~13 ms** |

### 14.3 Latencia EMERGENCIA

| Etapa | Tiempo |
|-------|--------|
| ABAJO detecta blanco | < 1 ms (1 kHz sampling) |
| TX UART hacia CENTRAL | ~700 µs (5 bytes a 230400 baud) |
| CENTRAL recibe + handler | ~50 µs |
| `motors_brake()` aplica | instantáneo |
| **Total** | **~2 ms desde detección a freno activo** |

Esto es 7× más rápido que el objetivo (< 15 ms).

---

## 15. Estructuras de datos enviadas

CENTRAL **principalmente recibe** datos. Lo único que envía son comandos administrativos cortos:

| Mensaje | Hacia | Payload |
|---------|-------|---------|
| `CENTRAL_RESET_OTOS` | ABAJO | 1 byte (flag) |
| `CENTRAL_CALIB_LINE` | ABAJO | 1 byte (0=carpet, 1=white) |
| `CENTRAL_RESET_TOP` | ARRIBA | 1 byte (flag) |
| `CENTRAL_TOP_CMD` | ARRIBA | hasta 16 bytes (TBD por necesidad) |

Como CENTRAL es master, no genera streams pesados — solo eventos puntuales.

**Internamente** CENTRAL mantiene:

```cpp
struct WorldModel {
    // Espejo del WORLD_SNAPSHOT recibido (24 bytes)
    int16_t my_x_mm, my_y_mm, my_heading_centideg;
    uint8_t my_pose_confidence;
    int16_t ball_x_mm, ball_y_mm;
    uint8_t ball_visible, ball_confidence;
    int16_t goal_opp_angle_centideg, goal_opp_distance_mm;
    uint8_t goal_opp_visible, goal_own_visible;
    uint16_t min_obstacle_mm;
    uint8_t referee_cmd, flags;

    // Espejo del LINE_URGENT recibido
    int16_t line_angle_centideg;
    uint8_t line_depth, line_imminent_exit;

    // Metadata
    uint32_t snapshot_last_rx_ms;
    uint32_t line_last_rx_ms;
};
```

---

## 16. Diagnóstico y debug

### 16.1 LED de estado

| Patrón | Significado |
|--------|-------------|
| Apagado | Boot incompleto |
| Encendido fijo | PLAYING — todo OK |
| Parpadeo lento (1 Hz) | WAITING_REFEREE |
| Parpadeo rápido (5 Hz) | SAFE_NO_TOP (ARRIBA caída) |
| Patrón SOS | LOST (múltiples fallos) |
| Encendido sólido en EMERGENCY_LINE | Alerta visual durante el freno |

### 16.2 USB Serial

A 115200 baud, cada 500 ms imprime:
- Estado actual de la FSM (`ATK_APPROACH`, `GK_PATROL`, etc.).
- `match_running`, `referee_cmd`.
- Pose actual (x, y, θ) + confidence.
- Pelota (visible/predicha) + distancia + confidence.
- Output de PIDs: heading_setpoint, omega_cmd, lateral_cmd.
- Velocidades de cada motor (calculadas y, si hay encoders, medidas).
- Contadores de UART: TX/RX/CRC errors por cada stream.

### 16.3 Comandos USB

Vía USB Serial el operario puede:
- `state` — imprime estado actual de la FSM.
- `pose` — pose del world model.
- `motor X Y` — aplica PWM signed Y al motor X (debug manual).
- `stop` — frena todos los motores.
- `cal_line` — envía CALIB_LINE al ABAJO.
- `reset_otos` — envía RESET_OTOS al ABAJO.
- `kick` — dispara el kicker (delantero).
- `dump` — dump completo de world model + PIDs + motor states.

---

## 17. Tabla resumen

| Aspecto | Valor |
|---------|-------|
| MCU | Teensy 4.1 sobre Zircon Rev v15 |
| UARTs activos | Serial1 (← ARRIBA), Serial2 (← ABAJO) |
| Frecuencia recepción | 100 Hz snapshot, 200 Hz línea |
| Frecuencia FSM | 100 Hz |
| Frecuencia PIDs capa media | 100 Hz |
| Frecuencia PID por motor (si hay encoders) | 1 kHz |
| Encoders | Opcional (AS5600 magnéticos recomendados) |
| Cinemática | Inversa omni-3 (ya implementada + 11 tests pasan) |
| Saturación | Proporcional (preserva dirección) |
| PIDs activos | Heading (siempre), Lateral arquero (si GK), Approach (si ATK) |
| Latencia ARRIBA → motor | ~13 ms |
| Latencia EMERGENCIA línea → freno | **~2 ms** ✓ < 15 ms |
| Watchdog ARRIBA | 500 ms |
| Watchdog hardware | 1 s (WDT del Teensy) |
| Carga CPU estimada | ~25% (con encoders activos), ~15% (sin encoders) |

---

## 18. Plan de trabajo factible

### Nivel 1 (Incheon MÍNIMO) — 1-2 semanas

- Recibir `WORLD_SNAPSHOT` y `LINE_URGENT` ✓ (ya implementado en el refactor reciente).
- FSM básica: WAIT_START → SEARCH → APPROACH → SEARCH (sin POSITION ni PUSH_KICK).
- PID heading activo siempre.
- PID approach activo cuando ve pelota.
- Modo EMERGENCY_LINE funcional.
- Sin encoders, sin behind-the-ball, sin kicker.
- Modo arquero básico: PID_lateral con measurement de ABAJO, oscilación lateral simple.

**Suficiente para**: robot persigue pelota y patea por embestida. Arquero patrulla la línea de fondo. Es **funcionalmente equivalente al nacional 2025** pero con arquitectura distribuida.

### Nivel 2 (Incheon IDEAL) — +1-2 semanas

- Nivel 1 + behind-the-ball del delantero.
- Orbit cuando el robot está del lado equivocado de la pelota.
- Kicker activado cuando alineado con arco rival.
- Set play KICKOFF básico.
- FSM arquero con GK_INTERCEPT + GK_CLEAR.
- Recovery automático en EMERGENCY_LINE.

**Suficiente para**: estrategia táctica reactiva pero competente. Gana partidos contra rivales medios.

### Nivel 3 (Roboliga Nov 2026) — 4-6 semanas post-Incheon

- Encoders AS5600 instalados + PID por motor (closed-loop).
- Coordinación con partner via WORLD_SNAPSHOT (decisión "quién va a la pelota").
- Set plays adicionales: GOAL_KICK, LATERAL.
- Tunear PIDs con datos reales del partido.
- WORLD_SNAPSHOT extendido con partner data integrada.

**Suficiente para**: defender título nacional argentino.

### Nivel 4 (Mundial 2027) — 4-8 semanas pre-mundial

- Anticipación de pelota usando velocidad recibida del WORLD_SNAPSHOT.
- Predicción de tiro del rival (cuando ve la pelota cerca del rival con vector hacia nuestro arco).
- Modo `GK_RUSH` para arquero atacante.
- Coordinación pase entre robots (un robot empuja la pelota hacia donde puede recibirla el partner).
- Modelo del rival (estadísticas online de los primeros minutos).

**Suficiente para**: top 5 mundial.

### Nivel 5 (largo plazo, 2027+)

- Aprendizaje por refuerzo offline (entrenar la FSM con simulación).
- Estrategia adaptativa por rival.
- Comportamientos emergentes complejos.

---

## 19. Compatibilidad con código del nacional 2025

El robot del nacional 2025 corre un firmware monolítico en el Zircon Rev v15. Ese código **NO se rompe** con esta nueva arquitectura — el código viejo sigue funcionando en `software/robot-{delantero,arquero}/` y se compila con el env `teensy41_legacy` del `platformio.ini`.

**Plan B explícito**: si el firmware nuevo no termina a tiempo para Incheon, se va a la competencia con el firmware viejo + las correcciones P0/P1 ya identificadas (bugs `currentYaw` raw, gap 3 < |Yp| < 5, UART sin sincronización, etc. — ver `docs/internal/analisis-definitivo-{arquero,delantero}.md`).

**Transición progresiva**:
1. Fase 1: probar el firmware nuevo en lab con el robot del 2025 (mismo hardware).
2. Fase 2: probar en cancha con scrimmage interno.
3. Fase 3: si Fase 2 va bien y antes de Incheon, usar el nuevo. Si no, ir con el viejo.

---

## 20. Lo que NO hace (límite de scope)

- **Procesamiento de imagen / blob detection**: vive en las OpenMV y en ARRIBA.
- **Lectura de sensores de luz**: vive en ABAJO.
- **Fusión sensorial (EKF)**: vive en ARRIBA.
- **Comm con árbitros**: bridge en placa COMM via ARRIBA.
- **Comm con partner**: ESP-NOW via COMM, integrado en el WORLD_SNAPSHOT por ARRIBA.
- **Logging persistente**: el Teensy 4.1 tiene SD slot — implementar si se necesita análisis post-partido (Nivel 4+).

---

## 21. Referencias

- Arquitectura general: [`docs/ARQUITECTURA-3-PLACAS-2026.md`](../ARQUITECTURA-3-PLACAS-2026.md)
- Firmware placa hermana ABAJO: [`docs/firmware/FIRMWARE-PLACA-ABAJO.md`](FIRMWARE-PLACA-ABAJO.md)
- Firmware placa hermana ARRIBA: [`docs/firmware/FIRMWARE-PLACA-ARRIBA.md`](FIRMWARE-PLACA-ARRIBA.md)
- Pinout Teensy 4.1 en Zircon: [`hardware/electronics/mapa-pines-teensy-ambos-robots.md`](../../hardware/electronics/mapa-pines-teensy-ambos-robots.md)
- Diferencia entre robots: [`journal/2026-03-20-diferencias-pines-motores-arquero-delantero.md`](../../journal/2026-03-20-diferencias-pines-motores-arquero-delantero.md)
- Análisis código viejo del delantero: [`docs/internal/analisis-definitivo-delantero.md`](../internal/analisis-definitivo-delantero.md)
- Análisis código viejo del arquero: [`docs/internal/analisis-definitivo-arquero.md`](../internal/analisis-definitivo-arquero.md)
- Limitaciones del robot marzo 2026: [`docs/internal/limitaciones-robot-marzo-2026.md`](../internal/limitaciones-robot-marzo-2026.md)
- Strategy playbooks (legacy, en `skills/`): striker-strategy.md, goalkeeper-strategy.md, kickoff-set-plays.md
- Cinemática inversa: `src/shared/kinematics.h` + tests en `test/test_kinematics/`
- Protocolo UART: `src/shared/proto.h`
- Implementación actual: `src/central/`
- AS5600 datasheet (encoders magnéticos): https://ams.com/as5600

---

*Documento mantenido por IITA — Instituto de Informática y Tecnología Aplicada, Salta, Argentina.*
