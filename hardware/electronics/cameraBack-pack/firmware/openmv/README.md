# Firmware OpenMV — Cámara TRASERA

## Estado del firmware

⚠️ **Hoy hay UN solo script genérico** (`current-generic.py`) que se carga en
**ambas cámaras** del robot. Esto es un problema porque:

- Cada cámara está montada en posición distinta → requiere HMIRROR/VFLIP
  potencialmente distintos.
- Cada cámara tiene FOV y altura distintos → requiere su propia matriz de
  homografía (H_MATRIX).
- Cada cámara puede recibir iluminación distinta → puede requerir exposure
  distinto.

**El objetivo (TASK-022)** es separar el script genérico en 2 scripts
específicos:

- **`cam_frontal.py`** — corre en la cámara frontal (ver `cameraFront-pack/`).
- **`cam_trasera.py`** — corre en la cámara trasera de este robot.

## Diferencia clave: la trasera NO rota sus coordenadas

La cámara trasera **NO** debe aplicar la rotación 180° en su propio código.
La rotación la hace el TOP en `cameras_fusion.cpp` líneas 25-29. La cámara
reporta lo que ve en SU PROPIO frame, como si fuera la única cámara del mundo.

Si la cámara trasera rotara sus coordenadas internamente, el TOP las
rotaría de nuevo y quedarían al revés. **Mantener el script igual al de la
frontal en términos de salida** — solo difieren en parámetros (H_MATRIX,
HMIRROR/VFLIP, posiblemente EXPOSURE_US).

## Archivos en esta carpeta

### `current-generic.py` (snapshot del actual, con bugs P0)

Idéntico al de la cámara frontal — es UN solo script para ambas cámaras hoy.
Tiene los mismos 3 bugs P0:

| Bug | Líneas | Síntoma |
|---|---|---|
| **Sentinel roto** | 81: `return 0, 0` → `codedYp = 0+100 = 100` | El parser TOP interpreta `(0, 0)` como pelota visible en el origen |
| **Crash bytearray** | 155: `bytearray(packet)` sin clamp previo | Si la homografía da X<0 o >255, `bytearray()` lanza `ValueError` |
| **Auto-WB / auto-gain** | 31-32: `set_auto_whitebal(True)`, `set_auto_gain(True)` | Thresholds LAB no funcionan si cambia la luz |

**No usar en producción.** Solo dejado como referencia.

### `target-cam-trasera-template.py` (objetivo a flashear, con bugs corregidos)

Template del script que **debería** correr en la cámara trasera cuando
TASK-022 esté cerrada. Tiene:

- ✅ Sentinel correcto: `(X=0, Y_coded=0)` cuando no hay blob.
- ✅ Clamp anti-crash: `max(0, min(255, int(v)))` en cada coordenada.
- ✅ Autos apagados.
- ✅ Pixels threshold de pelota subido a 20.
- ✅ Marcado `CAM_ID = 1` (back) para distinguir de la frontal.
- ⚠️ Placeholders en H_MATRIX, EXPOSURE_US, HMIRROR/VFLIP, thresholds LAB —
  **hay que calibrar específicamente para la trasera** (ver
  `../04-calibracion-lab-y-homografia.md`).

## Cómo flashear

Igual que la frontal:

1. Abrir **OpenMV IDE**.
2. Conectar **la cámara trasera** por USB (si tenés ambas conectadas
   simultáneamente, el IDE puede confundirse — desconectar la frontal antes).
3. Abrir `target-cam-trasera-template.py` (tras calibrar sus valores).
4. **File → Save Open Script to OpenMV Cam As "main.py"**.
5. La cámara reinicia y empieza a correr.

> ⚠️ **No flashear el script `cam_trasera.py` a la cámara frontal por error**.
> Si las 2 cámaras son físicamente idénticas, mantenerlas etiquetadas con
> cinta + sticker para no confundirlas durante calibración.

## Cómo verificar que está corriendo + rotación 180°

1. Conectar la cámara trasera por USB.
2. Abrir el OpenMV IDE → conectar.
3. Ver el "frame buffer": imagen QVGA.
4. Ver el "serial terminal": packets de 9 bytes saliendo (`Enviando: [...]`).
5. Conectar la cámara al PCB TOP en el conector U9.
6. Verificar rotación 180° (ver checklist en `../04-calibracion-lab-y-homografia.md` §6):
   - Pelota a 50 cm DETRÁS del robot, centrada.
   - Cámara trasera reporta: `ball_x ≈ 0, ball_y ≈ +50` (su frame).
   - TOP fusionado reporta: `ball_x_mm ≈ 0, ball_y_mm ≈ -500` (frame robot).
