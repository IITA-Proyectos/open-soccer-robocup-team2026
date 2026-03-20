---
title: "Análisis del Código del Arquero - Legacy 2025"
date: 2026-03-20
author: "Claude (Anthropic) bajo supervisión de Gustavo Viollaz"
ai-assisted: true
status: final
tags: [arquero, legacy, analisis, bugs, mejoras]
---

# Análisis del Código del Arquero - Legacy 2025

## Archivos analizados

- `legacy/2025-season/code/arquero/arquero-base.ino`
- `legacy/2025-season/code/libraries/zirconLib/zirconLib.h`
- `legacy/2025-season/code/libraries/zirconLib/zirconLib.cpp`
- `legacy/2025-season/code/delantero/perseguir-pelota.ino`
- `legacy/2025-season/code/misc/README.md`

## Contexto

El arquero usa un Teensy con placa Zircon (versiones Mark1/Naveen1), 3 motores TT con drivers H-bridge, sensor ultrasónico HC-SR04, 3 sensores de línea analógicos, giróscopo BNO055, y 8 sensores IR de pelota. La cámara OpenMV se comunica por UART a 19200 baud.

---

## 🔴 BUGS CRÍTICOS

### BUG 1 — Sensores de línea leídos antes de inicializar pines

**Ubicación:** `arquero-base.ino`, líneas globales

```cpp
// Se ejecuta ANTES de setup()
int s1 = readLine(1);
int s2 = readLine(2);
int s3 = readLine(3);
```

Estas variables se inicializan en scope global, antes de que `setup()` llame a `InitializeZircon()`. Los pines no están configurados → `analogRead()` retorna valores basura. Además, como son globales estáticas, nunca se re-leen.

**Solución:** Mover la lectura dentro de `loop()`:
```cpp
void loop() {
  int s1 = readLine(1);
  int s2 = readLine(2);
  int s3 = readLine(3);
  // ...
}
```

---

### BUG 2 — Variable `potencia` no declarada

**Ubicación:** `arquero-base.ino`, función `girar()`

```cpp
motor1(potencia, direccionGiro);
```

`potencia` no está declarada en ningún lado del archivo. Esto no compila, o si compila por algún contexto no visible, su valor es indefinido.

**Solución:** Declarar al inicio del archivo:
```cpp
const int POTENCIA_GIRO = 80; // Ajustar según necesidad
```

---

### BUG 3 — BNO055 nunca se inicializa (RAÍZ del problema de los 35°)

**Ubicación:** `zirconLib.cpp`

```cpp
Adafruit_BNO055 bno;              // Se declara
boolean compassCalibrated = false; // Siempre false

void InitializeZircon() {
  setZirconVersion();
  initializePins();
  // ¡¡¡ FALTA bno.begin() !!!
  // ¡¡¡ FALTA compassCalibrated = true !!!
}
```

`bno.begin()` nunca se llama. `compassCalibrated` siempre es `false`. `readCompass()` siempre retorna 0.

Si el equipo lo usaba, probablemente inicializaban el BNO055 directamente en el `.ino` sin calibración ni offset → los 35° de desviación son el heading real respecto al norte magnético, no un error.

**Solución para zirconLib:**
```cpp
void InitializeZircon() {
  setZirconVersion();
  initializePins();
  if (bno.begin()) {
    bno.setExtCrystalUse(true);
    delay(100);
    compassCalibrated = true;
  } else {
    Serial.println("ERROR: BNO055 no detectado!");
  }
}
```

---

## 🟡 PROBLEMA 1 — Los robots no van derecho

**Diagnóstico:** Sin encoders, potencia fija a motores no garantiza velocidad igual. Asimetrías mecánicas, de batería y de piso causan desvío. No hay feedback de ningún tipo.

**Solución — Control P (proporcional) sobre heading con BNO055:**

```cpp
float headingObjetivo; // Se captura al iniciar movimiento recto

void avanzarRecto(int potenciaBase) {
  float headingActual = readHeadingCorregido();
  float error = headingActual - headingObjetivo;

  // Normalizar a -180..+180
  if (error > 180) error -= 360;
  if (error < -180) error += 360;

  float correccion = error * Kp; // Kp ~ 1.0 a 3.0

  int potIzq = constrain(potenciaBase + correccion, 0, 100);
  int potDer = constrain(potenciaBase - correccion, 0, 100);

  motor2(potIzq, 1);
  motor3(potDer, 1);
}
```

Todos los equipos competitivos de RoboCup Junior que no tienen encoders usan esta técnica. El BNO055 es ideal porque entrega heading absoluto fusionado.

---

## 🟡 PROBLEMA 2 — Offset de 35° al arrancar el giróscopo

**Diagnóstico:** El BNO055 en modo NDOF reporta heading relativo al norte magnético. Si el robot no apunta al norte, el heading inicial no es 0°. Esto es comportamiento normal, no un error.

**Solución — Capturar offset en setup():**

```cpp
float headingOffset = 0;

void calibrarHeading() {
  headingOffset = readCompass();
}

float readHeadingCorregido() {
  float raw = readCompass();
  float corregido = raw - headingOffset;
  if (corregido < 0) corregido += 360;
  if (corregido >= 360) corregido -= 360;
  return corregido;
}
```

Llamar `calibrarHeading()` al final de `setup()` o al presionar un botón. Así 0° = la dirección donde el robot apunta al calibrar. No hace falta ponerlo chueco.

**Mejor aún para competencia:** Usar un botón para calibrar en la cancha.

---

## 🟠 PROBLEMAS DE CONFIABILIDAD

### R1 — `pulseIn()` es bloqueante

```cpp
long duracion = pulseIn(ECHO, HIGH);
```

Si el ultrasonido falla o no hay eco, el robot se congela hasta 1 segundo. Inaceptable en competencia.

**Solución:** Agregar timeout:
```cpp
long duracion = pulseIn(ECHO, HIGH, 10000); // timeout 10ms (~170cm max)
```
O usar lectura no bloqueante con `micros()` y máquina de estados.

---

### R2 — `Adelante()` usa variables `static` frágiles

El patrón de `static bool activo` funciona pero es frágil: si el loop no llama `Adelante()` en cada iteración mientras está activo, la lógica se rompe.

**Solución:** Manejar el estado del movimiento en la máquina de estados principal, no dentro de la función.

---

### R3 — `detenerMotores()` manda comandos redundantes

```cpp
motor1(0, 0); motor1(0, 1); // Se llama dos veces por motor
```

No es grave (potencia 0 = parado), pero es confuso. Una llamada por motor alcanza.

---

### R4 — Giro por tiempo es inherentemente impreciso

```cpp
duracionGiro = abs(angulo * 43 / 200); // Constante mágica
```

Depende de batería, piso, peso. Cambia entre partidos.

**Solución:** Girar usando BNO055 como feedback (control proporcional hasta alcanzar el heading deseado).

---

### R5 — Loop del arquero esencialmente vacío

Falta toda la lógica de comportamiento: oscilación en el arco, reacción a pelota, evasión de líneas. El comentario dice "ver archivo original".

---

### R6 — No hay detección de pelota en el arquero

La librería tiene `readBall()` para 8 sensores IR pero el arquero no los usa. Tampoco hay comunicación UART con OpenMV.

---

## 📋 PRIORIDADES PARA 2026

| # | Problema | Impacto | Esfuerzo |
|---|---------|---------|----------|
| 1 | Inicializar BNO055 en zirconLib | Crítico | Bajo |
| 2 | Heading con offset (eliminar 35°) | Crítico | Bajo |
| 3 | PID heading para ir derecho | Crítico | Medio |
| 4 | Lectura de sensores en loop() | Crítico | Bajo |
| 5 | Declarar variable potencia | Crítico | Trivial |
| 6 | Giro por heading (no por tiempo) | Alto | Medio |
| 7 | Ultrasonido no bloqueante | Alto | Bajo |
| 8 | Lógica de comportamiento del arquero | Alto | Alto |
| 9 | Integrar sensores IR de pelota | Medio | Medio |
| 10 | Limpiar detenerMotores() | Bajo | Trivial |
