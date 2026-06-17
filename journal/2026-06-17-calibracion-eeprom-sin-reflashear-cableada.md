---
title: "Calibración por EEPROM sin reflashear — cableado end-to-end (firmware + app) gateado"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.8 1M) + 1 subagente (panel app)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: host-testeado + compila; banco lo cierra el equipo (TASK-112)
tipo: implementacion
---

# Resumen

Sistema de calibración de la CENTRAL que permite tunear potencias de rueda desde el monitor
USB y guardarlas en EEPROM **sin reflashear**. Construido en incrementos sobre el módulo puro
`central_config` (commit `585cd40`). Workflow paralelo: firmware (sesión principal) + panel
de la app (subagente), sobre archivos disjuntos.

# Qué se cableó (end-to-end)

**Firmware (gateado `-DCENTRAL_EEPROM_CALIB`; sin el flag = 0 bytes, competencia byte-idéntica):**
- `src/central/central_eeprom_config.{h,cpp}` (NUEVO, glue Arduino): `g_central_cfg` viva +
  `g_cfg_min_pwm[3]`/`g_cfg_eff[3]` efectivos. `central_eeprom_init()` (defaults de los
  constexpr por robot + override de EEPROM si el blob es válido), `central_eeprom_apply_set()`,
  `central_eeprom_save()`. Offset EEPROM 0 (libre en la CENTRAL). Patrón espejo de `top_eeprom_config`.
- `motors_zircon.cpp`: bajo el flag, el piso y la eficiencia salen de `g_cfg_min_pwm`/`g_cfg_eff`
  (override de EEPROM) en vez de los constexpr — en AMBAS ramas (con y sin `CENTRAL_FLOOR_SCALE`).
- `main_central.cpp` setup: `central_eeprom_init()` antes de `motors_init()`.
- `central_telemetry_serial.cpp`: el `consume_line` ahora despacha `SET`/`GET`/`SAVE`
  (via `cc_parse_command`) → aplica a la config viva / persiste. Bajo `CENTRAL_USB_MONITOR`.
- `platformio.ini`: envs `central_robot2_arquero_calib` y `central_robot1_arquero_calib`
  (= arquero de banco + `-DCENTRAL_EEPROM_CALIB`; ya traen USB_MONITOR + MANUAL_START + FLOOR_SCALE).

**App (subagente, `tools/monitor-base`):**
- `protocol_central.py`: dataclass `CalibConfig` + campo opcional `ccfg` en `CentralFrame`
  (retrocompatible con `.get()`; ausente → None).
- `panel_central_calib.py` (NUEVO): `CentralCalibPanel` ("Calibrar CENTRAL") — campos editables
  con labels en español (piso PWM por rueda, eficiencia, PWM avance izq/der, PID giro Kp/Ki/Kd),
  clamp local, botones "Leer (GET)" / "Aplicar" / "GUARDAR en EEPROM (SAVE)".
- `gui_shell.py`: registrado el panel. Tests: protocol `ccfg` + smoke del panel.

# Qué APLICA hoy vs qué se GUARDA pero no aplica aún

- **APLICA ya:** `min_pwm[3]` + `eff[3]` → es el **strafe lateral** (el síntoma del domingo:
  calibración de potencias). Calibrás desde el panel, guardás, y el robot strafe-a con esos
  valores sin reflashear.
- **Se GUARDA pero NO aplica todavía:** `fwd_pwm_l/r` (avance recto) y `gyro_kp/ki/kd` (PID).
  Su efecto se cablea en el próximo incremento (tocan el mixer de avance y `strategy.cpp`).
  El panel ya los edita y persisten en EEPROM; el firmware los lee pero aún no los usa.

# Decisión de diseño (gate)

- `CENTRAL_EEPROM_CALIB` habilita: variables runtime + load EEPROM + override en motores +
  dispatch SET/GET/SAVE (este último también requiere `CENTRAL_USB_MONITOR`, el canal).
- **Defaults = constexpr por robot.** EEPROM en blanco/corrupta → CRC falla → constexpr →
  comportamiento idéntico. La EEPROM es un override opcional.
- **Competencia byte-idéntica:** `central_robot1`/`central_robot2` NO llevan el flag (compilan
  SUCCESS sin cambio). Para que la calibración sirva EN COMPETENCIA, el equipo agrega
  `-DCENTRAL_EEPROM_CALIB` a esos envs tras validar en banco (mismo criterio que los otros P0).

# Pendiente (próximo incremento)

- **Eco `ccfg` en el frame** para que el GET refrese el panel con los valores del robot. El
  parser de la app YA lo soporta (formato `"ccfg":{"min_pwm":[..],"eff":[..],"fwd_l":..,
  "fwd_r":..,"gkp":..,"gki":..,"gkd":..}`). Falta que el firmware lo emita (ampliar
  `telemetry_central` + golden). Hoy el GET solo re-emite un frame normal.
- **Cablear `fwd_pwm` (avance recto)** al mixer + **PID gyro** a `strategy.cpp`.

# Verificación

- Módulo puro `central_config`: 19/19 host.
- Firmware: `central_robot1/2` (competencia, byte-idéntico) + `central_robot2_arquero_calib` +
  `central_robot1_arquero_calib` compilan SUCCESS.
- App: 323 Python passed (1 fail PRE-EXISTENTE ajeno: `test_tooltip.py`, Tcl mal instalado en
  Python 3.14 local — no relacionado).
- "Compila"/"host verde" NO prueba el efecto en HW: el banco lo cierra el equipo (TASK-112).

# Archivos

**Firmware:** `src/central/central_eeprom_config.{h,cpp}` (nuevo) · `motors_zircon.cpp` ·
`main_central.cpp` · `central_telemetry_serial.cpp` · `platformio.ini` (2 envs calib).
**App:** `tools/monitor-base/monitor_base/{protocol_central.py, panel_central_calib.py, gui_shell.py}` +
tests.
**Docs:** journal (este) + banner ESTADO-ACTUAL + TASK-112.
