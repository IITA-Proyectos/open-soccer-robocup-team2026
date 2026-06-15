---
id: TASK-212
title: "Validar la salud del BNO con DATOS INDEPENDIENTES (OTOS + cámaras) + auto-recuperación stop→settle→reset"
date_created: 2026-06-15
assigned: [equipo (firmware TOP/CENTRAL)]
priority: P2  # Fase 1 sube a P1 si el primario único da problemas en banco; ver "Prioridad honesta"
pedido-por: María Viollaz (banco R2, 2026-06-15)
status: cerrado-analisis-completo  # análisis entregado 2026-06-15; IMPLEMENTACIÓN + banco = PENDIENTE (P2). Ver "Cierre 2026-06-15".
blocks: []   # NO bloquea Incheon — el robot compite hoy con primario-solo + fallback heading_valid=0
relacionada: TASK-207 (BNO bus Wire2), TASK-022 (homografía cámaras sin calibrar), imu_fusion.cpp, imu_freeze.h, sensors_imu.cpp, top_robot2_pri_fastbno
tags: [firmware, top, central, bno055, imu, fusion-sensorial, otos, camaras, robustez]
depends_on: []
---

# TASK-212 — Decidir QUÉ BNO anda usando datos independientes (no auto-referencia)

> **Idea de María (banco 2026-06-15):** en vez de que la TOP decida la salud del BNO
> *mirándose a sí mismo* (la heurística actual, que un BNO **congelado engaña**), que use
> **referencias FÍSICAMENTE independientes** — el **heading/omega del OTOS** y el **cambio
> de bearing de los arcos en las cámaras** — para saber **cuál BNO anda y cuál no**, e
> incluso **auto-recuperar**: si detecta deriva/falla, **frenar, quedarse quieto ~2 s y
> resetear** ese BNO.

> **La idea es correcta en su física.** Cruzar la IMU contra verdad de terreno es como se
> valida una IMU en serio, y **rompe el agujero por el que entró el incidente R1** (un BNO
> congelado tiene deriva 0 → el árbitro actual lo premia como "el más estable" y arrastra al
> sano: `hdg 0.0 → −142.4` clavado, `sensors_imu.cpp:302-311`). **PERO** el análisis profundo
> (workflow 5 agentes + chequeo adversarial, 2026-06-15) encontró **restricciones de hardware
> reales** que reencuadran el alcance. Este TASK deja el diseño hecho **y** esas restricciones
> a la vista, para que el equipo decida con los ojos abiertos.

---

## PARTE A — El diseño (cómo se haría)

### A.1 — Modelo de cross-validación (scoring de salud por BNO)

Corre en la TOP, pasivo, 100 Hz. **Se compara TASA DE ROTACIÓN (omega, °/s), no heading
absoluto** — porque las referencias externas solo son comparables en velocidad angular (el
arco no comparte el "cero" del BNO; el OTOS driftea en absoluto). En una ventana `W≈200 ms`:

- `w_bno_i` = Δyaw del BNO *i* / Δt (de `s.heading_deg`, ya existe en `imu_fusion.cpp`).
- `w_otos` = `comm_down_get_velocity().omega_centideg_s / 100` (**ya llega fresco** a 100 Hz, `comm_down.cpp:142-143` — hoy es **dato huérfano**, solo se usa en debug).
- `w_cam` = −Δ(ángulo del arco visible)/Δt — **trabajo nuevo**: hoy las cámaras NO guardan historia del arco (solo la pelota tiene estimador de velocidad). Espejo de `ball_velocity_update` en `cameras_runtime.cpp`.

**Validez de cada referencia** (ninguna vale siempre):
- OTOS: válido si `is_vel_fresh()` && `pose.confidence ≥ 60` && `slip_estimate < 50` (patinazo lo invalida). Continuo.
- Cámara: válida si el arco está visible && `camera_alive` && hay ≥2 muestras de arco en la ventana. **Intermitente** (necesita ver el arco).

**Verdad de rotación robusta:** `w_truth = mediana{ w_otos válido, w_cam válido }`. Un BNO está
sano si `|w_bno_i − w_truth| < tol` con `tol = 8°/s + 0.15·|w_truth|` (titular en banco).

**Distingue los 3 modos (lo que el árbitro auto-referencial NO puede):**
1. **Sano:** sigue a `w_truth` en reposo Y en giro.
2. **Congelado:** `|w_bno_i| ≈ 0` mientras `|w_truth| > 10°/s`. **Solo detectable con ref externa** (el auto-referencial ve "quietito" y lo premia — el bug R1).
3. **Deriva:** `|w_bno_i| > 2°/s` mientras `|w_truth| ≈ 0` (robot quieto). El BNO "inventa" rotación.

**Consenso (clave):** si **OTOS y cámara coinciden entre sí**, forman un consenso fuerte → un
BNO que difiera es el malo con alta confianza. Si **discrepan** (cámara puede mentir por
homografía/​polaridad; OTOS por patinazo) → **NO vetar**, solo bajar peso. Con **una sola
referencia** disponible → usarla solo para **bajar score, jamás para vetar/resetear** (ver
Parte B).

### A.2 — Por qué es mejor que el árbitro actual

El árbitro de hoy (`imu_fusion.cpp:159-198`) elige "el más estable" = el BNO que menos se mueve
respecto a **su propio** pasado **en reposo**. Es ciego por construcción a (1) un congelado
(varía 0 → gana siempre) y (2) todo lo que pasa **en movimiento** (solo mira en reposo). El
cross-check invierte la lógica: la verdad la fija una fuente **independiente del BNO**, y
funciona **en movimiento**. No reemplaza al árbitro: lo **supervisa** (un BNO vetado no puede
ganar referencia ni semillar un reseed → cierra el agujero del bug R1).

### A.3 — Auto-recuperación (frenar → settle → reset)

**Separación de responsabilidades:** la **TOP decide QUÉ BNO está mal** (tiene los datos); la
**CENTRAL decide CUÁNDO es seguro frenar** (es la única que mueve). Nunca al revés.

- TOP, si un BNO es `BAD` sostenido y hay consenso externo fuerte → setea **flag nuevo
  `recommend_settle`** (WorldSnapshot `flags` bit5; bits 5-7 reservados, `types.h:137`; aditivo, no rompe el wire). **La TOP no frena nada, solo recomienda.**
- CENTRAL lee el flag y entra a un sub-estado `SETTLE_IMU` **solo si es seguro**:
  - **Gate duro:** NUNCA en `INTERCEPT`/`CLEAR`/`imminent_exit` (defensa activa). Solo en `PATROL` con pelota lejos o en `WAIT_START`.
  - Frena (reusa `motors_brake` del freno de borde), `cmd=0` durante ~1,5-2 s (reusa el molde de `GK_WAIT_START`), avisa a la TOP "estoy quieto" (canal CENTRAL→TOP, **hoy es stub**, `comm_central.cpp:28-33`).
  - TOP resetea el BNO malo (soft-resync `sensors_imu.cpp:424-441`; si está **congelado** el soft-resync no alcanza → `present=false` y, si se quiere recuperar el chip, un `re-begin` real que **necesita ~2 s quieto** para recalibrar el gyro IMUPLUS).
  - **Candados:** timeout maestro (~3 s → abortar y jugar con `heading_valid=0`), **abort inmediato si la pelota se acerca** (defensa > calibración), cooldown ~10 s (no entrar en bucle si el chip está muerto).

### A.4 — Dónde se engancha (verificado en código)

| Hook | Archivo | Nota |
|---|---|---|
| Módulo **puro** nuevo `imu_cross_validate.{h,cpp}` (host-testeable) | `src/shared/` | Toda la matemática de scoring/consenso, aislada de hardware (patrón `imu_fusion`). |
| Consumir `omega` del OTOS (ya fresco, huérfano) | `src/top/comm_down.cpp:142-143` | Listo para usar. |
| `GoalRateTracker` (derivada del bearing de arco) | `src/top/cameras_runtime.cpp` | **Nuevo** (los arcos no tienen estimador; sí la pelota). |
| Campos `external_bad` / `external_ref` en `ImuSensorState` + usarlos en `usable`/`most_stable` | `src/shared/imu_fusion.{h,cpp}` | Cierra el agujero del bug R1. |
| Flag `recommend_settle` (bit5) | `src/top/main_top.cpp:182-189` | Junto a `heading_valid` (bit4). |
| `handle_frame` CENTRAL→TOP (**hoy stub**) | `src/top/comm_central.cpp:28-33` | Falta el cuerpo + un MsgType nuevo. |
| `world_model_recommend_settle()` | `src/central/world_model.cpp:96` | Espejo de `world_model_heading_valid()`. |
| Sub-estado `SETTLE_IMU` con gates | `src/central/strategy.cpp` (FSM) | Reusa molde de `WAIT_START` + brake del freno de borde. |
| Telemetría de salud por sensor | `src/top/top_telemetry_serial.cpp` | Para titular umbrales en banco SIN actuar. |

---

## PARTE B — ⚠️ Verificaciones contra la realidad (lo que reencuadra todo)

> Estas son del **chequeo adversarial**. Son las que hacen que esto sea un **tema a analizar
> honesto** y no una venta. Léanlas antes de poner una hora de banco.

### B.1 — 🔴 El robot que tiene BNO NO tiene OTOS (y viceversa)

El diseño asume "BNO + OTOS en el mismo robot". **No es así hoy:**
- **R2 (arquero, Virginia)** = el único con BNO → corre `down_robot2` con **`-DDOWN_NUM_OTOS_CONNECTED=0`** (`platformio.ini:1416`). **Sin OTOS.**
- **R1 (delantero, Elías)** = tiene 2 OTOS pero **0 BNO** (gyros desconectados; navega por OTOS).

**Consecuencia:** la referencia OTOS-omega —que el diseño declara su fuente independiente
**principal**— **no existe en el robot que tiene el BNO**. En R2 la única referencia externa
es la **cámara** (frágil: intermitente, homografía sin calibrar TASK-022, 30 Hz, polaridad de
arco ambigua 180°). El cross-check de 3 fuentes queda, hoy, reducido a "detector de freeze del
primario por cámara cuando el arco esté visible".

### B.2 — 🔴 El escenario estrella ("2 BNO discrepan") ya está eliminado en producción

La prod corre `top_robot2_pri` = `TOP_BNO_PRIMARY_ONLY` → el secundario está apagado
(`g_ready[1]=false`). El incidente R1 (congelado arrastra al sano) **no se puede reproducir en
el binario desplegado** porque no hay segundo BNO. El problema que **sí queda abierto** es otro:
que el **primario único** se congele o derive en cancha. Para ESE, el cross-check necesita una
ref externa que en R2 es solo la cámara.

### B.3 — 🔴 El omega del OTOS se satura a ±327°/s → falso positivo en giros rápidos

`omega_centideg_s` es `int16` → satura en **±327,67°/s** (`telemetry_sat.h:43-45`). Un omni en
`SEARCH` gira fácil >360°/s → `w_otos` se **clava en 327** mientras el BNO lee la tasa real
(mayor) → el diseño declararía **BAD a un BNO sano** justo en el régimen donde dice ser
superior. **Fix obligatorio:** marcar la referencia como NO-confiable cuando `|w| ≥ ~300°/s`, y
no evaluar en ventanas de omega cambiando rápido (arranques/frenos de giro), por la latencia
OTOS→TOP y cámara.

### B.4 — 🔴 Reset activo en el arquero = superficie de gol

Frenar 1,5-2 s a un **arquero** es exactamente cuando no se puede. Si el gate de seguridad
falla o la pelota entra a media secuencia, el arco queda **abierto con el robot clavado**.
Además reusa el `motors_brake` del freno de borde, que **ya tuvo** el bug de congelar al
arquero (fix 2026-06-11, `main_central.cpp:288-317`). **Recomendación: NO hacer la Fase 4
(reset activo) para Incheon.** El fallback `heading_valid=0` ya es estrictamente más seguro que
frenar. Si algún día se hace: reset **oportunista** en ventanas de quietud que **ya ocurren**
(`WAIT_START`, settle entre pulsos de patrulla), sin frenar a propósito.

### B.5 — Hay un camino más barato para el problema que de verdad queda

El modo **congelado** del primario único se detecta **sin ninguna referencia externa** con el
**freeze-detector que YA existe** (`TOP_ENABLE_BNO_FREEZE_DETECT`, `imu_freeze.h`, hoy OFF). Se
quitó el 2026-06-08 por dar **falsos-DEAD con el robot quieto** → re-activarlo necesita cruzar
"heading congelado" con "gyro≈0" para no confundir quieto-sano de congelado (un quieto sano
micro-varía; un congelado no). **Eso cubre el modo congelado (el único documentado en cancha)
sin OTOS, sin cámara y sin FSM nueva.** El cross-check externo queda como complemento para el
modo **deriva** y **solo cuando exista OTOS** (que hoy no está en R2).

---

## PARTE C — Plan por fases y prioridad honesta

| Fase | Qué | Riesgo | Prioridad |
|---|---|---|---|
| **0** | **Habilitar + sanear el freeze-detector existente** (cruzar heading-frozen con gyro para no dar falso-DEAD quieto). Cubre el modo congelado del primario único, sin refs externas. | Bajo | **P1 si el primario único preocupa**; el más barato |
| **1** | `imu_cross_validate.cpp` **puro host-testeable** + `GoalRateTracker`. Alimentar con cámara (y OTOS donde exista) y **solo exponer por telemetría** salud/score. **No tocar la fusión, no actuar.** Titular umbrales contra datos reales. | Cero (solo lectura) | P2 |
| **2** | Veto **pasivo** en la fusión (`external_bad` → un BNO vetado no gana referencia ni semilla reseed). **Solo cierra el bug R1**, no mueve el robot. Aditivo, fallback exacto. | Bajo | P2 |
| **3** | TOP setea `recommend_settle`, CENTRAL lo **lee y loguea**, **sin actuar**. Verificar que no salta en juego normal. | Bajo | P2 |
| **4** | Recuperación **ACTIVA** (frenar/settle/reset + gates/timeout/cooldown/abort). | **Alto** | **P2 / post-Incheon** (o descartar a favor de reset oportunista) |

**Prioridad honesta global: P2.** El robot **ya compite** con primario-solo + fallback
`heading_valid=0`; esto sube robustez, **no habilita la competencia**. La Fase 0 (freeze-detector)
puede subir a P1 si en banco el primario único se muestra poco confiable. **Costo de oportunidad
real:** cada hora de banco acá es una hora que no va a **calibrar las cámaras (TASK-022)**, que
es el bloqueante #1 de Incheon.

---

## Riesgos (formato coach)

- **`risk-no-fix`:** el árbitro auto-referencial sigue siendo engañable por un BNO congelado
  (ya pasó en R1). La mitigación actual (primario-solo) tapa el síntoma tirando la redundancia:
  si el único BNO se cuelga en cancha, `heading_valid=0` y el robot pierde orientación por rumbo
  **sin diagnóstico de cuál falló ni forma de recuperarlo**. Se desperdicia el omega del OTOS
  (fresco a 100 Hz, hoy huérfano) — donde el OTOS exista.
- **`risk-fix`:** Fases 0-3 son aditivas y de bajo riesgo (fallback exacto). El riesgo vive en
  Fase 4 (puede dejar al arquero quieto en mal momento) y en **falsos vetos** por una referencia
  que miente (cámara sin homografía, OTOS patinando, omega saturado) → vetar a un BNO **sano**
  es **peor** que el bug original.
- **`tiempo` (honesto):** Fase 0 ~0,5-1 día + banco. Fase 1 ~1,5-2 días + 0,5 banco. Fase 2 ~0,5
  día + banco. Fase 3 ~0,5 día. Fase 4 ~2-3 días + 1-2 sesiones de banco con pelota. Total ~5-6
  días de ingeniería + 3-4 sesiones de banco. **Fases 0-2 entregan valor en la primera semana.**

## Plan de prueba en hardware real (obligatorio)

1. **Fase 0/1 (diagnóstico, sin actuar):** robot girado a mano; con un BNO tapado/desconectado,
   confirmar que el scoring (o el freeze-detector) distingue **sano / congelado / deriva**.
   Medir primero el **lag y el ruido del omega OTOS→TOP** (donde haya OTOS) para fijar `TOL_BASE`
   — hoy es una estimación (8°/s), no un dato.
2. **Falsos positivos:** robot quieto 3-5 min apuntando al arco → **el sano NO debe declararse
   malo**. Giro rápido (`SEARCH`, >360°/s) → **el sano NO debe declararse malo** por saturación
   del omega (ver B.3).
3. **Fase 2 (veto pasivo):** confirmar que un vetado deja de ganar referencia y que el sano
   **nunca** queda vetado por error.
4. **Fase 4 (si se hace):** con pelota — confirmar que **NUNCA** frena en defensa activa, que el
   timeout y el abort-por-pelota sacan del settle, que el cooldown evita el bucle.

## Cierre 2026-06-15 — qué está cerrado y qué no

**CERRADO: el ANÁLISIS.** Lo que se pidió (el estudio profundo del enfoque de cross-validación
con datos independientes + auto-recuperación) **está entregado** en este documento, verificado
contra el código y pasado por un chequeo adversarial que reencuadró el alcance (Parte B).

**NO está hecho ni validado: la IMPLEMENTACIÓN.** Las Fases 0-4 son trabajo futuro. Esta TASK se
cierra **como documento de análisis/decisión**, NO como feature implementada. Cuando el equipo
decida retomarla, **abrir una task de implementación nueva** (Fase 0 = freeze-detector saneado es
el 80/20). Prioridad global P2; el robot compite hoy sin esto.

## Cierre (cómo se valida la implementación, cuando se haga)

Lo cierra el **equipo humano** tras validar en banco. Claude **NO** marca esto `done` ni asume
que funciona por compilar. **Verdad del código manda:** toda cita `archivo:línea` de acá se
verificó contra el repo real (`soccer-main`, main); las que el chequeo adversarial halló
imprecisas se corrigieron (la fusión de arco vive en `cameras.cpp`/`cameras_runtime.cpp` +
`shared/cameras_fusion.h`, no en un `cameras_fusion.cpp`).

> **Precondición fuerte para que esto valga la pena:** que un robot tenga **BNO + OTOS juntos**
> (re-habilitar OTOS en R2, o la arquitectura CANbus 2027). Mientras R2 sea BNO-sin-OTOS y R1
> OTOS-sin-BNO, el cross-check completo no tiene sus dos patas en el mismo robot — y la Fase 0
> (freeze-detector saneado) es el 80% del valor al 20% del costo.
