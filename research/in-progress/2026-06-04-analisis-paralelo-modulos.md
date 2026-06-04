---
title: "Análisis paralelo multi-agente del firmware (20 subsistemas) — backlog priorizado"
date: 2026-06-04
status: vivo
tipo: evaluacion
author: "Claude Opus 4.8 — análisis paralelo multi-agente"
---

# Backlog de mejoras del firmware — análisis paralelo de 20 subsistemas

> **Qué es esto.** Cada subsistema del firmware de las 3 placas (TOP / CENTRAL /
> DOWN) + visión + módulos puros compartidos fue auditado por un ingeniero
> independiente. Los hallazgos marcados HIGH pasaron por un revisor escéptico que
> los confirmó o degradó. Este documento consolida todo en un **backlog
> priorizado** y descarta los falsos positivos.
>
> **A 26 días de Incheon.** El criterio rector es: *no abrir frentes nuevos sobre
> código que ya compite.* Los quick-wins seguros están separados de la deuda
> post-competencia.

---

## Resumen ejecutivo (para el coach)

- **Salud general: SÓLIDA.** De 20 subsistemas, 15 están en estado `solid`, 4 con
  `minor-issues` y 0 en estado crítico. El firmware está bien arquitecturado: la
  lógica de decisión vive en módulos PUROS host-testeados y el glue Arduino es
  delgado. No hay ningún bug identificado que rompa un partido por sí solo en el
  camino vivo de competencia.
- **Conteo de hallazgos tras deduplicar y descartar falsos positivos: 2 HIGH, 9
  MEDIUM, ~40 LOW (robustez/claridad), ~22 simplificaciones.** (Detalle abajo.)
- **Los 3 temas más importantes (todos requieren BANCO, no escritorio):**
  1. **El freno de emergencia de borde (`EMERGENCY_LINE` → `motors_brake()`) no
     está confirmado que frene en el hardware del Zircon** — podría ser COAST.
     Afecta CENTRAL y la primitiva de actuación. (degradado de high→medium por el
     revisor.)
  2. **Un BNO que muere a mitad de partido sigue reportándose `present=true`**
     para siempre: heading congelado/erróneo sin failover. Confirmado HIGH por el
     revisor. Hoy el robot corre con 1 solo BNO sano → sin redundancia, este es el
     único sensor de heading.
  3. **Visión sin recalibrar (LAB + homografía, TASK-022) sigue siendo el
     bloqueante real #1.** El código de visión está sólido; falta trabajo de
     banco. El UART de la cámara frontal aún está sin confirmar (degradado a low,
     pero es verificación de banco trivial).
- **Bug latente de eje (no afecta el robot de hoy):** dos módulos NO cableados
  (`pose_fusion`) y varios configs de test tienen `field_width`/`field_height`
  invertidos respecto a la convención vigente (W=1820, H=2430). Arreglo seguro
  porque el módulo no corre, pero conviene corregirlo antes de cablear la fusión.
- **Hay pocos quick-wins de escritorio de alto valor.** El firmware ya está
  bien simplificado. Lo accionable seguro pre-Incheon es mayormente
  *observabilidad barata* (contadores de diagnóstico), *guardas defensivas que no
  cambian valores* (NaN, clamps, static_assert) y *correcciones de comentarios
  stale*.
- **Veredicto sobre tocar código a 26 días:** **SÍ, pero quirúrgicamente.** Los
  ítems marcados "Seguro pre-Incheon = sí" son aditivos o solo-comentario y no
  tocan el camino de control vivo. Los 3 temas grandes son de BANCO y NO deben
  tocarse a ciegas desde el escritorio. La mayor relación valor/riesgo HOY sigue
  siendo **desbloquear las TASK de banco abiertas (visión TASK-022, freno, BNO
  failover)**, no refactorizar.

---

## Metodología

Análisis paralelo de **20 subsistemas** del firmware, cada uno auditado por un
ingeniero independiente con mirada propia (bugs, robustez, claridad,
simplificación, alternativas de diseño). Los hallazgos de severidad **HIGH**
fueron sometidos a **verificación adversarial** por un segundo revisor escéptico
que los confirmó, degradó o marcó como falso positivo. Este documento:

1. Descarta/degrada los hallazgos con veredicto en contra.
2. Deduplica el mismo problema reportado en varios módulos (1 entrada, todas las
   ubicaciones).
3. Prioriza por severidad y, dentro de cada nivel, pone primero lo seguro de
   tocar antes de Incheon.

**Ajustes aplicados por la verificación adversarial:**

| Hallazgo | Severidad original | Veredicto del revisor | Severidad final |
|---|---|---|---|
| CC-02 / CA-03 (freno EMERGENCY_LINE) | high | confirmado, ajustado | **medium** |
| IMU-1 (BNO muerto = present para siempre) | high | confirmado | **high** |
| VIS-01 (UART cámara frontal sin confirmar) | high | confirmado, ajustado | **low** |

Ningún hallazgo fue marcado falso positivo; los tres HIGH originales se
mantuvieron como hallazgos reales, dos de ellos degradados de severidad.

---

## (A) BUGS Y ROBUSTEZ — tabla priorizada

> Orden: HIGH → MEDIUM → LOW. Dentro de cada nivel, primero lo **seguro de tocar
> pre-Incheon**.

### HIGH

| Subsistema | Ubicación(es) | Problema | Por qué importa | Acción | Riesgo si se toca | Seguro pre-Incheon |
|---|---|---|---|---|---|---|
| top-sensors / shared-pose (IMU-1) | `sensors_imu.cpp:248-253`; `lib/Adafruit_BNO055/Adafruit_BNO055.cpp:401-414, 863-867` | Un BNO que muere a mitad de partido reporta `present=true` para siempre → heading y gyroZ congelados/erróneos, **sin failover**. Hoy el robot corre con 1 solo BNO sano, así que es el ÚNICO sensor de heading. | El heading gobierna toda la orientación táctica (LINE_AVOID, orient-a-arco, PID). Un heading congelado pasa silenciosamente como válido. | Detectar lectura no fresca a costo casi nulo: `getVector` ya devuelve (0,0,0) ante fallo I2C → congelamiento exacto de heading+gyroZ a 0.000 por N ticks (o `i2c_present(addr)` barato cada ~10-20 ticks, NO por tick) baja `present` a false y deja que `imu_fusion` lo marque DEAD. | Bajo si el chequeo es periódico (no por-tick) para no reintroducir la contención BNO+ToF que el band-aid evita. Riesgo de falso-DEAD si el umbral de "congelado" es agresivo. | **Sí** (con validación de banco corta) |

### MEDIUM

| Subsistema | Ubicación(es) | Problema | Por qué importa | Acción | Riesgo si se toca | Seguro pre-Incheon |
|---|---|---|---|---|---|---|
| shared-control / shared-ball (SC-01) | `pids.cpp:28,51`; `kinematics.cpp:6` | Ninguna función de control protege contra NaN/Inf: un heading congelado/corrupto del BNO se propaga a ω sin detección (UB en el cast a int16). | El BNO es la fuente real de un NaN posible; SC-01 + IMU-1 se cubren mutuamente. Guarda barata cierra el agujero. | Guarda O(1) al entrar a `omega_degps_to_centideg` (última compuerta antes del int16): `if (!(x==x)) return 0;` ANTES de multiplicar. No tocar los módulos puros muy testeados. | Bajo: solo actúa con NaN (que hoy = UB). Para todo valor finito el comportamiento es idéntico. Agregar un test que pase NaN. | **Sí** |
| central-core (CC-01) | `comm_top.cpp:19-26`; diag en `main_central.cpp:166-174` | Un `WorldSnapshot` rechazado por tamaño (deploy wire-breaking v2/v3) es **invisible** en telemetría: indistinguible de "sin link". | Si se re-flashea una placa con schema viejo, el síntoma se ve igual que un cable suelto → diagnóstico a ciegas en el pit. | Agregar contador `g_snapshot_size_rejects` (incrementa si `type==WORLD_SNAPSHOT` pero `payload_len != sizeof`) y exponerlo en el print DIAG (`badsz=N`). Cero lógica de control. | Nulo en runtime: contador de diagnóstico aislado, solo un `Serial.print` más. | **Sí** |
| shared-infra (SI-02) | `comm_central.cpp:64` (TOP); `comm_down.cpp:83,94` (CENTRAL) | `Serial.write(buf,n)` es potencialmente **bloqueante** en el loop si el buffer TX se llena. | Un bloqueo en el loop del TOP/CENTRAL puede atrasar el snapshot/odometría y disparar watchdogs de frescura. | Gatear: `if (availableForWrite() >= n) write(); else drop+contador`. Dropear un snapshot @100Hz es inocuo (el siguiente llega en 10 ms). Exponer `tx_dropped`. | Bajo: el gate es aditivo. En operación normal `availableForWrite` siempre alcanza → comportamiento idéntico. Validar drop-rate ~0 en banco. | **Sí** |
| down-otos (otos-1) | `otos.cpp:134-161` | Al caer un OTOS en pleno partido (2→1 OTOS) la pose **teleporta ~100 mm** (deja de reportar el centro del robot, reporta el costado). | Un salto de pose puede meter ruido en la navegación si CENTRAL lo consume como delta. | Pre-Incheon (mínimo y seguro): confirmar que CENTRAL trata `confidence 100→60` como evento de no-confiar-en-delta (congelar fusión un par de ticks). NO tocar el promedio 2-OTOS. La corrección de offset real necesita el signo de montaje validado (TASK-004). | El offset depende del signo/eje real del montaje (`OTOS_SEPARATION_MM=200` tentativo, sin validar). Signo equivocado → el salto se DUPLICA. | **No** (la corrección de offset; el lado CENTRAL sí) |
| top-comms-loc (TCL-01) | `localization_runtime.cpp:59`; `sensors_imu.cpp:167-233`; `imu_fusion.cpp:67`; `main_top.cpp:196,201` | `bno_offset_centideg` se captura como **0 al boot** (antes de haber una fusión válida): la calibración "apuntar al arco rival" no surte efecto. Pose nunca válida hoy. | La pose absoluta queda mal anclada. Impacto latente: hoy main_top usa el heading directo del IMU, no el de la pose. | Capturar el offset DESPUÉS de tener fusión válida: correr 1 `sensors_imu_tick()` antes de leer el heading en init, o gatear por `heading_valid`. Como mínimo, no afirmar que calibra si la fusión no es válida. | Tocar el orden de init del IMU (boot ~40 s, secuencia I2C ToF/BNO delicada). Impacto es latente → riesgo > beneficio inmediato. Documentar y arreglar post-Incheon o con la TASK de localización. | **No** |
| top-main-cameras / shared-ball (CAM-02) | `cameras_runtime.cpp:54-58, 60-87`; `main_top.cpp:123-150` | `build_snapshot` envía pelota/arcos con coords **stale hasta 1 s** tras caída de cámara (watchdog largo) aunque el dato ya no llegue. | El arquero/delantero podrían perseguir una pelota fantasma hasta 1 s. Mitigado si CENTRAL gatea por su propia frescura. | Evaluar en banco bajar `CAMERA_TIMEOUT_MS` de la visibilidad a 150-250 ms, manteniendo el timeout largo solo para el flag de diagnóstico; o documentar que CENTRAL debe gatear la pelota. NO a ciegas. | Bajar el timeout sin medir la tasa real de packets puede hacer parpadear `ball_visible` si las N6 entregan <30 Hz efectivos (probable sin LAB recalibrado). Es tune de banco. | **No** |
| shared-ball (SB-01) | `cameras_runtime.cpp:140-144` | El timestamp de muestra de velocidad (`sample_ms`) no proviene de la cámara que aporta la posición fusionada (usa el `max` de los dos relojes). | Afecta el `dt` de la velocidad → `ball_predict` → anticipación del arquero (GK_INTERCEPT). | Pasar como `sample_ms` el timestamp de la cámara que realmente contribuyó, o derivar la velocidad del `millis()` de la recomputación. Mínimo: documentar el supuesto de tasas similares. | Cambia la cadena velocidad→predict→arquero. El fallback con vx=vy=0 sigue idéntico, pero el lead cambia en banco. Validar con visión recalibrada. | **No** |
| shared-ball (SB-02) | `ball_velocity.cpp:80-93` ← `cameras_runtime.cpp:143-144` | El handoff de pelota entre cámara front↔back puede inyectar un **pico de velocidad de un frame**. | Un pico espurio puede sesgar la anticipación del arquero. El clamp de `ball_predict` ya acota el daño. | Post-Incheon (defensivo): rechazar/atenuar derivadas cuya magnitud exceda una cota física (re-sembrar como en gap largo), o re-sembrar al detectar cambio de cámara fuente. | Cambia cuándo la velocidad es válida → cambia la anticipación. Requiere elegir umbral con datos de banco para no descartar tiros rápidos legítimos. | **No** |
| shared-localization (LOC-02) | `localization.cpp:112-120` | El filtro de rango usa `max_dim` (lado largo): una lectura imposible para el eje corto **pasa el filtro**. | Admite estimaciones de pose espurias al promedio. Pose no cableada hoy en competencia. | Filtrar por la dimensión del EJE que clasifica el ToF: tras `classify_wall()`, comparar contra `field_height` (N/S) o `field_width` (E/W). Mover el chequeo después de `classify_wall`. | Cambia qué estimaciones entran al promedio → puede alterar resultados que los 14 tests dan por buenos. Correr el gate host + banco; mejor post-Incheon. | **No** |

### LOW (robustez/claridad — selección consolidada; el resto vive en el detalle por subsistema)

| Subsistema | Ubicación(es) | Problema | Acción | Seguro pre-Incheon |
|---|---|---|---|---|
| top-sensors (TOF-3) | `sensors_tof.cpp:368-370` vs `:361-366` | El HC-SR04 NO se filtra por frescura (último valor pegado sin timeout); los 4 ToF SÍ. | Darle su propia marca de frescura y pasarlo por `tof_fresh_or_no_reading` (función pura ya testeada). | **Sí** |
| top-sensors (TOF-1) | `sensors_tof.cpp:260` (`#else`, path no-MULTI) | `begin(...,400000)` deja el bus a 400 kHz, contradice la coexistencia BNO+ToF a 100 kHz. Branch no compilado en competencia. | Cambiar el `400000` a `100000` para igualar el path MULTI. Blinda builds alternativos. | **Sí** |
| top-sensors (IMU-2) | `sensors_imu.cpp:139-143, 302-308, 228` | `capture_offset()`/`recalibrate_zero()` bloquean ~200 ms (`10×delay(20)`) y recalibrate corre desde el RX de CENTRAL en pleno loop. | Para el path en-caliente: acumular el offset sobre N ticks sin delay, o aceptar recalibrate solo fuera de juego. Init queda igual. | **Sí** |
| shared-pose (POSE-05) | `sensors_imu.cpp:266-270` | Soft-resync recalcula offset desde una lectura cruda puntual (sin promediar) en pleno loop. | Post-Incheon: promediar 3-5 lecturas al re-sembrar. Con 1 BNO casi nunca se ejecuta. | **Sí** |
| central-core (CC-05) | `world_model.cpp:50-56,112,116` | Dato aplicado exactamente en `millis()==0` queda "nunca recibido" hasta 500 ms (sentinela 0=nunca). | NO tocar hoy: gobierna `SAFE_NO_TOP`. Reemplazar por bool `*_seen` solo si molesta. | **No** |
| down-otos (otos-4) | `otos.cpp:62-67,185-201` | Recuperación en caliente ignora el retorno I2C de `resetTracking`/unidades y marca `present` aunque falle a mitad. | Condicionar `oh_set_present(true)` al éxito del reset. Tratar retorno positivo de la lib como warning (OK), no rechazar. | **Sí** |
| down-otos (otos-3) | `otos_fusion.h:30-38` ← `otos.cpp:142` | Heading dual degenera a 0° si las 2 IMUs discrepan ~180°. | Post-Incheon: si la discrepancia (wrap-aware) supera un umbral, preferir un OTOS o bajar confidence. La magnitud del vector ya es métrica de acuerdo. | **No** |
| shared-localization (LOC-01) | `localization.cpp:198`; `localization.h:54` | `pose.heading_centideg` puede salir fuera del rango documentado `[0,36000)`. | Opción A (segura): corregir el comentario a "sin normalizar". Opción B (post): normalizar con función entera. | **Sí** (solo comentario) |
| shared-localization (LOC-05) | `localization.cpp:136-145,196-197` | Estimaciones de pose pueden quedar fuera de cancha (negativas o > dimensión) sin clamp. | Clampear `x_mm`/`y_mm` a las dimensiones, o marcar pose inválido. Coordinar con consumidor. | **No** |
| shared-line (SL-4) | `line_filters.h:25` vs `comm_central.cpp:34` | Umbral de "levantado" duplicado: literal `28` y derivado `(32*7)/8`. Se desincronizan si N cambia. | Clampear el umbral efectivo a `n_sensors` en `lf_lifted_update` (un threshold > n nunca dispara). Trivial y defensivo. | **Sí** |
| shared-line (SL-5) | `down_model.cpp:262-277` | `line_present` sin debounce puede flickear 1↔0 en el borde de la franja. | NO agregar debounce mal hecho (mezclaría campos de épocas distintas y suma latencia al fail-safe). Dejar anotado. | **No** |
| shared-infra (SI-01) | `telemetry_sat.h:23-28` | `sat_i16`: el redondeo ±0.5 empuja una banda <1 LSB fuera de rango int16 antes del cast (UB técnico). | Redondear antes de comparar, o ajustar los límites a `±32766.5/±32767.5`. Agregar 2 tests de banda. | **Sí** |
| vision-openmv (VIS-02) | `cam-*-n6.py` (X_coded) | `int()` trunca hacia cero → asimetría sub-cm +X/-X, contradice el objetivo "simétrico". | Reemplazar `int(X)` por `round(X)` en ambos archivos. Mantiene el rango [0,200]; el clamp lo cubre. | **Sí** |
| vision-openmv (VIS-04) | `cam-*-n6.py`; `cameras_runtime.cpp:26` | El clamp físico [-100,100] satura la pelota a ±1 m (con `UNIT_TO_MM=10`). | Al calibrar la H (TASK-022), decidir si el rango cubre el alcance útil. Si falta rango, NO ampliar el clamp (rompe wire) → ajustar el factor. | **Sí** (solo el factor en TOP) |
| vision-openmv (VIS-03) | `cam-*-n6.py` (denom≈0) | Homografía degenerada descarta un blob REAL como "no detectado". | Post-calibración: clampear a un borde plausible o solo loguear en BRING_UP. Validar dónde cae el horizonte. | **No** |
| top-comms-loc (TCL-03) | `comm_arbiter.cpp:91-100`; `comm_central.cpp:42-51`; `comm_down.cpp:102-111` | Drenado UART sin cota (`while(available())`) puede demorar el `watchdog_feed` si el productor satura. (Mismo patrón que SI-02, lado RX.) | Acotar a presupuesto por tick: `for(n<MAX && available())`. Con MAX ≈ 1 ring se garantiza progreso. Verificar cap ≥ bytes/tick. | **Sí** |
| down-comms (DC-2) | `comm_top.cpp:88-89` | `slip_estimate`: cast a uint8_t sin saturación inferior (asimetría vs el resto de campos saturados). | Añadir helper puro `sat_u8` en `telemetry_sat.h` (espejo de `sat_i16`) y usarlo. Host-testeable. | **Sí** |
| down-line-raw (DLR-03) | `line_ring.cpp:91` | `analogReadResolution(10)` se setea como efecto global escondido al final de `line_ring_init()`. | Post-Incheon: moverlo al inicio de la función o documentar en `config_down.h` que el pipeline asume ADC 10-bit. | **Sí** |
| central-strategy (CS-3) | `strategy.cpp:652-657` | Estado oculto `static` en PATROL no se resetea en `strategy_init()` ni al re-entrar. | Promover a variables del namespace anónimo, inicializadas en `strategy_init()`/`transition_gk`. | **Sí** |

---

## (B) SIMPLIFICACIONES (sin perder funcionalidad/confiabilidad)

> El firmware ya está bien simplificado. Casi todo lo grande removible es **deuda
> conocida e intencional** (doble cadena de línea, módulos compat) que NO debe
> tocarse pre-Incheon.

### B.1 — Seguro pre-Incheon (quick-wins de bajo riesgo)

| Subsistema | Ubicación(es) | Qué simplificar | Riesgo si se toca |
|---|---|---|---|
| central-strategy (CS-1, CS-4) | `strategy.cpp:685-704, 298-343, 676-688` | En INTERCEPT, `ball_predict` se computa **dos veces** por tick (`bx_pred` se calcula, se pasa, se descarta con `(void)` y la función re-predice). Además lecturas redundantes de `world_model_get_ball_*`. Unificar a una sola fuente por tick. | Bajo: resultado numérico idéntico. Re-correr el espejo host `bt_decide_intercept`. |
| central-strategy (CS-5) | `strategy.cpp:432-444, 630-641` | Bloque LINE_AVOID duplicado entre ATK y GK (solo difiere la constante de velocidad). Extraer helper `compute_line_retreat(speed)`. | Bajo: refactor local mecánico, sin cambio numérico. |
| shared-ball (SB-3) | `cameras_fusion.cpp:46-51` | `fuse_ball_dual` usa promedio "ponderado" con pesos **idénticos** (media simple disfrazada). Reemplazar por `(front+back)*0.5f` como ya hace `fuse_goal_dual`. | Algebraicamente idéntico (mismo int16), cubierto por tests existentes. |
| shared-ball (SB-4) | `cameras_fusion.cpp:48-62,101-102` | Casts float→int16 en fusión sin clamp (seguros hoy por el rango del parser, frágiles ante recalibración). Aplicar `clamp_to_i16` por simetría. | En el rango de hoy nunca se activa → comportamiento idéntico. |
| shared-pose (POSE-02) | `pose_fusion.h:41-42`; `pose_fusion.cpp:59-60` | `otos_stale_ms`/`tof_stale_ms` se setean en config pero **nunca se leen** (knobs muertos). Módulo no cableado: momento barato para limpiar. | Nulo: campos no usados en código que no corre. |
| shared-pose (POSE-03) | `imu_fusion.cpp:238-241` | Tie-break redundante al elegir sensor tras impacto (rama muerta `a<b` siempre). Simplificar a `else pick=a;` con comentario. | Muy bajo: comportamiento idéntico, cubierto por test. |
| central-core (CC-04) | `comm_top.cpp:38-49`+`main_central.cpp:100`; `comm_down.cpp:63-73`+`:101` | El valor de retorno de `comm_top_tick()`/`comm_down_tick()` (frames procesados) se **descarta**. Cambiar firma a `void`. | Muy bajo: ningún caller usa el valor. Grep de confirmación antes. |
| central-actuation (CA-02) | `motors_zircon.cpp:93-95`; `motors_zircon.h:33-37` | `motors_set_one` contradice su doc cuando `MOTOR_INVERT[idx]=-1`. Aclarar en el comentario que aplica el invert lógico. | Ninguno: solo comentario. |
| shared-tactics (ST-02) | `motion_target.cpp/.h` + `test_central_motion` | Módulo **código muerto confirmado** (sin callers en producción, banners lo marcan). Eliminar los tres juntos (o dejar dormido sin tocar). | Muy bajo: borrar sin callers. Quitar el test del runner a la vez. |
| top-main-cameras (CAM-05) | `cameras_runtime.cpp:179-181,195` | `cameras_resync_total()` es alias exacto de `cameras_resyncs_total()`. Dejar un solo nombre. | Casi nulo, sin beneficio funcional. Agrupar con otra limpieza. |
| down-comms (DC-5) | `down_tx.cpp:27-35,61-75` | `encode_generic`/`down_encode_line` ignoran el retorno 0 (error) de `proto_encode`; `write_frame` con n==0 es no-op silencioso. Agregar contador (rama hoy inalcanzable). | Mínimo si solo se agrega contador. |
| shared-infra (SI-03) | `proto.cpp:47-117` | `FrameDecoder::feed()` — switch sin `default`; un estado nuevo caería silenciosamente a `return false`. Agregar `default: reset(); return false;`. | Nulo: el default es inalcanzable hoy. Mantenibilidad. |
| top-hal (TOPHAL-01) | `pinout_common.h:137,146-150`; `localization.h:25,35` | `NUM_TOF` no está atado por `static_assert` al tamaño `[4]` de los arrays: el plan de escalado a 6 ToF documentado produciría **out-of-bounds silencioso**. Agregar `static_assert`. | Prácticamente nulo: con NUM_TOF=4 el build pasa igual. Solo dispara si alguien sube NUM_TOF sin ampliar arrays. |
| top-hal (TOPHAL-02) | `comm_arbiter.cpp:40-41`; `pinout_common.h` | Pines 5/6 del árbitro definidos LOCALES, no declarados/reservados en el HAL (misma clase de colisión latente que el pin10). Mover a `pinout_common.h` o reservar con comentario. | Bajo si solo se agrega comentario/constantes nuevas. |
| down-line-raw (DLR-02) | `line_ring.cpp:95,98,129-130` | `g_last_sample_us` se captura al FINAL del tick, no en el instante del muestreo que su nombre promete. Reusar `t_start`. | Mínimo: cambia `sample_age_ms` en <1 ms (más preciso). |
| shared-tactics (ST-05) | `strategy_transitions.cpp:153-154` | `gk_decide_transition` calcula `dist` incondicionalmente aunque varios estados no lo usan. Mover el cálculo dentro de los cases. | Casi nulo: refactor puro en módulo no usado por el robot. |

### B.2 — Post-Incheon (deuda conocida, NO tocar ahora)

| Subsistema | Ubicación(es) | Qué simplificar | Por qué esperar |
|---|---|---|---|
| down-line-raw / shared-line (DLR-01, SL-3) | `line_ring.cpp:101-131` vs `down_model.cpp:241-301` | **Doble cadena de línea:** los pasos 2-6 de `line_ring_tick()` (~32 atan2/cos/sin a 1 kHz) son cómputo muerto en competencia; solo los consume diag, mientras producción reprocesa desde `line_ring_get_raw()`. Gatear con `#ifdef DIAG_BUILD` o unificar. | Rompe los builds diag (sin red de tests host sobre este glue). El robot compite hoy; el costo de CPU es tolerable. Alto riesgo / nulo beneficio competitivo. |
| shared-line (SL-1, SL-2) | `down_model.cpp:86-123, 104-117`; `line_geometry.cpp:62-80` | `dm_line_metrics` recalcula el centroide-Y que `lg_compute_xy` ya computó; selección del K-ésimo radio por min-scan repetido (más enredada que `nth_element`). | Churn en un módulo de seguridad (PID lateral del arquero) con márgenes de test ajustados (±6-8 mm). Sin ganancia funcional. |
| down-comms (DC-1) | `down_tx.cpp:54-74` vs `write_frame:39-47` | SEQ se incrementa aunque el frame se dropee por backpressure → infla la métrica `lost` del receptor (~33%). | Cambiar la semántica del SEQ toca un invariante con test verde que protege la detección de pérdida. Mejor con el test actualizado post-Incheon. |
| down-comms (DC-3) | `comm_central.cpp` y `comm_top.cpp` | RX duplicado: `FrameDecoder` + `g_frames_received` + `handle_frame(CENTRAL_RESET_OTOS)` replicados. Factorizar a `rx_drain(...)`. | Tocar el path RX de ambos UARTs por mejora estética arriesga regresión en el camino de comandos. Diferir. |
| shared-infra (SI-04) | `crc16.cpp:5-18`; `calib_storage.cpp:8-19` | CRC-16/CCITT duplicado entre proto y calib_storage. Unificar en un núcleo. | `cs_crc16_ccitt` define el formato persistido en EEPROM: cualquier cambio de resultado invalidaría la calibración ya grabada. |
| top-hal (TOPHAL-04) | `pinout_robot1.h:77-86`; `pinout_robot2.h:47-53` | `NUM_TOF_ACTIVE` y flags `ROBOT_HAS_TOF_*` duplicados/desincronizables a mano, sin consumidor (la enumeración ya es robusta a ToF ausentes). Eliminar o agregar `static_assert`. | Borrar config requiere confirmar que ningún env PIO/diag los referencia. La variante `static_assert` sí sería segura pre-Incheon. |
| central-actuation (CA-05) / shared-control (SC-02) | `motors_zircon.cpp:66-71`; `kinematics.cpp:19,27` | Doble saturación (`saturate_wheels` + clamp interno de `wheel_speed_to_pwm`). **NO remover el clamp de la librería:** es guarda defensiva legítima; otros callers (diag) no saturan antes. | Si alguien "simplifica" quitando `saturate_wheels` se PIERDE la preservación de trayectoria del omni. Recomendación: NO tocar. |

---

## (C) ALTERNATIVAS DE DISEÑO (post-Incheon)

| Subsistema | Propuesta | Justificación / cuidado |
|---|---|---|
| central-actuation (CA-01) | **Compensación de deadzone PWM.** A vx/vy bajos el motor queda en stall (zumba sin moverse). Añadir un piso `MOTOR_MIN_PWM` (típico 25-45) que eleve la magnitud conservando el signo, con umbral de ruido (`\|pwm\|<3→0`). Implementar en el glue (`apply_pwm_to_motor`), NO en la librería pura. | Calibrar `MOTOR_MIN_PWM` por robot en banco. Dejar en 0 (no-op) hasta validar. Si se pone alto, el robot pierde control fino y va a tirones. |
| central-actuation / central-core (CA-03 = CC-02) | **Freno de emergencia real.** `EMERGENCY_LINE` → `motors_brake()` (short-brake HIGH/HIGH) NO está confirmado que frene en el Zircon Rev v15 — puede ser COAST. Si en banco resulta COAST: implementar freno por reversa breve (pulso opuesto controlado) o ajustar el umbral de `imminent_exit` con más margen. | **Medir primero en banco** (datasheet del driver con Enzo). NO tocar el firmware a ciegas: un pulso de reversa mal dimensionado tira el robot fuera de cancha en el peor momento. *(degradado high→medium por el revisor)* |
| diag-suite (diag-06) | **Diag de aceptación de la VISIÓN recalibrada.** No existe un test que valide LAB/homografía v2 contra posiciones conocidas (solo existe `diag_top_cameras` para el ENLACE). Crear `diag_cam_acceptance`: pelota/arco en posiciones medidas (30/60/90 cm), imprimir reportado vs esperado en mm/°, reusando el parser de producción. | Es código NUEVO de banco (no toca firmware vivo) → riesgo de regresión nulo. Como la recalibración ES el bloqueante #1, este test reduce el riesgo de partido. |
| central-strategy (CS-2) / shared-tactics (ST-03) | **Limpiar estado/flags muertos de táctica.** `g_attack_color` se setea/lee pero ningún path de decisión lo consume (la polaridad la resuelve el TOP). `kicker_fire` caracteriza un predicado que `strategy.cpp` descarta con `(void)`. Corregir comentarios ahora; borrar accessors/flag post-Incheon. | Corregir comentarios es seguro. Borrar accessors es riesgo medio (verificar que `main_central`/tests no los referencien). |
| shared-tactics (ST-01) | **Anclar la red de caracterización a la FSM real.** `strategy_transitions` promete ser fiel a `strategy.cpp` pero nada mecánico lo fuerza → deriva silenciosa con gate verde. Mover los thresholds tácticos a un header compartido (`tactics_tuning.h`) incluido por ambos. | Extraer el header toca `strategy.cpp` (el cerebro que anda). Pre-Incheon: solo reforzar el banner de sincronización. Post: el header compartido. |
| down-otos / shared-pose (otos-2, POSE-04) | **Validar el modelo geométrico OTOS.** `OTOS_SEPARATION_MM=200` es tentativo (TASK-004); entra al slip (no al heading, que es promedio vectorial). Medir en banco y confirmar que `vx` es la componente perpendicular a la línea entre los 2 OTOS. | Solo el fix de comentario es seguro ahora. El slip no alimenta control hoy → riesgo de movimiento erróneo nulo por ahora. |

---

## FORTALEZAS — lo que ya está bien y NO hay que tocar

| Subsistema | Qué está sólido |
|---|---|
| central-strategy | FSM lineal y legible; transiciones centralizadas que resetean el PID (sin windup); fallbacks byte-idénticos verificados; saturación anti sign-flip de ω (≤327) aplicada uniformemente; convención de ejes consistente. |
| central-core | Separación glue/puro ejemplar; fail-safe en ingest (rechaza schema distinto en vez de reinterpretar basura); watchdog de frescura limpio (SAFE_NO_TOP); sin llamadas bloqueantes en el loop. |
| central-actuation | Matemática pura testeada; `MOTOR_INVERT` aplicado en un solo lugar; jerarquía de seguridad correcta (EMERGENCY > FSM > stale-stop); BNO local gateado OFF por default; inversión M2/U17 validada en banco. |
| top-main-cameras | Parser FSM exhaustivamente testeado (17 casos adversarios); publicación atómica del packet; loop no bloqueante con WDOG1 de 1 s; expiración de velocidad fantasma; bits de flags coinciden con `types.h`. |
| top-sensors | Glue delega TODA la inteligencia a módulos puros testeados; orden de init documentado con evidencia de banco; GUARD anti-cuelgue del BNO RIGHT; soft-resync no bloqueante; degradación elegante a 1 BNO / N ToF. |
| top-comms-loc | Debounce del árbitro + tracker SEQ extraídos como lógica pura (17 tests, fail-safe a STOP); `handle_frame` valida `payload_len` antes de memcpy; heading desacoplado de la validez de pose. |
| top-hal | Puramente declarativo, MUY bien documentado (fecha de banco + journal por constante); `#error` si no se define el robot; sin código ejecutable = sin races. |
| down-line-raw | Separación HW/algoritmo limpia; muxes validados en banco (0 sensores muertos); todos los getters indexados clampean; calibración bloqueante confinada a setup(). |
| down-comms | Maduro y blindado; camino vivo de línea pinneado al golden por test; saturación float→int con manejo NaN/inf; backpressure correcto (frame completo o nada); EEPROM con `update()` + validación magic/versión/CRC. |
| down-otos | Colisión 0x17 resuelta con dual-bus físico (Wire/Wire1); retornos I2C chequeados → máquina de salud con histéresis+latch; lectura fallida NO teleporta al origen; calibración bloqueante solo en boot. |
| shared-tactics | Módulos puros, tiempo inyectado; `behind_ball` matemáticamente correcto (16 tests); 35 transiciones cubiertas incluyendo órdenes de prioridad sutiles. |
| shared-control | Clamp≤327 documentado Y testeado contra sign-flip; anti-windup por conditional-integration con test de regresión; `dt` clampeado; divisiones peligrosas guardadas; bugs históricos cerrados con test. |
| shared-ball | ~50 tests cubren bordes reales; convenciones de marco/signo consistentes y cruzadas; fallback automático byte-idéntico garantizado por test; expiración por tiempo evita velocidades fantasma. |
| shared-localization | Aritmética entera deliberada (LUT Q12 host=target); guardas de índice en todos los helpers; fail-safe correcto; inversión de signo del heading centralizada con comentario anti-duplicación. |
| shared-pose | Módulos puros densamente testeados; wraparound angular correcto; int32 para acumular sin overflow; guard anti-cuelgue I2C; soft-resync no bloqueante. |
| shared-line | Módulos puros muy testeados (cross_track, penetration, saturación, mux-dead, corner); aritmética defensiva (overflow, wrap-safe); `data_valid` respetado en todos los helpers; `static_assert` de tamaño + validación de schema. |
| shared-infra | De los mejor blindados: `FrameDecoder` bounds-safe por construcción; 35+ tests adversarios; contratos con `static_assert` + validación tipo/tamaño/schema antes de memcpy; aritmética de tiempo wrap-safe. |
| vision-openmv | Contrato v2 coincide EXACTO con el parser del TOP; reglas de HW de la N6 respetadas; clamp anti-crash; `BRING_UP` separa calibración de competencia; filtro de forma fail-open con apagado explícito. |
| diag-suite | Receptores usan parsers de PRODUCCIÓN (no reimplementan); chequean `payload_len` contra `sizeof`; detectan staleness/CRC/seqGap; sketches de motor con anti-rebote y guards; espera-USB-con-timeout en todos. |

---

## SALUD POR SUBSISTEMA

| Subsistema | Health | Resumen (1 línea) |
|---|---|---|
| central-strategy | solid | Cerebro táctico sólido; solo simplificaciones locales (doble predict, estado static, LINE_AVOID duplicado). |
| central-core | solid | Orquestación fina y bien testeada; el único hueco es observabilidad (CC-01) y la dependencia HW del freno (CC-02). |
| central-actuation | minor-issues | Cadena de motor bien separada; deadzone PWM sin compensar y el freno HIGH/HIGH sin confirmar en el Zircon. |
| top-main-cameras | solid | Parser FSM y loop robustos; pelota stale hasta 1 s (CAM-02) y factor unit_to_mm placeholder son de banco. |
| top-sensors | minor-issues | Glue limpio; el manejo de BNO muerto (IMU-1, HIGH) no llegó al nivel del de los ToF. |
| top-comms-loc | minor-issues | Árbitro y SEQ blindados; offset de heading capturado en 0 al boot (TCL-01) y drenado UART sin cota. |
| top-hal | solid | Config declarativa impecable; solo guardas baratas (static_assert NUM_TOF, reservar pines 5/6). |
| down-line-raw | solid | Cadena cruda correcta; pipeline procesado es cómputo muerto en competencia (deuda dual-chain conocida). |
| down-comms | solid | Maduro y blindado; solo DRY del RX duplicado y comentarios stale. |
| down-otos | solid | Dual-bus resuelve la colisión 0x17; teleport ~100 mm al caer a 1-OTOS y separación sin validar. |
| shared-tactics | solid | Táctica pura correcta; riesgo es deriva silenciosa de la caracterización + `motion_target` muerto. |
| shared-control | solid | Núcleo de control de alta calidad; solo falta guarda NaN (SC-01) en la última compuerta. |
| shared-ball | solid | Fusión/velocidad/predicción bien testeadas; timestamp de velocidad y handoff front↔back a validar en banco. |
| shared-localization | solid | Trilateración limpia y testeada; filtro de rango por max_dim (LOC-02) y sin clamp a cancha. |
| shared-pose | solid | Percepción de pose de alta calidad; `pose_fusion` no cableado tiene ejes invertidos + knobs muertos. |
| shared-line | solid | Cadena nueva de línea muy testeada; deuda es la doble cadena (no tocar) y duplicaciones cosméticas. |
| shared-infra | solid | De los mejor blindados; solo TX potencialmente bloqueante (SI-02) y CRC duplicado (post). |
| vision-openmv | solid | Código sólido y alineado al wire; lo que queda es BANCO (recalibrar LAB/H, confirmar UART frontal). |
| src/main.cpp (legacy) | minor-issues | Sketch de banco de 4 movimientos (ROBOT 2), env huérfano; candidato a archivar post-Incheon, NO es firmware vivo. |
| diag-suite | minor-issues | Suite muy completa; ~7 sketches cumplieron su función y son candidatos a archivo (post-Incheon). |

---

## PRÓXIMOS PASOS RECOMENDADOS

### Aplicar YA — quick-wins seguros (aditivos / solo-comentario, no tocan el camino vivo)

> ✅ **APLICADOS 2026-06-04** (mergeados a main): SC-01, SI-03, SB-3, VIS-02 (commit `f73c8c0`) + CC-01, SI-02 (commit `f069c7f`). Segunda tanda en rama `mejoras/auditoria-2026-06-04`: POSE-01, POSE-03, DC-2, TOPHAL-01, TOF-1 + correcciones de comentarios y docs. Gate host **550/40/0**, las 3 placas compilan.

1. **SC-01** — Guarda NaN en `omega_degps_to_centideg` (cierra el agujero de heading
   corrupto del BNO desde el lado del control; complementa IMU-1). + test.
2. **CC-01** — Contador `g_snapshot_size_rejects` en la telemetría de CENTRAL
   (diagnóstico de deploy wire-breaking en el pit).
3. **SI-02 + TCL-03** — Gatear `Serial.write` por `availableForWrite` y acotar el
   drenado RX por tick (los 3 comm_* del TOP y los del CENTRAL). + contador
   `tx_dropped`.
4. **TOF-3** — Filtrar el HC-SR04 por frescura reusando `tof_fresh_or_no_reading`.
5. **TOPHAL-01** — `static_assert(NUM_TOF <= tamaño de arrays)` (1 línea, blinda el
   escalado a 6 ToF).
6. **VIS-02** — `round()` en vez de `int()` en las 2 cámaras (simetría +X/-X).
7. **Simplificaciones B.1 de cero-riesgo:** CS-1/CS-4 (doble predict), SB-3 (media
   simple), POSE-02/POSE-03 (knobs/rama muertos), ST-02 (borrar `motion_target`),
   CC-04 (`void`), SI-03 (`default` en el switch).
8. **Correcciones de comentarios stale** (todas cero-riesgo): diag-01 (Serial7),
   diag-02/diag-07 (ejes / 31B), DC-4, CS-2/ST-03, otos-5, LOC-01, SB-5, CA-02.

### Llevar al BANCO (los 3 temas grandes — NO tocar firmware a ciegas)

1. **IMU-1 (HIGH)** — Implementar el failover del BNO muerto (chequeo periódico de
   congelamiento) y validar en banco que no genera falsos-DEAD con el robot quieto.
2. **CC-02 / CA-03 (MEDIUM)** — Medir brake-vs-stop en el Zircon Rev v15 (datasheet
   del driver con Enzo). Decidir si hace falta freno por reversa.
3. **TASK-022 (bloqueante real #1)** — Recalibrar LAB + homografía de las 2 N6,
   confirmar el UART de la cámara frontal (VIS-01), medir `CAMERA_UNIT_TO_MM`. Crear
   el `diag_cam_acceptance` (diag-06) para validar contra posiciones conocidas.
4. **Tune de banco asociado:** CAM-02 (timeout de cámara), SB-1/SB-2 (velocidad de
   pelota), otos-1/otos-2 (teleport y separación OTOS).

### Dejar POST-Incheon (deuda conocida)

- Unificar la doble cadena de línea (DLR-01/SL-3), el RX duplicado (DC-3), el CRC
  duplicado (SI-04), el header de tuning táctico compartido (ST-01).
- Archivar `src/main.cpp` legacy (RM-03) y los ~7 sketches diag cumplidos
  (diag-03/04/05).
- Compensación de deadzone PWM (CA-01) — requiere calibración por robot.
- Cablear la fusión de pose (corrigiendo antes POSE-01: ejes invertidos en el
  default no-cableado).

---

> **Nota de método.** Cero hallazgos fueron marcados como falso positivo en la
> verificación adversarial; los 3 HIGH originales se confirmaron como reales, dos
> de ellos degradados de severidad (freno → medium, UART cámara → low). El único
> HIGH que sobrevive es IMU-1 (BNO muerto sin failover), seguro de mitigar pre-
> Incheon con validación de banco corta.
