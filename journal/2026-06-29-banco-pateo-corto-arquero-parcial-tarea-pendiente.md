# 2026-06-29 — Banco PARCIAL del pateo corto del arquero → queda como tarea pendiente (TASK-119)

**Autor:** Claude (Opus 4.8), sesión con María (arquero R2 / placa CENTRAL).
**Contexto:** seguimiento del commit `f21a653` (pateo corto sobre la línea, gateado).

## Qué se hizo
Se probó en banco el env `central_robot2_arqueromix_kickcorto` (flag `ARQMIX_KICK_SHORT_ON_LINE`:
golpe de 250 ms en vez de 450 ms cuando el despeje arranca sobre la línea). Objetivo: que el arquero
no se salga de la cancha por la inercia del golpe al despejar pegado al borde.

## Resultado (PARCIAL — no concluyente)
- El arquero **estuvo haciendo las cosas** (despejaba), **pero en un momento "se perdió"** (observación
  de María, sin causa confirmada — NO se infiere causa).
- **No había una cancha buena** para repetir y aislar el síntoma → la evaluación no se cerró.

## Decisión
- **NO se promueve el flag.** El arquero de competencia sigue siendo `central_robot2_arqueromix_quieto`,
  **byte-idéntico** al validado (md5 `8d0168cf81f7cdaf347b0ab89e030e59`). Cero cambio de binario.
- El pateo corto queda como **env de banco gateado** + **TASK-119** para cerrar cuando haya cancha.

## Verificación de código hecha esta sesión (no de memoria)
Cableado del flag en `amix_fsm.cpp`: `s_kick_corto = linea()` al entrar a `PATEANDO_adelante` (L273-274);
salida del golpe a `(s_kick_corto ? 250 : 450)` ms (L300). **El corte por línea durante el golpe y el
freno-plugging (`frenar_patada`) YA estaban en el arquero validado** — el aporte real del pateo corto es
solo recortar 200 ms de empuje cuando el golpe arrancó pisando la línea. Esto importa para interpretar el
banco: si los despejes evaluados NO arrancaron sobre la línea, el flag ni se activó (ver TASK-119, paso 1).

## Pendiente
TASK-119 (`team-tasks/2026-06-29-task-119-...`): re-probar en cancha buena, confirmar que el flag se
activa, medir beneficio (¿deja de salirse?) vs costo (¿despeja lo suficiente?), y caracterizar el "se
perdió". Decisión final dejar/tunear/rollback con la matriz de la TASK. Feature hermana a evaluar junto:
escape de línea al orientar (`e95df40`, env `central_robot2_arqueromix_orientesc`).
