---
title: "Cámara TRASERA — Calibración LAB + Homografía"
date: 2026-05-24
status: vigente
parte-de: cameraBack-pack
basado-en: skill `openmv-vision-tuning` + docs/firmware/CONTRATO-DATOS-CAMARAS.md §8
---

# Cámara TRASERA — Calibración LAB + Homografía

> Este doc cubre el workflow específico para calibrar la cámara trasera. El
> workflow es **idéntico** al de la cámara frontal (mismo procedimiento,
> mismas herramientas) excepto que los valores resultantes (H_MATRIX,
> HMIRROR/VFLIP, posiblemente EXPOSURE_US y thresholds LAB) son **distintos
> y específicos al montaje trasero**. Ver también
> `../cameraFront-pack/04-calibracion-lab-y-homografia.md`.

## 1. Setup previo (una vez por temporada)

1. Instalar **OpenMV IDE** (oficial, descarga desde openmv.io).
2. Conectar la cámara trasera por USB al notebook.
3. Confirmar que el IDE detecta la cámara y se puede subir scripts.
4. Tener un setup de iluminación lo más parecido posible a la cancha de
   competencia (intensidad, temperatura de color).

> 📌 Si calibrás las 2 cámaras en la misma sesión, conectar primero la
> frontal, calibrar, desconectar, conectar la trasera, calibrar. El IDE
> recuerda el último script y puede confundir cuál cámara estás programando.

## 2. Calibración de thresholds LAB

### 2.1 ¿Misma calibración que la frontal o distinta?

**Caso típico**: mismos thresholds LAB que la frontal. Las 2 cámaras son del
mismo modelo, con el mismo sensor, y los colores físicos son los mismos
(pelota naranja, arcos amarillo/azul son fijos en el field).

**Caso a investigar**: si la cámara trasera está en sombra del chasis del
robot (luz cenital del techo bloqueada parcialmente por la estructura),
**puede recibir menos luz** → sus valores LAB son ligeramente distintos.
En ese caso, recalibrar:

- Bajar el L_min un poco (objetos se ven más oscuros).
- O subir el `pixels_threshold` para que blobs ruidosos no se cuelen.

### 2.2 Procedimiento (idéntico al frontal)

**Paso 1 — Calibrar exposure fijo**:

```python
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.set_auto_whitebal(False)
sensor.set_auto_gain(False)
sensor.set_auto_exposure(True)   # PRIMERO automático para ver qué valor elige
sensor.skip_frames(time=2000)
print("Exposure:", sensor.get_exposure_us())  # ej: ~37000 µs, puede diferir de la frontal
```

**Paso 2 — Threshold Editor del OpenMV IDE** para cada color (igual que con
la frontal).

**Paso 3 — Subir `pixels_threshold`** de pelota a 20-50.

## 3. Calibración de la homografía (específico al montaje trasero)

> **➡️ Procedimiento vigente (reemplaza el método de "4 puntos a ojo"):**
> [`docs/firmware/CALIBRACION-HOMOGRAFIA-XY-N6.md`](../../../docs/firmware/CALIBRACION-HOMOGRAFIA-XY-N6.md).
> Usa una **lona con grilla de puntos** que la cámara detecta sola
> (`firmware/openmv/calib-homografia-n6.py`) + un **solver numpy en la PC**
> (`vision-optimization-pack/tools/solve_homografia.py`, validado). La trasera
> calibra con la lona DETRÁS del robot; su H es distinta a la frontal. Lo de abajo
> queda como contexto conceptual.

### 3.1 Por qué la homografía es DISTINTA a la frontal

La matriz `H` depende de:
- **Posición física** de la cámara en el robot (la trasera está atrás,
  típicamente espejada respecto a la frontal).
- **Altura** sobre el suelo (puede ser igual a la frontal si el deck
  superior es plano, pero hay que medir).
- **Ángulo de inclinación** (puede diferir).
- **Orientación del sensor** (si HMIRROR/VFLIP difieren del frontal, el
  sistema de coordenadas pixel cambia).

**Resultado**: la H_MATRIX de la cámara trasera **NO es la misma** que la
de la frontal. Hay que calibrarla independientemente.

### 3.2 Procedimiento (método tablero de ajedrez / 4 puntos conocidos)

1. **Montar la cámara trasera en su posición definitiva** en el robot.
2. **Marcar 4 puntos conocidos en el suelo** DETRÁS del robot (donde la
   cámara trasera los ve), formando un cuadrado de dimensiones medibles
   (ej: 50 cm × 50 cm).
3. **IMPORTANTE — anotar las coordenadas en el frame DE LA CÁMARA TRASERA**,
   NO en el frame del robot. La cámara reporta lo que ve enfrente de ella
   (que físicamente está atrás del robot, pero la cámara no lo sabe). Por
   ejemplo, un punto a 50 cm atrás del robot y centrado se anota como
   `(x = 0, y = +50)` para la cámara trasera, NO `(x = 0, y = −50)`.
4. **Tomar una foto** con el OpenMV IDE.
5. **Anotar las coordenadas en píxeles** `(u, v)` de cada punto en la imagen.
6. **Calcular H** con OpenCV en Python (igual que para la frontal).
7. Pegar la matriz resultante en el script `cam_trasera.py`:

```python
H_MATRIX = [
    [h00, h01, h02],
    [h10, h11, h12],
    [h20, h21, h22]
]
```

### 3.3 Validación de la homografía

Validar con pelotas a distancias conocidas **DETRÁS** del robot:

| Posición pelota (frame del robot) | (X, Y) esperados en frame del robot | (X, Y) reportados por la cámara | (X, Y) post-rotación 180° en TOP | Error |
|---|---|---|---|---|
| 30 cm DETRÁS, centrada | (0, −30) | (0, +30) | (0, −30) | ? |
| 50 cm DETRÁS | (0, −50) | (0, +50) | (0, −50) | ? |
| 80 cm DETRÁS | (0, −80) | (0, +80) | (0, −80) | ? |
| 50 cm DETRÁS, 30 cm a la izquierda del robot | (−30, −50) | (+30, +50) | (−30, −50) | ? |

> 📌 Lo crítico es que la última columna (post-rotación 180°) **coincida con
> la pelota física real desde la perspectiva del robot**. Si la pelota está
> realmente a 30 cm detrás y la cámara reporta algo distinto a (0, −30) en
> el frame del robot, hay error en la H_MATRIX.

**Criterio de aceptación (TASK-022)**: error < 10% del valor esperado.

## 4. HMIRROR / VFLIP — verificar al montar (DIFERENCIA CLAVE vs frontal)

| Flag | Efecto |
|---|---|
| `sensor.set_hmirror(True)` | Invierte eje horizontal de la imagen del sensor |
| `sensor.set_vflip(True)` | Invierte eje vertical |

**Probable diferencia con la frontal**: si las 2 cámaras están montadas en
posiciones "espejadas" (cable de la frontal hacia +X, cable de la trasera
hacia −X), HMIRROR de la trasera debe ser inverso al de la frontal.

**Procedimiento**:

1. Montar la cámara trasera en su posición definitiva.
2. Conectar al OpenMV IDE.
3. Mostrarle una imagen de referencia con una flecha pintada en el suelo que
   apunta hacia +Y del robot (adelante).
4. Mirar el preview en el IDE: **como la cámara trasera mira hacia atrás del
   robot**, la flecha (que apunta hacia adelante del robot) debe aparecer
   apuntando hacia **abajo** en el preview (porque está "atrás" desde la
   perspectiva de la cámara trasera).
5. Si está bien: HMIRROR/VFLIP están correctos.
6. Si la flecha apunta hacia arriba (en lugar de abajo) → toggle `VFLIP`.
7. Si la flecha está al revés en el lado lateral → toggle `HMIRROR`.

**Valores tentativos** (los mismos del script genérico actual):
`HMIRROR = True`, `VFLIP = True`. Pero pueden ser distintos para la trasera.
**Probarlos primero antes de asumirlos.**

## 5. Checklist completo de calibración cámara TRASERA (TASK-022)

- [ ] **Setup OpenMV IDE** conectado a la cámara trasera.
- [ ] **Exposure fijo** medido en cancha de competencia (`EXPOSURE_US = ?` — puede diferir del frontal).
- [ ] **Autos apagados** en el script.
- [ ] **`NARANJA_THRESHOLD`** calibrado (puede ser igual al frontal o distinto si la iluminación trasera difiere).
- [ ] **`AMARILLO_THRESHOLD`** calibrado.
- [ ] **`AZUL_THRESHOLD`** calibrado.
- [ ] **`pixels_threshold`** de pelota ≥ 20.
- [ ] **HMIRROR / VFLIP** verificados con preview del IDE.
- [ ] **H_MATRIX** calibrada con 4 puntos conocidos DETRÁS del robot.
- [ ] **Validación de homografía** con pelotas a 30/50/80/100 cm DETRÁS del robot — error < 10%.
- [ ] **Verificación de la rotación 180°**: probar pelota a 50 cm DETRÁS del robot, confirmar que el TOP reporta `ball_y_mm ≈ -500` (post-rotación), NO `+500`.
- [ ] **Sentinel corregido**: cuando no hay blob, retornar `(0, 0)` → `Y_coded = 0`.
- [ ] **Clamp anti-crash**: todos los valores `[0, 255]` antes de `bytearray()`.
- [ ] **Archivo renombrado** a `cam_trasera.py` (no `current-generic.py`).

## 6. Test específico de la rotación 180° (no se hace en la cámara)

Esta verificación no se hace en la OpenMV sino en el TOP, con la cámara
trasera ya calibrada. Pasos:

1. Compilar y flashear el TOP (`pio run -e top -t upload`).
2. Conectar el robot al USB del Teensy 4.0 del TOP.
3. Abrir el Serial Monitor → mandar comando `dump_cameras` (cuando esté
   implementado).
4. Poner una pelota a **50 cm DETRÁS** del robot, centrada.
5. Confirmar en el output:
   - **Cámara trasera reporta** (frame de la cámara): `ball_x ≈ 0, ball_y ≈ +50` (en cm).
   - **TOP fusionado reporta** (frame del robot): `ball_x_mm ≈ 0, ball_y_mm ≈ -500` (en mm, signo Y negativo porque está detrás).
6. Si el TOP reporta `+500` en vez de `−500`, hay error en `cameras_fusion.cpp` o en el `cam_id` asignado a la trasera.

## 7. Plan B — si la calibración no funciona

Mismas opciones que para la frontal (ver `../cameraFront-pack/04-calibracion-lab-y-homografia.md` §6).

## 8. Referencias

- Workflow general: skill `openmv-vision-tuning` (`.claude/skills/openmv-vision-tuning/SKILL.md`).
- Contrato byte-a-byte: [`03-protocolo-comunicacion.md`](03-protocolo-comunicacion.md).
- Pack hermano frontal: [`../cameraFront-pack/04-calibracion-lab-y-homografia.md`](../cameraFront-pack/04-calibracion-lab-y-homografia.md).
- TASK-022 (cámara operativa): `team-tasks/2026-05-18-task-022-camara-operativa.md`.
- Lógica de la rotación 180° (esto SÍ es responsabilidad del TOP, no de esta cámara): [`firmware/teensy/cameras_fusion.cpp`](firmware/teensy/cameras_fusion.cpp) líneas 25-29.
