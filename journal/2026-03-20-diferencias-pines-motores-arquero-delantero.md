---
title: "Diferencias en numeracion de motores entre robot arquero y delantero"
date: 2026-03-20
author: "Claude (Anthropic - Claude Sonnet 4.6)"
requested-by: "Maria Virginia Viollaz (@mariaviollaz)"
ai-assisted: true
ai-tool: "Claude (Anthropic - Claude Sonnet 4.6)"
status: final
tags: [electronica, movilidad, ambos, comparacion]
robot: ambos
area: electronica
tipo: comparacion
---

# Diferencias en numeracion de motores entre robot arquero y delantero

## Contexto

Los robots arquero y delantero utilizan la misma placa Zircon (Rev v15) y el mismo Teensy 4.1, pero los motores estan fisicamente conectados de forma distinta en cada robot. Esto implica que el mapeo de pines en el codigo difiere segun el robot compilado, controlado mediante las directivas `#define ROBOT1` (arquero) y `#define ROBOT2` (delantero).

---

## Asignacion de pines por robot

### Robot Arquero (ROBOT1)

| Motor logico | INA  | INB  | PWM  | Driver (Zircon) |
|-------------|------|------|------|-----------------|
| Motor 1     | 2    | 5    | 3    | U5              |
| Motor 2     | 8    | 7    | 6    | U17             |
| Motor 3     | 11   | 12   | 4    | U7              |

### Robot Delantero (ROBOT2)

| Motor logico | INA  | INB  | PWM  | Driver (Zircon) |
|-------------|------|------|------|-----------------|
| Motor 1     | 8    | 7    | 6    | U17             |
| Motor 2     | 11   | 12   | 4    | U7              |
| Motor 3     | 2    | 5    | 3    | U5              |

---

## Equivalencia entre robots

La siguiente tabla muestra que motor fisico del delantero (ROBOT2) corresponde a cada motor logico del arquero (ROBOT1):

| Motor logico arquero | Pines arquero (INA/INB/PWM) | Motor logico delantero | Pines delantero (INA/INB/PWM) |
|---------------------|----------------------------|----------------------|-------------------------------|
| Motor 1             | 2 / 5 / 3                  | Motor 3              | 2 / 5 / 3                     |
| Motor 2             | 8 / 7 / 6                  | Motor 1              | 8 / 7 / 6                     |
| Motor 3             | 11 / 12 / 4                | Motor 2              | 11 / 12 / 4                   |

En otras palabras: el **Motor 1 del arquero** usa los mismos pines que el **Motor 3 del delantero**, el **Motor 2 del arquero** corresponde al **Motor 1 del delantero**, y el **Motor 3 del arquero** corresponde al **Motor 2 del delantero**.

---

## Pines compartidos (iguales en ambos robots)

Los siguientes pines son identicos en arquero y delantero:

| Componente         | Pines Teensy 4.1                       |
|--------------------|----------------------------------------|
| OpenMV (UART)      | 0 (RX), 1 (TX)                         |
| BNO055 IMU (I2C)   | 18 (SDA), 19 (SCL)                     |
| Sensores IR pelota | 14, 15, 16, 17, 20, 21, 22, 23         |
| Pulsadores         | 9, 10                                  |
| Sensores de linea  | A11 (25), A12 (26), A13 (27)           |

---

## Archivos de referencia

- Codigo arquero: `software/robot-arquero/definitivo-arquero_6-9-2026`
- Codigo delantero: `software/robot-delantero/definitivo-delantero`
- Libreria: `software/libraries/zirconLib/zirconLib.cpp`
- Esquematico: `hardware/electronics/Zircon.pdf` (Rev v15, 2024-08-05)
