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
| FSM 2025 del arquero portada (10 estados) | ✅ código escrito (port fiel) |
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
                                                          │ amix_fsm  │  (FSM arquero 2025: 10 estados)
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
| `amix_io.h` | `struct AmixIO` + `extern AmixIO g_aio`: variables planas (pelota, arcos, heading, línea, árbitro, timers, frescura). |
| `amix_comm.cpp/.h` | **Único que toca Serial.** Lee TOP (Serial7) y DOWN (Serial1) a 230400, decodifica con `shared/proto` + `line_view`/`pose_view`, y **llena `g_aio`**. Heading = snapshot del TOP. |
| `amix_fsm.cpp/.h` | La **FSM del ARQUERO 2025** portada fiel (10 estados). Lee `g_aio`, decide, llama primitivas de `amix_motors`. Agrega el gate `match_running` + un timeout de seguridad al retroceso. |
| `amix_motors.cpp/.h` | **Manejo directo 2025**: `adproporcional/aiproporcional/impulso_inicial/avanzar/avanzar_patear/patear_atras` + `amix_set_motor(idx,pwm)`. Escribe `analogWrite(PWM)`+`digitalWrite(INA/INB)`. Sin mixer, sin cinemática omni. |
| `amix_config.h` | Pines (R1/R2 2026), constantes 2025 (PWM proporcionales, impulsos, patada), tolerancias, tiempos, selector de heading. |
| `README.md` | Guía corta + comando de flasheo. |
| `DOCUMENTACION.md` | Este archivo. |

## 5. La máquina de estados del arquero (10 estados, port fiel del 2025)

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
   PATEANDO_pausa_inicial (200 ms) → PATEANDO_adelante (450 ms, avanzar_patear)
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
- **`inicio_avanzar`**: llama `avanzar()` durante `AMIX_T_INICIO_AVANCE=400 ms`, **sin chequear la
  línea** a propósito (para despegarse del blanco antes de patrullar). Después → `moverce_derecha`.
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

**Tunear:** `AMIX_T_INICIO_AVANCE` (400 ms) = cuánto se despega de la línea antes de patrullar.
`AMIX_INICIO_RETRO_PWM` (100) = velocidad PROPIA del retroceso de inicio (ya no comparte con el
despeje). `AMIX_T_INICIO_RETRO_SAFETY` = 50 s TEMPORAL — bajar a ~4 s cuando el arranque ande.
Motores: TODO se mueve con PWM (`analogWrite` vía `amix_set_motor`); el retroceso va a PWM 100/255.

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
