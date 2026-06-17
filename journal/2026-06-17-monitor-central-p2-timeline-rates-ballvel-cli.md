---
title: "Monitor CENTRAL P2: timeline + Hz por placa + vector velocidad pelota + CLI --sim-central"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.8 1M) + 3 subagentes paralelos"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: implementado host-testeado (320 tests verde); banco visual lo cierra el equipo abriendo la app
tipo: journal
---

# Resumen

Cuatro mejoras P2 del monitor de la placa CENTRAL, encaradas en **workflow paralelo
(3 subagentes simultáneos sobre archivos disjuntos)** + la integración compartida hecha
por la sesión principal. Continúa el trabajo del journal
`2026-06-17-monitor-central-pose-xy-y-mejoras.md` (que dejó estos 4 ítems como pendientes).

# Qué se hizo

## P2.1 — `panel_central_timeline` (histórico de señales) — agente A
- `monitor_base/central_timeline.py` (PURO): `CentralTimelineSample.from_frame()`,
  `CentralSignalHistory` (ring buffer maxlen=300), `transitions()`.
- `monitor_base/panel_central_timeline.py`: clase `CentralTimelinePanel`
  (key `central_timeline`). Bandas booleanas (match, has_pose, heading_valid,
  ball_vis, line_valid, top_fresh, down_fresh) + sparklines (loop_ema_us, my_x,
  my_y) + banda de texto fsm_state + tablero de flapping (transiciones por señal).
- Tests: `test_central_timeline.py` (19) + smoke (4). **23 verde.**

## P2.2 — `panel_central_rates` (Hz por placa) — agente B
- `monitor_base/panel_central_rates.py`: clase `CentralRatesPanel`
  (key `central_tasas`). Consume el `BoardRateMeters` (módulo `rate_meter.py` ya
  existente, NO modificado). Muestra:
  - Hz de **llegada** de 3 enlaces: TOP→CENTRAL (Δ`top.fr`), DOWN→CENTRAL
    (Δ`down.rx`), CENTRAL telemetría (frame rate).
  - Hz de **cambio** de: pelota (`ball_x`, ε=5), pose (`my_x/y`, ε=5),
    FSM state, línea (`down.valid`).
  - **Alerta de flapping** de `heading_valid` (rojo si >2 Hz).
  - Reloj usado = `f.t_ms` (reloj monótono del firmware, no `time.time()`):
    refleja la cadencia REAL de la placa sin jitter de USB/Tk. El skew defensivo
    de `FrameRateMeter` cubre un reset de `t_ms`.
- Tests: `test_panel_central_rates_smoke.py` (3 puros + 2 smoke). **5 verde.**

## P2.3 — Vector de velocidad de la pelota en `panel_central_field` — agente C
- `central_field_geometry.py`: función pura nueva `ball_velocity_vector_in_field(
  heading_deg, ball_vx, ball_vy, scale=0.15, max_len=90)` (rota al marco cancha +
  escala + clampea preservando dirección).
- `panel_central_field.py`: `_draw_ball_velocity()` dibuja una flecha **naranja**
  (distinta del vector de comando ámbar) desde la pelota, solo si v ≥ 30 mm/s
  (evita ruido). Escala 0.15 px/(mm/s), clamp 90 px.
- Tests: 4 nuevos en `test_central_field_geometry.py`. **21 verde** (con los 17 previos).

## P2.4 — CLI `--sim-central` — sesión principal
- `monitor_base/__main__.py`: nuevo flag `--sim-central` (mutuamente exclusivo con
  `--sim`/`--port`/`--replay`). Rutea a `SimCentralSource` (ya existía en
  `sources.py`, sin cablear). Permite abrir la app y ejercitar TODOS los paneles
  central (Cerebro / Salud / Cancha / Tasas / Timeline) sin robot.
- Verificado: `_build_source(['--sim-central'])` → `SimCentralSource`; exclusión
  mutua con `--sim` da error de argparse (correcto).

## Integración (sesión principal)
- `gui_shell.py`: registrados `CentralRatesPanel` + `CentralTimelinePanel` en
  `_registry()` (junto a los 3 paneles central previos). Los 5 cargan sin error.

# Workflow paralelo (cómo se evitaron conflictos)

3 subagentes sobre **archivos disjuntos**, con prohibición explícita de tocar los
archivos compartidos (`gui_shell.py`, `__main__.py`). La sesión principal hizo la
integración compartida AL FINAL (registro + CLI). Cero conflictos. Patrón del skill
`superpowers:dispatching-parallel-agents`.

**Nota honesta:** los agentes A y B NO pudieron correr pytest (shell denegada en su
sesión) y lo reportaron explícitamente sin afirmar que pasaban. La sesión principal
**verificó los 3** corriendo la suite: timeline 23, rates 5, field 21, todos verde.
(Política "verificar antes de afirmar" cumplida.)

# Verificación

- **Suite monitor-base completa: 320 passed** (eran 288; +32 nuevos:
  timeline 23, rates 5, ball-vel 4 — los smoke se solapan en el conteo).
- Los 5 paneles central cargan en `_registry()` real sin error de import.
- `--sim-central` rutea correcto + exclusión mutua OK.
- **Firmware NO tocado** en esta tanda (todo es app Python).

# Archivos

**Nuevos:** `central_timeline.py`, `panel_central_timeline.py`,
`panel_central_rates.py`, `tests/test_central_timeline.py`,
`tests/test_panel_central_timeline_smoke.py`, `tests/test_panel_central_rates_smoke.py`.

**Modificados:** `central_field_geometry.py` (función vel), `panel_central_field.py`
(flecha vel), `tests/test_central_field_geometry.py` (+4), `gui_shell.py` (registro
×2), `__main__.py` (flag `--sim-central`).

# Pendiente (honesto)

- **Banco visual**: abrir `python -m monitor_base --sim-central` y mirar que los 5
  paneles se ven bien (los smoke headless prueban que no crashean, NO que se ven
  lindos — eso lo cierra un humano mirando).
- Con robot real: confirmar que las tasas Hz coinciden con lo esperado (TOP ~66 Hz,
  DOWN ~200 Hz declarados; la app los mide del Δ contador).
- Ayuda contextual (tooltips/help_text) de los 2 paneles nuevos — quedó sin entrada
  en `help_text.py` (cae al fallback "sin ayuda todavía"). P3.
