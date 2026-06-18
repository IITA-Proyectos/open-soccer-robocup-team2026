---
id: TASK-219
title: "Banco: modo CONTINUO de los ToF (A/B vs autonomo) — medir Hz/cara, zonas, jitter del loop"
date_created: 2026-06-18
assigned: [gustavo-viollaz, virginia-viollaz, elias]
priority: P2
status: pending
estimated_hours: 1.5
blocks: ["promover TOP_ENABLE_TOF_CONTINUOUS a produccion si el A/B da mejor"]
blocked_by: [placa TOP con los 4 ToF + monitor del TOP]
tags: [top-board, banco, tof, i2c, jitter, desactivado-por-defecto]
related:
  - src/top/sensors_tof.cpp
  - research/completed/2026-06-17-propuesta-frecuencia-confiable-sensores.md
---

# TASK-219 — Banco: modo continuo de los ToF

## Por qué existe

Hoy los VL53L7CX corren en modo AUTONOMO (el default de la librería). Según el manual
ST (UM3038), el modo CONTINUO entrega un bloque de resultados más chico por lectura I²C
→ cada `getRangingData()` ocupa menos el bus `Wire` → menos jitter del loop del TOP. El
portón de "leer solo si hay dato" (`isDataReady()`) ya estaba; esto suma el modo.

Se programó **desactivado por defecto**: se activa al compilar con `-DTOP_ENABLE_TOF_CONTINUOUS`
(entorno `top_robot2_pri_tofcont`). Con la bandera apagada (competencia, `top_robot2_pri`) el
modo es el autónomo de hoy = **sin cambio**. **La regla 1 manda: Claude no cierra esto, lo
valida el equipo.**

## Qué grabar

```
pio run -e top_robot2_pri_tofcont -t upload   # B (modo continuo)
pio run -e top_robot2_pri -t upload           # A (autonomo, revertir)
```

## Criterio de cierre (medible)

1. **Hz por cara**: medir la tasa real de refresco de cada ToF en A vs B (con el monitor
   del TOP). Objetivo: que B suba de ~8 Hz/cara hacia ~10-12 sin perder lecturas.
2. **Integridad de las 16 zonas**: con un objeto a distancia conocida (ej. 500 mm) en una
   cara, las 16 zonas deben dar ~500 mm estable, sin frames partidos ni saltos espurios.
3. **Jitter del loop del TOP**: el período de loop (panel `loop=`) NO debe empeorar; ideal
   que el peor caso baje.
4. **Regresión**: con `top_robot2_pri` (autónomo) todo igual que hoy.

Si B no mejora o rompe las zonas → quedarse en autónomo (`top_robot2_pri`).

## Nota

Es una mejora modesta de consistencia (no es el cuello de tasa, que es el bus I²C
compartido — eso se ataca separando buses, post-Incheon). Bajo riesgo: 1 archivo, 2 líneas,
reversible por bandera.
