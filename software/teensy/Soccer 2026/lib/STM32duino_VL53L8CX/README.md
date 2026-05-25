# STM32duino_VL53L8CX — librería vendoreada

Driver Arduino oficial para el sensor ToF multizona **VL53L8CX** de
STMicroelectronics. Wrapper C++ sobre el ULD ("Ultra Lite Driver") C nativo
de ST. Soporta tanto **I²C** como **SPI** (este repo solo usa I²C).

## Origen

- **Upstream:** https://github.com/stm32duino/VL53L8CX
- **Commit vendoreado:** `389da0523c219fa21d9e5cb417801a16ef9407be`
- **Tag más cercano:** `2.0.4` (exact match)
- **License:** BSD-3-Clause (ver `LICENSE`)
- **Fecha de vendoreo:** 2026-05-24
- **Vendoreado por:** Claude Opus 4.7 (Anthropic) — Requested-by Gustavo Viollaz (@gviollaz)

## Por qué está vendoreada

Política del repo (`lib/README.md`): todas las dependencias de firmware se
vendorean para compilar 100% offline (Avast/SSL roto, Incheon sin wifi
garantizado). Igual que Adafruit_BNO055, Adafruit_BusIO, STM32duino_VL53L7CX,
STM32duino_VL53L5CX, etc.

## Quién la usa

- `[env:diag_top_tof_as_l8cx]` — sketch hermano de `diag_top_tof` y
  `diag_top_tof_as_l5cx` para identificar el chip desconocido soldado en U2
  de la placa TOP. El equipo compró VL53L5CX / L7CX / L8CX mezclados sin
  trazabilidad y los carriers Pololu son físicamente idénticos. **Tercer y
  último candidato del trío** — si init OK acá, el chip es un L8CX. Si
  también falla, los 3 modelos quedan descartados y el próximo paso es
  inspeccionar el chip con lupa o reemplazar el módulo.

## Compatibilidad de plataforma

`library.properties` declara `architectures=*` — más permisivo que L5CX/L7CX
(que declaran `stm32, sam`). En la práctica el wrapper sólo usa `Wire`,
`digitalWrite`, `pinMode`, `delay` y opcionalmente `SPI` — todas APIs
Arduino estándar disponibles en Teensy 4.0. La capa de plataforma
(`vl53l8cx_platform.{h,c}`) está implementada sobre `Wire.h` y es portable.

## API — diferencias importantes vs L5CX/L7CX

El wrapper L8CX expone los métodos del ULD **sin prefijo**, distinto de L5CX
y L7CX que usan prefijo `vl53l5cx_*` / `vl53l7cx_*`. Esta diferencia es
**upstream-side**, no es bug del vendoreo. Es decir:

| Acción                | L5CX / L7CX                          | L8CX (este)               |
|-----------------------|--------------------------------------|---------------------------|
| Init del firmware     | `init_sensor()`                      | `init()`                  |
| Set resolución        | `vl53l5cx_set_resolution(...)`       | `set_resolution(...)`     |
| Check data ready      | `vl53l5cx_check_data_ready(&r)`      | `check_data_ready(&r)`    |
| Get ranging data      | `vl53l5cx_get_ranging_data(&res)`    | `get_ranging_data(&res)`  |
| Start ranging         | `vl53l5cx_start_ranging()`           | `start_ranging()`         |

Ver `src/vl53l8cx.h:58-83` para la lista completa de métodos del wrapper.

Constantes: `VL53L8CX_RESOLUTION_4X4` (=16), `VL53L8CX_RESOLUTION_8X8` (=64),
`VL53L8CX_RANGING_MODE_CONTINUOUS` (=1) — definidos en `src/vl53l8cx_api.h`.

Struct resultado: `VL53L8CX_ResultsData` con `distance_mm[]` (int16) y
`target_status[]` (uint8) indexados row-major top-left, igual que L5CX/L7CX.

## Constructor

Dos constructores en `src/vl53l8cx.h:55-56`:
```cpp
VL53L8CX(TwoWire *i2c, int lpn_pin, int i2c_rst_pin = -1);                 // I²C
VL53L8CX(SPIClass *spi, int cs_pin, int lpn_pin = -1, int i2c_rst_pin = -1,
         uint32_t spi_speed = 5000000);                                     // SPI
```

Este repo usa SOLO I²C (el módulo Pololu del bot tiene cableado I²C; SPI no
está ruteado). Pasar `lpn_pin = -1` para skip XSHUT — ya validamos que el
toggle no estaba ruteado al Teensy.

## Contenido podado

Se removieron `examples/`, `.git/`, `.github/`, `keywords.txt` y el
`README.md` del upstream (siguiendo `lib/README.md`). Se conservaron: `src/`,
`library.properties`, `LICENSE`.

Nota: este upstream **no tiene `library.json`** (solo `library.properties`
con metadata Arduino IDE). PlatformIO lo lee igual.

Nota: `src/vl53l8cx_buffers.h` pesa ~594 KB — es el firmware blob del sensor
(se carga por I²C al `init()`). Es esperable; no es bloat. Este blob es
**distinto** del de L5CX (~582 KB) y L7CX (~595 KB) — justamente por eso un
chip L8CX no inicializa con las libs L5CX/L7CX y viceversa (el `init()`
falla con err=255 cuando el blob no coincide con el silicon).

Nota: el wrapper L8CX nombra los archivos como `vl53l8cx.{h,cpp}` y
`vl53l8cx_platform.{h,c}` (sin sufijo `_class`, y con `.c` en vez de `.cpp`
para el ULD nativo). Distinto a L5CX (`vl53l5cx_class.h`, `platform.cpp`) y
L7CX (`vl53l7cx_class.h`, `vl53l7cx_platform.cpp`). Es upstream-side.

## Cómo actualizar

Procedimiento estándar — ver `lib/README.md` sección "Actualizar una librería
vendoreada".

## Referencias

- Sketch hermano: `src/diag/diag_top_tof_as_l8cx.cpp`
- Sketch L5CX: `src/diag/diag_top_tof_as_l5cx.cpp`
- Sketch L7CX original: `src/diag/diag_top_tof.cpp`
- Datasheet sensor: https://www.st.com/resource/en/datasheet/vl53l8cx.pdf
