---
title: "Detección de a qué arco atacar/defender — relevamiento (mix/2025/central) + propuesta"
date: 2026-06-21
author: "Claude Opus 4.8 (Anthropic) — workflow de 8 lectores + verificación adversarial"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
status: in-progress
tipo: relevamiento-y-propuesta
robot: ambos (DELANTERO mix / ARQUERO mix)
horizonte: "Incheon 2026"
---

# Detección de "a qué arco atacar / cuál defender" — DELANTERO MIX + ARQUERO MIX

> **Tipo:** investigación + relevamiento + propuesta (research/in-progress).
> **Pedido por:** Gustavo, 2026-06-21. Objetivo: ayudar a Elías (delantero mix) y Virginia (arquero mix)
> con cambios **quirúrgicos** que NO los hagan pisarse trabajando en paralelo.
> **Método:** relevamiento de 8 programas/dominios (workflow multi-agente) + lectura directa de
> `goal_polarity.{h,cpp}` + verificación de `platformio.ini`. **Pasada adversarial** que corrigió un
> error de raíz del primer borrador.
> **Estado de evidencia:** lectura de código real. **NADA testeado en hardware** — el cierre lo hace
> el equipo humano (regla #1 del repo).

---

## 0. Respuesta directa a la pregunta (TL;DR)

**Pregunta de Gustavo:** ¿el delantero mix y el arquero mix tienen implementada alguna forma de detectar,
cuando inician, a qué arco patear?

| Programa | ¿Sabe a qué arco atacar/defender? | Cómo |
|---|---|---|
| **DELANTERO MIX** (`centralmix`, Elías) | **Sí, indirectamente — y ya es el arco correcto.** | NO lo decide él. Recibe del TOP el arco rival **ya autodetectado** (`goal_opp`) y le pega. El "amarillo hardcodeado" que se ve en el código es un **renombre redundante**, no el que manda. |
| **ARQUERO MIX** (`arqueromix`, Virginia) | **No.** | Ignora por completo el arco (`grep goal_` en `amix_fsm.cpp` = 0). Defiende "lo que tenga atrás al arrancar" por *homing* a la línea del área. La dirección es **implícita en cómo se coloca físicamente el robot**. |

**Y el hallazgo que cambia todo:** la idea que proponés —usar las cámaras para ver el color del arco al
iniciar y de ahí sacar a quién atacar/defender— **ya está implementada y corriendo** en la placa TOP, en
el firmware de competencia. Se llama [`goal_polarity`](../../software/teensy/Soccer%202026/src/shared/goal_polarity.cpp).
Es más robusta que la versión "qué cámara lo ve": decide por **ángulo** ("el arco que tengo al frente es el
rival") con un **latch** que congela la decisión al arranque. No hay que construirla. Lo que falta es más
chico y más quirúrgico de lo que parecía.

> ⚠️ **Corrección importante respecto de una primera lectura:** NO es cierto que "hoy el delantero ataca
> siempre el arco amarillo físico y por eso en Incheon podría atacar su propio arco". Eso sería verdad si
> no existiera el TOP. Pero el TOP **autodetecta** la polaridad y se la pasa ya resuelta. En el build de
> competencia (`central_robot1_mix`, sin `-DMIX_ATTACK_BLUE`) **el delantero ya ataca el arco rival
> correcto.** El riesgo real es otro, mucho más acotado (ver §6).

---

## 1. Cómo funciona HOY el sistema (el dataflow real, verificado)

```
   cám frontal (N6) ─┐                          ┌─ goal_opp_*  (arco RIVAL  → ATACAR)
                     ├─► TOP ─► goal_polarity ─►┤
   cám trasera (N6) ─┘   (fusión front+back)    └─ goal_own_*  (arco PROPIO → DEFENDER)
        │                      │                        │
   ve amarillo Y azul    "el arco al frente          WorldSnapshot 0x60 @100Hz (UART)
   por color (LAB),       (|áng|<90°) es el            │
   manda AMBOS crudos     rival" + LATCH x30           ├─► DELANTERO MIX (centralmix): ataca goal_opp
   (headers 202/203)      (fija toda la 1ª mitad)      └─► ARQUERO MIX (arqueromix): HOY lo IGNORA
```

Las tres capas, con su archivo real:

1. **Cámaras OpenMV N6 (frontal + trasera).** Las dos corren el **mismo** programa y detectan los **dos
   arcos por color** (amarillo y azul) con umbrales LAB, además de la pelota naranja. Mandan posición cruda
   de cada color (header `202`=amarillo, `203`=azul), **sin decidir cuál es rival**. Es la arquitectura
   correcta: "cámara tonta, decide la placa".
   → [`hardware/electronics/camaras-openmv/main.py:47-49`](../../hardware/electronics/camaras-openmv/main.py)
   (LAB) y `:145-150` (packet v2 de 11 bytes con CRC8).

2. **Placa TOP — `goal_polarity` (acá vive tu idea).** Fusiona frontal+trasera e **infiere la polaridad**:
   el arco que el robot tiene **al frente** (`|ángulo| < 90°`) es el **rival**; el de atrás, el **propio**.
   Pasa por un **latch anti-rebote**: necesita **30 lecturas consistentes** (~0,3 s a 100 Hz) antes de
   fijar, y una vez fijada **no cambia en toda la mitad**. Si ve datos contradictorios (ambos arcos
   adelante) devuelve `UNKNOWN`. Si nunca confirma, usa un **fail-safe** (`YELLOW_IS_OPP` = amarillo rival).
   → [`software/teensy/Soccer 2026/src/shared/goal_polarity.cpp:9-34`](../../software/teensy/Soccer%202026/src/shared/goal_polarity.cpp)
   (inferencia) y `:42-61` (latch + fail-safe).
   → El mapeo final a `goal_opp/goal_own` lo hace [`src/top/snapshot_emitter.cpp:122-139`](../../software/teensy/Soccer%202026/src/top/snapshot_emitter.cpp)
   y `src/top/main_top.cpp:234-267`.

3. **Placa CENTRAL (donde corren los mix).** Recibe el `WorldSnapshot` con `goal_opp_*` (rival) y
   `goal_own_*` (propio) **ya resueltos** — ángulo + distancia + visible, **no color crudo**.
   → [`src/shared/types.h:114-123`](../../software/teensy/Soccer%202026/src/shared/types.h).

**Verificado en `platformio.ini`:** el flash de competencia del TOP es `top_robot2_pri` y trae
`-DTOP_ENABLE_SNAPSHOT_TIMER` (línea 390) → **corre `goal_polarity`**. Desde el recableado, **ambas TOP**
(la del delantero y la del arquero) corren ese mismo env (línea 23). Y **ningún** env define
`-DMIX_ATTACK_BLUE` ni `-DARQMIX_ATTACK_BLUE` → polaridad por default (`goal_opp → amarillo`).

---

## 2. Relevamiento por programa

### 2.1 DELANTERO MIX — `src/centralmix/` (Elías)

- **Qué hace con el arco:** lo recibe ya resuelto (`goal_opp`) y lo usa **solo como gatillo de patada**:
  orbita la pelota hasta tener el arco rival al frente dentro de `±60°` (`MIX_TOL_CENTRADO`) y ahí patea
  **siempre de frente** (no apunta con punteria fina; alinea por órbita).
  → `mix_fsm.cpp:418-419` y `:476-477` (gatillo), `mix_motors.cpp:180-208` (patada siempre adelante).
- **El "amarillo hardcodeado" — qué es realmente:** hay `kArcoRivalEsAmarillo = true` (`mix_fsm.cpp:86`)
  y un mapeo `goal_opp → goal_yellow_*` (`mix_comm.cpp:151-159`, rama `#else`/default). Encadenado, eso da
  `arco_rival == goal_opp`. O sea: **el "amarillo" es un alias de "el rival que dijo el TOP"**, no el color
  físico. En el build default **el delantero ataca el arco correcto autodetectado.**
- **Dónde sí hay un riesgo (acotado):** si alguien compilara con `-DMIX_ATTACK_BLUE` **dejando**
  `kArcoRivalEsAmarillo=true`, `mix_comm` invertiría el mapeo pero la FSM seguiría leyendo `yellow`=rival →
  **inconsistencia** que sí rompe. Hoy nadie lo compila así, pero es una trampa latente (ver §6, TEMA-2).
- **Deuda de documentación:** el `README` de centralmix dice *"Arco rival hardcodeado AMARILLO. Confirmar a
  qué arco ataca R1 antes del partido"* — **eso es engañoso**: el TOP ya lo autodetecta. Si no se corrige,
  Elías va a creer que tiene que "elegir el color a mano" cuando no hace falta.

### 2.2 ARQUERO MIX — `src/arqueromix/` (Virginia)

- **Qué hace con el arco:** **nada.** `grep goal_` sobre `amix_fsm.cpp` = 0 ocurrencias. La comm sí copia
  `goal_opp/goal_own` a `g_aio` pero el propio comentario aclara que es "por paridad/telemetría"
  (`amix_comm.cpp:88-89`), y la FSM nunca los lee.
- **Cómo sabe cuál es "su" arco:** por **geometría**, no por color. Al arrancar hace *homing*: retrocede
  (`retroceder_inicio()`, `amix_motors.cpp:167-172`) hasta ver la **línea del área** (sensor del DOWN), y
  ahí define "mi arco = atrás". → `amix_fsm.cpp:70-114`.
- **Cómo despeja:** rampa simétrica **recta al frente** del chasis (`amix_motors.cpp:137-155`), disparada
  cuando la pelota está cerca y centrada (`ball_para_despejar()`, `amix_fsm.cpp:56-60`). **No apunta a
  ningún arco** — patea "lejos", hacia donde mira el robot.
- **El riesgo:** si el robot quedó girado, "recto al frente" puede ser **hacia su propio arco** → autogol.
  El *homing* por línea no lo previene.

### 2.3 CENTRAL no-mix (semana pasada) — el patrón maduro

`hardware/electronics/central-board-pack/` + el firmware vivo `src/central/`. Es la referencia de cómo
debería ser: **separación de capas limpia**. CENTRAL no sabe de color; recibe `goal_opp/goal_own` y
**ejecuta**: se posiciona detrás de la pelota en la línea pelota→arco rival (`behind_ball.{h,cpp}`,
funciones puras parametrizadas por `goal_angle`) y orienta el heading-PID al arco rival. Si no ve el arco,
cae a un eje de ataque por BNO/OTOS (`src/central/strategy.cpp:817-839`, `atk_attack_axis`).
**Esta es la arquitectura a la que los dos mix tienen que converger.**

> ⚠️ **Dato clave para la propuesta:** el camino no-mix tiene el uso de `goal_own` **deshabilitado a
> propósito**, pendiente de validar en banco (`src/central/strategy.cpp:505` y `:661-669`), porque el arco
> propio lo ve la **cámara trasera** y su ángulo/distancia todavía no se validaron. Esto pesa sobre la
> propuesta del arquero (§6, TEMA-3).

---

## 3. Estrategias anteriores (campeones Nacional 2025)

- **DELANTERO 2025** (`_deprecated-2025/robot-delantero/definitivo-delantero.cpp:354-356`): arco rival
  **= amarillo, hardcodeado incondicional** en cada loop, bajo el comentario textual *"--- COLOCAR CUAL ES
  EL ARCO AL QUE AHI QUE HACER GOL ---"*. **Sin TOP que resolviera polaridad**: atacaba el amarillo físico
  siempre; cambiar de lado = editar y recompilar. El arco azul se decodificaba pero era código muerto.
  **(Ojo: este SÍ tenía el riesgo de "atacar tu propio arco" que el mix de hoy ya NO tiene, porque el mix
  hereda la variable pero el TOP cambió el comportamiento.)**

- **ARQUERO 2025** (`_deprecated-2025/robot-arquero/definitivo-arquero_6-9-2026`): el arquero real defendía
  por **geometría** (patrulla lateral por el signo de `Yp`, límite por sensor de línea blanca, despeje recto
  al frente). El `ARCO_CONTRINCANTE = amarillo` existía pero solo lo leían estados del bloque delantero
  co-residente, **inalcanzables** en el build arquero. **El arqueromix de hoy es un port fiel de esto** —
  por eso también ignora el arco.

- **VISIÓN 2025** (`_deprecated-2025/vision/enviar coordenadas 2 arcos y pelota`): la cámara (una sola, H7)
  detectaba pelota + **ambos arcos** por color y mandaba los dos **sin pre-decidir cuál es rival** — el
  patrón correcto, el mismo que las N6 de hoy. Umbrales LAB 2025 (formato OpenMV `Lmin,Lmax,Amin,Amax,Bmin,Bmax`):

  ```
  naranja  = (21, 67, 18, 79, -32, 127)   # pelota
  amarillo = (17, 70, -27, 14,  38, 111)   # arco amarillo  (canal B alto/positivo)
  azul     = ( 4, 36, -13, 57, -64,  -4)   # arco azul       (canal B muy negativo)
  ```
  Separa azul de amarillo por el **canal B**. Mensaje: 9 bytes `[201,Xp,Yp+100, 202,Xam,Yam+100, 203,Xaz,Yaz+100]`
  a 19200, **sin CRC** (frágil; las N6 ya lo mejoraron a 11 bytes con CRC8).

**Conclusión histórica:** la decisión de arco **siempre estuvo hardcodeada** (2025) o **se movió al TOP y se
automatizó** (2026, `goal_polarity`). El delantero mix ya cosechó esa mejora; el arquero mix todavía no.

---

## 4. Validación de tu idea (honesta, punto por punto)

> Tu idea: *"cámara frontal y trasera detectan color de arco al iniciar; color de adelante = atacar, color
> de atrás = defender; si no ve nada, color por defecto y listo."*

**Veredicto: tu intuición es correcta y ya está construida — redescubriste `goal_polarity`.** Lo que cambia
es la conclusión operativa: **no es "buena idea, construyámosla", es "buena idea, ya corre; falta que el
arquero la use".**

| Tu regla | Cómo está hoy en `goal_polarity` | Estado |
|---|---|---|
| "color de adelante = atacar" | "el arco con `\|ángulo\| < 90°` (hemisferio delantero) = rival" | ✅ Ya implementado, y mejor (por ángulo, no por "qué cámara") |
| "decidir al iniciar y dejarlo" | latch de 30 lecturas que **congela** la polaridad la 1ª mitad | ✅ Ya implementado |
| "si ve ambos colores adelante…" | tu idea no lo resuelve; el código devuelve `UNKNOWN` y no decide con datos contradictorios | ✅ Ya cubierto |
| "si no ve nada, color por defecto" | fail-safe `YELLOW_IS_OPP` | ✅ Existe — **pero es fijo en código, debería ser perilla** (mejora) |
| ruido de **un** frame | tu idea decidiría con un frame; el código exige 30 consistentes | ✅ Cubierto (mejor que la idea cruda) |
| **cambio de lado entre mitades** | el latch congela toda la mitad → en la 2ª queda invertido | ❌ **No cubierto.** Necesita re-latch (y eso es trabajo en el **TOP**, no en los mix) |
| **arranca mirando mal** (árbitro lo gira) | la inferencia inicial puede fijar **invertida** | ⚠️ Mitigado solo por el **procedimiento de mesa** "arrancar mirando a la cancha"; el código no lo valida |

**Dónde tu idea es más frágil que el código actual:** decidir por "qué cámara ve el color" es menos robusto
que decidir por ángulo. Ejemplo: si la frontal ve los **dos** arcos (robot en diagonal), "color de adelante"
es ambiguo; el criterio de ángulo + el `UNKNOWN` lo manejan mejor. Así que **conviene quedarse con el
mecanismo por ángulo que ya existe**, no reemplazarlo por "qué cámara".

---

## 5. Propuesta de estrategia (recomendada)

**No reinventar. Converger los dos mix al patrón maduro (`goal_polarity` → `goal_opp/goal_own`), que ya
corre.** Concretamente:

1. **El delantero mix ya está bien** en lo funcional (ataca `goal_opp`). Solo conviene **limpiar el
   doble-mapeo redundante** y **cerrar la trampa del flag** para que nadie lo rompa por accidente, y
   **corregir el README** que confunde.
2. **El arquero mix es el que tiene la oportunidad real:** que **empiece a leer `goal_own`** para no
   despejar hacia su propio arco — **de forma aditiva**, con "recto al frente" como fallback garantizado, y
   **solo después** de validar `goal_own_angle` en banco (hoy está deshabilitado en el camino no-mix por
   esa misma razón).
3. **El fallback configurable y el re-latch entre mitades** son mejoras válidas **pero son trabajo del
   TOP/`shared`, no de los mix.** Van en un track separado (lo toca quien mergea / Gustavo), **fuera** del
   trabajo paralelo de Elías y Virginia, porque tocar `goal_polarity`/`snapshot_emitter` afecta a **ambos
   robots y al camino no-mix**.

Pseudocódigo del estado objetivo (la mayoría **ya existe**, lo nuevo está marcado):

```
TOP (ya corre):   inferred = goal_polarity_infer(yellow_vis, yellow_ang, blue_vis, blue_ang)
                  polarity = latch_update(inferred)            // 30 consistentes, congela
                  goal_opp / goal_own  → WorldSnapshot         // ya resuelto, ya viaja

DELANTERO MIX:    apuntar/gatillar contra goal_opp_angle       // YA lo hace (vía alias yellow)
                  [NUEVO] borrar alias redundante + cerrar trampa del flag

ARQUERO MIX:      [NUEVO] si goal_own visible → despejar LEJOS de goal_own_angle
                          si no → recto al frente (comportamiento actual = fallback)

TOP (track aparte): [NUEVO] fallback por perilla (no #define) + re-latch entre mitades
```

---

## 6. Propuesta de implementación (quirúrgica, sin colisión)

> Formato coach. **Diseñada para que Elías (solo `centralmix/`) y Virginia (solo `arqueromix/`) trabajen en
> paralelo sin tocar el mismo archivo.** El mecanismo compartido (`goal_polarity`) **no lo edita ninguno de
> los dos.**

### (a) Cámaras OpenMV — **0 cambios para esta feature**
Ya detectan ambos arcos por color. **PERO** la calibración LAB es **precondición dura**, no un track P2
aparte: si los umbrales están mal para la luz de Incheon, `goal_polarity` nunca confirma y **todo degrada
al fallback amarillo**. Además hay **dos juegos de LAB** en el repo (`camaras-openmv/main.py` vs los packs
"calibrado 2026-06-09") y **no se sabe cuál está flasheado hoy**.
- **TEMA:** verificar qué LAB está en cada N6 y recalibrar a luz de Incheon (skill `openmv-vision-tuning`).
- `risk-no-fix`: la autodetección colapsa silenciosamente al default amarillo. `risk-fix`: meter variable
  nueva cerca del torneo. `tiempo`: 2–4 h banco. **Prioridad: P1 (precondición de todo lo demás).**

### (b) Contrato cámara→TOP→central — **0 cambios**
El `WorldSnapshot` ya trae `goal_opp_*/goal_own_*`. **No bumpear el schema.** Mandar color crudo o un bit
nuevo sería **wire-breaking** (re-flashear TOP+CENTRAL juntos) y en setup multi-placa es **prohibición dura
pre-Incheon**. `tiempo`: 0 h. **Prioridad: P2 (solo confirmar el Paso 0).**

### (c) DELANTERO MIX (Elías) — solo `src/centralmix/`
> **TEMA-1 — limpieza de claridad (no es bug activo):** que `arco_rival_*()` lea `goal_opp_*` directo y
> borrar el alias `kArcoRivalEsAmarillo`. Comportamiento **idéntico** al actual; gana legibilidad.
> `risk-no-fix`: ninguno funcional; sigue el modelo mental confuso. `risk-fix`: tocar la FSM cerca del
> torneo sin necesidad. `tiempo`: 1–2 h. **Prioridad: P2.**
>
> **TEMA-2 — cerrar la trampa del flag (este sí es el bug latente):** hacer que `-DMIX_ATTACK_BLUE` y
> `kArcoRivalEsAmarillo` no puedan quedar inconsistentes (idealmente, un solo punto de verdad; o un
> `static_assert`/comentario que lo impida). `risk-no-fix`: si alguien compila con el flag, el robot
> ataca al revés. `risk-fix`: mínimo. `tiempo`: 1 h. **Prioridad: P1.**
>
> **TEMA-5 — corregir el README de centralmix** (sacar "confirmar el color a mano"; explicar que el TOP
> autodetecta). `tiempo`: 20 min. **Prioridad: P1** (barato y evita un error de modelo mental que se
> propaga a las dos placas).

### (d) ARQUERO MIX (Virginia) — solo `src/arqueromix/`
> **TEMA-3 — que el arquero LEA `goal_own_angle` para no despejar a su propio arco.** Aditivo: si `goal_own`
> es visible, orientar el despeje **lejos** de ese ángulo; si no, "recto al frente" (comportamiento actual)
> como fallback garantizado.
> ⚠️ **Caveat fuerte:** `goal_own` viene de la **cámara trasera** y su ángulo **NO está validado en banco**
> — el camino no-mix lo tiene **deshabilitado** por eso (`src/central/strategy.cpp:505`). **Banco de
> validación de `goal_own_angle` ANTES de confiar en él.**
> `risk-no-fix`: posible autogol si el robot quedó girado. `risk-fix`: meter dependencia de visión (trasera,
> no validada) en un arquero que hoy es robusto justo por NO depender de visión. `tiempo`: 3–4 h +
> validación previa. **Prioridad: P1, pero bloqueada por el banco de `goal_own`.**
>
> **TEMA-4 (opcional) — usar `goal_own` para confirmar el signo del homing.** `tiempo`: 2 h. **Prioridad: P2.**

### (e) Track TOP (NO Elías/Virginia — lo toca quien mergea / Gustavo)
> **TEMA-6 — fallback por perilla runtime** (reemplazar el fail-safe fijo) **y TEMA-7 — re-latch entre
> mitades** (botón/perilla para re-arrancar la polaridad en la 2ª mitad). **Ambos tocan `goal_polarity` /
> `snapshot_emitter` / `main_top` = `src/shared` + TOP, archivos compartidos por R1, R2 y el camino no-mix.**
> Por eso van **fuera** del trabajo paralelo de los alumnos. `tiempo`: 4–6 h + banco. **Prioridad: P2**
> (mejora; el procedimiento de mesa cubre el caso mientras tanto).

### Orden sin colisión

```
Paso 0  (Gustavo/merge): confirmar que la TOP de CADA robot corre top_robot2_pri (goal_polarity ON)
        y emite goal_opp/goal_own. Verificado en platformio.ini; falta confirmarlo EN las placas.
        → No toca centralmix/ ni arqueromix/.

Paso 1  (Elías,  src/centralmix/,  su rama):   TEMA-2 (P1) + TEMA-5 (P1) + TEMA-1 (P2)
Paso 2  (Virginia, src/arqueromix/, su rama, EN PARALELO):  banco goal_own → TEMA-3 (P1) + TEMA-4 (P2)
        → Paso 1 y 2 NO comparten ningún archivo. Corren a la vez sin pisarse.
        → Ninguno edita src/shared/. (Sí lo COMPILAN vía build_src_filter, pero no lo modifican.)

Paso 3  (track TOP, aparte): TEMA-6 + TEMA-7. Secuencial, una sola mano sobre el TOP.
Paso 4  (Gustavo): merge secuencial de las ramas a main.
```

**Regla multi-agente:** si alguno necesitara tocar `src/shared/` o `platformio.ini`, primero
`git fetch && git log origin/main -10 -- <archivo>` (CLAUDE.md, regla #5).

---

## 7. Plan de prueba en hardware real (obligatorio)

> Sin esto ejecutado, queda en backlog. **Solo el equipo cierra estas TASK — Claude no las marca `done`.**

**Banco 0 — OBSERVABILIDAD (prerequisito).** Hoy la telemetría del TOP está **dormida** en `top_robot2_pri`
(competencia). Sin un canal para ver `goal_opp/goal_own` y el estado del latch, **nada de lo de abajo es
medible**. Definir cómo instrumentar (env de banco con telemetría, o LED/serial de diagnóstico) **antes**.

**Banco 1 — `goal_polarity` confirma bien.** Robot en el centro: encender mirando amarillo, luego azul,
luego girado 90° (ambos a los costados), luego sin ver arcos. **Aceptación:** mirando amarillo fija
`YELLOW_IS_OPP` < 1 s; mirando azul fija `BLUE_IS_OPP`; girado 90° → `UNKNOWN`+fallback; sin arcos →
fallback. **Un frame de ruido no debe invertir una polaridad ya fijada.**

**Banco 2 — DELANTERO MIX ataca el arco correcto (Elías).** Con polaridad fijada, 10 repeticiones por lado.
**Aceptación:** ≥9/10 orbita y patea hacia el rival, **nunca** hacia el propio.

**Banco 3 — ARQUERO MIX no autogolea (Virginia).** Pre-requisito: **Banco de `goal_own_angle`** (medir que
el ángulo del arco propio desde la trasera es confiable). Luego: despejar 10 veces bien orientado + 10 veces
girado 45°. **Aceptación:** el despeje sale **alejándose** del arco propio en ≥9/10 cuando `goal_own` es
visible; cuando no, cae a "recto" sin romper. **Cero despejes hacia el arco propio.**

**Banco 4 — re-latch entre mitades (track TOP).** Fijar polaridad, simular cambio de lado, ejecutar el
re-arranque y verificar que la nueva polaridad es la opuesta. **Documentar el procedimiento de mesa.**

---

## 8. Precondiciones y riesgos que NO hay que tapar

1. **Premisa de arranque (load-bearing):** `goal_polarity` asume que el robot **arranca mirando a la
   cancha** (al arco rival). Si el árbitro lo coloca girado, puede fijar la polaridad **invertida** para
   toda la mitad. El código no lo valida → **procedimiento de mesa obligatorio**, sobre todo para el arquero.
2. **`goal_own` no validado:** el camino no-mix lo tiene deshabilitado a propósito. La mejora del arquero
   (TEMA-3) **depende** de validarlo primero.
3. **Calibración LAB = precondición, no adorno:** mal calibrada → autodetección colapsa al fallback amarillo.
4. **Re-latch y perilla tocan el TOP**, no los mix. No meterlos en el trabajo paralelo de los alumnos.
5. **Dependencia del arquero con su TOP:** `arqueromix` necesita que la TOP **de su robot** corra
   `goal_polarity` y emita `goal_own`. Confirmar en banco (Paso 0), no asumir.

---

## 9. Qué queda para 2027

- **`goal_polarity.{h,cpp}` como pieza canónica reusable:** módulo PURO, host-testeable, con latch y
  fail-safe. Es el ejemplo de "decisión separada de visión y de motores". **Anotarlo en
  `FUENTES-DE-VERDAD.md` como el canónico de polaridad de arco** cuando esto se commitee.
- **Patrón "cámara tonta manda ambos, la placa decide con latch":** viene de 2025, es correcto, mantenerlo.
- **Procedimiento de mesa "arrancar mirando a la cancha + re-latch entre mitades":** hoy vive en la cabeza
  del equipo; escribirlo como checklist lo hace transferible.
- **Lección de arquitectura para TDP:** los mix nacieron con la deuda del 2025 (color hardcodeado / arco
  ignorado) por ser ports fieles; el delantero ya la superó vía el TOP, el arquero está a un paso.
  Documentar este "antes/después" puntúa en rúbrica (muestra iteración y entendimiento del sistema).

---

## 10. Gaps de evidencia (no inventar certeza)

1. Los **thresholds LAB físicamente flasheados hoy** y **cuál de los dos juegos** está en cada N6: requiere
   verificación en hardware.
2. No se corrió nada en hardware: el código mix está marcado NO TESTEADO (TASK-113/114 abiertas).
3. El mapeo exacto **robot ↔ env ↔ TOP** (R1/R2): `central_robot1_mix` = delantero y
   `central_robot2_arqueromix` = arquero según `platformio.ini`, pero el camino no-mix usa `ROBOT1=GK /
   ROBOT2=ATK` — **confirmar en banco** que cada CENTRAL está emparejada con la TOP correcta que emite
   `goal_own`.
4. La premisa "arranca mirando al rival" es un supuesto operativo, no validado por código.

---

*Generado con apoyo de Claude (workflow multi-agente de relevamiento + pasada adversarial). Atribución
según `AI-INSTRUCTIONS.md`. Documento de análisis — NO autoriza tocar firmware; espera evaluación de Gustavo.*
