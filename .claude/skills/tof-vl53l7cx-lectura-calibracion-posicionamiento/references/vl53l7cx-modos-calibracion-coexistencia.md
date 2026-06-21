# VL53L7CX — modos, calibración (xtalk/offset) y coexistencia BNO+ToF

> Procedimientos. La verdad que MANDA es UM3038 + el ULD API + el banco del robot. Lo que está
> sin confirmar contra banco está marcado.

## 1. Modo CONTINUO vs AUTÓNOMO (el "modo de fusión" del ToF)

| | AUTÓNOMO (default) | CONTINUO (`RANGING_MODE_CONTINUOUS=1`) |
|---|---|---|
| VCSEL | pulsado: encendido solo `integration_time` por frame | **siempre encendido** durante el ranging |
| `integration_time` | aplica (default **5 ms**); súmalo a todos los sub-frames | **NO tiene efecto** |
| Alcance / inmunidad al ambiente | menor | **mejor** (más energía emitida) |
| Consumo | menor (gana a BAJA frecuencia) | mayor |
| Recomendado para | bajo consumo | **ranging rápido / alto rendimiento** (habilita 60 Hz en 4x4) |

- **Por qué el 8x8 es ~4× más lento (modo autónomo):** la 4x4 se compone de UNA integration time; la
  8x8 de **CUATRO**. La suma de los integration times + 1 ms de overhead debe ser < el measurement
  period; si no, el periodo de ranging se **estira solo** (baja el fps efectivo).
- **Veredicto de ingeniería para competencia (confianza MEDIA, no cita textual de ST):** para un
  robot que corre rápido conviene **CONTINUO** — VCSEL siempre on da mejor alcance/inmunidad y
  permite 60 Hz; el ahorro del autónomo es irrelevante a batería que igual corre a alta tasa. En el
  robot está detrás de `TOP_ENABLE_TOF_CONTINUOUS` (default OFF, banco **TASK-219**:
  `sensors_tof.cpp:356-362`).

## 2. Power modes (evitar recargar el firmware)

- `set_power_mode(WAKEUP)` = HP idle (default). `set_power_mode(SLEEP)` = LP idle: **retiene firmware
  + config** en la RAM del módulo → al volver NO hay que recargar los ~84 KB. ⚠️ NO cambiar power
  mode mientras está rangeando.

## 3. Calibración de XTALK — SOLO si hay cover glass

El módulo viene auto-calibrado de fábrica. Xtalk = señal del VCSEL reflejada DENTRO del vidrio
protector. SOLO importa a **<~60 cm** (más allá el algoritmo de histograma lo cancela) — justo el
rango de wall-following cercano. **El robot HOY no tiene cover glass → no se calibra xtalk, y está
bien.** Si se monta vidrio:

1. Plugin `vl53l7cx_plugin_xtalk` (`vl53l7cx_calibrate_xtalk(...)`).
2. Target de **reflectancia conocida** cubriendo TODO el FoV, a **≥600 mm**, ANTES de `startRanging()`.
3. Guardar el buffer (`XTALK_BUFFER_SIZE = 776 bytes`) en flash del host.
4. Restaurarlo al boot (`vl53l7cx_set_caldata_xtalk(...)`).

**Offset:** calibrado de fábrica; `OFFSET_BUFFER_SIZE = 488 bytes` permite guardar/restaurar.
Recalibrar SOLO si cambia el setup óptico (cover glass). No inventes un paso de offset que el robot
no necesita.

## 4. Receta de coexistencia BNO055 + ToF del robot (verificada en código)

El TOP tiene 4 ToF + 2 BNO en buses I2C. El **BNO primario** está en `Wire2` (aparte, SIN ToF); el
**BNO secundario** comparte `Wire` con los 4 ToF. Pasos del firmware:

1. **Predim LP de los ToF antes de iniciar el BNO** (`sensors_tof_predim_lp`, `sensors_tof.cpp:244-255`):
   duerme los ToF (LP low) para dejar el bus limpio ANTES de `sensors_imu_init()`. Sin esto, los ToF
   despiertos en su dir de fábrica 0x29 ensucian el scan del BNO.
2. **Tres clocks** (`sensors_tof.cpp:154-173`): 1 MHz carga (default prod, TASK-211) → 400 kHz fallback
   (TASK-210) → **100 kHz runtime OBLIGATORIO**.
3. **Restore a 100 kHz al final del init** (`:377-380`): a >100 kHz el read multi-byte del BNO
   **secundario** se corrompe con los ToF rangeando en el mismo `Wire`.
4. **Fix de fondo:** mover el BNO **primario** (la fuente de heading) a `Wire2` → sin contención.

⚠️ **Inconsistencia viva (honestidad):** el código atribuye a la coexistencia ToF el "freeze del
yaw", pero la skill `bno055-imu-heading-robocup` re-diagnosticó (2026-06-21) que el freeze de
COMPETENCIA era el flag `bno_left_en=0` en EEPROM, NO los ToF. La corrupción multi-byte a 400 kHz es
real y el 100 kHz sigue obligatorio para el BNO secundario; pero **el "heading clavado en 0.0" fue el
flag de config**, no el rangeo. `TOP_TOF_NO_RANGE` (`:363-369`, TASK-223) existe justo para aislar
esto: enumera los ToF SIN VCSEL → si un freeze residual desaparece, es acople del rangeo; si persiste
con los ToF dormidos, NO son los ToF.

## 5. Trampa del stack overflow del host (status 0 en TODAS las zonas)

`status 0` ("Ranging data not updated") en TODAS las zonas pero con `distance_mm` que parecen válidas
= casi siempre **stack overflow del host** corrompiendo el campo `target_status` (está al FINAL de la
estructura de 1360 bytes). NO es el sensor. **Cura:** recortar la estructura con `VL53L7CX_DISABLE_*`
(baja a ~648 B) o dimensionar el stack de la tarea. Diagnosticá el stack ANTES de tocar hardware
(reportado por staff de ST en community.st.com).

## 6. Clock stretching del VL53L7CX

El sensor **estira la línea SCL cuando manda su ACK** durante la comunicación I2C — la línea de reloj
puede demorarse aunque los comandos se envíen OK. ⚠️ **Atestiguado por staff/usuarios de ST community,
NO figura como tal en la tabla de timing del datasheet** (que solo lista Fast mode plus 1 MHz / Fast
mode 400 kHz). Un master que no soporta clock stretching (I2C bit-banged, algunos periféricos)
convive mal. Trampa reportada: un ACK al final de un "read multi" justo antes del stop bit puede
hacer que el sensor mantenga SDA en bajo y bloquee el bus. El Teensy `Wire` (HW) lo soporta; un RPi
por HW lo rompe. **El firmware del robot NO configura nada de clock stretching** (verificado): lo que
documenta es la corrupción del read del BNO a >100 kHz, que es coexistencia/timing, no clock
stretching nombrado como tal.

**Fuentes:** UM3038 (modos, calibración, results); ULD API (`vl53l7cx_api.h`,
`vl53l7cx_plugin_xtalk.h`); ST community (status 0 / "fails at init" / "strange start-up behaviour").
