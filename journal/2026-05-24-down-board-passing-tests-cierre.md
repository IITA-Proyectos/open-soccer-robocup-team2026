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

## Test final post-lámina (2026-05-24, mismo día tarde)

> **Update final 2026-05-24** — Gustavo decidió sacar las 2 láminas
> protectoras de los OTOS sin esperar la tapa de protección, para hacer
> la prueba cuantitativa de tracking en la misma sesión.

### Hallazgo intermedio: A4 sin lámina = 0 movimiento detectado
Primer test post-lámina sobre hoja A4: el OTOS reportó **0 mm de delta
en 15 segundos** mientras Gustavo movía despacio 30 cm. Diagnóstico:
sin lámina, el OTOS enfoca correctamente, pero la hoja A4 blanca es
demasiado uniforme — no hay micro-textura que el sensor pueda seguir.
Antes con la lámina al menos "veía" patrones desenfocados que daban
ruido errático (de ahí los 28 mm de delta sin sentido). Comportamiento
análogo a por qué los mouse ópticos no funcionan sobre vidrio.

### Test definitivo: cartón corrugado sin lámina
Repetido sobre **cartón corrugado** (las ondas/fibras dan textura ideal):

```
t(s)     x(mm)      y(mm)    hdg(deg)   dist(mm)
  0.28  -260.5     +29.8     -14.5         0.0
 11.13  -260.5     +29.8     -14.5         0.0   ← aún quieto
 12.33  -256.2     +29.9     -14.5         4.3   ← arrancó
 13.54   -24.6      +2.7      -9.5       237.5   ← mitad del movimiento
 14.74   +17.5      -7.0      -8.7       280.4   ← llegada

Desplazamiento neto: 280.4 mm sobre movimiento real de 300 mm
Error: 19.6 mm = 6.5% (bajo el 8% de tolerancia de TASK-029)
Trayectoria: monotónica, sin saltos erráticos ✅
```

### Comparación pre/post

| Escenario | Reportado | Real | Eficiencia |
|---|---|---|---|
| Hoja A4 + lámina puesta | 28.6 mm | 300 mm | 9.5% (catastrófico) |
| Hoja A4 sin lámina | 0.3 mm | 300 mm | 0% (sin features) |
| **Cartón corrugado sin lámina** | **280.4 mm** | **300 mm** | **93.5%** ✅ |

**Mejora 10x con respecto al setup original.** Sacar la lámina + usar
superficie con textura visible son los 2 factores críticos para que el
OTOS funcione. En cancha verde RoboCup ambas condiciones están
satisfechas por defecto.

### Cierre TASK-029 y TASK-030
- **TASK-030** (sacar lámina) → **completed** mismo día.
- **TASK-029** (validación cuantitativa) → **validated-empirically**
  con tests 2 (rotación) y 3 (round trip) pendientes pero no críticos.
- Herramienta nueva agregada: `scripts/diag_otos_move_test.py` para
  re-ejecutar este test cuando haga falta (por ej. en cancha real).

## Lo que sigue pendiente al cierre final del día

### TASK-031 — Verificar comunicación UART real (sigue postergada)
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
