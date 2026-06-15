---
id: TASK-214
title: "Elías: PEGAR las 4 matrices de homografía (una por cámara) en este archivo — solo los VALORES"
date_created: 2026-06-15
assigned: [elias]
priority: P1
status: pending
pedido-por: Gustavo Viollaz (2026-06-15)
estimated_hours: 0.25
blocks: ["fijar las matrices calibradas de Elías en el baseline versionado robot1.h/robot2.h (HI-5)"]
tags: [vision, camaras, homografia, calibracion, elias, fill-in]
depends_on: []
---

# TASK-214 — Elías: pegá las 4 matrices de las cámaras ACÁ

## Qué hacés (5 minutos, copiar-y-pegar nada más)

Calibraste las 4 cámaras el 2026-06-14, cada una con su matriz `H_MATRIX`. Para que esa
calibración quede **guardada en el repo versionado** (hoy `robot1.h`/`robot2.h` tienen
placeholders viejos), necesito que me pases las 4 matrices. **Pegalas abajo y listo** — yo
me encargo de ponerlas en el firmware.

## 🟢 LO ÚNICO QUE TENÉS QUE HACER

Para **cada una de las 4 cámaras**: abrí su `main.py` (el que flasheaste en esa cámara,
`hardware/electronics/camaras-openmv/main.py` de esa OpenMV), **copiá el bloque `H_MATRIX`
COMPLETO** (las 3 filas entre `[` y `]`), y **pegalo abajo reemplazando el `PEGAR_ACA`** del
slot que corresponde a esa cámara.

## ⛔ LO QUE **NO** TENÉS QUE HACER

- ❌ NO edites ningún `.py`, `.h`, `.cpp` ni otro archivo. **Solo este archivo.**
- ❌ NO toques nada fuera de los 4 bloques `PEGAR_ACA` de abajo.
- ❌ NO cambies el formato, los nombres, ni el orden. Solo pegás los números.
- ❌ NO recalibres nada. Esto es solo COPIAR lo que YA tenés en cada `main.py`.

> Si una cámara no la calibraste todavía o no estás seguro, dejá su slot con `PEGAR_ACA` y
> avisá cuál — no inventes valores.

---

## 📋 LOS 4 SLOTS — pegá cada matriz donde dice `PEGAR_ACA`

### 1) ROBOT 1 (delantero) — cámara FRONTAL
```python
H_MATRIX = PEGAR_ACA
```

### 2) ROBOT 1 (delantero) — cámara TRASERA
```python
H_MATRIX = PEGAR_ACA
```

### 3) ROBOT 2 (arquero) — cámara FRONTAL
```python
H_MATRIX = PEGAR_ACA
```

### 4) ROBOT 2 (arquero) — cámara TRASERA
```python
H_MATRIX = PEGAR_ACA
```

---

## Ejemplo del formato (así se ve un `H_MATRIX` — pegá el TUYO, no este)

```python
H_MATRIX = [
    [ 7.54500000e-01, -1.23000000e-02,  3.45000000e+01],
    [-2.10000000e-03,  6.98000000e-01,  1.20000000e+01],
    [-1.10000000e-05,  9.80000000e-04,  1.00000000e+00],
]
```

(opcional, si lo tenés a mano: anotá al lado de cada una el error de reproyección que te
dio `solve_homografia.py` —medio/max en cm— y el `exposure_us`/`gain` que dejaste fijo. No
es obligatorio; lo obligatorio son las 4 matrices.)

## Qué pasa después (lo hace la IA / el equipo, no vos)

Con las 4 matrices pegadas acá, una sesión las copia al baseline versionado
(`src/shared/robot_config/robot1.h` y `robot2.h`, campos `CAM_FRONT_H_MATRIX` /
`CAM_BACK_H_MATRIX`) en formato C++ — cierra HI-5 de TASK-022. El robot-def queda reflejando
tu último-bueno medido, no un placeholder.

## Criterio de cierre

Los 4 slots con sus matrices reales pegadas (o marcados los que falten). Avisás y la IA hace el resto.

## Atribución

Template + esta TASK: Claude Opus 4.8 (Anthropic), 2026-06-15 (requested-by Gustavo Viollaz).
La calibración de las 4 matrices es de Elías (2026-06-14).
