# =============================================================================
# LEGACY - Temporada 2025 - OpenMV: Cálculo de coordenadas de pelota
# Fuente: https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025
# Archivo original: /OpenMV/calcula coordenadas pelota.py
# Autores originales: Equipo 2025 - Grupo OpenMV
# =============================================================================

import sensor, image, time
import math

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)    # 320x240
sensor.skip_frames(time=2000)
sensor.set_auto_whitebal(False)
clock = time.clock()

# Constantes físicas
h = 10   # Altura de la cámara (cm)
pi = math.pi
r = 13.5 / (pi * 2)  # Radio de la pelota (cm)

# Threshold naranja en LAB
orange_threshold = (30, 60, 20, 60, 10, 50)

def transformarcoordenadas(u, v):
    # Matriz de transformación homográfica
    H = [[-2.18880937e-03, -2.63497709e-01, -2.44260579e+02],
         [1.02845724e+00,  1.44625829e-03, -1.92956221e+02],
         [4.01073113e-04, -5.44887132e-02,  1.00000000e+00]]
    
    denominator = H[2][0]*u + H[2][1]*v + H[2][2]
    x = (H[0][0]*u + H[0][1]*v + H[0][2]) / denominator
    y = (H[1][0]*u + H[1][1]*v + H[1][2]) / denominator
    return x, y

while(True):
    clock.tick()
    img = sensor.snapshot()
    blobs = img.find_blobs([orange_threshold], pixels_threshold=100, area_threshold=100, merge=True)

    if blobs:
        largest_blob = max(blobs, key=lambda b: b.pixels())
        img.draw_rectangle(largest_blob.rect(), color=(255, 0, 0))
        img.draw_cross(largest_blob.cx(), largest_blob.cy(), color=(0, 255, 0))

        u, v = largest_blob.cx(), largest_blob.cy()
        x, y = transformarcoordenadas(u, v) 
        X = x * (h - r) / h
        Y = y * (h - r) / h
        print("Coordenadas pelota: (%.2f, %.2f)" % (X, Y))

        if x != 0:
            a_rad = math.atan2(Y, X)
            angulo = math.degrees(a_rad)
            sentido = 1 if angulo > 0 else 0
            print("angulo", abs(angulo), "sentido", sentido)
