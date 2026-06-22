---
id: TASK-225
title: "ToF leen el piso, no las paredes: medir alturas y decidir bajar el montaje"
date_created: 2026-06-22
assigned: [Gustavo, Enzo]
priority: P1
status: pending
estimated_hours: 2
blocks: [localizacion-por-tof, TASK-221-keeper-xy-walls, TASK-034-localizacion-incheon]
blocked_by: []
relacionado: [TASK-203, TASK-034, TASK-221]
tags: [top-board, tof, vl53l7cx, montaje, mecanica, localizacion, pared, hardware-test]
---

# TASK-225 — Los ToF leen el PISO, no las paredes (altura de montaje)

> Hallazgo 2026-06-22 (banco, robot2 SOLO en cancha): los 4 ToF leían ~360–475 mm sin importar la
> posición → es el **piso**, no las paredes (imposible estar a ~45 cm de las 4 paredes a la vez en una
> cancha de 1820×2430). Detalle + análisis 4×4/8×8 en
> `journal/2026-06-22-hallazgo-tof-leen-piso-no-pared.md`.

## Por qué (a verificar con medición)

El ToF está montado a **~170 mm** (estimado) y la pared mide **~140 mm** → el sensor queda **por encima
del borde de la pared**. Las zonas que devuelven algo miran hacia abajo y pegan en la alfombra a ~45 cm
antes de llegar a la pared. **NO es problema del bus a 400 kHz** (eso quedó validado, T1–T5); es geometría
de montaje.

## Impacto

- **Localización por paredes rota:** `keeper_xy_walls.h` (TASK-221) y cualquier trilateración usarían
  distancias de **piso**. Bloquea la pose por ToF (TASK-034).
- Para evasión de algo cercano sigue reaccionando; para MEDIR paredes, no.

## Qué hacer (humano)

1. **MEDIR con regla** la altura real del centro óptico del ToF y la altura de la pared de la cancha
   (las del overlay de la GUI son "aprox"). Anotar ambas.
2. Con el robot **en la cancha**, leer las **16 zonas crudas por sensor** (Claude lo hace por serie, o
   con la vista ToF-360 / zonemap) y ver el gradiente vertical: filas de arriba (¿pasan por encima de la
   pared → largo/sin retorno?), filas de abajo (¿piso → corto?). Identificar si **alguna fila pega en la
   pared** y a qué distancias.
3. **Decidir el fix** (en orden de preferencia):
   - **Bajar el montaje del ToF** a ≤ altura de pared (~13 cm) → mira casi horizontal, pega de lleno en
     la pared a toda distancia, **4×4 alcanza**, sin tocar firmware/protocolo/GUI. ← preferido.
   - Si no se puede bajar: re-elegir la fila de veto con el panel **"PARED + CANCHA"** de la GUI usando
     las alturas medidas (ventana angosta y dependiente de la distancia → frágil).
   - 8×8: **último recurso** (días de trabajo, rompe el wire app↔firmware, devuelve la mejora de loop del
     400 kHz, y aún necesitaría selección dinámica de zona). No empezar por acá.

## Criterio de cierre

- [ ] Altura real de ToF y de pared, medidas y anotadas.
- [ ] Gradiente de las 16 zonas por sensor, en cancha, capturado (journal).
- [ ] Decisión tomada y aplicada (bajar montaje / re-veto / otro) y **verificado en banco**: robot a
      distancia conocida de una pared despejada → el ToF lee la distancia REAL (no el piso).
- [ ] Re-confirmar `keeper_xy_walls` / pose por paredes con distancias buenas.

## Cambios de estado
- 2026-06-22: creada (Claude Opus 4.8, a pedido de Gustavo) tras el hallazgo en banco. P1 porque
  bloquea la localización por ToF.
