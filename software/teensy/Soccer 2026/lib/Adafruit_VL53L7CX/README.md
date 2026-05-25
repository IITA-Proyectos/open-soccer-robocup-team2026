# Adafruit_VL53L7CX — librería vendoreada

Driver Arduino de **Adafruit** para el sensor ToF multizona **VL53L7CX** de
STMicroelectronics. Wrapper C++ alternativo sobre el mismo ULD ("Ultra Lite
Driver") C nativo de ST que usa `STM32duino_VL53L7CX`, pero con una API más
simple y `Adafruit_BusIO` como capa de transporte I²C.

## Origen

- **Upstream:** https://github.com/adafruit/Adafruit_VL53L7
- **Commit vendoreado:** `1b0816a7eb1a8d7706d08ba075819027e51354e1`
- **Tag más cercano:** `1.0.0`
- **License:** MIT (ver `LICENSE`)
- **Fecha de vendoreo:** 2026-05-24
- **Vendoreado por:** Claude Opus 4.7 (Anthropic) — Requested-by Gustavo Viollaz (@gviollaz)

## Por qué está vendoreada

Lib paralela a `STM32duino_VL53L7CX` — ambas envuelven el ULD de ST pero la
API de Adafruit es más simple (`setResolution(64)`, `setRangingFrequency(15)`,
`startRanging()`). Se vendorea como **test de control** porque las 3 libs
STMicroelectronics (L5CX/L7CX/L8CX) fallaron en el VL53L7CX soldado en la
placa TOP (err=255 o hang en la carga del firmware blob). Si Adafruit
funciona donde ST falla, el problema estaba en las libs ST.

Política del repo (`lib/README.md`): todas las dependencias de firmware se
vendorean para compilar 100% offline (Avast/SSL roto, Incheon sin wifi
garantizado). Igual que Adafruit_BNO055, Adafruit_BusIO, etc.

## Quién la usa

- `[env:diag_top_tof_adafruit]` — sketch standalone que prueba el VL53L7CX
  frontal U2 de la placa TOP con la API Adafruit, como contraste contra los
  3 hermanos `diag_top_tof*` que usan las libs ST.

El usuario va a probar el mismo sketch en 2 setups:
1. La placa TOP del robot (mismo sensor que estuvo fallando con las libs ST).
2. Un setup de banco con Teensy 4.0 + VL53L7CX nuevo en protoboard.

## Dependencias

`library.properties` declara `depends=Adafruit BusIO`. La lib usa
`Adafruit_I2CDevice` para todo el transporte I²C (en vez de hablar `Wire`
directo como STM32duino). **Adafruit_BusIO YA está vendoreada en
`lib/Adafruit_BusIO/`** (usada por BNO055), así que no hay nada que
agregar. **Adafruit_Unified_Sensor** también está vendoreada en `lib/`
aunque esta lib no la requiera explícitamente.

## Compatibilidad de plataforma

`library.properties` declara `architectures=*` (a diferencia de las libs
STM32duino que declaran `stm32, sam`). El compilador la acepta sin
restricción en Teensy 4.0.

El upstream advierte que **NO funciona en AVR** (Uno/Mega) porque el blob
de firmware del L7CX (~85 KB) no entra en la flash AVR. En Teensy 4.0 hay
2 MB de flash, no es problema.

## Contenido podado

Se removieron `examples/`, `hw_tests/`, `webserial/`, `.git/`, `.github/`,
`.clang-format`, `.gitignore`, `README.md` del upstream (siguiendo
`lib/README.md`). Se conservaron: `src/`, `library.properties`, `LICENSE`.

Nota: este upstream **no tiene `library.json`** (solo `library.properties`
con metadata Arduino IDE — Adafruit no usa el formato PlatformIO).
PlatformIO lo lee igual.

Nota: `src/vl53l7cx_buffers.h` pesa ~595 KB — es el firmware blob del
sensor (idéntico al de `STM32duino_VL53L7CX`, se carga por I²C en el
`begin()`). Es esperable; no es bloat.

## Diferencias clave de API vs `STM32duino_VL53L7CX`

| Acción | Adafruit | STM32duino |
|---|---|---|
| Constructor | `Adafruit_VL53L7CX vl53l7cx;` (sin args) | `VL53L7CX g(&Wire, lpn_pin);` |
| Init completo | `begin(addr, &Wire, 400000)` | `begin()` + `init_sensor()` |
| Resolución | `setResolution(64)` (64 = 8x8, 16 = 4x4) | `vl53l7cx_set_resolution(VL53L7CX_RESOLUTION_8X8)` |
| Freq | `setRangingFrequency(15)` | `vl53l7cx_set_ranging_frequency_hz(15)` |
| Start | `startRanging()` | `vl53l7cx_start_ranging()` |
| Data ready | `isDataReady()` (devuelve `bool`) | `vl53l7cx_check_data_ready(&u8)` |
| Get | `getRangingData(&results)` (devuelve `bool`) | `vl53l7cx_get_ranging_data(&results)` |
| LPn/XSHUT | Manejo manual fuera de la lib | Ctor recibe `lpn_pin` |

La estructura `VL53L7CX_ResultsData` es **idéntica** en ambas libs (definida
en `vl53l7cx_api.h` del ULD de ST). El índice de zonas también es el mismo
(row-major desde el corner top-left según datasheet).

## Cómo actualizar

Procedimiento estándar — ver `lib/README.md` sección "Actualizar una librería
vendoreada".

## Referencias

- Datasheet sensor: https://www.st.com/resource/en/datasheet/vl53l7cx.pdf
- Upstream README: https://github.com/adafruit/Adafruit_VL53L7
- Hermana ST: `lib/STM32duino_VL53L7CX/README.md`
