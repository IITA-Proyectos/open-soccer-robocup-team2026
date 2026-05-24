---
title: "Cámara FRONTAL — Funcionalidad (qué detecta y cómo)"
date: 2026-05-24
status: vigente
parte-de: cameraFront-pack
basado-en: software/vision/enviar coordenadas 2 arcos y pelota + docs/firmware/CONTRATO-DATOS-CAMARAS.md
---

# Cámara FRONTAL — Funcionalidad

## 1. Qué hace la cámara frontal

La cámara frontal es uno de los 2 **sensores principales de pelota y arcos**
del robot. Hace 3 cosas en cada frame:

1. **Detecta** la pelota naranja por color LAB (blob más grande).
2. **Detecta** el arco amarillo y el arco azul por color LAB (blob más grande
   de cada uno).
3. **Envía** las coordenadas de los 3 objetos al TOP por UART (9 bytes/packet
   @ ~30 Hz).

Si un objeto **no es detectado** en el frame, la cámara debería enviar un
**sentinel** (X=0, Y_coded=0) para indicarlo. **Hoy hay un bug P0 que rompe el
sentinel** — ver `03-protocolo-comunicacion.md` §2.

## 2. Inicialización (al boot de la OpenMV)

```python
sensor.reset()
sensor.set_pixformat(sensor.RGB565)      # color RGB565 (16 bpp)
sensor.set_framesize(sensor.QVGA)        # 320×240 px
sensor.set_auto_whitebal(False)          # ⚠️ HOY ESTÁ EN True — BUG P0 (gap 3)
sensor.set_auto_gain(False)              # ⚠️ HOY ESTÁ EN True — BUG P0 (gap 3)
sensor.set_auto_exposure(False, exposure_us=EXPOSURE_US)  # ⚠️ HOY COMENTADO
sensor.set_hmirror(True)                 # FRONTAL: True (verificar montaje)
sensor.set_vflip(True)                   # FRONTAL: True (verificar montaje)
sensor.skip_frames(time=500)             # estabilización
```

> ⚠️ **Los 3 "autos" deben estar en False** (gap P0 #3 del contrato). El
> script actual los tiene en True, lo que **invalida los thresholds LAB
> calibrados**: si la iluminación cambia (Salta → Incheon), la pelota
> "desaparece" o aparecen falsos positivos. Apagar autos y fijar `EXPOSURE_US`
> medido en la cancha de competencia.

## 3. Detección por color LAB

Cada color tiene un umbral en espacio LAB (L=luminosidad, A=verde↔rojo,
B=azul↔amarillo) — 6 valores: `(L_min, L_max, A_min, A_max, B_min, B_max)`.

| Objeto | Threshold actual (placeholder) | Pixels mínimos | Notas |
|---|---|---|---|
| **Pelota naranja** | `(21, 67, 18, 79, -32, 127)` | **7** ⚠️ bajo, gap P1 #6 | Subir mínimo a 20–50 para evitar ruido |
| **Arco amarillo** | `(17, 70, -27, 14, 38, 111)` | 600 | OK |
| **Arco azul** | `(4, 36, -13, 57, -64, -4)` | 300 | OK |

```python
naranja_blobs = img.find_blobs([naranja_threshold],
                                pixels_threshold=NARANJA_MIN,
                                area_threshold=NARANJA_MIN,
                                merge=True)
```

`merge=True` une blobs adyacentes (importante para pelota fragmentada por
luces).

Si hay múltiples blobs del mismo color, se elige el **más grande** (mayor
cantidad de píxeles). Para pelota, asumimos que el blob mayor es la pelota
real; para arcos, asumimos que es el arco propio (los rivales no juegan con
arcos amarillos/azules — los arcos son fijos del field).

## 4. Pipeline de procesamiento por frame

```
sensor.snapshot()                    →  imagen QVGA RGB565
  ↓
img.find_blobs([naranja_threshold])  →  lista de blobs naranjas
img.find_blobs([amarillo_threshold]) →  lista de blobs amarillos
img.find_blobs([azul_threshold])     →  lista de blobs azules
  ↓
procesar_blob(blobs) por cada color  →  (X_cam, Y_cam) en pixels (cx, cy del blob mayor)
  ↓
transformar(u, v) con H_MATRIX       →  (x, y) en cm (espacio físico ground plane)
  ↓
corrección de perspectiva *(h-r)/h   →  (X, Y) en cm (espacio del jugador)
  ↓
clamp + encode Y_coded = Y + 100     →  (X_uint8, Y_coded_uint8)
  ↓
bytearray([201, Xp, Ypc, 202, Xam, Yamc, 203, Xaz, Yazc])
  ↓
uart.write(packet)                   →  9 bytes por Serial3 al TOP
```

## 5. Frame rate y carga

| Métrica | Valor estimado |
|---|---|
| Resolución | 320×240 RGB565 (~150 KB/frame) |
| `find_blobs` por color | ~5-10 ms cada uno (3 colores = 15-30 ms total) |
| Transformación homográfica + clamp | ~1 ms |
| `uart.write` (9 bytes a 19200 baud) | ~5 ms para vaciar buffer |
| **Frame rate efectivo** | **~30 Hz** |
| Tiempo entre paquetes en el TOP | ~33 ms |

> 📌 El `print()` de debug (`current-generic.py:156`) consume ~3 ms y reduce
> el fps. En producción debe quitarse — `target-cam-frontal-template.py`
> ya lo omite.

## 6. Fusión con la cámara trasera (en el TOP)

La cámara frontal NO sabe que existe la cámara trasera. La fusión la hace el
TOP en `cameras_fusion.cpp`:

```
front camera (cam_id=0)  →  coords se usan DIRECTO
back camera  (cam_id=1)  →  coords se ROTAN 180° (x→-x, y→-y)
```

Reglas de fusión:
- Ambas ven el objeto → promedio ponderado con `CONF_SINGLE_CAMERA = 80` cada
  una → `confidence = 95` (consenso).
- Solo una ve → esa, `confidence = 80`.
- Ninguna ve → `visible = false`, `confidence = 0`.
- Cámara con watchdog vencido (1000 ms sin packets) → se ignora aunque el
  último packet decía visible.

Detalles del algoritmo de fusión: `firmware/teensy/cameras_fusion.cpp`.
**Tests**: `tests/test_cameras_fusion.cpp` (16 tests cubren rot 180°, fuse
front+back, watchdog stale, ambas ciegas).

## 7. LEDs de diagnóstico

| LED | Encendido si |
|---|---|
| Rojo (LED 1) | Hay al menos 1 blob naranja detectado (pelota) |
| Verde (LED 2) | Hay al menos 1 blob amarillo detectado (arco amarillo) |
| Azul (LED 3) | Hay al menos 1 blob azul detectado (arco azul) |

Permite diagnosticar visualmente sin Serial Monitor. **Si los LEDs no se
prenden con los objetos a la vista, hay que recalibrar los thresholds LAB**
(ver `04-calibracion-lab-y-homografia.md`).

## 8. Decisiones de diseño documentadas

| Decisión | Justificación |
|---|---|
| QVGA en vez de VGA | Frame rate alto (30 Hz). VGA bajaría a ~10 Hz. |
| Blob más grande gana | Pelota fragmentada por reflejos vale como blob único más grande. Para arcos, los blobs ajenos son ruido. |
| `merge=True` en `find_blobs` | Une blobs adyacentes del mismo color. Pelota dividida por luces se ve como 1 sola. |
| Sentinel (X=0, Y=0) en vez de byte separado | Compatibilidad con protocolo viejo de 9 bytes. Cambiar requiere breaking change del contrato. |
| Coordenadas en cm (no mm) | Heredado del firmware del 2025. El TOP convierte a mm con `CAMERA_UNIT_TO_MM`. |

## 9. Limitaciones conocidas

| # | Limitación | Impacto | Plan |
|---|---|---|---|
| 1 | Bug P0: sentinel `Y_coded=100` causa pelota fantasma en origen | Robot persigue pelota inexistente | Fix de 5 min, ver template objetivo |
| 2 | Bug P0: crash bytearray con coordenadas negativas | Cámara se detiene en partido | Clamp `[0,255]` antes de bytearray |
| 3 | Bug P0: auto-WB / auto-gain encendidos | Thresholds LAB no funcionan si cambia la luz | Apagar autos + fijar exposure |
| 4 | Homografía placeholder (NO calibrada) | Distancias/ángulos mal | Calibrar en posición real (TASK-022) |
| 5 | Sin checksum/CRC en el protocolo | Bytes coincidentes con headers (201/202/203) causan desincronización | Protocolo objetivo con CRC — post-Incheon |
| 6 | `pixels_threshold=7` para pelota | Ruido detectado como pelota | Subir a 20–50 |
| 7 | Confianza fija (80) en la fusión | No diferencia detección grande vs pequeña | Agregar `pixels_count` al protocolo — post-Incheon |

## 10. Referencias

- Contrato byte-a-byte: [`03-protocolo-comunicacion.md`](03-protocolo-comunicacion.md)
- Calibración LAB + homografía: [`04-calibracion-lab-y-homografia.md`](04-calibracion-lab-y-homografia.md)
- Script actual (con bugs P0): [`firmware/openmv/current-generic.py`](firmware/openmv/current-generic.py)
- Template objetivo: [`firmware/openmv/target-cam-frontal-template.py`](firmware/openmv/target-cam-frontal-template.py)
- Parser del lado Teensy: [`firmware/teensy/cameras.{h,cpp}`](firmware/teensy/cameras.h)
- Fusión front+back: [`firmware/teensy/cameras_fusion.{h,cpp}`](firmware/teensy/cameras_fusion.h)
- Skill de calibración: `.claude/skills/openmv-vision-tuning/SKILL.md` en el repo.
- Hardware: [`01-hardware-y-conexion.md`](01-hardware-y-conexion.md)
