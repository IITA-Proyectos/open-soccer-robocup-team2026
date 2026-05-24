---
title: "2026-05-24 — Placa DOWN/BOTTOM pasa tests de banco — cierre del hardware-up del subsistema"
date: 2026-05-24
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [hardware-up, down-board, bottom, cierre, hito]
robot: ambos
area: electronica
tipo: hito
related-journals: [2026-05-24-hardware-up-down-anillo-linea.md, 2026-05-24-otos-lib-activada-y-power-cycle-bug.md]
related-tasks: [TASK-012, TASK-026, TASK-028, TASK-029, TASK-030, TASK-031]
---

# Placa DOWN/BOTTOM pasa tests de banco — cierre del subsistema

> **TL;DR.** La placa DOWN (también llamada "BOTTOM" — son sinónimos)
> completó los tests de banco con éxito el 2026-05-24. Los 2 subsistemas
> principales — anillo de 32 sensores de línea + 2 OTOS de odometría
> óptica — están operacionales y reportando datos correctos.
> Quedan 2 cosas pendientes que **el usuario decidió postergar
> conscientemente**: sacar la lámina protectora de los OTOS (cuando
> haya tapa de protección) y verificar la comunicación UART hacia las
> otras placas. Ninguna de las 2 bloquea seguir avanzando.

## Estado funcional confirmado

### ✅ Anillo de 32 sensores de línea (mux U1-U4)
- **0 sensores muertos** en el verdict del `diag_capture.py` (ver journal
  `2026-05-24-hardware-up-down-anillo-linea.md`).
- Pinout `config_down.h` + `line_ring.cpp` corregido contra schematic
  real (PIN_MUX_OUT A2/A3 → A8/A9; SEL A/B/C propios por mux; INH a GND).
- 31 de 32 sensores con datos por sweep (S31 truncado por bug menor del
  script de captura).

### ✅ OTOS U5 y U6 (odometría óptica)
- Lib SparkFun Qwiic OTOS activada en `otos.cpp` (ver journal
  `2026-05-24-otos-lib-activada-y-power-cycle-bug.md`).
- Ambos chips responden I²C en 0x17 (Wire + Wire1).
- Pose se actualiza con movimiento real verificado en banco.

## Lo que sabemos pendiente (NO bloquea, decisión del usuario)

### TASK-030 — Sacar lámina protectora OTOS
Los módulos SparkFun OTOS vienen con una lámina protectora sobre el lente
óptico (tipo film transparente que protege durante el envío/manipulación).
**Esa lámina sigue puesta** en los 2 OTOS de la placa actual. Sin sacarla,
el OTOS NO enfoca el piso correctamente → tracking errático aunque la
superficie tenga buena textura.

**Decisión del usuario 2026-05-24**: postergar hasta tener una **tapa de
protección** para el robot. Sacar la lámina sin tapa expone el lente a
golpes, polvo y rayones durante el manipuleo. Una vez que esté la tapa
(que va a proteger los OTOS de daño mecánico), se sacan las láminas y se
re-corre el test de TASK-029.

**Implicación práctica:** la validación cuantitativa de OTOS (TASK-029)
que parecía pendiente "por superficie A4 mala" probablemente sigue mala
**también** por la lámina puesta. Cuando se ataque TASK-029 hay que
sacar primero la lámina (TASK-030) y después medir.

### TASK-031 — Verificar comunicación UART real
Hoy DOWN reporta por **USB serial** (vía `diag_down`). La regla 8 de
CLAUDE.md exige confirmar que DOWN reporta línea **por UART real**
(Serial5 → TOP, Serial1 → CENTRAL) antes de declarar el hardware-up del
robot completo.

**Decisión del usuario 2026-05-24**: postergar para más adelante. Razón
implícita: requiere que también las placas TOP y/o CENTRAL estén en
condiciones de recibir/parsear las tramas, lo cual depende de TASK-006
(COMM flash) y de tener las 3 placas conectadas físicamente.

## Status del hardware-up regla 8 CLAUDE.md (al cierre de hoy)

| Condición | Estado |
|---|---|
| Robot encendido (placa DOWN) | ✅ |
| COMM flasheada | ❌ TASK-006 (sigue P0 bloqueante Incheon) |
| DOWN reportando línea por UART real | ❌ TASK-031 (postergada por usuario) |
| **OTOS leídos y reportando pose** | ✅ (cualitativo; cuantitativo bloqueado por TASK-030 lámina) |

**La moratoria de fábrica de papel SIGUE vigente** porque DOWN aún
reporta por USB serial, no por UART real. Pero el SUBSISTEMA DOWN está
declarado **operacional a nivel banco**.

## Próximas sesiones Claude (candidatos naturales)

1. **TASK-006** (COMM flash) — sigue siendo el P0 bloqueante de Incheon.
   Es la pieza que más cerca está de homologación del robot.
2. **TASK-022** (cámaras recalibradas para Incheon) — el otro P0
   bloqueante.
3. **TASK-031** (UART real DOWN→TOP/CENTRAL) — solo cuando esté
   disponible al menos otra placa para el otro lado del cable.
4. **TASK-029** + **TASK-030** (lámina OTOS + tracking real) — solo
   cuando esté la tapa de protección.

## Atribución

- Hardware en mano + decisiones operativas (postergar lámina y UART) —
  Gustavo Viollaz (@gviollaz).
- Cierre + documentación — Claude Opus 4.7 (Anthropic), sesión 2026-05-24.
