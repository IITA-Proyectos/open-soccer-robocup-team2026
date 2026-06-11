---
title: "Backlog priorizado para Incheon 2026 — consolidado y verificado"
date: 2026-06-11
author: "Claude (coach) — auditoría multi-agente 2026-06-11 (48 agentes, 122 hallazgos, 35 confirmados adversarialmente, 0 refutados) + inventario de team-tasks/journals/MEJORAS"
status: vivo — PARA DISCUTIR PRIORIDADES CON GUSTAVO
tipo: backlog
---

# Backlog Incheon 2026 (al 2026-06-11, ~19 días para el viaje)

> **Cómo leerlo.** Cada ítem: qué es · tipo (`admin` / `banco` = solo hardware /
> `código` = patch + banco / `docs`) · tiempo honesto · fuente. Deduplicado: cada
> tema aparece UNA vez aunque lo hayan señalado varias auditorías. Los ítems de
> `código` NO se tocan hasta después de la práctica 2026-06-12 y cada uno lleva su
> plan de banco. **P0** = sin esto no se compite o se desclasifica · **P1** =
> impacto alto en partidos · **P2** = deseable / capitaliza a 2027.
>
> **La foto en una frase:** el firmware está sólido (gate 59 suites / 810 tests /
> 0 fallos; DOWN madura, fail-safes principales puestos); lo que bloquea Incheon
> ya NO es código nuevo — son los ENTREGABLES de jueces, la visión en sede, y un
> puñado de bugs P1 conocidos cuyo fix es chico.

---

## P0 — Bloqueantes (7)

| # | Qué | Tipo | Tiempo | Fuente |
|---|---|---|---|---|
| P0-1 | **TASK-041: averiguar el deadline REAL del form TDP/póster/video.** Nadie lo sabe; en RCJ suele vencer SEMANAS antes del torneo. Si cierra ~15-20 jun, todo lo de abajo se re-ordena. **Manda sobre todo.** | admin (Gustavo) | 1 h | TASK-041 |
| P0-2 | **TASK-022: visión en sede** — deploy coordinado cámaras+TOP (wire-breaking, ensayar el procedimiento <5 min ANTES de viajar) + recalibración LAB bajo la luz de Incheon (kit listo). Bloqueante técnico #1 declarado. | banco | 6 h | ESTADO-ACTUAL |
| P0-3 | **Video técnico <3 min: NO grabado.** Guion listo (VIDEO-GUION.md). Subtítulos EN quemados + URL repo + tests en verde. | equipo | ½ día | MEJORAS-PENDIENTES |
| P0-4 | **Póster A1: sin maquetar** + faltan las FOTOS reales (robot, anillo, equipo, banco) + 2 QR. "El cap más duro de la rúbrica". | equipo | 8 h | MEJORAS-PENDIENTES |
| P0-5 | **BOM: costos y specs faltantes** (Zircon, motor 2026, rueda omni, dimensiones vs reglamento — A8 además verifica LEGALIDAD, batería, horas). Una tarde con facturas + balanza + calibre. | equipo | 4 h | MEJORAS-PENDIENTES |
| P0-6 | **Cap térmico de motores SIN aplicar en el camino del delantero.** Los motores 5V a 7,4 V se queman sobre ~70% duty; el techo (~150 PWM) solo existe en la rama FLOOR_SCALE (arquero) — TODOS los envs de delantero (incluido competencia) salen sin límite, y PUSH a 700 mm/s pone la rueda dominante en ~155 sostenido. El módulo puro `motor_power_cap` existe testeado, es la CARD CENTRAL-8: cablearlo en el embudo `apply_pwm_to_motor`. | código | 3 h | auditoría shared (confirmado) + MEJORAS |
| P0-7 | **Práctica 2026-06-12** — valida TODO el firmware nuevo (delantero OTOS, arquero integral, cámara pegajosa, signo del yaw). Los CSV re-priorizan este backlog. | banco | mañana | guiones listos |

## P1 — Impacto alto en partidos

### (a) Decisiones que solo Gustavo puede tomar (rápidas, destraban trabajo)

| # | Qué | Decisión pedida |
|---|---|---|
| D1 | **Bug B1 (el P1 técnico más serio, confirmado de nuevo):** el freno de borde de `main_central` eclipsa TODO el router de línea de la FSM → `LINE_AVOID` del delantero y el anti-flapping del arquero son código muerto, y como `imminent` es por NIVEL (≥6 sensores), un robot detenido sobre la línea queda **CLAVADO frenando para siempre** (arquero congelado = arco libre). 3 opciones ya analizadas en journal 2026-06-10; recomiendo la (b): tras ~400 ms de freno continuo, dejar pasar UN tick de strategy para que ejecute el escape. | Elegir opción (a)/(b)/(c) → patch 4 h + banco |
| D2 | **Env de PARTIDO del arquero R1 no existe:** `central_robot1` a secas corre el clamp viejo sin FLOOR_SCALE (re-crea los síntomas del 2026-06-09) y el comentario de `_slow` manda justo ahí. Crear `central_robot1_match` (= + FLOOR_SCALE, sin MANUAL_START) tras completar TASK-042. | OK para crearlo post-banco R1 |
| D3 | **PR #18 (traducciones EN) abierto desde el 06-10.** Mergear o cerrar. | 15 min |
| D4 | **Cámara pegajosa:** si mañana pasa los 5 criterios → promover `TOP_CAM_STICKY` a default de ambas TOP. | sí/no post-práctica |
| D5 | **Polaridad de arco (¿de qué lado pateamos?):** hay señales contradictorias — TASK-024 (P0, pending) ordena llamar `strategy_set_attack_color()` según el sorteo ("sin esto, autogol el 50% de los partidos"), pero el inventario la marca posiblemente-resuelta por el latch de `goal_polarity` en el TOP. **Hay que VERIFICAR la cadena completa en banco** (poner el robot mirando cada arco y ver a cuál ataca) antes de Incheon. | agendar banco 1 h |

### (b) Bugs de firmware confirmados — parches chicos, post-práctica, cada uno con banco

| # | Qué | Tiempo |
|---|---|---|
| F1 | **Statics de GK_PATROL sin reset entre corridas:** tras cada gol (STOP→GO) la patrulla puede re-arrancar en sub-fase REACQ (retrocediendo al arco) o con el guard `reacq_dry` agotado. Extraer a struct + reset en la transición. | 2,5 h |
| F2 | **Perillas de banco compiladas en competencia:** `GK_START_DELAY_MS=2000` (arquero inmóvil 2 s tras CADA GO del árbitro = ventana de gol) y `GK_PATROL_SPEED=200`. Convertir a `#ifndef` con el valor de banco solo en envs `*_demo`/`*_practica`. | 1 h |
| F3 | **Trilateración publica conf=70 con heading inválido** — y el arquero la consume para sus límites (`GK_POSE_CONF_MIN=40`). En R1 sin gyro = pose con rumbo basura. Fix de 1 línea (`pose.valid && heading_valid`). | 1 h |
| F4 | **El único BNO vivo (envs `_pri`, AMBOS robots) no tiene detector de muerte:** si muere/congela a mitad de partido, el snapshot sigue mandando `heading_valid=1` con rumbo muerto para siempre. Re-tunear el detector existente (`imu_freeze`, hoy OFF por falsos-DEAD) al patrón real: heading Y gyro bit-clavados simultáneos. | 4 h |
| F5 | **Árbitro de drift de `imu_fusion` elige al BNO congelado como referencia** (el incidente real del 2026-06-11: arrastró al primario sano). Es LO que bloquea volver al dual-BNO. Vetar como referencia a un sensor con heading bit-idéntico N ciclos; módulo puro + test que reproduce el incidente. | 5 h |
| F6 | **Falsos naranjas secuestran al arquero con 1 solo tick de `ball_visible`** (PATROL→INTERCEPT sin debounce). Exigir N ticks consecutivos + (tras la pegajosa) confianza >60. Los CSV de mañana dimensionan N. | 2 h |
| F7 | **Calibrar línea con la app monitor-base deja el `DownModel` vivo con la calib VIEJA hasta reiniciar** (la app actualiza una mitad del sistema). Invalidar y re-derivar al final de cada comando CAL_*. | 1,5 h |
| F8 | **Un OTOS que NACKea 3 ticks queda muerto el RESTO del partido** (política latch sin ningún re-arme cableado; nadie emite RESET_OTOS). En R1 sin gyro, los OTOS son TODO el rumbo. Re-probe periódico gateado. | 4 h |
| F9 | **Cadena de cámaras (3 fixes juntos, con TASK-022):** (1) `main.py` SIN try/except — una excepción transitoria mata la cámara hasta power-cycle, y el robot juega medio ciego sin que nadie lo sepa; (2) falta el modo competencia (exposición/WB/gain FIJOS) que el doc de calibración asume — "la causa #1 de anda en el lab y no en la cancha"; (3) `pixels_threshold=7` para pelota = ruido (checklist pide ≥20, validar a distancia máxima). | 4 h |
| F10 | **El gate puede mentir:** `run-host-tests.sh` reporta SKIP si una suite no compila y sale VERDE igual; y NADA compila los envs Arduino automáticamente. Fix: fallar con SKIP>0 + `scripts/build-firmwares.sh` con la lista canónica de envs. | 2 h |

### (c) Bancos de validación pendientes (código listo, falta correrlo)

| # | Qué | Tiempo |
|---|---|---|
| V1 | **`motors_brake()`: ¿frena de verdad o queda en COAST?** TODO el freno de borde confía en esto y nunca se midió (cámara lenta del celular alcanza). El experimento de mayor valor/costo de la CENTRAL. | 1 h |
| V2 | **Promoción de los 4 flags de confiabilidad construidos y OFF:** WDT CENTRAL, WDT DOWN, DOWN lean, freeze-detect (post-F4). Cards ya escritas (DOWN-1..4 etc.); si pasan → a envs de competencia. | ½ día |
| V3 | **TASK-042 completar R1:** FLOOR_SCALE en su env, fixes del TOP heredados (Δloop), patrulla v3.3 en el cuerpo R1. R1 es el segundo cuerpo del 2v2. | 3 h |
| V4 | **Checklist de cierre de patrulla (7 puntos) + re-apretar pulsos GK** (35→20°, settle 700→400 — el heading volvió a 100 Hz). La práctica de mañana cubre parte. | 2 h |
| V5 | **Detección de robot LEVANTADO jamás validada** (en RCJ te levantan muchas veces por partido). Card DOWN-11 nueva: 5 levantadas, EV_LIFTED en <300 ms. | 1 h |
| V6 | **Alimentación batería→TOP de R1:** encender SOLO a batería y confirmar que la TOP bootea (la corrección del 06-11 vs el "misterio batería-mata-todo" sigue sin explicación registrada). | 10 min (en la práctica) |

### (d) Hardware R1 (Enzo)

| # | Qué | Tiempo |
|---|---|---|
| H1 | ToF derecho de R1 no enumera (cable LP pin 11): continuidad + resoldar + power-cycle total. | 1 h |
| H2 | Re-test de los 2 BNOs de R1 en bus propio (el "muerto por golpe" fue falso diagnóstico → posible repuesto gratis) + test del giro primario-solo. | 2 h |

### (e) Competencia / proceso

| # | Qué | Tiempo |
|---|---|---|
| C1 | Ensayos de entrevista EN ×3 (Virginia+Elías+Enzo) + corrector EN a los 6 deliverables. | 4 h |
| C2 | CAD/STL del chasis subir (bonus CAD en riesgo). | 2 h |
| C3 | TASK-013: BOM de la placa TOP (alimenta el BOM de jueces). | 2 h |
| C4 | **Pasada de cierre de team-tasks:** ~12 TASKs siguen "pending" estando resueltas en banco (008, 012, 024?, 031, 033, 036-038, 100, 202, 204…) — inflan el backlog aparente y ya causaron trabajo duplicado antes. 1 línea de evidencia cada una. | 1 h |
| C5 | Cuarentena de envs obsoletos en platformio.ini (banners DEPRECADO sobre `top_robot1*` etc.) — los comentarios que hoy MIENTEN (`bnofreeze` dice lo contrario de lo que compila). | 1 h |

## P2 — Deseable / capitaliza a 2027 (resumen, 1 línea c/u)

**Confiabilidad:** SD logging de la caja negra para PARTIDOS (sin USB en cancha; ⚠️ candidato a subir a P1 — la estrategia declarada del equipo ES capturar aprendizaje) · consumir `sample_age_ms` en CENTRAL (línea vieja con link vivo) · detector de OTOS congelado-que-ACKea (el modo de falla del BNO, ahora en DOWN) · SEQ por tipo de frame (mata el artefacto `lost` gigante) · `TOF_STALE_TIMEOUT` 250→350 ms (round-robin lo dejó justo) · deconflict BNO↔ToF también en `top_robot2` · IMU_ZERO re-cera localization · GOTO_LINE exige línea fresca antes de retroceder 1,7 m ciego · blackbox_dump: motors_stop + feed del WDT.

**Limpieza de código (mejora legibilidad sin perder fail-safes):** borrar el arquero v1/Capa 3 muerto de strategy.cpp (~70 líneas: `gk_lateral_pid_output`, `GkState::LINE_AVOID` sin entrada — coordinar con D1 y con TASK-024/D5 antes de tocar `attack_color`) · gatear `imu_zircon.cpp` (compila Adafruit BNO en todos los binarios de una placa sin BNO) · `motion_target` placeholder auto-condenado · senders sin caller (marcar "superficie 2027") · consolidar las 8 copias del wrap de ángulo en `angle_util.h` (de a poco, NO big-bang) · `test/codigo_arquero/a.cpp` vacío · espejo `strategy_transitions` sin PUSH/PUSH_BACK (la suite verde caracteriza una FSM que ya no es la del robot).

**Docs:** poda de ESTADO-ACTUAL (698 líneas ≠ "1 página"; mover historial de mayo) · banners SPEC-vs-real en FIRMWARE-PLACA-ARRIBA/CENTRAL (describen EKF/Kalman que no existen) · mover los 3 docs de telemetría al `docs/firmware/` canónico · HANDOFF-NUEVA-SESION sin números embebidos · banners de cierre en MEJORAS-ANALISIS (C1/C2/C4 ya hechos) · tabla MAPA-DE-DATOS §4 (links ya validados) · docs/README.md de marzo · CHECKLIST-CALIBRACION con rama por-robot (R1 sin gyro) · FLAGS-CENTRAL.md (tabla de combos legales/prohibidos de los 14 flags).

**Tools:** monitor-base no avisa que arranca en SIMULADOR (título) · leer_caja_negra cuelgues/conteo · kit de calibración corre en QVGA con mínimos distintos a producción (el feedback de px no predice al robot).

**2027 / post-Incheon:** packet cámara v3 con confianza por blob (mata el falso del mismo lado; wire-breaking) · `fuse_goal_dual` promedia arcos (mismo bug que la pelota, mitigado por blobs de 600 px) · pose absoluta (TASK-035: ToF eje X + medir TOF_OFFSET) que despierta 6 módulos dormidos · ESP-NOW partner mínimo (1 byte "tengo la pelota") · tabla de módulos dormidos en ESTADO-ACTUAL · re-etiquetar TASKs de PCB (001/002/005/009) como pre-refabricación · research pipeline grooming · gate host 10× más rápido (compilar shared una vez).

---

## Hecho esta noche (2026-06-11, post-auditoría — solo docs y tools, cero firmware)

✅ Detector "heading congelado" del analizador de caja negra: umbral 3000→30 (era INALCANZABLE — el detector del modo de falla #1 nunca podía disparar) · ✅ tutorial de build apuntaba al clon señuelo `futbol2026` · ✅ `default_envs` apuntaba al cableado viejo de R1 · ✅ banner de recableado en TOP.md (cards IMU) · ✅ ESTADO-ACTUAL: bullet kickstart "pendiente" que ya estaba hecho (053fd0a) · ✅ moratoria de CLAUDE.md cerrada (condición cumplida hace semanas) · ✅ AI-INSTRUCTIONS: cámaras H7→N6 · ✅ tabla DOWN.md (envs "falta crear" que existen) · ✅ este backlog + `QUE-FLASHEO-HOY.md`.
