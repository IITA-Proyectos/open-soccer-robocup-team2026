---
title: "Cámara FRONTAL — Hardware y conexión"
date: 2026-05-24
status: vigente
parte-de: cameraFront-pack
---

# Cámara FRONTAL — Hardware y conexión

## 1. Qué módulo OpenMV es

| Atributo | Valor | Confianza |
|---|---|---|
| Familia | **OpenMV N6** | ✅ confirmado en banco 2026-05-31 |
| Procesador | STM32N6 (Cortex-M55 + Neural-ART NPU) | ✅ |
| Sensor de imagen | **PAG7936** | ✅ |
| Resolución usada | **QVGA (320×240) RGB565** | ✅ por código |
| Frame rate efectivo | **~30 Hz** (limitado por procesamiento de blobs) | ✅ estimación |
| Lente | Lente standard ~2.8 mm (FOV ~70° horizontal) | ⚠️ confirmar con Enzo |
| Firmware | MicroPython (script en `firmware/openmv/`) | ✅ |

> 📌 Las cámaras son **OpenMV N6** (sensor PAG7936), confirmado en banco
> 2026-05-31. El código del repo usa QVGA; el N6 da margen amplio para subir
> resolución o usar la NPU en el futuro.

## 2. Conexión con la placa TOP

| Atributo | Valor |
|---|---|
| **Puerto en el Teensy 4.0 (TOP)** | `Serial3` |
| Pin Arduino RX (TOP recibe de la cámara) | **15** |
| Pin Arduino TX (TOP envía a la cámara — no usado) | **14** |
| Pin header Teensy | header 16 (RX) + header 17 (TX) |
| Conector físico en el PCB TOP | **U8** "UART-CAMERA1" |
| Baud rate | **19200 8N1** |
| Constante en firmware Teensy | `UART_CAMERA1_BAUD = 19200` en `config_top.h:53` |
| Constante en firmware OpenMV | `UART(3, 19200)` en `current-generic.py:6` (puerto del OpenMV, no del Teensy) |
| Sentido de datos | Cámara → TOP (unidireccional en práctica; TX del TOP existe pero no se usa) |

> ⚠️ **Pendiente de confirmación (TASK-008)**: el wiring físico entre el
> conector U8 del PCB TOP y los pines 15/14 del Teensy 4.0 NO está verificado
> con osciloscopio. Si U8 va a otros pines del Teensy, la cámara frontal no
> envía a Serial3 sino al UART que esté cableado en U8.

## 3. Montaje físico en el robot

| Atributo | Valor | Confianza |
|---|---|---|
| Dirección que mira | **+Y del robot** (adelante) | ⚠️ confirmar con Enzo (asunción del repo) |
| Altura sobre el suelo (h) | **18.7 cm** (placeholder del script) | ⚠️ medir en el robot real (regla en la cancha) |
| Ángulo de inclinación | (no documentado) | ⚠️ medir |
| Distancia al centro del robot | (no documentado) | ⚠️ medir |
| HMIRROR | `True` (script actual) | ⚠️ verificar según montaje físico real |
| VFLIP | `True` (script actual) | ⚠️ verificar según montaje físico real |

> 📌 **Cómo se ajustan HMIRROR/VFLIP**: si al ver el preview del OpenMV IDE la
> pelota aparece a la izquierda cuando físicamente está a la derecha, hay que
> invertir HMIRROR. Si aparece arriba cuando está abajo, invertir VFLIP. Esto
> se calibra UNA VEZ al montar la cámara y queda fijo.

## 4. Coordenadas que envía esta cámara

La cámara frontal envía coordenadas (X, Y) en **centímetros** según la
homografía calibrada (ver `04-calibracion-lab-y-homografia.md` §3). El TOP
las recibe y las **NO rota** (usa `cam_id = 0` en
`cameras_fusion.cpp:25-29`).

Por convención del TOP:
- **+Y = frente del robot** (alejándose de la cámara frontal, que MIRA al
  frente)
- **+X = lateral derecho del robot**
- Origen ≈ proyección de la cámara en el suelo (no centro del robot)

> 📌 El centro de coordenadas técnico del frame de la cámara frontal es la
> **base de la cámara** proyectada al suelo. La diferencia con el "centro del
> robot" es típicamente pequeña (decenas de mm) y se ignora en Nivel 1+2.
> Para Nivel 3+ (EKF) habrá que sumar el offset cámara→centro a las
> coordenadas reportadas.

## 5. Pines del lado OpenMV

| Pin OpenMV | Función | Conectado a |
|---|---|---|
| **UART3 TX** | Cámara TX → Teensy RX | pin 15 del Teensy 4.0 (Serial3 RX) |
| **UART3 RX** | Cámara RX ← Teensy TX (no usado) | pin 14 del Teensy 4.0 (Serial3 TX) |
| **GND** | Masa común | GND del Teensy / PCB TOP |
| **VIN o 3V3** | Alimentación | desde el regulador 5V del PCB TOP (vía MP1584-EN) |

> Los OpenMV N6 aceptan 3.3 V y 5 V en VIN. Confirmar con Enzo qué pin de
> alimentación está usando en el cableado real para evitar dañar el módulo.

## 6. LEDs de diagnóstico (built-in del OpenMV N6)

El OpenMV N6 tiene 3 LEDs (rojo, verde, azul). El script actual los usa así:

| LED | Indica |
|---|---|
| **Rojo** | Pelota naranja detectada |
| **Verde** | Arco amarillo detectado |
| **Azul** | Arco azul detectado |

Útil para diagnosticar visualmente sin tener que mirar el Serial Monitor.

## 7. Pendientes de hardware específicos (no bloquean uso del pack)

1. ✅ **RESUELTO 2026-05-31** — modelo confirmado: **OpenMV N6 (sensor PAG7936)**.
2. **Medir altura `h` real** de la cámara frontal sobre el suelo (placeholder 18.7 cm).
3. **Medir ángulo de inclinación** y distancia al centro del robot (para Nivel 3+ EKF).
4. **Verificar wiring U8 ↔ Serial3 Teensy** con osciloscopio (TASK-008).
5. **Documentar el cable y conector exactos** usados entre OpenMV y PCB TOP.

## 8. Referencias

- Código vivo del lado Teensy: [`firmware/teensy/cameras_runtime.h`](firmware/teensy/cameras_runtime.h).
- Definición de los UARTs del TOP: [`firmware/teensy/config_top.h`](firmware/teensy/config_top.h) líneas 43–55.
- Skill de calibración LAB / homografía: `.claude/skills/openmv-vision-tuning/SKILL.md` en el repo.
- Pinout del Teensy 4.0 del TOP completo: `../top-board-pack/01-pinout-y-hardware.md`.
- Hardware OpenMV (datasheets): https://docs.openmv.io/openmvcam/quickref.html
