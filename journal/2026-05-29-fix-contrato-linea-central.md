---
title: "Fix P0 — contrato de línea DOWN→CENTRAL (LineStatus 5B → LineStatusV2 16B)"
date: 2026-05-29
placa: CENTRAL
branch: agente/central
author: Claude Opus 4.7 (Anthropic)
requested_by: Viollaz
tags: [firmware, central, down, uart, contrato, p0, audit]
---

# Fix P0 — contrato de línea DOWN→CENTRAL + auditoría firmware CENTRAL

## Pedido

Gustavo: *"haz un análisis del estado del desarrollo del firmware para placa
central, busca errores y posibilidades de mejora. Y en paralelo continúa con
el desarrollo de las funcionalidades pendientes."*

## Auditoría — hallazgos (formato coach, resumido)

Leí todo `src/central/` + lo compartido que consume. Prioridades:

- **P0 #1 — Contrato de línea roto (firmware, host-fixable).** `comm_down.cpp`
  exigía `payload_len == sizeof(LineStatus)` (5 B); DOWN manda `LineStatusV2`
  (16 B). `16 == 5` → false → **todos** los frames de línea descartados.
  CENTRAL ciego a la línea. **← arreglado en esta sesión.**
- **P0 #2 — Conflicto pines 7/8** (Serial2 vs motor). Ya trackeado: TASK-036 /
  Avance diag_central_motors. Plan existente: migrar Serial2 → Serial7 (28/29).
- **P1 #3 — Overflow omega en HeadingPID.** `output_clamp=360` deg/s → `360*100
  = 36000 > 32767` (int16) → wrap de signo con errores de heading >109°. Toca el
  cerebro; NO tocado esta sesión (requiere validación en banco).
- **P1 #4 — `LINE_AVOID` inalcanzable.** `main_central.cpp` hace brake+return
  antes de `strategy_tick()` con la misma condición que gatea la transición.
  Decisión de arquitectura; NO tocado.
- **P2** — BNO055 init pero nunca usado (~6 s boot); `strategy_set_attack_color`
  muerto; comentarios "motor server" stale; `COMMAND_TIMEOUT_MS=200` muerto
  (timeout real 500 en world_model); KICKOFF comentario vs código; PWM sin
  deadband.

## Fix aplicado (P0 #1)

Objetivo de diseño: **NO tocar el cerebro** (`strategy.cpp`). Lo logré
manteniendo intactas las firmas de los accessors de `world_model`.

1. **`src/shared/line_view.h`** (nuevo, header-only, sin Arduino) — helpers
   puros host-testeables que interpretan `LineStatusV2`:
   - `lsv2_from_frame` — extrae el struct de un `Frame`, validando
     `type==LINE_URGENT` y `payload_len==16` (único punto que valida tamaño →
     fail-safe ante cambios de schema).
   - `lsv2_imminent_exit` — gateado por `data_valid` **y** `!lifted` (honra el
     contrato documentado en `strategy.cpp:17`, que el código viejo no cumplía).
   - `lsv2_lifted`, `lsv2_line_present`, `lsv2_line_angle_deg` (NA-aware),
     `lsv2_penetration_u8` (clamp a 255), `lsv2_sensors_on_line`.
2. **`comm_down.cpp`** — `handle_frame` usa `lsv2_from_frame` → 
   `world_model_apply_line(LineStatusV2)`.
3. **`world_model.{h,cpp}`** — guarda `LineStatusV2`; los 4 accessors de línea
   delegan en los helpers de `line_view.h`. Firmas idénticas ⇒ `strategy.cpp`
   y los diag sin cambios.
4. Comentarios stale corregidos en `comm_down.h` y `main_central.cpp`
   ("LineStatus" → "LineStatusV2"). `LineStatus` viejo se mantiene en `types.h`
   (lo sigue usando `src/top`).

## Verificación

⚠️ **`pio test -e test_native` NO pudo correr en este entorno**: la sandbox no
tiene red y PlatformIO no puede descargar la dependencia Unity (timeout ~71 s,
ERRORED antes de compilar — el test existente `test_down_encode` también ERRORÓ
igual, confirmando que es ambiental, no del código).

Verificación alternativa hecha (offline, contra las fuentes reales):
- **`test/test_central_line_ingest/test_main.cpp`** (nuevo, Unity) escrito,
  modelado sobre `test_down_encode`. Queda listo para que el equipo lo corra
  con red: `pio test -e test_native -f test_central_line_ingest`.
- **Harness g++ standalone** compilando `proto.cpp + down_encode.cpp +
  crc16.cpp + line_view.h` (MinGW 11.2, `-Wall -Wextra`, 0 warnings):
  **8/8 checks PASS**, incluyendo el chain real encode→decode→interpret, el
  gating por lifted, y el rechazo del payload viejo de 5 B (guard de regresión).
  Confirmó `sizeof(LineStatus)=5` vs `sizeof(LineStatusV2)=16` (la causa exacta).
- **`-fsyntax-only`** sobre `world_model.cpp` y `comm_down.cpp` (stub Arduino):
  0 errores, 0 warnings.

**No hay validación en hardware.** El fix es código correcto y host-verificado;
que funcione sobre el robot lo cierra el equipo → **TASK-100**.

## Pendiente humano

- **TASK-100** (nueva, P0, blocked_by TASK-036): validar en banco el ingest de
  línea DOWN→CENTRAL + frenado de emergencia + gating lifted.
- Correr el test Unity con red para tener el verde "oficial".

## Notas

- Doc/código reconciliados: `ESTADO-ACTUAL.md:48` ya afirmaba "comm_down recibe
  LineStatusV2" — era aspiracional; ahora el código lo cumple.
- Discrepancia menor detectada (no corregida, fuera de scope): `ESTADO-ACTUAL.md`
  línea ~230 dice "DOWN→CENTRAL (Serial1)" pero `comm_down` usa **Serial2**.
  Anotar para TASK-031.
- Moratoria: este trabajo cae en la excepción "desbloqueo de hardware" (mismo
  criterio que diag_central_motors el 2026-05-28). El fix es pre-requisito
  literal del hito "DOWN reportando línea por UART real" a CENTRAL.
