---
title: "Análisis Técnico del Giróscopo BNO055 para RoboCup Junior Soccer"
date: 2026-03-20
author: "Claude (Anthropic) bajo supervisión de Gustavo Viollaz"
ai-assisted: true
status: final
tags: [giroscopo, bno055, imu, calibracion, heading, confiabilidad]
---

# Análisis Técnico del Giróscopo BNO055

## Contexto

Este documento analiza el sensor BNO055 de Bosch tal como se usa en los robots del equipo IITA Salta para RoboCup Junior Soccer Open 2026. El objetivo es entender los problemas actuales y proponer una inicialización confiable para competencia.

### Problemas observados en competencia
1. **Offset de ~35° al encender** — El heading no arranca en 0°, hay que poner el robot chueco para compensar.
2. **Drift acumulado durante partidos** — La orientación se desvía varios grados durante los 10 minutos de juego.
3. **Interferencia magnética de motores** — Los 3 motores DC generan campos magnéticos que afectan al magnetómetro.
4. **Recalibración espontánea por impactos** — Choques con otros robots pueden alterar la calibración.

---

## 1. EL SENSOR BNO055: QUÉ ES Y CÓMO FUNCIONA

### 1.1 Arquitectura interna

El BNO055 de Bosch es un System-in-Package (SiP) que contiene:
- **Acelerómetro triaxial** — mide aceleración lineal + gravedad
- **Giróscopo triaxial** — mide velocidad angular (rotación)
- **Magnetómetro triaxial** — mide campo magnético terrestre (brújula)
- **Procesador ARM Cortex-M0** — corre algoritmos de fusión sensorial *dentro del chip*

Lo que hace especial al BNO055 es que la fusión sensorial es interna: el chip combina los 3 sensores y entrega directamente ángulos de orientación (Euler o quaternion). No hay que implementar Kalman, Madgwick ni nada por fuera.

### 1.2 Modos de operación relevantes

El BNO055 tiene múltiples modos. Los tres relevantes para nosotros son:

| Modo | Sensores usados | Heading | Drift | Magnetómetro | Uso recomendado |
|------|----------------|---------|-------|-------------|----------------|
| **NDOF** | Accel + Gyro + Mag | Absoluto (norte magnético) | Bajo (el mag corrige) | **SÍ** | Ambientes sin interferencia magnética |
| **NDOF_FMC_OFF** | Accel + Gyro + Mag | Absoluto | Bajo | **SÍ** (sin Fast Mag Calib) | Ambientes con interferencia moderada |
| **IMUPLUS** | Accel + Gyro | Relativo (al encendido) | **Acumulativo** | **NO** | Ambientes con interferencia magnética fuerte |

### 1.3 El dilema NDOF vs IMUPLUS

Este es **el punto crítico de diseño** para nuestros robots:

**NDOF (modo actual)**:
- Ventaja: el magnetómetro corrige el drift del giróscopo. En teoría, el heading no se desvía con el tiempo.
- Problema: los **motores DC generan campos magnéticos** que confunden al magnetómetro. El chip "corrige" el heading hacia donde apunta el campo de los motores, no hacia donde el robot realmente mira. Esto causa saltos y desvíos erráticos.
- Problema: el BNO055 se **recalibra automáticamente en background**. Un impacto puede hacer que recalibre con valores incorrectos.

**IMUPLUS**:
- Ventaja: ignora el magnetómetro completamente. No hay interferencia de motores.
- Problema: sin magnetómetro, el heading acumula drift del giróscopo con el tiempo. El datasheet indica un zero-rate offset de hasta ±3°/s en modo gyro-only, pero en modo fusión el acelerómetro compensa parcialmente.
- En la práctica: el drift en IMUPLUS es del orden de **1-5° por minuto** dependiendo de la calidad del chip y la temperatura.

**Decisión recomendada para nuestros robots:**

> **Usar IMUPLUS** como modo principal. Para un partido de 10 minutos, un drift de 5-15° es manejable con recalibración periódica usando la cámara (ver sección 5). La alternativa (NDOF con motores) causa errores mucho más graves e impredecibles.

Esta es la misma conclusión a la que llegan otros equipos de robótica con motores DC. La comunidad de RoboCup y robótica móvil documenta extensivamente que IMUPLUS es preferible cuando hay motores cerca del sensor.

---

## 2. ANÁLISIS DEL CÓDIGO ACTUAL

### 2.1 Inicialización actual (ambos robots)

```cpp
// En setup():
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);

if (!bno.begin()) {
    Serial.println("¡No se pudo encontrar el BNO055!");
    while (1);  // BLOQUEO TOTAL si no se detecta
}
bno.setExtCrystalUse(true);

sensors_event_t event;
bno.getEvent(&event);
initialYaw = event.orientation.x;  // Captura heading como offset
```

### 2.2 Lectura en loop():

```cpp
sensors_event_t event;
bno.getEvent(&event);
currentYaw = event.orientation.x;
error = currentYaw - initialYaw;
if (error > 180) error = error - 360;
if (error < -180) error = error + 360;
correccion = error * kp;
```

### 2.3 Problemas identificados

**P1 — Modo NDOF por defecto (no se especifica IMUPLUS)**

`bno.begin()` sin parámetro arranca en **NDOF** (modo por defecto de la librería Adafruit). Esto activa el magnetómetro, que es exactamente lo que NO queremos con motores DC en el robot.

```cpp
// Actual (NDOF implícito):
bno.begin();

// Correcto para nuestro caso:
bno.begin(Adafruit_BNO055::OPERATION_MODE_IMUPLUS);
```

**P2 — El offset se captura instantáneamente sin esperar estabilización**

Después de `bno.begin()`, se lee `initialYaw` inmediatamente. El BNO055 necesita al menos 650ms después de inicializar para estabilizarse, y el giróscopo necesita unos segundos quieto para calibrar su zero-rate offset.

El `delay(100)` que a veces se pone es insuficiente. Esto explica parte de los 35° de desviación.

**P3 — No se espera calibración del giróscopo**

El BNO055 calibra el giróscopo automáticamente cuando está quieto. Pero el código no verifica el estado de calibración antes de leer el heading inicial. Si el robot se mueve durante el encendido, el giróscopo no calibra y los valores son erráticos.

**P4 — No se guardan/cargan offsets de calibración**

Cada vez que se enciende el robot, el BNO055 parte de cero. Se podrían guardar los offsets de calibración del acelerómetro y giróscopo en EEPROM y cargarlos al encender, acelerando la calibración.

**P5 — `while(1)` si el BNO055 no responde**

Si hay un problema de I2C (cable suelto, ruido), el robot se cuelga para siempre. En competencia esto es fatal.

**P6 — El arquero usa `currentYaw` raw en vez de `error`**

Documentado en el análisis del arquero. Las comparaciones `currentYaw <= 10 or currentYaw >= 350` solo funcionan si el robot apunta al norte al encender.

---

## 3. PROCEDIMIENTO DE INICIALIZACIÓN RECOMENDADO

Basado en el datasheet de Bosch, la experiencia de otros equipos de RoboCup (BohleBots BNO055 library), y las mejores prácticas documentadas:

### 3.1 Secuencia completa

```cpp
// === CONSTANTES ===
const int BNO_TIMEOUT_MS = 3000;       // timeout para detección del sensor
const int BNO_ESTABILIZACION_MS = 1000; // tiempo para estabilización post-init
const int BNO_GYRO_CALIB_MS = 2000;    // tiempo para calibración del gyro
const int LECTURAS_PROMEDIO = 10;       // lecturas para promediar heading inicial

// === VARIABLES ===
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);
float headingOffset = 0;
bool bnoOK = false;

void inicializarBNO() {
    // Paso 1: Detectar sensor con timeout
    unsigned long inicio = millis();
    while (!bno.begin(Adafruit_BNO055::OPERATION_MODE_IMUPLUS)) {
        if (millis() - inicio > BNO_TIMEOUT_MS) {
            Serial.println("ERROR: BNO055 no detectado!");
            bnoOK = false;
            return;  // NO bloquear, seguir sin giróscopo
        }
        delay(100);
    }
    bnoOK = true;

    // Paso 2: Usar cristal externo si está disponible
    bno.setExtCrystalUse(true);

    // Paso 3: Esperar estabilización post-init
    // El datasheet dice 650ms mínimo. Usamos 1000ms.
    Serial.println("BNO055: esperando estabilizacion...");
    delay(BNO_ESTABILIZACION_MS);

    // Paso 4: Esperar que el giróscopo calibre (robot QUIETO)
    Serial.println("BNO055: calibrando giroscopo (no mover!)...");
    esperarCalibracionGyro();

    // Paso 5: Capturar heading inicial promediando múltiples lecturas
    capturarHeadingInicial();

    Serial.print("BNO055: listo. Heading offset = ");
    Serial.println(headingOffset);
}

void esperarCalibracionGyro() {
    unsigned long inicio = millis();
    while (millis() - inicio < BNO_GYRO_CALIB_MS) {
        uint8_t sys, gyro, accel, mag;
        bno.getCalibration(&sys, &gyro, &accel, &mag);

        // Si gyro alcanzó calibración 3, listo
        if (gyro >= 3) {
            Serial.print("Gyro calibrado en ");
            Serial.print(millis() - inicio);
            Serial.println("ms");
            return;
        }
        delay(100);
    }
    // Si no calibró en el tiempo, seguir igual (mejor que nada)
    Serial.println("ADVERTENCIA: Gyro no alcanzó calibración completa");
}

void capturarHeadingInicial() {
    float suma = 0;
    for (int i = 0; i < LECTURAS_PROMEDIO; i++) {
        sensors_event_t event;
        bno.getEvent(&event);
        suma += event.orientation.x;
        delay(20);
    }
    headingOffset = suma / LECTURAS_PROMEDIO;
}
```

### 3.2 Lectura corregida en loop()

```cpp
float leerHeading() {
    if (!bnoOK) return 0;  // degradación elegante

    sensors_event_t event;
    bno.getEvent(&event);
    float heading = event.orientation.x - headingOffset;

    // Normalizar a -180..+180
    if (heading > 180) heading -= 360;
    if (heading < -180) heading += 360;

    return heading;
}
```

### 3.3 Recalibración por botón (para competencia)

```cpp
void loop() {
    // Botón 1: recalibrar heading (el robot debe estar mirando al arco)
    if (readButton(1) == HIGH) {
        capturarHeadingInicial();
        Serial.println("Heading recalibrado!");
        delay(500); // anti-rebote
    }
    // ... resto del loop
}
```

Esto permite al chico presionar un botón en la cancha antes de que empiece el partido, asegurando que 0° = dirección al arco contrincante. Sin necesidad de poner el robot chueco.

---

## 4. EL PROBLEMA DEL DRIFT Y CÓMO MANEJARLO

### 4.1 Cuánto drift esperar

En modo IMUPLUS, el drift típico del BNO055 es:
- **Estacionario**: ~0.5°/minuto (excelente)
- **En movimiento con motores**: ~2-5°/minuto (aceptable para un partido)
- **Con impactos fuertes**: puede saltar varios grados instantáneamente

Para un partido de 10 minutos: esperar 20-50° de drift acumulado sin corrección.

### 4.2 Estrategia de compensación del drift

El drift se puede compensar con **recalibración periódica usando la cámara**:

1. **Cuando el robot ve el arco contrincante centrado en la cámara**, sabe que está mirando al frente. Puede recalibrar el heading a 0° en ese instante.
2. **Cuando ve el arco propio**, sabe que está mirando hacia atrás (~180°).
3. **La recalibración por visión no reemplaza al giróscopo** — complementa. El giróscopo da orientación instantánea a alta velocidad (100Hz). La cámara da referencia absoluta a baja velocidad (cuando ve un arco).

```cpp
// Pseudocódigo de recalibración por visión:
void recalibrarPorVision() {
    // Si veo el arco contrincante centrado (Yam ~ 0)
    if (hayarco_amarillo && abs(Yam) < 10) {
        // El robot mira al arco → heading debería ser ~0°
        float headingActual = leerHeadingRaw();
        headingOffset = headingActual;  // recalibrar
    }
}
```

### 4.3 Hardware: ubicación física del sensor

Para minimizar interferencia magnética (relevante si se vuelve a NDOF en el futuro):
- Montar el BNO055 lo **más lejos posible** de los motores (parte superior del robot)
- Usar un cable largo de I2C si es necesario
- Separar la alimentación de motores de la del sensor (filtro LC o regulador separado)
- El blindaje magnético (lámina de mu-metal) es efectivo pero complicado de implementar

---

## 5. SECUENCIA COMPLETA DE ENCENDIDO PARA COMPETENCIA

Procedimiento físico recomendado para el día de la competencia:

1. **Encender el robot en el piso de la cancha**, apuntando al arco donde debe patear
2. **Esperar 3-4 segundos** sin mover el robot (el LED debería indicar que calibró)
3. **Presionar botón de calibración** (captura heading = 0°)
4. **Colocar en posición de inicio** del partido
5. Durante el partido, la recalibración por visión corrige el drift automáticamente

### Indicación visual con LED

Usar el LED integrado para comunicar el estado:
```cpp
void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    // LED parpadeando = inicializando
    for (int i = 0; i < 5; i++) {
        digitalWrite(LED_BUILTIN, HIGH); delay(100);
        digitalWrite(LED_BUILTIN, LOW); delay(100);
    }

    inicializarBNO();

    // LED fijo = listo para calibrar
    digitalWrite(LED_BUILTIN, HIGH);

    // Esperar botón de calibración
    Serial.println("Presionar boton cuando el robot apunte al arco");
    while (readButton(1) == LOW) {
        delay(50);
    }
    capturarHeadingInicial();

    // LED apagado = en juego
    digitalWrite(LED_BUILTIN, LOW);
}
```

---

## 6. GUARDADO DE OFFSETS EN EEPROM (MEJORA AVANZADA)

El BNO055 pierde su calibración al apagarse. Se pueden guardar los offsets de calibración en la EEPROM del Teensy y restaurarlos al encender para acelerar la calibración:

```cpp
#include <EEPROM.h>

// Guardar offsets (llamar después de calibración exitosa)
void guardarOffsets() {
    adafruit_bno055_offsets_t offsets;
    bno.getSensorOffsets(offsets);
    EEPROM.put(0, offsets);  // 22 bytes
    Serial.println("Offsets guardados en EEPROM");
}

// Cargar offsets (llamar durante inicialización)
void cargarOffsets() {
    adafruit_bno055_offsets_t offsets;
    EEPROM.get(0, offsets);

    // Cambiar a modo CONFIG para escribir offsets
    bno.setMode(Adafruit_BNO055::OPERATION_MODE_CONFIG);
    delay(25);
    bno.setSensorOffsets(offsets);
    // Volver a modo operativo
    bno.setMode(Adafruit_BNO055::OPERATION_MODE_IMUPLUS);
    delay(25);
    Serial.println("Offsets cargados desde EEPROM");
}
```

Esta técnica la usa la librería **BohleBots_BNO055**, creada específicamente para RoboCup Junior, y es el approach estándar en equipos de competencia.

---

## 7. COMPARATIVA: NUESTRO APPROACH ACTUAL vs PROPUESTO

| Aspecto | Actual | Propuesto |
|---------|--------|----------|
| Modo | NDOF (implícito) | **IMUPLUS** (explícito) |
| Magnetómetro | Activo (interferido por motores) | **Desactivado** |
| Espera post-init | ~100ms | **≥1000ms** + espera calibración gyro |
| Captura heading | 1 lectura instantánea | **Promedio de 10 lecturas** |
| Si BNO055 falla | `while(1)` — robot muerto | **Degradación elegante**, sigue sin gyro |
| Recalibración | Ninguna | **Botón** + recalibración por visión |
| Indicación visual | Ninguna | **LED** para estado de calibración |
| Offsets EEPROM | No | **Opcional** (mejora avanzada) |
| Drift a 10 min | Impredecible (mag interferido) | **~20-50°** (compensable por visión) |

---

## 8. REFERENCIAS

### Datasheets y documentación oficial
- [BNO055 Datasheet (Bosch)](https://cdn-shop.adafruit.com/datasheets/BST_BNO055_DS000_12.pdf) — Referencia completa del sensor, modos de operación, calibración, registros.
- [BNO055 Quick Start Guide (Bosch Application Note)](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bno055-an007.pdf) — Procedimiento de calibración paso a paso, reuso de calibration profile.
- [Adafruit BNO055 Library (GitHub)](https://github.com/adafruit/Adafruit_BNO055) — Librería Arduino que usamos. Ver ejemplo `restore_offsets` para EEPROM.
- [Adafruit BNO055 Guide](https://learn.adafruit.com/adafruit-bno055-absolute-orientation-sensor) — Tutorial completo con diagramas de conexión y calibración.

### Librerías específicas para RoboCup
- [BohleBots_BNO055 (GitHub)](https://github.com/zischknall/BohleBots_BNO055) — Librería creada por el equipo BohleBots de Alemania para RoboCup Junior. Incluye: guardado/carga de offsets en EEPROM, detección de impactos con recarga automática de calibración, heading relativo con `setReference()`. Es la referencia más directa para nuestro caso de uso.
- [BohleBots_BNO055 en Arduino Library Manager](https://www.arduino.cc/reference/en/libraries/bohlebots_bno055/) — Instalable directamente desde Arduino IDE.

### Discusiones técnicas relevantes
- [IMUPLUS vs NDOF con motores (RobotShop Forum)](https://community.robotshop.com/forum/t/bno055-9-dof-absolute-orientation-imu-fusion-breakout-board-tutorial/67316) — Experiencia documentada: "IMUPLUS mode is used because the motors, encoder magnets, and indoor magnetic fields wreak havoc with the IMU in NDOF mode".
- [BNO055 drift issue (Adafruit Forum)](https://forums.adafruit.com/viewtopic.php?t=172435) — Discusión detallada sobre cambio de NDOF a IMUPLUS para evitar interferencia magnética, incluyendo código.
- [BNO055 loading calibration data (Adafruit Forum)](https://forums.adafruit.com/viewtopic.php?t=83965) — Procedimiento de guardado/carga de calibración en EEPROM con orden correcto de operaciones.
- [Using BNO055 to prevent robot angular drift (Arduino Forum)](https://forum.arduino.cc/t/using-bno055-to-prevent-robot-angular-drift/538486) — Caso de uso idéntico al nuestro con 4 motores DC.
- [BNO055 drift over time (All About Circuits)](https://forum.allaboutcircuits.com/threads/values-from-bno055-are-drifting-when-measuring-for-a-longer-period-of-time-imu.163392/) — Análisis de drift en modo IMUPLUS y NDOF a largo plazo.

### Reglas de competencia
- [RoboCup Junior Soccer Rules 2026](https://robocup-junior.github.io/soccer-rules/2026-soccer-draft-rules/rules.html) — Regla relevante: "Robots must not produce magnetic interference in other robots on the field."
