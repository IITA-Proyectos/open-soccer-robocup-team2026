---
title: "Cámara TRASERA — Hardware y conexión"
date: 2026-05-24
status: vigente
parte-de: cameraBack-pack
---

# Cámara TRASERA — Hardware y conexión

## 1. Qué módulo OpenMV es

| Atributo | Valor | Confianza |
|---|---|---|
| Familia | **OpenMV H7** o **H7 Plus** | ⚠️ confirmar con TASK-013 (recuperar BOM) |
| Procesador | STM32H7 a 480 MHz (H7) o 400 MHz (H7 Plus con extra RAM) | ✅ |
| Sensor de imagen | OV5640 (H7 Plus) u OV7725 (H7 básico) | ⚠️ depende del modelo |
| Resolución usada | **QVGA (320×240) RGB565** | ✅ por código |
| Frame rate efectivo | **~30 Hz** (limitado por procesamiento de blobs) | ✅ estimación |
| Lente | Lente standard ~2.8 mm (FOV ~70° horizontal) | ⚠️ confirmar con Enzo |
| Firmware | MicroPython (script en `firmware/openmv/`) | ✅ |

> 📌 Probablemente la cámara trasera es **idéntica en modelo** a la frontal
> (el equipo compró 2 del mismo módulo). Si Enzo confirma esto, los thresholds
> LAB iniciales pueden ser los mismos — solo difieren en homografía y montaje.

## 2. Conexión con la placa TOP

| Atributo | Valor |
|---|---|
| **Puerto en el Teensy 4.0 (TOP)** | `Serial5` ✅ (soldada en pin 21, confirmado en banco 2026-05-31) |
| Pin Arduino RX (TOP recibe de la cámara) | **21** |
| Pin Arduino TX (TOP envía a la cámara — no usado) | **20** |
| Conector físico en el PCB TOP | (pin 21/20 — confirmar conector con Enzo) |
| Baud rate | **19200 8N1** |
| Constante en firmware Teensy | `UART_CAMERA2_BAUD = 19200` (`cameras_runtime.cpp` lee la trasera en `Serial5`) |
| Constante en firmware OpenMV | `UART(3, 19200)` en `current-generic.py:6` (UART3 del OpenMV — la H7 solo tiene UART3 expuesto) |
| Sentido de datos | Cámara → TOP (unidireccional en práctica; TX del TOP existe pero no se usa) |

> ✅ **Confirmado en banco 2026-05-31 (TASK-204)**: la cámara trasera quedó **soldada
> en Serial5 (RX pin 21)** — `diag_top_cameras` la ve con FORMATO OK. Esto revierte el
> movimiento provisional del 2026-05-29 (que la había puesto en Serial7). El UART
> **TOP→CENTRAL** se movió entonces a **Serial7 (pines 28/29)**. Firmware vivo ya
> corregido (`cameras_runtime.cpp` lee la trasera en `Serial5`).

## 3. Montaje físico en el robot

| Atributo | Valor | Confianza |
|---|---|---|
| Dirección que mira | **−Y del robot** (atrás) | ⚠️ confirmar con Enzo (asunción del repo) |
| Altura sobre el suelo (h) | **18.7 cm** (placeholder del script) | ⚠️ medir en el robot real |
| Ángulo de inclinación | (no documentado) | ⚠️ medir — puede diferir de la frontal si el chasis no es simétrico |
| Distancia al centro del robot | (no documentado) | ⚠️ medir |
| HMIRROR | `True` (script actual) | ⚠️ **probablemente distinto al frontal** — verificar con preview del IDE |
| VFLIP | `True` (script actual) | ⚠️ **probablemente distinto al frontal** — verificar con preview del IDE |

> 📌 **HMIRROR/VFLIP en la trasera**: si la cámara trasera está montada con el
> cable hacia el lado opuesto que la frontal (lo más común al "espejar" el
> diseño), HMIRROR/VFLIP tienen que invertirse respecto a la frontal. Hay que
> calibrar viendo el preview en el IDE y confirmando que "arriba en la imagen"
> coincide con "arriba del robot" (cielo, no piso).

## 4. Coordenadas que envía esta cámara — IMPORTANTE: rotación 180° en el TOP

**La cámara trasera ve el mundo desde una perspectiva opuesta**. Si la pelota
está físicamente a 50 cm DETRÁS del robot, la cámara trasera la ve enfrente
de ELLA (porque la cámara apunta al atrás del robot, así que "su frente" es
el atrás del robot).

**La cámara trasera NO debe rotar sus coordenadas** — debe enviar las
coordenadas en SU PROPIO frame (lo que ve), como si fuera la única cámara
del mundo.

**La rotación 180° la hace el TOP** en `cameras_fusion.cpp:25-29`:

```cpp
// Si cam_id == 1 (cámara trasera), rotar 180°:
if (cam_id == 1) {
    x = -x;   // invertir lateral
    y = -y;   // invertir profundidad
}
```

De esta forma, una pelota que la cámara trasera reporta como `(0, +50)`
(50 cm enfrente de la cámara) se convierte en `(0, -50)` en el frame del
robot (50 cm detrás del robot). **Eso es lo que el resto del firmware del
TOP y el CENTRAL espera**: coordenadas en el frame del robot, NO en el
frame de la cámara.

| Coord en el frame de la cámara trasera | Coord en el frame del robot (post-rotación 180°) | Interpretación física |
|---|---|---|
| (0, +50) | (0, −50) | Pelota a 50 cm detrás del robot |
| (+30, +50) | (−30, −50) | Pelota a 30 cm a la izquierda y 50 cm detrás |
| (−20, +80) | (+20, −80) | Pelota a 20 cm a la derecha y 80 cm detrás |
| (0, 0) (sentinel) | (0, 0) (sentinel) | No detectado (sentinel preservado) |

> 📌 El sentinel `(X=0, Y_coded=0)` se preserva en la rotación porque
> `-0 = 0`. El TOP detecta el sentinel ANTES de aplicar la rotación
> (`is_visible(x, y) = false` se evalúa primero), así que no hay riesgo
> de confundir un sentinel con una pelota en el origen.

## 5. Pines del lado OpenMV

| Pin OpenMV | Función | Conectado a |
|---|---|---|
| **UART3 TX** | Cámara TX → Teensy RX | pin 21 del Teensy 4.0 (Serial5 RX) |
| **UART3 RX** | Cámara RX ← Teensy TX (no usado) | pin 20 del Teensy 4.0 (Serial5 TX) |
| **GND** | Masa común | GND del Teensy / PCB TOP |
| **VIN o 3V3** | Alimentación | desde el regulador 5V del PCB TOP (vía MP1584-EN) |

> Los OpenMV H7 aceptan 3.3 V y 5 V en VIN. Confirmar con Enzo qué pin de
> alimentación está usando en el cableado real para evitar dañar el módulo.

## 6. LEDs de diagnóstico (built-in del OpenMV H7)

El OpenMV H7 tiene 3 LEDs (rojo, verde, azul). El script actual los usa así:

| LED | Indica |
|---|---|
| **Rojo** | Pelota naranja detectada |
| **Verde** | Arco amarillo detectado |
| **Azul** | Arco azul detectado |

Útil para diagnosticar visualmente sin tener que mirar el Serial Monitor.

## 7. Pendientes de hardware específicos (no bloquean uso del pack)

1. **Confirmar modelo OpenMV** — H7 vs H7 Plus (TASK-013).
2. **Medir altura `h` real** de la cámara trasera sobre el suelo (placeholder 18.7 cm).
3. **Medir ángulo de inclinación** y distancia al centro del robot (para Nivel 3+ EKF).
4. **Verificar wiring U9 ↔ Serial5 Teensy** con osciloscopio (TASK-008).
5. **Verificar HMIRROR/VFLIP del montaje trasero** — probablemente distintos a los del frontal si el chasis es simétrico.

## 8. Por qué la cámara trasera importa para el juego

Aunque la mayoría de la pelota está en frente del robot, la cámara trasera
es crítica para:

- **Arquero**: ver rebotes de su propio arco que van hacia atrás (puede
  perseguirla).
- **Delantero**: detectar cuándo perdió la pelota detrás suyo (puede girar
  más rápido para recuperarla).
- **Defensa**: ver rivales que se acercan por atrás.

Sin cámara trasera, el robot tiene un "ángulo muerto" de ~180° donde no
ve nada. La fusión front+back del TOP da una cobertura completa de 360°.

## 9. Referencias

- Código vivo del lado Teensy: [`firmware/teensy/cameras_runtime.h`](firmware/teensy/cameras_runtime.h).
- Lógica de la rotación 180° en la fusión: [`firmware/teensy/cameras_fusion.cpp`](firmware/teensy/cameras_fusion.cpp) líneas 25–29.
- Definición de los UARTs del TOP: [`firmware/teensy/config_top.h`](firmware/teensy/config_top.h) líneas 43–55.
- Skill de calibración LAB / homografía: `.claude/skills/openmv-vision-tuning/SKILL.md` en el repo.
- Pinout del Teensy 4.0 del TOP completo: `../top-board-pack/01-pinout-y-hardware.md`.
- Pack de la cámara frontal: `../cameraFront-pack/`.
- Hardware OpenMV (datasheets): https://docs.openmv.io/openmvcam/quickref.html
