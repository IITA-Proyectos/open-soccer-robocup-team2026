---
title: "diag_cam_acceptance — aceptación de la visión recalibrada (banco)"
date: 2026-06-04
status: vivo
tipo: procedimiento-banco
robot: ambos
area: vision
relacionado: TASK-022
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
---

# diag_cam_acceptance — validar la visión en banco (TASK-022)

> **Qué responde:** *¿la cámara, ya recalibrada (LAB + homografía), reporta la
> pelota en la posición correcta?* Es el test de **aceptación** que cierra el
> bloqueante #1 de Incheon: no recalibra, **verifica**.

## Por qué importa

La recalibración de las 2 cámaras OpenMV N6 es el bloqueante real #1 (TASK-022).
Hasta ahora existía `diag_top_cameras` para validar el **enlace** (que lleguen
bytes), pero **no había forma de validar que la posición reportada sea correcta**
contra una distancia medida. Este sketch lo cubre y, de paso, valida el contrato
de cámara v2 (11 bytes, CRC8 + sentinel + END), porque usa el **parser de
producción** (`src/top/cameras.cpp`) — el mismo que corre en el TOP.

## Hardware

- Placa **TOP (Teensy 4.0)** por USB.
- Cámara **FRONTAL** en `Serial3` (RX3 = pin 15, conector U8).
- Cámara **TRASERA** en `Serial5` (RX5 = pin 21, conector U9).
- Baud **19200** (= `cam-*-n6.py` y `cameras_runtime`).
- Una **pelota naranja** y una **cinta métrica**.

> Pre-requisito: las cámaras deben estar con los thresholds LAB recalibrados para
> la luz real (si no, `BALL vis=N` siempre). Ver `CALIBRACION-VISION-N6.md`.

## Flashear y abrir

```
pio run -e diag_cam_acceptance -t upload
pio device monitor -b 115200
```

## Lectura en vivo

Cada ~300 ms imprime una línea por cámara:

```
FRONTAL pkts=1234 crc=0 rsy=0 | BALL vis=Y raw=(3,58) mm=(30,580) ang=3.0deg dist=581mm | YEL=N BLU=Y
TRASERA pkts=0 crc=0 rsy=0 | BALL vis=N | YEL=N BLU=N
```

- `pkts` sube → el enlace anda. `crc` / `rsy` deberían quedarse en 0 (si suben,
  hay ruido en el UART o desajuste de baud/contrato).
- `raw` = coords crudas de la cámara (unidades, ya con el offset de 100 restado).
- `mm` = `raw × 10` (factor `CAMERA_UNIT_TO_MM`; **éste es justo el que TASK-022
  debe calibrar** — si la distancia reportada está escalada, ajustá ese factor).
- `ang` = ángulo a la pelota (0° = al frente, + = a la derecha).
- `dist` = distancia en mm.

## Modo aceptación (PASS / FAIL)

1. Poné la pelota a una distancia **medida con cinta**, centrada al frente
   (p.ej. 60 cm).
2. Escribí la distancia en **centímetros** + Enter (`60`).
3. Apretá **`p`**. Compara reportado vs esperado:

```
---- ACEPTACION ----
Camara: FRONTAL
  Esperado=600mm  Reportado=581mm  err=19mm  tol=60mm  -> PASS
  Angulo reportado=3.0deg (0=al frente; ~0 si la pelota esta centrada)
```

- **Tolerancia** = la mayor entre **±5 cm** y **±10 %** de la distancia esperada.
- Repetí a **30 / 60 / 90 cm** (y a izquierda/derecha mirando el ángulo). Si
  los 3 dan PASS y el ángulo es coherente, la homografía está aceptada.
- Si el error crece con la distancia → la **homografía** (H_MATRIX) está mal;
  si es un escalado parejo → ajustá `CAMERA_UNIT_TO_MM` (o la H).

### Comandos

| Tecla | Acción |
|---|---|
| `<número>` + Enter | distancia esperada de la pelota, en **cm** |
| `p` | chequear PASS/FAIL de la cámara seleccionada |
| `f` / `b` | elegir cámara **f**rontal / **b**ack (trasera) para el chequeo |
| `r` | resetear los contadores de los parsers |

## Notas

- **Aislado:** el sketch vive en `src/diag/diag_cam_acceptance.cpp` y solo compila
  el parser de producción (`src/top/cameras.cpp`). Ningún módulo de competencia lo
  incluye → **riesgo de regresión nulo**.
- No mide arcos por distancia (solo `visible`): el foco es la **pelota**, que es
  lo que la FSM persigue. Extenderlo a los arcos es trivial si hace falta.
- Si `BALL vis=N` con la pelota presente: recalibrar LAB, revisar luz, o acercar.
  Si `crc`/`rsy` suben: confirmar baud 19200 y que la cámara corre el `.py` v2.
