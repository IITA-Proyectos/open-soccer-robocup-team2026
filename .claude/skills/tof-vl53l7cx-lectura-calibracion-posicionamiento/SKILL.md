---
name: tof-vl53l7cx-lectura-calibracion-posicionamiento
description: Usar cuando un sensor ToF multizona VL53L7CX (o el VL53L5CX hermano) "no anda" o hay que leerlo/configurarlo/calibrarlo/usarlo para posicionar el robot de competencia (Teensy/Arduino, I2C) — distancias raras, loop lento por los ToF, el yaw del BNO se congela cuando los ToF rangean, zonas con status inválido, status 0 en TODAS las zonas, el boot tarda muchísimo, o hay que trilaterar / seguir pared con la matriz de distancias. Cubre resolución 4x4/8x8, modo continuo vs autónomo, frecuencia de ranging, filtrado por target_status, recorte de payload (VL53L7CX_DISABLE_*), coexistencia con el BNO055 en el bus, carga del firmware (~84 KB), calibración de xtalk/offset (cover glass), y posicionamiento por paredes. Triggers - "VL53L7CX / VL53L5CX", "ToF multizona / matriz de distancias", "4x4 / 8x8 / zonas / target_status", "el loop cae a 6 Hz por los ToF", "el yaw del BNO se congela con los ToF", "status 0 en todas las zonas", "el ToF tarda en bootear / carga firmware", "modo continuo vs autónomo", "calibrar xtalk / cover glass", "seguir pared / trilateración con ToF", "FoV / 90 grados", "sharpener", "clock stretching del ToF". NO es para fusionar la pose XY (fusion-pose-odometria-landmarks), elegir la técnica de localización (localizacion-rcj-soccer), tunear el lazo de movimiento (control-pid-zona-muerta), el timing del loop en sí (tiempo-real-determinismo) ni el heading del BNO055 (bno055-imu-heading-robocup).
---

# ToF VL53L7CX — lectura, calidad, calibración y posicionamiento por paredes

## Principio central — una distancia vale lo que vale su `target_status`

El VL53L7CX **no devuelve un número: devuelve una MATRIZ** de distancias con un estado de validez
POR ZONA, y carga un firmware de ~84 KB por I2C al boot. La frase ancla:

> **Una distancia del VL53L7CX vale lo que vale su `target_status` — nunca le creas a una zona sin
> filtrarla primero (`nb_target_detected>0` + status válido), y nunca leas los 4 sensores en la
> misma pasada del loop.** El error #1 casi nunca es el silicio: es (a) tratar el bloque de zonas
> como si fuera barato de leer (hunde el loop por I/O bloqueante) o (b) confiar en distancias con
> status 0/6 que ST cuenta como inválidas o ≤50%.

Modelo mental: separá SIEMPRE **tres planos** — **(1) bus/coexistencia** (carga de firmware, clock
I2C, el yaw del BNO que se congela), **(2) calidad del dato por zona** (status, frescura, xtalk a
<60 cm), **(3) uso geométrico** (trilateración/pared). El primero te roba el loop; el segundo te
miente con números plausibles; el tercero recién arma la pose. Diagnosticá en ese orden.

## Cuándo usar / cuándo NO

USAR: leer/configurar/calibrar uno o varios VL53L7CX; el loop del TOP cae a ~6 Hz al leer ToF; el
yaw del BNO se congela mientras los ToF rangean; zonas con status raro o status 0 en TODAS; el
boot tarda muchísimo (carga de firmware); elegir resolución/modo/frecuencia; recortar el payload;
usar la matriz para seguir pared o trilaterar.

NO usar (rutear):
- **FUSIONAR la pose XY** (OTOS + ToF + heading en un estimador) → [[fusion-pose-odometria-landmarks]].
  Esta skill PRODUCE la distancia/pose-por-paredes limpia; cablear el filtro es de la otra.
- **ELEGIR la técnica** (SLAM vs landmarks vs MCL) → [[localizacion-rcj-soccer]] (veredicto:
  cancha conocida → landmarks, no SLAM).
- **El HEADING del BNO055 que se congela** (el chip, el modo, la calib, el flag) →
  [[bno055-imu-heading-robocup]]. Acá tratamos el ToF solo como POSIBLE causante del freeze por bus.
- **Timing del loop en general** (I/O bloqueante/jitter/WCET) → [[tiempo-real-determinismo]];
  **tunear el lazo de movimiento** → [[control-pid-zona-muerta]].

Esta skill termina en "tenés distancias confiables y rápidas".

## Fundamentos del sensor (lo que MANDA: datasheet/UM3038/driver, no la memoria)

Tablas pesadas (16 `target_status`, campos por zona, constantes del API) →
[references/vl53l7cx-datasheet-y-registros.md](references/vl53l7cx-datasheet-y-registros.md).

| Tema | Dato duro | Trampa |
|---|---|---|
| Resolución | 4x4 = 16 zonas → hasta **60 Hz**; 8x8 = 64 zonas → hasta **15 Hz**. Default chip = 4x4 @ 1 Hz | `set_resolution()` ANTES de `set_ranging_frequency()` (el máx depende de la resolución) |
| Por qué 8x8 es ~4× más lento | compone **4 integration times** vs 1 del 4x4; el costo es TIEMPO, no corriente (~50 mA igual) | "consume más" → no, tarda más |
| **FoV** | **90° DIAGONAL (60×60 cuadrado)** | el "~65° diag" es del **VL53L5CX** hermano — confundirlos mal-dimensiona la cobertura de pared |
| Modo | default = **AUTÓNOMO** (VCSEL pulsado, integration time 5 ms SOLO aplica acá); **CONTINUO** = VCSEL siempre on → mejor alcance/inmunidad, habilita 60 Hz | NDOF-equivalente del ToF: elegir mal el modo |
| Boot | `begin()`/`init()` carga **~84 KB** de firmware a la RAM del módulo por I2C | NO hay "boot time" oficial en ms (sin confirmar); el comentario del robot dice "~85 KB" (`sensors_tof.cpp:392-393`) — diferencia menor, no propagar |
| Bus | I2C hasta 1 MHz (Fast mode plus), dir 0x52 | clock stretching: el sensor estira SCL en su ACK (**reportado por comunidad ST, NO está en la tabla de timing del datasheet** — un master bit-banged convive mal) |

⚠️ **Lib en este robot:** `Adafruit_VL53L7CX`, NO la de ST (`STM32duino_VL53L7CX`). La de ST
**desborda el buffer del Wire al cargar el firmware** (`vl53l7cx_platform.h:49-60`,
`DEFAULT_I2C_BUFFER_LEN=256` desborda en 2 bytes); Adafruit usa `maxBufferSize()-2` (reserva el
header) y anda out-of-the-box. Documentado en `src/top/sensors_tof.cpp:9-21`.

⚠️ **El VL53L7CX es pin-to-pin y driver-compatible con el VL53L5CX** (mismo ULD API, misma tabla
de status); casi toda la doc/foros de L5CX aplica — solo cambia el FoV (90° vs 65° diag).

## Validez por zona — el filtro de `target_status` (el corazón del dato)

| status | Significado | Confianza ST | Uso |
|---|---|---|---|
| **5** | Range valid | **100%** | el único que querés para precisión |
| 6 | Wrap around not performed (típico PRIMER frame) | ~50% | mete ruido en los primeros frames |
| 9 | Range valid with large pulse (target mergeado) | ~50% | aceptable como respaldo |
| 0 | Ranging data not updated | inválido | ver TRAMPA abajo |
| resto | (sigma alto, consistency, signal bajo…) | <50% | descartar |

Filtro ST: `nb_target_detected>0` **Y** status ∈ {5,6,9}. Para **precisión** (paredes/trilateración):
quedarse con **5** (y a lo sumo 9), DESCARTAR 6.

- **El robot HOY usa el filtro permisivo `status==5||6||9`** (`sensors_tof.cpp:208-235`,
  `mean_valid_zones`/`fill_zones`). **Tema-a-analizar (no bug):** incluir 6 contamina los primeros
  frames; para trilateración fina conviene endurecer a {5,9}. Decisión del equipo con banco.
- **TRAMPA DIAGNÓSTICA (alta confianza ST):** `status 0` en TODAS las zonas pero con distancias que
  parecen válidas = casi siempre **stack overflow del host** corrompiendo el campo status (está al
  FINAL de la estructura de 1360 bytes), NO un fallo del sensor. Diagnosticá el stack ANTES de
  sospechar del hardware → recortar la estructura con `VL53L7CX_DISABLE_*` (baja a ~648 B) o
  dimensionar el stack.
- **Frescura por sensor (P1-TOF-STALE):** un ToF colgado en un valor viejo (ej. 80 mm) se propagaba
  como `min_obstacle` para SIEMPRE → el robot evadía un fantasma toda la partida. Cura:
  `TOF_STALE_TIMEOUT_MS=250`, expira a `TOF_NO_READING=0xFFFF` (`sensors_tof.h` +
  `tof_fresh_or_no_reading`, pura y wrap-safe; usada en `sensors_tof.cpp:583-621`).

## Coexistencia en el bus y presupuesto del loop (el plano que te roba tiempo)

- **Causa raíz del loop lento = I/O bloqueante:** leer los 4 `getRangingData()` en la MISMA pasada
  hundía el loop del TOP a **~6 Hz** (cada uno trae el bloque grande por Wire a 100 kHz, ~60 ms;
  ~160 ms los 4) → el WorldSnapshot llegaba a ~4 Hz a la CENTRAL. Cura de DOS frentes:
  **(a) round-robin UN ToF por tick** (`sensors_tof.cpp:432-499`, `s_rr` en `:460-463`; con
  `TOP_ENABLE_TOF_SCHED` usa `tof_sched_next` que saltea el caído, `tof_schedule.h:104`) y
  **(b) recortar payload** con `VL53L7CX_DISABLE_*` (`platformio.ini:620-622`, ~5× menos I2C).
  **Lección:** presupuestá el PEOR caso del bus, no el promedio → [[tiempo-real-determinismo]].
- **TRES regímenes de clock I2C** (`sensors_tof.cpp:154-173`): **1 MHz** carga del firmware
  (default producción 2026-06-14, TASK-211, boot ~9,6 s), **400 kHz** fallback de carga (TASK-210),
  **100 kHz** runtime. El `begin()` carga a 1 MHz con fallback a 400 kHz reseteando por LP (`:340-352`).
- **BAND-AID OBLIGATORIO 100 kHz en runtime** (`sensors_tof.cpp:154-167, 377-380`): a >100 kHz el
  read multi-byte del **BNO055 secundario** (que comparte el bus `Wire` con los ToF) se corrompe.
  El `Wire.setClock(TOF_RUN_CLOCK_HZ)` al final del init es OBLIGATORIO; sin él reaparece la
  corrupción. ⚠️ **Honestidad / inconsistencia viva:** el código atribuye a esto el "freeze del
  yaw", pero **la skill [[bno055-imu-heading-robocup]] re-diagnosticó (2026-06-21) que el freeze de
  competencia era el flag `bno_left_en=0` en EEPROM, NO los ToF.** Lo que SÍ es real es la
  corrupción del read multi-byte a 400 kHz en el bus compartido → el 100 kHz sigue siendo
  obligatorio para el BNO **secundario**; pero NO le eches el "heading clavado" a los ToF sin antes
  descartar el flag de config. El fix de fondo fue mover el BNO **primario** a un bus aparte (Wire2).
- **`TOP_TOF_NO_RANGE`** (`sensors_tof.cpp:363-369`, default OFF, TASK-223): enumera/configura los
  ToF SIN `startRanging()` → sin pulsos de VCSEL → aísla si un freeze residual es por el RANGEO
  (acople eléctrico/VCSEL) y no por el bus. **`TOF_ONLY_INDEX`** (`:324-328`) inicializa SOLO un
  ToF — bisección por sensor.
- **Defaults fail-safe byte-neutros:** casi toda mejora (máscara, robust, rotación, continuo,
  scheduler, NO_RANGE) está detrás de un flag OFF que produce binario idéntico al previo.

## Calibración (xtalk / offset) — solo si hay cover glass

- El módulo viene **auto-calibrado de fábrica** y se usa SIN calibración adicional **si NO hay
  vidrio protector** encima. No inventes un paso de calibración que el robot no necesita hoy (no hay
  cover glass montado → xtalk sin calibrar, y está bien).
- **Xtalk (crosstalk)** = señal del VCSEL reflejada dentro del cover glass. SOLO importa a **<~60 cm**
  (más allá el histograma lo cancela) — justo el rango de wall-following cercano. Con cover glass sin
  calibrar, una pared a <60 cm puede dar distancia sesgada. Si se monta vidrio: plugin
  `vl53l7cx_plugin_xtalk`, target de reflectancia conocida cubriendo TODO el FoV a ≥600 mm, antes de
  rangear; guardar el buffer (776 B) y restaurarlo al boot. Procedimiento + buffers →
  [references/vl53l7cx-modos-calibracion-coexistencia.md](references/vl53l7cx-modos-calibracion-coexistencia.md).
- **INCONSISTENCIA datasheet vs driver (sharpener):** el datasheet cita 14% al describir el FoV;
  UM3038/API dicen **5%** (`set_sharpener_percent`). La fuente que MANDA = driver/UM3038: el default
  real es 5%. Subir el sharpener afila bordes pero borra targets secundarios (genera status 12).
- **Alcance útil EN LA CANCHA, no en el lab:** cae de ~3 m (oscuridad) a ~0,5 m con 5 klux (zonas
  internas). Para Incheon: medí `range_sigma_mm` por zona con la LUZ de competencia ANTES de tunear
  cualquier fusión de pose.

## Posicionamiento por paredes (de la matriz a la pose)

Wall tracking y obstacle detection son aplicaciones OFICIALES del datasheet (lista "Robotics: SLAM,
wall tracking, small object detection, cliff prediction"). La matriz da distancias **absolutas**
(independientes del color/reflectancia) por zona.

- **Reducción zona→distancia (PURA, host-testeable):** `tof_zone_masked_mean` promedia zonas
  habilitadas+válidas (`tof_zone_mask.h:51-66`); con `mask=~0` es byte-idéntico al promedio de todas
  las válidas (fail-safe). `tof_zone_masked_robust` (`:88-124`) además descarta rayos > dimensión de
  cancha (fuera/sin retorno) y < % de la **MEDIANA** (rebote en otro robot). Rotación/espejo
  por-sensor en `tof_zone_mask_orient.h:35-73` (grilla GW=4), detrás de `TOP_ENABLE_TOF_ROT`.
- **Convención de ejes/ToF** (`localization.h:20-28,35`): **[0]=FRENTE [1]=ATRÁS [2]=DERECHA
  [3]=IZQUIERDA** (primera persona); `tof_mount_angle_deg={0,180,270,90}`. (El comentario viejo
  decía [2]=izq/[3]=der, invertido — corregido 2026-05-31, ver `localization.h:20-23`.)
- **Trilateración por paredes** (`localization.cpp:96-106, 143-160`): `classify_wall` mapea
  (heading+mount) a N/S/E/W por cuadrantes; `center_perp_distance_mm` aplica radio del robot (F1a) +
  coseno por **LUT Q12 entera** (F1b, sin float → binario idéntico host/target). **Rechazo de
  outliers** (`:163-218`): entre 2 estimaciones del MISMO eje que difieren > umbral, descarta la más
  lejana de la pose anterior — **un robot rival tapando una pared da una lectura corta espuria**, y
  esto la voltea. `pose.valid` solo con ≥1 ToF útil por eje (X e Y).
- **El ARQUERO NO usa la trilateración clásica con heading:** con `TOP_KEEPER_XY_WALLS`
  (`localization_runtime.cpp:36-85`) la pose XY sale **heading-free** de las paredes por
  mediana+recorte (más robusto cuando el BNO no es confiable y el arquero mira al frente).
- ⚠️ **NO tratar como hecho:** la precisión "±2-3 cm" (`localization.h:7`) es expectativa de DISEÑO,
  no medida en cancha (TASK-035 abierta). El `heading_centideg` de salida está firmado
  [-18000,18000] normalizado y NO se consume hoy (`localization.h:54`).

Mapa completo `archivo:línea` del robot →
[references/vl53l7cx-mapeo-modulos-robot.md](references/vl53l7cx-mapeo-modulos-robot.md).

## Árbol de diagnóstico — el ToF "no anda" (deriva la causa, no la asume)

- **FASE 0 — ¿qué síntoma EXACTO?** Distancias raras pero el sensor responde (DATO: status/xtalk/luz)
  ≠ loop lento al leer ToF (BUS/timing) ≠ yaw del BNO congelado (COEXISTENCIA — y antes descartá el
  flag, ver arriba) ≠ status 0 en TODAS las zonas (STACK del host) ≠ boot de 40 s (carga de firmware).
  Cada uno apunta a un plano DISTINTO.
- **FASE 1 — ¿el silicio vive?** ¿`begin()` cargó el firmware (ackea 0x52)? ¿`isDataReady()` da true?
  ¿`getRangingData()` true? Si carga y entrega frames → silicio sano, bifurcá a calidad-del-dato o
  coexistencia. Oráculo: `diag_top_tof_zonemap` (TASK-203).
- **FASE 2 — ¿calidad del dato?** Mirá `nb_target_detected` y `target_status` POR ZONA, no el
  promedio. Status 0 en todas con distancias plausibles → STACK del host. Distancias cortas sesgadas
  a <60 cm con cover glass → xtalk. Alcance que se desploma → `ambient_per_SPAD` alto (luz). Primeros
  frames ruidosos → status 6 colándose.
- **FASE 3 — ¿roba el loop / congela el BNO?** ¿Leés los 4 en la misma pasada? → round-robin. ¿El
  bus quedó a >100 kHz en runtime? → restaurar 100 kHz (`:377-380`). ¿El freeze persiste con
  `TOP_TOF_NO_RANGE` (sin VCSEL)? SÍ persiste → es el bus; DESAPARECE → acople del rangeo (TASK-223).
  Bisección con `TOF_ONLY_INDEX`, scan I2C dual-bus. **Antes de culpar al ToF del heading: descartá
  el flag de config del BNO ([[bno055-imu-heading-robocup]]).**
- **FASE 4 — ¿config?** ¿Resolución ANTES de frecuencia? ¿Quedó en 4x4 @ 1 Hz default? ¿Autónomo
  cuando querías continuo? ¿El sharpener que asumiste (¿14 o 5%?) coincide con el del API (5%)?
- **FASE 5 — gate de verificación EN HARDWARE:** acercá una pared/objeto a distancia conocida y
  confirmá que la distancia reducida que llega al consumidor (snapshot/pose) CAMBIA con el sentido
  correcto. `begin() OK`/compila/"el diag lee" NO lo prueban. **Esta regla la cierra el equipo, no Claude.**

## Inconsistencias doc-vs-código a NO homogeneizar (marcar, no tapar)

- **GRAVE** — `docs/firmware/CONTRATO-DATOS-TOP.md:457,493,523,553` describe los 4 ToF como **STUB**
  ("4 ToF I2C completamente stub", `min_obstacle=0xFFFF` siempre, ref a `sensors_tof.cpp:51-67`). El
  código VIVO usa el driver Adafruit real desde 2026-05-24 con lectura real, round-robin y zonas; el
  archivo tiene ~629 líneas y otra estructura. **La VERDAD MANDA = el código.** (Verificado contra
  ambos: doc y `sensors_tof.cpp`.)
- `docs/CONVENCION-EJES-ROBOT.md:153,158` dice grilla **"8×8"** y corrección del izquierdo
  `(fila,col)→(7-fila,7-col)`. El código usa **4x4=16** (`TOF_RESOLUTION_ZONES=16`,
  `sensors_tof.cpp:151`) → la corrección correcta es **(3-fila,3-col)** y `tof_zone_mask_orient.h`
  opera con GW=4. (El mapeo de POSICIONES [0]frente/[1]atrás/[2]der/[3]izq SÍ coincide.)
- **"Clock stretching" NO está nombrado en el código** (verificado): lo que el código documenta es
  la corrupción del read multi-byte del BNO a >100 kHz (coexistencia/timing), no el clock-stretching
  del L7CX configurado. La skill explica el concepto (es real, comunidad ST) SIN afirmar que el
  firmware lo configura — no lo hace.

## Errores comunes

| Síntoma | Causa raíz real | Trampa (lo que parece) | Fix + verificación |
|---|---|---|---|
| loop del TOP cae a ~6 Hz al leer ToF | 4 `getRangingData()` en la misma pasada (~160 ms I/O) | "el VL53L7CX no sirve para tiempo real" | round-robin 1/tick (`sensors_tof.cpp:432-499`) + payload recortado (`platformio.ini:620-622`); medir loop rate en banco |
| el yaw del BNO se congela cuando los ToF rangean | read multi-byte del BNO secundario corrupto a >100 kHz en bus compartido | "se rompió el BNO / cristal / batería" | restaurar 100 kHz (`:377-380`, obligatorio); **antes descartar el flag `bno_left_en`** ([[bno055-imu-heading-robocup]]); BNO primario a Wire2 |
| status 0 en TODAS las zonas, con distancias plausibles | stack overflow del host corrompe el campo status (al final de 1360 B) | "el sensor falló, cambiarlo" | recortar con `VL53L7CX_DISABLE_*` (→648 B) o agrandar stack; diagnosticar el stack ANTES del HW |
| distancias ruidosas los primeros frames | status 6 ("wrap around not performed") colándose | "el sensor arranca mal" | endurecer filtro a {5} (y a lo sumo 9), descartar 6 (hoy acepta 5\|\|6\|\|9, `:208-235`) |
| pared a <60 cm da distancia sesgada | xtalk del cover glass sin calibrar | "mide mal de cerca / no sirve" | si hay vidrio: calibrar xtalk (buffer 776 B); sin vidrio viene de fábrica, no tocar |
| boot ~40 s con 4 ToF | cada `begin()` carga ~84 KB por I2C (~10 s a 100 kHz) | "el firmware del robot está pesado" | cargar SOLO a 1 MHz con fallback 400 kHz (`:340-352`), restaurar 100 kHz → ~9,6 s; nunca cargar en el loop |
| el sensor corre a 1 Hz / config "ignorada" | default 4x4 @ 1 Hz; o `set_ranging_frequency` antes de `set_resolution` | "no obedece la config" | `set_resolution()` ANTES de `set_ranging_frequency()`; leer la config de vuelta |
| un ToF colgado en 80 mm evade un fantasma toda la partida | sin marca de frescura el último valor bueno se propaga para siempre | "hay un obstáculo real" (falla silenciosa) | frescura por sensor + expirar a `TOF_NO_READING` tras 250 ms (`sensors_tof.h`, `tof_fresh_or_no_reading`) |
| el doc dice "8×8" y la corrección `(7-..)` deja zonas mal | doc `CONVENCION-EJES:153,158` quedó en 8x8; el código usa 4x4 | copiar la fórmula del doc → índices fuera de rango | para 4x4 la corrección es `(3-fila,3-col)`, GW=4 (`tof_zone_mask_orient.h`); marcar la inconsistencia |

**Anti-racionalizaciones:** "el sensor es lento" → no, leer los 4 juntos es I/O bloqueante; round-robin
lo arregla. "devuelve basura, está roto" → status 0 en todas = stack del host, no el sensor.
"los ToF me congelan el heading" → primero descartá el flag `bno_left_en` (el freeze de competencia
fue eso, no los ToF). "el datasheet dice sharpener 14%" → el API/UM3038 dice 5%, manda el driver.
"±2-3 cm de precisión" → es diseño, no medido (TASK-035). "compila / el diag lee" → no prueba que la
distancia llegue bien al consumidor; verificá el EFECTO en hardware.

## Skills relacionadas

- **Fusionar la pose XY** (predict OTOS + correct ToF/heading) → [[fusion-pose-odometria-landmarks]];
  **elegir la técnica** → [[localizacion-rcj-soccer]] (cancha conocida → landmarks).
- **El BNO055** (heading, freeze, el flag `bno_left_en`, la coexistencia desde el lado del IMU) →
  [[bno055-imu-heading-robocup]] (par obligatorio para el caso "ToF congela el BNO").
- **Timing del loop** (I/O bloqueante/jitter/WCET — la raíz del loop a 6 Hz) →
  [[tiempo-real-determinismo]]; **tunear el lazo** → [[control-pid-zona-muerta]]; **la planta** →
  [[dinamica-omni-3-ruedas]].
- Método de debug → `superpowers:systematic-debugging`; verificar antes de cerrar →
  `superpowers:verification-before-completion`. Test en banco → [[hardware-test-protocol]];
  documentar → [[engineering-journal]].

## Referencias (no inflar el inline)

- [references/vl53l7cx-datasheet-y-registros.md](references/vl53l7cx-datasheet-y-registros.md) —
  los 16 `target_status` con su confianza, campos por zona + qué apaga cada `VL53L7CX_DISABLE_*`,
  resolución/frecuencia/FoV/alcance/precisión, constantes del API, e inconsistencias de fuentes
  (sharpener 14% vs 5%, firmware 84 vs 85 KB).
- [references/vl53l7cx-modos-calibracion-coexistencia.md](references/vl53l7cx-modos-calibracion-coexistencia.md)
  — continuo vs autónomo en detalle, power modes, calibración de xtalk/offset paso a paso, la receta
  de coexistencia BNO+ToF del robot, la trampa del stack overflow, clock stretching.
- [references/vl53l7cx-mapeo-modulos-robot.md](references/vl53l7cx-mapeo-modulos-robot.md) — mapa
  `archivo:línea` de los módulos del robot (sensors_tof, localization, máscaras, scheduler), las
  inconsistencias doc y las TASKs abiertas, con lo que está SIN CONFIRMAR marcado.
