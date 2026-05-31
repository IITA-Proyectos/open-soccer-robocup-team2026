---
title: "TOP — Orientación interna de las zonas ToF: el izquierdo está rotado 180° (corrección + test)"
date: 2026-05-31
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8, Anthropic)"
status: final
tags: [top-board, tof, vl53l7cx, zonas, orientacion, lidar, hardware-test]
robot: top (R1 montado)
area: percepcion
tipo: hardware-test + fix
---

# TOP — Orientación de zonas ToF: el izquierdo rotado 180°

> **TL;DR.** Gustavo caracterizó en banco la orientación interna de la grilla
> 8×8 de los 4 ToF (con `diag_top_tof_zonemap`). Resultado:
> **TOF0/1/2 (frente/atrás/derecha) comparten la misma orientación interna**;
> el **TOF3 (izquierdo)**, de otro fabricante y montado mirando hacia abajo,
> está **rotado 180°** respecto a los otros 3 (arriba↔abajo, izq↔der).
> Implementé la corrección como header puro `src/shared/tof_zone_orient.h`
> (rota 180° **solo** al izquierdo), con `test_tof_zone_orient` host-native
> (7 tests, suite total ahora 253/253 verde), y agregué al diag una **vista
> corregida** (tecla `c`) para confirmar visualmente. NO toca el firmware vivo
> (el lidar-360 / Sprint B lo usará cuando se escriba).

## Cómo se caracterizó (banco)

Con `diag_top_tof_zonemap` (imprime la grilla 8×8 cruda + marca la zona más
cercana), Gustavo movió un objeto a posiciones conocidas del campo de visión de
cada sensor y anotó qué fila/columna se encendía.

**TOF0 (frente)** — observado: mano arriba→aparece a la izquierda de pantalla;
abajo→derecha; izquierda→abajo; derecha→arriba. Es decir el marco interno de
TOF0 está girado ~90° respecto a la grilla impresa (filas y columnas
intercambiadas), pero sin espejo (el ciclo de las 4 direcciones gira parejo).
TOF1 y TOF2 comparten ESTE mismo marco interno.

**TOF3 (izquierdo)** — "lo de arriba es como abajo de los otros, y lo de
izquierda es como la derecha de los otros". Eso es exactamente una **rotación
de 180°** respecto al marco de TOF0/1/2: invertir fila Y columna.

## Qué se decidió y por qué

- El marco interno común de **TOF0/1/2 se toma como "canónico"**. La corrección
  solo necesita **alinear el izquierdo** con ese marco. Por eso
  `tof_zone_orient` aplica 180° únicamente a TOF3 (los otros 3 = identidad).
- La rotación de ~90° que tiene ese marco canónico respecto a la grilla
  impresa NO se corrige acá: eso es parte del mapeo **zona→azimut** que hará el
  lidar-360 (cada sensor aporta su sector angular). `tof_zone_orient` solo
  garantiza que los 4 sensores sean **mutuamente consistentes**, que es la
  precondición para encadenarlos.

## Implementación (host-testeable, NO toca firmware vivo)

- **`src/shared/tof_zone_orient.h`** (nuevo, header puro): `tof_zone_needs_180()`
  (true solo para el izquierdo) + `tof_raw_zone_for_canonical(sensor, zona, W)`
  que para el izquierdo invierte fila y columna y para los demás es identidad.
  Funciona en 8×8 y 4×4. Una sola fuente de verdad para diag + firmware futuro.
- **`test/test_tof_zone_orient`** (nuevo, 7 tests): identidad en no-izquierdos,
  solo el izquierdo necesita 180°, puntos conocidos, involución (aplicar 2 veces
  = identidad), biyección (sin colisiones), 4×4, y casos defensivos. Verde.
- **`diag_top_tof_zonemap`**: tecla `c` alterna vista CRUDA ↔ CORREGIDA. En
  corregida, el izquierdo se ve ya rotado para coincidir con los otros 3.
- Suite host-native: **20 envs / 253 tests / 0 fallos** (era 19/246).

## Pendiente

- **Confirmación visual** (equipo): correr el zonemap en vista corregida (`c`)
  y verificar que, con la mano en la misma posición física, los 4 sensores
  marcan la misma celda relativa. (TASK-203, parte final.)
- **R2**: repetir la caracterización (puede diferir si el montaje del izquierdo
  es distinto).
- **Lidar-360 (firmware, Sprint B)**: usar `tof_zone_orient` + el mapeo
  zona→azimut por sensor para el barrido 360°. Requiere antes que
  `sensors_tof.cpp` lea los 4 ToF (hoy lee 1).

## Archivos

- `src/shared/tof_zone_orient.h` — **nuevo**, corrección de orientación (180° al izq).
- `test/test_tof_zone_orient/test_main.cpp` — **nuevo**, 7 tests host.
- `src/diag/diag_top_tof_zonemap.cpp` — vista corregida (tecla `c`).
- `platformio.ini` — `-I src/shared` en el env del zonemap.
- `docs/CONVENCION-EJES-ROBOT.md` — sección de orientación de zonas actualizada.
- `team-tasks/2026-05-31-task-203-*` — marcada caracterización hecha.
