# Group Team Interview — Cheat-Sheet del equipo (RoboCupJunior Soccer Open 2026, Incheon)

> **Qué es la Group Team Interview (5 pts):** desafío en vivo junto a 1-3 equipos. Tiene 3 partes:
> **(1) Show & Tell**, **(2) Teamwork-Task** (los jueces ponen una tarea de programación en el
> momento) y **(3) Questions** (banco de preguntas por categoría). Se juzga con 3 criterios, cada
> uno en escala 0/1/3/5. **Apuntamos a Excellent (5) en los tres.**
>
> **Regla de oro de la entrevista (lo que mira el juez):** que **TODOS** contribuyan, que haya
> **fluidez técnica** real (no recitar) y **enfoques innovadores**. Nadie monopoliza, nadie queda mudo.
>
> **Nota de idioma:** la entrevista se da en **inglés** (lengua de la competencia). El marcador 🇬🇧
> señala las frases que conviene tener traducidas y ensayadas en voz alta antes de Incheon; la versión
> en inglés de esta hoja vive en `docs/competencia/en/ENTREVISTA-PREP.md`.

---

## Cómo se puntúa (mapa 1:1 a la rúbrica — el juez busca esto)

| Criterio de rúbrica | Qué es Excellent (5) | Dónde lo cubrimos en esta hoja |
|---|---|---|
| **Teamwork & Communication** | Colaboración fluida, los miembros se apoyan, **roles claros**, **TODOS contribuyen** | §2 (reparto de roles) + §1 (cada uno habla en el Show&Tell) + §5 (protocolo de pase de palabra) |
| **Technical Understanding** | **Fluidez técnica fuerte** y resolución de problemas | §4 (respuestas modelo por categoría, con números reales) |
| **Task Execution** | Completa **eficientemente** con **enfoques innovadores** | §3 (tips del Teamwork-Task: cómo dividirnos, dónde está el código, flash rápido) |

> **Placeholders a completar antes de Incheon** (registrados en gaps al final): `IITA Low Battery Messi`,
> roles definitivos de `María Virginia Viollaz`/`Elías Cordero`, y confirmar que en Incheon compiten 2 personas
> (el roster del repo: Gustavo Viollaz director, Enzo Juárez coach, María Virginia Viollaz y Elías Cordero
> competidores). En la entrevista **hablan los competidores presentes**; el coach no responde por ellos.

---

## §1 — Show & Tell (60-90 s) → apunta a Teamwork & Communication (Excellent)

> **Objetivo:** en 90 s el juez tiene que ver (a) un robot real, (b) UNA idea diferencial y (c) que **el
> equipo trabaja como equipo**. Estrategia anti-monólogo: **cada miembro dice una parte**, encadenada.
> Llevar el robot encendido (o un robot + laptop con la suite de tests verde de fondo).

**Guion repartido (cronometrado, 2 voces — adaptar si hay más competidores):**

| Tiempo | Quién | Línea (en español de trabajo — 🇬🇧 traducir) |
|---|---|---|
| 0-15 s | `María Virginia Viollaz` | "Somos `IITA Low Battery Messi`, de Salta, Argentina, en la sub-liga **Open**. Llegamos a Incheon como **campeones nacionales** de la Roboliga Argentina 2025. Traemos **2 robots**: un **arquero** y un **delantero**." |
| 15-40 s | `María Virginia Viollaz` | "Nuestro robot usa una **arquitectura distribuida de 3 placas**: una placa **percibe** (2 cámaras OpenMV N6, 1 IMU, 4 sensores ToF), una placa **decide** (FSM táctica + 3 motores omni) y una placa **toca el piso** (anillo de 32 sensores de línea + 2 sensores ópticos de odometría). Se hablan por UART a 230400 baud." |
| 40-65 s | `Elías Cordero` | "Lo que más nos enorgullece es **cómo verificamos el firmware sin la placa**: la lógica de decisión vive en módulos C++ puros que compilamos y testeamos en la PC con g++. Hoy corremos **624 tests host-native en 44 suites, 0 fallos** (verificado 2026-06-04 con `scripts/run-host-tests.sh`). Eso nos deja **iterar rápido y seguro** a días de la competencia." |
| 65-85 s | `Elías Cordero` | "Y una decisión táctica de la que estamos orgullosos: el **arquero anticipa**. En vez de seguir la posición actual de la pelota, proyecta dónde **va a estar** usando su velocidad (`pos + v·0.2 s`, con tope). Se lo mostramos en cancha si quieren." |
| 85-90 s | ambos | "Todo está **open-source con licencia MIT** en GitHub, documentado para que otro equipo lo replique. ¿Por dónde quieren empezar?" |

**Tips de entrega (suben Teamwork & Communication a Excellent):**
- **Pase de palabra explícito**: "...y eso lo trabajó sobre todo `Elías Cordero`, contale vos" → muestra roles claros y que se apoyan.
- Mantener contacto visual con **los jueces Y los otros equipos** (la rúbrica premia compromiso con todos los presentes).
- Tener el robot **físico en la mano** y señalar las 3 placas mientras se nombran (las placas apiladas como pisos: TOP / CENTRAL / DOWN).
- Cerrar con **pregunta abierta** → invita a la conversación en vez de cortarla.

---

## §2 — Reparto de roles técnicos → apunta a Teamwork & Communication (Excellent: "roles claros, todos contribuyen")

> **Idea:** cada competidor **domina un área** y es el "dueño" de esas preguntas. Cuando llega una pregunta,
> el dueño contesta y el otro **suma un dato**, nunca se pisan. Abajo una propuesta de reparto basada en el
> historial del repo (María/Virginia con experiencia en **visión y trayectorias**; Elías en **robótica e
> ingeniería electromecánica**). **Confirmar/ajustar nombres antes de Incheon.**

| Área | Dueño/a sugerido | Por qué (evidencia del repo) | Pregunta tipo que contesta sin dudar |
|---|---|---|---|
| **Visión + Estrategia/Trayectorias** | `María Virginia Viollaz` (visión + trayectorias) | Experiencia 2025 en visión artificial y trayectorias | "¿Cómo detectan la pelota?" / "¿Cómo decide el arquero a dónde ir?" |
| **Electrónica + Mecánica/Tracción** | `Elías Cordero` (robótica + Ing. electromecánica) | Estudiante de Ing. Electromecánica; banco de motores | "¿Por qué motores omni a 120°?" / "¿Cómo eligieron los componentes?" |
| **Software / Arquitectura / Testing** | **compartido** (los dos) | Es el diferencial del equipo; ambos deben poder explicar la idea de "módulos puros + tests host" | "¿Cómo testean sin la placa?" / "¿Cómo se comunican las 3 placas?" |
| **Development & Documentation** | **compartido** | Journal de ingeniería + FUENTES-DE-VERDAD + tests trazables | "¿Cómo trackean el progreso?" / "¿Cómo saben que algo anda?" |

**Regla de equipo para la entrevista (decir esto si un juez pregunta "¿quién hizo qué?"):**
> "Trabajamos por áreas pero con **un repositorio compartido**: cada cambio va con su entrada en el
> **journal de ingeniería** y, si toca un dato crítico, se actualiza la tabla de **FUENTES-DE-VERDAD**
> en el mismo commit. Así ninguno depende de la memoria del otro."

---

## §3 — Teamwork-Task en vivo (los jueces ponen la tarea) → apunta a Task Execution (Excellent: "eficiente + innovador")

> El juez da una tarea de programación en el momento (ej.: "que el robot gire hasta ver la pelota y se
> acerque", "que pare al cruzar la línea", "que patrulle de lado a lado"). **No improvisar la organización:
> seguir este protocolo.** Lo que da Excellent es que se vea un **método** (no caos) y un **enfoque
> propio** (reusar nuestros módulos puros + flashear rápido).

### 3.1 — Cómo nos dividimos (decirlo en voz alta para que el juez lo escuche)
1. **30 s de plan compartido**: uno repite la tarea con sus palabras y propone el enfoque ("esto es básicamente la FSM en estado APPROACH / es un strafe lateral / es el frenado de borde"). El otro confirma o ajusta.
2. **Roles para la tarea**: uno **escribe el código**, el otro **prepara el flasheo y mira el robot** (ojos en el hardware, no en la pantalla). Se turnan si la tarea tiene 2 partes.
3. **Hablar mientras se hace**: narrar lo que se toca ("voy a modificar `strategy.cpp` estado APPROACH, subo la velocidad de acercamiento"). El juez puntúa lo que entiende.

### 3.2 — Dónde está el código (saber esto de memoria = velocidad = Task Execution)

| Qué quieren que haga el robot | Archivo a tocar | Pista concreta |
|---|---|---|
| Decidir qué hace (perseguir, patrullar, interceptar) | `software/teensy/Soccer 2026/src/central/strategy.cpp` | FSM dual: ATTACKER (KICKOFF/SEARCH/POSITION/APPROACH/LINE_AVOID) y GOALKEEPER (PATROL/INTERCEPT/CLEAR/LINE_AVOID) |
| Mover el robot en una dirección (vx, vy, ω) | `software/teensy/Soccer 2026/src/shared/kinematics.{h,cpp}` | cinemática inversa omni-3: `v_i = -vx·sin(θ_i) + vy·cos(θ_i) + ω·R`. **+X=derecha, +Y=frente, ω CCW+** |
| Aplicar PWM a los motores | `software/teensy/Soccer 2026/src/central/motors_zircon.{h,cpp}` | PWM 8-bit 0-255. `MOTOR_INVERT={+1,-1,+1}` (M2/U17 va invertido por HW) |
| Ajustar un lazo de control | `software/teensy/Soccer 2026/src/shared/pids.{h,cpp}` | heading + lateral + distancia. **OJO: clamp del HeadingPID ≤327** (ω·100 es int16, 360 desborda) |
| Anticipar la pelota (arquero) | `software/teensy/Soccer 2026/src/shared/ball_predict.{h,cpp}` | `lookahead_s=0.2`, `max_lead_mm=400` (tuneables) |
| Constantes del robot (velocidad, geometría) | `software/teensy/Soccer 2026/src/central/config_central.h` | `MAX_SPEED_MM_S=1000`, `WHEEL_ANGLES_DEG={60,-60,180}` (⚠️ tentativos) |

### 3.3 — Cómo cargamos firmware rápido (tener esto preparado ANTES)
- **Build/flash embebido**: `pio run -e central_robot1 -t upload` (o `top_robot1` / `down`). El entorno compila **100% offline** (libs vendoreadas en `lib/`), así que **no dependemos de internet del venue**.
- **Verificación instantánea sin placa**: `bash scripts/run-host-tests.sh` → corre los 624 tests host en segundos (624 tests / 44 suites / 0 failures, verificado 2026-06-04). **Mostrar esto al juez es un golazo**: "antes de subir al robot, lo validamos en la PC". **Aclaración honesta:** el runner host compila los **módulos puros** (shared + down); los tests de central/top usan Arduino y se compilan **on-target** en la placa.
- **Diagnóstico de banco**: hay ~40 sketches en `src/diag/` (`diag_central_motors`, `diag_central_strafe`, `diag_central_rx_all`...) que reusan los parsers de producción. Si la tarea es "mové un motor", `diag_central_motors` ya lo hace.
- **Truco de bring-up que evita perder tiempo (decirlo si algo no responde):** los sensores I²C (ToF y OTOS) **persisten su dirección con 3.3 V** → si no aparecen, hacer **power-cycle real** (cortar batería + USB ~10 s), no solo reset. Saberlo ahorra 20 min de debug en vivo.

### 3.4 — Enfoque innovador para mostrar (lo que sube Task Execution a Excellent)
- "Vamos a resolverlo **reusando un módulo puro que ya está testeado** en vez de escribir lógica nueva a ciegas" → eficiencia + criterio de ingeniería.
- "Le ponemos **fallback**: si el dato nuevo no está, hace exactamente lo de antes" → es nuestra técnica de *fallback byte-idéntico*, cero regresión. Muy vendible.

---

## §4 — Questions: respuestas modelo por categoría → apunta a Technical Understanding (Excellent: "fluidez técnica fuerte + resolución de problemas")

> Cada respuesta: **dato concreto + por qué + (si aplica) una iteración real**. El juez premia que demos
> **números** y que contemos **una decisión basada en datos**, no generalidades. 🇬🇧 = ensayar en inglés.
> Las respuestas marcadas con 💡 incluyen una historia de iteración (testeo→dato→cambio) — esas son las que
> más puntúan.

### General (decisiones de diseño, inspiración)

**P: ¿Por qué eligieron una arquitectura de 3 placas en vez de una sola?**
> "Por un principio: **procesar donde está el sensor y decidir en el centro**. La placa de visión procesa
> las cámaras y la IMU, la central solo recibe un resumen del mundo (un *WorldSnapshot* de **31 bytes a
> 100 Hz**) y decide. Cada microcontrolador queda **debajo del 30% de CPU**, así nos queda margen para
> mejoras. Además es **modular**: si en 2027 cambiamos la cámara, solo tocamos el firmware de esa placa."

**P: ¿Qué los inspiró / de dónde sacaron la arquitectura?**
> "Es el patrón estándar de robótica móvil (lo usan equipos de Middle Size League como CAMBADA). Y partimos
> de **lo que ya nos funcionó**: la placa central es el **Zircon** con la que ganamos el Nacional 2025; las
> placas de percepción y de piso las sumamos alrededor sin reemplazarla."

### Electrical (selección de componentes, troubleshooting)

**P: ¿Cómo eligieron los componentes?**
> "Criterio COTS y barato, comprable en LCSC: **Teensy 4.0/4.1** (Cortex-M7 a 600 MHz) como cerebros,
> **VL53L7CX** ToF multizona para distancia, **BNO055** para heading, **OTOS de SparkFun** para odometría
> óptica, y cámaras **OpenMV N6**. Alimentamos con **LiPo 2S (7.4 V)** → protección con diodos Schottky →
> 2 reguladores buck **MP1584** por placa (5 V lógica, 3.3 V sensores). Caracterizamos la deriva del OTOS en
> banco — ver el gráfico de error en `docs/competencia/assets/fig9_otos_error.png`."

**P (troubleshooting): Contame un problema eléctrico que hayan tenido y cómo lo resolvieron.** 💡
> "Los **4 sensores ToF** arrancan todos en la misma dirección I²C (0x29) y chocan en un bus compartido.
> Investigando el esquemático descubrimos que los pines para enumerarlos **no estaban ruteados** en el PCB.
> Enzo hizo un **bodge**: cableó la pata de control de cada ToF a un GPIO del Teensy. Y aprendimos algo clave:
> sus direcciones **persisten mientras tengan 3.3 V**, así que hay que **power-ciclar**, no resetear. Después
> de eso, los 4 ToF enumeran a 0x2A-0x2D y se desbloqueó la **localización 2D por trilateración**."

**P: ¿Por qué un motor va invertido en el código?**
> "El driver del motor 2 (U17) tiene las entradas cruzadas por hardware en el shield Zircon. En vez de
> recablear, lo corregimos en **un solo lugar del firmware**: `MOTOR_INVERT={+1,-1,+1}`. Lo **validamos en
> banco** girando cada motor por separado con `diag_central_motors`."

### Mechanical (features, materiales, manufactura)

**P: ¿Cómo se mueve el robot?**
> "Base **omnidireccional KIWI**: 3 ruedas omni a **120°** con 3 motores DC. Eso nos da movimiento
> holonómico (puede ir en cualquier dirección sin girar). La cinemática inversa está en un módulo puro
> testeado con **11 tests**, e incluye **saturación proporcional**: si una rueda satura, escalamos las 3
> por igual para no deformar la trayectoria."

**P: ¿Tienen pateador (kicker)?**
> "No. El robot **empuja la pelota por inercia** cuando el delantero se alinea con el arco rival
> (tolerancia **12°**, a menos de **80 mm**). Es una decisión de diseño: **menos componentes, menos
> energía, menos puntos de falla**. La lógica está en el módulo `behind_ball` con 16 tests."

**P: ¿De qué está hecho el chasis?**
> "El **plato base estructural es directamente la PCB de abajo** (≈175 × 166 mm, contorno redondeado tipo
> plato, montaje M3). Las 3 placas se apilan como pisos. `[GAP: confirmar materiales del resto del chasis,
> altura entre pisos y piezas impresas 2026 — registrado en gaps]`."

**P (manufactura): ¿Cómo lo fabricaron?**
> "Las **3 PCBs custom** las diseñamos en EasyEDA y son fabricables tal cual (tenemos gerbers + BOM). La
> central es comercial (Zircon de Robomov). Las piezas mecánicas son **impresión 3D**. `[GAP: subir los
> STL del chasis 2026 — los del repo son del 2025 con dribbler/solenoide ya descartados]`."

### Strategy (posicionamiento, tácticas)

**P: ¿Qué hace el arquero?**
> "Patrulla el arco y, cuando la pelota se acerca, **intercepta anticipando**: en vez de ir a la X actual
> de la pelota, va a la X **predicha** = posición + velocidad × 0.2 s, con un tope de 400 mm. La velocidad
> de la pelota la calculamos en la placa de visión por diferencias finitas con un filtro EMA. Si la pelota
> está quieta, el adelanto es 0 y se comporta como un arquero normal — **fallback automático**."

**P: ¿Y el delantero?**
> "Busca la pelota, se **posiciona detrás** de ella alineado al arco rival, y **empuja**. La FSM tiene los
> estados SEARCH → POSITION → APPROACH → empuje, con un estado `LINE_AVOID` que **bypasea todo** si está por
> salir de la cancha."

**P: ¿Cómo evitan salirse de la cancha?**
> "Anillo de **32 sensores de línea** en la placa de piso. Cuando detecta salida inminente, hay un **bus
> directo de emergencia** de esa placa a la central (1 salto UART) para frenar en **<15 ms** — a 1 m/s eso
> es 15 mm. Si frenáramos pasando por la placa de visión (2 UARTs), ya habríamos cruzado."

### Software (sensores, evasión de problemas, comunicación robot-robot, debugging)

**P: ¿Cómo procesan los sensores / arman la visión del mundo?**
> "La placa de visión **fusiona** 2 cámaras + IMU + 4 ToF y arma un *WorldSnapshot* de 31 bytes: pose propia,
> pelota (posición **y velocidad**), arcos, obstáculo más cercano y el comando del árbitro. Lo manda a la
> central a 100 Hz. La central solo consume ese resumen — no toca sensores crudos."

**P: ¿Cómo se comunican las placas? ¿Y qué pasa si se pierde un mensaje?**
> "UART con un **protocolo propio**: `[START 0xAA | LEN | TYPE | SEQ | PAYLOAD | CRC-16 | END 0x55]`. El
> **CRC** detecta corrupción y el **SEQ** detecta paquetes perdidos. El decodificador es una **máquina de
> estados byte por byte que se resincroniza sola**: un byte de basura no contamina el frame siguiente. Y hay
> **watchdogs**: si un stream no llega en 500 ms, la central pasa a modo seguro."

**P: ¿Cómo debuggean? Contame un bug de software real.** 💡
> "Tenemos ~40 sketches de diagnóstico que reusan los parsers de producción. Un bug real: la central quedaba
> **ciega a la línea** porque decodificaba el formato viejo (5 bytes) y **descartaba** los frames nuevos (16
> bytes). Era invisible en telemetría — se veía igual que un cable suelto. Lo cazamos con un **harness en
> g++ offline** de la cadena real codificar→decodificar→interpretar, migramos al formato nuevo y lo cubrimos
> con tests. **Regla del equipo**: ante una falla, primero grepear el journal por si ya nos pasó."

**P: ¿Coordinación entre los 2 robots?**
> "Por **ESP-NOW** vía la placa de comunicación (ESP32-C6). El compañero comparte pose y si ve la pelota, y
> eso entra al WorldSnapshot. `[GAP: confirmar estado de la coordinación partner en banco]`."

### Development & Documentation (inspiración, tracking, testing)

**P: ¿Cómo trackean el progreso?**
> "Con tres índices vivos: **ESTADO-ACTUAL** (estado del robot en 1 página, lectura obligatoria),
> **FUENTES-DE-VERDAD** (un doc canónico por tema; quien crea o supera un doc actualiza la tabla en el
> **mismo commit**) y un **journal de ingeniería** cronológico. Más **packs autocontenidos** por subsistema
> para onboarding."

**P: ¿Cómo testean? (la estrella) 💡**
> "Separamos la **lógica de decisión en módulos C++ puros** —sin Arduino— y los testeamos en la PC con g++.
> Hoy: **624 tests / 44 suites / 0 failures** (verificado 2026-06-04 con `scripts/run-host-tests.sh`), que
> crecieron de forma trazable (246 → 262 → 324 → 354 → 545 → 624). Ver el gráfico de crecimiento en
> `docs/competencia/assets/fig8_test_growth.png`. **Aclaración honesta:** el runner host compila los
> **módulos puros** (shared + down); los tests de central/top usan Arduino y se compilan **on-target**.
> Nació de un problema real: el **antivirus bloqueaba PlatformIO**, así que vendoreamos el framework de tests
> y escribimos un runner en g++ que **esquiva el antivirus y corre sin internet**. Eso nos deja verificar
> firmware embebido **sin tener la placa en la mano**."

**P: ¿Cómo se aseguran de que un cambio no rompe lo que andaba?**
> "Dos cosas: (1) **gate verde obligatorio** —los 624 tests pasan (624 / 44 / 0)— antes de cualquier merge; y (2)
> **fallback byte-idéntico**: cada feature nueva, si el dato no está disponible, produce **exactamente** el
> comando anterior. Lo verificamos con un test que compara la salida con y sin el dato. Así una feature
> 'duerme' hasta que el dato fluye y **nunca mete una regresión**."

**P: ¿Usan control de versiones? ¿Cómo trabajan en equipo?**
> "Git en un repo público compartido en GitHub (**IITA**, https://github.com/IITA-Proyectos/open-soccer-robocup-team2026),
> **MIT, todo open-source**. Desarrollamos en ramas y siempre
> `git fetch` + merge antes de pushear. Cada commit lleva **atribución humano/IA**. Auditamos el firmware con
> revisiones independientes (20 subsistemas, los hallazgos críticos pasan por un **segundo revisor escéptico**)."

---

## §5 — Protocolo de equipo durante la entrevista (lo que el juez observa, no lo que decimos)

> Esto es lo que separa un 3 de un 5 en **Teamwork & Communication**. Ensayar como coreografía.

| Situación | Qué hacer (para Excellent) | Qué NO hacer |
|---|---|---|
| Pregunta del área de mi compañero | "Eso lo trabajó `[X]`, contale" + el dueño responde + **yo sumo un dato** | Contestar yo por encima del dueño |
| No sé la respuesta | "Honestamente eso lo tengo a medias, lo validamos en banco; lo que sí sé es..." + redirigir a lo que sí domino | Inventar / quedarse callado |
| Mi compañero se traba | Apoyarlo: "y un dato que ayuda acá es..." | Dejarlo solo o corregirlo en público con brusquedad |
| El juez pregunta algo que no validamos en banco | **Ser honestos**: "está implementado y testeado en host, falta validación en cancha" | Decir que anda si no lo probamos |
| Hay otros equipos en la sala | Saludar, escuchar sus respuestas, ofrecer ayuda si preguntan | Ignorarlos (la rúbrica premia compromiso con todos) |

**Frase de cierre de equipo (memorizar, 🇬🇧):** "Todo lo que vieron está open-source y documentado para que
otro equipo lo replique — si quieren les pasamos el repo." (Refuerza Documentation & Community y deja buena impresión.)

---

## §6 — Honestidad calibrada (qué decir si preguntan por lo que falta)

> Decir la verdad **con marco de ingeniería** suma; mentir y que se note resta. Tener listas estas:

- **Visión sin recalibrar para Incheon:** "El código de visión está sólido y testeado; lo que falta es
  **calibración de banco** (LAB + homografía) para la iluminación del venue. Tenemos el kit listo para
  recalibrar en <5 min." (Es nuestro bloqueante real #1 — no esconderlo.)
- **Cinemática tentativa:** "Los ángulos y el radio de rueda están como **tentativos** en el código porque
  faltaba medir el robot armado; el módulo es puro y testeado, solo hay que cargar las constantes reales."
- **1 sola IMU sana:** "Corremos con **1 BNO055 sano + 4 ToF**; la segunda IMU falló y está documentado el
  riesgo. La pose igual computa."
- **Heading que se congela:** "Detectamos que la IMU y los ToF compiten en el bus I²C y el heading se
  congelaba; mitigamos bajando el bus a 100 kHz y leyendo la IMU a 20 Hz. El fix de fondo (IMU en bus
  aparte) está anotado."

---

## §7 — Checklist de preparación (hacer ANTES de viajar)

- [ ] Completar `IITA Low Battery Messi` y confirmar quiénes son los **competidores presentes** en Incheon.
- [ ] Cada competidor ensaya **su área** (§2) hasta responder de corrido, con números.
- [ ] **Traducir al inglés** §1 (Show&Tell) y las respuestas 💡 de §4, y practicarlas en voz alta. 🇬🇧
- [ ] Ensayar el **Show&Tell cronometrado** (90 s) 3 veces con el robot en mano.
- [ ] Ensayar el **protocolo de pase de palabra** (§5) en un simulacro de 5 preguntas.
- [ ] Laptop lista: repo clonado, `pio` funcionando offline, `scripts/run-host-tests.sh` probado el día
      anterior, robots cargados con `_robot1`/`_robot2`.
- [ ] Tener a mano el **mapa de "dónde está el código"** (§3.2) impreso o en pantalla para el Teamwork-Task.
- [ ] Llevar el **power-cycle** internalizado (§3.3) para no perder tiempo si un sensor no aparece.

---

## Gaps (datos reales faltantes — completar antes de Incheon)

- `IITA Low Battery Messi` oficial registrado en RoboCup Junior para Incheon (confirmado 2026-06-05; "IITA - Open Soccer RoboCup Team 2026" es el descriptor interno del repo).
- ✅ RESUELTO 2026-06-05: compiten **María Virginia Viollaz** (visión/estrategia) y **Elías Cordero** (electro-mecánica), ambos 18. Viajan también **Enzo Velázquez (coach)** y **Cecilia Budeguer (mentora)**; **Gustavo Viollaz (mentor)** no viaja.
- ✅ RESUELTO 2026-06-05: Salta, Argentina · campeones de la final nacional de la Roboliga Argentina 2025 (UAI).
- **Materiales y dimensiones del chasis 2026** (altura entre pisos/standoffs, piezas impresas, diámetro y peso del robot) — no documentados; afecta respuestas de la categoría Mechanical.
- **STL/CAD del chasis 2026** para poder decir "es replicable" con confianza (los del repo son del 2025 con dribbler/solenoide ya descartados).
- **Estado de la coordinación partner (ESP-NOW) en banco** — para responder con seguridad la pregunta de "robot-robot".
- **Número de tests vigente** al momento de viajar: verificado **624 tests / 44 suites / 0 failures (2026-06-04 vía `scripts/run-host-tests.sh`)**. Correr el runner el día anterior y usar la cifra real del día.
- **Nombre legal de IITA** (✅ resuelto 2026-06-05): **IITA = Instituto de Innovación y Tecnología Aplicada** / Fundación Innovar. Unificado en todos los docs.
- **Figuras de datos** (`docs/competencia/assets/fig8_test_growth.png`, `fig9_otos_error.png`): generarlas con `gen_figuras.py` antes de viajar — el script existe pero los PNG aún no están generados en `assets/`.
- **Traducción al inglés** de todo el material de entrevista (requisito de idioma de la competencia): la versión EN vive en `docs/competencia/en/ENTREVISTA-PREP.md`; falta **ensayarla en voz alta**.
