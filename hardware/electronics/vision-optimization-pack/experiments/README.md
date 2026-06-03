---
title: "experiments/ — sandbox del agente de visión"
date: 2026-06-03
---

# experiments/ — banco de pruebas (NO es producción)

Dejá acá lo que NO va (todavía) a los archivos canónicos:

- **Variantes** de los scripts N6 mientras tuneás (ej. `cam-frontal-n6.exp-thresholds.py`).
- **Datasets** de fotos/capturas para probar thresholds offline.
- **Resultados/medidas** de cada experimento (Hz, error de distancia, false positives) — anotá qué setup dio cada número.
- Notas de qué probaste y qué funcionó.

## Regla

> Cuando un cambio queda **validado**, **promovelo al archivo canónico**
> (`cam-*-n6.py` / `calib-lab-n6.py` / el parser del TOP — ver `01-mapa-de-programas.md`)
> y **borrá o marcá** la variante de acá. Esto evita que una versión vieja del
> experimento "pise" a la buena. Lo de `experiments/` nunca es la fuente de verdad.
