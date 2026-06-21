---
title: "arqueromix — la patrulla rebota por el ARCO PROPIO (cámara trasera), no por la línea"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco)"
status: COMPILA · NO validado en banco · RIESGO marcado (goal_own no validado)
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: feature-banco
---

# arqueromix — patrulla por arco propio

## Pedido (Virginia)

Cambiar el patrullaje: que NO rebote hasta la línea, sino que rebote cuando vea el **arco propio** a
cierto **ángulo** (= llegar al borde del arco) y se vaya al otro lado. Misma lógica de la línea, pero
con la **cámara trasera** siguiendo el arco propio. **Decisión Virginia (preguntada): REEMPLAZAR la
línea del todo en la patrulla** (riesgo aceptado, ver abajo).

## Verificación previa (código, vía workflow de exploración)

Antes de diseñar, verifiqué el flujo del dato:
- `goal_own_angle`/`goal_own_visible` llegan en el snapshot (`types.h:118-123`), poblados por
  `amix_comm` (`apply_top_snapshot`, `amix_comm.cpp:96-97`). Convención robot: 0=frente, +=derecha.
- El TOP **ya rota 180° la cámara trasera** (`cameras_fusion.cpp:45-50`) → el ángulo del arco propio
  viene en el marco del robot, no hay que rotar nada en arqueromix.
- El arco propio está DETRÁS → `goal_own_angle ≈ ±180°` centrado en el arco.
- **`goal_polarity`** decide own/opp por hemisferio, latcheado al arranque (asume robot mirando a la
  cancha; si arranca girado, puede latchear invertido).

## ⚠️ Riesgo marcado (honestidad — NO está validado)

La investigación del compañero (`research/in-progress/2026-06-21-deteccion-arco-*`) es clara:
`goal_own` **NO está validado en banco** (el camino `strategy.cpp` lo tiene deshabilitado a propósito),
depende de la **calibración LAB** de las cámaras y de que la trasera vea el arco. Se lo dije a Virginia;
eligió reemplazar la línea igual. **Riesgo concreto:** si `goal_own_visible=0`, no hay rebote → el
arquero puede irse del arco (sin línea de respaldo en la patrulla). Mitigación: flag de fallback.

## Implementación

- **`amix_config.h`**: sección nueva. `AMIX_PATRULLA_POR_ARCO` (default true; `-DARQMIX_PATRULLA_LINEA`
  → false = rebote por línea viejo). `AMIX_TOL_ARCO_OWN_DEG=30` (umbral de borde, knob principal).
  `AMIX_ARCO_OWN_SIGN` (signo, flippable con `-DARQMIX_FLIP_ARCO_OWN`).
- **`amix_fsm.cpp`**: helpers `wrap180_local`, `rear_goal_dev()` (= wrap180(goal_own_angle−180)×SIGN),
  `borde_arco_der()/izq()` (sólo con `goal_own_visible`). En `moverce_derecha`/`moverce_izquierda`, el
  `if (linea())` → chequeo del borde del arco (gateado por commit). Reúsa los estados `salir_linea_*`
  (la mecánica de rebote no cambia, sólo el trigger). El fallback de línea queda detrás del flag.
- La LÍNEA sigue usándose para el HOMING (`inicio_retroceder`) y el retroceso del despeje
  (`PATEANDO_atras`) — sólo se sacó de la patrulla.

## Verificación

- `pio run -e central_robot2_arqueromix` (default = por arco) → **SUCCESS**.
- `-DARQMIX_PATRULLA_LINEA` (fallback = por línea) → **SUCCESS**.
- ⚠️ Compila ≠ anda. Lo cierra el equipo en banco (regla #1).

## Cómo verificar en banco (Virginia)

1. Flashear `central_robot2_arqueromix`. GO, arquero centrado: debe patrullar y **rebotar en cada
   borde del arco** (sin tocar la línea).
2. ¿Rebota donde NO corresponde el borde? Ajustar `AMIX_TOL_ARCO_OWN_DEG` (subir = patrulla más ancha).
3. ¿Rebota al revés / no rebota? → `-DARQMIX_FLIP_ARCO_OWN`.
4. ¿Se va de largo sin rebotar? La cámara no ve el arco (`goal_own_visible=0`) = el riesgo conocido →
   volver a la línea con `-DARQMIX_PATRULLA_LINEA`, o validar la cámara/LAB primero.

## Archivos

- `amix_config.h`, `amix_fsm.cpp`, `DOCUMENTACION.md` (§17.2 nueva).
