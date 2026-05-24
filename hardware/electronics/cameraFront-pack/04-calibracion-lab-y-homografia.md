---
title: "Cámara FRONTAL — Calibración LAB + Homografía"
date: 2026-05-24
status: vigente
parte-de: cameraFront-pack
basado-en: skill `openmv-vision-tuning` + docs/firmware/CONTRATO-DATOS-CAMARAS.md §8
---

# Cámara FRONTAL — Calibración LAB + Homografía

> Este doc cubre el workflow específico para calibrar la cámara frontal. Para
> la calibración de la cámara trasera, ver `cameraBack-pack/04-calibracion-lab-y-homografia.md`
> (similar pero con diferencias en HMIRROR/VFLIP y H_MATRIX).
>
> El workflow general (sin diferencias front/back) está en la skill
> `openmv-vision-tuning` del repo. Acá se reduce a los pasos accionables.

## 1. Setup previo (una vez por temporada)

1. Instalar **OpenMV IDE** (oficial, descarga desde openmv.io).
2. Conectar la cámara frontal por USB al notebook.
3. Confirmar que el IDE detecta la cámara y se puede subir scripts.
4. Tener un setup de iluminación lo más parecido posible a la cancha de
   competencia (intensidad, temperatura de color).

## 2. Calibración de thresholds LAB — los 3 colores

### 2.1 Por qué hay que recalibrar para Incheon

Los thresholds LAB del script actual fueron calibrados con la iluminación de
**la cancha del IITA en Salta**. La iluminación de Incheon es distinta
(diferente intensidad, diferente temperatura de color, posibles reflejos
distintos del piso). Los mismos colores físicos (pelota naranja, arcos
amarillo/azul) **producen valores LAB distintos** bajo iluminación distinta.

**Sin recalibrar**, los blobs aparecerán o desaparecerán impredeciblemente,
y la cámara reportará pelota fantasma o no la ve cuando está enfrente.

### 2.2 Procedimiento

**Paso 1 — Calibrar exposure fijo (CRÍTICO)**:

```python
# En el OpenMV IDE, probar este script standalone:
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.set_auto_whitebal(False)
sensor.set_auto_gain(False)
sensor.set_auto_exposure(True)   # PRIMERO automático para ver qué valor elige
sensor.skip_frames(time=2000)
print("Exposure:", sensor.get_exposure_us())  # ej: ~37000 µs
```

Anotar el valor reportado. Si la imagen se ve bien con auto, ese valor sirve
como base. Luego fijar:

```python
sensor.set_auto_exposure(False, exposure_us=37000)   # o el valor leído
```

**Paso 2 — Calibrar threshold LAB de cada color**:

1. Apuntar la cámara frontal hacia la pelota naranja (sin obstáculos).
2. En el OpenMV IDE → menú **Tools → Machine Vision → Threshold Editor**.
3. Ajustar los 6 sliders (L, A, B mín/máx) hasta que la pelota se vea como
   blob blanco sólido en el preview de la derecha y el fondo aparezca negro.
4. Copiar los 6 valores como tupla:
   ```python
   NARANJA_THRESHOLD = (L_min, L_max, A_min, A_max, B_min, B_max)
   ```
5. Repetir para arco amarillo y arco azul.

**Paso 3 — Verificar `pixels_threshold`**:

- Pelota: subir de 7 (actual, demasiado bajo) a **20-50**.
- Arcos: dejar 300-600 (depende del tamaño del arco visto desde la posición).

### 2.3 Tip de recalibración rápida en Incheon

Llevar un script "calibrator" que muestre los valores LAB del píxel central
en tiempo real:

```python
while True:
    img = sensor.snapshot()
    cx, cy = 160, 120
    pixel_lab = img.get_pixel(cx, cy, rgbtuple=True)
    img.draw_cross(cx, cy, color=(255,255,255))
    img.draw_string(0, 0, "LAB: %d %d %d" % image.rgb_to_lab(pixel_lab),
                    color=(255,255,255))
```

Apuntar la cámara a la pelota, ver el LAB en pantalla, ajustar `NARANJA_THRESHOLD`
con margen ±10 alrededor.

## 3. Calibración de la homografía (específico al montaje frontal)

### 3.1 Qué es la homografía

La cámara frontal ve una imagen 2D pero la pelota está en el suelo 3D.
La **matriz H 3×3** transforma de píxeles `(u, v)` de la imagen a coordenadas
físicas `(x, y)` en cm sobre el plano del suelo, tomando en cuenta la
perspectiva.

`H` depende de:
- Posición y altura de la cámara en el robot.
- Ángulo de inclinación de la cámara.
- Lente (FOV) del módulo OpenMV.

**Cada cámara necesita SU PROPIA H** porque están montadas en posiciones
distintas. La cámara frontal y la trasera tienen H diferentes.

### 3.2 Procedimiento (método tablero de ajedrez / 4 puntos conocidos)

1. **Montar la cámara frontal en su posición definitiva** en el robot.
2. **Marcar 4 puntos conocidos** en el suelo, formando un cuadrado de
   dimensiones medibles (ej: 50 cm × 50 cm). Por ejemplo, las 4 esquinas de
   un cuadrado dibujado con cinta.
3. Anotar las **coordenadas físicas reales** de cada punto en el sistema de
   la cámara (origen = proyección de la cámara al suelo):
   - P1: (x1, y1) = (−25, 25) cm — esquina superior-izquierda del cuadrado
   - P2: (x2, y2) = (+25, 25) cm — esquina superior-derecha
   - P3: (x3, y3) = (+25, 75) cm — esquina inferior-derecha (más lejos)
   - P4: (x4, y4) = (−25, 75) cm — esquina inferior-izquierda
4. **Tomar una foto** con el OpenMV IDE de los 4 puntos.
5. **Anotar las coordenadas en píxeles** `(u, v)` de cada punto en la imagen.
6. Calcular la matriz H con OpenCV en Python (ejemplo en la skill
   `openmv-vision-tuning`).
7. Pegar la matriz resultante en el script:

```python
H_MATRIX = [
    [h00, h01, h02],
    [h10, h11, h12],
    [h20, h21, h22]
]
```

### 3.3 Validación de la homografía (CRÍTICO)

Después de calibrar, validar con pelotas a distancias **conocidas**:

| Posición pelota | (X, Y) esperados | (X, Y) reportados por script | Error |
|---|---|---|---|
| 30 cm enfrente | (0, 30) | ? | ? |
| 50 cm enfrente | (0, 50) | ? | ? |
| 80 cm enfrente | (0, 80) | ? | ? |
| 100 cm enfrente | (0, 100) | ? | ? |
| 50 cm enfrente, 30 cm derecha | (+30, 50) | ? | ? |
| 50 cm enfrente, 30 cm izquierda | (−30, 50) | ? | ? |

**Criterio de aceptación (TASK-022)**: error < 10% del valor esperado. Si es
mayor, recalibrar H_MATRIX.

### 3.4 Calibrar `CAMERA_UNIT_TO_MM` del TOP (consecuencia de la calibración)

El TOP convierte las unidades reportadas por la cámara a milímetros con:

```cpp
// firmware/teensy/cameras_runtime.cpp:25
constexpr float CAMERA_UNIT_TO_MM = 10.0f;   // placeholder
```

Después de calibrar H, este factor debe coincidir con la unidad real que
manda la cámara:

- Si H reporta en cm → `CAMERA_UNIT_TO_MM = 10.0` (1 cm = 10 mm). ✅ valor actual.
- Si H reporta en mm → `CAMERA_UNIT_TO_MM = 1.0`.
- Si H reporta en pulgadas (no debería) → `CAMERA_UNIT_TO_MM = 25.4`.

## 4. HMIRROR / VFLIP — verificar al montar

Estos 2 flags **invierten la imagen** según cómo esté físicamente montada la
cámara:

| Flag | Efecto | Cuándo se necesita |
|---|---|---|
| `sensor.set_hmirror(True)` | Invierte eje horizontal (espejo izquierda-derecha) | Si la cámara está montada con el cable hacia arriba o si la lente está rotada 180° respecto al chasis |
| `sensor.set_vflip(True)` | Invierte eje vertical | Si la cámara está montada al revés (cabeza abajo) |

**Procedimiento de calibración**:

1. Montar la cámara frontal en su posición definitiva.
2. Conectar al OpenMV IDE.
3. Mostrarle una imagen de referencia con una flecha que apunta hacia +Y del
   robot (adelante).
4. Mirar el preview en el IDE: si la flecha aparece bien orientada
   (apuntando hacia arriba en pantalla, espejada respecto al lado lateral),
   los flags están bien.
5. Si la flecha está invertida horizontalmente → toggle `HMIRROR`.
6. Si la flecha está al revés verticalmente → toggle `VFLIP`.

Para la cámara frontal del robot IITA, **los valores actuales son
`HMIRROR=True` y `VFLIP=True`** (`current-generic.py:42-43`). Si el montaje
no se cambió, dejar así. Si se cambió el montaje, **recalibrar**.

## 5. Checklist completo de calibración cámara FRONTAL (TASK-022)

- [ ] **Setup OpenMV IDE** conectado a la cámara frontal.
- [ ] **Exposure fijo** medido en cancha de competencia (`EXPOSURE_US = ?`).
- [ ] **Autos apagados** (`set_auto_whitebal(False)`, `set_auto_gain(False)`, `set_auto_exposure(False, exposure_us=EXPOSURE_US)`).
- [ ] **`NARANJA_THRESHOLD`** calibrado para pelota naranja bajo iluminación de competencia.
- [ ] **`AMARILLO_THRESHOLD`** calibrado para arco amarillo.
- [ ] **`AZUL_THRESHOLD`** calibrado para arco azul.
- [ ] **`pixels_threshold`** de pelota ≥ 20 (no detecta ruido).
- [ ] **HMIRROR / VFLIP** verificados según montaje físico real.
- [ ] **H_MATRIX** calibrada con 4 puntos conocidos.
- [ ] **Validación de homografía** con pelotas a 30/50/80/100 cm — error < 10%.
- [ ] **`CAMERA_UNIT_TO_MM`** del TOP coincide con las unidades del script.
- [ ] **Sentinel corregido**: cuando no hay blob, retornar `(0, 0)` → `Y_coded = 0` (no 100).
- [ ] **Clamp anti-crash**: todos los valores `[0, 255]` antes de `bytearray()`.
- [ ] **Archivo renombrado** a `cam_frontal.py` (no `current-generic.py`).

## 6. Plan B — si la calibración no funciona

Si después de seguir todos los pasos la cámara no detecta confiablemente:

1. **Verificar el sensor físico** — abrir el OpenMV IDE, ver el preview en
   tiempo real, confirmar que la imagen sale (no negra, no congelada).
2. **Probar con pelota de prueba bien iluminada** y fondo simple (mesa blanca).
3. **Mirar los valores LAB del píxel central** con el script "calibrator"
   (ver §2.3). Si los valores no se acercan al threshold, ajustar threshold.
4. **Bajar `pixels_threshold` a 1 temporalmente** para ver si la detección
   funciona con cualquier blob. Si no detecta NADA con threshold=1, hay un
   problema más profundo (LAB calibrados completamente mal, exposición
   inadecuada, sensor cubierto, etc.).
5. **Escalar a Discord RCJ / foro OpenMV** con foto de la imagen, código y
   valores LAB observados vs esperados.

## 7. Referencias

- Workflow general (no específico a front/back): skill `openmv-vision-tuning` (`.claude/skills/openmv-vision-tuning/SKILL.md`).
- Contrato byte-a-byte (incluye §8 con detalles del template objetivo): [`03-protocolo-comunicacion.md`](03-protocolo-comunicacion.md).
- TASK-022 (cámara operativa, criterio de cierre completo): `team-tasks/2026-05-18-task-022-camara-operativa.md`.
- Docs OpenMV: https://docs.openmv.io/openmvcam/tutorial/color_tracking.html
