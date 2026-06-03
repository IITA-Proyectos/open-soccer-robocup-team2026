---
title: "Evaluación holística del repo (firmware + docs) — hallazgos, prioridades, riesgos"
date: 2026-06-03
status: final
audiencia: "Gustavo + equipo"
author: "Claude Opus 4.8 (Anthropic), vía Claude Code — eval por 13 subagentes + síntesis"
requested-by: "Gustavo Viollaz (@gviollaz)"
tipo: evaluacion
area: todo
robot: ambos
tags: [evaluacion, auditoria, prioridades, bugs, coherencia, riesgo, incheon]
---

# Evaluación holística — IITA Soccer Open 2026

> **Cómo se hizo.** Auditoría read-only de TODO el repo con 13 subagentes en paralelo
> (1 por dominio: control, sensado, localización, línea, protocolo, los 3 firmwares,
> diags, tests, y 3 grupos de docs). Cada hallazgo viene con tipo, severidad,
> **riesgo de cambiar**, **riesgo de decidir con info errada**, y si es fix de riesgo
> cero. Total: **112 hallazgos** (2 CRÍTICO, 17 ALTO, 31 MEDIO, 62 BAJO).
>
> **Política aplicada (la que pediste):** lo de **riesgo cero lo implementé directo**
> (verificándolo yo contra el código y con gate verde); lo riesgoso quedó **documentado**
> (comentario indicativo en el archivo + esta página) para decidir despiertos. **No** toqué
> supuestos de hardware (pines/orientaciones) ni las contradicciones intencionales de
> FUENTES-DE-VERDAD.

## 0. Lo que YA quedó implementado esta sesión (gate verde, pusheado)

**Merges de agentes a main:** TOP (localización F1a/F1b + `pose_filter` + `pose_fusion` +
`diag_top_comm_down` + 35 tests), CENTRAL (`diag_central_arbitro_strafe` + veredicto de
completitud en `diag_central_rx_all`), DOWN (sin novedades). + fix de compilación (kicker
en el diag nuevo).

**Bugs corregidos (riesgo de regresión nulo — la conducta vieja ERA el bug):**
1. **CRÍTICO — overflow int16 de omega.** `HeadingPID.output_clamp` 360→327 (`pids.h`).
   `cmd.omega_centideg_s = omega*100` es int16; con 360 (=36000) **desbordaba y el robot
   giraba a fondo al revés** al saturar el PID (POSITION/APPROACH/GK CLEAR). Verificado que
   el clamp no se sobreescribe; `test_pids` sigue 18/18.
2. **ALTO — `partner_alive` falso al boot.** Guard `!=0` en `comm_arbiter_partner_is_fresh()`
   (mismo patrón ya parcheado en `comm_down`).
3. **CRÍTICO — `sample_age_ms` siempre ~255.** `line_ring` guardaba la *duración* del tick y
   `comm_central` la usaba como *timestamp* → el campo del contrato quedaba saturado. Ahora
   `g_last_tick_us = micros()` (timestamp real). Consumidor único verificado.

**Correcciones de riesgo cero (comentarios/docs desincronizados con el código YA verificado):**
- **Signo de ángulo "+90°" (riesgo de gol en contra):** `CONTRATO-DATOS-CAMARAS §4.4` y
  `CONTRATO-DATOS-TOP §3.2/§3.4` decían "+90°=izquierda"; el código (`cameras_fusion.cpp:97`)
  y `CONVENCION-EJES` dicen **+90°=DERECHA**. Corregido. (El diagrama de §3.4 ya estaba bien —
  NO lo invertí, lo verifiqué antes.)
- **`Serial7` vs `Serial1`:** docs (FIRMWARE-PLACA-CENTRAL §11, CONTRATO-DATOS-CENTRAL,
  DIAG-CENTRAL-DRIVE, central-board-pack/README) decían que el WorldSnapshot del TOP entra por
  `Serial1`; el código usa **Serial7 (pin 28)**. Corregido (incluido un prerequisito de cableado
  que mandaba a conectar el TOP a los pines 0/1, que son del DOWN).
- **TASK-100** banner: el link DOWN→CENTRAL es **Serial1**, NO Serial2/7-8 (7/8 resuelto).
- 9 fixes menores: `n<32`→`n<SENSOR_COUNT`, env `zircon_robot*`→`central_robot*`, timing calib
  ~500→~320ms, comentarios stale (ball_velocity, sensors_tof, motion_target placeholder,
  COMMAND_TIMEOUT_MS/OTOS_I2C_ADDR sin uso), TASK-012 P0→P1, TASK-033 etiqueta A/B.

Commits: `e405cb3`,`ae9ff7b`,`f9a93f0` (merges) · `376d15d`,`b841d9c`,`2c4ff2b`,`afd77f7` (fixes).

---

## 1. PRIORIDADES para Incheon (síntesis del coach)

Ordenadas por impacto en "que el robot juegue de verdad":

1. **Visión sin calibrar (TASK-022) — bloqueante #1.** Las N6 transmiten pero el color LAB no
   está calibrado → no ve la pelota. Todo el ATAQUE depende de esto. (Kit + procedimiento ya
   en el repo: `docs/firmware/CALIBRACION-VISION-N6.md`.)
2. **Línea sin calibrar (`data_valid=0`) — bloqueante #2.** Sin esto el freno de borde,
   LINE_AVOID y el strafe del arquero quedan inertes (gateados por `data_valid`). Calibrar
   verde+blanco y persistir en EEPROM + validar en banco.
3. **Confirmar en banco el SIGNO del heading/BNO (ALTO, riesgo de realimentación positiva).**
   Si el `my_heading_centideg` del snapshot crece en sentido opuesto al que asume el HeadingPID
   (CCW+), el robot **gira sin parar** en POSITION/APPROACH/CLEAR. NO se puede verificar sin banco.
   → usar `diag_top_bno` + `diag_central_drive`.
4. **Polaridad de M2 + `WHEEL_ANGLES_DEG` (ALTO).** Contradicción abierta: `DIAG-CENTRAL-MOTORS`
   dice `{+1,+1,+1}`, el banco 2026-06-01 dice M2 invertido; `WHEEL_ANGLES_DEG={60,-60,180}` es
   TENTATIVO e **idéntico para ambos robots** pese a que el mapeo motor→índice está rotado entre
   ROBOT1/ROBOT2. Cerrar en banco antes de confiar en cualquier traslación omni.
5. **Cablear la localización fusionada (pose_fusion/pose_filter).** Están implementadas+testeadas
   pero **NO en el lazo** (`localization_runtime` sólo llama `localization_compute`). Hoy la pose
   x/y casi nunca es `valid`. Decidir + cablear (con banco).
6. **Reconciliar la deuda de 2 cadenas en DOWN** (line_ring 1kHz vs down_model 200Hz, calibraciones
   separadas) — post-Incheon, pero pesa en CPU y en riesgo de divergencia de calib.

---

## 2. Hallazgos CRÍTICOS (2) — ambos YA corregidos

| # | Hallazgo | Estado |
|---|---|---|
| C1 | Overflow int16 de omega (giro invertido a fondo al saturar el PID) — `strategy.cpp` vía `pids.h` | ✅ corregido (clamp 327) |
| C2 | `sample_age_ms` siempre ~255 (duración usada como timestamp) — `line_ring.cpp`/`comm_central.cpp` | ✅ corregido (timestamp real) |

## 3. Hallazgos ALTOS (17) — qué corregir / decidir / banco

> Los que NO toqué llevan **riesgo de decidir con info errada** porque dependen de hardware o de
> conducta no verificable con el humano dormido.

**Coherencia de convención (riesgo de "gol en contra" / control invertido):**
- ✅ `CONTRATO-DATOS-TOP §3.2/§3.4` y `CONTRATO-DATOS-CAMARAS §4.4`: "+90°=izquierda" → **DERECHA** (corregido).
- ⚠️ **`behind_ball`/`strategy` mezcla `atan2(x,y)` (horario+) con heading CCW+** (`behind_ball.cpp:57`,
  `strategy.cpp:312/374`). **needs-bench**: si el BNO entrega CCW+, el setpoint `heading+goal_angle`
  mezcla convenciones y el HeadingPID corrige al lado equivocado. *Riesgo de decidir mal: invierte el
  control de rumbo.* NO tocar a ciegas → verificar en banco (es la misma incógnita de la prioridad #3).
- ⚠️ **Convención de signo del heading sin verificar** (`strategy.cpp:313`) — idem #3. **needs-bench.**

**Bugs de firmware (documentados; fix con algún riesgo):**
- ✅ `partner_alive` boot (corregido).
- ⚠️ **`EV_CORNER` usa ángulos uniformes i*360/32** que no son la geometría física real del anillo
  (`down_model.cpp:164`). El ángulo de línea sí usa geometría real; sólo el flag corner se toma del
  cálculo uniforme. **needs-bench** (cuánto degrada). Riesgo: detección de esquina poco fiable.
- ⚠️ **`DOWN_DEBUG_SERIAL`: `cross_track_mm` se calcula DESPUÉS del broadcast** → nunca se transmite
  (`comm_central.cpp:132`). Sólo build de banco; invalida esa herramienta de bring-up del arquero.
  Fix: mover el bloque antes del broadcast (cambia conducta de debug → revisar). *Comentario indicativo
  pendiente de poner.*
- ⚠️ **`platformio.ini` env `diag_central_comm_down`: el comentario dice Serial2/pin7** pero el código
  compilado usa Serial1/pin0 (`platformio.ini:757`). **Trampa de cableado de banco.** (doc-fix de
  riesgo cero — pendiente de aplicar, ver §6.)

**Estructura / dormido:**
- ⚠️ **`pose_fusion`+`pose_filter` implementados+testeados pero SIN cablear** (`pose_fusion.cpp:81`):
  la fusión ToF+OTOS que resolvería "los dos mapas" NO está en ejecución. *Riesgo de decidir con info
  errada:* creer que "la localización ya fusiona OTOS" (es falso). → prioridad #5. **needs-human-decision.**
- ⚠️ **El cerebro real (`strategy.cpp`) no tiene test directo**; `test_strategy_transitions` (35 tests)
  prueba una RÉPLICA (`strategy_transitions.cpp`) que el firmware **no llama**. Riesgo: creer la FSM
  "testeada" cuando lo testeado es una copia que puede divergir. **needs-human-decision** (o cablear la
  réplica / portar strategy a host-testeable).
- ⚠️ **Deuda de 2 cadenas DOWN** (`comm_central.cpp:99`) — prioridad #6. **needs-human-decision.**
- ⚠️ **`WHEEL_ANGLES_DEG` idéntico ROBOT1/ROBOT2 con mapeo motor→índice rotado** — prioridad #4. **needs-bench.**
- ⚠️ **`drive_straight` nombra vx/vy al revés que `kinematics`** (`drive_straight.h:9`): hoy los call-sites
  cruzan los ejes a mano (sin bug), pero es una mina para el próximo. *Comentario indicativo pendiente.*
  **needs-human-decision** (renombrar campos = toca firma+tests).
- ⚠️ **Colisión de ID TASK-204** (dos archivos distintos son "TASK-204"). **needs-human-decision.**
- ⚠️ **TASK-100** describe Serial2 + conflicto 7/8 → ✅ banner agregado.

## 4. Temas a CORREGIR (MEDIO, doc/coherencia — mayormente decisión humana)

- `build_snapshot`: marcar explícitamente qué campos son REALES vs PLACEHOLDER (ball x/y/dist en
  unidades sin calibrar, `my_pose_confidence` hardcodeado, pose nunca válida con el HW actual).
- **LINE_AVOID en `strategy` es inalcanzable**: `main_central` hace `return` antes con la MISMA
  condición (EMERGENCY_LINE). Decidir si el estado FSM LINE_AVOID debe existir o se elimina.
- **Ejes de cancha invertidos** en `FIRMWARE-PLACA-ARRIBA §6.4` (X=1820/Y=2430) vs firmware y
  CONVENCION-EJES (X=2430/Y=1820). Doc-fix (verificar antes).
- `central-board-pack/05-arquitectura-3-placas.md` quedó PRE-fix 2026-06-02 (COMM=Serial4, árbitro por
  UART) → contradice el firmware vivo. Doc-fix grande.
- **Packs de cámara + PACKS-INDEX dicen "OpenMV H7/H7 Plus"** — el robot vigente usa **N6**. Doc-fix.
- `diag_central_recv1/send1` y `diag_central_comm_down`: comentarios con Serial2/pin7 (reasignado). Doc-fix.
- Skills (`SKILL.md`, `striker-strategy.md`, `communication-module-integration.md`) rotuladas
  "ACTUAL 2026" pero describen el robot **2025 con kicker / ESP-NOW primario** — agregar nota de "base
  2025, no es el diseño vigente" (sin homogeneizar; revisar alcance).
- Tests dormidos a documentar: `test_central_motion` (motion_target), `test_central_trajectory`
  (bt_classify) — caracterizan módulos sin caller. (Comentarios indicativos pendientes.)

## 5. Temas a SEGUIR DESARROLLANDO (keep-developing)

- **Sin evasión de obstáculos**: `min_obstacle_mm` se publica pero `strategy` nunca lo consulta.
- **Cablear `bt_classify`** (clasificación de trayectoria de la pelota): la velocidad ya llega y el
  arquero ya anticipa por `ball_predict`, pero `bt_classify` (dejar-pasar/interceptar/desviar) no se llama.
- **Anti-windup por back-calculation** en los PID (hoy clamp simple; la I del HeadingPID es casi inerte:
  autoridad ~2.5°/s vs salida 327°/s).
- **`test_proto` no cubre byte-stuffing / colisión START/END** en el payload.
- **Exponer `resync_events()`** en la telemetría de los `comm_*` (hoy `g_frames_lost` mezcla pérdida de
  cable con descartes del decoder → el "lost enorme con crc=0" de la memoria).
- Portar `strategy.cpp` a una forma host-testeable (o cablear `strategy_transitions` como única fuente).

## 6. Fixes de riesgo cero PENDIENTES (doc-text, los dejo listados para aplicar)

Verificados contra código pero no alcancé a aplicar todos (todos son texto, sin riesgo):
- `platformio.ini:757` env `diag_central_comm_down`: comentario Serial2/pin7 → Serial1/pin0.
- `FIRMWARE-PLACA-ARRIBA.md:90`: fila BNO "Wire+Wire1 24/25" → ambos en Wire (18/19).
- `FIRMWARE-PLACA-ABAJO.md:291`: OTOS Wire1 "24/25" → SDA17/SCL16.
- `ARQUITECTURA-3-PLACAS-2026.md`: `status: propuesta`→`vigente`; "24 B"→"27 B" (líneas 407/432).
- `ESTADO-ACTUAL.md`: consolidar el conteo de tests vivo (389/32) al tope de la sección.
- `team-tasks/README.md:43`: TASK-006 `pending`→`done`.
- `TASK-035`: banner "prereq 4 ToF cumplido (banco 2026-05-30) → desbloqueada".
- Comentarios indicativos: `drive_straight.h` (trampa de naming) ✅ puesto en .cpp;
  `comm_down.cpp` (g_frames_lost mide huecos de SEQ); `test_central_motion/_trajectory` (dormidos).

---

## Apéndice — método y trazabilidad

- 13 dominios, ~1.8M tokens de análisis. Conteo: 112 hallazgos (CRÍTICO 2, ALTO 17, MEDIO 31, BAJO 62).
- Por recomendación: implement-now 10, comment-in-place 35, needs-human-decision 30, needs-bench 17,
  keep-developing 20.
- Esta página resume; el detalle por archivo:línea de cada hallazgo está en los resultados del workflow
  de eval (no versionado — pedir regenerar si hace falta el dump completo).
- **Regla mantenida:** Claude no cierra TASKs de hardware; los cambios de pin/orientación NO se tocaron
  sin banco; FUENTES-DE-VERDAD intacto en sus contradicciones intencionales.
