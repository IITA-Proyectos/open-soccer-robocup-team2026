---
title: "Research: alternativas de entorno de simulación para probar el firmware sin robot"
date: 2026-06-11
author: "Claude (Anthropic - Claude Fable 5)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: completed
tipo: journal
---

# 2026-06-11 — Research: entornos de simulación (Gazebo / Webots / MATLAB / propio)

**Qué pidió Gustavo.** Estudiar alternativas para levantar un entorno simulado
donde probar los programas de fútbol. Ideal: las 3 placas con delays, comms y
ruido. De mínima: física + programa de la CENTRAL. Informe de viabilidad a
corto plazo.

**Qué se hizo.** Investigación de 4 familias de alternativas (simulador propio
host-native, HIL con placa real, Webots/rcj-soccersim, Gazebo/MATLAB/CoppeliaSim/Renode)
+ inventario de activos del repo que abaratan cada camino (FSM host-compilable,
encoders de contratos host-native, caja negra como ground truth, monitor-base
como visualizador). Sesión de solo-lectura sobre el firmware: **cero cambios
de código**.

**Conclusión (resumen).** Ruta incremental recomendada:
**(A)** gemelo 2D host-native de la CENTRAL (días, único con payoff plausible
pre-Incheon) → **(B)** HIL: CENTRAL real + fake TOP/DOWN desde PC por USB-UART
(única que prueba ruido de comms REAL; ~1 semana) → **(D)** SIL de 3 placas
lógicas con UARTs virtuales (el "ideal", post-Incheon) y/o **(C)** Webots
adaptando rcj-soccersim (inversión 2027, sinergia con Rescate Simulado/Erebus).
Gazebo (Windows experimental) y MATLAB/Simulink (licencia + divergencia de
implementación) descartados para este equipo y plazo. **Nada de esto es
P0/P1**: a 19 días de Incheon el banco real tiene prioridad absoluta.

**Doc completo (canónico):**
[`research/completed/2026-06-11-alternativas-entorno-simulacion-firmware.md`](../research/completed/2026-06-11-alternativas-entorno-simulacion-firmware.md)
— incluye risk-no-fix/risk-fix/tiempos por alternativa y el plan de prueba
obligatorio (replay de caja negra con criterio de aceptación ≥90% de
coincidencia de estados FSM).

**Decisiones que quedan en manos del equipo** (§6 del informe): autorizar o no
una sesión acotada para A antes de Incheon; salida CSV vs monitor-base;
stock de adaptadores USB-UART para B.

**Cambios en el repo:** este journal + el doc de research + fila nueva
"Simulación" en `docs/FUENTES-DE-VERDAD.md`. Nada más.
