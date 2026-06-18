---
id: TASK-218
title: "Validar en banco la PREDICCIÓN DE RUMBO (predict step) del TOP — A/B medialuna + titrar cap/deadband"
date_created: 2026-06-17
assigned: [gustavo-viollaz, virginia-viollaz, elias]
priority: P1
status: pending
estimated_hours: 2
blocks: ["promover TOP_ENABLE_HEADING_PREDICT a produccion (si el A/B da bien)"]
blocked_by: [placa TOP + BNO sano + CENTRAL recibiendo snapshot + cancha para el strafe del arquero]
tags: [top-board, banco, heading, prediccion, predict-step, extrapolacion, gateado, latencia]
related:
  - research/completed/2026-06-17-extrapolacion-predict-step.md
  - src/shared/heading_predict.h
  - journal/2026-06-17-prediccion-rumbo-predict-step-cableada.md
  - docs/firmware/CONTROL-ARQUERO-LATERAL-Y-LATENCIAS.md
---

# TASK-218 — Banco: predicción de rumbo (predict step) del TOP

## Por qué existe

Pedido de Gustavo (2026-06-17): en vez de transmitir a la CENTRAL el último heading
"quieto" (zero-order hold), transmitir `heading_crudo + ω·Δt` (extrapolación lineal con
la ω MEDIDA del gyro). Cableado GATEADO `-DTOP_ENABLE_HEADING_PREDICT` (default OFF →
competencia byte-idéntica). Módulo PURO `src/shared/heading_predict.h`, host-tested
(`test_heading_predict` 14/14). **La regla 1 de CLAUDE.md exige banco: Claude NO cierra
esto** — solo programó y testeó host.

## Qué flashear

| Placa | Env | Para qué |
|---|---|---|
| TOP | `top_robot2_pri` | A (control: heading quieto, lo de hoy) |
| TOP | `top_robot2_pri_hpredict` | B (con predicción) |
| CENTRAL | `central_robot2_arquero_bb` (o el de banco con caja negra) | medir la conducta |

```
pio run -e top_robot2_pri_hpredict -t upload     # B
pio run -e top_robot2_pri -t upload              # A (revertir)
```

## Criterio de cierre (medible)

1. **A/B de la "medialuna" del strafe del arquero** (la oscilación de rumbo que la
   latencia provoca). Con la caja negra, medir **período y amplitud de la oscilación**
   en A vs B, mismo guion. **CRITERIO: B no debe AUMENTAR la oscilación.** Si la sube →
   bajar `max_extrap_ms` (60→40) o subir `deadband_dps` (2→4) y repetir; si igual sube,
   NO promover (revertir a A).
2. **Girar el robot 360° a mano** (lento y rápido) mirando el heading que reporta el
   monitor: con B debe seguir el giro **más fresco** (menos atraso) **sin overshoot** ni
   saltos. CRITERIO: sin overshoot visible.
3. **Titrar** `max_extrap_ms` y `deadband_dps` (constantes en `heading_predict.h`
   `heading_predict_default_cfg()`) con los datos.
4. **Regresión:** con `top_robot2_pri` (flag OFF) la conducta es IDÉNTICA a antes
   (binario byte-idéntico — FLASH data 102584 sin cambio, ya verificado al compilar).
5. **(Opcional) freeze-bridge:** si se puede forzar/observar un heading congelado con el
   gyro vivo, ver que el rumbo transmitido sigue avanzando hasta el cap (no se clava).

## Notas / límites

- La predicción ataca la LATENCIA, no crea info nueva: no reemplaza subir la tasa de
  sensado ni el fix del freeze del BNO (es aditiva).
- El heading CRUDO queda intacto para `imu_freeze` y `localization`; solo cambia el
  heading TRANSMITIDO. Si `BNO_FREEZE_DETECT` está activo y detecta congelado, baja
  `heading_valid` → la predicción degrada a hold (el detector tiene precedencia).
- La predicción se hace en el TOP (compensa el staleness intra-TOP + freeze-bridge). La
  versión consumidor-side (CENTRAL, que conoce la edad real del dato y compensa también
  el transporte) requiere subir ω al snapshot (cambio de contrato) → follow-up post-Incheon.
- NO se tocó la pelota (ya usa `ball_predict`; extrapolar su entrada = doble-conteo con
  el D del PID) ni señales de seguridad (línea/obstáculo nunca se extrapolan).

## Escape

Cualquier mal comportamiento en banco/cancha → flashear `top_robot2_pri` (sin el flag).
