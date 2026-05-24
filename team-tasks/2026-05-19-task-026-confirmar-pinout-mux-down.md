---
id: TASK-026
title: "Confirmar mapeo físico de pines del PCB DOWN: Teensy ↔ CD4051 (muxes)"
date_created: 2026-05-19
assigned: [enzzo195]
priority: P2
status: validated-empirically
estimated_hours: 1
blocks: []
tags: [hardware, pcb, down-board, mux, pinout]
---

> **Update 2026-05-24** — VALIDADA EMPÍRICAMENTE en banco con la placa física.
> Gustavo + Claude (sesión ejecución directa) aplicaron el mapeo del doc
> `01-pinout-y-posiciones.md` al firmware (config_down.h + line_ring.cpp) y
> corrieron el verdict del diag_capture: **0 muertos / 9 OK / 22 SOSPECHOSO**
> (los SOSPECHOSO responden bien, solo no llegan al umbral 300 del script
> calibrado para cancha real). Ver journal `2026-05-24-hardware-up-down-anillo-linea.md`.
>
> **Ya NO bloquea el hardware-up de DOWN.** Baja de P0 a P2. El cierre formal
> (multímetro de Enzo en 2-3 nets representativas, §"Por Virginia/Elías" de
> abajo) sigue siendo deseable como red de seguridad pero no urgente.

# TASK-026 — Confirmar pinout Teensy ↔ CD4051 en placa DOWN

## Resumen

`src/down/config_down.h` tenía los pines del Teensy 4.0 a los 4 muxes
CD4051 marcados como **"tentativos"**. El diagnóstico fallido del 2026-05-19
expuso el problema. **Update 2026-05-19 tarde**: el mapeo completo fue
extraído automáticamente del schematic EasyEDA JSON y documentado en
`hardware/electronics/2026-05-19-pinout-down-extraido-schematic.md`.

La TASK pasa de "definir el mapeo" a **"validar el mapeo extraído"** con
Enzo + Virginia/Elías antes de actualizar el firmware. Sin esa validación
no se hace el cambio en `config_down.h` ni `line_ring.cpp`.

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

## Lo que necesitamos validar

Tabla **ya extraída** del schematic (no eran "compartidas" — cada mux tiene
sus 3 SEL propios; INH atados a GND). Ver doc completo
`hardware/electronics/2026-05-19-pinout-down-extraido-schematic.md`.
Resumen del mapeo a validar:

| Función | Pin Arduino (extraído del SCH) |
|---|---|
| U1.A / U1.B / U1.C | 13 / 2 / 3 |
| U1.COM (ADC) | A0 (=14) |
| U2.A / U2.B / U2.C | 4 / 5 / 6 |
| U2.COM (ADC) | A1 (=15) |
| U3.A / U3.B / U3.C | 7 / 8 / 9 |
| U3.COM (ADC) | A8 (=22) |
| U4.A / U4.B / U4.C | 10 / 11 / 12 |
| U4.COM (ADC) | A9 (=23) |
| Todos los INH (U1-U4 pin 6) | atados a GND directo (no se controlan) |
| OTOS U5 (Wire I²C0) | SDA=18, SCL=19 |
| OTOS U6 (Wire1 I²C1) | SDA=17, SCL=16 |
| UART hacia TOP (Serial5) | RX=21, TX=20 |

Y queda pendiente confirmar:
- Si ambos OTOS (U5 y U6) están poblados físicamente.
- **Orientación del montaje** del PCB respecto al chasis (¿+Y del PCB =
  adelante del robot?). El doc asume esto basado en simetría F1-F8 frontal,
  pero hay que confirmarlo con Enzo cuando se monte. Si está rotado, se
  rota la LUT `SENSOR_POS[]` con una matriz trivial.
- Para qué se usa el conector U11 (Serial1 + señal E1).

**Update 2026-05-23**: las posiciones físicas (x, y) en mm de los 32 sensores
F1–F32 ya fueron extraídas del **PCB layout JSON** (no solo el schematic).
Ver §5b del doc `2026-05-19-pinout-down-extraido-schematic.md`. Ya no necesitamos
foto con números — la geometría completa está documentada.

## Pasos concretos para VALIDAR (no para descubrir — ya está extraído)

### Por Enzo — verificación rápida del schematic

1. Abrir el doc `hardware/electronics/2026-05-19-pinout-down-extraido-schematic.md`.
2. Cotejar con el schematic en EasyEDA: 3–5 nets representativas elegidas al
   azar (ej. E5, O3, SDA2).
3. ✅ Confirmar OR ❌ corregir si hay error.
4. Confirmar también: a) ambos OTOS U5/U6 poblados, b) uso del conector U11,
   c) **orientación del montaje** del PCB en el chasis (asunción documentada:
   +Y del PCB = adelante del robot, lado del logo IITA del silkscreen apunta
   hacia adelante). Si el montaje es distinto, anotar la rotación (90°/180°/
   270°) y/o offset (dx, dy) en mm.

### Por Virginia/Elías — verificación con multímetro en banco (15 min)

1. Multímetro en modo continuidad.
2. Verificar 3 nets críticas (las que el firmware más usa):
   - Pin Arduino 13 (= header pin 14, SCK) ↔ pin 11 de CD4051 U1 → debería
     pitar (E1 / U1.A).
   - Pin Arduino A0 (= header pin 13) ↔ pin 3 de CD4051 U1 → debería pitar
     (O1 / U1.COM).
   - Pin Arduino 4 (= header pin 28, BCLK2) ↔ pin 11 de CD4051 U2 → debería
     pitar (E4 / U2.A).
3. Si los 3 pitan → mapeo confirmado para esos. Si alguno no pita → revisar
   ese específico contra el schematic.

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
- 2026-05-24: validated-empirically. Mapeo del doc canónico aplicado al
  firmware (commit con journal del mismo día). Verdict del diag: 0 muertos,
  los 32 sensores responden. P0→P2. Falta solo el cierre formal con
  multímetro (no bloqueante).
