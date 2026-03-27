---
title: "Lecciones aprendidas: Control PID para movimiento con giróscopo"
date: 2026-03-27
author: "Claude (Anthropic)"
ai-assisted: false
status: final
tags: [pid, giroscopo, movilidad, bno055, control]
robot: ambos
---

# Lecciones aprendidas: Control PID para movimiento con giróscopo

Este documento resume las lecciones aprendidas durante el desarrollo del test de movimiento con corrección PID usando el giróscopo BNO055.

## 1. Corrección PID debe invertirse al ir hacia atrás

### El problema
Cuando el robot iba hacia adelante con corrección PID, mantenía la línea recta perfectamente. Pero cuando iba hacia atrás, **daba una vuelta en U de 180 grados** en vez de retroceder en línea recta.

### La causa
Al ir hacia atrás, el "frente" del movimiento es la parte trasera del robot. Es como manejar un auto en reversa: si el auto se desvía a la derecha, tenés que girar el volante **al revés** de lo que harías yendo adelante.

### La solución
**Invertir la corrección PID cuando el robot va hacia atrás:**

```cpp
// ADELANTE: corrección NORMAL
void moverAdelante(int velocidadBase) {
  float correccion = calcularCorreccionPID(heading);
  int velM1 = velocidadBase + (int)correccion;  // Motor derecho
  int velM2 = velocidadBase - (int)correccion;  // Motor izquierdo
  // ...
}

// ATRÁS: corrección INVERTIDA
void moverAtras(int velocidadBase) {
  float correccion = calcularCorreccionPID(heading);
  int velM1 = velocidadBase - (int)correccion;  // ¡INVERTIDO!
  int velM2 = velocidadBase + (int)correccion;  // ¡INVERTIDO!
  // ...
}
```

---

## 2. Detección de botón con flanco de subida

### El problema
Al presionar el botón una vez, el programa detectaba múltiples presiones y saltaba varios estados de golpe (por ejemplo, saltaba directamente del test 1 al test 3).

### La causa
El loop se ejecuta muy rápido (~10000 veces por segundo). Si el botón está presionado durante 100ms, se detecta como cientos de presiones.

### La solución
**Detectar flanco de subida + esperar a que se suelte el botón:**

```cpp
bool botonAnterior = false;

bool botonPresionado() {
  bool estadoActual = (digitalRead(PIN_BOTON) == HIGH);
  
  // Solo detecta cuando PASA de LOW a HIGH (flanco de subida)
  if (estadoActual && !botonAnterior) {
    botonAnterior = estadoActual;
    delay(50);  // Pequeño debounce
    return true;
  }
  
  botonAnterior = estadoActual;
  return false;
}

void esperarSoltarBoton() {
  // Espera hasta que el usuario SUELTE el botón
  while (digitalRead(PIN_BOTON) == HIGH) {
    delay(10);
  }
  delay(50);
  botonAnterior = false;
}

// Uso:
if (botonPresionado()) {
  esperarSoltarBoton();  // ¡Importante!
  // cambiar estado...
}
```

---

## 3. Compatibilidad de la librería BNO055

### El problema
El código usaba `Adafruit_BNO055::OPERATION_MODE_IMUPLUS` pero la librería instalada no tenía ese modo definido, causando error de compilación.

### La solución
**Usar `bno.begin()` sin parámetros** (modo NDOF por defecto). Es más compatible y funciona igual de bien para nuestro caso:

```cpp
// ANTES (puede no compilar en todas las versiones):
while (!bno.begin(Adafruit_BNO055::OPERATION_MODE_IMUPLUS)) {

// DESPUÉS (más compatible):
while (!bno.begin()) {
```

---

## 4. Independencia de zirconLib para tests

### El problema
Usar zirconLib en tests agrega dependencias innecesarias y hace más difícil entender qué pines se están usando.

### La solución
**Para tests simples, definir los pines directamente:**

```cpp
// En vez de usar zirconLib:
// #include <zirconLib.h>
// InitializeZircon();
// readButton(1);

// Definir pines directamente:
#define INA1 8
#define INB1 7
#define PWM1 6
#define PIN_BOTON 9

// Y configurarlos en setup():
pinMode(PIN_BOTON, INPUT);
pinMode(INA1, OUTPUT);
// etc.
```

Esto hace el código más **autocontenido** y **fácil de entender**.

---

## 5. Resetear PID al cambiar de dirección

### El problema
Si el PID acumula error integral mientras va adelante, ese error se arrastra cuando cambia a ir hacia atrás, causando comportamiento errático.

### La solución
**Llamar a `resetPID()` antes de cada cambio de dirección:**

```cpp
void resetPID() {
  errorAnterior = 0;
  integral = 0;
  tiempoAnteriorPID = millis();
}

// Uso:
case LOOP_PAUSA_1:
  if (millis() - tiempoInicioTest >= PAUSA) {
    resetPID();  // ¡Importante antes de cambiar dirección!
    estadoTest = LOOP_ATRAS;
  }
  break;
```

---

## 6. Parámetros PID que funcionaron

Para el ROBOT 2 (delantero) con velocidad base 100:

```cpp
float Kp = 3.0;    // Proporcional
float Ki = 0.05;   // Integral (bajo para evitar windup)
float Kd = 0.5;    // Derivativo

const float MAX_CORRECCION = 80;   // Limitar corrección máxima
const float INTEGRAL_MAX = 50;     // Anti-windup
```

### Cómo ajustar:
- **Robot oscila mucho** → Bajar Kp (ej: 2.0)
- **No corrige suficiente** → Subir Kp (ej: 4.0)
- **Error constante que no desaparece** → Subir Ki (ej: 0.1)
- **Reacciona muy brusco** → Subir Kd (ej: 1.0)

---

## Código de referencia

El test completo está en: `software/staging/shared/test-gyro-movimiento-basico.ino`

Este código puede usarse como base para:
- Tests de movimiento omnidireccional
- Navegación autónoma
- Cualquier movimiento que requiera mantener orientación
