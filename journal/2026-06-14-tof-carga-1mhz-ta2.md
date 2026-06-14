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
- ✅ Compila: `top_robot2_pri` (SUCCESS, sin cambios) + `top_robot2_pri_1mhz` (SUCCESS).
- ⏳ Banco PENDIENTE: `pio run -e top_robot2_pri_1mhz -t upload`, ~20 power-cycles. Cierre en TASK-211:
  `tof_init` < ~12 s + **0 fallbacks** en el log + 4/4 ToF siempre. Si hay fallbacks → quedarse en TA-1.

## Decisión post-banco
Si pasa limpio → mover `-DTOP_TOF_INIT_1MHZ` a `top_robot2_pri` (default) y reflashear ambos robots.
Si no → producción se queda en TA-1 (ya competitivo); opcional revisar pull-ups (2,2 kΩ) y reintentar.
