---
title: "Pack autocontenido — Cámara FRONTAL (OpenMV) del robot Soccer 2026"
date: 2026-05-24
status: vigente
audiencia: "IA + humanos que programen la cámara frontal"
fuente-canonica: "docs/FUENTES-DE-VERDAD.md"
---

# Pack autocontenido — Cámara FRONTAL (OpenMV)

## Para qué existe este directorio

Todo lo que hace falta saber para **programar, calibrar y diagnosticar** la
**cámara frontal** (la OpenMV que mira hacia adelante del robot), **en un
solo lugar**.

La cámara frontal es uno de los 2 OpenMV del robot. Su par es la cámara
trasera (ver pack `cameraBack-pack/`). Ambas hacen el mismo trabajo
conceptualmente — detectar pelota naranja + arcos amarillo/azul por color
LAB — pero **están en posiciones físicas distintas, conectadas a UARTs
distintos del TOP, y necesitan calibración independiente** (homografía,
exposición, montaje).

## Regla de oro

> Si lo que está en este pack **contradice** algo del repo vivo
> (`software/vision/` y `software/teensy/Soccer 2026/src/top/cameras*`),
> **gana el repo vivo**. Este pack es una **foto curada del 2026-05-24** —
> los archivos `.py` y `.cpp/.h` son copias snapshot, NO son la fuente
> ejecutable.

**Compilación y ejecución siguen viviendo en sus ubicaciones originales**:
- Firmware OpenMV: `software/vision/enviar coordenadas 2 arcos y pelota` se
  flashea desde **OpenMV IDE** a la cámara física (no por PlatformIO).
- Parser del TOP: `pio run -e top` apunta a `src/top/cameras.{h,cpp}` y
  `src/top/cameras_runtime.{h,cpp}`.
- Tests: `pio test -e native` apunta a `test/test_cameras_fusion/`.

## ⚠️ Estado especial del firmware OpenMV (importante leer antes)

**Hoy hay UN SOLO script genérico** para ambas cámaras
(`software/vision/enviar coordenadas 2 arcos y pelota`, incluido en este pack
como `firmware/openmv/current-generic.py`). Para que las 2 cámaras funcionen
correctamente **hay que duplicar y calibrar 2 scripts separados**, porque la
homografía, el HMIRROR/VFLIP y el FOV son distintos para cada montaje físico.

Esa duplicación + calibración es **TASK-022**, abierta. Mientras tanto, este
pack incluye:
- El script genérico actual con su bugs P0 documentados (sentinel roto, crash
  bytearray, auto-WB encendido).
- Un **template objetivo** (`firmware/openmv/target-cam-frontal-template.py`)
  pensado específicamente para la cámara frontal, con valores tentativos y
  placeholders donde Enzo / Virginia tienen que medir.

## Estructura del pack

```
cameraFront-pack/
├── README.md                                  ← estás aquí
├── 01-hardware-y-conexion.md                  ← qué OpenMV + UART específico + montaje + FOV
├── 02-funcionalidad.md                        ← qué detecta + parámetros LAB + blobs + frame rate
├── 03-protocolo-comunicacion.md               ← contrato byte-a-byte de los 9 bytes/packet
├── 04-calibracion-lab-y-homografia.md         ← workflow OpenMV-vision-tuning específico al frontal
├── firmware/
│   ├── openmv/                                ← código que corre en la cámara OpenMV física
│   │   ├── current-generic.py                 (snapshot del script actual, NO calibrado por cámara)
│   │   ├── target-cam-frontal-template.py     (template objetivo con bugs P0 corregidos)
│   │   └── README.md                          (explica el gap entre actual y objetivo)
│   └── teensy/                                ← código del lado TOP que escucha esta cámara
│       ├── cameras.{h,cpp}                    (parser robusto de los 9 bytes — común a ambas cámaras)
│       ├── cameras_runtime.{h,cpp}            (wiring sobre Serial3 + Serial5 — ambas cámaras)
│       ├── cameras_fusion.{h,cpp}             (fusión front+back, rota 180° la back — común)
│       └── config_top.h                       (define UARTs y bauds — común)
└── tests/
    └── test_cameras_fusion.cpp                (16 tests host-native que cubren la fusión de ambas)
```

## Índice por pregunta

| Pregunta | Doc del pack |
|---|---|
| ¿Qué modelo OpenMV es la cámara frontal? | `01-hardware-y-conexion.md` §1 (H7 / H7 Plus, confirmar con TASK-013) |
| ¿En qué Serial del Teensy 4.0 entra la cámara frontal? | `01-hardware-y-conexion.md` §2 (**Serial3**, RX=15, TX=14, conector U8) |
| ¿A qué baud rate habla la cámara con el TOP? | `01-hardware-y-conexion.md` §2 (**19200 8N1**) |
| ¿Dónde está montada físicamente la cámara frontal? | `01-hardware-y-conexion.md` §3 (mira hacia +Y del robot) |
| ¿Qué detecta la cámara? | `02-funcionalidad.md` §1 (pelota naranja + arco amarillo + arco azul) |
| ¿Cómo funciona la detección por color LAB? | `02-funcionalidad.md` §3 + skill `openmv-vision-tuning` |
| ¿Qué frame rate alcanza? | `02-funcionalidad.md` §5 (~30 Hz QVGA, limitado por procesamiento OpenMV) |
| ¿Cuál es el formato binario del paquete que envía la cámara? | `03-protocolo-comunicacion.md` §1.2 (9 bytes, headers 201/202/203) |
| ¿Qué pasa cuando la cámara no detecta nada? | `03-protocolo-comunicacion.md` §2 (sentinel: X=0, Y_coded=0) |
| ¿Cuáles son los bugs P0 conocidos del script actual? | `03-protocolo-comunicacion.md` §9 (fantasma, crash bytearray, auto-WB) |
| ¿Cómo calibrar los thresholds LAB para Incheon? | `04-calibracion-lab-y-homografia.md` §2 + skill `openmv-vision-tuning` |
| ¿Cómo calibrar la homografía para esta cámara? | `04-calibracion-lab-y-homografia.md` §3 |
| ¿Por qué la cámara frontal NO rota coordenadas, pero la trasera SÍ? | `cameraBack-pack/01-hardware-y-conexion.md` (la rotación 180° aplica solo a back) |
| ¿Cómo se fusionan las 2 cámaras en el TOP? | `firmware/teensy/cameras_fusion.cpp` + ver `cameraFront-pack/02-funcionalidad.md` §6 |
| ¿Hay tests del parser? | `tests/test_cameras_fusion.cpp` (16 tests cubren rot 180°, fuse, watchdog) |

## ⚠️ Pendientes humanos importantes

**Crítico — bloqueante para Incheon (TASK-022):**

1. **Crear `cam_frontal.py` específico** — duplicar `current-generic.py`, ponerle
   nombre claro, y calibrar SUS PROPIOS valores de: HMIRROR/VFLIP, H_MATRIX,
   EXPOSURE_US, thresholds LAB. Ver template en `firmware/openmv/target-cam-frontal-template.py`.

2. **Corregir bug P0 del sentinel** (líneas 80-81 del script actual): cuando no
   hay blob, retornar `(0, 0)` para X e Y → `Y_coded = 0` (no 100). El parser
   del TOP ya espera esta convención.

3. **Corregir bug P0 del crash bytearray**: clamp todos los valores a `[0,255]`
   antes de `bytearray(packet)`.

4. **Apagar auto-WB y auto-gain**: `sensor.set_auto_whitebal(False)` +
   `sensor.set_auto_gain(False)` + `sensor.set_auto_exposure(False, exposure_us=X)`
   con `X` medido en la cancha de Incheon.

5. **Calibrar homografía para la cámara frontal** en su posición de montaje real
   (procedimiento en §3 de `04-calibracion-lab-y-homografia.md`).

**Importante pero no urgente:**

6. **Confirmar wiring físico** del conector U8 ↔ Serial3 del Teensy (TASK-008).
7. **Calibrar `CAMERA_UNIT_TO_MM`** del TOP (hoy en 10.0, placeholder) — medir
   pelota a 30/50/80/100 cm.
8. **Subir `pixels_threshold` de pelota** de 7 a 20–50 (hoy detecta ruido).
9. **Confirmar modelo OpenMV** (H7 vs H7 Plus) para BOM de Incheon.

## Lo que NO está en este pack (y por qué)

| Categoría | Por qué no está |
|---|---|
| Pack de la cámara trasera | Existe en paralelo: `cameraBack-pack/` |
| Código de TOP / DOWN / CENTRAL completo | Cada placa tiene su propio pack en `hardware/electronics/*-board-pack/` |
| OpenMV IDE / firmware del módulo OpenMV | No vive en este repo — se instala en la cámara desde el IDE oficial de OpenMV |
| Hardware de la cámara (datasheet) | Reference externa: https://docs.openmv.io/ |
| Journals, tasks, plans superpowers | Quedan en sus carpetas como historia/gestión |
| `mapa-pines-placas-nuevas.md` superado | Toda la info útil está en este pack |

## Diferencias con el pack `cameraBack-pack/`

Los 2 packs son casi idénticos en estructura pero difieren en:

| Aspecto | cameraFront-pack/ | cameraBack-pack/ |
|---|---|---|
| Serial del Teensy 4.0 | **Serial3** (RX=15, TX=14) | **Serial5** (RX=21, TX=20) |
| Conector en PCB TOP | **U8** "UART-CAMERA1" | **U9** "UART-CAMERA2" |
| Constante de baud | `UART_CAMERA1_BAUD` | `UART_CAMERA2_BAUD` |
| Dirección a la que mira físicamente | **+Y del robot** (adelante) | **−Y del robot** (atrás) |
| Rotación aplicada por la fusión | **0°** (sus coords se usan directo) | **180°** (la fusión invierte signo de x e y) |
| `cam_id` en `cameras_fusion.cpp` | `0` | `1` |
| HMIRROR / VFLIP recomendados | Verificar montaje (actual: True/True) | Verificar montaje (puede diferir) |
| H_MATRIX | Calibrada para posición frontal | Calibrada para posición trasera |

## Atribución

- Script OpenMV original (`enviar coordenadas 2 arcos y pelota`) — equipo IITA 2025/2026 (sin firma).
- Parser Teensy (`cameras.{h,cpp}` + `cameras_runtime` + `cameras_fusion`) — Claude + sesiones previas (ver journals).
- Contrato de datos cámaras (`CONTRATO-DATOS-CAMARAS.md`) — Claude (sonnet 4.6), 2026-05-18.
- Curado y consolidación del pack — Claude Opus 4.7 (Anthropic), sesión 2026-05-24.
- Requested-by — Gustavo Viollaz (@gviollaz).
