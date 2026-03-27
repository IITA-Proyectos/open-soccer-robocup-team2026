---
title: "Lecciones aprendidas: PID para movimiento lateral omnidireccional"
date: 2026-03-27
author: "Claude (Anthropic - Claude Opus 4.6) bajo supervisión de Gustavo Viollaz"
ai-assisted: true
status: final
tags: [pid, movimiento, lateral, omnidireccional, cinematica, lecciones]
robot: ambos
---

# Lecciones aprendidas: PID para movimiento lateral omnidireccional

Este documento resume los bugs encontrados y las soluciones aplicadas en el test de movimiento lateral. Las lecciones son aplicables a **cualquier movimiento omnidireccional con corrección PID de heading**, no solo al movimiento lateral.

Código de referencia: `software/staging/shared/test-motores-lateral-simple.ino`

---

## 1. NUNCA llamar calcularCorreccionPID() dos veces por loop

### El problema

La función `calcularCorreccionPID()` actualiza variables internas cada vez que se ejecuta: `errorAnterior`, `integral`, y `tiempoAnteriorPID`. Si se llama una vez en `moverLateral()` para mover los motores y otra vez en el bloque de `Serial.print()` para mostrar el valor, la segunda llamada tiene un `dt` de casi cero (microsegundos entre ambas llamadas). Esto hace que la derivada (`(error - errorAnterior) / dt`) se dispare a valores enormes, causando correcciones erráticas.

### La solución

Guardar el resultado en una variable global y reutilizarla:

```cpp
// Variables globales para debug
float ultimoHeading = 0;
float ultimaCorreccion = 0;

void moverLateral(int direccion) {
  float heading = leerHeading();
  float correccion = calcularCorreccionPID(heading);  // UNA sola vez

  ultimoHeading = heading;          // guardar para Serial
  ultimaCorreccion = correccion;    // guardar para Serial

  // ... usar correccion para los motores ...
}

void loop() {
  moverLateral(direccion);

  // Para imprimir, usar los valores guardados
  Serial.print(ultimaCorreccion);  // NO llamar calcularCorreccionPID() de nuevo
}
```

### Regla general

Cualquier función PID que tenga estado interno (derivada, integral) debe llamarse **exactamente una vez por ciclo de loop**. Si necesitás el valor en otro lugar, guardalo en una variable.

---

## 2. Los 3 motores deben participar en la corrección de heading

### El problema

En un robot omnidireccional de 3 ruedas, la rotación (giro sobre el eje central) requiere que **todos los motores** giren en la misma dirección tangencial. Si solo 2 de los 3 motores corrigen el heading, la corrección es asimétrica: el robot no gira limpiamente sobre su centro, sino que hace un arco, y la corrección es menos efectiva.

El código original solo aplicaba la corrección a M1 y M2 (los frontales), dejando M3 (trasero) a velocidad fija.

### La solución

Cada motor recibe dos componentes sumadas:

```
velocidad_total = componente_LATERAL + componente_ROTACIÓN
```

La componente lateral es lo que desplaza el robot. La componente de rotación es lo que corrige el heading. Ambas se suman para cada motor:

```cpp
// Componente lateral (calibrada empíricamente)
float lat_M1 = DIR_M1 * VEL_M1 * direccion;
float lat_M2 = DIR_M2 * VEL_M2 * direccion;
float lat_M3 = DIR_M3 * VEL_M3 * direccion;

// Componente rotación (PID, los 3 participan)
float rot_M1 = ROT_M1 * correccion * FACTOR_ROTACION;
float rot_M2 = ROT_M2 * correccion * FACTOR_ROTACION;
float rot_M3 = ROT_M3 * correccion * FACTOR_ROTACION;

// Total
float total_M1 = lat_M1 + rot_M1;
float total_M2 = lat_M2 + rot_M2;
float total_M3 = lat_M3 + rot_M3;
```

---

## 3. Cómo derivar los signos de rotación (ROT_M1, ROT_M2, ROT_M3)

Los signos de rotación NO se inventan — se derivan de una función de giro que ya funciona en el robot.

### Procedimiento

1. Buscar la función `girar()` en el código definitivo del robot. En nuestro caso:

```cpp
void girar() {  // gira antihorario
  analogWrite(PWM1, 100); digitalWrite(INA1, 0); digitalWrite(INB1, 1);  // M1
  analogWrite(PWM2, 100); digitalWrite(INA2, 0); digitalWrite(INB2, 1);  // M2
  analogWrite(PWM3, 100); digitalWrite(INA3, 0); digitalWrite(INB3, 1);  // M3
}
```

2. Para cada motor, ver qué dirección corresponde en la función `motorN()` del test:
   - Motor1: INA1=0, INB1=1 → en motor1(), esto es `dir = -1`
   - Motor2: INA2=0, INB2=1 → en motor2(), esto es `dir = +1` (hardware invertido)
   - Motor3: INA3=0, INB3=1 → en motor3(), esto es `dir = -1`

3. Entonces girar **antihorario** = velocidades con signo: M1=-V, M2=+V, M3=-V

4. Girar **horario** (opuesto) = M1=+V, M2=-V, M3=+V

5. El PID produce corrección positiva cuando necesita girar **horario** (robot desviado antihorario). Entonces los signos de rotación para corrección positiva = horario:

```cpp
ROT_M1 = +1;   // corrección positiva → M1 positivo → horario
ROT_M2 = -1;   // corrección positiva → M2 negativo → horario
ROT_M3 = +1;   // corrección positiva → M3 positivo → horario
```

### Verificación rápida

Si el robot corrige para el lado equivocado (se desvía más en vez de menos), invertir **los tres signos**:

```cpp
ROT_M1 = -1;
ROT_M2 = +1;
ROT_M3 = -1;
```

---

## 4. Saturación proporcional, NUNCA constrain(0, 255)

### El problema

Con `VEL_M1 = 55` y `MAX_CORRECCION = 50`, una corrección de -50 hace que `velM1 = 55 + (-50) = 5`. Si la corrección fuera un poco más grande, el motor quedaría en 0 — **una rueda muerta**. Mientras tanto, el otro motor queda a velocidad alta. Esto causa el síntoma de "una rueda se queda quieta".

El error es usar `constrain(velocidad, 0, 255)` — clampear a 0 impide que el motor cambie de dirección cuando la corrección lo pide.

### La solución: saturación proporcional

En vez de clampear cada motor individualmente, si alguno supera 255, escalar **todos** proporcionalmente:

```cpp
float maxM = max(abs(total_M1), max(abs(total_M2), abs(total_M3)));
if (maxM > 255) {
    float escala = 255.0 / maxM;
    total_M1 *= escala;
    total_M2 *= escala;
    total_M3 *= escala;
}
```

Y el signo del total determina la dirección del motor (no se clampea a 0):

```cpp
int dir = (total > 0) ? 1 : ((total < 0) ? -1 : 0);
motor(abs(total), dir);
```

### Por qué funciona

La saturación proporcional **mantiene la proporción** entre los 3 motores. Si la cinemática dice que M1 debe ir a 300, M2 a -200 y M3 a 100, la saturación los escala a 255, -170, 85 — la **dirección del movimiento** se preserva, solo la velocidad se reduce.

Clampear individualmente a [0, 255] destruye la proporción: un motor que debería ir a -50 queda en 0, y el robot se mueve en una dirección completamente distinta a la calculada.

---

## 5. FACTOR_ROTACION como parámetro separado

El `FACTOR_ROTACION` (0.0 a 1.0) escala la componente de rotación independientemente de Kp/Ki/Kd. Esto permite ajustar el balance entre "mantener el heading" y "mantener la velocidad lateral" sin tocar los parámetros del PID.

- `FACTOR_ROTACION = 0.0` → sin corrección de heading (solo lateral puro)
- `FACTOR_ROTACION = 0.5` → corrección moderada (buen punto de partida)
- `FACTOR_ROTACION = 1.0` → corrección máxima (puede sacrificar velocidad lateral)

### Cómo ajustar

1. Empezar con `FACTOR_ROTACION = 0.0` y verificar que el movimiento lateral funciona
2. Subir a 0.3, verificar que corrige sin oscilar
3. Subir de a 0.1 hasta encontrar el balance
4. Si oscila: bajar FACTOR_ROTACION (o bajar Kp)
5. Si no corrige suficiente: subir FACTOR_ROTACION (o subir Kp)

---

## 6. Nota sobre convención de motor2

En nuestro robot, `motor2()` tiene el hardware invertido respecto a `motor1()` y `motor3()`. Es decir, `dir = +1` en motor2 produce la **misma** rotación física que `dir = -1` en motor1.

Esto se descubrió empíricamente: María invirtió DIR_M1 y DIR_M2 respecto a los valores originales del programa porque el robot iba para el lado equivocado. Los valores calibrados (DIR_M1=-1, DIR_M2=+1, DIR_M3=+1) son correctos para ROBOT2.

Para ROBOT1 (arquero), los pines de motores son distintos y la calibración de direcciones debe rehacerse.

---

## 7. Valores calibrados actuales (ROBOT2, marzo 2026)

| Parámetro | Valor | Quién lo calibró |
|-----------|-------|------------------|
| VEL_M1 | 55 | María (empírico) |
| VEL_M2 | 55 | María (empírico) |
| VEL_M3 | 100 | María (empírico) |
| DIR_M1 | -1 | María (empírico) |
| DIR_M2 | +1 | María (empírico) |
| DIR_M3 | +1 | María (empírico) |
| ROT_M1 | +1 | Derivado de girar() |
| ROT_M2 | -1 | Derivado de girar() |
| ROT_M3 | +1 | Derivado de girar() |
| Kp | 3.0 | Del test anterior |
| Ki | 0.05 | Del test anterior |
| Kd | 0.5 | Del test anterior |
| FACTOR_ROTACION | 0.5 | Valor inicial |

---

## Resumen: las 3 reglas de oro del PID omnidireccional

1. **Calcular el PID una sola vez por loop** — guardarlo en variable si se necesita en otro lado
2. **Los 3 motores participan en la corrección** — rotación = movimiento tangencial de las 3 ruedas
3. **Saturación proporcional, nunca clampear a 0** — si un motor supera 255, escalar todos; el signo determina dirección
