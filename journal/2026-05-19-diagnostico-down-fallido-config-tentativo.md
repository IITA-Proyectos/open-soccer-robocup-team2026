---
title: "2026-05-19 — Diagnóstico DOWN fallido: config_down.h tentativo me llevó a pelearme con hardware que sí anda"
date: 2026-05-19
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [hardware-up, down-board, diag, error-de-razonamiento, leccion, pinout, mux]
robot: ambos
area: electronica
tipo: postmortem
related-tasks: [TASK-001, TASK-026]
---

# Diagnóstico DOWN fallido: configuración tentativa tomada como cierta

> **🟢 CASO CERRADO 2026-05-24.** El pinout fue corregido y validado
> empíricamente con la placa física en banco (0 sensores muertos en el
> verdict). Ver `journal/2026-05-24-hardware-up-down-anillo-linea.md`.
> Esta entrada se conserva intacta como evidencia del error original (no
> editar el cuerpo — patrimonio de la lección "no asumir sin verificar").

> **TL;DR.** Armé un script de diagnóstico automatizado (`scripts/diag_capture.py`)
> que capturó datos de los 32 sensores en 3 lecturas (mesa / blanco / negro) y
> emitió un veredicto: 8 OK, 8 sospechosos, **16 muertos**, y un "bug de mux" en
> los 16 OK (S0=S4, etc.). **Veredicto incorrecto.** Enzo probó la placa y los
> 32 sensores andan físicamente. La causa real: `config_down.h` tiene los pines
> de control y salida de los muxes como "tentativos" (lo dice su propio
> comentario), `line_ring.cpp` los usa sin verificar, y el diag terminó
> toggleando pines inventados — lecturas de pines al aire en vez de los muxes
> reales del PCB.

## Lo que hicimos (paso a paso)

1. Flasheo del `diag_down` (commit `cc2829a`) en la Teensy 4.0 de la placa DOWN.
2. Detección de bug: el diag mostraba **8 sensores en vez de 32** porque el env
   compilaba con `DOWN_NUM_MUXES_CONNECTED=1` (default conservador). Agregado
   `-DDOWN_NUM_MUXES_CONNECTED=4` al env `diag_down` (commit `33ce303`),
   reflash → 32 sensores visibles.
3. Armado `scripts/diag_capture.py` (Python + pyserial) — abre COM10 desde mi
   sesión, lee 2 s de salida, parsea las líneas `LUZ S0:... S31:...`, calcula
   stats por sensor y guarda en `.captures/<label>.json`. Comando `--verdict`
   combina las 3 capturas (carpet / blanco / negro) y emite veredicto OK /
   SOSPECHOSO / MUERTO.
4. Test masivo de luz: Gustavo cerró el Serial Monitor, puso la placa sobre la
   mesa → captura "carpet"; tapó con hoja blanca → captura "blanco"; tapó con
   algo opaco → captura "negro". Datos en `.captures/{carpet,blanco,negro}.json`.

## Lo que el script reportó (resultados crudos)

```
Total: 32 sensores  |  OK: 8  Sospechosos: 8  Muertos: 16
MUERTOS    : [16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31]
SOSPECHOSOS: [8, 9, 10, 11, 12, 13, 14, 15]
```

Y un patrón "S0=S4, S1=S5, ..." consistente en las 3 capturas que interpreté
como "bug de mux: la línea de selección C no se mueve".

## Mi diagnóstico inicial (incorrecto)

Concluí, con tono de certeza: "tu placa tiene solo 4 sensores físicos únicos
en mux 1 + 2 únicos en mux 2 + 16 pines al aire = 6 sensores funcionales de
32. No alcanza para Incheon."

**Estaba mal.**

## La corrección de Enzo y la causa raíz

Gustavo: *"me dice enzo que probó y estaban andando todos los sensores. Me
parece que es posible que estés con un diagrama de pines equivocado."*

Verificado contra la documentación del repo:

1. **`src/down/config_down.h` líneas 52–61**: el propio archivo dice textual
   *"Mapeo tentativo (a confirmar): E10=C, E11=B, E12=A según el schematic"*
   y *"Asignaciones específicas del Teensy: pendiente Q3-similar para DOWN"*.
   Los valores actuales (`PIN_MUX_SEL_A=2`, `B=3`, `C=4`, `INH=5..8`,
   `OUT=A0..A3`) son **invenciones del firmware**, no copia del schematic.
2. **`hardware/electronics/2026-05-17-placa-base-down-componentes-y-circuito.md`
   sección 4 "Open items" #5**: *"Qué ADC del Teensy lee cada mux COM y cómo
   se comparten S0/S1/S2 — schematic."* El doc del PCB también admite que el
   mapeo no fue extraído del schematic.
3. **`hardware/electronics/mapa-pines-placas-nuevas.md` líneas 130–134**:
   describe la arquitectura (4 muxes, 8 canales, A/B/C compartidas, INH
   individuales, O1-O4 al ADC) pero NO el mapeo físico Teensy↔mux.

Lo que pasó realmente con el diag:
- Los pines que `select_mux_channel()` toggleaba probablemente NO son los
  A/B/C reales del CD4051 → los 4 muxes seleccionaban canales fijos o
  cualquier valor según ruido.
- Los pines `analogRead(PIN_MUX_OUT[m])` no son las salidas O1-O4 reales →
  algunos leían señales válidas (por casualidad), otros leían rail (1023
  saturado).
- El "patrón S0=S4" no era bug del mux — era el pin asociado a "C" sin
  controlar nada, o leyendo al revés.

**No hay manera con este firmware de saber cuántos sensores físicos andan.
Los datos capturados son ruido — útiles solo como evidencia de que el firmware
está apuntando a pines equivocados.**

## Lección (no repetir)

Este es el **mismo patrón de error** de la sesión de Avast (el "no hay red"):
**asumí sin verificar**. Specifically:
- `config_down.h` decía explícitamente "tentativo" → debí haberlo verificado
  contra el schematic ANTES de diagnosticar hardware.
- El doc del PCB del 17-may decía "schematic pendiente para confirmar mapeo"
  → ya había bandera roja explícita.
- Pelearme con los datos físicos (16 muertos!) en vez de cuestionar mi propia
  configuración fue el error.

**Regla para próximas sesiones Claude**: cuando un comentario en código diga
"tentativo / a confirmar / pendiente", **NO usarlo como base de diagnóstico
hardware sin verificación previa**. Es un sello de "no confiar en este número
hasta que alguien con el PCB lo confirme".

## Lo que sí salió bien

A pesar del veredicto erróneo, esto NO fue tiempo perdido:
- El script `scripts/diag_capture.py` funciona perfecto técnicamente: lee
  serial, parsea, agrega, calcula stats, formato bonito, veredicto
  configurable. **Reutilizable** apenas el firmware tenga el mapeo correcto.
- Confirmamos que `pio device monitor` / pyserial funcionan desde la sesión
  Claude → automatizamos lecturas sin que el usuario copie/pegue.
- Confirmamos que **el setup de testing en hardware funciona** (compilación →
  flash → captura serial → veredicto automático). El loop está cerrado, solo
  faltan los datos correctos.
- Identificamos un gap documental crítico que llevaba semanas sin resolverse
  (mapeo Teensy↔mux). Ahora explícito en TASK-026.

## Status del primer hardware-up

**Sigue PENDIENTE.** La regla 8 del CLAUDE.md exige "robot encendido + COMM
flasheada + DOWN reportando línea por UART real". Hoy:

- ✅ Placa DOWN físicamente alimentada y respondiendo por USB Serial.
- ✅ Firmware (diag) cargado y ejecutando.
- ❌ **NO podemos afirmar que los sensores reportan línea correctamente**
  (firmware apunta a pines equivocados).
- ❌ COMM no flasheada.
- ❌ Robot completo no encendido.

La moratoria sigue vigente. Lo de hoy es un **hardware-up parcial con bug
detectado**, no un hardware-up confirmado.

## Próximos pasos

1. **TASK-026** (creada hoy): Enzo confirma el mapeo real de los pines del PCB
   DOWN: qué pin del Teensy 4.0 corresponde a cada A/B/C del CD4051, qué pin
   a cada INH[0..3], qué pin analógico a cada O1..O4. Sin esto, ningún test
   del anillo es válido. **P0 — bloquea el hardware-up de DOWN.**
2. Cuando Enzo pase el mapeo: actualizar `config_down.h` (sin "tentativo" en
   los comentarios), recompilar `diag_down`, reflashear, repetir el test
   masivo con el script. Esperamos `Total: 32 OK`.
3. Datos en `.captures/` quedan archivados con README de "inválidos por
   firmware apuntando a pines equivocados — esperar TASK-026".
4. Actualizar `docs/ESTADO-ACTUAL.md`: DOWN sigue sin hardware-up validado.
