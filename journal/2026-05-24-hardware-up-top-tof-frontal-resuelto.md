---
title: "Hardware-up: VL53L7CX frontal funcionando + bug raiz en lib STM32duino"
date: 2026-05-24
author: "Claude Opus 4.7 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: vivo
tipo: hardware-up
modulo: top-board
sensor: VL53L7CX
related-tasks: [TASK-032]
related-journals:
  - 2026-05-24-hardware-up-down-anillo-linea.md
  - 2026-05-24-otos-lib-activada-y-power-cycle-bug.md
---

# Hardware-up: VL53L7CX frontal funcionando + bug raiz en lib STM32duino

> **TL;DR.** El VL53L7CX frontal (U2) de la placa TOP **funciona y mide**.
> Las 3 libs STMicroelectronics (L5/L7/L8) que probamos en serie fallaron
> todas con el mismo sensor; la lib **Adafruit_VL53L7CX** lo levanto al
> primer intento en el mismo hardware. Bug raiz identificado:
> `STM32duino_VL53L7CX/src/vl53l7cx_platform.h:49-60` desborda en 2 bytes
> el buffer interno del `Wire` de Teensy 4.0 al cargar el firmware blob del
> sensor, lo que rompe silenciosamente el upload. **Decision:** firmware
> vivo migrado a la lib Adafruit. Sigue dando lectura util a la API publica
> de `sensors_tof_init/tick/get_distance` sin cambios.

## El debug — cronologia honesta de las hipotesis

Fueron ~3 horas. Las primeras 2.5 nos fuimos por hipotesis erradas. La pista
correcta vino de Gustavo sugiriendo probar una lib distinta (Adafruit) como
test de control. **Sin esa sugerencia probablemente seguiriamos forkeando
la lib ST.**

### Hipotesis 1 (fallida) — "XSHUT no esta ruteado al pin que asume el firmware"

Primer sintoma: `vl53l7cx.init_sensor() = 255` (err generico de ST). Pensamos
que el toggle del pin XSHUT (`PIN_TOF_XSHUT[0] = 2`) por parte de la lib
estaba dejando al chip en un estado raro porque ese pin no esta ruteado en
la placa del robot.

- Patch: `-DDIAG_TOF_SKIP_XSHUT` -> ctor recibe `lpn_pin = -1` (commit `759f7b2`).
- Resultado: **mismo err=255**. Confirmacion de que XSHUT no era el problema:
  el chip respondia al I2C scan (0x29 ACK), simplemente la carga del init fallaba.

### Hipotesis 2 (fallida) — "I2C clock muy rapido para la carga del blob"

El init del L7CX carga un firmware blob de ~85 KB por I2C al sensor. Si el
clock es muy alto puede haber problemas de capacitancia/integridad en buses
sucios. Probamos slow mode.

- Patch: `-DDIAG_TOF_SLOW_I2C` (commit `a58e3b0`) -> baja Wire a 100 kHz.
- Resultado: **mismo err=255**. Tampoco era eso.

### Hipotesis 3 (fallida) — "El chip no es L7CX; el equipo compro L5/L7/L8 mezclados sin trazabilidad"

Esta hipotesis tenia base real. Pololu confirma que los carriers fisicos de
las 3 familias (L5CX, L7CX, L8CX) son identicos a la vista — no hay manera
de distinguirlos sin lupa sobre el silicon. El equipo compro los 3 modelos
en paralelo y no anotaron cual quedo soldado en U2.

Vendoreamos las otras dos libs y armamos sketches hermanos para identificar
el sensor por la lib que lo levante:

- `[env:diag_top_tof_as_l5cx]` con `STM32duino_VL53L5CX` (commit `d7872ac`)
  -> **silent hang en la carga del blob** (LED se apaga, no llega a setup
  completo, no imprime error). 
- `[env:diag_top_tof_as_l8cx]` con `STM32duino_VL53L8CX` (commit `8048ea0`)
  -> **err=255**, mismo sintoma que el L7CX original.

A esta altura el verdict era: o las 3 libs ST tienen el mismo bug, o el
sensor fisico esta dañado, o no es ninguno de los 3 (silicon remarcado /
falso modulo Pololu). Eramos pesimistas.

### Hipotesis 4 (correcta) — "Probar la otra lib popular para L7CX"

Gustavo: "y si probas con la lib Adafruit? Es otra familia de drivers, si
funciona ahi, el problema es de las libs ST." Esta fue la pista clave.

Vendoreamos `Adafruit_VL53L7CX` y creamos `[env:diag_top_tof_adafruit]`
(commit `131358e`). 

- Setup A — mismo cableado, misma placa TOP del robot, mismo U2:
  **FUNCIONA**. Init OK, isDataReady() devuelve true, grilla 8x8 con
  zonas validas (la mayoria status=5) y distancias coherentes con la
  escena fisica enfrente del sensor.
- Setup B — Teensy 4.0 fresco en banco, modulo VL53L7CX nuevo en
  protoboard (cableado VIN/GND/SDA18/SCL19, LPn al aire):
  **TAMBIEN FUNCIONA**. Esto descarta cualquier hipotesis de
  "hardware especifico del robot": es la lib.

Conclusion: el sensor U2 esta sano, el chip es un VL53L7CX genuino, el
cableado de la placa TOP esta bien. El problema era **100% software de las
libs STMicroelectronics con Teensy 4.0**.

## Bug raiz identificado

Mientras la lib Adafruit cargaba el blob en ~6 segundos, las libs ST
fallaban en el mismo paso. El diff de codigo entre las dos implementaciones
del helper `WrMulti` (que escribe el blob por I2C en chunks) es claro.

### STM32duino_VL53L7CX — `lib/STM32duino_VL53L7CX/src/vl53l7cx_platform.h:49-60`

```c
#ifndef DEFAULT_I2C_BUFFER_LEN
  #ifdef ARDUINO_SAM_DUE
    /* FIXME: It seems that an I2C buffer of BUFFER_LENGTH does not work
       on Arduino DUE. So, we need to decrease the size */
    #define DEFAULT_I2C_BUFFER_LEN (BUFFER_LENGTH - 2)
  #else
    #ifdef BUFFER_LENGTH
      #define DEFAULT_I2C_BUFFER_LEN BUFFER_LENGTH
    #else
      #define DEFAULT_I2C_BUFFER_LEN 32
    #endif
  #endif
#endif
```

En Teensy 4.0 (y en cualquier core Arduino con `BUFFER_LENGTH > 32` que no
este en la lista de `ifdef`), `DEFAULT_I2C_BUFFER_LEN = BUFFER_LENGTH`. Para
Teensy 4.0 el `Wire` por default tiene `BUFFER_LENGTH = 256`.

Pero cada chunk de escritura del blob es **2 bytes header + N bytes payload**.
La lib pasa `payload = DEFAULT_I2C_BUFFER_LEN`, entonces el `Wire.write()`
real es de **258 bytes** -> el buffer interno de Wire es 256 -> desborda en
2 bytes. `Wire.endTransmission()` devuelve 0 (success) o algun error de
overflow segun la version del core, pero los 2 ultimos bytes del chunk se
**descartan silenciosamente**. El firmware blob queda corrompido en cada
chunk, el sensor lo rechaza al validar el checksum interno, init devuelve
err=255.

El upstream **ya tiene la mitad del fix**: para Arduino DUE hacen
`BUFFER_LENGTH - 2`. Pero no contemplaron Teensy ni hicieron la regla
generica `min(BUFFER_LENGTH, X) - 2`. Es bug upstream.

### Adafruit_VL53L7CX — `lib/Adafruit_VL53L7CX/src/platform.cpp:74-76`

```cpp
uint8_t WrMulti(VL53L7CX_Platform* p_platform, ...) {
  // ...
  // Reserve 2 bytes for register address
  uint32_t maxPayload = p_platform->i2c_dev->maxBufferSize() - 2;
  uint8_t buffer[maxPayload + 2];
  // ...
}
```

Adafruit usa `Adafruit_BusIO::maxBufferSize() - 2`. El `- 2` esta
explicitamente reservado para el header del registro, no para el payload.
**Por eso funciona en Teensy 4.0 directamente.** Misma plataforma, misma
operacion, sin fork ni patch.

## Decision

**Migrar el firmware vivo a la lib Adafruit_VL53L7CX**. Justificacion:

1. **Funciona out of the box en Teensy 4.0** — no requiere forkear ni
   parchear el upstream (que es lo que estaria pasando si nos quedaramos
   con la lib ST).
2. **API mas simple, Arduino-style** — `begin(addr, &Wire, freq)` /
   `setResolution()` / `isDataReady()` / `getRangingData()`. Todos los
   setters devuelven `bool`. Menos friccion para el alumno que toque el
   codigo en 2027.
3. **Misma cobertura funcional para el firmware actual** — 4x4 o 8x8 @
   15 Hz, threshold detection, motion indicator, xtalk calibration. No
   perdemos features.
4. **MIT-licensed** vs BSD-3-Clause de ST. Misma libertad de uso, sin
   restricciones para el repo.

Alternativa rechazada: parchear `STM32duino_VL53L7CX/src/vl53l7cx_platform.h`
con `min(BUFFER_LENGTH, 256) - 2`. **No** — preferimos no mantener un fork
de una lib upstream para un fix de 1 linea cuando hay otra lib que ya lo
tiene resuelto y es mas pulida.

## Lo que cerramos hoy

- ✅ **VL53L7CX U2 frontal validado fisicamente.** Mide distancias
  coherentes, 60+ zonas validas / 64 al apuntar a pared lisa.
- ✅ **Bug raiz identificado y documentado** (este journal + banner
  DEPRECATED en los READMEs de `lib/STM32duino_VL53L7CX/` y
  `lib/STM32duino_VL53L5CX/`).
- ✅ **Lib Adafruit_VL53L7CX vendoreada** en `lib/Adafruit_VL53L7CX/`
  (commit `131358e`).
- ✅ **Sketch de control standalone** `[env:diag_top_tof_adafruit]`
  funcionando, sirve como referencia futura del init correcto.
- ✅ **Firmware vivo `src/top/sensors_tof.cpp` migrado** del stub
  TODO_TOF_LIB a la implementacion real Adafruit. API publica de
  `sensors_tof.h` sin cambios -> ningun otro modulo de TOP se rompe.
- ✅ **Nuevo `[env:diag_sensors_tof_live]`** permite testear el modulo
  migrado en aislamiento (sin main_top, sin camaras, sin IMU) en banco
  con Teensy 4.0 + 1 ToF + HC-SR04 opcional.

## Lo que NO se hizo

- ❌ **Integracion end-to-end del TOP completa.** El robot todavia no se
  encendio con TOP + DOWN + CENTRAL + COMM por UART real. La regla 8 de
  CLAUDE.md (moratoria de fabrica de papel hasta el primer hardware-up
  completo) sigue vigente: hoy es un hardware-up parcial del **subsistema
  ToF frontal del TOP**, no del robot completo.
- ❌ **No se probo el `[env:top]` en hardware.** Compila limpio (gate ✅)
  pero no flashee el binario en la Teensy del TOP del robot — eso es de
  la sesion humana, esta sesion era debug + migracion.
- ❌ **Solo el ToF U2 esta activo.** Los otros 3 slots (U3 en Wire, U5 +
  U17 en Wire1) quedan retornando `TOF_NO_READING` porque fisicamente no
  hay modulos soldados. Cuando lleguen los modulos restantes hay que
  agregar el codigo de enumeracion XSHUT (cambio de address I2C
  individual por sensor) — eso es trabajo de una sesion posterior.
- ❌ **No se reporto el bug upstream al proyecto stm32duino.** Queda
  pendiente para post-Incheon.

## Para 2027 / post-Incheon

- **Reportar bug upstream a `stm32duino/VL53L7CX`** (y probablemente
  tambien a `stm32duino/VL53L5CX` y `VL53L8CX`, comparten la misma
  estructura). PR sugerido: cambiar la regla de `DEFAULT_I2C_BUFFER_LEN`
  a algo como:
  ```c
  #ifdef BUFFER_LENGTH
    #if BUFFER_LENGTH > 32
      #define DEFAULT_I2C_BUFFER_LEN (BUFFER_LENGTH - 2)
    #else
      #define DEFAULT_I2C_BUFFER_LEN BUFFER_LENGTH
    #endif
  #else
    #define DEFAULT_I2C_BUFFER_LEN 32
  #endif
  ```
  El `- 2` es el header del registro, deberia ser universal y no solo para
  Arduino DUE.
- **Vincular el fix al README** del fork (si terminamos manteniendo uno) o
  al README del repo si nos termina respondiendo el upstream.
- **Cuando lleguen los otros 3 modulos ToF**, planificar la enumeracion
  XSHUT en 4 sensores: encender uno, asignar address, encender el
  siguiente, etc. La lib Adafruit lo soporta con `setAddress(uint8_t)`.

## Atribucion

- **Debug + diagnostico + migracion + journal + banners** — Claude Opus
  4.7 (Anthropic), sesion 2026-05-24, modo ejecucion con asistencia
  humana.
- **Hardware en mano (placa TOP + sensor breadboard de control) +
  alimentacion + pista clave "proba con Adafruit"** — Gustavo Viollaz
  (@gviollaz).
- **Lib `Adafruit_VL53L7CX`** — Limor 'ladyada' Fried (Adafruit
  Industries) con asistencia de Claude Code. MIT license.
- **Lib `STM32duino_VL53L7CX`** — STMicroelectronics. BSD-3-Clause.
  Marcada DEPRECATED en este repo por el bug descrito, conservada
  vendoreada para trazabilidad historica del debug.
