# 2026-06-14 — Monitor de SALUD de la placa TOP + zonas de ToF en la telemetría

> Sesión Claude (Opus 4.8), **modo autónomo** (Gustavo durmiendo 1 h, pedido de
> "avanzá con tu mejor criterio"). Frame: coach soccer. Atribución: diseño +
> programación host-testeable por Claude; **validación en hardware = del equipo**
> (regla 1 CLAUDE.md — NO marco nada como "funciona en placa").

## Qué se pidió

Una app Python para **monitorear la placa TOP**: que al conectarse le diga al
firmware que mande datos, y que sea **operativa** — ver qué sensor anda o no.
Además, Gustavo pidió puntualmente las **zonas de los ToF** y verificar que no
duplicáramos esfuerzo con lo ya hecho por otro agente.

## Análisis previo (no repetir — verificado en código, no en docs)

Un workflow multi-agente (4 discovery + 5 verificaciones adversariales) + chequeos
directos confirmaron:

- **El monitor TOP ya existe** (`tools/monitor-base/`): firmware emisor v2
  (`telemetry_top.{h,cpp}`, 9 bloques JSON Lines), handshake DORMIDO→`STREAM ON`/
  `PING`/auto-off 3 s (validado en placa, commit `152c0f9`), transporte serial
  (`sources.py`), parser v2 (`protocol_top.py`), simulador y golden. **El monitor
  viaja DORMIDO en el binario de competencia `top_robot2_pri`** (`-DTOP_USB_MONITOR`)
  → no hay env de banco aparte. Por eso NO se reescribió nada de eso.
- **Las zonas de los ToF NO viajaban en el stream.** El firmware lee 16 zonas (4×4)
  y las **promedia** a 1 distancia por sensor (`mean_valid_zones`, `sensors_tof.cpp`).
  El enmascarado/rotación (A2.2) está serializado en EEPROM pero **no aplicado**.
  → exponerlas era trabajo nuevo de firmware (lo de esta sesión).

## Qué se hizo

### App PC (vista nueva, reusa el core) — commit 1
`python -m monitor_base --top-salud [--sim | --port COMx | --selftest]`

- `health.py` (PURO): veredicto por sensor desde un `TopFrame`
  (OK / REVISAR / FALLA / SIN DATO + motivo). Cubre cámaras (watchdog + **pelota
  fantasma** por delta front↔back), BNO L/R + rumbo (valid/disagree), ToF + US
  (sentinela), OTOS (frescura + slip), línea (MUX_DEAD/INVALID/NOISY), snapshot.
  **16 tests.**
- `zones.py` (PURO): `ZoneGrid` (4×4) + `zone_color` (heatmap). **4 tests.**
- `gui_top_health.py`: tablero Tkinter — semáforo por sensor + grilla de zonas por
  ToF + per-cámara + OTOS/escape (con compuertas de frescura) + **botones de config**
  (CAM/BNO/US/TOF on/off + POS + CFG SAVE/LOAD/RESET, grammar verificada).
- `protocol_top.py`: campo `zones` **ADITIVO** opcional (schema sigue 2; frame sin
  `"z"` → `zones=None`; no rompe el contrato ni al monitor viejo).
- `simulator_top.py`: emite `z` para desarrollar sin robot.
- `__main__.py`: flag `--top-salud` + `run_selftest_top_salud` (smoke headless).

### Firmware: exponer zonas en la telemetría (campo `z`) — commit 2
Decisión clave de ingeniería: **`z` ADITIVO, NO wire-breaking** (el workflow había
asumido bump v2→v3). Un parser viejo ignora `"z"`; no hay reflasheo coordinado.

- `telemetry_top.{h,cpp}`: campo `tof_zones[TT_MAX_TOF][16]` + serialización
  `"z":[[...16...],...]` en el bloque `tof`. Buffer del glue 1536→2048.
- `sensors_tof.{h,cpp}`: buffer `g_zones_mm[NUM_TOF][16]` + `fill_zones()` (hermano
  de `mean_valid_zones`, sin promediar) + getter `sensors_tof_get_zone_mm()` gateado
  por la misma frescura/habilitación que la distancia. **Resolución 4×4=16** (la de
  producción; el 8×8=64 sigue siendo solo del diag de banco → no arriesga el loop).
- `top_telemetry_serial.cpp`: llena `f.tof_zones` por sensor.
- Golden regenerado en C++ (`test_main.cpp`) **y** Python (`golden_top_v2.jsonl`)
  con un script (evita contar 16 elementos a mano).

## Verificación (lo que SÍ probé)

- **Python: 108 → 116 tests verdes** (+34 sobre el baseline 82). Incluye smoke real
  end-to-end de la GUI contra el simulador (28 frames, 0 perdidos, cierre limpio).
- **Firmware host-test `test_telemetry_top`: 20/20** — el serializador produce
  EXACTAMENTE el golden con zonas (igualdad de string).
- **`pio run -e top_robot2_pri`: SUCCESS** (RAM/FLASH holgados; +192 B en el frame).
- **Suite host-native completa**: [resultado en el commit — ver gate].

## Lo que NO está hecho (gates del equipo)

- ⚠️ **Validación en placa real**: `pio SUCCESS` / golden ≠ dato real por USB. El
  flujo TOP↔app y que las zonas reales aparezcan en el stream lo cierra el equipo
  → **TASK-209**.
- **A2.2 (que el robot IGNORE zonas mascaradas)**: NO implementado. Hoy las zonas
  son de SOLO LECTURA (ver). El enmascarado que cambia la navegación es otra tanda,
  bench-gated.
- 8×8=64 zonas en producción: diferido (arriesga el loop 100 Hz / I²C).

## Notas para el que sigue

- La app degrada con gracia: si un firmware viejo (sin `z`) está flasheado, la
  grilla muestra "pendiente firmware" y el resto del tablero anda igual.
- `gui_top.py` (el radar viejo) NO se tocó (carril de TASK-208 / otro agente). Esta
  vista es nueva y separada (`gui_top_health.py`).
- Inconsistencia ya conocida (no la arreglé acá): `docs/firmware/USO-MONITOREO-Y-
  TELEMETRIA.md` nombra envs viejos para monitorear la TOP; el real es `top_robot2_pri`.
