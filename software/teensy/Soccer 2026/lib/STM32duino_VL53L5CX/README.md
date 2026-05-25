# STM32duino_VL53L5CX — librería vendoreada

Driver Arduino oficial para el sensor ToF multizona **VL53L5CX** de
STMicroelectronics. Wrapper C++ sobre el ULD ("Ultra Lite Driver") C nativo
de ST.

## Origen

- **Upstream:** https://github.com/stm32duino/VL53L5CX
- **Commit vendoreado:** `e904f764c1bdd2c35032cb4e9f9bbfdd94329865`
- **Tag más cercano:** `1.2.3`
- **License:** BSD-3-Clause (ver `LICENSE.md`)
- **Fecha de vendoreo:** 2026-05-24
- **Vendoreado por:** Claude Opus 4.7 (Anthropic) — Requested-by Gustavo Viollaz (@gviollaz)

## Por qué está vendoreada

Política del repo (`lib/README.md`): todas las dependencias de firmware se
vendorean para compilar 100% offline (Avast/SSL roto, Incheon sin wifi
garantizado). Igual que Adafruit_BNO055, Adafruit_BusIO, STM32duino_VL53L7CX,
etc.

## Quién la usa

- `[env:diag_top_tof_as_l5cx]` — sketch hermano de `diag_top_tof` para
  identificar el chip desconocido soldado en U2 de la placa TOP. El equipo
  compró VL53L5CX / L7CX / L8CX mezclados sin trazabilidad y los carriers
  Pololu son físicamente idénticos. Si este sketch inicializa OK, el chip es
  un L5CX.

## Compatibilidad de plataforma

`library.properties` declara `architectures=stm32, sam` (no incluye `teensy` ni
`*`). Mismo caso que la lib L7CX vendoreada: en la práctica el wrapper sólo
usa `Wire`, `digitalWrite`, `pinMode` y `delay` — todas APIs Arduino estándar
disponibles en Teensy 4.0. La capa de plataforma (`platform.{h,cpp}`) está
implementada sobre `Wire.h` y es portable. PIO no filtra por `architectures`
para libs en `lib/` del project root al día de hoy.

## Contenido podado

Se removieron `examples/`, `.git/`, `.github/`, `keywords.txt` y el
`README.md` del upstream (siguiendo `lib/README.md`). Se conservaron: `src/`,
`library.properties`, `LICENSE.md`.

Nota: este upstream **no tiene `library.json`** (solo `library.properties` con
metadata Arduino IDE). PlatformIO lo lee igual.

Nota: `src/vl53l5cx_buffers.h` pesa ~582 KB — es el firmware blob del sensor
(se carga por I²C al `init()`). Es esperable; no es bloat. Este blob es
**distinto** del de la lib L7CX — esa es justamente la razón por la que un
chip L5CX no se inicializa con la lib L7CX y vice versa (el `init_sensor()`
falla con err=255 cuando el blob no coincide con el silicon).

Nota: el wrapper tiene los archivos de plataforma con nombres **distintos**
al de L7CX. Acá se llaman `platform.h` / `platform.cpp` / `platform_config.h`
(sin el prefijo `vl53l5cx_`). En L7CX son `vl53l7cx_platform.{h,cpp}`. Es
upstream-side, no es bug del vendoreo.

## Cómo actualizar

Procedimiento estándar — ver `lib/README.md` sección "Actualizar una librería
vendoreada".

## Referencias

- Sketch hermano: `src/diag/diag_top_tof_as_l5cx.cpp`
- Sketch L7CX original: `src/diag/diag_top_tof.cpp`
- Datasheet sensor: https://www.st.com/resource/en/datasheet/vl53l5cx.pdf
