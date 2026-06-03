---
title: "04 — Backlog de optimización de visión (priorizado, con métricas)"
date: 2026-06-03
---

# 04 — Qué optimizar, en qué orden, y cómo se mide

> Leyenda: **[CÓDIGO]** lo puede hacer el agente sin banco · **[BANCO]** necesita la
> cámara + luz real (humano valida) · **[MIXTO]** el agente prepara, el banco confirma.
> Cada ítem: qué tocar → cómo se mide → criterio de aceptación.

## P0 — Que VEA la pelota (bloqueante #1 de todo el robot, TASK-022)

1. **[BANCO] Calibrar LAB + exposición fija** (naranja/amarillo/azul) con la cámara
   montada y luz real. → *Medida:* con `BRING_UP=False`, las 3 detecciones salen
   estables al mover el objeto; el recuadro rodea solo el objeto. → *Criterio:* TASK-022.
2. **[CÓDIGO] Higiene del script de producción** (antes del banco, para que la
   calibración rinda): sentinel no-blob → `Y_coded=0`; clamp `[0,255]` antes de
   `bytearray`; `auto_whitebal(False)`+`auto_gain(False)`+exposición fija; subir
   `NARANJA_PIXELS_MIN` (7→20–50). → *Medida:* compila/corre sin crash; no manda
   fantasmas con la pelota tapada. → *Criterio:* no quedan los bugs P0 del pack de cámara.

## P1 — Robustez de detección

3. **[CÓDIGO] Robustez del threshold a iluminación**: exposición/WB fijos, y
   estructura para recalibrar en Incheon cambiando solo 3 tuples. → *Medida:* el
   color no se rompe al variar la luz ±X con exposición fija. → *Criterio:* recalibración < 5 min.
4. **[CÓDIGO] Menos false positives del fondo**: filtrar blobs por área/merge,
   descartar el fondo verde/pared. → *Medida:* con la pelota fuera de cuadro, 0
   detecciones espurias. → *Criterio:* recuadro solo sobre objetos reales.
5. **[CÓDIGO] Frame rate estable** (~30 Hz QVGA): perfilar el loop, evitar trabajo
   por frame innecesario. → *Medida:* Hz reportado estable. → *Criterio:* ≥25 Hz sostenido.
6. **[MIXTO] Homografía Y≈distancia**: `H_MATRIX` + `CAM_HEIGHT_CM` por cámara +
   calibrar `CAMERA_UNIT_TO_MM` del TOP (`cameras_runtime.cpp`, hoy placeholder=10.0).
   → *Medida:* pelota a 30/50/80/100 cm reales. → *Criterio:* <10% error.

## P1 — Lado TOP (host-testeable, TDD)

7. **[CÓDIGO] Robustez de `cameras_fusion`/`ball_velocity`**: casos borde de la
   fusión front+back, expiración por tiempo, clamp int16. → *Medida:* tests host
   nuevos (RED→GREEN). → *Criterio:* `run-host-tests.sh` verde, cobertura ampliada.
8. **[MIXTO] Signo del eje X de la cámara** (TASK-202): verificar en banco; si está
   invertido, negar el signo en el borde. → *Criterio:* pelota a la derecha → `ball_x>0`.

## P2 — Calidad / a futuro

9. **[CÓDIGO] Kit `calib-lab-n6.py` a prueba de banco**: tuple con margen, # blobs +
   área en consola, recuadro en vivo, sin cuelgues (API OpenMV estándar — confirmar en N6).
10. **[BANCO] HMIRROR/VFLIP por montaje** de cada cámara (pueden diferir).
11. **[CÓDIGO/futuro] Migrar a `csi`** (hoy `sensor`, deprecado pero funciona en 4.8.1) — post-Incheon.

## Cómo entregar cada mejora

- Si es **código de cámara**: editar `cam-frontal-n6.py`/`cam-trasera-n6.py` (o
  `calib-lab-n6.py`) **en su lugar canónico**, probar variantes en `experiments/`.
- Si es **lógica del TOP**: TDD (test host primero) sobre `cameras_fusion`/`ball_velocity`.
- **Gate** antes de pushear: `top_robot1`/`top_robot2` compilan + `run-host-tests.sh` verde.
- Marcar lo que quedó para **banco** con flag claro. **No cerrar TASK-022** (lo cierra el humano).
