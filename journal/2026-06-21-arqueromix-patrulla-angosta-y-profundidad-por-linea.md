---
title: "arqueromix — patrulla más angosta + control de profundidad por LÍNEA (no meterse al área)"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco) + workflow de verificación"
status: COMPILA · NO validado en banco
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: feature-banco
---

# arqueromix — patrulla angosta + profundidad por línea

## Contexto y decisión (Virginia, vía preguntas)

El arquero se mete al **área chica** (deriva hacia ATRÁS). Virginia propuso usar la cámara para la
profundidad; lo verifiqué con un **workflow** y la cámara **NO sirve para distancia** (~20-30%: pierde
el arco justo cuando está cerca — desenfoque/fuera de FOV/timeout). Virginia confirmó (preguntas):
- El "se acerca mucho" es a la **línea de fondo / atrás (PROFUNDIDAD)**, no lateral.
- Quiere el recorrido **lateral más angosto** (más centrado).

Conclusión: profundidad NO por cámara → por **LÍNEA** (señal confiable). Es lo que ella misma describió
la primera vez ("avanzar para salirse de la línea con impulso corto").

## Checkpoints guardados (tags)

- `arqueromix-funciona-arco-fallback-2026-06-21` (la versión que validó que funciona).
- `arqueromix-startup-fix-2026-06-21` (con el fix del arranque; checkpoint antes de ESTE cambio).

## Cambios

1. **Recorrido lateral angosto:** `AMIX_TOL_ARCO_OWN_DEG` 30°→**20°** (más centrado al arco). Lo lateral
   sigue por la cámara (ángulo), que SÍ es confiable.
2. **Profundidad por línea** (`AMIX_PROFUNDIDAD_POR_LINEA`, default ON; `-DARQMIX_NO_PROFUNDIDAD` apaga):
   en `moverce_derecha`/`moverce_izquierda`, si `goal_own_visible && linea()` → `estado = inicio_avanzar`
   (avanza recto al frente HASTA despegar de la línea → vuelve a patrullar). Reúsa el estado del homing.

## Por qué es confiable (la regla que evita el conflicto)

La línea tiene dos roles según si se ve el arco:
- **VE el arco:** lo lateral lo resuelve el ÁNGULO del arco → la línea queda LIBRE para profundidad
  (línea detectada = derivó atrás → avanzar al frente).
- **NO ve el arco:** la línea es el rebote LATERAL (fallback) y la profundidad NO actúa.
El avance va RECTO al frente (hacia el campo, lejos del fondo) = saca del área. 100% local (sin cámara).

## ⚠️ Límite honesto

Cuando la cámara NO ve el arco, NO hay control de profundidad (la línea se usa para lo lateral). En ese
modo degradado el arquero puede derivar atrás. Es el costo de no tener una señal de profundidad
independiente. Con el arco visible (caso normal) sí protege. Marcado en doc §17.3.

## Verificación

- `pio run -e central_robot2_arqueromix` (default) → **SUCCESS**.
- `-DARQMIX_NO_PROFUNDIDAD` → **SUCCESS**.
- ⚠️ Compila ≠ anda. Lo cierra el equipo en banco.

## Plan de prueba en banco (Virginia)

1. ¿La patrulla quedó más angosta/centrada? Ajustá `AMIX_TOL_ARCO_OWN_DEG` (20°).
2. Empujá el arquero hacia atrás (al área) MIENTRAS ve el arco: al detectar la línea del fondo debe
   **avanzar al frente y salir**, no quedarse adentro.
3. ¿Avanza de más / oscila adelante-atrás? Es el `inicio_avanzar` (knobs `AMIX_T_INICIO_AVANCE_MIN/SAFETY`).
4. ¿Algo raro? Apagá la profundidad con `-DARQMIX_NO_PROFUNDIDAD` y queda como el checkpoint.

## Archivos

- `amix_config.h` (`AMIX_TOL_ARCO_OWN_DEG` 30→20 + `AMIX_PROFUNDIDAD_POR_LINEA`).
- `amix_fsm.cpp` (depth check en moverce_*).
- `DOCUMENTACION.md` (§17.3 nueva).
