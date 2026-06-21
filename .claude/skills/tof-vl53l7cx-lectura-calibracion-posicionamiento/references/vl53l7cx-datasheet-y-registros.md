# VL53L7CX — datasheet, target_status, campos por zona y constantes del API

> Tabla de consulta. La verdad que MANDA es el datasheet ST `vl53l7cx.pdf` + UM3038 + el header
> del ULD API (`vl53l7cx_api.h`), NO la memoria. El VL53L7CX es driver-compatible con el VL53L5CX
> (mismo ULD, misma tabla de status); casi toda la doc de L5CX aplica — solo cambia el FoV.

## 1. Los 16 `target_status` (UM3038 Table 4)

| status | Significado | Validez |
|---|---|---|
| 0 | Ranging data not updated | inválido |
| 1 | Signal rate too low on SPAD array | <50% |
| 2 | Target phase | <50% |
| 3 | Sigma estimator too high | <50% |
| 4 | Target consistency failed | <50% |
| **5** | **Range valid** | **100%** |
| 6 | Wrap around not performed (típico PRIMER rango) | ~50% |
| 7 | Rate consistency failed | <50% |
| 8 | Signal rate too low for current target | <50% |
| **9** | Range valid with large pulse (puede ser target mergeado) | ~50% |
| 10 | Range valid but no target detected at previous range | bajo |
| 11 | Measurement consistency failed | <50% |
| 12 | Target blurred by another due to sharpener | <50% |
| 13 | Target detected but inconsistent data (frecuente en targets secundarios) | <50% |
| 255 | No target detected (solo si `nb_target_detected` habilitado) | n/a |

**Regla de validez ST:** `nb_target_detected > 0` **Y** `target_status ∈ {5, 6, 9}` → rango bueno.
Para **precisión** (paredes/trilateración): quedarse SOLO con **5** (y a lo sumo 9), descartar 6
(es típicamente el primer frame y mete ruido). Fuente: UM3038 "Results interpretation" +
`github.com/pimoroni/vl53l5cx-python` REFERENCE.md.

## 2. Campos de salida por zona (UM3038 Table 3) y qué apaga cada macro

Por defecto TODOS habilitados → estructura `VL53L7CX_ResultsData` ≈ **1360 bytes**. ST recomienda
mantener SIEMPRE `nb_target_detected` + `target_status`.

| Campo | Qué es | Macro para apagarlo |
|---|---|---|
| `distance_mm` | distancia por zona (mm) | (no se apaga) |
| `target_status` | validez por zona | (no apagar) |
| `nb_target_detected` | targets por zona (chequear PRIMERO) | (no apagar) |
| `signal_per_spad` | señal de retorno por SPAD | `VL53L7CX_DISABLE_SIGNAL_PER_SPAD` |
| `ambient_per_spad` | **luz ambiente por SPAD** (calidad/alcance) | `VL53L7CX_DISABLE_AMBIENT_PER_SPAD` |
| `nb_spads_enabled` | SPADs activos | `VL53L7CX_DISABLE_NB_SPADS_ENABLED` |
| `range_sigma_mm` | desvío estimado del rango | `VL53L7CX_DISABLE_RANGE_SIGMA_MM` |
| `reflectance` | reflectancia % del target | `VL53L7CX_DISABLE_REFLECTANCE_PERCENT` |
| `motion_indicator` | indicador de movimiento | `VL53L7CX_DISABLE_MOTION_INDICATOR` |

Apagar los 6 (lo que hace el robot, `platformio.ini:620-622`) baja la estructura a ~648 B y ~5×
menos transferencia I2C → clave para el round-robin. ⚠️ Si apagás `AMBIENT_PER_SPAD` perdés la
señal de calidad por luz (trade-off del recorte).

## 3. Resolución, frecuencia, FoV, alcance, precisión

| Parámetro | Valor |
|---|---|
| 4x4 | 16 zonas (`VL53L7CX_RESOLUTION_4X4 = 16`), hasta **60 Hz** |
| 8x8 | 64 zonas (`= 64`), hasta **15 Hz** (compone 4 integration times) |
| Default chip | 4x4 @ **1 Hz**, modo AUTÓNOMO |
| Orden de config | `set_resolution()` **ANTES** de `set_ranging_frequency_hz()` (el máx depende de la resolución) |
| **FoV** | **90° DIAGONAL** (60×60 cuadrado). ⚠️ El VL53L5CX es 65° diag — no confundir |
| Alcance | ~2 a 350 cm; cae de ~3 m (oscuridad) a ~0,5 m con 5 klux (zonas internas, blanco 88%) |
| Precisión datasheet | 4x4 ±7-9 mm / 8x8 ±10-11 mm (20-200 mm) — NO es la precisión en cancha con luz+rival |
| Consumo | ~50 mA activo (igual 4x4 y 8x8: el costo del 8x8 es TIEMPO, no corriente) |
| Bus | I2C hasta 1 MHz (Fast mode plus); dir 8-bit 0x52 |

## 4. Constantes del API (ULD) — útiles para no adivinar

| Constante | Valor |
|---|---|
| `VL53L7CX_RESOLUTION_4X4` / `_8X8` | 16 / 64 |
| `VL53L7CX_RANGING_MODE_CONTINUOUS` / `_AUTONOMOUS` | 1 / 3 |
| `VL53L7CX_DEFAULT_I2C_ADDRESS` | 0x52 (8-bit) |
| `VL53L7CX_POWER_MODE_SLEEP` / `_WAKEUP` | 0 / 1 |
| `VL53L7CX_STATUS_OK` | 0 |
| `VL53L7CX_OFFSET_BUFFER_SIZE` | 488 bytes |
| `VL53L7CX_XTALK_BUFFER_SIZE` | 776 bytes |
| `VL53L7CX_NVM_DATA_SIZE` | 492 bytes |
| `VL53L7CX_DEFAULT_RANGING_FREQUENCY_HZ` | 1 |
| Firmware blob | ~84 KB (cargado a RAM del módulo en `init()`/`begin()`) |

## 5. Inconsistencias de FUENTES (marcar, no homogeneizar)

- **Sharpener:** el datasheet cita **14%** al describir el FoV; UM3038 + el API
  (`set_sharpener_percent`) dicen **5%**. **La fuente que MANDA = el driver/UM3038: el default real
  es 5%.** No copiar el 14% del datasheet como default del API.
- **Tamaño del firmware:** ST dice **~84 kbytes**; el comentario del robot (`sensors_tof.cpp:392-393`)
  dice **~85 KB**. Diferencia menor — no propagar el número del comentario como dato de ST.
- **Boot time en ms:** NO hay cifra oficial única (carga de ~84 KB por I2C + rutina de boot + esperas
  de 10 ms en el reset). Marcar SIN CONFIRMAR; lo medido en el robot es ~9,6 s (4 ToF a 1 MHz).
- **VL53L7CX (90° diag) vs VL53L5CX (65° diag):** mismo ULD/driver/tabla de status; solo cambia el
  FoV. El supuesto "~60-65° diag" para el L7CX es del L5CX — confundirlos mal-dimensiona la cobertura.

**Fuentes:** datasheet ST `vl53l7cx.pdf`; UM3038 ("A guide to using the VL53L7CX", mirror Pololu);
header ULD `vl53l7cx_api.h`; `github.com/pimoroni/vl53l5cx-python` REFERENCE.md; ST community
(community.st.com) para clock stretching y status 0.
