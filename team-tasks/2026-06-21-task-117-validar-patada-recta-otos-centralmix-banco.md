# TASK-117 — Validar en banco la patada RECTA y FUERTE con corrección de rumbo por OTOS (centralmix)

- **Placa:** CENTRAL (R1, delantero) + depende de DOWN (R1) mandando Pose2D del OTOS (confidence>0).
- **Asignado:** equipo (banco) — Elías
- **Prioridad:** P1 (la patada es la jugada que define gol; que salga derecha y fuerte impacta partidos).
- **Estado:** abierta
- **Build:** `pio run -e central_robot1_mix_bno -t upload` (✅ **compilado por Claude** 2026-06-21:
  `central_robot1_mix` y `central_robot1_mix_bno` SUCCESS). Escape: cualquier env de competencia.

## Por qué
La patada del centralmix (`avanzar_patear`, port 2025) era **lazo abierto**: rampa lenta
M1=+vel / M2=-vel / M3=0 hasta 240, **sin realimentación** → si las 2 ruedas delanteras no están
parejas, **curva**. Pedido de Elías: que patee **DERECHO, FUERTE y RÁPIDO** usando el OTOS.

Lo que se hizo (journal `2026-06-21-centralmix-patada-recta-otos.md`):
1. **Fuerte/rápida:** arranca en `MIX_KICK_VEL_START` (120, sobre el piso del motor) y rampa
   agresiva hasta `MIX_KICK_VEL_FINAL` (255) en ~24 ms (antes ~120 ms a 240).
2. **Derecha (heading-hold OTOS):** al iniciar la patada se ancla `otos_heading_deg` como objetivo;
   durante el empuje se agrega un término de **giro** (mismo signo en las 3 ruedas, como `girar()`)
   proporcional al error de rumbo (`MIX_KICK_HEADING_KP`), clampeado a `MIX_KICK_CORR_MAX`.
   Usa el **OTOS local de DOWN**, independiente del BNO del snapshot.

## Pre-requisito
- DOWN de R1 mandando OTOS (`down_robot1*`): en el debug USB tiene que verse `otos_conf` = 100 (los
  2 OTOS sanos) o 60 (uno). Si `otos_conf=0` → **la patada va recta a ciegas** (sin corrección): el
  OTOS no está dando pose y hay que revisarlo (TASK-004 montaje OTOS) antes de evaluar el "derecho".
- TOP de R1 con `top_robot1_pri_rt` y CENTRAL con `central_robot1_mix_bno` (igual que TASK-115/116).

## Cómo validar (en orden)
1. **Compila** (ya hecho) + flashear.
2. **Debug USB (115200, USB de la CENTRAL):** `pio device monitor -b 115200`. Confirmar `otos_hdg`
   se MUEVE al girar el robot a mano y `otos_conf` > 0. (Si no se mueve / conf=0 → arreglar OTOS primero.)
3. **Ruedas al aire** (robot en soporte), forzar una patada (con la FSM, o llamando la primitiva en un
   diag): ver que las 2 delanteras empujen FUERTE y rápido. Al girar el robot a mano durante el empuje,
   las ruedas deben reaccionar para **volver al rumbo** anclado.
4. **En piso, patada real contra una pelota, mirando una línea recta de referencia:**
   - **¿Va derecho?** Si **curva** y la corrección **empeora** la curva (realimentación positiva,
     tiende a girar) → **invertir el signo** de `MIX_KICK_HEADING_KP` en `mix_config.h` (poné `-2.5f`).
   - Si va derecho pero **corrige poco** (sigue curvando suave) → **subir** `MIX_KICK_HEADING_KP`
     y/o `MIX_KICK_CORR_MAX`; si para darle autoridad falta margen, **bajar** `MIX_KICK_VEL_FINAL`
     (deja headroom para el término de giro).
   - Si **zigzaguea** (sobre-corrige) → **bajar** `MIX_KICK_HEADING_KP`.
5. **¿Fuerte/rápida?** ¿La pelota sale con más pique que antes? Si el robot hace **reset al patear**
   (brownout por el pico de corriente) → **bajar** `MIX_KICK_VEL_START` y/o `MIX_KICK_PASO` (rampa
   menos agresiva). Si querés AÚN más fuerte y no hay brownout → `MIX_KICK_VEL_START` más alto.
6. **Regresión:** confirmar que la patada CORTA (por línea, `PATEANDO_corto_*`) y la LARGA usan ambas
   la primitiva nueva (las dos llaman `avanzar_patear`) y que el escape de línea sigue andando.

## Criterio de cierre
- Compila (✅) + flashea.
- En el debug, `otos_hdg` se mueve y `otos_conf` > 0.
- La patada sale **DERECHA** (corrección con el signo correcto) y **más FUERTE/RÁPIDA** que la vieja,
  sin que el robot se resetee.
- Decisión: si suma → queda; si el OTOS no es confiable en partido → se puede dejar el empuje fuerte
  "a ciegas" (sin corrección) bajando `MIX_KICK_HEADING_KP` a 0.

## Escape / rollback
Cualquier env de competencia (`central_robot1` / `central_robot1_delantero_practica`). Para volver a
la patada vieja (lazo abierto) sin revertir todo: poné `MIX_KICK_HEADING_KP = 0` (sin corrección) y
`MIX_KICK_VEL_START`/`PASO`/`VEL_FINAL` a los valores 2025 (0 / 20 / 240). El centralmix sigue siendo
build aislado (no toca `src/central/`).

## Relación
- **TASK-113** (validar centralmix), **TASK-115** (BNO snapshot), **TASK-116** (kickoff). Misma rama.
- **TASK-004** (montaje OTOS): si `otos_conf=0`, esta task depende de resolver el OTOS primero.
