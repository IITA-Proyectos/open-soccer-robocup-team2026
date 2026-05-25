---
title: "TOP rev 1.0 — Hallazgo forense: XSHUT/LPn de los 4 TOFs NO están ruteados"
date: 2026-05-25
author: "Claude Opus 4.7 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [top-board, hardware, xshut, tof, forensic-analysis, rev-1.1]
robot: ambos
area: electronica
tipo: analisis
---

# TOP rev 1.0 — Hallazgo forense: XSHUT/LPn de los 4 TOFs NO están ruteados

> **TL;DR.** Verificación forense del schematic + PCB de TOP rev 1.0
> (archivos `SCH_Roboliga2026_TOP_2026-04-12.json` +
> `PCB_PCB_Roboliga2026_TOP_2026-04-12.json`) confirma que los 4 pines
> XSHUT/LPn de los slots ToF (U2, U3, U5, U17) **están intencionalmente
> sin conectar** (NC flags explícitos en SCH, sin nets en el netlist del
> PCB). Implicancia dura: **máximo 2 ToFs soportados sin rework** (1 por
> bus I²C, porque ambos arrancan en 0x29 y sin XSHUT individual no se
> pueden enumerar). La línea `PIN_TOF_XSHUT[4] = {2,3,4,5}` en
> `config_top.h:68` es ficción heredada del diseño aspiracional — esos
> tracks no existen en la placa fabricada. Decisión técnica para Incheon
> (¿2 ToFs sin rework vs 4 ToFs con bodge?) escalada a `team-tasks/TASK-033`.

## Contexto

Ayer (2026-05-24) cerramos el hardware-up del VL53L7CX frontal (U2) con
la lib Adafruit (ver `journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md`).
Quedó pendiente entender qué hacer con los otros 3 slots (U3 en Wire,
U5 + U17 en Wire1). El plan original asumía enumeración estándar VL53L7CX:
encender los sensores de uno por bus vía XSHUT, asignar address I²C
custom (0x52, 0x54, 0x56, 0x58), prender el siguiente. Esto requiere
que cada XSHUT/LPn esté ruteado a un GPIO del Teensy.

Hoy Gustavo pidió: **"¿están ruteados los Xshut de los 4 ToFs en la
placa?"**. Pregunta que cambia decisiones de Incheon (cuántos ToFs
podemos prometer al firmware) y, si la respuesta es no, abre un
wishlist para TOP rev 1.1 (post-Incheon, 2027).

## Qué se hizo

Análisis forense de los archivos JSON canónicos del PCB rev 1.0 (delegado
a un subagente para evitar contaminar contexto):

- **SCH JSON**: `hardware/electronics/top-board-pack/ground-truth/SCH_Roboliga2026_TOP_2026-04-12.json`
- **PCB JSON**: `hardware/electronics/top-board-pack/ground-truth/PCB_PCB_Roboliga2026_TOP_2026-04-12.json`

Métodos:

1. **Búsqueda literal de strings** `XSHUT`, `Xshut`, `xshut`, `LPn`, `LPN` en
   ambos JSON.
2. **Coordenadas exactas de los pads Xshut** en el símbolo VL53L7CX del
   schematic (705 -600, 705 -500, 920 -595, 920 -495). Cada coord debería
   aparecer ≥2 veces si hay un wire saliendo: una en la definición del
   símbolo, otra en el endpoint del wire ruteado.
3. **Búsqueda de flags "No Connect" (NC)** — en EasyEDA son flags color
   `#33cc33`. Si están sobre un pad, el ERC los acepta como
   "intencionalmente sin conectar".
4. **Volcado del netlist completo del PCB** (`routerRule.nets`) para
   confirmar que ningún net `XSHUT*` / `LPn*` está definido.

## Qué se midió / observó (evidencia dura)

### Schematic

- Las 4 coordenadas Xshut (705 -600, 705 -500, 920 -595, 920 -495)
  aparecen **exactamente 1 vez cada una** en el SCH JSON — solo en la
  definición del símbolo VL53L7CX. **No hay un segundo endpoint** que
  indique un wire saliendo del pad.
- Se encontraron **8 flags `No Connect` explícitos** (color `#33cc33`)
  en el schematic: 4 sobre los pads Xshut + 4 sobre los pads INT.
  Esto es una afirmación intencional del diseñador: "estos pads no se
  conectan, no es un olvido, el ERC tiene que aceptarlo".

### PCB

- Netlist completo del PCB (líneas 455-482 del PCB JSON,
  `routerRule.nets`):
  ```
  +3.3V, +5V, +7.4V, 3.3V, ECHO, GND, LOGV, OUT1, OUT2,
  RX1, RX3, RX4, RX5, RX_OUT, SCL0, SCL1, SDA0, SDA1,
  TRIG, TX1, TX3, TX4, TX5, TX_OUT, U12_3, U13_3
  ```
  **No aparece ningún net `XSHUT*` ni `LPn*` ni `Xshut*`.**
- Búsqueda literal de `XSHUT` / `Xshut` / `xshut` en el PCB JSON
  completo: **0 matches**.

### Conclusión forense

Los 4 XSHUT/LPn de los slots U2/U3/U5/U17 están **intencionalmente sin
conectar** en TOP rev 1.0. No es un bug de ruteo escondido ni un net que
quedó en el schematic sin tracks (como pasó con la DOWN en 04-12, donde
10 nets quedaron sin rutear — ese fue olvido de DRC). Acá el diseñador
puso NC flags explícitos en cada pad, marcándolos como "intencionalmente
flotantes".

## Conclusión

1. **TOP rev 1.0 soporta máximo 2 ToFs sin rework hardware** — 1 sensor
   por bus I²C (U2 en Wire, U5 en Wire1). Ambos quedan en address default
   0x29, uno por bus, sin necesidad de XSHUT.
2. **Para usar los 4 ToFs hay que hacer bodge**: soldar 4 jumpers desde
   los pads Xshut (NC) hasta 4 GPIOs libres del Teensy 4.0. Trabajo
   delicado (los pads del módulo Pololu son ~0.5 mm, frágiles para
   soldar directamente) + riesgo de cortocircuitos. Estimación: 1-2
   horas de Enzo + riesgo de rotura del módulo.
3. **`config_top.h:68` con `PIN_TOF_XSHUT[4] = {2, 3, 4, 5}` es
   ficción**. Esos pines del Teensy no están ruteados a ningún pad
   Xshut en la placa fabricada. Es código heredado del diseño
   aspiracional pre-fabricación. **El firmware actual (`sensors_tof.cpp`
   migrado a Adafruit el 2026-05-24) ya no usa este array** — opera con
   1 sensor en 0x29 default sin XSHUT, así que no hay bug runtime hoy.
   Pero la línea queda como trampa para futuras sesiones que asuman que
   los pines existen.
4. **Solución de fondo: TOP rev 1.1 (post-Incheon)**. Rutear los 4
   XSHUT/LPn a 4 GPIOs del Teensy es de los items P0 del wishlist
   capturado hoy (ver `research/in-progress/2026-05-25-top-board-rev-1.1-wishlist.md`).

## Próximos pasos

### Inmediato (esta sesión)

- ✅ Banner aclaratorio en `config_top.h` para que ninguna sesión futura
  asuma que `PIN_TOF_XSHUT[]` está ruteado.
- ✅ Update de `docs/ESTADO-ACTUAL.md` con la deuda nueva.
- ✅ Wishlist de TOP rev 1.1 capturado en `research/in-progress/`.
- ✅ Team-task TASK-033 abierto para que Gustavo decida entre 2 ToFs
  (recomendación coach) vs 4 ToFs con bodge para Incheon.

### Post-Incheon (2027)

- Workshop con el equipo para confirmar prioridades del wishlist + sumar
  lo que aprendamos en torneo.
- Diseño TOP rev 1.1 en KiCad o EasyEDA — Enzo + mentor de PCB si
  conseguimos uno.
- Fabricar 2 unidades para tener spare.

## Atribución

- **Análisis forense del SCH + PCB JSON** — Claude Opus 4.7 (Anthropic),
  vía subagente del session 2026-05-25, sobre archivos ground-truth del
  `top-board-pack/`.
- **Pregunta clave que destrabó el hallazgo** ("¿están ruteados los
  Xshut?") + wishlist de mejoras para rev 1.1 (los 7 items) — Gustavo
  Viollaz (@gviollaz).
- **Diseño original de TOP rev 1.0** (con NC flags explícitos) — Enzo
  (enzzo195), 2026-04-12. Decisión intencional, no bug.
- **Journal + documentación** — Claude Opus 4.7 (Anthropic), session
  2026-05-25.

## Referencias

- Journal hardware-up TOF: `journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md`
- Wishlist rev 1.1: `research/in-progress/2026-05-25-top-board-rev-1.1-wishlist.md`
- Team-task decisión Incheon: `team-tasks/2026-05-25-task-033-decidir-cuantos-tofs-incheon.md`
- Archivos analizados:
  - `hardware/electronics/top-board-pack/ground-truth/SCH_Roboliga2026_TOP_2026-04-12.json`
  - `hardware/electronics/top-board-pack/ground-truth/PCB_PCB_Roboliga2026_TOP_2026-04-12.json`
