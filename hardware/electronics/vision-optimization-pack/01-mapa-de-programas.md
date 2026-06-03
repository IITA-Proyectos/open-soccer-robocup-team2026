---
title: "01 — Mapa de programas de visión: qué hay y DÓNDE está el canónico"
date: 2026-06-03
---

# 01 — Mapa de programas (dónde editar de verdad)

> El agente edita estos archivos **en su lugar**. Este pack no los duplica.

## Scripts que corren EN la cámara (OpenMV, MicroPython)

| Archivo (canónico) | Rol | ¿Editar? |
|---|---|---|
| `software/vision/enviar coordenadas 2 arcos y pelota` | Genérico **vivo original** (sin extensión; 1 script para ambas cámaras, con bugs P0). | Referencia — lo reemplazan los `cam-*-n6.py` |
| `hardware/electronics/cameraFront-pack/firmware/openmv/cam-frontal-n6.py` | **PRODUCCIÓN frontal** (N6, bugs P0 corregidos, flag `BRING_UP`). | **SÍ** |
| `hardware/electronics/cameraBack-pack/firmware/openmv/cam-trasera-n6.py` | **PRODUCCIÓN trasera**. | **SÍ** |
| `…/cameraFront-pack/firmware/openmv/calib-lab-n6.py` (y copia en cameraBack) | **Kit de calibración** LAB (sonda + tuple sugerido; no transmite). | **SÍ** (mejorarlo) |
| `…/cameraFront-pack/firmware/openmv/current-generic.py` | Snapshot del genérico que funciona (módulo `sensor` + `pyb.UART`). | Referencia (patrón a respetar) |

> ⚠️ Lo que el agente toca para "ver mejor" son **`cam-frontal-n6.py` y
> `cam-trasera-n6.py`** (y el kit `calib-lab-n6.py`). El generic queda de patrón.

## Parámetros editables en los `cam-*-n6.py` (líneas aprox.)

`BRING_UP` (autos ON/OFF) · `UART_PORT` · `EXPOSURE_US` · `HMIRROR`/`VFLIP` ·
`NARANJA/AMARILLO/AZUL_THRESHOLD` (LAB) · `*_PIXELS_MIN` · `H_MATRIX` ·
`CAM_HEIGHT_CM` · `SENTINEL_*`.

## Lado TOP — parser + fusión (C++, host-testeable)

| Archivo (canónico) | Rol |
|---|---|
| `software/teensy/Soccer 2026/src/top/cameras.{h,cpp}` | Parser robusto de los 9 bytes (común a ambas cámaras). |
| `…/src/top/cameras_runtime.{h,cpp}` | Wiring sobre Serial3 (frontal) + Serial5 (trasera) + velocidad de pelota. |
| `…/src/shared/cameras_fusion.{h,cpp}` | Fusión front+back (rota 180° la trasera, watchdog). 16 tests. |
| `…/src/shared/ball_velocity.{h,cpp}` | Velocidad de la pelota (EMA + reset al perder). 16 tests. |

## Tests host-native (correr con `bash scripts/run-host-tests.sh`)

`test/test_cameras_fusion/` (16) · `test/test_ball_velocity/` (16).

## Contratos / docs de referencia

- Protocolo 9 bytes: `docs/firmware/CONTRATO-DATOS-CAMARAS.md`.
- Packs de hardware por cámara: `cameraFront-pack/` y `cameraBack-pack/` (01-hardware, 02-funcionalidad, 03-protocolo, 04-calibración).
- Procedimiento de calibración de banco: `docs/firmware/CALIBRACION-VISION-N6.md`.
- Flasheo N6: `hardware/electronics/CAMARAS-N6-FLASHEO-Y-CALIBRACION.md`.
