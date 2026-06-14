---
title: "TOP: monitor/calibración USB DORMIDO en el binario de competencia (espejo de DOWN) + modo texto humano por ENTER + tests host"
date: 2026-06-13
author: "Claude (Anthropic, Opus 4.8) — sesión banco"
requested-by: "Gustavo Viollaz (@gviollaz)"
area: comunicacion
tipo: resultado
robot: ambos
status: code-complete + gate host verde — PENDIENTE BANCO (no cierra hardware Claude)
relacionada: TASK-205, TASK-306 (patrón DOWN), TELEMETRIA-TOP.md
---

# TOP — monitor de sensores/calibración DORMIDO en competencia (2026-06-13)

**Sesión:** Claude (coach/firmware) + Gustavo. Repo `open-soccer-robocup-team2026`, branch `main`.
**Pedido (Gustavo):** "hacer para la placa TOP algo similar a lo de la placa de abajo: que el
monitoreo/calibración viva en el programa principal, conecto el USB, corro la app de Python y NO
hace falta reflashear. El monitor está DORMIDO hasta que conecto el USB y recibe un comando de la
app. Si recibe un ENTER, que mande texto formateado por pantalla unos segundos. Si no hay ENTER ni
app, no manda nada."

## Qué se hizo (firmware host-testeable; Claude NO cierra hardware)

Se llevó el monitor de la TOP al **mismo patrón que DOWN** (TASK-306): viaja **EN el binario de
competencia** `top_robot2_pri` (vale para AMBAS TOP desde el recableado 2026-06-11) vía el flag
nuevo `-DTOP_USB_MONITOR`, pero **arranca DORMIDO**. Máquina de 3 estados (en
`src/top/top_telemetry_serial.cpp`):

- **DORMIDO** (boot): no envía **nada**. Banner `[TOP-MONITOR] dormido - esperando app (STREAM ON / PING) o ENTER`.
- **MÁQUINA**: la app (`monitor_base --top`) manda `STREAM ON` + `PING` (latido 1 s) → **JSON Lines**
  continuo (20 Hz) para la app. Acepta `IMU ZERO` / `IMU SAVE` (calibración del heading) EN VIVO.
- **HUMANO** (lo nuevo que pidió Gustavo): un **ENTER** en un monitor serie crudo (línea vacía o
  texto no reconocido; CR o LF) → **bloque de texto legible** (cámaras/IMU/ToF/WorldSnapshot) a 2 Hz
  durante 3 s, sin app y sin tipear comandos. Repetir Enter lo mantiene.
- **Auto-off**: host mudo > 3 s → vuelve solo a DORMIDO. Sacar el cable = modo partido.
- **Match-safe**: en partido no hay USB → nunca llega una línea → nunca despierta.

Detalle de diseño que evita un bug de la app: un ENTER (NONE/UNKNOWN) solo despierta a HUMANO
**desde DORMIDO**; si ya estamos en MÁQUINA (app activa) un LF suelto tras un comando CRLF NO tumba
el stream de máquina.

### Archivos tocados (carril TOP — no piso a DOWN ni a la app del otro agente)

- `src/shared/telemetry_top.{h,cpp}` — **función pura nueva `tt_format_human()`** (bloque de texto
  ASCII, mismo dato que el JSON; `--` = no visible/sin lectura).
- `src/top/top_telemetry_serial.cpp` — reescrito: 3 estados, auto-off, wake por CR/LF, `fill_frame`
  compartido entre emisor JSON y humano. Gate `#if defined(TOP_DEBUG_TELEMETRY) || defined(TOP_USB_MONITOR)`.
- `src/top/top_telemetry_serial.h` — comentario del gate.
- `src/top/main_top.cpp` — los 2 guards de init/tick ahora OR (`TOP_DEBUG_TELEMETRY || TOP_USB_MONITOR`).
- `src/top/comm_central.{cpp,h}` — **fix de link**: el cache `g_last_snap` + `comm_central_get_last_snapshot()`
  estaba gateado SOLO por `TOP_DEBUG_TELEMETRY` → con `TOP_USB_MONITOR` no compilaba. Guards ensanchados a OR.
- `platformio.ini` — `-DTOP_USB_MONITOR` agregado a `[env:top_robot2_pri]` (lo heredan `_sticky` y
  `_debug_telemetry`). Comentario explicando el patrón + la advertencia de byte-identidad.
- `test/test_telemetry_top/test_main.cpp` — **+5 tests** del formateador humano (shape, valores
  visibles, dashes cuando no hay dato, buffer chico, args nulos).
- `docs/firmware/TELEMETRIA-TOP.md` — §0 modo de operación (3 estados), §1-bis bloque humano, build.

## Verificación (lo que Claude SÍ puede cerrar)

- **Gate host completo: 60 envs / 839 tests / 0 fallos** (`run-host-tests.sh`). `test_telemetry_top`
  pasó de 12 → **17 tests** (los 5 nuevos del modo humano). El módulo puro (JSON + texto + parser)
  queda 100% bajo test.
- ✅ **`pio run -e top_robot2_pri` → SUCCESS** (2026-06-13). Se instaló PlatformIO en la máquina
  (winget Python 3.12 → `pip install platformio` 6.1.19 → bajó plataforma teensy + toolchain ARM;
  Avast NO bloqueó). El binario de competencia con el monitor dormido **linkea y genera
  `firmware.hex`** (FLASH code 68324 + data 99512 + headers 8288 ≈ 176 KB de 2 MB) → el **fix de
  link** del snapshot cache (`comm_central`, ensanchado a `TOP_USB_MONITOR`) queda **confirmado**.
- Para el gate host (g++) se usó el de Webots (`C:\Program Files\Webots\msys64\mingw64\bin`); el
  toolchain Teensy ya quedó cacheado en `~/.platformio`. `pio` accesible en PowerShell nueva (PATH
  de usuario actualizado por winget).
- ⚠️ El **comportamiento en runtime** (dormido/wake/auto-off/texto humano/match-safe) sigue siendo
  banco — compilar no prueba conducta. Test plan abajo.

## NO validado (regla no negociable: hardware lo cierra el equipo)

⚠️ **El binario de competencia `top_robot2_pri` YA NO es byte-idéntico** (ahora lleva el monitor
dormido). Es match-safe por diseño, pero **lo valida el equipo en banco**. Ver test plan abajo.

### Test plan — banco (equipo)

**Subsistema:** monitor/telemetría USB TOP. **Robot:** R1 y/o R2 (mismo env `top_robot2_pri`).
**Setup:** TOP flasheada con `pio run -e top_robot2_pri -t upload`; las 3 placas a batería (>7,6 V)
para que TOP arme WorldSnapshot real; cámaras + BNO + ToF conectados.

1. **Compila:** `pio run -e top_robot2_pri` → SUCCESS (cierra el riesgo de link del snapshot cache).
2. **Dormido al boot:** abrir monitor serie, reset → ver `[TOP-MONITOR] dormido`. Sin tocar nada: **NO** llega nada. ✔
3. **ENTER → texto humano:** apretar Enter → sale el bloque de 5 líneas (CAM/IMU/ToF/SNAP) ~3 s y para solo; repetir Enter lo mantiene. ✔
4. **App → máquina:** `python -m monitor_base --top --port COMx` → stream JSON continuo + vista TOP; al desenchufar, ~3 s → dormido. ✔
5. **Calibración IMU en vivo:** con el robot apuntando al arco rival, `IMU ZERO` → ver ack `[TOP] heading cero recalibrado`; girar a mano y confirmar que `hdg` arranca de ~0; `IMU SAVE` → ack. ✔
6. **Regresión partido:** sin USB conectado, el robot juega igual (TOP sigue mandando WorldSnapshot a CENTRAL por Serial4 a 100 Hz; el wake no dispara sin host). ✔
7. (Opcional) Confirmar `snap` poblado (no `valid:0`) una vez que TOP difunde snapshots.

**Doc esperada:** anotar en el journal que los 7 pasos dan lo esperado; recién ahí se cierra en TASK-205.

## Coordinación (no chocar con el otro agente)

- El otro agente trabaja la **app `tools/monitor-base` y el firmware/telemetría de DOWN**. Esta
  sesión tocó SOLO el carril **TOP** (`src/top`, `src/shared/telemetry_top`, envs `top_*`,
  `docs/firmware/TELEMETRIA-TOP.md`) + `platformio.ini` SOLO en el bloque `[env:top_robot2_pri]`.
- **Pendiente coordinado (app, carril del otro agente):** la vista TOP de `monitor_base` debe
  mandar `STREAM ON` + `PING` (ya lo hace para DOWN) para despertar el monitor dormido. No se tocó
  Python en esta sesión.
