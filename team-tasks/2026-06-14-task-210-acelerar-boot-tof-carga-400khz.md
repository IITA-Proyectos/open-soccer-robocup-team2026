---
id: TASK-210
title: "Acelerar el boot del TOP: cargar el firmware de los 4 ToF a 400 kHz (TA-1)"
date_created: 2026-06-14
assigned: [virginia, elias]
priority: P1
status: codigo-listo-falta-banco
estimated_hours: 1
blocks: []
tags: [firmware, top, tof, i2c, boot, performance]
depends_on: []
---

# TASK-210 — Acelerar el boot del TOP cargando los ToF a 400 kHz (TA-1)

> ### ⏳ 2026-06-14 — CÓDIGO APLICADO, FALTA VALIDAR EN BANCO
> El cambio ya está en `src/top/sensors_tof.cpp` + instrumentación de tiempos en
> `src/top/main_top.cpp`. **NO está validado en hardware** (regla no negociable:
> sólo el equipo cierra TASKs de hardware). Esta task es el plan de banco para cerrarlo.

## Resumen (1 línea)
El boot del TOP tarda ~40 s; ~30-32 s son la carga del firmware de los 4 ToF VL53L7CX
**a 100 kHz**. Cargarlos a **400 kHz** (sólo durante la carga, volviendo a 100 kHz para
el runtime) baja esa fase a ~8 s → boot total ~40 s → **~12-13 s**.

## Contexto — por qué importa (P1)
- En partido, si un juez resetea el robot o se reinicia por watchdog, hoy se pierden ~40 s
  de juego. En Incheon los robots entran y salen: cada reinicio es tiempo de cancha real.
- **La causa del cuello de botella NO es que "los ToF sean lentos": es el clock.** El bus
  está clavado a 100 kHz por la **coexistencia BNO+ToF en runtime** (a 400 kHz, leer el
  BNO multi-byte *mientras los ToF rangean* congela el yaw — banco 2026-06-02/06-08).
- **Pero la carga del firmware NO es runtime:** pasa en `setup()`, con el BNO ya iniciado
  y **sin que nadie lo lea** (el loop todavía no arrancó). Es la condición "ToF-solo".
- **Esa condición ya está validada a 400 kHz en este hardware:** `diag_top_tof_quad_live.cpp`
  (líneas 118 y 146) enumera y carga los 4 ToF **a 400 kHz**, validado en banco 2026-05-30.
  El propio código lo dice: *"400 kHz solo servía con ToF-solo o BNO-solo"* (sensors_imu.cpp:242).

**En criollo:** producción bajó TODO a 100 kHz para matar el freeze de runtime, y de paso
se comió 4× el tiempo de carga sin necesidad. La carga puede ir rápida y el runtime quedarse lento.

## Qué se cambió (ya aplicado en el código)
En `src/top/sensors_tof.cpp`:
1. Dos constantes nuevas: `TOF_INIT_CLOCK_HZ = 400000` (carga) y `TOF_RUN_CLOCK_HZ = 100000` (runtime).
2. Los `begin()` de los ToF (path MULTI y single) cargan a `TOF_INIT_CLOCK_HZ`.
3. **`Wire.setClock(TOF_RUN_CLOCK_HZ)` OBLIGATORIO al final del init**, antes de que arranque
   el loop. Sin esto se reintroduce el freeze del heading. (Es el punto a verificar en banco.)

En `src/top/main_top.cpp`:
4. Instrumentación de boot: imprime `[boot] imu_init=.. tof_init=.. setup_total=.. ms` al final
   del `setup()` para medir el antes/después.

**Lo que NO se tocó (a propósito):** el clock de runtime sigue en 100 kHz → el comportamiento
del loop (anti-freeze BNO+ToF) es byte-equivalente al de hoy. El cambio es sólo en la fase de carga.

## Pasos concretos (banco)
1. **Medir baseline (ANTES):** flashear el `top_robot2_pri` ACTUAL (sin el cambio, o sea el de
   producción de hoy) y anotar el `[boot] tof_init=.. ms`. Esperado ~30-32 s.
   - Atajo: si no querés re-flashear el viejo, el número viejo ya está documentado (~30-32 s);
     basta con confirmar que el NUEVO da ~8 s.
2. Flashear el firmware NUEVO: `pio run -e top_robot2_pri -t upload`.
3. **POWER-CYCLE** (las direcciones I2C de los ToF persisten con 3V3 — sin power-cycle puede dar
   enumeración rara). Abrir `pio device monitor -b 115200`.
4. Leer el `[boot] tof_init=.. ms` nuevo y los `[sensors_tof] multi-ToF ... de 4 midiendo`.

## Criterio de cierre
- [ ] **Velocidad:** `[boot] tof_init` ≤ ~10 s (vs ~30-32 s antes) y `setup_total` ≤ ~15 s.
- [ ] **Sin regresión del heading (EL PUNTO CRÍTICO):** girar el robot a mano ~30 s → el `hdg`
      del panel `[TOP]` **TRACKEA el giro** (no se congela). Esto valida que el `setClock(100000)`
      de vuelta quedó bien puesto.
- [ ] **ToF sanos:** `4 de 4 midiendo` y `min_obst` baja al acercar la mano al frente.
- [ ] **Regresión vecinos:** 5 min de marcha → WorldSnapshot sigue ~100 Hz, sin resets espurios
      del watchdog (el WDT se arma DESPUÉS del init, así que la carga más rápida no lo afecta).

## Plan de prueba en hardware real (resumen)
Banco TOP con BNO + 4 ToF activos. Robot apuntando al arco rival al encender (invariante del boot:
el cero del BNO se captura ahí). Comparar `[boot] tof_init` antes/después + verificar que el yaw
no se congela girando el robot. Si el heading SÍ se congela → el restore a 100 kHz falló: revisar
que `Wire.setClock(TOF_RUN_CLOCK_HZ)` esté al final de `sensors_tof_init()` en TODOS los paths.

## Riesgos
- **risk-no-fix:** boot sigue ~40 s; recuperación lenta tras reset en partido, estrés en kickoff.
- **risk-fix:** muy bajo. Es el clock que `diag_top_tof_quad_live` ya corre OK en banco. El único
  cuidado (el `setClock(100000)` de vuelta) es verificable en 30 s mirando que el yaw no se congele.

## Próximos pasos (NO en esta task — sólo si TA-1 cierra OK)
- **TA-2 (P2):** probar la carga a **1 MHz** (Fast Mode Plus) → tof_init ~3-4 s. No validado en
  este hardware (depende de pull-ups/capacitancia); requiere fallback a 400 kHz si `begin()` falla.
- **TA-3 (P2):** solapar la estabilización del BNO (Wire2) con la carga de los ToF (Wire) → -2-3 s.
- **TA-4 (P2):** titular `STABILIZE_MS`/`LP_SETTLE_MS` (hoy conservadores) → -1-2 s.

## Notas / decisiones
- 2026-06-14: creada + código aplicado por el coach (Claude Opus 4.8, pedido de Gustavo). Análisis
  completo de dónde se va el tiempo de boot en la sesión de chat de esa fecha.

## Cambios de estado
- 2026-06-14: `pending` → `codigo-listo-falta-banco` (cambio aplicado en firmware; falta validar HW).
