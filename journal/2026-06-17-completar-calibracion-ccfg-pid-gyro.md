---
title: "Completar calibración: eco ccfg (GET-refresh) + PID gyro cableado; veredicto honesto sobre fwd_pwm"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.8 1M) + 2 subagentes (mapeo + panel Hz)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: host-testeado + compila; banco = TASK-112
tipo: implementacion
---

# Resumen

Continuación de la calibración por EEPROM (commit `5031090`). Se cerró el código pendiente
identificado, con un veredicto honesto sobre uno de los parámetros. Objetivo del pedido:
"completar el código para que solo reste pruebas y calibración". Lo logrado: el código de
calibración queda completo y gateado; lo que resta es banco (no codificar).

# Qué se completó

## A1 — Eco `ccfg` en el frame (GET-refresh) ✅
- `telemetry_central.{h,cpp}`: el frame ahora tiene un sub-objeto OPCIONAL `ccfg` (campos
  `has_ccfg` + min_pwm/eff/fwd/gyro). El serializador lo emite SOLO si `has_ccfg` (sin el flag,
  frame byte-idéntico). Tests host nuevos: `test_tc_ccfg_emitted_when_has_ccfg` +
  `test_tc_no_ccfg_when_flag_off` (12/12 verde; golden sin cambio).
- `central_telemetry_serial.cpp`: bajo `-DCENTRAL_EEPROM_CALIB`, `fill_frame` llena `ccfg`
  desde `g_central_cfg`. El comando `GET` (que ya re-emitía un frame) ahora trae los valores
  → el panel "Calibrar CENTRAL" se refresca con lo que el robot tiene en EEPROM.
- Formato exacto coordinado con el parser de la app:
  `"ccfg":{"min_pwm":[..],"eff":[..],"fwd_l":..,"fwd_r":..,"gkp":..,"gki":..,"gkd":..}`.

## A3 — PID de rumbo (giroscopio) calibrable ✅
- `strategy.cpp` `strategy_init()`: bajo `-DCENTRAL_EEPROM_CALIB`, si `g_central_cfg.gyro_kp != 0`
  se sobreescriben las ganancias del `g_heading_pid` (kp/ki/kd = gyro_*/1000.0f). Hay UN solo
  `HeadingPID` global que usan arquero y delantero → un único punto de inyección.
  **Default no-op:** sin calibración válida (`gyro_kp == 0`) el PID queda IDÉNTICO al de hoy.
  `central_eeprom_init()` (setup línea 194) corre ANTES de `strategy_init()` (línea 214).
- Esto le da al equipo una perilla para **matar la oscilación** desde el monitor, sin reflashear.

## A2 — Avance recto (`fwd_pwm_l/r`): NO se cableó (veredicto honesto)
- El mapeo de un subagente confirmó contra la cinemática (`kinematics.cpp`): en el **omni de 3
  ruedas** el avance recto (vy) usa las 3 ruedas con componentes acoplados — las dos delanteras
  empujan en sentidos NO paralelos (a 120°). Un "PWM izquierda/derecha" separado **no mapea 1:1**
  y forzarlo daría rotación parásita.
- **Decisión coach:** NO meter código confuso para "completar". El "avance derecho" se calibra
  honestamente con lo que YA existe: **`eff[3]`** (balance de potencia por rueda, open-loop) +
  el **PID gyro** (corrección activa de rumbo, closed-loop, ahora calibrable por A3). Entre los
  dos, el avance derecho está cubierto.
- `fwd_pwm_l/r` queda en el struct/EEPROM/panel (se guardan, inocuo) como campo RESERVADO; su
  efecto no se cablea porque sería redundante/engañoso. Documentado en TASK-112.

## B — Panel de Hz por placa
- Verificado por un subagente: el panel `CentralRatesPanel` YA estaba creado y registrado
  (sesión P2). No requirió código nuevo. Muestra Hz de llegada TOP/DOWN/telemetría + Hz de
  cambio + flapping de heading.

# Estado de la calibración (cierre)

| Parámetro | Aplica | Cómo |
|---|---|---|
| `min_pwm[3]` | ✅ | piso por motor (strafe lateral) |
| `eff[3]` | ✅ | eficiencia por rueda (strafe + avance derecho open-loop) |
| `gyro_kp/ki/kd` | ✅ | PID de rumbo (corrección activa; mata oscilación) |
| `fwd_pwm_l/r` | ⏸ reservado | el avance derecho se hace con eff + PID gyro (omni-3 no permite PWM izq/der separado) |
| GET-refresh (ccfg) | ✅ | el panel ve los valores del robot |

# Verificación

- `telemetry_central` 12/12 (ccfg) + `central_config` 19/19.
- Firmware: `central_robot1/2` (competencia byte-idéntica) + `central_robot{1,2}_arquero_calib`
  compilan SUCCESS.
- App: 322 Python passed (1 skip Tk; tooltip pre-existente deseleccionado).
- "Compila" no es "funciona": el efecto de la calibración (incluido el PID) lo cierra el banco (TASK-112).

# Qué queda (honesto) — todo BANCO, no código

El código de las funcionalidades relevadas está completo y gateado. Lo que resta para "robot
operativo" es banco/calibración (no codificar):
- TASK-110 (FLOOR_SCALE arquero R1 · watchdog · pose XY).
- TASK-111 (lazo RT de línea).
- TASK-112 (calibración EEPROM: ahora incluye PID gyro y GET-refresh).
- Calibración de cámaras en sede (TASK-022/214).

# Archivos

`src/shared/telemetry_central.{h,cpp}` · `src/central/central_telemetry_serial.cpp` ·
`src/central/strategy.cpp` · `test/test_telemetry_central/test_main.cpp` + docs.
