# Mapa de EEPROM por placa

> Teensy 4.0/4.1: **1080 bytes** de EEPROM emulada en flash, lifetime ~100k ciclos/celda.
> Usar siempre `EEPROM.update()` (escribe solo si el byte cambió). Cada placa tiene su PROPIA
> EEPROM (chips distintos) — este doc evita colisiones de OFFSET dentro de cada placa.
>
> Patrón canónico (3 capas): módulo PURO host-testeable (serialize + CRC16-CCITT) → glue
> Arduino (`EEPROM.update` byte a byte) → carga ungated al boot. Toda región lleva
> **magic + version + CRC**; si cualquiera falla → defaults (no-op = competencia byte-idéntica).

## Placa TOP (Teensy 4.0)

| Offset | Bytes | Subsistema | Módulo | Magic/Ver |
|--------|-------|------------|--------|-----------|
| `[0, 319]` | 320 | *(libre)* | — | — |
| `[320, 367]` | 48 | **Calib BNO055 dual** | `src/top/sensors_imu.cpp` (`EE_BASE=320`) | magic `0xB2` v1 (sin CRC) |
| **`[368, 460]`** | **93** | **TopConfig** (enables cámara/BNO/ToF/US + bearing/zonas ToF) | `src/top/top_eeprom_config.cpp` (`TOP_CONFIG_EEPROM_OFFSET=368`) + puro `src/shared/top_config.{h,cpp}` | magic `0x7C` v1 + CRC16 |
| `[461, 1079]` | 619 | *(libre — grow-room para TopConfig v2+)* | — | — |

⚠️ **No pisar `[320, 367]`** (calib del IMU): `TOP_CONFIG_EEPROM_OFFSET=368` arranca justo después.
Si se agrega un subsistema nuevo, tomar de `[461, 1079]` y agregar fila acá **en el mismo commit**.

## Placa DOWN (Teensy 4.0)

| Offset | Bytes | Subsistema | Módulo | Magic/Ver |
|--------|-------|------------|--------|-----------|
| `[0, 200]` | 201 | **Calib línea 32 sensores** + sintonía | `src/down/eeprom_calib.cpp` (`EC_EEPROM_OFFSET=0`) + puro `src/shared/calib_storage.{h,cpp}` | magic `0x494954A1` v2 + CRC16 |
| `[201, 1079]` | 879 | *(libre)* | — | — |

## Notas

- **Defaults no-op:** EEPROM en blanco (0xFF) → magic falla → defaults. La carga al boot NO cambia
  la conducta de competencia salvo lo que el equipo persista a propósito (`CFG SAVE` / `CAL SAVE`).
- **El flasher de Teensy normalmente NO borra la EEPROM emulada** (sobrevive al re-flash) → una
  config de una sesión anterior queda viva; `version` + CRC la rechazan si cambió el formato, y
  `CFG RESET` + `CFG SAVE` la limpia a mano. El bloque/línea de telemetría delata qué quedó activo.
- **Serializar BYTE-A-BYTE little-endian**, nunca `memcpy` del struct (el padding difiere
  host x86 ↔ Teensy ARM → rompería la byte-identidad del blob entre los tests host y el firmware).
