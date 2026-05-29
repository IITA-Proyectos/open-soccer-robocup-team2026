---
id: TASK-301
title: "Validar en hardware real las 3 mejoras de robustez DOWN (P0.2 calib EEPROM + P1.5 all-white + P1.6 backpressure)"
date_created: 2026-05-29
assigned: [virginia-viollaz, elias, gviollaz]
priority: P1
status: pending
estimated_hours: 1.5
blocks: [confianza operativa DOWN en torneo]
blocked_by: [TASK-031 (UART real DOWN↔TOP/CENTRAL), placa DOWN energizada en banco]
tags: [hardware, down-board, robustez, calib, eeprom, uart, linea]
---

# TASK-301 — Validar HW: robustez DOWN (P0.2 + P1.5 + P1.6)

## Resumen

El 2026-05-29 se implementaron en firmware los 3 hallazgos in-scope del
audit `research/in-progress/2026-05-29-auditoria-exhaustiva-placa-down.md`.
**Todos compilan (`pio run -e down` OK) y la lógica pura está testeada
host-native (g++), pero NINGUNO se validó en hardware real.** Claude no
cierra tasks de hardware (regla 1 de CLAUDE.md) — este TASK lo cierra el
equipo con la placa en mano.

Los 3 cambios viven en:
- `src/shared/line_filters.{h,cpp}` (`lf_all_white`)
- `src/shared/down_model.cpp` (uso de saturación en `dm_update`)
- `src/down/comm_central.{h,cpp}` (calib EEPROM + backpressure Serial1)
- `src/down/comm_top.{h,cpp}` (backpressure Serial5)
- `src/down/main_down.cpp` (carga de calib al boot)

## Criterio de cierre

### Criterio 1 — P0.2: calibración sobrevive al power cycle
- [ ] Cargar `pio run -e down -t upload`. Abrir Serial Monitor 115200.
- [ ] Calibrar: enviar comando carpet (paso 0) sobre carpet, luego comando
      blanco (paso 1) sobre la línea blanca. Confirmar print
      `[DOWN] calib persistida en EEPROM`.
- [ ] Verificar `data_valid=1` en las tramas que salen.
- [ ] Desconectar batería + USB 10 s. Reconectar.
- [ ] Confirmar print `[DOWN] calib cargada de EEPROM (persistida)` al boot
      y `data_valid=1` SIN recalibrar. (Si imprime "EEPROM sin calib valida"
      → la persistencia falló: documentar y reabrir.)

### Criterio 2 — P1.5: rechazo de saturación "todo blanco"
- [ ] Con el robot calibrado, exponer TODOS los sensores a blanco/luz
      extrema simultánea (linterna fuerte sobre todo el anillo, o robot
      sobre superficie blanca total).
- [ ] Confirmar que DOWN reporta `line_present=0` + `data_valid=0` +
      `EV_CALIB_SUSPECT` seteado (NO un `line_angle` espurio).
- [ ] Quitar la luz/blanco: confirmar que vuelve a operar normal (la calib
      NO se corrompió — el tick saturado saltea la adaptación).

### Criterio 3 — P1.6: backpressure no degrada el 1 kHz
- [ ] Correr DOWN emitiendo a TOP (Serial5, 100 Hz) y CENTRAL (Serial1,
      200 Hz) en simultáneo durante ≥60 s.
- [ ] Leer los contadores `comm_top_get_frames_dropped()` y
      `comm_central_get_frames_dropped()` (exponerlos por debug serial o
      diag): deben mantenerse bajos/cero en operación normal.
- [ ] Verificar que el muestreo del `line_ring` (1 kHz) NO se degrada por la
      carga de TX (medir período real del tick de línea o usar el contador de
      ticks de `line_ring`).

### Cierre
- [ ] Journal nuevo con resultados de los 3 criterios.
- [ ] Si los 3 pasan: marcar P0.2/P1.5/P1.6 como **validados HW** en el audit
      y en `docs/ESTADO-ACTUAL.md`.

## Dependencias / notas

- El Criterio 1 y 3 dependen de tener la cadena UART real andando (TASK-031).
  El Criterio 2 se puede probar standalone con `diag_down` adaptado o leyendo
  el serial USB.
- Para compilar `[env:down]` en una máquina con Avast/SSL roto, ver TASK-302
  (OTOS sin vendorear) o aplicar la excepción Avast (TASK-025).

## Cambios de estado

- 2026-05-29: creada al cierre de la sesión de implementación de robustez DOWN
  (Claude Opus 4.7, requested-by Gustavo). Firmware listo + compila; HW pendiente.
