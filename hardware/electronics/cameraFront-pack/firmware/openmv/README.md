# Firmware OpenMV — Cámara FRONTAL

## Estado del firmware

⚠️ **Hoy hay UN solo script genérico** (`current-generic.py`) que se carga en
**ambas cámaras** del robot. Esto es un problema porque:

- Cada cámara está montada en posición distinta → requiere HMIRROR/VFLIP
  potencialmente distintos.
- Cada cámara tiene FOV y altura distinto → requiere su propia matriz de
  homografía (H_MATRIX).
- Cada cámara puede recibir iluminación distinta → puede requerir exposure
  distinto.

**El objetivo (TASK-022)** es separar el script genérico en 2 scripts
específicos:

- **`cam_frontal.py`** — corre en la cámara frontal de este robot.
- **`cam_trasera.py`** — corre en la cámara trasera (ver `cameraBack-pack/`).

## Archivos en esta carpeta

### `current-generic.py` (snapshot del actual, con bugs P0)

Es la copia del script que vive en `software/vision/enviar coordenadas 2
arcos y pelota` del repo. Tiene 3 bugs P0 conocidos:

| Bug | Líneas | Síntoma |
|---|---|---|
| **Sentinel roto** | 81: `return 0, 0` → `codedYp = 0+100 = 100` | El parser TOP interpreta `(0, 0)` como pelota visible en el origen → robot persigue pelota fantasma permanente |
| **Crash bytearray** | 155: `bytearray(packet)` sin clamp previo | Si la homografía da X negativo o > 255, `bytearray()` lanza `ValueError` y la cámara se detiene en partido |
| **Auto-WB / auto-gain** | 31-32: `set_auto_whitebal(True)`, `set_auto_gain(True)` | Los thresholds LAB calibrados en Salta no funcionan en Incheon |

**No usar en producción.** Solo dejado como referencia histórica del bug.

### `target-cam-frontal-template.py` (objetivo a flashear, con bugs corregidos)

Es el template del script que **debería** correr en la cámara frontal cuando
TASK-022 esté cerrada. Tiene:

- ✅ Sentinel correcto: cuando no hay blob, envía `(X=0, Y_coded=0)` → parser TOP marca correctamente "no visible".
- ✅ Clamp anti-crash: `max(0, min(255, int(v)))` en cada coordenada antes del bytearray.
- ✅ Autos apagados: `set_auto_whitebal(False)`, `set_auto_gain(False)`, `set_auto_exposure(False, exposure_us=PLACEHOLDER)`.
- ✅ Pixels threshold de pelota subido a 20.
- ✅ Marcado `CAM_ID = 0` (front) para futura distinción.
- ⚠️ Placeholders en H_MATRIX, EXPOSURE_US, thresholds LAB — **hay que calibrar antes de flashear** (ver `../04-calibracion-lab-y-homografia.md`).

## Cómo flashear

1. Abrir **OpenMV IDE** (descarga desde openmv.io).
2. Conectar la cámara frontal por USB.
3. Verificar que el IDE detecta la cámara.
4. Abrir el `.py` deseado (típicamente `target-cam-frontal-template.py` tras
   calibrar).
5. **File → Save Open Script to OpenMV Cam As "main.py"**.
6. La cámara reinicia y empieza a correr el script automáticamente.

> El nombre `main.py` es lo que el OpenMV ejecuta al boot. Si subís el script
> con otro nombre, **NO se ejecuta solo**.

## Cómo verificar que está corriendo

1. Conectar la cámara por USB.
2. Abrir el OpenMV IDE → conectar.
3. Ver el "frame buffer" en la derecha: si hay imagen, el sensor está vivo.
4. Ver el "serial terminal" en la izquierda: si imprime `Enviando: [...]`,
   está enviando packets. (Sacar el `print()` en producción para ganar fps.)
5. Conectar la cámara al PCB TOP y verificar con el USB Serial del Teensy que
   los packets llegan (con `dump_cameras` del debug del TOP).

## Si la cámara no inicia

- Mantener apretado el botón **BOOT** mientras se enchufa el USB → entra a
  modo bootloader del DFU. Permite reflashear el firmware base del OpenMV si
  se corrompió.
- Si el IDE no detecta la cámara: instalar drivers (depende del SO).
