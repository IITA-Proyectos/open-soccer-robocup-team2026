---
title: "Firmware Placa ARRIBA — Especificación funcional completa"
date: 2026-05-11
status: especificación
audience: equipo IITA Soccer Open
tags: [firmware, placa-arriba, top-board, cerebro-sensorial, fusion, ekf, kalman, partner-comm, especificacion]
---

# Firmware Placa ARRIBA — Especificación funcional completa

> Documento de referencia integral del programa que corre en la placa superior
> (Teensy 4.0) del robot 2026. Define qué hace el cerebro sensorial: percibe el
> mundo, fusiona todas las fuentes y entrega un *world snapshot* enriquecido al
> CENTRAL para que decida.

---

## Tabla de contenidos

1. [Resumen](#1-resumen)
2. [Hardware sobre el que corre](#2-hardware-sobre-el-que-corre)
3. [Responsabilidades funcionales](#3-responsabilidades-funcionales)
4. [Modos de operación](#4-modos-de-operación)
5. [Procesamiento del IMU dual (BNO055 × 2)](#5-procesamiento-del-imu-dual)
6. [Procesamiento de ToF multizona (4 × VL53L5/L7CX)](#6-procesamiento-de-tof-multizona)
7. [Procesamiento de cámaras (2 × OpenMV)](#7-procesamiento-de-cámaras)
8. [Fusión sensorial → World Snapshot](#8-fusión-sensorial--world-snapshot)
9. [Estimación de pelota con predicción Kalman](#9-estimación-de-pelota-con-predicción-kalman)
10. [Estimación de rivales](#10-estimación-de-rivales)
11. [Comunicación con partner (ESP-NOW vía COMM)](#11-comunicación-con-partner)
12. [Integración de información de ABAJO](#12-integración-de-información-de-abajo)
13. [Comunicaciones UART](#13-comunicaciones-uart)
14. [Detección de fallos y recovery](#14-detección-de-fallos-y-recovery)
15. [Timing y latencias](#15-timing-y-latencias)
16. [Estructuras de datos enviadas](#16-estructuras-de-datos-enviadas)
17. [Diagnóstico y debug](#17-diagnóstico-y-debug)
18. [Tabla resumen](#18-tabla-resumen)
19. [Plan de trabajo factible — Niveles 1 a 5](#19-plan-de-trabajo-factible)
20. [Lo que NO hace (límite de scope)](#20-lo-que-no-hace-límite-de-scope)
21. [Referencias](#21-referencias)

---

## 1. Resumen

La placa ARRIBA es el **cerebro sensorial** del robot. No toma decisiones tácticas ni controla motores — su trabajo es **ver, fusionar y entregar conocimiento**.

**Entrega al CENTRAL cada 10 ms (100 Hz)** un único struct `WorldSnapshot` con:

- **Pose propia**: dónde está el robot en cancha (x, y, heading) y velocidad (vx, vy, omega), con índice de confianza 0-100.
- **Pelota**: posición (relativa o absoluta), velocidad estimada, confianza. Si no es visible, se predice por inercia con confianza decayente.
- **Compañero**: pose y estado recibidos vía ESP-NOW (cuando llega).
- **Rivales**: estimación de hasta 2 oponentes con baja confianza (obstáculos detectados por ToF + cámara que no son pelota ni partner).
- **Arcos**: posiciones fijas conocidas, confirmadas cuando se ven.
- **Comando árbitro**: start/stop/halftime recibido por la placa COMM.
- **Flags**: match running, in_own_penalty_area, partner_alive, partner_sees_ball, IMU degradado, etc.

**Fusiona 5 fuentes**:
1. IMU dual (BNO055 × 2) → heading absoluto con redundancia.
2. ToF multizona (4 sensores VL53L5/L7CX) → distancias a paredes → pose XY por trilateración.
3. Cámaras (2 × OpenMV) → posiciones relativas de pelota y arcos.
4. Odometría OTOS (recibida de ABAJO) → pose acumulada por óptica de piso.
5. Línea (recibida de ABAJO) → confirmación de bordes y áreas chicas.

**Habla con el robot compañero** vía la placa COMM (ESP32-C6) por ESP-NOW. Intercambia snapshots a 10 Hz para coordinación.

La placa ARRIBA es el módulo más complejo computacionalmente del robot. Su carga estimada está en **~45% de CPU del Teensy 4.0** con el plan Nivel 3 (EKF + Kalman pelota + partner comm). Margen suficiente para el chip a 600 MHz.

---

## 2. Hardware sobre el que corre

> **🔧 ACTUALIZACIÓN ToF 2026-05-30 (recableado de Enzo, confirmado en banco).**
> Este doc es de diseño (2026-05-11) y partes están desactualizadas. Verdad de
> hardware sobre ToF: los **4 ToF VL53L7CX cuelgan TODOS del bus `Wire`** (I²C0,
> 18/19), cada uno con su pata **LP** cableada por bodge a un pin del Teensy
> (**{9,10,11,12}, activo-alto**), y **enumeran a 0x2A/0x2B/0x2C/0x2D** (NO
> 0x52..0x58). Esto **liberó `Wire1` (24/25) para la placa DOWN**. ⚠️ Las
> direcciones I²C persisten con 3V3 → power-cycle obligatorio al enumerar.
> El UART TOP→CENTRAL es **Serial4 (RX 16 / TX 17)** (fix 2026-06-02: el Teensy 4.0
> NO expone Serial7 28/29 en el borde —son pads SMD traseros, no cableables con
> header—; COMM=Serial2 7/8, CENTRAL=Serial4 16/17). La cámara trasera quedó en Serial5.
> Pinout canónico: `hardware/electronics/top-board-pack/01-pinout-y-hardware.md`.
> Detalle: `journal/2026-05-30-top-tof-4-en-bus-unico-enumeracion-ok.md`.

| Componente | Cantidad | Conexión | Notas técnicas |
|-----------|----------|----------|----------------|
| MCU Teensy 4.0 | 1 | — | Cortex-M7 600 MHz, 1 MB RAM, 2 MB flash |
| Cámaras OpenMV N6 (antes H7 Plus) | 2 | UART (Serial3 frontal + Serial5 trasera) | 19200 baud, protocolo viejo 9 bytes/packet |
| BNO055 IMU | 2 | I2C dual (Wire + Wire1) | Wire1 remapeado a pines 24/25 (Q3 confirmado) |
| Sensor ToF VL53L7CX | 4 fijos (plan: 6) | **TODOS en `Wire` (I²C0)**, LP individual por bodge | 8×8 SPAD multizona. Dir 0x2A..0x2D. Plan: +2 móviles para pelota |
| Ultrasonido HC-SR04 | 1 | TRIG=pin 4 / ECHO=pin 3 | Frontal, fallback de ToF, lectura bloqueante 25 ms (banco 2026-05-31) |
| Placa COMM (ESP32-C6) | 1 | UART (**Serial2, RX 7 / TX 8**) | Bridge a árbitros + ESP-NOW partner (fix 2026-06-02) |
| Conector hacia DOWN | 1 | UART (Serial1) | Recibe ODOM_POSE/VEL de ABAJO |
| Conector hacia CENTRAL | 1 | UART (**Serial4, RX 16 / TX 17**) | Envía WORLD_SNAPSHOT (fix 2026-06-02: el Teensy 4.0 no expone Serial7 28/29 en el borde) |
| Conector Dean-T-F batería | 1 | 7.4V LiPo | Comparte con CENTRAL y ABAJO |
| Reguladores MP1584-EN | 2 | 7.4V → 5V y 7.4V → 3.3V | Para sensores y MCU |
| LED de estado | 1 | LED_BUILTIN (pin 13) | Diagnóstico humano |

### Buses I2C — distribución crítica

| Bus | SDA | SCL | Periféricos | Tráfico estimado |
|-----|-----|-----|-------------|------------------|
| Wire (I2C0) | 18 | 19 | BNO055 #1 (0x28) + **los 4 ToF** (0x2A/0x2B/0x2C/0x2D) | ~30 KHz transacciones |
| Wire1 (I2C1) | 25 (remap) | 24 (remap) | **(libre para placa DOWN)** — el 2do BNO se movió a `Wire` (0x29) el 2026-05-31 | ~30 KHz |

Los 2 BNO055 quedaron **ambos en el bus `Wire`** (18/19): LEFT=0x28 y RIGHT=0x29
(pad ADR del 2do puenteado a 3V3, recableado 2026-05-31), lo que liberó `Wire1`
(24/25) para la placa DOWN. Los **4 ToF cuelgan del mismo bus `Wire`**
(recableado 2026-05-30) y se enumeran al boot por su pin **LP** (bodge):
arrancan todos en 0x29, se duermen todos, se despierta uno por uno y a cada uno
se le asigna **0x2A → 0x2B → 0x2C → 0x2D** (ninguno queda en 0x29). Pines LP
confirmados en banco: **{9,10,11,12}, activo-alto**. ⚠️ Las direcciones I²C de
los VL53L7CX persisten con 3V3 → **power-cycle obligatorio al enumerar** (un
reset del Teensy no las borra).

### UARTs

| Serial | TX | RX | Conectado a | Baud | Rol |
|--------|----|----|-------------|------|-----|
| Serial1 | 1 | 0 | conector U16 ← DOWN | 230400 | Recibe ODOM |
| **Serial5** | **20** | **21** | ← Cámara 2 (trasera) | 19200 | **Cámara trasera soldada acá** (✅ banco 2026-05-31, FORMATO OK) |
| Serial3 | 14 | 15 | conector U8 ↔ Cámara 1 | 19200 | Protocolo OpenMV ✅ FORMATO OK |
| **Serial2** | **8** | **7** | conector U15 ↔ COMM | 115200 | **Bridge árbitros + partner** (fix 2026-06-02) |
| **Serial4** | **17** | **16** | → CENTRAL | 230400 | **Envía WORLD_SNAPSHOT** (TX4=pin 17 → CENTRAL pin 28; fix 2026-06-02: el Teensy 4.0 no expone Serial7 28/29 en el borde) |

> **✅ Actualización 2026-05-31 (TASK-204, vale para todo este doc):** la **cámara
> trasera** quedó soldada en **Serial5 (pin 21)** (confirmado en banco). El HC-SR04
> quedó en **pines 4/3** (TRIG/ECHO), no en 6/7 → el viejo conflicto del pin 7 ya no
> aplica. Donde más abajo diga "Serial5 → CENTRAL" o "HC-SR04 en 6/7", está superado.
>
> **🔧 Corrección 2026-06-02 (banco):** el UART **TOP→CENTRAL** NO está en Serial7
> (pines 28/29) — el **Teensy 4.0 no expone Serial7 28/29 en el borde** (son pads SMD
> traseros, no cableables con header). Mapeo real del TOP: **COMM = Serial2 (RX 7 / TX 8)**
> @115200, **CENTRAL = Serial4 (RX 16 / TX 17)** @230400 (cable TOP pin 17 TX4 → CENTRAL
> pin 28 RX7 + GND). El lado CENTRAL es un Teensy 4.1 y SÍ recibe en su Serial7 (pin 28):
> ese extremo no cambia.

Quedan libres Serial6 (BLOQUEADO por Wire1 remap, pines 24/25); Serial7 NO es usable en el Teensy 4.0 (28/29 son pads SMD traseros, sin header en el borde).

---

## 3. Responsabilidades funcionales

| # | Responsabilidad | Frecuencia objetivo |
|---|-----------------|---------------------|
| R1 | Inicializar y mantener los 2 BNO055 (modo IMUPLUS) | Una vez al boot + 100 Hz lectura |
| R2 | Detectar discrepancias entre los 2 BNO055 | Cada lectura (100 Hz) |
| R3 | Estrategia de reseteo del IMU ante impacto detectado | Evento, ~1 vez por partido |
| R4 | Inicializar y leer los 4 ToF multizona | 15-30 Hz por sensor |
| R5 | Procesar matriz 8×8 de cada ToF para detectar paredes | 15-30 Hz |
| R6 | Trilateración con ToF → pose XY del robot | 15-30 Hz |
| R7 | Parser robusto del protocolo OpenMV (2 cámaras) | ~30 Hz (limitado por cámara) |
| R8 | Fusionar visión dual (frente + atrás) | 30 Hz |
| R9 | Recibir ODOM_POSE/VEL de ABAJO | 100 Hz |
| R10 | Recibir LINE_STATUS de ABAJO (legacy, opcional) | — |
| R11 | Fusión sensorial EKF → pose propia con confianza | 100 Hz |
| R12 | Predicción Kalman de pelota (extrapolación cuando no se ve) | 100 Hz |
| R13 | Estimación de rivales (de obstáculos ToF + cámara) | 30 Hz |
| R14 | Comm con placa COMM (recibir árbitros + recibir partner) | 100 Hz tick, 10 Hz envío partner |
| R15 | Fusión con datos del partner | 10 Hz |
| R16 | Construir `WorldSnapshot` y enviar a CENTRAL | 100 Hz |
| R17 | Recibir comandos administrativos del CENTRAL | bajo, eventos |
| R18 | Detección de IMU/ToF/cámara fallando | Continuo |
| R19 | Recovery: degradación elegante si una fuente cae | Continuo |
| R20 | Diagnóstico USB + LED de estado | 1 Hz print |

---

## 4. Modos de operación

| Modo | Cuándo se activa | Comportamiento |
|------|------------------|----------------|
| `BOOT` | Al encender | Init de subsistemas en orden: I2C, IMU, ToF, cámaras, UARTs |
| `CALIBRATING_IMU` | Comando al boot o `CENTRAL_RESET_TOP` con flag | Espera estabilización + calibración gyro de ambos BNO055 (~2-3 s) |
| `NORMAL` | Default tras `BOOT` | Loop completo: lectura + fusión + envío snapshot a 100 Hz |
| `DEGRADED_IMU_SINGLE` | Si uno de los 2 BNO055 falla | Sigue con el otro. Marca flag en snapshot |
| `DEGRADED_NO_CAMERAS` | Si ambas cámaras caen | World snapshot reporta `ball_visible=0` siempre, confianza pose baja |
| `DEGRADED_NO_TOF` | Si los 4 ToF fallan | Pose XY queda en confianza 0 (solo heading IMU disponible) |
| `LOST` | Si todas las fuentes fallan | Envía snapshot vacío, parpadea LED. CENTRAL detecta y entra en modo seguro |

**ARRIBA no conoce el rol del robot** (arquero/delantero) ni el modo del partido (running/stop). Siempre reporta lo mismo. El estado del partido viene como `referee_cmd` y `flags.match_running` en el snapshot que CENTRAL consulta para decidir.

---

## 5. Procesamiento del IMU dual (BNO055 × 2)

### 5.1 Lectura y frecuencia

Cada BNO055 ofrece:
- **Modo IMUPLUS** (recomendado): fusión acelerómetro + gyro, sin magnetómetro. Inmune a interferencia magnética de motores. Heading relativo (no Norte magnético).
- **Frecuencia máxima de actualización interna**: 100 Hz.
- **Latencia I2C** (400 kHz): ~300 µs por lectura de `orientation.x`.

Polling rate del firmware: **100 Hz**. Suficiente para control de heading y para detectar saltos.

### 5.2 Consistencia entre los 2 sensores

Los 2 BNO055 están en posiciones físicamente distintas dentro del robot (montados a izquierda y derecha de la placa TOP). En condiciones normales deberían reportar headings muy similares (diferencia < 2°) — son dos copias del mismo "hacia dónde miro".

**Algoritmo de chequeo de consistencia** (cada tick a 100 Hz):

```cpp
struct ImuReading {
    float heading_deg;      // -180 a +180
    uint8_t calibration;    // 0-3 según getCalibration() del BNO055
    uint32_t timestamp_ms;
    bool valid;
};

float wrap_diff(float a, float b) {
    float d = a - b;
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

void imu_consistency_check() {
    float diff = abs(wrap_diff(imu_left.heading_deg, imu_right.heading_deg));

    if (diff < 2.0f) {
        // OK — ambos sensores acuerdan. Usar promedio.
        fused_heading = (imu_left.heading_deg + imu_right.heading_deg) / 2.0f;
        imu_status = IMU_OK_BOTH;
    } else if (diff < 10.0f) {
        // Discrepancia menor. Sospechoso pero no fatal.
        // Usar el sensor con mejor calibración o histórico.
        if (imu_left.calibration >= imu_right.calibration) {
            fused_heading = imu_left.heading_deg;
        } else {
            fused_heading = imu_right.heading_deg;
        }
        imu_status = IMU_DISAGREE_MINOR;
    } else {
        // Discrepancia grande: > 10°. Uno de los dos saltó por impacto.
        // Usar histórico para decidir cuál creer.
        fused_heading = resolve_imu_conflict_by_history();
        imu_status = IMU_DISAGREE_MAJOR;
    }
}
```

### 5.3 Detección de IMU degradado / impacto

Cuando hay discrepancia mayor entre los 2 sensores, alguno saltó. Usamos la **historia reciente** para decidir cuál creer:

```cpp
// Buffer circular de últimos 10 readings por sensor (últimos 100 ms a 100 Hz).
struct ImuHistory {
    float heading_deg[10];
    int head_idx;
};

ImuHistory hist_left, hist_right;

float resolve_imu_conflict_by_history() {
    // Calcular la velocidad angular promedio de cada sensor en los últimos 100 ms.
    // El sensor que tuvo un salto súbito (impacto) tiene |omega| anómalamente alto
    // mientras que el sensor estable tiene |omega| consistente con el movimiento real.

    float omega_left  = compute_avg_omega(hist_left);
    float omega_right = compute_avg_omega(hist_right);

    // El sensor "creíble" es el que tiene menor magnitud de cambio reciente
    // (asumiendo que un impacto causa un salto grande mientras que el movimiento
    // real es continuo a velocidad moderada).
    if (abs(omega_left) < abs(omega_right) * 2.0f) {
        return imu_left.heading_deg;  // left es más estable
    } else {
        return imu_right.heading_deg; // right es más estable
    }
}
```

**Cuándo se considera que un IMU está roto/degradado:**
- Si su `getCalibration()` reporta 0 durante > 5 segundos en NORMAL.
- Si reporta saltos > 30° entre ticks consecutivos (10 ms).
- Si no responde a I2C (timeout o NACK).

Cuando uno se considera roto, ARRIBA entra en modo `DEGRADED_IMU_SINGLE` y reporta el flag en el snapshot. CENTRAL puede usar esto para confiar menos en el heading durante la próxima jugada.

### 5.4 Estrategia de reseteo del BNO055

Resetear el offset (re-capturar `headingOffset` con `bno.getEvent()`) es **destructivo** — pierde la referencia angular acumulada. Solo se hace cuando hay razones fuertes:

**Casos donde SÍ resetear:**

| Evento | Justificación |
|--------|---------------|
| Al boot del robot | Captura referencia inicial (asume robot mirando al arco rival) |
| Comando `CENTRAL_RESET_TOP` con flag IMU | Operario presiona botón antes del partido para recalibrar |
| Cuando ARRIBA detecta que el robot está **completamente alineado con un arco visible en cámara** | Si la cámara ve el arco centrado en su FOV, sabemos que estamos mirando al arco. Resetear `heading = 0°` (frente al arco rival) es seguro |
| Después de un partido completado (modo `RESET` del árbitro) | Recalibrar para el siguiente partido |

**Casos donde NO resetear (aunque parezca tentador):**

| Evento | Por qué no |
|--------|-----------|
| Cada vez que veo un arco | El arco puede no estar exactamente centrado. Resetear sin certeza introduce error |
| Si los 2 BNO055 discrepan | El reseteo no resuelve la discrepancia. Mejor degradación a uno solo |
| Si la calibración interna baja a 0 | El IMU sigue funcionando aunque la calibración baje. Solo afecta precisión, no validez |
| Cada cierto tiempo "por las dudas" | El drift es predecible y compensable con visión periódica. Reset agresivo es peor que el drift |

**Recalibración periódica por visión (sin reset):**
En lugar de resetear, ARRIBA puede aplicar una **corrección suave** cuando ve el arco rival:

```cpp
// Cuando vemos el arco rival, sabemos que apuntamos hacia él.
// La diferencia entre el ángulo visual del arco (cámara) y nuestro heading
// estimado es el "error" que vamos corrigiendo lentamente.
if (camera_sees_opponent_goal && camera_goal_angle_known) {
    float visual_angle_to_goal = camera_get_goal_angle();
    float expected_heading_for_goal_centered = 0.0f;  // si miramos al frente
    float drift = visual_angle_to_goal - expected_heading_for_goal_centered;

    // Aplicar corrección con ganancia baja (alpha 0.05) para no afectar
    // mediciones instantáneas pero corregir drift acumulado.
    headingOffset += drift * 0.05f;
}
```

Esto **NO es reset** — es ajuste suave del offset que corrige drift sin perder referencia.

---

## 6. Procesamiento de ToF multizona

### 6.1 Datasheet de los sensores

El equipo confirmó tener **VL53L7CX** disponibles (y VL53L5CX en pedido). Características relevantes:

| Parámetro | VL53L5CX | VL53L7CX |
|-----------|----------|----------|
| Resolución | 4×4 o 8×8 SPAD | 4×4 o 8×8 SPAD |
| FOV | 65° (4×4) o 45° (8×8) | **90° (4×4) o 60° (8×8)** |
| Rango | hasta 4 m | hasta 4 m |
| Frecuencia máxima | 60 Hz (4×4), 15 Hz (8×8) | 60 Hz (4×4), 15 Hz (8×8) |
| Interface | I2C @ 1 MHz | I2C @ 1 MHz |
| Bytes por matriz 4×4 | 16 zonas × 2 bytes = 32 bytes | mismo |
| Latencia I2C lectura completa | ~3-5 ms | mismo |

**Decisión del firmware**: usar **resolución 4×4 a 30 Hz** por sensor.
- 4×4 = 16 zonas por sensor → 64 zonas total entre los 4 sensores.
- 30 Hz es suficiente para reaccionar a obstáculos en movimiento.
- 8×8 da más resolución espacial pero solo 15 Hz — el extra de detalle no compensa la latencia adicional.

### 6.2 Cobertura espacial

Los 4 ToF están montados orientados hacia los 4 puntos cardinales del robot:

```
                    [ToF F]      ← mira hacia adelante (+Y)
                       ↑
                       |
        [ToF L] ←  [Robot] → [ToF R]
                       |
                       ↓
                    [ToF B]      ← mira hacia atrás (-Y)
```

Con FOV 90° del L7CX en modo 4×4, **cubren 360° completo** (cada uno cubre ±45° en su dirección). Esto es ideal para detectar paredes en cualquier dirección.

**Tabla de cobertura**:

| ToF | Dirección | FOV cubierto |
|-----|-----------|--------------|
| F | +Y (frente) | 270° a 90° (cruzando 0°) |
| R | +X (derecha) | 0° a 180° (cruzando 90°) |
| B | -Y (atrás) | 90° a 270° (cruzando 180°) |
| L | -X (izquierda) | 180° a 360° (cruzando 270°) |

Hay solape entre ToF adyacentes — útil para detectar discrepancias.

### 6.3 Detección de paredes

Cada zona del 4×4 reporta la distancia mínima detectada en su cono angular. Para detectar paredes:

```cpp
// Por cada ToF (F, R, B, L), tomamos la distancia mínima de las 16 zonas
// que apuntan al frente y a los costados cercanos. La pared más cercana
// es la distancia mínima.

struct ToFReading {
    uint16_t distance_mm[16];   // 4×4 zonas, fila 0 a 3 desde abajo
    uint8_t validity[16];        // 0-255, calidad de cada zona
    uint32_t timestamp_ms;
};

uint16_t get_nearest_wall_distance(ToFReading& tof) {
    uint16_t min_dist = 4000;  // máximo del sensor
    for (int z = 0; z < 16; z++) {
        if (tof.validity[z] > 50 && tof.distance_mm[z] < min_dist) {
            min_dist = tof.distance_mm[z];
        }
    }
    return min_dist;
}
```

### 6.4 Trilateración para pose XY

Con la cancha de **2430 mm × 1820 mm** (RoboCup Junior Soccer Open), las paredes están en posiciones conocidas:
- Pared izquierda: X = 0
- Pared derecha: X = 1820
- Pared abajo (arco propio): Y = 0
- Pared arriba (arco rival): Y = 2430

Si el robot está en pose `(x_r, y_r, θ)`:
- ToF F (mira al frente con orientación θ) detecta pared a distancia `d_F`. Si θ ≈ 90° (mirando hacia +Y), entonces `d_F ≈ 2430 - y_r`.
- ToF R (mira a +X relativo al robot, dirección absoluta θ-90°): `d_R ≈ 1820 - x_r` si θ ≈ 90°.
- Similar para B y L.

**Algoritmo simplificado (cuando el robot está alineado con los ejes de la cancha):**

```cpp
void estimate_pose_from_tof(float heading_deg, ToFData* tofs, Pose& out) {
    // Solo aplicable cuando heading está cerca de un eje cardinal
    // (lo más común en estrategia básica: mirar al arco propio o rival).

    if (abs(heading_deg - 90.0f) < 15.0f) {
        // Mirando al arco rival (+Y)
        out.y_mm = 2430 - tofs[F].nearest_mm;        // pared del arco rival
        out.x_mm = 1820 - tofs[R].nearest_mm;        // pared derecha
        // Cross-check con los otros 2:
        float y_check = tofs[B].nearest_mm;
        float x_check = tofs[L].nearest_mm;
        out.confidence = (abs(out.y_mm - (2430 - y_check)) < 100 &&
                          abs(out.x_mm - x_check) < 100) ? 90 : 50;
    }
    // ... casos similares para otros heading
}
```

**Algoritmo general (cualquier heading) — para Nivel 3:**

Usar las 4 distancias como observaciones de un **filtro EKF** que actualiza `(x, y, θ)` a partir de:
1. Estado anterior + modelo de movimiento (predicción).
2. Observaciones: 4 distancias a paredes con sus ángulos relativos al robot.
3. Innovación: diferencia entre distancia esperada (dada la pose hipotética) y distancia medida.

Esto da pose **independiente de orientación** y es el approach estándar en robótica móvil.

### 6.5 Frecuencias reales por sensor

| Operación | Tiempo | Frecuencia máxima |
|-----------|--------|-------------------|
| `get_ranging_data()` por sensor (I2C 1 MHz, 4×4) | ~3 ms | 60 Hz por sensor (limitado por sensor) |
| Procesar 16 zonas y extraer min distance | ~50 µs | trivial |
| Trilateración simple | ~100 µs | trivial |
| EKF update | ~1 ms | 100+ Hz alcanzable |

**Frecuencia objetivo del firmware**: 30 Hz para cada ToF. Los 4 cuelgan del
**mismo bus `Wire`** (recableado 2026-05-30), así que se leen secuencialmente en
ese bus → ~4 × ~3 ms ≈ 12 ms. Con tick a 30 Hz (33 ms) queda margen. (El diseño
original asumía 2 buses paralelos; ya no aplica.)

---

## 7. Procesamiento de cámaras

### 7.1 Protocolo viejo OpenMV

Mantenemos el protocolo viejo del nacional 2025 inicialmente (decisión Q6 del coach). Estructura: 9 bytes por packet, headers 201/202/203, decoder con state machine ya implementado en `src/top/cameras.{h,cpp}`.

Cada cámara reporta:
- Pelota (Xp, Yp coordenadas relativas al robot).
- Arco amarillo (Xam, Yam).
- Arco azul (Xaz, Yaz).
- Frecuencia: ~30 Hz (limitada por la cámara, no por el UART).

**Latencia frame OpenMV**: el procesamiento on-board de blobs + transmisión UART = ~30-50 ms desde "la cámara ve la pelota" hasta "TOP tiene el dato". Es lo mejor que se puede sin migrar a protocolo nuevo (futuro).

### 7.2 Fusión dual (2 cámaras)

Las 2 cámaras están montadas en orientaciones distintas (típicamente frente + atrás según `docs/multi-camera-world-model.md`):

```
       [Cam Frontal] ↑
         (FOV ~70°)
          [Robot]
       [Cam Trasera] ↓
         (FOV ~70°)
```

Cobertura total: ~140° (con zonas ciegas laterales). Para 2026 está OK; para 2027 evaluar 4 cámaras o lentes wide-angle.

**Algoritmo de fusión**:

```cpp
struct CameraObservation {
    int16_t ball_x_relative;     // mm respecto al robot
    int16_t ball_y_relative;
    bool ball_visible;
    float ball_confidence;       // basado en tamaño del blob, estabilidad
    uint32_t timestamp_ms;
    uint8_t camera_id;           // 0 = frontal, 1 = trasera
};

void fuse_cameras(CameraObservation& cam0, CameraObservation& cam1, BallEstimate& out) {
    // Convertir coords relativas a la cámara → coords relativas al robot
    // (la cámara trasera ve con rotación 180°).
    if (cam1.camera_id == 1) {
        cam1.ball_x_relative = -cam1.ball_x_relative;
        cam1.ball_y_relative = -cam1.ball_y_relative;
    }

    if (cam0.ball_visible && cam1.ball_visible) {
        // Ambas ven la pelota. Promediar ponderado por confianza.
        float w0 = cam0.ball_confidence;
        float w1 = cam1.ball_confidence;
        out.x = (cam0.ball_x_relative * w0 + cam1.ball_x_relative * w1) / (w0 + w1);
        out.y = (cam0.ball_y_relative * w0 + cam1.ball_y_relative * w1) / (w0 + w1);
        out.confidence = max(w0, w1) + 10;  // refuerzo por consenso
        out.visible = true;
    } else if (cam0.ball_visible) {
        out.x = cam0.ball_x_relative;
        out.y = cam0.ball_y_relative;
        out.confidence = cam0.ball_confidence;
        out.visible = true;
    } else if (cam1.ball_visible) {
        out.x = cam1.ball_x_relative;
        out.y = cam1.ball_y_relative;
        out.confidence = cam1.ball_confidence;
        out.visible = true;
    } else {
        out.visible = false;
        // Predicción Kalman se hace en la siguiente capa (sección 9).
    }
}
```

### 7.3 Detección de arcos para ayuda con pose

Los arcos están en posiciones fijas conocidas (centro de cada lado corto de la cancha). Si la cámara los detecta:
- Ángulo del arco respecto al frente del robot.
- Tamaño del blob → estimación de distancia (calibración offline).

Esto da una **observación adicional** al filtro EKF de pose:
- Si ARRIBA cree que el robot está en (x, y, θ) y la cámara ve el arco rival a ángulo `α_observed`, el ángulo esperado dado la pose hipotética es `α_expected = atan2(GOAL_OPP_Y - y, GOAL_OPP_X - x) - θ`.
- La diferencia `α_observed - α_expected` es la innovación que el EKF usa para corregir la pose.

---

## 8. Fusión sensorial → World Snapshot

### 8.1 Filtro EKF (pose propia)

El filtro Kalman extendido (EKF) integra todas las fuentes de pose:

**Estado**: `[x, y, θ, vx, vy, ω]` (6 variables)

**Modelo de movimiento (predicción)**:
```
x(t+dt)  = x(t)  + vx * dt
y(t+dt)  = y(t)  + vy * dt
θ(t+dt)  = θ(t)  + ω * dt
vx(t+dt) = vx(t)         (asume velocidad constante entre observaciones)
vy(t+dt) = vy(t)
ω(t+dt)  = ω(t)
```

**Observaciones disponibles** (cada una con su matriz H y ruido R):
- IMU dual → `θ` directo (R bajo, alta confianza).
- ODOM OTOS (de ABAJO) → `(x, y, θ, vx, vy, ω)` con drift (R medio).
- ToF trilateración → `(x, y)` (R medio, depende de calibración).
- Cámara viendo arco → restricción sobre `(x, y, θ)` combinados (R medio-alto).

**Frecuencia del EKF**: 100 Hz tick. Predicción siempre. Update solo cuando llega observación nueva.

**Complejidad computacional**:
- Predicción: 6×6 matriz multiplicación = ~36 ops. Trivial.
- Update: invertir matriz S = H*P*H' + R, depende del tamaño de la observación. Para observaciones de 2-3 dims: ~1000 ops. Manejable.

**Total EKF**: ~5000 ops por tick. A 100 Hz = 500 K ops/s. **0.1% del Cortex-M7 a 600 MHz**. Negligible.

### 8.2 Confianza de la pose

Cada componente del estado tiene su varianza estimada por el EKF. La confianza expuesta en el `WorldSnapshot` es:

```cpp
uint8_t compute_pose_confidence() {
    float sigma_x = sqrt(P[0][0]);   // raíz de varianza
    float sigma_y = sqrt(P[1][1]);
    float sigma_theta = sqrt(P[2][2]);

    // Si σ_x + σ_y < 30 mm y σ_θ < 3°, confianza alta.
    if (sigma_x < 15 && sigma_y < 15 && sigma_theta < 3.0f) return 95;
    if (sigma_x < 50 && sigma_y < 50 && sigma_theta < 10.0f) return 70;
    if (sigma_x < 200 && sigma_y < 200 && sigma_theta < 30.0f) return 40;
    return 10;
}
```

CENTRAL usa esta confianza para decidir si confiar en la pose para decisiones críticas (ej. patear al arco) o si esperar a tener más certeza.

---

## 9. Estimación de pelota con predicción Kalman

Cuando la pelota deja de ser visible (oclusión, sale del FOV), seguimos teniendo idea de dónde está si tenemos su última posición + velocidad.

**Filtro Kalman 2D simple para pelota**:

**Estado**: `[ball_x, ball_y, ball_vx, ball_vy]` (4 dims)

**Modelo de movimiento**:
```
ball_x(t+dt)  = ball_x(t)  + ball_vx * dt
ball_y(t+dt)  = ball_y(t)  + ball_vy * dt
ball_vx(t+dt) = ball_vx(t) * decay   (la pelota frena por fricción)
ball_vy(t+dt) = ball_vy(t) * decay
```

`decay = 0.99` por tick a 100 Hz → fricción del 1% por tick = decay a 36% en 100 ticks (1 segundo). Razonable para pelota rodando en carpet.

**Observación cuando es visible**: `(ball_x, ball_y)` con ruido R bajo.

**Confianza decayente cuando NO es visible**:

```cpp
uint8_t compute_ball_confidence(uint32_t time_since_last_seen_ms) {
    if (time_since_last_seen_ms == 0) return 95;  // visible AHORA
    if (time_since_last_seen_ms < 200) return 80;  // muy reciente
    if (time_since_last_seen_ms < 500) return 60;  // razonable
    if (time_since_last_seen_ms < 1000) return 30; // dudoso
    if (time_since_last_seen_ms < 2000) return 10; // muy dudoso
    return 0;  // perdido
}
```

CENTRAL sabe interpretar esto: si confianza > 60 puede tomar decisiones (perseguir la pelota predicha). Si < 30 debe entrar en modo búsqueda.

---

## 10. Estimación de rivales

Los oponentes no son colaborativos (no transmiten su pose). ARRIBA los **estima** combinando:

1. **ToF ve un obstáculo cercano** (típicamente 15-30 cm). Si la posición estimada no es:
   - El compañero (sabemos dónde está por ESP-NOW).
   - La pared (sabemos dónde está por mapa de la cancha).
   - Una pelota (sabemos dónde está por cámaras).
   → Es un rival probablemente.

2. **Cámara ve un blob no identificado** (no naranja de pelota, no cyan/magenta de arco). Si el tamaño es consistente con un robot (15-22 cm cuadrado a 50 cm de distancia), es un rival.

**Confianza típica para rivales**: 30-50. Es **estimación**, no certeza. CENTRAL debe usar esto solo para evasión gruesa, no para estrategia fina.

**Tracking simple** (Nivel 3): mantener 2 "tracks" de rivales con posición + último tiempo visto. Si un rival no se ve por > 1 segundo, decae su confianza a 0 y se elimina del snapshot.

---

## 11. Comunicación con partner

### 11.1 Stack de comunicación

```
   Robot 1 (yo)                          Robot 2 (partner)
   ┌──────────────┐                      ┌──────────────┐
   │   ARRIBA     │                      │   ARRIBA     │
   │  Teensy 4.0  │                      │  Teensy 4.0  │
   └──────┬───────┘                      └──────┬───────┘
          │ UART (Serial2, 7/8)                 │ UART (Serial2, 7/8)
          │ a 115200 baud                       │
          ▼                                     ▼
   ┌──────────────┐  ESP-NOW (2.4 GHz)   ┌──────────────┐
   │   COMM       │═══════════════════════│   COMM       │
   │  ESP32-C6    │  ~1-5 ms latencia      │  ESP32-C6    │
   └──────────────┘                       └──────────────┘
```

### 11.2 ESP-NOW características

| Parámetro | Valor |
|-----------|-------|
| Latencia típica | 1-5 ms en condiciones limpias |
| Alcance | ~50 m línea directa, ~10 m con obstáculos |
| Bandwidth | hasta 250 Kbps |
| Frecuencia | 2.4 GHz (puede tener interferencia de WiFi del estadio) |
| Tamaño máximo de packet | 250 bytes |
| Sin handshake / sin retry | UDP-like, packets se pueden perder |

### 11.3 Frecuencia de envío

**10 Hz** (cada 100 ms). Justificación:
- Suficiente para coordinación táctica (¿quién va a la pelota?).
- No satura el canal — deja espacio para árbitros u otros tráficos.
- A 16 bytes por packet × 10 Hz × 2 robots = 320 bytes/s. Trivial.

### 11.4 Payload del snapshot del partner

```cpp
struct PartnerSnapshot {
    int16_t x_mm;                    // pose en cancha
    int16_t y_mm;
    int16_t heading_centideg;
    uint8_t pose_confidence;
    int16_t ball_x_mm;               // pelota detectada
    int16_t ball_y_mm;
    uint8_t ball_visible;
    uint8_t ball_confidence;
    uint8_t state;                   // estado FSM (búsqueda, persiguiendo, defendiendo)
    uint8_t intent;                  // qué planea hacer (atacar, defender, esperar)
    uint8_t reserved[3];
};  // 16 bytes
```

### 11.5 Fusión con datos del partner

Cuando llega un `PartnerSnapshot`:

```cpp
void integrate_partner_data(const PartnerSnapshot& p) {
    // 1. Actualizar pose del compañero (siempre)
    g_partner.x = p.x_mm;
    g_partner.y = p.y_mm;
    g_partner.heading = p.heading_centideg / 100.0f;
    g_partner.last_update_ms = millis();
    g_partner.alive = true;

    // 2. Si nosotros no vemos la pelota pero el partner SÍ con alta confianza,
    //    adoptar SU observación.
    if (!g_ball.visible && p.ball_visible && p.ball_confidence > 60) {
        // La posición de la pelota del partner es relativa a la cancha
        // (no a su robot). Coordenadas absolutas se mantienen.
        g_ball.x = p.ball_x_mm;
        g_ball.y = p.ball_y_mm;
        g_ball.confidence = p.ball_confidence * 0.7f;  // descuento por ser dato indirecto
        g_ball.visible_in_my_snapshot = false;          // marcado como "via partner"
    }

    // 3. Decisión de roles: si ambos vamos a la pelota, el más cercano va.
    //    Se decide en CENTRAL con esta info ya digerida.
}
```

### 11.6 Watchdog del partner

Si no llega `PartnerSnapshot` en > 500 ms, marcar `partner_alive = false`. CENTRAL puede decidir jugar solo sin coordinación.

---

## 12. Integración de información de ABAJO

ABAJO envía:
- `DOWN_OTOS_POSE` y `DOWN_OTOS_VEL` cada 10 ms → pose odométrica del robot.
- `DOWN_LINE_STATUS` (legacy, no debería llegar — la línea va por bus emergencia directo a CENTRAL).

ARRIBA usa la pose odométrica como **una observación más** en su EKF:
- OTOS tiene **drift acumulado** (~1-2% del recorrido). En un partido de 10 minutos con ~100 m de recorrido total, el drift es ~1-2 m. Inaceptable como única fuente.
- Pero como **observación dentro del EKF combinada con ToF + cámara**, es muy útil:
  - ToF da posición absoluta pero a 30 Hz.
  - OTOS da posición acumulada a 100 Hz.
  - El EKF las combina: OTOS suaviza entre ticks de ToF, ToF corrige el drift de OTOS.

**Análisis del slip estimate (de ABAJO)**: si `slip > 50 mm/s`, ARRIBA aumenta el ruido R de la observación OTOS en el EKF (confía menos durante ese intervalo).

---

## 13. Comunicaciones UART

### 13.1 Stream principal: WORLD_SNAPSHOT → CENTRAL (100 Hz, Serial4 RX 16 / TX 17)

Cada 10 ms, ARRIBA arma el snapshot completo y lo envía. Estructura del payload:

```cpp
struct WorldSnapshot {
    // Pose propia
    int16_t my_x_mm;
    int16_t my_y_mm;
    int16_t my_heading_centideg;
    uint8_t my_pose_confidence;

    // Pelota
    int16_t ball_x_mm;
    int16_t ball_y_mm;
    uint8_t ball_visible;
    uint8_t ball_confidence;

    // Arco rival visible
    int16_t goal_opp_angle_centideg;
    int16_t goal_opp_distance_mm;
    uint8_t goal_opp_visible;
    uint8_t goal_own_visible;

    // Obstáculo mínimo
    uint16_t min_obstacle_mm;

    // Árbitro + flags
    uint8_t referee_cmd;     // 0=stop, 1=start, 2=halftime, 3=reset
    uint8_t flags;           // bits: match_running, in_own_pen_area, partner_alive, etc.
} __attribute__((packed));   // 24 bytes
```

**Total con overhead de protocolo**: 24 + 7 = **31 bytes/frame**. A 100 Hz = 3100 bytes/s = 1.3% del baud 230400. Holgado.

**Nota**: la pose del compañero, los rivales y la velocidad de la pelota NO están en el snapshot actual. Para Nivel 3+ se agregará un `WORLD_SNAPSHOT_EXTENDED` con esos datos.

### 13.2 Streams secundarios

- **Recepción desde ABAJO** (Serial1): `DOWN_OTOS_POSE/VEL` a 100 Hz para fusión EKF.
- **Recepción desde COMM** (Serial2, 7/8): `COMM_REFEREE_CMD`, `COMM_PARTNER_DATA`, `COMM_STATUS_REQ` (eventos).
- **Envío a COMM** (Serial2, 7/8): `TOP_PARTNER_DATA` a 10 Hz, `TOP_STATUS_REPLY` a demanda.
- **Recepción desde CENTRAL** (Serial4, 16/17): `CENTRAL_RESET_TOP`, `CENTRAL_TOP_CMD` (eventos).

### 13.3 Heartbeat

**Mismo principio que ABAJO**: NO hay heartbeat explícito. El stream continuo de `WORLD_SNAPSHOT` a 100 Hz es el heartbeat implícito. CENTRAL detecta caída con timeout de 500 ms.

---

## 14. Detección de fallos y recovery

| Subsistema | Detección | Recovery |
|------------|-----------|----------|
| 1 BNO055 | `getCalibration()=0` por > 5s O timeout I2C | Modo DEGRADED_IMU_SINGLE. Usa el otro |
| Ambos BNO055 | Ambos timeout o calibración 0 | Modo `LOST` para pose θ. Confianza heading = 0 |
| 1 ToF | Timeout I2C o readiness check falla | Excluir ese cuadrante del cálculo de pose |
| Todos los ToF | Todos fallan | Modo DEGRADED_NO_TOF. Pose XY solo por OTOS + cámara |
| 1 cámara | No llegan packets por > 1s | Solo usar la otra. Cobertura reducida |
| Ambas cámaras | No llegan packets por > 1s | Modo DEGRADED_NO_CAMERAS. Pelota solo por predicción |
| UART hacia CENTRAL | No se puede TX (raro) | LED parpadea, intentar reiniciar Serial |
| UART desde ABAJO | Timeout 500 ms | Confianza pose baja, sigue con cámara+ToF+IMU |
| UART desde COMM | Timeout 500 ms | Marcar partner_alive=false, sin árbitros |

---

## 15. Timing y latencias

### 15.1 Loop principal

```
loop():
    # RX (cada loop, no bloquea)
    comm_down_tick()        # ~50 µs (drena Serial1)
    comm_arbiter_tick()     # ~50 µs (drena Serial2, COMM 7/8)
    comm_central_tick()     # ~50 µs (drena Serial4, CENTRAL 16/17)
    cameras_tick()          # ~100 µs (parsea Serial3 + Serial5)

    # Sensores periódicos
    if since_imu_tick >= 10 ms:
        imu_dual_tick()         # ~600 µs (2 I2C lecturas paralelas + consistency)

    if since_tof_tick >= 33 ms:    # 30 Hz
        tof_quad_tick()         # ~12 ms (4 lecturas I2C en 1 bus Wire, recableado 2026-05-30)

    # Fusión
    if since_ekf_tick >= 10 ms:
        ekf_predict_and_update()    # ~1 ms

    # Send
    if since_snapshot_send >= 10 ms:
        build_and_send_snapshot()    # ~200 µs

    # Partner @ 10 Hz
    if since_partner_send >= 100 ms:
        send_partner_data_to_comm()  # ~150 µs
```

**Loop budget**: ~5 ms en peor caso (cuando coinciden EKF + ToF + send). A 100 Hz (10 ms entre snapshots) hay margen.

### 15.2 Latencia camera → snapshot

| Etapa | Tiempo |
|-------|--------|
| Pelota cruza FOV de cámara | 0 ms |
| OpenMV procesa frame y blob | ~30 ms |
| UART transmite 9 bytes a 19200 baud | ~5 ms |
| `cameras_tick()` parsea | < 1 ms |
| Fusión con la otra cámara | < 1 ms |
| EKF update + build snapshot | ~2 ms |
| TX a CENTRAL | ~1.5 ms |
| **Total** | **~40 ms** |

Esto es **lo mejor alcanzable con el protocolo OpenMV viejo**. Migración a baud 230400 (no migrar protocolo) lo bajaría a ~32 ms. Migración a protocolo nuevo + 115200 baud lo bajaría a ~25 ms.

### 15.3 Latencia ToF → snapshot

| Etapa | Tiempo |
|-------|--------|
| ToF mide (interno) | ~16 ms (60 Hz interno) |
| Próximo tick de `tof_quad_tick()` (peor caso) | < 33 ms |
| I2C transferencia | ~3 ms × 4 ToF en 1 bus `Wire` (recableado 2026-05-30) |
| Trilateración + EKF | ~1 ms |
| Snapshot + TX | ~3 ms |
| **Total peor caso** | **~55 ms** |

---

## 16. Estructuras de datos enviadas

Ya cubiertas en §13.1 (`WorldSnapshot`) y §11.4 (`PartnerSnapshot`).

Para el Nivel 4 (Mundial 2027) habrá `WorldSnapshotExtended` con:
- Pose y velocidad del compañero.
- Hasta 2 rivales con pose estimada.
- Velocidad y dirección de la pelota.
- Histórico de confidence por entidad.

Tamaño estimado: 40-50 bytes. Cabe en el payload máximo del protocolo (32 bytes), así que para extender hay que partir en 2 frames `WORLD_SNAPSHOT_A` y `WORLD_SNAPSHOT_B`.

---

## 17. Diagnóstico y debug

### 17.1 LED de estado

| Patrón | Significado |
|--------|-------------|
| Apagado | Boot incompleto |
| Encendido fijo | NORMAL — todo OK |
| Parpadeo lento (1 Hz) | 1 sensor caído (IMU, ToF, cámara), recuperable |
| Parpadeo medio (2 Hz) | Modo DEGRADED — varias fuentes caídas |
| Parpadeo rápido (5 Hz) | Modo LOST — sin pose ni pelota |
| 3 parpadeos + pausa | CALIBRATING_IMU |

### 17.2 USB Serial

Imprime cada 1 segundo en NORMAL:
- Pose actual `(x, y, θ)` + confianza.
- Pelota visible o predicha + confianza + tiempo desde última observación.
- Conteo de frames TX/RX por cada UART.
- Status de cada BNO055 (calibration 0-3, raw heading).
- Status de cada ToF (frames OK / errores).
- Status del partner (alive / heading recibido).

---

## 18. Tabla resumen

| Aspecto | Valor |
|---------|-------|
| MCU | Teensy 4.0 a 600 MHz |
| IMUs | 2 × BNO055 (modo IMUPLUS) en I2C dual |
| Frecuencia IMU | 100 Hz |
| Consistency check IMU | Cada tick, ±2° tolerancia |
| Estrategia reseteo IMU | Solo eventos puntuales + corrección suave por visión |
| ToF | 4 × VL53L7CX en modo 4×4 SPAD |
| Frecuencia ToF | 30 Hz por sensor |
| Cobertura ToF | 360° (FOV 90° × 4 sensores cardinales) |
| Cámaras | 2 × OpenMV N6 (antes H7 Plus) |
| Frecuencia cámaras | ~30 Hz |
| Protocolo cámaras | OpenMV viejo (9 bytes/packet) |
| Fusión sensorial | EKF 6D (x, y, θ, vx, vy, ω) a 100 Hz |
| Predicción pelota | Kalman 4D (x, y, vx, vy) con decay |
| Confidence pose objetivo | > 70 en condiciones normales |
| Comm partner | ESP-NOW vía COMM (ESP32-C6) a 10 Hz |
| UART hacia CENTRAL | Serial4 (RX 16 / TX 17), 230400 baud, 100 Hz (fix 2026-06-02: NO Serial7 — el Teensy 4.0 no lo expone en el borde) |
| Latencia camera → snapshot | ~40 ms |
| Latencia ToF → snapshot | ~55 ms peor caso |
| Carga CPU estimada | ~45% (con Nivel 3 completo) |
| Heartbeat explícito | No (stream continuo lo reemplaza) |

---

## 19. Plan de trabajo factible

El firmware completo es ambicioso. Lo factible depende del tiempo disponible y de la madurez de las placas. Plan progresivo:

### Nivel 1 (Incheon MÍNIMO) — 1 semana

Solo lo crítico para tener un robot que reaccione a la pelota.

- IMU dual con consistency check básico (sin EKF — solo promedio).
- 1 cámara funcionando con parser viejo OpenMV.
- Pelota: detección sin predicción (si no es visible, `ball_visible=0`).
- Pose: solo heading (sin x, y absoluto).
- WorldSnapshot básico → CENTRAL.
- Sin partner comm.
- Sin ToF (esperan llegar al equipo).

**Suficiente para**: el robot ve la pelota, gira hacia ella, ataca. Es lo que hicimos en 2025 pero con arquitectura distribuida.

### Nivel 2 (Incheon IDEAL) — 2 semanas

Si hay tiempo y placas listas:

- Nivel 1 + segunda cámara fusionada.
- ToF cuando llegan (3-4 sensores configurados, trilateración simple).
- Pose XY estimada cuando se ven arcos (sin EKF aún).
- Línea recibida de ABAJO integrada.
- Comm con árbitros funcionando (start/stop).

**Suficiente para**: robot juega 2v2 con coordinación básica de "quién va a la pelota" (decidida en CENTRAL por proximidad).

### Nivel 3 (Roboliga Nov 2026) — 4-6 semanas post-Incheon

- EKF completo con todas las fuentes (IMU + ToF + cámaras + OTOS).
- Predicción Kalman de pelota cuando se pierde de vista.
- Confidence per-entity en el WorldSnapshot.
- Partner comm vía ESP-NOW + fusión con datos del partner.
- Recalibración suave del IMU por visión.

**Suficiente para**: defender título nacional argentino.

### Nivel 4 (Mundial 2027) — 4-8 semanas post-Roboliga

- Estimación de rivales con tracking en el tiempo.
- WorldSnapshot extendido con velocidad de la pelota y movimiento de todos los robots.
- Coordinación táctica entre partners (pase, marcaje, formaciones).
- Calibración avanzada de cámaras (recalibrar offset por iluminación).
- Detección automática de "el rival va a patear" para anticipar.

**Suficiente para**: pelear top 5 mundial.

### Nivel 5 (Largo plazo, 2027+)

- Redes neuronales en cámaras (reconocer rivales por silueta vs blobs simples).
- SLAM completo con mapa actualizado dinámicamente.
- Estrategia táctica adaptativa al rival (aprender en tiempo real).

---

### Factibilidad por nivel

| Nivel | ¿Factible para Incheon junio 2026? | Riesgo |
|-------|------------------------------------|--------|
| 1 | **Sí**, con 1 semana de trabajo dedicado | Bajo |
| 2 | Sí si los ToF llegan a tiempo y se prueban en lab | Medio |
| 3 | **No** para Incheon (mucho EKF + tuning). Para Nacional Nov 2026 sí | Alto si se intenta apurar |
| 4 | Mundial 2027 | OK con tiempo |
| 5 | Investigación futura | OK |

**Recomendación coach**: apuntar a Nivel 2 para Incheon. Nivel 3 después con calma para Nacional Nov 2026. No intentar Nivel 4 antes de Mundial 2027 — es donde la diferencia entre top 10 y top 5 se decide.

---

## 20. Lo que NO hace (límite de scope)

- **Control de motores**: vive en CENTRAL.
- **FSM táctica**: vive en CENTRAL.
- **PIDs**: viven en CENTRAL.
- **Cinemática inversa omni-3**: vive en CENTRAL.
- **Procesamiento de imagen avanzado**: vive **dentro de las OpenMV**. ARRIBA solo recibe blobs ya detectados.
- **Comunicación con árbitros del torneo (protocolo oficial)**: implementado en la placa COMM (ESP32-C6). ARRIBA solo recibe comandos pre-decodificados.
- **Almacenamiento persistente**: no hay flash de usuario en Teensy 4.0 (solo Teensy 4.1). Para guardar calibración, usar EEPROM emulada limitada o agregar SD en otra placa.

---

## 21. Referencias

- Arquitectura general: [`docs/ARQUITECTURA-3-PLACAS-2026.md`](../ARQUITECTURA-3-PLACAS-2026.md)
- Firmware placa hermana ABAJO: [`docs/firmware/FIRMWARE-PLACA-ABAJO.md`](FIRMWARE-PLACA-ABAJO.md)
- Pinout de la placa ARRIBA: [`hardware/electronics/mapa-pines-placas-nuevas.md`](../../hardware/electronics/mapa-pines-placas-nuevas.md)
- Sistema posicionamiento (versión marzo 2026): [`docs/internal/sistema-posicionamiento-y-comunicacion.md`](../internal/sistema-posicionamiento-y-comunicacion.md)
- Multi-camera world model: [`docs/multi-camera-world-model.md`](../multi-camera-world-model.md)
- LiDAR / ToF analysis: [`docs/lidar-tof-slam-analysis.md`](../lidar-tof-slam-analysis.md)
- Análisis técnico BNO055: [`docs/internal/giroscopo-bno055-analisis-tecnico.md`](../internal/giroscopo-bno055-analisis-tecnico.md)
- Protocolo UART: `src/shared/proto.h`
- Tipos compartidos: `src/shared/types.h`
- Implementación actual: `src/top/`
- BNO055 datasheet: https://cdn-shop.adafruit.com/datasheets/BST_BNO055_DS000_12.pdf
- VL53L7CX datasheet: https://www.st.com/resource/en/datasheet/vl53l7cx.pdf
- VL53L5CX datasheet: https://www.st.com/resource/en/datasheet/vl53l5cx.pdf
- ESP-NOW reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
- Probabilistic Robotics (Thrun, Burgard, Fox) — referencia académica del EKF aplicado a robótica móvil.

---

*Documento mantenido por IITA — Instituto de Informática y Tecnología Aplicada, Salta, Argentina.*
