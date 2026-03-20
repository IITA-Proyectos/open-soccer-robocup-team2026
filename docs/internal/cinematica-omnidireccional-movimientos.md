---
title: "Cinemática Omnidireccional y Primitivas de Movimiento"
date: 2026-03-20
author: "Claude (Anthropic) bajo supervisión de Gustavo Viollaz"
ai-assisted: true
status: final
tags: [cinematica, omnidireccional, movimiento, pid, giroscopo, motores]
---

# Cinemática Omnidireccional y Primitivas de Movimiento

## 1. GEOMETRÍA DE NUESTRO ROBOT

### 1.1 Configuración de ruedas (verificada desde código)

Analizando las funciones `avanzar()`, `girar()`, `aiproporcional()` y `adproporcional()` del código actual, la disposición de motores es:

```
            FRENTE DEL ROBOT
               ▲
              / \
        M2  /   \  M1
     (150°)/     \(30°)
          /       \
         ─────────
             M3
           (270°)
```

- **M1** (motor1): frontal derecho, orientado a ~30° desde el frente
- **M2** (motor2): frontal izquierdo, orientado a ~150° desde el frente
- **M3** (motor3): trasero central, orientado a ~270° (perpendicular al frente)

### 1.2 Verificación desde código existente

| Movimiento | M1 | M2 | M3 | Coincide con modelo |
|-----------|-----|-----|-----|----|
| Avanzar | +100 | -100 | 0 | ✅ (M1 y M2 empujan hacia adelante, M3 no contribuye) |
| Girar antihorario | misma dir | misma dir | misma dir | ✅ (todos en la misma dirección tangencial) |
| Lateral derecha | -50 | -50 | +89 | ✅ (M3 empuja lateral, M1/M2 compensan) |

---

## 2. MODELO CINEMÁTICO INVERSO

### 2.1 Fórmulas

Para un robot omnidireccional de 3 ruedas, la cinemática inversa calcula la velocidad de cada motor a partir de la velocidad deseada del robot:

**Entrada**: `(vx, vy, omega)` en el marco del robot
- `vx`: velocidad lateral (positivo = derecha)
- `vy`: velocidad frontal (positivo = adelante)
- `omega`: velocidad angular (positivo = antihorario)

**Salida**: `(m1, m2, m3)` velocidades de cada motor

```
m1 = -sin(30°)  * vx + cos(30°)  * vy + L * omega
m2 = -sin(150°) * vx + cos(150°) * vy + L * omega
m3 = -sin(270°) * vx + cos(270°) * vy + L * omega
```

Simplificando con valores trigonométricos:

```
m1 = -0.5  * vx + 0.866 * vy + L * omega
m2 = -0.5  * vx - 0.866 * vy + L * omega
m3 =  1.0  * vx + 0.0   * vy + L * omega
```

Donde `L` es un factor de escala para la rotación (depende de la distancia del centro a las ruedas). En la práctica, `L` se calibra experimentalmente.

### 2.2 Forma polar (más intuitiva para el juego)

En vez de pensar en `(vx, vy)`, es más natural para fútbol pensar en:
- **velocidad**: rapidez de traslación (0 a 100)
- **dirección**: ángulo de traslación (0° = adelante, 90° = derecha, 180° = atrás)
- **omega**: velocidad de giro para mantener/cambiar heading

Conversión:
```
vx = velocidad * sin(direccion)
vy = velocidad * cos(direccion)
```

### 2.3 Saturación (limitación de motores)

Si algún `m_i` supera el máximo (255 para PWM), hay que **escalar proporcionalmente** todos los motores para mantener la dirección:

```cpp
float maxMotor = max(abs(m1), max(abs(m2), abs(m3)));
if (maxMotor > MOTOR_MAX) {
    float escala = MOTOR_MAX / maxMotor;
    m1 *= escala;
    m2 *= escala;
    m3 *= escala;
}
```

Sin esto, al combinar traslación + rotación rápida, un motor satura y el robot se mueve en dirección equivocada.

---

## 3. CONTROL DE HEADING CON PID

### 3.1 El problema

Sin encoders, no sabemos la velocidad real de cada motor. Pero con el BNO055 sabemos la **orientación real** del robot en todo momento. Usamos eso como feedback para un control PID sobre el heading.

### 3.2 Controlador P (proporcional) para heading

El más simple y probablemente suficiente para nuestro caso:

```cpp
float headingError = headingObjetivo - headingActual;
// Normalizar a -180..+180
if (headingError > 180) headingError -= 360;
if (headingError < -180) headingError += 360;

float omega = headingError * Kp_heading;
omega = constrain(omega, -OMEGA_MAX, OMEGA_MAX);
```

**Kp_heading**: cuán agresivo gira para corregir. Valores típicos: 1.0 a 4.0.
- Muy bajo (0.5): corrige lento, el robot se desvía mucho antes de corregir.
- Muy alto (5.0): oscila alrededor del heading objetivo.
- Punto dulce: calibrar en la cancha. Empezar con 2.0.

### 3.3 Controlador PD (proporcional + derivativo)

Agrega amortiguamiento para reducir la oscilación:

```cpp
float headingError = headingObjetivo - headingActual;
// Normalizar...
float derivada = (headingError - headingErrorAnterior) / dt;
float omega = headingError * Kp + derivada * Kd;
headingErrorAnterior = headingError;
```

**Kd**: frena la rotación cuando se acerca al objetivo. Valores típicos: 0.01 a 0.1.

### 3.4 ¿Por qué no PID completo (con integral)?

El término integral (I) acumula error a lo largo del tiempo. En un robot de fútbol que cambia de dirección constantemente, el integral se acumula y causa movimientos erráticos ("integral windup"). **PD es suficiente** para nuestro caso. El término P corrige el error, el D evita overshooting.

---

## 4. LA FUNCIÓN PRIMITIVA: `moverRobot()`

### 4.1 Interfaz

```cpp
void moverRobot(float velocidad, float direccion, float headingObjetivo);
```

**Parámetros:**
- `velocidad`: rapidez de traslación (0 a 100). 0 = solo girar.
- `direccion`: ángulo de traslación en grados, marco del robot.
  - 0° = adelante
  - 90° = derecha
  - 180° = atrás
  - -90° (o 270°) = izquierda
- `headingObjetivo`: hacia dónde debe MIRAR el robot (grados, marco cancha).
  - 0° = al arco contrincante (referencia de encendido)
  - 90° = a la derecha de la cancha
  - Usa PID con giróscopo para mantener este heading.

### 4.2 Usos en el juego

| Acción | velocidad | dirección | headingObjetivo |
|--------|-----------|-----------|----------------|
| Avanzar recto al arco | 80 | 0° | 0° |
| Ir a la pelota (a la derecha) | 70 | 45° | 0° |
| Orbitar pelota (horario) | 50 | 90° | 0° |
| Orbitar pelota (antihorario) | 50 | -90° | 0° |
| Solo girar para buscar | 0 | 0° | (incremental) |
| Retroceder mirando al frente | 60 | 180° | 0° |
| Lateral derecho (arquero) | 60 | 90° | 0° |
| Lateral izquierdo (arquero) | 60 | -90° | 0° |

Esto simplifica TODA la lógica del robot. En vez de manejar 6 funciones diferentes de movimiento (avanzar, girar, orbitar, lateral, retroceder, patear), hay **una sola función** que hace todo.

### 4.3 Implementación

```cpp
// === CONSTANTES DE CALIBRACION ===
const float Kp_heading = 2.0;     // Ajustar en cancha
const float Kd_heading = 0.05;    // Ajustar en cancha
const float OMEGA_MAX = 100.0;    // Velocidad máxima de rotación
const float L_ROTACION = 0.6;     // Factor de escala rotación
const int MOTOR_MAX = 200;        // PWM máximo

// === VARIABLES PID ===
float headingErrorAnterior = 0;
unsigned long pidTiempoAnterior = 0;

void moverRobot(float velocidad, float direccionGrados, float headingObjetivo) {
    // 1. Leer heading actual del giroscopo
    float headingActual = leerHeading(); // usa BNO055 con offset

    // 2. Calcular omega con PD
    float headingError = headingObjetivo - headingActual;
    if (headingError > 180) headingError -= 360;
    if (headingError < -180) headingError += 360;

    unsigned long ahora = millis();
    float dt = (ahora - pidTiempoAnterior) / 1000.0;
    if (dt < 0.001) dt = 0.001; // evitar division por cero
    float derivada = (headingError - headingErrorAnterior) / dt;
    headingErrorAnterior = headingError;
    pidTiempoAnterior = ahora;

    float omega = headingError * Kp_heading + derivada * Kd_heading;
    omega = constrain(omega, -OMEGA_MAX, OMEGA_MAX);

    // 3. Convertir direccion polar a vx, vy
    float direccionRad = direccionGrados * PI / 180.0;
    float vx = velocidad * sin(direccionRad);
    float vy = velocidad * cos(direccionRad);

    // 4. Cinematica inversa: calcular velocidad de cada motor
    float m1 = -0.5 * vx + 0.866 * vy + L_ROTACION * omega;
    float m2 = -0.5 * vx - 0.866 * vy + L_ROTACION * omega;
    float m3 =  1.0 * vx + 0.0   * vy + L_ROTACION * omega;

    // 5. Saturacion proporcional
    float maxM = max(abs(m1), max(abs(m2), abs(m3)));
    if (maxM > MOTOR_MAX) {
        float escala = MOTOR_MAX / maxM;
        m1 *= escala;
        m2 *= escala;
        m3 *= escala;
    }

    // 6. Aplicar a motores
    aplicarMotor(PWM1, INA1, INB1, (int)m1);
    aplicarMotor(PWM2, INA2, INB2, (int)m2);
    aplicarMotor(PWM3, INA3, INB3, (int)m3);
}

void aplicarMotor(int pinPWM, int pinINA, int pinINB, int velocidad) {
    if (velocidad >= 0) {
        digitalWrite(pinINA, 1);
        digitalWrite(pinINB, 0);
    } else {
        digitalWrite(pinINA, 0);
        digitalWrite(pinINB, 1);
    }
    analogWrite(pinPWM, abs(velocidad));
}
```

---

## 5. PROBLEMAS Y LIMITACIONES

### 5.1 Sin encoders: no hay control de velocidad individual

El PID sobre heading funciona porque el giróscopo mide la **salida real** del sistema. Pero la **velocidad de traslación** no tiene feedback — no sabemos si el robot realmente se mueve a la velocidad pedida. Esto significa:
- El robot puede ir más lento en una dirección que en otra (asimetría de motores)
- En superficies resbalosas, las ruedas patinan y el robot no avanza

Por ahora es aceptable — el giróscopo corrige lo más crítico (heading). En el futuro, encoders permitirían PID por motor.

### 5.2 Drift del giróscopo

En modo IMUPLUS, el heading tiene drift. El PID de heading seguirá el heading driftádo como si fuera la realidad. Solución: recalibrar periódicamente por visión (ver doc de giróscopo).

### 5.3 Heading en marco cancha vs marco robot

- `headingObjetivo` está en **marco cancha** (0° = al arco, fijo)
- `direccion` está en **marco robot** (0° = adelante del robot, gira con el robot)

Si el robot está girado 30° a la derecha y querés que vaya hacia el arco, la dirección de traslación debería ser -30° en el marco del robot. Esto se puede hacer automático si se desea traslación en marco cancha:

```cpp
void moverRobotCancha(float velocidad, float direccionCancha, float headingObjetivo) {
    float heading = leerHeading();
    float direccionRobot = direccionCancha - heading;
    moverRobot(velocidad, direccionRobot, headingObjetivo);
}
```

### 5.4 La dirección real de cada motor puede estar invertida

Los signos en la cinemática inversa asumen una convención de positivo/negativo para cada motor. Si un motor está cableado al revés, hay que negar su valor. Esto se calibra una vez.

---

## 6. PLAN DE PRUEBA

Para calibrar y verificar el sistema:

1. **Test estático de heading**: Robot quieto, heading = 0, verificar que no gira.
2. **Test de giro**: moverRobot(0, 0, 90) → debe girar 90° a la derecha y parar.
3. **Test avanzar recto**: moverRobot(60, 0, 0) → debe ir derecho corrigiendo con PID.
4. **Test lateral**: moverRobot(60, 90, 0) → debe ir a la derecha mirando al frente.
5. **Test orbitar**: moverRobot(50, 90, 0) → equivalente a orbitar horario.
6. **Test combinado**: moverRobot(60, 45, 30) → avanza a 45° mientras gira a heading 30°.
7. **Calibrar Kp/Kd**: Si oscila, bajar Kp o subir Kd. Si responde lento, subir Kp.
8. **Calibrar L_ROTACION**: Si gira demasiado rápido para poca traslación, bajar L.
9. **Calibrar signos**: Si un motor gira al revés, negar su salida.

---

## 7. REFERENCIAS

- [Three omni-directional wheels control on a mobile robot (Ribeiro et al., RoboCup)](https://www.researchgate.net/publication/228786543) — Paper fundacional de cinemática para 3 ruedas omni en RoboCup.
- [Omnidirectional Control (Raul Rojas, FU Berlin)](http://robocup.mi.fu-berlin.de/buch/omnidrive.pdf) — Teoría completa de control omnidireccional por el equipo FU-Fighters de RoboCup.
- [Kinematics of omni-directional wheeled robots (Gregwar)](https://gregwar.github.io/omnidirectional-wheeled-robots) — Derivación paso a paso con diagramas, incluye saturación.
- [Dynamical Models for Omni-directional Robots with 3 and 4 Wheels (Oliveira et al., RoboCup SSL)](https://www.researchgate.net/publication/256089847) — Modelos dinámicos para 3 y 4 ruedas de un equipo SSL.
