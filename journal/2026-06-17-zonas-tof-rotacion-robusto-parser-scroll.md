---
title: "Zonas ToF: rotación firmware-dueño + reductor robusto + fix parser ZONE + scroll del monitor (todo tras flag, off=byte-idéntico)"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: código aplicado + host-tested + compila; cierre = banco (equipo, regla #1)
tipo: implementacion
---

# Resumen

Pedido de Gustavo: que el robot deje de promediar lecturas de ToF con la mitad de las zonas
mirando fuera de la cancha. Camino acordado: trabajar los ToF por zonas → orientarlas (rotación)
→ vetar las que salen de la cancha → recién ahí localizar. Se implementó el procesamiento de
zonas en el firmware del TOP (la rotación pasa a ser responsabilidad del firmware, no de la app),
un reductor robusto independiente, el fix de un bug del parser, y scroll en el monitor. Todo
**detrás de un flag, apagado por defecto** → el binario de competencia (`top_robot2_pri`) es
byte-idéntico salvo el fix del parser (ver abajo). **Nada validado en banco (regla #1): lo cierra
el equipo.**

# Qué se hizo (host-tested)

1. **Rotación de zonas firmware-dueño** — módulo puro `src/shared/tof_zone_mask_orient.h`
   (host 7/0) que rota/espeja la máscara de zonas de marco CANÓNICO (el del display de la app) a
   CRUDO del sensor, con la **misma convención que la app** (`tof_layout.zone_source_map`; coinciden
   en 0/180, y para 90/270 hay que iterar las crudas — se corrigió un bug de dirección que habría
   puesto el veto en zonas equivocadas). Se cablea en `sensors_tof.cpp` (los 2 caminos) tras
   `-DTOP_ENABLE_TOF_ROT`. Comandos nuevos `TOF n ROT 0|90|180|270` y `TOF n FLIP NONE|H|V`
   (enum + parser + handlers en `top_telemetry_serial.cpp`) → guardan en `g_top_cfg`, persiste
   con `CFG SAVE` (la serialización rot/flip ya existía).
2. **Reductor robusto** — `tof_zone_masked_robust` en `src/shared/tof_zone_mask.h` (host 8/0):
   antes de promediar las zonas vetadas, descarta (a) las > dimensión de cancha (oblicuas / sin
   retorno / ven afuera) y (b) los outliers bajos < 70% de la mediana (rebote en otro robot).
   Cableado tras `-DTOP_ENABLE_TOF_ROBUST`. Independiente de la rotación (no necesita tocar la app).
   Límite conocido y documentado en test: si un robot DOMINA el FOV no recupera la pared.
3. **Fix parser (bug latente)** — `TT_TOK_MAX` 4→5 en `telemetry_top.cpp`: `TOF n ZONE ON|OFF <idx>`
   son 5 tokens y el tokenizer cortaba en 4 → la rama era inalcanzable (caía a UNKNOWN). Ahora
   funciona. `test_telemetry_top` 23/0. ⚠️ Esto cambia el binario de competencia, pero SOLO en el
   parser de comandos del monitor (dormido en partido); la conducta de cancha es idéntica.
4. **App `tof_layout.py`** — constante `FIRMWARE_OWNS_ROTATION` (default False = comportamiento
   histórico, la app pliega la rotación en la máscara cruda; True = manda `TOF n ROT/FLIP` reales +
   la máscara en marco canónico, para placas con `-DTOP_ENABLE_TOF_ROT`). Es el único acople
   app↔firmware (van juntos). pytest `test_tof_layout` 18/0.
5. **Scroll del monitor** — `panel.py`: el área de cada panel ahora vive en un canvas con barra
   vertical + rueda del mouse (relleno cuando el contenido entra). Cubre los ~20 paneles de una
   (los botones de Config ToF ya no se cortan). Smoke de paneles 18/0.

# Envs nuevos (banco; los 3 compilan SUCCESS)

- `top_robot2_pri_tofrobust` — solo el reductor robusto (anda sin tocar la app).
- `top_robot2_pri_tofrot` — solo la rotación firmware-dueño (requiere app en FIRMWARE_OWNS_ROTATION).
- `top_robot2_pri_zonas` — los dos juntos (programa de competencia completo + zonas).

Para R2 ARQUERO: TOP=`top_robot2_pri_zonas`, CENTRAL=`central_robot2_arquero` (sin cambios),
DOWN=`down_robot2_rt`. La rotación/robusto es de la placa **TOP**; el arquero es de la CENTRAL.

# Verificación

- Host: `tof_zone_mask_orient` 7/0, `tof_zone_robust` 8/0, `telemetry_top` 23/0; gate host completo
  exit 0. App: `test_tof_layout` 18/0, smoke de paneles 18/0. (1 fail aislado del suite del monitor
  = `init.tcl` de Tk mal instalado, no regresión; pasa solo.)
- Firmware: `top_robot2_pri_tofrot` / `_tofrobust` / `_zonas` compilan SUCCESS.
- "Compila/pasa host" NO prueba el efecto real: la validación en cancha la cierra el equipo (regla #1).

# Diseño completo

`docs/superpowers/specs/2026-06-17-localizacion-tof-pose-xy-design.md` (el plan de pose XY por fases;
esto implementa la parte de zonas/orientación/veto).

# Pendiente equipo (banco)

Flashear los envs en R2, calibrar rotación + veto de zonas fuera de cancha (app en
FIRMWARE_OWNS_ROTATION=True + CFG SAVE; o CFG RESET si había veto viejo plegado), y confirmar en el
monitor que las distancias se limpian. Tunear `TOF_ROBUST_FIELD_MAX_MM` (2430) / `TOF_ROBUST_LOW_KEEP_PCT`
(70) según lo que se vea. Escape: `top_robot2_pri` (TOP de competencia sin zonas).
