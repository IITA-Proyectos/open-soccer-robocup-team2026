---
title: "Arquitectura distribuida de 3 placas — análisis profundo y propuesta de esquema robusto"
date: 2026-05-11
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: draft
tags: [arquitectura, distribuida, top-board, down-board, central, uart, topologia, decision]
robot: ambos
area: software
tipo: decision
relacionado:
  - issue: 9
  - docs/internal/sistema-posicionamiento-y-comunicacion.md
  - docs/multi-camera-world-model.md
  - docs/lidar-tof-slam-analysis.md
  - research/in-progress/2026-05-10-diseno-firmware-3-placas.md
---

# Arquitectura distribuida de 3 placas — análisis y propuesta

## Resumen ejecutivo (5 puntos)

1. **El esquema básico de Enzo (issue #9) es sólido pero necesita un ajuste**: las 3 placas con CENTRAL (Teensy 4.1) como master, TOP y DOWN como pre-procesadores especializados. La topología STAR (ambas slaves → CENTRAL) es correcta como base, pero conviene **enriquecerla con un bus secundario DOWN→TOP para fusión sensorial**.
2. **DOWN no controla motores**. El PID lateral del arquero que Gustavo propone se implementa como **"sugerencia de comando lateral" que DOWN envía a CENTRAL cada 10 ms**, no como control directo. Latencia objetivo: < 20 ms desde detección de línea → corrección PWM en motores.
3. **TOP es el cerebro sensorial**: cámaras + IMU dual + ToF + (opcional) odometría desde DOWN. Output: world snapshot con pose propia, pelota, arcos, obstáculos. La CENTRAL **no fusiona** — solo recibe el snapshot y decide.
4. **Detección de "fin del área chica" NO la puede hacer DOWN sola** — requiere conocimiento de pose absoluta (cámara identifica arcos → triangulación). Es responsabilidad del TOP.
5. **Mi implementación actual (TOP master) no matchea con este esquema**. Hay un refactor pendiente: mover `strategy`, `world_model`, `motors` de TOP a CENTRAL. Estimación: 2-3 días de trabajo. Plan B vigente: si no llegamos, ir a Incheon con la arquitectura monolítica del 2025.

---

## 1. Contexto

### 1.1 Lo que propuso Enzo (issue #9 + imagen adjunta)

```
┌────────────────────┐   UART   ┌─────────────────────┐   UART   ┌────────────────────┐
│   Placa de Abajo   │ ───────► │   Placa Central     │ ◄─────── │   Placa de Arriba  │
│   Teensy 4.0       │          │   Teensy 4.1        │          │   Teensy 4.0       │
├────────────────────┤          ├─────────────────────┤          ├────────────────────┤
│ • 24 sensores luz  │          │ • Manejo motores    │          │ • Módulo árbitros  │
│   con MUX 3×8      │          │   con PID           │          │ • BNO055 × 2       │
│ • 2 odom OTOS I2C  │          │ • Decisiones        │          │ • Cámaras × 2      │
│ • Arma packet UART │          │ • FSM principal     │          │ • Ultrasonido      │
│                    │          │ • Lee ambas placas  │          │ • TOF × 4          │
│                    │          │   por UART          │          │ • Arma packet UART │
└────────────────────┘          └─────────────────────┘          └────────────────────┘
```

Topología STAR — ambas slaves envían independientemente al CENTRAL. CENTRAL fusiona y decide.

### 1.2 Lo que propuso Gustavo (chat 2026-05-11)

- **CENTRAL** (Teensy 4.1, Zircon Rev v15): motores + decisiones + estrategia + FSM principal. **Es la que decide qué hacer**.
- **DOWN** (Teensy 4.0): si hay línea / dónde; no salirse de cancha; **PID local del arquero para movimiento lateral pisando línea de fondo**; detectar bordes; detectar fin del área chica.
- **TOP** (Teensy 4.0): BNO055 + ToF + cámaras. Dice dónde cree que está el robot (x, y, orientación) y dónde está la pelota.
- **Pregunta abierta de Gustavo**: ¿DOWN → CENTRAL + TOP → CENTRAL (independiente), o DOWN → TOP → CENTRAL (TOP integra info de DOWN antes de pasarla)?

### 1.3 Lo que ya hay en el repo (3 docs clave)

| Doc | Contenido relevante |
|-----|---------------------|
| `docs/internal/sistema-posicionamiento-y-comunicacion.md` (2026-03-20) | Arquitectura por **4 capas** (sensores → comm → world model → estrategia). **Implementación progresiva en 7 fases** (Nivel 1 simple → Nivel 3 EKF). Define lo que necesita saber el robot: pose propia, pelota, partner, oponentes, arcos. |
| `docs/multi-camera-world-model.md` (2026-03-28) | Recomienda **2 cámaras** (frente + atrás) para 2026. WorldModel unificado con confidence-per-source. Pendiente Kalman pelota. |
| `docs/lidar-tof-slam-analysis.md` (2026-03-28) | ToF mejor que LiDAR para Junior. **NO SLAM completo** — solo posición por trilateración. Confirmado: 2026 permite ToF en Soccer Vision. |

Hay un patrón claro: el equipo ya **decidió fusión multi-sensor + world model centralizado**. La pregunta abierta es **dónde corre cada parte**.

---

## 2. Análisis funcional por placa

### 2.1 PLACA CENTRAL (Teensy 4.1, Zircon Rev v15) — Master del robot

**Función principal**: decidir qué hacer y ejecutarlo.

**Responsabilidades primarias** (sí o sí):
- **FSM principal** (delantero / arquero según dipswitch + comando árbitro).
- **PID de motores** — cinemática inversa omni-3, PD heading con BNO desde TOP.
- **Control PWM directo** a los 3 H-bridges del Zircon.
- **Decisión táctica** (atacar / defender / pase / kicker).
- **Watchdog general** — si TOP o DOWN no responde, decide modo seguro.

**Responsabilidades secundarias** (pueden delegarse):
- World model fusion **NO** (la hace TOP — sería redundante).
- Comunicación con árbitros **NO** (la hace TOP via placa COMM externa).

**Lo que NO debe hacer**:
- Procesamiento de imagen de cámaras (responsabilidad TOP).
- Lectura de 32 sensores de línea multiplexados (responsabilidad DOWN).
- Filtrado Kalman de pose (responsabilidad TOP).

**Carga estimada**:
- 100 Hz FSM tick + motor PID = ~20% CPU del Teensy 4.1.
- 100 Hz UART decode × 2 (de TOP y DOWN) = ~5% CPU.
- **Mucha capacidad libre** para estrategia compleja, partner coordination, etc.

### 2.2 PLACA ARRIBA (Teensy 4.0) — Cerebro sensorial

**Función principal**: producir un "world snapshot" pre-procesado y entregarlo al CENTRAL a 100 Hz.

**Responsabilidades primarias**:
- **Lectura BNO055 × 2** (Wire bus 0 + Wire1 remap pines 24/25). Modo IMUPLUS para evitar interferencia magnética de motores.
- **Lectura 4 ToF VL53L7CX/L5CX** (2 en cada bus I2C). Distancia a paredes para detección de obstáculos y triangulación grueso.
- **Lectura HC-SR04 frontal** (fallback ToF + frontal cercano).
- **Parser cámaras OpenMV** × 2 (Serial3 + Serial5, protocolo viejo 9 bytes inicialmente, migrar después).
- **Comunicación con placa COMM** (Serial4 → ESP32-C6 árbitros + ESP-NOW partner).
- **Fusión sensorial → pose propia (x, y, heading)**:
  - Nivel 1 (Incheon mínimo): heading IMU + pelota relativa cámara. Sin x,y absoluto.
  - Nivel 2 (Incheon ideal): pose por triangulación cámaras (arcos) o ToF (paredes).
  - Nivel 3 (post-mundial): EKF completo.

**Responsabilidades secundarias** (a discutir):
- **Recibir odometría OTOS desde DOWN** (UART secundario) para integrarla en la fusión.
- **Detección de área chica** (línea pintada): cuando la cámara ve la línea blanca rectangular del área propia, reporta "estoy en el área chica" como flag al CENTRAL.

**Lo que NO debe hacer**:
- Control de motores.
- Decisiones tácticas.
- Lectura directa de sensores de línea (responsabilidad DOWN).

**Carga estimada**:
- 100 Hz BNO055 dual ≈ 5% CPU.
- ~30 Hz ToF × 4 ≈ 10% CPU (I2C tiene latencia).
- Parser cámaras (2 UARTs a 19200) = trivial < 1% CPU.
- Fusión + envío UART = ~5% CPU.
- **Total: ~25% CPU**, queda mucho margen para Kalman/EKF.

### 2.3 PLACA ABAJO (Teensy 4.0) — Sensor de piso

**Función principal**: detectar la cancha (líneas y movimiento) y reportar al CENTRAL.

**Responsabilidades primarias**:
- **Lectura 24-32 sensores ALS-PT19** vía 3-4 muxes CD4051 (selección A/B/C compartida, lectura por canal en paralelo).
- **Cálculo del ángulo de la línea** detectada (weighted avg de cosenos/senos).
- **Cálculo de profundidad** (cuántos sensores ven blanco).
- **Flag "imminent_exit"** — bandera urgente cuando el robot está por salir de la cancha.
- **Lectura 2× SparkFun OTOS** (Wire + Wire1 si hay 2 buses; Q5 dijo que van uno a cada lado para análisis diferencial).
- **Fusión OTOS dual → pose local (x, y, heading) odométrica**.
- **PID lateral local del arquero** (lo nuevo que Gustavo propone) — ver análisis detallado en §3.1.

**Responsabilidades secundarias**:
- Detección de bordes (consecuencia natural del cálculo de ángulo de línea — si depth > umbral, está cruzando el borde).
- **Detección "fin del área chica" — NO POSIBLE solo desde DOWN** (ver análisis en §3.2).

**Lo que NO debe hacer**:
- Control de motores directo (envía sugerencia al CENTRAL).
- Visión.
- Decisiones tácticas.

**Carga estimada**:
- 1 kHz tick lectura 32 sensores via mux + algoritmo ángulo ≈ 15% CPU.
- 100 Hz OTOS lectura + fusión ≈ 5% CPU.
- 100 Hz UART send ≈ 1% CPU.
- PID lateral arquero (cuando activo) ≈ 2% CPU.
- **Total: ~25% CPU** — mucho margen.

---

## 3. Análisis crítico de los puntos específicos que planteó Gustavo

### 3.1 "PID para movimiento lateral del arquero mientras pisa un poquito la línea de fondo"

**Idea**: el arquero patrulla lateralmente manteniéndose pisando la línea de fondo de su área. Como DOWN ve la línea con 32 sensores en anillo, puede calcular con altísima resolución qué tan adentro/afuera está y corregir.

**Análisis**:

| Opción | Cómo funciona | Latencia | Pros | Contras |
|--------|--------------|----------|------|---------|
| **A) PID local en DOWN, DOWN envía PWM directo al motor** | DOWN tiene línea propia a motores | 1-2 ms | Latencia mínima, máxima resolución | **Rompe el modelo** "CENTRAL decide todo". DOWN no tiene los motores cableados (sólo CENTRAL los tiene). |
| **B) PID local en DOWN, DOWN envía "sugerencia lateral" al CENTRAL en cada packet** | DOWN calcula `delta_vx_lateral` y lo manda como campo en `LineStatus`. CENTRAL lo aplica si role=ARQUERO y `match_running=true`. | 10-15 ms (1 UART hop) | DOWN sigue siendo slave; CENTRAL es master pero respeta la sugerencia; latencia aceptable | DOWN tiene que saber el modo del robot (necesita un mensaje TOP/CENTRAL → DOWN con "modo arquero ON/OFF") |
| **C) Sin PID local. DOWN envía solo "ángulo línea", CENTRAL calcula PID** | Todo en CENTRAL | 20-30 ms (DOWN→CENTRAL UART + tick CENTRAL) | Simplicidad — un solo lugar con el PID | Latencia mayor; CENTRAL tiene más carga; no aprovecha la resolución local de DOWN |

**Recomendación: Opción B** (PID local en DOWN, sugerencia al CENTRAL).

Implementación:
```cpp
// En DOWN, struct enviado en cada DOWN_LINE_STATUS:
struct LineStatusV2 {
    int16_t angle_centideg;       // ángulo línea
    uint8_t depth_count;          // sensores en blanco
    uint8_t imminent_exit_flag;
    int16_t suggested_vx_mm_s;    // ← NUEVO: PID lateral output (positive = derecha)
    uint8_t suggested_active;     // ← NUEVO: 1 si DOWN considera que su sugerencia es válida
} __attribute__((packed));

// CENTRAL recibe LineStatusV2 y, si role=ARQUERO + match_running + line.suggested_active:
//   cmd.vx_mm_s += line.suggested_vx_mm_s;  // sumarlo al cmd actual
```

DOWN entra en modo "PID activo" cuando CENTRAL le manda `TOP_SET_DOWN_MODE` con flag arquero. Por defecto el PID está apagado y `suggested_active = 0`.

### 3.2 "Detectar fin del área chica"

**Idea**: el robot detecte que está saliendo del área chica para poder reposicionarse o defender mejor.

**Realidad RoboCup Junior 2026**:
- El área chica es un **rectángulo pintado en blanco** alrededor de cada arco. Misma pintura que la línea de fondo.
- Los 32 sensores de DOWN **no pueden distinguir** "estoy en línea de fondo" vs "estoy en lateral del área chica" por color (ambas son blancas iguales).
- La diferencia es **geométrica**: la línea del área chica forma un ángulo de 90° con la línea de fondo.

**Cómo detectarlo**:

| Approach | Quién lo hace | Comentarios |
|----------|--------------|-------------|
| **A) Posicionamiento absoluto** (cámaras detectan arcos → triangulación → pose `(x, y)`). Comparar contra coords conocidas del área chica. | TOP (cámaras + fusión) | **La forma robusta**. Una vez que TOP estima pose, área chica = comparación numérica. |
| **B) Reconocimiento de patrón geométrico** desde los 32 sensores (forma de "L" vs línea recta única). | DOWN (algoritmo más complejo en line_ring) | Posible pero frágil — si el robot rota, el patrón rota con él. Necesita IMU. |
| **C) Calibrar zona "casa" del arquero** y trackearla con OTOS + IMU. Si OTOS te dice "estás a 30 cm de tu zona", reportar flag. | DOWN (OTOS) o TOP (fusión) | Funciona si OTOS no driftea. Riesgo de drift acumulado en 10 min de partido. |

**Recomendación**: **A primario, C secundario**. Es decir:
- **TOP** reporta al CENTRAL `in_own_penalty_area = true/false` cuando la pose estimada cae dentro del rectángulo del área chica propia.
- **DOWN** reporta `is_on_white_line = true/false` (binario, basado en sensores).
- CENTRAL combina ambos: si `is_on_white_line && in_own_penalty_area`, el robot puede inferir que está pisando el borde del área chica (no la línea de fondo).

**Esto requiere posicionamiento absoluto del TOP**. Si TOP no lo tiene en el Nivel 1 inicial, la detección de área chica queda en **Nivel 2** (después de Incheon o como mejora a último momento si el equipo tiene tiempo).

### 3.3 "Posicionamiento del robot (x, y, orientación)"

Ya analizado en `docs/internal/sistema-posicionamiento-y-comunicacion.md`. Plan progresivo:

| Nivel | Fuente | Precisión | Cuándo se implementa |
|-------|--------|-----------|----------------------|
| 1 (Incheon mínimo) | Heading IMU + pelota relativa cámara | Sin x,y absoluto | Ahora |
| 2 (Incheon ideal) | Pose por triangulación: tamaño/ángulo de arcos en cámara + OTOS desde DOWN como segundo source | ~10-15 cm | Si TOP termina rápido |
| 3 (post-mundial) | EKF completo (IMU + OTOS + cámaras + ToF) | ~3-5 cm | Roboliga Nov 2026 |

### 3.4 "¿DOWN → TOP → CENTRAL o DOWN → CENTRAL paralelo a TOP → CENTRAL?"

Esta es **la decisión arquitectónica más importante** del análisis. Veamos los 4 topologías candidatas.

---

## 4. Topologías analizadas

### Topología A — STAR centric (esquema Enzo, baseline)

```
   DOWN ─UART→ CENTRAL ←UART─ TOP
                  │
                  └──→ Motores
```

**Latencia DOWN → motores**: 1 hop UART (~10 ms) + CENTRAL decide + PWM = **~15-20 ms**.

**Pros**:
- Simplicidad arquitectónica.
- CENTRAL único punto de decisión, fácil de razonar.
- Cada slave es independiente; falla de una no impacta a la otra.

**Contras**:
- TOP no puede usar la odometría OTOS para mejorar fusión (DOWN no le habla).
- Si CENTRAL recibe dos snapshots desalineados en tiempo (TOP 50 ms viejo, DOWN 5 ms viejo), no puede hacer fusión temporal.

### Topología B — Hierarchical UP (DOWN → TOP → CENTRAL)

```
   DOWN ─UART→ TOP ─UART→ CENTRAL → Motores
```

**Latencia DOWN → motores**: 2 hops UART (~20 ms) + CENTRAL decide + PWM = **~25-30 ms**.

**Pros**:
- TOP integra OTOS en su fusión sensorial → world model más rico.
- Un único stream hacia CENTRAL — más simple.

**Contras**:
- **Latencia del flag "imminent_exit" se duplica** (DOWN → TOP → CENTRAL). 25 ms para "estoy saliendo de la cancha" es demasiado — el robot puede salirse antes de reaccionar.
- TOP se vuelve cuello de botella; si TOP se cuelga, DOWN deja de hablar con CENTRAL.

### Topología C — Híbrida con bus de emergencia (recomendada)

```
   DOWN ─UART→ TOP ─UART→ CENTRAL → Motores
     │                        ▲
     └────UART (urgent)───────┘
```

DOWN envía a **dos lugares**:
- **TOP**: odometría OTOS para fusión sensorial (no urgente, 100 Hz).
- **CENTRAL**: línea + flag imminent_exit + sugerencia PID arquero (URGENTE, 100-200 Hz).

**Latencia**:
- DOWN → CENTRAL emergencia: **1 hop UART, ~10 ms** ✓
- DOWN → TOP → CENTRAL (fusión completa): 2 hops, ~25 ms (aceptable para pose).

**Pros**:
- Lo mejor de A y B. La emergencia ("voy a salir!") viaja rápido. La fusión sensorial sigue siendo posible.
- DOWN puede entregar sugerencia de PID lateral al CENTRAL directo, sin pasar por TOP (latencia mínima para el arquero).

**Contras**:
- DOWN necesita 2 UARTs salida (Serial5 a TOP + Serial1 a CENTRAL).
- Más complejidad de cableado.
- Requiere que DOWN tenga 2 UARTs físicamente cableadas — verificar con Enzo en TASK-009 si es el caso.

### Topología D — TOP master (mi implementación actual, NO matchea con Enzo)

```
   DOWN ─UART→ TOP → CENTRAL (motor server tonto) → Motores
                ▲
                └─ Strategy + World Model + FSM aquí
```

**Esta es la topología que YO implementé en los commits previos**. CENTRAL (Zircon) es un "motor server" tonto que solo aplica cinemática inversa y aplica PWM. TOP corre la FSM completa.

**Pros**:
- Alinea master con sensorial (todo en un mismo MCU = fácil fusionar).
- Zircon (CENTRAL) queda "tonto" → menos riesgo de tocar lo que ya funciona.

**Contras**:
- **NO matchea el esquema de Enzo** (que dice "CENTRAL maneja motores Y decisiones").
- Si TOP se cuelga, el robot queda paralizado (CENTRAL no sabe qué hacer sin TOP).

---

## 5. Best practices (RoboCup MSL + Junior winners)

### CAMBADA (Middle Size League, U. Aveiro, top mundial)

- **Arquitectura distribuida** sobre 3 procesadores: bajo nivel (control motor + sensores), alto nivel (vision + strategy), un FPGA dedicado a vision.
- **World model centralizado** (alto nivel) con sensor fusion EKF.
- Comunicación inter-proceso por TCP/IP local (no UART — son adultos con más capacidad).
- **Lección aplicable**: separar **bajo nivel** (control + sensores rápidos) de **alto nivel** (estrategia + percepción) es una decisión estándar en robótica móvil.

### PCBWay (3° mundial RCJ Soccer Open 2022)

- **4 cámaras + 4 PCBs auxiliares**, cada cámara con su propio MCU que pre-procesa y manda al Teensy central.
- Topología STAR igual que Enzo propone: cámaras-MCUs → master.
- **Lección aplicable**: el principio "procesa donde está el sensor, decide en el centro" es lo estándar.

### RoboTeam Twente (RoboCup Small Size League)

- Pose estimation en **el robot**: IMU + encoders + visión externa (overhead camera del partido).
- En SSL la visión está centralizada externa, no aplicable directo a Junior. Pero la **fusión EKF de IMU + odometría** sí lo es.

### RCJ Soccer Open 2024-2025 (winners TDPs)

- **Mayoría usan un único MCU** (Teensy 4.1 monolítico).
- Los que distribuyen lo hacen para sensores muy específicos (ej. línea ring → MCU dedicado).
- **Lección**: distribuir tiene costo (firmware más complejo, sincronización). Solo distribuir donde aporta valor concreto.

---

## 6. Recomendación final — arquitectura "STAR centric con bus de emergencia"

### Diagrama de flujo de datos

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  PLACA TOP (Teensy 4.0) — Cerebro sensorial                                  │
│  ┌──────────────────────────────────────────────────────────────────────┐    │
│  │ Sensores:                                                            │    │
│  │   • Cámara 1 (Serial3) + Cámara 2 (Serial5)                          │    │
│  │   • BNO055 × 2 (Wire bus 0 + Wire1 bus 1 remap 24/25)                │    │
│  │   • ToF × 4 (2 en cada bus I2C)                                       │    │
│  │   • HC-SR04 (TRIG/ECHO)                                              │    │
│  │   • Recibe odometría OTOS de DOWN (Serial4 desde DOWN_TX)            │    │
│  │   • Recibe REFEREE_CMD de COMM (Serial4 hacia COMM)                  │    │
│  │ Procesa:                                                              │    │
│  │   • World snapshot (pose propia, pelota, arcos, obstáculos)           │    │
│  │   • Comm con árbitros + ESP-NOW partner                              │    │
│  └────────────────────────┬─────────────────────────────────────────────┘    │
│                           │ UART Serial1                                      │
│                           ▼ snapshot 100 Hz                                   │
└──────────────────────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│  PLACA CENTRAL (Teensy 4.1, Zircon Rev v15) — Master del robot              │
│  ┌──────────────────────────────────────────────────────────────────────┐    │
│  │ Inputs UART:                                                          │    │
│  │   • Serial1 ← TOP (world snapshot)                                   │    │
│  │   • Serial2 ← DOWN (línea + emergencia + sugerencia arquero)         │    │
│  │ Procesa:                                                              │    │
│  │   • FSM principal (delantero / arquero según dipswitch)              │    │
│  │   • Decisión táctica → MotorCommand (vx, vy, omega, kicker)          │    │
│  │   • Watchdog: TOP timeout → modo seguro; DOWN timeout → ciegos       │    │
│  │ Outputs:                                                              │    │
│  │   • PWM directo a los 3 motores omni (Zircon H-bridges)              │    │
│  │   • Comando dribbler / kicker (solo delantero)                       │    │
│  │   • Serial1 → TOP (reset OTOS, cal cameras, etc.)                    │    │
│  │   • Serial2 → DOWN (set mode arquero, calibrate line, etc.)          │    │
│  └──────────────────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────────────────┘
                            ▲
                            │ UART Serial2 (emergencia)
┌──────────────────────────────────────────────────────────────────────────────┐
│  PLACA DOWN (Teensy 4.0) — Sensor de piso                                    │
│  ┌──────────────────────────────────────────────────────────────────────┐    │
│  │ Sensores:                                                             │    │
│  │   • 32 ALS-PT19 via 4 muxes CD4051                                    │    │
│  │   • SparkFun OTOS × 2 (uno a cada costado, análisis diferencial)      │    │
│  │ Procesa:                                                              │    │
│  │   • Ángulo línea + depth + imminent_exit (1 kHz)                      │    │
│  │   • OTOS dual → pose odométrica local (x, y, heading, slip)          │    │
│  │   • PID lateral del arquero (activable por mensaje del CENTRAL)       │    │
│  │     → sugiere vx_mm_s al CENTRAL                                      │    │
│  │ Outputs UART:                                                         │    │
│  │   • Serial5 → TOP (odometría OTOS, 100 Hz, para fusión)              │    │
│  │   • Serial1 → CENTRAL (línea + imminent_exit + sugerencia, 100 Hz)   │    │
│  └──────────────────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Justificación

1. **CENTRAL master** = matchea con Enzo + tradición del Zircon que ya funciona.
2. **TOP cerebro sensorial** = aprovecha que TOP tiene cámaras + IMU + ToF en el mismo MCU = fusión EKF natural.
3. **DOWN con bus dual** (TOP + CENTRAL) = línea/emergencia rápido al CENTRAL, odometría a TOP para fusión completa. Lo mejor de A y B.

### Latency budget

| Evento | Camino | Latencia objetivo |
|--------|--------|------------------|
| "Voy a salir" → frenar | DOWN sensores → CENTRAL UART → PWM motor | **< 15 ms** |
| Pelota visible → girar hacia ella | Cámara → TOP parser → fusión → CENTRAL UART → strategy → motor | < 50 ms |
| Árbitro START → robot arranca | COMM → TOP UART → CENTRAL UART → strategy | < 30 ms |
| Arquero PID lateral tick | DOWN sensor → DOWN PID → sugerencia UART → CENTRAL aplica | **< 15 ms** |

### Protocolo UART (ya implementado en `src/shared/proto.h`)

Mantenemos el frame con START + LEN + TYPE + SEQ + PAYLOAD + CRC + END. Nuevos `MsgType` que hay que agregar:

| ID | Mensaje | Sentido | Propósito |
|----|---------|---------|-----------|
| 0x13 | DOWN_LINE_URGENT | DOWN → CENTRAL | Flag imminent_exit + ángulo línea + sugerencia PID (alta prioridad) |
| 0x14 | DOWN_ODOM | DOWN → TOP | OTOS pose + velocidad + slip (para fusión) |
| 0x22 | CENTRAL_SET_DOWN_MODE | CENTRAL → DOWN | Activa/desactiva PID arquero |
| 0x60 | TOP_WORLD_SNAPSHOT | TOP → CENTRAL | World model completo (pose, pelota, obstáculos, refcmd) |
| 0x61 | CENTRAL_RESET_TOP | CENTRAL → TOP | Reset world model / recalibrar |

---

## 7. Implicación para el firmware actual — refactor pendiente

Mi implementación de hoy (commits `a40cae6` → `f6a9b91`) tiene **TOP como master** (Topología D). No matchea con la recomendación. Refactor necesario:

| Módulo actual | Cambio |
|---------------|--------|
| `src/top/strategy.{h,cpp}` | **MOVER a `src/central/`** (Teensy 4.1) |
| `src/top/world_model.{h,cpp}` | **PARCIALMENTE MOVER** — la fusión sigue en TOP, pero la versión "consumida" la lee CENTRAL. Renombrar a `src/top/world_snapshot.{h,cpp}` |
| `src/top/motors.{h,cpp}` | **DEPRECAR** — los motores los maneja CENTRAL directo, no por UART al Zircon |
| `src/zircon/main_zircon.cpp` | **AMPLIAR** — pasa de "motor server tonto" a "master con FSM + world snapshot consumer" |
| `src/down/comm_top.{h,cpp}` | **MANTENER** (DOWN sigue enviando odom a TOP) |
| `src/down/comm_central.{h,cpp}` | **NUEVO** — bus de emergencia DOWN → CENTRAL |
| `src/central/` | **NUEVA carpeta** — `main_central.cpp`, `strategy.cpp`, `motors_pid.cpp`, `comm_top.cpp`, `comm_down.cpp` |
| `src/zircon/` | **RENOMBRAR a `src/central/`** o mantener como sub-módulo de motores HAL |

### Estimación

- **Refactor "TOP→CENTRAL master"**: 2-3 días de trabajo (mover archivos, cambiar protocolos, recompilar).
- **Implementar bus de emergencia DOWN→CENTRAL**: 1 día (agregar UART al firmware DOWN, nuevo MsgType).
- **Implementar PID lateral en DOWN**: 1-2 días (ajuste, calibración).
- **Probar end-to-end**: depende de hardware listo.

**Total**: ~5-7 días de firmware. Posible si Enzo entrega placas físicas en ~1 semana y el resto del equipo libera tiempo.

### Si NO se llega — Plan B

Mantener arquitectura monolítica del 2025 (Zircon + Teensy 4.1 corre todo). Ir a Incheon con eso + bugs P0 marzo arreglados. Las placas nuevas se debuggean post-mundial para Nacional Nov 2026.

---

## 8. Riesgos y mitigaciones

| Riesgo | Severidad | Mitigación |
|--------|-----------|-----------|
| Refactor TOP→CENTRAL consume tiempo que falta para integrar hardware | Alto | Hacer refactor en paralelo a integración hardware (Virginia/Elías hardware, Claude firmware) |
| Bus de emergencia DOWN→CENTRAL no implementable si DOWN no tiene 2 UARTs físicos | Medio | Verificar con Enzo en TASK-009. Si solo hay 1 UART en DOWN, ir a topología A pura (sin bus emergencia) |
| TOP cuelga → CENTRAL no recibe world snapshot → robot ciego | Alto | Watchdog en CENTRAL: si TOP timeout 500 ms, modo seguro (parar motores) |
| OTOS no llega a tiempo / no funciona como se espera | Medio | DOWN reporta odometría solo si OTOS_quality > umbral. CENTRAL no depende de OTOS para FSM básica |
| Detección área chica requiere posicionamiento absoluto que no llegamos | Bajo | Es feature P2 — no bloqueante para Incheon |
| Conflicto Wire1/Serial4 en pines 16/17 del TOP (Q3 inferida) | Alto | Esperar TASK-003 / TASK-009 para confirmar. Si Wire1 no está en pines 24/25, ajustar firmware |

---

## 9. Acciones inmediatas

1. **Enzo y equipo**: revisar este análisis y validar topología. Comentario en issue #9.
2. **Confirmar disponibilidad de 2do UART en DOWN** (para el bus emergencia DOWN→CENTRAL).
3. **Si topología C aprobada**: arrancar refactor del firmware (mover `strategy` + `world_model` a CENTRAL, agregar `comm_central` en DOWN).
4. **Si topología A aprobada** (sin bus emergencia): refactor más simple (solo mover lógica TOP→CENTRAL).
5. **TASK-009 (PCB JSON 04-20)** sigue siendo bloqueante para confirmar pinout final.

---

## 10. Referencias

- Issue #9 — Cuentas remoto + códigos arrancar (Enzo, 2026-04-29).
- Issue #10 — README hardware (Elías, 2026-04-18). Tiene esquema Canva del equipo.
- Issue #11 — Documentación para competencia (Enzo, 2026-05-05). Necesidad TDP.
- `docs/internal/sistema-posicionamiento-y-comunicacion.md` — arquitectura por capas.
- `docs/multi-camera-world-model.md` — fusión de cámaras.
- `docs/lidar-tof-slam-analysis.md` — ToF para posicionamiento.
- `research/in-progress/2026-05-10-diseno-firmware-3-placas.md` — diseño firmware (mi versión TOP-master).
- CAMBADA team papers (MSL, U. Aveiro). https://www.it.pt/Projects/Index/3
- PCBWay TDP RCJ 2022/2024. https://junior.robocup.org/tdp/
