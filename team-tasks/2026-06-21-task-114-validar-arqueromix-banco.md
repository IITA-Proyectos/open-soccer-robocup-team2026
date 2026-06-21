# TASK-114 — Validar en banco el arqueromix (port arquero 2025 sobre TOP/DOWN)

- **Placa:** CENTRAL (R2, arquero — Virginia)
- **Asignado:** equipo (banco) — Virginia / Gustavo / Elías
- **Prioridad:** P2 (es una PRUEBA paralela; el stack actual `src/central/` no se toca ni se reemplaza hasta que esto se valide)
- **Estado:** abierta
- **Build:** `pio run -e central_robot2_arqueromix -t upload` (compila OK 2026-06-21, FLASH ~19 KB). Escape: `central_robot2_arquero`.

## Por qué
Versión experimental: FSM del ARQUERO 2025 + manejo directo de motores, alimentado por datos
de TOP/DOWN (`src/arqueromix/`, sin world_model). Es el hermano arquero de `centralmix` (que
hizo lo mismo con el delantero el viernes 2026-06-19). Objetivo: debuggear el arquero con la
FSM conocida + aprender de la experiencia del viernes. Compila pero **compilar ≠ anda**.
Ver journal `2026-06-21-arqueromix-port-arquero-2025.md` + `src/arqueromix/DOCUMENTACION.md`.

## Cómo validar (en orden — cada paso desbloquea el siguiente)
1. **Primitivas de motor, una por una, SIN FSM** (robot en soporte, ruedas al aire): verificar
   que `adproporcional` hace strafe a la DERECHA, `aiproporcional` a la IZQUIERDA, `avanzar` va
   al frente, `avanzar_patear` patea, `patear_atras` retrocede recto, `impulso_inicial_mov` da
   el strafe fuerte. ⚠️ El 2025 arquero era ROBOT1 con su layout; con los pines 2026 alguna
   primitiva puede salir invertida o lateral. Corregir signos por rueda en `amix_motors.cpp`
   (o `AMIX_MOTOR_INVERT` en `amix_config.h`).
2. **Comm:** confirmar que `g_aio` se puebla (pelota x/y, heading + heading_valid, línea
   present/depth, match_running) con datos reales de TOP/DOWN. Sin esto la FSM ve todo en cero.
   El heading viene del **snapshot del TOP** (NO BNO local) — confirmar que llega válido.
3. **Signo lateral de la pelota:** poner la pelota a la derecha del arquero y ver que va a la
   derecha. Si va al revés → invertir `ball_a_la_derecha()` en `amix_fsm.cpp`.
4. **Re-tuneo píxeles→mm** (mirando la telemetría de la pelota): `AMIX_TOL_CERCANIA_MM` (cuándo
   patea por profundidad), `AMIX_TOL_CENTRADO_MM` (cuándo la considera centrada), `AMIX_TOL_DESVIO_MM`
   (cuándo corrige lado). Estaban en píxeles 2025 (140/3/5), ahora son mm.
5. **Línea desde DOWN:** confirmar que `line_present` se prende al tocar la línea lateral del
   arco (rebote) y al volver tras el retroceso. El 2025 distinguía s1/s2/s3; acá es una señal
   agregada de DOWN. Si hace falta el "qué lado", refinar por `line_angle_deg` (como centralmix).
6. **FSM completa, robot en piso:** patrulla lateral siguiendo la pelota → al tenerla cerca+centrada
   despeja (200→450→1000 ms → retroceso a la línea → 1000 ms) → retoma patrulla. Verificar que el
   rebote en el borde (350 ms) no lo mete al arco.

## Criterio de cierre
- Las primitivas de motor van en el sentido correcto (strafe der/izq, avance, patada, retroceso).
- `g_aio` refleja TOP/DOWN en vivo (incluido heading válido del TOP).
- El arquero patrulla siguiendo la pelota y despeja al tenerla cerca, con umbrales re-tuneados
  y sin meterse al arco.
- **Decisión:** si anda → se sigue mejorando como alternativa de arquero (aprendiendo de las
  mejoras P0/P1 del análisis fiel: localización por ToF/OTOS, heading-hold real, ball_predict);
  si no → se descarta y se sigue con `src/central/` (sin costo, build aislado).

## Mejoras candidatas (post-validación, del ANALISIS-FIEL-ARQUERO-2025 §7)
- **P0** watchdog + estado seguro (el retroceso sin timeout del 2025 ya tiene safety acá).
- **P1** localización absoluta del arco (ToF+OTOS) para volver al punto exacto, no "hasta ver blanco".
- **P1** heading-hold real (cerrar el PID que el 2025 calculaba y tiraba).
- **P1** `ball_predict` para anticipar el tiro (la banda muerta 2025 lo dejaba quieto justo al moverse).

## Escape / rollback
Flashear `central_robot2_arquero` (arquero de competencia). El arqueromix no toca nada de eso
(build aislado: `build_src_filter = +<arqueromix/> +<shared/>`). TOP (`top_robot2_pri`) y DOWN
(`down_robot2`) no cambian.
