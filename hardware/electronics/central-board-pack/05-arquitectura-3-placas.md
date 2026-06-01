---
title: "Arquitectura distribuida de 3 placas — Robot 2026"
date: 2026-05-11
status: propuesta
audience: equipo IITA Soccer Open (Director, Coach, Competidores)
tags: [arquitectura, hardware, software, top-board, down-board, central, distribuida]
---

# Arquitectura del Robot 2026 — 3 Placas Distribuidas

> Propuesta de arquitectura para el robot IITA Soccer Open 2026.
> Define qué hace cada placa, cómo se comunican y por qué se diseña así.

---

## Resumen ejecutivo

El robot 2026 distribuye su inteligencia en **3 placas especializadas** conectadas por UART:

```
┌────────────────────────┐         ┌────────────────────────┐         ┌────────────────────────┐
│   PLACA ARRIBA         │  UART   │   PLACA CENTRAL        │  UART   │   PLACA ABAJO          │
│   Teensy 4.0           │ ───────►│   Teensy 4.1 (Zircon)  │◄─────── │   Teensy 4.0           │
│                        │ snapshot│                        │  línea  │                        │
│   CEREBRO SENSORIAL    │  100 Hz │   CEREBRO DECISOR      │  100 Hz │   SENSOR DE PISO       │
│   "Veo el mundo"       │         │   "Decido qué hacer"   │         │   "Toco el suelo"      │
├────────────────────────┤         ├────────────────────────┤         ├────────────────────────┤
│ • 2 cámaras OpenMV     │         │ • FSM principal        │         │ • 32 sensores luz      │
│ • 2 IMU BNO055         │         │ • Motores 3-omni + PID │         │ • 2 OTOS odométricos   │
│ • 4 ToF + 1 ultrasonido│         │ • Kicker / dribbler    │         │ • Measurement línea    │
│ • Comm árbitros RCJ    │         │ • Coordinación partner │         │ • Detección bordes     │
│ • Fusión sensor → pose │         │ • Watchdog global      │         │                        │
│   (x, y, heading, ball)│         │                        │         │                        │
└────────────────────────┘         └────────────────────────┘         └────────────────────────┘
                                              │                              │
                                              │   bus de emergencia          │
                                              ◄──────UART (línea urgente)────┘
```

**Las 3 placas son especialistas, ninguna es generalista**:
- **ARRIBA** percibe el mundo (cámaras + IMU + ToF + comm árbitros) y entrega un *world snapshot* pre-procesado.
- **CENTRAL** decide qué hacer (FSM táctica), corre todos los lazos de control (PIDs) y mueve los motores. Es el master del robot.
- **ABAJO** es un sensor puro: detecta línea + odometría y entrega measurements al CENTRAL, sin lógica de control.

Esta separación permite que cada MCU corra a < 30% de CPU y deja margen para mejoras futuras (Kalman, partner coordination, estrategia avanzada) sin saturar ningún procesador.

---

## Por qué esta arquitectura

Cinco principios de diseño justifican la elección:

**1. Procesa donde está el sensor, decide en el centro.** Es la regla estándar de robótica móvil (CAMBADA en RoboCup Middle Size League, PCBWay en RoboCup Junior 2022/2024). Pre-procesar localmente reduce ancho de banda UART y permite que cada placa aproveche al máximo sus periféricos sin pelearse con tráfico de otras placas.

**2. Latencia de seguridad < 15 ms.** El robot a 1 m/s recorre 15 mm en 15 ms. Si la detección "estoy saliendo de la cancha" pasa por dos UARTs antes de frenar, el robot ya cruzó la línea. Por eso ABAJO tiene **bus directo de emergencia con CENTRAL** además de la ruta normal por ARRIBA.

**3. Master único, slaves especializadas.** CENTRAL es el único que toma decisiones tácticas. ARRIBA y ABAJO entregan datos pre-procesados. Esto evita decisiones contradictorias entre placas y simplifica el modelo mental.

**4. Continuidad con lo que funciona.** CENTRAL es el Zircon Rev v15 con Teensy 4.1 — la placa que ganó el Nacional 2025. Las placas ARRIBA y ABAJO se suman como pre-procesadores, no la reemplazan. Si una placa nueva falla en Incheon, CENTRAL puede degradarse a modo monolítico (sensores básicos + estrategia simple).

**5. Capitalizable post-Incheon.** Cada placa tiene un dominio claro y un protocolo de comunicación bien definido. En 2027 se puede reemplazar una sin tocar las otras (mejor cámara → solo cambia firmware de ARRIBA; mejor sensor de línea → solo cambia ABAJO).

---

## PLACA CENTRAL — Cerebro decisor

**Hardware**: Teensy 4.1 montado sobre PCB Zircon Rev v15.
**Rol**: Master del robot. Decide qué hacer y lo ejecuta.

### Responsabilidades

| Responsabilidad | Detalle |
|-----------------|---------|
| Máquina de estados táctica | FSM principal (delantero / arquero según dipswitch). Estados: SEARCH, APPROACH, POSITION, PUSH, GOALKEEPER_PATROL, INTERCEPT, KICK, LINE_AVOID. |
| Control de motores | Cinemática inversa omni-3. Aplica PWM directo a los 3 H-bridges del Zircon. |
| Lazos de control (PIDs) | **Todos los PIDs del robot corren acá**: PID de heading (consume IMU desde ARRIBA), PID lateral del arquero (consume measurement de línea desde ABAJO, solo en modo arquero), PID de approach a la pelota. Un único lugar con todas las ganancias tuneables. |
| Watchdog global | Si ARRIBA timeout 500 ms → modo seguro (parar motores, parpadear LED). Si ABAJO timeout 500 ms → estrategia ciega (sin información de línea). |
| Coordinación táctica | Aplica reglas de partner (recibe partner data desde ARRIBA via comm árbitros) y decide quién va a la pelota. |
| Kicker y dribbler | Activa solenoide kicker cuando la pelota está alineada con el arco rival (solo delantero). |
| Modo "match running" | Solo mueve motores cuando recibió START del árbitro (también via ARRIBA). |

### Lo que NO hace

- No procesa imagen de cámaras (lo hace ARRIBA).
- No lee directamente 32 sensores de línea (lo hace ABAJO).
- No corre Kalman ni filtros de fusión (lo hace ARRIBA).
- No comunica directamente con el módulo de árbitros (lo hace ARRIBA via placa COMM).

### Inputs

- **UART desde ARRIBA**: `WORLD_SNAPSHOT` cada 10 ms con pose propia (x, y, heading), pelota (x, y, visible), arcos, obstáculos, comando del árbitro, datos del partner.
- **UART desde ABAJO** (canal de emergencia): `LINE_URGENT` cada 5-10 ms con measurement de línea (ángulo + profundidad signed) + flag `imminent_exit`. Sin lógica de control — sólo medición física cruda.

### Outputs

- **PWM a los 3 motores omni** (directo, sin UART).
- **GPIO al solenoide kicker** y dribbler PWM (solo delantero).
- **UART hacia ARRIBA**: comandos como recalibrar cámaras, resetear pose.
- **UART hacia ABAJO**: comandos administrativos (calibrar umbrales de línea, reset OTOS). ABAJO no necesita conocer el modo del robot — sólo reporta lo que ve.

### Carga estimada

| Tarea | CPU |
|-------|-----|
| FSM 100 Hz | 5% |
| Cinemática inversa + PIDs (heading + lateral arquero + approach) | 10% |
| Decode UART × 2 streams 100 Hz | 5% |
| **Total** | **~20%** |

El Teensy 4.1 (Cortex-M7 a 600 MHz) tiene mucha capacidad libre para estrategia avanzada futura.

---

## PLACA ARRIBA — Cerebro sensorial

**Hardware**: Teensy 4.0 montado sobre placa "Roboliga2026 TOP".
**Rol**: Percibe el mundo y entrega un *world snapshot* pre-procesado al CENTRAL.

### Responsabilidades

| Responsabilidad | Detalle |
|-----------------|---------|
| Visión multi-cámara | Procesa 2 OpenMV H7/H7+ via UART. Cada cámara reporta blobs (pelota, arco propio, arco rival). ARRIBA fusiona ambas vistas. |
| IMU dual (heading absoluto) | 2 BNO055 en buses I2C separados (Wire bus 0 + Wire1 bus 1 remapeado a pines 24/25). Modo IMUPLUS para evitar interferencia magnética de motores. Si uno falla, sigue el otro. |
| Obstáculos cercanos | 4 sensores ToF VL53L7CX (2 en cada bus I2C) + 1 HC-SR04 frontal. Reporta distancia mínima en cada cuadrante. |
| Comunicación con árbitros | Bridge UART hacia placa COMM (ESP32-C6) que implementa el protocolo oficial RCJ Communication Module y reporta start/stop/halftime al ARRIBA. |
| Comunicación con partner | ESP-NOW transparente vía placa COMM. Recibe pose y pelota del robot compañero, lo agrega al world snapshot. |
| Fusión sensorial → pose | Calcula pose propia (x, y, heading) combinando IMU + odometría OTOS (recibida desde ABAJO) + visión de arcos cuando son visibles. |
| Detección de área chica | Cuando la pose estimada cae dentro del rectángulo del área chica propia, reporta el flag al CENTRAL. |

### Lo que NO hace

- No controla motores.
- No toma decisiones tácticas (no decide si atacar, defender, etc.).
- No lee sensores de línea.

### Inputs

- 2 OpenMV cámaras (UART Serial3 + Serial5).
- 2 BNO055 (I2C Wire + Wire1).
- 4 ToF VL53L7CX (I2C, repartidos en 2 buses).
- 1 HC-SR04 ultrasonido (GPIO TRIG/ECHO).
- Placa COMM (UART Serial4) — comandos árbitros + datos partner.
- ABAJO (UART) — odometría OTOS para fusión.

### Outputs

- **CENTRAL (UART Serial7)**: `WORLD_SNAPSHOT` 100 Hz con todo lo percibido.
- **Placa COMM (UART Serial4)**: status del robot + datos a enviar al partner.
- **ABAJO (UART)**: comandos administrativos (reset, calibración).

### Carga estimada

| Tarea | CPU |
|-------|-----|
| 100 Hz IMU dual | 5% |
| ~30 Hz ToF × 4 | 10% |
| Parser cámaras (2 × 19200 baud) | < 1% |
| Fusión sensorial | 5% |
| Comm árbitros + partner | 3% |
| **Total** | **~25%** |

Margen amplio para integrar EKF de pose o filtros Kalman de pelota en 2027.

---

## PLACA ABAJO — Sensor de piso

**Hardware**: Teensy 4.0 montado sobre placa "Roboliga 2026 Futbol" (base).
**Rol**: Detecta el suelo (líneas + movimiento) y reporta al CENTRAL.

### Responsabilidades

| Responsabilidad | Detalle |
|-----------------|---------|
| Anillo de sensores de línea | 32 sensores ALS-PT19 con LEDs activos pareados, multiplexados via 4 chips CD4051 (cada mux con sus propios A/B/C = 12 pines SEL, 4 salidas analógicas a A0/A1/A8/A9 en paralelo, INH a GND). Lectura completa de los 32 sensores en ~80 µs. Ver `hardware/electronics/down-board-pack/01-pinout-y-posiciones.md`. |
| Cálculo del ángulo de línea | Algoritmo de centroide angular: cada sensor i está en posición física θ_i = i × 11.25°. Los sensores que ven blanco contribuyen un vector unitario en su ángulo. La línea detectada = atan2 de la suma. |
| Detección de "salida inminente" | Si N sensores adyacentes ven blanco simultáneamente, flag `imminent_exit` enviado por bus directo al CENTRAL para frenar en < 15 ms. |
| Odometría óptica | 2 sensores SparkFun OTOS montados a cada costado del robot (uno a la izquierda, uno a la derecha del centro). Lectura I2C en 2 buses separados (Wire + Wire1). |
| Análisis diferencial OTOS | Detecta slip lateral comparando velocidad de los dos OTOS. Si difieren mucho en X, hay patinazo (típicamente al patear o chocar). Reporta `slip_estimate` al CENTRAL como dato de calidad. |
| Pose odométrica local | Fusiona los 2 OTOS en una pose central (x, y, heading) que ARRIBA usa para su fusión sensorial completa. |
| Reporte de measurement | Entrega al CENTRAL la medición física cruda de la línea (ángulo + profundidad signed, en mm). Si el robot está pisando blanco, profundidad > 0; si está afuera del blanco, profundidad = 0. Es la *measurement* del control PID lateral — el PID en sí corre en CENTRAL. |

### Lo que NO hace

- No controla motores.
- No ejecuta lazos de control (PIDs). Es un sensor puro — entrega *measurements*, no *outputs*.
- No procesa imagen.
- No toma decisiones tácticas.
- No conoce el modo del robot (delantero / arquero / match running). CENTRAL decide qué measurement usar según contexto.
- No conoce la pose absoluta del robot en la cancha (eso lo calcula ARRIBA con cámaras).

### Inputs

- 32 sensores ALS-PT19 via 4 muxes CD4051 (analógico).
- 2 SparkFun OTOS (I2C dual).
- CENTRAL (UART) — comandos administrativos: calibrar línea, resetear OTOS.

### Outputs

- **ARRIBA (UART)**: odometría OTOS para que ARRIBA pueda fusionar con sus sensores.
- **CENTRAL (UART, canal de emergencia)**: `LINE_URGENT` 100-200 Hz con ángulo línea, profundidad signed (mm), flag `imminent_exit`. CENTRAL usa la profundidad como measurement de su PID lateral cuando está en modo arquero.

### Carga estimada

| Tarea | CPU |
|-------|-----|
| 1 kHz lectura 32 sensores via mux | 15% |
| 100 Hz OTOS dual + fusión odométrica | 5% |
| 2 UARTs send | 2% |
| **Total** | **~22%** |

Suficiente margen para subir el polling a 2 kHz si hace falta más resolución temporal en la detección de línea.

---

## Comunicación entre placas

### Topología

```
                       ARRIBA  ←─UART administrativa─→  CENTRAL
                          ▲
                          │ UART 100 Hz: odometría OTOS
                          │ (para fusión sensorial completa)
                          │
                        ABAJO  ──UART 100-200 Hz───►  CENTRAL
                                  bus de emergencia
                          measurement línea + imminent_exit
```

ARRIBA habla con CENTRAL (snapshot completo) y con COMM (árbitros).
ABAJO habla con ARRIBA (odometría) y con CENTRAL (emergencia).
CENTRAL es el único que controla motores.

### Protocolo de mensajes

Todos los mensajes usan el frame estándar definido en `src/shared/proto.h` con START (0xAA), CRC-16/CCITT, SEQ y END (0x55). La especificación completa está en `research/in-progress/2026-05-10-diseno-firmware-3-placas.md` §2.

| Mensaje | Sentido | Frecuencia | Contenido |
|---------|---------|------------|-----------|
| `WORLD_SNAPSHOT` | ARRIBA → CENTRAL | 100 Hz | Pose propia (x, y, heading, confianza), pelota (x, y, visible), arcos (ángulo, distancia), obstáculo mínimo, datos partner, comando árbitro, flag área chica |
| `LINE_URGENT` | ABAJO → CENTRAL | 100-200 Hz | Ángulo línea (centideg), profundidad signed (mm), flag `imminent_exit`. CENTRAL deriva el error de su PID lateral arquero a partir de la profundidad. |

### Asignación física de UARTs en cada placa

**Placa ABAJO (Teensy 4.0)** — 2 UARTs cableados según PCB 04-12 (verificado):
- **Serial1** (pines 0/1 del Teensy 4.0) — conector U11 → bus de emergencia hacia CENTRAL.
- **Serial5** (pines 21/20 del Teensy 4.0) — conector U10 "COMUNICATION" → odometría hacia ARRIBA.

Los otros 5 UARTs del Teensy 4.0 (Serial2, 3, 4, 6, 7) no están cableados en la placa DOWN. Suficiente para los 2 streams necesarios.

**Placa ARRIBA (Teensy 4.0)** — 4 UARTs cableados según PCB 04-12 (verificado):
- **Serial1** (pines 0/1) — conector U16 "UART_COMM_IN" → recibe odometría desde ABAJO.
- **Serial3** (pines 15/14) — conector U8 "UART-CAMERA1" → cámara 1.
- **Serial4** (pines 16/17) — conector U15 "UART_COMM_OUT" → placa COMM (árbitros + ESP-NOW).
- **Serial5** (pines 21/20) — conector U9 "UART-CAMERA2" → cámara 2.

El TOP envía `WORLD_SNAPSHOT` a CENTRAL por **Serial7** (pines 28/29). Serial2 (7/8) del TOP queda libre.

**Placa CENTRAL (Teensy 4.1, Zircon Rev v15)** — 8 UARTs hardware (reasignado 2026-05-31):
- **Serial7** (28/29) → recibe del ARRIBA (`WORLD_SNAPSHOT`).
- **Serial1** (0/1) → recibe del ABAJO (`LINE_URGENT`).
- **Serial2** (7/8) → LIBRE para el driver del motor 2 (U17). Conflicto F8/TASK-036 RESUELTO.
| `DOWN_ODOM` | ABAJO → ARRIBA | 100 Hz | Pose odométrica OTOS (x, y, heading), velocidad, slip |
| `MOTOR_COMMAND_*` | (interno CENTRAL) | 100 Hz | No UART — CENTRAL aplica directo |
| `RESET_OTOS` | CENTRAL → ABAJO | en eventos | Reset de pose odométrica |
| `CALIB_LINE` | CENTRAL → ABAJO | en eventos | Calibrar umbrales de línea (carpet/blanco) |
| `CALIB_CAMERAS` | CENTRAL → ARRIBA | en eventos | Recalibrar visión |

### Latencias objetivo

| Cadena de eventos | Tiempo |
|-------------------|--------|
| Sensor de línea ve blanco → motor frena | **< 15 ms** |
| Cámara ve pelota → motor gira hacia ella | < 50 ms |
| Árbitro presiona START → motores arrancan | < 30 ms |
| Tick PID lateral arquero | **< 15 ms** |

### Watchdogs

- CENTRAL espera `WORLD_SNAPSHOT` cada 10 ms. Si no llega en 500 ms → modo seguro (motores detenidos).
- CENTRAL espera `LINE_URGENT` cada 10 ms. Si no llega en 500 ms → estrategia ciega de línea (asume no estar en el borde).
- ARRIBA espera `DOWN_ODOM` cada 10 ms. Si no llega en 500 ms → fusión sin odometría (degradación a sólo cámaras + IMU).
- CENTRAL aplica un timeout de 200 ms al PID lateral arquero — si ABAJO deja de mandar measurements de línea, CENTRAL congela el output del PID y degrada a estrategia ciega.

---

## Decisiones de diseño justificadas

### ¿Por qué CENTRAL es Teensy 4.1 y no Teensy 4.0?

El Teensy 4.1 tiene más memoria, más UARTs y, sobre todo, **es el chip del Zircon Rev v15 que ganó el Nacional 2025**. Mantener CENTRAL como Teensy 4.1 preserva el código probado y el cableado físico de motores. Las placas nuevas se montan alrededor del Zircon, no lo reemplazan.

### ¿Por qué dos IMU BNO055 en ARRIBA?

El BNO055 puede sufrir saltos de heading por:
- Impactos al chocar con otro robot.
- Interferencia magnética de los motores DC (incluso en modo IMUPLUS, hay vibración acoplada al acelerómetro).

Con dos unidades en posiciones físicas diferentes, ARRIBA puede:
- Detectar discrepancias (un IMU saltó, el otro no).
- Usar la lectura más estable o promediar.
- Continuar operando si uno falla.

Costo: dos chips de ~$15 USD cada uno. Beneficio: confiabilidad muy superior.

### ¿Por qué dos OTOS en ABAJO?

Los SparkFun OTOS son sensores ópticos de odometría — leen el movimiento del suelo como un mouse óptico de alta gama. Un solo OTOS en el centro daría pose, pero no detecta slip lateral.

Con dos OTOS a izquierda y derecha del centro, ABAJO puede:
- Calcular slip al patear (cuando una rueda patina, los OTOS reportan velocidades distintas).
- Inferir rotación instantánea por la diferencia de los desplazamientos Y.
- Redundancia: si un OTOS pasa por encima de una línea blanca (que distorsiona la lectura óptica), el otro sigue dando dato válido.

### ¿Por qué bus de emergencia ABAJO → CENTRAL?

A 1 m/s, un robot recorre 1 mm cada milisegundo. Si la única ruta de "voy a salir de la cancha" pasa por ABAJO → ARRIBA → CENTRAL (dos UARTs en serie, ~25 ms de latencia), el robot ya cruzó 25 mm más allá del borde antes de reaccionar.

El bus directo ABAJO → CENTRAL deja esa cadena en un solo hop UART (~10 ms) y le da al CENTRAL margen para frenar dentro de los primeros 15 mm post-detección.

### ¿Por qué todos los PIDs corren en CENTRAL y no en las placas slaves?

El cálculo PID en sí es trivial (~1 µs por tick). El cuello de botella de latencia es el UART entre placas (~700 µs para enviar 16 bytes a 230400 baud), que se atraviesa con o sin PID local. Pasar el `error` por UART o pasar el `output_PID` cuesta lo mismo en términos de tiempo.

Entonces no hay ventaja de latencia en distribuir PIDs, y sí hay desventajas claras al hacerlo:

- **Estado distribuido**: cada PID acumula estado (integral, error previo). Si ese estado vive en tres placas distintas, debuggear "por qué el robot oscila" implica conectarse a tres MCUs distintos.
- **Ganancias dispersas**: tunear Kp/Ki/Kd implica reflashear la placa donde corre el PID. Si todos viven en CENTRAL, una sola compilación.
- **Combinar PIDs es trivial en CENTRAL**: el PID de heading y el PID lateral del arquero se pueden sumar y aplicar a la misma cinemática inversa omni-3. Si están en placas distintas, sincronizarlos es complicado.
- **ABAJO como sensor puro es testeable**: sin estado de control, ABAJO se puede testear con datos sintéticos sin simular el robot completo.

Por eso ABAJO entrega *measurements* (profundidad signed en mm, ángulo de línea) y CENTRAL calcula `error = setpoint - measurement` y aplica el PID. Lo mismo aplica al PID de heading (BNO055 en ARRIBA entrega `heading_deg`, CENTRAL aplica el PID) y a cualquier otro lazo de control futuro.

### ¿Por qué la fusión sensorial corre en ARRIBA y no en CENTRAL?

ARRIBA tiene físicamente todos los sensores de percepción ambiental (cámaras, IMU, ToF) en el mismo MCU. Llevar los datos crudos al CENTRAL para fusionarlos allá consumiría ~10 KB/s extra de UART para nada — ARRIBA puede hacer la fusión local y mandar al CENTRAL un snapshot de ~30 bytes con la pose ya cocida.

CENTRAL queda libre para hacer estrategia táctica avanzada (Kalman de pelota, predicción, coordinación con partner) sin pelear por ciclos con la fusión sensorial.

### ¿Por qué la placa COMM (árbitros) la maneja ARRIBA y no CENTRAL?

Tres razones:
1. ARRIBA ya tiene 4 UARTs disponibles libres y CENTRAL no tanto.
2. El comando del árbitro (start/stop/halftime) viaja naturalmente con el resto del world snapshot — es un input perceptual más.
3. El ESP-NOW partner es lógicamente parte de "lo que sé del mundo", como las cámaras y los ToF. Va con la percepción.

CENTRAL recibe el comando como flag dentro del snapshot, sin tener que parsear nada de la placa COMM.

---

## Plan de desarrollo progresivo

La arquitectura completa se puede construir incrementalmente. Cada nivel añade capacidad sin requerir que el siguiente esté listo.

| Nivel | Capacidad | Estado |
|-------|-----------|--------|
| 1 (Incheon mínimo) | CENTRAL + cámara frontal + IMU + 3 sensores de línea (sin placa ABAJO) + comm árbitros | Mantiene capacidad del 2025 |
| 2 (Incheon ideal) | Nivel 1 + 1 OTOS + 32 sensores de línea via ABAJO + PID lateral arquero (corriendo en CENTRAL con measurement de ABAJO) | Si ABAJO está lista |
| 3 (Incheon + ) | Nivel 2 + 2 cámaras + 2 IMU + 4 ToF via ARRIBA + fusión sensorial | Si ARRIBA está lista |
| 4 (Roboliga Nov) | Nivel 3 + EKF pose + Kalman pelota + coordinación partner via ESP-NOW | Post-Incheon |
| 5 (Mundial 2027) | Nivel 4 + estrategia avanzada (orbit, behind-the-ball, set plays) | Virginia como coach |

---

## Mapa de flujo de datos (referencia rápida para onboarding)

> **⚠️ ACTUALIZACIÓN 2026-05-19.** Este mapa fue armado el 2026-05-15 sobre
> WorldSnapshot **v1 = 24 bytes**. El contrato vigente desde 2026-05-18 es
> **v2 = 27 bytes** (+`ball_vx_mm_s` +`ball_vy_mm_s`), con `static_assert` en
> `types.h`. La tabla de abajo sigue siendo correcta en pines/baud/frecuencia,
> pero **el tamaño del struct WorldSnapshot dejó de ser 24 B**. Para el
> contrato byte-a-byte canónico ver **`docs/firmware/CONTRATO-DATOS-CENTRAL.md`**.
> El resto de la información del mapa (UARTs, gaps sin confirmar) sigue válido.

> **Para quien llega nuevo al repo (relevo 2027):** este es el mapa único de
> "qué viaja por dónde". Los valores salen de `config_{top,down,central}.h` y
> `types.h` al 2026-05-15. Lo marcado ⚠️ NO está confirmado en hardware —
> tiene TASK abierta. No asumir que está resuelto.

```
   OpenMV cam1 ──Serial3 19200──┐
   OpenMV cam2 ──Serial5 19200──┤
   Placa COMM  ──Serial4 115200─┤        ┌── Serial7 230400 ──► CENTRAL
   (árbitros)                   ▼        │   WORLD_SNAPSHOT 100 Hz (24 B)
                          ┌───────────────┐
       DOWN ─Serial5─────►│  PLACA ARRIBA │── Serial7 ──► CENTRAL
       odometría OTOS     │  (Teensy 4.0) │   (TX7 pin 29 → CEN pin 28)
       100 Hz             └───────────────┘
                          ┌───────────────┐
                          │  PLACA CENTRAL│── PWM directo ──► 3 motores omni
                          │  (Teensy 4.1) │── GPIO ─────────► kicker (ROBOT2)
                          │  FSM + PIDs   │
                          └───────────────┘
                                  ▲
            Serial1 (CENTRAL) ◄───┘  LINE_URGENT ~100 Hz
            bus de EMERGENCIA        LineStatus (ángulo+profundidad+imminent)
                                  ▲
                          ┌───────────────┐
                          │  PLACA ABAJO  │  línea muestreada 1 kHz,
                          │  (Teensy 4.0) │  enviada a 100 Hz
                          └───────────────┘
                DOWN ─Serial5 230400─► ARRIBA  (odometría OTOS 100 Hz)
```

### Tabla de enlaces UART

| Enlace | Pines (config) | Baud | Mensaje | Struct | Freq | Confirmado |
|--------|----------------|------|---------|--------|------|------------|
| ARRIBA → CENTRAL | CEN Serial7 RX28/TX29 | 230400 | `WORLD_SNAPSHOT` | `WorldSnapshot` (24 B) | 100 Hz | ✅ 2026-05-31: CENTRAL en Serial7 (28/29); cablear TOP pin 29 → CEN pin 28 |
| ABAJO → CENTRAL | CEN Serial1 (0/1) | 230400 | `LINE_URGENT` | `LineStatus` | ~100 Hz | ✅ reasignado 2026-05-31 (antes Serial2/7-8 → libera motor 2) |
| ABAJO → ARRIBA | DOWN Serial5 RX20/TX21 → TOP Serial1 | 230400 | odometría OTOS | pose/vel | 100 Hz | parcial (TASK-008 rewiring) |
| cam1 → ARRIBA | TOP Serial3 RX15/TX14 | 19200 | blobs pelota/arco | proto viejo 9 B | ~30 Hz | OK |
| cam2 → ARRIBA | TOP Serial5 RX21/TX20 | 19200 | blobs pelota/arco | proto viejo 9 B | ~30 Hz | OK |
| COMM ↔ ARRIBA | TOP Serial4 RX16/TX17 | 115200 | start/stop/partner | RCJ proto | evento | ⚠️ firmware COMM pendiente (TASK-006) |
| CENTRAL → motores | GPIO directo (no UART) | — | PWM + INA/INB | `MotorCommand` interno | 100 Hz | OK |

### Gaps de flujo de datos sin cerrar (NO asumir resueltos)

1. ✅ **RESUELTO 2026-05-31:** ARRIBA→CENTRAL = TOP `Serial7` (TX7 pin 29) →
   CEN `Serial7` (RX7 pin 28); DOWN→CENTRAL = CEN `Serial1` (0/1). `Serial2` (7/8)
   libre para el motor 2 (U17). Falta solo cablear + validar el stream en banco.
2. **Baud DOWN↔CENTRAL**: el bus de emergencia (lo más crítico para no
   salirse de cancha) no tiene constante de baud en `config_central.h`.
   Verificar que ambos extremos coincidan antes de integrar.
3. **DOWN físico**: PCB 04-12 solo tiene ruteados 1 mux + 1 OTOS. Si la placa
   que llegó es esa, odometría/línea van degradadas. → **TASK-001 / TASK-009**.
4. **Frecuencia LINE_URGENT**: la línea se *muestrea* a 1 kHz en DOWN
   (`LINE_TICK_INTERVAL_US=1000`) pero se *envía* a 100 Hz
   (`COMM_SEND_INTERVAL_MS=10`). El brake de emergencia <15 ms lo garantiza el
   chequeo en `main_central.cpp` cada loop, no la frecuencia de envío
   (ver §12 de `FIRMWARE-PLACA-CENTRAL.md`).

---

## Referencias técnicas

- Diseño del firmware (interfaz módulo por módulo): `research/in-progress/2026-05-10-diseno-firmware-3-placas.md`
- Análisis detallado de topologías alternativas: `research/in-progress/2026-05-11-analisis-arquitectura-3-placas-distribuida.md`
- Sistema de posicionamiento y comunicación (versión de marzo 2026): `docs/internal/sistema-posicionamiento-y-comunicacion.md`
- Visión multi-cámara y world model: `docs/multi-camera-world-model.md`
- Análisis ToF / LiDAR para Junior: `docs/lidar-tof-slam-analysis.md`
- Arquitectura del sistema 2025 (legacy): `docs/ARQUITECTURA-SISTEMA-2025.md`
- Pinout de las placas nuevas: `hardware/electronics/mapa-pines-placas-nuevas.md`

Equipos de referencia:
- CAMBADA (RoboCup Middle Size League, U. Aveiro) — arquitectura distribuida con world model centralizado.
- PCBWay (RoboCup Junior Soccer Open 2022/2024) — 4 cámaras + master Teensy 4.1.
- TDPs de RoboCup Junior Soccer Open 2024-2025 — referencia de mejores prácticas: https://junior.robocup.org/tdp/

---

*Documento mantenido por IITA — Instituto de Informática y Tecnología Aplicada, Salta, Argentina.*
