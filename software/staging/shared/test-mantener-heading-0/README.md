---
title: "Test: Mantener ángulo inicial con PID (corrección omnidireccional)"
date: 2026-04-03
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: true
ai-tool: "Claude (Anthropic)"
requested-by: "Elias (IITA)"
status: final
tags: [test, pid, heading, omnidireccional, bno055, estabilidad, control]
robot: ambos
---

# Test: Mantener ángulo inicial con PID (corrección omnidireccional)

## Descripción general

Programa de control de lazo cerrado que captura la orientación inicial del robot al encenderse (ángulo objetivo) y utiliza un controlador PID para activar los motores de forma coordinada. El sistema busca oponerse activamente a cualquier fuerza externa que intente girar o desplazar al robot de su "norte" relativo.

**Archivo**: `test-mantener-angulo.ino`

## ¿Para qué sirve?

Valida la capacidad del robot para mantener su orientación durante el juego, resistiendo choques o derivas mecánicas. Permite tunear las constantes del PID ($K_p$, $K_i$, $K_d$) en tiempo real y verificar la respuesta del motor lateral (M3) como estabilizador ante empujones transversales.

## Hardware requerido

* Teensy (Robot 1 o Robot 2, seleccionable con `#define`)
* BNO055 en I2C `0x28`
* 3 motores con drivers H-bridge
* Botón en **Pin 9**

**Nota**: A diferencia de otros tests, este programa **NO usa zirconLib** para mantener compatibilidad directa con el firmware base del Robot 2 (delantero), definiendo los pines manualmente.

## Estados del programa

El flujo se controla mediante el botón del Pin 9 y se visualiza en el Serial Monitor (19200 baud):

| Estado | Acción | Indicador Visual |
| :--- | :--- | :--- |
| **1. ESPERANDO_INICIO** | Espera presión de botón tras la calibración inicial. | LED apagado |
| **2. TEST_HEADING** | Modo pasivo. Permite girar el robot a mano para verificar que el BNO055 lee correctamente. | LED parpadea si error > 20° |
| **3. MODO_MANTENER** | **Activo**. Los motores aplican fuerza para volver al ángulo 0°. | LED encendido (fijo) |

## Estrategia de control (PID)

La corrección se calcula sobre el error de heading ($0 - headingActual$) y se distribuye a los motores mediante una mezcla diferencial y proporcional:

1.  **Corrección Diferencial (M1/M2)**: Corrigen la rotación pura. Un error positivo (giro a la derecha) hace que M1 y M2 giren en sentidos opuestos para rotar a la izquierda.
2.  **Corrección Lateral (M3)**: Multiplicada por un `FACTOR_M3`, este motor se activa para resistir el desplazamiento lateral que suele acompañar a los impactos en robots omnidireccionales.
3.  **Saturación y Mínimos**: Se aplica una `VEL_MINIMA` (60) para vencer la inercia y una `VEL_MAXIMA` (200) para proteger los drivers.

## Parámetros de calibración (Serial)

Durante el **MODO_MANTENER**, se pueden enviar comandos por el Serial Monitor para ajustar el comportamiento sin reiniciar:

| Comando | Acción | Qué observar |
| :--- | :--- | :--- |
| `+` / `-` | Sube/Baja **Kp** (0.5) | Ajusta la agresividad. Si oscila mucho, bajar Kp. |
| `r` | **Recalibrar** | Establece el ángulo actual como el nuevo 0°. |
| `p` | **Pausa** | Detiene motores (seguridad) pero mantiene el cálculo PID. |
| `v` | **Valores** | Imprime en consola: Kp, Ki, Kd, Heading y Ciclos. |

## Selección de robot

El mapeo de pines cambia según la definición al inicio del código:

```cpp
#define ROBOT1   // Configuración para el Arquero
//#define ROBOT2   // Configuración para el Delantero 

Los pines cambian según el robot seleccionado.

## Instrucciones de uso

1. Posicionar el robot en la orientación deseada para el test.
2. Subir el código y esperar la calibración del BNO055 (5 segundos sin mover).
3. Presionar botón para entrar a TEST_HEADING; verificar lecturas en monitor.
4. Presionar botón nuevamente para activar el MODO_MANTENER. 

Filtro de error: Existe un UMBRAL_ERROR_GRADOS de 5° para evitar que los motores zumben o vibren cuando el robot está prácticamente en posición.

Anti-windup: La parte integral del PID está limitada a INTEGRAL_MAX (40.0) para evitar respuestas violentas tras bloqueos prolongados.

Velocidad Serial: Asegurarse de configurar el monitor a 19200 baud. 

## Documentación relacionada

- `test-4-movimientos.ino` — Estructura base de motores y estados.

- `docs/internal/pid-tuning-guia.md` — Guía de ajuste de constantes Kp/Ki/Kd.