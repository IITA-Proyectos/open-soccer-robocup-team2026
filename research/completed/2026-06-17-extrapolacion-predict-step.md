---
title: "Extrapolación lineal (predict step) para datos stale a la CENTRAL — análisis + implementación gateada del heading"
date: 2026-06-17
author: "Claude (Opus 4.8) — coach técnico"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: completed
tipo: research + implementacion-gateada
gate: -DTOP_ENABLE_HEADING_PREDICT (default OFF → competencia byte-idéntica)
banco: team-tasks/2026-06-17-task-218-banco-heading-predict.md
---

# Extrapolación lineal (predict step): no mandar el dato viejo quieto, mandarlo mejorado

## 0. La pregunta

> "En vez de mandar el dato viejo quieto, ¿podemos enviar el dato viejo MEJORADO con
> una estimación de la posición actual? Ej: ángulo_actual ≈ ángulo_anterior + ω·Δt.
> Lo mismo con la pelota y otros datos." — Gustavo, 2026-06-17

Sí. Es técnica estándar y tiene nombre: **paso de predicción (predict step)** de un
estimador predict/correct — formalmente **dead reckoning de 1er orden** / **filtro
alpha-beta** / el predict de un Kalman. Mandar el último valor "quieto" es un
*zero-order hold* (asume velocidad cero entre muestras); `valor + velocidad·Δt` es un
*first-order hold* (asume velocidad constante).

**Matiz que ordena todo:** extrapolar **NO crea información nueva** — descuenta la
**latencia/staleness** entre muestras. No reemplaza subir la tasa de sensado (análisis
hermano, complementario, sobre la TASA confiable — pendiente de redactar) ni el fix del
freeze del BNO. Es **aditiva**: este doc ataca la LATENCIA; aquel atacaría la TASA.

## 1. Las 3 reglas duras (sin ellas, extrapolar es PEOR que el hold)

Verificadas adversarialmente (workflow 11 agentes + datasheets, 2026-06-17):

1. **Velocidad MEDIDA, no diferenciada.** ω del gyro y Δ del OTOS = el sensor mide la
   tasa directo → derivada limpia → seguro. La velocidad de la pelota se *diferencia*
   de posiciones de cámara ruidosas (~10 fps) → extrapolar eso amplifica ruido
   (σ_vel ≈ σ_pos·√2/Δt). Regla: extrapolar gana solo si **|velocidad| ≫ ruido**.

2. **ACOTAR Δt con un cap propio + decaer confianza.** El error crece lineal con Δt
   (ruido) y cuadrático con Δt² (aceleración no modelada). **HALLAZGO P0:** el único
   guard de dato-muerto del firmware es `SNAPSHOT_TIMEOUT_MS=500` (world_model.cpp),
   **demasiado grueso** para un extrapolador (un dato de 480 ms se proyectaría como
   válido → posición absurda). Hace falta un **`max_extrap_ms` ~50-80 ms**,
   obligatorio, no opcional.

3. **RESET por evento.** Re-anclar al dato absoluto fresco cuando cambia (borra
   deriva); deadband cuando está quieto (no extrapolar ruido); resetear en eventos
   bruscos (rebote de pelota, re-detección, freeze).

## 2. Veredicto por señal

| Señal | ¿Extrapolar? | Velocidad | Estado | Por qué |
|---|---|---|---|---|
| **Heading** | **SÍ — el mejor** | MEDIDA (ω gyro, limpia) | **IMPLEMENTADO gateado (este doc)** | ω medida + mitiga el freeze del BNO |
| **Pose XY** | SÍ — canónico | MEDIDA (Δ OTOS) | `pose_fusion`/`pose_filter` ya escritos, gateados (`-DTOP_ENABLE_POSE_FUSION`) | Es literalmente el predict step; falta banco |
| **Pelota** | CONDICIONAL — ya corre | DIFERENCIADA (ruidosa) | `ball_predict` cableado (arquero) | NO extrapolar la entrada (doble-conteo con el D del PID = temblor); mejorar `ball_predict` con la edad + gate de salto de dirección |
| **Línea** | **NO** | — | `LineStatusV2` @200 Hz | Señal de seguridad; extrapolar un cruce = falso anti-out |
| **Obstáculo** | **NO** | — | `min_obstacle_mm` | Sin modelo de velocidad del rival → obstáculos fantasma |

**Por qué el heading primero y solo:** velocidad medida (la regla #1 le da vía libre),
es la raíz del mapa de localización, y la extrapolación con el gyro **puentea el modo
de falla estrella** (heading congelado por contención I²C, banco 2026-06-02/08). La
pelota ya tiene `ball_predict` y agregar extrapolación de *entrada* sería doble-conteo;
la pose ya tiene `pose_fusion` gateado; línea/obstáculo nunca.

## 3. Qué se implementó (gateado, este commit)

**Módulo PURO `src/shared/heading_predict.h`** (host-testeado, `test_heading_predict`
14/14): `heading_estimado = ancla + ω·Δt` con las 3 reglas horneadas — ω de entrada
con clamp, cap `max_extrap_ms`, deadband, re-anclaje en muestra fresca, wrap a ±18000
centideg. Defaults: cap 60 ms, clamp ±600°/s, deadband 2°/s (tunear en banco).

**Cableado gateado `-DTOP_ENABLE_HEADING_PREDICT` (default OFF):**
- `sensors_imu.cpp` — alimenta el predictor en el tick con el heading fusionado + la
  ω del primario YA leída (`in[0].gyro_z_dps`, cero I²C extra) + getter
  `sensors_imu_get_heading_centideg_predicted()` que extrapola al instante de uso.
- `main_top.cpp` (build_snapshot) y `snapshot_emitter.cpp` (emisor por timer de
  competencia) — los **dos** sitios de transmisión usan el heading predicho bajo el
  flag; con el flag OFF, el `#else` es la línea de hoy → **binario byte-idéntico**.
- **El heading CRUDO queda intacto** para el detector de freeze (`imu_freeze`, que
  necesita igualdad EXACTA) y `localization` — solo cambia el heading TRANSMITIDO.
- Env de banco `top_robot2_pri_hpredict` en `platformio.ini`.

**Por qué es de bajo riesgo:** 100% firmware host-testeable, cero HW, cero cambio de
contrato (viaja en el campo `my_heading_centideg` existente, 31 B sin cambios),
competencia byte-idéntica por construcción (#ifdef + #else-original).

## 4. Lo que NO se hizo (y por qué)

- **Extrapolar la pelota de ENTRADA:** NO. El backfire real (verificado) es el
  *adelanto de fase apilado*: la entrada extrapolada + `ball_predict` + el término D
  del PID derivan el ruido de la velocidad de cámara DOS veces → el arquero tiembla.
  Regla maestra: **anticipar la pelota UNA sola vez, en `ball_predict`**. Mejora futura
  (P2): compensar la edad DENTRO de `ball_predict` (`lookahead_efectivo = 0.2 s − edad`)
  + gate de "salto de dirección → reset velocidad" en `ball_velocity` (el rebote no
  dispara las guardias actuales). NO en este commit.
- **Pose XY:** ya está `pose_fusion` gateado; cablearlo/tunearlo es otra TASK.
- **Extrapolación del lado CONSUMIDOR (CENTRAL):** la placement ideal (la CENTRAL
  conoce la edad real del dato) requiere subir ω al snapshot (cambio de contrato 31 B).
  Esta implementación extrapola en el TOP (compensa el staleness intra-TOP + el
  freeze-bridge). El paso consumidor queda como follow-up post-Incheon.

## 5. Lo que NO resuelve (honestidad)

- No crea información nueva: el techo de dato fresco lo fija el sensor (BNO fusión
  100 Hz) y el bus (ToF ~8 Hz). Extrapolar tapa los huecos, no los elimina.
- No reemplaza el fix del freeze del BNO (sacar la lectura I²C de la contención). El
  freeze-bridge es un band-aid acotado por el cap; si está `BNO_FREEZE_DETECT`, el
  detector baja `heading_valid` y degradamos a hold (el detector tiene precedencia).
- **NO validado en hardware.** Regla #1 de CLAUDE.md: solo el equipo cierra una TASK
  de hardware. Claude programó host-testeable y gateado; el banco decide si suma.

## 6. Plan de banco (TASK-218 — criterio de cierre)

1. **A/B de la "medialuna" del strafe del arquero** con `top_robot2_pri` vs
   `top_robot2_pri_hpredict`, midiendo **período y amplitud de oscilación** del rumbo.
   Criterio: la predicción NO debe aumentar la oscilación (si la aumenta → revertir o
   bajar `max_extrap_ms`/subir deadband).
2. **Girar el robot 360° a mano** (lento y rápido): el rumbo transmitido debe seguir el
   giro más fresco (menos atraso) **sin overshoot** ni saltos.
3. **Titrar** `max_extrap_ms` (50-80) y `deadband_dps` con la caja negra.
4. **Regresión:** con `top_robot2_pri` (flag OFF) la conducta es idéntica (binario
   byte-idéntico — confirmar tamaño FLASH sin cambio).
5. (Opcional) freeze-bridge: forzar/observar un heading congelado con el gyro vivo y
   ver que el rumbo transmitido sigue avanzando hasta el cap.

## 7. Referencias

- Análisis completo (workflow): este doc lo consolida.
- Skills: `fusion-pose-odometria-landmarks` (predict/correct), `control-embebido-tiempo-real`
  (§5 latencia/predictor, §6 multi-rate), `tiempo-real-determinismo`.
- Código: `src/shared/heading_predict.h` + `test/test_heading_predict/` + gating en
  `sensors_imu`/`main_top`/`snapshot_emitter` + env `top_robot2_pri_hpredict`.
- Hermano: `research/completed/2026-06-17-propuesta-frecuencia-confiable-sensores.md` (el
  fix de TASA; este es el de LATENCIA — complementarios).
