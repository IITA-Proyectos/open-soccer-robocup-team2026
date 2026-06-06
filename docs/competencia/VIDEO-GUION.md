---
title: "GUION del Short Form Video TDP — RoboCupJunior Soccer Open 2026 (versión de trabajo ES)"
date: 2026-06-05
status: borrador-juzgado
idioma: español-rioplatense con subtítulos quemados en inglés (subtitular antes de entregar)
formato-objetivo: video < 3 minutos (Short Form Video TDP)
rubrica: RoboCupJunior Soccer 2026 — Short Form Video TDP (1 pt, escala 0/1; nivel máximo = Satisfactory=1)
equipo: "IITA Low Battery Messi — Salta, Argentina"
---

> # ⚠️ VERSIÓN DE TRABAJO EN ESPAÑOL
> **El video FINAL lleva narración en español rioplatense + SUBTÍTULOS QUEMADOS EN INGLÉS** durante todo el video (jueces internacionales). Todos los textos sobreimpresos (títulos, números, pasos) van en inglés en la versión final.
>
> *Working draft in Spanish. The FINAL video carries Spanish voice-over with burned-in ENGLISH subtitles throughout.*

---

# CÓMO LEER ESTE GUION (nota para el equipo, NO va en el video)

Este es el guion **shot-by-shot** del **Short Form Video TDP** (rúbrica RCJ Soccer 2026: **1 punto**, escala **0/1**; techo = **Satisfactory = 1**). La rúbrica pide UNA cosa para el punto: que el video muestre **la feature de la que el equipo está MÁS orgulloso, explicada claro para que OTRO competidor par la entienda y la aprenda**.

**Enfoque de esta versión:** la feature es el **testing host-native** (probar el cerebro del robot en la computadora, sin el hardware completo), contado como la **solución a un problema real del equipo**: reconstruimos dos robots desde cero para el mundial, y como en Argentina importar componentes es lento y la aduana obliga a fraccionar los pedidos, el robot nunca estaba completo — así que probamos el cerebro sin esperar a las piezas.

Cada fila de la tabla tiene 4 columnas:
- **Tiempo** — marca de inicio del shot (acumulativo). Cierra **< 3:00**.
- **Narración (ES)** — texto EXACTO que se locuta, a ~145 palabras/min.
- **Imagen en pantalla** — \`[IMG: ...]\` = qué se ve (grabar o capturar).
- **Texto/gráfico en pantalla** — sobreimpresos en INGLÉS en la versión final.

---

# CRITERIO DE RÚBRICA → DÓNDE SE CUMPLE (mapa para el juez)

| Criterio de rúbrica (Short Form Video TDP, 0/1) | Nivel apuntado | Dónde se cumple en este guion |
|---|---|---|
| **"Easy to follow / a peer competitor understands it"** (Satisfactory = 1) | **Satisfactory (= máximo, 1 pt)** | Historia problema→solución (bloques 1–5); ritmo ~145 ppm; subtítulos EN; jerga explicada; demo en pantalla de las suites en verde |
| **"Muestra LA feature de la que el equipo está MÁS orgulloso"** | Cumplido | Bloque 2 nombra la feature: **probar el cerebro del robot sin el hardware completo** |
| **"Otro competidor aprende de ella"** (replicabilidad — estándar de oro RCJ) | Cumplido | Bloque 4 da la receta de 3 pasos para que cualquier equipo lo copie |

---

# GUION SHOT-BY-SHOT (objetivo total: **< 3:00**)

## BLOQUE 1 — Gancho: la historia (criterio: "easy to follow", enganchar) · 0:00–0:30

| Tiempo | Narración (ES — texto exacto) | Imagen en pantalla | Texto/gráfico en pantalla (EN) |
|---|---|---|---|
| 0:00 | "Salimos campeones nacionales… con dos robots que, siendo honestos, tenían una estructura y una tecnología bastante pobres." | [IMG: foto/clip de los robots viejos de la competencia nacional; ambiente de cancha] | **Título:** "We won nationals — with two robots we knew weren't good enough" |
| 0:10 | "Así que para el mundial tomamos una decisión: rehacer dos robots desde cero, con algo mucho más decente. Somos IITA Low Battery Messi, de Salta, Argentina." | [IMG: foto del equipo; transición a los robots nuevos en construcción] | **Lower-third:** "IITA Low Battery Messi · Salta, Argentina · RoboCupJunior Soccer Open" |
| 0:20 | "Pero empezar de cero tuvo un problema que en Argentina conocemos bien: conseguir los componentes." | [IMG: piezas sueltas, PCBs sin poblar, mesa de trabajo a medio armar] | **Texto:** "Building from scratch had one big problem: getting the parts" |

## BLOQUE 2 — El problema real + la feature (criterio: nombra la feature) · 0:30–1:05

| Tiempo | Narración (ES — texto exacto) | Imagen en pantalla | Texto/gráfico en pantalla (EN) |
|---|---|---|---|
| 0:30 | "Acá importar es lento y complicado. No podíamos pedir todo junto ni todo en un mismo lugar: la aduana nos obligaba a fraccionar los pedidos. Los componentes llegaban de a poco, durante semanas." | [IMG: animación simple de envíos fraccionados llegando en distintas fechas; o cajas llegando de a una] | **Texto:** "Argentina imports = slow + must split orders across shipments → parts arrived piece by piece" |
| 0:46 | "O sea: durante mucho tiempo NO tuvimos el robot completo para probar nada físicamente. Y el mundial no esperaba. Entonces, ¿cómo avanzás cuando todavía no tenés el robot?" | [IMG: robot a medio armar, faltando piezas; reloj/calendario marcando el tiempo] | **Texto:** "No complete robot to test on — and the deadline kept coming" |
| 0:58 | "Esta es la feature de la que estamos más orgullosos: pudimos probar el CEREBRO del robot sin tener el robot." | [IMG: corte a una notebook con una terminal limpia] | **Título:** "Our proudest feature: testing the robot's BRAIN without the robot" |

## BLOQUE 3 — Cómo lo hicimos + la demo (criterio: "easy to follow" + el juez VE la prueba) · 1:05–1:55

| Tiempo | Narración (ES — texto exacto) | Imagen en pantalla | Texto/gráfico en pantalla (EN) |
|---|---|---|---|
| 1:05 | "La idea es simple: separamos el programa del robot en dos partes. Por un lado, el cerebro —decidir adónde ir, cómo patear, la estrategia— en módulos puros, sin nada que dependa del hardware." | [IMG: diagrama de 2 capas: "BRAIN (pure logic)" arriba, "hardware glue (thin)" abajo, con flecha] | **Diagrama:** "Brain (pure logic) ↔ thin hardware layer" |
| 1:20 | "Como ese cerebro no depende de los sensores ni los motores, lo corremos enterito en la computadora. Sin la placa, sin batería, sin esperar que lleguen las piezas." | [IMG: captura del comando empezando a correr la suite de tests] | **Texto:** "The brain runs on a laptop — no board, no battery, no waiting for parts" |
| 1:33 | "Y esto es lo que vemos al correr todas las pruebas, hoy mismo:" | [IMG: **screencast REAL** de la terminal: línea por línea PASS… PASS… acelerado] | **Texto:** "live run · 2026-06" |
| 1:41 | "Cientos de pruebas automáticas. Cero fallos. En segundos, sin tocar el robot. Cada vez que encontrábamos un error, lo dejábamos como una prueba para que no vuelva a pasar nunca más." | [IMG: congelar en la línea final con el total de tests y 0 failures; zoom y resaltado] | **Número grande:** "Hundreds of tests / 0 failures · in seconds" |

> **Nota de producción:** confirmar el número exacto de tests/suites corriendo \`scripts/run-host-tests.sh\` el día de grabar y poner ESE número en pantalla (no hardcodear una cifra vieja). Ver "Registro de gaps".

## BLOQUE 4 — Cómo lo replicás vos (criterio: "another competitor learns from it") · 1:55–2:35

| Tiempo | Narración (ES — texto exacto) | Imagen en pantalla | Texto/gráfico en pantalla (EN) |
|---|---|---|---|
| 1:55 | "Y lo mejor: lo podés copiar para tu robot, sobre todo si a vos también te cuesta conseguir el hardware. Son tres pasos." | [IMG: tarjeta "How to replicate — 3 steps"] | **Título:** "Replicate it in 3 steps" |
| 2:01 | "Uno: sacá la lógica de decisión del código del robot y ponela en módulos aparte, que no dependan de los sensores ni los motores." | [IMG: split de código: lógica mezclada (tachado) vs. módulo puro limpio] | **Paso 1:** "Move decision logic into PURE modules (no hardware deps)" |
| 2:12 | "Dos: guardá el framework de tests dentro de tu propio repositorio, así no dependés de internet ni de que algo te lo bloquee." | [IMG: árbol de carpetas resaltando la carpeta del framework de tests] | **Paso 2:** "Vendor the test framework into your repo (works offline)" |
| 2:22 | "Tres: compilá y corré esas pruebas en la computadora con un solo comando. Cada vez que cambiás algo, en segundos sabés si rompiste el cerebro del robot." | [IMG: captura del script de una línea corriendo] | **Paso 3:** "Compile & run the tests on your laptop with one command" |

## BLOQUE 5 — Cierre + el robot real (criterio: replicabilidad / open-source) · 2:35–2:58

| Tiempo | Narración (ES — texto exacto) | Imagen en pantalla | Texto/gráfico en pantalla (EN) |
|---|---|---|---|
| 2:35 | "Cuando por fin llegaron todas las piezas, el cerebro ya estaba probado. Y el robot empezó a moverse." | [IMG: **tomas del robot nuevo moviéndose en la cancha** — las que graben en las 1–2 semanas siguientes] | **Texto:** "When the parts finally arrived, the brain was already tested" |
| 2:46 | "Todo nuestro código está abierto, con licencia MIT, en GitHub. Si construir tu robot también es cuesta arriba, llevátelo. Nos vemos en Incheon. ¡Gracias!" | [IMG: pantallazo del repo en GitHub; cierra con tarjeta de equipo + handle] | **Tarjeta final:** "Open-source · MIT · github.com/IITA-Proyectos/open-soccer-robocup-team2026 · IITA Low Battery Messi · Incheon 2026" |

> **Cierre del video: ≈ 2:58** — bajo el límite de 3:00. Si la locución se estira, el primer recorte es la segunda mitad de la narración del Bloque 1 (0:10) o acortar el Bloque 4.

---

# SHOT LIST (qué grabar / capturar, en orden de prioridad)

### A. Screencasts (la prueba en pantalla — lo más importante)
1. **[CAPTURA — CRÍTICA]** Screencast REAL de la suite de tests corriendo de principio a fin, con la línea final del total de tests + 0 failures bien legible. Terminal con fuente grande y alto contraste. Confirmar el número el día de grabar.
2. **[CAPTURA]** El comando / script de una línea corriendo.
3. **[CAPTURA]** Árbol de carpetas mostrando los módulos puros y la carpeta del framework de tests vendoreado.
4. **[CAPTURA]** Pantallazo del repo en GitHub con el código y el archivo LICENSE (MIT) visible.
5. **[CAPTURA]** Split de código "antes/después": lógica mezclada vs. módulo puro limpio.

### B. Diagramas a producir (motion graphics)
1. **[DIAGRAMA]** Las 2 capas: "Brain (pure logic)" ↔ "thin hardware layer". Reutilizable en Bloques 3 y 4.
2. **[GRÁFICO]** Tarjeta "Replicate it in 3 steps".
3. **[OPCIONAL]** Animación simple de los envíos fraccionados llegando en distintas fechas (Bloque 2).

### C. Footage del robot / equipo (rollo B)
1. **[FOTO/CLIP]** Robots viejos de la competencia nacional (gancho 0:00).
2. **[FOTO]** Foto del equipo (0:10).
3. **[CLIP]** Robots nuevos en construcción / piezas sueltas / PCBs sin poblar (0:10–0:46).
4. **[CLIP — graba 1–2 semanas después]** Robot nuevo moviéndose en la cancha (cierre, Bloque 5). **Esta es la única toma que puede grabarse después del domingo.**

---

# NOTAS DE PRODUCCIÓN

- **Duración:** objetivo **< 3:00** (límite duro). Conteo ≈ 2:58.
- **Idioma / subtítulos:** narración en **español rioplatense** + **subtítulos quemados en inglés** durante TODO el video. Todos los textos sobreimpresos en inglés.
- **Audio:** voz clara; música de fondo suave y baja, sin tapar la narración. Locutar despacio en el Bloque 4 (los 3 pasos son lo que el competidor tiene que retener).
- **Legibilidad de las terminales:** fuente ≥ 18 pt, alto contraste, zoom sobre la línea del total de tests. Es el momento clave: el juez tiene que LEER el número.
- **Honestidad técnica (no sobrevender):** el video se centra en lo que está verificado y es demostrable (las pruebas corren y pasan hoy; el robot se mueve en las tomas reales). No afirmar que features no validadas en cancha "ya juegan al fútbol".
- **Plan de grabación realista:**
  - **Domingo:** toda la narración + screencasts + diagramas + footage de piezas/robots en construcción + foto del equipo.
  - **1–2 semanas después:** solo las tomas del robot nuevo moviéndose en cancha (Bloque 5). El video se edita y cierra cuando lleguen esas tomas.

---

# REGISTRO DE GAPS (datos reales faltantes — completar antes de cerrar el video)

| # | Gap | Tipo | Dónde impacta |
|---|---|---|---|
| 1 | ✅ **RESUELTO:** nombre del equipo = **IITA Low Battery Messi** (IITA = Instituto de Innovación y Tecnología Aplicada, la institución que les enseña robótica) | Identificación | Título, lower-third, tarjeta final |
| 2 | **[NÚMERO DE TESTS]** correr \`scripts/run-host-tests.sh\` el día de grabar y poner el total real (tests / suites / 0 failures) en pantalla en 1:41 | Dato | Bloque 3 |
| 3 | **[FOTO/CLIP]** robots VIEJOS de la competencia nacional (gancho 0:00) | Footage | Bloque 1 |
| 4 | **[FOTO]** equipo + **[CLIP]** robots nuevos en construcción / piezas sueltas | Footage | Bloques 1–2 |
| 5 | **[CLIP — post-domingo]** robot nuevo moviéndose en cancha (cierre) | Footage | Bloque 5 |
| 6 | **[CAPTURA]** screencast real de la suite de tests + split de código antes/después | Captura | Bloque 3 y 4 |
