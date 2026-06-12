---
title: "Banco práctica 2026-06-12 — María/R2: arquero strafe (v1→v6 PFM), monitor USB en competencia, fix sensores débiles, calibración línea"
date: 2026-06-12
author: "María Virginia Viollaz (banco) + Claude (firmware/coach)"
status: sesión EN CURSO al momento del corte (cambio de chat por contexto) — v6 compilada SIN flashear/probar
---

# Banco 2026-06-12 — María con ROBOT 2 (día completo)

## Validado en hardware HOY ✅

1. **Botón fantasma del Zircon (pin 9)**: el pulsador onboard quedó clavado → GO
   permanente (el robot patrullaba al prender; el STOP se re-disparaba solo).
   Mitigado por software: flag `CENTRAL_MANUAL_START_NO_BUTTON` + envs `*_nobtn`.
   Pendiente hardware: revisar/cambiar el pulsador.
2. **Monitor USB EN el binario de competencia de DOWN** (pedido María, TASK-306):
   `-DDOWN_USB_MONITOR` en envs `down`/`down_robot2`. Validado en banco 3/3:
   dormido al boot ✅ · streamea 20 Hz con la app (STREAM ON + PING keepalive) ✅
   · **se apaga solo a los ~3 s sin host** (sacar cable = modo partido) ✅.
   + Fixes TASK-306: CAL_* re-deriva el DownModel EN VIVO (antes: hasta reboot),
   CAL_SAVE con ACK/NAK, CAL_AUTO_OFF con sanity-check, getters por-OTOS gateados
   también para el monitor.
3. **App monitor-base**: keepalive PING 1 s + STREAM ON al conectar; título con
   fuente; banner rojo SIMULADOR; guarda anti-comandos-en-sim; panel CALIBRAR
   guiado (1·Verde 2·Blanco 3·Guardar) + grilla semáforo 32 sensores + veredicto.
   82/82 pytest. La app del juez de línea para Corea quedó utilizable sin IA.
4. **Fix sensores débiles** (raíz del "no detecta línea"): S01/S08 de R2 son
   físicamente flojos (~70 counts de rango vs ~280) y UN débil invalidaba TODA la
   calib (`lc_is_suspect`) → robot ciego de línea. Ahora `lc_count_weak` +
   exclusión por-sensor del centroide (patrón EV_SENSOR_NOISY); data_valid=1
   hasta `max_weak_sensors=4` débiles (EV_CALIB_SUSPECT avisa); 5+ invalida como
   siempre. Default 0 = semántica histórica (tests viejos intactos). +9 tests.
5. **Calibración de línea de R2 guardada** (EEPROM + respaldo en
   `docs/pruebas-banco/datos-banco-2026-06-12/calib-linea-r2-2026-06-12.txt`):
   29/32 sensores con margen ≥40 (S01/S08 débiles físicos, S19 al límite).
   Lección de método: el calibrador captura el blanco INSTANTÁNEO → hoja blanca
   grande cubriendo TODO el anillo, sin sombras (26 malos → 1 con la hoja bien).
6. **Arquero "strafe simple"** (`central_robot2_arquero_strafe_bb`, gateado
   `GK_SIMPLE_STRAFE`): v3/v4 con pausas VALIDADA en banco por María — strafe
   lateral, rebote por línea con **ESCAPE de ~12 cm SIN leer sensores antes de
   decidir** (fix diagnosticado POR MARÍA: parado sobre la línea hacía "cosas
   raras"), frente al arco rival por pulsos en pausas.

## La saga del control de rumbo (planta IDENTIFICADA con datos)

- ω continuo capado 40°/s durante strafe 200 mm/s → **runaway** (pierde contra
  la deriva parásita ~80°/s). ω continuo kp=3 capado 120°/s → **oscilación
  violenta ±140° + trompo** (actuador cuantizado por pisos: giro todo-o-nada).
- Conclusión de ingeniería: a 200 mm/s el robot está en régimen CUANTIZADO; el
  control fino continuo clásico es inviable. **Skills nuevas** (en
  `.claude/skills/`, también para el Claude de Elías):
  `control-pid-zona-muerta` (PFM/duty-cycling, deadband, PI-feedforward,
  titración) y `dinamica-omni-3-ruedas` (la planta medida: pisos, regímenes,
  parásita, mínimos físicos, hechos validados de banco).
- **v6 implementada** según las skills: `src/shared/pfm_heading.h` (puro, 8
  tests) — PI + PFM: corrección entregada en pulsos de magnitud fija (100°/s)
  durante fracción de ventanas de 160 ms; deadband 5°; integrador anti-windup
  que APRENDE la deriva (auto-calibración); red de seguridad re-escuadre a 45°.
  **COMPILADA, NO FLASHEADA NI PROBADA — primer paso de la próxima sesión.**

## Hallazgos de hardware del día (anotar/reparar)

- ⚠️ **Cable USB del banco: 5 cortes en la sesión**, 2 veces dejó un STOP sin
  enviar (robot corriendo sin control del juez-PC). CAMBIARLO antes de seguir.
- ⚠️ **El BNO de R2 se congeló 2 veces en frío** (hdg bit-clavado, girándolo a
  mano; revive con power-cycle de 10 s). Distinto del caso R1 (que era el bus).
  Refuerza F4 del backlog (detector de muerte del BNO). **Regla de banco: antes
  de CADA prueba de movimiento, girar el robot a mano y verificar que hdg
  trackea.**
- ⚠️ **Cámara frontal de R2 muda** (pkts_F=0; la trasera anda). Pendiente.
- El robot calienta con batería conectada en reposo (revisar con Enzo).
- Conector USB de la DOWN también flojo (2 cortes).

## Proceso

- **Regla nueva aprendida (memoria + aplicada todo el día): el SUCCESS de pio
  upload NO garantiza que el binario llegó** — verificar SIEMPRE el panel serial
  con un marcador único del binario antes de juzgar conducta (nos comió ~40 min
  con la patrulla vieja corriendo disfrazada de strafe).
- Gates: host 59 suites / 819+8 tests verdes (down_calib 17, down_model 25,
  pfm_heading 8); pio: 5 envs DOWN + strafe_bb + regresiones SUCCESS; app 82/82.
- TASK-306 creada (app confiable para Incheon). PR #18 mergeado a la mañana.
- Commits del día a nombre de María (git config local del repo).
