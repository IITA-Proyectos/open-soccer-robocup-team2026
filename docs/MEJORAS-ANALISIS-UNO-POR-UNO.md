---
title: "Análisis uno-por-uno de las mejoras abiertas (quick-wins + las otras)"
proyecto: "IITA Low Battery Messi — RoboCupJunior Soccer Open 2026"
date: 2026-06-06
status: documento de trabajo (snapshot del estado al 2026-06-06, ~24 días para Incheon)
proposito: "Recorrer TODA mejora abierta de las 3 fuentes (auditoría coach 2026-06-05, ESTADO-MADUREZ-FEATURES, gaps de robot-variants) una por una, clasificarla, y decir qué se haría y qué card de banco aplica."
fuentes:
  - research/in-progress/2026-06-05-auditoria-coach-confiabilidad.md
  - docs/ESTADO-MADUREZ-FEATURES.md
  - docs/robot-variants/REFERENCIAS-POR-ROBOT.md
  - docs/robot-variants/ROBOT-DEFINITION-DESIGN.md
metodo: "Cada fila se verificó contra el código real (archivo:línea citado). NO se asumió 'abierto' por estar en la lista: muchos hallazgos ya tienen tag [audit 2026-06-05] aplicado."
---

# Análisis uno-por-uno de las mejoras

> **Cómo leer este doc.** Cada mejora es una fila con: **clasificación**, **qué se haría**, y **card de banco** (placa + env) si la necesita.
> Clasificaciones:
> - **YA-HECHO** = verificado en código que el fix ya está aplicado (casi siempre con tag `[audit 2026-06-05]`). No queda trabajo de implementación.
> - **DESKTOP-IMPLEMENTABLE** = se puede hacer acá hoy: módulo PURO en `src/shared/` + test host, o doc nuevo/edición de doc. NO toca el binario de competencia.
> - **NEEDS-BENCH** = requiere el robot armado (medir, validar, promover un flag). Lleva card de banco.
> - **NEEDS-USER** = requiere una decisión humana (borrar vs mantener código, deploy wire-breaking coordinado, etc.) antes de tocar nada.
>
> **Hallazgo grande del barrido:** la auditoría 2026-06-05 está **casi totalmente cerrada**. De los 39 hallazgos, ~30 ya están implementados (batches 1-3 + sesión de hoy). Lo que queda abierto es esencialmente **banco** (promover flags ya construidos) y **decisiones de usuario**. La salud del código sigue alta (gate host +650/47/0 verde).

---

## A. Fuente: Auditoría coach-confiabilidad 2026-06-05 (39 hallazgos)

### A.1 HIGH

| # | Mejora | Clasificación | Qué se haría / estado verificado | Card de banco (placa · env) |
|---|---|---|---|---|
| H1 | **gk-intercept-int16-overflow** (bug) | YA-HECHO (y el bug era refutado) | El hallazgo se verificó **refuted** (el decode de cámara acota ball_x a ≤1540 mm → no desborda). Aun así se agregó la defensa en profundidad: `clamp_velocity_mm_s()` existe en `pids.h:36` y **se usa** en `strategy.cpp:662,722` (GK PATROL e INTERCEPT) + test en `test_pids/test_main.cpp:233-253`. | — |
| H2 | **arriba-13-1-snapshot-v1-24b** (doc-error) | YA-HECHO | `FIRMWARE-PLACA-ARRIBA.md:767` ahora tiene banner "Layout v3 = 31 bytes" + struct corregido (ball_vx/vy, goal_own, `static_assert==31`) líneas 782-806. `[fix audit 2026-06-05]`. | — |
| H3 | **heading-valid-from-boot-readiness** (reliability) | YA-HECHO | `sensors_imu_get_heading_valid()` devuelve `g_fusion.fused_valid` (`sensors_imu.cpp:379`) y se usa en `main_top.cpp:167` para el bit4 del snapshot. Cierra el agujero del readiness-al-boot. | — |
| H4 | **central-no-hw-watchdog** (reliability) | YA-HECHO (binario gated OFF → validar) | WDOG1 portado a `main_central.cpp:73-95` bajo `-DCENTRAL_ENABLE_WDT` (default OFF). El **código está**; falta **promoverlo** validando en banco. | **CENTRAL · `central_robot1_wdt`** — 30 min sin reset espurio + auto-reset al colgar el loop a propósito. |
| H5 | **bno-freeze-detect-gated-off** (reliability) | NEEDS-BENCH | El detector (`imu_freeze.h`, host-testeado) existe y está correcto; sigue **gated OFF** en competencia (decisión consciente). Acción: validar y promover el flag. | **TOP · `top_robot1_bnofreeze`** — robot quieto 5-10 min sin falso-DEAD (confirmar jitter ≥1 LSB centideg) → luego forzar congelamiento (subir clock o contención BNO+ToF) y confirmar que detecta → si pasa, agregar `-DTOP_ENABLE_BNO_FREEZE_DETECT` a `top_robot1/2`. **Único HIGH realmente abierto.** |

### A.2 MEDIUM

| # | Mejora | Clasificación | Qué se haría / estado verificado | Card de banco (placa · env) |
|---|---|---|---|---|
| M1 | **central-11-snapshot-27b-line-5b-stale** (doc-error) | YA-HECHO | `FIRMWARE-PLACA-CENTRAL.md:794` (31 B v3), `:814` (LineStatusV2 16 B), `:973` (espejo 31 B). `[fix audit 2026-06-05]`. | — |
| M2 | **abajo-otos-wire1-pines-24-25** (doc-error) | YA-HECHO | `FIRMWARE-PLACA-ABAJO.md:300` corregido a "pines default 16/17" + nota "el remap 24/25 es del TOP". `[fix audit 2026-06-05]`. | — |
| M3 | **hcsr04-pulsein-blocks-when-disconnected** (reliability) | DESKTOP-IMPLEMENTABLE (parcial) / NEEDS-BENCH (wiring) | El **módulo puro** `hcsr04_backoff.h` ya existe con tag `[audit 2026-06-05]` y test. Pero **NO está cableado** en `sensors_tof.cpp` (grep: 0 usos en `src/top/`). El wiring son 2-3 líneas Arduino gateadas → no se aplica acá (toca el binario). Acá se puede: robustecer el módulo puro / sumar tests. | **TOP** (cuando se cablee) — confirmar que un HC-SR04 desconectado no degrada el uplink de 100 Hz. |
| M4 | **imu-freeze-no-extra-coverage-single-bno** (reliability) | NEEDS-BENCH | Duplicado funcional de **H5** (mismo flag, misma acción). | **TOP · `top_robot1_bnofreeze`** (ver H5). |
| M5 | **line-ring-dead-1khz-chain** (reliability) | YA-HECHO (gated OFF → validar) | Los pasos 2-6 de `line_ring_tick()` ya están detrás de `#if defined(DOWN_DEBUG_SERIAL) \|\| defined(LINE_RING_PROCESS)` (`line_ring.cpp:105`); `-DDOWN_LEAN_LINE_PIPELINE` los apaga en competencia. Código listo; falta validar. | **DOWN · `down_lean`** — confirmar wire LineStatusV2 byte-idéntico + headroom CPU. |
| M6 | **otos-pose-no-age-on-wire** (reliability) | NEEDS-USER | Pose2D no lleva edad; agregarla es **wire-breaking** (re-flasheo coordinado TOP/CENTRAL/DOWN, riesgoso a 24 días). Opción barata DESKTOP: **documentar** que la frescura de pose se infiere del watchdog de enlace + confidence (no hay age por-mensaje). La nota de doc **no se encontró aún** → si el usuario quiere, se agrega a `MAPA-DE-DATOS.md`/`CONTRATO-DATOS`. | — (decisión usuario: documentar vs deploy coordinado) |
| M7 | **down-no-hw-watchdog** (reliability) | YA-HECHO (gated OFF → validar) | WDOG1 portado a `main_down.cpp:64-80` bajo `-DDOWN_ENABLE_WDT` (default OFF). Falta promover. | **DOWN · `down_wdt`** — boot sin reset espurio + auto-reset al desconectar un OTOS en caliente. |
| M8 | **central-rx-ring-no-extra-buffer** (reliability) | YA-HECHO | `comm_down.cpp:91` aplica `Serial1.addMemoryForRead(g_down_rx_extra, ...)` guardado por plataforma. Cierra la asimetría con el TOP. | — |
| M9 | **central-brake-coast-unconfirmed** (reliability) | NEEDS-BENCH | No es bug demostrado: hay que MEDIR si `motors_brake()` (INA=INB=HIGH) frena de verdad o queda en COAST en el driver del Zircon. Path de seguridad de borde. | **CENTRAL** (banco, sin env especial) — empujar el robot y disparar `imminent_exit`, medir distancia de frenado vs `motors_stop()`; si COAST, probar INA=INB=LOW o contra-PWM. |
| M10 | **gk-intercept-untested-coverage-gap** (test-gap) | DESKTOP-IMPLEMENTABLE (parcial) / YA-HECHO (defensa) | La aritmética crítica YA está blindada con `clamp_velocity_mm_s` (testeado). Lo que falta para cerrar el test-gap *formalmente*: extraer el cálculo de vx del arquero (`vx_intercept + vx_lateral_pid*0.3`) a un **helper puro** y testear signo+saturación con `target_x` grande. Hoy sigue inline en `strategy.cpp:718-722` (no se puede testear sin refactor; strategy.cpp es forbidden). | — (acá: agregar test del helper puro si se decide extraerlo a `src/shared/`) |

### A.3 LOW

| # | Mejora | Clasificación | Qué se haría / estado verificado | Card de banco |
|---|---|---|---|---|
| L1 | **loc-heading-output-no-wrap** (bug, path dormido) | YA-HECHO | `localization.cpp:213-216` normaliza `(bno - offset)` a `(-18000,18000]` antes de castear a int16. | — |
| L2 | **loc-pose-int16-narrowing** (bug, latente) | YA-HECHO | `loc_clamp_i16()` (`localization.cpp:18`) se aplica en los 4 estimadores + promedio final (`:147-156,207-208`). | — |
| L3 | **drive-straight-axis-naming** (doc-error) | YA-HECHO | `drive_straight.h:57` ahora advierte "NAMING INTERNO — NO coincide con kinematics.h" (vx=avance, vy=lateral; el caller rebindea). | — |
| L4 | **line-geometry-degraded-sign-doc** (doc-error) | YA-HECHO | `line_geometry.h:25-36` documenta que `lg_compute()` (n<32) pondera por vector unitario vs `lg_compute_xy()` por posición; signo conservado. | — |
| L5 | **localization-output-comment-axis** (doc-error) | YA-HECHO | Resuelto junto con L1 (`localization.cpp:209-216`); convención de salida firmada documentada. | — |
| L6 | **config-uart-pin-comment-mismatch** (doc-error) | YA-HECHO | `config_central.h:119-122` agrega la nota: "28/29 son pines de Serial7 en el 4.1 (CENTRAL); en el TOP (4.0) son back-pads, no pin físico". | — |
| L7 | **otos-separation-unvalidated-scale** (doc-error) | YA-HECHO | `config_down.h:118-121` corrige: la separación afecta SOLO `slip_estimate`, NO el heading (que es promedio vectorial desde el audit #7). | — |
| L8 | **doc-central-watchdog-misleading** (doc-error) | YA-HECHO | `config_central.h:129` renombrado a "Timeout de datos (snapshot stale → motors_stop) — NO es un watchdog de HW". | — |
| L9 | **arriba-13-1-header-serial-only-no-banner** (doc-error) | YA-HECHO | Es el "por qué" de H2; el banner de `FIRMWARE-PLACA-ARRIBA.md:767` lo cubre. | — |
| L10 | **kicker-test-names-vestigial** (doc-error) | YA-HECHO | Tests renombrados a `commits_push`/`no_push` (`test_strategy_transitions/test_main.cpp:276-309,447-449`); comentarios marcan `kicker_fire` como campo vestigial. | — |
| L11 | **host-harness-skip-comment-stale** (doc-error) | DESKTOP-IMPLEMENTABLE | **ABIERTO.** `scripts/run-host-tests.sh:15-17,43` aún dice que los tests con deps central/top se reportan SKIP, pero la corrida real da SKIP=0. Fix: actualizar el comentario (los que solo usan inline de headers SÍ corren host; SKIP=0 hoy). Cambio de comentario puro. | — |
| L12 | **min-obstacle-no-reading-sentinel-65535** (reliability) | YA-HECHO | `types.h:120` documenta `0xFFFF = SIN obstáculo (sentinel)` con tag `[audit 2026-06-05]`. (Queda como verificación aguas-abajo en central si algún consumidor lo usa crudo — bajo riesgo). | — |
| L13 | **camera-back-rx-ring-no-extra-buffer** (reliability) | YA-HECHO | `cameras_runtime.cpp:106-107` aplica `addMemoryForRead` a Serial3/Serial5. | — |
| L14 | **gk-intercept-no-line-avoid-priority-inside-state** (reliability) | NEEDS-BENCH | No es bug de strategy; depende de que `motors_brake()` frene (= M9). Opcional sin riesgo: bajar `GK_CLEAR_SPEED_MM_S` si el banco muestra COAST. | **CENTRAL** — junto con M9 (medición de frenado a 500 mm/s). |
| L15 | **atk-kickoff-edge-consumed-on-line-or-stop** (reliability) | NEEDS-USER | Caso de borde (línea inminente justo en el pitido); el KICKOFF es boost de 250 ms. La red de caracterización ya lo testea como "line_avoid_beats_kickoff" (semi-intencional). Decisión del coach si vale tocar el orden de transiciones de `strategy.cpp` (forbidden). | — |
| L16 | **lsv2-sample-age-unused** (reliability) | DESKTOP-IMPLEMENTABLE (parcial) — helper hecho, falta cablear | **Helpers puros YA construidos**: `lsv2_sample_is_stale()` y `lsv2_line_usable()` en `line_view.h:88,113`. Pero **NO se consumen** en `world_model.cpp`/`strategy.cpp` (grep: 0 lecturas de `sample_age_ms` en central). El wiring vive en código forbidden (world_model/strategy). Acá: asegurar tests del helper; el cableado lo decide/aplica el equipo. | — |
| L17 | **lsv2-schema-reject-no-counter** (reliability) | YA-HECHO | `comm_down.cpp:22,60,134` lleva `g_line_schema_rejects` y se imprime en el debug de CENTRAL (`main_central.cpp:271`), espejando CC-01. | — |
| L18 | **central-loop-no-loop-time-supervisor** (reliability) | YA-HECHO (gated OFF) | `loop_monitor.h` (puro, tag `[audit 2026-06-05]`) cableado en `main_central.cpp:175,285-288` bajo `-DCENTRAL_ENABLE_LOOP_MONITOR` (default OFF, binario idéntico). | — (telemetría de pit; activar en banco si se quiere observabilidad) |
| L19 | **motion-target-dead-code** (test-gap) | NEEDS-USER | `motion_target.{h,cpp}` sigue presente; `mt_compute()` sin callers (banner en `.h:2`). Decisión: **borrar** (`motion_target.*` + `test_central_motion`) o **blindar** la convención angular de escape con un test. No se borra sin OK del usuario. | — |
| L20 | **down-calib-lazyinit-test-gap** (test-gap) | DESKTOP-IMPLEMENTABLE | Extraer a funciones PURAS host-testeables: (1) la precedencia EEPROM-vs-derivación de calib (mini-FSM), (2) la decisión de rama de `otos_reset()` (re-detect vs resetTracking) sobre `OtosHealth`. Tests nuevos en `test_down_calib`/`test_otos_health`. Cero cambio en binario. | — |
| L21 | **worldsnapshot-no-golden-roundtrip-test** (test-gap) | YA-HECHO | `test_central_contract/test_main.cpp` tiene `test_snapshot_roundtrip`. | — |
| L22 | **snapshot-v3-no-byte-roundtrip-test** (test-gap) | YA-HECHO | Cubierto por `test_snapshot_roundtrip` (= L21, duplicado). | — |
| L23 | **default-omega-clamp-327-no-regression-guard** (test-gap) | YA-HECHO | `test_pids/test_main.cpp:335`: `TEST_ASSERT_TRUE(pid.output_clamp <= 327.0f)`. | — |
| L24 | **imu-freeze-held-ms-wrap-untested** (test-gap) | YA-HECHO | `test_imu_freeze/test_main.cpp:205` `test_millis_wrap_held_ms_safe` cubre el cruce 0xFFFFFFFF→0. | — |

---

## B. Fuente: ESTADO-MADUREZ-FEATURES.md (N0..N3 + deuda H)

> Estos NO son bugs: son **capacidad construida que espera banco / pose absoluta / decisión**. Todos están code-complete + host-testeados salvo los N0/N1.

### B.1 N0 — Ideas (sin análisis profundo)

| Ítem | Clasificación | Qué se haría | Card de banco |
|---|---|---|---|
| **Mapa de velocidad posición+dirección** | NEEDS-USER (+ depende de pose absoluta + mejores motores) | Estudio en profundidad; depende de pose absoluta válida (hoy inerte) y de motores mejores (hoy cap 70%). Roadmap, no acción inmediata. | — (futuro) |
| **Visión por YOLO (NN en NPU N6)** | NEEDS-USER (roadmap) | Dataset + entrenar/cuantizar + desplegar en NPU + LAB como fallback. Declarado en `USO-DE-IA.md §4.7`. | — (futuro, banco posterior) |

### B.2 N1 — Analizado (no programado)

| Ítem | Clasificación | Qué se haría | Card de banco |
|---|---|---|---|
| **Recalibración de visión (TASK-022)** | NEEDS-BENCH | **Bloqueante #1.** Herramientas listas; ejecutar calibración LAB + homografía con la luz real de la sede. | **TOP/cámaras** — `calib-lab-n6.py` + `diag_cam_acceptance`; recalibrar LAB por cámara + homografía (4 puntos al suelo). Re-flashear las 2 cámaras + alinear a convención simétrica [-100,100]. |
| **Robot-definition único** | NEEDS-BENCH (compilación Teensy) + DESKTOP (diseño hecho) | Diseño + seed `robot2.h` ya están (aditivos, ROBOT1 byte-idéntico). Falta la migración byte-idéntica con `pio` (crear `robot1.h`/`active_robot.h`, reapuntar configs). No se compila Teensy acá. | **Cualquier placa** (con `pio`) — migración §4 de `ROBOT-DEFINITION-DESIGN.md`: grupo por grupo, comparar binario, gate verde antes de seguir. |
| **2 BNO en 2 buses (ROBOT2)** | NEEDS-BENCH (+ cambio de código) | Cambio en `sensors_imu.cpp` para leer un BNO por bus (Wire/Wire1), ambos en 0x28, gateado per-robot. Requiere HW ROBOT2. | **TOP (ROBOT2)** — ver gap C.2. |
| **ESP-NOW robot-a-robot** | NEEDS-BENCH | Integración firmware COMM (ESP32-C6) + protocolo + validación. HW listo. | **COMM** — banco (firmware upstream RCJ, no en este repo). |

### B.3 N2 — Programado pero NO cableado (10 módulos puros, host-testeados)

> El **desbloqueante común = pose absoluta**: hoy `localization` está cableada pero nunca da `valid` (falta ToF en eje X + medir `TOF_OFFSET_MM`). Conseguir lecturas X confiables desbloquea ~6 módulos de una. Por eso casi todos quedan **NEEDS-BENCH** (la medición es física) o **NEEDS-USER** (decisión de cablear). Acá NO se cablean: el caller vive en `strategy.cpp` (forbidden).

| Módulo | Clasificación | Qué falta para cablear (→N3) | Card de banco |
|---|---|---|---|
| **pose_fusion** | NEEDS-BENCH | Runtime que alimente deltas OTOS + pose ToF válida. Bloqueado por pose absoluta. | **TOP** — conseguir ToF eje X (ver `TOF_OFFSET_MM`). |
| **pose_targeting** | NEEDS-BENCH | Pose absoluta confiable; enchufar en apuntado. | idem pose absoluta. |
| **behind_ball_abs** | NEEDS-BENCH | Pose absoluta; reemplazar `behind_ball` relativo. | idem. |
| **clear_aim** | NEEDS-BENCH | Target de despeje confiable; enchufar en GK CLEAR. | idem. |
| **tof_distance_hold** | NEEDS-USER | Decidir una conducta de fallback (nav sin cámara) que lo invoque. | — (decisión + luego banco). |
| **otos_position** | NEEDS-USER (+ ruteo) | **Rutear OTOS a CENTRAL** (hoy va DOWN→TOP) + caller en estrategia. Cambio de ruteo = decisión. | **DOWN/CENTRAL** — banco tras decidir el ruteo. |
| **pose_filter** | NEEDS-BENCH | Consumidor de pose en runtime (= pose absoluta). | idem pose absoluta. |
| **motion_target** | NEEDS-USER | **Cablear o borrar/blindar** (= L19; código muerto con convención angular ambigua). | — (decisión). |
| **strategy_transitions** | NEEDS-USER | Es espejo de caracterización; decidir si se unifica con la FSM viva. | — (decisión). |
| **hcsr04_backoff** | NEEDS-BENCH (wiring Arduino) | Integración de 2-3 líneas gateadas en `sensors_tof.cpp` + `pio` (= M3). Módulo puro ya listo. | **TOP** — verificar con `pio` + banco que no degrada el uplink. |

### B.4 N3 — Operativo no testeado

**§5.1 Features detrás de flag (gated OFF en competencia)** — todas **NEEDS-BENCH** (promover el flag validando):

| Flag | Clasificación | Card de banco (placa · env) |
|---|---|---|
| `TOP_ENABLE_BNO_FREEZE_DETECT` | NEEDS-BENCH | **TOP · `top_robot1_bnofreeze`** (= H5/M4). |
| `CENTRAL_ENABLE_WDT` | NEEDS-BENCH | **CENTRAL · `central_robot1_wdt`** (= H4). |
| `DOWN_ENABLE_WDT` | NEEDS-BENCH | **DOWN · `down_wdt`** (= M7). |
| `DOWN_LEAN_LINE_PIPELINE` | NEEDS-BENCH | **DOWN · `down_lean`** (= M5). |
| `CENTRAL_ENABLE_MANUAL_START` | NEEDS-USER (NUNCA a competencia) | solo banco; arrancar sin árbitro viola RCJ. |

**§5.2 Cableado con fallback, a TUNEAR en banco** — todas **NEEDS-BENCH**:

| Ítem | Clasificación | Card de banco |
|---|---|---|
| **GK cross-track strafe** | NEEDS-BENCH | **CENTRAL/DOWN** — tunear gains + confirmar eje/signo del strafe con dato OTOS fluyendo. |
| **Drive-straight OTOS** | NEEDS-BENCH | **CENTRAL/DOWN** — tunear `DS_KP_*`; validar OTOS fresco en juego. |
| **Anticipación de pelota (bt_classify)** | NEEDS-BENCH | **CENTRAL** — tunear `lookahead_s`/`max_lead_mm` + factores de amenaza (GK INTERCEPT). |
| **Trilateración / localization** | NEEDS-BENCH | **TOP** — cableado pero inerte: ToF eje X + medir `TOF_OFFSET_MM`. |

**§5.3 Valores de config sin validar (N3 + deuda H)** — todas **NEEDS-BENCH** (medición física):

| Constante | Clasificación | Card de banco (placa · diag) |
|---|---|---|
| `WHEEL_ANGLES_DEG` ✅ calibrado `{330,210,90}` 2026-06-08; resta tuneo fino lateral + sentido | NEEDS-BENCH | **CENTRAL** — `diag_central_strafe` en el robot armado (ya traslada; falta que NO rote). (H) |
| `MOTOR_INVERT` ROBOT2 | NEEDS-BENCH | **CENTRAL (R2)** — `diag_central_motors` en el delantero. (H) |
| brake vs COAST | NEEDS-BENCH | **CENTRAL** — medir frenado (= M9). |
| `TOF_OFFSET_MM` (placeholder 95) | NEEDS-BENCH | **TOP** — medir radio real; alimenta trilateración. (H) |
| `OTOS_SEPARATION_MM` (tentativo 200) | NEEDS-BENCH | **DOWN** — medir separación real (afecta slip). (H) |
| Cap de potencia 70% motores | NEEDS-BENCH (⚠️ seguridad) | **CENTRAL** — verificar que el firmware lo limite; si no, fix gateado. Ver `MEJORAS-PENDIENTES.md` E1. |

### B.5 Deuda H — Hardcode / limpieza

| Deuda | Clasificación | Qué se haría |
|---|---|---|
| **Config por-robot esparcida** | NEEDS-BENCH (con `pio`) | Centralizar en el robot-definition (= robot-def, B.2). |
| **Color arco hardcodeado** | NEEDS-USER / DESKTOP | Derivar del comando "play side" del árbitro (hoy `yellow=opp/blue=own` fijo en `main_top.cpp`). Toca el binario → no acá; documentable. |
| **Calibración de cámara (homografía + LAB)** | NEEDS-USER + NEEDS-BENCH | Decisión de diseño (distancia sí al robot-def; color = baseline + recalibrar en sede) ya tomada en `ROBOT-DEFINITION-DESIGN.md §6`; ejecutar en banco. |
| **Constantes TENTATIVO/placeholder** | NEEDS-BENCH | Reemplazar por valores medidos (= §5.3). |

---

## C. Fuente: gaps de docs/robot-variants/ (ROBOT2)

> El robot-definition está **diseñado + seed `robot2.h`** (aditivo, ROBOT1 byte-idéntico). Los gaps de ROBOT2 son las diferencias confirmadas vs el firmware actual. Casi todos requieren **HW ROBOT2 armado** (NEEDS-BENCH) o un **cambio de `.cpp`** que solo se aplica con `pio` (no acá).

| # | Gap ROBOT2 | Clasificación | Qué se haría | Card de banco |
|---|---|---|---|---|
| C1 | **OTOS — R2 sin OTOS** | DESKTOP (env) + NEEDS-BENCH (validar) | El código YA soporta 0 OTOS (`otos.cpp` usa `NUM_OTOS>=1/>=2`). Falta crear `[env:down_robot2]` con `-DDOWN_NUM_OTOS_CONNECTED=0` (no existe `down_robot2`) + corregir `pinout_robot2.h:51 ROBOT_HAS_OTOS` a 0 por coherencia. El env es editable acá (platformio.ini es **forbidden** → documentar como card). | **DOWN (R2) · `down_robot2`** — confirmar boot estable con NUM_OTOS=0 (fallback exacto: drive_straight/GK paralelo/cross_track). |
| C2 | **IMU — 2 BNO, 2º en Wire1, ambos 0x28** | NEEDS-BENCH (+ cambio de código) | `sensors_imu.cpp` hoy pone los 2 BNO en `Wire` y distingue por 0x28/0x29. Para R2: leer el 2º BNO de `Wire1@0x28`, gateado per-robot (tabla `IMU_BNO_BUS[]`/`IMU_BNO_ADDR[]` del robot-def). MISMATCH real, cambio de `.cpp`. | **TOP (R2)** — `diag_top_bno`/`diag_bno_tof` con los 2 BNO en buses separados; medir `HEADING_SIGN`. |
| C3 | **ToF — modelo distinto + rotados ~90° + uno ~40° FOV** | NEEDS-BENCH (+ cambio de código) | Parametrizar modelo por slot (`TOF_MODEL[]`, libs L5CX/L7CX/L8CX en `lib/`); mover `TOF_MOUNT_ANGLE_DEG` de `pinout_common.h` a per-robot; agregar `TOF_FOV_DEG[]` (ausente hoy). Validar pines `PIN_TOF_XSHUT` R2 (copia sin validar). | **TOP (R2) · `top_robot2`** — `diag_top_tof_quad_live`: confirmar pines XSHUT, mapeo índice→posición, modelo y FOV. |
| C4 | **Motores — pines/dirección posiblemente distintos** | NEEDS-BENCH | Pines ya per-robot; `MOTOR_INVERT` R2 = copia de R1 sin validar (posible `{-1,+1,+1}` si el invertido real es U17 índice 0). Medir y fijar. | **CENTRAL (R2) · `central_robot2`** — `diag_central_motors` en el delantero → fijar pines + invert reales. |
| C5 | **Cámaras — calibración no versionada por robot** | NEEDS-USER (diseño) + NEEDS-BENCH (recalibrar) | Decisión de diseño ya tomada (`ROBOT-DEFINITION-DESIGN.md §6`): homografía al robot-def + generador `.py`; LAB = baseline + recalibrar en sede. Ejecutar. | **cámaras** — recalibrar (= TASK-022, card B.2). |
| C6 | **Puertos comm — iguales en ambos** | YA-HECHO (confirmado común) | La auditoría confirma que no hay `#if ROBOT` en los puertos. Nada que hacer. | — |
| C7 | **DOWN rompe el patrón (flags numéricos, no ROBOT1/2)** | DESKTOP (doc) + NEEDS-BENCH | Unificar criterio creando `down_robot1`/`down_robot2` que internamente seteen `DOWN_NUM_OTOS_CONNECTED`. Toca platformio.ini (forbidden) → documentar como card. | **DOWN** — al crear los envs, validar byte-idéntico R1 con `pio`. |

---

## Cómo usar las cards de banco

Cada card "NEEDS-BENCH" se mapea a un **env de PlatformIO** ya existente (o a crear) y a un **diag**. Referencias operativas: `docs/RUNBOOK-BANCO-INCHEON.md` (cómo validar) y `docs/ESTADO-MADUREZ-FEATURES.md §5` (qué falta para promover cada flag). **Regla dura:** ningún flag se promueve a `*_robot1/2` de competencia sin pasar su card de banco; ningún paso de robot-def entra a main sin que ROBOT1 compile byte-idéntico (`pio`).

Las 4 cards de **promoción de flag** (ya con env dedicado y código listo) son las de mejor relación valor/esfuerzo restante:
- `top_robot1_bnofreeze` → `-DTOP_ENABLE_BNO_FREEZE_DETECT` (único HIGH abierto).
- `central_robot1_wdt` → `-DCENTRAL_ENABLE_WDT`.
- `down_wdt` → `-DDOWN_ENABLE_WDT`.
- `down_lean` → `-DDOWN_LEAN_LINE_PIPELINE`.
