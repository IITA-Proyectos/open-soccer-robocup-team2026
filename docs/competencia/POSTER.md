---
title: "POSTER técnico — RoboCupJunior Soccer Open 2026 (versión de trabajo ES)"
date: 2026-06-04
status: borrador-juzgado
idioma: español-rioplatense (TRADUCIR a inglés antes de entregar)
formato-objetivo: A1 apaisado (máx 70.7 cm alto × 100 cm ancho)
rubrica: RoboCupJunior Soccer 2026 — Poster Design & Presentation (5 pts, 6 criterios × 0/1/3/5)
---

> # ⚠️ VERSIÓN DE TRABAJO EN ESPAÑOL
> **El POSTER FINAL debe entregarse en INGLÉS (requisito de rúbrica RoboCupJunior).
> Este documento es la maqueta de trabajo en español para que el equipo lo lea y mejore.
> TRADUCIR TODO al inglés antes de imprimir/enviar.**
>
> *Working draft in Spanish. The FINAL poster must be submitted in ENGLISH (RCJ rubric requirement). Translate before printing.*

---

# CÓMO LEER ESTA MAQUETA (nota para el equipo, NO va impresa)

Este archivo describe el **layout físico** de un poster **A1 apaisado** (máx **70.7 cm de alto × 100 cm de ancho**).
Cada sección de abajo es una **ZONA del poster** con: (a) su posición en la grilla, (b) el **TEXTO EXACTO** que va impreso, y (c) las **imágenes** con `[FOTO: ...]`.

Los **títulos de zona están redactados para que un JUEZ encuentre cada criterio de la rúbrica de un vistazo**:

| Criterio de rúbrica (Poster, 5 pts) | ¿Dónde lo encuentra el juez? | Nivel apuntado |
|---|---|---|
| **Abstract** | Zona B — "ABSTRACT" | Excellent |
| **Method / Robot Production / Design** | Zona C, D, E, F — "METHOD & DESIGN" | Excellent |
| **Data / Results / Discussion** | Zona G, H — "DATA, RESULTS & DISCUSSION" | Excellent |
| **Photos / Images** | Todas las zonas (≥12 figuras etiquetadas Fig.N + créditos en pie) | Excellent |
| **Layout** | Grilla de 12 columnas, paleta fija, tipografías fijas (Zona Pie) | Excellent |
| **Presentation** (en vivo) | Guion de sesión — Zona I "PRESENTATION PLAN" | Excellent |

---

# GRILLA DEL POSTER (A1 apaisado, 100 cm ancho × 70.7 cm alto)

```
 100 cm de ANCHO  →
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ ZONA A · TÍTULO / IDENTIFICACIÓN  (banda superior full-width, ~10 cm alto)                  │  ▲
├───────────────┬──────────────────────────────────┬───────────────────────────────────────┤  │
│ ZONA B        │ ZONA C  METHOD & DESIGN (1/3):     │ ZONA G  DATA, RESULTS & DISCUSSION    │  │
│ ABSTRACT      │  Arquitectura + justificación      │  (1/2): tabla iteraciones            │  │
│ (col 1-3)     │  (col 4-8)                          │  testeo→dato→modificación (col 9-12) │  │  70.7 cm
│               ├──────────────────────────────────┤                                       │  de
│ ZONA B2       │ ZONA D  METHOD & DESIGN (2/3):     │                                       │  ALTO
│ TEAM JOURNEY  │  Sensores + lenguaje + software    ├───────────────────────────────────────┤  │
│ (recorrido)   │  (col 4-8)                          │ ZONA H  DATA (2/2): métodos de test   │  │
│ (col 1-3)     ├──────────────────────────────────┤  repetibles + gráficos (col 9-12)     │  │
│               │ ZONA E  METHOD & DESIGN (3/3):     │                                       │  │
│ ZONA B3       │  BOM + costo + tiempo desarrollo   ├───────────────────────────────────────┤  │
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
> ### Empujar para ganar: un robot de fútbol de 3 placas, sin pateador, verificado por software
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
> Presentamos un robot de RoboCupJunior Soccer Open con una **arquitectura distribuida de 3 placas Teensy + 1 módulo de comunicación**, en el que cada microcontrolador es **especialista, no generalista**: **TOP** (Teensy 4.0) percibe el mundo (2 cámaras OpenMV N6, 1 IMU BNO055, 4 sensores ToF VL53L7CX, 1 ultrasonido, árbitro RCJ por GPIO) y publica un **WorldSnapshot de 31 bytes a 100 Hz**; **CENTRAL** (Teensy 4.1 sobre PCB Zircon) decide (máquina de estados táctica + cinemática inversa omni-3 + PIDs) y mueve **3 ruedas omni kiwi a 120°**; **DOWN** (Teensy 4.0) es el plato estructural y el sensor de piso (**anillo de 32 sensores de línea** multiplexados + 2 odómetros ópticos OTOS), y **difunde** su medición a ambas placas (*broadcast simétrico* con detección de pérdida por secuencia).
>
> El robot **no tiene pateador físico**: el delantero **empuja la pelota por inercia** al alinearse con el arco rival, lo que reduce componentes, energía y puntos de falla. La contribución metodológica central es una **disciplina de verificación de firmware embebido en la PC sin la placa**: la lógica de decisión vive en **módulos C++ puros** compilados y testeados con `g++` offline (**658 tests / 47 suites / 0 fallos**, medido el 2026-06-05 19:50 ART con `scripts/run-host-tests.sh`), con el *glue* de Arduino delgado. Tres innovaciones diferenciales: **(1) fail-safe en capas** con bus de emergencia directo DOWN→CENTRAL para frenar en **<15 ms** en el borde; **(2) fallback byte-idéntico** que deja "dormir" cada feature nueva hasta que su dato fluye, sin regresión; **(3) arquero que anticipa por velocidad de pelota**. El proyecto es **open-source (MIT)** e incluye PCBs fabricables (EasyEDA), contratos de datos byte-a-byte y un *diario de ingeniería* con cada iteración medida en banco. **Estado honesto (no sobrevendemos):** el bloqueante #1 que queda para Incheon es la **visión sin recalibrar** (color LAB + homografía); funciones como la **trilateración 2D con ToF**, el **strafe del arquero** y el **drive-straight con OTOS** están *code-complete* y verificadas en host, pero **pendientes de validación en banco** (sus conductas "duermen" hasta que su dato fluye). Este poster documenta el diseño con suficiente detalle para que **otro equipo lo replique**.

---

# ZONA B2 — TEAM JOURNEY (recorrido del equipo)
*(Columna izquierda, medio — refuerza el criterio **Layout/Presentation** con la "narrativa del recorrido" que premia la rúbrica; refuerza también el Abstract con contexto humano.)*

**TEXTO EXACTO (impreso):**

> ## NUESTRO RECORRIDO
> - **Dic-2025:** campeones nacionales (Roboliga Argentina (UAI)) con un robot monolítico sobre la placa Zircon.
> - **2026:** rediseño a **3 placas** reutilizando el cerebro campeón (Zircon) como CENTRAL y sumando percepción (TOP) y piso (DOWN) — *continuidad, no descarte*.
> - **May–Jun 2026:** bring-up de las 3 placas físicas, ~30 sesiones de banco documentadas, suite de tests de 180 → **658 tests / 47 suites / 0 fallos** (medido 2026-06-05 19:50 ART con `scripts/run-host-tests.sh`).
> - **Jun–Jul 2026:** Incheon. Estrategia declarada del equipo: **invertir en aprendizaje**, jugar partidos honestos y capturar datos.
>
> ### 🤖 Innovación 2026 — cómo trabajamos (metodología "VIBE", asistida por IA)
> Adoptamos un flujo asistido por IA (Claude) que llamamos **VIBE**: la IA **acelera** el diseño y la documentación, y el **equipo de 18 años decide, valida en banco y es el único responsable** de lo que se sube al robot. Cuatro frentes: **VIBE PCB Design** (EasyEDA comandado por Claude vía MCP), **VIBE 3D Design** (Fusion 360 vía MCP — recién empezando), **VIBE Coding** (firmware C++ con red de seguridad de 658 tests host) y **Claude para documentar y gestionar** el proyecto (TDP, contratos de datos byte-a-byte, *journal*). Compartimos la **metodología** —no solo el código— como aporte a la comunidad de RoboCupJunior.
>
> ### 🛣️ Próximo paso (roadmap declarado)
> **Comunicación robot-a-robot** (arquero ↔ delantero) por **ESP-NOW** vía la placa COMM (ESP32-C6, ya en el robot): compartir pose, si cada uno ve la pelota y su estado para **coordinar estrategia**. Falta integrarlo al WorldSnapshot y validarlo en banco — fiel a nuestra disciplina, la conducta cooperativa "duerme" hasta que el dato fluya, sin regresión. También planeamos pasar a **4 ruedas omni** con **motores más cortos, con encoders** (más estabilidad y control, y espacio liberado para el **kicker** y el **dribbler**, que quedan para el año próximo).

`[FOTO: equipo IITA con el robot/trofeo en el Nacional 2025 (UAI) — etiquetar Fig.1]`

> **Roles del equipo:**
> | Rol | Integrante | Aporte técnico |
> |---|---|---|
> | Mentor (no viaja) | Gustavo Viollaz | Coordinación, sesiones de banco, integración 3 placas |
> | Coach | Enzo Juárez Velázquez | Diseño de PCB (EasyEDA), *bodges* de hardware, validación eléctrica |
> | Competidora | María Virginia Viollaz (18) | Visión artificial, trayectorias, parser de cámara |
> | Competidor | Elías Cordero (Ing. Electromecánica, UNSa) | Banco de motores, cinemática, mediciones |

> **Nota de gap:** confirmar **roster formal** (edades/categoría) y "quién hizo qué" para los créditos.

---

# ZONA B3 — OPEN SOURCE & COMMUNITY
*(Columna izquierda, abajo — apoya el bonus de TDP y el componente "Documentation & Community". En el poster aporta a **Method/Design** "intención de compartir TODO el conocimiento accionable".)*

**TEXTO EXACTO (impreso):**

> ## OPEN SOURCE (TODO publicado, MIT)
> - **Software:** firmware completo de las 3 placas (C++17) + visión (MicroPython) + **658 tests / 47 suites / 0 fallos** (medido 2026-06-05 19:50 ART con `scripts/run-host-tests.sh`) + scripts de banco.
> - **Hardware:** proyectos **EasyEDA** completos de TOP y DOWN (esquemático + PCB + Gerbers + BOM + Pick&Place); CENTRAL = Zircon Rev v15 (esquemático público).
> - **Cómo, no solo qué:** documentos vivos **FUENTES-DE-VERDAD** (un doc canónico por tema), **MAPA-DE-DATOS** (cada mensaje: tipo/tamaño/pin/frecuencia/quién lo llena y consume) y **diario de ingeniería** con cada iteración.
>
> **Repo:** `https://github.com/IITA-Proyectos/open-soccer-robocup-team2026` `[QR al repo]`

---

# ZONA C — METHOD & DESIGN (1/3): ARQUITECTURA + JUSTIFICACIÓN
*(Centro, arriba — elemento obligatorio "Method/Robot Production/Design". Apunta a **Excellent**: producción completa, clara y concisa, CON justificación de cada decisión.)*

**TEXTO EXACTO (impreso):**

> ## MÉTODO Y DISEÑO — ¿Por qué 3 placas?
> Cada placa procesa **donde está el sensor** y se decide en **el centro** (regla estándar de robótica móvil). Esto reduce el tráfico UART, apunta a dejar cada MCU **<30% de CPU** (*objetivo de diseño*, no medido con osciloscopio aún) y permite reemplazar una placa sin tocar las otras.
>
> | Placa | MCU | Rol | Sensores / actuadores |
> |---|---|---|---|
> | **TOP** | Teensy 4.0 | "Veo el mundo" | 2 cámaras OpenMV N6 · 1 IMU BNO055 · 4 ToF VL53L7CX · 1 HC-SR04 · árbitro GPIO |
> | **CENTRAL** | Teensy 4.1 (Zircon) | "Decido" | FSM táctica · cinemática omni-3 · 3 PIDs · **3 motores** |
> | **DOWN** | Teensy 4.0 | "Toco el suelo" | 32 sensores de línea (4 mux CD4051) · 2 OTOS |
> | **COMM** | ESP32-C6 | árbitro RCJ | nivel GPIO 3.3 V hacia TOP |
>
> **Decisiones justificadas (data-driven):**
> - **Sin pateador:** el delantero empuja por inercia → menos masa, menos energía, menos fallas.
> - **Bus de emergencia DOWN→CENTRAL** (1 salto UART): a 1 m/s el robot recorre **1 mm/ms**; pasar la alarma de borde por 2 UART en serie agrega ~25 mm de *overshoot* → se cablea un atajo directo para frenar **<15 ms**.
> - **Todos los PIDs en CENTRAL:** un solo lugar con todas las ganancias.
> - **Continuidad:** CENTRAL es el Zircon que ganó el Nacional 2025; si una placa nueva falla, degrada a modo monolítico.

`[FOTO: diagrama de bloques renderizado del flujo de datos — 3 cajas TOP/CENTRAL/DOWN con flechas etiquetadas "WorldSnapshot 31 B @100 Hz", "LineStatusV2 16 B @200 Hz", "bus emergencia" — Fig.2 · archivo docs/competencia/assets/fig2_dataflow.png (gen_diagramas.py)]`

---

# ZONA D — METHOD & DESIGN (2/3): SENSORES, LENGUAJE Y SOFTWARE
*(Centro, medio — cubre los obligatorios "lenguaje de programación" y "sensores usados", con insight de software.)*

**TEXTO EXACTO (impreso):**

> ## SENSADO, LENGUAJE Y SOFTWARE
> **Lenguajes:** **C++17** (firmware, `namespace iitasoccer`, structs *packed* con `static_assert` de tamaño) + **MicroPython** (visión OpenMV N6, detección por color en espacio LAB).
> **Build:** PlatformIO (57 entornos) + un runner `g++` offline para los tests.
>
> **Sensores y para qué:**
> | Sensor | Cantidad | Función |
> |---|---|---|
> | Cámara OpenMV N6 (STM32N6 + NPU) | 2 | Pelota + arcos (QVGA, ~30 Hz) |
> | IMU BNO055 | 1 sano | Heading (yaw) |
> | ToF VL53L7CX (8×8 zonas) | 4 | Distancia a paredes → **localización 2D** |
> | OTOS (odometría óptica) | 2 | Pose/velocidad por *slip* del piso |
> | Anillo de línea (fototransistor) | 32 | Borde de cancha (frenado) |
> | HC-SR04 ultrasonido | 1 | Obstáculo frontal (redundante c/ToF) |
>
> **Insight de software (estructura del código):**
> - **WorldSnapshot v3 = 31 B** (`static_assert(sizeof==31)`), evolución de contrato v1(24 B)→v2(27 B)→v3(31 B).
> - **Protocolo UART robusto:** trama `[0xAA | LEN | TYPE | SEQ | PAYLOAD | CRC16-CCITT | 0x55]`; el decodificador es una máquina de estados byte-a-byte que **resincroniza sola** (un byte basura no contamina el frame siguiente).
> - **Cinemática omni-3 pura:** `v_i = -vx·sin(θ_i) + vy·cos(θ_i) + ω·R`, con saturación **proporcional** (escala las 3 ruedas para preservar la trayectoria).
> - **Arquero que anticipa:** apunta a la **X predicha** = `pos + v·lookahead` (no a la X actual).

`[FOTO: captura de pantalla de la suite de 658 tests host pasando en verde (terminal run-host-tests.sh) — Fig.3]`
`[DIAGRAMA: flowchart de la FSM táctica dual — ATTACKER: WAIT_START→KICKOFF→SEARCH→POSITION→APPROACH (+LINE_AVOID); GOALKEEPER: WAIT_START→PATROL→INTERCEPT→CLEAR (+LINE_AVOID); EMERGENCY_LINE bypassa la FSM. (El empuje al arco NO es un estado: ocurre dentro de APPROACH.) Fuente lista: el diagrama Mermaid de docs/competencia/assets/diagramas.md (verificado vs strategy.cpp); detalle en docs/firmware/ESTRATEGIA-ALTO-NIVEL.md — Fig.4 · archivo docs/competencia/assets/fig4_fsm.png (gen_diagramas.py)]`

---

# ZONA E — METHOD & DESIGN (3/3): BOM, COSTO Y TIEMPO DE DESARROLLO
*(Centro, abajo — elementos obligatorios "tiempo y costo de desarrollo" + "BOM de componentes mayores".)*

**TEXTO EXACTO (impreso):**

> ## COMPONENTES MAYORES (BOM) · COSTO · TIEMPO
>
> La **BOM completa de componentes mayores** (con **part numbers**, **fuente/proveedor**, columna **Nuevo vs. reusado**, **Kit/Custom** y costos unitarios reales LCSC) es la **fuente única** `docs/competencia/BOM.md` — este poster la **referencia**, no la duplica. Extracto de cabecera:
>
> | Componente mayor | Part number / modelo | Cant. | Nuevo / Reusado | Costo unit. (USD) |
> |---|---|---|---|---|
> | Cámara OpenMV N6 | OpenMV Cam N6 (STM32N6 + NPU) | 2 | Nuevo | **USD 165 c/u** (ref. int.; el más caro) |
> | MCU TOP / DOWN | Teensy 4.0 (LCSC `C99001332551`) | 2 | Nuevo | **USD 23.80 c/u** (ref. int.) |
> | MCU CENTRAL | Teensy 4.1 (en PCB Zircon) | 1 | **Reusado** (campeón 2025) | **USD 31.50** (ref. int.) |
> | IMU BNO055 | Bosch BNO055 (U10/U11) | 2 (1 sano) | Nuevo | **~USD 35 c/u** (ref. int.) |
> | ToF VL53L7CX | ST VL53L7CX (módulo Pololu) | 4 | Nuevo | **USD 19.95 c/u** (ref. int.) |
> | OTOS SparkFun | SparkFun OTOS (U5/U6) | 2 | Nuevo | **USD 84.95 c/u** (ref. int.) |
> | PCB Zircon Rev v15 (CENTRAL) | Robomov Zircon Rev v15 (COTS) | 1 | **Reusado** | **~USD 200** (precio Robomov suelto pendiente — kit USD 529) |
> | Mux CD4051BM | TI CD4051BM (LCSC `C353976`) | 4 | Nuevo | 0.96 |
> | Regulador buck MP1584-EN | MP1584-EN (módulo SIP) | 6 | Nuevo | **USD 0.90 c/u** (ref. int.) |
> | Batería LiPo 2S 7.4 V | LiPo 2S (mAh/C a confirmar) | 1–2 | Nuevo | **~USD 10–25** (specs pendientes) |
> | 3 motores DC + 3 ruedas omni | Motor "TT" + rueda omni KIWI | 3+3 | Nuevo | **motor pendiente** (TT ~USD 3 vs Pololu HP USD 23.95) |
> | **TOTAL por robot** | — | — | — | **≈ USD 1.000 nuevo · ≈ USD 770 reusando CENTRAL** (ref. int.; 2 robots ≈ USD 1.800–2.000) |
>
> **Tiempo de desarrollo:** rediseño 2026 ≈ **8 semanas** de ingeniería intensiva (mayo–junio), sobre la base del robot campeón 2025; esfuerzo total ≈ **4 meses** (feb–jun 2026) trazable en `journal/`. Suite de tests creciendo **180 → 246 → 262 → 324 → 354 → 545 → 658** (medido 2026-06-05 19:50 ART con `scripts/run-host-tests.sh`; ver Fig.8).
>
> **Precios reales disponibles en el repo (unitarios LCSC, citados verbatim en BOM.md):** fototransistor ALS-PT19 ≈ 0.116 · CD4051BM 0.96 · LED 0402 0.016 · diodo B5819W 0.024 USD.

> **Nota de gap (registrar):** costos = **precio internacional de referencia** (USD, verificado 2026-06-05; fuente única `BOM.md` §3). **Pendiente del equipo (chico):** precio del **Zircon** suelto (Robomov publica el kit a USD 529), qué **motor** usan (TT ~USD 3 vs Pololu HP USD 23.95), el **tipo de cambio ARS/USD** del día y las **horas** de desarrollo. El costo *landed* local es **mayor** por las restricciones de importación argentinas.

---

# ZONA F — FOTO PRINCIPAL DEL ROBOT
*(Centro, base — ancla visual del poster. Cumple "Photos/Images: abundantes, etiquetadas y citadas".)*

`[FOTO: robot 2026 completo, vista superior mostrando las 3 ruedas omni a 120° y el stack de 3 placas — Fig.5, etiquetar TOP/CENTRAL/DOWN con líneas guía]`
`[FOTO: vista lateral mostrando la pila de placas (standoffs) y el montaje de motores — Fig.6]`
`[FOTO: detalle del anillo de 32 sensores de línea en la cara inferior del plato DOWN — Fig.7]`

> **Pie:** *Fig.5–7 — Robot IITA 2026. Fotos originales del equipo (CC BY 4.0).*

---

# ZONA G — DATA, RESULTS & DISCUSSION (1/2): ITERACIONES TESTEO → MODIFICACIÓN
*(Columna derecha, mitad superior — elemento obligatorio "Data/Results/Discussion". Apunta a **Excellent**: datos de test significativos + **modificaciones MAYORES hechas como resultado del testing** + vínculo claro testeo→evaluación→modificación.)*

**TEXTO EXACTO (impreso):**

> ## DATOS, RESULTADOS Y DISCUSIÓN
> Cada fila es una iteración **real** de banco: medimos → evaluamos → **modificamos la fuente en un solo punto** → re-verificamos con la suite host.

> | # | Problema observado | Dato medido (banco) | Modificación aplicada |
> |---|---|---|---|
> | 1 | **Los 4 ToF chocan en I²C** (todos nacen en 0x29); el PCB rev 1.0 no ruteó XSHUT (8 *No-Connect*) | Forense del esquemático: 0 nets XSHUT. Tras *bodge* + **power-cycle**: 4 LP en pines {9,10,11,12} enumeran a **0x2A–0x2D** | Migrar los 4 ToF al bus único `Wire`; liberar `Wire1` para DOWN; **desbloquea la localización 2D** |
> | 2 | **Heading del BNO se congela** en producción (yaw clavado en −108.3°) aunque el snapshot llega sano | BNO + ToF no coexisten a 400 kHz; a **100 kHz + BNO @20 Hz** el heading sobrevive (band-aid) | I²C a 100 kHz + lectura del BNO a 20 Hz; decisión de diseño: el heading-hold del arquero usa el **OTOS** (local), no el BNO |
> | 3 | **Odometría OTOS** reporta basura sobre hoja A4 | A4 con lámina: 28.6/300 mm (9.5%); A4 sin lámina: 0.3 mm; **cartón corrugado: 280.4/300 mm = 6.5% error** (< 8% tol.) | Quitar lámina + exigir piso texturado → **mejora 10×**; la cancha verde de RCJ ya lo cumple |
> | 4 | **Árbitro no llegaba al Teensy** (el TOP esperaba un frame UART que la COMM nunca emite) | La COMM oficial entrega START/STOP como **nivel GPIO 3.3 V**; en PLAY sube **un solo** pin | Leer pines 5/6 con `INPUT_PULLDOWN`, `match_running = pin5 **OR** pin6`; fail-safe a STOP. **El árbitro movió el robot por 1ª vez** |
> | 5 | **Motor 2 (U17) gira invertido** por HW (INA/INB cruzados) | Banco rueda-por-rueda (María/Elías) | `MOTOR_INVERT = {+1, −1, +1}` aplicado en **un solo lugar** |
> | 6 | En *strafe* lateral **solo gira el motor 1** | A `vx=150`/`MAX=1000`, M1 y M2 reciben ~13% PWM (33/255) → debajo del arranque; **M3 a 180° proyecta 0 (correcto)** | Diagnóstico: deadzone PWM + geometría kiwi (no es bug de pin). Propuesta `MOTOR_MIN_PWM` 25–45 por robot |
> | 7 | **Overflow int16 de omega** (CRÍTICO): clamp 360 → 36000 centideg > 32767 → giro **invertido a fondo** | Auditoría con 13 subagentes; verificado contra el código | `HeadingPID.output_clamp` 360 → **327** (327·100 < 32767); test anti *sign-flip* |
> | 8 | **CENTRAL ciega a la línea**: decodificaba el contrato viejo (5 B) y descartaba todos los frames de 16 B | `payload_len==5` rechazaba el **100%** de los frames reales | Migrar a `LineStatusV2` (16 B); harness g++ 8/8 PASS |

> **Discusión:** las modificaciones mayores **#1, #4 y #8 desbloquearon capacidades enteras** (localización 2D, homologación del árbitro, frenado de borde). La auditoría paralela (20 subsistemas, revisión adversarial) cerró con **15/20 "solid", 0 críticos**.

---

# ZONA H — DATA (2/2): MÉTODOS DE TEST REPETIBLES + GRÁFICOS
*(Columna derecha, mitad inferior — elemento obligatorio "métodos de testing repetibles". Apunta a **Excellent**: el método descripto para que **otros lo repitan**, con gráficos/tablas.)*

**TEXTO EXACTO (impreso):**

> ## MÉTODOS DE TEST (repetibles por cualquier equipo)
> **M1 — Verificación host-native (sin la placa).** La lógica vive en módulos C++ puros; se compilan con
> `g++ -std=gnu++17 -I src/shared lib/Unity/src/unity.c src/shared/*.cpp test/test_X/*.cpp` y se corre el binario.
> **Resultado: 658 tests / 47 suites / 0 fallos** (medido 2026-06-05 19:50 ART con `scripts/run-host-tests.sh`). **Cifra viva —** la re-medimos cada sesión y sigue subiendo; por eso va con fecha y hora, nunca queda desfasada. Esquiva el antivirus que bloqueaba PlatformIO.
>
> **M2 — Power-cycle obligatorio en bring-up I²C.** Las direcciones de los VL53L7CX y los OTOS **persisten con 3V3**; un reset no alcanza. Protocolo: *flashear → cortar y reponer energía (10 s) → abrir monitor*. (Sin esto: falso negativo "ningún sensor responde".)
>
> **M3 — Odometría sobre superficie texturada.** Mover 300 mm controlados y comparar; tolerancia 8% (resultado: 6.5%). Script `diag_otos_move_test.py`.
>
> **M4 — Diagnóstico que reusa los parsers de producción.** Los ~40 sketches de banco **no reimplementan** el decodificador: validan `payload_len` contra `sizeof` y detectan *staleness*/CRC/SEQ-gap.

`[GRÁFICO: barras del crecimiento de la suite de tests — 180 → 246 → 262 → 324 → 354 → 545 → 658 — Fig.8 · archivo docs/competencia/assets/fig8_test_growth.png (gen_figuras.py)]`
`[GRÁFICO: barras de error de odometría OTOS por superficie — A4-lámina 9.5% error, A4-limpio 0%, cartón 6.5% — Fig.9 · archivo docs/competencia/assets/fig9_otos_error.png (gen_figuras.py)]`
`[FOTO: sesión de banco con monitor serial decodificando un WorldSnapshot / diag_central_motors — Fig.10]`
`[FOTO: el bodge de los 4 LP de ToF cableados a GPIO 9/10/11/12 (historia visual fuerte) — Fig.11]`

> **Nota de gap:** las cargas de CPU (~20/25/22%) y latencias (<15 ms) son **objetivos de diseño**, no mediciones con osciloscopio (TASK-014). Medir antes de afirmarlas como datos.

---

# ZONA I — PRESENTATION PLAN
*(Columna derecha, base — apunta al criterio **Presentation**: presente TODA la sesión + activamente comprometido con jueces/participantes/invitados + responde todas las preguntas.)*

**TEXTO EXACTO (impreso, breve):**

> ## VISITÁ NUESTRO ROBOT
> Demostramos en vivo: **(1)** la suite de **658 tests / 47 suites / 0 fallos** corriendo en la notebook, **(2)** el frenado de borde **<15 ms**, **(3)** el arquero que anticipa. Preguntanos cómo replicar cualquiera.
> `[QR a repo]`  ·  `[QR a video TDP <3 min]`

**Checklist interno del equipo (NO impreso) para asegurar Excellent en Presentation:**
- Los **4 integrantes** presentes y rotando; cada uno domina su dominio (visión / motores / PCB / integración).
- Banco vivo: notebook con `run-host-tests.sh` listo + un diag de banco para mostrar el decodificador.
- Banco de preguntas ensayado por categoría (General, Eléctrica, Mecánica, Estrategia, Software, Desarrollo&Documentación).
- Material para regalar/compartir (QR, one-pager) → suma a Sportsmanship y Community.

---

# ZONA PIE — CRÉDITOS, LAYOUT Y LICENCIA
*(Banda inferior full-width — cierra el criterio **Layout** (tipografías consistentes, sin errores) y **Photos** (citadas).)*

**TEXTO EXACTO (impreso, chico):**

> **Licencia:** código y hardware bajo **MIT** © 2026 IITA / Fundación Innovar.
> **Imágenes:** Fig.1, 5–7, 10–11 fotos originales del equipo (CC BY 4.0). Fig.2–4, 8–9 diagramas originales generados por el equipo. Esquemático Zircon © Robomov (uso con atribución).
> **Repo:** https://github.com/IITA-Proyectos/open-soccer-robocup-team2026

**Especificación de LAYOUT (guía de diseño, NO impresa):**
- **Tipografías (2, consistentes):** títulos en una *sans* geométrica (p. ej. Montserrat/Inter Bold); cuerpo en una *sans* humanista legible (p. ej. Inter/Source Sans). Tamaños: títulos de zona ≥48 pt, subtítulos ≥32 pt, cuerpo ≥24 pt (legible a 1.5 m).
- **Paleta fija (4 colores):** azul profundo (TOP), naranja (CENTRAL), verde (DOWN) + gris neutro de fondo. Cada zona de placa usa su color para crear un mapa mental.
- **Numeración de figuras** Fig.1…Fig.11 consistente; cada figura con su **pie etiquetado y citado**.
- **Revisión ortográfica obligatoria** tras traducir al inglés (criterio Layout exige *sin errores de ortografía*).
- **Diseño original** (no plantilla genérica): el código de color por placa + el diagrama de flujo de datos son la "firma" creativa del poster.

---

# CHECKLIST FINAL ANTES DE IMPRIMIR (NO va impreso)

- [ ] **TRADUCIR TODO al inglés** (requisito duro de rúbrica) y correr corrector ortográfico.
- [x] ✅ Identidad COMPLETA 2026-06-05: equipo IITA Low Battery Messi · región Roboliga Argentina 2025 (UAI) · roster (María Virginia Viollaz / Elías Cordero + coach Enzo Juárez Velázquez / mentora Cecilia Budeguer).
- [ ] Costos de **referencia internacional ya cargados** (≈USD 1.000 nuevo / 770 reusando CENTRAL). Falta del equipo: **precio Zircon suelto, motor, tipo de cambio ARS/USD y horas**.
- [ ] Tomar y colocar **todas las `[FOTO:]`** (Fig.1–11) etiquetadas y citadas.
- [x] ✅ Generadas 2026-06-05 (PNG @300dpi): **Fig.2** (fig2_dataflow.png), **Fig.4** (fig4_fsm.png), **Fig.8–9** (fig8/fig9). Falta solo maquetarlas en el A1.
- [ ] Confirmar el **número de tests vivo** al cierre (verificado **658 tests / 47 suites / 0 fallos** el 2026-06-05 18:39 ART con `scripts/run-host-tests.sh`; re-correr antes de imprimir).
- [ ] Verificar que el poster cabe en **A1 apaisado (≤70.7×100 cm)** y es legible a 1.5 m.
- [ ] Generar los **QR** (repo + video TDP).
