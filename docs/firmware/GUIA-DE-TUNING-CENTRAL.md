---
title: "Guía de tuning de la CENTRAL (cómo cambiar la estrategia en cancha)"
date: 2026-06-15
author: "Claude (Anthropic - Claude Opus 4.8 1M)"
status: vivo
tipo: guia
area: control
scope: src/central
---

# Guía de tuning de la CENTRAL

> **Para qué sirve esto.** Es la lista de TODAS las "perillas" que podés girar para
> cambiar cómo juega el robot, sin tener que leer el código. Están organizadas por
> **qué querés cambiar** (ej. "que el delantero pegue más fuerte"), no por nombre raro.
> Para cada perilla te decimos: cómo se llama exacto, en qué archivo y línea está, qué
> valor tiene hoy, qué hace en castellano, qué pasa si la subís y qué pasa si la bajás.
>
> Placa: **CENTRAL** (Teensy 4.1). Es la única placa que mueve los 3 motores.
> Todos los archivos están en
> `software/teensy/Soccer 2026/src/central/` y `.../src/shared/`.

---

## 1. Cómo usar esta guía + la REGLA DE ORO

Algunas palabras que vas a ver todo el tiempo:

- **PWM**: es "cuánta fuerza" le mandás a un motor, en una escala de 0 a 255. No es
  velocidad real: 70 de PWM puede mover una rueda mucho o poco según cuánta fricción ve.
- **mm/s**: milímetros por segundo. La velocidad "pedida". El robot la traduce a PWM.
- **Piso de PWM (deadband)**: el mínimo de PWM para que una rueda ARRANQUE bajo carga.
  Por debajo del piso, la rueda raspa y no se mueve. Cada rueda tiene su propio piso.
- **Strafe**: ir de costado (izquierda↔derecha) sin girar el cuerpo. Es como patrulla el arquero.
- **omega (ω)**: velocidad de giro del cuerpo del robot, en grados por segundo.
- **Gyro / BNO / heading**: el "rumbo", hacia dónde mira el frente del robot.
- **build flag / env**: una opción que se enciende al COMPILAR (no se cambia en vivo).
  Algunas perillas solo existen si se flasheó el firmware con cierto flag (lo aclaramos).

### ⭐ REGLA DE ORO

1. **Cambiá UN parámetro a la vez.** Si tocás tres juntos y mejora (o empeora), no sabés cuál fue.
2. **Anotá el valor viejo** antes de tocar. En un papel o en el journal. Siempre se puede volver.
3. **Probá en banco** (robot levantado sobre una caja, ruedas al aire, o en la cancha real)
   ANTES de un partido. Muchos de estos valores tienen "historia de banco": ya se probaron
   y se eligieron por una razón. Si la contamos, leela antes de pisarla.
4. **Subir el PWM NO es gratis.** Los motores son de 5 V corriendo a 7,4 V: **arriba de
   ~150 de PWM sostenido se queman.** Nunca pases de ~150 en ningún piso/cap de motor.
5. Si un parámetro dice "TUNEAR EN BANCO" o "needs_bench", es porque su valor todavía es
   un punto de partida, no una verdad medida. Tratalo con cuidado.

---

## 2. El delantero (ROBOT2)

El delantero juega en 4 momentos: **busca** la pelota → se **acerca** → la **empuja**
(no tiene pateador: "patear" = empujar a fondo por inercia) → y al arrancar el partido
hace un **saque**. Todos estos parámetros están en `src/central/strategy.cpp`.

> ⚠️ Hay una versión "de práctica sin gyro" del delantero (env `central_robot1_delantero_practica`,
> flag `-DATK_OTOS_NOGYRO`). Esa cambia algunos gatillos del empuje (los `ATK_NOGYRO_*`, ver el final
> de esta sección). En competencia/R2 esos no existen.

### 2.1 Buscar la pelota

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `ATK_SEARCH_SPIN_PWM` | strategy.cpp:161 | **40** | Con cuánta fuerza GIRA buscando la pelota. Es PWM crudo directo, es **el número real** que controla el giro de búsqueda hoy. | ~30 a ~70 | **Subir** = gira más decidido (puede pasar de largo la pelota). **Bajar de ~30** = puede no arrancar o girar a tirones (queda muy por debajo del piso 70). |
| `ATK_SEARCH_VY_MM_S` | strategy.cpp:150 | **200** mm/s | Cuánto avanza hacia adelante MIENTRAS gira buscando (solo si NO se compiló con `-DATK_SEARCH_SPIN_ONLY`; con ese flag pasa a 0 = gira en el lugar). | 0 a ~400 mm/s | **Subir** = barre la cancha más rápido pero puede chocar o pasar la pelota. **Bajar a 0** = gira casi en el lugar, más seguro, más lento. |
| `ATK_BALL_CONFIRM_MS` | strategy.cpp:164 | **200** ms | Cuánto tiempo SEGUIDO tiene que ver la pelota antes de salir a perseguirla. Filtra "falsos naranjas" de 1-2 frames (reflejos). | ~100 a ~500 ms | **Subir** = más seguro contra falsas alarmas, pero reacciona más lento. **Bajar** = reacciona más rápido pero puede salir disparado por un reflejo. |
| `ATK_SEARCH_OMEGA_DEG_S` | strategy.cpp:160 | 7 °/s | ⚠️ **PARÁMETRO MUERTO HOY.** Está definido pero el código NO lo usa (la búsqueda gira por `ATK_SEARCH_SPIN_PWM`, no por este). Tocarlo no cambia nada. | no aplica | Ninguno hoy. Lo dejamos en la lista para que sepas que **no es la perilla del giro de búsqueda** (esa es `ATK_SEARCH_SPIN_PWM`). |

> *Historia de banco (Elías, 2026-06-14):* el giro de búsqueda arrancó por `OMEGA` a 60 °/s,
> pero giraba tan rápido que pasaba de largo la pelota → 30 → 7. A 7 °/s las ruedas caían MUY
> por debajo del piso de PWM (70) y giraba a tirones. **Por eso terminaron usando PWM crudo
> (`ATK_SEARCH_SPIN_PWM`), que subieron de 30 a 40.**

### 2.2 Acercarse a la pelota

El robot frena suave: lejos va al máximo, cerca al mínimo, en el medio rampa.

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `ATK_APPROACH_MAX_SPEED` | strategy.cpp:167 | **400** mm/s | Velocidad máxima yendo derecho a la pelota cuando está LEJOS. | ~300 a ~600 | **Subir** = llega antes pero con menos control (puede empujarla de costado). **Bajar** = más controlado pero más lento (le roban la pelota). |
| `ATK_APPROACH_MIN_SPEED` | strategy.cpp:168 | **200** mm/s | Velocidad mínima cuando ya está CERCA. No baja de acá aunque esté pegado. | ~150 a ~300 | **Subir** = llega con más empuje aún cerca, menos preciso al frenar. **Bajar mucho** = puede no vencer el piso de PWM y quedarse clavado sin tocar la pelota. |
| `ATK_APPROACH_CLOSE_MM` | strategy.cpp:169 | **50** mm | A esta distancia o menos, va al mínimo. Marca dónde empieza a frenar. | ~30 a ~100 | **Subir** = frena desde más lejos (más suave). **Bajar** = mantiene velocidad alta hasta casi tocar (más agresivo). |
| `ATK_APPROACH_FAR_MM` | strategy.cpp:170 | **500** mm | A esta distancia o más, va al máximo. | ~300 a ~800 | **Subir** = solo va al máximo cuando está muy lejos (conservador). **Bajar** = llega al máximo antes (agresivo). |

> *Historia (v2, 2026-06-11, spec Gustavo = movimiento 2025):* `ATK_APPROACH_MAX_SPEED`
> bajó de 600 a 400 a propósito. La idea es **acercarse lento; la potencia fuerte va en el EMPUJE.**

### 2.3 Empujar ("patear" sin pateador)

Cuando la pelota está cerca y apuntada al arco, se compromete al empuje: full adelante
por un tiempo fijo, **sin re-mirar la pelota** (a esa distancia la tapa el paragolpes),
y después retrocede un poco para despegarse.

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `ATK_PUSH_SPEED_MM_S` | strategy.cpp:195 | **700** mm/s | Velocidad del empuje. **Acá va la potencia del "pateo".** Es la velocidad más alta de todo el delantero, a propósito. | ~500 a ~800 (cerca del techo físico) | **Subir** = pega más fuerte, la pelota llega más lejos (mejor gol), pero menos control y más riesgo de cruzar la línea del área. **Bajar** = empuje más débil, menos gol. |
| `ATK_PUSH_MS` | strategy.cpp:196 | **500** ms | Cuánto dura el empuje a fondo (a ciegas). | ~300 a ~700 | **Subir** = recorre más (mejor gol, más riesgo de meterse en el área). **Bajar** = empujón más corto y controlado. |
| `ATK_KICK_DIST_MM` | strategy.cpp:187 | **80** mm | Distancia gatillo del empuje (modo CON gyro): pelota más cerca que esto + apuntando al arco = empuja. | ~60 a ~120 | **Subir** = dispara desde más lejos (menos seguro de tenerla justo enfrente). **Bajar** = espera tenerla casi pegada (las cámaras la pierden contra el paragolpes). |
| `ATK_KICK_ANGLE_DEG` | strategy.cpp:188 | **12** ° | Cuán bien tiene que apuntar el frente al arco antes de empujar (modo CON gyro). | ~8 a ~20 | **Subir** = empuja aunque esté algo torcido (manda la pelota desviada). **Bajar** = exige apuntar muy fino (casi nunca dispara si el rumbo no es muy ajustado). |
| `ATK_PUSH_BACK_SPEED_MM_S` | strategy.cpp:197 | **300** mm/s | Velocidad del retroceso corto después de empujar (se despega de pelota y línea). | ~200 a ~400 | **Subir** = se despega más rápido pero se aleja más de la jugada. **Bajar** = se separa poco, puede quedar pegado. |
| `ATK_PUSH_BACK_MS` | strategy.cpp:198 | **250** ms | Cuánto dura ese retroceso antes de volver a buscar. | ~150 a ~400 | **Subir** = más margen, pierde tiempo lejos. **Bajar** = vuelve a buscar antes, puede quedar encima de la pelota. |

> *Historia (v2, 2026-06-11):* todo este bloque copia el "PATEANDO" del robot 2025, que
> pateaba con rampa a ~94% de PWM durante 500 ms y retrocedía ~200 ms. Por eso 700 mm/s, 500 ms y 250 ms.

### 2.4 Rodear la pelota (ponerse detrás) y alinearse al arco

Si la pelota NO está en línea con el arco, el robot primero la **rodea** para encararla bien.

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `ATK_ATTACK_LINE_TOL_DEG` | strategy.cpp:185 | **30** ° | Si el ángulo (pelota↔arco) es menor a esto, está "alineado" y va directo; si es mayor, rodea primero. | ~20 a ~40 | **Subir** = encara directo aunque esté algo desviada (más rápido, puede empujar para un costado). **Bajar** = rodea más seguido para encarar bien (más preciso, más lento). |
| `ATK_BEHIND_BALL_GAP_MM` | strategy.cpp:184 | **120** mm | Cuánta separación deja con la pelota cuando se pone detrás de ella. | ~100 a ~150 | **Subir** = se posiciona más lejos detrás (más margen). **Bajar** = se pega más (puede tocarla mientras rodea y descolocarla). |
| `ATK_POSITION_REACHED_MM` | strategy.cpp:186 | **80** mm | Cuán cerca del punto-objetivo detrás de la pelota tiene que llegar para darse por "posicionado". | ~50 a ~120 | **Subir** = se conforma con estar más o menos detrás (más rápido, menos preciso). **Bajar** = exige llegar al punto exacto (más preciso, puede quedar "puliendo"). |
| `ATK_POSITION_MAX_SPEED` | strategy.cpp:189 | **400** mm/s | Velocidad máxima mientras rodea. | ~300 a ~500 | **Subir** = rodea más rápido (puede descolocar la pelota). **Bajar** = rodea más tranquilo y preciso, más lento. |

> *Historia (v2):* `ATK_POSITION_MAX_SPEED` bajó de 500 a 400 para un "orbit sereno" (rodear más calmo).

### 2.5 Saque inicial (kickoff) y salida de línea

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `ATK_KICKOFF_SPEED_MM_S` | strategy.cpp:199 | **500** mm/s | Empujón inicial al frente cuando el árbitro da el GO. | ~300 a ~700 | **Subir** = saque más explosivo (puede pasarse si no hay pelota enfrente). **Bajar** = saque más suave. |
| `ATK_KICKOFF_DURATION_MS` | strategy.cpp:200 | **250** ms | Cuánto dura ese empujón antes de pasar a buscar. | ~150 a ~500 | **Subir** = avanza más al frente (puede chocar). **Bajar** = pasa antes a buscar. |
| `ATK_LINE_RETREAT_SPEED` | strategy.cpp:172 | **600** mm/s | Con qué velocidad retrocede al ver la línea blanca del borde. | ~400 a ~700 | **Subir** = se despega del borde más rápido pero recorre más. **Bajar** = más controlado pero puede rozar/salirse. |
| `ATK_LINE_AVOID_MIN_MS` | strategy.cpp:179 | **800** ms | Tiempo mínimo que retrocede sí o sí (atraviesa la franja blanca del medio donde el sensor satura). | ~500 a ~1200 | **Subir** = se asegura de cruzar la franja pero se va más lejos. **Bajar** = sale antes pero puede "creer" que se despegó estando sobre la franja saturada. |
| `ATK_LINE_CLEAR_MARGIN_MS` | strategy.cpp:180 | **300** ms | Cuánto tiene que estar SIN VER la línea para considerar que se despegó de verdad. | ~150 a ~500 | **Subir** = más seguro de estar lejos del borde, retrocede más tiempo. **Bajar** = vuelve a jugar antes, puede quedar pegado y re-disparar la huida (loop). |
| `ATK_LINE_AVOID_MAX_MS` | strategy.cpp:181 | **3000** ms | Tope de seguridad: corta la huida aunque nunca confirme que se despegó. | ~2000 a ~4000 | **Subir** = más margen en casos raros, pero si falla retrocede mucho (a 600 mm/s puede cruzar la cancha). **Bajar** = corta antes (más seguro contra irse lejos), puede volver sobre el borde. |

> *Historia (Elías, 2026-06-14):* `ATK_LINE_RETREAT_SPEED` subió de 400 a 600 ("salida bien rápida").
> El esquema mínimo+margen+tope se copió del arquero (banco 2026-06-09): antes la salida era por
> tiempo ciego de 4 s fijos y "quedaba trabado / cruzaba la cancha".

### 2.6 Empuje sin gyro — SOLO práctica R1 (`-DATK_OTOS_NOGYRO`)

Estos solo existen cuando se flashea el env de práctica del delantero (robot sin BNO, con 2 OTOS
en DOWN). En competencia/R2 no existen. **Todos están marcados "TUNEAR EN BANCO".**

| Parámetro | Archivo:línea | Valor | Qué hace | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|
| `ATK_NOGYRO_PUSH_DIST_MM` | strategy.cpp:212 | 150 mm | Distancia gatillo del empuje sin gyro (más lejos que con gyro porque la dirección la pone el vector a la pelota). | **Subir** = dispara desde más lejos. **Bajar** = espera tenerla más cerca (más seguro de la dirección). |
| `ATK_NOGYRO_PUSH_TOL_DEG` | strategy.cpp:215 | 15 ° | Cuán cerca del eje al arco tiene que estar la pelota para empujar (gatillo geométrico). | **Subir** = empuja aunque esté más desviada. **Bajar** = exige la pelota bien sobre el eje. |
| `ATK_NOGYRO_OMEGA_MAX_DEGPS` | strategy.cpp:216 | 40 °/s | Tope de giro al sostener el rumbo durante el empuje (anti bang-bang, igual que el arquero). | **Subir** = corrige el rumbo más rápido, arriesga giro brusco. **Bajar** = más suave, puede no enderezar a tiempo. |
| `ATK_NOGYRO_BAILOUT_DEG` | strategy.cpp:217 | 45 ° | Si el error de rumbo es mayor a esto, NO corrige (probablemente el signo está mal o el dato es basura). | **Subir** = tolera errores más grandes (arriesga enroscarse). **Bajar** = se rinde antes ante errores grandes. |

### 2.7 Freno anti-choque — SOLO env de práctica con obstáculos

`ATK_OBSTACLE_STOP_MM` **no es una constante del .cpp**: se define como flag en
`platformio.ini:358` (env `*_obst_bb`: `-DATK_OBSTACLE_STOP_MM=250`). Solo existe en ese env.
Si el obstáculo más cercano (mín. de 4 ToF + el HC-SR04 frontal del TOP) está más cerca que 250 mm,
corta el avance. **Subir** = frena antes (más seguro, se traba más lejos del objeto). **Bajar** =
deja acercarse más (más agresivo). Se cambia en `platformio.ini`, no en `strategy.cpp`.

---

## 3. El arquero (ROBOT1)

El arquero **patrulla** la boca del arco de costado, **intercepta** siguiendo la X de la
pelota, **despeja** cuando la pelota se le acerca mucho, y **sale de la línea** si se va a salir.
Todo en `src/central/strategy.cpp`.

> ⚠️ Hay DOS arqueros conviviendo: la **patrulla clásica v3.3** (mover-parar-mover) y el
> **strafe simple** (`-DGK_SIMPLE_STRAFE`, lo que se usa hoy en banco con María). Los parámetros
> del strafe simple llevan prefijo `GKS_` o `GK_BALL_*` y se aclaran. Antes de tunear, fijate
> qué env está flasheado (ver sección 7).

### 3.1 Arrancar (delay de START y retroceso al arco)

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `GK_START_DELAY_MS` | strategy.cpp:257 | **2000** ms | Cuánto espera quieto tras el START antes de moverse (para acomodarlo/soltarlo). | 0 a 3000 | **Subir** = más tiempo para acomodar, pero arranca tarde (perdés defensa). **Bajar a 0** = sale apenas suena el START. El comentario dice explícito: **"para COMPETENCIA bajar a 0".** |
| `GK_GOTO_LINE_VY_BACK` | strategy.cpp:263 | **420** mm/s | Velocidad con la que retrocede a su arco hasta tocar la línea de fondo. | 350 a 470 | **Subir** = llega antes, pero arriba de ~470 la trasera satura y guiña (gira). **Bajar de ~350** = muy lento y la dirección se ensucia. |
| `GK_GOTO_LINE_VX_RIGHT` | strategy.cpp:259 | **0** | Componente lateral del retroceso. 0 = retrocede RECTO atrás. | dejar en 0 | **Subir** (darle lateral) = el retroceso sale en círculos con estos pisos de PWM. **Dejar 0** = derecho. Orden de Gustavo: "recto atrás". |
| `GK_GOTO_LINE_TIMEOUT_MS` | strategy.cpp:264 | **4000** ms | Si no encuentra la línea en este tiempo, pasa a patrullar igual. | 3000 a 6000 | **Subir** = insiste más (riesgo: retrocede de más y se sale por atrás). **Bajar** = corta antes (puede quedar mal posicionado adelante). |
| `GK_GOTO_LINE_VX_TRIM_MM_S` | strategy.cpp:280 | **0** mm/s | Perilla fina para corregir la deriva lateral del retroceso. **TOPE FÍSICO ±19.** | -19 a +19 | Si el robot deriva a la derecha → trim negativo (hacia la izq). **NO pasar ±19**: arriba, la trasera se dispara a 107 = patada lateral brusca. |
| `GK_GOTO_LINE_HEADING_TRIM_DEG` | strategy.cpp:286 | **0** ° | Perilla fina para corregir la deriva de RUMBO del retroceso (de a 1°). | -10 a +10 | Deriva a la derecha → sesgar a la izquierda (positivo). Calibrar después del `VX_TRIM`. |

> *Historia (banco 2026-06-09):* `GK_START_DELAY_MS` lo pidió Gustavo ("no me da el tiempo para
> acomodarlo"). Los dos trims volvieron a 0 para aislar variables: con -15 salía una "J", se
> sospecha que la causa es el signo del heading de la cámara, no el trim. **Re-tunear los trims
> recién después de confirmar el signo del BNO de robot2.**

### 3.2 Patrullar (despegarse de la línea y barrer la boca del arco)

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `GK_PATROL_SPEED_MM_S` | strategy.cpp:242 | **200** mm/s | Velocidad normal del strafe (patrulla y centrado con la pelota). El "modo observación". | 150 a 300 (arriba de ~420 entra otro régimen, la trasera satura) | **Subir** (hacia 420) = cubre el arco más rápido y la dirección sale más fiel, pero más brusco. **Bajar** = más derecho, pero llega tarde a pelotas de costado. |
| `GK_ADVANCE_SPEED_MM_S` | strategy.cpp:295 | **300** mm/s | Velocidad con la que avanza para despegarse de la línea después de tocarla. | 250 a 400 | **Subir** = se despega más rápido (puede quedar tan adelante que ya no ve la línea de guía). **Bajar** = más suave (con carga puede no despegar = loop avance-toque). |
| `GK_ADVANCE_MS` | strategy.cpp:304 | **100** ms | Cuánto sigue avanzando DESPUÉS de dejar de ver la línea (define cuán adelante queda la patrulla). | 50 a 200 | **Subir** = queda más adelante (riesgo: deja de ver la línea de guía y el rebote no funciona). **Bajar** = más pegada (riesgo: la pisa y dispara la alarma de salida). |
| `GK_ADVANCE_TIMEOUT_MS` | strategy.cpp:305 | **1500** ms | Tope duro de la fase de avance. | 1000 a 2500 | **Subir** = tolera avances más largos. **Bajar** = corta antes (puede quedar demasiado pegado). |
| `GK_PATROL_REACQ_VY_MM_S` | strategy.cpp:309 | **200** mm/s | Retroceso suave para volver a ver la línea si el strafe derivó adelante. | 150 a 300 | **Subir** = re-engancha más rápido (puede pisar la línea). **Bajar** = más suave, más lento en recuperar la referencia. |
| `GK_PATROL_REACQ_MAX_MS` | strategy.cpp:310 | **700** ms | Cuánto tiempo máximo retrocede buscando la línea. | 500 a 1000 | **Subir** = insiste más (retrocede demasiado). **Bajar** = se rinde antes y patrulla sin referencia. |

> *Historia (banco 2026-06-09/10):* `GK_ADVANCE_MS` bajó de 350 a 100. Con 350 (~10 cm) la
> patrulla quedaba tan adelante que el anillo de sensores JAMÁS veía la línea → el rebote no
> disparaba ("no se guiaba de la línea", Gustavo). Con ~3 cm la línea queda visible de guía sin pisarla.

### 3.3 Límites de patrulla por posición (usa la pose del TOP)

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `GK_PATROL_X_CENTER_MM` | strategy.cpp:319 | **910** mm | Centro de la oscilación, frente al arco (la cancha es 1820 mm de ancho → centro ~910). | medir en banco frente al arco | Corre el centro de la patrulla a un lado u otro. Si está mal contra la trilateración real, patrulla descentrado y deja medio arco libre. |
| `GK_PATROL_X_HALF_RANGE_MM` | strategy.cpp:320 | **350** mm | Cuánto se aleja del centro a cada lado antes de rebotar (cubre ~70 cm de boca). | 250 a 450 (ajustar a la boca real) | **Subir** = cubre más ancho (puede irse al lateral). **Bajar** = cubre menos (deja libres las esquinas del arco). |
| `GK_POSE_CONF_MIN` | strategy.cpp:321 | **40** | Confianza mínima de la pose del TOP para usar estos límites. Por debajo, patrulla por tiempo. | 30 a 70 | **Subir** = solo confía en pose muy buena (casi nunca usa límites por posición). **Bajar** = confía en pose floja (patrulla descentrado). |

> *needs_bench:* `GK_PATROL_X_CENTER_MM` y `GK_PATROL_X_HALF_RANGE_MM` hay que **medirlos
> frente al arco real de la cancha de Gustavo** (verificar qué reporta la trilateración).

### 3.4 Mantenerse pegado a la línea (PID lateral, Capa 3)

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `GK_CROSS_TRACK_SETPOINT_MM` | strategy.cpp:334 | **-40** mm | A qué distancia de la línea se queda. -40 = "apenas pisándola", la línea ~40 mm detrás del centro. | -60 a -20 | **Subir hacia 0** = se centra más sobre la línea → **vuelve el flapping** (causa #1 del banco). **Bajar** (más negativo) = se aleja más (pierde la línea de guía). |
| `GK_CT_VY_SIGN` | strategy.cpp:343 | **+1** | Para qué lado corrige la distancia a la línea. | +1 o -1 | Si en banco ves que el robot **se ALEJA** de la línea en vez de mantenerla, **ponelo en -1.** |
| `GK_LINE_AVOID_DEBOUNCE_MS` | strategy.cpp:348 | **150** ms | Cuánto tiene que persistir la alarma "me salgo" antes de disparar la huida (filtra un roce). | 100 a 300 | **Subir** = más tolerante a roces (tarda en reaccionar a salida real). **Bajar** = reacciona más rápido (vuelve el flapping). |
| `GK_LINE_AVOID_COOLDOWN_MS` | strategy.cpp:349 | **400** ms | Tiempo tras volver a patrullar en que no puede re-disparar la huida (deja que el PID recupere). | 300 a 700 | **Subir** = más bloqueado (si todavía sale, no reacciona). **Bajar** = puede re-disparar enseguida (flapping). |
| `GK_LATERAL_SETPOINT_DEPTH` | strategy.cpp:244 | **1.0** | Setpoint de FALLBACK del PID lateral cuando no hay distancia-a-línea real (modo viejo). | no tocar sin entender el modo fallback | Solo aplica cuando el `cross_track` real no está disponible. Es el camino de respaldo. |

> *Historia (banco 2026-06-09):* `GK_CROSS_TRACK_SETPOINT_MM` era 0 (centrado) y ponía 6+
> sensores en blanco → disparaba "me salgo" → flapping violento entre patrullar y huir (la causa
> #1 del banco). Con la línea ~40 mm detrás, el anillo la toca con 2-4 sensores y patrulla el borde tranquilo.

### 3.5 Interceptar y despejar

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `GK_INTERCEPT_KP_VS_BALL_X` | strategy.cpp:243 | **4.0** | Con cuánta fuerza sigue la X de la pelota (se mueve de costado proporcional a cuánto está corrida). | 2.0 a 6.0 | **Subir** = reacciona más fuerte (oscila, se pasa). **Bajar** = sigue más suave (llega tarde, le entra el gol). |
| `GK_CLEAR_TRIGGER_MM` | strategy.cpp:412 | **250** mm | Si la pelota se acerca a menos de esto, sale a DESPEJAR (empujarla lejos) en vez de solo defender. | 200 a 350 | **Subir** = sale a despejar con la pelota más lejos (más agresivo, deja el arco). **Bajar** = solo despeja muy encima (reacciona tarde). |
| `GK_CLEAR_RELEASE_MM` | strategy.cpp:413 | **400** mm | Una vez despejando, si la pelota se aleja más de esto, vuelve a defender (debe ser mayor que TRIGGER). | 350 a 500 (> 250) | **Subir** = persigue más lejos antes de volver. **Bajar** (cerca de TRIGGER) = entra y sale de despeje = tiembla. |
| `GK_CLEAR_SPEED_MM_S` | strategy.cpp:414 | **500** mm/s | Velocidad yendo derecho a la pelota a despejarla (sin pateador, empuja por inercia). | 420 a 550 | **Subir** = empuja más fuerte/lejos (arriba de cierto punto la dirección se ensucia, se aleja del arco). **Bajar** = despeje más débil (no saca la pelota). |

### 3.6 Anticipar tiros al arco (clasificación de trayectoria)

Cuando la pelota va apuntada al arco propio, el arquero refuerza la respuesta. Si NO hay amenaza,
todo queda igual que sin estos parámetros (comportamiento idéntico).

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `GK_BT_SPEED_MIN_MM_S` | strategy.cpp:437 | **80** mm/s | Por debajo de esta velocidad la pelota se considera quieta (no es tiro). | 50 a 120 | **Subir** = solo pelotas rápidas son "tiro" (ignora tiros lentos). **Bajar** = el ruido se confunde con tiro (falsas amenazas). |
| `GK_BT_TOWARD_TOL_CENTIDEG` | strategy.cpp:438 | **4500** (=45°) | Ancho del cono "va hacia el arco". 0 deshabilita. | 3000 a 6000 (30° a 60°) | **Subir** (cono más ancho) = más tiros marcados amenaza (sobre-reacciona). **Bajar** = solo tiros muy de frente (un diagonal no dispara el refuerzo). |
| `GK_BT_THREAT_LEAD_FACTOR` | strategy.cpp:460 | **1.5** | Cuánto MÁS anticipa la pelota cuando es amenaza. 1.0 = sin cambio. | 1.0 a 2.0 | **Subir** = anticipa más (si predice de más deja hueco del otro lado). **Bajar hacia 1.0** = no aporta nada extra. |
| `GK_BT_THREAT_KP_FACTOR` | strategy.cpp:461 | **1.5** | Cuánto MÁS agresivo reacciona a la X cuando es amenaza. 1.0 = sin cambio. | 1.0 a 2.0 | **Subir** = reacción más fuerte (oscila/se pasa). **Bajar hacia 1.0** = sin refuerzo. |

> *needs_bench:* los dos factores de amenaza (1.5) son valores iniciales conservadores. Tunear en banco.

### 3.7 El arquero "strafe simple" (lo que se usa hoy en banco — `-DGK_SIMPLE_STRAFE`)

Esta es la versión que María está tuneando. Es un strafe continuo con un control de rumbo PFM
(ver sección 5) y una huida de línea por tiempo.

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `GKS_ESCAPE_MS` | strategy.cpp:1148 | **1700** ms | Cuánto dura la huida lateral tras tocar la línea (strafe a ciegas al lado contrario). | 1300 a 2200 | **Subir** = huye más lejos (más seguro de despegarse, se aleja del centro del arco). **Bajar** = "se queda trabado en la línea blanca" (el problema que reportó María). |
| `GKS_ESCAPE_SPEED_MM_S` | strategy.cpp:1158 | **470** mm/s | Velocidad de esa huida (más rápida que el strafe normal). | 420 a 500 (no mucho más de 470) | **Subir** (más de ~470) = la trasera satura, la huida sale **diagonal**. **Bajar** = más lento, tarda en despegarse. **Para más distancia, subí `GKS_ESCAPE_MS`, no esta.** |
| `GKS_RESQUARE_DEG` | strategy.cpp:1144 | **45** ° | Si el robot se desvía más de esto, frena y se gira en el lugar (freno de emergencia del rumbo). | 35 a 60 | **Subir** = tolera más torcedura (patrulla de costado al arco). **Bajar** = salta a corregir con menos error (patrulla entrecortada). |
| `GKS_RESQUARE_EXIT` | strategy.cpp:1145 | **12** ° | Apenas el error baja de esto, corta el giro de corrección (la inercia termina). | 8 a 15 | **Subir** = corta antes (queda algo torcido). **Bajar** = gira hasta casi 0 (la inercia lo pasa y oscila). |
| `GKS_RESQUARE_MAX_MS` | strategy.cpp:1146 | **900** ms | Tope del re-escuadre parado. | 600 a 1200 | **Subir** = insiste más (pierde tiempo sin defender). **Bajar** = corta antes (vuelve todavía chueco). |
| `GKS_SETTLE_MS` | strategy.cpp:1147 | **300** ms | Pausa quieto después del re-escuadre antes de retomar el strafe. | 200 a 500 | **Subir** = más firme al retomar, más quieto (vulnerable). **Bajar** = retoma antes (puede arrancar girando por inercia). |
| `GK_PATROL_BOUNCE_COOLDOWN_MS` | strategy.cpp:379 | **800** ms | Tiempo mínimo entre rebotes (1 solo rebote por toque de línea). | 500 a 1200 | **Subir** = ignora toques por más tiempo (si toca otra línea enseguida, no rebota). **Bajar** = un toque sostenido = muchos rebotes (tiembla). |

**Centrado con la pelota (solo con `-DGK_SIMPLE_BALL`):**

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `GK_BALL_TRACK_SIGN` | strategy.cpp:1171 | **+1** | Hacia qué lado se mueve para centrarse con la pelota. | +1 o -1 | Si en cancha el arquero **se ALEJA** de la pelota en vez de enfrentarla, **ponelo en -1.** Es lo PRIMERO a chequear si el centrado anda al revés. |
| `GK_BALL_TRACK_DB_MM` | strategy.cpp:1170 | **40** mm | Zona muerta: si la pelota está a menos de 40 mm del centro, no se mueve (anti-jitter). | 30 a 60 | **Subir** = más estable, centra más grueso. **Bajar** = centra más fino (tiembla persiguiendo el ruido). |
| `GK_BALL_MAX_ABS_X_MM` | strategy.cpp:1176 | **900** mm | Si la pelota se reporta a ≥900 mm de costado, ignora el dato (filtra el salto de la fusión de cámaras + queda central). | 700 a 1000 | **Subir** = acepta pelotas más a los costados (vuelve a tragar el dato saltarín, se va a los dos lados). **Bajar** = solo centra con pelotas cerca del eje (más central). |

> *Historia (banco María, 2026-06-14):* `GKS_ESCAPE_MS` se subió 600→900→1300→1700 ("le sigue
> faltando distancia"). Como la trasera satura a ~420 mm/s, **la palanca buena para más distancia
> es la DURACIÓN, no la velocidad.** El `GK_BALL_TRACK_SIGN` empezó en -1 (se alejaba) → +1. Y
> `GK_BALL_MAX_ABS_X_MM` está porque la fusión de las 2 cámaras del TOP teletransportaba la pelota
> (+1000 → -1000 en 20 ms, imposible) y tiraba al arquero a los dos lados.

### 3.8 Patrulla clásica v3.3 (mover-parar-mover) — solo si NO se usa strafe simple

Estos solo aplican en la patrulla segmentada (la otra rama). Si hoy corre el strafe simple, no los toques.

| Parámetro | Archivo:línea | Valor | Qué hace (corto) |
|---|---|---|---|
| `GK_PATROL_SEG_MS` | strategy.cpp:377 | 1200 ms | Cuánto dura cada tramo de strafe antes de frenar a medir rumbo. |
| `GK_PATROL_STOP_MS` | strategy.cpp:378 | 300 ms | Freno entre tramos (mide el rumbo quieto = confiable). |
| `GK_PATROL_MAX_SEGS_SAME_DIR` | strategy.cpp:380 | 3 | Tramos seguidos sin línea ni pose antes de invertir (fail-safe). |
| `GK_REORIENT_ENTER_DEG` | strategy.cpp:381 | 35 ° | Error que dispara la re-orientación parada con pulsos. |
| `GK_REORIENT_EXIT_DEG` | strategy.cpp:382 | 25 ° | Corta el pulso de giro en vivo (anticipa la inercia). |
| `GK_REORIENT_MS_PER_DEG` | strategy.cpp:383 | 2.0 ms/° | Largo del pulso proporcional al error (acotado por MIN/MAX). |
| `GK_REORIENT_PULSE_MIN_MS` | strategy.cpp:384 | 40 ms | Pulso mínimo (por debajo el motor no arranca). |
| `GK_REORIENT_PULSE_MAX_MS` | strategy.cpp:385 | 80 ms | Pulso máximo (más largo se pasa por la inercia). |
| `GK_REORIENT_SETTLE_MS` | strategy.cpp:386 | 700 ms | Quieto tras el pulso (inercia + ≥2 muestras frescas a 4 Hz). |
| `GK_REORIENT_MAX_PULSES` | strategy.cpp:387 | 2 | Máximo de pulsos por parada (más = ping-pong sobre dato viejo). |
| `GK_LINE_RETREAT_SPEED` | strategy.cpp:245 | 420 mm/s | Velocidad de huida en la patrulla v3.3 clásica (no en el strafe simple). |
| `GK_GYRO_HOLD_TARGET_DEG` | strategy.cpp:354 | 0 ° | Rumbo a mantener sin cámara (0 = frente del boot = arco rival). **No cambiar.** |
| `GK_CAMERA_ORIENT_ENABLED` | strategy.cpp:396 | **false** | Usar el arco propio (cámara trasera) como referencia. **DESHABILITADO** hasta validar la trasera en banco (con true hace la "J/U" del retroceso). |
| `GK_ORIENT_OMEGA_MAX_DEGPS` | strategy.cpp:406/408 | 40 (con `-DCENTRAL_FLOOR_SCALE`) / 10 (sin) | Tope de giro al orientarse. Sin el flag NO subir de ~10 o se dispara el bang-bang de la trasera. |

---

## 4. La fuerza y suavidad de los motores ⭐ (el grupo estrella)

> **Esto es lo PRIMERO que tocás si el robot "no se mueve", "rota raro" o "el strafe arquea".**
> Acá viven los pisos de PWM, la inversión de motores, el impulso de arranque y el freno térmico.
> Casi todo está en `src/central/config_central.h` y `src/central/motors_zircon.cpp`.

### La historia corta del 70/70/107 (leela antes de tocar los pisos)

El robot tiene 3 ruedas omni. Las **dos delanteras** van OBLICUAS (a 60°): sus rodillos ruedan
de costado → **mucha fricción** → necesitan más PWM para arrancar. La **trasera** va alineada
al strafe → **menos fricción** → arranca con menos.

Pero hay un truco: en el strafe, la cinemática pide que la trasera vaya al **DOBLE de velocidad**
que las delanteras (relación 2:1). Como la trasera rinde más por PWM, lo logra con ~1,5× el PWM
de las delanteras: **107 vs 70.** Por eso `{70, 70, 107}`.

- Banco R1 2026-06-08: la trasera estuvo en **42** (con piso alto el robot rotaba en el strafe).
- Banco R2 2026-06-09: un barrido (42→50→...→107) mostró que la trasera necesita **~107** para
  sostener esa relación 2:1. Decisión de Gustavo: R1 parte de los valores validados en R2.
- Banco R1 2026-06-11 (post-reparación): se confirmó `{70,70,107}` cuando se descubrió que el
  síntoma "delanteras débiles" era el **M2 mal cableado**, no el piso.
- Banco María 2026-06-14: subir la trasera a **120 EMPEORÓ** la medialuna → se revirtió a 107.

### 4.1 Pisos de PWM por rueda (el mínimo para arrancar)

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `MOTOR_MIN_PWM` (ROBOT1 / arquero) | config_central.h:66 | **{70, 70, 107}** | Piso por rueda {del-IZQ, del-DER, trasera}. | del. 70-90, trasera 42-107. **NUNCA pasar ~150 (queman).** | **Subir delanteras** = más empuje en el piso si no avanza. **Subir trasera** = el robot ROTA en el strafe (la trasera se adelanta). **Bajar trasera** gradual si rota (107→95→85). |
| `MOTOR_MIN_PWM` (ROBOT2 / delantero, **comparte arquero+delantero R2**) | config_central.h:103 | **{70, 70, 107}** | Igual que R1 pero rama ROBOT2. ⚠️ cambia los DOS roles de R2. | del. 70-90, trasera 95-130. NUNCA ~150. | **Subir trasera a 120 EMPEORA la medialuna (probado, no hacerlo).** **Subir delanteras** si no mueven el robot. |

### 4.2 Eficiencia de las ruedas (la forma "buena" de darle más a la trasera)

Esto solo lo usan los builds con `-DCENTRAL_FLOOR_SCALE` (hoy el arquero). Es la manera **simétrica**
de darle más PWM a la trasera **sin meter la asimetría** que mete subir el piso.

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `MOTOR_EFF_X100` (ROBOT1) | config_central.h:74 | **{100, 100, 131}** | Cuánta velocidad da cada rueda por PWM. La trasera rinde ~1,31× más. | trasera 110-140 (es un ratio, no quema) | **Bajar la trasera** (131→115) = MÁS PWM a la trasera a ambos lados (endereza el strafe sin asimetría). **Subir** = menos. |
| `MOTOR_EFF_X100` (ROBOT2) | config_central.h:122 | **{100, 100, 115}** | Igual, ya bajada a 115 (más trasera) respecto del medido 131. | 110-131. Revertir = 131. | **Bajar** (115→110) = aún más trasera. Si rota al OTRO lado (trasera se adelanta) → **subir hacia 131**. |

> *Historia (A/B banco María, 2026-06-14, arquero sin BNO):* `MOTOR_EFF_X100` de R2 bajó 131→115
> para enderezar el strafe open-loop (derivaba ~8°/s, la trasera parecía floja). Es la palanca
> preferida sobre el piso, porque no introduce yaw parásito.

### 4.3 Sentido de los motores (si una rueda gira al revés)

| Parámetro | Archivo:línea | Valor | Qué hace | Si lo cambiás |
|---|---|---|---|---|
| `MOTOR_INVERT` (ROBOT1) | config_central.h:47 | **{+1, +1, +1}** | +1 = derecho, -1 = invertido. Sirve si un driver tiene los cables cruzados. | Si un motor gira al revés, su signo a -1. NOTA: si el M2 se recablea cruzado otra vez, volver a `{+1,-1,+1}`. |
| `MOTOR_INVERT` (ROBOT2) | config_central.h:97 | **{+1, +1, +1}** | Igual, los 3 derechos. | Hoy ninguno lo necesita. |

> *Historia:* el M2 de R1 estuvo en -1 (driver cruzado de fábrica) hasta la reparación del
> 2026-06-11; al recablearlo derecho, las delanteras se cancelaban con el -1 → se pasó a +1.

### 4.4 Impulso de arranque (kickstart) — rompe la inercia al arrancar

Solo con `-DCENTRAL_MOTOR_KICKSTART` (activo en producción). Cuando una rueda arranca de cero,
le da un "golpe" fuerte los primeros 40 ms para vencer el rozamiento, y después baja al PWM normal.

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `KICKSTART_PWM_CAP` | motors_zircon.cpp:64 | **{145, 145, 150}** | El golpe (fijo) por rueda. La trasera un toque más (150) porque "se quedaba". | 130-150. **~150 = LÍMITE DE QUEMADO.** | **Subir** (hacia 150) = arranca más decidido, más cerca de quemar. **Bajar** (130) = más suave, puede no romper inercia. **Para más golpe, subí la VENTANA, no el cap.** |
| `KICKSTART_WINDOW_MS` | motors_zircon.cpp:52 | **40** ms | Cuánto dura ese golpe antes de pasar al PWM de régimen. | 30-80 ms (más largo = más calor) | **Subir** = el golpe dura más (la palanca **preferida** para más impulso, más segura que subir el cap). **Bajar** = golpe más corto. |
| `KICKSTART_FACTOR_X10` | motors_zircon.cpp:58 | **99** (×9,9) | Multiplicador del golpe. Tan alto que cualquier base llega al cap (= impulso fijo). | dejar alto | Prácticamente no cambia nada mientras sea alto. Bajarlo mucho devuelve el golpe desparejo. |

> *Historia (Elías, 2026-06-14):* `KICKSTART_PWM_CAP` subió de {130,130,140} a {145,145,150}
> ("impulso inicial más fuerte"). ~150 es el techo: el código recomienda subir `KICKSTART_WINDOW_MS`
> antes que el cap si hace falta más.

### 4.5 Velocidades, freno y escalas globales

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `burn_cap` (`fscfg.burn_cap`) | motors_zircon.cpp:175 | **150** | Freno TÉRMICO: ningún PWM escalado pasa de 150 (solo con `-DCENTRAL_FLOOR_SCALE`). | 140-150. **NO subir de 150.** | **Subir** = más potencia pico pero RIESGO REAL de quemar motores. **Bajar** = más seguro, capa la velocidad máxima. |
| `MOTOR_PWM_NOISE_THRESH` | config_central.h:196 | **5** | Si el PWM pedido es ≤5, manda 0 (no zumba parado). Global. | 3-10 | **Subir** = más silencio parado pero ignora correcciones finas. **Bajar** = correcciones más chicas pero puede zumbar. |
| `OMEGA_SIGN` | config_central.h:175 | **-1.0** | Invierte el sentido del giro en el mixer. | +1 o -1 | Si el robot **gira para el lado contrario** al pedido (o el PID de rumbo lo hace girar MÁS en vez de corregir), invertir. Hoy -1 es el correcto. |
| `WHEEL_ANGLES_DEG` | config_central.h:163 | **{330, 210, 90}** | Dónde está cada rueda (geometría). | no tocar salvo recalibración | Si el robot **traslada al revés**, sacar el +180 → {150,30,270}. Mal puesto = va en círculos o diagonal aplastada. |
| `WHEEL_RADIUS_MM` | config_central.h:151 | **100** mm | Radio del robot (del centro a cada rueda). | medida real del chasis | **Subir** = gira más agresivo. **Bajar** = más suave. Solo afecta el giro. |
| `MAX_SPEED_MM_S` | config_central.h:181 | **1000** mm/s | Velocidad máxima estimada (re-escala todo el mapeo velocidad→PWM). | 800-1200 | **Subir** = la misma velocidad pedida da MENOS PWM (va más lento). **Bajar** = da MÁS PWM (más agresivo, satura antes). |
| `MAX_PWM` | config_central.h:180 | **255** | Techo de la escala de PWM (hardware). | **no tocar** | No es de tuning. Cambiarlo rompe la escala. |
| `MOTION_SCALE` (slow-mo) | motors_zircon.cpp:137 | **0.7** (con `-DCENTRAL_SLOW_MOTION`) / 1.0 | Camara lenta para observar en banco. **NUNCA en competencia.** | 0.6-0.85 cuando se usa | **Subir** = más rápido. **Bajar de ~0.6** = no se mueve (cae bajo el stiction del motor). |

---

## 5. Las ganancias de control (PID y PFM, en simple)

Un control automático tiene tres partes, fáciles de entender con una analogía de manejar a un punto:

- **P (proporcional)** = "reacción al error de AHORA". Mucho error → corrección fuerte. Si es muy
  alta, el robot tiembla/oscila (se pasa y vuelve). Si es muy baja, queda "flojo" y tarda.
- **I (integral)** = "memoria del error arrastrado". Borra un desvío chico y constante (ej. siempre
  2° torcido). Si es muy alta, acumula de más y sobrepasa ("resaca"/windup).
- **D (derivada)** = "anticipa hacia dónde va el error" = freno/amortiguador. Reduce el sobrepaso.
  Si es muy alta, amplifica el ruido del sensor y tiembla nervioso.

### 5.1 PID de rumbo (HeadingPID) — `src/shared/pids.h`

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `HeadingPID.kp` | pids.h:49 | **3.0** | P del rumbo: reacción al error de orientación de ahora. | 1.5 a 5.0 | **Subir** = se planta más firme al rumbo (pasado un punto, oscila). **Bajar** = más suave pero "flojo", la inercia lo desvía. |
| `HeadingPID.ki` | pids.h:50 | **0.05** | I del rumbo: borra un sesgo chico y constante. Chico a propósito (el I es peligroso). | 0.0 a 0.2 | **Subir** = borra mejor sesgos (windup si me paso). **Bajar/0** = más estable pero puede quedar siempre un poco desviado. |
| `HeadingPID.kd` | pids.h:51 | **0.5** | D del rumbo: amortigua el sobrepaso. | 0.0 a 1.5 | **Subir** = menos sobrepaso (deja subir el kp); pasado un punto amplifica ruido y tiembla. **Bajar/0** = más propenso a pasarse. |
| `HeadingPID.integral_clamp` | pids.h:63 | **50.0** | Anti-windup: tope de cuánto acumula el I (si queda trabado, no dispara un giro gigante al soltarse). | 20 a 80 | **Subir** = más autoridad del I (riesgo de "resaca"). **Bajar** = más seguro contra windup, el I ayuda menos. |
| `HeadingPID.output_clamp` | pids.h:64 | **327.0** | Tope de velocidad de giro. **SEGURIDAD CRÍTICA.** | 100 a **327 (TOPE DURO)** | **NUNCA pasar de 327:** con 360 el número se "envuelve" (overflow int16) y el robot **gira al revés a casi máxima velocidad** (bug crítico, eval 2026-06-03). |

> El mismo 3.0 del kp está espejado como `DS_KP_HEADING` en strategy.cpp:228 (para el drive-straight).

### 5.2 PID lateral del arquero (LateralPID) — `src/shared/pids.h`

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `LateralPID.kp` | pids.h:97 | **50.0** | P del lateral: cuánto se mueve de costado según lo desalineado que está de su línea. (Escala grande porque la salida es en mm/s.) | 20 a 100 | **Subir** = corrige más agresivo (tiembla/serpentea si me paso). **Bajar** = más lento en volver a su lugar. |
| `LateralPID.ki` | pids.h:98 | **5.0** | I del lateral: borra un desfase fijo de su línea. | 0.0 a 15 | **Subir** = borra mejor el desfase (windup). **Bajar/0** = puede quedar siempre un poco corrido. |
| `LateralPID.kd` | pids.h:99 | **10.0** | D del lateral: amortigua el vaivén. | 0.0 a 25 | **Subir** = menos sobrepaso (amplifica ruido si me paso). **Bajar/0** = más propenso a rebotar. |
| `LateralPID.setpoint` | pids.h:102 | **1.0** | El objetivo de profundidad (modo viejo/depth). En cancha el arquero usa otro setpoint basado en distancia a la línea (ver 3.4). | calibrar en banco con el robot sobre la línea | Mal valor = arquero parado en el lugar equivocado o falsas alarmas de "me salí". |
| `LateralPID.integral_clamp` | pids.h:108 | **20.0** | Anti-windup lateral. | 10 a 40 | **Subir** = más autoridad (saltito al soltarse). **Bajar** = más seguro. |
| `LateralPID.output_clamp` | pids.h:109 | **800.0** | Velocidad lateral máxima. (Sin riesgo de overflow, 800 está lejos de 32767.) | 300 a 1200 | **Subir** = tapa tiros lejanos más rápido (puede derrapar/pasarse). **Bajar** = más prolijo pero llega tarde. |

### 5.3 Control de rumbo PFM del arquero (strafe simple)

El robot **no sabe girar despacio y continuo** (es todo-o-nada por los pisos de PWM). Así que en
vez de un giro suave, el PFM da "pulsitos" de giro prendidos una fracción del tiempo; el promedio
de esos pulsos da la corrección fina. Estos son los **defaults del módulo PFM** en
`src/shared/pfm_heading.h:54` (todos en la misma línea, dentro de `pfm_heading_default_cfg()`).

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro | Si lo SUBÍS / BAJÁS |
|---|---|---|---|---|---|
| `.kp` | pfm_heading.h:54 | **1.0** | P del rumbo del strafe (°/s de corrección por grado de error). | 0.5 a 3.0 | **Subir** = corrige más fuerte (oscila si me paso). **Bajar** = pierde contra la deriva parásita (~80°/s), queda torcido. |
| `.ki` | pfm_heading.h:54 | **0.2** | I: aprende SOLA la deriva sistemática (feedforward automático). | 0.1 a 0.8 | **Subir** = cancela mejor la deriva (oscila lento si me paso). **Bajar** = tarda en compensar. |
| `.deadband_deg` | pfm_heading.h:54 | **10.0** | Zona muerta: torcido menos de 10° → no corrige (no persigue el ruido). | 5 a 15 | **Subir** = más tranquilo (acepta más torcedura). **Bajar** = más preciso pero tiembla. Regla: **"tiembla → subir deadband".** |
| `.omega_on_degps` | pfm_heading.h:54 | **100.0** | Tamaño FIJO del pulsito de giro (apenas sobre el mínimo útil con FLOOR_SCALE). | 80 a 150 | **Subir** = pulsos más fuertes (a tirones). **Bajar** = más suave (si baja del mínimo útil, el robot deja de poder girar). |
| `.integ_max_degps` | pfm_heading.h:54 | **100.0** | Anti-windup del I (cubre los ~80°/s de deriva medida). | 80 a 150 | **Subir** = compensa derivas más grandes (riesgo de "latigazo"). **Bajar** = más seguro, puede no cancelar toda la deriva. |
| `.window_ms` | pfm_heading.h:54 | **160** ms | Ventana donde reparte los pulsos. | 120 a 200 | **Subir** = pulsos más espaciados (más grueso). **Bajar** = más fino (si baja mucho, cada pulso es tan corto que el motor no se mueve). |
| `.kd_rate` (default) | pfm_heading.h:54 | **0.0** | Amortiguación en el DEFAULT (APAGADA → comportamiento histórico). El arquero la pisa con el de abajo. | 0 (no tocar acá) | Si lo subís ACÁ afecta a TODO uso del PFM. Para amortiguar el arquero, tocá `GK_PFM_KD_RATE` (abajo). |
| `GK_PFM_KD_RATE` | strategy.cpp:1236 | **0.30** | La "D" del rumbo del arquero (solo con `-DGK_PFM_RATE_DAMP`): frena el sobrepaso que la latencia provoca (la "medialuna"). | 0.0 a 0.6 | **Subir** = mata la medialuna (de más, el control se pone perezoso). **Bajar/0** = vuelve la medialuna. **needs_bench.** |

**Estimador de velocidad de giro** (alimenta la D del PFM, solo con `-DGK_PFM_RATE_DAMP`),
defaults en `src/shared/heading_rate.h:37`:

| Parámetro | Archivo:línea | Valor | Qué hace | Rango seguro |
|---|---|---|---|---|
| `.min_change_deg` | heading_rate.h:37 | **0.1** ° | Cambio mínimo de rumbo para contar como muestra nueva (filtra jitter del BNO ~±0,05°). | 0.05 a 0.3 |
| `.stale_ms` | heading_rate.h:37 | **150** ms | Sin muestra nueva por esto → declara velocidad de giro = 0. | 100 a 300 |
| `.max_abs_degps` | heading_rate.h:37 | **600.0** °/s | Tope defensivo de la velocidad estimada (el robot físico no gira más rápido). | 400 a 800 (límite de cordura) |

> *Historia (María, 2026-06-14):* `kp1 ki0.2 db10` salió de su titración de banco y está
> preservado como el default histórico. La D (`GK_PFM_KD_RATE=0.30`) la agregó el coach el
> 2026-06-14 para matar la "medialuna" (sobrepaso de rumbo por latencia); está marcada para tunear.

---

## 6. Seguridad y tiempos (lo que NO tocás a la ligera)

Estos son frenos de emergencia y ritmos del lazo. Cambiarlos mal puede congelar o descontrolar el
robot. Están en `src/central/main_central.cpp` y `src/central/world_model.cpp`.

| Parámetro | Archivo:línea | Valor | Qué hace | Por qué cuidado |
|---|---|---|---|---|
| `GK_EDGE_BRAKE_MAX_MS` | main_central.cpp:293 | **350** ms | Freno de emergencia del arquero: cuánto frena pegado al borde antes de SOLTAR y dejar correr la huida. | **NO subir de ~500.** Banco 2026-06-14: arriba de eso el arquero se **congelaba** sobre su línea izquierda (deadlock 5+ s). 350 = frena el golpe y suelta justo antes de congelarse. Si lo tocás, banco en AMBAS líneas (el bug solo salía en la izquierda). |
| Gate de strategy (`>= 10`) | main_central.cpp:320 | **10** ms (100 Hz) | Cada cuánto corre la jugada y se aplican comandos. | 100 Hz matchea el snapshot del TOP. **No bajar de 10** (no entra dato nuevo). Si lo cambiás, los PID están tuneados para este ritmo: revisar ganancias. El freno de borde NO espera este gate (corre cada vuelta). |
| `SNAPSHOT_TIMEOUT_MS` | world_model.cpp:21 | **500** ms | Sin datos del TOP por esto → PARA los motores y parpadea (modo "me quedé ciego"). | 300-700. **Subir** = actúa con mundo viejo (peligroso). **Bajar** = micro-cortes del link lo trancan de más. |
| `LINE_TIMEOUT_MS` | world_model.cpp:22 | **500** ms | Sin datos de línea (DOWN) por esto → el dato de línea deja de ser fresco (deshabilita el freno de borde). | 300-700. Habilita/inhabilita el freno de borde. No tocar a la ligera. |
| `OTOS_TIMEOUT_MS` | world_model.cpp:19 | **500** ms | Frescura de la odometría de piso (OTOS). En R1 sin BNO, el OTOS ES el rumbo del delantero. | 300-700. Menos crítico (es estimación, no freno), pero afecta la navegación. |
| `CENTRAL_WDT_WT_FIELD` (watchdog HW) | main_central.cpp:77 | **1** (→ 1.0 s) | "Hombre muerto": si el loop se cuelga, el chip se resetea solo a 1.0 s. Gateado por `-DCENTRAL_ENABLE_WDT`, **APAGADO por default.** | **No prenderlo en competencia sin pasar banco** (cero resets espurios + auto-reset comprobado al colgar el loop a propósito). Mínimo físico 0.5 s. |

---

## 7. Qué env flasheo para cada conducta (mapa flags → env)

Muchas perillas solo existen si el firmware se flasheó con cierto **build flag** (que viene de un
**env** de `platformio.ini`). Esta tabla conecta "qué conducta quiero" con "qué flag/env lo prende".
Si una perilla de las de arriba "no hace nada", probablemente **no flasheaste el env que la activa.**

| Conducta / perilla | Build flag | En qué env vive (aprox.) | Qué cambia |
|---|---|---|---|
| Arquero strafe simple (`GKS_*`, PFM) | `-DGK_SIMPLE_STRAFE` | env del arquero strafe (ej. `strafe_bb`) | Activa el arquero que María tunea hoy (strafe continuo + PFM + huida por tiempo). |
| Centrado con la pelota (`GK_BALL_*`) | `-DGK_SIMPLE_BALL` | el env de strafe con pelota | El arquero se centra con la pelota. Sin el flag, ese env queda byte-idéntico (solo patrulla). |
| Amortiguación de la medialuna (`GK_PFM_KD_RATE`) | `-DGK_PFM_RATE_DAMP` | env del arquero con D | Prende la "D" del rumbo (mata el sobrepaso). Sin el flag, el PFM corre sin amortiguación. |
| Escalado uniforme de pisos (`MOTOR_EFF_X100`, `burn_cap`, `GK_ORIENT_OMEGA_MAX_DEGPS`=40) | `-DCENTRAL_FLOOR_SCALE` | env del arquero | Usa la eficiencia por rueda y el freno térmico. Sin el flag, `GK_ORIENT_OMEGA_MAX_DEGPS` cae a 10. |
| Impulso de arranque (`KICKSTART_*`) | `-DCENTRAL_MOTOR_KICKSTART` | producción | El golpe anti-inercia de 40 ms. Activo en producción. |
| Delantero giro en el lugar (`ATK_SEARCH_VY_MM_S`=0) | `-DATK_SEARCH_SPIN_ONLY` | `central_robot1_delantero_practica` | Busca girando en el lugar (sin avanzar). |
| Delantero sin gyro (`ATK_NOGYRO_*`) | `-DATK_OTOS_NOGYRO` | `central_robot1_delantero_practica*` | Gatillos del empuje geométricos (sin BNO, con OTOS). En competencia no existen. |
| Freno anti-choque (`ATK_OBSTACLE_STOP_MM`) | `-DATK_OBSTACLE_STOP_MM=250` | `central_robot1_delantero_practica_obst_bb` | Corta el avance ante un obstáculo a <250 mm. Se cambia en `platformio.ini`. |
| Camara lenta de banco (`MOTION_SCALE`=0.7) | `-DCENTRAL_SLOW_MOTION` | `central_robotN_slow` | Todo en cámara lenta para observar. **NUNCA en competencia.** |
| Watchdog de hardware (`CENTRAL_WDT_WT_FIELD`) | `-DCENTRAL_ENABLE_WDT` | `central_robot1_wdt_hangtest` (test) | Prende el auto-reset por cuelgue. APAGADO por default. |

> **Cómo saber qué env está flasheado:** mirá el env que usás al compilar/subir en PlatformIO
> (`pio run -e <env>`) o preguntale a quien flasheó. Los nombres exactos están en `platformio.ini`.

---

## Apéndice: dónde están las perillas (mapa rápido de archivos)

- **Conducta del delantero y del arquero:** `src/central/strategy.cpp`
- **Pisos de PWM, geometría, sentido de motores, timeouts globales:** `src/central/config_central.h`
- **Impulso de arranque, freno térmico, slow-mo, escala velocidad→PWM:** `src/central/motors_zircon.cpp`
- **PID de rumbo y lateral:** `src/shared/pids.h`
- **Control de rumbo PFM (strafe simple) y estimador de velocidad de giro:** `src/shared/pfm_heading.h`, `src/shared/heading_rate.h`
- **Frenos de emergencia y ritmo del lazo:** `src/central/main_central.cpp`, `src/central/world_model.cpp`
- **Flags que solo se cambian al compilar:** `platformio.ini`
