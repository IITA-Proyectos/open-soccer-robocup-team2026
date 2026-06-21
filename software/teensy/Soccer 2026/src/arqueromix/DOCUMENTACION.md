---
title: "arqueromix — qué quedó hecho (estado + diseño)"
date: 2026-06-21
author: "Claude (Opus 4.8) — coach, pedido de Virginia"
status: COMPILA · NO validado en banco (prueba)
scope: software/teensy/Soccer 2026/src/arqueromix/
---

# arqueromix — qué quedó hecho

> **En una frase.** Es el **arquero 2025** (campeón Nacional BsAs: su máquina de estados
> y su manejo directo de motores) **revivido sobre el robot 2026**: en vez de leer sus
> propios sensores, lee los datos que mandan las placas **TOP y DOWN** por serie. Es el
> HERMANO ARQUERO de `centralmix` (que hizo lo mismo con el delantero el viernes). **Es
> una prueba: si anda, se sigue por acá; si no, se sigue con `src/central/` y no se perdió
> nada** (build aislado).

## 1. Estado (qué está hecho y qué no)

| | Estado |
|---|---|
| Estructura de carpeta + archivos | ✅ creada (`src/arqueromix/`) |
| Env de compilación `central_robot2_arqueromix` | ✅ en `platformio.ini` (aditivo, no toca nada) |
| **Compila** | ✅ `pio run -e central_robot2_arqueromix` → SUCCESS, FLASH ~19 KB |
| Aislamiento (no afecta lo actual) | ✅ `build_src_filter = +<arqueromix/> +<shared/>` (NO compila `src/central/`) |
| FSM 2025 del arquero portada (11 estados) | ✅ código escrito (port fiel) |
| Manejo directo de motores | ✅ código escrito (pines R1/R2) |
| Lectura de TOP/DOWN (comm propio) | ✅ código escrito (decodifica `shared/proto`) |
| **Heading por serie del TOP (no BNO local)** | ✅ (pedido de Virginia) |
| **Validado en banco** | ❌ **NO** → TASK-114 (compila ≠ anda) |

**Compilar NO prueba que ande.** Faltan: verificar el sentido de cada motor, el signo
lateral de la pelota, y re-tunear umbrales (cambiaron de píxeles a mm). Ver §7.

## 2. Objetivo y decisión de fondo

El arquero 2025 tiene una FSM que el equipo entiende y un manejo de motores simple y
directo. La idea es **reusar ese cerebro** pero darle los **ojos del robot 2026** (cámara
+ línea + heading que ya procesan TOP y DOWN), exactamente como se hizo con el delantero
el viernes (`centralmix`). Es una **rama experimental paralela**, no reemplaza nada. Cero
riesgo para el stack actual porque vive en otra carpeta y otro env.

## 3. Arquitectura y flujo de datos

```
   TOP  (Serial7) ── WorldSnapshot ──┐   (pelota + arcos + HEADING + árbitro)
                                      │   ┌──────────┐     ┌──────────┐
   DOWN (Serial1) ── LineStatusV2 ───┼──►│ amix_comm│──►  │  g_aio   │  (variables planas
                  ── Pose2D/Vel2D ────┘   │ (decode) │     │ (AmixIO) │   estilo 2025)
                                          └──────────┘     └────┬─────┘
                                                                │ lee
                                                          ┌─────▼─────┐
                                                          │ amix_fsm  │  (FSM arquero 2025: 11 estados)
                                                          └─────┬─────┘
                                                                │ llama
                                                          ┌─────▼──────┐
                                                          │ amix_motors│  (directo: INA/INB+PWM)
                                                          └─────┬──────┘
                                                                ▼  pines Zircon (R1=R2 en 2026)
```

**Clave (igual que centralmix):** NO usa `world_model`. `amix_comm` deja los datos en
**variables planas** (`g_aio`, tipo `AmixIO`) — como las globales del 2025 — y el resto
(FSM + motores) es **autocontenido estilo 2025**, leyendo `g_aio`.

**Diferencia con centralmix (pedido de Virginia):** el **HEADING viene del snapshot del
TOP** por serie (`my_heading_centideg` + bit4 `heading_valid`), **NO de un BNO local**.
Las placas CENTRAL 2026 no traen BNO propio; el rumbo se procesa en el TOP. Más simple
(sin Wire/BNO) y correcto. Con `-DARQMIX_HEADING_OTOS` usa el heading del OTOS (DOWN).

## 4. Qué hace cada archivo

| Archivo | Qué hace |
|---|---|
| `main_arqueromix.cpp` | `setup()`: init comm/motores/FSM. `loop()`: `amix_comm_tick()` → `amix_fsm_tick()`. |
| `amix_io.h` | `struct AmixIO` + `extern AmixIO g_aio`: variables planas (pelota, **arcos POR ROL `goal_opp`/`goal_own` — SIN color**, heading, línea, árbitro, timers, frescura). |
| `amix_comm.cpp/.h` | **Único que toca Serial.** Lee TOP (Serial7) y DOWN (Serial1) a 230400, decodifica con `shared/proto` + `line_view`/`pose_view`, y **llena `g_aio`**. Heading = snapshot del TOP. **Arcos: copia directa `goal_opp`/`goal_own` (la polaridad la resolvió el TOP; acá no se mira color).** |
| `amix_fsm.cpp/.h` | La **FSM del ARQUERO 2025** portada fiel (11 estados). Lee `g_aio`, decide, llama primitivas de `amix_motors`. Agrega el gate `match_running` + un timeout de seguridad al retroceso. |
| `amix_motors.cpp/.h` | **Manejo directo 2025**: `adproporcional/aiproporcional/impulso_inicial/avanzar/avanzar_patear/patear_atras/`**`girar`** + `amix_set_motor(idx,pwm)`. Escribe `analogWrite(PWM)`+`digitalWrite(INA/INB)`. Sin mixer, sin cinemática omni. (`girar` = rotación pura para alinear al arco rival.) |
| `amix_config.h` | Pines (R1/R2 2026), constantes 2025 (PWM proporcionales, impulsos, patada), tolerancias, tiempos, selector de heading. |
| `README.md` | Guía corta + comando de flasheo. |
| `DOCUMENTACION.md` | Este archivo. |

## 5. La máquina de estados del arquero (11 estados, port fiel del 2025)

Flujo del arquero: patrullar lateral siguiendo la pelota en el eje lateral, y al tenerla
cerca+centrada, despejar (pausa → patada → pausa → retroceso a la línea → reposicionar).

```
impulso_inicial (40 ms, strafe fuerte)
        ▼
moverce_derecha ◄──────────────► moverce_izquierda
   │   │   │                         │   │   │
   │   │   └ línea(borde) → impulso_izquierda (350 ms) ─┐
   │   │      línea(borde) → impulso_derecha (350 ms) ──┘  (cada impulso vuelve a su moverce)
   │   │
   │   └ pelota desviada (|lateral|≥DESVIO): elige lado por signo de ball_x_mm
   │
   └ pelota cerca+centrada (profundidad≤CERCANIA && |lateral|≤CENTRADO)
              ▼
   PATEANDO_pausa_inicial (200 ms) → ALINEAR_arco_opp (gira a apuntar al ARCO RIVAL) → PATEANDO_adelante (450 ms, avanzar_patear hacia el arco rival)
              ▼
   PATEANDO_pausa (1000 ms) → PATEANDO_atras (retroceso recto hasta ver línea + safety 4 s)
              ▼
   avanzar_despues_de_patear (1000 ms) → moverce_derecha  (retoma patrulla)
```

- **Patrulla (`moverce_*`):** `ad/aiproporcional()` hace strafe lateral CON corrección de
  rumbo en 3 bandas según el `error` (= heading − heading_inicial). Sin pelota patrulla a
  `pd=1`; con pelota desviada `pd=1.5` (corrige más fuerte).
- **Decisión por la pelota:** cerca+centrada → patea; desviada → va al lado de la pelota;
  banda muerta (entre centrado y desvío, o centrada-pero-lejos) → para.
- **Rebote en el borde:** al ver línea, impulso temporizado de 350 ms al lado OPUESTO para
  no quedarse trabado oscilando (igual que el 2025).
- Los estados DELANTERO del 2025 (girar/apuntar/centrar/patear largo) **no se portan acá**:
  eso es `centralmix`.

## 6. El mapeo 2025 → 2026 (la traducción que hace el adaptador)

| Dato | El arquero 2025 lo leía de… | En arqueromix viene de… | Adaptación |
|---|---|---|---|
| ¿Ve pelota? | `Xp != 0` (cámara local) | `ball_visible` (snapshot TOP) | directo |
| Seguir la pelota | signo+magnitud de `Yp` (píxeles) | **`angulo_pelota_deg = atan2(x,y)`** | **FIX 2026-06-21:** se sigue por ÁNGULO (como centralmix), robusto a la escala. `|áng|>CENTRADO_DEG(8°)`→strafe al lado; `áng>0`→derecha. ⚠️ RE-VERIFICAR SIGNO |
| Cerca para despejar | `Xp<=140` (cámara) | `dist=√(x²+y²) ≤ CERCANIA_MM` **+ `|áng|≤KICK_DEG`** | distancia euclídea + ángulo. ⚠️ `CERCANIA_MM` (250) es el knob de tuning (escala sin calibrar) → RE-TUNEAR |
| Rumbo (`error`) | **BNO local** del arquero | **heading del snapshot TOP** | `error = heading − heading_inicial`; sin BNO local |
| Línea (3 sensores) | `s1/s2/s3` analógicos locales | **DOWN** (`line_present/depth`) | 3 sensores → una señal de DOWN. ⚠️ pierde el "qué lado", RE-TUNEAR |
| Árbitro | (no tenía) | `match_running` (snapshot) | **nuevo** gate GO/STOP |
| Motores (salida) | `analogWrite/digitalWrite` inline | primitivas `amix_motors` | mismos valores, pines 2026 |

## 7. Lo que falta validar en banco (TASK-114) — ⚠️ compila ≠ anda

1. **Sentido de cada motor / primitiva.** El 2025 arquero era ROBOT1 con un layout de
   pines; arqueromix usa los pines 2026 (R1=R2). Una primitiva `adproporcional` (strafe
   derecha) podría salir a la izquierda o invertida. **Verificar cada primitiva con las
   ruedas al aire** antes de la FSM (`amix_set_motor` suelto por índice).
2. **Signo lateral de la pelota.** `ball_x_mm>0`→derecha es la elección intuitiva; si el
   arquero va para el lado contrario de la pelota, invertir en `ball_a_la_derecha()`.
3. **Seguimiento por ÁNGULO (FIX 2026-06-21).** El seguimiento de la pelota es por
   `angulo_pelota_deg` (robusto a la escala), no por mm. Tunables: `AMIX_TOL_CENTRADO_DEG`
   (8° — banda muerta angular: más chico = sigue más agresivo), `AMIX_TOL_KICK_DEG` (30° —
   ángulo para despejar) y **`AMIX_TOL_CERCANIA_MM`** (250 — distancia para despejar, EL knob
   principal porque la escala del snapshot está sin calibrar: subir si nunca despeja). Ver §13.
4. **Heading del TOP.** Confirmar que `g_aio.heading_deg`/`heading_valid` llegan sanos del
   snapshot (mirar el monitor). Si el heading no es confiable, la patrulla usa la banda
   centrada (no corrige rumbo) — degrada, no rompe.
5. **Línea desde DOWN.** El 2025 distinguía borde (s1|s2) vs vuelta-a-línea (s1|s2|s3); acá
   ambos son `line_present`. Confirmar que DOWN reporta la línea lateral del arco a tiempo.
6. **Comm.** Confirmar que `g_aio` se puebla con datos reales de TOP/DOWN (telemetría).

## 8. Decisiones de diseño (y por qué)

- **Heading por SNAPSHOT del TOP (no BNO local).** Pedido explícito de Virginia y correcto:
  las CENTRAL 2026 no traen BNO; el rumbo viene de arriba. (centralmix había dejado un BNO
  local como default — acá se corrige.)
- **`match_running` agregado.** El 2025 arrancaba solo; en RCJ no se mueve hasta el START.
- **Timeout de seguridad en el retroceso.** El 2025 `PATEANDO_atras` no tenía timeout
  (retrocedía hasta ver blanco; si no llegaba, se colgaba). Se agrega un tope de 4 s.
- **Manejo directo de motores (no el mixer 2026).** Que sea como el 2025 para debuggear fácil.
- **Sin `world_model`.** Variables planas (`g_aio`) como las globales del 2025.
- **Línea de DOWN, sin "qué lado".** El DOWN agrega los 32 sensores en una señal; el 2025
  distinguía s1/s2/s3. Hoy ambos branches (borde / vuelta) usan `line_present`. Se puede
  refinar por `line_angle_deg` a futuro (como hizo centralmix por sectores).

## 12. Límite honesto: arqueromix es MODO NO-REGRESIÓN, no reemplazo del arquero R2

El contrato plano `AmixIO` **recorta** la línea a 3 campos (`line_present`/`line_angle_deg`/
`line_depth`), pero el `LineStatusV2` del DOWN trae 10+ campos. En particular **NO expone**:
- `cross_track_mm` — el arquero R2 que YA anda en banco lo usa para hacer **strafe paralelo**
  a la línea por error lateral real; arqueromix no lo tiene (patrulla por signo de pelota, como 2025).
- `IMMINENT_EXIT` (event flag) — freno anticipado de borde; arqueromix solo ve `line_present`.
- `data_valid` — con `data_valid=0`, `line_angle_deg` devuelve 0 (que también es "línea al
  frente"). Para el arquero el riesgo es bajo (no usa el ángulo, solo present/depth), pero
  conviene saberlo. ⚠️ OJO: `line_depth` acá = **conteo de sensores** (0..32), NO `penetration_mm`
  como en `world_model_get_line_depth()` del CENTRAL clásico (mismo nombre, dos semánticas).

➡️ **arqueromix reproduce el arquero CAMPEÓN 2025 (simple, robusto) como modo de comparación /
no-regresión, NO reemplaza al arquero R2** (que además tiene Y-hold de profundidad, pose XY por
paredes y escape acotado). Si en banco se quiere paridad, el camino es **ampliar `AmixIO` +
`apply_down_line`** con `cross_track_mm`/`cross_track_valid` (helper `lsv2_cross_track_mm` ya
existe en `line_view.h`) e `imminent_exit` — trabajo concreto, no opcional, para esa paridad.
Decisión del equipo tras validar el port base (TASK-114).

## 13. FIX 2026-06-21 — seguimiento por ángulo (la cámara veía la pelota pero no se movía)

**Síntoma (banco Virginia):** los motores andaban bien y la cámara detectaba la pelota (su LED
prendía), pero el arquero **no hacía ningún movimiento** para seguirla ni despejarla.

**Causa raíz (comparando con cómo usa la cámara el delantero `centralmix`):** el FSM decidía con
`ball_x_mm`/`ball_y_mm` **crudos en mm** y umbrales en mm (CENTRADO=30, DESVIO=50, CERCANIA=140).
Pero la escala del snapshot está **sin calibrar** (`CAMERA_UNIT_TO_MM=10` en el TOP), así que una
pelota más o menos al frente caía en la **banda muerta** (ni "cerca+centrada" ni "desviada") → el
FSM ejecutaba `parar()`, que pisaba la patrulla → **el robot se congelaba justo al ver la pelota.**
`centralmix` no tiene este problema porque sigue la pelota por su **ÁNGULO** (`atan2`), que NO
depende de la escala.

**Fix (igual que centralmix):** `amix_comm` ahora calcula `angulo_pelota_deg = atan2(ball_x, ball_y)`
y el FSM **sigue la pelota por ángulo**: si `|áng| > 8°` strafe hacia su lado (la sigue); si está
alineada y lejos, mantiene posición; despeja si `dist ≤ CERCANIA_MM` y `|áng| ≤ 30°`. La banda
muerta angular es angosta → trackea cualquier pelota off-center. Compila SUCCESS.

**Cómo verificar en banco (Virginia):**
1. Con el robot quieto y `match_running` en GO, mové la pelota a la **derecha** → el arquero debe
   strafear a la derecha; a la **izquierda** → a la izquierda. (Si va al revés → invertir
   `ball_a_la_derecha()` o revisar el signo del strafe.)
2. Acercá la pelota al frente → al quedar cerca debe arrancar la **secuencia de despeje**. Si nunca
   despeja, **subí `AMIX_TOL_CERCANIA_MM`** (250→400…) mirando la telemetría; si despeja de muy
   lejos, bajalo.
3. Si querés que siga la pelota más agresivo (menos zona de "quieto"), bajá `AMIX_TOL_CENTRADO_DEG`
   (8°→5°). Tunables todos en `amix_config.h`.

## 14. Banco 2026-06-21 (Virginia) — la vuelta de 180° al inicio + potencia del despeje

**Reporte:** con el fix de la cámara el arquero YA reacciona a la pelota, pero (a) al arrancar
**da una vuelta de 180°** y queda con la cámara frontal mirando NUESTRO arco (no el del rival),
y como queda al revés no se pudo verificar si seguía la pelota para el lado correcto; (b) el
despeje pega con demasiada potencia.

**(a) El 180° = DERIVA DE RUMBO (yaw parásito del strafe).** El strafe lateral anda bien, pero el
omni-3 en strafe abierto **rota solo** (yaw parásito — ver skill `dinamica-omni-3-ruedas`). El
2025 lo contrarrestaba con la corrección de rumbo de 3 bandas (modula la rueda trasera según el
`error` de heading). En este robot esa corrección o está **al revés** (mismo tipo de bug que el
`OMEGA_SIGN` del mixer 2026) o **no tiene heading válido** para corregir → el robot deriva hasta
dar 180°. Cambios:
- **Arranque más suave:** `impulso_inicial` bajado (M1/M2 90→70, M3 153→110) para que no dé un
  tirón al iniciar (153 además rozaba el techo térmico).
- **Perilla para dar vuelta la corrección:** ✅ **RESUELTO 2026-06-21** — el signo correcto es **-1**
  y YA es el DEFAULT del programa base (ver §15). El env `_flip` se removió (ya no hace falta;
  el fallback al signo viejo queda como `-DARQMIX_HEADING_SIGN_OLD`, solo diagnóstico).
- ⚠️ **La corrección SOLO funciona con heading válido del TOP.** Confirmar en el monitor que
  `heading_valid=1` (TOP `top_robot2_pri` con el BNO sano). Si el heading no llega, NINGÚN signo
  corrige y el strafe deriva igual → ahí el tema es el heading del TOP, no esta perilla.

**(b) Potencia del despeje bajada** (pedido Virginia): `AMIX_PATAD_M1` 250→180, `AMIX_PATAD_M2`
150→120, `AMIX_ATRAS` (retroceso) 150→120. Si queda corto y no despeja, subir `PATAD_M1`.

**Cómo verificar (Virginia):**
1. Flashear `central_robot2_arqueromix`. Mirar al arrancar: ¿sigue dando 180°?
2. Si SÍ se da vuelta → flashear `central_robot2_arqueromix_flip` y comparar. Si con el flip
   queda derecho → era el signo (lo dejamos así). Si con AMBOS se da vuelta → el heading del TOP
   no está llegando válido (revisar el monitor) o el strafe deriva sin corrección posible.
3. Con el arquero ya derecho, recién ahí se puede verificar el seguimiento izq/der de la pelota
   (lo que no se pudo por quedar al revés).

## 15. Banco 2026-06-21 (cierre) — signo de rumbo VALIDADO + pateo con rampa

**(1) Signo de rumbo — VALIDADO.** Virginia confirmó en banco que la versión con la corrección
INVERTIDA (signo -1) deja el arquero **derecho** (sin la vuelta de 180°). Ese signo quedó como el
**DEFAULT del programa base** (`AMIX_HEADING_CORRECT_SIGN = -1`). Se removió el env `_flip`: ahora
se flashea directo `central_robot2_arqueromix`. (Fallback al signo viejo: `-DARQMIX_HEADING_SIGN_OLD`.)

**(2) Pateo con RAMPA (como el delantero).** Virginia: "encuentra bien la pelota pero no apunta como
corresponde" → copiar la patada del delantero (con rampa de aceleración). El pateo del arquero
pasó de **PWM fijo asimétrico** (180/120, que veraba → no apuntaba) a una **rampa simétrica** igual
a la del delantero (`centralmix`): sube de 0 a `AMIX_KICK_VEL_FINAL` (180) de a `AMIX_KICK_PASO`
(20) cada `AMIX_KICK_INTERVALO_MS` (10 ms) → llega al pico en ~90 ms. Patrón `M1=+vel, M2=-vel, M3=0`
= avance **RECTO** al frente (por eso ahora "apunta"). La rampa se resetea en `parar()` → cada
despeje arranca de 0. Implementado en `amix_motors.cpp::avanzar_patear` (port de la del delantero).

**Tunear el despeje:** si no llega a despejar, subir `AMIX_KICK_VEL_FINAL` (180→210…). Si sigue sin
apuntar bien (patea de costado), achicar `AMIX_TOL_KICK_DEG` (30°→20°) para que solo dispare con la
pelota más al frente. Todo en `amix_config.h`.

## 16. Inicio del programa — HOMING al área chica (banco Virginia 2026-06-21)

**Cambio pedido:** en vez de empezar a patrullar directo, al arrancar el arquero **se posiciona en
su arco** primero. Secuencia nueva (reemplaza al `impulso_inicial` viejo):

```
inicio_retroceder  → va HACIA ATRÁS (hacia el arco propio) hasta DETECTAR la línea del área chica
        │ (line_present de DOWN)        [safety: si nunca la ve, sale tras 4 s]
        ▼
inicio_avanzar     → avanza un poco A CIEGAS (NO lee los sensores) durante ~400 ms
        │
        ▼
moverce_derecha    → recién ACÁ empieza a patrullar
```

- **`inicio_retroceder`** (`amix_fsm.cpp`): llama `retroceder_inicio()` (primitiva DEDICADA con PWM
  propio `AMIX_INICIO_RETRO_PWM=100` y dirección flippable) hasta que `linea()` da true (DOWN ve el
  blanco del área). Safety `AMIX_T_INICIO_RETRO_SAFETY` = **50 s TEMPORAL** (banco Virginia, para
  observar el retroceso; bajar a ~4 s cuando ande).
- **`inicio_avanzar`**: llama `avanzar_inicio()` (velocidad propia `AMIX_INICIO_AVANCE_PWM=75`, recto al
  frente, lejos del fondo) para SALIR de la línea del área. **FIX 2026-06-21 (banco Virginia): el avance
  ya NO termina por reloj fijo** — antes eran 400 ms fijos y el arquero a veces quedaba muy cerca del
  fondo / medio metido en el área. Ahora sale cuando **cumplió el impulso MÍNIMO (`AMIX_T_INICIO_AVANCE_MIN`
  =400 ms) Y ya NO pisa la línea** (`!linea()`), o por **tope de seguridad** (`AMIX_T_INICIO_AVANCE_SAFETY`
  =1200 ms, para no quedarse trabado si la línea nunca se "apaga"). Así NUNCA arranca a patrullar pisando
  el área. Después → `moverce_derecha`. (El MÍNIMO cubre un parpadeo inicial de la línea; recién después
  mira `!linea()`.)
- El `match_running` (árbitro) gobierna: el homing arranca con el **GO**. ✅ **El sentido del
  retroceso quedó VALIDADO con el env BASE** (banco Virginia 2026-06-21: va para atrás bien, no
  hizo falta `_retroflip`).
- ✅ **RE-HOMING en CADA GO (banco Virginia 2026-06-21).** Antes el homing corría una sola vez (el
  primer GO) porque el FSM no se reiniciaba entre STOP y GO sin apagar la batería. Ahora se detecta
  el flanco STOP→GO en `amix_fsm_tick` (`s_was_running`) y **cada GO reinicia el FSM a
  `inicio_retroceder`** → el arquero vuelve a buscar su línea cada vez que arranca.

⚠️ **Si al GO el robot va hacia ADELANTE en vez de atrás** (banco Virginia 2026-06-21): dos causas
posibles — (a) arranca **sobre una línea** → `line_present` ya es true → saltea el retroceso y pasa
directo a `inicio_avanzar` (avanza); (b) el **retroceso está invertido** en este robot. Test: flashear
`central_robot2_arqueromix_retroflip` (`-DARQMIX_FLIP_INICIO_RETRO`) → si ahora va para atrás, era (b).
Si con ambos arranca yendo adelante apenas detecta línea, es (a) (arranca sobre el blanco).

**Tunear:** `AMIX_T_INICIO_AVANCE_MIN` (400 ms) = impulso mínimo del avance de salida (subir si la
línea parpadea y sale antes de despegar). `AMIX_T_INICIO_AVANCE_SAFETY` (1200 ms) = tope de seguridad:
si sigue pisando línea, patrulla igual (subir si a veces queda pisando; bajar si avanza de más hacia el
campo). `AMIX_INICIO_AVANCE_PWM` (75) = VELOCIDAD del avance del homing (subir hacia 85 si stuttea/no
arranca; NO bajar de 70 = piso de las delanteras). `AMIX_INICIO_RETRO_PWM` (100) = velocidad PROPIA del
retroceso de inicio. `AMIX_T_INICIO_RETRO_SAFETY` = 50 s TEMPORAL — bajar a ~4 s cuando el arranque ande.
Motores: TODO se mueve con PWM (`analogWrite` vía `amix_set_motor`); el retroceso va a PWM 100/255.

## 17. Salida de la LÍNEA LATERAL — movimiento a ciegas + no volver (banco Virginia 2026-06-21)

**Problema:** patrullando hacia un costado, al llegar a la línea lateral el arquero se quedaba
**enganchado** (oscilaba en la línea): el rebote no la despegaba del todo, o la pelota la tiraba de
vuelta hacia la línea.

**Fix (lo mismo que hace el homing al ver el blanco):** al tocar la línea lateral, hace un
**movimiento A CIEGAS** (sin leer ningún sensor) hacia el lado OPUESTO durante `AMIX_T_SALIR_LINEA`
(≈450 ms, parecido al avance del homing), y después **patrulla para el otro lado SIN volver
enseguida** a la línea que dejó.

Estados nuevos (reemplazan a `impulso_der/izq`):
- **`salir_linea_izq`** (tocó la línea de la DERECHA): strafe IZQUIERDA a ciegas → `moverce_izquierda`.
- **`salir_linea_der`** (tocó la línea de la IZQUIERDA): strafe DERECHA a ciegas → `moverce_derecha`.

**"No vuelve a la derecha" (commit):** al terminar la salida, se arma una ventana `s_commit_until_ms
= now + AMIX_T_PATRULLA_COMMIT` (≈1 s). Durante esa ventana, la patrulla **ignora el lado de la
pelota** (no flipea de dirección) → no se vuelve a meter en la línea que acaba de dejar. El despeje
(pelota cerca+centrada) y el rebote de la OTRA línea siguen activos. Pasada la ventana, vuelve a
seguir la pelota normal.

**Tunear:** `AMIX_T_SALIR_LINEA` (450 ms) = cuánto se aleja de la línea a ciegas (subir si sigue
enganchándose). `AMIX_T_PATRULLA_COMMIT` (1000 ms) = cuánto patrulla el otro lado antes de volver a
mirar la pelota (subir si vuelve muy rápido hacia la línea).

### 17.1 Refuerzo 2026-06-21 (banco Virginia: "a veces toca, sale y vuelve a meterse")

Dos causas: (1) la salida no tenía suficiente impulso para despegarse; (2) como el DOWN NO distingue
QUÉ línea es (da una sola señal), si la salida no despejaba del todo, el `moverce` re-detectaba la
MISMA línea y rebotaba para el lado contrario = **de vuelta a la línea**. Fixes:
- **Más impulso en la salida:** la salida a ciegas usa `AMIX_PD_SALIR=1.9` (más fuerte que la
  patrulla `pd=1.0`). Subir si todavía no se despega.
- **Rebote inteligente durante el commit:** mientras está en commit (recién salió), si vuelve a ver
  línea es la MISMA que dejó → **sigue saliendo para el mismo lado** (no rebota de vuelta). Pasado el
  commit, el rebote es normal (lado opuesto). Así no se mete de nuevo en la línea que recién dejó.
- **Tiempos revisados:** cada estado usa su propio timer (`millis_inicio_estado` al entrar), el
  commit es `millis() < s_commit_until_ms`. NO se encontró bug de tiempo; el problema era la
  dirección del rebote, no los tiempos.

### 17.2 La patrulla rebota por el ARCO PROPIO, no por la línea (pedido Virginia 2026-06-21)

**Cambio pedido (decisión Virginia: REEMPLAZAR la línea en la patrulla).** En vez de rebotar contra
la **línea**, la patrulla rebota cuando el **ARCO PROPIO** (que la cámara trasera ve por detrás, vía
snapshot del TOP) llega a cierto **ángulo** = el arquero llegó al **borde de su arco** → se va al otro
lado. Misma mecánica de rebote (reúsa los estados `salir_linea_*` + el commit), sólo cambia **qué lo
dispara**.

**Geometría.** El arco propio está DETRÁS del arquero (mira al campo) → `goal_own_angle ≈ ±180°` cuando
está CENTRADO en su arco. El "desvío" se mide respecto de 180°: `rear_goal_dev = wrap180(goal_own_angle
− 180) × SIGN` (≈0 centrado, crece hacia un lado al correrse). **Borde** = `|rear_goal_dev| ≥
AMIX_TOL_ARCO_OWN_DEG`. Helpers `borde_arco_der()` / `borde_arco_izq()` en `amix_fsm.cpp` (sólo
disparan con `goal_own_visible`).

**FALLBACK A LÍNEA cuando la cámara NO ve el arco (fix 2026-06-21).** En `moverce_*`, el borde es:
`en_borde = goal_own_visible ? borde_arco_*() : linea()`. Es decir: **rebota por el ARCO cuando la
cámara lo ve** (pedido Virginia) **y por la LÍNEA cuando no lo ve**. Así la patrulla **SIEMPRE rebota**
y nunca se va de largo. (Antes, en modo puro-arco, si la cámara no veía el arco NO rebotaba nada → el
arquero se iba caminando y PARECÍA que el programa no estaba cargado; este fix lo resuelve.) Para
diagnosticar dónde está rebotando: si rebota **angosto** (al borde del arco) está usando la cámara; si
rebota **ancho** (recién en las líneas de la cancha) la cámara no ve el arco y está en fallback.

**⚠️ Notas.** `goal_own` NO está validado en banco; depende de la **calibración LAB** de las cámaras N6
y de que el robot **arranque mirando a la cancha** (si arranca girado, la polaridad del TOP queda
invertida y el seguimiento del arco se rompe). Fallbacks por flag: `-DARQMIX_PATRULLA_LINEA` = patrulla
SÓLO por línea (ignora el arco). El homing y el retroceso del despeje usan la línea siempre.

**Tunear:** `AMIX_TOL_ARCO_OWN_DEG` (30°) = umbral de "borde" (más chico = patrulla más angosta, rebota
antes; más grande = más ancha) — EL knob principal. `AMIX_ARCO_OWN_SIGN` (signo del desvío): si rebota
en el borde EQUIVOCADO o no rebota, invertir con `-DARQMIX_FLIP_ARCO_OWN`.

**Cómo verificar (Virginia):** flashear `central_robot2_arqueromix`. Con `match_running` en GO y el
arquero centrado en su arco, debería patrullar de lado a lado y **rebotar al llegar a cada borde del
arco** (sin tocar la línea). (1) ¿Rebota donde corresponde el borde del arco? Ajustá `AMIX_TOL_ARCO_OWN_DEG`.
(2) ¿Rebota al revés / no rebota? Probá `-DARQMIX_FLIP_ARCO_OWN`. (3) ¿Se va de largo sin rebotar? La
cámara no está viendo el arco (`goal_own_visible=0`) → es el riesgo conocido; volvé a la línea con
`-DARQMIX_PATRULLA_LINEA` o validamos la cámara primero.

### 17.3 Patrulla más angosta + PROFUNDIDAD por línea (no meterse al área) — banco Virginia 2026-06-21

**Problema:** el arquero se mete al **área chica** (deriva hacia ATRÁS, hacia su arco) durante la patrulla.
Decisión de Virginia (confirmada): el problema es de **profundidad** (adelante-atrás), NO lateral. Y la
**cámara NO sirve para distancia** (verificado por workflow: pierde el arco JUSTO cuando está cerca —
desenfoque/fuera de FOV/timeout, ~20-30% confiable). → La señal **confiable de profundidad es la LÍNEA**.

**Dos cambios:**
1. **Recorrido lateral más angosto:** `AMIX_TOL_ARCO_OWN_DEG` 30°→**20°** → la patrulla queda más
   centrada frente al arco (rebota antes). Esto usa la cámara para lo LATERAL (ángulo), que sí es confiable.
2. **Control de PROFUNDIDAD por línea** (`AMIX_PROFUNDIDAD_POR_LINEA`, default ON): en `moverce_*`, si el
   arquero **VE el arco** (la patrulla ya cubre lo lateral por el ángulo, NO usa la línea para rebotar de
   lado) **Y** detecta la **línea** → derivó hacia atrás → pasa a **`inicio_avanzar`** = avanza recto al
   frente HASTA despegar de la línea → vuelve a patrullar. Reúsa el estado del homing. Cuando NO ve el
   arco, la línea sigue siendo el rebote LATERAL (fallback) y este control NO actúa. Apagar:
   `-DARQMIX_NO_PROFUNDIDAD`.

**Por qué es confiable:** el control de profundidad es 100% línea (sin cámara). La regla "si ve arco, la
línea es profundidad; si no ve arco, la línea es rebote lateral" evita el conflicto: con el arco visible
lo lateral lo resuelve el ángulo, así que la línea queda libre para profundidad. El avance va RECTO al
frente = hacia el campo, lejos del fondo = saca del área.

**⚠️ Límite honesto:** cuando NO ve el arco (cámara perdida), NO hay control de profundidad (la línea se
usa para lo lateral). En ese modo degradado el arquero puede derivar atrás. Es el costo de no tener una
señal de profundidad independiente de la cámara/línea-lateral. Con el arco visible (caso normal) sí protege.

**Cómo verificar (Virginia):** flashear. (1) ¿La patrulla quedó más angosta/centrada? Ajustá
`AMIX_TOL_ARCO_OWN_DEG`. (2) Empujá el arquero hacia atrás (al área) mientras ve el arco: al detectar la
línea del fondo debe **avanzar al frente y salir**, no quedarse adentro. (3) Si avanza de más / oscila
adelante-atrás, es el `inicio_avanzar` (mismo knob `AMIX_T_INICIO_AVANCE_*`). (4) Apagar todo esto:
`-DARQMIX_NO_PROFUNDIDAD`.

## 18. Despeje DIRIGIDO al arco rival + arcos por ROL, no por color (pedido Gustavo 2026-06-21)

**Cambio pedido.** Que (1) la determinación de cuál arco es el PROPIO y cuál el RIVAL viva en la
placa **TOP** (ya era así: `goal_polarity` —"el arco al frente del robot es el rival"—, fijado al
arranque por un latch), (2) la placa CENTRAL (este programa) **nunca pregunte por COLOR**, y (3) el
arquero, al despejar, **apunte al ARCO RIVAL (`goal_opp`) y patee alineado hacia ahí**, sin importar
el color.

**Qué se hizo (sólo en `src/arqueromix/`, SIN tocar el TOP ni `shared/`):**

- **`amix_io.h` / `amix_comm.cpp` — fuera el color.** Antes el snapshot se copiaba a campos
  `goal_yellow_*/goal_blue_*` con un `#ifdef ARQMIX_ATTACK_BLUE`. Ahora se copia DIRECTO a
  `goal_opp_*` (rival) y `goal_own_*` (propio), tal como vienen **ya resueltos** del TOP. El arquero
  no nombra ni invierte colores: el TOP es el único dueño de "qué arco es cuál" (`goal_polarity`).
- **`amix_motors.{h,cpp}` — primitiva `girar()`.** Rotación pura en el lugar (3 ruedas al mismo
  sentido). Sirve para apuntar el frente al arco rival (el despeje sale recto al frente).
- **`amix_fsm.{h,cpp}` — estado nuevo `ALINEAR_arco_opp`.** Se intercala en la secuencia de despeje,
  ENTRE `PATEANDO_pausa_inicial` y `PATEANDO_adelante`:

  ```
  ... pelota cerca+centrada → PATEANDO_pausa_inicial (200 ms)
        ▼
  ALINEAR_arco_opp:  ¿ve el arco rival? → gira hasta tenerlo al frente (|áng| ≤ AMIX_TOL_ARCO_OPP_DEG)
        │             ¿no lo ve / ya alineado / timeout AMIX_T_ALINEAR_OPP? → patea igual (recto)
        ▼
  PATEANDO_adelante (avanzar_patear) → ahora va DIRIGIDO al arco rival
  ```

  **Fallback robusto:** si no ve el arco rival, ya está alineado, o se acaba el tiempo de giro,
  patea igual (recto al frente = comportamiento del arquero 2025). Nunca se cuelga apuntando.

**Por qué "apuntar al rival" y no "lejos del propio":** despejar hacia el arco contrario aleja la
pelota de nuestro arco Y es ofensivo, y usa `goal_opp` (cámara FRONTAL, la misma que valida el
delantero). Es lo que pidió Gustavo.

**Tunear (en `amix_config.h`):** `AMIX_TOL_ARCO_OPP_DEG` (12° — cuán fino apunta antes de patear),
`AMIX_GIRO_ALINEAR_PWM` (90 — fuerza del giro), `AMIX_T_ALINEAR_OPP` (300 ms — tope de giro, arranca
conservador), y el env `central_robot2_arqueromix_giroflip` (`-DARQMIX_FLIP_GIRO_ALINEAR`) si gira
para el lado CONTRARIO al arco.

**A validar en banco** *(la cámara trasera/`goal_own` se da por validada por decisión del equipo
2026-06-21; si falla, se debuggea ahí — igual el despeje usa `goal_opp`, cámara frontal):*
1. Con el arco rival visible, el arquero **gira para apuntarle** y el despeje sale **hacia el arco
   rival** (no a un costado). Si gira al revés → `-DARQMIX_FLIP_GIRO_ALINEAR`.
2. Sin ver el arco rival, **patea recto** sin demorarse (fallback).
3. El giro de alineación **no saca al arquero de su arco** de forma peligrosa: si pasa, bajar
   `AMIX_T_ALINEAR_OPP` y/o `AMIX_GIRO_ALINEAR_PWM`.
4. Compila ✅ (`pio run -e central_robot2_arqueromix` → SUCCESS). **Compila ≠ anda.**

## 9. Cómo compilar, flashear y volver atrás

```bash
# Compilar:
pio run -e central_robot2_arqueromix
# Flashear a la CENTRAL de R2 (la de Virginia):
pio run -e central_robot2_arqueromix -t upload
# Heading por OTOS en vez del TOP:
#   agregar -DARQMIX_HEADING_OTOS al build_flags del env
# VOLVER al arquero de competencia (descarta arqueromix):
pio run -e central_robot2_arquero -t upload
```
El env `central_robot2_arqueromix` **extiende** `central_robot2` (board Teensy 4.1) y solo
cambia `build_src_filter` para compilar `arqueromix/ + shared/`. **TOP y DOWN no se tocan:**
seguí con `top_robot2_pri` y `down_robot2`.

## 10. Cómo continuar (orden sugerido)

1. **Banco — primitivas de motor** una por una, ruedas al aire (TASK-114 paso 1).
2. **Banco — comm**: ver `g_aio` poblado en vivo (heading, pelota, línea).
3. **Banco — FSM completa** + re-tuneo de umbrales y signo lateral (§7).
4. Decisión: si anda → seguir mejorando acá; si no → volver a `src/central/` (nada perdido).

## 11. Referencias

- Código: `src/arqueromix/` (este directorio).
- Hermano delantero (el del viernes): `src/centralmix/` + `journal/2026-06-19-centralmix-port-delantero-2025.md`.
- Base 2025: `software/_deprecated-2025/robot-arquero/definitivo-arquero_6-9-2026`.
- Análisis fiel 2025: `docs/internal/ANALISIS-FIEL-ARQUERO-2025.md` (la fuente del port).
- Validación de banco: `team-tasks/2026-06-21-task-114-validar-arqueromix-banco.md`.
- Journal: `journal/2026-06-21-arqueromix-port-arquero-2025.md`.
- Arquero 2026 actual (para comparar): `src/central/strategy.cpp` (FSM GK).
