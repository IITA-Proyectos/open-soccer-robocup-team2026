# VL53L7CX — mapa archivo:línea de los módulos del robot IITA

> Para que el SKILL.md no repita código inline. Los anchors marcados ✅ los verifiqué leyendo el
> archivo en esta sesión; los marcados ⚠️ vienen del agente de investigación y conviene re-confirmar
> la línea exacta si el archivo cambió. La VERDAD MANDA = el código vivo.

## `src/top/sensors_tof.cpp` (629 líneas) — el driver vivo (4 ToF + HC-SR04) ✅

| Líneas | Qué |
|---|---|
| 9-21 | nota de migración: por qué Adafruit_VL53L7CX y no la lib de ST (buffer overflow) |
| 60-61, 593-600 | zonas crudas 4x4 (`g_zones_mm[NUM_TOF][16]`) + getter `sensors_tof_get_zone_mm` (campo `z` de telemetría) |
| 148-152 | `TOF_RESOLUTION_ZONES = 16` (4x4) + `TOF_RANGING_FREQ_HZ = 15` |
| 154-173 | los TRES clocks: `TOF_INIT_CLOCK_HZ=400k` (`:166`), `TOF_RUN_CLOCK_HZ=100k` (`:167`), `TOF_INIT_CLOCK_FAST_HZ=1M` (`:173`) |
| 204-235 | `mean_valid_zones` + `fill_zones`: filtro `status==5\|\|6\|\|9` (permisivo) |
| 239-255 | `sensors_tof_predim_lp`: dormir ToF antes de iniciar el BNO |
| 316-352 | enumeración 4 ToF por LP {9,10,11,12} → 0x2A..0x2D; carga 1 MHz con fallback 400 kHz reseteando por LP |
| 324-328 | `TOF_ONLY_INDEX`: bisección — init/rangea SOLO un ToF |
| 353-355 | `setResolution(16)` + `setRangingFrequency(15)` |
| 356-362 | `TOP_ENABLE_TOF_CONTINUOUS`: modo continuo (default OFF, TASK-219) |
| 363-369 | `TOP_TOF_NO_RANGE`: enumera SIN `startRanging()` (sin VCSEL, TASK-223) |
| 377-380 | **restore a 100 kHz OBLIGATORIO** antes del loop |
| 432-499 | `sensors_tof_tick`: round-robin 1 ToF/tick (`s_rr` en `:459-463`); aplica máscara/robust/orient |
| 477-496 | `tof_zone_masked_mean` vs `tof_zone_masked_robust` (flags `TOP_ENABLE_TOF_ROBUST`/`_ROT`) |
| 583-621 | getters: `get_distance_mm` (frescura), `get_zone_mm`, `get_min_distance_mm` (min 4 ToF + HC-SR04) |

`src/top/sensors_tof.h` — sentinel `TOF_NO_READING=0xFFFF`, `TOF_MAX_RANGE_MM`,
`TOF_STALE_TIMEOUT_MS=250`, `tof_fresh_or_no_reading` (pura, wrap-safe). ⚠️ (referido por el agente
como `:29-72`; el comportamiento lo verifiqué en su uso en `sensors_tof.cpp:588,610`).

## `platformio.ini` — flags de build ✅ (parcial)

| Líneas | Qué |
|---|---|
| 620-622 | bloque compartido `-DVL53L7CX_DISABLE_*` (6 macros: ambient/nb_spads/signal/range_sigma/reflectance/motion) |
| 1552 (comentario) / 1557, 1561 | envs `top_robot1_pri` / `top_robot2_pri` repiten los `DISABLE_*` + `TOP_ENABLE_MULTI_TOF` + `TOP_ENABLE_HCSR04` |

## `src/shared/` — módulos PUROS host-testeables ✅

| Archivo | Líneas | Qué |
|---|---|---|
| `localization.h` | 20-28, 35 | convención ToF [0]frente/[1]atrás/[2]der/[3]izq; `tof_mount_angle_deg={0,180,270,90}` |
| `localization.h` | 54 | `heading_centideg` firmado [-18000,18000], NO consumido hoy |
| `localization.cpp` | 75-79 | `center_perp_distance_mm` (F1a radio + F1b coseno LUT Q12) |
| `localization.cpp` | 96-106 | `classify_wall` (heading+mount → N/S/E/W por cuadrantes) |
| `localization.cpp` | 163-218 | `reject_outliers` (rival tapando pared = lectura corta espuria) |
| `tof_zone_mask.h` | 51-66 | `tof_zone_masked_mean` (mask=~0 → byte-idéntico) |
| `tof_zone_mask.h` | 88-124 | `tof_zone_masked_robust` (descarta > cancha y < % de la MEDIANA) |
| `tof_zone_mask_orient.h` | 35-73 | rotación/flip de la máscara (GW=4, `TOP_ENABLE_TOF_ROT`) |
| `tof_schedule.h` | 78, 104 | `tof_sched_set_ready`, `tof_sched_next` (round-robin saltea el caído) |

## `src/top/localization_runtime.cpp` — glue I/O ✅ (flag verificado)

| Líneas | Qué |
|---|---|
| 24, 36-85 | `TOP_KEEPER_XY_WALLS`: estimador XY HEADING-FREE del arquero (pose por paredes, mediana+recorte) |

## Inconsistencias doc-vs-código (verificadas contra ambos lados) ✅

| Doc | Dice | Realidad (código) |
|---|---|---|
| `CONTRATO-DATOS-TOP.md:457,493,523,553` | "4 ToF I2C completamente stub", `min_obstacle=0xFFFF` siempre, ref `sensors_tof.cpp:51-67` | driver Adafruit real desde 2026-05-24; archivo de 629 líneas; round-robin + zonas reales |
| `CONVENCION-EJES-ROBOT.md:153,158` | grilla "8×8", corrección `(7-fila,7-col)` | 4x4=16 (`TOF_RESOLUTION_ZONES=16`) → corrección `(3-fila,3-col)`, GW=4 |

## TASKs abiertas relevantes

- **TASK-203** — orientación interna de zonas (`diag_top_tof_zonemap`).
- **TASK-219** — banco del modo CONTINUO.
- **TASK-223** — aislar si un freeze residual del BNO es por el rangeo (`TOP_TOF_NO_RANGE`).
- **TASK-210 / 211** — boot a 400 kHz / 1 MHz (implementadas en código; cierre de banco por el equipo).
- **TASK-035** — validar la trilateración en cancha (precisión ±2-3 cm SIN confirmar).
- **TASK-221** — validar `keeper_xy_walls` en banco.

## Marcar SIN CONFIRMAR (no tratar como hecho)

- Precisión "±2-3 cm" de la trilateración (`localization.h:7`) = diseño, NO medida en cancha.
- Boot time exacto en ms (no hay cifra oficial ST; ~9,6 s medido en el robot a 1 MHz).
- `TOF_TICK_INTERVAL_MS=30` (citado en `sensors_tof.cpp:447`) — el intervalo real lo fija
  `main_top.cpp`, no leído línea por línea en esta sesión.
