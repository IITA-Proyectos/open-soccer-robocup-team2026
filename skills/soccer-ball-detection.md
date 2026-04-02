# 🎯 Detección de Pelota y Arcos — RoboCupJunior Soccer Open

---

## 1. PELOTA: GOLF BALL NARANJA (42mm)

Soccer Open usa una pelota pasiva naranja (no IR). La detección es por cámara.

### OpenMV: detección por color LAB

```python
import sensor, image, math, time
from robust_comm import RobustComm  # Ver skill RobustComm

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)   # 320x240
sensor.set_auto_gain(False)
sensor.set_auto_whitebal(False)
sensor.set_auto_exposure(False, exposure_us=5000)  # Calibrar en venue
sensor.skip_frames(time=2000)

# Thresholds LAB (CALIBRAR EN EL VENUE con Tools → Threshold Editor)
BALL_ORANGE = (35, 75, 30, 80, 30, 80)  # Calibrar!
GOAL_CYAN   = (40, 80, -60, -15, -40, 0)
GOAL_MAGENTA = (25, 60, 20, 65, -30, 15)

comm = RobustComm(uart_id=3, baud=230400)
clock = time.clock()

while True:
    clock.tick()
    img = sensor.snapshot()

    # --- Pelota ---
    ball_blobs = img.find_blobs([BALL_ORANGE], pixels_threshold=20,
                                area_threshold=20, merge=True)
    best_ball = None
    if ball_blobs:
        # Filtrar por circularidad (golf ball es muy redonda)
        round_blobs = [b for b in ball_blobs if b.roundness() > 0.6]
        if round_blobs:
            best_ball = max(round_blobs, key=lambda b: b.pixels())

    if best_ball:
        b = best_ball
        angle = int((b.cx() - 160) * 47.5 / 160)  # FOV ~47.5° H7
        # Distancia por tamaño: golf ball 42mm
        # Empírico: a 100mm → ~80px, a 300mm → ~27px, a 600mm → ~13px
        distance = int(3400 / max(math.sqrt(b.pixels()), 1))
        confidence = int(min(100, b.density() * b.roundness() * 200))
        comm.send_ball(angle, distance, confidence)
    else:
        comm.send_no_detection()

    # --- Arcos (enviar si visibles) ---
    # Se puede enviar en frame alternos para no sobrecargar UART
    # O usar un msg_id diferente

    if comm.update():  # Leer comandos del controller
        if comm.last_msg_id == 0x10:  # SET_MODE
            pass  # Cambiar configuración
```

---

## 2. ARCOS COLOREADOS

Los arcos son **cyan** y **magenta** (cambian de lado en el halftime):

```python
def detect_goals(img):
    result = {'cyan': None, 'magenta': None}

    for color, name in [(GOAL_CYAN, 'cyan'), (GOAL_MAGENTA, 'magenta')]:
        blobs = img.find_blobs([color], pixels_threshold=80,
                               area_threshold=80, merge=True)
        if blobs:
            best = max(blobs, key=lambda b: b.pixels())
            result[name] = {
                'angle': int((best.cx() - 160) * 47.5 / 160),
                'width': best.w(),
                'pixels': best.pixels()
            }
    return result
```

---

## 3. CALIBRACIÓN EN VENUE

SIEMPRE calibrar en el venue real. La iluminación cambia todo.

```
1. Abrir OpenMV IDE
2. Tools → Machine Vision → Threshold Editor
3. Apuntar cámara a la pelota naranja en la cancha real
4. Ajustar L, A, B hasta que solo la pelota quede en blanco
5. Copiar valores a BALL_ORANGE
6. Repetir para GOAL_CYAN y GOAL_MAGENTA
7. Probar con distintas posiciones y ángulos
```

---

## 4. LÍNEAS BLANCAS (DETECCIÓN DE BORDE)

Para no salir del campo, detectar líneas blancas con sensores IR en la base del robot (estándar en RoboCup Junior) o con cámara:

```python
# Detección de línea blanca con cámara (parte inferior de la imagen)
def check_white_line(img):
    # Analizar solo las últimas 30 filas de la imagen (piso cercano)
    roi = (0, img.height() - 30, img.width(), 30)
    stats = img.get_statistics(roi=roi)
    # Si el brillo promedio es alto → línea blanca
    return stats.l_mean() > 70  # Calibrar threshold
```

Pero lo más común es usar **sensores IR de línea** en la base del robot (KY-033, QRE1113, etc.).

---

## FUENTES

- RoboCupJunior Soccer Rules: golf ball naranja, arcos cyan/magenta
- RoBorregos OpenMV code (2024)
- OpenMV IDE Threshold Editor
- IITA legacy 2025 vision code
