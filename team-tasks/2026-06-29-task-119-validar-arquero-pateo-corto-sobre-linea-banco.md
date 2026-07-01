# TASK-119 — Validar en banco el PATEO CORTO sobre la línea del arquero (arqueromix)

- **Placa:** CENTRAL (R2, arquero — Virginia/María). TOP `top_robot2_pri` + DOWN `down_robot2` sin cambios.
- **Asignado:** equipo (banco) — Virginia / María / Gustavo
- **Prioridad:** P2 (mejora deseable; el arquero de competencia validado YA anda y NO se toca hasta cerrar esto)
- **Estado:** abierta — **evaluación PARCIAL, sin cerrar** (faltó cancha buena para terminar, 2026-06-29)
- **Build (banco):** `pio run -e central_robot2_arqueromix_kickcorto -t upload`
- **Escape / competencia:** `pio run -e central_robot2_arqueromix_quieto -t upload` (vuelve EXACTO, byte-idéntico md5 `8d0168cf81f7cdaf347b0ab89e030e59`)

## Por qué
El arquero, al despejar arrancando SOBRE la línea, se salía de la cancha por la inercia del golpe.
El flag `ARQMIX_KICK_SHORT_ON_LINE` (commit `f21a653`) acorta el empuje al frente del golpe a **250 ms**
(`AMIX_T_PAT_ADELANTE_CORTO`) en vez de **450 ms** (`AMIX_T_PAT_ADELANTE`), SOLO cuando el golpe ARRANCA
sobre la línea. Es la misma idea del pateo corto del delantero (centralmix, `PATEANDO_corto`).

**Cableado verificado en código (`amix_fsm.cpp`):**
- Al entrar a `PATEANDO_adelante` (saliendo de ALINEAR): `s_kick_corto = linea()` — ¿pisa línea al arrancar el golpe? ([amix_fsm.cpp:273-274](../software/teensy/Soccer%202026/src/arqueromix/amix_fsm.cpp#L273)).
- En el golpe: sale por tiempo a `(s_kick_corto ? 250 : 450)` ms ([amix_fsm.cpp:300](../software/teensy/Soccer%202026/src/arqueromix/amix_fsm.cpp#L300)).
- ⚠️ **Las otras dos protecciones YA estaban en el arquero validado, NO son nuevas:** el corte por línea
  durante el golpe (`if (linea()) → frenar_patada`) y el freno-plugging (`frenar_patada`, 200 PWM × 250 ms).
  Entonces el aporte REAL del pateo corto es chico y específico: recortar 200 ms de empuje cuando el golpe
  empezó pisando la línea y todavía no se re-disparó el freno.

## Resultado de banco PARCIAL (2026-06-29, María) — por qué queda pendiente
- El arquero con el pateo corto **estuvo haciendo las cosas** (despejaba), **pero en un momento "se perdió"**
  (observación cruda — la CAUSA no está confirmada, no inventar).
- **No había cancha buena** para repetir y aislar el síntoma → la evaluación NO se pudo cerrar.
- Decisión consecuente: **NO se promueve el flag.** El binario de competencia sigue siendo el validado
  `central_robot2_arqueromix_quieto` (byte-idéntico). El pateo corto queda como env de banco gateado.

## Qué averiguar cuando haya cancha buena (en orden)
1. **¿El pateo corto se está ACTIVANDO?** Confirmar que los despejes que se evalúan ARRANCAN con el arquero
   pisando la línea (si no, `s_kick_corto=false` → patea 450 ms → el flag no actúa y se ve el arquero de antes).
   Mirar por el monitor (`tools/monitor-base`, `python -m monitor_base --monitor`) o debug USB la línea/estado.
2. **¿Dejó de salirse al despejar?** (el beneficio) — contar éxitos: ¿en cuántos de cuántos intentos? ¿repetible?
3. **¿Despeja lo suficiente?** (el costo de acortar) — ¿saca la pelota de la zona de peligro o queda picando cerca?
4. **El síntoma "se perdió":** caracterizar QUÉ pasó (¿se desorientó / heading derivó / se quedó en un estado /
   se metió al área / se fue a un borde?). Mirar `estado`, `heading`/`hdg_valid`, línea y pose en el monitor en el
   momento del fallo. Esto decide si el problema es del pateo corto o de otra cosa del arquero (puede ser independiente).

## Notas de observación en cancha — hipótesis María (2026-06-29): el escape no alcanza → queda SOBRE la línea → arruina el movimiento siguiente
> Verificado en `amix_fsm.cpp`: **todos los estados que despegan de la línea salen por un TIMEOUT de seguridad
> como último recurso.** Si el timeout vence mientras todavía pisa la línea, el arquero pasa al siguiente
> estado IGUAL, sobre la línea. El escape NO es garantía de "me despegué", es "intento con límite de tiempo".
> Esto da una base mecánica concreta a la hipótesis. **Qué mirar en el monitor (`--monitor`): el campo
> `estado` y la línea — en cada estado de escape, ¿salió porque la línea se apagó (bien) o por timeout con
> la línea todavía prendida (mal)? ¿Y el movimiento que sigue arranca torcido / cruza la línea / se sale?**

**PRIMERO de todo — el signo del escape (causa #1 candidata):** el escape se mueve según `line_angle_deg`,
cuya convención de signo NO está validada en banco (`AMIX_ACOMODAR_LINEA_SIGN`). **Si el signo está al revés,
el escape empuja HACIA la línea en vez de despegarse → nunca se despega → siempre vence el timeout sobre la
línea = exactamente el síntoma.** Chequear: al pisar la línea, ¿el arquero se ALEJA del borde o se mete más?
Si se mete → recompilar con `-DARQMIX_FLIP_ACOMODAR_LINEA` y re-probar. Es la perilla de mayor impacto.

Momentos a observar (mapeados al FSM):
1. **Al iniciar** (`inicio_lateral_izq`→`inicio_retroceder`→`inicio_avanzar` L184→`acomodar_linea` L395):
   el avance de salida sale por `!linea()` **o** safety `AMIX_T_INICIO_AVANCE_SAFETY`=1200 ms; después
   `acomodar_linea` por `!linea()` **o** safety `AMIX_T_ACOMODAR_LINEA_SAFETY`=1500 ms. ¿Sale despegado o por
   safety pisando línea? Si por safety → entra a orientar sobre la línea.
2. **Al patear** (`PATEANDO_adelante`→`frenar_patada` L320): el freno-plugging es por TIEMPO
   (`AMIX_T_FRENO_PATADA`=250 ms) y **NO chequea línea**. ¿Queda despegado tras el freno, o sigue a la pausa
   cerca de la línea? Si no despega → subir `AMIX_T_FRENO_PATADA` o `AMIX_FRENO_PATADA_PWM`.
3. **Al volver del pateo** (`PATEANDO_atras` L343 → `acomodar_linea`/`orientar_frente` L370): el retroceso va
   A PROPÓSITO hasta la línea; después tiene que despegarse. El escape-al-orientar (`-DARQMIX_ORIENT_LINE_ESCAPE`,
   env `_orientesc`) solo actúa mientras `t < AMIX_T_ORIENTAR_SAFETY`=3000 ms; vencido eso, **gira igual sobre
   la línea**. ¿El giro de orientación queda apuntando bien o lo dejó pegado al borde?

Perillas si el escape no alcanza (subir = despega más, ojo de no salirse para el otro lado):
- `AMIX_T_ACOMODAR_LINEA_SAFETY` (1500), `AMIX_T_INICIO_AVANCE_SAFETY` (1200), `AMIX_T_ORIENTAR_SAFETY` (3000):
  más tiempo de escape antes de rendirse. ⚠️ subir demasiado = se cuelga despegándose.
- `AMIX_PD_BASE` (0.85): la fuerza del strafe de despegue. Si las delanteras (piso ~70) tironean, el despegue
  es débil → subir hacia 0.90.
- `ARQMIX_FLIP_ACOMODAR_LINEA`: invierte el sentido del escape (ver "causa #1" arriba).

## Criterio de cierre + DECISIÓN (humano, en cancha)
Según resultado, cae en una de estas (el arquero validado es el piso a batir — la vara para tocar el binario es ALTA):

| ¿Se sale? | ¿Despeja bien? | Decisión |
|---|---|---|
| No (dejó de salirse) | Sí | **Dejarlo** → promover el flag al binario de competencia (cambia el binario → **re-validar el arquero completo**, no solo el despeje) |
| No | No (queda corto) | Subir `AMIX_T_PAT_ADELANTE_CORTO` 250→300/350 y re-probar (todavía no es rollback) |
| Sigue saliéndose | Sí | La duración no era el problema → la palanca es el freno (`AMIX_FRENO_PATADA_PWM`/`AMIX_T_FRENO_PATADA`) → probable rollback |
| Sigue saliéndose | No | El cambio no resolvió nada → **volver atrás** (quedarse con `_quieto`) |

Cierre = decisión tomada (dejar/tunear/rollback) con evidencia repetible (≥5 intentos), y el síntoma "se perdió"
explicado o descartado como independiente del pateo corto.

## Perillas de tuning (en `src/arqueromix/amix_config.h`)
- `AMIX_T_PAT_ADELANTE_CORTO` (250 ms, bajo `#ifdef ARQMIX_KICK_SHORT_ON_LINE`): **subir** si no despeja lo
  suficiente; **bajar** si todavía se sale.
- `AMIX_FRENO_PATADA_PWM` (200) / `AMIX_T_FRENO_PATADA` (250 ms): el freno-plugging que mata la inercia (esta es
  la palanca si el problema es la inercia, no la duración del golpe).

## Rollback (trivial — todo gateado)
- Volver al validado: `pio run -e central_robot2_arqueromix_quieto -t upload` (byte-idéntico).
- Borrar el código si se decide descartar: `git revert f21a653` (pateo corto) / `git revert e95df40` (escape de
  línea al orientar, env `central_robot2_arqueromix_orientesc` — feature hermana, también gateada y pendiente).

## Relación
- TASK-114 (validar arqueromix general). Misma rama arquero.
- Feature hermana sin cerrar: **escape de línea al orientar** (`-DARQMIX_ORIENT_LINE_ESCAPE`, env
  `central_robot2_arqueromix_orientesc`, commit `e95df40`) — también anti-"salirse de la cancha", gateada,
  pendiente de banco. Conviene evaluarlas juntas cuando haya cancha.
