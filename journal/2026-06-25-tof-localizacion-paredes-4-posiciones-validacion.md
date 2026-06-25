---
title: "ToF localización por paredes — validación empírica en 4 posiciones (modo max-range) + fórmula y alcance"
date: 2026-06-25
status: vivo (validación de banco; FÓRMULA confirmada, falta cablear)
placa: TOP (ROBOT2)
env: top_robot2_pri_tofmaxrange (4×4 + 2 Hz + continuo)
autor: "Claude Opus 4.8 (1M context) + Virginia (placa/posiciones), vía Claude Code"
testeado-en-hardware: SÍ (mediciones reales por serie; el cableado a pose lo cierra el equipo)
herramienta: tools/monitor-base/tof_zonas_promedio.py
---

# Los ToF SÍ localizan por paredes — validado leyendo el serial en 4 posiciones conocidas

## Método

Robot QUIETO en una posición conocida; Claude leyó la telemetría por COM15 ~70 s/posición y
**promedió las 16 zonas crudas de cada ToF** (`tof_zonas_promedio.py`). Las zonas son ruidosas frame
a frame pero **se estabilizan muy bien al promediar** (las franjas buenas con desvío 1–10 %). Env
`top_robot2_pri_tofmaxrange` (4×4 + 2 Hz + continuo) — el que ve las paredes NEGRAS.

## La fórmula: la "franja perpendicular"

En cada ToF hay una **fila/columna que apunta horizontal y mide la pared** (col 2 en los laterales por
la rotación; fila 2–3 en frente/fondo). Las filas/cols de abajo ven **piso** (cortas, súper estables);
las de arriba, **cielo** (vacías). La distancia a la pared = **promedio de la franja perpendicular**.

## Las 4 posiciones (franja perpendicular, mm)

| | LEFT (col2) | RIGHT (col2) | FRONT (fila2) | BACK | Lectura |
|---|---|---|---|---|---|
| **Centro** | 874 | 760 | 1131 (pared) | 1377 (hueco) | L+R+robot≈1804≈ancho ✓; FRONT=pared frente (1131≈1130) ✓ |
| **Arco, centrado** | 857 | 700 | 670 (piso) | 1000 (hueco) | laterales = centro ✓; FRONT fuera de alcance; BACK ve el HUECO del arco |
| **Arco, izquierda** | 533 ↓ | ~1050 ↑ | 680 (piso) | 640 (maciza) | L bajó / R subió (movió en X) ✓; BACK macizo = 640 (no 1000) → hueco confirmado |
| **Corner izq** | ~250 (toda la matriz) | NO (1735) | piso | ~250 (toda la matriz) | esquina nítida; **RIGHT a 1735 NO se ve** |

## Resultados duros

1. **Eje X (ancho 1820): VALIDADO y repetible.** La suma `LEFT + RIGHT + robot ≈ 1820` en las 3
   posiciones donde ambas se ven; al correr el robot a la izq, LEFT bajó ~320 y RIGHT subió ~350
   (movimiento en X correcto). La pared lateral cercana **siempre** está a ≤910 mm → **X medible
   siempre**.
2. **Alcance perpendicular con NEGRO ≈ 1,4–1,5 m** (no 1,9). Vio paredes a 825 / 1130 / 1135 mm; **NO**
   la de 1735 mm (RIGHT corner). Las lecturas estables a 1900–2700 mm son **diagonales/elevación**, no
   la perpendicular.
3. **HUECO del arco:** BACK frente al hueco de la portería da de más (1000–1377, ve a través);
   frente a pared maciza da la pared real (640). Hay que tratarlo (no usar BACK cuando apunta al hueco).
4. **Eje Y (largo 2430):** la pared de fondo cercana (≤1215) cae al **límite del alcance en el centro
   del largo** (marginal), pero **cerca de los fondos se ve bien**. → **Ideal para el ARQUERO** (juega
   cerca de su fondo + centrado-ish): ahí la localización por ToF es sólida.

## Veredicto

**Sí se puede localizar el robot solo con ToF.** X sólido en toda la cancha; Y bueno cerca de las
paredes de fondo (caso del arquero), marginal en el medio campo. La fórmula = franja perpendicular
promediada + geometría de cancha conocida.

## Pendiente (cablear, gateado)

1. Seleccionar la franja perpendicular por ToF (col 2 laterales / fila 2–3 fondo), vetar piso/cielo.
2. Promediar → distancia a la pared cercana de cada eje. Manejar: el hueco del arco (BACK) y elegir
   la pared en alcance.
3. Pose (x, y) por geometría de cancha. Medir `radio_sensor` (offset sensor→centro, asumido ~85 mm).
4. Si hace falta más alcance en Y: bajar a 1 Hz (más integración, más lento) — A EVALUAR.

## Herramienta

`tools/monitor-base/tof_zonas_promedio.py` — lee N s del serial y promedia las 16 zonas/sensor
(media±std+n) para re-medir en cualquier posición.
