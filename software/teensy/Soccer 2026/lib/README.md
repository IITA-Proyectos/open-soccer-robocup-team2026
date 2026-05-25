# `lib/` — Librerías vendoreadas (commiteadas al repo)

> **Por qué están acá.** Estas librerías están **incluidas en el repo a
> propósito** para que cualquier integrante (Virginia, Elías, Enzo) pueda
> clonar y **compilar los firmwares sin depender del registry de PlatformIO**
> ni de tener internet/Avast funcionando (ver `team-tasks/TASK-025` — Avast
> bloquea las descargas de PlatformIO por SSL-scanning).
>
> Vendoreadas el 2026-05-19, código fuente **podado** (sin `examples/`,
> `examples_processing/`, `docs/` — solo lo que el compilador necesita).
> Total: ~1.9 MB (incluye VL53L7CX vendoreada 2026-05-24 con firmware blob ~595 KB
> y VL53L5CX vendoreada 2026-05-24 con firmware blob ~582 KB).

## Contenido

| Carpeta | Librería | La usa | Origen |
|---|---|---|---|
| `Unity/` | Framework de tests unitarios | `test_native` (suites host) | throwtheswitch/Unity 2.6.x |
| `Adafruit_BNO055/` | Driver IMU BNO055 | `top`, `central_robot1`, `central_robot2` | adafruit/Adafruit BNO055 1.6.x |
| `Adafruit_BusIO/` | I2C/SPI helper (dep. de BNO055) | idem | adafruit/Adafruit BusIO |
| `Adafruit_Unified_Sensor/` | Interfaz de sensor (dep. de BNO055) | idem | adafruit/Adafruit Unified Sensor |
| `STM32duino_VL53L7CX/` | Driver ToF multizona VL53L7CX | `diag_top_tof` (futuro `top` cuando se integre) | stm32duino/VL53L7CX |
| `STM32duino_VL53L5CX/` | Driver ToF multizona VL53L5CX | `diag_top_tof_as_l5cx` (identificacion chip desconocido U2) | stm32duino/VL53L5CX |

## Cómo funciona

- PlatformIO escanea `lib/` automáticamente (Library Dependency Finder).
  Cuando un `.cpp` hace `#include <Adafruit_BNO055.h>`, usa la copia de acá
  **en lugar de bajarla del registry**.
- Por eso `platformio.ini` ya **no** tiene `lib_deps = adafruit/...` en los
  envs `top` / `central_robot1` / `central_robot2` — las libs viven acá.
- **Verificado 2026-05-19**: borrando `.pio/libdeps` y compilando, las 4
  placas (`top`, `down`, `central_robot1`, `central_robot2`) dan `SUCCESS`
  sin tocar el registry.

## Límite conocido — Unity y los tests

`platformio.ini` usa `test_framework = unity`. Ese mecanismo de PlatformIO
**siempre resuelve `throwtheswitch/Unity` desde el registry**, sin importar
que Unity esté acá vendoreado (probado: `lib_deps = symlink://` no lo evita).

- **Los FIRMWARES compilan 100% offline** — eso es lo importante para flashear
  el robot.
- **Los TESTS host-native (`pio test -e test_native`)** sí necesitan bajar
  Unity una vez (~2 s con red). Si Avast bloquea, aplicar la excepción de
  `team-tasks/TASK-025`. La copia `lib/Unity/` queda como respaldo (se puede
  copiar a mano a `.pio/libdeps/test_native/Unity` si hace falta correr
  offline).

## Lo que NO está acá (no se puede vendorear en git)

El **toolchain** (compilador ARM GCC ~279 MB + framework Teensy ~94 MB) vive
en `C:\Users\<usuario>\.platformio\` — son 490 MB, imposible/incorrecto
subirlo a git. Cada máquina lo baja una vez con `pio run`. Si Avast bloquea,
ver `docs/firmware/SETUP-ENTORNO-BUILD-WINDOWS.md`.

## Actualizar una librería vendoreada

1. Bajar la versión nueva (`pio pkg install` en una máquina con red).
2. Copiar de `.pio/libdeps/<env>/<Lib>/` a `lib/<Lib>/` **solo** los `.cpp`,
   `.h`, `utility/`, `src/`, `library.properties`/`library.json`, `LICENSE`.
   NO copiar `examples/`, `docs/`.
3. Compilar las 4 placas para verificar. Commit.
