---
title: "Ruta de latencia: diagnóstico del banco del domingo + lazo RT de línea (env down_rt para R1) + plan"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: env RT R1 creado (compila); validación banco = TASK-111; ruta priorizada propuesta
tipo: journal
---

# Resumen

Gustavo reportó el banco del domingo: el hardware se comportó bien, pero los robots no
jugaban correctamente — oscilaban (por latencia de lazo, ya mejorada), faltaba calibración
de potencias de rueda, y las líneas blancas del borde tardaban en detectarse. Pidió loop
principal rápido y datos rápidos a CENTRAL. Se diagnosticó contra el código y se creó el
env RT que faltaba para R1.

# Diagnóstico (síntoma → causa verificada en código → acción)

| Síntoma (domingo) | Causa (código, verificada) | Acción |
|---|---|---|
| Oscilación por latencia | TOP ya emite snapshot @100 Hz por ISR (snapshot_emitter); DOWN @200 Hz. CENTRAL drena por tick. Residuo: `central_robot1` aún tiene `-DCENTRAL_DEBUG_SERIAL` (~30 prints/500 ms = jitter); R2 ya lo quitó. | Quick-win: quitar DEBUG_SERIAL de R1 (propuesto) + medir loop_monitor en banco |
| Línea tarda en detectarse | El binario de partido de DOWN (`down`/`down_robot2`) NO trae las mejoras RT: barrido lento ~717 µs; en R1 el spike I²C del OTOS (3-4 ms) bloquea la lectura de luz (platformio.ini:1781) | Crear `down_rt` para R1 (HECHO) + validar (TASK-111) |
| Calibración de potencias de rueda | `MOTOR_MIN_PWM[3]` + eff por rueda; R1 "a verificar" (config_central.h) | Banco puro (titración) — el monitor nuevo (panel cancha + Hz + caja negra) ayuda |
| Datos rápidos a CENTRAL | TOP @100 Hz ISR ✓ · DOWN @200 Hz ✓ · cuello = velocidad del loop de CENTRAL | Quitar jitter (DEBUG_SERIAL) + medir loop_us |

# Qué se hizo

- **A2 aplicado (quick-win latencia): QUITADO `-DCENTRAL_DEBUG_SERIAL` de `central_robot1`**
  (R2 ya lo había hecho el 2026-06-16). El bloque de ~30 `Serial.print` cada 500 ms en el loop
  de motores se fue → cero pico de jitter periódico por USB CDC. Ataca la "oscilación por
  latencia". La salud por USB sigue por el monitor DORMIDO (`-DCENTRAL_USB_MONITOR` en los *_bb).
  ⚠️ BANCO R1: confirmar `loop_us(max)` más parejo. `central_robot1` compila SUCCESS.
- **Env `down_rt` (NUEVO) para R1**: paridad con `down_robot2_rt` (R2) PERO **+ `DOWN_OTOS_FAST_I2C`**
  porque R1 tiene OTOS y el spike del OTOS era el cuello de la lectura de luz. Trae: ADC_FAST +
  ADC_DUAL (barrido 717→126 µs) + OTOS_FAST_I2C (spike 3-4 ms → <0,6 ms) + EARLY_EVIDENCE (F3,
  aviso de línea más temprano) + RELIABLE_GATE + RX_HARDEN. + `down_rt_bench` (con loop-monitor/debug
  para medir). **NO toca el binario de partido** (`down` sigue byte-idéntico) → validar y promover (TASK-111).

# Por qué las mejoras ya existían gateadas

El repo tiene una familia de envs RT de DOWN incremental por capa (`down_adcfast`, `down_adcdual`,
`down_otosfast`, `down_earlyev`, `down_reliable`, `down_rxharden`, `down_rt_all`) + el consolidado
de R2 (`down_robot2_rt`). Faltaba el consolidado de R1. La arquitectura es correcta: las mejoras RT
no validadas viven en envs aparte hasta probarse en banco. Solo se completó la paridad.

# Ruta priorizada propuesta (en qué avanzar)

**Fase A — Latencia (atacar los síntomas del domingo; bajo riesgo, ya existe gateado):**
- A1 (P0): validar `down_rt` (R1) y `down_robot2_rt` (R2) en banco → detección de línea rápida (TASK-111).
- A2 (P0, propuesto): quitar `-DCENTRAL_DEBUG_SERIAL` de `central_robot1` (R2 ya lo hizo) → menos jitter.
- A3 (P0): medir `loop_us(max/ema)` con el monitor en ambos robots → confirmar loop rápido y sin spikes.

**Fase B — Calibración (banco puro, lo cierra el equipo):**
- B1 (P0): titrar potencias de rueda (`MOTOR_MIN_PWM` + eff) por robot, usando el panel de cancha +
  Hz + caja negra del monitor.
- B2 (P1): re-tunear el PID de rumbo (ganancias) ahora que la latencia bajó → matar la oscilación.

**Fase C — Completar funcionalidades (prepararlas para validar):**
- C1 (P1): pose XY — validar `*_posefusion` (TASK-110/210/211) → el robot se ubica en cancha.
- C2 (P1): watchdog — validar `*_wdt` y promover (TASK-110) → robustez en partido.
- C3 (P2): cross-validación de heading (`*_xval`) — robustez del rumbo.

**Fase D — Consolidar pruebas (dejar operativo):**
- D1: plan de scrimmage sistemático (un robot por rol, partidos cortos, caja negra).
- D2: regresión: cada env promovido a competencia se re-valida que no rompe lo anterior.

# Sugerencia (la mía como coach)

Empezar por **Fase A** (latencia) porque ataca directo lo que viste el domingo y es casi todo
"activar lo que ya existe + medir", no código nuevo. Con la línea detectándose rápido y el loop
sin jitter, **Fase B** (calibrar potencias + PID) va a ser mucho más fácil y los robots van a
dejar de oscilar. Recién con eso estable, **Fase C** (pose/watchdog) y **Fase D** (consolidar).
El orden importa: calibrar potencias ANTES de bajar la latencia te haría calibrar contra un
blanco móvil.

# Archivos

- `platformio.ini`: +`down_rt` +`down_rt_bench` (R1 RT, paridad con R2).
- `team-tasks/2026-06-17-task-111-...`: checklist de banco del lazo RT.
- `journal/2026-06-17-ruta-latencia-y-lazo-rt.md` (este).

# Pendiente equipo (banco)

TASK-111 (validar lazo RT ambos robots) + Fase B (calibración) son banco. A2 (quitar DEBUG_SERIAL
R1) es decisión de Gustavo (quick-win, R2 ya lo hizo).
