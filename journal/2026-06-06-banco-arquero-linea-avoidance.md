---
title: "Banco arquero (ROBOT1): línea DOWN→CENTRAL valid=1 + esquive de línea por ángulo de escape"
date: 2026-06-06
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "María Viollaz"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8, 1M context)"
status: final
tags: [movilidad, control, linea, arquero, resultado, banco, P1]
robot: arquero
area: movilidad
tipo: resultado
---

## Contexto

María (trabajando desde la compu de Gustavo) quería empezar a probar en cancha
que el robot **se mueva sin salirse de las líneas blancas**. Robot bajo prueba:
**ROBOT1 (arquero)**. Sesión de banco con las 3 placas (TOP no conectada; se
trabajó con DOWN + CENTRAL).

## Qué se hizo

Validación bottom-up con los programas `diag_*` (NO firmware de competencia):

1. **DOWN — sensores de línea:** censo de los 32 sensores (`diag_down`) +
   calibración verde/blanco contra la cancha real (`diag_down_calibracion`,
   guardada en EEPROM).
2. **Enlace DOWN→CENTRAL:** `diag_central_rx_all` (línea 200 Hz + OTOS 100 Hz).
3. **Movimiento + esquive de línea:** `diag_central_line_sweep_robot1`.

**Cambios de firmware** (commit `8956d10`, ya en `origin/main`):
- `src/down/comm_central.cpp`: `calib_min_margin` **120 → 40**.
- `src/diag/diag_down_calibracion.cpp`: `MIN_MARGIN` **80 → 40** (alineado).
- `src/diag/diag_central_line_sweep.cpp`: arranque/stop por **ENTER** (el botón
  físico pin 9 no era accesible), **heading-hold con el OTOS de la base**, signos
  de centrado y de rumbo corregidos, y **esquive de línea por ángulo de escape**
  (`ESCAPE_DIR_SIGN = -1`).

## Qué se midió / observó

- **Censo:** 32/32 sensores responden, **0 muertos**.
- **Calibración:** `sospechosos: 0/32`, guardada en EEPROM. Sensores de menor
  señal: S09 (margen 82), S16 (88), S32 (108), S15 (110), S30 (116).
- **Enlace DOWN→CENTRAL:** 200 Hz línea + 100 Hz OTOS pose/vel, `0 crcErr`,
  `0 seqGap`. Sólido.
- **`valid` (compuerta maestra de la línea):**
  - Con `calib_min_margin = 120` → `CALIB?` / `valid=0` perpetuo (umbral
    inalcanzable para estos sensores).
  - Con `60` → arrancaba `valid=1` pero la auto-adaptación del carpet erosionaba
    el margen del sensor más flaco (S09) por debajo de 60 en ~1-2 s → volvía a
    `valid=0`.
  - Con **`40`** (≈2× la banda de histéresis ±20) → **`valid=1` estable**.
- **Eventos:** `IMMINENT_EXIT` (salida inminente) y `CORNER` (esquina) disparan
  correctamente al cubrir varios sensores con blanco.
- **Movimiento (ROBOT1):** los 3 motores mueven (M1/M2 opuestos + M3 acompaña;
  mapeo ya validado previamente). El strafe genérico por cinemática daba
  **círculos** (WHEEL_ANGLES sin calibrar) → se usó control directo de motores.
  Sin corrección de rumbo derivaba/rotaba; **con heading-hold (OTOS) patrulla
  derecho**.
- **Esquive de línea:** con `ESCAPE_DIR_SIGN = -1` el robot **huye siempre para
  el lado opuesto a la línea** (validado acercando la línea por izquierda y por
  derecha, consistente).
- **Batería (dato operativo importante):** a **~7,60 V el robot NO se mueve**
  (motores sin fuerza) y además se degrada la detección de línea (blanco≈verde,
  margen ~0) y aparece `seqGap` / `OTOS vel [STALE]`. **Cambiar la batería lo
  resolvió de inmediato.**

## Conclusión

**Objetivo cumplido en banco:** el arquero (ROBOT1) se mueve y **esquiva la línea
blanca** (huye al lado opuesto) combinando la línea de DOWN con el rumbo del OTOS
de la base. La detección de línea quedó **robusta (`valid=1` estable)** tras bajar
el umbral de calibración a un valor alcanzable por el hardware real (40).

Importante: todo esto vive en el **programa de banco** `diag_central_line_sweep`,
**no** en el firmware de competencia todavía.

## Próximos pasos

1. **Calibrar `WHEEL_ANGLES`** (cinemática omni) para movimiento omnidireccional
   limpio — hoy el strafe genérico da círculos y se trabajó con control directo.
2. **Portar el esquive de línea** (ángulo de escape) a la conducta de competencia
   (`LINE_AVOID` real en `strategy`).
3. Si reaparece `CALIB?` estable a margen 40, el sensor **S09** es el límite
   físico (óptica/altura del sensor) o reducir `adapt_alpha`.
4. Revisar el artefacto `OTOS vel [STALE]` / `seqGap` (no bloquea; probable
   coexistencia I²C/batería en DOWN).

## Referencias

- Commit del firmware: `8956d10` (en `origin/main`).
- Programas: `diag_down`, `diag_down_calibracion`, `diag_central_rx_all`,
  `diag_central_line_sweep_robot1` (todos en `software/teensy/Soccer 2026/`).
