---
name: openmv-vision-tuning
description: Use when calibrating or troubleshooting OpenMV cameras (H7 / H7 Plus) for the soccer robot — LAB color thresholds for orange golf ball (passive IR ball 2026) and cyan/magenta goals, exposure lock under varying field lighting, FOV and mount tuning, multi-camera consistency, frame rate vs accuracy trade-offs. Critical for Incheon where lighting differs from the IITA Salta lab.
---

# OpenMV Vision Tuning — Calibración de Cámaras para Soccer

> **Status: outline only — content pending iteration with real cameras.**

## When to use

- Calibración inicial de cámara (primera vez en una cancha nueva).
- Re-calibración tras cambio de iluminación (cancha distinta, hora del día, sombras).
- Diagnóstico de falsos positivos / negativos (la cámara "no ve" la pelota o ve fantasmas).
- Setup de configuración multi-cámara (1, 2 o 4 cámaras consistentes entre sí).
- Trade-off frame rate vs accuracy (más resolución vs más FPS).

## When NOT to use

- Cambios al firmware del Teensy (eso es `vibe-robotics-coding`).
- Diseño del mount mecánico de la cámara (eso es `vibe-mechanical-design`).
- Cambios al protocolo UART OpenMV↔Teensy (eso es `vibe-robotics-coding`).

## Stack visión

- **Hardware:** OpenMV H7 / H7 Plus (la H7+ tiene más RAM, soporta resoluciones más altas).
- **Lenguaje:** MicroPython.
- **Targets 2026:**
  - Pelota: golf ball naranja **pasiva** (42mm, regla 2026 — no más pelota IR activa).
  - Arcos: cyan (propio) / magenta (rival).
  - Líneas: blancas sobre carpet verde (no detectadas por cámara — sensores IR línea hacen eso).
- **Output al Teensy:** UART, packet con ball angle/dist/conf, goal angle/visible.

## Protocolo de calibración (planned content)

[TODO: desarrollar con cámaras reales del equipo]

### 1. Exposure y white balance lock

Una vez calibrados, **bloquearlos**. Cualquier auto-ajuste durante el partido es enemigo (la cámara "aprende" mal cuando aparecen robots oscuros o uniformes).

```python
sensor.set_auto_gain(False, gain_db=...)
sensor.set_auto_whitebal(False, rgb_gain_db=(...,...,...))
sensor.set_auto_exposure(False, exposure_us=...)
```

### 2. LAB color thresholds

**Pelota naranja (golf ball 42mm pasiva 2026):**
```python
BALL_ORANGE_LAB = (L_min, L_max, A_min, A_max, B_min, B_max)
# Calibrar bajo luz objetivo, no luz de lab.
```

**Arco cyan (propio):**
```python
GOAL_CYAN_LAB = (50, 100, -50, -10, -50, -10)  # Punto de partida típico
```

**Arco magenta (rival):**
```python
GOAL_MAGENTA_LAB = (30, 80, 20, 70, -30, 10)  # Punto de partida típico
```

### 3. Protocolo de tuning

1. Robot ensamblado, cámara montada como en partido (no en mano, no en mesa).
2. Iluminación de la cancha (no la del lab si difiere).
3. Pelota a distancias 20/50/100/150 cm — verificar detección estable.
4. Capturar `img.compress()` muestras en `research/references/openmv-samples/` con timestamp y condición.
5. Ajustar thresholds con `image.find_blobs([thresh], …)` hasta:
   - `confidence > 50` para distancias < 100cm.
   - Sin falsos positivos sobre carpet, paredes negras, líneas blancas, otros robots.
6. Lock config y commit el archivo en `software/vision/`.

### 4. Frame rate vs accuracy

- **Objetivo:** 30 FPS para tracking de pelota dinámica.
- **Trade-offs:**
  - Resolución más baja → más FPS pero menos precisión angular.
  - ROI (region of interest) → más FPS, pero pierde periferia.
  - Múltiples `find_blobs` calls → corta FPS.
- Medir FPS con `clock.fps()` y loguearlo en `testing/results/`.

### 5. Multi-camera consistency

Si hay 2+ cámaras, **todas calibradas en la misma sesión** con la misma pelota e iluminación. Sino: cada cámara "ve" colores distintos y la fusión en `WorldModel` (ver playbook `multi-camera-world-model.md`) falla.

## Common failures (planned content)

[TODO: poblar con casos reales del 2025 + lo que pase en Incheon]

- **Falso positivo en líneas naranjas/rojizas del público.** Solución: ROI que excluye horizonte, o LAB más estricto en L (brillo).
- **Blob splitting** (la pelota se detecta como 2-3 blobs pequeños). Solución: `merge=True` y `area_threshold` razonable, o lente más enfocada.
- **Goal color shift** (cyan se ve más verde con luz fluorescente). Solución: thresholds dependientes de iluminación → presets switcheables, o calibrar in-situ.
- **Exposure auto-adjust** durante partido (cámara "aprende" mal el color promedio cuando entra robot oscuro). Solución: lock explícito sí o sí.
- **Cámara pierde la pelota a > 1.5m.** Solución: lente con FOV adecuado, o aceptar el rango con fallback de IR pelota array.
- **Latencia del frame** (cámara reporta posición vieja → robot persigue donde estuvo). Solución: medir latencia, predecir con Kalman si > 50ms.

## Configuración para Incheon (planned)

[TODO: definir antes del viaje]

- Capturar muestras de luz de Songdo Convensia si hay fotos del año pasado en RCJ Forum.
- Tener 2-3 perfiles de calibración guardados (luz "alta", "media", "baja").
- Protocolo: **15 min antes del partido**, recalibrar bajo la luz real de la cancha asignada.
- Tener pendrive con scripts de calibración listos para correr offline.

## Captura de aprendizaje (post-Incheon)

Llevar cuaderno (papel) + script de captura: cada partido anotar:
- Iluminación percibida.
- Falsos positivos / negativos vistos.
- Ajuste de threshold que hicimos (si aplica).

Después del torneo: consolidar en `research/completed/2026-07-XX-vision-incheon-lessons.md`.

## References

- Reglas pelota 2026: https://github.com/robocup-junior/ir-golf-ball
- `skills/soccer-ball-detection.md` (playbook) — auditar contra esta skill en Fase 1.
- `software/vision/` — código actual.
- `research/completed/2026-04-19-analisis-seleccion-camaras-robocup-soccer-open.md` — análisis previo de selección de cámaras.
- `hardware-test-protocol` skill — test plan para validar la calibración.
