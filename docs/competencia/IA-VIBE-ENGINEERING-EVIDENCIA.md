---
title: "IA, Vibe Engineering y Fast Robotics en RoboCup — evidencia, reglas y postura del equipo"
team: "IITA Low Battery Messi"
org: "Instituto de Innovación y Tecnología Aplicada (IITA) / Fundación Innovar — Salta, Argentina"
date: 2026-06-13
status: documento vivo (versión de trabajo ES — traducir a EN para entrega)
proposito: "Responder con EVIDENCIA VERIFICABLE la pregunta '¿usar IA es trampa?' y argumentar por qué las competencias deberían PROMOVER el uso disciplinado de IA en ingeniería, no prohibirlo. Companion de USO-DE-IA.md."
metodo: "Investigación con fan-out de búsquedas web + fetch de fuentes primarias + verificación adversarial de cada afirmación (3 votos). Solo se citan fuentes que se pudieron abrir y verificar; lo no verificado se marca como tesis propia del equipo."
---

> ⚠️ **VERSIÓN DE TRABAJO EN ESPAÑOL.** Para jueces de RoboCupJunior se traduce al inglés (`en/`). Companion de [`USO-DE-IA.md`](USO-DE-IA.md), que detalla CÓMO y DÓNDE usamos IA. Este documento responde la pregunta de fondo: **¿es legítimo? ¿es trampa?** — con evidencia.

# IA, Vibe Engineering y Fast Robotics en RoboCup — evidencia y postura

## 0. Resumen ejecutivo (TL;DR para el juez)

Tuvimos una preocupación honesta: que alguien viera nuestro uso intensivo de IA como "trampa". Investigamos el tema con seriedad y la conclusión, sostenida por **fuentes primarias verificables**, es clara:

> **El uso DISCIPLINADO de IA en ingeniería no es trampa: es _compatible_ con lo que las reglas de RoboCupJunior permiten (por inferencia razonable — las reglas no nombran la IA, pero permiten recursos externos con atribución), está _en línea con el espíritu_ de la misión de RoboCup, y coincide con cómo la evidencia 2025-2026 describe la nueva forma de trabajar — siempre que el equipo retenga la propiedad explicativa de su robot (poder explicar CÓMO funciona).**

Dos planos sostienen esto:

1. **Plano normativo (reglas).** Las reglas oficiales de RoboCupJunior Soccer 2026 y las General Rules **no mencionan ni prohíben** IA, machine learning, LLMs ni "vibe coding". Permiten **explícitamente** código y apoyo externo (mentores, docentes, comunidades de internet) con dos condiciones: **acreditar** a los creadores/licencias y **priorizar el aprendizaje** (no importar "soluciones completas de otros"). La prueba de legitimidad que las reglas fijan no es *qué herramienta usaste*, sino *si podés explicar cómo funciona tu robot*. Un flujo **human-in-the-loop** cae de lleno del lado correcto.

2. **Plano de evidencia (2025-2026).** Estudios controlados muestran que la IA acelera **más a los desarrolladores junior** (el perfil de nuestro equipo), que su adopción en la industria ya es masiva, y —con honestidad— que el uso **indisciplinado** puede dañar el aprendizaje mientras que el uso orientado a **construir comprensión** lo preserva. Hay **tensión real** acá, y la reconocemos: parte de la evidencia muestra que la IA *mal* usada **daña** el aprendizaje. No la barremos bajo la alfombra — al contrario, es exactamente por eso que nuestra defensa NO es "IA siempre bien", sino "IA con el humano en el loop y verificada". La línea no es IA-sí/IA-no; es IA-para-entender vs. IA-para-no-entender.

**Lo que este documento NO hace:** no sobrevende. Distingue lo que está verificado contra fuentes primarias de lo que es **nuestra tesis** (la analogía histórica, el impacto motivacional). Y reconoce que la frontera legítimo/ilegítimo es real: hay un uso de IA que SÍ sería trampa, y explicamos por qué el nuestro no lo es.

---

## 1. Por qué escribimos esto

Somos un equipo que el año pasado ganó la **primera edición** del nacional de RoboCupJunior Soccer en Argentina con un robot **básico** (una cámara, tres sensores de luz). Soccer era una categoría **recién estrenada** en el país, y nosotros veníamos de otras categorías de RoboCupJunior (Rescue Line) — así que entramos a Soccer como una liga nueva para nosotros y ganamos su primera edición con ese robot básico. Este año adoptamos un flujo de trabajo asistido por IA —lo llamamos **VIBE** (ver `USO-DE-IA.md`)— y lo usamos para **transformar ese robot básico en uno profesional**: tres placas, fusión de sensores, 658 tests automáticos. **No empezamos de cero: evolucionamos lo que ya ganaba.** Y no hacíamos *vibe coding* antes de ganar el nacional — lo incorporamos *después*, como un experimento deliberado de **vibe engineering** para hacer **fast robotics** y, de paso, subirnos rápido a una categoría (Soccer) en la que no teníamos experiencia.

Eso nos dejó una pregunta incómoda y honesta: **¿un juez podría pensar que esto es trampa?** En vez de esconderlo, lo investigamos. Este documento es el resultado.

---

## 2. Qué dicen las reglas de RoboCup (plano normativo)

> Todo lo de esta sección está verificado contra **fuentes primarias** (las reglas oficiales en HTML y PDF), con búsqueda de texto completo. Las citas son verbatim.

### 2.1 Las reglas RCJ 2026 NO prohíben la IA

Las **RoboCupJunior Soccer Rules 2026** y las **General Rules** (act. 2026-02-23) **no contienen ninguna mención** a "artificial intelligence", "machine learning", "neural", "LLM", "ChatGPT", "generative" ni "vibe" — verificado por búsqueda de texto completo sobre el HTML y el PDF. **No existe una prohibición de la ingeniería asistida por IA.** Lo único que gobierna el uso de recursos externos son las cláusulas generales de código externo y prioridad de aprendizaje (§2.2–2.3).

> **Precisión honesta (no sobre-extender):** esto aplica a las **Soccer Rules y General Rules**. Otras ligas de RCJ tratan la IA — pero para **DIVULGARLA, no para prohibirla**: las reglas de **OnStage 2026** piden "clarify which third party code and/or libraries, **including the use of AI**, were used". Eso va **en línea con nuestra postura**: donde la federación toca el tema, pide **transparencia**, no abstinencia.

### 2.2 Las reglas PERMITEN explícitamente el apoyo y el código externo

Texto verbatim (Soccer §1.4 "Plagiarism Guidelines" / General Rules):

> *"External Code Use: Teams are allowed to use external code but must credit the original creators."*
> *"Always pay attention to licensing rules."*

Y sobre el apoyo de personas y comunidades (Soccer §1.5 / General Rules):

> *"Support from other teams, mentors, teachers, parents, sponsors, internet communities etc. is a core part of how teams learn and grow."*

Las reglas son **tool-permissive**: permiten recursos externos con dos condiciones (acreditar + licenciar). No enumeran ni restringen *tipos de herramienta*.

> **Matiz interpretativo (lo decimos nosotros, no la regla):** las reglas nunca nombran "IA". Que "external code" cubra el código asistido por IA es una **extensión razonable por analogía**, corroborada por cómo la federación trata la IA en OnStage (divulgar, no prohibir). Lo presentamos como **argumento**, no como cita literal.

### 2.3 La frontera legítimo/ilegítimo: un test de SUSTANCIA, no de herramienta

Acá está el corazón. Las reglas fijan la línea no por *qué usaste* sino por *cuánto aprendiste y si podés explicarlo*. Verbatim:

> *"Teams should prioritize learning and not use complete solutions from others."* (Learning Priority)
> *"To ensure fair competition and maximize learning it is required that none of the support they receive does the work of competing for the team."* (cláusula de sustancia)
> *"A good indication is the team's ability to explain not only what their robots' components do but also how they do it."* — la regla lo presenta como indicación de legitimidad.

Es decir: **la trampa no es usar una herramienta poderosa; la trampa es entregar lo que no entendés.** El test es poder explicar el CÓMO.

> **Matiz honesto (de un verificador):** "the team's ability to explain" es *"a good indication"*, una indicación primaria usada en el juzgamiento — no el único mecanismo de enforcement (también existen la regla de no recibir ayuda externa *durante* la competencia, la conducta, y el crédito/licencia). Lo citamos como **la indicación primaria**, que es lo que es.

> **Otra precisión (para no confundir categorías):** foros del comité de **RCJ Rescue** sí restringen herramientas que **ejecutan autónomamente la TAREA de competencia** (cámaras con IA que siguen la línea, OCR pre-hecho). Eso apunta a herramientas que *compiten por el robot en la cancha*, **no** a herramientas usadas para **construir y programar** el robot — que es de lo que habla este documento. Son dos cosas distintas y las mantenemos separadas.

### 2.4 La misión de RoboCup es PROMOVER la investigación en IA

De la fuente primaria (www.robocup.org/objective), corroborado palabra por palabra por RoboCup Asia-Pacific:

> *"use RoboCup as a vehicle to promote robotics and AI research, by offering a publicly appealing, but formidable challenge."*

Prohibir que los estudiantes **aprendan a usar IA** estaría en tensión con el **espíritu** de esa misión. (Matiz honesto, que no escondemos: la misión habla de la IA como **campo de investigación** —hacer robots inteligentes, con el norte del humanoide 2050—, no literalmente del **uso de herramientas de IA** para construirlos. Saltar de una a la otra es una extensión por analogía, igual que con "external code"; la presentamos como argumento, no como deducción directa.)

---

## 3. Qué dice la evidencia 2025-2026 (plano empírico)

> Estudios con muestras chicas o alcances específicos. Los citamos **con su n, su intervalo de confianza y su matiz de significancia** — porque la honestidad es parte del argumento, no un adorno.

### 3.1 La IA acelera MÁS a los junior (= nuestro perfil)

Un análisis agrupado de **3 RCTs reales** (Microsoft, Accenture, una Fortune 100; **4.867 desarrolladores**) halló un **+26,08 % (SE 10,3 %)** de tareas completadas con GitHub Copilot, y —clave— el beneficio fue significativo para **recién contratados y posiciones junior**, no para los senior de larga antigüedad. Junior +21–40 % de output vs. senior +7–16 %.
*Fuente:* MIT/Princeton/UPenn, **publicado en Management Science (2025)**, DOI 10.1287/mnsc.2025.00535 (fuente canónica = el DOI; el PDF enlazado en Referencias es el preprint).
> **Caveat honesto:** solo el experimento de Microsoft alcanzó significancia individual; Accenture y la empresa anónima apuntaron en la misma dirección pero la significancia **emerge al agrupar**. Citamos el 26 % *pooled* con ese matiz. Encaja con nuestro caso: somos estudiantes/junior, el grupo que **más** se beneficia.

### 3.2 La adopción ya es masiva en la industria

La encuesta **McKinsey "State of AI 2025"** (nov-2025; n=1.993 en 105 países) halló que **62 % de las organizaciones** al menos experimentan con agentes de IA (23 % escalando + 39 % experimentando).
> **Caveat:** la propia McKinsey señala una *"scaling gap"* — no más del ~10 % escala agentes en una función dada. "Experimentar" ≠ despliegue masivo en producción. Lo emparejamos con ese matiz.

### 3.3 Disciplinado vs. indisciplinado: la tensión que tomamos en serio

Un RCT de **Anthropic (feb-2026, n=52** ingenieros mayormente junior aprendiendo una librería nueva): el grupo con IA promedió **50 % vs. 67 %** del grupo que codeó a mano en un quiz **sin** IA — casi dos notas de diferencia (Cohen's d=0,738; p=0,01). **PERO** quienes sacaron buena nota *"used AI assistance not just to produce code but to build comprehension while doing so — whether by asking follow-up questions, requesting explanations, or posing conceptual questions."*
*Fuente:* Anthropic — investigación sobre IA y aprendizaje de programación (anthropic.com/research/AI-assistance-coding-skills). *(El preprint arXiv asociado: abrir el enlace y confirmar el ID exacto antes de citarlo — no lo fijamos de memoria.)*
> Este estudio en parte juega **EN CONTRA** del uso ingenuo de IA —el grupo con IA sacó peor nota en el quiz— y aun así sostiene nuestra distinción: lo que separó a los que aprendieron fue **usar la IA para construir comprensión** (preguntar, pedir explicaciones), no la herramienta en sí. Por eso insistimos en la disciplina, no en prohibir. **Caveat del propio estudio:** midió comprensión *inmediata*, no a largo plazo; n=52 es chico.

### 3.4 La IA no es magia uniforme (y por eso hay que medir)

Un RCT de **METR (inicios 2025, n=16)** halló que dar IA a devs **open-source muy expertos** trabajando en **sus propios repos maduros** (22k+ estrellas) **aumentó** el tiempo de tarea un **19 %** (los frenó), con una brecha llamativa entre percepción y medición (predijeron 24 % más rápido, midieron 19 % más lento).
*Fuente:* arXiv 2507.09089.
> **Caveats (a citar con precisión):** el +19 % es estimación puntual con **IC 95 % de −26 % a +9 %**, n=16; alcance **estrecho** (expertos en repos maduros, herramientas de inicios de 2025); un follow-up (feb-2026, n=57) **no reprodujo** el slowdown. La lección honesta: la IA ayuda **más donde hay menos expertise previo** (junior, proyectos en crecimiento) y los speedups auto-reportados no son confiables → **hay que medir.** Nuestro robot pasó de básico a profesional: es justo el territorio donde el junior/proyecto-en-crecimiento más gana, no el del experto-en-repo-maduro que se frena. Y por eso medimos todo con **658 tests automáticos**, no a ojo.

### 3.5 Contexto institucional sobre IA en educación

Marcos y reportes de organismos internacionales encuadran el aprender-a-usar-IA como una competencia del siglo XXI (no como un atajo): el **AI Competency Framework** de UNESCO (para estudiantes y para docentes), el reporte **"Empowering learners for the age of AI"** de la OECD, y el **Future of Jobs Report 2025** del Foro Económico Mundial (upskilling urgente). *(Fuentes primarias listadas en Referencias; los citamos como contexto de política educativa, no como medición cuantitativa.)*

---

## 4. La línea legítimo/ilegítimo — dónde caemos nosotros

La frontera es real y la respetamos. Resumida:

| Trampa (uso ilegítimo) | Educación (lo que hacemos) |
|---|---|
| Pegar la salida sin entenderla | Acelerar tareas que entendemos |
| No poder explicar lo entregado | Poder explicar, modificar y defender cada parte (el test RCJ del "CÓMO") |
| No poder reproducirlo | Repo público MIT, flujo reproducible |
| Creerle a la IA | Verificar contra **658 tests** + validación de banco |
| Esconder que se usó | Declararlo (`USO-DE-IA.md` + este documento) |
| Herramienta que **compite por** el robot en la cancha | IA que nos ayuda a **construir y programar** el robot fuera de la cancha |

Nuestra gobernanza (detallada en `USO-DE-IA.md`): **la IA propone, el equipo dispone.** Nada llega al robot sin revisión humana del diff, gate de tests en verde, o validación en banco. *Aceptar* un aporte de IA nos cuesta **más** esfuerzo humano que generarlo — y esa asimetría es a propósito: garantiza que entendemos lo que entregamos. Eso es exactamente lo que el test RCJ del "explicar cómo" exige.

---

## 5. Nuestra tesis (lo decimos como opinión, no como hallazgo verificado)

> **Honestidad metodológica:** las dos ideas de esta sección **no** se sostuvieron con fuentes primarias que sobrevivieran nuestra verificación adversarial. Las presentamos como **tesis y opinión del equipo**, claramente separadas de las secciones 2 y 3.

**La IA hoy es como la robótica educativa hace 20 años.** RoboCup nació en 1997 como un desafío "públicamente atractivo pero formidable" para empujar la investigación; meter jóvenes en STEM a través de una competencia nueva fue parte de su impacto. Creemos —es nuestra lectura— que la IA generativa está hoy en ese mismo momento: una tecnología nueva, productiva y útil que las nuevas generaciones quieren aprender. Una competencia que fue **pionera** en lo nuevo corre el riesgo, si no se sube a esto, de dejar de motivar a las generaciones que buscan justamente lo nuevo, lo productivo, lo útil.

**El impacto motivacional importa.** Para nosotros, aprender a dirigir y verificar una IA fue de las experiencias más motivadoras del proyecto: nos obligó a un nivel más alto (especificar problemas con precisión, diseñar tests, gobernar el proceso) y nos dejó **construir algo profesional en semanas**, no en años. Esa sensación de "una idea puede ser un robot que anda, rápido" es combustible. Creemos que las competencias deberían capitalizarla, no temerle.

*(No tenemos —y lo decimos— una cita primaria verificada que cuantifique esta analogía o este efecto motivacional específico en RoboCup. Es nuestra experiencia y nuestra lectura.)*

---

## 6. Recomendación

Para las categorías y los comités, con el mayor respeto y como aporte a la comunidad:

1. **Promover la transparencia, no la prohibición.** El modelo de OnStage 2026 (divulgar el uso de IA) es el camino correcto y debería generalizarse: pedir que los equipos **declaren** cómo usaron IA, no que finjan no usarla.
2. **Mantener y reforzar el test del "CÓMO".** La mejor defensa contra el uso ilegítimo ya existe en las reglas: que el equipo pueda explicar cómo funciona su robot. Es tecnología-neutral y a prueba de futuro.
3. **Distinguir construir-el-robot de competir-por-el-robot.** Restringir herramientas que ejecutan la tarea en cancha es razonable; restringir herramientas que ayudan a diseñar/programar fuera de cancha sería incoherente con la misión de promover la investigación en IA.
4. **Capitalizar la motivación.** Tratar el uso disciplinado de IA como una competencia STEM del siglo XXI a enseñar, igual que se enseñó robótica hace 20 años.

---

## 7. Referencias (verificadas — fetcheadas y citadas)

**Reglas y postura oficial (fuentes primarias):**
- RoboCupJunior Soccer Rules 2026 (draft, "as of 2026-01-21): https://robocup-junior.github.io/soccer-rules/2026-soccer-draft-rules/rules.html · PDF: …/rules.pdf
- RoboCupJunior General Rules (act. 2026-02-23): https://robocup-junior.github.io/general-rules/main/general-rules.html · PDF: …/general-rules.pdf
- RoboCup — Objective/Mission: https://www.robocup.org/objective · RoboCup Asia-Pacific: https://robocupap.org/objective/
- RoboCup — A Brief History: https://www.robocup.org/a_brief_history_of_robocup

**Evidencia de productividad y adopción (fuentes primarias):**
- "The Effects of Generative AI on High-Skilled Work" (MIT/Princeton/UPenn; **publicado en Management Science 2025**, DOI 10.1287/mnsc.2025.00535 = fuente canónica; preprint PDF: https://economics.mit.edu/sites/default/files/inline-files/draft_copilot_experiments.pdf)
- McKinsey — "The State of AI in 2025: Agents, Innovation, and Transformation": https://www.mckinsey.com/capabilities/quantumblack/our-insights/the-state-of-ai
- Anthropic — investigación sobre el impacto de la IA en el aprendizaje de programación (2026, n=52): https://www.anthropic.com/research/AI-assistance-coding-skills *(confirmar el ID del preprint arXiv asociado abriendo la página antes de citarlo)*
- METR — "Measuring the Impact of Early-2025 AI on Experienced OSS Developer Productivity" (n=16): https://arxiv.org/abs/2507.09089

**IA en educación (fuentes primarias — contexto de política):**
- UNESCO AI Competency Framework for Students: https://www.unesco.org/en/articles/ai-competency-framework-students · for Teachers: …/ai-competency-framework-teachers
- OECD — "Empowering learners for the age of AI": https://www.oecd.org/en/publications/empowering-learners-for-the-age-of-ai_29eee1cb-en.html
- World Economic Forum — Future of Jobs Report 2025: https://www.weforum.org/press/2025/01/future-of-jobs-report-2025-78-million-new-job-opportunities-by-2030-but-urgent-upskilling-needed-to-prepare-workforces/

**Debate comunitario y definición (foros / secundarias — calidad marcada):**
- ChiefDelphi (FRC robotics) — "How is your team using AI/LLMs for robot code in 2026?": https://www.chiefdelphi.com/t/how-is-your-team-using-ai-llms-for-robot-code-in-2026/513448 *(foro)*
- ChiefDelphi — "Lowering the barrier to code: AI's role in FRC robotics": https://www.chiefdelphi.com/t/lowering-the-barrier-to-code-ais-role-in-frc-robotics/503634 *(foro)*
- Simon Willison — "Vibe engineering": https://simonwillison.net/2025/Oct/7/vibe-engineering/ *(blog)*
- MIT Technology Review — "What is vibe coding, exactly?": https://www.technologyreview.com/2025/04/16/1115135/what-is-vibe-coding-exactly/ *(secundaria)*
- Sony AI — "RoboCup and Its Role in the History and Future of AI": https://ai.sony/blog/RoboCup-and-Its-Role-in-the-History-and-Future-of-AI/ *(secundaria)*
- IEEE Spectrum — RoboCup: https://spectrum.ieee.org/robocup-robot-soccer *(secundaria)*

---

## 8. Honestidad metodológica (cómo se hizo este documento)

Este documento es, él mismo, un ejemplo de lo que defiende: **uso de IA disciplinado, verificado y declarado.**

- **Método:** investigación con fan-out de búsquedas web en paralelo → fetch de fuentes → **verificación adversarial** (3 votos independientes por afirmación; se descartaba si 2 de 3 la refutaban). **Cada afirmación que CITAMOS en las secciones 2 y 3 pasó esa verificación contra fuentes primarias; lo que no la pasó, lo dejamos afuera o lo marcamos como tesis propia (§5).** El proceso descartó afirmaciones que no se sostuvieron — incluso algunas que nos convenían.
- **Regla de citas:** solo se citan fuentes que se pudieron **abrir y verificar**. Donde la conexión es una inferencia (IA ↔ "external code"), se dice. Donde la evidencia tiene límites (n chico, IC amplio, significancia solo al agrupar), se dice.
- **Lo que NO se pudo verificar** (comentarios de foros RCJ por país; la cuantificación de la analogía histórica y del efecto motivacional) se marca explícitamente como **tesis y opinión del equipo** (§5), separado de lo verificado (§2–§3).
- **Transparencia de IA:** la investigación y la redacción fueron asistidas por IA (Claude), bajo revisión y decisión humana del equipo — exactamente la gobernanza human-in-the-loop que el documento defiende y que `USO-DE-IA.md` detalla.

Que un documento *sobre* el uso responsable de IA haya sido hecho *con* uso responsable de IA, declarándolo y verificándolo, es la mejor prueba de la tesis: **la IA bien usada no reemplaza el rigor — lo exige.**

---

*Versión de trabajo en español — IITA Low Battery Messi, Salta, Argentina. Companion de `USO-DE-IA.md`. Traducir al inglés antes de entregar / compartir con jueces.*
