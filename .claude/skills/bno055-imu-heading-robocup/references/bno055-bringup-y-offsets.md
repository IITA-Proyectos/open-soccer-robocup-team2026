# BNO055 — bringup, calibración y guardar/restaurar offsets

> Orden ground-truth del driver Adafruit. El BNO055 NO tiene EEPROM interna: arranca
> des-calibrado SIEMPRE → la persistencia de offsets es responsabilidad del MCU host.

## Secuencia de bringup correcta (orden importa)

1. **Leer `CHIP_ID` (0x00), esperar 0xA0** — reintentar hasta ~850 ms (el boot del Cortex-M0
   interno tarda). Si nunca da 0xA0 → no está en el bus (cableado/dirección/clock-stretch).
2. **`setMode(CONFIG)`** (OPR_MODE 0x3D = 0x00).
3. **Reset por software:** `SYS_TRIGGER (0x3F) = 0x20`; esperar a que `CHIP_ID` vuelva a 0xA0 +
   `delay(30–50)`. ⚠️ Ver advertencia de soft-reset en el SKILL.md (no es seguro en todos los
   lotes para un watchdog de producción; al boot, con el chip recién alimentado, es OK).
4. **`PWR_MODE (0x3E) = 0x00`** (NORMAL).
5. **`PAGE_ID (0x07) = 0`**.
6. **`SYS_TRIGGER (0x3F) = 0x00`** (limpiar trigger).
7. *(opcional)* **`setExtCrystalUse`** → `SYS_TRIGGER = 0x80`. SOLO si hay cristal físico, en
   CONFIG, tras el reset, ANTES del modo de fusión. Sin cristal físico → falla silenciosa.
8. **Escribir offsets guardados** (si tenés, ver abajo) — EN CONFIG MODE.
9. **`setMode(IMUPLUS)`** (0x08) + `delay(20–30)`.
10. **Recién después** leer Euler — y solo si `SYS_STATUS (0x39) == 5`.

## Calibración por sensor (qué mover, qué leer)

`getCalibration()` lee `CALIB_STAT (0x35)`: SYS / GYRO / ACCEL / MAG, cada uno 0..3.

- **GYRO → 3:** robot **QUIETO** unos segundos en cualquier posición (trivial). En este robot
  `init_one_bno` espera hasta `GYRO_CALIB_MS=2000`.
- **ACCEL → 3:** 6 poses estables (+X,−X,+Y,−Y,+Z,−Z), lo más tedioso; usar una escuadra/bloque.
- **MAG → 3:** movimiento normal del dispositivo (Fast Mag Calib; ya no hace falta el figure-8).
- **SYS:** agregado de la fusión.

⚠️ **En IMUPLUS:** MAG queda en 0 (no usa mag) y SYS puede no llegar a 3 — es CORRECTO. Esperá
solo **GYRO=3 (heading) y ACCEL=3 (tilt)**. NUNCA bloquees el boot por MAG/SYS en IMUPLUS.

## Guardar / restaurar offsets (22 bytes) — el chip no tiene EEPROM

1. Una vez calibrado (GYRO=3), **leé los 22 bytes** de offsets (0x55–0x6A). Conviene en CONFIG.
2. **Guardalos** en la EEPROM/flash del MCU host (Teensy).
3. **Al boot, reescribilos EN CONFIG MODE** (paso 8 del bringup). La escritura efectiva de cada
   par ocurre al escribir el **MSB**.

⚠️ Escribir offsets EXIGE **CONFIG mode** (fuera de CONFIG la escritura se ignora o da
`SYS_ERR=6`). El patrón Adafruit: guardar el modo actual → ir a CONFIG con `delay(25)` →
escribir → restaurar el modo.

## Layout EEPROM real del robot (`src/top/sensors_imu.cpp:150–181`)

- `EE_BASE = 320`, `magic = 0xB2`, `version = 1`.
- Por sensor: `[valid (1 byte)] + [blob (22 bytes)]`.
- `ee_load_into_bno(i, bno)` restaura; `ee_save_from_bno(i, bno)` guarda.
- **Auto-save** cada ~200 ticks si `calib_gyro >= 3`.
- ⚠️ **Fallo histórico:** header EEPROM inválido (magic/version no coinciden) → la carga falla
  EN SILENCIO → el sensor no restituye offsets → drift leve tras boot, síntoma "a veces anda,
  a veces no" según cuánto se movió el robot.

⚠️ **Dos EEPROMs distintas** (fuente de confusión): la del CHIP (no existe, se emula en el MCU)
vs la del MCU host (Teensy), donde viven TANTO los offsets de calib COMO los flags de config
(`bno_left_en`, etc.). La trampa del flag (Fase 3c del árbol) vive en la del MCU.
