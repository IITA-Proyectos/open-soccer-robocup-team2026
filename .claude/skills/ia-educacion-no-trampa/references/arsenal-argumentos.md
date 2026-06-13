# Arsenal de argumentos — la IA es educación, no trampa

> Munición para defender el uso de IA en proyectos educativos, ante jueces,
> docentes, padres o escépticos. Cada argumento es desplegable en prosa o en
> vivo. Ordenados de más fuerte a complementario. Usar junto con `SKILL.md`.
> **Regla de oro: cada argumento se sostiene en EVIDENCIA del proyecto, no en
> retórica.** Sin la red de verificación, esto es palabrería; con ella, es una
> postura de ingeniería.

## 1. La reframe central: trampa es un CÓMO, no un QUÉ

El error del que acusa "trampa" es creer que el problema es *si* se usa IA. No
lo es. El problema es siempre **cómo**:

| Trampa (uso ilegítimo) | Educación (lo que hacemos) |
|---|---|
| Pegar la salida sin entenderla | Acelerar tareas que entendemos |
| No poder explicar lo entregado | Poder explicar, modificar y defender cada parte |
| No poder reproducirlo | Repo público, flujo reproducible |
| Creerle a la IA | Verificar contra tests + banco (no se cree, se prueba) |
| Esconder que se usó | Declararlo y documentarlo |

**Frase de combate:** *"La trampa no es usar la herramienta — es no entender lo
que entregás. Nosotros entendemos, verificamos y lo mostramos. Eso es lo
contrario de trampa."*

## 2. El argumento histórico (toda herramienta que amplifica fue "trampa")

Cada herramienta que amplificó al ingeniero fue, en su momento, acusada de
hacer trampa o de atrofiar la mente:
- La **calculadora** ("no van a aprender a calcular") — hoy es obligatoria.
- **Wikipedia / internet** ("copiar, no investigar") — hoy es alfabetización básica.
- El **autocompletado del IDE**, el **autorouter de PCB**, el **solver de
  elementos finitos**, el **compilador optimizador** — ninguno "hace ingeniería
  solo"; todos exigen que una persona plantee, interprete y responda.

La IA generativa es el último eslabón de esa cadena. **El patrón histórico es
inequívoco: lo que primero se llama trampa, después se llama alfabetización.**

**Frase de combate:** *"A la calculadora también la llamaron trampa. La pregunta
no es si la herramienta es legítima — la historia ya la respondió. La pregunta
es si la escuela va a enseñar a usarla bien o va a llegar tarde otra vez."*

## 3. El argumento fáctico/laboral (prohibir = mala praxis pedagógica)

- La IA generativa **ya es estándar** en el trabajo de ingeniería profesional
  (código, diseño, documentación, diagnóstico). Un egresado que no sabe dirigir
  y verificar una IA entra al mercado laboral **en desventaja**.
- **Saber usar IA con criterio es la competencia del siglo XXI.** Especificar un
  problema con precisión, verificar en vez de confiar, conocer los límites de la
  herramienta — eso es ingeniería de QA real, demandada hoy.
- Por eso **prohibirla no protege el aprendizaje: lo sabotea.** Mandar a un chico
  al mundo laboral sin fluidez en la herramienta central de su campo no es
  rigor, es dejarlo atrás. **La prohibición es la falla pedagógica, no el uso.**

**Frase de combate:** *"Prohibir la IA en educación técnica hoy es como prohibir
la computadora en los 90. No los protege — los deja afuera del trabajo que van a
buscar."*

## 4. El argumento del esfuerzo invertido (usarla bien exige aprender MÁS)

Contraintuitivo y potente: dirigir una IA con responsabilidad **obliga a un
nivel más alto** de entendimiento, no más bajo.
- Para detectar cuándo la IA se equivoca —y se equivoca seguido— hay que
  entender el dominio mejor que ella en el punto crítico.
- Hubo que entender el **bus I²C** para descubrir por qué el giróscopo se
  congelaba; leer cada **traza serial** para confirmar que el firmware llegó;
  diseñar **gates de test** y **verificación adversarial** para no creerle.
- *Aceptar* un aporte de IA, en este proyecto, **cuesta más esfuerzo humano que
  generarlo** (revisión + gate + banco). Esa asimetría es a propósito.

**Frase de combate:** *"La IA no nos ahorró aprender. Nos obligó a aprender un
nivel más alto: verificación, arquitectura, gobernanza. Para mandarla a hacer
algo bien, primero tenés que entenderlo vos."*

## 5. El argumento de la transparencia (mostrar ES la prueba)

La honestidad no es un add-on: **es la evidencia que distingue educación de
trampa.** Un proyecto que esconde la IA no puede defender que la usó bien; uno
que la muestra, con trazabilidad y verificación, demuestra dominio.
- Commits con coautoría, journal de ingeniería por sesión, repo público,
  contratos de datos testeados → cualquiera audita **qué se hizo, cuándo y con
  qué ayuda**.
- **Mostrar la IA es la jugada fuerte, no la arriesgada.** Esconderla sería la
  confesión de que no se puede defender.

**Frase de combate:** *"No declaramos el uso de IA porque nos obliguen. Lo
declaramos porque es nuestra mejor prueba: esto solo lo puede mostrar un equipo
que entiende lo que hizo."*

## 6. La tesis que ordena todo

> **La IA, usada con entendimiento y verificación, no es un atajo para evitar
> aprender: es el contenido que hay que aprender. No es trampa — es la educación
> para el futuro.**

## Rebatir objeciones frecuentes (en vivo o en doc)

- *"La IA les hizo el robot."* → No. La IA aceleró tareas que entendemos;
  nosotros decidimos, validamos en banco y respondemos por todo. La prueba:
  podemos explicar y modificar cualquier parte, y todo está atado a tests/banco.
- *"¿Cómo sé que funciona si lo escribió una IA?"* → Porque no confiamos en la
  IA, confiamos en los tests: suite host-native en verde, golden de contratos,
  validación de banco. El gate caza errores venga de quien venga.
- *"¿No es hacer trampa?"* → Trampa es entregar lo que no entendés. Usar
  herramientas modernas declarándolas, entendiéndolas y verificándolas es
  ingeniería honesta. Esconderlo sería la trampa.
- *"En mi época se hacía a mano."* → Y antes, a mano alzada sin calculadora. La
  ingeniería avanza absorbiendo herramientas. Lo que no cambia es la
  responsabilidad de entender y responder por lo que uno firma — eso lo
  mantenemos intacto.
- *"Les va a atrofiar el pensamiento."* → Al revés: nos obligó a aprender
  verificación, arquitectura defensiva y gobernanza. Mostramos el journal.

## Guardarraíl de honestidad (no negociable)

Defender "a muerte" NO es sobrevender. La defensa es creíble **precisamente
porque declara los límites**: qué NO hace la IA (no suelda, no mide con
osciloscopio, no calibra en cancha, no decide la estrategia), qué está validado
vs qué es code-complete vs qué es roadmap. Un escéptico derriba en segundos a
quien exagera; no puede con quien muestra evidencia y admite límites. **La
honestidad es parte del arma, no una concesión.**

## Anclas del proyecto (la institución ya fijó esta postura)

- `docs/competencia/USO-DE-IA.md` — el documento maestro (7 frentes VIBE,
  gobernanza, valor educativo, FAQ de jueces). Esta skill lo generaliza a
  cualquier doc.
- `AI-INSTRUCTIONS.md` — reglas de atribución (coautoría en commits).
- Frame del repo (CLAUDE.md): los alumnos deciden y validan en hardware; la IA
  acelera pero NO cierra tareas de hardware.
