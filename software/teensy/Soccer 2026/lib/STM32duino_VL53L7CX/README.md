# DEPRECATED EN ESTE REPO — NO USAR EN CODIGO NUEVO

**Bug identificado 2026-05-24**: esta lib tiene un bug en
`src/vl53l7cx_platform.h` lineas 49-60 que rompe el init del sensor en
Teensy 4.0 (y en cualquier plataforma con `BUFFER_LENGTH > 32` que no este
en su lista de `#ifdef`). El sintoma es `init_sensor() = err 255` aunque
el sensor responda OK al I2C scan en 0x29.

**Detalle tecnico**: `DEFAULT_I2C_BUFFER_LEN = BUFFER_LENGTH` (256 en
Teensy 4.0), pero cada chunk de write es `2 bytes header + 256 bytes
payload = 258 bytes`, desbordando el buffer interno del `Wire`. El upload
del firmware blob (~85 KB) se corrompe silenciosamente. El upstream ya
tiene la mitad del fix (`BUFFER_LENGTH - 2`) pero solo para Arduino DUE
(`#ifdef ARDUINO_SAM_DUE`), no para Teensy.

**Solucion usada en este repo**: migramos a `lib/Adafruit_VL53L7CX/`
(mismo sensor, lib distinta, sin este bug — usa `maxBufferSize() - 2`).
Ver `journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md` para el
debug completo (3 hipotesis fallidas + root cause + diff de codigo).

**Por que mantenemos esta lib vendoreada**: trazabilidad historica del
debug + sketches diag hermanos (`diag_top_tof`, `diag_top_tof_no_xshut`)
que documentan los intentos. **NO usar en codigo nuevo del repo.** El
firmware vivo (`src/top/sensors_tof.cpp`) usa `Adafruit_VL53L7CX`.

---

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
