---
id: TASK-026
title: "Confirmar mapeo físico de pines del PCB DOWN: Teensy ↔ CD4051 (muxes)"
date_created: 2026-05-19
assigned: [enzzo195]
priority: P0
status: pending
estimated_hours: 1
blocks: [hardware-up DOWN, todos los tests con sensores reales, line_ring funcional]
tags: [hardware, pcb, down-board, mux, pinout, bloqueante]
---

# TASK-026 — Confirmar pinout Teensy ↔ CD4051 en placa DOWN

## Resumen

`src/down/config_down.h` tiene los pines del Teensy 4.0 conectados a los 4
muxes CD4051 marcados como **"tentativos"** desde hace semanas. El firmware
los está usando como si fueran ciertos. El 2026-05-19, el diagnóstico
automatizado de sensores (`scripts/diag_capture.py` + `diag_down`) reportó
**16 sensores muertos + bug de mux**, cuando en realidad Enzo confirmó que
los 32 sensores físicos andan. La causa fue mi config tentativo, no el
hardware.

**Sin este dato confirmado, ningún test del anillo de línea es válido.**

## Contexto

Lo que `config_down.h` dice HOY (líneas 50–61):

```cpp
constexpr int PIN_MUX_OUT[4] = { A0, A1, A2, A3 };
// Mapeo tentativo (a confirmar): E10=C, E11=B, E12=A según el schematic.
// Asignaciones específicas del Teensy: pendiente Q3-similar para DOWN.
constexpr int PIN_MUX_SEL_A = 2;
constexpr int PIN_MUX_SEL_B = 3;
constexpr int PIN_MUX_SEL_C = 4;
constexpr int PIN_MUX_INH[4] = { 5, 6, 7, 8 };
```

Los docs del PCB del 17-may también admiten que el dato falta:
- `hardware/electronics/2026-05-17-placa-base-down-componentes-y-circuito.md`
  sección 4 "Open items" #5: *"Qué ADC del Teensy lee cada mux COM y cómo se
  comparten S0/S1/S2 — schematic."*
- `hardware/electronics/mapa-pines-placas-nuevas.md` líneas 130–134 describe
  la arquitectura pero NO el mapeo físico.

Ver journal `2026-05-19-diagnostico-down-fallido-config-tentativo.md` para
el postmortem completo.

## Lo que necesitamos

Una **tabla confirmada** (mirando el schematic o midiendo con multímetro
sobre la placa real):

| Función | Pin del Teensy 4.0 (a completar) |
|---|---|
| Línea de selección A del mux (compartida 4 muxes) | ? |
| Línea de selección B del mux (compartida) | ? |
| Línea de selección C del mux (compartida) | ? |
| INH del mux #1 (U1) | ? |
| INH del mux #2 (U2) | ? |
| INH del mux #3 (U3) | ? |
| INH del mux #4 (U4) | ? |
| Output O del mux #1 (entrada analógica) | ? |
| Output O del mux #2 | ? |
| Output O del mux #3 | ? |
| Output O del mux #4 | ? |

Y opcionalmente: cuál de las 2 posiciones OTOS (U5/U6) está poblada y en qué
bus I²C (Wire/Wire1/Wire2) cae.

## Pasos concretos

### Opción A — Leer del schematic en EasyEDA (más rápido)

1. Abrir el proyecto en EasyEDA / KiCad
   (`hardware/electronics/pcb_design/down_board/`).
2. Localizar U1 (CD4051BM). Click en cada pad (A=pin 11, B=pin 10, C=pin 9,
   INH=pin 6, COM=pin 3 del CD4051).
3. Trazar cada net hasta el pad correspondiente del Teensy U7.
4. Anotar el número de pin lógico del Teensy 4.0 (no el número de pad físico
   del módulo).
5. Repetir para U2, U3, U4.

### Opción B — Multímetro en continuidad (si EasyEDA no está accesible)

1. Identificar los pads del Teensy 4.0 en la cara Top de la placa.
2. Probar continuidad pin del Teensy ↔ pad 11 (A) de U1. Cuando suena, ese
   pin es PIN_MUX_SEL_A.
3. Repetir para B (pad 10), C (pad 9), INH (pad 6), COM (pad 3) de los 4
   muxes.

## Criterio de cierre

- [ ] Tabla completa de los 11 mapeos (3 SEL + 4 INH + 4 OUT) confirmada.
- [ ] PR / commit a `src/down/config_down.h`: reemplazar las constantes
      tentativas por los valores reales. Borrar/actualizar el comentario
      "Mapeo tentativo (a confirmar)".
- [ ] Recompilación de `pio run -e down` + `pio run -e diag_down` exitosa.
- [ ] Re-ejecución del test masivo (`scripts/diag_capture.py`) con resultado
      `32 OK, 0 sospechosos, 0 muertos`.
- [ ] Cierre del bloqueo del hardware-up de DOWN; actualizar
      `docs/ESTADO-ACTUAL.md`.

## Notas / decisiones

_(actualizar cuando se ejecute)_

## Cambios de estado

- 2026-05-19: creada tras detectar que el diagnóstico de sensores fallaba
  por config tentativa. Enzo confirmó verbalmente que los 32 sensores
  físicos andan; necesitamos formalizar el mapeo para que el firmware lo use.
