---
name: control-pid-zona-muerta
description: Use when a PID/feedback loop oscillates violently, loses to a disturbance, or the actuator can't do small corrections (deadzone, PWM floors, bang-bang, all-or-nothing motors). Symptoms - robot spins on its axis, fishtails/serpentea, correction overshoots and re-corrects opposite, raising or lowering gains both fail, "pierde la pulseada" against parasitic drift. Covers deadzone compensation, PFM/duty-cycling, PI as automatic feedforward, anti-windup, bench tuning titration.
---

# Control PID con actuadores de zona muerta (control industrial aplicado al robot)

## Principio central

Cuando el actuador **no puede hacer "poquito"** (zona muerta + pisos de PWM), un PID
clásico falla por DOS modos opuestos — y subir o bajar ganancias solo cambia de modo:

| Modo de falla | Síntoma | Por qué |
|---|---|---|
| **Autoridad insuficiente** | El error crece monótono ("pierde la pulseada") | La corrección clampeada < perturbación sistemática (ej: tope 40°/s vs deriva parásita 80°/s) |
| **Sobre-corrección cuantizada** | Oscilación violenta ±50-140°, trompos | Cualquier salida que supera el piso dispara el MÍNIMO FÍSICO del actuador (≫ lo pedido) → overshoot → corrección opuesta → ciclo límite |

**Banco 2026-06-12 (robot 2, strafe 200 mm/s):** clamp 40°/s → runaway 0→-150° en 3 s;
clamp 120°/s kp=3 → oscilación ±140° y trompo. AMBOS extremos confirmados en datos.

## Las 4 herramientas (en orden de aplicación)

1. **PFM / duty-cycling de la corrección** (la clave con actuador todo-o-nada):
   no pidas "girar a 15°/s" (imposible); pedí "girar al mínimo físico, el 20% del
   tiempo". Se promedia en el tiempo → corrección fina EFECTIVA con actuador grosero.
   Período corto (100-200 ms) para que el promedio sea suave a escala del robot.
   `duty = clamp(u_deseado / OMEGA_FISICO_ON, -1, +1)` → aplicar omega ON los
   primeros `|duty|·T` ms de cada ventana T, OFF el resto.
2. **Zona muerta DEL ERROR (deadband)**: si `|err| < umbral` (3-8°), salida CERO.
   Evita que el lazo persiga ruido y tiemble permanentemente alrededor del setpoint.
3. **PI con anti-windup = feedforward automático**: el integrador aprende solo la
   perturbación SISTEMÁTICA (deriva parásita del strafe) — esto ES la
   "auto-calibración" pedida. Límite duro del integrador (anti-windup) al rango que
   puede cancelar (~±perturbación máxima conocida); congelar integración mientras la
   salida está saturada o el duty está OFF.
4. **Red de seguridad por estado**: si `|err| > umbral_grande` (45°), el lazo está
   perdido → PARAR el movimiento, re-escuadrar quieto (donde el actuador es más
   confiable), asentar, retomar. Convierte el descontrol en recuperación.

## Método de ajuste de banco (titración, 2-3 corridas)

1. Arrancar: kp = el validado del robot (HeadingPID 3.0), ki=0, deadband 5°,
   OMEGA_ON = mínimo físico medido, T=150 ms.
2. ¿Serpentea? → subir deadband (5→8°) o bajar kp (3→2). ¿Deriva lenta sin
   corregir? → activar ki chico (0.5/s) con anti-windup.
3. Medir SIEMPRE con datos (caja negra / panel), nunca a ojo: criterio = |err| < 10°
   sostenido durante el movimiento completo.

## Errores comunes

- **Subir el clamp para "ganarle" a la oscilación** → la empeora (más overshoot).
- **D-term contra actuador cuantizado** → amplifica el ruido de cuantización; PFM primero.
- **Integrador sin anti-windup** → tras una saturación larga, descarga un latigazo.
- **Tunear sin identificar la planta**: medí PRIMERO la perturbación (deriva con lazo
  abierto) y el mínimo físico del actuador — esos 2 números dimensionan todo.

## Valores YA validados del arquero (NO re-tunear a ciegas)

Antes de tocar el lazo de rumbo del arquero, leé los docs canónicos — esto ya está medido:

- **Heading-hold por trim de la TRASERA** (strafe continuo, `strategy.cpp`): `GK_STRAFE_KP=2.0`,
  `KI=1.0`, `deadband=2°`, `band=18°`, `trim_max=30 PWM`, `i_max=18`; `GK_REAR_TRIM_SIGN=-1` (R2,
  confirmado banco 2026-06-18 — OJO: un signo invertido TAMBIÉN queda estable, pero a 180° del
  objetivo; se discrimina con captura de rumbo inicial). **Trim traslacional: tope ±19 mm/s** (arriba
  la trasera se dispara a su piso 107 = patada lateral).
- **PFM clásico** (correcciones por debajo del piso): `kp=2.0, ki=0.4, deadband 5°, 100°/s, ventana 160 ms`.
- **La latencia del heading YA está al máximo de mitigación (verificado en código 2026-06-18):** el BNO
  se lee a **100 Hz** en `top_robot2_pri` (`-DTOP_BNO_FAST` + `-DTOP_BNO_PRIMARY_ONLY` horneados desde
  2026-06-16 → `BNO_READ_INTERVAL_MS=10`), y `top_robot2_pri_hpredict` suma predicción (extrapola con el
  gyro) ENCIMA. `top_robot2_pri_fastbno` es REDUNDANTE con el base. ⚠️ El doc CONTROL-ARQUERO-LATERAL
  (2026-06-14) todavía dice "BNO 20 Hz / fix = fastbno" — **DESACTUALIZADO**, el código ya lo hornea.
  Si AÚN oscila con 100 Hz+predicción, NO es latencia: bajar `GK_STRAFE_KI` (1.0→0.5) y/o esperar el
  warm-up del BNO (la deriva inicial del giro es calibración del sensor, no rate).
- **Docs canónicos:** lazo + latencias → `docs/firmware/CONTROL-ARQUERO-LATERAL-Y-LATENCIAS.md` (ojo:
  su afirmación de "BNO 20 Hz" quedó vieja); todas las perillas con rango → `docs/firmware/GUIA-DE-TUNING-CENTRAL.md`.

**REQUIRED BACKGROUND:** la planta concreta de este robot está en la skill
`dinamica-omni-3-ruedas` (pisos, deriva parásita medida, regímenes de velocidad).
