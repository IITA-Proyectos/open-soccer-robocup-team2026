---
title: "Seguidor de línea para el arquero — ¿la info necesaria sube de la DOWN a la CENTRAL?"
date: 2026-06-21
author: "Claude Opus 4.8 (Anthropic) — workflow de 5 lectores + verificación adversarial"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
status: in-progress
tipo: relevamiento-y-factibilidad
robot: ARQUERO (R2) — arqueromix vs central_robot2_arquero
horizonte: "Incheon 2026"
---

# Seguidor de línea para el ARQUERO — ¿la información necesaria llega a la CENTRAL?

> Pedido por Gustavo, 2026-06-21. Verificado leyendo el código real (DOWN + contrato + arqueromix +
> el arquero de competencia). **Nada testeado en hardware** — el cierre lo hace el equipo (regla #1).

---

## 1. Respuesta directa (TL;DR)

**Sí: la información necesaria para un seguidor de línea de arquero SÍ sube de la placa DOWN a la CENTRAL.
NO se queda abajo.** El cable DOWN→CENTRAL manda un paquete de 16 bytes (`LineStatusV2`) a 200 Hz, y adentro
viaja **`cross_track_mm`**: la **distancia perpendicular firmada** del centro del robot a la línea (en mm,
con signo). Esa es exactamente la señal para mantener la línea del área a una distancia fija bajo el robot.

**El corte NO está en el cable ni en la DOWN. Está en el programa `arqueromix`:** su adaptador
`apply_down_line` copia solo **3 de los 12 campos** del paquete (`line_present`, `line_angle_deg`,
`line_depth`) y **descarta `cross_track_mm`** antes de que la FSM del arquero lo vea.

Y el dato más importante para decidir: **el arquero de COMPETENCIA `central_robot2_arquero` YA sigue la
línea por `cross_track` hoy** (su lazo por defecto lo usa). El que NO lo hace es `arqueromix`, que es la
FSM paralela y simple (port 2025) que Virginia viene flasheando esta semana. **Son dos firmwares de arquero
distintos.**

**Veredicto:** lo que querés es alcanzable **sin tocar la DOWN ni el protocolo**. Hay dos caminos —
**(A)** portar el lazo de `cross_track` a `arqueromix` (más trabajo del que parece), o **(B)** usar el
arquero `central_robot2_arquero` que ya lo hace. Detalle en §5–§6.

---

## 2. Qué información sube por el cable DOWN→CENTRAL (lista definitiva)

El "wire" de línea es **un solo mensaje**: el frame `LINE_URGENT` (tipo `0x10`) que lleva el struct
`LineStatusV2` de **16 bytes exactos** (`src/shared/types.h:142-156`, con `static_assert`), difundido a
200 Hz a CENTRAL (Serial1) **y** a TOP (Serial5). La DOWN arma el frame con `memcpy` del struct entero
(`src/shared/down_encode.cpp`) — **no recorta nada en el cable**.

| Campo | Tipo / unidad | Significado | ¿Sube? | ¿Lo usa `arqueromix` hoy? |
|---|---|---|---|---|
| `schema_version` | u8 | versión del contrato (=2) | ✅ | no (lo chequea el parser) |
| `data_valid` | u8 0/1 | **compuerta maestra**: 0 = robot levantado/calib dudosa → no confiar | ✅ | ❌ no lo expone |
| `line_angle_centideg` | i16 (°×100) | orientación de la línea (0=frente, +90=derecha) | ✅ | ⚠️ se copia pero la FSM no lo lee |
| `escape_angle_centideg` | i16 (°×100) | dirección sugerida para alejarse del borde | ✅ | ❌ |
| `penetration_mm` | u16 mm | cuán adentro de la zona de línea está (0=recién tocando) | ✅ | ❌ |
| **`cross_track_mm`** | **i16 mm FIRMADO** | **EL CAMPO CLAVE: distancia perpendicular centro→línea. + = línea adelante, − = atrás.** | **✅** | **❌ NO lo copia — se descarta** |
| `line_present` | u8 0/1 | ¿hay línea? (con histéresis) | ✅ | ✅ (binario) |
| `sensors_on_line` | u8 0..32 | **CONTEO** de sensores en blanco. NO dice *cuáles* | ✅ | ✅ (como `line_depth`, umbral ≥1) |
| `event_flags` | u8 bitmask | `IMMINENT_EXIT`, `CORNER`, `LINE_END`, `LIFTED`… (eventos, no sensores) | ✅ | ❌ |
| `quality` | u8 0..100 | confianza (hoy placeholder: 85/95/0) | ✅ | ❌ |
| `sample_age_ms` | u8 ms | edad de la medición (para descartar datos viejos) | ✅ | ❌ |
| `reserved` | u8 | relleno | ✅ | — |

### Lo que la DOWN COMPUTA pero NO empaqueta (se queda abajo) — el corazón de tu pregunta

1. **El estado por-sensor / bitmask de "cuál sensor ve blanco" — NO EXISTE en el contrato.** La DOWN sí
   sabe internamente, sensor por sensor, cuál ve blanco (`line_ring_get_white(i)`), pero **eso no se
   serializa**. Lo más cerca que llega a CENTRAL es `sensors_on_line` (un **conteo** 0..32 sin identidad)
   y `cross_track_mm` (el centroide). **Desde la CENTRAL NO se puede saber "tocaron específicamente los
   sensores traseros izquierdos".**
2. **Los crudos ADC por sensor** (`g_raw[i]`, 0..1023) — solo entran al cálculo localmente. CENTRAL no los ve.
3. **La "cadena vieja" de procesamiento** (`line_ring_tick`: `g_angle_deg`, `g_lifted`…) — en competencia
   **nadie la consume**; solo los diagnósticos de banco por USB. (Hay dos cadenas en paralelo: deuda
   conocida, documentada en `comm_central.cpp:225-229`.)

> ⚠️ **Cobertura:** `cross_track_mm` y `penetration_mm` **solo se calculan si están los 32 sensores**
> (`n==32`, los 4 muxes — `down_model.cpp:94`). Con anillo parcial van a N/A. La config dice 4 muxes en
> producción; **se confirma en banco, no por código**.

---

## 3. Cómo usa HOY el arquero (`arqueromix`) la línea — dónde se corta la cadena

```
anillo 32 sensores → DOWN (cross_track, ángulo, conteo…) → [CABLE 200 Hz: LineStatusV2 COMPLETO, 16 B]
   → CENTRAL recibe los 16 bytes → amix_comm.apply_down_line() ✂️ ACÁ SE CORTA → g_aio (3 campos) → FSM
```

El corte exacto (`src/arqueromix/amix_comm.cpp`, función `apply_down_line`):

```cpp
g_aio.line_present   = lsv2_line_present(ls);
g_aio.line_angle_deg = lsv2_line_angle_deg(ls);   // se copia pero la FSM no lo lee
g_aio.line_depth     = lsv2_sensors_on_line(ls);  // conteo 0..32
// cross_track_mm NUNCA se copia → la FSM del arquero ni lo ve
```

La estructura plana del arquero (`amix_io.h`) **solo tiene 3 campos de línea** (`line_present`,
`line_angle_deg`, `line_depth`) — no hay ni lugar para `cross_track`. Y de esos 3, la FSM usa **2 de forma
binaria**: el helper `linea()` = `line_present && line_depth >= 1` (hay / no hay línea). Con eso el arquero
hoy: (a) hace homing al área al arrancar, (b) detecta el fin del retroceso tras despejar, (c) **patrulla
por el ÁNGULO del arco** (cámara), **sin usar la línea** para el ping-pong.

**Por eso hoy NO podría hacer tu seguidor fino:** no tiene el error perpendicular continuo a la línea
(`cross_track`), que llega al cable y a la CENTRAL pero `amix_comm` descarta. La propia `DOCUMENTACION.md`
de arqueromix (§12) lo reconoce: *"recorta la línea a 3 campos… NO expone cross_track_mm… trabajo concreto,
no opcional"*.

---

## 4. Geometría de los sensores (para "la línea bajo la parte de atrás")

**32 sensores ALS-PT19** en 3 anillos, posiciones reales en `SENSOR_POS[32]` (`src/shared/sensor_geometry.cpp`).
Convención: **+X = derecha, +Y = adelante, 0° = frente**, origen = centro del PCB.

Periferia trasera (los que tocarían la línea si el robot la mantiene bajo su cola), aprox:

```
   trasero-IZQ                          trasero-DER
 S13(-68,-50) S14(-60,-60)      S18(+49,-69) S19(+61,-60)
 S15(-49,-69) S16(-36,-77)      S17(+36,-77) S20(+70,-50)
                  ↑__ los más atrás (Y≈-77) __↑
              hueco central ~72 mm SIN sensor a 270°
```

Tres cosas que importan:
1. **Geometría simétrica** izq↔der (S13↔S20, S16↔S17 espejo). Hay ~8 sensores externos traseros entre
   Y=−50 y −77 mm — candidatos a "tocar sin meter el cuerpo".
2. **Hueco trasero central (~72 mm a 270°):** entre S16 y S17 no hay cobertura externa. Si la línea queda
   **justo** bajo el centro-atrás, podría caer en el hueco (lo cruzo con el plan de banco, §7).
3. **El eje Y NO está validado físicamente** (TASK-027): el código asume centro-PCB = centro-robot y
   +Y = adelante. Si el montaje está rotado, "atrás geométrico" ≠ "atrás real" → **afecta el signo de
   `cross_track`**. Validar antes de confiar.

⚠️ **Límite real:** `cross_track_mm` es el **centroide Y de TODOS los blancos**, no solo de los traseros.
Si la línea toca también sensores frontales/laterales (p. ej. en una atajada, con la pelota empujando), el
centroide se "adelanta" y mezcla frente+atrás. **No hay helper que aísle solo la periferia trasera.** Para
esta estrategia se maneja con el **setpoint negativo** — pero eso NO aísla el subconjunto trasero cuando
hay blancos frontales simultáneos. Es una limitación a tener presente, no un bloqueante.

---

## 5. Factibilidad del seguidor — y la corrección de eje importante

**Tu idea** se descompone en TRES ejes independientes (esto es lo que el borrador inicial confundió y la
revisión corrigió):

| Parte de tu idea | Eje | Señal | ¿La tenemos? |
|---|---|---|---|
| **"línea del área bajo la cola, a distancia fija"** (que no se meta) | **vy (adelante/atrás)** | `cross_track_mm` con setpoint **negativo** | ✅ sube al cable |
| **"moverse suave izq↔der"** | **vx (lateral)** | oscilación / seguir la pelota en X | ✅ es el eje del omni |
| **"mirando al arco rival"** | rumbo (ω) | `goal_opp_angle` | ✅ ya en `amix_io.h` |

**La corrección clave:** la línea del área corre **izquierda-derecha** (a lo largo de X). Su perpendicular
es el **adelante/atrás (Y)**. Entonces "mantener la línea bajo la cola" se controla **moviéndose en vy**,
NO en vx. El movimiento izq↔der (vx) es **separado** y es el que da la patrulla. O sea: el seguidor =
**oscilar en vx + mantener `cross_track` con vy + rumbo al arco rival** — no "un PID lateral sobre
cross_track".

### Lo que YA existe (verificado en el arquero de competencia)

El arquero `central_robot2_arquero` (firmware de competencia, FSM v3.3 — **distinto** de `arqueromix`) ya
hace la parte difícil:
- `GK_CROSS_TRACK_SETPOINT_MM = -40.0f` (`src/central/strategy.cpp:352`) — comentario: *"con la línea
  ~40 mm DETRÁS del centro el anillo la toca con 2-4 sensores: patrulla el borde sin disparar la alarma de
  salida"*. **Eso es literalmente tu pedido.**
- El consumo está en `gk_lateral_pid_output()` (`strategy.cpp:601-614`), **en el lazo por defecto** (NO
  gateado), y la corrección de `cross_track` se aplica a **vy** (`strategy.cpp:354-361`).
- `goalkeeper_tick()` (la FSM completa con PATROL/INTERCEPT/LINE_AVOID) está en `strategy.cpp:1385`.

> ⚠️ **Honestidad sobre la madurez (regla #1):** ese lazo **NO está validado en banco**. El propio código
> dice, en `strategy.cpp:359-361`: *"GK_CT_VY_SIGN — 🔧 SIGNO A CONFIRMAR EN BANCO (Fase 4): si el robot se
> ALEJA de la línea en vez de mantenerla, invertir a −1"*. El destino-a-vy se corrigió en banco el
> 2026-06-09, **pero el SIGNO sigue sin confirmar**. Y el `-40 mm = 2-4 sensores` es un **comentario de
> código, no una medición** (depende de la calibración mm de `cross_track` y de TASK-027). No te vendo
> "ya funciona": te digo "ya está cableado, falta cerrarlo en banco".
>
> Aparte: las mejoras `GK_STRAFE_PID` / `GK_PINGPONG` (heading-hold por rueda trasera, `strategy.cpp:410-411`)
> están **OFF por defecto** y son solo de banco — no confundir con el lazo vivo de `cross_track`.

### ¿Hace falta subir más datos por el cable? — NO

Para esta estrategia **alcanza con lo que ya sube** (`cross_track` + `line_present` + `sensors_on_line` +
`imminent_exit` + `goal_opp_angle`). El **bitmask por-sensor NO hace falta** (el centroide negativo ya
codifica "línea bajo la cola") y agregarlo sería **wire-breaking** (ver §6a). **No tocar la DOWN.**

---

## 6. Propuesta de implementación

> Formato coach. **Quién toca qué:** Virginia = `arqueromix/*`. El contrato DOWN / `src/shared/*` lo toca
> SOLO quien mergea (regla CLAUDE.md §5).

### (a) DOWN / contrato — **0 cambios necesarios**
> **TEMA A1 — bitmask por-sensor (NO recomendado para Incheon).** Sería un `uint32` con "qué sensor ve
> blanco". `risk-no-fix`: no podés discriminar por-sensor — **pero no hace falta** para esta estrategia.
> `risk-fix`: **WIRE-BREAKING** (rompe el `static_assert` de 16 B → re-flashear DOWN+CENTRAL+TOP+arqueromix
> juntos). `tiempo`: 4-8 h + re-validación de 4 binarios. **Prioridad: P2** (capitalizable 2027). **Para
> Incheon: NO tocar el contrato.**

### (b) y (c) — DOS caminos para tener el seguidor

**CAMINO B (recomendado si lo querés YA): usar `central_robot2_arquero`.**
Ese firmware **ya sigue la línea por `cross_track`** en su lazo por defecto. No hay que portar nada; el
trabajo es **de banco**: confirmar el signo `GK_CT_VY_SIGN`, tunear el setpoint, validar la regla del área.
- `risk-no-fix`: seguís con `arqueromix`, que no sigue la línea.
- `risk-fix`: es OTRA FSM que la que Virginia viene tuneando esta semana (homing, salida de línea, pateo
  con rampa). Cambiar de firmware de arquero es una **decisión de equipo** (cuál es el "vivo" para Incheon).
- `tiempo`: 1-2 días de banco (sin código nuevo). **Prioridad: P1.**

**CAMINO A: portar el lazo de `cross_track` a `arqueromix` (Virginia).**
- **TEMA B1 — exponer `cross_track_mm` + `data_valid` + `imminent_exit` a `g_aio`.** Los helpers ya existen
  (`lsv2_cross_track_mm`, `lsv2_imminent_exit`, `line_view.h`). `tiempo`: 1-2 h. **Prioridad: P1** (sin
  esto, nada del seguidor en `arqueromix` funciona). ⚠️ **Signo:** `cross_track` + = línea adelante; el
  setpoint "línea atrás" es **negativo**. Mal signo = el robot se aleja (síntoma clásico).
- **TEMA C1 — estado de patrulla por línea en la FSM.** Oscilar en **vx** (suave, con rampa) + mantener
  `cross_track` con **vy** (setpoint negativo) + rumbo a `goal_opp_angle` + frenar si `imminent_exit`.
  ⚠️ **Esto NO es "copiar 3-4 líneas":** `arqueromix` no tiene PID lateral ni la máquina
  PATROL/INTERCEPT/LINE_AVOID que envuelve al lazo en `strategy.cpp` (anti-flapping, debounce, cooldown,
  fix de eje). Portarlo bien implica traer ese andamiaje + los signos. `tiempo`: **2-4 días** (código +
  banco), no 1. **Prioridad: P1, pero pesado.**

**Recomendación honesta:** si querés el seguidor **pronto y robusto**, **Camino B** (ya está hecho, falta
banco). El Camino A tiene sentido solo si el equipo decide que `arqueromix` es EL arquero y quiere unificar
todo ahí — y en ese caso hay que presupuestar el port completo, no una copia.

---

## 7. Plan de prueba en banco

> Regla #1: solo el equipo con la placa cierra una TASK de hardware. Claude no marca `done` por "compila".

**Prep:** robot sobre la cancha con la línea real del área; telemetría USB mostrando `cross_track_mm`,
`data_valid`, `sensors_on_line`, `imminent_exit` en vivo.

1. **Signo + calibración de `cross_track` (gate, cruza TASK-027).** Poner la línea **detrás** del centro a
   mano → `cross_track` debe salir **negativo**; adelante → positivo. **Aceptación:** signo correcto en
   ≥9/10 colocaciones y magnitud coherente con regla (±10 mm). Si el signo está invertido → revisar montaje
   (eje Y) o invertir `GK_CT_VY_SIGN` antes de seguir.
2. **Lazo de profundidad aislado (vy).** Robot quieto, mover la cancha adelante/atrás bajo él → debe
   moverse en vy para mantener `cross_track` en −40 mm. **Aceptación:** estabiliza en setpoint ±15 mm sin
   "fishtail", con 2-4 sensores en blanco (no ≥6, que dispara `imminent_exit`).
3. **Cruce hueco-trasero × setpoint.** Con la línea ~40 mm atrás, verificar que el centroide **NO cae en el
   hueco central de 72 mm** (`sensors_on_line` se mantiene 2-4, **no 0**). Si cae en el hueco, ajustar el
   setpoint (−30 / −50) hasta que toquen sensores externos traseros de forma estable.
4. **Patrulla completa (vx + vy + rumbo).** Oscila izq↔der suave mirando al arco rival, línea bajo la cola.
   **Aceptación:** recorre el ancho del área sin que ningún sensor frontal toque la línea de fondo;
   velocidad suave; `imminent_exit` no se dispara en patrulla normal.
5. **Regresión:** homing y despeje siguen funcionando. Skill: `hardware-test-protocol`.

---

## 8. Riesgo reglamentario — área chica (GATE, leer el reglamento)

**Lo que sé del CÓDIGO** (no del reglamento): el firmware está pensado para tocar la línea con la periferia
trasera sin meter el cuerpo (setpoint −40, `EV_IMMINENT_EXIT` como freno con ≥6 sensores).

**Lo que NO sé y hay que LEER** (no lo invento): **no sé de memoria si la regla RCJ Soccer Open 2026 cuenta
"tocar la línea" o "estar dentro del área".** En RCJ Soccer el concepto relevante suele ser *lack of
progress* / doble-defensa en el goalie area, no necesariamente "tocar la línea = estar dentro". **Hay que
leer el reglamento oficial 2026 (sección goalie/penalty area) ANTES de fijar la estrategia.**

> **Gate de coach:** sin esa confirmación reglamentaria, el tuning del seguidor queda en backlog. Si la
> línea cuenta como "dentro", la estrategia cambia a "línea **justo delante** de la periferia trasera, sin
> tocarla" y el setpoint de `cross_track` cambia de signo/magnitud. **Abrir mini-tarea: alguien lee el
> rulebook y confirma la definición.**

---

## 9. Gaps declarados (no inventé nada de esto)

- **No medí en hardware** ruido/latencia/calibración de `cross_track` (regla #1). El código lo calcula; la
  precisión la valida el equipo (Paso 1).
- **Signo `GK_CT_VY_SIGN` sin confirmar** en banco (`strategy.cpp:359-361`, Fase 4). El `-40 mm = 2-4
  sensores` es comentario de código, no medición.
- **Eje Y sin validar** (TASK-027): afecta el signo del setpoint.
- **Cobertura 4 muxes:** `cross_track`/`penetration` solo con `n==32`. Config dice 4 muxes; no verificado
  en el binario.
- **Hueco trasero ~72 mm:** interacción con el setpoint −40 (Paso 3).
- **Reglamento área chica (§8):** pendiente de lectura del rulebook oficial 2026. Es un GATE.
- Los datos de `central/strategy.cpp` (setpoint, `gk_lateral_pid_output`, signo) los verificó la pasada
  adversarial leyendo ese archivo directamente; el relevamiento inicial lo tenía como fuera de scope.

**Archivos clave:** contrato `src/shared/types.h:142-156` · helper `src/shared/line_view.h:62-72` · cálculo
`src/shared/down_model.cpp:90-129` · **el corte** `src/arqueromix/amix_comm.cpp` (`apply_down_line`) +
`amix_io.h` · **lo ya hecho** `src/central/strategy.cpp` (`gk_lateral_pid_output:601-614`, setpoint :352,
signo :359-361, `goalkeeper_tick:1385`) · `goal_opp_angle` en `amix_io.h:42`.

---

*Análisis con apoyo de Claude (workflow 5 lectores + pasada adversarial). NO autoriza tocar firmware —
espera evaluación de Gustavo. Atribución según `AI-INSTRUCTIONS.md`.*
