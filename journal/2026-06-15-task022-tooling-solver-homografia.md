---
title: "TASK-022 (cámaras): tooling host del solver de homografía + fix de punteros de producción stale"
date: 2026-06-15
author: "Claude (Anthropic - Claude Opus 4.8 1M) — coach"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M) — workflow de inventario (7 agentes) + verificación adversarial"
status: final
tags: [vision, camaras, calibracion, homografia, tooling, host, task-022]
robot: ambos
area: vision
tipo: implementacion
---

# TASK-022 — tooling host del solver de homografía (acelera el banco)

## Contexto

Gustavo pidió avanzar con TASK-022 (cámara operativa, **bloqueante #1 de Incheon**) con un
workflow paralelo. **La verdad honesta:** el núcleo de TASK-022 es BANCO con cámara real
(calibrar LAB de color bajo la luz de Incheon, lock de exposición/WB, fps@VGA, validar
distancias con regla) — Claude NO lo puede cerrar. Un workflow (7 agentes + verificación
adversarial) identificó lo **host-testeable/tooling** que SÍ acelera el banco sin tocar hardware.

## Qué se hizo (PC tooling, validado con Python; sin cámara)

- **`solve_homografia.py` endurecido** (`hardware/electronics/vision-optimization-pack/tools/`):
  - **HI-1 `--csv FILE`**: carga las correspondencias de un CSV (`u,v,X,Y` por fila, `#` comentarios).
    Antes el `--csv` estaba PROMETIDO en el docstring pero NO implementado → había que **editar el
    fuente a mano** en cada recalibración, y la homografía se corre **≥4 veces** (2 cámaras × 2 robots
    + re-validación en sede). Es lo que más acelera el banco.
  - **HI-2 guardas anti-calibración-mala**: puntos colineales (pixel o físico), <4 puntos, o H[2,2]≈0
    ahora lanzan un **mensaje claro** en vez de devolver una H inválida en silencio (el equipo se la
    llevaba al banco como buena y perdía tiempo persiguiendo un "error de montaje" inexistente).
  - **HI-3 modo `--validate u,v ...`**: predice el (X,Y) cm de píxeles de prueba con la H resuelta →
    confirmar la calibración **numéricamente ANTES de flashear** (espeja el Paso 5 del doc sin gastar
    una corrida de banco con la pelota).
  - Fix de un emoji `⚠️` que **crasheaba** el script en la consola cp1252 de Windows (bug latente).
  - `print_h_block` ahora apunta a `main.py` (producción), no a `cam-*-n6.py` (ver HI-6).
- **`test_solve_homografia.py` (NUEVO)**: suite host (numpy, sin cámara) que fija el comportamiento
  del único cálculo de calibración 100% PC y queda como red de regresión 2027: recuperación de una H
  conocida, error de reproyección, colinealidad, <4 puntos, parseo CSV. **14/14 PASS.**
- **HI-6 (doc P1, de-risk de Incheon)**: 3 docs canónicos mandaban a pegar la `H_MATRIX` en
  `cam-*-n6.py`, que están **DEPRECADOS** (banner "NO USAR EN PRODUCCIÓN", 2026-06-08); el script de
  producción que se flashea es `camaras-openmv/main.py` (FUENTES-DE-VERDAD.md). Sin esto, el equipo iba
  a pegar la H en el archivo equivocado en sede. Corregido el **banner del doc de homografía** +
  `ESTADO-ACTUAL.md:300` + el `print_h_block` del solver.

## Pendiente (banco — Claude NO cierra TASK-022)

- Recalibrar **LAB** (naranja/amarillo/azul) en `main.py` bajo la luz de Incheon (la luz del venue
  invalida los thresholds de Salta) + **lock de exposición/WB/gain** + estabilidad 10 min.
- **fps real @VGA** (la H está atada a VGA) — si <25 Hz, optimizar/bajar resolución (rompe la H → recalibrar).
- **Validar distancia con regla** (<10% error) a 30/50/80/100 cm → confirma `CAMERA_UNIT_TO_MM=10`.
- Opcional (decisión Gustavo): **HI-5** alinear el baseline LAB/H de `robot2.h` al último-bueno de
  `main.py` (hoy `robot2.h` tiene placeholders genéricos; `robot2.h` no se compila en ningún target →
  sería doc aditiva, byte-idéntico) — el review lo dejó bloqueado a tu confirmación.

## Verificación

`python test_solve_homografia.py` → 14/14. `--selftest` reproduce la H conocida (|H_est−H_true| ~1e-11).
`--csv` + `--validate` ejercitados a mano. Todo PC/numpy; ningún cambio toca el firmware ni el binario.
