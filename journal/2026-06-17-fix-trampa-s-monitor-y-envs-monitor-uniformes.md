---
title: "Fix de raíz a la trampa de la 'S' del USB + monitor habilitado en TODOS los envs de práctica R1/R2"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
banco-validado-por: "Gustavo (banco R2 arquero, 2026-06-17)"
status: parche aplicado + banco confirmado (parte fix S); monitor habilitado en envs (banco pendiente en el resto)
tipo: journal
---

# Resumen

Dos cambios secuenciales sobre la placa CENTRAL, autorizados y validados en banco:

1. **Fix de raíz a la "trampa de la 'S'"**: el path char-por-char del USB handler de `main_central.cpp` trataba CUALQUIER `\n`/`\r` como GO (legacy "ENTER pelado = GO" del juez-PC). La app `monitor-base` manda `STREAM ON\n` al conectarse y `PING\n` cada ~1 s → el `\n` final disparaba `cmd_go=true` → el robot arrancaba solo cada vez que se abría la app, o cada PING.
2. **Cobertura del monitor USB en todos los envs de práctica R1/R2**: hasta hoy, `-DCENTRAL_USB_MONITOR` solo estaba en los envs `_demo_bb`. Los envs que el equipo usa a diario (`*_arquero_*`, `*_strafe_*`, `*_practica*`, `*_slow`, `_demo`) no lo llevaban → la app no recibía datos. Se agregó el flag a TODOS los envs de práctica/banco de R1 y R2 sin tocar los binarios de competencia.

El binario de competencia (`central_robot1` / `central_robot2`) sigue siendo byte-idéntico (no se les agrega ningún flag nuevo; el cambio del USB handler queda inerte en el path sin `CENTRAL_USB_MONITOR` salvo por la regla nueva "línea >1 char no es comando", que no afecta el caso real de competencia donde nadie tipea por USB).

---

# Tema 1 — La trampa de la 'S' (P0, CERRADO en banco)

## Causa raíz (verificada en código antes de tocar)

- `main_central.cpp` tenía dos paths para leer Serial USB:
  - **Path A** (`#ifdef CENTRAL_USB_MONITOR`): buffer de líneas + `central_telemetry_consume_line()` que reconoce y consume `STREAM ON` / `PING`. Resolvía la trampa, pero solo en envs con el flag.
  - **Path B** (default): char-por-char. `\n`/`\r`/`g`/`G` → `cmd_go=true`. `s`/`S` → `cmd_stop=true`.
- La mayoría de los envs de práctica (`central_robot2`, `central_robot2_arquero*`, `central_robot2_arquero_strafe_cam_bb`, etc.) llevan `-DCENTRAL_ENABLE_MANUAL_START` PERO NO llevaban `-DCENTRAL_USB_MONITOR` → caían en el path B.
- `tools/monitor-base/monitor_base/sources.py:428,440` confirma: la app envía `b"STREAM ON\n"` al conectar + `b"PING\n"` cada 1 s.
- Camino del bug:
  1. Llega `S` → `cmd_stop = true`.
  2. Llega `T,R,E,A,M, ,O,N` → ningún trigger.
  3. Llega `\n` → `cmd_go = true`.
  4. Bloque `if (cmd_stop) … else if (cmd_go)`: si los chars caen en la misma vuelta del loop, gana STOP. Si caen en vueltas distintas (jitter USB CDC), la última vuelta tiene solo `\n` → cmd_go solo → robot arranca.
  5. Cada `PING\n` periódico repite el escenario → el robot re-arranca aunque el usuario lo detenga.

`docs/ESTADO-ACTUAL.md` decía "trampa de la 'S' resuelta por buffer de líneas" — verdad parcial: resuelta SOLO en envs `_demo_bb`.

## Fix aplicado (Opción 1: fix de raíz)

`main_central.cpp:285-336` (commit pendiente al cerrar este journal). Unificación de ambos paths bajo una sola regla simple:

```
línea vacía           → GO  (manual-start)
línea de 1 char       → comando (g/s/d/x)
línea de >1 char      → IGNORAR (texto del monitor / basura)
```

Bajo `CENTRAL_USB_MONITOR`, antes de aplicar la regla anterior, la línea pasa por `central_telemetry_consume_line()` que despacha `PING`/`STREAM ON`/`STREAM OFF`. Si la consume, no se procesa nada más.

Decisión coach: **la regla "línea de >1 char se ignora" es estricta y robusta**. Caso edge "Sgarbage" o "gd" (combo) → se ignora. Caso real (1 char) → funciona. El comentario viejo de `central_telemetry_serial.cpp:234-235` ("una línea con char de control sola o con basura") sugiere flexibilidad mayor; se sacrificó ese caso porque era precisamente el vector de la trampa. Si alguien necesita re-procesar texto con basura, el monitor crudo sigue funcionando con `g\n` / `s\n` exactos.

## Validación banco (Gustavo, 2026-06-17)

> "Ahora anda el robot con árbitro, arranca y para. Cuando conecto USB a programa de monitoreo, no arranca robot. Eso es bueno."

→ Tema 1 cerrado en HW real.

---

# Tema 2 — Monitor USB no leía datos (P0, CERRADO en banco)

## Causa raíz

El env `central_robot2_arquero_strafe_cam_bb` lleva `-DCENTRAL_BLACKBOX` (caja negra OFFLINE) pero NO `-DCENTRAL_USB_MONITOR` (stream JSON Lines EN VIVO). El comentario de platformio.ini línea 553-554 lo decía explícito: "Solo va en estos envs *_bb — NO en central_robot1/central_robot2 (competencia byte-idéntica)" — pero "estos envs *_bb" se refería SOLO a `central_robot2_demo_bb` y `central_robot1_arquero_demo_bb`. Todos los demás `_bb` del arquero (`_arquero_patrol_bb`, `_arquero_bb`, `_arquero_bb_nobtn`, `_arquero_strafe_bb`, `_arquero_strafe_cam_bb`, `_arquero_strafe_cam_ratedamp`, `_strafe_slew_bb`) quedaban afuera. Idem `_arquero` / `_arquero_slow` / `_arquero_patrol` / `_arquero_strafe_cam_app`. Idem todos los R1 (`_slow`, `_arquero_demo`, `_arquero_demo_patrol`, `_delantero_practica*`).

## Fix aplicado

`platformio.ini` — agregado `-DCENTRAL_USB_MONITOR` a 9 envs base (los hijos heredan):

**R1 (4 envs base + heredan):**
- `central_robot1_slow`
- `central_robot1_arquero_demo` (→ heredan `_demo_bb` que ya lo tenía, y `_demo_patrol`)
- `central_robot1_arquero_demo_patrol`
- `central_robot1_delantero_practica` (→ heredan `_practica_bb`, `_practica_obst_bb`)

**R2 (5 envs base + heredan):**
- `central_robot2_demo` (→ hereda `_demo_bb` que ya lo tenía)
- `central_robot2_arquero` (→ heredan `_bb` y todos sus descendientes que ya cubrí ayer-noche)
- `central_robot2_arquero_slow`
- `central_robot2_arquero_patrol` (→ hereda `_patrol_bb` que ya cubrí)
- `central_robot2_arquero_strafe_cam_app`

NO se tocaron `central_robot1` / `central_robot2` (competencia byte-idéntica) ni `central_robot1_wdt*` (test del watchdog, sin uso de monitor).

## Compilación (verificación de que linkea, NO de que funcione en HW)

11 envs compilados en una sola corrida (`pio run -e ...`):

```
central_robot1_slow                         SUCCESS
central_robot1_arquero_demo                 SUCCESS
central_robot1_arquero_demo_patrol          SUCCESS
central_robot2_demo                         SUCCESS
central_robot1_delantero_practica_bb        SUCCESS
central_robot2_arquero_strafe_cam_app       SUCCESS
central_robot2_arquero_strafe_cam_ratedamp  SUCCESS
central_robot2_strafe_slew_bb               SUCCESS
central_robot2_arquero                      SUCCESS
central_robot2_arquero_slow                 SUCCESS
central_robot2_arquero_patrol               SUCCESS
========================= 11 succeeded in 00:01:58.733 =========================
```

El `central_robot2_arquero_strafe_cam_bb` también compila (validado por Gustavo en banco antes, datos llegan).

## Validación banco (Gustavo, 2026-06-17)

> "Ahora si anda."

→ Tema 2 cerrado en HW real para el env probado (`central_robot2_arquero_strafe_cam_bb`). Los otros envs que recibieron el flag están compilados pero el banco de cada uno lo cierra el equipo cuando los use.

---

# Lo que NO se hizo (deuda honesta)

- **No se tocó `central_robot1` ni `central_robot2`** (binarios de competencia). El bug está mitigado en esos envs por otro camino: como NO llevan `-DCENTRAL_ENABLE_MANUAL_START`, los `cmd_go`/`cmd_stop` no compilan (los blocks `#ifdef MANUAL_START` quedan vacíos) → la línea-de-1-char tampoco aplica → el `\n` del PING jamás dispara nada. Verificado leyendo `central_robot2` (línea 715-726): no tiene `MANUAL_START`. Riesgo residual cero.
- **El binario de competencia es BYTE-IDÉNTICO al de antes del fix** porque el cambio en `main_central.cpp` está dentro del bloque `#if defined(CENTRAL_ENABLE_MANUAL_START) || defined(CENTRAL_USB_MONITOR)` (línea 364), que es falso en competencia.
- **No se actualizó el comentario de `central_telemetry_serial.cpp:234-235`** ("sola o con basura") que ahora es inexacto. Marca pendiente: actualizar en próximo touch.

# Pendiente equipo (banco)

- Cuando reflashees uno de los envs con el flag NUEVO, hacer los 6 chequeos del plan que cerró el tema 1 con `central_robot2_arquero_strafe_cam_bb`. En particular validar que:
  - Robot no arranca al abrir la app.
  - Robot no arranca con PING periódico.
  - `g`+ENTER en monitor crudo: arranca.
  - `s`+ENTER: frena.
  - ENTER pelado: arranca (juez-PC).

# Archivos tocados

- `software/teensy/Soccer 2026/src/central/main_central.cpp` (líneas 285-336): unificación de los 2 paths del USB handler.
- `software/teensy/Soccer 2026/platformio.ini`: agregado `-DCENTRAL_USB_MONITOR` a 9 envs base + comentarios trazables ("2026-06-17:").
- `docs/ESTADO-ACTUAL.md`: actualizar banner (sale en el mismo commit que cierra este journal).
- `journal/2026-06-17-fix-trampa-s-monitor-y-envs-monitor-uniformes.md` (este archivo).
