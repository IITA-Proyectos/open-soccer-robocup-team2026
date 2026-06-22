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

## Cómo validar (en orden) — defaults SEGUROS, una perilla por vez
> El diseño se revisó con un red-team multi-agente (journal). Defaults de arranque CONSERVADORES a
> propósito: `MIX_KICK_HEADING_KP=-2.5`, `MIX_KICK_CORR_MAX=30`. Este lazo **NO hereda `OMEGA_SIGN`**
> (es mixer-free) → el signo se valida ACÁ, por separado de la navegación.

1. **Compila** (ya hecho) + flashear.
2. **Debug USB (115200):** `pio device monitor -b 115200`. Confirmar al girar el robot a mano:
   `otos_hdg` se MUEVE, `otos_conf`>0, y durante una patada `kick_err`/`kick_corr` cambian. (Si
   `otos_conf=0` → OTOS no da pose, arreglar primero; la patada igual va recta a ciegas.)
3. **SIGNO primero — robot LEVANTADO (ruedas al aire) + `MIX_KICK_VEL_FINAL` bajo (~100).** Forzar una
   patada, girar el robot a mano ~15° desde el rumbo anclado y mirar la corrección:
   - Si las ruedas reaccionan para **volver al rumbo** → signo OK.
   - Si lo **alejan** (realimentación positiva) → **invertir** `MIX_KICK_HEADING_KP` a `+2.5f`.
   - (Hacerlo levantado y a VEL baja evita que, con el signo mal, el robot se vaya de la cancha.)
4. **AUTORIDAD — tras fijar el signo, en piso, contra una pelota, mirando una recta de referencia:**
   - Subir la magnitud de `MIX_KICK_HEADING_KP` (hacia ~−10) y `MIX_KICK_CORR_MAX` (hacia ~90) hasta
     que corrija **firme sin zigzag**. `kick_err` debe converger a 0 sin overshoot grande.
   - Si **zigzaguea** → bajar `MIX_KICK_HEADING_KP` o `MIX_KICK_CORR_MAX`.
5. **¿Sale DERECHA (no de costado)?** El arreglo de saturación (escalado-vector) saca el **strafe
   parásito** que metía el clamp por-rueda. Validar midiendo **desvío LATERAL** del robot con la
   corrección activa (no solo el ángulo). Si igual deriva de costado apuntando bien → es deriva
   lateral (crab); eso NO lo corrige el heading-hold (es el cross-track futuro, ver Relación).
6. **¿La trasera gira?** M3 solo hace giro; con `corr<107` no llega a su piso y **no gira** (el giro
   sale de M1/M2). Si el giro es flojo, probar `MIX_KICK_REAR_FLOOR=107` y ver si mejora (ojo: puede
   meter zigzag a ganancia chica). Medir si M3 se mueve durante la patada.
7. **OTOS ciego a mitad — test del gate de confidence:** tapar/desconectar un OTOS a mitad de patada y
   confirmar que `kick_corr` cae a 0 (la patada NO se tuerce contra heading basura). Sin el fix de
   confidence esto fallaba; con él, pasa. Distingue "frescura de enlace" de "salud del sensor".
8. **¿Fuerte/rápida?** ¿La pelota sale con más pique? Si el robot se **resetea** al patear (brownout) →
   bajar `MIX_KICK_PASO` (rampa menos agresiva) o `MIX_KICK_VEL_FINAL`.
9. **Regresión:** la patada CORTA (`PATEANDO_corto_*`) y la LARGA usan **la misma** `avanzar_patear` →
   re-titular sirve para las dos, pero **probar las dos**. Confirmar que el escape de línea sigue.

## Criterio de cierre
- Compila (✅) + flashea.
- En el debug, `otos_hdg` se mueve, `otos_conf`>0, `kick_err`/`kick_corr` se ven.
- Signo de `MIX_KICK_HEADING_KP` confirmado (paso 3). La patada sale **DERECHA** (sin strafe parásito) y
  **más FUERTE/RÁPIDA** que la vieja, sin reset.
- El gate de confidence apaga la corrección con OTOS ciego (paso 7).
- Decisión: si suma → queda; si el OTOS no es confiable en partido → empuje fuerte "a ciegas"
  poniendo `MIX_KICK_HEADING_KP=0` (la patada sigue derecha por ser `avanzar` un +Y puro de la matriz).

## Escape / rollback
Cualquier env de competencia (`central_robot1` / `central_robot1_delantero_practica`). Para volver a
la patada vieja (lazo abierto) sin revertir todo: poné `MIX_KICK_HEADING_KP = 0` (sin corrección) y
`MIX_KICK_VEL_START`/`PASO`/`VEL_FINAL` a los valores 2025 (0 / 20 / 240). El centralmix sigue siendo
build aislado (no toca `src/central/`).

## Relación
- **TASK-113** (validar centralmix), **TASK-115** (BNO snapshot), **TASK-116** (kickoff). Misma rama.
- **TASK-004** (montaje OTOS): si `otos_conf=0`, esta task depende de resolver el OTOS primero.

## Mejora futura — CROSS-TRACK (no entra ahora; la generalización correcta de la idea per-OTOS de Elías)
El heading-hold corrige el RUMBO, no la deriva LATERAL (crab): el robot puede apuntar perfecto y
viajar de costado, metiendo la pelota al lado del arco. La corrección de eso es el **cross-track**:
medir cuánto se corrió lateralmente de la recta de patada con `x/y` del OTOS y meter un canal de
**strafe** (que en el omni-3 reparte `+vx/2, +vx/2, −vx` en M1/M2/M3 — ESO sí trata las ruedas
distinto, como intuía Elías, pero por la cinemática, no a ojo). Requiere: (a) cablear `otos_x_mm` /
`otos_y_mm` a `g_io` (hoy `apply_down_pose` los decodifica y los **tira**); (b) anclar la recta
(x0,y0,rumbo) al iniciar la patada; (c) medir en banco que la pose XY del OTOS **no deriva** >~20 mm
durante la patada (~300–800 ms), si no el lazo persigue una recta corrida. → TASK futura aparte.
