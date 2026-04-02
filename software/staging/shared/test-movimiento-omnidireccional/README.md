---
title: "Test: Primitivas de movimiento omnidireccional con PD heading"
date: 2026-04-02
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: true
ai-tool: "Claude (Anthropic)"
requested-by: "María (IITA)"
status: final
tags: [test, movimiento, omnidireccional, cinematica, pd, heading, giroscopo, bno055]
robot: ambos
---

# Test: Primitivas de movimiento omnidireccional con PD heading

## Descripción general

Programa interactivo que prueba 9 movimientos omnidireccionales distintos usando la función `moverRobot(velocidad, dirección, headingObjetivo)`. Combina traslación en cualquier dirección con control PD de heading por giroscopio. Cada test dura 3 segundos y se avanza con el botón 1 (zirconLib).

**Archivo**: `test-movimiento-omnidireccional.ino`

## ¿Para qué sirve?

Valida la cinemática inversa omnidireccional completa: que el robot pueda moverse en cualquier dirección mientras mantiene una orientación deseada. Es el test más completo de movimiento, ya que cubre adelante, atrás, laterales, diagonales, giro puro y órbita.

## Hardware requerido

- Teensy (Robot 1 o Robot 2, seleccionable con `#define`)
- BNO055 en I2C `0x28`
- 3 motores con drivers H-bridge
- Botón 1 (zirconLib)

**Nota**: Este es el único test en staging que **usa zirconLib** (`InitializeZircon()`, `readButton()`).

## Los 9 tests

| # | Velocidad | Dirección | Heading | Qué debería hacer |
|---|-----------|-----------|---------|-------------------|
| 1 | 0 | 0° | 0° | Quieto mirando al frente — no debe moverse |
| 2 | 60 | 0° | 0° | Avanzar recto — PD corrige desviaciones |
| 3 | 60 | 180° | 0° | Retroceder — atrás sin girar |
| 4 | 60 | 90° | 0° | Lateral derecha — cangrejo mirando al frente |
| 5 | 60 | -90° | 0° | Lateral izquierda — cangrejo mirando al frente |
| 6 | 60 | 45° | 0° | Diagonal adelante-derecha — sin girar |
| 7 | 0 | 0° | 90° | Solo girar a heading 90° — gira y para |
| 8 | 0 | 0° | -90° | Solo girar a heading -90° — gira al otro lado |
| 9 | 50 | 90° | 0° | Órbita — lateral derecha manteniendo heading (simula orbitar pelota) |

## Cinemática inversa

La función `moverRobot()` implementa:

1. **PD sobre heading**: calcula omega (velocidad de rotación) con `Kp_heading` y `Kd_heading`
2. **Polar a cartesiano**: convierte velocidad + dirección a vx, vy
3. **Cinemática inversa** para 3 ruedas omni a 30°/150°/270°:
   - `m1 = -0.5·vx + 0.866·vy + L·ω`
   - `m2 = -0.5·vx - 0.866·vy + L·ω`
   - `m3 = 1.0·vx + 0.0·vy + L·ω`
4. **Saturación proporcional**: si algún motor supera `MOTOR_MAX`, escala todos

## Parámetros a calibrar en la cancha

| Parámetro | Valor default | Qué ajustar |
|-----------|--------------|-------------|
| `Kp_heading` | 2.0 | Subir si corrige heading lento, bajar si oscila |
| `Kd_heading` | 0.05 | Subir si oscila mucho |
| `L_ROTACION` | 0.6 | Bajar si gira demasiado y no traslada |
| `MOTOR_MAX` | 200 | PWM máximo permitido |
| `SIGNO_M1/M2/M3` | 1 | Cambiar a -1 si un motor gira al revés |

## Selección de robot

En el código, descomentar la línea correspondiente:

```cpp
//#define ROBOT1   // Pines del arquero
#define ROBOT2     // Pines del delantero (activo por default)
```

Los pines cambian según el robot seleccionado.

## Instrucciones de uso

1. Seleccionar robot (`ROBOT1` o `ROBOT2`) en el código
2. Subir al Teensy, Serial Monitor a 19200 baud
3. No mover durante calibración (~5 seg)
4. Presionar botón 1 para iniciar
5. Cada test corre 3 segundos, luego espera botón para el siguiente
6. Anotar qué tests funcionan bien y cuáles no para ajustar parámetros

## Notas importantes

- **Usa zirconLib** (a diferencia de los otros tests en staging que definen pines directamente). Asegurarse de que la librería esté instalada.
- Usa `OPERATION_MODE_IMUPLUS` para el BNO055.
- Los valores calibrados en este test (Kp, Kd, L, signos) deben trasladarse después a los programas definitivos.

## Documentación relacionada

- `docs/internal/cinematica-omnidireccional-movimientos.md` — Modelo cinemático completo (referencia principal)
- `test-motores-lateral-simple/` — Test específico para movimiento lateral
- `test-gyro-movimiento-basico/` — Test específico para adelante/atrás
