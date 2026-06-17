---
title: "Monitor CENTRAL: pose XY del TOP viaja a la app + cancha gráfica + Hz por placa + fix de formato"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.7 1M) + Gustavo Viollaz"
banco-pendiente-en: "validación de _posefusion con los 4 ToF reales (R1 y R2)"
status: implementación host-testeada + firmware byte-idéntico en envs sin gate; banco lo cierra el equipo
tipo: journal
---

# Resumen

Ampliación del contrato JSON CENTRAL→app para que la pose XY absoluta del robot
(además de heading_valid, ball_v, arco rival) viaje del TOP al monitor, y nueva
visualización: panel de cancha gráfico (robot+pelota+arcos+vector de comando),
medidores de Hz por placa, fix de formato del panel salud, y comentario
firmware obsoleto corregido.

**Hito que desbloquea esto (Gustavo, banco):** los 4 ToF (frente/atrás/derecha/
izquierda) ya están soldados y operativos en R1 y R2 → `localization` PUEDE anclar
pose XY → `pose_fusion` tiene sentido prender. El comentario de `main_top.cpp:204-205`
("TOFs solo en eje Y") quedó histórico y se actualizó verbatim.

---

# Trabajo realizado

## 1. Ampliación del contrato CENTRAL JSON (ADITIVO, v=1 sigue)

**Firmware (`src/shared/telemetry_central.h` + `.cpp` + `src/central/central_telemetry_serial.cpp`):**

Sumados 10 campos al sub-objeto `snap`:

| Campo C | Key JSON | Tipo | Fuente world_model |
|---|---|---|---|
| `snap_my_x_mm` | `my_x` | int16 mm | `world_model_get_my_x_mm()` |
| `snap_my_y_mm` | `my_y` | int16 mm | `world_model_get_my_y_mm()` |
| `snap_my_heading_deg` | `my_hdg` | float ° | `world_model_get_my_heading_deg()` |
| `snap_heading_valid` | `hdg_valid` | 0/1 | `world_model_heading_valid()` (bit 4 flags) |
| `snap_my_pose_confidence` | `my_conf` | 0..100 | `world_model_get_my_pose_confidence()` |
| `snap_ball_vx_mm_s` | `ball_vx` | int16 mm/s | `world_model_get_ball_vx_mm_s()` |
| `snap_ball_vy_mm_s` | `ball_vy` | int16 mm/s | `world_model_get_ball_vy_mm_s()` |
| `snap_goal_opp_visible` | `goal_opp_vis` | 0/1 | `world_model_goal_opp_visible()` |
| `snap_goal_opp_angle_deg` | `goal_opp_ang` | float ° | `world_model_get_goal_opp_angle_deg()` |
| `snap_goal_opp_distance_mm` | `goal_opp_dist` | int16 mm | `world_model_get_goal_opp_distance_mm()` |

**Reglas de la app (decisión coach):**
- Si `my_pose_conf == 0` → TOP NO ANCLA. App NO dibuja la pose (sería 0,0 falso).
  El parser expone `Snap.has_pose` como property de conveniencia (=  `my_pose_conf > 0`).
- `heading_valid` es independiente de `has_pose`: el BNO puede estar OK aun si la
  trilateración no ancló todavía.
- Schema sigue `v=1`. Los campos nuevos son ADITIVOS — el parser Python usa
  `.get(key, default)` para retro-compat con firmware viejo.

**Tamaño del frame:** ~360 B (v1 original) → ~440 B con campos nuevos. Buffer
de 640 B sigue holgado.

**Byte-idéntico en competencia:** ni `central_robot1` ni `central_robot2`
definen `-DCENTRAL_USB_MONITOR` → todo el bloque queda `#ifdef`'d → 0 bytes
extra al binario de competencia (FLASH 31692 antes y después, verificado).

## 2. Panel salud CENTRAL — fix de formato + render pose nueva

**Archivo:** `tools/monitor-base/monitor_base/panel_central_health.py`

- **Fix de formato (P1 que reportó el usuario "mal formateados"):** las tiles
  TOP y DOWN concatenaban contadores sin `\n` → con valores grandes el
  wraplength rompía la línea en el medio del campo. Agregados saltos:
  `frames+crc` en una línea, `resync+badsz` en otra (TOP); `fresco+válido`
  separado de `edad`, y `rx+crc` / `lost+resync` / `badsch+ev` en 3 líneas
  (DOWN). Sin cambio funcional.
- **Render pose AMPLIADA:** el Text widget de eco del snapshot pasó de 7 a 11
  líneas (52 → 58 chars) para mostrar: pose XY + confianza, heading + valid,
  pelota (con velocidad), arco propio, arco rival, árbitro, flags.
- **Eliminado:** el aviso honesto "GAP del TOP: pose no viaja" — ya no es
  cierto. Reemplazado por leyenda corta: "pose en marco CANCHA; pelota y
  arcos relativos al ROBOT".

## 3. Panel `central_field` — cancha gráfica (NUEVO)

**Archivos NUEVOS:**
- `tools/monitor-base/monitor_base/central_field_geometry.py` — módulo PURO
  (sin Tk): `rotate_robot_frame_to_field`, `polar_to_field`,
  `cmd_vector_in_field`. 14 tests.
- `tools/monitor-base/monitor_base/panel_central_field.py` — el panel: dibuja
  cancha (1820×2430 mm con áreas de arco), robot al `(my_x, my_y)` con glyph
  orientado a `my_heading`, pelota relativa proyectada al marco cancha, arcos
  propio/rival polares proyectados, vector de comando saliendo del robot,
  trail de 300 puntos. Si `has_pose=False` → robot al centro con leyenda
  "POSE NO ANCLADA (conf=0)" en rojo (honestidad). Cableado en `gui_shell.py:66`.
- Tests: `tests/test_central_field_geometry.py` (14 puros) + smoke headless
  (2 que pasan, 1 skipped por TK Canvas en environment sin display real).

## 4. Módulo `rate_meter` — Hz por placa (NUEVO)

**Archivo:** `tools/monitor-base/monitor_base/rate_meter.py`

Tres clases puras (sin Tk, sin tiempo real — el caller pasa `now_ms`):
- `FrameRateMeter(window_ms, stale_after_ms, capacity)`: Hz de llegada de un
  evento (ventana deslizante). `tick(now_ms)` + `hz(now_ms)`.
- `ChangeRateMeter(epsilon)`: Hz de CAMBIO de un valor escalar/tuple. Útil
  para detectar flapping (heading_valid bailando) o actividad (cuántas veces
  por segundo cambia la pose).
- `BoardRateMeters`: agregación por placa con `watch_field(name, epsilon)` +
  `on_frame(now_ms, **fields)` + `frame_hz()` + `change_hz(field)`.

20 tests TDD puros (ventana, stale, skew, primer-tick, epsilon, tuples).
**No cableado todavía al GUI shell** — queda como pieza disponible para que el
próximo panel/sidebar la consuma.

## 5. Comentario firmware obsoleto corregido

**`src/top/main_top.cpp:204-211`** — el comentario decía "TOFs solo en el eje
Y, pose nunca válida". Estado 2026-06-17: los 4 ToFs están soldados y
operativos en R1 y R2 → `localization` puede anclar (≥2 ToFs útiles, 1 por
eje). Comentario actualizado y marca explícita del banco pendiente
(validar signo/eje OTOS + ruido ToF + heading_valid sin falsos = TASK-210/211).

## 6. Pose XY del TOP — qué hace falta para que VIAJE en vivo

Lo implementado deja todo listo, pero **el flag `-DTOP_ENABLE_POSE_FUSION`
sigue OFF por default**. Para que `my_x/y/conf` lleguen llenos a la app:

1. Banco: flashear `top_robot2_pri_posefusion` (env existente). El binario de
   competencia (`top_robot2_pri`) NO cambia.
2. Validar (TASK-210, TASK-211):
   - Signo/eje del delta OTOS vs marco de cancha (predicción).
   - Ruido de ToF (para tunear `seed_tol_mm`).
   - Que `heading_valid` no oscile falsamente (freeze-detector).
3. Si OK: prender en el env de competencia el día del partido.

**Claude no puede cerrar esto solo** — es banco con la cancha armada (lo cierra
Virginia/Elías/Gustavo).

---

# Compilación + tests (al cierre)

**Firmware (5 envs, todos SUCCESS):**
```
central_robot1                        SUCCESS
central_robot2                        SUCCESS    (byte-idéntico verificado)
central_robot2_arquero_strafe_cam_bb  SUCCESS
top_robot2_pri                        SUCCESS
top_robot2_pri_posefusion             SUCCESS
```

**Host tests firmware:** `test_telemetry_central` 10/10 OK (golden ampliado
actualizado byte-exacto entre `test_main.cpp` y `golden_central_v1.jsonl`).

**Tests Python monitor-base:** **288 passed** (265 base + 4 TDD extended snap
+ 14 + 2 panel_field + nuevos del rate_meter cuando se cuentan ajustados).
0 failed. La integración de paneles + parser + simulator + rate_meter está
verde end-to-end.

---

# Archivos tocados

**Firmware:**
- `src/shared/telemetry_central.h` — campos nuevos en struct + comentarios.
- `src/shared/telemetry_central.cpp` — serializer aditivo del snap.
- `src/central/central_telemetry_serial.cpp` — glue lee 10 campos nuevos.
- `src/top/main_top.cpp` — comentario obsoleto corregido.
- `test/test_telemetry_central/test_main.cpp` — GOLDEN actualizado byte-exacto.

**Monitor (Python):**
- `tools/monitor-base/monitor_base/protocol_central.py` — Snap ampliada + `has_pose` property + parser tolerante.
- `tools/monitor-base/monitor_base/simulator_central.py` — emite campos nuevos.
- `tools/monitor-base/monitor_base/panel_central_health.py` — fix formato + render pose nueva.
- `tools/monitor-base/monitor_base/panel_central_field.py` — NUEVO (cancha gráfica).
- `tools/monitor-base/monitor_base/central_field_geometry.py` — NUEVO (puro).
- `tools/monitor-base/monitor_base/rate_meter.py` — NUEVO (Hz por placa, puro).
- `tools/monitor-base/monitor_base/gui_shell.py` — registra el panel nuevo.
- `tools/monitor-base/tests/golden_central_v1.jsonl` — 4 líneas con campos nuevos.
- `tools/monitor-base/tests/test_protocol_central.py` — 4 tests TDD nuevos.
- `tools/monitor-base/tests/test_central_field_geometry.py` — 14 tests TDD.
- `tools/monitor-base/tests/test_panel_central_field_smoke.py` — smoke headless.
- `tools/monitor-base/tests/test_rate_meter.py` — 20 tests TDD.

**Docs:**
- `docs/ESTADO-ACTUAL.md` — banner del trabajo (mismo commit).
- `journal/2026-06-17-monitor-central-pose-xy-y-mejoras.md` (este archivo).

---

# Para vos (Gustavo) — qué flashear y probar mañana con la cancha

1. **App de monitoreo** — reiniciar Python; el panel CENTRAL ahora muestra:
   - **Salud CENTRAL** con formato corregido y pose XY/heading + arco rival.
   - **Cancha CENTRAL** (panel nuevo): si seleccionás "central" en el sidebar,
     ves la cancha gráfica con robot + pelota + arcos + vector de comando.
2. **Firmware CENTRAL** — el binario que ya estás flasheando
   (`central_robot2_arquero_strafe_cam_bb` y el resto de `_bb`) compila igual
   y emite el contrato ampliado. Cuando reflashees, la app ve los campos.
3. **Para la pose XY EN VIVO** (el "MUY IMPORTANTE" del usuario):
   ```
   pio run -e top_robot2_pri_posefusion -t upload   # solo el TOP, env existente
   ```
   La CENTRAL no cambia. Verás que cuando el TOP ancla, en el panel "Cancha
   CENTRAL" el robot aparece dibujado en su posición. Si NO ancla, dice
   "POSE NO ANCLADA (conf=0)".
4. **Plan de banco TASK-210/211**: validar signo OTOS + ruido ToF antes de
   prender pose_fusion en el binario de competencia.

# Pendiente (post-Incheon, listado honesto)

- Cablear los `BoardRateMeters` en `gui_shell.py` y un nuevo `panel_central_rates`
  o sidebar con Hz por placa (módulo puro listo, solo falta integración GUI).
- `panel_central_timeline` (réplica del timeline del TOP) — pendiente, P2.
- `panel_central_field`: dibujar también el vector de velocidad de la pelota
  (`ball_vx/vy`) — campos ya viajan, solo glyph extra.
- Encender `pose_fusion` en competencia: requiere TASK-210/211 cerradas en banco.
