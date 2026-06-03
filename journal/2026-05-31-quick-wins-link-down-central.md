---
title: "Quick-wins del link DOWN→CENTRAL: schema gate + telemetría SEQ + observabilidad"
date: 2026-05-31
placa: CENTRAL
branch: agente/central
author: Claude Opus 4.8 (Anthropic)
requested_by: Viollaz
tags: [firmware, central, down, uart, protocolo, telemetria, analisis]
---

# Quick-wins del link DOWN→CENTRAL (post-análisis profundo)

## Contexto

Tras el análisis profundo del link DOWN↔CENTRAL
(`docs/firmware/ANALISIS-COMM-DOWN-CENTRAL-2026-05-31.md`, workflow de 56 agentes),
el coach pidió avanzar con los "quick wins" de firmware que caen en scope
**CENTRAL / shared**. Implementados + verificados (compila + host); **NO validados
en hardware** (eso lo cierra el equipo).

## Implementado (3 hallazgos del análisis)

1. **#4 — Gate de `schema_version` en `lsv2_from_frame` (`src/shared/line_view.h`).**
   Antes validaba sólo tipo + tamaño (16 B). Ahora también
   `schema_version == LSV2_SCHEMA`: un schema futuro del MISMO tamaño (campos
   reordenados, `reserved` reusado) se **rechaza** en vez de reinterpretarse como
   basura. Seguro: DOWN setea `s.schema_version = LSV2_SCHEMA` en
   `down_model.cpp:102` → los frames reales pasan. Test nuevo
   `test_wrong_schema_rejected` en `test_central_line_ingest`.

2. **#3 — Detección de pérdida de frames en producción (`src/central/comm_down.cpp`).**
   El `SEQ` del protocolo viajaba pero CENTRAL lo ignoraba (la única detección de
   huecos vivía en el diag de banco). Ahora `handle_frame` acumula
   `g_frames_lost += (uint8_t)(seq - last - 1)` — mide la **magnitud** del hueco y
   maneja el wrap 255→0. Getter `comm_down_get_frames_lost()`.

3. **#8 (observabilidad) — `data_valid` + `event_flags` expuestos + print de telemetría.**
   Nuevos `world_model_line_data_valid()` / `world_model_line_event_flags()`. El
   print de debug de `main_central.cpp` (cada 500 ms) ahora muestra
   `down[rx=… crc=… lost=… valid=… ev=0x…]` — la telemetría del enlace que faltaba
   para diagnosticar "a ojo" en banco (modo aprendizaje Incheon).

Bonus de consistencia: el header de `comm_down.h` ahora documenta el conflicto
7/8 / plan-B Serial7 (antes lo omitía — hallazgo LINK-06 del análisis).

## Verificación

- `pio run -e central_robot1` → **SUCCESS** (firmware de producción CENTRAL; usa
  todo lo tocado: `line_view` vía `world_model`, `comm_down` con SEQ, el print).
- `pio run -e diag_central_comm_down` → **SUCCESS**.
- **Harness g++ offline** del schema gate (`proto.cpp + crc16.cpp + down_encode.cpp
  + line_view.h`, `-Wall -Wextra`, 0 warnings): **7/7 PASS** — schema=2 aceptado,
  schema=99 (frame válido, CRC OK, 16 B) **rechazado** por el gate, payload viejo
  5 B rechazado, tipo equivocado rechazado.
- `pio test -e test_native` **NO** se corrió (sin red para bajar Unity). El test
  `test_wrong_schema_rejected` queda listo para correr con red.
- **NO validado en hardware.** Claude no cierra TASKs de hardware.

## NO tocado a propósito (scope DOWN / decisión de banco)

- **#1 `[env:down]` compila con 1 mux (8 sensores), no 32.** Es firmware de
  **competencia de DOWN** y tiene un comentario deliberado ("default 1 mux para
  placa 04-12 actual"). Depende del estado físico actual del board, que el
  agente/equipo DOWN conoce mejor → **lo dejo flagueado para DOWN**, no lo piso
  desde la sesión central. (Análisis FASE 1 #5.)
- **#2 `sample_age_ms` roto** (mide duración de tick, pegado en 255): el fix toca
  `src/down/comm_central.cpp` + `line_ring.{h,cpp}` (código DOWN) → **agente DOWN**.
- **#5 watchdog freshness 500 ms vs objetivo de freno 15 ms**: decisión de diseño
  (separar enlace-vivo de accionable-para-emergencia). Próxima sesión.
- **Contratos v1→v2** (`CONTRATO-DATOS-CENTRAL/DOWN.md` describen LineStatus v1):
  ~2 h de docs. Próxima sesión.

## Archivos tocados

`src/shared/line_view.h`, `src/central/comm_down.{h,cpp}`,
`src/central/world_model.{h,cpp}`, `src/central/main_central.cpp`,
`test/test_central_line_ingest/test_main.cpp`, +
`docs/firmware/ANALISIS-COMM-DOWN-CENTRAL-2026-05-31.md` (estado de implementación).
