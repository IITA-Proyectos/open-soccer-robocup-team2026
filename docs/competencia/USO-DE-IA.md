---
title: "Uso de Inteligencia Artificial en el proyecto — RoboCupJunior Soccer Open 2026"
team: "IITA Low Battery Messi"
org: "Instituto de Innovación y Tecnología Aplicada (IITA) / Fundación Innovar"
date: 2026-06-05
status: documento vivo (versión de trabajo ES — traducir a EN para entrega)
proposito: "Declarar de forma transparente y fundamentada CÓMO y DÓNDE usamos IA, qué hace la IA y qué hacen las personas, y cómo garantizamos la autoría y la correctitud."
---

> ⚠️ **VERSIÓN DE TRABAJO EN ESPAÑOL.** Para la entrega/jueces de RoboCupJunior se traduce al inglés (`en/USO-DE-IA.md`). Este documento es deliberadamente extenso: está pensado para responder a fondo si un juez pregunta "¿cómo usaron IA?".

# Uso de Inteligencia Artificial en el proyecto

## 0. Resumen ejecutivo (TL;DR para el juez)

Usamos IA (principalmente **Claude**, operada por nosotros vía *Claude Code* y servidores **MCP**) como una **herramienta de ingeniería que acelera el trabajo**, exactamente como una calculadora, un entorno de CAD, un compilador o un linter aceleran el trabajo — **pero sin reemplazar las decisiones humanas**. Nuestro principio es uno solo y es innegociable:

> **La IA acelera; los competidores (18 años) deciden, validan en hardware real y son los únicos responsables de lo que se sube al robot.**

La IA toca **siete frentes**: diseño de PCB (*VIBE PCB Design*), diseño 3D (*VIBE 3D Design*), programación de firmware (*VIBE Coding*), **testing**, **documentación**, **debugging**, y a futuro **reconocimiento de imágenes con redes neuronales (YOLO)**. En **todos**, hay un **humano que revisa, valida y aprueba** antes de que el cambio sea real, y una **red de seguridad objetiva** (gate de tests, validación de banco) que prueba que el cambio es correcto. Nada se "cree" porque lo dijo la IA: **se verifica**.

**El propósito central de todo esto es uno: acelerar tiempos** — comprimir el ciclo de ingeniería completo, del **concepto** al **robot andando**. Nuestra meta declarada es recorrer las **siete etapas** (concepto → PCB → 3D → montaje → programación → documentación → testeo) en **30 días** con los materiales a mano, porque cada frente VIBE le quita el cuello de botella a una etapa. Lo desarrollamos a fondo en el §1.4, con el caveat honesto de que este año el ciclo fue más lento — pero por la importación de materiales en Argentina, no por el método.

Este documento existe porque creemos que la transparencia es parte de la buena ingeniería. No escondemos que usamos IA: **explicamos por qué su uso es legítimo, educativo y verificable**, y exactamente dónde empieza y termina su rol.

> 📎 **Companion:** la pregunta de fondo "¿es trampa?" la respondemos con evidencia verificable (reglas RCJ, misión RoboCup, estudios 2025-2026) en [`IA-VIBE-ENGINEERING-EVIDENCIA.md`](IA-VIBE-ENGINEERING-EVIDENCIA.md). Este documento (USO-DE-IA) explica el **CÓMO**; el companion argumenta el **POR QUÉ es legítimo** y por qué las competencias deberían promoverlo.

---

## 1. Filosofía y postura ética (la fundamentación)

### 1.1 Por qué consideramos legítimo usar IA
Toda la historia de la ingeniería es la historia de **herramientas que amplifican al ingeniero**: la regla de cálculo, la hoja de cálculo, el autorouter de PCB, el compilador optimizador, el solver de elementos finitos. Ninguna de esas herramientas "hace ingeniería sola": **requieren que una persona plantee el problema, interprete el resultado y se haga responsable**. La IA generativa es la herramienta más reciente de esa línea. Usarla bien es una **competencia del siglo XXI** que la educación técnica debería enseñar, no prohibir.

Lo que distingue el uso **legítimo** del **ilegítimo** no es *si* se usa IA, sino **cómo**:
- **Ilegítimo:** pegar un pedido, copiar la salida sin entenderla, no poder explicar lo que se entregó, no poder reproducirlo, atribuirse trabajo que no se comprende.
- **Legítimo (lo nuestro):** usar la IA para **acelerar tareas que entendemos**, **revisar cada salida**, **validar contra la realidad** (banco/tests), **entender el resultado lo suficiente para defenderlo y modificarlo**, y **declarar abiertamente** dónde se usó.

### 1.2 El principio rector: *human-in-the-loop* con responsabilidad humana
En nuestro flujo, la IA **propone**; el humano **dispone**. Concretamente, ningún artefacto llega al robot o al entregable sin pasar por **al menos uno** de estos controles humanos/objetivos:
1. **Revisión humana explícita** del cambio (leemos el diff, entendemos qué cambia y por qué).
2. **Gate de verificación objetivo** (la suite de tests host-native tiene que seguir en verde).
3. **Validación física en banco** (el comportamiento se prueba con el robot real, no se asume).
4. **Trazabilidad** (cada commit asistido queda marcado con coautoría; el *journal* de ingeniería registra cada iteración).

### 1.3 Honestidad sobre los límites
La IA en este proyecto **no** mide con osciloscopio, **no** suelda, **no** calibra la cámara en la cancha real, **no** decide la estrategia de juego por nosotros y **no** garantiza que algo funcione: solo lo hace plausible y rápido. Las cargas de CPU, las latencias, la calibración de visión y la validación de motores **son trabajo humano de banco**. Lo decimos explícitamente en el TDP y en este documento.

### 1.4 El objetivo de fondo: acelerar tiempos — el ciclo de 30 días

Todo lo anterior (la legitimidad, el *human-in-the-loop*, los límites honestos) describe **cómo** usamos IA. Esta subsección dice **para qué**. Y la respuesta es una sola, y es el corazón de nuestro proyecto:

> **Usamos IA para ACELERAR TIEMPOS: comprimir el ciclo de ingeniería completo, desde el CONCEPTO hasta el ROBOT ANDANDO.**

No usamos IA por moda ni para "parecer modernos". La usamos porque resuelve el problema más caro de cualquier equipo de robótica: **el tiempo que pasa entre tener una idea y tener un robot que la ejecuta en la cancha**. Ese *time-to-working-robot* es, históricamente, de meses. Nuestra apuesta —y la creemos posible— es bajarlo a **semanas**.

**La meta concreta: el ciclo completo en 30 días.** Sostenemos que, con los materiales disponibles (motores, sensores, cámaras), un equipo que use IA con disciplina puede recorrer **las siete etapas completas** del desarrollo de un robot de competencia en **30 días**:

1. **Diseño conceptual** — arquitectura del robot, estrategia de juego, división en subsistemas.
2. **Diseño de PCB + fabricación** — esquemático, ruteo, Gerbers, BOM, pedido a fábrica.
3. **Modelo 3D del chasis + impresión 3D** — CAD paramétrico de piezas y soportes, exportar e imprimir.
4. **Montaje** — armado mecánico y electrónico del robot físico.
5. **Programación** — firmware de las placas y la lógica de juego.
6. **Documentación** — TDP, póster, contratos de datos, *journal*, entregables de rúbrica.
7. **Testeo de programas** — verificación de la lógica y validación de comportamiento.

**El ciclo completo.** No una etapa: las siete. Y **creemos que es posible** porque tenemos evidencia de que cada etapa, por separado, ya se comprime.

**Por qué es posible: cada frente VIBE le quita el cuello de botella a una etapa.** La IA no acelera "el proyecto" en abstracto; acelera **cada eslabón** de la cadena, que es donde realmente se pierde el tiempo:

- **VIBE PCB Design** (esquemático/ruteo asistido vía MCP, §4.1) comprime el **diseño electrónico**: lo más tedioso y propenso a error —mapear pines, generar pinouts, revisar netlists— deja de costar días.
- **VIBE 3D Design** (Fusion 360 vía MCP, §4.2) comprime el **chasis**: el primer borrador imprimible de una pieza pasa de horas de CAD a minutos.
- **VIBE Coding** (módulos puros + host-tests, §4.3 y §4.4) comprime **programación y testeo a la vez**, y con una ventaja decisiva: como la lógica se compila y se prueba en la PC **sin la placa**, se puede avanzar el cerebro del robot **antes de que el hardware exista**. La programación deja de estar bloqueada esperando el montaje.
- **Documentación y debugging asistidos** (§4.5 y §4.6) comprimen el **cierre**: documentar al nivel que premia la rúbrica y diagnosticar fallas deja de ser el cuello de botella final.

Sumadas, estas compresiones atacan **todas** las etapas del ciclo a la vez. Esa es la razón por la que un objetivo que suena agresivo —30 días— nos parece alcanzable: no depende de trabajar más horas, sino de que **ninguna etapa quede como cuello de botella**.

**El caveat honesto (porque este documento no sobrevende).** Este año **no** logramos el ciclo de 30 días: fue más lento. Pero la causa **no fue el método** —fue la **provisión de materiales**. Importar componentes en Argentina es lento e impredecible: la aduana fracciona los pedidos y las piezas (motores, sensores, cámaras, conectores) llegan de a poco a lo largo de **semanas**. Buena parte de nuestro calendario lo fijó la logística de importación, no el ritmo de ingeniería.

Y acá está la conexión más importante de todo el documento: **nuestra disciplina de host-testing nació justamente de esa lentitud de materiales.** Separar la lógica pura del hardware y compilarla/testearla en la PC con `g++` (§4.3, §4.4) fue, antes que una buena práctica de software, una **respuesta directa** a no tener el hardware completo: nos permitió construir y validar el cerebro del robot mientras los componentes seguían en tránsito. El cuello de botella físico nos **empujó** a la práctica que más acelera el cuello de botella de software. La meta de los 30 días sigue en pie; lo que aprendimos es que, cuando el método ya no es el límite, el límite se corre a la cadena de suministro — y hasta para eso, la IA nos dejó seguir avanzando.

---

## 2. Qué es "VIBE" (nuestra metodología)

Llamamos **VIBE** internamente a nuestra forma de trabajar asistidos por IA. No es "vibe coding" en el sentido peyorativo de "tirar prompts y rezar": para nosotros VIBE es un **proceso disciplinado** con cuatro frentes de *creación* (PCB, 3D, código, gestión) y tres prácticas *transversales* (testing, documentación, debugging), todos sujetos al principio del §1.2.

La unidad de trabajo típica no es "un prompt", sino un **flujo orquestado**:
1. **Encuadre humano del problema** (qué, restricciones, criterio de aceptación).
2. **Exploración/propuesta de la IA** (a veces varios agentes en paralelo, cada uno dueño de una parte).
3. **Verificación central** (gate de tests + revisión humana del diff).
4. **Validación física** cuando aplica (banco).
5. **Commit trazable** y registro en el *journal*.

Una característica clave de nuestra orquestación: cuando lanzamos **varios agentes de IA en paralelo**, cada uno es **dueño único de un archivo** (para que no se pisen) y hay una **verificación central** que corre el gate antes de aceptar nada. Es el mismo principio de *ownership* y *CI* del desarrollo de software profesional.

---

## 3. Marco de gobernanza: cómo garantizamos correctitud y autoría

Esta sección es la que sostiene todo lo demás. Sin esto, "usamos IA" sería una excusa; con esto, es una **metodología auditable**.

| Control | Qué es | Por qué nos protege |
|---|---|---|
| **Gate de tests host-native** | Una suite de **858 tests / 61 suites / 0 fallos** (cifra viva, medida 2026-06-14 con `scripts/run-host-tests.sh`) que compila la lógica pura en la PC con `g++`. Tiene que estar en **verde** antes de cualquier merge. | Es un **árbitro objetivo**: si la IA (o un humano) rompe algo, el gate lo caza. La correctitud no depende de confiar en la IA. |
| **Fallback byte-idéntico + feature flags** | Cada capacidad nueva entra como **módulo puro gateado** (`#ifdef`), apagado por defecto, con fallback **byte-idéntico** al binario anterior. | El binario de competencia **no cambia** hasta que validamos. Una idea de la IA no puede "colarse" al robot sin aprobación. |
| **Validación de banco** | El comportamiento físico (motores, sensores, visión, frenado de borde) se prueba con el robot. | La IA no valida hardware; **nosotros sí**. La realidad es el juez final. |
| **Ownership por archivo** | En trabajo paralelo, un solo agente (o persona) edita cada archivo. | Evita conflictos y mantiene la responsabilidad clara. |
| **Trazabilidad** | Commits asistidos firmados con `Co-Authored-By`; *journal* de ingeniería con cada sesión e iteración medida. | Cualquiera puede auditar **qué se hizo, cuándo y con qué ayuda**. Transparencia total. |
| **Contratos de datos byte-a-byte** | Las placas se comunican por contratos de bytes documentados y testeados (golden tests de offsets). | La IA puede proponer cambios de protocolo, pero un cambio que rompa el contrato **falla un test golden** y no pasa. |

**Conclusión de gobernanza:** en este proyecto, *aceptar* un aporte de IA **cuesta más esfuerzo humano** que generarlo. Esa asimetría es a propósito: garantiza que entendemos y respondemos por todo lo que entregamos.

---

## 4. Áreas de aplicación (en profundidad)

### 4.1 VIBE PCB Design — diseño electrónico asistido por IA (EasyEDA vía MCP)

**Qué hacemos.** Diseñamos las PCB del robot (placas **TOP** y **DOWN** propias; **COMM**, fork del módulo oficial RCJ) en **EasyEDA**, donde un agente de IA propone y edita el **esquemático y el ruteo** a través de un **servidor MCP** que expone EasyEDA como herramienta programable. El humano **valida cada cambio** del esquemático/PCB.

**Cómo, en detalle.**
- El MCP traduce intención de diseño ("conectá este mux a estos pines", "ruteá esta net", "generá la tabla de pinout desde el esquemático") en operaciones concretas sobre el proyecto EasyEDA.
- La IA es especialmente útil para tareas **tediosas y propensas a error humano**: mapear el *scrambling* de pines de los multiplexores del anillo de línea, generar la tabla de pinout completa desde el JSON del esquemático/PCB (con un script reproducible), revisar netlists y detectar nets sin rutear.
- El humano corre **DRC/ERC**, valida footprints contra datasheets, y decide los compromisos de layout (longitud de pistas, masas, separación).

**Por qué se justifica.** El diseño de PCB tiene una capa enorme de **trabajo mecánico verificable** (¿esta net llega a este pin?, ¿coincide el pinout con el firmware?) donde el error humano es caro (una placa mal ruteada = semanas de retraso y plata). La IA reduce ese error y libera tiempo para las decisiones de ingeniería reales. **La validación final (DRC/ERC, fabricación, bring-up en banco) es 100% humana.**

**Evidencia en el repo.** Proyectos EasyEDA completos de TOP y DOWN (esquemático + PCB + Gerbers + BOM + Pick&Place); el `MAPA-CONEXIONES-3-PLACAS.md` consolidado; scripts de extracción de pinout desde el esquemático. El estado honesto: quedan tareas de banco (rutear nets pendientes en DOWN, DRC/ERC final) que son humanas.

### 4.2 VIBE 3D Design — diseño mecánico asistido por IA (Fusion 360 vía MCP)

**Qué hacemos.** Modelamos piezas mecánicas (chasis, soportes, jigs) en **Autodesk Fusion 360** comandado por un agente vía un **servidor MCP local** (corre en `localhost`), con un catálogo de operaciones paramétricas (sketches, extrude/revolve/loft/sweep, patrones, fillet/shell/boolean, exportar STL/STEP).

**Cómo, en detalle.** El agente traduce una descripción ("una caja hueca de tales medidas con agujeros para tornillos M3 en estas posiciones") en una secuencia de operaciones de Fusion. Trabajamos en **unidades paramétricas** (1 unidad = 1 cm) y exportamos a **STL/STEP** para imprimir o fabricar.

**Por qué se justifica.** El modelado CAD repetitivo (patrones, agujeros, paredes uniformes) es ideal para asistencia; la IA acelera el *primer borrador* de una pieza que después el humano ajusta para fabricación (tolerancias de impresión, orientación, soportes).

**Estado honesto.** Este frente está **recién empezando** — lo declaramos así. La mecánica final, las tolerancias de impresión 3D, el ensamblaje y la validación física son humanas. Lo documentamos como una **línea emergente** de nuestro flujo, no como algo maduro.

### 4.3 VIBE Coding — firmware C++ asistido por IA

**Qué hacemos.** El firmware de las **3 placas Teensy** (TOP/CENTRAL/DOWN) más la **COMM** se desarrolla con asistencia de IA, pero bajo una **arquitectura específicamente diseñada para que la IA sea segura y verificable**:

- **Lógica pura separada del hardware.** Toda la lógica de decisión vive en **módulos C++ puros** (`src/shared/`, sin Arduino, sin `Serial`/`Wire`/`analogWrite`); el *glue* de Arduino es delgado. Esto permite **compilar y testear la lógica en la PC con `g++`, sin la placa**.
- **Capacidad nueva = módulo puro + test + flag + fallback byte-idéntico.** Toda feature asistida por IA entra apagada por defecto, con su test, y sin cambiar el binario de competencia hasta validar.
- **Orquestación multi-agente con dueño único por archivo + verificación central.** Para tareas grandes lanzamos varios agentes en paralelo (cada uno dueño de un archivo) y un paso central corre el gate antes de aceptar.

**Por qué se justifica.** La IA escribe código rápido pero puede equivocarse; nuestra arquitectura convierte ese riesgo en algo **detectable y acotado**: si la lógica está mal, **un test host la caza en segundos**; si algo no se validó, **está apagado por un flag**. La IA nos da velocidad; la arquitectura nos da seguridad.

**Evidencia en el repo.** Firmware C++17 de 3 placas, **834 tests host-native / 60 suites / 0 fallos** (cifra viva), módulos puros como `ball_predict`, `kinematics`, `localization`, `pids`, `imu_fusion`, `drive_straight`; auditorías de confiabilidad implementadas como *batches* gateados y byte-idénticos; commits con `Co-Authored-By`.

### 4.4 Testing asistido por IA

**Qué hacemos.** La IA nos ayuda a **construir y hacer crecer la red de tests** que después es nuestro árbitro objetivo. No es "la IA dice que anda": es "la IA escribió tests que **prueban** que anda, y los tests corren solos".

**Cómo, en detalle.**
- **Suite host-native creciente y trazable** (180 → … → 658 → 834 casos), corrida con un único script offline.
- **Golden tests de contrato byte-a-byte**: para los datos que cruzan placas por bytes crudos (el `WorldSnapshot` de 31 B, `LineStatusV2` de 16 B, el paquete cámara→TOP), tests que **fijan el offset de cada campo**, de modo que un reordenamiento accidental **rompe el test** aunque el tamaño no cambie.
- **Verificación adversarial**: cuando un agente afirma haber arreglado/encontrado algo, otro agente (o un *mutation test*) **intenta refutarlo** — por ejemplo, alteramos a propósito un offset y confirmamos que el test golden **falla**, probando que el test no pasa "en vacío".
- **Cobertura como red contra regresiones**: cada bug que encontramos se convierte en un test que "nunca vuelve a pasar desapercibido".

**Por qué se justifica.** El testing es **exactamente** el contrapeso del riesgo de la IA. Cuanto más código asistido, **más** tests automáticos queremos. La IA hace barato lo que antes era caro (escribir muchos tests), y eso **eleva** la calidad, no la baja.

### 4.5 Documentación asistida por IA

**Qué hacemos.** Usamos IA para producir y mantener **documentación técnica densa y consistente**: el TDP, el póster, el guion de video, la preparación de entrevista, el BOM, los **contratos de datos byte-a-byte**, el *journal* de ingeniería, los runbooks de banco, y este mismo documento.

**Cómo, en detalle.**
- La IA mantiene la **consistencia transversal** (que el mismo número/dato aparezca igual en todos los entregables) — algo donde el error humano es altísimo. Incluso construimos un **script de "cifra viva"** que re-mide los tests y propaga el número con su **fecha y hora de medición** a todos los documentos, para que **nunca quede una cifra desfasada** (un riesgo real frente a un juez que corre el test y ve otro número).
- La IA traduce los entregables al inglés (requisito de rúbrica) preservando datos y estructura; la corrección final la revisamos nosotros.
- El humano aporta el **contenido de verdad** (decisiones de diseño, datos de banco, identidad del equipo) y **valida** que la doc no "sobrevenda".

**Por qué se justifica.** Documentar bien es parte del puntaje (TDP, póster) y de la **replicabilidad** (que otro equipo pueda copiar nuestra metodología). La IA hace que documentar deje de ser el cuello de botella y nos deja documentar **al nivel de detalle que la rúbrica premia**.

### 4.6 Debugging asistido por IA

**Qué hacemos.** Usamos IA para **diagnosticar** problemas: leer trazas, cruzar síntomas con el código, generar hipótesis, y proponer fixes mínimos y quirúrgicos — siempre bajo verificación.

**Cómo, en detalle.**
- **Auditorías sistemáticas**: corrimos auditorías de confiabilidad tipo "coach campeón" (decenas de hallazgos, verificados adversarialmente y clasificados por severidad) que destaparon bugs latentes y deuda — cada hallazgo verificado contra el código real antes de aceptarlo.
- **Diagnóstico de fallas de banco**: la IA nos ayudó a entender modos de falla reales y sutiles, por ejemplo: el **yaw del BNO que se congela** por contención I²C con los ToF a cierto clock; el **brownout del riel de 3,3 V** de los OTOS que se confunde con "otro chip" por una dirección I²C rara; el **árbitro por nivel de GPIO** (no UART). En todos, la IA propuso la hipótesis y **el banco la confirmó o refutó**.
- **Fixes con disciplina**: todo fix de debugging entra como cambio mínimo, gateado o byte-idéntico, con su test, y pasa el gate.

**Por qué se justifica.** El debugging es donde la IA es más útil **y** donde más fácil es engañarse: por eso lo atamos a **verificación adversarial** (¿el bug es real? ¿el fix realmente lo arregla? ¿rompe algo el gate?). Una hipótesis de la IA **no es una conclusión** hasta que el código o el banco la confirman.

### 4.7 Futuro: reconocimiento de imágenes con YOLO (IA en la visión)

**Qué planeamos.** Hoy la visión detecta la pelota y los arcos con **umbrales de color en espacio LAB** (clásico, rápido, pero sensible a la iluminación — por eso recalibrar para la luz de Incheon es nuestro bloqueante #1). El **siguiente paso natural** es **reconocimiento por redes neuronales (YOLO)** corriendo **en la propia cámara**: usamos **OpenMV Cam N6**, que integra un **NPU (Neural-ART)** pensado para inferencia de redes neuronales en el borde.

**Por qué tiene sentido (y por qué es IA "de verdad").** YOLO es una **red neuronal convolucional** de detección de objetos: es IA en el sentido más estricto (un modelo entrenado que infiere). Frente a los umbrales de color, una red:
- es **robusta a cambios de iluminación** (el problema que más nos pega hoy),
- puede distinguir **pelota vs. reflejos vs. ruido** y clasificar arcos por color/forma con más tolerancia,
- corre **on-device** en el NPU del N6, sin depender de una PC.

**Cómo lo encararíamos (roadmap honesto).** Capturar y etiquetar un dataset de la cancha/pelota reales → entrenar/afinar un modelo YOLO chico → cuantizarlo y desplegarlo en el NPU del N6 → mantener el detector LAB como **fallback** (coherente con nuestra disciplina de *fallback* siempre presente). **Esto es trabajo futuro**, no algo entregado hoy; lo declaramos como roadmap.

**Por qué se justifica.** Es la evolución coherente de nuestra visión: pasar de heurística de color a **percepción aprendida**, aprovechando hardware que ya tenemos (el NPU del N6). Y encaja con la filosofía del proyecto: una capacidad nueva, validada en banco, con fallback.

---

## 5. Qué NO hace la IA en este proyecto (límites explícitos)

Para que quede sin ambigüedad frente a un juez:
- **No decide qué se sube al robot.** Eso lo decide el equipo, y queda detrás de flags y del gate.
- **No valida hardware.** Motores, frenado de borde, calibración de cámara, set-points de los reguladores, ángulos de rueda: todo se mide/valida en banco, por personas.
- **No mide performance real.** Cargas de CPU y latencias son objetivos de diseño hasta medirlas con instrumento.
- **No reemplaza el entendimiento.** Podemos explicar, modificar y defender cada subsistema; si no lo entendiéramos, no lo entregaríamos.
- **No es infalible.** Por eso existe toda la §3 (gobernanza): asumimos que la IA se puede equivocar y construimos el proceso para **cazar** esos errores.

---

## 6. Stack de IA y herramientas usadas (inventario para reproducibilidad)

| Herramienta | Rol | Quién valida |
|---|---|---|
| **Claude** (vía *Claude Code*) | Asistente principal: código, docs, debugging, orquestación de agentes | Equipo (revisión de diff + gate) |
| **Servidor MCP de EasyEDA** | Operar el diseño de PCB (esquemático/ruteo/pinout) | Equipo (DRC/ERC + fabricación) |
| **Servidor MCP de Fusion 360** (local) | Operar el modelado 3D paramétrico | Equipo (tolerancias + ensamble) |
| **Suite de tests host-native** (`g++` + Unity, offline) | Árbitro objetivo de correctitud de la lógica | Automático (gate verde obligatorio) |
| **Orquestación multi-agente** (dueño-único-por-archivo + verificación central) | Paralelizar tareas grandes con seguridad | Equipo (verificación central) |
| **OpenMV Cam N6 + NPU Neural-ART** | (Futuro) inferencia YOLO on-device para visión | Equipo (dataset + validación en cancha) |

Todo el proyecto es **open-source (MIT)** y los flujos son **reproducibles**: otro equipo puede ver no solo *qué* hicimos sino *cómo* lo hicimos con IA.

---

## 7. Valor educativo (por qué esto nos hace mejores ingenieros, no peores)

- **Aprendimos a especificar problemas con precisión** (un buen prompt es una buena especificación de ingeniería).
- **Aprendimos a verificar en vez de confiar**: diseñamos gates, tests golden y verificación adversarial — competencias de QA reales.
- **Aprendimos arquitectura defensiva**: separar lógica pura de hardware, *feature flags*, fallback byte-idéntico — patrones de la industria.
- **Aprendimos trazabilidad y trabajo en equipo**: ownership por archivo, coautoría, *journal* de ingeniería.
- **Aprendimos los límites de la herramienta**: sabemos qué NO delegar (validación física, decisiones de diseño, medición).

La IA no nos ahorró aprender: nos **obligó a aprender** un nivel más alto (verificación, arquitectura, gobernanza) para usarla con responsabilidad.

---

## 8. Declaración de autoría y transparencia (para los jueces de RoboCupJunior)

- **El diseño, las decisiones de ingeniería, la validación en hardware y la estrategia de juego son del equipo.**
- **La IA se usó como herramienta de aceleración** en los siete frentes descritos, siempre con **revisión humana y verificación objetiva**.
- **Todo lo asistido por IA está trazado** (commits con coautoría, *journal* de ingeniería) y es **reproducible** (repo público MIT).
- **Podemos explicar, modificar y defender cada subsistema** del robot en vivo.
- **No sobrevendemos**: declaramos abiertamente lo que está validado, lo que está *code-complete* pero pendiente de banco, y lo que es roadmap (p. ej. YOLO).

### Preguntas frecuentes que anticipamos
- *"¿La IA hizo el robot por ustedes?"* → No. La IA aceleró tareas que entendemos; nosotros decidimos, validamos en banco y respondemos por todo. La prueba: podemos explicar y modificar cualquier parte, y todo está atado a tests/banco.
- *"¿Cómo sabemos que el código funciona si lo escribió una IA?"* → Porque **no confiamos en la IA, confiamos en los tests**: 834 tests host-native en verde, golden de contratos, y validación de banco. El gate caza errores vengan de quien vengan.
- *"¿No es hacer trampa?"* → Usar herramientas modernas declarándolas y entendiéndolas es ingeniería honesta. La trampa sería esconderlo o no poder explicarlo. Este documento es lo contrario.

---

## 9. Roadmap de uso de IA

1. **Visión por IA (YOLO en el NPU del N6)** — de umbrales LAB a percepción aprendida, con fallback LAB.
2. **Maduración de VIBE 3D Design** — del primer borrador asistido a piezas finales para fabricación.
3. **Single "robot definition" asistido** — centralizar la configuración por-robot en un único archivo (en curso) para escalar a múltiples robots.
4. **Más verificación adversarial automatizada** — paneles de jueces-IA y *mutation testing* sistemático sobre los módulos críticos.

---

*Documento elaborado como parte de los entregables de RoboCupJunior Soccer Open 2026. Versión de trabajo en español; la versión inglesa para entrega vive en `docs/competencia/en/USO-DE-IA.md`.*
