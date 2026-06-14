---
title: "TOP telemetría v2 (A1 del monitor de posicionamiento): detecciones POR CÁMARA + OTOS/línea/escape de la base, en JSON + texto humano"
date: 2026-06-14
author: "Claude (Anthropic, Opus 4.8) — superpowers (Brainstorm→Plan→Execute→Review) + workflow multi-agente"
requested-by: "Gustavo Viollaz (@gviollaz)"
area: comunicacion
tipo: resultado
robot: ambos
status: code-complete + gate host verde + pio SUCCESS — PENDIENTE BANCO (hardware lo cierra el equipo)
relacionada: TASK-205, TASK-206, research/in-progress/2026-06-13-diseno-monitor-general-top-config-persistente.md
---

# A1 — Telemetría TOP v2: per-cámara + base (OTOS/línea/escape)

**Sesión:** Claude + Gustavo. Metodología pedida por Gustavo: **superpowers** (análisis completo
antes de codear, validar + documentar cada paso, test al final) + **workflow** multi-agente para
el análisis en paralelo.

## Qué es A1

Primer paso del "Monitor del sistema de posicionamiento" (spec del 2026-06-13). La TOP es el hub
donde converge todo antes de la CENTRAL. A1 EXPONE en telemetría lo que el firmware **ya computa y
hoy tira**, sin cambiar conducta:

1. **Detecciones POR CÁMARA** (`camf`/`camb`): pelota + 2 arcos que ve cada óptica por separado,
   ADEMÁS del fusionado (`cam`). El delta front↔back delata la **pelota fantasma** del promedio
   fusionado (la que vimos en banco el 2026-06-13: velocidad ±13 m/s) → se decide cuál cámara
   apagar con dato.
2. **Base (DOWN)**: OTOS (`base`: pose+vel) + línea + **vector de escape** (`line`: `escape_cd`
   dirección + `pen_mm` magnitud) que la TOP recibe por `comm_down` y no exponía.
3. Todo visible también en el **modo texto humano** (ENTER) → diagnóstico en banco sin la app.

## Proceso (superpowers + workflow)

- **Brainstorm:** iterado con Gustavo (per-cámara, ubicación/rotación ToF, OTOS/línea de la base,
  reframe a "monitor de posicionamiento"). Spec en `research/in-progress/`.
- **Análisis (workflow, 5 agentes en paralelo):** 4 Explore mapearon cámaras / relay de base /
  serializador+contrato / glue, + 1 síntesis → plan de 10 tareas verificado contra el código.
  Hallazgos clave: (a) NO había getters per-cámara (recompute_fused calculaba las CamObs y las
  descartaba); (b) el "vector de escape" = `escape_angle_centideg`+`penetration_mm` de LineStatusV2,
  no un struct aparte; (c) `comm_down` ya exponía todo (pose/vel/línea + frescura).
- **Execute:** 10 tareas en orden, host-test/compile en cada checkpoint.
- **Review:** este journal + doc del contrato.

## Cambios (todos host-testeables o compile-verificados)

- `src/shared/cameras_fusion.{h,cpp}`: helper PURO nuevo `cam_obs_to_polar()` (ángulo/dist de UNA
  cámara, misma convención que `fuse_goal_dual`) + 4 tests.
- `src/top/cameras_runtime.{h,cpp}`: persiste las CamObs pre-fusión (ball/yellow/blue × front/back)
  + 18 getters per-cámara. Aditivo, solo lectura (~+48 B RAM), no cambia la fusión.
- `src/shared/telemetry_top.{h,cpp}`: **schema 1→2**. Struct +~40 campos (per-cámara + base). Frame
  init pone sentinelas N/A de línea. Serializer: bloques `camf`/`camb` (tras `cam`) + `base`/`line`
  (tras `snap`). `tt_format_human`: líneas CAMF/CAMB/BASE/LINE (FRESH/STALE/INVALID + `--` en N/A).
- `src/top/comm_central.{cpp,h}`: (sin cambio nuevo; ya OR-gateado en el commit del monitor dormido).
- `src/top/top_telemetry_serial.cpp`: `#include comm_down.h` + cableado en `fill_frame()` (per-cámara
  + base **gateado por `is_*_fresh()`**: si no fresh, no expone ceros como reales). Buffers JSON
  1024→1536, humano 768→1024.
- **App (`tools/monitor-base`, parser TOP — NO la GUI):** `protocol_top.py` v2 (dataclasses
  `CamPer`/`Base`/`Line`, sentinelas −32768/65535→None, decode de eventos de línea); `simulator_top.py`
  emite v2; `conftest.py` → `golden_top_v2.jsonl` (nuevo, byte-idéntico al GOLDEN C++, sha256
  verificado); tests del parser/sim a v2. **`gui_top.py` NO se tocó** (panels = carril del otro
  agente; el contrato queda documentado para que sume CAMF/CAMB/BASE/LINE).
- `docs/firmware/TELEMETRIA-TOP.md`: contrato v2 (bloques + ejemplo JSON + texto humano + transición
  v1→v2 wire-breaking).

## Verificación (lo que Claude SÍ cierra)

- **Host:** `test_cameras_fusion` 23→**27**; `test_telemetry_top` 17→**19**; **`pytest tools/monitor-base` 82/82**.
  Gate host completo: **VERDE** (ver corrida; el GOLDEN C++ y el `.jsonl` son byte-idénticos, sha256).
- **Firmware Teensy:** `pio run -e top_robot2_pri` **SUCCESS** + `-e top_robot2_pri_debug_telemetry`
  **SUCCESS** (FLASH code 71076 / data 100536 / headers 8608; sobra). Cierra el riesgo de link.

## NO validado (regla no negociable — hardware lo cierra el equipo)

- ⚠️ El binario de competencia cambió (struct/serializer más grandes); sigue **match-safe** (stream
  dormido sin USB). Banco: con USB + app/ENTER ver `camf`/`camb` (desacuerdo de cámaras), `base`
  (OTOS) y `line` (vector de escape) con datos REALES de DOWN por UART. **WIRE-BREAKING v1→v2:**
  reflashear el TOP **y** usar la app v2 (la vieja rechaza el frame, fail-safe).

## Coordinación (no chocar con el otro agente)

- El otro agente trabaja la **GUI/DOWN** de `monitor-base`. Esta sesión tocó el **parser TOP**
  (`protocol_top.py`/`simulator_top.py`/golden/tests) + firmware TOP + docs. **No** se editó
  `gui_top.py` ni archivos de DOWN. Antes de editar se verificó que los archivos TOP de la app
  estaban limpios (último commit TOP = FASE 2; el agente activo está en DOWN).
- **Pendiente coordinado:** que la GUI (`gui_top.py`) sume los paneles per-cámara + base/línea (el
  contrato v2 está en `TELEMETRIA-TOP.md`).

## Próximo (A2, TASK-206)

Config persistente en EEPROM (deshabilitar cámara/BNO/ToF + zonas/orientación/ubicación de ToF).
