# BNO055 — registros, modos, escalas y códigos de estado (página 0)

> Tabla de consulta. La verdad que MANDA es el datasheet Bosch (BST-BNO055-DS000) + el header
> `Adafruit_BNO055.h` (ground-truth del driver). Todos los registros de abajo son página 0.

## Mapa de registros (página 0)

| Reg | Nombre | Qué es |
|---|---|---|
| 0x00 | CHIP_ID | debe leer **0xA0** (chequeo de vida #1) |
| 0x07 | PAGE_ID | 0 = página 0 (datos); 1 = config de sensores |
| 0x08–0x0D | ACC_DATA X/Y/Z | accel crudo |
| 0x0E–0x13 | MAG_DATA X/Y/Z | mag crudo |
| 0x14–0x19 | GYR_DATA X/Y/Z | gyro crudo (yaw_rate en Z, 16 LSB/dps) |
| 0x1A–0x1F | EUL Heading/Roll/Pitch | Euler fusionado (Heading = yaw, 16 LSB/°) |
| 0x20–0x27 | QUA W/X/Y/Z | quaternion fusionado (2^14 LSB/unidad) |
| 0x28–0x2D | LIA_DATA X/Y/Z | aceleración lineal (sin gravedad) |
| 0x2E–0x33 | GRV_DATA X/Y/Z | vector gravedad |
| 0x34 | TEMP | temperatura |
| 0x35 | CALIB_STAT | calib SYS/GYR/ACC/MAG (ver abajo) |
| 0x36 | ST_RESULT | resultado self-test (ver abajo) |
| 0x37 | INT_STA | estado de interrupciones |
| 0x38 | SYS_CLK_STATUS | bit0: 0=se puede configurar reloj, 1=en uso |
| 0x39 | SYS_STATUS | estado del sistema (ver abajo) |
| 0x3A | SYS_ERR | código de error (ver abajo) |
| 0x3B | UNIT_SEL | unidades (ver abajo) |
| 0x3D | OPR_MODE | modo de operación (ver tabla) |
| 0x3E | PWR_MODE | 0=NORMAL, 1=LOW_POWER, 2=SUSPEND |
| 0x3F | SYS_TRIGGER | 0x80=ext crystal, 0x20=RST_SYS, 0x40=RST_INT |
| 0x41 | AXIS_MAP_CONFIG | remap de ejes (solo en CONFIG) |
| 0x42 | AXIS_MAP_SIGN | signo de ejes (solo en CONFIG) |
| 0x55–0x5A | ACC_OFFSET X/Y/Z | offset accel (LSB primero) |
| 0x5B–0x60 | MAG_OFFSET X/Y/Z | offset mag |
| 0x61–0x66 | GYR_OFFSET X/Y/Z | offset gyro |
| 0x67–0x68 | ACC_RADIUS | radio accel |
| 0x69–0x6A | MAG_RADIUS | radio mag |

**Offsets = 22 bytes contiguos 0x55–0x6A** (NUM_BNO055_OFFSET_REGISTERS). La escritura efectiva
de cada par ocurre al escribir el **MSB**. Leer/escribir offsets conviene/EXIGE CONFIG mode.

## OPR_MODE (0x3D) — los 13 modos

| Valor | Modo | Fusiona | Heading |
|---|---|---|---|
| 0x00 | CONFIG | nada | — (único para offsets/axis-remap/UNIT_SEL) |
| 0x01 | ACCONLY | accel crudo | — |
| 0x02 | MAGONLY | mag crudo | — |
| 0x03 | GYRONLY | gyro crudo | — |
| 0x04 | ACCMAG | crudo | — |
| 0x05 | ACCGYRO | crudo | — |
| 0x06 | MAGGYRO | crudo | — |
| 0x07 | AMG | accel+mag+gyro crudo (sin fusión) | — |
| **0x08** | **IMU / IMUPLUS** | **accel+gyro** | **RELATIVO al power-on, drift lento** |
| 0x09 | COMPASS | accel+mag | absoluto (norte), lento |
| 0x0A | M4G | accel+mag (estilo gyro magnético) | relativo magnético |
| 0x0B | NDOF_FMC_OFF | accel+gyro+mag | absoluto, fast-mag-cal OFF |
| 0x0C | NDOF | accel+gyro+mag | ABSOLUTO al norte, auto-corrige drift |

Delays de cambio de modo: ~7 ms CONFIG→operación, ~19 ms operación→CONFIG (Adafruit usa
`delay(30)`). Toda transición pasa por CONFIG.

## UNIT_SEL (0x3B) y escalas

| Bit | Campo | 0 | 1 |
|---|---|---|---|
| 0 | accel | m/s² | mg |
| 1 | gyro | dps | rps |
| 2 | Euler | **grados** | radianes |
| 4 | temp | °C | °F |
| 7 | orientación | Windows | Android |

Escalas con default (grados / m/s²):
- **EULER: 1° = 16 LSB** → `heading_deg = raw_int16 / 16.0`
- gyro: 1 dps = 16 LSB
- accel / lin-accel / gravity: 1 m/s² = 100 LSB
- quaternion: 1 unidad = 2^14 = 16384 LSB
- temp: 1 °C = 1 LSB

## CALIB_STAT (0x35)

`SYS=(reg>>6)&3`, `GYRO=(reg>>4)&3`, `ACCEL=(reg>>2)&3`, `MAG=reg&3`. Cada uno 0..3 (0=sin
calibrar, 3=full). En **IMUPLUS**: MAG se queda en 0 (correcto, no usa mag), SYS puede no llegar
a 3; **solo importan GYRO=3 y ACCEL=3**.

## ST_RESULT (0x36) — self-test

bit0=accel, bit1=mag, bit2=gyro, bit3=MCU. **0x0F = los 4 self-tests OK.**

## SYS_STATUS (0x39)

| Valor | Significado |
|---|---|
| 0 | idle |
| 1 | **ERROR** (leé SIEMPRE SYS_ERR 0x3A) |
| 2 | inicializando periféricos |
| 3 | inicializando sistema |
| 4 | corriendo self-test |
| **5** | **fusión corriendo** (lo que querés en IMUPLUS/NDOF) |
| 6 | corriendo SIN fusión (modo raw) |

## SYS_ERR (0x3A)

| Valor | Error |
|---|---|
| 0 | sin error |
| 1 | init de periférico falló |
| 2 | init de sistema falló |
| 3 | self-test falló |
| 4 | valor de registro fuera de rango |
| 5 | dirección de registro fuera de rango |
| 6 | **error de escritura** (típico: escribiste fuera de CONFIG mode) |
| 7 | modo low-power no disponible para ese modo |
| 8 | accel power-mode no disponible |
| 9 | error de config del algoritmo de fusión |
| 10 | error de config de sensor |

## Axis remap (AXIS_MAP_CONFIG 0x41 / AXIS_MAP_SIGN 0x42)

Solo en CONFIG. Default config = 0x24 (X=X, Y=Y, Z=Z). Reasigna/invierte ejes para alinear el
frame del chip con el montaje físico SIN tocar matemática en el firmware (preferible a corregir
ángulos a mano). El plano X-Y debe quedar horizontal para que EUL_Heading sea el yaw del robot.
Bosch define presets P0–P7 (combinaciones de remap+signo); P1 es el default.
