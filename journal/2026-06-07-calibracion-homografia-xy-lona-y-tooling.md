---
title: "Procedimiento de calibración de homografía (XY) de las cámaras N6: lona + tooling + PDF"
date: 2026-06-07
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M, Anthropic)"
status: final
tags: [vision, camaras, openmv, n6, homografia, calibracion, XY, TASK-022, TASK-202, herramientas]
robot: ambos
area: vision
tipo: resultado
---

# Calibración de homografía (XY) — procedimiento simple y confiable + lona

## Contexto

Gustavo pidió un procedimiento **rápido, simple y confiable** para calibrar la
posición XY que reportan las cámaras (la `H_MATRIX`, hoy placeholder), y decidir
si alcanza con poner la pelota en puntos sueltos o conviene una **lona con grilla**.

## Qué se hizo

- **Análisis de lo existente:** el "Paso 4 — Homografía" de `CALIBRACION-VISION-N6.md`
  y el §3 de los packs eran un **stub** (4 puntos leídos a ojo, "calcular H con
  OpenCV" sin solver real). El modelo del código (`cam-*-n6.py::transformar`) es
  correcto: H del plano del suelo + corrección `(h−r)/h` por el radio de la pelota
  → calibrar contra puntos en el piso es lo válido.
- **Decisión (documentada):** **lona con grilla > pelota en puntos sueltos** (1 foto
  captura muchos puntos, sin leer píxeles a ojo, mínimos cuadrados robustos,
  repetible en Incheon). La pelota queda como **validación** final.
- **Tooling (3 piezas):**
  - `solve_homografia.py` (PC, numpy): DLT + normalización de Hartley + SVD;
    reporta error de reproyección. **Validado** (`--selftest` recupera una H
    conocida con error ~1e-11).
  - `calib-homografia-n6.py` (en cámara, ambos packs): detecta los puntos negros
    con `find_blobs` (lo verificado en la N6), los ordena por filas con corte en
    los mayores saltos de cy (robusto a la perspectiva), los dibuja numerados e
    imprime las correspondencias listas para el solver.
  - `gen_lona_calibracion.py` (PC, reportlab) → **`lona-calibracion-homografia-A1.pdf`**:
    lona A1 (594×841 mm) a escala 1:1, grilla 5×5 (X=−24/−12/0/+12/+24 cm; filas
    y=0/15/30/45/60 cm desde el borde cercano), flecha +Y, eje X=0, y una **regla
    de control de escala de 200 mm** para verificar la impresión.
- **Doc canónico nuevo:** `docs/firmware/CALIBRACION-HOMOGRAFIA-XY-N6.md` (paso a
  paso ~10 min/cámara). Registrado en `FUENTES-DE-VERDAD.md`; los stubs viejos
  (Paso 4 de `CALIBRACION-VISION-N6.md` + §3 de los packs) ahora apuntan acá.

## Qué se midió/observó

- `solve_homografia.py --selftest` → **OK** (reproyección ~0, |H−H_true| ~1e-11).
- PDF verificado: **A1 exacto (594×841 mm), 25 puntos**, render visual correcto
  (grilla, flecha, regla de escala, etiquetas).
- `py_compile` OK en los 4 scripts Python.
- **Nada en hardware** — todo host/código. La calibración real es de banco.

## Conclusión

Queda un procedimiento de homografía completo y a prueba de banco: imprimir la
lona (1:1, verificar con la regla de 200 mm) → `calib-homografia-n6.py` imprime
correspondencias → `solve_homografia.py` da la `H_MATRIX` → pegar en `cam-*-n6.py`
→ validar con la pelota a 30/50/80/100 cm (<10% error). El solver está validado;
los scripts de cámara son fail-safe pero **no probados en la N6 real**.

## Próximos pasos (banco / humano)

- Imprimir la lona A1 (print shop o plotter; vinilo para Incheon). **Verificar la
  regla de 200 mm.**
- Correr el flujo por cámara (frontal y trasera tienen H distintas).
- Confirmar en la N6 que `find_blobs`/`draw_string` del kit andan (fail-safe igual).
- Validar el signo de X (pelota izquierda → X negativo, TASK-202).
- Mejora futura (P2): grilla de **AprilTags** si `find_apriltags` corre en la N6.
- Claude **no cierra TASK-022/202** (banco/humano).
