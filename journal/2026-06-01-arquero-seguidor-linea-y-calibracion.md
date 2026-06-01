---
title: "Sesión banco: arquero seguidor de línea (DOWN→CENTRAL) + herramienta de calibración"
date: 2026-06-01
author: "Claude Opus 4.8 (Anthropic), supervisión María Viollaz"
ai-assisted: true
status: en-progreso
tags: [arquero, seguidor-linea, down-central, calibracion, line_ring, cross_track, pid-heading, banco]
robot: ambos (Zircon Rev v15 + Teensy 4.1/4.0)
---

# Sesión banco: arquero seguidor de línea + calibración de sensores

## Objetivo de la sesión

Lograr que el robot se comporte como **arquero**: se desplaza lateralmente a lo
largo de la línea blanca del área (que corre paralela a su frente), manteniéndose
centrado sobre ella adelante/atrás, patrullando el arco. Cuando la línea se curva
en las esquinas, cambia de lado.

## Lo que se logró (hitos)

### 1. Botón del arquero — antirrebote por estabilidad
El botón (pin 9) daba apretones falsos por ruido. Se reemplazó la lógica vieja
(`esperarSoltarBoton` + `delay`) por detección de flanco con antirrebote por
estabilidad de tiempo (`BTN_DEBOUNCE_MS = 50`), el mismo método probado de
`diag_central_motors`. Ahora arranca con un apretón y sigue solo; otro apretón
para.

### 2. Movimiento lateral — control directo de motores (NO cinemática genérica)
La cinemática genérica (`inverse_kinematics`) daba CÍRCULOS porque
`WHEEL_ANGLES_DEG` no está calibrado. La solución correcta fue control directo,
basado en lo probado en banco con el robot nuevo:
- **Movimiento lateral usa los TRES motores**: M1 y M2 en direcciones CONTRARIAS
  entre sí, M3 (trasero) ACOMPAÑA y va más rápido (es el que más empuja).
- Direcciones para un lado: M1 dir=-1 (vel 55), M2 dir=+1 (vel 55), M3 dir=+1 (vel 100).
- Para el otro lado se invierten los tres.
- Polaridad de M2 INVERTIDA por hardware (INA/INB al revés que M1 y M3).
- Pines robot nuevo (Zircon Rev v15): M1=(INA2,INB5,PWM3), M2=(INA8,INB7,PWM6),
  M3=(INA11,INB12,PWM4).
- Confirmado físicamente: con esta lógica el robot se mueve lateral (de costado).
  Tiene algo de desvío sin giroscopio (esperado).

### 3. Herramienta de calibración de sensores (placa DOWN) — NUEVA
Creado `src/diag/diag_down_calibracion.cpp` + env `diag_down_calibracion`.
Pensada para usar en CANCHA. Corre en DOWN, se controla por monitor serie:
- `c` = capturar verde (carpet), `b` = capturar blanco, `t` = probar detección
  en vivo, `s` = GUARDAR en EEPROM (persistente), `v` = ver promedios/umbrales,
  `m` = ver sensores sospechosos, `l` = cargar de EEPROM, `x` = borrar.
- Usa las funciones REALES del firmware (`line_ring` + `eeprom_calib`), así lo
  calibrado es lo que después usa el firmware.
- **Procedimiento prolijo**: capturar verde con los 32 sensores sobre carpet;
  capturar blanco con una hoja de papel blanco grande cubriendo TODOS los
  sensores (no una línea fina, que dejaría sensores mal calibrados).
- Resultado de la calibración de María: solo 2 de 32 sensores sospechosos
  (S9 margen~72-77, S16 margen~70-73), apenas bajo el umbral de 80. 30/32 OK.
  Decisión: dejarlos así, es buena calibración.

### 4. Detección de línea — FIX de raíz (dos calibraciones desincronizadas)
**Problema descubierto**: había DOS rutas de detección con calibraciones
distintas:
- `line_ring` (lo que María calibra con el calibrador y se usa para el centroide).
- `DownModel` (`dm_update`, de donde sale `line_present` que llega a CENTRAL).

Al arrancar, `main_down` cargaba el blanco de EEPROM SOLO en el DownModel, no en
el line_ring → el line_ring quedaba con su blanco DEFAULT (800) y NO detectaba.
Por eso `linea=no` casi siempre y `cross=N/A`.

**Solución de fondo** (3 archivos):
- `line_ring.h` / `line_ring.cpp`: nueva función `line_ring_set_calibration()`
  que carga carpet/white por sensor desde afuera (sin muestrear hardware).
- `main_down.cpp`: tras cargar la calib de EEPROM, ahora también la inyecta en
  el line_ring con `line_ring_set_calibration()`. Mensaje nuevo en boot:
  `[DOWN] line_ring calibrado con blanco de EEPROM`.

Resultado validado: tras el fix, `linea=SI` SOSTENIDO cuando hay línea, y
`cross` da valores reales que varían (no el 74mm fijo de antes).

### 5. cross_track_mm — DOWN calcula la posición adelante/atrás de la línea
`comm_central.cpp` (bloque `#ifdef DOWN_DEBUG_SERIAL`, solo banco, NO competencia):
- Calcula el CENTROIDE en Y de los sensores que ven blanco (usando
  `SENSOR_POS[i].y_mm` de `sensor_geometry.h`, +Y = adelante).
- Lo mete en `cross_track_mm` (que antes venía N/A): POSITIVO = línea adelante
  del centro, NEGATIVO = atrás.
- Re-codifica el frame con ese valor y lo reenvía por Serial5 (U10), donde está
  soldado el cable hacia CENTRAL.

### 6. Arquero (seguidor) — `diag_central_line_sweep.cpp` reescrito
Patrulla lateral (los 3 motores) + corrige adelante/atrás con `cross_track_mm`
para centrarse sobre la línea:
- Presencia de línea derivada de `cross_track_mm != N/A` (line_ring calibrado),
  NO de `line_present` del DownModel (que está mal afinado).
- Deadband de centrado `CENTRADO_DEADBAND_MM = 15`: si |cross| < 15mm, está
  centrado y no corrige. Si no, empuja adelante/atrás con `CENTRADO_PWM = 45`.
- Si pierde la línea `LOST_LINE_MS = 800`, frena (esquina / fin del área).
- Watchdog de enlace SIEMPRE activo. Envs `_nosafety_` saltean levantado.
- Estado validado: `linea=SI cross=...mm corr=...` coherente; el centrado
  responde (corrige cuando descentrado, para cuando centrado).

## Problema pendiente (dónde quedamos)

El arquero se mueve pero **describe un SEMICÍRCULO**: sin giroscopio, el robot
no mantiene su orientación mientras patrulla, así que rota en vez de ir derecho
a lo largo de la línea.

Se integró un **PID de heading** (BNO055, portado del código de marzo
`test-motores-lateral-simple`) en `diag_central_line_sweep.cpp`, usando
`imu_zircon` de la placa CENTRAL. PERO: **el giroscopio está en la placa de
ARRIBA (TOP), que todavía no está conectada.** El código tiene degradación
elegante (si no hay BNO, no corrige), pero queda PENDIENTE:
- Decidir si sacar el PID por ahora o dejarlo desactivado hasta tener TOP.
- Verificar si los signos `ROT_M1/M2/M3` están bien para este robot (ajuste de prueba).

## Convenciones / decisiones clave registradas

- **Movimiento lateral = 3 motores** (M1, M2 contrarios + M3 acompaña), NO solo 2.
- M2 tiene polaridad INA/INB invertida por hardware en el robot nuevo.
- La cinemática genérica NO sirve hasta calibrar `WHEEL_ANGLES_DEG` con Enzo
  (pendiente, da círculos).
- Una sola fuente de verdad para detección de línea: el `line_ring` calibrado
  por María (no el DownModel).
- El cable de DOWN→CENTRAL sale por U10 (Serial5); con `down_debug` DOWN reenvía
  la línea por ahí. CENTRAL la recibe por Serial1 (header UART del Zircon, pin 0).

## Archivos tocados esta sesión

Nuevos:
- `src/diag/diag_down_calibracion.cpp` (+ env `diag_down_calibracion` en `platformio.ini`)

Modificados:
- `src/diag/diag_central_line_sweep.cpp` (reescrito: arquero seguidor + PID heading)
- `src/down/comm_central.cpp` (cálculo de `cross_track_mm` en bloque debug)
- `src/down/line_ring.h` / `line_ring.cpp` (función `line_ring_set_calibration`)
- `src/down/main_down.cpp` (carga calib EEPROM también al line_ring)
- `platformio.ini` (env `diag_down_calibracion`; `imu_zircon.cpp` y filtros en
  envs del arquero)

## Cómo reproducir / probar (runbook)

```
# DOWN — calibrar sensores (con la placa DOWN por USB)
pio run -e diag_down_calibracion -t upload
pio device monitor -b 115200
#   c (verde) -> b (blanco con papel) -> m (sospechosos) -> t (probar) -> s (guardar)

# DOWN — firmware de banco que manda la línea + cross_track
pio run -e down_debug -t upload
#   verificar en boot: "[DOWN] line_ring calibrado con blanco de EEPROM"

# CENTRAL — arquero (sin seguridad, ruedas al aire)
pio run -e diag_central_line_sweep_nosafety_robot1 -t upload
pio device monitor -b 115200
#   apretar boton pin 9 -> patrulla. Mirar linea=SI, cross=...mm, corr=...
```

## Próximos pasos

1. Resolver el semicírculo: cuando esté la placa TOP con el BNO055, activar el
   PID de heading (ya integrado). Verificar signos ROT.
2. Detectar el cambio de lado en las esquinas del área (cuando la línea se curva).
3. Bajar el umbral de `imminent_exit` (salta con solo 3-4 sensores; demasiado
   sensible).
4. Reactivar la seguridad (env sin `_nosafety`) cuando DOWN esté bien calibrado.
5. Medir `WHEEL_ANGLES_DEG` reales con Enzo (soluciona el desvío de fondo).
