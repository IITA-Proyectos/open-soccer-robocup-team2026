# 2026-06-14 — TA-2: carga de los ToF a 1 MHz (bench env opt-in)

**Autor:** Claude Opus 4.8 (coach), a pedido de Gustavo.
**Tipo:** cambio de firmware (host: compila; falta banco).
**TASK:** [TASK-211](../team-tasks/2026-06-14-task-211-boot-tof-carga-1mhz-ta2.md). Continúa [TASK-210](../team-tasks/2026-06-14-task-210-acelerar-boot-tof-carga-400khz.md) (TA-1).

## Contexto
TA-1 (400 kHz) ya cerró en banco: boot ~40 s → ~14,4 s, `tof_init` ~12 s. El `tof_init` no bajó a
~8 s porque hay un piso fijo (esperas internas del init de cada ToF + settle de los LP) que no
escala con el clock. TA-2 sube SOLO la carga a 1 MHz para recortar la parte de transferencia.

## Qué se hizo
- `src/top/sensors_tof.cpp`: constante `TOF_INIT_CLOCK_FAST_HZ=1000000`; en el path MULTI, gateado
  por `-DTOP_TOF_INIT_1MHZ`, intenta cargar a 1 MHz y si falla **resetea el sensor por LP y recae a
  400 kHz** (TA-1), con log por sensor. Sin el flag, byte-idéntico a TA-1.
- `platformio.ini`: env de banco `top_robot2_pri_1mhz` (= `top_robot2_pri` + el flag).

## Por qué opt-in + fallback (no default)
El VL53L7CX y el LPI2C del Teensy 4.0 soportan 1 MHz, pero el bus FÍSICO (pull-ups, capacitancia,
bodge de los LP) puede no bancarlo → carga corrupta. El fallback garantiza que TA-2 nunca quede
funcionalmente peor que TA-1. Producción (`top_robot2_pri`) intacta hasta validar en banco.

## Verificación
- ✅ Compila: `top_robot2_pri` + `top_robot1` (SUCCESS).
- ✅ **BANCO (Gustavo + Virginia, 2026-06-14, TOP COM22): ANDA a 1 MHz.** >15 power-cycles, los 4 ToF
  cargaron a 1 MHz en TODOS, **0 fallbacks**. Medido: `tof_init`=6,86 s, `setup_total`=**9,6 s**,
  `imu_init`=2,5 s; `4 de 4 midiendo`; `min_obst` vivo (292-510 mm); heading trackea el giro sin freeze.

## Decisión post-banco → PROMOVIDO A DEFAULT ✅
Pasó limpio → 1 MHz dejó de ser opt-in. Promovido en el CÓDIGO (`sensors_tof.cpp`:
`TOF_INIT_CLOCK_FAST_HZ` con fallback a 400 kHz) → TODOS los programas de booteo del TOP arrancan a
1 MHz. Eliminados el flag `-DTOP_TOF_INIT_1MHZ` y el env de banco `top_robot2_pri_1mhz`.

## Cadena completa de boot (cierre del tema)
**~40 s (original) → 14,4 s (TA-1, 400 kHz, TASK-210) → 9,6 s (TA-2, 1 MHz, TASK-211).** 4,2× más rápido.
Runtime intacto a 100 kHz (anti-freeze del BNO). Ambos robots arrancan en ~9,6 s al reflashear desde `main`.
