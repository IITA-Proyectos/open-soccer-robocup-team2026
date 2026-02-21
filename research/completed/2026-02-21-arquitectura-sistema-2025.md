---
title: "Arquitectura del sistema de robots — Temporada 2025"
date: 2026-02-21
updated: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: false
ai-tool: "Claude (Anthropic - Claude Opus 4.6)"
status: final-verificado
tags: [arquitectura, hardware, software, openmv, teensy, protocolo, sensores, analisis]
nota: "Actualizado post-verificación cruzada contra código fuente real. Hipótesis #12 REFUTADA. Agregados hallazgos N1, N2, N3."
---

# Arquitectura del Sistema de Robots — Temporada 2025

## Análisis Técnico Completo para Planificación 2026

**Equipo**: IITA RoboCup Junior Soccer Open (ahora Soccer Vision)
**Autor del análisis**: Claude (Anthropic — Claude Opus 4.6)
**Supervisión**: Gustavo Viollaz (@gviollaz)
**Fecha**: 21 de febrero de 2026
**Última actualización**: 21 de febrero de 2026 — post-verificación cruzada contra código fuente
**Fuentes**: Repositorios `IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025` y `IITA-Proyectos/open-soccer-robocup-team2026`

> ⚠️ **Estado de verificación**: Este documento fue verificado línea por línea contra el código fuente real de ambos repositorios (2025 y 2026). Se corrigió la hipótesis #12 (REFUTADA) y se agregaron 3 hallazgos nuevos (N1, N2, N3). Ver [análisis cruzado completo](2026-02-21-analisis-cruzado-verificacion-hipotesis.md) para metodología y detalles.

---

## 1. Resumen Ejecutivo

El sistema 2025 consiste en dos robots (arquero y delantero) construidos sobre la placa controladora Zircon (diseño propio en dos variantes: Mark1 y Naveen1) con microcontrolador Teensy 4.1, cámara OpenMV H7 como sensor de visión primario, giroscopio BNO055 para orientación, sensores de línea reflectivos para detección de límites, sensores IR para detección de pelota, y un sensor ultrasónico HC-SR04 para distancia en el arquero.

El equipo ganó el campeonato nacional en diciembre 2025 con este sistema. Sin embargo, el análisis revela **problemas estructurales significativos** en software que deben resolverse antes de competir internacionalmente en Incheon (junio-julio 2026).

### Hallazgos

Se identificaron **23 puntos de falla originales** + **3 hallazgos nuevos de verificación** categorizados en: bugs de software (8), deficiencias de diseño (7), vulnerabilidades de protocolo (5), riesgos por cambios de reglas 2026 (3), y hallazgos de verificación cruzada (3).

De los 23 originales: **19 confirmados**, **3 parcialmente confirmados**, **1 refutado** (#12).

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

**⚠️ Punto de falla #1** *(parcialmente confirmado)*: La detección de versión usa `INPUT_PULLDOWN` (no pin puramente flotante). El pulldown interno de ~100kΩ del Teensy 4.1 mitiga significativamente el riesgo de ruido. Es razonablemente confiable si el pin está conectado a VCC (Naveen1) o abierto (Mark1). El riesgo existe pero es menor al indicado originalmente.

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

#### 3.4.2 Sensores de Pelota IR (x8 — Fotodiodos infrarrojos)

- **Cantidad**: 8 fotodiodos distribuidos en anillo alrededor del robot
- **Tipo**: Fotodiodos IR que detectan la señal pulsada de la pelota (40 kHz modulada)
- **Lectura**: `readBall(n)` → `1024 - analogRead(ballpin_n)` → Valor invertido (mayor = más cerca)
- **Distribución**: 8 sensores a 45° cada uno para cobertura de 360°

**⚠️ Punto de falla #5**: En el código actual del delantero (perseguir-pelota.ino), los sensores IR **no se usan**. Toda la detección de pelota viene exclusivamente de la cámara OpenMV. Esto significa que si la pelota está detrás del robot (fuera del campo de visión de la cámara), el delantero no la detecta. Los 8 sensores IR están instalados pero **desperdiciados** en el programa de competencia.

#### 3.4.3 Giroscopio: Adafruit BNO055

- **Chip**: Bosch BNO055 — IMU de 9 ejes con fusión sensorial integrada
- **Interfaz**: I2C (dirección 0x28)
- **Dato utilizado**: `orientation.x` (Euler heading, 0-360°)
- **Función de lectura**: `readCompass()` en zirconLib (requiere `compassCalibrated = true`)
- **Control proporcional**: `error = currentYaw - initialYaw`, con wrapping ±180° y `kp = 0.3`

**⚠️ Punto de falla #6** *(confirmado)*: La variable `compassCalibrated` se inicializa como `false` y **nunca se establece como `true`** en `zirconLib.cpp`. La función `readCompass()` devuelve siempre 0 y imprime "Compass not calibrated!". En `lateral_con_giróscopo`, el BNO055 se usa **directamente** sin pasar por `readCompass()`, accediendo al objeto `bno` directamente.

**Hallazgo adicional de verificación**: En el archivo original "para que persiga la pelota" (2025), el BNO055 fue **DELIBERADAMENTE COMENTADO**. El equipo intentó control de heading pero lo desactivó antes de la competencia. El robot ganó el nacional SIN control de orientación.

#### 3.4.4 Sensor Ultrasónico: HC-SR04 (solo arquero)

- **Tipo**: Sensor de distancia por ultrasonido
- **Pines**: TRIG=9, ECHO=10
- **Función**: `medirDistancia()` → distancia en cm
- **Uso**: Determinar proximidad al arco para el control de posición del arquero

**⚠️ Punto de falla #7**: La función `pulseIn(ECHO, HIGH)` es **bloqueante** — detiene todo el programa hasta 1 segundo si no hay eco.

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

### 4.2 Detección de Objetos por Color (Blob Detection)

El sistema detecta hasta 3 objetos por color usando thresholds LAB:

| Objeto | Threshold LAB (L_min, L_max, A_min, A_max, B_min, B_max) | Archivo |
|--------|-----------------------------------------------------------|---------|
| Pelota naranja | (30, 60, 20, 60, 10, 50) | calcula-coordenadas |
| Pelota naranja | (76, 18, 13, 88, 6, 127) | enviar 2 arcos (**diferente!**) |
| Arco amarillo | (0, 79, -22, -8, 46, 127) | enviar 2 arcos |
| Arco azul | (31, 19, -36, 60, -61, 5) | enviar 2 arcos |

**⚠️ Punto de falla #8-9** *(confirmado)*: En los thresholds `(76, 18, ...)` y `(31, 19, ...)`, L_min es **mayor** que L_max. OpenMV `find_blobs()` espera L_min < L_max. Valores invertidos rompen la detección de blobs.

### 4.3 Transformación Homográfica (Píxeles → Centímetros)

El sistema convierte coordenadas de píxeles (u,v) a coordenadas físicas (x,y en cm) usando una **matriz de homografía 3x3**.

**⚠️ Punto de falla #10**: Existen **dos matrices de homografía diferentes** en el código — calibradas para montajes de cámara diferentes (h=10cm vs h=18.7cm).

### 4.4 Indicadores LED

El código de visión más avanzado usa los LEDs integrados del OpenMV como indicadores:
- **LED Rojo**: Pelota naranja detectada
- **LED Verde**: Arco amarillo detectado
- **LED Azul**: Arco azul detectado

---

## 5. Protocolo de Comunicación OpenMV → Teensy

### 5.1 Capa Física

| Parámetro | Valor |
|-----------|-------|
| Interfaz | UART (Serial asíncrono) |
| OpenMV TX | UART3 (OpenMV) → Pin 0 RX1 (Teensy) |
| Baud rate | 19200 bps (versión final) / 115200 bps (versión anterior) |
| Formato | 8N1 (8 bits, sin paridad, 1 stop bit) |

**⚠️ Punto de falla #11** *(parcialmente confirmado)*: El par funcional principal usa el MISMO baud rate (19200 en ambos lados). Sin embargo, existen archivos alternativos/anteriores con 115200, lo que representa un riesgo de configuración si se carga la versión incorrecta.

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

### 5.4 Análisis de Separación Header/Dato

> **⚠️ CORRECCIÓN — Punto de falla #12 REFUTADO** (actualización post-verificación, 21 feb 2026)
>
> El análisis original afirmaba que los headers (201-204) podían colisionar con valores de datos legítimos. **Esto es INCORRECTO**. La verificación del código fuente real demuestra que el protocolo fue diseñado con separación intencional:
>
> ```python
> # OpenMV — codificación de datos
> byteXp = min(max(int(Xp * 2), 0), 200)   # Rango: 0–200
> byteYp = min(max(int((Yp + 50) * 2), 0), 200)  # Rango: 0–200
> # Headers: 201, 202, 203 → FUERA del rango de datos
> ```
>
> Los datos se limitan explícitamente al rango 0–200 con `min(max(...), 200)`. Los headers (201, 202, 203, 204) están **fuera del rango de datos** por diseño. No hay posibilidad de colisión header/dato.
>
> **Problemas reales del protocolo que SÍ persisten:**
> - #13: Sin checksum ni CRC → corrupción de datos no detectada
> - #14: Lectura de bloque fijo (`Serial1.available() >= 6`) → desincronización permanente ante pérdida de byte
> - Sin mecanismo de resincronización
> - Sin timeout de protocolo

**Sugerencia para protocolo 2026**:
```
[0xFF][LENGTH][Xp_hi][Xp_lo][Yp_hi][Yp_lo][Xa_hi][Xa_lo][Ya_hi][Ya_lo][CHECKSUM]
```
Con byte de inicio fijo (0xFF), longitud, resolución de 16 bits, y checksum XOR.

---

## 6. Capa de Software — Teensy (Control)

### 6.1 Programa del Arquero

**⚠️ Punto de falla #15** *(confirmado — peor de lo esperado)*: El archivo original del arquero (6.7KB) tiene:
- Variable `potencia` usada pero nunca declarada
- Funciones `leerGiroscopio()`, `avanzarDerecha()`, `avanzarIzquierda()`, `corregirAngulo()` nunca definidas
- Código ejecutable FUERA de funciones (después de que `loop()` cierra)
- Variables `s1`, `s2`, `s3` declaradas DOS VECES (global y local)
- `enum Direccion` declarado DOS VECES
- DOS funciones `setup()` en el mismo archivo
- Variables `verde_izq`, `verde_cen`, `verde_der` usadas pero nunca definidas
- Error de sintaxis: `blanco_s1!` en lugar de `!blanco_s1`

El archivo es claramente un work-in-progress con múltiples iteraciones mezcladas, NO un programa funcional.

**⚠️ Punto de falla #16** *(confirmado — peor de lo documentado)*: Los sensores de línea se leen como `int s1 = readLine(1);` a nivel global. `readLine()` llama `analogRead(linepin)`, pero los pines se configuran en `InitializeZircon()` dentro de `setup()`. Las variables globales se inicializan ANTES de `setup()`, por lo que `readLine()` lee pines NO CONFIGURADOS. Los valores s1, s2, s3 son BASURA y nunca se actualizan.

**⚠️ Punto de falla #17**: La función `Adelante()` usa variables `static` que la convierten en función de "una sola vez".

### 6.2 Programa del Delantero

**Máquina de estados**:

| Estado | Acción | Transición |
|--------|--------|-----------|
| GIRANDO | Rotar en lugar | → AVANZANDO si hay pelota |
| AVANZANDO | Avanzar al frente | → CENTRANDO si pelota cerca; → GIRANDO si pierde pelota |
| CENTRANDO | Alinear arco y pelota | → PATEANDO si alineados |
| PATEANDO_adelante | Full power 2 seg | → GIRANDO |

**⚠️ Punto de falla #18** *(confirmado)*: En PATEANDO, la condición de timeout está **invertida**:
```c
if(millis() - millis_inicio_estado <= 2000) {  // ← debería ser >=
```
El `<=` hace que los motores se apaguen inmediatamente en el primer ciclo. El robot nunca patea realmente.

**⚠️ Punto de falla #19** *(confirmado)*: El cálculo de ángulo del arco usa coordenadas de la pelota:
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

**⚠️ Punto de falla #21**: No hay funciones de movimiento de alto nivel.

### 6.4 Código del Dribbler

**⚠️ Punto de falla #22** *(confirmado)*: El dribbler espera un string "pelota detectada" por Serial que NINGÚN otro programa envía. Además:
- Usa `Serial` (USB) en lugar de `Serial1` (UART desde OpenMV)
- `readStringUntil()` bloqueante con 1 segundo de timeout
- `delay(2000)` dentro del bloque detiene todo por 2 segundos

El dribbler NUNCA se activó automáticamente durante la competencia.

**⚠️ Punto de falla #23 — REGLAS 2026**: La zona de captura de pelota se reduce de 3.0 cm a **1.5 cm**. Hay que verificar que con el dribbler activo la pelota no penetre más de 1.5 cm.

---

## 7. Hallazgos Nuevos de Verificación Cruzada

> Los siguientes hallazgos fueron descubiertos durante la verificación cruzada contra código fuente real (21 feb 2026) y NO aparecían en el análisis original ni en el análisis de ChatGPT.

### 🆕 N1 — Conflicto Pin 0/RX1 en modo Naveen1 (SEVERIDAD: ALTA)

En `zirconLib.cpp`, las variables `motor1pwm`, `motor2pwm`, `motor3pwm` se declaran como `int` globales (valor por defecto 0). En modo Naveen1, estas variables **nunca se asignan** porque Naveen1 usa solo 2 pines por motor, sin PWM separado.

Sin embargo, `initializePins()` ejecuta:

```cpp
pinMode(motor1pwm, OUTPUT);  // motor1pwm = 0 → pinMode(0, OUTPUT)
pinMode(motor2pwm, OUTPUT);  // motor2pwm = 0 → pinMode(0, OUTPUT)
pinMode(motor3pwm, OUTPUT);  // motor3pwm = 0 → pinMode(0, OUTPUT)
```

**Pin 0 en Teensy 4.1 es RX1 (Serial1 receive)**. Configurar RX1 como OUTPUT podría romper la comunicación UART desde la cámara OpenMV.

**Factor mitigante**: `Serial1.begin(19200)` se llama DESPUÉS de `InitializeZircon()`, lo que reconfigura el pin para UART. Funciona por accidente, pero es un bug latente que puede manifestarse si cambia el orden de inicialización.

**Corrección recomendada**: En modo Naveen1, no llamar `pinMode()` sobre variables PWM no asignadas, o asignarles un pin dummy/no-conectado.

### 🆕 N2 — Código migrado significativamente truncado (SEVERIDAD: MEDIA)

Comparación entre archivos originales (repo 2025) y migrados (repo 2026):

| Archivo | Original (2025) | Migrado (2026) | Diferencia |
|---------|-----------------|----------------|------------|
| Arquero | 6,656 bytes (completo) | 2,626 bytes (parcial) | Falta toda la lógica de oscilación |
| calibrar-threshold.py | 7,087 bytes (herramienta completa) | 901 bytes (solo comentario) | Solo stub apuntando al repo original |
| giro-y-avance-zircon.ino | ~4 KB (estimado) | 476 bytes (solo comentario) | Solo stub |
| junta-control-y-movilidad.ino | ~6 KB (estimado) | 489 bytes (solo comentario) | Solo stub |

**Archivos del repo 2025 COMPLETAMENTE AUSENTES en 2026**:
- `codigo de movilidad con cámara y control` (6.1KB — la integración Teensy+OpenMV más completa)
- `avance lateral tiempo` (4.1KB)
- `lateral_con_giróscopo` (2.4KB — el único código funcional del BNO055)
- `enviar paq. de datos` (2.3KB — protocolo V1)
- `enviar cordenadas 2 arcos y pelota` (6.7KB — la versión más avanzada del OpenMV)
- `enviar coordenadas pelota(con redondez)` (3.0KB)
- `enviar coordenadas 1 arco y pelota` (5.1KB)
- `Enviar paquete de datos solo pelota` (4.0KB)
- `Calibrar_Treshold.py` (7.1KB — herramienta de calibración completa)
- `UART Teensy` (1.6KB)
- `probar sensores de linea` (1.9KB)
- `ultimo dribbler` (1.0KB)
- Carpetas: `ARQUERO/`, `DELANTERO/`, `Dribbler/`, `OpenMV/H7/`, `OpenMV/H7 plus/`
- Archivos STL y diseños 3D

**Acción requerida**: Migrar TODOS los archivos completos del repo 2025, sin stubs.

### 🆕 N3 — Código de competencia probablemente NO está en el repositorio (SEVERIDAD: ALTA)

Evidencia convergente de que el código que REALMENTE CORRIÓ en el campeonato nacional difiere del repositorio:

1. El delantero nunca patea (bug #18 timeout invertido) — un equipo campeón no gana sin patear
2. El ángulo del arco es inútil (bug #19) — el centrado no funciona sin ángulo correcto
3. El arquero no compila — no puede haber corrido tal cual
4. El BNO055 está comentado — el giroscopio estaba deshabilitado

**Explicaciones posibles**:
- a) El código del repo es versión de desarrollo, modificada manualmente antes de cargar a los robots sin commitear los cambios finales
- b) Existían versiones locales en las computadoras del equipo que no se subieron
- c) Los bugs del delantero se compensaron con hardware (dribbler manual, etc.) y la estrategia era más simple de lo que el código sugiere

**Observación crítica**: El repositorio NO refleja con precisión lo que funcionó en competencia. El primer paso debería ser reconstruir la versión exacta que corrió en cada robot durante el nacional.

**Acción requerida**: Sesión con María Virginia y Elías para reconstruir las versiones exactas de competencia.

---

## 8. Resumen de Puntos de Falla (Actualizado post-verificación)

### Críticos (impiden funcionamiento correcto)

| # | Estado | Componente | Problema |
|---|--------|-----------|----------|
| 6 | ✅ Confirmado | zirconLib | `compassCalibrated` siempre false — giroscopio inaccesible vía librería |
| 8-9 | ✅ Confirmado | OpenMV | Thresholds con L_min > L_max — detección de color rota |
| ~~12~~ | ❌ **REFUTADO** | ~~Protocolo UART~~ | ~~Colisión header/dato~~ → Headers 201-204 están fuera del rango de datos 0-200 por diseño |
| 13-14 | ✅ Confirmado | Protocolo UART | Sin checksum, sin resync — datos corruptos sin detección |
| 15-16 | ✅ Confirmado (peor) | Arquero | Código no compila, sensores leen pines no configurados |
| 18 | ✅ Confirmado | Delantero | Condición de pateo invertida (≤ en vez de ≥) — robot nunca patea |
| 19 | ✅ Confirmado | Delantero | Ángulo arco calcula con datos de pelota — variable inútil |
| **N1** | 🆕 Nuevo | zirconLib | Pin 0/RX1 configurado como OUTPUT en modo Naveen1 → puede romper UART |
| **N3** | 🆕 Nuevo | General | Código de competencia probablemente no está en el repositorio |

### Altos (degradan rendimiento significativamente)

| # | Estado | Componente | Problema |
|---|--------|-----------|----------|
| 2 | ✅ Confirmado | Motores | Sin función moveOmni() unificada |
| 3 | ✅ Confirmado | Línea | Thresholds hardcodeados |
| 5 | ✅ Confirmado | Sensores IR | 8 sensores instalados pero no usados |
| 7 | ✅ Confirmado | Ultrasónico | pulseIn() bloqueante |
| 11 | ⚠️ Parcial | OpenMV/UART | Par funcional sincronizado, pero archivos alternativos con baud diferente |
| 22 | ✅ Confirmado | Dribbler | Activación por string serial que nadie envía |
| **N2** | 🆕 Nuevo | Migración | Código migrado truncado, archivos críticos ausentes |

### Moderados (riesgos para 2026)

| # | Estado | Componente | Problema |
|---|--------|-----------|----------|
| 1 | ⚠️ Parcial | Zircon | Detección versión por pulldown (mitigado vs flotante puro) |
| 4 | ✅ Confirmado | Línea | Solo 3 sensores |
| 17 | ✅ Confirmado | Arquero | Función Adelante() con variables static |
| 20 | ✅ Confirmado | Delantero | avanzarAlFrente() no es realmente adelante |
| 21 | ✅ Confirmado | zirconLib | No hay funciones de movimiento de alto nivel |
| 23 | ✅ Confirmado | Dribbler | Zona de captura 3→1.5 cm en reglas 2026 |

---

## 9. Métricas de Fiabilidad del Análisis

| Métrica | Resultado |
|---------|----------|
| Hipótesis verificadas correctas | 19/23 (83%) |
| Hipótesis parcialmente confirmadas | 3/23 (13%) |
| Hipótesis refutadas | 1/23 (4%) — #12 colisión header/dato |
| Hallazgos nuevos descubiertos | 3 (N1, N2, N3) |
| Total de problemas documentados | 25 (22 confirmados + 3 nuevos) |

Ver documento completo de verificación: [análisis cruzado](2026-02-21-analisis-cruzado-verificacion-hipotesis.md)

---

## 10. Recomendaciones para 2026 (Revisadas post-verificación)

Ver **[Mapa de Prioridades Revisado](2026-02-21-mapa-prioridades-revisado.md)** para el plan de acción completo con 4 niveles de prioridad (P0-P3).

### Arquitectura de Software Propuesta para 2026

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

## 11. Inventario de Sensores del Sistema

| Sensor | Cant. | Tipo | Interfaz | Ubicación | Usado en 2025 |
|--------|-------|------|----------|-----------|---------------|
| OpenMV H7 | 1 | Cámara RGB | UART a Teensy | Superior, mirando al frente | ✅ Delantero |
| BNO055 | 1 | IMU 9-ejes | I2C | En la Zircon PCB | ⚠️ Comentado antes de competencia |
| HC-SR04 | 1 | Ultrasónico | GPIO | Frontal (arquero) | ✅ Solo arquero |
| IR Ball | 8 | Fotodiodo IR | ADC analógico | Anillo 360° | ❌ No usados en código final |
| Line | 3 | Reflectivo analógico | ADC analógico | Inferior (izq/centro/der) | ✅ Arquero |
| Botones | 2 | Pulsador digital | GPIO digital | Accesibles externamente | Parcialmente |
| Dribbler | 1 | Motor DC con H-bridge | PWM + DIR | Frontal inferior | ❌ Sin activación automática |

---

*Documento generado por Claude (Anthropic — Claude Opus 4.6) bajo supervisión de Gustavo Viollaz (@gviollaz), 21 de febrero de 2026.*
*Actualizado post-verificación cruzada contra código fuente real, 21 de febrero de 2026.*
*Fuente: análisis de código completo de los repositorios IITA-Proyectos.*