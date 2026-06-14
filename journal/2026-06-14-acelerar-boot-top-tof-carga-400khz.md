# 2026-06-14 — Acelerar el boot del TOP: carga de los ToF a 400 kHz (TA-1)

**Autor:** Claude Opus 4.8 (coach), a pedido de Gustavo.
**Tipo:** análisis + cambio de firmware (host: compila; falta banco).
**TASK:** [TASK-210](../team-tasks/2026-06-14-task-210-acelerar-boot-tof-carga-400khz.md).

## Qué se pidió
Analizar a fondo por qué el encendido del robot tarda tanto ("configurando TOF y BNO")
y proponer/aplicar la mejor forma de acelerarlo sin perder confiabilidad.

## Diagnóstico — dónde se van los ~40 s del boot del TOP
| Fase | Tiempo aprox. |
|---|---|
| Carga firmware **4× VL53L7CX** a **100 kHz** (`begin()`, ~85 KB/sensor) | **~30-32 s** ⬅️ el monstruo |
| Init BNO primario (STABILIZE 1 s + GYRO_CALIB ≤2 s + captura cero) | ~1,5-3 s |
| Settle LP (`LP_SETTLE_MS` 120 ms ×4 + recover) | ~0,7 s |

El bus está a 100 kHz por la **coexistencia BNO+ToF en RUNTIME** (a 400 kHz, leer el BNO
multi-byte mientras los ToF rangean congela el yaw — banco 2026-06-02/06-08). **Pero la
carga del firmware NO es runtime:** ocurre en `setup()`, con el BNO iniciado y sin que
nadie lo lea. Es la condición "ToF-solo", que **ya está validada a 400 kHz** en este
hardware: `diag_top_tof_quad_live.cpp` carga los 4 ToF a 400 kHz (banco 2026-05-30).

## Qué se hizo (TA-1)
`src/top/sensors_tof.cpp`:
- Constantes `TOF_INIT_CLOCK_HZ=400000` (carga) y `TOF_RUN_CLOCK_HZ=100000` (runtime).
- `begin()` de los ToF (path MULTI y single) cargan a 400 kHz.
- **`Wire.setClock(100000)` OBLIGATORIO al final del init** (todos los return) → el runtime
  vuelve a 100 kHz antes de que arranque el loop. Sin esto se reintroduce el freeze.
- Nota stale ("NO tocamos setClock") actualizada.

`src/top/main_top.cpp`:
- Instrumentación de boot: imprime `[boot] imu_init=.. tof_init=.. setup_total=.. ms`.

**Impacto esperado:** tof_init ~30-32 s → ~8 s ; boot total ~40 s → ~12-13 s. El runtime
queda igual (100 kHz) → comportamiento anti-freeze del loop sin cambios.

## Verificación
- ✅ `pio run -e top_robot2_pri` → **SUCCESS** (40 s; FLASH code 74920, sin errores).
- ⏳ Banco PENDIENTE (regla no negociable: el equipo cierra las TASK de HW). Criterio en TASK-210:
  velocidad (tof_init ≤ ~10 s) + sin regresión de heading (girar robot → hdg trackea) + 4 ToF OK.

## Futuro (no en esta sesión)
TA-2 carga a 1 MHz (~3-4 s, requiere validar pull-ups + fallback a 400 kHz) · TA-3 solapar
estabilización del BNO (Wire2) con la carga de ToF (Wire) · TA-4 titular STABILIZE/LP_SETTLE.
Causa raíz a 2027: separar buses ToF/BNO para correr todo a 400 kHz-1 MHz también en runtime.
