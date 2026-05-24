# STM32duino_VL53L7CX — librería vendoreada

Driver Arduino oficial para el sensor ToF multizona **VL53L7CX** de
STMicroelectronics. Wrapper C++ sobre el ULD ("Ultra Lite Driver") C nativo
de ST.

## Origen

- **Upstream:** https://github.com/stm32duino/VL53L7CX
- **Commit vendoreado:** `7b21bdb34cb06c95267f4e52e5762e6e9373a449`
- **Tag más cercano:** `1.0.3`
- **License:** BSD-3-Clause (ver `LICENSE.md`)
- **Fecha de vendoreo:** 2026-05-24
- **Vendoreado por:** Claude Opus 4.7 (Anthropic) — Requested-by Gustavo Viollaz (@gviollaz)

## Por qué está vendoreada

Política del repo (`lib/README.md`): todas las dependencias de firmware se
vendorean para compilar 100% offline (Avast/SSL roto, Incheon sin wifi
garantizado). Igual que Adafruit_BNO055, Adafruit_BusIO, etc.

## Quién la usa

- `[env:diag_top_tof]` — sketch standalone para validar el VL53L7CX frontal U2
  de la placa TOP.

Cuando los TOFs se integren al firmware vivo (`src/top/sensors_tof.cpp`),
esta lib también la usará el `[env:top]`.

## Compatibilidad de plataforma

`library.properties` declara `architectures=stm32, sam` (no incluye `teensy` ni
`*`). En la práctica el wrapper sólo usa `Wire`, `digitalWrite`, `pinMode` y
`delay` — todas APIs Arduino estándar disponibles en Teensy 4.0. La capa de
plataforma (`vl53l7cx_platform.{h,cpp}`) está implementada sobre `Wire.h` y es
portable. Si en el futuro PlatformIO empieza a filtrar por `architectures`, hay
que parchearla agregando `teensy` o `*` a esa línea — pero al día de hoy PIO no
lo hace para libs en `lib/` del project root.

## Contenido podado

Se removieron `examples/`, `.git/`, `.github/`, `extras/`, `keywords.txt` y el
`README.md` del upstream (siguiendo `lib/README.md`). Se conservaron: `src/`,
`library.properties`, `LICENSE.md`.

Nota: este upstream **no tiene `library.json`** (solo `library.properties` con
metadata Arduino IDE). PlatformIO lo lee igual.

Nota: `src/vl53l7cx_buffers.h` pesa ~595 KB — es el firmware blob del sensor
(se carga por I²C al `init()`). Es esperable; no es bloat.

## Cómo actualizar

Procedimiento estándar — ver `lib/README.md` sección "Actualizar una librería
vendoreada".

## Referencias

- Spec del primer uso: `docs/superpowers/specs/2026-05-24-diag-top-tof-vl53l7cx-design.md`
- Datasheet sensor: https://www.st.com/resource/en/datasheet/vl53l7cx.pdf
