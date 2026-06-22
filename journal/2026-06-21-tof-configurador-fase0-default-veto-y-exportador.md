---
title: "ToF — Fase 0 del configurador visual: veto default 'fila 2' + exportador de layout estático"
date: 2026-06-21
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
tipo: feature-monitor
toca-competencia: NO (solo tools/monitor-base, Python; no toca firmware)
status: lógica host-testeada (35 tests OK) · GUI sin probar (no hay display/hardware acá)
---

# Fase 0 del configurador de zonas ToF — primer incremento

Parte del plan acordado (ver `docs/firmware/TOF-ZONAS-TOP-MONITOR-ANALISIS.md`): empezar por una
herramienta VISUAL para definir ubicación/rotación de los 4 ToF y el veto de zonas, con la idea de
que lo ESTÁTICO (ubicación/rotación) se descubre mirando y se hornea FIJO en el firmware, y el VETO
queda dinámico.

## Qué se hizo (todo en `tools/monitor-base`, monitor Python)

Aprovecha la infra que YA existía (`tof_layout.py`/`gui_tof_setup.py`: grillas con números por zona,
controles de posición/rotación/flip, veto por click y por fila, save/load por serial vía
`config_path_for_serial`). Se agregó lo que faltaba:

1. **Veto DEFAULT "2ª fila desde abajo"** (`TofLayout.apply_default_veto`): deja válida SOLO la fila
   `grid_w-2` (en 4×4 = fila 2, zonas display 8..11) y anula las 2 filas de arriba (ven por encima de
   la pared) + la fila de abajo (choca el piso). Se pliega por la rotación de cada sensor con
   `raw_zone_mask`. Botón "⬛ Default: SÓLO 2ª fila desde abajo" en la GUI. Es el punto de arranque
   pedido por Gustavo (ajustable luego a mano).
2. **Exportador del layout ESTÁTICO** (`export_static_layout_header` + `collect_saved_layouts` +
   `flip_to_bits`): junta los `.json` guardados POR SERIAL y genera un header C++
   (`tof_static_layout.h`) con `{serial → bearing[4], rotation[4], flip[4]}` para HORNEARLO fijo en el
   firmware (Fase 1: el firmware leerá su serial y aplicará su entrada). El `bearing` usa la
   convención HARDWARE correcta (FRONT=0, BACK=180, RIGHT=270, LEFT=90) → NO propaga el bug der/izq.
   Botón "📤 Exportar header estático firmware" en la GUI.
3. **Warnings stale corregidos** en `gui_tof_setup`: el firmware YA aplica `ZONEMASK` (y soporta
   `ROT/FLIP`); el texto decía "pendiente de firmware". Ahora dice la verdad (la máscara SÍ baja; los
   `ROT/FLIP` no se mandan a propósito porque la rotación va plegada en la ZONEMASK en modo default).

## Verificación (host, sin hardware)
- `python -m pytest tests/test_tof_layout.py tests/test_tof_360.py` → **35 passed**. Tests nuevos:
  default veto deja viva solo la fila 2; raw mask = 0x0F00 (rot 0) y 0x00F0 (sensor a 180°); el
  exportador emite el bearing correcto sin el bug der/izq.
- `py_compile` de `gui_tof_setup.py` y `tof_layout.py` OK.
- ⚠️ La GUI NO se probó (no hay display/hardware en esta máquina): el cableado matchea los patrones
  existentes y compila, pero verla andar con la placa lo cierra el equipo.

## Qué sigue
- Banco: abrir el configurador con la TOP (env con telemetría despierta), confirmar que se ven los
  números CRUDOS, orientar cada ToF (girar hasta que "lo de adelante salga arriba"), aplicar el default,
  guardar (queda keyed por serial), exportar el header.
- Fase 1 (firmware, coordinar con el equipo): consumir `tof_static_layout.h` (lookup por serial) y
  sacar POS/ROT/FLIP del path vivo. Fases 2-4: echo de máscara, EEPROM A/B, reductor unificado.
