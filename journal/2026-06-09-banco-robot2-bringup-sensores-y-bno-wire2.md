# 2026-06-09 — Banco ROBOT2: bring-up de sensores + el 2do BNO está VIVO en Wire2

**Quién:** Gustavo (banco, robot2) · Claude Opus 4.8 (diagnóstico/docs).
**Contexto:** primer día de banco con **ROBOT2** (el delantero) lo bastante armado como para
encender la placa TOP y revisar TODOS sus sensores de una. El objetivo del día era simple:
**¿levantan los sensores del robot2?** Y de paso resolver la duda vieja del 2do BNO (que en
ROBOT1 nunca anduvo bien por compartir bus con los ToF).

## Qué se hizo (bring-up de sensores del TOP de robot2)

Se corrió el bring-up de la placa TOP de robot2 y salió **bien en casi todo**:

| Sensor | Resultado |
|---|---|
| **BNO #1** (el de siempre, en `Wire` 18/19, junto a los ToF) | OK, levanta |
| **4 ToF** (VL53, anillo de obstáculos) | 4/4 OK |
| **HC-SR04** (ultrasonido frontal) | OK |
| **Árbitro** (GPIO nivel pin5/6) | OK |
| **2 cámaras** (frontal + trasera, por UART) | OK — **CRC resync = 0** (sin errores de sincronización) |
| **BNO #2** (el segundo, que en robot1 nunca andaba) | **¡VIVO!** — ver abajo, el hallazgo del día |

Resumen humano: el robot2, a nivel sensores del TOP, **arrancó limpio**. Las cámaras enganchan
sin perder bytes (el contador de "tuve que re-sincronizar por CRC" quedó en 0), que es justo lo
que queríamos ver antes de seguir.

## El hallazgo del día: 24/25 es **Wire2**, no Wire1 — y el BNO #2 está vivo ahí

Veníamos arrastrando una confusión de nombres. El 2do BNO se soldó a los **pads de abajo del
Teensy 4.0** (back-pads), que salen a los **pines 24/25**. Todos los docs y el código decían que
esos pines eran del bus **`Wire1`**. **Es falso:** en el Teensy 4.0 los pines **24/25 son del bus
`Wire2`** (el periférico LPI2C4). `Wire1` es 16/17 y NO maneja 24/25. Por eso el scan viejo
"nunca encontraba" el 2do BNO: estaba mirando el bus equivocado.

- Se arregló el diagnóstico (`diag_top_i2c_scan`, commit **`9da8e9e`**) para que escanee los **3
  buses con sus pines nativos**: `Wire` (18/19) + `Wire1` (16/17) + `Wire2` (24/25), **sin remapear
  pines**.
- Resultado en banco: el **BNO #2 aparece como `0x28` VIVO en `Wire2`**. `Wire1` quedó vacío.
- Punto clave: el BNO #2 está **SOLO en su bus** (no comparte con nada). Eso importa muchísimo,
  porque el problema histórico del BNO de robot1 es que **comparte `Wire` con los 4 ToF** y, cuando
  los ToF están midiendo, el read del BNO se corrompe y el rumbo (heading) **se congela**.

## La convención que decidimos: PRIMARIO (Wire2 solo) / SECUNDARIO (Wire + ToF)

Decisión de Gustavo:

- **PRIMARIO = el BNO de `Wire2`** (el que está solo, sin ToF). Es la **fuente de rumbo preferida**
  porque no tiene con quién pelear el bus → no se congela.
- **SECUNDARIO = el BNO de `Wire`** (el que comparte con los 4 ToF). Es el **respaldo**: si el
  primario falla, el robot cae a este, igual que hoy.

Por qué esto es cómodo de programar: el módulo de fusión (`g_fusion`) **ya** prioriza el sensor de
**índice 0** y, si ese no está, degrada al que esté presente. Entonces alcanza con **poner el
primario en el índice 0** y el "failover" (cambio automático al respaldo) **sale gratis**, sin
lógica nueva.

El perfil `src/shared/robot_config/robot2.h` **ya quedó cargado** con todo esto (commit `cafc98d`):
`IMU_BNO_BUS[2] = {2, 0}` (idx0 = primario por `Wire2`/bus 2 · idx1 = secundario por `Wire`/bus 0),
`IMU_BNO_ADDR[2] = {0x28, 0x28}` (ambos en la misma dirección, se distinguen por **bus**, no por
dirección), más `HEADING_SIGN`, `BNO_READ_INTERVAL_MS` e `I2C_CLOCK_HZ`.

## Lo que FALTA (no se cierra hoy): el read por Wire2 en el firmware de producción

Ojo: lo de arriba es el **diseño + el diagnóstico confirmado en banco**. El **read real** del 2do
BNO por `Wire2` **todavía NO está en el firmware de producción** (`sensors_imu.cpp`). Hoy ese
archivo sigue creando **los dos BNO en `&Wire`** (direcciones `0x28`/`0x29`), que es el esquema de
robot1. Falta:

1. **Firmware (gateado `#if defined(ROBOT2)`):** que `sensors_imu.cpp` lea el primario por `Wire2`
   (con `Wire2.begin()`) usando la tabla `IMU_BNO_BUS[]`/`IMU_BNO_ADDR[]` del perfil. **ROBOT1
   tiene que quedar byte-idéntico**: el código de hoy va literal en la rama `#else`. Solo cambian
   comentarios libremente.
2. **Validar compilando** (acá **NO se puede compilar Teensy**): `pio run -e top_robot1`
   (comparar el byte-diff del `.elf` → debe dar idéntico al de hoy) **y** `pio run -e top_robot2`.

Hasta que eso esté hecho y validado en banco, el primario por `Wire2` queda como **pendiente**,
no como "ya anda".

## Archivos / commits del día

- `9da8e9e` — `diag_top_i2c_scan` ahora escanea `Wire2` (24/25) con pines nativos (antes remapeaba
  mal `Wire1`).
- `cafc98d` — docs + `robot2.h`: corregir `Wire1`→`Wire2` del 2do BNO + convención
  primario(`Wire2` solo)/secundario(`Wire`+ToF).
- `95ab416` — renombrar las constantes de pin `WIRE1_*`→`WIRE2_*` (24/25 = Wire2 LPI2C4).
- Docs (esta sesión): `docs/robot-variants/ROBOT-DEFINITION-DESIGN.md` §IMU actualizado (estado:
  HW confirmado en banco; read por `Wire2` gateado ROBOT2 **pendiente** de implementar+compilar).

## Para la próxima sesión (banco / `pio`)

- Implementar el read del primario por `Wire2` gateado `ROBOT2` en `sensors_imu.cpp` + compilar
  `top_robot1` (byte-idéntico) y `top_robot2`.
- Una vez flasheado robot2: confirmar en `main_top` que el rumbo del **primario** sigue al giro
  **sin congelarse** (que era el síntoma del BNO de robot1) y que el failover al secundario degrada
  con gracia.

## Addendum 2026-06-09 (tarde) — VALIDADO: top_robot2 PRODUCCION con los 2 BNO y heading vivo

Gustavo flasheo `top_robot2` (produccion, commit 0f503f2) y el banco dio:
- **`imu_L=Y imu_R=Y`** — PRIMARIO (Wire2 24/25) + SECUNDARIO (Wire 18/19) leyendo a la vez.
- **El heading TRACKEA el giro EN PRODUCCION** (-1.7 -> 25 -> 47 -> 6.0 siguiendo el giro fisico,
  estable en reposo) con ToF + 2 camaras (resync=0) + snapshot 100 Hz corriendo.
- Es la validacion del FIX DE FONDO del freeze: con el BNO primario en bus propio (sin ToF),
  el congelamiento que robot1 nunca pudo resolver por software NO ocurre. Ver TASK-207.
- Falta en la cadena: DOWN (down_robot2) -> diag_central_rx_all -> motores (diag_central_motors,
  pines rotados de R2) -> central_robot2.
