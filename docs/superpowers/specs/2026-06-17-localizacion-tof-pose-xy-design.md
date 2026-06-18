---
title: "Diseño: posicionamiento XY confiable con ToF (pose para posicionar, luz para seguridad)"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: spec aprobado en diseño — implementación POST-Incheon, gateada por flags, banco lo cierra el equipo
tipo: spec / design
alcance: TOP (estimación de pose) + app monitor + contrato CENTRAL (semántico)
relacionados:
  - docs/superpowers/specs/2026-05-25-localization-sprint1-trilateration-design.md (trilateración base, vigente)
  - docs/firmware/ESTIMACION-FUSION-TOP.md (capa estimador; algunas partes quedaron desactualizadas, ver §10)
  - docs/CONVENCION-EJES-ROBOT.md (frame canónico de cancha)
  - .claude/skills/localizacion-rcj-soccer + fusion-pose-odometria-landmarks (lentes)
---

# Posicionamiento XY confiable con ToF — spec de diseño

> **Cómo leer este documento.** La **Parte A** (§1–§7) es para IMPLEMENTAR: qué se
> construye, en qué orden, qué archivo se toca. La **Parte B** (§8–§9) es para
> VERIFICAR que se implementó bien: tests host, regresión y criterios de banco por
> fase. La **Parte C** (§10–§12) son inconsistencias a corregir, riesgos y glosario.
>
> **Regla dura de este proyecto:** todo cambio va **detrás de un flag de
> compilación, apagado por defecto**. El binario de competencia (`top_robot2_pri`)
> NO cambia hasta que el equipo valide en banco y promueva el flag. **El testing en
> hardware lo cierra el equipo humano, no Claude** (regla #1 del repo).

---

## 1. Resumen ejecutivo

**Qué.** Dar a los dos robots una **pose XY confiable** estimada con los 4 ToF +
heading, para que el **arquero** se ubique respecto de su línea de fondo sin
depender de tocar la franja a ciegas, y el **delantero** sepa en qué parte de la
cancha está (hoy solo busca pelota y patea).

**Por qué.** En banco: el arquero no se posiciona bien cerca del fondo y el
delantero no tiene noción de su posición. La maquinaria de pose ya existe a medias
pero (a) no corre en el binario de partido y (b) no está protegida contra su falla
raíz (un heading malo rota todo el mapa).

**Enfoque elegido (ver §4).** **Evolucionar la trilateración que ya existe** —no
MCL ni scan-matching— agregándole protección de heading, suavizado/última-pose-buena,
confianza graduada, y cableándola en el camino real del firmware. El mini-lidar por
zonas queda como última fase. **Pose para POSICIONAR; sensores de luz para
SEGURIDAD de borde** (no se sacan hasta medir el error de pose pegado a la pared).

**Alcance.** Estimación de pose en la placa TOP + visualización/calibración en la
app + un cambio **semántico** (no de cableado) en el contrato a CENTRAL.

**Fuera de alcance.** MCL/filtro de partículas, ICP/scan-matching, SLAM, odometría
visual de cámara, y **sacar los sensores de luz** (esto último requiere medición de
banco previa; ver §4 decisión D2). El binario de competencia de Incheon.

---

## 2. Contexto y problema (verificado contra el código)

Estado actual de la pose (camino que CORRE hoy):

1. **Trilateración cruda.** `localization_compute` toma 1 distancia por ToF,
   clasifica cada uno a una pared con el heading (`classify_wall`,
   [localization.cpp:96-106]) y deriva X/Y por álgebra directa. Confianza
   **binaria 70/0**.
2. **El heading entra crudo, sin chequear validez.** `localization_compute` usa
   `heading_deg` sin mirar `heading_valid` ([localization.cpp:122];
   `LocalizationInputs` ni tiene ese campo, [localization.h:24-28]). Un heading
   corrido ~90° **intercambia los ejes X/Y** → pose **coherente pero equivocada**
   (pasa todos los filtros). **Esta es la falla raíz.**
3. **La pose cae a 0 si falta un eje.** `pose.valid` solo es true con ≥1 ToF por
   eje ([localization.cpp:197]); si falta uno, x/y=0 → la CENTRAL lo ve como salto
   a la esquina. No hay "última pose buena".
4. **El único anti-outlier exige 2+ ToF en el mismo eje** ([localization.cpp:166-195]);
   con 1 ToF/eje (caso típico) no filtra nada → un rival tapando la pared o una
   abertura de arco entran directo.
5. **La fusión está escrita pero en código muerto en competencia.** `pose_fusion`
   y `pose_filter` (puros, host-testeados) se cablean **solo dentro de
   `build_snapshot()`** ([main_top.cpp:140-200]); pero el env de competencia
   `top_robot2_pri` define `TOP_ENABLE_SNAPSHOT_TIMER`, y bajo ese flag el snapshot
   lo arma el **emisor por interrupción**, que publica la pose **cruda** y comenta
   explícito *"el estimador pose_fusion no se duplica acá"* ([snapshot_emitter.cpp:95-99]).
   → **Prender el flag de fusión solo NO alcanza**: hay que llevar la pose buena al
   emisor real.
6. **Las 64/16 zonas del VL53L7CX se promedian a 1 número** y se descarta el resto
   ([sensors_tof.cpp:447-449]). Hoy corre a **4×4 = 16 zonas** (no 8×8), refresco
   real **~8 Hz por sensor** (round-robin 1 ToF/tick), bus a **100 kHz** forzado
   porque a más velocidad el BNO se congela (contención I²C ToF↔BNO).

Cómo se traduce en los síntomas:

- **Arquero (R2, sin OTOS):** no tiene XY; llega al fondo retrocediendo a ciegas
  hasta que la placa DOWN reporta contacto con la franja. La detección de la franja
  de fondo falla sobre todo por el **filtro espacial** (descarta blancos reales sin
  vecino-de-índice adyacente; el código lo llama *"el modo de falla del arquero
  sobre la línea de fondo"*), más sensores de luz flojos secundarios. **Nota:** el
  fix de ese modo de falla (`DOWN_EARLY_EVIDENCE`) **ya está activo en
  `down_robot2_rt`** — medir si alcanza antes de invertir más.
- **Delantero (R1, con OTOS):** `attacker_tick` **nunca** lee `my_x/my_y`; navega
  solo por pelota + eje de ataque. Sin XY no puede hacer juego posicional.

---

## 3. Objetivos y criterios de éxito

| # | Objetivo | Criterio medible (cierra el equipo en banco) |
|---|---|---|
| O1 | Pose no miente cuando el heading está mal | Con heading inválido, la pose NO se publica como confiable (no `conf` alto sobre un mapa rotado) |
| O2 | Pose estable, sin saltos a la esquina | Robot quieto: 0 saltos > umbral; al perder un eje, mantiene última-buena con confianza que decae, no x/y=0 |
| O3 | La mejora REALMENTE llega al binario de partido | Con el flag prendido, la pose que ve CENTRAL es la estimada (no la cruda); con el flag apagado, binario byte-idéntico |
| O4 | El arquero se ubica por Y objetivo | Apunta a una Y respecto de su fondo usando `my_y`; la franja pasa a ser confirmación |
| O5 | El delantero conoce su mitad de cancha | `attacker_tick` puede consultar `my_x/my_y` con confianza para conducta posicional |
| O6 | Robustez ante rival/arco (mini-lidar) | Detecta y descarta el rayo que ve el arco o un robot; pose no se envenena (fase final) |
| O7 | "Muy confiable" cerca del borde antes de tocar la seguridad | Error de pose medido pegado a cada pared/esquina < umbral a definir, con rival simulado, ANTES de evaluar quitar la luz |

---

## 4. Decisiones de diseño (tomadas; recomendadas — ajustables en revisión)

- **D1 — Técnica: evolucionar la trilateración directa. NO MCL/ICP.** El mapa es
  conocido y el heading rompe la simetría → la pose es álgebra directa. MCL/ICP dan
  ganancia marginal para 4 sensores en un rectángulo conocido, a cambio de mucho
  cómputo y código no depurable por el equipo. (Si en 2027 hay LIDAR real, se
  reconsidera.)
- **D2 — Pose para POSICIONAR; luz para SEGURIDAD de borde.** No se sacan los
  sensores de luz (out-of-bounds) hasta demostrar en banco error de pose chico
  **pegado a la pared y en esquinas**, con rival simulado. El ToF es más débil
  justo en el borde (incidencia rasante, abertura de arco, rival pegado).
- **D3 — Una sola fuente de verdad de pose.** Un orquestador único (`pose_estimator_runtime`,
  o la propia `localization_runtime` extendida) calcula la pose buena en el loop; el
  emisor por interrupción y `build_snapshot` la **leen** del mismo getter. Nada de
  duplicar el estado de fusión en dos lugares.
- **D4 — R1 y R2 corren el mismo código.** La fusión degenera sola a "ToF +
  suavizado + última-buena" cuando no hay OTOS (caso permanente de R2). No se
  bifurca por robot.
- **D5 — Rotación/flip de zonas: plegar en la máscara por ahora.** La app ya pliega
  la rotación dentro de la máscara de zonas (cruda). El campo separado `zone_rotation_deg`/
  `flip` (que YA se guarda en EEPROM pero NO se aplica) solo se justifica cuando se
  exponga distancia por-rayo (mini-lidar, F5). Hasta entonces NO se agregan comandos
  `TOF n ROT/FLIP` al firmware; se documenta `zone_rotation_deg`/`flip` como
  reservado-para-F5. (Resuelve la "doble verdad" app vs firmware — ver §10.)
- **D6 — Medir antes de tunear.** Ninguna ganancia/umbral se fija "a ojo": F0
  (banco) entrega los números (ruido por ToF/zona, drift del BNO, geometría real)
  que siembran las configuraciones. Las que hay hoy (K, gates de 400 mm) son
  provisorias.

> Estas 6 decisiones se pueden revisar. Si alguna cambia, se actualiza el spec
> antes de implementar.

---

## 5. Arquitectura

```
                 ┌─────────────────────────── placa TOP (loop ~ varios kHz) ───────────────────────────┐
  4× ToF ──┐     │  localization_runtime_tick()  →  localization_compute()  ── pose cruda + heading_valid │
  BNO  ────┼───► │           │                                                                            │
  OTOS ◄───┼─DOWN│           ▼                                                                            │
 (solo R1)│     │   pose_estimator_tick()  [F3]                                                          │
          │     │     predice con OTOS (R1) / corrige con ToF / gatea heading / hold-last-good           │
          │     │           │                                                                            │
          │     │           ▼   getter único: pose_estimator_get()  →  {x, y, heading, conf 0-100, valid}│
          │     │     ┌──────┴───────────────────────────┐                                              │
          │     │     ▼                                   ▼                                              │
          │     │  emisor por interrupción            build_snapshot()  (#else / respaldo)               │
          │     │  (camino de PARTIDO)  [F3 clave]                                                       │
          │     └───────────┬──────────────────────────────────────────────────────────────────────────┘
          │                 │ WorldSnapshot (my_x/y/heading/conf)  — contrato SIN cambio de wire
          ▼                 ▼
        DOWN              CENTRAL (FSM arquero/delantero consume my_x/y/conf)
```

**Contrato a CENTRAL — sin cambio de cableado.** `WorldSnapshot` ya tiene
`my_x_mm`, `my_y_mm`, `my_heading_centideg`, `my_pose_confidence` ([types.h:98-103])
y `heading_valid` (bit del flag). **El único cambio es semántico:** `my_pose_confidence`
pasa de **binaria {0,70}** a **graduada 0-100**. Esto obliga a revisar cómo la
CENTRAL la consume hoy (¿compara `==70` o usa umbral?) y coordinarlo (ver §4 nada
cambia en el wire, pero sí en la interpretación → decisión abierta DA-1, §11).

**Restricción de la interrupción (invariante a respetar).** El emisor por
interrupción NO puede hacer cálculo pesado (float/trigonometría/bus). La estimación
corre **en el loop** (un solo escritor) y el emisor solo **lee** el struct cacheado.
Esto preserva el contrato seqlock existente.

---

## 6. Las capas de confiabilidad (qué hace cada una)

Ordenadas por confiabilidad ganada por hora de trabajo:

- **C0 — Gate de heading.** No trilaterar si `heading_valid` es false → no anclar un
  mapa rotado. Es la raíz; sin esto el resto se construye sobre arena.
- **C1 — Última-pose-buena + confianza graduada.** Reemplaza el corte duro
  `valid→x/y=0` por sostener la última pose buena con confianza que decae; y la
  confianza 70/0 por un 0-100 (cantidad de ToF consistentes + edad de la corrección
  + consenso). Lo hace `pose_filter` (ya escrito).
- **C2 — Consenso contra el rectángulo conocido.** `frente+atrás ≈ largo` y
  `izq+der ≈ ancho`; votación 3-vs-1 para descartar el ToF disidente **aun con 1 ToF
  por eje**. Barato, sin I²C extra; cubre el agujero del anti-outlier actual.
- **C3 — Filtro temporal.** Gate de salto + suavizado como red final contra el ToF
  falso puntual (`pose_filter`).
- **C4 — Cableado en el camino real.** Llevar C0-C3 al emisor por interrupción
  (camino de partido). Sin esto, todo lo anterior es inerte en un partido.
- **C5 — Mini-lidar por zonas.** Usar las zonas del VL53L7CX para distinguir
  **pared plana** (gradiente suave entre zonas) de **robot** (escalón local) y de
  **abertura de arco** (zonas centrales mucho más lejanas), y quedarse con las zonas
  que ven pared real en vez de promediar a ciegas. Es la mayor robustez, pero la más
  cara: requiere el mapeo zona→ángulo (no existe) y geometría medida (F0).

---

## 7. Plan de implementación por fases (Parte A)

> Cada fase: **objetivo · cambios de diseño (archivo) · flag nuevo · qué NO cambia**.
> La verificación de cada fase está en §8 (no repetir acá). Confirmar durante la
> implementación los detalles marcados "(confirmar)".

### F0 — Banco de mediciones (PRE-requisito de todo; no toca código de robot)

**Objetivo:** obtener los números que siembran todas las configuraciones. Sin esto,
cualquier ganancia/umbral es adivinanza (D6).

**Entregables (los produce el equipo en banco):**
- σ (ruido) y bias de cada ToF, por zona y por incidencia, a 3-4 distancias.
- `TOF_OFFSET_MM` real (plano del sensor → centro) por robot, con calibre. Hoy es
  placeholder ([pinout_common.h:121]).
- Offset/altura/tilt reales de los 4 ToF ([robot_geometry.h:46-51], hoy aprox).
- Drift del BNO (°/min, motores ON vs OFF) y validación de que `heading_valid`
  **cae** cuando el BNO se congela (robot quieto 3-5 min sin falso-CONGELADO).
- Signo de giro del BNO (CCW+ vs CW+) y signo del eje X de la cámara (TASK-202).
- Comportamiento del ToF mirando la **boca del arco** y con un **rival pegado** a la
  pared (para diseñar el rechazo de C2/C5).

**Herramientas de app a construir (módulos puros + paneles):**
- Captura de ruido: robot quieto N≥500 muestras → σ por ToF/zona, export.
- Ground-truth: marcar posiciones con cinta (centro, esquinas, frente a cada arco),
  registrar la pose reportada y computar error XY (media/máx). Reusa
  `central_field_geometry.py` + el grabador existente.

### F1 — Gate de heading en la trilateración (la raíz)

- `localization.h`: agregar `bool heading_valid` a `LocalizationInputs`.
- `localization.cpp` (`localization_compute`): si `!heading_valid` → devolver
  `valid=false` **sin** clasificar paredes, con una causa distinta a "sin ToF".
- `localization_runtime.cpp`: pasar `sensors_imu_get_heading_valid()` (mismo getter
  que ya usa `main_top.cpp`). Revisar la política de `prev_valid` (hoy no se baja
  nunca → un mal heading puede dejar un prev viejo anclando el anti-outlier).
- **Flag:** `TOP_ENABLE_LOC_HEADING_GATE` (apagado por defecto).
- **No cambia:** con el flag apagado, `localization_compute` se comporta igual
  (byte-idéntico).

### F2 — Última-pose-buena + confianza graduada

- Cablear `pose_filter` (hold-last-good + gate de salto + confianza que decae) sobre
  la salida de `localization_compute`, **dentro de `localization_runtime`** (antes
  de cachear la pose que devuelve `localization_runtime_get_pose()`), de modo que
  **tanto el emisor por interrupción como `build_snapshot` consuman la pose ya
  filtrada** sin duplicar. (confirmar: que el emisor lea efectivamente
  `localization_runtime_get_pose()`; si no, ver F3.)
- Reemplazar la confianza binaria 70/0 por la graduada que provee el filtro.
- **Flag:** `TOP_ENABLE_POSE_FILTER` (apagado por defecto).
- **No cambia:** con el flag apagado, se publica la pose cruda y la confianza 70/0
  de hoy.

### F3 — Fusión en el camino real (orquestador único; suma OTOS en R1)

- **Módulo nuevo** `src/top/pose_estimator_runtime.{h,cpp}` (glue, sin lógica
  nueva): dueño de `PoseFusionState` + `PoseFilterState` + sus configuraciones.
  - `pose_estimator_init()`.
  - `pose_estimator_tick()` arma las entradas (pose ToF de `localization_runtime`;
    OTOS de `comm_down`; frescura fina del OTOS; heading y `heading_valid` del BNO),
    llama `pose_fusion_update` (+ `pose_filter`), y cachea `{x,y,heading,conf,valid}`.
  - `pose_estimator_get()`.
- `main_top.cpp` loop: llamar `pose_estimator_tick()` (al lado de
  `localization_runtime_tick()`). **El cálculo corre en el loop, nunca en la
  interrupción.**
- **Cambio clave (lo que hace que entre al partido):** en el emisor por interrupción
  ([snapshot_emitter.cpp:95-99]), publicar `pose_estimator_get()` en vez de la pose
  cruda; `conf` = la graduada.
- `build_snapshot`: leer la misma fuente (`pose_estimator_get()`), eliminando la
  duplicación de estado de fusión.
- R1 (con OTOS): la rama predict/correct queda activa. R2 (sin OTOS): el predict no
  corre (degenera a ToF + filtro) con el MISMO código (D4).
- **Flag:** reusar `TOP_ENABLE_POSE_FUSION` (ya existe el interlock `#error` que exige
  el detector de freeze). El env de banco `top_robot2_pri_posefusion`/
  `top_robot1_pri_posefusion` ya existe — confirmar que con este cambio realmente
  surte efecto en el camino real.
- **Banco antes de confiar:** signo/eje del delta OTOS vs marco de cancha; que
  `heading_valid` no dé falsos; que el detector de freeze no dé falso-CONGELADO.
- **No cambia:** con `TOP_ENABLE_POSE_FUSION` apagado (competencia), byte-idéntico.

### F4 — Consenso entre ejes contra el rectángulo (robustez sin zonas)

- Extender `reject_outliers` ([localization.cpp:166-195]) de "consistencia entre ToF
  del mismo eje" a: chequeo `frente+atrás ≈ largo` / `izq+der ≈ ancho` (dimensiones
  conocidas) + votación 3-vs-1, que filtra **con 1 ToF por eje**. Emitir confianza
  según votos consistentes.
- **Flag:** `TOP_ENABLE_LOC_RECT_CONSENSUS` (apagado por defecto). Usa `TOF_OFFSET_MM`
  y diámetro reales (de F0).
- **No cambia:** con el flag apagado, el anti-outlier actual.

### F5 — Mini-lidar por zonas (mayor robustez; objetivo 2027)

- Escribir el **mapeo zona→azimut/elevación** que falta ([tof_zone_orient.h:18-20]),
  apoyado en la geometría MEDIDA en F0.
- En `sensors_tof.cpp`: **separar la selección de zona de la reducción** — clasificar
  pared (gradiente suave) / robot (escalón local) / arco (hueco central), quedarse con
  las zonas de pared real (mediana), en vez de `tof_zone_masked_mean` a ciegas.
- Decisión 8×8 (64 zonas) vs 4×4 (16): depende del **presupuesto I²C/loop MEDIDO**
  (bus a 100 kHz por la contención con el BNO; el loop ya se estranguló antes).
  Empezar con 4×4 (las 16 zonas ya viajan crudas a la app).
- **Flag:** `TOP_ENABLE_TOF_ZONE_SELECT` (apagado por defecto).
- Recién acá tendría sentido aplicar `zone_rotation_deg`/`flip` en firmware (D5).

### Cambios en la app (transversales, acompañan F1-F5)

- `panel_central_field.py`: mostrar la confianza graduada (atenuar/colorear el
  símbolo según 0-100) y la fuente (ToF / OTOS / última-buena) en vez del binario
  "anclada / no anclada".
- Herramienta de **ground-truth** (F0) y panel de **captura de ruido/geometría**.
- `panel_tof_setup.py` / `tof_layout.py`: alinear con D5 (rot/flip plegado en
  máscara hasta F5; no marcarlos como comando de firmware).

---

## 8. Verificación de la implementación (Parte B)

> Cómo saber, por fase, que se implementó **correctamente**. Tres niveles:
> **(H) host** = lo puede correr cualquiera con `scripts/run-host-tests.sh`;
> **(R) regresión** = el binario de competencia no cambió; **(B) banco** = lo cierra
> el equipo humano con hardware (regla #1). Una fase NO está "lista" hasta pasar H+R;
> "validada" recién tras B.

### Regresión común a TODAS las fases (R)

- **(R1)** Con todos los flags nuevos apagados, los envs de competencia
  (`top_robot2_pri`, `top_robot1_pri*`) **compilan** y el binario es **byte-idéntico**
  al previo. Verificación concreta: compilar antes y después del cambio con los flags
  apagados y comparar el `.hex`/tamaño de FLASH; deben coincidir. (Es el invariante
  "competencia no se toca".)
- **(R2)** El gate host completo sigue verde: `scripts/run-host-tests.sh` sin
  regresiones (conteo de tests ≥ el previo, 0 fallos).

### F0 — Banco

- **(B)** Existen, versionados, los números: σ por ToF/zona, `TOF_OFFSET_MM` real,
  offset/altura/tilt, drift del BNO, signos BNO/cámara. Sin estos, F2-F5 **no se
  tunean** (criterio de bloqueo, no de aprobación).
- **(B)** Demostrado en banco que `heading_valid` **cae** ante BNO congelado (quieto
  3-5 min) y NO da falso-CONGELADO. (Habilita F1/F3.)
- **(H)** Las herramientas nuevas de la app (captura de ruido, ground-truth) tienen
  su test de humo en `tools/monitor-base/tests/`.

### F1 — Gate de heading

- **(H)** `test_localization` extendido: con `heading_valid=false`, `localization_compute`
  devuelve `valid=false` y NO clasifica paredes; con `heading_valid=true`, idéntico al
  comportamiento previo. Caso de causa-de-invalidez distinta a "sin ToF".
- **(R)** Flag apagado → `localization_compute` byte-equivalente (R1/R2 arriba).
- **(B)** Con el flag prendido en banco: girar el robot con el BNO sano → la pose
  trackea; forzar/simular heading congelado → la pose pasa a no-confiable (no publica
  una pose rotada con confianza alta).

### F2 — Última-buena + confianza graduada

- **(H)** `test_pose_filter` (existe) cubre hold-last-good + gate de salto + decay;
  agregar el test de integración localization_runtime→filtro (al perder un eje,
  sostiene última-buena con confianza que decae, NO x/y=0).
- **(H)** Test de la fórmula de confianza graduada (mapea #ToF/edad/consenso a 0-100
  de forma monótona y acotada).
- **(R)** Flag apagado → se publica pose cruda + conf 70/0 (byte-idéntico).
- **(B)** Robot quieto: 0 saltos > umbral en 60 s; al tapar un ToF, la pose no salta
  a la esquina.

### F3 — Fusión en el camino real

- **(H)** `test_pose_fusion` (existe) verde; test del orquestador
  `pose_estimator_runtime` (arma entradas, R2 sin OTOS degenera a ToF+filtro, R1
  predice+corrige).
- **(R)** `TOP_ENABLE_POSE_FUSION` apagado → competencia byte-idéntica.
- **(B) — la verificación que importa:** con el flag prendido, **la pose que llega a
  CENTRAL es la estimada, no la cruda** (confirmar en el monitor que `conf` es
  graduada y que la pose se mueve suave); signo/eje del delta OTOS correcto (R1 no se
  va en diagonal al avanzar); sin saltos. **Métrica de cierre del consumidor:** con la
  pose alimentando al arquero, **cero flips espurios** del bang-bang con el robot
  quieto cerca del umbral (no "se ve suave al plotear").

### F4 — Consenso entre ejes

- **(H)** `test_localization` con casos: 1 ToF/eje + un disidente que rompe la suma
  de pares → se descarta el disidente; confianza baja con menos votos.
- **(R)** Flag apagado → anti-outlier actual.
- **(B)** Rival simulado tapando una pared (1 ToF/eje): la pose NO se envenena;
  `conf` refleja la pérdida de votos.

### F5 — Mini-lidar por zonas

- **(H)** Test del mapeo zona→ángulo (geometría de F0) y del clasificador
  pared/robot/arco con grillas sintéticas (gradiente vs escalón vs hueco).
- **(R)** Flag apagado → `tof_zone_masked_mean` a ciegas (byte-idéntico).
- **(B)** Presupuesto I²C/loop medido con la resolución elegida (el loop no se
  estrangula); frente a la boca del arco, el sensor se descarta para ese eje; frente
  a un rival, se vetan las zonas del escalón.

### Verificación del objetivo O7 (gate para tocar la seguridad de borde)

- **(B)** Solo después de F1-F4 validadas: medir el error de pose **pegado a cada
  pared y en esquinas**, con rival simulado, contra cinta. **Recién con ese número**
  se decide (decisión humana) si la pose puede reemplazar a la luz como seguridad de
  out-of-bounds. Hasta entonces, D2 manda: la luz se queda.

---

## 9. Orden crítico (no saltear)

```
heading confiable (F0 anti-freeze)  →  medir ruido/geometría (F0)  →  gate de heading (F1)
   →  última-buena + confianza (F2)  →  fusión en camino real (F3)  →  consenso (F4)
   →  [validar borde]  →  decidir luz (O7)  →  mini-lidar (F5, 2027)
```

**No tunear ningún filtro antes de medir (F0). No sacar la luz antes de O7.**

---

## 10. Inconsistencias a corregir (gana el código) — §FUENTES-DE-VERDAD

Al tocar estos temas, corregir el doc en el **mismo commit** (regla del repo):

1. **rot/flip — doble verdad.** `top_config` ya **serializa** `zone_rotation_deg`/`flip`
   en EEPROM, pero el firmware **no los aplica** (solo un diag) y la app los **pliega
   en la máscara** marcándolos "informativos". Resolución (D5): hasta F5, documentar
   `zone_rotation_deg`/`flip` como **reservado-para-F5**; NO agregar comandos
   `TOF n ROT/FLIP`. (Corrige la premisa del pedido "guardar rotación": ya se guarda;
   lo que falta es aplicarla, y eso recién sirve con el mini-lidar.)
2. **FOV del ToF.** `FIRMWARE-PLACA-ARRIBA.md` se contradice (90° vs ~70°). El real
   de ST es ~45°/eje (~63° diagonal) en la variante estándar. Corregir contra el
   datasheet.
3. **Tasa del ToF.** El doc dice "4×4 a 30 Hz"; el código configura 15 Hz interno y
   el refresco real es ~8 Hz/sensor por round-robin. Gana el código.
4. **Estado del detector de freeze (aclaración, no error del código).** Verificado
   directo: `top_robot2_pri` **incluye** `TOP_ENABLE_BNO_FREEZE_DETECT`
   ([platformio.ini:388]) → detecta el valor *clavado* y baja `heading_valid`. Lo que
   **no** cubre es la **deriva lenta**, y la trilateración cruda igual ignora
   `heading_valid` (eso lo arregla F1). (Un análisis interno lo dio por apagado;
   estaba desactualizado.)
5. **ESTIMACION-FUSION-TOP.md** asume el cableado en `build_snapshot`; quedó
   desactualizado porque competencia adoptó el emisor por interrupción. Actualizar
   con el camino real (F3).

---

## 11. Riesgos (formato coach)

- **Heading corrido (no congelado) — P0.** `risk-no-fix`: pose coherente-pero-falsa
  todo el partido; R2 sin OTOS es el caso crítico. `risk-fix`: F1 cubre el freeze, NO
  la deriva → hace falta una fuente de heading de respaldo (decisión abierta DA-2).
  `tiempo`: F1 chico; respaldo de heading = diseño aparte.
- **Doble pasa-bajos en serie (fusión + filtro) — P1.** Lag que hace al arquero
  sobrepasar la línea del arco; el gate de salto NO compone (dos saltos sub-umbral =
  teleport). `risk-fix`: elegir UN suavizador / UN gate sobre la salida; medir el lag
  en banco. `tiempo`: tuning de banco.
- **Envenenamiento lento por rival que se arrima — P1.** El gate de salto atrapa el
  teleport de 1 frame, no el rival que empuja la pose de a 50-100 mm/tick. `risk-fix`:
  contar ticks de corrección en la misma dirección y desconfiar del landmark (parte de
  C2/C5). `tiempo`: diseño + banco.
- **Presupuesto I²C al subir a 8×8 — P1.** `risk-no-fix` (de subir): re-estrangular el
  loop. `risk-fix`: medir antes; quedarse en 4×4 si alcanza. `tiempo`: medición F0.
- **Cambiar la confianza a 0-100 rompe a CENTRAL si compara `==70` — P1.** `risk-fix`:
  revisar el consumidor y coordinar el umbral antes de cablear (DA-1). `tiempo`: chico.
- **Sacar la luz sin pose validada en el borde — P0 (evitado por D2).** `risk-no-fix`:
  salir de cancha = penalidad/descalificación. Mitigación: D2 + O7.

---

## 12. Decisiones abiertas (para el equipo / banco)

- **DA-1.** Semántica de `my_pose_confidence` graduada: ¿cómo la consume CENTRAL y a
  partir de qué umbral confía la FSM? Coordinar con la placa CENTRAL antes de F2/F3.
- **DA-2.** Fuente de heading de respaldo cuando el BNO deriva (no solo congela):
  ¿heading del OTOS (R1)? ¿bearing al arco por cámara? Es la raíz; sin plan B la pose
  miente cuando el BNO deriva.
- **DA-3.** R2 sin OTOS: ¿con qué se ancla la pose cerca del borde si el ToF no
  alcanza? (más anclas ToF / bearing de arco por cámara / mini-lidar).
- **DA-4.** Resolución ToF 4×4 vs 8×8: decidir con el presupuesto I²C medido (F0).
- **DA-5.** O7: el umbral de error de pose en el borde que habilitaría (algún día)
  quitar la luz. Lo fija el equipo con datos.

---

## 13. Glosario (términos claros, sin jerga ambigua)

- **Detrás de un flag de compilación, apagado por defecto:** el código existe pero no
  se compila/activa salvo que se defina su `-D...`; con el flag apagado el binario se
  comporta igual que hoy. (NO es "hardcodeado" = valor fijo en el código.)
- **Byte-idéntico:** el binario compilado no cambió ni un byte → la conducta de
  competencia es exactamente la de antes.
- **Trilateración:** calcular la posición a partir de distancias a referencias
  conocidas (acá, las 4 paredes).
- **Pose:** posición + orientación del robot (x, y, heading).
- **Última-pose-buena (hold-last-good):** sostener la última posición confiable
  cuando la medición nueva no sirve, en vez de saltar a cero.
- **Emisor por interrupción:** la rutina que arma y manda el `WorldSnapshot` desde una
  interrupción de temporizador (el camino de partido hoy).
- **Mini-lidar:** usar las muchas zonas de un ToF multizona como rayos a distintos
  ángulos (en vez de promediarlas a una sola distancia).
```
