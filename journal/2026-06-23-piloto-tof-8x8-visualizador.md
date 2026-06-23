---
title: "Piloto ToF 8×8 — los 4 ToF a 64 zonas (gateado) + visualizador de 4 grillas 8×8"
date: 2026-06-23
status: vivo (piloto; valida LECTURA 8×8, NO localización todavía)
placa: TOP (ROBOT2)
env: top_robot2_pri_tof8x8 (banco) — competencia top_robot2_pri INTACTA
autor: "Claude Opus 4.8 (1M context), vía Claude Code — pedido Virginia"
analisis: "2 workflows paralelos: diagnóstico+plan (6 agentes) y desarrollo+integración (3 agentes), + revisión de la sesión principal"
testeado-en-hardware: NO (compila + host-tests; lo cierra el equipo → TASK-226)
---

# Piloto: leer los 4 ToF en 8×8 y verlo, para decidir si sirve para posicionamiento

## Por qué (banco Virginia + Gustavo)

Los ToF montados a 160 mm leen el PISO/cielo en 4×4: desde el centro de la cancha las paredes
(0,9-1,2 m) caen en una **ventana angular ≤16°** y las 4 filas de 15° no calzan (hallazgo
`2026-06-22-hallazgo-tof-leen-piso-no-pared.md`, datos corregidos: ToF 160 mm < pared 220-260 mm).
Con **8×8 (8 filas de 7,5°)** hay el doble de resolución vertical → más chance de que una fila pegue
en la pared. Virginia quiere usar los ToF para **localizar el robot**. Antes de la fase completa
(~6-10 días), un PILOTO valida que 8×8 **se lee** sin romper el loop ni el BNO.

## Qué hace el piloto (gateado, competencia byte-idéntica)

**Firmware** (`src/top/sensors_tof.cpp`, TODO bajo `#ifdef TOP_TOF_8X8`):
- `TOF_EFFECTIVE_ZONES` = 64 con el flag / 16 sin → buffer `g_zones_mm[NUM_TOF][64]`
  (**anti-overflow obligatorio**), loops e índices usan la constante.
- Los **4 ToF a 8×8** (`TOF_RESOLUTION_ZONES=64`), ranging a `~8 Hz` (8×8 no sostiene 15;
  `setResolution` ANTES de `setRangingFrequency`).
- **Emite el debug `ZN8`** (1 línea de texto por tick, el ToF del round-robin): las 64 zonas crudas
  de `g_tof_results.distance_mm[]` (65535 si `target_status ∉ {5,6,9}`) **+ el `dt_us`** del
  `getRangingData` de ese ToF — el costo del bloque 8×8 en el loop.

**Contrato `ZN8,<idx>,<res>,<dt_us>,<v0..v63>`** (texto, no empieza con `{` → invisible al protocolo
binario; idx 0..3, res 64, mm orden fila*8+col, 65535=sin lectura).

**Monitor** (`tools/monitor-base/`): vista nueva **`--tof8x8`** (`gui_tof8x8.py`) — 4 grillas 8×8
(FRONT/BACK/RIGHT/LEFT) coloreadas por distancia + el **`dt_us` y los fps por sensor** (para ver si el
loop aguanta los 4 a 8×8). Lógica pura testeada (`tests/test_tof8x8.py`, 15 tests). Solo lectura.

**Env** `[env:top_robot2_pri_tof8x8]` = `top_robot2_pri` + `-DTOP_TOF_8X8`.

## Lo que el piloto NO toca

El contrato `z` vivo (sigue 16 zonas), el golden, la EEPROM, la máscara, la localización, los envs de
competencia. **Verificado:** `top_robot2_pri` recompilado es **byte-idéntico** (md5 del `firmware.hex`
igual antes/después; FLASH code 79560 sin cambio). El piloto suma +192 B solo con el flag.

## ⚠️ Lo que el piloto VIENE A MEDIR (no asumir)

1. **Impacto en el loop:** 4 ToF a 8×8 = `getRangingData` ~4× más largo. El round-robin (1 ToF/tick,
   30 ms) puede "latir". Por eso se emite `dt_us` por sensor — **mirar el PEOR caso, no el promedio**.
2. **Freeze del BNO:** la ventana I²C a 400 kHz se alarga a 8×8 → riesgo de congelar el yaw.
   **Criterio bloqueante:** girar el robot ≥60 s con los ToF a 8×8 y confirmar que el heading se mueve.

## Verificación (host)

- `pio run -e top_robot2_pri_tof8x8` SUCCESS + `top_robot2_pri` SUCCESS y **byte-idéntico** (md5).
- `python -m monitor_base --tof8x8 --selftest` rc=0; `pytest` 350 passed (15 nuevos), sin regresiones.
- NO testeado en hardware (regla #1) → **TASK-226**.

## Plan de banco → TASK-226 (lo cierra el equipo)

1. Flashear `top_robot2_pri_tof8x8` (TOP). Abrir `python -m monitor_base --tof8x8` (o `pio device
   monitor -b 115200` para ver crudo).
2. ¿Salen líneas `ZN8` con 64 valores, idx rotando 0..3? ¿Las 4 grillas se pueblan?
3. **Resolución vertical:** objeto a distancia conocida tapando la mitad inferior del FOV → ver qué
   FILA de las 8 pega (mm reales) y que **se desplaza al subir el objeto** (lo que 4×4 no da).
4. **Loop:** mirar `dt_us`/fps — ¿el peor caso es tolerable con los 4 a 8×8? Si no, bajar a menos
   sensores (el código no cambia) o bajar `TOF_RANGING_FREQ_HZ`.
5. **BNO:** girar 60 s, yaw vivo sin congelarse (bloqueante).
6. Si pasa → recién ahí la fase de **selección 2-por-columna + pose en TOP** (idea de Virginia, no
   rompe el contrato `z`). Si no pasa, no se justifica.

## Comando de flasheo

```
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"; pio run -e top_robot2_pri_tof8x8 -t upload
```
Volver a competencia: `pio run -e top_robot2_pri -t upload`.
