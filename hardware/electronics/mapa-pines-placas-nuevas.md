---
title: "Mapa de pines — Placas TOP y DOWN del robot 2026 (Roboliga 2026)"
date: 2026-05-10
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: draft
tags: [electronica, teensy, pines, placa-nueva, top-board, down-board, referencia]
robot: ambos
area: electronica
tipo: referencia
---

# Mapa de pines — Placas TOP y DOWN del robot 2026

Decodificación de los schematics del 2026-04-12 (`hardware/electronics/pcb_design/`). Las dos placas usan **Teensy 4.0** (no 4.1 como el Zircon Rev v15).

> **Status: borrador inicial inferido desde schematic visual.** Confirmar contra fabricación real antes de comprometer firmware.

---

## Resumen arquitectónico

| Placa | Procesador | Rol propuesto | Responsabilidad |
|-------|-----------|---------------|------------------|
| **TOP** (Roboliga2026 TOP) | Teensy 4.0 (U14) | Master | Estrategia, fusión sensorial, comunicación con árbitros, cámaras, IMU dual, ToF, ultrasonido |
| **DOWN** (Roboliga 2026 Futbol) | Teensy 4.0 (U7) | Slave sensores piso | Anillo de 32 sensores de línea (LEDs activos), 2× SparkFun OTOS (tracking óptico de posición) |
| **COMM** (no en repo) | ESP32 (asumido) | Comm árbitros + ESP-NOW | Display QR + start/stop + inter-robot |
| **MOTORES** (no en repo, ¿Zircon sigue?) | ¿Teensy 4.1 o externo? | Drivers H-bridge para 3 omni | A confirmar con coach |

---

## PLACA TOP — Roboliga2026 TOP REV 1.0 (2026-04-08)

### Componentes principales

| Designador | Componente | Función |
|-----------|-----------|---------|
| U14 | **Teensy 4.0** | MCU master del robot |
| U10, U11 | 2× BNO055 | **IMU dual**, uno por bus I2C, direcciones distintas |
| U2, U3, U5, U17 | 4× ToF "TOF_#3" | Distancia a paredes/obstáculos (manufacturer por confirmar) |
| U6 | HC-SR04 | Ultrasonido frontal (TRIG/ECHO) |
| U8 | UART-CAMERA1 | OpenMV cámara 1 |
| U9 | UART-CAMERA2 | OpenMV cámara 2 |
| U15 | UART_COMM_OUT | UART hacia placa COMM (árbitros) |
| U16 | UART_COMM_IN | UART desde placa DOWN |
| U1 | Conector 6P "PINES MODULO" | OUT1, OUT2, RX_OUT, TX_OUT, USB_D+, USB_D- (¿hacia placa motores?) |
| U7 | Conector 4P | LOGV, 3.3V, GND, +5V |
| U12, U13 | 2× MP1584-EN | Reguladores switching (5V y 3.3V) |
| XP1 | Dean-T-F | Conector batería 7.4V |
| D1, D2 | B5819W | Diodos Schottky protección |

### Buses I2C — **dos buses separados (decisión clave)**

| Bus | SDA | SCL | Periféricos |
|-----|-----|-----|-------------|
| I2C0 | SDA0 (pin 18) | SCL0 (pin 19) | BNO055 U10 + ToF U2 + ToF U3 |
| I2C1 | SDA1 (pin 17) | SCL1 (pin 16) | BNO055 U11 + ToF U5 + ToF U17 |

> **Por qué dos buses:** los dos BNO055 comparten dirección I2C por default (0x28). Con un solo bus habría colisión. Separándolos:
> - I2C0 → BNO055 #1 + 2 ToFs cercanos.
> - I2C1 → BNO055 #2 + 2 ToFs cercanos.
> Cada bus es independiente; usar `Wire` y `Wire1` en código.

### UARTs (4 hardware serial en uso)

| Serial | TX pin | RX pin | Conectado a | Propósito |
|--------|--------|--------|-------------|-----------|
| Serial1 | TX1 (1) | RX1 (0) | U16 UART_COMM_IN | Recibir datos de placa DOWN (línea + tracking) |
| Serial3 | TX3 (8) | RX3 (7) | U8 UART-CAMERA1 | OpenMV cámara 1 |
| Serial4 | TX4 (17?) | RX4 (16?) | U15 UART_COMM_OUT | Hacia placa COMM (árbitros + ESP-NOW) |
| Serial5 | TX5 (21?) | RX5 (20?) | U9 UART-CAMERA2 | OpenMV cámara 2 |

> **A verificar:** los pines TX/RX exactos de Serial4 y Serial5 en Teensy 4.0 según net del schematic. El Teensy 4.0 tiene 7 hardware serial (Serial1-Serial7) en pines distintos al Teensy 4.1, así que el firmware antiguo NO sirve directo.

### Pinout Teensy 4.0 TOP — inferido del schematic

(Confirmar contra fabricación. Donde dice "?" es lectura dudosa del schematic visual.)

| Pin Teensy | Net | Función / Conectado a |
|-----------|-----|----------------------|
| 0 (RX1) | RX1 | Serial1 RX ← placa DOWN |
| 1 (TX1) | TX1 | Serial1 TX → placa DOWN |
| 7 (RX3) | RX3 | Serial3 RX ← cámara 1 |
| 8 (TX3) | TX3 | Serial3 TX → cámara 1 |
| 16 (SCL1 / RX4?) | SCL1 / RX4 | **Conflicto a verificar:** pin 16 es SCL1 y a la vez RX4 en Teensy 4.0 (Wire1 vs Serial4) |
| 17 (SDA1 / TX4?) | SDA1 / TX4 | **Conflicto a verificar:** pin 17 es SDA1 y a la vez TX4 |
| 18 (SDA0) | SDA0 | I2C bus 0 |
| 19 (SCL0) | SCL0 | I2C bus 0 |
| 20 (RX5?) | RX5 | Serial5 RX ← cámara 2 |
| 21 (TX5?) | TX5 | Serial5 TX → cámara 2 |
| 23 (OUT1C) | OUT1 | ¿PWM motor? Conector U1 |
| 26 (OUT1D) | OUT2 | ¿PWM motor? Conector U1 |
| 30 (OUT2) | ECHO | HC-SR04 echo |
| 29 (LRCLK2) | TRIG | HC-SR04 trigger |
| 25 (RX2) | RX_OUT | Conector U1 (¿hacia placa motores?) |
| 24 (TX2) | TX_OUT | Conector U1 (¿hacia placa motores?) |
| Otros | NC | A confirmar |

> **🚨 Conflicto crítico de pines:** el schematic muestra Serial4 (RX4/TX4) en los mismos pines que I2C1 (SDA1/SCL1) — pines 16/17 del Teensy 4.0. **Esto NO puede coexistir.** O el schematic asignó nombres sin verificar conflicto, o uno de los dos se mueve. Verificar con `enzzo195` (el diseñador) y/o medir con multímetro qué net llega a cada pin físico.

---

## PLACA DOWN — Roboliga 2026 Futbol REV 1.0 (2026-04-01)

### Componentes principales

| Designador | Componente | Función |
|-----------|-----------|---------|
| U7 | **Teensy 4.0** | MCU slave de sensores de piso |
| F1-F32 | 32× ALS-PT19 | Fotodiodos sensores de luz (línea) |
| LED1-LED32 | 32× LED 0402 naranja | Emisores activos (par con cada sensor) |
| R1-R32 | 32× 330Ω | Limit current LEDs |
| R33-R64 | 32× 10kΩ | Pull-up/pull-down sensores |
| U1, U2, U3, U4 | 4× CD4051BM | Multiplexores analógicos 8:1 |
| U5, U6 | 2× SparkFun OTOS | Tracking óptico de odometría (posición X, Y, heading) |
| U8, U9 | 2× MP1584-EN | Reguladores switching |
| U10 | Conector 4P "COMUNICATION" | RX5/TX5/+5V/GND → UART hacia TOP board |
| U11 | Conector 4P | RX1/TX1/+5V/GND + señal E1 |
| C1-C6 | 6× 100nF | Decoupling caps |
| D1, D2 | B5819W | Diodos Schottky |
| XP1 | Dean-T-F | Batería 7.4V |

### Anillo de 32 sensores de línea (LEDs activos)

**Concepto:** sensor de línea reflectivo activo. Cada par (LED + ALS-PT19) ilumina el suelo y mide el reflejo. Carpet verde refleja poco, línea blanca refleja mucho. Distribuidos en anillo en la base del robot.

**Multiplexación 32 → 4 entradas analógicas:**
- 4× CD4051BM, cada uno con 8 canales analógicos.
- Cada mux toma 8 sensores → 1 salida (`O1`, `O2`, `O3`, `O4`).
- Las salidas O1-O4 van a 4 pines analógicos del Teensy 4.0.
- Líneas de control A, B, C (3 bits) **compartidas entre los 4 muxes** → todos los muxes seleccionan el mismo canal en paralelo.
- Líneas `INH` (enable) de cada mux conectadas a pines independientes (E5-E8) → permite habilitar muxes por separado si fuera necesario.

**Estimación de timing:**
- 8 canales × ~5µs por lectura analógica = **~40µs para leer los 32 sensores en paralelo** usando 4 ADCs del Teensy 4.0 (que tiene 2 ADCs simultáneos).
- En el peor caso secuencial: ~320µs.
- Frecuencia objetivo: **1 kHz de lecturas del anillo** es perfectamente alcanzable.

**Lo que el firmware DOWN debe entregar al TOP** (procesado, no crudo):
- Ángulo de la línea respecto al frente del robot (0° = frente, 90° = derecha, 180° = atrás).
- Distancia al borde (qué tan profundo está el sensor en la línea blanca).
- Flag "salida inminente" (cuántos sensores en blanco simultáneamente).

### Tracking óptico — SparkFun OTOS (dual)

**Qué es:** sensor SparkFun "Optical Tracking Odometry Sensor" — un combo de sensor de mouse óptico + IMU integrado que entrega **posición (x, y) + heading** por integración óptica del piso. Comunicación I2C.

**Configuración dual en buses separados:**
- U5 → I2C bus 1 (SDA1/SCL1)
- U6 → I2C bus 2 (SDA2/SCL2)

> **Por qué dual:** dos OTOS dan redundancia (uno se rompe → seguimos con el otro), pero también pueden montarse en posiciones distintas del chasis para mejorar precisión por triangulación. La decisión depende del montaje físico. Si están a 180° entre sí, un OTOS ve cuando el otro está sobre línea blanca (que distorsiona la lectura óptica). **A confirmar con coach cómo se montaron.**

**Lo que entrega cada OTOS:**
- X, Y en mm (posición acumulada desde último reset).
- Heading en grados.
- Velocidades lineales y angular.

**Lo que el firmware DOWN debe entregar al TOP** (fusionado):
- Posición (x, y) del robot en mm desde inicio.
- Heading absoluto.
- Velocidad lineal y angular.
- Flag de calidad (los 2 OTOS coinciden o divergen).

### Buses I2C — dos buses separados (para 2× OTOS)

| Bus | SDA | SCL | Periféricos |
|-----|-----|-----|-------------|
| I2C1 | SDA1 | SCL1 | OTOS U5 |
| I2C2 | SDA2 | SCL2 | OTOS U6 |

### UARTs

| Serial | Conectado a | Propósito |
|--------|-------------|-----------|
| Serial5 (RX5/TX5) | U10 COMUNICATION | UART hacia placa TOP |
| Serial1 (RX1/TX1) | U11 + señal E1 | UART secundario (¿debug? ¿otra placa?) |

---

## Cableado entre placas (hipótesis)

```
              ┌─────────────────────┐
              │   PLACA COMM        │
              │   (ESP32 + display) │
              └────────┬────────────┘
                       │ UART (Serial4 del TOP)
                       │
              ┌────────▼────────────┐
              │   PLACA TOP         │
              │   Teensy 4.0 master │
              │   - 2× BNO055       │
              │   - 4× ToF          │
              │   - HC-SR04         │
              │   - 2× UART cámaras │
              └────────┬────────────┘
                       │ UART (Serial1 del TOP ↔ Serial5 del DOWN)
                       │
              ┌────────▼────────────┐
              │   PLACA DOWN        │
              │   Teensy 4.0 slave  │
              │   - Anillo 32 línea │
              │   - 2× SparkFun OTOS│
              └─────────────────────┘

       ¿PLACA MOTORES?
       └────────────────► conector U1 del TOP (OUT1/OUT2/RX_OUT/TX_OUT)
                          ¿Es el Zircon Rev v15 con drivers H-bridge?
                          ¿O placa nueva no documentada?
```

---

## Preguntas resueltas (sesión coach 2026-05-10)

| # | Pregunta original | Respuesta del coach |
|---|------------------|----------------------|
| 1 | Manejo de motores | **Zircon Rev v15 + Teensy 4.1 sigue activo** como "motor server". Recibe comandos del TOP por UART y ejecuta PWM en los 3 omni. La placa DOWN tiene forma irregular para dejar espacio físico a los motores que vienen del Zircon. |
| 3 | Manufacturer ToF | **VL53L5CX comprados (pendientes de arribo) + VL53L7CX posiblemente en stock.** Ambos son ToF de matriz 8×8 SPAD con librería específica de ST. Firmware ToF queda como hito tardío hasta que llegue el hardware. |
| 4 | SparkFun OTOS | Confirmado SparkFun "Optical Tracking Odometry Sensor". Dos buses I2C separados porque los 2 OTOS comparten dirección I2C por default. |
| 5 | Montaje OTOS | **Uno a cada costado del robot** — habilita **análisis diferencial** para detectar trayectoria asimétrica al patear y forzar "avanzar derecho". Si `vx_left ≠ vx_right`, el robot está girando → corrección angular. |
| 6 | Placa COMM | **Copia 100% del módulo oficial RCJ** (mismas dimensiones): ESP32 + pantalla OLED + acelerómetro (anti-tamper) + 2 botones de programación. **Firmware oficial RCJ a cargar tal cual.** Para ESP-NOW inter-robot: evaluar firmware modificado o app secundaria en el mismo ESP32. |

## Q3 resuelta con análisis del PCB EasyEDA (2026-05-10)

Inspección de `pcb_design/top_board/BackupProjects_*.zip` (proyecto EasyEDA completo). El zip externo contenía otro zip; el zip interno contiene `1-PCB_PCB_Roboliga2026_TOP.json` (255KB, layout físico con coordenadas y routing).

**Hallazgo:** los 4 nets potencialmente conflictivos **TIENEN tracks ruteados** en el PCB fabricado:

| Net | TRACKs en TOP PCB | Implicancia |
|-----|-------------------|-------------|
| SCL1 | 5 | I2C bus 1 — clock |
| SDA1 | 5 | I2C bus 1 — data |
| RX4 | 11 | Serial4 — RX |
| TX4 | 9 | Serial4 — TX |

Es decir: **4 conexiones físicas distintas a la PCB**, no 2 multiplexadas. Como en Teensy 4.0 los pines 16/17 son `SCL1/SDA1` *o* `RX4/TX4` (mutuamente exclusivos), **la única configuración compatible con 4 nets distintos en la placa es:**

- **Wire1 remapeada a pines 24/25** (`Wire1.setSCL(24); Wire1.setSDA(25);`).
- **Serial4 en pines 16/17** (default).

**Acción crítica para el firmware TOP:**
```cpp
// ANTES de Wire1.begin():
Wire1.setSCL(24);
Wire1.setSDA(25);
Wire1.begin();
// Serial4 queda en sus pines default 16/17, sin tocar.
Serial4.begin(230400);
```

> **No 100% confirmado todavía.** La inferencia es por "única explicación consistente con el PCB ruteado". La confirmación final requiere:
> - Abrir el proyecto en EasyEDA y ver a qué pad físico del footprint del Teensy 4.0 (`COMM-TH_34P-L35.6-W17.8-P2.54_C9900133251`) está atado cada net. Pad 18/19 = pines 16/17 lógicos; pad 26/27 = pines 24/25.
> - O verificar con multímetro: continuidad entre el net visible en la placa y el pin físico del módulo Teensy.

## Preguntas que siguen abiertas

| # | Pregunta | Acción para resolver |
|---|---------|---------------------|
| 7 | Pinout exacto del Teensy 4.0 en ambas placas (verificación final de Q3) | Abrir proyecto en EasyEDA o medir con multímetro: confirmar que SCL1/SDA1 caen en pad 26/27 (pines 24/25 lógicos) del módulo Teensy. |

---

## Próximos pasos

1. Resolver Q2 (conflicto pines) con multímetro o consulta a `enzzo195`.
2. Validar el pinout inferido midiendo continuidad en placa fabricada.
3. Cargar firmware oficial RCJ en la placa COMM (independiente del firmware TOP/DOWN).
4. Esperar arribo de ToF VL53L5CX para integrarlos al firmware TOP.
5. Diseño de firmware en `research/in-progress/2026-05-10-diseno-firmware-3-placas.md`.
