---
title: "03 — Calibración y tuning (LAB, exposición, flip, homografía)"
date: 2026-06-03
---

# 03 — Calibración y tuning

> El **procedimiento completo de banco** (paso a paso, ~15 min/cámara, repetible)
> está en **`docs/firmware/CALIBRACION-VISION-N6.md`**. Acá va el resumen para el
> agente + qué se puede preparar sin la cámara en la mano.

## Los 5 pasos (resumen)

1. **UART** — confirmar que la N6 le llega al TOP (probar `UART_PORT` 1/2/3;
   frontal→Serial3, trasera→Serial5). Ajustar `HMIRROR`/`VFLIP` si la imagen sale espejada.
2. **LAB** — con `calib-lab-n6.py`: por cada color (naranja/amarillo/azul) poner el
   objeto llenando la sonda, leer el `TUPLE sugerido`, pegarlo, ver que el recuadro
   verde rodee SOLO el objeto. Copiar los 3 tuples al script de producción.
3. **Exposición** — `BRING_UP=False` + ajustar `EXPOSURE_US` (~37000) hasta que se
   vea bien; re-verificar los 3 colores (con autos ON la luz rompe los LAB en partido).
4. **Homografía** — 4 puntos conocidos → `H_MATRIX`; validar `Y ≈ distancia` con <10% a 30/50/80/100 cm.
5. **Guardar** — `Save as main.py` en la N6, power-cycle, confirmar que transmite sin IDE.

## Qué puede mejorar el agente SIN banco (alto valor)

- Hacer el `calib-lab-n6.py` **más robusto y rápido**: que sugiera tuples con
  margen ajustable, que reporte # de blobs y área, que dibuje el recuadro en vivo,
  que no se cuelgue (usa API OpenMV estándar — verificar contra docs, **no se pudo
  probar en la N6 real todavía**).
- Dejar los `cam-*-n6.py` con **exposición fija lista** (`BRING_UP=False` como
  default seguro), sentinel correcto, clamps anti-crash, `pixels_min` sano.
- Estructurar los thresholds para que **recalibrar en Incheon** sea cambiar 3 tuples
  y nada más.

## Qué es banco/humano (no inventar)

Los **valores reales** de LAB / `EXPOSURE_US` / `H_MATRIX` / `CAM_HEIGHT_CM` salen
de medir con la cámara montada y la luz real → **TASK-022** (Virginia). El agente
deja placeholders + flags, no números inventados.
