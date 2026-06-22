# 2026-06-21 — centralmix: patada RECTA y FUERTE con corrección de rumbo por OTOS

## Qué se pidió (Elías)
> "ahora está pateando con una rampa de velocidad y unos PWM para los motores fijos, ¿podrías
> hacer que lo haga con los OTOS? y que lo haga derecho y fuerte, una patada fuerte y rápida."

Y un segundo punto: **verificar que el TOP mande heading válido** (si `heading_error_deg` queda en 0).

## Diagnóstico de la patada vieja
`avanzar_patear()` (port 2025) era **lazo abierto**: rampa lenta (0 → 240 de a 20 cada 10 ms ≈ 120 ms)
sobre M1=+vel / M2=−vel / M3=0, **sin realimentación**. Si las dos ruedas delanteras no están parejas
(fricción, batería, tolerancias), la patada **curva** y no hay nada que lo corrija. No era ni
especialmente fuerte ni recta.

## Qué se hizo (solo la primitiva + plomería de datos; la FSM NO se tocó)
La patada sigue llamándose `avanzar_patear()` (la FSM la llama igual en `PATEANDO_adelante` y en
`PATEANDO_corto_adelante` → las dos patadas, larga y corta, se actualizan solas). Ahora:

1. **FUERTE y RÁPIDA.** Arranca en `MIX_KICK_VEL_START` (120, **sobre el piso** del motor ~70 → bite
   inmediato) y rampa **agresiva** (`MIX_KICK_PASO`=45 cada `MIX_KICK_INTERVALO_MS`=8 ms) hasta
   `MIX_KICK_VEL_FINAL`=255 → **~24 ms a tope** (antes ~120 ms a 240). La rampa **no desaparece**
   (evita brownout del regulador por el pico de arranque), pero es mucho más rápida.

2. **DERECHA (heading-hold con el OTOS).** Al **entrar** a la patada (flanco) se **ancla**
   `otos_heading_deg` como objetivo y se decide si el OTOS es confiable AHORA (`otos_confidence>0` +
   pose fresca < `MIX_KICK_OTOS_FRESH_MS`=200 ms). En cada tick se mide el error de rumbo y se agrega
   un término de **giro** —mismo signo en las 3 ruedas, **exactamente como `girar()`**— proporcional
   al error (`MIX_KICK_HEADING_KP`=2.5 PWM/°), **clampeado** a `MIX_KICK_CORR_MAX`=70. El empuje
   siempre domina → aunque el signo del Kp esté mal **no descontrola** (a lo sumo curva, no gira en el
   lugar). Si el OTOS no está fresco/sano → `corr=0` (patada recta **a ciegas**, como antes pero rápida).

   **Por qué el OTOS y no el BNO:** `otos_heading_deg` viene del OTOS **local de DOWN** y se puebla
   SIEMPRE, independiente de la fuente de heading de navegación (el BNO del TOP por snapshot en el env
   `_bno`). Así la patada sale derecha aunque el `heading_valid` del snapshot esté en 0 (ver pendiente #2).

### Detalle de implementación
- `mix_io.h`: + `otos_confidence` (0/60/100, salud del OTOS de DOWN) y `t_last_otos_pose_ms`
  (frescura **específica** de la pose — `t_last_down_frame_ms` también sube con la línea, no servía).
- `mix_comm.cpp` (`apply_down_pose`): puebla esos dos campos desde el `Pose2D` (fuera de todo `#ifdef`,
  vale en los 3 modos de heading).
- `mix_config.h`: bloque del kicker reescrito — `MIX_KICK_VEL_START/FINAL/PASO/INTERVALO_MS` +
  `MIX_KICK_HEADING_KP/CORR_MAX/OTOS_FRESH_MS`. Todo tuneable.
- `mix_motors.cpp`: `avanzar_patear()` reescrito (rampa rápida + corrección); `parar()` ahora cierra
  la rampa (`s_kick_active=false`) → cada empuje **re-ancla** el rumbo aunque la patada se aborte por
  línea; `wrap180` local; estado nuevo `s_kick_heading_target` / `s_kick_use_otos`. Ahora se escriben
  los motores en CADA tick (la corrección cambia entre intervalos), barato.
- `main_centralmix.cpp`: **print de debug** por USB Serial (115200), throttleado a ~6,7 Hz, con `if
  (Serial)` (sin PC no cuesta nada) y apagable con `-DMIX_NO_DEBUG_SERIAL`. Muestra pelota + heading
  (`hdg`/`hvalid`/`herr`) + OTOS (`otos_hdg`/`otos_conf`) + arco + línea + árbitro + frescura de enlaces.
  (El print de la pelota que Elías recordaba **no estaba** en el commit; se debió perder. Este lo repone
  y amplía.)

## Pendiente #2 — ¿el TOP manda heading válido?
Cadena verificada en el TOP (`src/top/main_top.cpp:217,284`): `s.my_heading_centideg` **siempre** se
manda (del IMU), pero el **bit4 `heading_valid`** se prende SOLO si `sensors_imu_get_heading_valid()`
(= `g_fusion.fused_valid`). Y `mix_comm` (`apply_top_snapshot`, `#ifndef MIX_HEADING_OTOS`) actualiza
`heading_error_deg` **únicamente cuando ese bit4 llega prendido**. ⇒ **si el bit4 no llega,
`heading_error_deg` queda clavado en 0** (exactamente la sospecha de Elías). El fix del BNO del TOP
(`sensors_imu.cpp:279`, `bno_left_en`) y el banco del 2026-06-21 dicen que el BNO da heading y
`fused_valid` debería ser true con `top_robot1_pri_rt` — pero **end-to-end hasta el `herr` del
centralmix NO está verificado**. El debug print de arriba lo deja medir directo en banco (girar el
robot → ¿`hvalid=1` y `herr` se mueve?). **No lo puede cerrar Claude (regla #1).** → TASK-115/117.

## REVISIÓN post red-team (workflow multi-agente, 2026-06-21) — Elías cuestionó "misma corr a las 2 ruedas"
Elías preguntó si sumar la misma `corr` a ambos motores delanteros era un problema, y pidió diseñar
la patada bien con un workflow. Se corrieron 10 agentes (cinemática R1 + datos OTOS + integración →
3 diseños → red-team adversarial). Hallazgos que **cambiaron la implementación**:

- **"Misma corr a las 3" NO es bug — es giro PURO.** En el omni-3 el término de giro es `ω·R`,
  **idéntico** para las 3 ruedas (mismo radio). Es lo que hace `girar()`. Sumarla solo a 2 SÍ sería
  bug (metería strafe). Verificado contra `src/shared/kinematics.cpp` (`v_i=-vx·sinθ+vy·cosθ+ω·R`) y
  `WHEEL_ANGLES={330,210,90}`. De paso: `avanzar()` {+,−,0} **es +Y puro** en esa geometría.
- **El bug REAL era la saturación.** El clamp **por-rueda** de `mix_set_motor` (M1 satura a 255, M2 no)
  TUERCE el vector e inyecta **strafe lateral parásito** (la patada sale de costado aunque apunte
  bien). FIX: **escalar el vector** de 3 ruedas por un factor uniforme (preserva dirección) — mismo
  patrón que `saturate_wheels()` del shared. (El "42% de asimetría L/R" que dije primero era
  impreciso: el clamp es simétrico CW/CCW; lo que rompe es la dirección por strafe.)
- **Piso de la trasera (M3).** M3 solo hace giro; con `corr<107` (su piso físico, `MOTOR_MIN_PWM`
  rear) **no gira** → el giro queda en el diferencial M1/M2. Se agregó `MIX_KICK_REAR_FLOOR` (default
  0=off; bang-bang a ganancias chicas) para elevar M3 al piso si en banco el giro es flojo.
- **OTOS vivo-pero-ciego.** El gate por-tick miraba solo frescura. `apply_down_pose` sella el timestamp
  **incondicionalmente**, así que con `confidence=0` pero DOWN mandando pose, la pose seguía "fresca"
  y se corregía contra basura. FIX: el gate ahora exige `g_io.otos_confidence>0` (una línea).
- **Seguridad del signo.** Este lazo es **mixer-free** → NO hereda `OMEGA_SIGN=-1` (el fix del único
  caso de realimentación POSITIVA del repo). El default se bajó a `KP=-2.5 / CORR_MAX=30` (era −10/90):
  con el signo mal, a 240 PWM por 500 ms el robot se iría de la cancha. Se titula de menor a mayor,
  confirmando el signo con el robot LEVANTADO y VEL bajo primero.
- **Cross-track (la generalización correcta de la idea per-OTOS de Elías):** corrige la deriva LATERAL
  (crab), que el heading-hold no ve. Queda como **mejora futura** (cablear `otos_x/y_mm` a `g_io` —
  hoy se decodifican y se tiran— y medir la deriva XY del OTOS en la patada). NO entra ahora.

Diagnóstico nuevo: `g_io.kick_err_deg` / `g_io.kick_corr` (los escribe `avanzar_patear`, los imprime
el debug por USB) para titular la corrección en vivo.

## Compilación
`pio run -e central_robot1_mix_bno` y `central_robot1_mix` → **SUCCESS** (FLASH ~20,1 KB con el debug).
Warnings `PRIMER_IMPULSO_INICIAL_GIRANDO`/`ESPERAR` sin `case` = preexistentes (estados muertos del enum 2025).

## ⚠️ Sin verificar (lo que cierra el banco)
- **No probado en hardware** (regla #1). El **signo** de la corrección (`MIX_KICK_HEADING_KP`) y la
  fuerza/rampa se titulan en banco → **TASK-117** (con árbol de tuning: signo, autoridad, zigzag, brownout).
- Que DOWN dé `otos_conf>0` (si no, la patada va recta a ciegas). Depende de TASK-004 (montaje OTOS).

Ver [[project-iita-soccer-2026-strategy]]. Hermano de `2026-06-21-centralmix-kickoff-medialuna.md` y
`2026-06-21-centralmix-bno-del-top-snapshot.md`.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
