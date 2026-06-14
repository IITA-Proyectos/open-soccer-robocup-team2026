---
id: TASK-211
title: "TA-2: cargar el firmware de los ToF a 1 MHz (bench env opt-in) — boot aún más rápido"
date_created: 2026-06-14
assigned: [virginia, elias]
priority: P2
status: codigo-listo-falta-banco
estimated_hours: 1
blocks: []
depends_on: [TASK-210]
tags: [firmware, top, tof, i2c, boot, performance]
---

# TASK-211 — TA-2: carga de los ToF a 1 MHz (Fast Mode Plus)

> ### ⏳ 2026-06-14 — CÓDIGO APLICADO (bench env), FALTA VALIDAR EN BANCO
> Continuación de [TASK-210](2026-06-14-task-210-acelerar-boot-tof-carga-400khz.md) (TA-1, ya
> cerrada: 400 kHz → boot ~14,4 s). TA-2 sube la carga a **1 MHz** para bajar otros ~4 s.
> Implementado como **env de banco opt-in** (`top_robot2_pri_1mhz`); **producción
> (`top_robot2_pri`) queda BYTE-IDÉNTICA a TA-1** hasta validar. Compila OK; falta banco.

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
- [ ] **Velocidad:** `tof_init` baja respecto de los ~12 s de TA-1 (objetivo ~8 s).
- [ ] **Fiabilidad:** en ~20 power-cycles, **`4 de 4 midiendo` SIEMPRE** y **0 mensajes de fallback**
      (si aparece fallback aunque sea a veces → 1 MHz es marginal en este bus → quedarse en TA-1).
- [ ] **Sin regresión:** heading trackea el giro (igual que TA-1), cámaras + DOWN OK.

### Decisión post-banco
- **Si pasa limpio** (0 fallbacks en 20 ciclos) → promover: mover `-DTOP_TOF_INIT_1MHZ` a
  `top_robot2_pri` (default de producción) y reflashear ambos robots.
- **Si hay fallbacks** → dejar producción en TA-1 (400 kHz). El env de 1 MHz queda como histórico.
  Opcional: revisar pull-ups del bus I2C (bajar a 2,2 kΩ) y reintentar.

## Riesgos
- **risk-no-fix:** se quedan en ~14 s de boot (ya competitivo). Cero impacto en partido.
- **risk-fix:** muy bajo por el diseño opt-in + fallback. Lo peor que puede pasar en el env de banco
  es que algún ToF cargue a 400 kHz (fallback) → mismo resultado funcional que TA-1, sólo sin el
  ahorro extra. Producción no se toca hasta la decisión post-banco.

## Notas / decisiones
- 2026-06-14: creada + código aplicado por el coach (Claude Opus 4.8, pedido de Gustavo), tras
  cerrar TA-1 (TASK-210) en banco con Virginia + Gustavo.

## Cambios de estado
- 2026-06-14: creada → `codigo-listo-falta-banco` (env de banco listo y compilando; falta HW).
