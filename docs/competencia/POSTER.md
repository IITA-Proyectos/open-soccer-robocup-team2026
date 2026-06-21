---
title: "POSTER técnico — RoboCupJunior Soccer Open 2026 (versión de trabajo ES)"
date: 2026-06-04
status: borrador-juzgado
idioma: español-rioplatense (TRADUCIR a inglés antes de entregar)
formato-objetivo: A1 apaisado = ISO A1 84.1 cm ancho × 59.4 cm alto (NO superar — ver nota de tamaño abajo)
rubrica: RoboCupJunior Soccer 2026 — Poster Design & Presentation (5 pts, 6 criterios × 0/1/3/5)
---

> # ⚠️ VERSIÓN DE TRABAJO EN ESPAÑOL
> **El POSTER FINAL debe entregarse en INGLÉS (requisito de rúbrica RoboCupJunior).
> Este documento es la maqueta de trabajo en español para que el equipo lo lea y mejore.
> TRADUCIR TODO al inglés antes de imprimir/enviar.**
>
> *Working draft in Spanish. The FINAL poster must be submitted in ENGLISH (RCJ rubric requirement). Translate before printing.*
>
> ⚠️ **TAMAÑO — leer antes de maquetar (requisito duro).** La rúbrica RCJ Soccer 2026 exige que el póster **NO supere A1** (`.claude/skills/rcj-deliverables-judge/references/rubrica-oficial-2026.md`). Maquetar TODO en **A1 apaisado = 84.1 cm ancho × 59.4 cm alto** (la grilla ASCII de abajo ya está dibujada apaisada a esa proporción: ancho 84.1 > alto 59.4). Re-flowear las 12 columnas a esa medida en la herramienta de diseño (Figma/Inkscape). Confirmar el límite exacto en las reglas 2026 antes de imprimir.

---

# CÓMO LEER ESTA MAQUETA (nota para el equipo, NO va impresa)

Este archivo describe el **layout físico** de un poster **A1 apaisado** (máx **84.1 cm de ancho × 59.4 cm de alto** — NO superar A1).
Cada sección de abajo es una **ZONA del poster** con: (a) su posición en la grilla, (b) el **TEXTO EXACTO** que va impreso, y (c) las **imágenes** con `[FOTO: ...]`.

Los **títulos de zona están redactados para que un JUEZ encuentre cada criterio de la rúbrica de un vistazo**:

| Criterio de rúbrica (Poster, 5 pts) | ¿Dónde lo encuentra el juez? | Nivel apuntado |
|---|---|---|
| **Abstract** | Zona B — "ABSTRACT" | Excellent |
| **Method / Robot Production / Design** | Zona C, D, E, F — "METHOD & DESIGN" | Excellent |
| **Data / Results / Discussion** | Zona G, H — "DATA, RESULTS & DISCUSSION" | Excellent |
| **Photos / Images** | Todas las zonas (14 figuras etiquetadas Fig.N + créditos en pie) | **Developing hoy → Proficient alcanzable** (6 de 14 figuras existen como archivo: los diagramas Fig.2/4/8/9 + Fig.13 zonas ToF + Fig.14 árbol de salud — todos regenerados/creados 2026-06-14; Fig.12 timeline es draft; faltan las 8 fotos `[FOTO:]`) |
| **Layout** | Grilla de 12 columnas, paleta fija, tipografías fijas (Zona Pie) | Excellent |
| **Presentation** (en vivo) | Guion de sesión — Zona I "PRESENTATION PLAN" | Excellent |

---

# GRILLA DEL POSTER (A1 apaisado, 84.1 cm ancho × 59.4 cm alto)

```
 84.1 cm de ANCHO  →
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ ZONA A · TÍTULO / IDENTIFICACIÓN  (banda superior full-width, ~8 cm alto)                   │  ▲
├───────────────┬──────────────────────────────────┬───────────────────────────────────────┤  │
│ ZONA B        │ ZONA C  ¿Por qué 3 placas?         │ ZONA G  DATA, RESULTS & DISCUSSION    │  │
│ ABSTRACT      │  Arquitectura + justificación      │  (1/2): tabla iteraciones            │  │
│ (col 1-3)     │  (col 4-8)                          │  testeo→dato→modificación (col 9-12) │  │ 59.4 cm
│               ├──────────────────────────────────┤                                       │  de
│ ZONA B2       │ ZONA D  ¿Qué sensa, con qué código?│                                       │  ALTO
│ TEAM JOURNEY  │  Sensores + lenguaje + software    ├───────────────────────────────────────┤  │
│ (recorrido)   │  (col 4-8)                          │ ZONA H  DATA (2/2): métodos de test   │  │
│ (col 1-3)     ├──────────────────────────────────┤  repetibles + gráficos (col 9-12)     │  │
│               │ ZONA E  ¿Cuánto cuesta, cuánto     │                                       │  │
│ ZONA B3       │  tardó?  BOM + costo + timeline    ├───────────────────────────────────────┤  │
│ OPEN SOURCE   │  (col 4-8)                          │ ZONA I  PRESENTATION PLAN + QR        │  │
│ (col 1-3)     │ ZONA F  FOTO grande del robot       │  (col 9-12)                           │  │
├───────────────┴──────────────────────────────────┴───────────────────────────────────────┤  │
│ ZONA PIE · Créditos de imágenes · Licencia · Tipografías/paleta · Repo  (banda full-width)  │  ▼
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

**Sistema de grilla:** 12 columnas, gutter 1.2 cm, margen 2 cm. Tres macro-columnas: **Izquierda (col 1-3)** = identidad + abstract + recorrido + open source; **Centro (col 4-8)** = Method & Design; **Derecha (col 9-12)** = Data/Results + Presentation.

---

# ZONA A — TÍTULO / IDENTIFICACIÓN
*(Banda superior full-width — cumple el elemento obligatorio "Title/Identification": nombre del equipo + región + sub-liga Open)*

**TEXTO EXACTO (impreso):**

> ## IITA Low Battery Messi
> ### Aprendimos a diseñar nuestro propio hardware usando IA — y lo validamos nosotros
> *(en inglés sugerido: "We learned to design our own hardware with AI — and validated it ourselves")*
> #### Subtítulo técnico: Empujar para ganar — un robot de fútbol de 3 placas, sin pateador, verificado por software
> *(en inglés sugerido: "Push to Score: a kicker-less, 3-board soccer robot verified in software")*
>
> **RoboCupJunior Soccer — Open League** · **Región:** Salta, Argentina · clasificaron en la final nacional de la Roboliga Argentina 2025 (organizada por la UAI)
> **Organización:** IITA (Instituto de Innovación y Tecnología Aplicada) / Fundación Innovar
> **Campeones nacionales Roboliga Argentina (dic-2025 (UAI)) → RoboCup 2026, Incheon (30 jun – 6 jul)**

`[FOTO: logo del equipo / IITA a la izquierda de la banda]`
`[FOTO: render o foto limpia del robot completo (vista 3/4) a la derecha de la banda, fondo neutro]`
`[FOTO: bandera/ícono pequeño de Argentina + "Salta" para la región]`

> **Nota de gap:** confirmar la **regional exacta** de clasificación antes de imprimir (✅ nombre del equipo: IITA Low Battery Messi).

---

# ZONA B — ABSTRACT
*(Columna izquierda, arriba — elemento obligatorio "Abstract". Apunta a **Excellent**: resume CADA componente crítico con lenguaje científico y con intención clara de compartir conocimiento accionable. NO repite el detalle de las otras zonas: lo sintetiza.)*

**TEXTO EXACTO (impreso):**

> ## ABSTRACT
>
> **Lo que más nos enorgullece: aprendimos a diseñar nuestro propio hardware usando la IA como herramienta — y lo validamos nosotros.** Somos estudiantes; no sabíamos diseñar placas electrónicas. Para el mundial rehicimos dos robots con un flujo asistido por IA que llamamos **VIBE** (Claude Code por MCP a EasyEDA) y aprendimos a diseñar desde cero las dos placas nuevas, **TOP** (percepción) y **DOWN** (piso/línea). La IA fue la herramienta; el equipo aprendió, decidió y validó — somos los responsables de cada cosa que subió al robot.
>
> **El robot es una arquitectura distribuida de 3 placas Teensy + 1 módulo de comunicación**, cada microcontrolador especialista: **TOP** (Teensy 4.0, placa nueva) percibe (2 cámaras OpenMV N6, IMU BNO055, 4 ToF VL53L7CX, ultrasonido, árbitro RCJ por GPIO) y publica un **WorldSnapshot de 31 bytes a 100 Hz de diseño** *(banco 2026-06-14: 66 Hz)*. **CENTRAL** (Teensy 4.1 sobre PCB Zircon **reusada — el cerebro campeón 2025**) decide (FSM táctica + cinemática omni-3 + PIDs) y mueve **3 ruedas omni a 120°**. **DOWN** (Teensy 4.0, placa nueva) es el plato estructural y el sensor de piso (anillo de 32 sensores de línea + 2 OTOS), y **difunde** su medición a ambas placas. El delantero **no tiene pateador**: empuja la pelota por inercia (menos componentes, energía y puntos de falla).
>
> **El método que aportamos a la comunidad** es la disciplina de **verificar el firmware en la PC sin la placa**: la lógica vive en módulos C++ puros, testeados con `g++` offline. Esa es la **garantía** de que el aprendizaje fue serio y no "copiar-pegar de la IA": **858 tests / 61 suites / 0 fallos** (medido 2026-06-14 con `scripts/run-host-tests.sh` usando el g++ de Webots).
>
> **Estado honesto (no sobrevendemos):** el bloqueante #1 que queda para Incheon es la **visión sin recalibrar** (color LAB + homografía bajo la luz de la sede). La trilateración 2D con ToF, el strafe del arquero y el drive-straight con OTOS están *code-complete* y verificados en host, pero pendientes de validación en banco — sus conductas "duermen" hasta que su dato fluye, sin regresión. Este poster documenta el **método** y el **diseño** con detalle para que **otro equipo junior lo replique**.

---

# ZONA B2 — TEAM JOURNEY (recorrido del equipo)
*(Columna izquierda, medio — refuerza el criterio **Layout/Presentation** con la "narrativa del recorrido" que premia la rúbrica; refuerza también el Abstract con contexto humano.)*

**TEXTO EXACTO (impreso):**

> ## NUESTRO RECORRIDO
> - **Dic-2025 — campeones del Nacional con un robot MUCHO más básico:** arriba **una sola cámara** (sin ToF ni ultrasonido), abajo **solo 3 sensores de luz**, todo sobre la placa Zircon… y **aún así ganamos la 1ª competencia nacional de RoboCupJunior Soccer de Argentina**.
> - **2026 — el salto:** mismo cerebro campeón (Zircon), mucha más percepción. Arriba: **2 cámaras + IMU + 4 ToF + ultrasonido**. Abajo: **anillo de 32 sensores + 2 OTOS**. Rediseño a **3 placas** sumando percepción (TOP) y piso (DOWN) — *continuidad, no descarte*.
> - **May–Jun 2026:** bring-up de las 3 placas físicas, ~30 sesiones de banco documentadas, suite de tests host creciendo **246 → 262 → 324 → 354 → 403 → 470 → 545 → 658 → 834 → 858 / 0 fallos** (cientos de tests host, 0 fallos — ver Fig.8).
> - **Jun–Jul 2026:** Incheon. Estrategia declarada del equipo: **invertir en aprendizaje**, jugar partidos honestos y capturar datos.
>
> ### 🤖 ⭐ NUESTRA FEATURE DE ORGULLO — aprendimos a diseñar hardware con IA (metodología "VIBE")
> **No sabíamos diseñar placas electrónicas.** Lo más importante de este proyecto no es un sensor: es que aprendimos a diseñarlas con la IA como herramienta. Cómo lo hicimos, concreto:
> - **VIBE PCB Design** — las **dos placas nuevas (TOP y DOWN) se diseñaron casi todo con IA** (Claude Code por MCP a EasyEDA). No fue "ya sabíamos": fue aprender CÓMO diseñar PCBs con IA.
> - **Ensayo-y-error honesto:** también **probamos comandar Flux con IA, pero NO funcionó.** Lo contamos porque parte de aprender es lo que no anduvo.
> - **VIBE 3D Design** — rediseño del soporte de motores en Fusion 360 vía IA (recién empezando) para meter los motores más adentro.
> - **VIBE Coding** — firmware C++ con la red de seguridad de **cientos de tests host, 0 fallos** (la garantía: validamos en la PC antes de confiar; no es copiar-pegar de la IA).
>
> **El objetivo del VIBE es comprimir el ciclo concepto→robot-andando.** Meta: recorrerlo entero (PCB, fabricación, 3D, impresión, montaje, firmware, docs, testeo) en **30 días** — creemos que es posible. *(Este año fue más lento por la importación de materiales en Argentina, no por el método.)* Compartimos la **metodología** como aporte a RoboCupJunior, para que otro equipo junior pueda hacer lo mismo.
>
> ### 🛣️ Próximo paso (roadmap declarado)
> **Comunicación robot-a-robot** (arquero ↔ delantero) por **ESP-NOW** vía la placa COMM (ESP32-C6, ya en el robot): compartir pose, si cada uno ve la pelota y su estado para **coordinar estrategia**. Falta integrarlo al WorldSnapshot y validarlo en banco — fiel a nuestra disciplina, la conducta cooperativa "duerme" hasta que el dato fluya, sin regresión. También planeamos pasar a **4 ruedas omni** con **motores más cortos, con encoders** (más estabilidad y control, y espacio liberado para el **kicker** y el **dribbler**, que quedan para el año próximo).
> **(FUTURO — año próximo)** Evolucionar la interfaz del diseño modular de **UART punto-a-punto** a un **bus CAN troncal** entre las 3 placas Teensy (robusto al ruido, un par de cables, escalable a N nodos) + una **4ª placa ESP32 gateway** que puentea el bus al exterior por inalámbrico: para el robot **compañero** y para **telemetría** — monitoreo en vivo estilo **Fórmula 1** (ver sensores en entrenamiento, registrar y analizar para mejorar el software).

`[FOTO ANTES/DESPUÉS — Fig.1a: robot campeón del Nacional 2025 — versión básica: 1 cámara, 3 sensores de luz · Fig.1b: robot 2026 — 2 cámaras + IMU + 4 ToF + ultrasonido arriba, anillo de 32 sensores + 2 OTOS abajo. Mismo cerebro Zircon.]`
`[FOTO: equipo IITA con el robot/trofeo en el Nacional 2025 (UAI) — etiquetar Fig.1]`

> **Roles del equipo:**
> | Rol | Integrante | Aporte técnico | ¿Viaja a Incheon? |
> |---|---|---|---|
> | **Director del proyecto** | Gustavo Viollaz | Coordinación, integración de las 3 placas, sesiones de banco | **No** (obligaciones laborales) |
> | **Coach principal** | Enzo Juárez Velázquez | Diseño de PCB con IA (VIBE PCB design, EasyEDA vía MCP), validación eléctrica, *bodges* de hardware | **Sí** |
> | **Coach secundaria** | Cecilia Budeguer | Acompañamiento en Incheon (respaldo del equipo; Enzo además dirige el equipo IITA de RCJ Rescue Line en Corea) | **Sí** |
> | **Competidora** | María Virginia Viollaz (18) | Visión artificial, trayectorias, parser de cámara; aprendió a diseñar con IA | **Sí** |
> | **Competidor** | Elías Cordero (Ing. Electromecánica, UNSa) | Motores, cinemática, mediciones; aprendió a diseñar con IA | **Sí** |
>
> *El **equipo aprendió, decidió y validó**; el diseño con IA fue **guiado por el coach principal Enzo**, con los estudiantes María y Elías aprendiendo, decidiendo y validando. La IA fue la herramienta — los responsables somos nosotros.*

---

# ZONA B3 — OPEN SOURCE & COMMUNITY
*(Columna izquierda, abajo — apoya el bonus de TDP y el componente "Documentation & Community". En el poster aporta a **Method/Design** "intención de compartir TODO el conocimiento accionable".)*

**TEXTO EXACTO (impreso):**

> ## OPEN SOURCE (TODO publicado, MIT)
> - **Software:** firmware completo de las 3 placas (C++17) + visión (MicroPython) + **cientos de tests host, 0 fallos** (cifra viva y método en Zona H) + scripts de banco.
> - **Hardware:** proyectos **EasyEDA** completos de TOP y DOWN —**las dos placas que aprendimos a diseñar con IA** (VIBE PCB design, Claude vía MCP)— con esquemático + PCB + Gerbers + BOM + Pick&Place; CENTRAL = Zircon Rev v15 (esquemático público, **cerebro campeón 2025 reusado**).
> - **Cómo, no solo qué:** documentos vivos **FUENTES-DE-VERDAD** (un doc canónico por tema), **MAPA-DE-DATOS** (cada mensaje: tipo/tamaño/pin/frecuencia/quién lo llena y consume) y **diario de ingeniería** con cada iteración.
>
> **Repo:** `https://github.com/IITA-Proyectos/open-soccer-robocup-team2026` `[QR al repo]`

---

# ZONA C — METHOD & DESIGN (1/3): ARQUITECTURA + JUSTIFICACIÓN
*(Centro, arriba — elemento obligatorio "Method/Robot Production/Design". Apunta a **Excellent**: producción completa, clara y concisa, CON justificación de cada decisión.)*

**TEXTO EXACTO (impreso):**

> ## MÉTODO Y DISEÑO — ¿Por qué 3 placas?
> *Las dos placas nuevas de esta arquitectura (**TOP** y **DOWN**) las **diseñamos aprendiendo con IA** (VIBE PCB design, Claude vía MCP a EasyEDA); **CENTRAL** es el Zircon campeón 2025 reusado. La arquitectura de abajo es la **evidencia de lo que diseñamos así y de cómo lo validamos**.*
>
> Cada placa procesa **donde está el sensor** y se decide en **el centro** (regla estándar de robótica móvil). Esto reduce el tráfico UART, apunta a dejar cada MCU **<30% de CPU** *(objetivo de diseño — no medido con osciloscopio, TASK-014)* y permite reemplazar una placa sin tocar las otras.
>
> **Diseño en 2 módulos, una interfaz de datos limpia:** el robot se parte en un **MÓDULO SUPERIOR = percepción + comunicación + fusión de sensores** (saber dónde está todo —pelota, arcos, obstáculos— y a qué velocidad se mueve, fusionado en el **WorldSnapshot**; a futuro, comunicarse con el robot **compañero** para compartir info) y un **MÓDULO INFERIOR = drive train + cerebro de decisión** (motores, drivers y lógica de juego; una placa auxiliar de piso —DOWN— manda su info de línea/odometría **ya pre-procesada** hacia arriba; a futuro, encoders). **Ventaja:** cada módulo se mejora y se testea por separado → **acelera tiempos** (ver "VIBE"). El inferior deja **lugar para KICKER + DRIBBLER** (este año no se hizo por falta de tiempo para montar motores más cortos → se hizo lo realizable: el delantero empuja por inercia).
>
> | Placa | MCU | Rol | Sensores / actuadores |
> |---|---|---|---|
> | **TOP** | Teensy 4.0 | "Veo el mundo" | 2 cámaras OpenMV N6 · 2 IMU BNO055 (ambos 0x28, buses separados: Wire2 24/25 + Wire 18/19) · 4 ToF VL53L7CX · 1 HC-SR04 · árbitro GPIO |
> | **CENTRAL** | Teensy 4.1 (Zircon) | "Decido" | FSM táctica · cinemática omni-3 · 3 PIDs · **3 motores** |
> | **DOWN** | Teensy 4.0 | "Toco el suelo" | 32 sensores de línea (4 mux CD4051) · 2 OTOS |
> | **COMM** | ESP32-C6 | árbitro RCJ | nivel GPIO 3.3 V hacia TOP |
>
> **Decisiones justificadas (data-driven):**
> - **Sin pateador:** el delantero empuja por inercia → menos masa, menos energía, menos fallas.
> - **Bus de emergencia DOWN→CENTRAL** (1 salto UART): a 1 m/s el robot recorre **1 mm/ms**; pasar la alarma de borde por 2 UART en serie agrega ~25 mm de *overshoot* → se cablea un atajo directo para frenar en **<15 ms** *(objetivo de diseño — no medido con osciloscopio, TASK-014)*.
> - **Localización con 4 ToF (no LiDAR ni EKF):** el array de ToF cuesta **~USD 80**, da **±2–3 cm** y se programó en **~1 día**; un EKF/MCL pedía **3–5 días** de desarrollo y un LiDAR ~USD 100 para una cancha de 4 paredes ortogonales → elegimos la solución barata, simple y suficiente (BOM §2).
> - **Todos los PIDs en CENTRAL:** un solo lugar con todas las ganancias.
> - **Continuidad:** CENTRAL es el Zircon que ganó el Nacional 2025; si una placa nueva falla, degrada a modo monolítico.

`[FIGURA Fig.2 — Flujo de datos entre las 3 placas · diagrama de bloques renderizado: cajas TOP/CENTRAL/DOWN con flechas etiquetadas "WorldSnapshot 31 B @100 Hz de diseño", "LineStatusV2 16 B @200 Hz", "bus emergencia" · archivo docs/competencia/assets/fig2_dataflow.png (gen_diagramas.py) · Original diagram by the team — CC BY 4.0]`

---

# ZONA D — METHOD & DESIGN (2/3): SENSORES, LENGUAJE Y SOFTWARE
*(Centro, medio — cubre los obligatorios "lenguaje de programación" y "sensores usados", con insight de software.)*

**TEXTO EXACTO (impreso):**

> ## ¿Qué sensa y con qué código?
> *El firmware lo escribimos con **VIBE Coding** (C++ asistido por IA) sobre una red de seguridad de **cientos de tests host, 0 fallos** — la garantía de que cada cosa que aprendimos a diseñar fue validada por nosotros antes de subirla al robot (cifra viva y método en Zona H).*
>
> **Lenguajes:** **C++17** (firmware, `namespace iitasoccer`, structs *packed* con `static_assert` de tamaño) + **MicroPython** (visión OpenMV N6, detección por color en espacio LAB).
> **Build:** PlatformIO (**más de 80 entornos `[env:]`**: producción + el resto, de diagnóstico/banco/test) + un runner `g++` offline para los tests.
>
> **Sensores y para qué:**
> | Sensor | Cantidad | Función |
> |---|---|---|
> | Cámara OpenMV N6 (STM32N6 + NPU) | 2 | Pelota + arcos (QVGA, ~30 Hz) |
> | IMU BNO055 | 2 (ambos 0x28, en buses I²C separados) | Heading (yaw) |
> | ToF VL53L7CX (lee **4×4 = 16 zonas** crudas) | 4 | Distancia a paredes → **localización 2D** |
> | OTOS (odometría óptica) | 2 | Pose/velocidad por *slip* del piso |
> | Anillo de línea (fototransistor) | 32 | Borde de cancha (frenado) |
> | HC-SR04 ultrasonido | 1 | Obstáculo frontal (redundante c/ToF) |
>
> **Innovación de sensado — zonas crudas del ToF (2026-06-14).** Antes el firmware promediaba las 16 zonas de cada ToF a UNA distancia y descartaba el resto. Ahora la telemetría **expone la grilla cruda 4×4 de cada sensor** (campo `z`, aditivo): vemos QUÉ parte del muro tiene cerca, no solo "qué tan cerca". *(El enmascarado/rotación de zonas — ignorar las que no apuntan al campo — es roadmap; hoy las zonas son de solo lectura.)*
>
> **Insight de software (estructura del código):**
> - **WorldSnapshot v3 = 31 B** (`static_assert(sizeof==31)`), evolución de contrato v1(24 B)→v2(27 B)→v3(31 B). Lo publica el TOP a **100 Hz de diseño** *(banco 2026-06-14: 66 Hz)*.
> - **Protocolo UART robusto:** trama `[0xAA | LEN | TYPE | SEQ | PAYLOAD | CRC16-CCITT | 0x55]`; el decodificador es una máquina de estados byte-a-byte que **resincroniza sola** (un byte basura no contamina el frame siguiente).
> - **Cinemática omni-3 pura:** `v_i = -vx·sin(θ_i) + vy·cos(θ_i) + ω·R`, con saturación **proporcional** (escala las 3 ruedas para preservar la trayectoria).
> - **Arquero que anticipa:** apunta a la **X predicha** = `pos + v·lookahead` (no a la X actual).

`[FIGURA Fig.3 — La red de seguridad: 858 tests host en verde · captura de la terminal run-host-tests.sh · Foto original del equipo, CC BY 4.0]`
`[FIGURA Fig.4 — Cómo decide el robot: la FSM táctica dual · flowchart — ATTACKER: WAIT_START→KICKOFF→SEARCH→POSITION→APPROACH (+LINE_AVOID); GOALKEEPER: WAIT_START→GOTO_LINE→PATROL→INTERCEPT→CLEAR (+LINE_AVOID); EMERGENCY_LINE bypassa la FSM. (El empuje al arco NO es un estado: ocurre dentro de APPROACH.) Fuente: diagrama Mermaid de docs/competencia/assets/diagramas.md (verificado vs strategy.cpp + strategy_transitions.h); detalle en docs/firmware/ESTRATEGIA-ALTO-NIVEL.md · archivo docs/competencia/assets/fig4_fsm.png (gen_diagramas.py — ✓ regenerada 2026-06-14 con GOTO_LINE) · Original diagram by the team — CC BY 4.0]`
`[FIGURA Fig.13 — ¿Qué ve realmente un ToF? Las 16 zonas crudas 4×4 de un sensor (valores ILUSTRATIVOS) en orientación canónica · innovación de sensado 2026-06-14 · archivo docs/competencia/assets/drafts/fig_zonas_tof_4x4.png · Original diagram by the team — CC BY 4.0]`

---

# ZONA E — METHOD & DESIGN (3/3): BOM, COSTO Y TIEMPO DE DESARROLLO
*(Centro, abajo — elementos obligatorios "tiempo y costo de desarrollo" + "BOM de componentes mayores".)*

**TEXTO EXACTO (impreso):**

> ## ¿Cuánto cuesta y cuánto tardó? (BOM · costo · tiempo)
>
> La **BOM completa de componentes mayores** (con **part numbers**, **fuente/proveedor**, columna **Nuevo vs. reusado**, **Kit/Custom** y costos unitarios reales LCSC) es la **fuente única** `docs/competencia/BOM.md` — este poster la **referencia**, no la duplica. Extracto de cabecera:
>
> | Componente mayor | Part number / modelo | Cant. | Nuevo / Reusado | Costo unit. (USD) |
> |---|---|---|---|---|
> | Cámara OpenMV N6 | OpenMV Cam N6 (STM32N6 + NPU) | 2 | Nuevo | **USD 165 c/u** (ref. int.; el más caro) |
> | MCU TOP / DOWN | Teensy 4.0 (LCSC `C99001332551`) | 2 | Nuevo | **USD 23.80 c/u** (ref. int.) |
> | MCU CENTRAL | Teensy 4.1 (en PCB Zircon) | 1 | **Reusado** (campeón 2025) | **USD 31.50** (ref. int.) |
> | IMU BNO055 | Bosch BNO055 (U10/U11) | 2 (ambos 0x28, buses separados) | Nuevo | **~USD 35 c/u** (ref. int.) |
> | ToF VL53L7CX | ST VL53L7CX (módulo Pololu) | 4 | Nuevo | **USD 19.95 c/u** (ref. int.) |
> | OTOS SparkFun | SparkFun OTOS (U5/U6) | 2 | Nuevo | **USD 84.95 c/u** (ref. int.) |
> | PCB Zircon Rev v15 (CENTRAL) | Robomov Zircon Rev v15 (COTS) | 1 | **Reusado** | **USD 250** (ref. máx.; suelto pendiente — kit USD 529) |
> | Mux CD4051BM | TI CD4051BM (LCSC `C353976`) | 4 | Nuevo | 0.96 |
> | Regulador buck MP1584-EN | MP1584-EN (módulo SIP) | 6 | Nuevo | **USD 0.90 c/u** (ref. int.) |
> | Batería LiPo 2S 7.4 V | **LiPo 2S 6800 mAh** (≈50 Wh; C/marca a confirmar) | 1 | Nuevo | **USD 42.99** (ref. Gens Ace 50C) |
> | 3 motores DC + 3 ruedas omni | Motor "TT" + rueda omni KIWI | 3+3 | Nuevo | **USD 23.95 c/u motor** (ref. máx. Pololu HP; TT ~3) + ruedas 6.50 |
> | **TOTAL por robot** | — | — | — | **≈ USD 1.168 nuevo · ≈ USD 887 reusando CENTRAL** (ref. int., valor más alto/ítem; 2 robots ≈ USD 2.055–2.336) |
>
> **Tiempo de desarrollo — hitos fechados (trazables en `journal/`, BOM §3.2):**
> | Hito | Fecha |
> |---|---|
> | Kickoff del proyecto | 2026-02-21 |
> | Bring-up DOWN (32 sensores + 2 OTOS) | 2026-05-24 |
> | Bring-up TOP (4 ToF en bus único, BNO) | 2026-05-25 → 05-31 |
> | Árbitro homologado (mueve el robot por 1ª vez) | 2026-06-02/03 |
> | Demo completa del robot | 2026-06-11 |
> | TOP→CENTRAL validado en banco (66 Hz, 0 CRC) | 2026-06-14 |
>
> Esfuerzo total ≈ **4 meses** (feb–jun 2026); el rediseño intensivo (mayo–junio) fue de ≈ **8 semanas** sobre la base del robot campeón 2025. *Las placas TOP y DOWN se diseñaron aprendiendo con IA (VIBE PCB design); el soporte de motores se rediseñó en 3D (VIBE 3D, Fusion vía IA — recién empezando).* Suite de tests host creciendo **246 → 262 → 324 → 354 → 403 → 470 → 545 → 658 → 834 → 858** (cientos de tests host, 0 fallos — ver Fig.8 y Zona H).
>
> **Precios reales disponibles en el repo (unitarios LCSC, citados verbatim en BOM.md):** fototransistor ALS-PT19 ≈ 0.116 · CD4051BM 0.96 · LED 0402 0.016 · diodo B5819W 0.024 USD.
>
> `[FIGURA Fig.12 — Línea de tiempo del proceso constructivo (kickoff → TOP→CENTRAL) · draft en docs/competencia/assets/drafts/fig_proceso_constructivo_timeline.svg/.png — terminar y numerar al maquetar · Original diagram by the team — CC BY 4.0]`

> **Nota de gap (registrar):** costos = **precio internacional de referencia** (USD, verificado 2026-06-05; fuente única `BOM.md` §3). **Tipo de cambio (2026-06-13): 1480 ARS = 1 USD.** Como referencia MÍNIMA en pesos (USD × 1480): ≈ **ARS 1.728.640/robot nuevo** · ≈ **ARS 1.312.760/robot reusando CENTRAL**. ⚠️ Esto es un **piso**, no el costo real: el costo *landed* local es **MAYOR** por impuestos y restricciones de importación argentinas (la aduana obliga a fraccionar pedidos). **Pendiente del equipo (chico):** precio del **Zircon** suelto (Robomov publica el kit a USD 529), qué **motor** usan (TT ~USD 3 vs Pololu HP USD 23.95) y las **horas** de desarrollo.

---

# ZONA F — FOTO PRINCIPAL DEL ROBOT
*(Centro, base — ancla visual del poster. Cumple "Photos/Images: abundantes, etiquetadas y citadas".)*

`[FOTO Fig.5 — vista superior del robot 2026: foto + OVERLAY DE ETIQUETAS (figura-de-datos) con líneas guía a las 3 placas (TOP azul / CENTRAL naranja / DOWN verde) y a las **3 ruedas omni a 120°**. El color de cada etiqueta = el color de placa de la paleta del póster.]`
`[FOTO Fig.6 — vista lateral: la pila de placas (standoffs) + el montaje de motores.]`
`[FOTO Fig.7 — detalle del anillo de 32 sensores de línea en la cara inferior del plato DOWN.]`

> **Pie pre-escrito (citar al maquetar):**
> - *Fig.5 — El robot de un vistazo: 3 placas especialistas + 3 ruedas omni a 120°. Foto original del equipo con overlay de etiquetas, CC BY 4.0.*
> - *Fig.6 — La pila de placas y el drive train. Foto original del equipo, CC BY 4.0.*
> - *Fig.7 — El anillo de 32 sensores que detecta el borde de la cancha. Foto original del equipo, CC BY 4.0.*

---

# ZONA G — DATA, RESULTS & DISCUSSION (1/2): ITERACIONES TESTEO → MODIFICACIÓN
*(Columna derecha, mitad superior — elemento obligatorio "Data/Results/Discussion". Apunta a **Excellent**: datos de test significativos + **modificaciones MAYORES hechas como resultado del testing** + vínculo claro testeo→evaluación→modificación.)*

**TEXTO EXACTO (impreso):**

> ## DATOS, RESULTADOS Y DISCUSIÓN
> Cada fila es una iteración **real**: medimos → evaluamos → **modificamos la fuente en un solo punto** → re-verificamos. La columna **Madurez** dice qué tan lejos llegó: **VALIDADO EN BANCO** (en la placa real) · **VERIFICADO EN HOST** (tests `g++` / golden / `pio` compila) · **DIAGNOSTICADO** (causa hallada, fix pendiente de banco).

> | # | Problema observado | Dato medido | Modificación aplicada | Madurez |
> |---|---|---|---|---|
> | 1 | **Los 4 ToF chocan en I²C** (todos nacen en 0x29); el PCB rev 1.0 no ruteó XSHUT (8 *No-Connect*) | Forense del esquemático: 0 nets XSHUT. Tras *bodge* + **power-cycle**: 4 LP en pines {9,10,11,12} enumeran a **0x2A–0x2D** | Migrar los 4 ToF al bus único `Wire` (junto al BNO secundario, ambos BNO en 0x28); dejar `Wire2` (24/25) como bus dedicado del BNO primario, sin ToF; **desbloquea la localización 2D** | **VALIDADO EN BANCO** |
> | 2 | **Heading del BNO se congelaba** (yaw clavado) — era el BNO **secundario** en el bus `Wire` compartido con los ToF (contención I²C) | Solución de raíz: **dual-bus** — primario en `Wire2` (24/25) SOLO, sin ToF → no sufre contención; secundario queda de centinela | Primario robusto con los 4 ToF activos; heading validado en banco 2026-06-21 (sigue el giro). 100 kHz por el secundario | **RESUELTO 2026-06-21** |
> | 3 | **Odometría OTOS** reporta basura sobre hoja A4 | A4 con lámina: 28.6/300 mm (9.5%); A4 sin lámina: 0.3 mm; **cartón corrugado: 280.4/300 mm = 6.5% error** (< 8% tol.) | Quitar lámina + exigir piso texturado → **mejora 10×**; la cancha verde de RCJ ya lo cumple | **VALIDADO EN BANCO** |
> | 4 | **Árbitro no llegaba al Teensy** (el TOP esperaba un frame UART que la COMM nunca emite) | La COMM oficial entrega START/STOP como **nivel GPIO 3.3 V**; en PLAY sube **un solo** pin | Leer pines 5/6 con `INPUT_PULLDOWN`, `match_running = pin5 **OR** pin6`; fail-safe a STOP | **VALIDADO EN BANCO** — *movió el robot por 1ª vez* |
> | 5 | **Motor 2 (U17) giraba invertido** por HW (INA/INB cruzados; hasta la reparación de jun-2026) | Banco rueda-por-rueda (María/Elías) | `MOTOR_INVERT` en **un solo lugar** *(hoy ambos robots `{+1,+1,+1}` tras el recableado de jun-2026)* | **VALIDADO EN BANCO** |
> | 6 | En *strafe* lateral **solo gira el motor 1** | A `vx=150`/`MAX=1000`, M1 y M2 reciben ~13% PWM (33/255) → debajo del arranque; **M3 a 180° proyecta 0 (correcto)** | Diagnóstico: deadzone PWM + geometría kiwi (no es bug de pin) → piso de PWM por rueda `{70,70,107}` + impulso inicial | **VALIDADO EN BANCO** (R2; R1 a verificar) |
> | 7 | **Overflow int16 de omega** (CRÍTICO): clamp 360 → 36000 centideg > 32767 → giro **invertido a fondo** | Revisión interna adversarial; verificado contra el código | `HeadingPID.output_clamp` 360 → **327** (327·100 < 32767); test anti *sign-flip* | **VERIFICADO EN HOST** |
> | 8 | **CENTRAL ciega a la línea**: decodificaba el contrato viejo (5 B) y descartaba todos los frames de 16 B | `payload_len==5` rechazaba el **100%** de los frames reales | Migrar a `LineStatusV2` (16 B); harness g++ 8/8 PASS | **VERIFICADO EN HOST** |
> | 9 | **Pelota fantasma:** la fusión promediaba las 2 cámaras cuando ambas veían pelota → punto medio inexistente | La telemetría per-cámara (camf/camb) muestra el **delta front↔back** que delata el promedio | Telemetría TOP v2 expone cada cámara por separado → se decide CON DATO cuál apagar | **VERIFICADO EN HOST** (golden C++⇄Python byte-idéntico) |
> | 10 | Un sensor que miente (cámara fantasma) seguía contaminando la fusión tras reiniciar | — | **Config persistente en EEPROM**: apagar cámara/BNO/US/ToF + fijar ubicación de cada ToF, y que **sobreviva al power-cycle** | **VERIFICADO EN HOST** (test_top_config 12, pio SUCCESS) — pendiente banco |
> | 11 | El **botón físico de arranque** (pin 9 del Zircon) quedó CLAVADO en GO el 2026-06-12 (polaridad) | El pulsador onboard dio problemas en ambos robots | **Deshabilitado por default** en TODA la CENTRAL (opt-in que ningún env define); el arranque pasa a teclado serie + árbitro por GPIO | **VERIFICADO EN HOST** (envs de competencia byte-idénticos) — pendiente banco |
> | 12 | El delantero R1 (env de práctica) giraba tan rápido buscando que pasaba de largo la pelota | Banco de práctica con alumnos | Giro de búsqueda 60→30 °/s + evasión de línea apenas ve blanco (salida temporizada) | **VERIFICADO EN HOST** (env de práctica; duración a tunear en banco) |
> | 13 | El arquero **strafe con retroceso-al-arco + avance** (FSM nueva GOTO_BACK→ADVANCE→strafe) | Banco 2026-06-14: la **secuencia FSM corrió**, PERO el rumbo oscila ±37° (a veces queda parado) | Reafirma por qué NO usamos PID continuo de rumbo (descontrola) sino PI+PFM; revisión con CSV 2026-06-15 | **VALIDADO EN BANCO (parcial)** — conducta abierta |
> | 14 | El OTOS de la DOWN salía pose `conf=0` (todo ceros) en el ROBOT2 | `diag_central_rx_all`: OTOS pose 100 Hz pero x=y=hdg=0, vel nunca difunde | **Binario equivocado:** R2 no tiene OTOS y estaba con el binario `down` (asume 2) → flashear `down_robot2` (OTOS=0), los consumidores caen al fallback | **VALIDADO EN BANCO** (hallazgo + fix) |
> | 15 | **Probamos comandar Flux con IA** para diseñar PCB | No funcionó | Cambiamos a EasyEDA por MCP (VIBE PCB design) — lo contamos porque el ensayo-y-error honesto es parte de aprender | Ensayo descartado |

> **Discusión:** las modificaciones mayores **#1, #4, #8 y #9 desbloquearon o protegieron capacidades enteras** (localización 2D, homologación del árbitro, frenado de borde, detectar la pelota fantasma). Distinguimos a propósito lo **validado en banco** (en la placa real) de lo **verificado en host** (compila/pasa tests pero todavía no en la placa). Nuestra **revisión interna adversarial** (20 subsistemas en una pasada del 2026-06-04 → 15/20 "solid", 0 críticos; + pasadas con varios subagentes) NO es una métrica independiente: es nuestro propio control de calidad.

---

# ZONA H — DATA (2/2): MÉTODOS DE TEST REPETIBLES + GRÁFICOS
*(Columna derecha, mitad inferior — elemento obligatorio "métodos de testing repetibles". Apunta a **Excellent**: el método descripto para que **otros lo repitan**, con gráficos/tablas.)*

**TEXTO EXACTO (impreso):**

> ## MÉTODOS DE TEST (repetibles por cualquier equipo)
> **M1 — Verificación host-native (sin la placa).** La lógica vive en módulos C++ puros; se compilan con
> `g++ -std=gnu++17 -I src/shared lib/Unity/src/unity.c src/shared/*.cpp test/test_X/*.cpp` y se corre el binario.
> **Resultado: 858 tests / 61 suites / 0 fallos** (medido **2026-06-14** con `scripts/run-host-tests.sh`). **Cifra viva —** la re-medimos cada sesión y sigue subiendo; por eso va con fecha, nunca queda desfasada. Esquiva el antivirus que bloqueaba PlatformIO. **Reproducibilidad (clave para la demo):** `run-host-tests.sh` necesita un `g++` en el PATH — nosotros usamos el que trae **Webots**; **sin un compilador en el PATH el script reporta 0 tests** (no es que fallen: no compilan). Documentamos el toolchain para que el juez/la demo no obtenga un 0 engañoso.
>
> **M2 — Power-cycle obligatorio en bring-up I²C.** Las direcciones de los VL53L7CX y los OTOS **persisten con 3V3**; un reset no alcanza. Protocolo: *flashear → cortar y reponer energía (10 s) → abrir monitor*. (Sin esto: falso negativo "ningún sensor responde".)
>
> **M3 — Odometría sobre superficie texturada.** Mover 300 mm controlados y comparar; tolerancia 8% (resultado: 6.5%). Script `diag_otos_move_test.py`.
>
> **M4 — Diagnóstico que reusa los parsers de producción.** Los ~40 sketches de banco **no reimplementan** el decodificador: validan `payload_len` contra `sizeof` y detectan *staleness*/CRC/SEQ-gap.
>
> **M5 — Enlace TOP→CENTRAL validado en banco (2026-06-14).** Con `diag_central_rx_all` en la CENTRAL: el WorldSnapshot del TOP llega a **66 Hz** (100 Hz de diseño), con **0 CRC** y **0 seqGap**, y decodifica entero (pose, heading válido, pelota, arco rival). Es la prueba de que las 3 placas hablan de verdad, no solo en simulación.
>
> **M6 — Tablero de salud por sensor (`python -m monitor_base --top-salud`), validado en la placa TOP (2026-06-14).** Semáforo OK/REVISAR/FALLA/SIN DATO por sensor (cámaras incl. pelota fantasma, BNO L/R, 4 ToF, US, OTOS, línea, snapshot) + grilla de zonas crudas de cada ToF. Conectó a la placa real y mostró dato real. *(Es un ecosistema Python aparte: 116 tests Python del monitor — NO se suman a los 858 tests host C++.)*

`[FIGURA Fig.8 — La red de seguridad creció sin pausa · barras: 246 → 262 → 324 → 354 → 403 → 470 → 545 → 658 → 834 → 858 tests · archivo docs/competencia/assets/fig8_test_growth.png (gen_figuras.py — ✓ regenerada 2026-06-14 con el punto 858) · Original diagram by the team — CC BY 4.0]`
`[FIGURA Fig.9 — La odometría depende del piso · barras de error OTOS por superficie: A4-lámina 9.5%, A4-limpio 0%, cartón 6.5% — sólo el cartón 6.5% está verificado en banco (TASK-029); lámina/limpio pendientes de confirmar · archivo docs/competencia/assets/fig9_otos_error.png (gen_figuras.py) · Original diagram by the team — CC BY 4.0]`
`[FIGURA Fig.14 — ¿Cómo sabe el monitor que un sensor miente, sin la cancha? La lógica de veredicto del tablero de salud (health.py): semáforo OK/REVISAR/FALLA/SIN DATO por sensor + el criterio que lo dispara · reglas verificadas en host (16 tests), tablero validado en banco 2026-06-14 · archivo docs/competencia/assets/drafts/fig_arbol_salud.png · Original diagram by the team — CC BY 4.0]`
`[FOTO Fig.10 — Una sesión de banco: el monitor serial decodificando un WorldSnapshot (diag_central_motors). Foto original del equipo, CC BY 4.0.]`
`[FOTO Fig.11 — El bodge: los 4 LP de ToF cableados a GPIO 9/10/11/12 (historia visual fuerte). Foto original del equipo, CC BY 4.0.]`

> **Nota de honestidad:** las cargas de CPU (~20/25/22%) y latencias (<15 ms) son *objetivos de diseño — no medidos con osciloscopio, TASK-014*. Medir antes de afirmarlas como datos.

---

# ZONA I — PRESENTATION PLAN
*(Columna derecha, base — apunta al criterio **Presentation**: presente TODA la sesión + activamente comprometido con jueces/participantes/invitados + responde todas las preguntas.)*

**TEXTO EXACTO (impreso, breve):**

> ## VISITÁ NUESTRO ROBOT
> Demostramos en vivo: **(0)** cómo **aprendimos a diseñar las placas TOP y DOWN con IA** (EasyEDA + Claude vía MCP) — nuestra feature de orgullo, con la honestidad del ensayo-y-error (Flux no funcionó); **(1)** la suite de **858 tests / 61 suites / 0 fallos** corriendo en la notebook (la garantía de que lo validamos nosotros); **(2)** el frenado de borde *(<15 ms de diseño)*; **(3)** el arquero que anticipa. Preguntanos cómo replicar cualquiera — y cómo aprender a diseñar con IA igual que nosotros.
> `[QR a repo]`  ·  `[QR a video TDP <3 min]`

**Checklist interno del equipo (NO impreso) para asegurar Excellent en Presentation:**
- En Incheon: **competidores María y Elías + coach principal Enzo** (que además asiste al equipo IITA de RCJ Rescue Line) **+ coach secundaria Cecilia** (respaldo). **El director Gustavo NO viaja.** Los presentes rotan; cada uno domina su dominio (visión / motores / PCB-diseño-con-IA / integración).
- **BLOQUEANTE antes de viajar:** verificar que `run-host-tests.sh` corre **VERDE en LA notebook de demo** con su toolchain (el `g++` de Webots en el PATH). Sin compilador en el PATH el script da **0 tests** y la demo de "la garantía" se cae en vivo.
- Banco vivo: notebook con `run-host-tests.sh` listo + un diag de banco para mostrar el decodificador (p. ej. `diag_central_rx_all` mostrando el TOP→CENTRAL a 66 Hz).
- Banco de preguntas ensayado por categoría (General, Eléctrica, Mecánica, Estrategia, Software, Desarrollo&Documentación).
- Material para regalar/compartir (QR, one-pager) → suma a Sportsmanship y Community.

---

# ZONA PIE — CRÉDITOS, LAYOUT Y LICENCIA
*(Banda inferior full-width — cierra el criterio **Layout** (tipografías consistentes, sin errores) y **Photos** (citadas).)*

**TEXTO EXACTO (impreso, chico):**

> **Licencia:** código y hardware bajo **MIT** © 2026 IITA / Fundación Innovar.
> **Imágenes:** Fig.1, 3, 5–7, 10–11 fotos/capturas originales del equipo (CC BY 4.0). Fig.2, 4, 8–9, 12, 13, 14 diagramas originales generados por el equipo (CC BY 4.0). Esquemático Zircon © Robomov (uso con atribución).
> **Repo:** https://github.com/IITA-Proyectos/open-soccer-robocup-team2026

**Especificación de LAYOUT (guía de diseño, NO impresa):**
- **Tipografías (2, consistentes):** títulos en una *sans* geométrica (p. ej. Montserrat/Inter Bold); cuerpo en una *sans* humanista legible (p. ej. Inter/Source Sans). Tamaños: títulos de zona ≥48 pt, subtítulos ≥32 pt, cuerpo ≥24 pt (legible a 1.5 m a A1; hacer la matemática de impresión ANTES de dibujar cada figura — ver skill `rcj-diagramas-poster`).
- **Paleta fija (4 colores):** azul profundo (TOP), naranja (CENTRAL), verde (DOWN) + gris neutro de fondo. Cada zona de placa usa su color para crear un mapa mental.
- **Numeración de figuras** Fig.1…Fig.12 consistente; cada figura con su **pie etiquetado y citado**.
- **Revisión ortográfica obligatoria** tras traducir al inglés (criterio Layout exige *sin errores de ortografía*).
- **Diseño original** (no plantilla genérica): el código de color por placa + el diagrama de flujo de datos son la "firma" creativa del poster.

> **Qué VA IMPRESO vs nota interna (regla de maquetación):** va al póster **solo** lo que está dentro de los bloques `> ## ...` ("TEXTO EXACTO (impreso)") + las figuras `[FIGURA ...]`/`[FOTO ...]` con su pie. **NO va impreso** nada bajo un encabezado marcado "(NO va impreso)" / "(NO impresa)" / "(NO impreso)" ni los renglones en cursiva `*(...)*` que abren cada ZONA (son guía de zona/rúbrica para el equipo).

---

# CHECKLIST FINAL ANTES DE IMPRIMIR (NO va impreso)

- [ ] **TRADUCIR TODO al inglés** (requisito duro de rúbrica) y correr corrector ortográfico.
- [x] ✅ Identidad COMPLETA 2026-06-13: equipo IITA Low Battery Messi · región Roboliga Argentina 2025 (UAI) · roster confirmado — **Director Gustavo Viollaz (no viaja)** · **Coach principal Enzo Juárez Velázquez (viaja)** · **Coach secundaria Cecilia Budeguer (viaja)** · competidores **María Virginia Viollaz** y **Elías Cordero (viajan)**.
- [ ] Costos de **referencia internacional ya cargados** (valor más alto/ítem: ≈USD 1.168 nuevo / 887 reusando CENTRAL; 2 robots ≈2.055–2.336). **Tipo de cambio cargado: 1480 ARS = 1 USD (2026-06-13)** → piso en pesos ≈ARS 1.728.640/robot nuevo (USD×1480 = MÍNIMO; landed real mayor por importación). Batería **6800 mAh** cargada. Falta del equipo: **precio Zircon suelto, motor real, C-rating/marca batería y horas**.
- [ ] Tomar y colocar **todas las `[FOTO:]`** (fotos del equipo: Fig.1, 5–7, 10–11) etiquetadas y citadas. Pies pre-escritos ya en cada zona.
- [x] ✅ Generadas 2026-06-05 (PNG @300dpi): **Fig.2** (fig2_dataflow.png), **Fig.4** (fig4_fsm.png), **Fig.8–9** (fig8/fig9). Falta maquetarlas en el A1 **y regenerar Fig.4 con GOTO_LINE + Fig.8 con el punto 858**.
- [ ] Terminar **Fig.12** (timeline del proceso constructivo) desde el draft `assets/drafts/fig_proceso_constructivo_timeline.svg`.
- [ ] Confirmar el **número de tests vivo** al cierre (verificado **858 tests / 61 suites / 0 fallos** el **2026-06-14** con `scripts/run-host-tests.sh` usando el g++ de Webots; **re-correr el día previo a entregar y re-propagar la cifra**).
- [ ] **BLOQUEANTE:** verificar que `run-host-tests.sh` corre **VERDE en LA notebook de demo** (con su toolchain `g++` en el PATH) ANTES de viajar — sin compilador da 0 tests.
- [ ] Verificar que el poster cabe en **A1 apaisado (≤84.1 cm ancho × 59.4 cm alto)** y es legible a 1.5 m.
- [ ] Generar los **QR** (repo + video TDP).
