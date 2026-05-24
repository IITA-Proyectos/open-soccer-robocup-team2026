---
title: "Cámara TRASERA — Funcionalidad (qué detecta y cómo)"
date: 2026-05-24
status: vigente
parte-de: cameraBack-pack
basado-en: software/vision/enviar coordenadas 2 arcos y pelota + docs/firmware/CONTRATO-DATOS-CAMARAS.md
---

# Cámara TRASERA — Funcionalidad

## 1. Qué hace la cámara trasera

La cámara trasera es uno de los 2 **sensores principales de pelota y arcos**
del robot. Hace 3 cosas en cada frame, **idénticas a la cámara frontal**:

1. **Detecta** la pelota naranja por color LAB (blob más grande).
2. **Detecta** el arco amarillo y el arco azul por color LAB (blob más grande
   de cada uno).
3. **Envía** las coordenadas de los 3 objetos al TOP por UART (9 bytes/packet
   @ ~30 Hz).

Si un objeto **no es detectado** en el frame, la cámara debería enviar un
**sentinel** (X=0, Y_coded=0) para indicarlo. **Hoy hay un bug P0 que rompe
el sentinel** — ver `03-protocolo-comunicacion.md` §2.

**Diferencia con la frontal**: la cámara trasera mira el mundo opuesto. La
**rotación 180° la aplica el TOP** (no la cámara). Para la cámara, sus
coordenadas son "lo que veo enfrente de mí" (que físicamente es "atrás del
robot").

## 2. Inicialización (al boot de la OpenMV)

```python
sensor.reset()
sensor.set_pixformat(sensor.RGB565)      # color RGB565 (16 bpp)
sensor.set_framesize(sensor.QVGA)        # 320×240 px
sensor.set_auto_whitebal(False)          # ⚠️ HOY ESTÁ EN True — BUG P0 (gap 3)
sensor.set_auto_gain(False)              # ⚠️ HOY ESTÁ EN True — BUG P0 (gap 3)
sensor.set_auto_exposure(False, exposure_us=EXPOSURE_US)  # ⚠️ HOY COMENTADO
sensor.set_hmirror(True)                 # ⚠️ verificar — probablemente DISTINTO al frontal si el montaje es espejo
sensor.set_vflip(True)                   # ⚠️ verificar — idem
sensor.skip_frames(time=500)             # estabilización
```

> ⚠️ **HMIRROR / VFLIP de la cámara trasera pueden diferir del frontal**.
> Si el chasis es simétrico y la cámara trasera está montada como espejo de
> la frontal (cable hacia el otro lado), HMIRROR y/o VFLIP se invierten.
> Calibrar viendo el preview en el OpenMV IDE: una flecha que apunta hacia
> +Y del robot (adelante) tiene que aparecer apuntando hacia ABAJO en el
> preview de la cámara trasera (porque la cámara MIRA hacia atrás).

## 3. Detección por color LAB

Idéntica a la cámara frontal. Mismos thresholds iniciales, pero **pueden
ajustarse independientemente** si la iluminación recibida por la cámara
trasera es distinta (por ej, sombras del propio robot que tapan la luz del
techo).

| Objeto | Threshold inicial (mismo que frontal) | Pixels mínimos |
|---|---|---|
| **Pelota naranja** | `(21, 67, 18, 79, -32, 127)` | **7** ⚠️ subir a 20–50 |
| **Arco amarillo** | `(17, 70, -27, 14, 38, 111)` | 600 |
| **Arco azul** | `(4, 36, -13, 57, -64, -4)` | 300 |

Si en Incheon la cámara trasera no detecta tan bien como la frontal (porque
está en la sombra del chasis del robot), recalibrar SUS thresholds
específicamente, con sensibilidad un poco más alta (rango LAB más amplio).

## 4. Pipeline de procesamiento por frame

Idéntico a la cámara frontal:

```
sensor.snapshot()                    →  imagen QVGA RGB565
  ↓
img.find_blobs([naranja_threshold])  →  lista de blobs naranjas
img.find_blobs([amarillo_threshold]) →  lista de blobs amarillos
img.find_blobs([azul_threshold])     →  lista de blobs azules
  ↓
procesar_blob(blobs) por cada color  →  (X_cam, Y_cam) en pixels (cx, cy del blob mayor)
  ↓
transformar(u, v) con H_MATRIX       →  (x, y) en cm (espacio físico, FRAME DE LA CÁMARA)
  ↓
corrección de perspectiva *(h-r)/h   →  (X, Y) en cm
  ↓
clamp + encode Y_coded = Y + 100     →  (X_uint8, Y_coded_uint8)
  ↓
bytearray([201, Xp, Ypc, 202, Xam, Yamc, 203, Xaz, Yazc])
  ↓
uart.write(packet)                   →  9 bytes por Serial5 al TOP
```

**La rotación 180° NO ocurre acá.** Ocurre en el TOP, en `cameras_fusion.cpp`.

## 5. Frame rate y carga

Idéntico a la cámara frontal:

| Métrica | Valor estimado |
|---|---|
| Resolución | 320×240 RGB565 (~150 KB/frame) |
| `find_blobs` por color | ~5-10 ms cada uno (3 colores = 15-30 ms total) |
| Transformación homográfica + clamp | ~1 ms |
| `uart.write` (9 bytes a 19200 baud) | ~5 ms para vaciar buffer |
| **Frame rate efectivo** | **~30 Hz** |

## 6. Rotación 180° en la fusión (el detalle más crítico de este pack)

La fusión del TOP (`cameras_fusion.cpp`) hace esto en cada packet de la
cámara trasera:

```cpp
// cameras_fusion.cpp, función cam_obs_to_robot_frame() líneas 15-32
void cam_obs_to_robot_frame(CamObservation& obs, int cam_id) {
    if (cam_id == 1) {       // cam_id == 1 → cámara trasera
        obs.x_mm = -obs.x_mm;
        obs.y_mm = -obs.y_mm;
    }
    // cam_id == 0 (frontal) → no se rota nada
}
```

| Coord en el frame de la cámara trasera | Coord en el frame del robot (post-fusión) | Interpretación |
|---|---|---|
| (0, +50) cm | (0, −50) cm | Pelota a 50 cm DETRÁS del robot |
| (+30, +50) cm | (−30, −50) cm | Pelota detrás y a la izquierda |
| (−30, +50) cm | (+30, −50) cm | Pelota detrás y a la derecha |
| (0, 0) sentinel | (0, 0) sentinel | No detectado (preservado) |

> 📌 **El sentinel se preserva trivialmente** porque `-0 = 0`. Y el parser
> del TOP detecta `is_visible(x, y) = false` **antes** de aplicar la
> rotación, así que un sentinel nunca se confunde con una pelota en el origen.

### Por qué la rotación se hace en el TOP y no en la cámara

Razones:
- La cámara trasera **no sabe** que existe el robot ni su orientación. Solo
  ve un frame y reporta coordenadas en SU frame.
- Si la rotación se hiciera en la cámara, el script de la cámara trasera
  sería diferente al de la frontal **en código** — más superficie para bugs.
- La fusión del TOP ya tiene que convertir todos los frames a un origen
  común (el robot), así que aprovecha para hacer la rotación de la trasera
  ahí mismo.

### Fusión de detecciones

Cuando AMBAS cámaras detectan el mismo objeto físico:

- **Pelota a 50 cm enfrente del robot**: la cámara frontal la ve en
  `(0, +50)`, la trasera no la ve (es ángulo muerto suyo). El TOP fusiona
  con `confidence = 80` (single camera).
- **Pelota a 50 cm detrás del robot**: la cámara trasera la ve en
  `(0, +50)` (su frame), el TOP la rota a `(0, −50)` (frame del robot).
  La cámara frontal no la ve (ángulo muerto suyo). `confidence = 80`.
- **Pelota directamente a la izquierda del robot**: dependiendo del FOV de
  ambas, puede que ambas la vean parcialmente. Si las 2 reportan
  coordenadas coherentes después de la rotación, `confidence = 95`
  (consenso).

## 7. LEDs de diagnóstico

| LED | Encendido si |
|---|---|
| Rojo (LED 1) | Hay al menos 1 blob naranja detectado (pelota) |
| Verde (LED 2) | Hay al menos 1 blob amarillo detectado (arco amarillo) |
| Azul (LED 3) | Hay al menos 1 blob azul detectado (arco azul) |

## 8. Decisiones de diseño específicas a la cámara trasera

| Decisión | Justificación |
|---|---|
| Rotación 180° en el TOP, no en la cámara | Script de la trasera idéntico al de la frontal → menos código duplicado |
| Mismo protocolo de 9 bytes | El TOP procesa ambas con el mismo parser, solo difiere en qué Serial drena |
| Mismos thresholds LAB iniciales | Mismo módulo OpenMV con misma sensibilidad. Ajustar SOLO si la iluminación recibida difiere |
| H_MATRIX distinta a la frontal | Posición y ángulo de montaje distintos → calibración independiente obligatoria |

## 9. Limitaciones conocidas

Las mismas que la cámara frontal (mismo script base) + una específica:

| # | Limitación | Impacto |
|---|---|---|
| 1 | Bug P0: sentinel `Y_coded=100` causa pelota fantasma en origen | Robot persigue pelota inexistente |
| 2 | Bug P0: crash bytearray con coordenadas negativas | Cámara se detiene en partido |
| 3 | Bug P0: auto-WB / auto-gain encendidos | Thresholds LAB no funcionan si cambia la luz |
| 4 | Homografía placeholder (NO calibrada) | Distancias/ángulos mal |
| 5 | Sin checksum/CRC en el protocolo | Bytes coincidentes con headers causan desincronización |
| 6 | `pixels_threshold=7` para pelota | Ruido detectado como pelota |
| 7 | Confianza fija (80) en la fusión | No diferencia detección grande vs pequeña |
| **8 (específico)** | **Si HMIRROR/VFLIP no se ajustan al montaje trasero, las coordenadas son incoherentes y la rotación 180° del TOP no las arregla** | Pelota reportada en lugar opuesto al real |

## 10. Referencias

- Contrato byte-a-byte: [`03-protocolo-comunicacion.md`](03-protocolo-comunicacion.md)
- Calibración LAB + homografía: [`04-calibracion-lab-y-homografia.md`](04-calibracion-lab-y-homografia.md)
- Script actual (con bugs P0): [`firmware/openmv/current-generic.py`](firmware/openmv/current-generic.py)
- Template objetivo: [`firmware/openmv/target-cam-trasera-template.py`](firmware/openmv/target-cam-trasera-template.py)
- Parser del lado Teensy: [`firmware/teensy/cameras.{h,cpp}`](firmware/teensy/cameras.h)
- **Fusión front+back con rotación 180°**: [`firmware/teensy/cameras_fusion.cpp`](firmware/teensy/cameras_fusion.cpp) líneas 25–29.
- Pack hermano (frontal): `../cameraFront-pack/`
- Skill de calibración: `.claude/skills/openmv-vision-tuning/SKILL.md` en el repo.
