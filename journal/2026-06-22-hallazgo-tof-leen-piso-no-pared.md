---
title: "Hallazgo de banco: los ToF leen el PISO, no las paredes (altura de montaje > pared)"
date: 2026-06-22
author: "Claude Opus 4.8 (Anthropic) + Gustavo Viollaz (placa)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
tipo: banco-hallazgo
toca-competencia: NO (diagnóstico; no se tocó firmware)
status: hallazgo confirmado por datos · acción en team-tasks/TASK-225
---

# Los ToF leen el piso, no las paredes — robot SOLO en la cancha

## El dato (capturas de Gustavo, robot2 SOLO en la cancha real)

Con el robot solo en la cancha, los **4 ToF leían ~360–475 mm** (los 4). Eso es **imposible para
paredes**: la cancha es ~1820 × 2430 mm, así que desde cualquier punto no podés estar a ~45 cm de las
4 paredes a la vez (si estás a 45 cm de un lateral, del otro estás a ~1370 mm). Y daba ~450 mm **sin
importar la posición** → firma de **lectura de PISO** (depende de la altura/ángulo del sensor, no de
las paredes; una lectura de pared cambiaría al moverse).

## Por qué: el ToF está MÁS ALTO que la pared

Medidas aprox. del overlay de la GUI (marcadas "MEDIR" — **falta medir bien**):
- ToF a **~170 mm** de altura · pared de cancha **~140 mm**.

El ToF está ~30 mm por encima del borde de la pared. Mirando horizontal pasa por arriba; las zonas
que devuelven algo miran ~20° hacia abajo y **pegan en la alfombra a ~45 cm**, antes de llegar a la
pared. La ventana angular para ver la pared desde el centro es angosta (~2–10° abajo) **y depende de
la distancia** → la fila que dejamos activa (2ª desde abajo) cae fuera de esa ventana → piso.

(Contexto relacionado, TASK-203: el ToF **izquierdo** es de otro fabricante y se montó **mirando
abajo** → ve piso todavía más fácil.)

## Implicación

- **No invalida el bus a 400 kHz** (eso era velocidad de lectura — validado T1–T5). Esto es **geometría
  de montaje**, otra cosa.
- **SÍ rompe la localización por ToF**: la pose por paredes (`keeper_xy_walls.h`, TASK-221) y cualquier
  trilateración usarían distancias de **piso**, no de pared. Para esquivar algo cercano sigue
  reaccionando, pero no mide paredes. **P1 si la localización por ToF está en el plan (TASK-034).**

## Análisis 4×4 → 8×8 (consultado por Gustavo)

Pasar a 8×8 NO es la primera opción:
- **Complejo (medio-grande, días):** el "16 zonas/4×4" está cableado en ~12–15 archivos (firmware:
  resolución + máscaras 16→64 bits + **contrato de telemetría rompe el wire** + EEPROM; monitor: parser
  + toda la GUI de grillas/veto/rotación/pared/export; ~6–8 tests). Re-validación HW.
- **Costo runtime:** 8×8 capa a 15 Hz (ya estamos ahí) PERO el bloque I2C es **~4×** → cada lectura a
  400 kHz pasa de ~16 a ~64 ms → **devuelve casi toda la mejora de loop del 400 kHz**.
- **No resuelve la raíz:** da más resolución vertical (discrimina mejor) pero la ventana sigue siendo
  angosta y dependiente de la distancia → necesitarías selección **dinámica** de zona, no veto fijo.

**Fix barato y robusto: bajar el montaje del ToF a ≤ altura de pared (~13 cm)** → mira casi horizontal,
pega de lleno en la pared a toda distancia, **4×4 sobra**, sin tocar firmware/protocolo/GUI.

## Qué sigue → TASK-225
1. **Medir** bien altura real de ToF y de pared (las del overlay son estimadas).
2. Leer las **16 zonas crudas por sensor** con el robot en la cancha (ver si alguna fila ya pega en la
   pared, o confirmar que con esta altura no hay ninguna buena).
3. Decidir: **bajar el sensor** (preferido) vs re-elegir fila con el panel de pared vs 8×8 (último).
