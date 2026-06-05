# `_deprecated/` — diags CUMPLIDOS u OBSOLETOS (no flashear)

> ⚠️ **Estos sketches ya NO tienen `[env]` en `platformio.ini` — no se pueden compilar ni flashear.**
> Son diagnósticos que **ya cumplieron su función** (la pregunta que respondían está resuelta y *baked*
> en el firmware vivo) o que asumen **hardware obsoleto** (cableado/arquitectura que ya no existe).
> Se conservan solo como **historia**. Para cada uno hay un reemplazo VIGENTE indicado abajo.

## ToF (placa TOP) — bring-up cumplido

| Archivo | Qué hacía | Por qué está cerrado |
|---|---|---|
| `diag_top_tof.cpp` | Probar el ToF frontal con la lib ST VL53L7CX | La lib ST fallaba (err=255); se migró a **Adafruit_VL53L7CX** (vivo en `src/top/sensors_tof.cpp`) |
| `diag_top_tof_as_l5cx.cpp` | ¿El chip es un VL53L5CX? | Respondido: **NO** |
| `diag_top_tof_as_l8cx.cpp` | ¿Es un VL53L8CX? | Respondido: **NO** (es L7CX) |
| `diag_top_tof_adafruit.cpp` | Test de control con la lib Adafruit | **Ganó** → promovido a `sensors_tof.cpp` |
| `diag_top_tof_lp_discover.cpp` | Descubrir a qué pines cayó el bodge LP | Resuelto: **LP en pines 9/10/11/12** |
| `diag_top_tof_enumerate.cpp` | Enumerar los 4 ToF (hipótesis LP con pin 22) | Hipótesis errónea; superado por `_census` |
| `diag_top_tof_census.cpp` | Censo definitivo de los 4 ToF + control LP | **Resolvió la incógnita** (0x2A–0x2D, LP 9/10/11/12) |

**Re-diagnosticar los 4 ToF en banco hoy** → usá **`diag_top_tof_quad_live`** (VIGENTE, con `[env]`),
que lee los 4 ToF a la vez con la lib y el pinout actuales.

## BNO055 (placa TOP) — arquitectura I²C obsoleta

| Archivo | Qué hacía | Por qué está cerrado |
|---|---|---|
| `diag_top_bno.cpp` | Verificar 2 BNO055 (LEFT en `Wire`, RIGHT en `Wire1` REMAP pines 24/25) + sentido de giro | Arquitectura vieja: tras el recableado (2026-05-31) **ambos BNO viven en `Wire`** y el derecho (0x29) es la **unidad FALLADA** (el robot corre con 1 solo BNO). El dual-bus 24/25 ya no existe. |

**Verificar los BNO en banco hoy** → **`diag_bno_dual_live`** (fusión de los 2, degrada a 1) o
**`diag_bno_left`** (solo el 0x28 vivo, incluye sentido de giro / signo). Ambos VIGENTES, con `[env]`.

> El firmware vivo (`src/top/sensors_tof.cpp`, `src/top/sensors_imu.cpp`) usa el pinout y los buses
> actuales. Ver `docs/FUENTES-DE-VERDAD.md`.
