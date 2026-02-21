---
title: "Arquitectura del sistema de robots — Temporada 2025"
date: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: false
ai-tool: "Claude (Anthropic - Claude Opus 4.6)"
status: final
tags: [arquitectura, hardware, software, openmv, teensy, protocolo, sensores, analisis]
---

# Arquitectura del Sistema de Robots — Temporada 2025

## Análisis Técnico Completo para Planificación 2026

**Equipo**: IITA RoboCup Junior Soccer Open (ahora Soccer Vision)
**Autor del análisis**: Claude (Anthropic — Claude Opus 4.6)
**Supervisión**: Gustavo Viollaz (@gviollaz)
**Fecha**: 21 de febrero de 2026
**Fuentes**: Repositorios `IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025` y `IITA-Proyectos/open-soccer-robocup-team2026`

---

## 1. Resumen Ejecutivo

El sistema 2025 consiste en dos robots (arquero y delantero) construidos sobre la placa controladora Zircon (diseño propio en dos variantes: Mark1 y Naveen1) con microcontrolador Teensy 4.1, cámara OpenMV H7 como sensor de visión primario, giroscopio BNO055 para orientación, sensores de línea reflectivos para detección de límites, sensores IR para detección de pelota, y un sensor ultrasónico HC-SR04 para distancia en el arquero.

El equipo ganó el campeonato nacional en diciembre 2025 con este sistema. Sin embargo, el análisis revela **problemas estructurales significativos** en software que deben resolverse antes de competir internacionalmente en Incheon (junio-julio 2026).

### Hallazgos Críticos

Se identificaron **23 puntos de falla** categorizados en: bugs de software (8), deficiencias de diseño (7), vulnerabilidades de protocolo (5), y riesgos por cambios de reglas 2026 (3).

---

## 2. Diagrama de Arquitectura del Sistema

```
┌─────────────────────────────────────────────────────────────────┐
│                        ROBOT (Arquero o Delantero)              │
│                                                                 │
│  ┌──────────────┐     UART (Serial1)      ┌──────────────────┐ │
│  │  OpenMV H7   │ ───────────────────────► │   Teensy 4.1     │ │
│  │  (Cámara)    │   19200 baud            │   (Controlador)  │ │
│  │              │   Protocolo propietario  │                  │ │
│  │  - RGB565    │   [Header][Data][Data]   │  ┌────────────┐ │ │
│  │  - QVGA      │                          │  │ zirconLib  │ │ │
│  │  - find_blobs│                          │  │ (HAL)      │ │ │
│  │  - Homografía│                          │  └─────┬──────┘ │ │
│  └──────────────┘                          │        │        │ │
│                                            │        ▼        │ │
│                              ┌─────────────┤  ┌──────────┐  │ │
│  ┌──────────────┐            │             │  │ Motores  │  │ │
│  │  BNO055      │◄───── I2C ┤             │  │ x3 (120°)│  │ │
│  │  (Giroscopio)│            │             │  └──────────┘  │ │
│  └──────────────┘            │             │                │ │
│                              │  Zircon PCB │  ┌──────────┐  │ │
│  ┌──────────────┐            │  (Mark1 o   │  │ Sensores │  │ │
│  │  HC-SR04     │◄── GPIO ──┤   Naveen1)  │  │ IR x8    │  │ │
│  │  (Ultrasón.) │            │             │  └──────────┘  │ │
│  └──────────────┘            │             │                │ │
│                              │             │  ┌──────────┐  │ │
│  ┌──────────────┐            │             │  │ Línea x3 │  │ │
│  │  Dribbler    │◄── PWM ───┤             │  │ (Analog) │  │ │
│  │  (Motor DC)  │            │             │  └──────────┘  │ │
│  └──────────────┘            └─────────────┤                │ │
│                                            │  ┌──────────┐  │ │
│                                            │  │Botones x2│  │ │
│                                            │  └──────────┘  │ │
│                                            └────────────────┘ │
│                                                                 │
│  ⚠️ 2026: Falta Communication Module (obligatorio internacional)│
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Capa de Hardware

### 3.1 Microcontrolador: Teensy 4.1

- **MCU**: NXP i.MX RT1062, ARM Cortex-M7 a 600 MHz
- **RAM**: 1024 KB, Flash: 8 MB
- **Puertos seriales**: 8 UART hardware (se usa Serial1 para OpenMV)
- **ADC**: Múltiples canales analógicos (se usan para sensores IR y de línea)
- **PWM**: Múltiples canales (control de motores y dribbler)
- **I2C**: Bus para giroscopio BNO055

**Observación**: El Teensy 4.1 tiene capacidad de sobra para lo que se le exige. El cuello de botella no es el hardware de procesamiento sino el software.

### 3.2 Placa Controladora: Zircon PCB

La placa Zircon es un PCB diseñado para RoboCup Junior que actúa como shield/carrier para el Teensy. Existe en **dos variantes** con diferente pinout:

| Componente | Mark1 | Naveen1 |
|------------|-------|---------|
| Motor 1 | INA=2, INB=5, PWM=3 | DIR1=3, DIR2=4 (sin PWM separado) |
| Motor 2 | INA=8, INB=7, PWM=6 | DIR1=6, DIR2=7 |
| Motor 3 | INA=11, INB=12, PWM=4 | DIR1=5, DIR2=2 |
| Ball IR 1-4 | 14, 15, 16, 17 | 20, 21, 14, 15 |
| Ball IR 5-8 | 20, 21, 22, 23 | 16, 17, 18, 19 |
| Botones | 9, 10 | 8, 10 |
| Línea 1-3 | A11, A13, A12 | A8, A9, A12 |
| Detección versión | Pin 32 LOW = Mark1 | Pin 32 HIGH = Naveen1 |

**Diferencia clave en control de motores**:
- **Mark1**: Usa esquema clásico DIR + DIR + PWM (3 pines por motor, 9 total). La dirección se fija con `digitalWrite` y la velocidad con `analogWrite` en el pin PWM.
- **Naveen1**: Usa esquema H-Bridge directo con 2 pines PWM por motor (6 pines total). La velocidad y dirección se controlan ambas con `analogWrite` en los pines de dirección.

**⚠️ Punto de falla #1**: La detección de versión depende de un pulldown en pin 32. Si el pin queda flotante o hay ruido, el robot podría inicializar con el pinout incorrecto, causando comportamiento errático de todos los motores.

**Sugerencia**: Agregar un `Serial.println(getZirconVersion())` obligatorio en el `setup()` de cada programa, y verificar visualmente antes de cada partido.

### 3.3 Configuración de Motores (Omnidireccional 3 ruedas)

El robot usa 3 motores DC con ruedas omnidireccionales a 120° entre sí:

```
         Motor 1 (trasero)
            ↕
    Motor 3 ←→ Motor 2
     (izq)      (der)
```

La librería expone `motor1(power, direction)`, `motor2(power, direction)`, `motor3(power, direction)` con potencia 0-100 y dirección 0/1.

**⚠️ Punto de falla #2**: No existe una función de movimiento omnidireccional unificada `move(angle, speed)`. Cada programa reimplementa las combinaciones de motores de manera diferente e inconsistente. Esto es una fuente primaria de bugs.

### 3.4 Sensores

#### 3.4.1 Sensores de Línea (x3 — Reflectivos analógicos)

- **Cantidad**: 3 (izquierda, centro, derecha — posición inferior del robot)
- **Tipo**: Sensores reflectivos analógicos (probablemente QRE1113 o similar)
- **Lectura**: `readLine(n)` → `analogRead(linepin_n)` → Valor 0-1023
- **Calibración del arquero**:

| Color | S1 (Izq) | S2 (Centro) | S3 (Der) |
|-------|-----------|-------------|----------|
| Blanco | 575-753 | 494-762 | 590-754 |
| Verde | 210-340 | 370-467 | 254-342 |
| Negro | 174-227 | 418-422 | 234-269 |

**⚠️ Punto de falla #3**: Los rangos están hardcodeados con valores exactos calibrados para UNA cancha. En competencia internacional las condiciones de iluminación serán completamente diferentes (fluorescentes vs LED, luz natural, etc.). Los umbrales necesitan ser recalibrados in-situ y almacenados como constantes editables.

**⚠️ Punto de falla #4**: Solo 3 sensores de línea cubren un ángulo muy limitado. Los equipos de élite usan arrays de 16-32 sensores en anillo para cobertura de 360°. Con 3 sensores, el robot puede salirse de la cancha por los laterales sin detectar la línea.

**Sugerencia**: Mínimo duplicar a 6 sensores (frente, atrás, izq, der, y dos diagonales). Idealmente un anillo de 12+ sensores.

#### 3.4.2 Sensores de Pelota IR (x8 — Fotodiodos infrarrojos)

- **Cantidad**: 8 fotodiodos distribuidos en anillo alrededor del robot
- **Tipo**: Fotodiodos IR que detectan la señal pulsada de la pelota (40 kHz modulada)
- **Lectura**: `readBall(n)` → `1024 - analogRead(ballpin_n)` → Valor invertido (mayor = más cerca)
- **Distribución**: 8 sensores a 45° cada uno para cobertura de 360°

**⚠️ Punto de falla #5**: En el código actual del delantero (perseguir-pelota.ino), los sensores IR **no se usan**. Toda la detección de pelota viene exclusivamente de la cámara OpenMV. Esto significa que si la pelota está detrás del robot (fuera del campo de visión de la cámara), el delantero no la detecta. Los 8 sensores IR están instalados pero **desperdiciados** en el programa de competencia.

**Sugerencia**: Implementar fusión sensorial: cámara como sensor primario cuando la pelota está en el campo de visión, IR como sensor de respaldo para detección de 360°.

#### 3.4.3 Giroscopio: Adafruit BNO055

- **Chip**: Bosch BNO055 — IMU de 9 ejes con fusión sensorial integrada
- **Interfaz**: I2C (dirección 0x28)
- **Dato utilizado**: `orientation.x` (Euler heading, 0-360°)
- **Función de lectura**: `readCompass()` en zirconLib (requiere `compassCalibrated = true`)
- **Control proporcional**: `error = currentYaw - initialYaw`, con wrapping ±180° y `kp = 0.3`

**⚠️ Punto de falla #6**: La variable `compassCalibrated` se inicializa como `false` y **nunca se establece como `true`** en `zirconLib.cpp`. La función `readCompass()` devuelve siempre 0 y imprime "Compass not calibrated!". Sin embargo, en `lateral_con_giróscopo`, el BNO055 se usa **directamente** sin pasar por `readCompass()`, accediendo al objeto `bno` directamente. Esto crea dos formas incompatibles de leer el giroscopio:

1. `readCompass()` vía zirconLib → **nunca funciona** (falta calibración)
2. `bno.getEvent()` directo → funciona pero **no usa la abstracción** de zirconLib

**Sugerencia**: Corregir `InitializeZircon()` para que inicialice y calibre el BNO055, o eliminar la referencia de la librería y usar siempre acceso directo con un wrapper propio.

#### 3.4.4 Sensor Ultrasónico: HC-SR04 (solo arquero)

- **Tipo**: Sensor de distancia por ultrasonido
- **Pines**: TRIG=9, ECHO=10
- **Función**: `medirDistancia()` → distancia en cm
- **Uso**: Determinar proximidad al arco para el control de posición del arquero

**⚠️ Punto de falla #7**: La función `pulseIn(ECHO, HIGH)` es **bloqueante** — detiene todo el programa hasta 1 segundo si no hay eco. En un robot que necesita reaccionar en milisegundos, esto puede causar "congelamiento" momentáneo. Además, `delayMicroseconds()` también es bloqueante.

**Sugerencia**: Reemplazar con lectura no-bloqueante usando interrupciones o la librería NewPing.

---

## 4. Capa de Software — OpenMV (Visión)

### 4.1 Configuración de Cámara

```python
sensor.reset()
sensor.set_pixformat(sensor.RGB565)      # Color 16-bit
sensor.set_framesize(sensor.QVGA)        # 320x240 píxeles
sensor.skip_frames(time=2000)            # Estabilización 2 seg
sensor.set_auto_whitebal(False)          # Balance de blancos fijo
```

La configuración es correcta para RoboCup. QVGA (320x240) es el estándar que equilibra resolución con velocidad de procesamiento. Desactivar auto white balance es esencial para que los thresholds de color sean estables.

### 4.2 Detección de Objetos por Color (Blob Detection)

El sistema detecta hasta 3 objetos por color usando thresholds LAB:

| Objeto | Threshold LAB (L_min, L_max, A_min, A_max, B_min, B_max) | Archivo |
|--------|-----------------------------------------------------------|---------|
| Pelota naranja | (30, 60, 20, 60, 10, 50) | calcula-coordenadas |
| Pelota naranja | (76, 18, 13, 88, 6, 127) | enviar 2 arcos (**diferente!**) |
| Arco amarillo | (0, 79, -22, -8, 46, 127) | enviar 2 arcos |
| Arco azul | (31, 19, -36, 60, -61, 5) | enviar 2 arcos |

**⚠️ Punto de falla #8**: Existen **dos sets diferentes de thresholds para la pelota naranja** en distintos archivos. Esto sugiere que los thresholds no están centralizados y se fueron modificando independientemente. Además, en el threshold `(76, 18, ...)`, el L_min (76) es **mayor** que L_max (18), lo que es físicamente imposible en LAB y causaría que `find_blobs()` no detecte nada o detecte todo incorrectamente.

**⚠️ Punto de falla #9**: Los thresholds de arco azul `(31, 19, ...)` también tienen L_min > L_max. Esto es un **bug crítico** que probablemente significa que los valores están en un orden diferente al esperado, o que se copió/pegó incorrectamente.

**Sugerencia**: Centralizar todos los thresholds en un archivo de configuración único (`config.py`). Usar la herramienta `Calibrar_Treshold.py` para recalibrar en cada sede de competencia. Validar programáticamente que L_min < L_max, A_min < A_max, B_min < B_max.

### 4.3 Transformación Homográfica (Píxeles → Centímetros)

El sistema convierte coordenadas de píxeles (u,v) a coordenadas físicas (x,y en cm) usando una **matriz de homografía 3x3**:

```python
def transformarcoordenadas(u, v):
    H = [[...], [...], [...]]  # Matriz 3x3 calibrada
    denominator = H[2][0]*u + H[2][1]*v + H[2][2]
    x = (H[0][0]*u + H[0][1]*v + H[0][2]) / denominator
    y = (H[1][0]*u + H[1][1]*v + H[1][2]) / denominator
    return x, y
```

Seguido de corrección por altura de la cámara y radio de la pelota:

```python
X = x * (h - r) / h    # h = altura cámara, r = radio pelota
Y = y * (h - r) / h
```

**⚠️ Punto de falla #10**: Existen **dos matrices de homografía diferentes** en el código:

- Matriz en `calcula-coordenadas-pelota.py` con `h = 10 cm`
- Matriz en `enviar cordenadas 2 arcos y pelota` con `h = 18.7 cm`

Estas matrices son **completamente diferentes**, lo que indica que se calibraron para montajes de cámara diferentes. Si se usa la matriz incorrecta, las coordenadas calculadas serán erróneas.

**Sugerencia**: Documentar exactamente el montaje mecánico de la cámara (altura, ángulo, lente) y recalibrar la homografía para el montaje definitivo 2026. Guardar la matriz junto con la documentación del setup físico.

### 4.4 Indicadores LED

El código de visión más avanzado usa los LEDs integrados del OpenMV como indicadores:

- **LED Rojo**: Pelota naranja detectada
- **LED Verde**: Arco amarillo detectado
- **LED Azul**: Arco azul detectado

Esto es una buena práctica para debugging en campo.

---

## 5. Protocolo de Comunicación OpenMV → Teensy

### 5.1 Capa Física

| Parámetro | Valor |
|-----------|-------|
| Interfaz | UART (Serial asíncrono) |
| OpenMV TX | UART3 (OpenMV) → Pin 0 RX1 (Teensy) |
| Baud rate | 19200 bps (versión final) / 115200 bps (versión anterior) |
| Formato | 8N1 (8 bits, sin paridad, 1 stop bit) |

**⚠️ Punto de falla #11**: Existen **dos baud rates diferentes** en el código. `enviar paq. de datos` usa 115200, mientras que `enviar cordenadas 2 arcos y pelota` y `perseguir-pelota.ino` usan 19200. Si el OpenMV y el Teensy no están en el mismo baud rate, la comunicación falla silenciosamente.

### 5.2 Estructura de Paquetes

El protocolo evolucionó durante la temporada. Hay **tres versiones incompatibles**:

**Versión 1**: Solo pelota (4 campos con headers individuales)
```
[201][byteX] [202][byteY] [203][byteAng] [204][sentido]
```

**Versión 2**: Pelota + 1 arco (6 bytes, 2 headers)
```
[201][Xp][Yp] [202][Xa][Ya]
```

**Versión 3**: Pelota + 2 arcos (9 bytes, 3 headers)
```
[201][Xp][Yp] [202][Xam][Yam] [203][Xaz][Yaz]
```

### 5.3 Codificación de Datos

| Dato | Rango real | Codificación | Decodificación |
|------|-----------|--------------|----------------|
| X (distancia) | 0-100 cm | `int(X * 2)` → 0-200 | `byte / 2.0` |
| Y (lateral) | -50 a +50 cm | `int((Y + 50) * 2)` → 0-200 | `(byte / 2.0) - 50` |
| Ángulo | -100° a +100° | `int(ang + 100)` → 0-200 | `byte - 100` |
| Sentido | 0 o 1 | Directo | Directo |
| Sin detección | — | 0 | Verificar `!= 0` |

**⚠️ Punto de falla #12**: Los headers (201, 202, 203, 204) pueden colisionar con valores de datos legítimos. Si X×2 = 201, el Teensy interpretaría ese dato como un header, desincronizando el stream completo. No hay checksum, CRC, ni mecanismo de resincronización.

**⚠️ Punto de falla #13**: El receptor del Teensy en `perseguir-pelota.ino` lee exactamente 6 bytes con `Serial1.available() >= 6`. Si la transmisión pierde un byte, el receptor se desincroniza permanentemente.

**⚠️ Punto de falla #14**: En `codigo de movilidad con cámara y control`, el receptor usa una máquina de estados byte-a-byte. Si llegan headers consecutivos sin dato intermedio (pérdida de byte), la máquina salta un campo sin registrarlo. No hay timeout ni recovery.

**Sugerencia para protocolo 2026**:
```
[0xFF][LENGTH][Xp_hi][Xp_lo][Yp_hi][Yp_lo][Xa_hi][Xa_lo][Ya_hi][Ya_lo][CHECKSUM]
```
Con byte de inicio fijo (0xFF), longitud, resolución de 16 bits, y checksum XOR.

---

## 6. Capa de Software — Teensy (Control)

### 6.1 Programa del Arquero

**Estrategia**: Oscilar lateralmente sobre la línea del arco, mantener orientación frontal con giroscopio, y usar sensores de línea para no entrar al arco ni salirse de la cancha.

**⚠️ Punto de falla #15**: El código del arquero tiene **variables redeclaradas** (`int s1, s2, s3` aparece múltiples veces), **funciones no definidas** (`leerGiroscopio()`, `avanzarDerecha()`, `avanzarIzquierda()`, `corregirAngulo()`), y **secciones duplicadas** (hay dos bloques `void setup()` y dos bloques de declaración de variables). El archivo claramente es un **work-in-progress que no compila** tal cual.

**⚠️ Punto de falla #16**: Los sensores de línea se leen en **la inicialización global** (`int s1 = readLine(1);`) en lugar de en el `loop()`. s1, s2, s3 se leen UNA sola vez al arrancar y nunca se actualizan.

**⚠️ Punto de falla #17**: La función `Adelante()` usa variables `static` que la convierten en función de "una sola vez".

### 6.2 Programa del Delantero

**Máquina de estados**:

| Estado | Acción | Transición |
|--------|--------|-----------|
| GIRANDO | Rotar en lugar | → AVANZANDO si hay pelota |
| AVANZANDO | Avanzar al frente | → CENTRANDO si pelota cerca; → GIRANDO si pierde pelota |
| CENTRANDO | Alinear arco y pelota | → PATEANDO si alineados |
| PATEANDO_adelante | Full power 2 seg | → GIRANDO |

**⚠️ Punto de falla #18**: En PATEANDO, la condición de timeout está **invertida**:
```c
if(millis() - millis_inicio_estado <= 2000) {  // ← debería ser >=
```
El `<=` hace que los motores se apaguen inmediatamente en el primer ciclo. El robot nunca patea realmente.

**⚠️ Punto de falla #19**: El cálculo de ángulo del arco usa coordenadas de la pelota:
```c
anguloRadArco = atan2(decodedYp, decodedXp);  // ← debería ser Ya, Xa
```

**⚠️ Punto de falla #20**: `avanzarAlFrente()` mueve motor2 y motor1 pero deja motor3 en 0, generando movimiento diagonal en lugar de recto.

### 6.3 Librería zirconLib

| Función | Estado |
|---------|--------|
| `InitializeZircon()` | ✅ Funcional |
| `setZirconVersion()` | ✅ Funcional |
| `readCompass()` | ❌ Nunca funciona (compassCalibrated siempre false) |
| `readBall(n)` | ✅ Funcional |
| `readLine(n)` | ✅ Funcional |
| `readButton(n)` | ✅ Funcional |
| `motor1/2/3(power, dir)` | ✅ Funcional |
| `getZirconVersion()` | ✅ Funcional |

**⚠️ Punto de falla #21**: No hay funciones de movimiento de alto nivel. Cada programa debe saber qué combinación de motor1/motor2/motor3 produce cada movimiento.

**Sugerencia**: Agregar `moveOmni(angle, speed, rotation)`, `moveForward(speed)`, `moveLateral(speed)`, `rotate(speed)`, `stop()`.

### 6.4 Código del Dribbler

**⚠️ Punto de falla #22**: El dribbler espera un **string por Serial** ("pelota detectada") para activarse. En ninguna parte del código del Teensy ni del OpenMV se envía este string. El dribbler probablemente **nunca se activó automáticamente** en competencia.

**⚠️ Punto de falla #23 — REGLAS 2026**: La zona de captura de pelota se reduce de 3.0 cm a **1.5 cm**. Hay que verificar que con el dribbler activo la pelota no penetre más de 1.5 cm.

---

## 7. Código No Migrado del Repo 2025

| Archivo | Tamaño | Contenido |
|---------|--------|-----------|
| `avance lateral tiempo` | 4 KB | Movimiento lateral temporizado |
| `codigo de movilidad con cámara y control` | 6 KB | Integración completa Teensy+OpenMV con máquina de estados |
| `lateral_con_giróscopo` | 2.4 KB | Control proporcional con BNO055 |
| `enviar paq. de datos` | 2.3 KB | OpenMV envío de 4 campos con headers 201-204 |
| `ultimo dribbler` | 1 KB | Control de dribbler por Serial string |
| `probar sensores de linea` | 1.9 KB | Test de sensores con movimiento reactivo |
| `OpenMV/enviar cordenadas 2 arcos y pelota` | 6.7 KB | Versión más avanzada — pelota + 2 arcos con LEDs |
| `OpenMV/enviar coordenadas pelota(con redondez)` | 3 KB | Filtrado por redondez del blob |
| `OpenMV/Enviar paquete de datos solo pelota` | 4 KB | Versión intermedia |
| `OpenMV/Calibrar_Treshold.py` | 7 KB | Herramienta de calibración interactiva |
| `OpenMV/UART Teensy` | 1.6 KB | Test de comunicación UART |
| Carpetas `OpenMV/H7/` y `OpenMV/H7 plus/` | — | Configuraciones para las dos versiones de cámara |

**Recomendación**: Migrar todos estos archivos al repo 2026 bajo `legacy/2025-season/code/`.

---

## 8. Resumen de Puntos de Falla

### Críticos (impiden funcionamiento correcto)

| # | Componente | Problema |
|---|-----------|----------|
| 6 | zirconLib | `compassCalibrated` siempre false — giroscopio inaccesible vía librería |
| 8-9 | OpenMV | Thresholds con L_min > L_max — detección de color potencialmente rota |
| 12-14 | Protocolo UART | Sin checksum, colisión header/dato, sin resync — datos corruptos sin detección |
| 15-16 | Arquero | Código no compila, sensores leídos una vez |
| 18 | Delantero | Condición de pateo invertida (≤ en vez de ≥) — robot nunca patea |
| 19 | Delantero | Ángulo arco calcula con datos de pelota — variable inútil |

### Altos (degradan rendimiento significativamente)

| # | Componente | Problema |
|---|-----------|----------|
| 2 | Motores | Sin función moveOmni() unificada — movimiento inconsistente |
| 3 | Línea | Thresholds hardcodeados — falla con iluminación diferente |
| 5 | Sensores IR | 8 sensores instalados pero no usados — sin detección 360° |
| 7 | Ultrasónico | pulseIn() bloqueante — robot se congela momentáneamente |
| 10-11 | OpenMV/UART | Dos homografías y dos baud rates — configuración incorrecta = datos basura |
| 22 | Dribbler | Activación por string serial — nunca se activa automáticamente |

### Moderados (riesgos para 2026)

| # | Componente | Problema |
|---|-----------|----------|
| 1 | Zircon | Detección versión por pin flotante |
| 4 | Línea | Solo 3 sensores — robot puede salir por laterales |
| 17 | Arquero | Función Adelante() con variables static |
| 20 | Delantero | avanzarAlFrente() no es realmente adelante |
| 21 | zirconLib | No hay funciones de movimiento de alto nivel |
| 23 | Dribbler | Zona de captura 3→1.5 cm en reglas 2026 |

---

## 9. Recomendaciones para 2026

### 9.1 Prioridad Inmediata (Febrero-Marzo)

1. Migrar TODOS los archivos del repo 2025 al directorio legacy
2. Rediseñar protocolo UART con start byte, longitud, checksum, y timeout
3. Corregir zirconLib: arreglar BNO055, agregar funciones de movimiento omnidireccional
4. Centralizar configuración: un archivo `config.h`/`config.py` con todos los thresholds y constantes
5. Verificar dribbler cumple 1.5 cm de zona de captura

### 9.2 Prioridad Media (Marzo-Abril)

6. Implementar fusión sensorial: cámara + sensores IR para detección 360°
7. Ampliar sensores de línea: mínimo 6, idealmente 12+ en anillo
8. Reemplazar pulseIn() con lectura ultrasónica no-bloqueante
9. Unificar programas: un solo binario con selección de rol por botón/switch
10. Implementar Communication Module (obligatorio para internacional)

### 9.3 Prioridad para Internacional (Abril-Junio)

11. Calibración automatizada de thresholds in-situ
12. Recalibrar homografía para montaje definitivo de cámara
13. Preparar documentación RoboCup: BOM con precios, poster A1, video, portfolio
14. Testing extensivo del protocolo bajo condiciones adversas
15. Evaluar OpenMV H7 vs H7 Plus para procesamiento de visión más pesado

### 9.4 Arquitectura de Software Propuesta para 2026

```
┌─────────── Nuevo stack de software propuesto ───────────┐
│                                                          │
│  OpenMV (vision.py)                                      │
│  ├── config.py          ← Thresholds, homografía         │
│  ├── detect_ball()      ← Naranja con filtro redondez    │
│  ├── detect_goals()     ← Amarillo + azul                │
│  ├── detect_lines()     ← Opcional: líneas por visión    │
│  └── send_packet()      ← Protocolo robusto con checksum │
│                                                          │
│  Teensy (main.cpp)                                       │
│  ├── config.h           ← Constantes, pines, calibración │
│  ├── zirconLib2.h       ← HAL mejorada con moveOmni()   │
│  ├── protocol.h         ← Parser UART robusto            │
│  ├── sensors.h          ← Lectura no-bloqueante          │
│  ├── strategy_goalie.h  ← Lógica del arquero             │
│  ├── strategy_striker.h ← Lógica del delantero           │
│  ├── comm_module.h      ← Communication Module 2026      │
│  └── main.cpp           ← Setup + loop + selección rol   │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

---

## 10. Inventario de Sensores del Sistema

| Sensor | Cant. | Tipo | Interfaz | Ubicación | Usado en 2025 |
|--------|-------|------|----------|-----------|---------------|
| OpenMV H7 | 1 | Cámara RGB | UART a Teensy | Superior, mirando al frente | ✅ Delantero |
| BNO055 | 1 | IMU 9-ejes | I2C | En la Zircon PCB | ✅ Arquero lateral |
| HC-SR04 | 1 | Ultrasónico | GPIO | Frontal (arquero) | ✅ Solo arquero |
| IR Ball | 8 | Fotodiodo IR | ADC analógico | Anillo 360° | ❌ No usados en código final |
| Line | 3 | Reflectivo analógico | ADC analógico | Inferior (izq/centro/der) | ✅ Arquero |
| Botones | 2 | Pulsador digital | GPIO digital | Accesibles externamente | Parcialmente |
| Dribbler | 1 | Motor DC con H-bridge | PWM + DIR | Frontal inferior | ❌ Sin activación automática |

---

*Documento generado por Claude (Anthropic — Claude Opus 4.6) bajo supervisión de Gustavo Viollaz (@gviollaz), 21 de febrero de 2026.*
*Fuente: análisis de código completo de los repositorios IITA-Proyectos.*
