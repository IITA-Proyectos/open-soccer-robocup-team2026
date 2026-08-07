# 🧠 YOLO Vision Pipeline (RoboCup Junior)

## Resumen
Este skill reemplaza la tradicional detección por color (espacio LAB/HSV) por inferencia de modelos de Deep Learning ligeros (YOLO/MobileNet) que ofrecen robustez extrema ante cambios de iluminación.

## Cuándo usar este skill
- Cuando los robots pierden la pelota bajo la luz del sol o focos LED asimétricos en el campo.
- Para detección simultánea de múltiples clases: **Pelota**, **Arco Azul**, **Arco Amarillo**, **Robots Rivales**.
- Al desarrollar en cámaras **OpenMV H7+** o sistemas con coprocesadores como **Raspberry Pi 4/5**.

## Arquitectura del Pipeline

### 1. Entrenamiento del Modelo (YOLOv8 Nano / Edge Impulse)
Para hardware embebido (OpenMV H7+ o RPi), se recomienda **YOLOv8 Nano (n)** o **FOMO (Faster Objects, More Objects)**.
1. **Dataset**: Capturar al menos 1000+ imágenes de la pelota naranja (golf), los arcos (Cyan/Magenta), y paredes bajo diferentes luces. Aumentar (Augmentation) con cambios de brillo, desenfoque de movimiento y ruido.
2. **Exportación**: Convertir a TensorFlow Lite (`.tflite`) int8 quantizado para máxima velocidad sin FPU.

### 2. Implementación en OpenMV (MicroPython)
```python
import sensor, image, time, tf

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA) # 320x240
sensor.skip_frames(time = 2000)

# Cargar modelo TFLite
net = tf.load('rcj_yolo_quant.tflite', load_to_fb=True)
labels = ['ball', 'goal_blue', 'goal_yellow', 'robot']

clock = time.clock()
while(True):
    clock.tick()
    img = sensor.snapshot()
    
    # Inferencia (FOMO o YOLO muy ligero)
    for obj in net.classify(img, min_scale=1.0, scale_mul=0.8, x_overlap=0.5, y_overlap=0.5):
        # Detección
        print("Detected %s at %d, %d" % (labels[obj.classid()], obj.x(), obj.y()))
        img.draw_rectangle(obj.rect())
```

### 3. Implementación en Raspberry Pi (C++ / Python)
Si las reglas de peso lo permiten (ej. Open League), usar una Raspberry Pi con una cámara USB a >60 FPS usando NCNN o TensorRT.
* **Ventaja**: Permite inferencia y cálculo de homografía simultáneo para pasar de (x, y) en píxeles a (x, y) en milímetros respecto al robot.

## Integración con el World Model
- El resultado de la visión (coordenadas x,y del bounding box) se pasa por una transformación proyectiva (Homografía) usando la calibración intrínseca de la cámara.
- La salida en milímetros es fusionada en un Filtro de Kalman con la IMU para calcular la velocidad de la pelota `(Vx, Vy)`.
