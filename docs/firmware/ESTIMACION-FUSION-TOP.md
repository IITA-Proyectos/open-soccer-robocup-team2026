---
title: "Capa de estimación/fusión del TOP — diseño de la capa 'estimador' entre la pizarra de sensores y el WorldSnapshot a CENTRAL"
date: 2026-06-14
author: "Claude (Anthropic - Claude Opus 4.8 1M)"
status: vivo
tipo: diseno
area: control
scope: src/shared + src/top
---

# Capa de estimación/fusión del TOP

> **Cómo leer este doc.** Es un diseño + backlog, NO una orden de tocar el firmware
> vivo. Cada cosa que se cablee va detrás de un flag **default-OFF** y se **tunea en
> banco post-Incheon**. La regla no negociable del repo sigue valiendo: testing en
> hardware real lo cierra el equipo humano, no Claude. Acá se separa con honestidad
> lo que es **"cablear y tunear"** (módulos que ya existen, son puros y están
> host-testeados) de lo que **falta construir**.

---

## 1. Esencia en una frase

Hoy el TOP arma el `WorldSnapshot` **leyendo el último valor cacheado de cada
sensor** (`build_snapshot`, `main_top.cpp:105-195`): sale a 100 Hz pero su
**contenido** es "lo que dijo cada sensor la última vez que habló", no "el estado
del mundo *ahora*". La capa que falta entre la **PIZARRA** (lecturas crudas) y el
**ENVÍO** (snapshot a CENTRAL) es un **ESTIMADOR**: una capa que **predice a
100 Hz** con lo rápido-pero-derivante (odometría OTOS), **corrige cuando llega un
dato fresco** con lo lento-pero-absoluto (paredes por ToF, arcos por cámara) y
**rechaza outliers** (saltos imposibles por oclusión, glitches), emitiendo un estado
**coherente** (todos los slots referidos al mismo instante) y **fresco** (propagado
a *ahora*).

---

## 2. Diagrama de la capa

```
            PIZARRA (lecturas crudas, cada una a SU tasa)
   ┌──────────────────────────────────────────────────────────────────┐
   │  4 ToF paredes (~30 Hz, casi siempre solo eje Y)                   │
   │  2 BNO055 heading (20-100 Hz según bus)                            │
   │  2 cámaras: pelota + 2 arcos (~30 Hz)                              │
   │  OTOS de DOWN por UART: pose+vel (100 Hz, SUAVE, deriva)           │
   │  ToF/HC-SR04 obstáculo (round-robin, ~120 ms/sensor)              │
   └──────────────────────────────────────────────────────────────────┘
                                 │  valores crudos + timestamps de frescura
                                 ▼
   ╔══════════════════════════════════════════════════════════════════╗
   ║                    ESTIMADOR  (a construir/cablear)               ║
   ║   por cada cantidad:  PREDICT@100Hz → CORRECT-on-fresh → GATE     ║
   ║                                                                    ║
   ║   HEADING (raíz)  predict: gyro/BNO   correct: arco/pared  [base] ║
   ║   POSE x,y        predict: ΔOTOS rot. correct: ToF        gate    ║
   ║                     a marco-cancha       (residual >400 → reject) ║
   ║   PELOTA x,y       predict: p+=v·dt    correct: frame cám  gate   ║
   ║   PELOTA vx,vy     α-β / EMA           (innovación)               ║
   ║   ARCOS ang/dist   predict: pass+hold  correct: frame cám  gate   ║
   ║   VEL PROPIA       (de OTOS, hoy no viaja en el snapshot)         ║
   ╚══════════════════════════════════════════════════════════════════╝
                                 │  estado coherente, propagado a `now`
                                 ▼
            WorldSnapshot (31 B, types.h) — emitido a 100 Hz
                                 │  Serial4
                                 ▼
                              CENTRAL (strategy / world_model)
```

> **Regla de oro del estimador:** *predecí con lo rápido-pero-derivante, corregí con
> lo lento-pero-absoluto.* El OTOS @100 Hz es el reloj de la predicción; el ToF/arco
> es el ancla absoluta que mata la deriva.

---

## 3. Tabla por cantidad estimada

| Cantidad | Método HOY (archivo:línea) | Módulo que existe | ¿Cableado? | Técnica recomendada | Mejora que aporta |
|---|---|---|---|---|---|
| **POSE x,y** | Trilateración ToF+IMU directa, sin propagación. `build_snapshot` lee el pose cacheado (`main_top.cpp:111-114`); `localization_compute` (`localization.cpp:110-221`) a ~30 Hz (`main_top.cpp:332-335`). Si invalid → x/y=0, conf=0. | `localization.{h,cpp}` (trilateración, entera, LUT cos Q12) · `pose_fusion.{h,cpp}` (complementario ToF+OTOS) · `pose_filter.{h,cpp}` (EMA/mediana + gate salto) | localization: **SÍ** · pose_fusion: **NO** · pose_filter: **NO** | Complementario 1D por eje: predict = ΔOTOS rotado a cancha; correct = tirón K hacia ToF (`pose_fusion.cpp:125-194`) | Pose **continua y suave** en vez de intermitente/0; tapa los huecos donde la trilateración es invalid (la mayoría del tiempo) |
| **HEADING** (raíz) | Siempre del BNO. 2 BNO055 IMUPLUS → `imu_fusion_update` (promedio circular pesado + glitch-reject) → `sensors_imu_get_heading_centideg` → snapshot (`main_top.cpp:124`). Sin referencia absoluta. | `imu_fusion.{h,cpp}` (fusión 2 BNO) · `imu_freeze.{h,cpp}` (detector de congelado, **GATED OFF**) · `goal_polarity` (color rival) | imu_fusion: **SÍ** · imu_freeze: **GATED OFF** (`-DTOP_ENABLE_BNO_FREEZE_DETECT`) | **Estabilizar primero** (activar freeze-detect, desambiguar BNO/OTOS, bajar latencia). Luego complementario 1D: predict gyro, correct bearing de arco | Recupera la red contra "BNO vivo pero clavado"; frena la deriva sin cota del IMUPLUS |
| **PELOTA x,y** | Fusión front+back por **promedio** `(f.x+b.x)·0.5` (`cameras_fusion.cpp:54-88`) → "pelota fantasma". Re-lee el último packet a 100 Hz sin propagar entre frames (`cameras_runtime.cpp:94-140`). | `cameras_fusion.{h,cpp}` (fuse_ball_dual) · `ball_sticky.h` (titularidad+memoria, **default-OFF** `-DTOP_CAM_STICKY`) | fusión: **SÍ** · sticky: **DEFAULT-OFF** | **α-β 1D por eje** (predict pos+=v·dt @100Hz, correct al frame) + activar `ball_sticky` | Mata la pelota fantasma; pos **propagada** a 100 Hz; vel sin el lag del EMA |
| **PELOTA vx,vy** | Diferencias finitas + EMA α=0.4 (`ball_velocity.cpp:30-98`). **Solo gatea gap TEMPORAL** (`dt_ms>200` → re-siembra); **NO hay gate de innovación espacial hoy** — un blob fantasma dentro de los 200 ms se mete derecho al EMA. Reset duro al perder visión. Viaja en snapshot (`main_top.cpp:136-137`). | `ball_velocity.{h,cpp}` · `ball_predict.{h,cpp}` (lead) · `ball_trajectory.{h,cpp}` | velocity: **SÍ** · predict/trajectory: SÍ pero en **CENTRAL** | Estado vel del mismo **α-β** (1 sola estructura para pos+vel) | Velocidad suave sin lag; el gate de innovación **(que recién llega con el α-β propuesto, NO existe hoy)** protegería de blobs saltados |
| **ARCOS ang/dist** | Crudo por frame, sin historia. `fuse_goal_dual` promedia (x,y) si ambas ven (`cameras_fusion.cpp:90-125`) → ángulo que ninguna vio. Passthrough a strategy (`strategy.cpp:711`). | `cameras_fusion.{h,cpp}` (fuse_goal_dual) · `goal_polarity.{h,cpp}` (latch color) · `pose_targeting.h` (geometría, **sin cablear**) | fusión+latch: **SÍ** · landmark: **NO** | Pre-filtro hold-last + mediana-3 + gate de salto sobre el **bearing**; luego usarlo como landmark de heading (complementario 1D) | Mata el spike de 1 frame (poste/reflejo) que hoy apunta mal el tiro |
| **VEL PROPIA** | **No viaja en el snapshot.** Llega a CENTRAL por un **camino paralelo** (OTOS directo DOWN→CENTRAL, Serial1). TOP recibe el mismo OTOS (`comm_down.cpp:139-140`) pero `build_snapshot` no lo lee (solo debug, `main_top.cpp:383-386`). | `otos_fusion.h` (2 OTOS, en DOWN) · `comm_down` (RX en TOP, ignorado) | en DOWN: **SÍ** · en TOP-snapshot: **NO** | Cablear los campos `my_vx/my_vy/my_omega` al snapshot (WIRE-BREAKING) o consumir el ΔOTOS para predecir pose | Una sola fuente de verdad para CENTRAL; habilita la predicción de pose |

---

## 4. El patrón MULTI-RATE explicado

Cada sensor habla a su propia tasa. El snapshot sale a 100 Hz. Si emitimos el último
valor cacheado, **mentimos frescura**: a 1 m/s la pelota recorre ~33 mm entre dos
frames de cámara (33 ms) y ~120 mm entre dos lecturas del mismo ToF. El patrón
correcto, por cantidad:

1. **PREDICT @100 Hz** — con lo **rápido-pero-derivante**. Avanzá el último valor a
   *ahora* usando un modelo cinemático barato:
   - pose: `pose += Δodometría_OTOS` (la OTOS habla a 100 Hz; es el reloj natural).
   - pelota: `pos += v·dt`.
   - heading: `heading += ω·dt`.
2. **CORRECT-on-fresh** — con lo **lento-pero-absoluto**. Cuando un sensor entrega
   dato nuevo (frame de cámara, ToF valid, bearing de arco), tirá suave hacia él con
   ganancia K: `estado += K·(medición − estado)`. El absoluto no deriva, así que ancla.
3. **GATE / rechazo de outlier** — antes de corregir, mirá el **residual de
   innovación** `medición − predicción`. Si es físicamente imposible (ToF saltó >400 mm
   por una pared tapada; blob que saltó 1 m en 33 ms), **rechazá** (no integres,
   mantené la predicción).

> ⚠️ **El gate de salto atrapa TELEPORTS, NO envenenamiento LENTO.** El gate de
> `pose_fusion` (`pose_fusion.cpp:154`, |res|>400 mm) y el de `pose_filter`
> (`pose_filter.cpp:186`) comparan UNA medición contra el estado con umbral fijo. Eso
> atrapa el salto de 1 frame (poste, reflejo, reset de OTOS) — bien. Pero **NO** atrapa
> una corrupción incremental sostenida: un rival que se arrima a la pared empujando la
> lectura del ToF de a 50-100 mm/tick, cada paso pasa el gate, y la suma camina la pose
> fuera de lugar mientras el gate dice "todo normal". En cancha el caso sostenido es
> **más común** que el teleport. La red contra esto es distinta: acumular cuántos ticks
> seguidos la corrección ToF empuja en la MISMA dirección y, si supera un budget,
> desconfiar del landmark (ver mejora #16, slip/sesgo sostenido).

### ⚠️ La corrección ToF NO es heading-independiente (la SEGUNDA raíz)

El diseño trataba la trilateración ToF como el "ancla absoluta lento-pero-confiable"
que mata la deriva del OTOS. **Pero la trilateración DEPENDE del heading**:
`localization_compute` usa `heading_deg` (del BNO) para calcular el `world_angle` y
para `classify_wall` (`localization.cpp:122,137,139`). **Si el heading está
congelado/equivocado, el ToF asigna sus lecturas a la pared EQUIVOCADA y proyecta la
distancia con el ángulo equivocado.** Resultado: el "absoluto" que `pose_fusion` usa
para corregir (`pose_fusion.cpp:148-164`) está corrupto **en la misma dirección** que
el error de heading. La premisa "predict deriva (heading) + correct ancla
(independiente)" es **FALSA con heading malo**: predict y correct comparten el mismo
error de raíz, así que la fusión **no corrige, refuerza**.

> **Por eso el pre-requisito del heading tiene que ser un INTERLOCK en código, no
> prosa.** Hoy `pose_fusion_update` no recibe ni consulta ninguna señal de
> heading-válido (`PoseFusionInputs` no tiene `heading_valid`; el pass-through y la
> corrección ToF corren incondicionalmente). El cierre del agujero (ver §7 Fase 2):
> (a) agregar `bool heading_valid` a `PoseFusionInputs` y, si es `false`, **saltear la
> corrección ToF** y degradar confidence (no integrar predict OTOS rotado por un
> heading malo); (b) gatear el `tof_valid` de la fusión por
> `sensors_imu_get_heading_valid()` **además** de la readiness del ToF
> (`localization_runtime.cpp:82-83` hoy solo chequea distancia); (c) `#error` de
> compilación si `TOP_ENABLE_POSE_FUSION` está ON y `TOP_ENABLE_BNO_FREEZE_DETECT` está
> OFF.

### ⚠️ El SEED no tiene gate — el ORIGEN x,y es otra raíz silenciosa

El **primer** `tof_valid` ancla directo: `st.x=tof_x; st.y=tof_y; confidence=90`,
**SIN chequeo de plausibilidad** (`pose_fusion.cpp:92-123`, PASO 1). El gate de salto
solo existe DESPUÉS de `initialized=true` y compara contra una predicción que arranca
**del propio seed**. O sea: **todo el gating posterior es relativo a un origen que el
filtro nunca valida.** Si al boot un rival ya está parado contra la pared (o un ToF da
una lectura corrupta en el primer frame valid), el filtro se ancla a una pose
equivocada y desde ahí "corrige" coherentemente hacia el lugar errado. Fix: **gate de
seed** — no anclar con el primer `tof_valid` suelto; exigir N tofs valid consecutivos
consistentes (mediana de ~3-5 dentro de un radio) o consenso con la pose OTOS
acumulada, antes de marcar `initialized=true`. El heading es la raíz; el **origen
x,y** es la segunda raíz no declarada.

### ⚠️ La fuente "absoluta" tiene su rechazo de outliers DE-FACTO APAGADO

`reject_outliers` (`localization.cpp:166-192`) requiere `count>=2` estimaciones **en
el mismo eje** para descartar una. Pero los ToF están "casi siempre solo eje Y" (§2):
con **un solo ToF por eje, `reject_outliers` no hace NADA**. Un rival tapando un ToF da
una lectura corta que pasa el filtro de rango (`localization.cpp` solo descarta
`d>cancha` o `d<10mm`), genera una estimación de pared más cercana, y `pose.valid=true`
con una posición corrupta < 400 mm de la predicción → `pose_fusion` la ancla. El gate
de 400 mm **no protege** contra esto. Fix: exponer en `localization` la cuenta de ToFs
por eje y **degradar `tof_valid`** (o bajar K) cuando el eje se sostiene con UNA sola
estimación sin corroboración (ver mejora #21).

### El HEADING es la RAÍZ — estabilizar primero

El heading **rota todo el mapa**. Si está mal, la pose puede ser internamente
coherente pero **toda equivocada** (el robot "cree" mirar al arco y no). Tres
consecuencias que vuelven el heading un **pre-requisito**:

- **El delta OTOS se integra rotado por heading.** Si el heading miente, la
  predicción de pose **deriva en la dirección equivocada** (§7, riesgo de frame).
- **El cero depende del boot.** `capture_offset` asume que el robot arranca mirando
  al arco rival (`sensors_imu.cpp:366-369`). Si arranca mal apuntado, todo el mapa
  queda rotado.
- **Modo de falla real (banco R1):** un BNO que ackea con yaw clavado pasa como
  `heading_valid` **para siempre** porque `imu_freeze` está GATED OFF. En el arquero
  eso es un gol en contra silencioso.

> ⚠️ **El `heading_valid` de hoy NO es "heading confiable".**
> `sensors_imu_get_heading_valid()` (`sensors_imu.cpp:498`) devuelve `fused_valid`,
> que es `true` **mientras el BNO ackee en el bus** — NO mientras el yaw sea correcto.
> El único detector que distingue "BNO vivo pero clavado" es `imu_freeze`, hoy **GATED
> OFF**. **Sin `imu_freeze` activado-y-validado en banco NO existe ninguna señal de
> "heading frozen" que la fusión pueda gatear.** Por eso la mejora #1 (activar+validar
> `imu_freeze`) **no es una P independiente: es PRE-CONDICIÓN DURA de la mejora #7**
> (cablear `pose_fusion`). El flag de pose **no debe compilar sin** el de freeze-detect
> (ver §7, regla `#error`).

#### Doble conteo de heading por la VENTANA del ΔOTOS (no por la puerta)

El diseño blinda correctamente la **puerta del heading**: el heading reportado es
**SIEMPRE el del BNO**, `pose_fusion` NO lo fusiona (`pose_fusion.cpp:90`,
`pose_fusion.h:16-18`, pass-through verificado). **Pero hay un doble conteo
IMPLÍCITO por la ventana de atrás:** el Δde-posición del OTOS que se quiere integrar
(`pose_fusion.cpp:128-132`, sumado **crudo**) **ya viene rotado por el heading PROPIO
del OTOS** — `down/otos.cpp:142` produce su heading absoluto (`fuse_dual_heading_deg`
de sus 2 IMUs internas) y con él integra su `x,y` de mundo. Al sumar
`Δ(otos_x,otos_y)` sobre una pose cuyo heading de referencia es el BNO, **se
re-importa silenciosamente el heading del OTOS**. El heading del OTOS no entra por la
puerta del slot `heading`; entra por la **ventana del delta de pose**.

El fix correcto **NO es rotar por un offset BNO−OTOS capturado al boot** (lo que
decían §6 y la mejora #8 originales). Ese offset **no es constante**: ambos headings
derivan independientemente (el BNO por yaw-drift del giroscopio, `imu_fusion.h:20-24`
lo admite; el OTOS por su propia IMU), así que un offset viejo mete error de
dirección **creciente** → justo el modo "pose coherente pero toda equivocada". El fix
es atar la des-rotación al heading **vivo** del mismo instante:

1. **Des-rotar** el delta de mundo-OTOS a marco-robot con el `heading_otos` del MISMO
   instante en que el OTOS lo produjo: `Δrobot = R(−heading_otos)·Δmundo_otos`.
2. **Re-rotar** a cancha con el heading del **PRIMARIO (BNO)**:
   `Δcancha = R(heading_bno_cancha)·Δrobot`.

Así la pose queda referida 100% al BNO y el heading del OTOS solo aporta el **MÓDULO**
del desplazamiento, nunca su dirección. Equivale a rotar por la diferencia
**instantánea** `(heading_bno − heading_otos)` de ESTE tick, **no** la de boot.

> **Regla normativa (load-bearing).** `Pose2D.heading_centideg` (el heading del OTOS,
> que llega vivo al TOP por `down/comm_top.cpp:72` → `comm_down.cpp:53` →
> `comm_down_get_pose()`) **NO se usa como fuente de heading en TOP**. El primario de
> heading es el BNO, único y sin fusión. El **único uso permitido** del heading del
> OTOS es el **detector de divergencia** `|BNO−OTOS|>umbral` (mismo dato que pide la
> mejora #3) para gatear/invalidar — **nunca para fusionar**. Cuando se cablee
> `PoseFusionInputs` con `comm_down_get_pose()`, ese campo se **descarta**
> explícitamente (hoy `build_snapshot` ya solo lee el BNO, `main_top.cpp:124`).

> **Un solo filtro de heading.** El heading se filtra UNA sola vez, en `imu_fusion`.
> Si se encadena `pose_filter` tras `pose_fusion` (§7 Fase 2 paso 3), su EMA circular
> sobre `heading_centideg` (`pose_filter.cpp:208-219`) re-suaviza el MISMO heading BNO
> (no hay segunda fuente → no es doble conteo) **pero agrega lag a la raíz**. Hay que
> **bypassear** el suavizado de heading de `pose_filter` (pasar heading directo) o
> medir y aceptar el lag en banco. La regla "un solo gate" se extiende al heading: un
> solo filtro, y que sea el de `imu_fusion`.

> **Aclaración sobre `heading += ω·dt` (§4).** Esa fila de la tabla de predicción es
> **ilustrativa del patrón genérico**. En ESTE robot el heading NO se propaga por
> integración de `ω`: se toma **directo del BNO** (pass-through, única fuente). Hoy NO
> hay integrador de heading propio del estimador. Si alguna vez se quisiera propagar
> heading entre updates del BNO, se usaría el `ω` del **mismo BNO/giroscopio primario**
> (atado a `imu_freeze`), **jamás** el `ω` del OTOS — eso sería un segundo conteo y, con
> el BNO congelado, el `ω` muerto-pero-plausible rotaría todo el mapa a ritmo constante.

> **Por eso el orden no es negociable: heading estable → recién después cablear la
> propagación de pose.** Mientras el heading no sea confiable (freeze sin detectar,
> doble conteo BNO/OTOS sin desambiguar), encender la fusión de pose **amplifica** el
> error de heading en vez de ayudar.

---

## 5. Realización numérica (float con FPU vs Q8, dt, derivada)

El Teensy 4.0 (Cortex-M7) **tiene FPU hard-float habilitada por defecto** (el core
compila `-mfloat-abi=hard -mfpu=fpv5-d16`): un mult/add float es ~1 ciclo. Esto
cambia el cálculo costo/beneficio del punto fijo:

- **Punto fijo (Q8/Q12)** en `pose_fusion`/`localization`: el motivo declarado es
  **reproducibilidad byte-idéntica host↔target sin libm** (válido para tests), **no
  rendimiento**. Con FPU no compra velocidad. Su costo: el factor K=26/256 crea una
  **ZONA MUERTA** en la corrección — residuales `|ex|≤9 mm` dan corrección entera **0**
  (verificado: `9·26/256=0`, `10·26/256=1`). El anclaje grueso funciona normal
  (400 mm→40 mm de tirón); lo que se pierde es el **afinado fino <9 mm**, que queda
  flotando dentro de esa banda (como cada tick re-integra el ΔOTOS, el residual puede
  mantenerse en la banda y la pose NO converge a <9 mm del ToF). Con float esa banda
  desaparece. Recomendación: para módulos NUEVOS de estimación, **float**; mantener
  entero solo donde la reproducibilidad de test es un requisito declarado.
- **dt medido, no fijo.** Los PID y `ball_velocity` usan `dt` real por muestra,
  clampeado a un rango sano (`pids.cpp:64-69`). Mantener eso. La predicción de pose es
  **event-driven por delta de odometría** (no necesita dt). El `lookahead` de
  `ball_predict` hoy es **fijo 0.2 s** (`ball_predict.cpp:6`) — debería atarse a la
  latencia **medida** OTOS→snapshot→CENTRAL→motor, no a una constante.
- **Derivada filtrada.** `ball_velocity` filtra (EMA α=0.4) pero **los PID no**
  (`pids.cpp:86,138` derivan crudo) → `kd` amplifica el ruido del BNO. Falta un
  pasa-bajos de 1 polo (derivative-on-measurement).
- **Gate de freshness FINO ≠ heartbeat.** `comm_down_is_pose_fresh()` usa
  `DOWN_HEARTBEAT_TIMEOUT_MS=500` (50 ticks a 100 Hz). Eso es para "DOWN vivo", **no**
  para "dato fresco para integrar". `pose_fusion` espera ~30-60 ms. Antes de cablear,
  exponer `comm_down_pose_age_ms()` y gatear con ~30-50 ms, o integraría deltas de
  hasta 500 ms como frescos → salto al volver de un gap.
- **Medir el WCET ANTES de cablear.** `loop_monitor.h` ya existe pero no instrumenta
  los estimadores. Correr `pose_fusion + pose_filter + ball_velocity + α-β + bt_classify`
  juntos a 100 Hz cuesta µs no medidos. El TOP **ya tuvo el loop a 6 Hz** por I2C
  bloqueante: el presupuesto de "todos los estimadores a 100 Hz" es folklore hasta
  medirlo en el Teensy real.
- **El `omega*100 > 32767` (overflow angular int16) YA está resuelto** en el firmware
  vivo y debe respetarse como invariante: `telemetry_sat.h::omega_rad_s_to_centideg_s_sat`
  satura el camino OTOS→snapshot, y `pids.h` fija `output_clamp=327` °/s con
  `pids.cpp::omega_degps_to_centideg` como conversor seguro. **Cualquier NUEVO productor
  de `omega_centideg_s`** (o el campo `my_omega` del snapshot, mejora #17) **DEBE reusar
  estos helpers, nunca un `static_cast` crudo `*100`.** (Hueco vivo conocido:
  `drive_straight.cpp:28` produce `omega` sin clamp; hoy benigno porque su único caller
  usa `target≈cur` — KICKOFF; ver mejora #22.)
- **Pérdida de resolución del heading en la trilateración.** `localization.cpp:122`
  hace `heading_deg = (bno_heading_centideg − offset)/100` — división **entera** que
  descarta hasta 0.99° **antes** de clasificar pared y proyectar el coseno (LUT de
  grados enteros). Aceptable para gating de cuadrante (45° de margen), pero al rotar el
  ΔOTOS (mejora #7/#8) el heading que rota el delta **NO debe pasar por este truncado**:
  usar el centideg crudo.

> **REGLA: EXPIRACIÓN POR AGE DEL DATO ≠ decay de confidence ≠ clamp de dt.** El
> diseño gatea bien la **entrada** (no integrar un delta viejo cuando el sensor vuelve)
> y hace **decay de confidence**, pero eso **NO acota la divergencia de SALIDA**:
> cuánto tiempo el estimador puede seguir propagando hacia `now` MIENTRAS el corrector
> absoluto está ausente. Hoy `pose_fusion` free-runea sobre el drift del OTOS sin ToF,
> con `confidence` que **pisa en `conf_min=10` y NUNCA llega a 0** (`pose_fusion.cpp:67,
> 181`) → reporta `valid=true` con conf=10 y una pose que diverge **sin techo**. Tres
> conceptos distintos, cada uno con su prueba de banco:
> - **clamp de dt** = anti-salto-puntual tras un stall del loop. Acota UN paso, no
>   cuántos se acumulan.
> - **decay de confidence** = degradación suave de la confianza reportada.
> - **expiración por age** = anti-free-run: cada slot lleva su **propio timestamp de
>   última corrección** y deja de propagarse / pasa a `valid=false` cuando su age supera
>   un umbral **por-cantidad**: pelota **~200 ms**, pose-sin-ToF **~500 ms**
>   (`tof_stale_ms` ya está en el config, `pose_fusion.h:51`), heading atado al
>   freeze-detect. Banco adversarial por cantidad: *cortar el corrector absoluto y
>   verificar que la estimación deja de reportarse `valid`, no que deriva con conf=10.*

> ⚠️ **El α-β de pelota (mejora #9) DEBE heredar la expiración de `ball_velocity`.**
> `ball_velocity.cpp:52-65` tiene una expiración REAL (`max_gap_ms=200`): cuando los
> packets se cortan pero la cámara sigue diciendo "visible" por su watchdog (~1 s), pone
> `valid=false` y `vx=vy=0` — la defensa anti-pelota-fantasma. Si el α-β reemplaza
> `ball_velocity` **sin portar esa expiración**, una pelota que sale del frame con
> cámara colgada en "visible" propaga `pos += v·dt` a 100 Hz indefinidamente → pelota
> fantasma que se va volando recta. **Reemplazar sin portar la expiración es perder una
> red, no ganar una.** El α-β tiene que: tras >200 ms sin frame fresco, `vel→0` y dejar
> de propagar.

> **Veredicto de técnica (transversal):** **filtro COMPLEMENTARIO 1D** (pose, heading)
> y **α-β 1D** (pelota), **NO Kalman/EKF**. Razón: el complementario y el α-β ya están
> escritos/host-testeados, son O(1) por tick, deterministas y **auditables por
> estudiantes de 18 años**. Un EKF agregaría matrices de covarianza y tuning de Q/R que
> nadie va a calibrar bien antes de junio, multiplicando la superficie de bugs en el
> loop vivo — viola "mejora corta y documentada > ambiciosa y opaca". El EKF queda como
> **capitalización 2027** (la FPU del M7 lo habilita sin reescribir el resto).

### El consumidor de la pose es CUANTIZADO — el enemigo no es la varianza, es el TIEMPO cerca del umbral

El único consumidor vivo de la pose **no es proporcional, es bang-bang**. El
seguimiento de pelota del arquero está cuantizado a 3 niveles (`vx = +200/0/−200` mm/s
con deadband de 40 mm, `strategy.cpp:1345-1349,1170`), y `my_x` se usa como
**cruce-de-umbral** para fin-de-tramo y flip de dirección (`strategy.cpp:1585-1617`).
En un cuantizador así, lo que importa **NO es la suavidad de ploteo** (para lo que el
diseño venía optimizando) sino **cuánto TIEMPO la pose pasa cerca del umbral**: 5 mm de
jitter pegado a 350 mm (`GK_PATROL_X_HALF_RANGE_MM=350`) hace chatter de flip-flop de
dirección a tasa de loop → el "bandazo" que `strategy.cpp:1574-1579` ya documenta como
modo de falla de estos motores (piso 25 PWM). Encender la corrección K sin contabilizar
esto mete temblor **directo al bang-bang del arquero**.

Por eso, antes de cablear `pose_fusion`:

- **Cerrar el lazo medir→K (titración, no "a ojo").** Hoy K=26 (~0.10,
  `pose_fusion.cpp:159`) es un default **escrito a mano sin relación con ningún σ**, y
  en TODO el firmware vivo **no existe una sola medición numérica de ruido** (solo el
  deadband de 40 mm tuneado a ojo). La Fase 0 punto 1 debe entregar una **regla
  explícita**, no una intención: (a) medir `σ_ToF` en mm/eje con robot quieto (N≥500);
  (b) **fijar K por fórmula** — arrancar con K tal que `K·σ_ToF ≤ ~5 mm` de paso de
  corrección por tick, y subir K solo hasta que el tiempo-de-anclaje tras un empujón
  baje de un umbral, **registrando en cada paso el conteo de flips**. Sin esa tabla
  titrada (K vs flips vs tiempo-de-anclaje) la Fase 2 no cierra.
- **Histéresis dimensionada por σ en los umbrales que consumen pose.** Entrar al flip
  en 350, salir en 320 (no el mismo 350 para ambos lados), atado al σ medido. El
  deadband de 40 mm de la pelota ya es esa idea pero **sin número**; replicarla en los
  umbrales de pose y **derivarla de la medición**.
- **Cambiar la métrica de cierre** de "pose suave al plotear" a **"CERO flips espurios
  del cuantizador con robot quieto cerca del umbral"** (contar flips de `direction` y
  transiciones de `xdir` por segundo durante 60 s con el robot quieto en `x≈xc+350`).
- **El encadenado `pose_fusion`+`pose_filter` agrega LAG al consumidor de umbral.** Dos
  pasa-bajos en serie (K=0.10 + EMA α=0.25, `pose_filter.cpp:122-127`) son un filtro de
  mayor orden → retardo de fase que, en un consumidor de umbral, **hace sobrepasar la
  línea del arco antes de que la pose "se entere"** (el arquero se mete al arco,
  `strategy.cpp:1628-1636` lo marca como requisito duro). Elegir **uno** como
  suavizador, no ambos en serie, salvo que el banco muestre que el lag combinado cabe en
  el margen. Además **el gate de salto NO compone**: dos saltos sub-umbral consecutivos
  (350+350) = un teleport de 700 mm aceptado en 2 ticks; medir saltos en ventana de 2-3
  ticks o un solo gate sobre la salida final.
- **El lead fijo de 0.2 s amplifica el temblor de vel de pelota.** Al titrar el α-β,
  medir la salida ya **leadeada** (`pos + v·lookahead`), no la vel cruda; criterio: que
  el lead **no introduzca cruces espurios del deadband de 40 mm** con pelota quieta o a
  velocidad constante. Atar `lookahead` a la latencia medida (#19) ANTES de subir β,
  porque un lookahead inflado magnifica cualquier β.
- **Filtrar la derivada de los PID es PRE-REQUISITO, no un P2 suelto.** El MISMO heading
  tembloroso entra DOS veces: por el `kd` del PID de rumbo (`pids.cpp:86`, derivada
  cruda × 1/dt; a 100 Hz, ×100 → temblor de motor directo) **y** rotando la predicción
  de pose (temblor de mapa). El filtro de derivada (pasa-bajos 1 polo,
  derivative-on-measurement) sube a Fase 0 junto con la estabilización de heading;
  dimensionar la frecuencia de corte a partir del jitter de heading medido, no a ojo.

> **Precedente del propio repo (banco 2026-06-14):** el teleport de cámara
> +1000→−1000 que tiraba al arquero fuera (`strategy.cpp:1335-1343`) se arregló con un
> **gate de plausibilidad**, NO con más filtro. Esa es la lección a generalizar: contra
> el cuantizador, gate + histéresis > suavizado.

---

## 6. Qué YA existe y solo falta CABLEAR · vs · qué falta construir

### Ya existe, puro, host-testeado — solo CABLEAR + TUNEAR

| Módulo | Qué hace | Por qué no está vivo |
|---|---|---|
| `pose_fusion.{h,cpp}` | Complementario ToF+OTOS: predict ΔOTOS, correct ToF, K en Q8, gate de salto 400 mm, confidence graduada. **El corazón del predict/correct de pose.** | Ningún caller en `src/top/` lo invoca (verificado por grep). El input que espera (ΔOTOS) llega vivo por `comm_down` y se ignora. |
| `pose_filter.{h,cpp}` | EMA/mediana + gate de salto >400 mm (hold-last-good) + decay de confidence. | Sin cablear. Sería la 2da red para el outlier "por oclusión". |
| `otos_fusion.h` | Math de fusión de 2 OTOS (heading vectorial, slip). | Corre en **DOWN**, no en la pose de TOP. |
| `imu_freeze.{h,cpp}` | Detector de BNO "vivo pero con yaw clavado". | **GATED OFF** (`#ifdef TOP_ENABLE_BNO_FREEZE_DETECT`). El cableado ya está escrito dentro del `#ifdef`. |
| `ball_sticky.h` | Titularidad+memoria que **reemplaza** el promedio front+back (fix de la pelota fantasma). | **DEFAULT-OFF** (`-DTOP_CAM_STICKY`). |
| `ball_predict` / `ball_trajectory` | Lead lineal + clasificación de trayectoria. | Corren al **consumir** (CENTRAL), no propagando-a-now en TOP. |
| `otos_health.h` | **Confidence GRADUADA 0/60/100** del lado OTOS a partir de la salud REAL de los 2 OTOS (histéresis de caída + latch), `oh_pose_confidence()` (`otos_health.h:79`). Reemplaza el viejo confidence-del-boot que "mentía". | Corre en **DOWN** (`down/otos.cpp`). En TOP el `my_pose_confidence` sigue **binario 70/0** (`main_top.cpp:114`). **Es justo lo que la mejora #13 proponía "construir": ya existe.** |
| `heading_rate.h` | Estimador PURO host-testeado de **ω (°/s)** robusto a cadencia lenta del rumbo (deriva solo en muestra-nueva, mantiene entre repeticiones, `heading_rate.h:65-95`). | Corre del lado **consumo (CENTRAL)** (`pfm_heading.h`). El `my_omega` del snapshot (mejora #17) lo **reusaría/alimentaría**, no hay que construir el estimador de ω. |
| `loop_monitor.h` | Mide dt del loop (max_us + EMA). | No instrumentado sobre los estimadores. |

### Falta construir (nuevo, pero chico)

- **Caller/orquestador** (`pose_estimator_runtime` o ampliar `localization_runtime`)
  que arme `PoseFusionInputs`, llame `pose_fusion_update` cada 10 ms y deje la salida
  para `build_snapshot`. Detrás de flag default-OFF.
- **Rotación del ΔOTOS a marco-cancha** por la **diferencia VIVA** `(heading_bno −
  heading_otos)` de ESTE tick — **NO un offset de boot** (ambos headings derivan; un
  offset viejo mete error de dirección creciente, ver §4). Hoy `pose_fusion.cpp:128-132`
  suma el delta **crudo** (re-importa el heading del OTOS por la ventana de atrás). LUT
  cos/sin Q12 ya existe en `localization.cpp`. **Inseparable del cableado de
  `pose_fusion`**, no un paso posterior.
- **Gate de SEED** (el origen x,y es la segunda raíz): N tofs valid consistentes o
  consenso con OTOS antes de `initialized=true`. Hoy el primer `tof_valid` ancla sin
  validar (`pose_fusion.cpp:92-123`).
- **`heading_valid` en `PoseFusionInputs`** + degradar tof_valid de `localization`
  cuando un eje se sostiene con una sola estimación (`reject_outliers` de-facto apagado
  en config solo-eje-Y, §4) + detección de **sesgo sostenido** (no solo el gate de
  salto). + `#error` si POSE_FUSION ON con FREEZE_DETECT OFF.
- **α-β 1D para la pelota** (estructura pos+vel, 2 ganancias por eje). Reemplaza
  `ball_velocity` y agrega propagación. Default-OFF con fallback al método actual.
- **`goal_filter.h`** — gemelo de `pose_filter` pero para el bearing de arco
  (hold-last + mediana-3 + gate). No existe; es chico.
- **Gate de freshness fino** (`comm_down_pose_age_ms()`) y **campos de velocidad
  propia** en el `WorldSnapshot` (WIRE-BREAKING: bump de schema, re-flashear TOP+CENTRAL).
- **Propagación de pelota/heading a `now`** dentro de `build_snapshot` (dead-reckoning
  explícito con clamp de dt + **expiración por age**, ver §5).
- **Sello de frescura en el `WorldSnapshot`.** Hoy el snapshot (31 B, `types.h:98-139`)
  **NO lleva timestamp ni `sample_age` por slot ni global** → CENTRAL no puede gatear
  por staleness, justo lo contrario de la tesis "fresco". El **precedente del fix ya está
  en el repo**: el contrato hermano `LineStatusV2` (`types.h:153`) lleva
  `sample_age_ms`. Proponer un campo de edad análogo en el `WorldSnapshot`
  (WIRE-BREAKING, encadenar con la mejora #17).
- **Cantidad sin analizar: OBSTÁCULO (`min_obstacle_mm`).** Único slot del snapshot
  (`types.h:126`) ausente de la tabla §3; se emite crudo round-robin ~120 ms/sensor
  (`main_top.cpp:175`) — el caso de libro del "último valor cacheado mentido fresco".
  Mejora chica: hold-last + edad por sensor (ver mejora #23).

---

## 7. Plan por fases (gateado off-by-default) + plan de banco

> **Pre-requisito que bloquea todo lo de pose: el HEADING congelado.** Mientras
> `imu_freeze` esté GATED OFF y el doble conteo BNO/OTOS sin desambiguar, **no se
> enciende la fusión de pose**. Un heading que miente rota la predicción OTOS en la
> dirección equivocada. Fase 0 es condición de salida para Fase 2+.

### Fase 0 — Estabilizar el HEADING (raíz) y MEDIR el ruido

Esto va **primero** y es barato (mayormente flags + un diag).

1. **Medir ruido de cada sensor con el robot QUIETO** (pre-requisito de todo tuning):
   cada ToF (σ en mm), deriva del OTOS en 60 s (mm), jitter del heading. *Sin estos
   números, tunear K es tunear sobre arena.*
2. **Activar `imu_freeze`** en un env `*_freeze`: robot quieto ≥2 min → CERO
   falsos-DEAD; luego inducir el freeze → verificar que lo detecta. Recién con esto se
   puede re-habilitar el 2º BNO con seguridad.
3. **Desambiguar BNO vs OTOS en CENTRAL** (dominio del agente CENTRAL): fijar fuente
   canónica por estado del FSM + detector de divergencia `|BNO−OTOS|>umbral`. **No
   fusionarlos aún.**
4. **Bajar latencia del primario** a 100 Hz (`-DTOP_BNO_FAST` con `-DTOP_BNO_PRIMARY_ONLY`).
5. **Instrumentar WCET** con `loop_monitor.h` sobre los estimadores en seco (diag).

   *Banco:* robot quieto (ruido + freeze + WCET) y robot girando a velocidad conocida
   (latencia heading). Criterio de cierre: heading no congela, no deriva detectado,
   loop holgado a 100 Hz con todos los estimadores corriendo.

### Fase 1 — Pelota (independiente del heading, bajo riesgo)

1. **Activar `ball_sticky`** (flip del flag tras validar): pelota adelante + naranja
   espurio atrás → confirmar que NO reporta pelota trasera; rodear el robot → medir
   lag de traspaso.
2. **Calibrar `CAMERA_UNIT_TO_MM`** (hoy placeholder=10, TASK-022) **antes** de
   cualquier filtro: pelota a 30/50/80/100 cm, medir unidades vs distancia real.
3. **α-β 1D** detrás de flag default-OFF: pelota rodando a velocidad medida, comparar
   pos/vel estimada vs real; verificar que quieta no deriva. + gate de innovación
   (patear fuerte → NO rechaza el movimiento real; blob falso → SÍ lo rechaza).

### Fase 2 — Pose (gateada por Fase 0; el grueso del trabajo es banco)

> **Interlock de compilación (no negociable):** el build debe tener un `#error` si
> `TOP_ENABLE_POSE_FUSION` está ON y `TOP_ENABLE_BNO_FREEZE_DETECT` está OFF. El
> pre-requisito del heading se vuelve un **candado de software**, no disciplina humana
> (ver §4). Para estudiantes que cablean por flags, un flag default-OFF que se puede
> encender solo **no es** un pre-requisito.

1. **Cablear `pose_fusion`** detrás de `-DTOP_ENABLE_POSE_FUSION`: alimentar
   `PoseFusionInputs` con `tof_x/y = localization_compute` (`tof_valid = pose.valid`
   **gateado además por `sensors_imu_get_heading_valid()` y por la edad del ToF**, no
   solo readiness de distancia), `otos_x/y = comm_down_get_pose()` (`otos_fresh` con el
   **gate fino**, no el heartbeat de 500 ms), heading pass-through, dt del scheduler.
   **Agregar `bool heading_valid` a `PoseFusionInputs`:** si es `false` → saltear la
   corrección ToF y degradar confidence (predict y correct están envenenados por el
   mismo heading). **Descartar explícitamente `Pose2D.heading_centideg`** del OTOS (solo
   monitor de divergencia, nunca fuente). `build_snapshot` lee la salida fusionada.
2. **Rotar el ΔOTOS a marco-cancha INSEPARABLE del cableado** (no como P2 posterior):
   `pose_fusion` sin la rotación (#8) integra el delta en el marco del OTOS y deriva en
   dirección equivocada en cuanto el robot gira; encima la corrección ToF tira hacia
   otro marco → estado intermedio incoherente entre dos marcos. El predict y el correct
   deben vivir en el MISMO marco **desde el primer build con el flag ON**. Usar el
   heading **vivo** (centideg crudo, sin el `/100` de `localization`), no un offset de
   boot (§4).
3. **Gate de SEED** antes de anclar: N tofs valid consistentes (mediana 3-5 en radio) o
   consenso con OTOS, no el primer `tof_valid` suelto (§4).
4. **Encadenar `pose_filter`** como 2da red (hold-last-good por oclusión). Decidir
   **UN** gate, no dos (evitar doble-filtrar con el gate de `pose_fusion`), y **bypassear
   el suavizado de heading** de `pose_filter` (un solo filtro de heading, el de
   `imu_fusion`). Resolver la **semántica de hold opuesta**: `pose_fusion` pisa en
   conf=10, `pose_filter` llega a 0 — decidir cuál gana y cuándo la pose deja de
   reportarse `valid` (expiración por age, §5).
5. **Graduar `my_pose_confidence`** (hoy binario 70/0) **reusando `oh_pose_confidence`
   (lado OTOS, ya existe) + la confidence de `pose_fusion` (lado ToF)** — coordinar con
   CENTRAL (semántica del rango). **No construir desde cero.** Documentar que
   confidence-alta ≠ pose-correcta cuando el landmark está corrupto: un REJECT sostenido
   del gate debe **bajar** la confidence, no mantenerla.

   *Banco:* medir deriva OTOS pura en 60 s; robot rotado 90° y trasladando (validar que
   la propagación va en la dirección del BNO, no del OTOS, tras la rotación de frame);
   salto al anclar a ToF; pose tapando una pared con un objeto (gate de oclusión);
   **rival contra la pared AL BOOT** (gate de seed); **tapar el ToF 10 s con OTOS vivo**
   (verificar que la pose deja de reportarse `valid`, no que deriva con conf=10); **cero
   flips espurios del cuantizador con robot quieto cerca del umbral**.

### Fase 3 — Coherencia temporal + arcos (capitalización 2027)

1. **`goal_filter.h`** sobre el bearing de arco + frescura por-arco independiente.
2. **Propagar pelota/heading a `now`** en `build_snapshot` (dead-reckoning + clamp dt).
3. **Campos de velocidad propia** en el snapshot (WIRE-BREAKING, schema bump).
4. **`estimate_world_now()`** — un único paso a 100 Hz que propague CADA slot al mismo
   `now`. Incremental, cada sub-paso detrás de su flag, verde en banco antes del siguiente.

> **Honestidad sobre el esfuerzo.** La pose **no es "reescribir"**: es **cablear y
> tunear** (`pose_fusion`/`pose_filter` ya existen y están host-testeados). El riesgo
> real no es el código — es el **tuning en banco** (K, gates, rotación de frame) y el
> **pre-requisito del heading**. El α-β de pelota y `goal_filter` sí tienen código
> nuevo, pero chico y aislado. Todo detrás de flags default-OFF; integración
> post-Incheon en banco; **no se toca el firmware vivo ahora.**

---

## Resumen (6 líneas)

1. Entre la pizarra y el snapshot falta una capa **estimador**: hoy `build_snapshot` (`main_top.cpp:105-195`) emite el último valor cacheado de cada sensor — fresco en cadencia (100 Hz) pero **viejo en contenido**.
2. El patrón es **predecí@100Hz con OTOS (rápido/deriva) + corregí con ToF/arco (lento/absoluto) + rechazá outliers**; el **heading es la raíz** y se estabiliza PRIMERO.
3. Mucho **YA EXISTE puro y host-testeado pero SIN CABLEAR**: `pose_fusion` (complementario ToF+OTOS), `pose_filter`, `imu_freeze` (gated off), `ball_sticky` (default-off). El trabajo es **cablear + tunear**, no reescribir.
4. Técnica: **complementario 1D** (pose/heading) y **α-β 1D** (pelota), NO Kalman/EKF — auditable por estudiantes, O(1), ya escrito; EKF queda a 2027.
5. Numérico: **float con la FPU del M7** para módulos nuevos (el Q8 crea una **zona muerta** de ±9 mm en la corrección, no un sesgo), dt medido, derivada filtrada, gate de freshness **fino** (~30-50 ms, no el heartbeat de 500 ms), **expiración por age** (anti-free-run, distinta del decay de confidence), y **medir WCET antes de cablear**.
6. Plan gateado off-by-default: Fase 0 estabilizar heading + medir ruido (bloquea pose, con `#error` que ata POSE_FUSION a FREEZE_DETECT), Fase 1 pelota, Fase 2 pose, Fase 3 coherencia temporal — todo en banco post-Incheon, sin tocar el firmware vivo.

---

## Mejoras candidatas (priorizadas)

> **Nota (re-auditoría 2026-06-14):** esta tabla es la **versión 1**. La revisión
> adversarial de 6 lentes (ver `## Review adversarial (resumen)` al final) reordenó
> dependencias y agregó mejoras nuevas (#21-#23). El **backlog autoritativo** es la
> tabla `## Backlog priorizado de mejoras` al final del doc — ordenada por
> impacto/esfuerzo y con las dependencias duras explícitas (la más importante: **#7 NO
> compila sin #1**). Esta v1 se conserva para trazabilidad.

| # | Prioridad | Mejora | Cantidad | Esfuerzo | ¿Cablear o construir? |
|---|---|---|---|---|---|
| 1 | **P0/P1** | Activar y validar `imu_freeze` (detector de BNO congelado) en env `*_freeze` | heading | bajo | cablear (flag) |
| 2 | **P1** | Estabilizar base de heading: primario a 100 Hz (`TOP_BNO_FAST` + `PRIMARY_ONLY`) | heading | bajo | flags |
| 3 | **P1** | Desambiguar doble conteo BNO↔OTOS en CENTRAL (fuente canónica + detector de divergencia) | heading | medio | construir (CENTRAL) |
| 4 | **P1** | Medir ruido de ToF + deriva OTOS + WCET con robot quieto (pre-requisito de todo tuning) | todas | bajo | banco/diag |
| 5 | **P1** | Activar `ball_sticky` por default (fix pelota fantasma) | pelota x,y | bajo | cablear (flag) |
| 6 | **P1** | Calibrar `CAMERA_UNIT_TO_MM` (TASK-022) antes de tunear filtros | pelota, arcos | medio | banco |
| 7 | **P1** | Cablear `pose_fusion` detrás de `-DTOP_ENABLE_POSE_FUSION` (predict ΔOTOS + correct ToF) | pose x,y | medio | cablear + tunear |
| 8 | **P2** | Rotar el ΔOTOS a marco-cancha por heading antes de integrar (riesgo de frame) | pose x,y | medio | construir (chico) |
| 9 | **P2** | α-β 1D por eje para pelota (pos propagada @100Hz + vel sin lag) + gate de innovación | pelota x,y,v | medio | construir (chico) |
| 10 | **P2** | Gate de freshness fino para OTOS (`comm_down_pose_age_ms()`, ~30-50 ms) | pose x,y | bajo | construir (getter) |
| 11 | **P2** | `goal_filter.h` (hold-last + mediana-3 + gate sobre el bearing de arco) | arcos | bajo | construir (chico) |
| 12 | **P2** | Encadenar `pose_filter` tras `pose_fusion` (hold-last-good por oclusión) | pose x,y | bajo | cablear |
| 13 | **P2** | Graduar `my_pose_confidence` (hoy binario 70/0) con la confidence de `pose_fusion` | pose x,y | bajo | cablear (coord. CENTRAL) |
| 14 | **P2** | Filtrar la derivada de los PID (pasa-bajos de 1 polo) | control | bajo | construir |
| 15 | **P2** | Política float-vs-Q8 documentada; migrar la corrección de `pose_fusion` a float | pose x,y | bajo | refactor + re-test |
| 16 | **P2** | Slip detection accionable (gatear corrección con `otos_slip` en patadas/choques) | pose x,y | bajo | construir |
| 17 | **P2** | Campos de velocidad propia en el `WorldSnapshot` (WIRE-BREAKING, schema bump) | vel propia | medio | construir (deploy) |
| 18 | **P2** | Bearing de arco como landmark de heading (complementario, correct lento-absoluto) | heading | alto | construir (post-pose) |
| 19 | **P2** | Atar `lookahead` de `ball_predict` a la latencia medida (no 0.2 s fijo) | pelota | medio | construir |
| 20 | **P2** | `estimate_world_now()` — propagar todos los slots al mismo `now` (coherencia temporal) | todas | alto | construir (incremental) |

---

## Backlog priorizado de mejoras

> **Cómo leer esta tabla.** Es el **backlog autoritativo** post-re-auditoría. Ordenado
> por **relación impacto/esfuerzo** (quick wins arriba). **⚡CABLEAR** = el módulo ya
> existe, es puro y host-testeado; alto valor por bajo esfuerzo, solo conectar +
> tunear. **🔨CONSTRUIR** = código nuevo (chico). Las prioridades P0/P1/P2 siguen el
> frame del coach (P0 bloqueante Incheon · P1 impacto alto en partido · P2 capitalizable
> 2027). **Nada de esto se cablea sin banco; el firmware vivo no se toca ahora.**
> **Dependencia dura:** **#7 (pose_fusion) NO debe compilar sin #1 (imu_freeze)** —
> `#error` de build; #7 incluye #8/#21/#22-seed como parte inseparable.

| # | Mejora | Impacto | Esfuerzo | Riesgo | Pre-requisito | Prio |
|---|---|---|---|---|---|---|
| 5 | **⚡CABLEAR** `ball_sticky` por default (mata la pelota fantasma del promedio front+back) | alto | bajo | bajo (flag, fallback al método actual) | calibrar `CAMERA_UNIT_TO_MM` (#6) para el caso "más cercana" | **P1** |
| 1 | **⚡CABLEAR** + validar `imu_freeze` (BNO "vivo pero clavado") en env `*_freeze` | **alto (raíz)** | bajo | bajo (flag) · falsos-DEAD a vigilar en banco | medir jitter heading quieto (#4) | **P1** |
| 4 | **MEDIR** ruido σ_ToF + deriva OTOS + jitter heading + WCET con robot quieto → **fija K y la histéresis por fórmula** (hoy K=26 es "a ojo") | **alto (habilita todo el tuning)** | bajo | nulo (solo banco/diag) | — | **P1** |
| 10 | **🔨CONSTRUIR** getter `comm_down_pose_age_ms()` + gate de freshness fino OTOS (~30-50 ms, no el heartbeat de 500 ms) | alto (sin esto integra deltas de 500 ms como frescos) | bajo | bajo (getter aislado) | — | **P1** |
| 2 | **⚡FLAGS** primario de heading a 100 Hz (`TOP_BNO_FAST` + `PRIMARY_ONLY`) — baja latencia de la raíz | medio | bajo | bajo (re-habilitar 2º BNO necesita #1 antes) | #1 | **P1** |
| 14 | **🔨CONSTRUIR** filtrar la derivada de los PID (pasa-bajos 1 polo, derivative-on-measurement) — el `kd` amplifica el jitter del BNO ×100, y ese MISMO heading rota la pose | **alto (sube a Fase 0: pre-req de control Y de rotación de pose)** | bajo | bajo | jitter heading medido (#4) | **P1** |
| 13 | **⚡CABLEAR** graduar `my_pose_confidence` (hoy binario 70/0) **reusando `oh_pose_confidence` (otos_health, YA EXISTE) + confidence de pose_fusion** — NO construir | medio | bajo | bajo (coord. semántica con CENTRAL) | #7 para el lado ToF | **P2** |
| 11 | **🔨CONSTRUIR** `goal_filter.h` (hold-last + mediana-3 + gate sobre el bearing de arco) — mata el spike de 1 frame que apunta mal el tiro | medio | bajo | bajo (módulo aislado) | calibrar arcos (#6) | **P2** |
| 6 | **MEDIR/CALIBRAR** `CAMERA_UNIT_TO_MM` (TASK-022, hoy placeholder=10) antes de tunear cualquier filtro de cámara | alto (todo lo de pelota/arco depende de la escala) | medio | bajo | — | **P1** |
| 3 | **🔨CONSTRUIR (CENTRAL)** desambiguar BNO↔OTOS: fuente canónica por FSM + detector de divergencia `\|BNO−OTOS\|>umbral` (insumo del descarte del heading OTOS) | **alto (raíz)** | medio | medio (cross-placa) | #1, #4 | **P1** |
| 7 | **⚡CABLEAR + TUNEAR** `pose_fusion` detrás de `-DTOP_ENABLE_POSE_FUSION` — **incluye INSEPARABLE: rotación ΔOTOS viva (#8), gate de seed (#21), `heading_valid` interlock (#22)**. Pose continua donde la trilateración es invalid (la mayoría del tiempo) | **alto** | medio (cablear) + **alto (tuning en banco)** | **alto: amplifica el error de heading si se enciende mal; temblor directo al bang-bang del arquero** | **#1 (`#error` de build), #4 (K=f(σ)), #3, #10, #14** | **P1** |
| 9 | **🔨CONSTRUIR** α-β 1D por eje pelota (pos propagada @100 Hz + vel sin lag) + **gate de innovación (NO existe hoy)** — **DEBE portar la expiración `max_gap_ms=200` de `ball_velocity`** o regresa la pelota fantasma | medio-alto | medio | medio (si pierde la expiración, pelota fantasma vuela recta) | #6 (escala), #19 (lookahead) | **P2** |
| 8 | **🔨CONSTRUIR** rotar ΔOTOS por la diferencia **VIVA** `(heading_bno−heading_otos)` de cada tick (NO offset de boot) — fundido dentro de #7 | **alto (sin esto la pose deriva en dirección equivocada al girar)** | medio | medio | parte de #7 | **P1** |
| 21 | **🔨CONSTRUIR** gate de SEED: N tofs valid consistentes / consenso OTOS antes de anclar (el origen x,y es la 2ª raíz no gateada) — dentro de #7 | alto (rival contra la pared AL BOOT = mapa rotado silencioso) | bajo | bajo | parte de #7 | **P1** |
| 22 | **🔨CONSTRUIR** `heading_valid` en `PoseFusionInputs` + degradar `tof_valid` cuando un eje se sostiene con 1 sola estimación (`reject_outliers` de-facto OFF en solo-eje-Y) + detección de **sesgo sostenido** (no solo gate de salto) | alto (la corrección ToF NO es heading-independiente) | bajo-medio | bajo | parte de #7 | **P1** |
| 16 | **🔨CONSTRUIR** slip/sesgo accionable: gatear corrección con `otos_slip` y contar ticks de empuje en la misma dirección (firma de heading sesgado / landmark caminado) | medio | bajo | bajo | #7 | **P2** |
| 19 | **🔨CONSTRUIR** atar `lookahead` de `ball_predict` a la latencia medida (no 0.2 s fijo) — un lookahead inflado magnifica el temblor de vel | medio | medio | bajo | latencia medida (#4) | **P2** |
| 12 | **⚡CABLEAR** `pose_filter` tras `pose_fusion` (hold-last por oclusión) — **bypassear su suavizado de heading; decidir UN gate y la semántica de hold (conf→10 vs →0)** | medio | bajo | medio (lag agregado al consumidor de umbral; el gate no compone) | #7 | **P2** |
| 23 | **🔨CONSTRUIR** OBSTÁCULO: hold-last + edad por sensor sobre `min_obstacle_mm` (hoy crudo round-robin ~120 ms) | bajo | bajo | bajo | — | **P2** |
| 24 | **🔨CONSTRUIR** clampear `omega` en `drive_straight_compute` a ±`omega_max_degps` (≤327) antes del `*100` — hoy seguro solo porque el único caller usa `target≈cur` (KICKOFF); con error de 180° hace wrap y gira al revés a fondo (`drive_straight.cpp:28`) | medio (control) | bajo | bajo | — | **P1** |
| 15 | **REFACTOR** política float-vs-Q8 documentada; migrar la corrección de `pose_fusion` a float (elimina la zona muerta de ±9 mm) | medio | bajo | bajo (re-test host) | #7 estable primero | **P2** |
| 18 | **🔨CONSTRUIR** bearing de arco como landmark de heading (correct lento-absoluto) — **única red contra el "heading sesgado por offset de boot"** (escapa a imu_freeze y a heading_valid) | medio-alto | alto | medio | #7, #11 | **P2** |
| 17 | **🔨CONSTRUIR** campos de velocidad propia + **sello de edad** en el `WorldSnapshot` (WIRE-BREAKING, precedente `LineStatusV2.sample_age_ms`); `my_omega` **reusa `heading_rate.h`** | medio | medio | medio (re-flashear TOP+CENTRAL) | coordinar schema con CENTRAL | **P2** |
| 20 | **🔨CONSTRUIR** `estimate_world_now()` — propagar CADA slot al mismo `now` con **expiración por age + clamp dt** (no confundirlos) | alto (coherencia temporal real) | alto | medio | #7, #9, #17 | **P2 / 2027** |

**Quick wins claros (⚡ = solo cablear módulo existente, alto valor / bajo esfuerzo):**
#5 (`ball_sticky`), #1 (`imu_freeze`), #13 (`oh_pose_confidence` ya computa lo que #13
"iba a construir"), #2 (flags de heading). El #4 (medir ruido) no es código pero
**desbloquea todo el tuning** y cuesta una tarde de banco.

**El nudo:** #7 (cablear `pose_fusion`) es el de mayor impacto pero su esfuerzo REAL es
el **tuning en banco** y su riesgo es **alto** — solo baja a aceptable después de
cerrar #1, #4, #3, #14 y con los sub-fixes #8/#21/#22 fundidos adentro. No es un
quick win; es el premio que los quick wins habilitan.

---

## Review adversarial (resumen)

> Re-auditoría 2026-06-14 — 6 lentes adversariales sobre este diseño, cada hallazgo
> verificado contra el código (`archivo:línea`). El doc es **factualmente sólido** (las
> ~15 afirmaciones load-bearing chequeadas se sostienen; ninguna falsa). Los agujeros
> son de **diseño/completitud**, NO de firmware vivo. Lo de severidad ALTA ya está
> corregido arriba (§4, §5, §7, tabla §3, §6).

**Lente 1 — Doble conteo de heading.** *Parcialmente blindado, con un agujero real.* El
doc respeta bien la **puerta**: heading reportado = SIEMPRE BNO, `pose_fusion` no fusiona
(pass-through verificado `pose_fusion.cpp:90`). **Pero hay doble conteo por la VENTANA:**
el ΔOTOS que se integra (`pose_fusion.cpp:128-132`) ya viene rotado por el heading propio
del OTOS (`down/otos.cpp:142`) → al sumarlo crudo se re-importa. El fix original
(offset BNO−OTOS de **boot**) era insuficiente: ambos headings derivan, el offset no es
constante. **Corregido:** des-rotar por el heading_otos **vivo** y re-rotar por el BNO
(§4); regla normativa de que el `Pose2D.heading_centideg` del OTOS solo sirve como monitor
de divergencia; un solo filtro de heading (el de `imu_fusion`). *Severidad: ALTA.*

**Lente 2 — Fusión con heading congelado.** *El pre-requisito era PROSA, no interlock, y
peor: el "ancla absoluta" ToF NO es heading-independiente.* `localization_compute` usa el
heading para asignar paredes (`localization.cpp:122,137,139`) → con heading frozen,
predict (OTOS) **y** correct (ToF) comparten el mismo error: la fusión refuerza el error,
no lo corrige. Y `heading_valid` HOY = "BNO ackea" (`sensors_imu.cpp:498`), no "heading
confiable", porque `imu_freeze` está GATED OFF. **Corregido:** `heading_valid` en
`PoseFusionInputs` que saltea la corrección ToF; gate del `tof_valid` por
`sensors_imu_get_heading_valid()`; `#error` si POSE_FUSION ON con FREEZE_DETECT OFF (§4,
§7); modo de falla aparte "heading sesgado por offset de boot" (lo ataja solo #18).
*Severidad: ALTA.*

**Lente 3 — K-y-temblor.** *El doc diseñaba para suavidad de PLOTEO, pero el único
consumidor vivo es CUANTIZADO* (bang-bang del arquero, deadband 40 mm,
`strategy.cpp:1345-1349`; flip por cruce de umbral `:1585-1617`; motores con bandazo
documentado `:1574-1579`). El enemigo no es la varianza, es el TIEMPO cerca del umbral.
Y el lazo medir→K no cerraba: K=26 es "a ojo", no hay UNA medición de ruido en el
firmware. **Corregido:** regla `K=f(σ)`, histéresis dimensionada por σ en los umbrales de
pose, métrica de cierre cambiada a "cero flips espurios con robot quieto", contabilizar
el lag del encadenado y del lead 0.2 s, derivada de PID a Fase 0 (§5). *Severidad: ALTA.*

**Lente 4 — Outlier-gating.** *Tres agujeros ALTOS:* (1) el **SEED no tiene gate**
(`pose_fusion.cpp:92-123`) → todo el gating es relativo a un origen no validado (rival
contra la pared AL BOOT); (2) la fuente "absoluta" tiene `reject_outliers` **de-facto
apagado** en config solo-eje-Y (`localization.cpp:166-192`); (3) el gate atrapa
**teleports, no envenenamiento lento** sostenido (rival arrimándose <400 mm/tick). Además
el doc **afirmaba** un gate de innovación de pelota que NO existe (`ball_velocity` solo
gatea gap temporal). **Corregido:** gate de seed (#21), degradar `tof_valid` por
eje-único (#22), detección de sesgo sostenido (#16), tabla §3 corregida. *Severidad: ALTA.*

**Lente 5 — Predicción divergente.** *El doc gatea la ENTRADA pero no acota la SALIDA del
dead-reckoning.* `pose_fusion` free-runea sobre el drift OTOS sin ToF con confidence que
pisa en 10 y **nunca llega a invalid** (`pose_fusion.cpp:67,181`) → diverge sin techo
reportando `valid=true`. Y el α-β propuesto, al reemplazar `ball_velocity`, **descartaba
su expiración `max_gap_ms=200`** (regresión a pelota fantasma). El doc confundía "clamp de
dt" (anti-salto puntual) con "expiración por age" (anti-free-run). **Corregido:** regla
explícita de **expiración por age** por-cantidad (pelota ~200 ms, pose-sin-ToF ~500 ms,
heading atado a freeze-detect), distinta del decay y del clamp; α-β debe portar la
expiración (§5, #9). *Severidad: ALTA.*

**Lente 6 — Numérico/overflow.** *El doc SOBREVIVE: ningún overflow ni mal-uso de dt
inducido.* Su tesis de §5 es correcta contra el código. Dos reparos MEDIOS de
completitud: (a) el riesgo estrella `omega*100>32767` **ya está blindado**
(`telemetry_sat.h` + `output_clamp=327` + `omega_degps_to_centideg`) y conviene citarlo
como invariante para `my_omega` (#17); (b) hay UN productor sin clamp,
`drive_straight.cpp:28`, hoy benigno (caller KICKOFF) → nueva mejora **#22-control**.
Dos BAJOS: el truncado Q8 es una **zona muerta de ±9 mm**, no un "sesgo sistemático"
(frase corregida); y `localization.cpp:122` pierde ~1° por `/100` entero antes de
clasificar pared. **Corregido** todo en §5. *Severidad: MEDIA.*

**Lente extra — Completitud.** *Agujeros de cobertura, no de exactitud.* Faltaban dos
módulos de estimación que YA existen, puros y host-testeados: **`otos_health.h`**
(`oh_pose_confidence` graduada 0/60/100 — justo lo que la mejora #13 decía "construir") y
**`heading_rate.h`** (estimador de ω). Faltaba analizar el **obstáculo**
(`min_obstacle_mm`) y señalar que el **`WorldSnapshot` no tiene sello de edad** pese a que
el hermano `LineStatusV2` ya lo tiene (`types.h:153`). **Corregido:** ambos módulos
agregados a §6, #13 reescrita como "reusar", filas de obstáculo (#23) y frescura del
snapshot (#17) agregadas. *Severidad: ALTA (por otos_health).*
