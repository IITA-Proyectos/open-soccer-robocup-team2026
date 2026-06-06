---
title: "Estado de madurez de features — checklist de lo NO 100% operativo+testeado"
proyecto: "IITA Low Battery Messi — RoboCupJunior Soccer Open 2026"
date: 2026-06-05
status: documento VIVO (se actualiza al graduar cada ítem)
proposito: "Tener claro qué está pensado pero no operativo, en qué nivel de madurez, y qué falta para subirlo — para ir limpiando hacia diseños prolijos, sin hardcode y todo testeado."
---

# Estado de madurez de features (checklist de limpieza)

> **Para qué sirve.** Es la lista única de **todo lo que NO está 100% operativo y testeado**. Cada ítem tiene un **nivel de madurez** y **qué falta para subirlo**. La meta es ir moviéndolos hacia **N4 (operativo+testeado)** y, en paralelo, **eliminar el hardcode** (que toda config viva en el *robot definition*).
>
> Lo que ya está en **N4 no aparece acá** (no es problema). Este doc es la cola de trabajo, no el inventario completo.

## 0. Terminología (importante, para no confundirnos)

- **Cableado / *wired*** = está **integrado al path de ejecución vivo** (lo llama `strategy.cpp` / un `main_*.cpp` / el binario de competencia). Lo opuesto: *code-complete pero no cableado* = el módulo existe y está testeado en host, pero **nadie lo invoca** todavía.
- **Hardcodeado** = el valor está **fijo en el código**, esparcido, sin centralizar. Lo opuesto: parametrizado en el **robot-definition** (un único archivo por robot). → ver `docs/robot-variants/`.
- Son **dos ejes distintos**: algo puede estar cableado pero hardcodeado, o no cableado y limpio. El ideal es **N4 + sin hardcode**.

## 1. Escala de madurez

| Nivel | Nombre | Definición |
|---|---|---|
| **N0** | Idea / concepto | Anotado como oportunidad; **no analizado en profundidad**. |
| **N1** | Analizado | Análisis/diseño completo; **no programado** (o solo herramientas/scaffold). |
| **N2** | Programado | **Code-complete** (a veces host-testeado), **pero NO cableado / no operativo** en el robot. |
| **N3** | Operativo no testeado | **Cableado y corre** (o detrás de flag), **pero sin validar** en banco/competencia. |
| **N4** | Operativo + testeado | 100% andando y verificado. **No entra en este doc.** |

> Eje extra: **(H)** marca **deuda de hardcode** (valor a mover al robot-definition), independientemente del nivel.

---

## 2. N0 — Ideas / conceptos (sin análisis profundo)

| Ítem | Qué es | Qué falta para subir a N1 |
|---|---|---|
| **Mapa de velocidad posición+dirección** | Bajar velocidad cerca del borde **solo si se va hacia el borde** (100% si se aleja). Otros equipos ya lo hacen. | Estudio en profundidad + **depende de pose absoluta** (N2 abajo) + mejores motores (hoy limita el cap 70%). Ver `MEJORAS-PENDIENTES.md` (e). |
| **Visión por YOLO (NN en NPU N6)** | Reemplazar umbrales LAB por red neuronal on-device (robusta a iluminación). | Dataset etiquetado + entrenar/cuantizar + desplegar en NPU + mantener LAB como fallback. Roadmap declarado (`USO-DE-IA.md` §4.7). |

---

## 3. N1 — Analizado (diseñado, no programado / solo scaffold)

| Ítem | Estado | Qué falta para subir a N2 | Bloqueante |
|---|---|---|---|
| **Recalibración de visión (TASK-022)** | Herramientas listas (`calib-lab-n6.py`, `diag_cam_acceptance`, `CALIBRACION-VISION-N6.md`); el parsing/wire está N4. | Ejecutar la calibración LAB + homografía en la luz real. | **Banco (bloqueante #1).** El robot no ve la pelota hasta hacerlo. |
| **Robot-definition único** | Diseño + seed `robot_config/robot2.h` (aditivo, en curso vía workflow). | Cablear los config existentes para que tiren del robot-def, byte-idéntico, con `pio`. | Compilación Teensy (no se puede acá) + revisión. (H) |
| **2 BNO en 2 buses (ROBOT2)** | Diseño claro: 2 BNO misma dir base (0x28) en Wire + Wire1. | Cambio en `sensors_imu.cpp` para leer un BNO por bus, gateado per-robot. | Banco (hardware ROBOT2) + `pio`. |
| **ESP-NOW robot-a-robot** | Hardware listo (ESP32-C6); roadmap declarado. | Integración firmware COMM + protocolo + validación. | Banco. |

---

## 4. N2 — Programado pero NO cableado (code-complete, host-testeado, nadie lo llama)

> Todos existen como módulo PURO con su test host, pero **`strategy.cpp`/`main_*` NO los incluyen**. La mayoría espera **pose absoluta válida** (ver nota al pie).

| Módulo | Archivos | Test host | Qué falta para cablear (→N3) |
|---|---|---|---|
| **pose_fusion** | `src/shared/pose_fusion.{h,cpp}` | `test/test_pose_fusion` | Runtime que alimente deltas OTOS + pose ToF válida; publicar pose fusionada. Bloqueado por pose absoluta. |
| **pose_targeting** | `src/shared/pose_targeting.{h,cpp}` | `test/test_pose_targeting` | Pose absoluta confiable; enchufar en estrategia de apuntado. |
| **behind_ball_abs** | `src/shared/behind_ball_abs.{h,cpp}` | `test/test_behind_ball_abs` | Pose absoluta; reemplazar el `behind_ball` relativo. |
| **clear_aim** | `src/shared/clear_aim.{h,cpp}` | `test/test_clear_aim` | Target de despeje confiable; enchufar en GK CLEAR. |
| **tof_distance_hold** | `src/shared/tof_distance_hold.{h,cpp}` | `test/test_tof_distance_hold` | Conducta de fallback (nav sin cámara) que lo invoque. |
| **otos_position** | `src/shared/otos_position.{h,cpp}` | `test/test_otos_position` | **Rutear OTOS a CENTRAL** (hoy va DOWN→TOP) + caller en estrategia. |
| **pose_filter** | `src/shared/pose_filter.{h,cpp}` | `test/test_pose_filter` | Consumidor de pose en runtime. |
| **motion_target** | `src/shared/motion_target.{h,cpp}` | `test/test_central_motion` | **Decisión needs-user**: cablear, o borrar/blindar (código muerto con convención angular ambigua). |
| **strategy_transitions** | `src/shared/strategy_transitions.{h,cpp}` | `test/test_strategy_transitions` | Es espejo de caracterización; `strategy.cpp` no lo usa. Decidir si se unifica con la FSM viva. |
| **hcsr04_backoff** | `src/shared/hcsr04_backoff.h` (+ test) | `test/test_hcsr04_backoff` | Integración Arduino (2-3 líneas gateadas) en `sensors_tof.cpp` + verificar con `pio`. |

> **Nota — el desbloqueante común = pose absoluta.** Hoy la trilateración (`localization`) está cableada pero **nunca da `valid`** (necesita ToF en eje X + `TOF_OFFSET_MM`); por eso toda la familia de pose absoluta queda inerte. Conseguir lecturas X confiables desbloquea ~6 módulos de una.

---

## 5. N3 — Operativo NO testeado (cableado/flag, sin validar en banco)

### 5.1 Features detrás de flag (gated-OFF en competencia)
| Flag | Env de banco | Qué hace | Qué falta para promover a competencia |
|---|---|---|---|
| `TOP_ENABLE_BNO_FREEZE_DETECT` | `top_robot1_bnofreeze` | Detecta BNO congelado → baja `heading_valid`. **Único HIGH del audit.** | Quieto 5-10 min sin falso-DEAD + forzar congelamiento; tunear N/T. |
| `CENTRAL_ENABLE_WDT` | `central_robot1_wdt` | Watchdog HW (WDOG1) en la placa master de motores. | 30 min sin reset espurio + auto-reset al colgar. |
| `DOWN_ENABLE_WDT` | `down_wdt` | Watchdog HW en DOWN (cuelgue de I²C OTOS). | Boot sin reset + auto-reset al desconectar OTOS. |
| `DOWN_LEAN_LINE_PIPELINE` | `down_lean` | Apaga el pipeline de línea muerto (solo diag) → libera CPU. | Confirmar wire idéntico + headroom CPU. |
| `CENTRAL_ENABLE_MANUAL_START` | (solo banco) | START manual por botón/USB. | **NUNCA va a competencia** (arrancar sin árbitro viola RCJ). |

### 5.2 Cableado con fallback pero a TUNEAR en banco
| Ítem | Archivos | Qué falta |
|---|---|---|
| **GK cross-track strafe** | `strategy.cpp` (`gk_lateral_pid_output`) | Tunear gains + confirmar eje/signo del strafe con dato OTOS fluyendo. |
| **Drive-straight OTOS** | `strategy.cpp` + `drive_straight` | Tunear `DS_KP_*`; validar que el dato OTOS llega fresco en juego. |
| **Anticipación de pelota (bt_classify)** | `strategy.cpp` (GK INTERCEPT) + `ball_predict`/`ball_trajectory` | Tunear `lookahead_s`/`max_lead_mm` + factores de amenaza. |
| **Trilateración / localization** | `src/shared/localization.*` + `src/top/localization_runtime.cpp` | Cableado pero **inerte** (nunca `valid`): conseguir ToF eje X + medir `TOF_OFFSET_MM`. |

### 5.3 Valores de config sin validar (N3 + deuda H)
| Constante | Archivo | Estado | Qué falta |
|---|---|---|---|
| `WHEEL_ANGLES_DEG` | `config_central.h` | TENTATIVO (da círculos) | Medir en el robot armado + `diag_central_strafe/drive`. (H) |
| `MOTOR_INVERT` ROBOT2 | `config_central.h` | Copia de ROBOT1 sin validar; pines/dirección posiblemente distintos | `diag_central_motors` en el delantero. (H) |
| brake vs COAST | `motors_zircon.cpp` (`motors_brake`) | Sin confirmar en el Zircon | Medir si frena o queda en COAST (path de freno de borde). |
| `TOF_OFFSET_MM` | `pinout_common.h` | Placeholder (95) | Medir radio real; alimenta trilateración. (H) |
| `OTOS_SEPARATION_MM` | `config_down.h` | TENTATIVO (200) | Medir separación real (afecta slip dual). (H) |
| Cap de potencia 70% motores | (a ubicar) | **Sin confirmar** que el firmware lo limite | ⚠️ Seguridad: verificar; si no, fix gateado. Ver `MEJORAS-PENDIENTES.md` E1. |

---

## 6. Deuda de HARDCODE / limpieza (eje H — diseños prolijos sin hardcode)

| Deuda | Dónde | Plan |
|---|---|---|
| **Config por-robot esparcida** | `config_central.h`, `config_down.h`, configs TOP, `pinout_common.h`, sensores | Centralizar TODO en el **robot-definition** único (en curso). Ver `docs/robot-variants/ROBOT-DEFINITION-DESIGN.md`. |
| **Color arco hardcodeado** | `main_top.cpp` (`yellow=opp/blue=own`) | Derivar del comando "play side" del árbitro (no fijo). |
| **Calibración de cámara** (homografía + LAB) | scripts `.py` de cámaras + `cameras_runtime.cpp` (placeholders) | Mover la calib de DISTANCIA (homografía) al robot-def; decidir si la de COLOR (LAB) también (es más per-venue). Ver `ROBOT-DEFINITION-DESIGN.md`. |
| **Constantes TENTATIVO/placeholder** | varias (ver §5.3) | Reemplazar por valores medidos en banco, ya parametrizados en el robot-def. |

---

## 7. Resumen / cómo usar este doc

- **Prioridad de limpieza sugerida:** (1) cap de seguridad 70% (E1, seguridad) → (2) pose absoluta (desbloquea ~6 módulos N2) → (3) promover flags N3 validados en banco → (4) centralizar hardcode en robot-def → (5) tunear lo cableado-con-fallback.
- **Al graduar un ítem:** subilo de nivel acá (o sacalo si llega a N4) y marcá el commit. Mantené este doc como la **única cola de "lo no terminado"**.
- **Conteo actual (aprox):** N0 = 2 · N1 = 4 · N2 = 10 · N3 = ~15 (flags + tune + config) · deuda-H = 4 grupos. La **salud del código es alta** (gate +650/47/0 en verde); casi todo lo de arriba es *capacidad construida que espera banco/pose/decisión*, no bugs.

> Relacionados: `docs/competencia/MEJORAS-PENDIENTES.md` (deliverables + roadmap (e)), `docs/RUNBOOK-BANCO-INCHEON.md` (cómo validar en banco), `docs/robot-variants/` (auditoría por-robot + diseño del robot-def), `docs/competencia/USO-DE-IA.md` (metodología).
