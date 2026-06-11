# 2026-06-11 — Auditoría global "¿TODOS los programas andan en AMBOS robots?" + logging v1.1

> Pedida por Gustavo tras el banco nocturno (M2 de R1 recableado en la reparación):
> *"No sé si ese error es puntual de un archivo o general — tenemos que estar
> seguros que TODOS los programas se comportan correctamente en los dos robots."*
> Corrida con 13 agentes en paralelo (5 lentes + verificación adversarial de bugs).

## 1. VEREDICTO DEL M2 (la pregunta central)

**El error era PUNTUAL del cableado físico, y el fix es estructuralmente GENERAL
— con DOS excepciones encontradas y corregidas hoy.**

- `MOTOR_INVERT` vive en UN lugar (`config_central.h`, rama por-robot) y se aplica
  en UN embudo (`motors_zircon.cpp::apply_pwm_to_motor`). **Heredaron el fix
  automáticamente**: `central_robot1*` (7 envs), `central_robot2*` (6 envs),
  `diag_central_drive_*`, `diag_central_strafe_*`, y `diag_central_arbitro_strafe_*`
  (el programa de DEMO de R1 — verificado que usa el embudo).
- **Excepciones (pines directos, sin embudo) — auditadas una por una:**
  - `diag_central_line_sweep.cpp` ❌ tenía el M2 invertido HARDCODEADO (motor2()
    con INA/INB cruzados) → movía el M2 al revés en el R1 de hoy Y en R2 desde
    siempre. **CORREGIDO** (motor2 = espejo de motor1) con banner: re-validar en
    banco los signos del sketch (DIR_M2/ROT_M2/ESCAPE_DIR_SIGN se calibraron como
    conjunto con el cableado viejo).
  - `diag_central_motors.cpp` ✅ neutro {+1,+1,+1} = correcto de fábrica HOY, pero
    sus comentarios/prints ENSEÑABAN a re-poner el −1 → **corregidos** (eran el
    vector de regresión humana más peligroso).
  - `diag_central_brake.cpp` ✅ inmune (un solo sentido, patrón simétrico).
  - `diag_central_atras_adelante.cpp` ✅ quedó correcto DE REBOTE (estaba escrito
    para el cableado de R2, que ahora es el de ambos).
  - `src/main.cpp` (env `teensy41_legacy`) = robot 2025, otro pinout — NO flashear
    en hardware 2026 (ya estaba claro).
- **El riesgo mayor NO era código sino PAPEL**: ≥8 docs/comentarios ordenaban
  "NO volver a {+1,+1,+1}" o presentaban {+1,-1,+1} como canon vigente — la
  anti-instrucción exacta. Barrida completa hoy (FUENTES-DE-VERDAD fila M2,
  TASK-042, DEMO-PLAN, RUNBOOK, DIAG-CENTRAL-MOTORS.md/-ARBITRO-STRAFE.md,
  REFERENCIAS-POR-ROBOT, platformio.ini, main_central.cpp, memoria de sesión).

**Política que nos salvó y queda ratificada**: toda inversión/config de motor va
por `MOTOR_INVERT`/`config_central.h` y el embudo — NUNCA hardcodeada en sketches.

## 2. Bugs confirmados (verificación adversarial) — temas-a-analizar

- **[P1] Freno de emergencia vs FSM** (`main_central.cpp:261`): frena al PRIMER
  tick de `imminent_exit` (sin el debounce de 150 ms del router GK) y congela la
  FSM → pisa el anti-flapping de la patrulla v3.2/v3.3 y puede dejar al robot
  clavado sobre la línea. Opciones en el reporte (gate por estado de escape /
  freno de un tick / debounce compartido). Tocar SOLO con banco de línea.
- **[P1] Statics de GK_PATROL sin reset al re-entrar** (`strategy.cpp`): re-entrada
  a PATROL retoma sub-fase/contadores viejos. Fix propuesto: promover a `g_patrol_*`
  y resetear en `transition_gk` (decidir: `direction` se conserva, `reacq_dry` se
  resetea con GO nuevo).
- **[P1] Trilateración publica pose confidence=70 con heading MUERTO** (R1 sin
  BNOs): `build_snapshot` no gatea confidence por `heading_valid` → la pose puede
  acotar al arquero con datos basura. Fix: `confidence = valid && heading_valid ? 70 : 0`.
- **[P1·conocido] Soft-resync del imu_fusion elige al BNO congelado** como
  referencia (lo vivimos anoche) — mitigado con `top_robot2_pri`; fix de fondo:
  freeze-detect ANTES de arbitrar deriva.
- **[P2] GK: `imminent` durante WAIT_START saltea el delay de 2 s** (arranque
  sorpresa). **[P2] `otos_is_fresh` da "fresco" en R2 con pose (0,0,0)** (DOWN
  difunde aunque tenga 0 OTOS) — hoy inocuo (drift-cancel con ceros = 0).

## 3. Logging v1.1 — IMPLEMENTADO HOY (pedido del coach)

1. **Agujero EMERGENCY_LINE cerrado**: la caja negra quedaba ciega exactamente
   durante el freno de borde (el `return` salteaba el tick). Nuevo
   `blackbox_tick_emergency()` + columna `emerg` en el CSV.
2. **Metadata de corrida en el volcado** (línea `#`): build timestamp, robot,
   rol, `MOTOR_INVERT`, pisos — sin esto, con el hardware cambiando cada noche,
   los CSV eran in-interpretables a posteriori.
3. **`tools/blackbox/analizar_corrida.py`** (NUEVO): reporte automático en consola
   (timeline de estados, % pelota/arco/heading, transiciones) + **detectores**
   (pelota teletransportada = falso naranja, flapping de estados, PWM sin comando,
   heading congelado girando, frenos de borde, snapshot stale) + PNG de 4 paneles
   (estados / pelota XY / cmd vs PWM / heading). Probado con CSV sintético:
   cazó las anomalías plantadas. Robusto a BOM/cp1252 de Windows.
4. **`top_robot2_pri_debug_telemetry`** (env nuevo): el único env de telemetría
   TOP extendía `top_robot1` (cableado obsoleto) → ninguna TOP real podía usar la
   app monitor-base. Ahora sí.

**Roadmap v2 (del reporte de la auditoría, priorizado — próximos días):**
BbRec v2 con señales de "porqué" (eje de ataque y su fuente cámara/BNO, target del
orbit, cross-track GK, edad del snapshot) · eventos discretos sub-muestra
(transiciones y pulsos de 40-80 ms caen entre muestras de 20 ms) · sincronización
CENTRAL↔TOP por flancos GO/STOP · SD de la Teensy 4.1 (SdFat+RingBuf, archivo por
corrida) para partidos de Incheon · SOP de logging post-partido · lector multi-corrida.

## 4. Estado de los robots al cierre (pre-demo)

| | R1 | R2 |
|---|---|---|
| CENTRAL demo | `diag_central_arbitro_strafe_robot1` ✅ validado end-to-end con app | `central_robot2_demo(_bb)` — delantero v2, corrida bb pendiente |
| TOP | `top_robot2_pri` (recableada arq. R2; sin BNOs hoy) | `top_robot2_pri` recomendado |
| MOTOR_INVERT | {+1,+1,+1} ✅ piso | {+1,+1,+1} ✅ |
| Pendientes HW | ToF derecho (LP pin 11) · VIN Teensy TOP + puente VUSB-VIN · BNOs re-test | — |
