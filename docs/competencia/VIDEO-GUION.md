---
title: "GUION del Short Form Video TDP — RoboCupJunior Soccer Open 2026 (versión de trabajo ES)"
date: 2026-06-04
status: borrador-juzgado
idioma: español-rioplatense con subtítulos en inglés (TRADUCIR/subtitular antes de entregar)
formato-objetivo: video < 3 minutos (Short Form Video TDP)
rubrica: RoboCupJunior Soccer 2026 — Short Form Video TDP (1 pt, escala 0/1; nivel máximo = Satisfactory=1)
---

> # ⚠️ VERSIÓN DE TRABAJO EN ESPAÑOL
> **El video FINAL conviene subtitularlo en INGLÉS** para los jueces internacionales (la narración puede quedar en español rioplatense con subtítulos quemados en inglés, o regrabarse la voz en inglés). El resto de los entregables (poster, TDP) **deben** estar en inglés por rúbrica; el video acepta narración nativa + subtítulos.
>
> *Working draft in Spanish. The FINAL video should carry burned-in ENGLISH subtitles for international judges (Spanish voice-over is acceptable for the Short Form Video).*

---

# CÓMO LEER ESTE GUION (nota para el equipo, NO va en el video)

Este archivo es el **guion shot-by-shot** del **Short Form Video TDP** (rúbrica RCJ Soccer 2026: **1 punto**, escala **0/1**; el techo es **Satisfactory = 1**). La rúbrica pide UNA sola cosa para sacar el punto: que el video **muestre LA feature de la que el equipo está MÁS orgulloso, explicada de forma clara para que OTRO competidor par la entienda y aprenda de ella**.

Cada fila de la tabla tiene 4 columnas:
- **Tiempo** — marca de inicio del shot (cumulativo). El total cierra **< 3:00**.
- **Narración (ES)** — el **texto EXACTO** que se locuta. Calculado a **~145 palabras/min** (rango pedido 140–160).
- **Imagen en pantalla** — `[IMG: ...]` = qué se ve (grabar o capturar).
- **Texto/gráfico en pantalla** — lower-thirds, títulos y números que aparecen sobreimpresos (en **inglés** en la versión final).

Los **títulos de bloque mapean 1:1 al criterio de la rúbrica** para que el JUEZ encuentre de un vistazo por qué este video se gana el punto. Abajo de la tabla: **shot list**, **notas de producción** y **registro de gaps** (datos reales faltantes).

---

# CRITERIO DE RÚBRICA → DÓNDE SE CUMPLE (mapa para el juez)

| Criterio de rúbrica (Short Form Video TDP, 0/1) | Nivel apuntado | Dónde se cumple en este guion |
|---|---|---|
| **"Easy to follow / a peer competitor understands it"** (Satisfactory = 1) | **Satisfactory (= máximo, 1 pt)** | Estructura problema→solución→cómo-replicarlo (bloques 1–5); ritmo ~145 ppm; subtítulos EN; jerga explicada; demo en pantalla de 40 suites en verde (545 tests / 40 suites / 0 failures, verificado 2026-06-04 vía scripts/run-host-tests.sh) |
| **"Muestra LA feature de la que el equipo está MÁS orgulloso"** | Cumplido | Bloque 1 nombra la feature en los primeros 12 s: **testing host-native de firmware embebido** |
| **"Otro competidor aprende de ella"** (replicabilidad — estándar de oro RCJ) | Cumplido | Bloque 4 da la **receta exacta** (módulos puros + `g++` + Unity vendoreado) para que cualquier equipo la copie |

---

# POR QUÉ ESTA FEATURE (justificación de la elección — nota para el equipo)

Se eligió **el testing host-native de firmware embebido** como "feature más orgullosa" entre las 5 candidatas del proyecto. Razones, en orden de peso para ESTE entregable:

1. **Es la única que se demuestra LITERALMENTE en pantalla en segundos.** Un video puede mostrar **40 suites de tests pasando en verde en una terminal** — el juez y cualquier competidor *ven* la prueba, no se las cuento. Las otras features (fail-safe en 3 placas, trilateración ToF, arquero que anticipa) son potentes pero hoy están **"verificadas solo en host" y NO validadas en cancha** (visión sin recalibrar = bloqueante #1; cinemática sin calibrar; pose nunca sale `valid`); mostrarlas como terminadas sería deshonesto y arriesgado.
2. **Resuelve un dolor que TODOS los equipos RCJ tienen:** verificar firmware embebido sin quemar horas de banco ni depender de la placa física. Un competidor par se lleva algo que **puede aplicar a su propio robot el lunes**. Eso es exactamente lo que el criterio premia ("learn from it").
3. **Tiene un número real, fresco y verificable HOY.** Corrí la suite el **2026-06-04** y dio **545 tests / 40 suites / 0 failures (verified 2026-06-04 via scripts/run-host-tests.sh)** en segundos, 100% offline. No es una promesa: es reproducible con `bash scripts/run-host-tests.sh`.
4. **Tiene una historia de origen concreta y relatable:** el antivirus **Avast** bloqueaba el registry de PlatformIO (`pio test` no podía bajar Unity), así que el equipo lo **esquivó** vendoreando Unity y compilando con `g++`. Obstáculo real → solución ingeniosa → ahora cualquiera lo replica.
5. **Es la más "replicable"**, y la replicabilidad es el estándar de oro de RCJ. La receta cabe en 20 segundos de pantalla.

> Las otras 4 features tienen su lugar en el **TDP** y el **poster** (donde se las describe con su estado honesto de "host-tested vs bench-validated"). Para el **video de 1 punto**, gana la que un par entiende y copia más rápido.

---

# GUION SHOT-BY-SHOT (objetivo total: **< 3:00**)

> **Conteo de palabras de narración:** ~410 palabras → a 145 ppm ≈ **2 min 50 s** de locución + colas de respiración → cierra **bajo 3:00**. (Si queda justo, recortar la narración del Bloque 3 que está marcada como *opcional*.)

## BLOQUE 1 — Gancho + nombre de la feature (criterio: "muestra LA feature más orgullosa") · 0:00–0:18

| Tiempo | Narración (ES — texto exacto) | Imagen en pantalla | Texto/gráfico en pantalla (EN en versión final) |
|---|---|---|---|
| 0:00 | "¿Cómo verificás que el firmware de tu robot está bien… sin la placa, sin batería, y sin perder horas de banco?" | [IMG: plano corto del robot armado sobre la cancha, quieto; luego corte rápido a una notebook] | **Título:** "How we test robot firmware — WITHOUT the board" · logo [NOMBRE DEL EQUIPO] |
| 0:08 | "Somos [NOMBRE DEL EQUIPO], de Salta, Argentina. Esta es la feature de la que estamos más orgullosos: **testing host-native de firmware embebido**." | [IMG: foto del equipo / del robot; transición a una terminal limpia] | **Lower-third:** "[TEAM NAME] · Salta, Argentina · RoboCupJunior Soccer Open" |

## BLOQUE 2 — El problema (criterio: "easy to follow" — primero el porqué) · 0:18–0:48

| Tiempo | Narración (ES — texto exacto) | Imagen en pantalla | Texto/gráfico en pantalla (EN) |
|---|---|---|---|
| 0:18 | "El problema: la lógica de decisión de un robot de fútbol —cinemática, PIDs, la máquina de estados, el protocolo entre placas— es difícil de probar EN la placa. Cada cambio significa compilar, flashear y mirar el robot. Lento y caro." | [IMG: timelapse de flashear un Teensy y mover el robot a mano en la cancha; reloj corriendo en una esquina] | **Texto:** "Edit → flash → watch the robot → repeat = SLOW" |
| 0:34 | "Y encima nos pegó un obstáculo real: nuestro antivirus, Avast, bloqueaba el registry de PlatformIO. El comando `pio test` no podía bajar el framework de tests. Estábamos trabados." | [IMG: captura de pantalla del error de PlatformIO / Avast bloqueando la descarga (recrear o screenshot real)] | **Texto:** "Avast blocked PlatformIO's registry → `pio test` couldn't run" |

## BLOQUE 3 — La idea + la demo en vivo (criterio: "easy to follow" + el juez VE la prueba) · 0:48–1:48

| Tiempo | Narración (ES — texto exacto) | Imagen en pantalla | Texto/gráfico en pantalla (EN) |
|---|---|---|---|
| 0:48 | "La idea es simple: separamos el firmware en dos. Por un lado, **módulos PUROS** en C++ —sin Arduino, sin `Serial`, sin `Wire`— que contienen TODA la lógica de decisión. Por el otro, una capa de pegamento Arduino, finita, que solo conecta esos módulos a los pines." | [IMG: diagrama animado de 2 capas: "PURE C++ (logic)" arriba, "Arduino glue (thin)" abajo, con una flecha; resaltar `src/shared/`] | **Diagrama:** "src/shared/ = PURE modules (no Arduino)  ·  glue = thin" |
| 1:04 | "Como esos módulos no dependen del hardware, los compilamos y corremos en la PC con `g++`, usando el framework Unity que dejamos guardado dentro del repo. Sin internet, sin registry, sin Avast en el medio." | [IMG: captura del comando `bash scripts/run-host-tests.sh` empezando a correr] | **Texto:** "Compile with `g++` + vendored Unity → runs on the laptop, 100% offline" |
| 1:18 | "Y esto es lo que vemos al correr la suite, hoy mismo:" | [IMG: **screencast REAL** de la terminal escupiendo línea por línea `PASS test_proto`, `PASS test_kinematics`, `PASS test_localization`… acelerado] | **Texto (aparece al cierre):** "live run · 2026-06-04" |
| 1:26 | "Cuarenta suites de tests. Quinientos cuarenta y cinco casos. Cero fallos. En segundos, sin tocar el robot." | [IMG: congelar en la línea final `PASS=40  FAIL=0  SKIP=0  (tests corridos: 545)`; zoom y resaltado] | **Número grande:** "545 tests / 40 suites / 0 failures · in seconds"<br>[FIGURE: docs/competencia/assets/fig8_test_growth.png] |
| 1:36 | *(opcional, recortable si pasa de 3:00)* "Y no es magia de una vez: la suite creció de forma trazable sesión a sesión —180, 246, 324, 403… hasta 545— cada bug que encontramos quedó como un test que nunca vuelve a pasar desapercibido." | [IMG: gráfico de barras animado del crecimiento 180→246→324→403→545 — usar `docs/competencia/assets/fig8_test_growth.png` (gen_figuras.py)] | **Gráfico:** "Tests over time: 180 → 246 → 324 → 403 → 545" |

## BLOQUE 4 — Cómo lo replicás vos (criterio: "another competitor learns from it" — el corazón del punto) · 1:48–2:30

| Tiempo | Narración (ES — texto exacto) | Imagen en pantalla | Texto/gráfico en pantalla (EN) |
|---|---|---|---|
| 1:48 | "Lo mejor: lo podés copiar para tu robot. Son tres pasos." | [IMG: tarjeta "How to replicate — 3 steps" apareciendo] | **Título:** "Replicate it in 3 steps" |
| 1:53 | "Uno: sacá la lógica de decisión del código Arduino y ponela en módulos puros, en archivos aparte. Pasale el tiempo como parámetro en vez de llamar a `millis()`, así los tests son deterministas." | [IMG: split de código: a la izquierda `loop()` con lógica mezclada (tachado), a la derecha un módulo puro limpio] | **Paso 1:** "Move decision logic into PURE modules · inject time, don't call millis()" |
| 2:06 | "Dos: guardá el framework Unity dentro de tu repo, así no depende de internet ni de un antivirus." | [IMG: árbol de carpetas resaltando `lib/Unity/`] | **Paso 2:** "Vendor Unity into `lib/Unity` (no network needed)" |
| 2:14 | "Tres: compilá cada test con `g++`, linkeando tus módulos puros, y corré el binario en la PC. Nosotros lo dejamos en un script de una línea: `run-host-tests.sh`." | [IMG: captura del comando, con `cd "software/teensy/Soccer 2026"` arriba y luego `g++ -std=gnu++17 -I src/shared -I src/down -I lib/Unity/src lib/Unity/src/unity.c src/shared/*.cpp test/test_X/test_main.cpp -o ...`; abajo `bash scripts/run-host-tests.sh`] | **Paso 3:** `cd "software/teensy/Soccer 2026"` → `g++ ... -I lib/Unity/src ...` link pure modules + run on host · one script: `run-host-tests.sh` |
| 2:24 | "El glue Arduino queda fino y se verifica solo compilando. Toda la inteligencia, probada antes de prender el robot." | [IMG: diagrama de 2 capas del Bloque 3 reapareciendo, con un check verde sobre "PURE C++"] | **Texto:** "Logic verified BEFORE powering the robot" |

## BLOQUE 5 — Cierre + dónde aprenderlo (criterio: replicabilidad / open-source) · 2:30–2:55

| Tiempo | Narración (ES — texto exacto) | Imagen en pantalla | Texto/gráfico en pantalla (EN) |
|---|---|---|---|
| 2:30 | "Esto nos dejó iterar rápido y sin miedo, incluso a días de competir. Todo el código y el script están abiertos, con licencia MIT, en nuestro repo de GitHub." | [IMG: pantallazo del repo en GitHub mostrando `scripts/run-host-tests.sh` y la carpeta `test/`] | **Texto:** "Open-source · MIT · github.com/IITA-Proyectos/open-soccer-robocup-team2026" |
| 2:42 | "Si sos competidor de RoboCupJunior y querés probar tu firmware sin la placa, copiá la receta. Nos vemos en Incheon. ¡Gracias!" | [IMG: robot moviéndose en la cancha (clip corto); cierra con tarjeta de equipo + handle] | **Tarjeta final:** "[TEAM NAME] · RoboCupJunior Soccer Open · Incheon 2026 · github.com/IITA-Proyectos/open-soccer-robocup-team2026" |

> **Cierre del video: ≈ 2:55** — bajo el límite de 3:00 con margen. Si la locución se estira, recortar la narración *opcional* del Bloque 3 (0:48 → vuelve a ≈ 2:43).

---

# SHOT LIST (qué grabar / capturar, en orden de prioridad)

### A. Screencasts (lo más importante — son la prueba en pantalla)
1. **[CAPTURA — CRÍTICA]** Screencast REAL de `bash scripts/run-host-tests.sh` corriendo de principio a fin, con la última línea `PASS=40  FAIL=0  SKIP=0  (tests corridos: 545)` bien legible. *(Verificado el 2026-06-04: la suite da exactamente eso, exit code 0.)* Grabar en una terminal con fuente grande y tema de alto contraste.
2. **[CAPTURA]** El comando, precedido por `cd "software/teensy/Soccer 2026"`, luego `g++ -std=gnu++17 -I src/shared -I src/down -I lib/Unity/src lib/Unity/src/unity.c src/shared/*.cpp test/test_X/test_main.cpp -o ...` (de `scripts/run-host-tests.sh`, líneas 32–34) en pantalla, resaltando que el include real es `lib/Unity/src` (NO `lib/Unity`) y que linkea `src/shared/*.cpp`.
3. **[CAPTURA]** Recrear (o screenshot real) del **error de Avast / PlatformIO registry** bloqueando `pio test` (contexto: TASK-025). Si no se puede recrear el error exacto, mostrar la nota en `docs/ESTADO-ACTUAL.md` que lo documenta.
4. **[CAPTURA]** Árbol de carpetas mostrando `lib/Unity/` y `test/test_*/` (hay **40+ carpetas de test**).
5. **[CAPTURA]** Pantallazo del repo en **GitHub** (org `IITA-Proyectos`) con `scripts/run-host-tests.sh` y la carpeta `test/` abiertas, y el archivo `LICENSE` (MIT) visible.
6. **[CAPTURA]** Split de código "antes/después": un módulo puro limpio (ej. `src/shared/ball_predict.cpp` o `src/shared/kinematics.cpp`) al lado de un `loop()` con lógica mezclada (puede ser ilustrativo).

### B. Diagramas a producir (motion graphics)
1. **[DIAGRAMA]** Las **2 capas**: "PURE C++ modules (`src/shared/`, no Arduino)" arriba ↔ "Arduino glue (thin)" abajo, con flecha. Reutilizable en Bloques 3 y 4.
2. **[DIAGRAMA]** **Gráfico de barras del crecimiento de tests**: 180 → 246 → 324 → 403 → 545 (fechas: snapshots de `docs/ESTADO-ACTUAL.md`; 545 verificado 2026-06-04).
3. **[GRÁFICO]** Tarjeta "Replicate it in 3 steps" con los 3 pasos.

### C. Footage del robot / equipo (rollo B)
1. **[FOTO/CLIP]** Robot armado sobre la cancha, vista corta (gancho 0:00 y cierre 2:42).
2. **[FOTO/CLIP]** Timelapse de flashear el Teensy + mover el robot a mano (Bloque 2, ilustra "lento y caro").
3. **[FOTO]** Foto del equipo o de los competidores (Bloque 1, identificación).
4. **[CLIP]** Robot moviéndose en cancha para el cierre.

---

# NOTAS DE PRODUCCIÓN

- **Duración:** objetivo **< 3:00** (límite duro de rúbrica). Conteo: ~410 palabras de narración a **~145 ppm** ≈ **2:50** + colas ≈ **2:55**. La narración *opcional* del Bloque 3 (gráfico de crecimiento) es el primer recorte si hay que ganar tiempo.
- **Idioma / subtítulos:** la narración va en **español rioplatense**; **subtítulos quemados en inglés** durante TODO el video (la rúbrica/jueces son internacionales). Alternativa: regrabar voz en inglés. Todos los **textos sobreimpresos en pantalla van en inglés** en la versión final (en esta maqueta están en EN para facilitar la traducción).
- **Audio:** voz clara, sin música tapando la narración; música de fondo suave y baja. Locutar despacio en el Bloque 4 (los 3 pasos son lo que el competidor tiene que retener).
- **Legibilidad de las terminales:** fuente ≥ 18 pt, tema de alto contraste, zoom/resaltado sobre la línea `PASS=40 FAIL=0 SKIP=0 (tests corridos: 545)`. Es el momento clave: el juez tiene que LEER el número.
- **Honestidad técnica (no sobrevender):** el video se centra en lo que está **verificado y es demostrable** (la suite corre y pasa hoy). No afirmar que features no validadas en banco "funcionan en cancha". Esto protege Sportsmanship y la credibilidad ante el juez.
- **Datos reales ya verificados (usar verbatim):**
  - **545 tests / 40 suites / 0 failures (verified 2026-06-04 via scripts/run-host-tests.sh)** (exit code 0). *(NOTA: la rúbrica y otros entregables citaban un tope previo más bajo; el número subió a 545 tests / 40 suites — está verificado en esta sesión y es la cifra a publicar.)*
  - Crecimiento trazable: **180 → 246 → 262 → 324 → 354 → 403 → 545** (snapshots en `docs/ESTADO-ACTUAL.md`).
  - Comando exacto y receta: `scripts/run-host-tests.sh` (líneas 32–34).
  - Origen: Avast bloqueaba el registry de PlatformIO → **TASK-025**; solución = Unity vendoreado en `lib/Unity` + `g++`.
  - Licencia **MIT**, repo público: https://github.com/IITA-Proyectos/open-soccer-robocup-team2026 (org `IITA-Proyectos`).
  - **Organización: IITA** (usar el acrónimo de forma consistente). ⚠️ La expansión legal es un **gap real, NO resolver a ciegas**: el `LICENSE` dice "Instituto de **Innovación** y Tecnología Aplicada"; el `README`/`POSTER` dicen "Instituto de **Informática** y Tecnología Aplicada". Donde haya que expandir, escribir: "IITA (Instituto de [Innovación/Informática] y Tecnología Aplicada — VERIFY legal name)".
- **Continuidad con los otros entregables:** este video destaca la MISMA feature #1 que el poster y el TDP marcan como su diferencial de proceso, así el juez ve un mensaje coherente entre video, poster y TDP.

---

# REGISTRO DE GAPS (datos reales faltantes — completar antes de grabar)

| # | Gap | Tipo | Dónde impacta en el video |
|---|---|---|---|
| 1 | **[NOMBRE DEL EQUIPO]** oficial registrado en RoboCup Incheon 2026 (el repo usa "IITA — Open Soccer RoboCup Team 2026" como descriptor interno) | Identificación | Título 0:00, lower-third 0:08, tarjeta final 2:42 |
| 2 | **[REGIÓN]** / nombre de la regional o superregional con que clasificaron (Salta, Argentina confirmado; falta la regional formal) | Identificación | Lower-third 0:08 (hoy dice solo "Salta, Argentina") |
| 3 | **URL del repo RESUELTA** → https://github.com/IITA-Proyectos/open-soccer-robocup-team2026 (ya sobreimpresa en 2:30 y 2:42; sin gap pendiente) | Open-source | Texto 2:30 y tarjeta final 2:42 |
| 3b | **Nombre legal de IITA SIN resolver:** `LICENSE` dice "Instituto de **Innovación** y Tecnología Aplicada"; `README`/`POSTER` dicen "Instituto de **Informática**…". Verificar el nombre legal real antes de expandir el acrónimo en pantalla/locución | Identificación | Lower-third 0:08, notas |
| 4 | **[FOTO/CLIP: robot 2026 armado moviéndose en la cancha]** — clip real para gancho (0:00) y cierre (2:42) | Footage | Bloques 1 y 5 |
| 5 | **[FOTO: equipo / competidores]** para la identificación | Footage | Bloque 1 (0:08) |
| 6 | **[CLIP: timelapse de flashear el Teensy + mover el robot a mano]** para ilustrar "lento y caro" | Footage | Bloque 2 (0:18) |
| 7 | **[CAPTURA: error real de Avast/PlatformIO bloqueando `pio test`]** — recrear o screenshot; si no, mostrar la nota de TASK-025 en `ESTADO-ACTUAL.md` | Captura | Bloque 2 (0:34) |
| 8 | **[DECISIÓN]** ¿voz en español con subtítulos EN, o regrabar voz en inglés? Definir antes de locutar | Producción | Todo el video |
| 9 | **[VERIFICAR al cerrar]** re-correr `scripts/run-host-tests.sh` el día de grabar para confirmar que la cifra sigue siendo **545/40/0** (o actualizar el número sobreimpreso si cambió por nuevos tests) | Dato | Bloque 3 (1:26, 1:36) |
| 10 | **[OPCIONAL]** confirmar fechas exactas de cada snapshot del gráfico de crecimiento (180→…→545) si se quiere fechar las barras | Dato | Bloque 3 (1:36, narración opcional) |
