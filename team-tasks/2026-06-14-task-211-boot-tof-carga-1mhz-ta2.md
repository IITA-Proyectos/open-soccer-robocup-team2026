---
id: TASK-211
title: "TA-2: cargar el firmware de los ToF a 1 MHz (bench env opt-in) — boot aún más rápido"
date_created: 2026-06-14
assigned: [virginia, elias]
priority: P2
status: done
estimated_hours: 1
blocks: []
depends_on: [TASK-210]
tags: [firmware, top, tof, i2c, boot, performance]
---

# TASK-211 — TA-2: carga de los ToF a 1 MHz (Fast Mode Plus)

> ### ✅ 2026-06-14 — VALIDADO EN BANCO Y PROMOVIDO A DEFAULT (Gustavo + Virginia)
> **Gustavo y Virginia corrieron >15 power-cycles de la TOP (`top_robot2_pri`, COM22): los 4 ToF
> cargaron a 1 MHz en TODOS, CERO fallbacks → ANDA a 1 MHz.** Medido (1ª corrida representativa):
> - **`setup_total` = 9,6 s** (TA-1 400 kHz = 14,4 s · original = ~40 s) · **`tof_init` = 6,86 s** · `imu_init` = 2,5 s.
> - **`4 de 4` ToF midiendo** · `min_obst` vivo (292-510 mm) · **0 mensajes de fallback** en >15 arranques.
> - Heading trackea el giro (`0.0 → -39.2 → +30.6 → -45.0`), sin freeze.
>
> **PROMOVIDO A DEFAULT:** 1 MHz dejó de ser opt-in. Ahora es el comportamiento por defecto en
> `src/top/sensors_tof.cpp` (`TOF_INIT_CLOCK_FAST_HZ`, con fallback a 400 kHz) → **TODOS los programas
> de booteo del TOP** (top_robot1/2, top_robot2_pri, etc.) arrancan a 1 MHz. Se eliminó el flag
> `-DTOP_TOF_INIT_1MHZ` y el env de banco `top_robot2_pri_1mhz` (ya innecesarios).

## Resumen (1 línea)
Cargar el firmware de los 4 VL53L7CX a **1 MHz** (en vez de 400 kHz) durante el boot → `tof_init`
~12 s → **~8 s** esperado. Con **fallback automático a 400 kHz** por sensor si el bus no banca 1 MHz.

## Contexto — por qué P2 (no P1)
- Con TA-1 el boot ya bajó de ~40 s a **~14,4 s** → ya es competitivo para Incheon. TA-2 es la
  frutilla: otros ~4 s. **No es bloqueante.**
- El `tof_init` de TA-1 quedó en ~12 s (no ~8 s) porque, además de la transferencia I2C, hay un
  "piso fijo" (esperas internas del init de cada ToF + settle de los LP) que no escala con el clock.
  A 1 MHz la **transferencia** baja 2,5× respecto de 400 kHz → recorta esa parte del `tof_init`.
- **Riesgo real:** el VL53L7CX soporta 1 MHz y el LPI2C del Teensy 4.0 también, PERO el bus físico
  (pull-ups, capacitancia, bodge de los LP) puede no bancar 1 MHz limpio → carga corrupta → ToF que
  no levanta. Por eso es **opt-in + fallback**, no default.

## Qué se cambió (ya aplicado)
`src/top/sensors_tof.cpp`:
- Constante `TOF_INIT_CLOCK_FAST_HZ = 1000000`.
- En el path MULTI, **gateado por `-DTOP_TOF_INIT_1MHZ`**: intenta `begin()` a 1 MHz; si falla,
  **resetea el sensor por LP** (sleep→wake, porque pudo quedar a medio cargar) y reintenta a
  400 kHz (TA-1). Cada fallback imprime `[sensors_tof] ToF N: carga 1 MHz fallo -> fallback 400 kHz`.
- **Sin el flag → `#else` = exactamente TA-1** (400 kHz). Producción intacta.

`platformio.ini`:
- Nuevo env de banco **`[env:top_robot2_pri_1mhz]`** = `top_robot2_pri` + `-DTOP_TOF_INIT_1MHZ`.

✅ Compila: `top_robot2_pri` (SUCCESS, sin cambios) y `top_robot2_pri_1mhz` (SUCCESS).

## Pasos concretos (banco)
1. `pio run -e top_robot2_pri_1mhz -t upload` (con el Teensy del TOP conectado).
2. **POWER-CYCLE** + `pio device monitor -b 115200`.
3. Anotar `[boot] tof_init=.. ms` y si aparece algún `fallback 400 kHz` en el log.
4. **Repetir el power-cycle ~20 veces** (la integridad a 1 MHz puede ser intermitente).

## Criterio de cierre
- [x] **Velocidad:** `tof_init` = **6,86 s** (vs ~12 s de TA-1) → boot total **9,6 s**. ✅ (superó el objetivo ~8 s).
- [x] **Fiabilidad:** **>15 power-cycles**, `4 de 4 midiendo` SIEMPRE y **0 mensajes de fallback**. ✅
- [x] **Sin regresión:** heading trackea el giro, sin freeze; runtime intacto a 100 kHz. ✅

### Decisión post-banco → PROMOVIDO ✅
Pasó limpio (0 fallbacks en >15 ciclos) → **1 MHz es ahora el DEFAULT de producción**, hecho en el
código (no por flag): `src/top/sensors_tof.cpp` carga a `TOF_INIT_CLOCK_FAST_HZ` (1 MHz) con fallback
a 400 kHz. Aplica a TODOS los programas de booteo del TOP. Eliminados el flag `-DTOP_TOF_INIT_1MHZ`
y el env de banco `top_robot2_pri_1mhz`. Ambos robots arrancan en ~9,6 s al reflashear desde `main`.

## Riesgos
- **risk-no-fix:** se quedan en ~14 s de boot (ya competitivo). Cero impacto en partido.
- **risk-fix:** muy bajo por el diseño opt-in + fallback. Lo peor que puede pasar en el env de banco
  es que algún ToF cargue a 400 kHz (fallback) → mismo resultado funcional que TA-1, sólo sin el
  ahorro extra. Producción no se toca hasta la decisión post-banco.

## Notas / decisiones
- 2026-06-14: creada + código aplicado por el coach (Claude Opus 4.8, pedido de Gustavo), tras
  cerrar TA-1 (TASK-210) en banco con Virginia + Gustavo.
- 2026-06-14: **validado en banco por Gustavo + Virginia** (>15 power-cycles, 0 fallbacks, ANDA a
  1 MHz) → **promovido a default**. Boot final: **~9,6 s** (era ~40 s). Cadena boot completa:
  ~40 s (original) → 14,4 s (TA-1, 400 kHz) → **9,6 s (TA-2, 1 MHz)**. Tema RESUELTO y cerrado.

## Cambios de estado
- 2026-06-14: creada → `codigo-listo-falta-banco` (env de banco listo y compilando; falta HW).
- 2026-06-14: `codigo-listo-falta-banco` → `done` ✅ (validado por Gustavo + Virginia; 1 MHz promovido a default).
