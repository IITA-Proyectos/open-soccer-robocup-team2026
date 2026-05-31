---
id: TASK-202
title: "Verificar en HW los signos de izquierda/derecha: cámara (ball_x) + giro del BNO"
date_created: 2026-05-31
date_due: 2026-06-10
assigned: [mariaviollaz, elias]
priority: P1
status: pending
estimated_hours: 1
blocks: [localizacion-correcta, control-orientacion-correcto]
blocked_by: []
tags: [firmware, top-board, central, convencion, ejes, camara, bno, hardware-test]
---

# TASK-202 — Verificar signos de izquierda/derecha en hardware

## Por qué

Se fijó la convención canónica de ejes (`docs/CONVENCION-EJES-ROBOT.md`):
**primera persona, +X = derecha del robot, +Y = frente**. El código de
movimiento (kinematics) y táctica (behind_ball) ya la cumplen. Pero hay 2
signos que **solo se pueden confirmar con el robot en la mano**, y si están
invertidos el robot se mueve/gira para el lado contrario (gol en contra). El
firmware compila igual con el signo equivocado — por eso hay que medirlo.

## Qué verificar

### A — Signo del eje X de la cámara (`ball_x`)
1. Robot encendido, cámara andando, debug que imprima `ball_x` (o el
   WorldSnapshot que llega al CENTRAL).
2. Poné la pelota claramente a la **DERECHA** del robot (su derecha, primera
   persona).
3. **Criterio:** `ball_x` debe ser **POSITIVO**. Si sale negativo → el eje X de
   la cámara está invertido respecto a la convención.
4. Repetir con la pelota a la izquierda (debe dar `ball_x` negativo).
5. Probar las DOS cámaras (frontal y trasera; la trasera ya invierte signo en
   `cam_obs_to_robot_frame`, confirmar que quede bien).

**Si está invertido:** negar el signo de x en `cam_obs_to_robot_frame`
(`cameras_fusion.cpp`) — avisar a Claude para hacerlo + re-test.

### B — Sentido de giro del BNO (CW vs CCW)
`localization.cpp:27` asume **heading+ = CCW** (giro a la izquierda). Si el BNO
da al revés, toda la pose y el HeadingPID quedan con el giro invertido.
1. Debug que imprima el heading del IMU (`sensors_imu_get_heading_deg`).
2. Girá el robot sobre su eje hacia **SU IZQUIERDA** (CCW visto desde arriba).
3. **Criterio:** el heading debe **AUMENTAR**. Si disminuye → el BNO da CW+ y
   hay que invertir el signo (avisar a Claude).

## Criterio de cierre
- Tabla completa: ball_x derecha (signo), ball_x izquierda (signo), por cámara.
- Sentido del BNO confirmado (sube al girar a la izquierda: sí/no).
- Si algo está invertido: documentado + corregido + re-testeado.
- Journal con los resultados.

## Relación con otras tasks
- Va de la mano con TASK-203 (orientación de zonas ToF) y con el diag
  `diag_central_drive` (CENTRAL) para el tema C de la convención (mezcla CW/CCW
  en strategy).

## Cambios de estado
- 2026-05-31: creada por Claude (Opus 4.8) al fijar la convención de ejes, a
  pedido de Gustavo (asegurar que todos interpreten izq/der igual).
