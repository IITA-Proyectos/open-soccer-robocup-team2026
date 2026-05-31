---
title: "Análisis del subsistema de visión — detección + protocolo cámara↔Teensy + migración OpenMV H7 Plus → N6"
date: 2026-05-31
author: "Claude Opus 4 (Anthropic), vía Claude Code — workflow (5 agentes de análisis en paralelo + síntesis)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus, Anthropic) — modo workflow"
status: final
tags: [vision, openmv, n6, camaras, deteccion, protocolo, uart, velocidad, incheon, analisis]
robot: top
area: vision
tipo: analisis
---

# Análisis de visión — 2 OpenMV N6 → TOP (Teensy 4.0) → CENTRAL

> **TL;DR.** Hoy hay **un solo script genérico** (`current-generic.py`) flasheado en
> ambas cámaras, con **3 bugs P0** (pelota fantasma por sentinel roto, crash por
> `bytearray` sin clamp, exposición automática que invalida los thresholds LAB).
> Existen `target-cam-*-template.py` con esos bugs ya corregidos pero **sin
> flashear**. El protocolo es **9 bytes crudos** (headers 201/202/203 + X + Y+100),
> 19200 8N1, sin CRC. Detección por `find_blobs` + LAB de pelota naranja / arco
> amarillo / arco azul. La **velocidad de la pelota** se puede calcular en el TOP
> (los `ball_vx/vy` del WorldSnapshot existen y `bt_classify` ya los consume, pero
> `build_snapshot()` los deja en 0). **Prioridad del usuario — cargar programas a
> las N6:** OpenMV IDE actualizado + firmware oficial N6 vía DFU (USB-C); el código
> `find_blobs`/LAB/UART **porta tal cual**, lo único a cambiar es la capa de cámara
> (`sensor` → `csi`) y **recalibrar LAB** (sensor PAG7936 ≠ el de la H7).

> **Origen:** generado por un workflow (5 agentes de análisis + síntesis). Lo
> dependiente de hardware está marcado **A-CONFIRMAR** (lo cierra quien prueba en banco).

## 1. Estado actual de la visión

Todos los scripts viven en `hardware/electronics/cameraFront-pack|cameraBack-pack/firmware/openmv/`.

- **VIVO (producción, con bugs P0):** `current-generic.py` — un único script genérico
  flasheado en **ambas** cámaras. Misma lógica que el legacy
  `software/vision/enviar coordenadas 2 arcos y pelota` (existe, 156 líneas; `.py` sin extensión).
- **OBJETIVO (TASK-022, bugs corregidos, SIN flashear):** `target-cam-frontal-template.py`
  (`CAM_ID=0`) y `target-cam-trasera-template.py` (`CAM_ID=1`). La trasera **no rota**
  internamente — la rotación 180° la hace el TOP (`cameras_fusion.cpp:25-30`).

**Qué detectan:** blob más grande por color LAB → **pelota naranja** (header 201),
**arco amarillo** (202), **arco azul** (203). Pipeline (`current-generic.py:112-156`):
`snapshot()` QVGA RGB565 → `find_blobs([thr], merge=True)` → blob mayor → homografía 3×3 +
corrección de perspectiva → 9 bytes por UART ~30 Hz. Sin ROI (frame completo 320×240).

## 2. Protocolo cámara ↔ Teensy

**Transporte (VERIFICADO):** 19200 8N1. Cámara `UART(3, 19200)` (`current-generic.py:6`);
Teensy `Serial3`/`Serial7` (`cameras_runtime.cpp:91-92`). **Frontal = Serial3** (RX15/TX14,
U8); **trasera = Serial7** (RX28/TX29, U9 — movida de Serial5 el 2026-05-29; pines U9 A-CONFIRMAR).

**Formato — 9 bytes crudos, sin framing** (cámara `current-generic.py:149-155`; TOP `cameras.cpp:30-95`):

| byte | contenido | encoding |
|---|---|---|
| 0 | **201** header pelota | sync |
| 1 | X pelota | uint8 0..200 |
| 2 | Y pelota | **Y+100** |
| 3 | **202** header arco amarillo | sync |
| 4 | X amarillo | uint8 |
| 5 | Y amarillo | Y+100 |
| 6 | **203** header arco azul | sync |
| 7 | X azul | uint8 |
| 8 | Y azul | Y+100 |

La cámara da (X,Y) en cm (homografía), codifica **Y como Y+100** (Y∈[−100,100]→0..200); X crudo.
El TOP decodifica `Y = byte − 100`. FSM de 9 estados con resync (`cameras.cpp:50-75`), drenado
acotado 64 B/tick, watchdog 1000 ms.

**Gaps (VERIFICADO):** sin CRC / sin fin de trama / sin longitud; riesgo de colisión header-dato
si X desborda y vale 201/202/203; sentinel "no visible" ambiguo; `CAMERA_UNIT_TO_MM=10.0` placeholder.
Migración objetivo (post-Incheon, en `CONTRATO-DATOS-CAMARAS.md`): framing `proto.h` con CRC16.

## 3. Plan de detección (azul + amarillo + naranja, Y como distancia)

**Método (mantener):** `find_blobs([thr_LAB], pixels_threshold, area_threshold, merge=True)`, blob mayor.
Thresholds LAB actuales (`current-generic.py:58-60`):

| Objeto | Threshold (L,A,B mín/máx) | pixels_min vivo→target |
|---|---|---|
| Pelota naranja | `(21,67, 18,79, −32,127)` | 7 → 20 |
| Arco amarillo | `(17,70, −27,14, 38,111)` | 600 |
| Arco azul | `(4,36, −13,57, −64,−4)` | 300 |

**3 bugs P0 (VERIFICADO) — corregidos en los templates, falta flashear:**
1. **Sentinel roto:** sin blob retorna `(0,0)` → `Y_coded=100` → el TOP cree que ve la pelota en
   el origen → persigue fantasma. Fix: sentinel `Y_coded=0`.
2. **Crash bytearray:** `bytearray(packet)` sin clamp; X<0 o >255 lanza `ValueError` y **cuelga la
   cámara**. Fix: clampear todo a [0,255].
3. **Exposición automática:** auto-WB + auto-gain **ON** invalidan los LAB al cambiar la luz. Fix:
   autos OFF + exposición fija (a re-ajustar con la N6).

**Y como distancia — viable pero no directo:** la cámara ya proyecta a cm en el plano del suelo
(Y NO es el pixel crudo). Si el objeto está al frente, Y≈distancia; la distancia real es
`sqrt(X²+Y²)` (lo que ya hace `fuse_goal_dual`). **Falta:** calibrar `H_MATRIX` por cámara
(la trasera con la suya), fijar `CAMERA_UNIT_TO_MM` contra cancha (error <10% a 30/50/80/100 cm),
ampliar el clamp de Y (hoy satura a 1 m), flag de visibilidad explícito, y **resolver el signo de
`atan2`** (docs contradictorias: `cameras_fusion.h:40` +90°=derecha vs `CONTRATO §4.4` +90°=izquierda → A-CONFIRMAR en banco).

## 4. Velocidad y dirección de la pelota

**Factible y de bajo riesgo → calcularla en el TOP** (no en la cámara, no en CENTRAL):
- **Cámara NO** (protocolo de 9 bytes fijo, sin timestamp; no tocar OpenMV en Hito 1).
- **TOP SÍ** (ahí está la fusión + posición en marco-robot).
- **CENTRAL solo consume:** `bt_classify()` ya espera `ball_vx/vy_mm_s` (7 tests verdes) pero hoy
  entra v=0 → todo `BT_STILL`.

**Hueco exacto (VERIFICADO):** `WorldSnapshot.ball_vx/vy` existen y viajan (v2, 27 B, static_assert),
pero `build_snapshot()` (`main_top.cpp:68-71`) escribe posición y NO velocidad. `BallFused`
(`cameras_fusion.h:31-36`) no tiene campos de velocidad.

**MVP (sin tocar OpenMV ni el contrato):** estimador en TOP host-testable (estado
`{last_x,last_y,last_ms,vx,vy,valid}`), **derivar solo al llegar packet nuevo** (datos 30 Hz, loop
100 Hz; usar `g_last_packet_ms_*` para el `dt`), EMA α≈0.3-0.5, **reset al perder la pelota**
(descartar el primer frame al reaparecer), getters `cameras_get_ball_vx/vy` + las **2 líneas que
faltan** en `build_snapshot()`. Velocidad cualitativa ("¿viene hacia mí?"), suficiente para encender
toda la cadena `bt_classify` ya testeada.

## 5. ★ Migración H7 Plus → N6 + cómo CARGAR programas ★

**Cambio de chip:** N6 = **STM32N6** (Cortex-M55 800 MHz + NPU Neural-ART, 4.2 MB RAM), sensor
**PAG7936 1 MP global shutter** (≠ el OV de la H7), **USB-C** (la H7 es micro-USB), VIN 4.7–5.7 V
(~150 mA@5V), I/O 3V3. Fuentes: openmv.io/products/openmv-n6, blog.st.com/openmv-n6, docs.openmv.io.

**Pasos para flashear cada N6 (×2):**
1. Actualizar **OpenMV IDE** a la versión que lista "OpenMV N6" (el firmware de la H7 **NO** sirve).
2. Conectar por **USB-C**; en la primera conexión la IDE ofrece instalar el firmware oficial.
3. Bootloader **DFU** (no el legacy de la H7). Si no entra solo → **doble-pulsar RESET**.
4. Ante "erase internal filesystem" → **No** (salvo querer limpiar).
5. **Connect** + play. Repetir en la 2ª unidad.

**Qué del código H7 porta TAL CUAL:** toda la lógica `find_blobs([thr])`, las 6-tuplas LAB,
`blob.cx()/cy()/rect()/pixels()`, la homografía, el empaquetado UART `[201,X,Y,202,…]`, `pyb.LED`.

**Qué cambiar (solo la capa de adquisición):** el script usa el módulo `sensor` **deprecado desde
fw 4.5** → en N6 migrar a `csi`:
- `import sensor` → `import csi`; `csi0 = csi.CSI()`
- `sensor.reset()` → `csi0.reset()`; `set_pixformat/framesize` → `csi0.pixformat/framesize`
- `set_auto_whitebal/gain/exposure` → `csi0.auto_whitebal/auto_gain/auto_exposure`
- `set_hmirror/vflip` → `csi0.hmirror/vflip`; `skip_frames` → `csi0.snapshot(time=...)`; `snapshot()` → `csi0.snapshot()`
- **`UART(3,…)`: A-CONFIRMAR la numeración de UART en N6** (3 buses; baud/protocolo/cableado a Serial3 del Teensy se mantienen).

**Riesgos:**
- **Recalibrar LAB obligatorio** (sensor distinto → color/balance cambian) + rehacer homografía. Skill `openmv-vision-tuning`.
- **Global shutter:** ventaja real con el robot en movimiento; la exposición fija se re-ajusta.
- **UART:** confirmar pin/numeración N6 (niveles 3V3 OK con Teensy 4.0).
- **Alimentación:** VIN 4.7–5.7 V → el MP1584 (5 V) sirve; no meter 5 V en I/O.
- **Doc del repo desactualizada:** `cameraFront-pack/01-hardware-y-conexion.md` aún dice "H7/H7 Plus" → actualizar a N6 (cantidad de LEDs RGB A-CONFIRMAR, no asumir).

**¿NPU?** Para Incheon **NO**: pelota + arcos de color fijo → `find_blobs` clásico es más simple,
determinista, >30 fps y reusa todo lo calibrado. NPU/YOLO (dataset + entrenamiento) = mejora post-torneo.

## 6. Plan de acción priorizado (camino crítico: hardware N6 primero)

| # | Tarea | Tipo | Depende de | Estado |
|---|---|---|---|---|
| 1 | **Flashear las 2 N6** (firmware oficial + script migrado a `csi`) | Cámara | IDE actualizada + N6 en mano | A-CONFIRMAR (UART N6) |
| 2 | **Corregir 3 bugs P0** (sentinel, clamp, exposición) usando `target-*-template.py` | Cámara | #1 | fix escrito, VERIFICADO |
| 3 | **Recalibrar LAB + exposición** (naranja/amarillo/azul) | Cámara/banco | #1,#2 | A-CONFIRMAR (sensor nuevo) |
| 4 | **Calibrar `H_MATRIX` por cámara** + validar Y≈distancia (30/50/80/100 cm) | Cámara/banco | #3 | A-CONFIRMAR |
| 5 | **Fijar `CAMERA_UNIT_TO_MM`** contra cancha | Teensy + banco | #4 | placeholder=10.0 |
| 6 | **Estimador de velocidad en TOP** + 2 líneas en `build_snapshot()` + test | Teensy/C++ | **independiente (paralelo)** | hueco VERIFICADO |
| 7 | Verificar trasera Serial7/U9 + resolver signo `atan2` | Teensy + banco | placa armada | A-CONFIRMAR |
| 8 | (Post-Incheon) framing CRC16 + flag visible + NPU | ambos | — | no para Incheon |

**Recomendación:** lanzar **#6 (TOP/C++) en paralelo** con **#1 (cámara)** — #6 no depende del
hardware N6 y enciende toda la lógica de trayectoria ya testeada. El camino crítico real es
**#1→#2→#3→#4** (solo arranca con las N6 físicas).

## Archivos clave
- Cámara: `cameraFront-pack/firmware/openmv/{current-generic.py, target-cam-frontal-template.py}` + `cameraBack-pack/.../target-cam-trasera-template.py`; legacy `software/vision/enviar coordenadas 2 arcos y pelota`.
- TOP: `src/top/{cameras.cpp, cameras_runtime.cpp, main_top.cpp:68-71}`; shared `src/shared/{cameras_fusion.cpp, cameras_fusion.h:31-36, types.h:104-105, ball_trajectory.cpp}`.
- Docs: `docs/firmware/CONTRATO-DATOS-CAMARAS.md`; `cameraFront-pack/01-hardware-y-conexion.md` (a actualizar a N6).
