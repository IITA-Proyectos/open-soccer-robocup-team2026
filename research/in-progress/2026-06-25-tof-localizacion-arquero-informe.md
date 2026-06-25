---
title: "INFORME — Localización del arquero por paredes ToF: 5 lecturas de banco, alcance, y fórmula de ajuste con el BNO"
date: 2026-06-25
status: in-progress (lecturas hechas y consistentes; fórmula con BNO PROPUESTA, validación en banco PENDIENTE)
placa: TOP (ROBOT2)
env_medicion: top_robot2_pri_tofmaxrange (4×4 + 2 Hz + continuo + settle BNO 3 s)
autor: "Claude Opus 4.8 (1M context) + Virginia (placa/posiciones), vía Claude Code"
testeado-en-hardware: lecturas SÍ (serie real); el cableado a pose/heading lo cierra el equipo (regla 1)
construye_sobre:
  - src/shared/keeper_xy_walls.h        # estimador XY heading-free YA EXISTENTE (este informe lo extiende)
  - research/in-progress/2026-05-25-localizacion-tof-imu-analisis.md
  - docs/CONVENCION-EJES-ROBOT.md
  - journal/2026-06-25-tof-localizacion-paredes-4-posiciones-validacion.md   # data cruda de las 4 estáticas
herramientas:
  - tools/monitor-base/tof_zonas_promedio.py     # promedia las 16 zonas/ToF + los 2 BNO
  - tools/monitor-base/probe_heading_diag.py     # vuelca todos los campos de heading
tareas: TASK-227 (validar localización con BNO en R2), TASK-228 (calibrar zonas en R1)
---

# Localización del arquero por paredes ToF — informe de banco + fórmula con BNO

## 0. Resumen ejecutivo

- **Se puede localizar al arquero solo con los 4 ToF.** El eje **X (ancho) es sólido y repetible**;
  el eje **Y (largo) es bueno cerca de los fondos** (régimen del arquero) y marginal en el medio campo.
- **Alcance real con pared NEGRA ≈ 1,4–1,5 m** (medido). Coincide con el `wall_reach ≈ 1000` que ya usa
  [`keeper_xy_walls.h`](../../software/teensy/Soccer%202026/src/shared/keeper_xy_walls.h) (conservador).
- **El módulo `keeper_xy_walls.h` YA hace la pose por paredes**, pero **heading-free**: vale solo si el
  arquero mira al frente; **bajo rotación da x/y coherentes pero MENTIROSOS** (lo dice el propio módulo,
  líneas 11–14). **Este informe propone la extensión rotación-aware usando el BNO.**
- **El BNO primario (Wire2) anda perfecto**: lectura estable (std 0) y magnitud/signo correctos. Pero su
  **"cero" es relativo al encendido** (modo IMU, sin magnetómetro), y el heading del WorldSnapshot **no
  aplica ningún offset de cancha** → hay que **anclar el cero mirando al arco rival** (ver §4).

---

## 1. Método

Robot QUIETO en una posición CONOCIDA; se leyó la telemetría por COM15 ~70 s/posición y se **promediaron
las 16 zonas crudas de cada ToF** (`tof_zonas_promedio.py`). Las zonas son ruidosas frame a frame pero
**se estabilizan muy bien al promediar** (desvío 1–10 % en las franjas buenas). Env de medición
`top_robot2_pri_tofmaxrange` = 4×4 (16 zonas) + ranging a 2 Hz + modo continuo — la config que **ve las
paredes negras** (el negro absorbe el IR; ver journal 2026-06-23). Orden de ToF (igual que
`keeper_xy_walls.h` y `localization.h`): **[0] FRENTE, [1] ATRÁS, [2] DERECHA, [3] IZQUIERDA**.

Convención de cancha ([`docs/CONVENCION-EJES-ROBOT.md`](../../docs/CONVENCION-EJES-ROBOT.md)): origen en la
esquina propia izquierda; **+X = derecha** (lado corto, ancho 1820 mm); **+Y = arco rival** (lado largo,
2430 mm); pared propia en y=0. **Heading 0 = mirando al arco rival; +heading = giro CCW = a la izquierda;
−heading = CW = a la derecha.** `tof_offset` (plano del sensor → centro del robot) ≈ **95 mm**.

---

## 2. Las 5 lecturas

Para cada ToF, la **"franja perpendicular"** (la fila/columna que apunta horizontal y mide la pared; las de
abajo ven piso —cortas y súper estables—, las de arriba el cielo —vacías—). Valores en mm, promediados.

### 2.1 Cuatro posiciones ESTÁTICAS (sin rotación, arquero mirando al frente)

| Posición | LEFT (col2) | RIGHT (col2) | FRONT (fila2) | BACK | Lectura |
|---|---|---|---|---|---|
| **Centro de cancha** | 874 | 760 | 1131 (pared) | 1377 (hueco) | L+R+robot ≈ 1804 ≈ ancho ✓; FRONT mide la pared frente (1131 ≈ 1130) ✓ |
| **Arco, centrado en X** | 857 | 700 | 670 (piso) | 1000 (hueco) | laterales = centro ✓; FRONT fuera de alcance (pared rival a ~2,3 m); BACK ve el **hueco** de la portería |
| **Arco, corrido a la izq** | 533 ↓ | ~1050 ↑ | 680 (piso) | 640 (maciza) | L bajó / R subió (movió en X) ✓; BACK macizo = 640 (no 1000) → **hueco confirmado** |
| **Corner izquierdo** | ~250 (toda la matriz) | NO ve (1735) | piso | ~250 (toda la matriz) | esquina nítida en las 2 paredes cercanas; **RIGHT a 1735 mm NO se ve** |

### 2.2 Lectura con ROTACIÓN (45° a la derecha, sobre línea de fondo, centrado en X)

Con `probe_heading_diag.py` se leyeron el heading + las zonas a la vez. **El BNO funciona y es estable:**

```
robot a 45° a la DERECHA, QUIETO:
  imu.heading_deg (fusionado)   = -45.3°  (std 0.0)   ← magnitud 45° ✓, signo: derecha = negativo ✓
  imu.left_deg    (BNO primario)= -45.3°              ← el primario (Wire2) sano
  imu.right_deg                 = inválido (PRIMARY_ONLY, esperado)
  snap.my_heading_deg           = -45.3°              ← IDÉNTICO al crudo del BNO (sin offset de cancha)
  base.pose_heading_deg         = 0.0    (OTOS/DOWN no aporta)
```

Zonas crudas (45°): las paredes se "corren" a posiciones intermedias de cada matriz (ya no caen en la franja
del centro), exactamente lo que predice la rotación. **Sin el heading no se pueden seleccionar las zonas
perpendiculares automáticamente; con el heading, sí** (esa es la fórmula de §3).

---

## 3. Resultados duros

1. **Eje X (ancho 1820): VALIDADO y repetible.** En las 3 posiciones donde se ven ambas laterales, la suma
   `LEFT + RIGHT + robot ≈ 1820`. Al correr el robot a la izquierda, **LEFT bajó ~320 y RIGHT subió ~350**
   (movimiento en X correcto). La pared lateral **más cercana siempre está a ≤ 910 mm** → en alcance → **X
   medible en cualquier posición.**
2. **Alcance perpendicular con NEGRO ≈ 1,4–1,5 m.** Vio paredes a 825 / 1130 / 1135 mm; **NO** vio la de
   1735 mm (RIGHT en el corner). Lecturas estables a 1900–2700 mm = **diagonales/elevación**, no la
   perpendicular. (Coherente con `wall_reach ≈ 1000` de `keeper_xy_walls.h`, que es conservador.)
3. **HUECO del arco confirmado.** BACK frente al hueco de la portería da de más (1000–1377, ve a través);
   frente a pared maciza da la pared real (640). → BACK no es confiable cuando apunta al hueco; tratarlo
   aparte (usar laterales + el conocimiento de que está en el arco, o vetar BACK en ese sector).
4. **Eje Y (largo 2430):** la pared de fondo cercana (≤ 1215 mm) cae al **límite del alcance en el centro
   del largo** (marginal), pero **cerca de los fondos se ve bien** → **régimen ideal del ARQUERO.**
5. **BNO sano, cero relativo al boot** (ver §4): mide rotación correctamente (estable, signo/magnitud OK),
   pero su 0 hay que anclarlo mirando al arco.

---

## 4. El heading del BNO: por qué "no queda en cero" mirando al arco (auditoría de código)

> Causa-raíz **certera por código** (auditoría multi-agente 2026-06-25; ver §6). El síntoma de Virginia
> ("arranco mirando al arco y dice 47° en vez de 0") tiene dos partes que NO hay que confundir.

- **El BNO está en modo IMU (`OPERATION_MODE_IMUPLUS`)** — giroscopio + acelerómetro, **sin magnetómetro**
  (`sensors_imu.cpp:223`). Ventaja: **no deriva por los motores** (no usa el campo magnético). Consecuencia:
  el yaw es **relativo al encendido**; el "cero" lo fija `g_offset[i]`, el yaw crudo promediado en
  `sensors_imu_init()`. → **"0 = mirando al arco" SOLO vale si el robot arranca mirando al arco** y el
  sellado del offset sale limpio.
- **El heading del WorldSnapshot NO resta ningún offset de cancha** (`main_top.cpp` / `snapshot_emitter.cpp`
  vía `sensors_imu_get_heading_centideg()`): es el crudo menos el cero-de-boot. Existe un **segundo** offset
  (`bno_offset_centideg` en `localization_runtime.cpp`) pero su **único** consumidor es la trilateración X/Y
  (`localization.cpp`); **nunca vuelve al heading**. Por eso `snap.my_heading_deg == crudo` (lo que medimos).
- **El 47° vs el −45,3°** son dos sellados distintos, no un error de 2°: el −45,3 (boot quieto a 45° der, con
  el settle de 3 s) es **correcto** (signo `HEADING_SIGN=−1`, std 0). El 47° fue un boot con el cero mal
  fijado (HIPÓTESIS: el chip no estaba estable o el robot no quedó exactamente al arco al sellar). **A
  caracterizar en banco** (TASK-227).

### Cómo anclar el cero al arco (3 opciones, de la auditoría)

| | Qué hace | Riesgo | Esfuerzo |
|---|---|---|---|
| **FIX C — re-cero en vivo (recomendado como red operativa)** | Usar `sensors_imu_recalibrate_zero()` (ya existe, `sensors_imu.cpp:514`) vía el comando **`IMU ZERO`** del monitor USB: poner el robot **mirando al arco** y disparar el cero. **Sin reflashear.** | MUY BAJO; no toca el contrato con CENTRAL | ~0,5 h (documentar/confirmar cableado del comando) |
| **FIX B — sellar mejor el cero al boot** | Gatear la captura del offset hasta `gyro_calib≥3` + reintento + fallback por timeout (el settle de 3 s es un `delay()` ciego; esto lo hace determinista). Sigue siendo cero relativo-al-boot. | BAJO; no cambia contrato con CENTRAL | 1–2 h |
| **FIX A — heading en marco-cancha** | Restar `bno_offset_centideg` al heading del snapshot (gateado). Pone el heading en el MISMO marco que la X/Y. | **ALTO**: cambia el CONTRATO de heading con CENTRAL (hoy espera "0 = boot"); CENTRAL podría des-rotar dos veces. Banco TOP+CENTRAL antes de tocar nada. | 2–4 h |

**Recomendación:** **FIX C ya** como procedimiento operativo (fija el cero al arco sin reflashear, ataca
directamente el modo de falla "no quedó al arco al encender"), **FIX B** para robustez de boot, y **FIX A
solo si** el equipo decide que el heading debe estar en marco-cancha (decisión de contrato, no de bug).

---

## 5. La fórmula: ajustar las lecturas con el BNO (extensión rotación-aware de `keeper_xy_walls`)

**Idea:** el BNO da el giro θ del robot; con θ se sabe **qué zona de qué ToF apunta perpendicular a cada
pared**, y se corrige la distancia por el ángulo. Es la pieza que le falta a `keeper_xy_walls.h` para no
mentir bajo rotación.

### 5.1 Geometría

- **Montaje de cada ToF** (rumbo del eje del sensor en el marco del robot, CCW desde el frente +Y):
  FRENTE = 0°, IZQUIERDA = +90°, ATRÁS = 180°, DERECHA = −90°.
- **Rumbo del eje del sensor en el marco de CANCHA:** `Φ_s = θ + φ_montaje` (θ = heading del BNO, 0 = arco).
- **Zona (i, j) del sensor:** azimut `α` (columnas, 4×4 ⇒ ≈ {−22,5°, −7,5°, +7,5°, +22,5°}) y elevación `ε`
  (filas, mismo paso). El signo de α/ε depende del **mapeo crudo→físico por sensor** (las rotaciones
  FRONT/BACK 180°, RIGHT 90°, LEFT 270° del visualizador) → **confirmar en banco.**
- **Rumbo de cancha del rayo de la zona:** `γ = θ + φ_montaje + α`.
- **Rumbo "hacia" cada pared** (CCW desde +Y): pared del FRENTE (+Y) = 0°; IZQUIERDA (−X) = +90°; del
  FONDO propio (−Y) = 180°; DERECHA (+X) = −90°. (Chequeo: con θ=0, el ToF DERECHA (φ=−90) apunta a −90° =
  pared derecha ✓.)
- **Ángulo fuera de la normal:** `β = norm180(γ − ψ_pared)`.

### 5.2 Distancia perpendicular por zona

```
D_perp = d_zona · cos(ε) · cos(β)        con β = (θ + φ_montaje + α) − ψ_pared
```

válida cuando **|β| ≲ 25–30°** (la pared cae dentro del FoV) y el `target_status` de la zona es bueno.
`d_zona` es la lectura cruda (slant range); `cos(ε)` proyecta a horizontal; `cos(β)` proyecta a la normal.

**Distancia robusta a la pared W** = promedio de `D_perp` sobre **todas** las zonas (de cualquier ToF y
columna) con |β|<umbral y status válido. Con θ conocido, esta selección es **determinista** — esa es la
"selección de zonas perpendiculares con el BNO" que pediste.

### 5.3 Pose (convención del repo)

```
y = D_perp(fondo)      + tof_offset          (si la pared de fondo está en alcance; tof_offset ≈ 95 mm)
x = D_perp(izquierda)  + tof_offset          (si la izquierda está en alcance)
  = field_width − (D_perp(derecha) + tof_offset)   (si la derecha está en alcance)
  → si ambas válidas y consistentes: promedio
θ = heading del BNO (con el cero anclado al arco, §4)
```

**Compuerta (reemplaza el "congelar XY durante el escape" heading-free):** si **ninguna** zona tiene
|β|<umbral para una pared (robot muy rotado / pared fuera del FoV), ese eje queda **inválido** — explícito,
manejado por el BNO en vez de por la capa de wiring.

### 5.4 Dos variantes de implementación

- **Mínima (barata, bajo riesgo):** mantener el reductor mediana+recorte que YA tiene `keeper_wall_dist_mm`,
  y multiplicar por `cos(θ + φ_s − ψ_W)` + gatear en |θ + φ_s − ψ_W| < umbral. Para rotaciones chicas (el
  arquero casi siempre mira al frente) alcanza y es el cambio más chico.
- **Completa (rotación-aware real):** la selección por zona de §5.2 — más robusta y extiende el alcance útil
  bajo rotación, pero pide el mapeo crudo→físico por sensor bien calibrado (TASK-227 / TASK-228).

> **Nota honesta:** la fórmula es geometría estándar y es consistente con los datos, pero **no está validada
> en banco**. Los signos de α/ε por sensor y el cero del heading se confirman en hardware (TASK-227). Hasta
> entonces es PROPUESTA.

---

## 6. Trazabilidad

- Data cruda de las 4 estáticas: `journal/2026-06-25-tof-localizacion-paredes-4-posiciones-validacion.md`.
- Auditoría del heading (causa-raíz + 3 fixes + crítica adversarial): workflow `diag-heading-offset`
  (2026-06-25, 6 agentes). Conclusiones certeras por código citadas en §4.
- Módulo que este informe extiende: `src/shared/keeper_xy_walls.h` (+ TASK-221, validación heading-free).
- Modo del BNO (IMUPLUS) confirmado en `sensors_imu.cpp:223` + `docs/.../giroscopo-bno055-analisis-tecnico.md`.

## 7. Pendiente (NO lo cierra Claude — regla 1)

- **TASK-227** — validar en banco la localización con BNO en R2: anclar el cero al arco (FIX C/B),
  caracterizar el 47°, y validar la selección de zonas perpendiculares + la pose bajo rotación.
- **TASK-228** — calibrar las zonas ToF del OTRO robot (R1): mapeo crudo→físico por sensor + alcance con
  negro, para que la misma fórmula valga en los dos robots.
