---
name: rcj-doc-voz-estudiante
description: Use when writing or rewriting RoboCupJunior competition documentation in the team's voice — TDP sections, abstracts, system overviews, program/software documentation, video scripts, poster text, interview answers — or when synthesizing the essence of a complex robot system for judges or other teams. Triggers - "escribí/redactá la sección", "documentá este programa/código", "explicá cómo funciona el sistema", "describí la esencia", "resumí la arquitectura", text that sounds like a professor or an AI instead of a student, or any deliverable prose meant to score rubric points.
---

# Redacción campeona con voz de estudiante (TDP / póster / video)

## Principio central

**Cerebro de ingeniero senior, manos de estudiante de 18.** El ingeniero elige
QUÉ decir (decisiones, trade-offs, datos, qué callar); el estudiante define
CÓMO suena (frases cortas, jerga explicada, dueño en primera persona, pasión
concreta). La voz ES puntaje: los jueces sondean students-do-the-work, y un
texto que suena a profesor o a IA se paga en la entrevista aunque el contenido
sea perfecto.

**REQUIRED BACKGROUND:** las puertas Excellent a las que apunta cada texto
están verbatim en `rcj-deliverables-judge` → `references/rubrica-oficial-2026.md`.
Escribir apuntando a una puerta concreta, no a "que quede lindo".

## Cuándo NO usar

- Journal/docs internos del repo → `engineering-journal` (ahí la precisión
  técnica manda y la voz no importa).
- Estructura/checklist de los entregables (zonas del póster, columnas BOM,
  timing del video) → `rcj-judging-package`.
- Puntuar un texto ya escrito → `rcj-deliverables-judge`.

## Proceso

1. **Esencia primero (1 frase).** Antes de escribir nada: ¿qué hace el sistema
   y qué lo hace especial, en UNA oración que un visitante repite de memoria?
   Esa frase abre la sección. Test: si se borra el resto, ¿el lector se lleva
   algo completo?
2. **Capas modulares.** Esencia → 3-5 bloques (uno por responsabilidad, no por
   componente físico) → detalle dentro de cada bloque. Regla: el lector puede
   PARAR al final de cualquier capa sabiendo algo cerrado. Nunca zambullirse
   en detalle fino antes de cerrar el mapa global.
3. **Selección con lente senior.** Cada afirmación importante lleva el trío
   **decisión → por qué → dato medido**. Sin dato: decir cómo se mediría o
   declarar que falta. Elegir las 2-3 historias con mejor relación
   aprendizaje/palabras y soltar el resto (síntesis = tachar, no comprimir).
4. **Escribir con las reglas de voz** (abajo).
5. **Pasada anti-profesor/anti-IA** (checklist abajo) + auto-juzgarse contra
   la puerta de rúbrica elegida antes de dar por terminado.

## Reglas de voz (el estudiante)

- **Frases cortas.** Una idea por frase, ~20 palabras o menos de promedio. Si
  una frase necesita dos comas y un paréntesis, son dos frases.
- **Jerga: explicada en su primer uso, siempre.** "histéresis (el sensor tiene
  que pasarse bien del umbral para cambiar de estado, así no titila)". Vale
  para FSM, PID, odometría, mux, CRC… La rúbrica premia que OTRO equipo pueda
  aprender; algunos lectores tienen 14 años.
- **Primera persona con dueño.** "Probamos X y se nos quemó un motor" >
  "se evaluó X". Lo que hizo UNA persona, con nombre si va a la entrevista.
- **La pasión es una anécdota, no un adjetivo.** El momento en que algo
  funcionó a las 11 de la noche, el cable que nos comió 40 minutos. PROHIBIDO
  transmitir entusiasmo con marketing ("innovador", "de vanguardia", "robusto"
  sin número).
- **Honestidad explícita.** "Esto compila y pasa los tests, pero todavía no lo
  probamos en cancha" — puntúa MEJOR que inflar, y desarma la sonda del juez.
- **Números siempre.** 31 bytes, 100 Hz, 6,5 % de error. Un número bien puesto
  vale un párrafo.

## Documentar programas (puerta Software-Excellent)

- Tabla módulo → qué hace → por qué existe (función, no archivo).
- **UN** flowchart o pseudocódigo del comportamiento estrella — no de todo.
- Evidencia git REAL y verificada: nombres exactos de branches/commits (correr
  `git log`, no citar de memoria — los nombres traducidos o muertos son
  munición de juez).
- El bug favorito contado como historia: síntoma → dato → causa → fix → test
  de regresión. Es la forma más barata de demostrar comprensión profunda.

## Pasada anti-profesor / anti-IA (checklist final)

| Señal | Test / Fix |
|---|---|
| Suena a paper ("se procederá", "cabe destacar", pasiva en cadena) | Reescribir en activa con dueño |
| Palabras de marketing ("leverage", "vanguardia", "robusto/seamless" sin dato) | Borrar o respaldar con número |
| Párrafo de +6 líneas | Partir o convertir en lista/figura |
| Jerga desnuda en primer uso | Paréntesis explicativo o reformular |
| Simetría de IA ("En resumen…", listas perfectamente paralelas, 3 adjetivos por sustantivo) | Romper el patrón; dejar irregularidad humana |
| Cero anécdotas, cero fracasos | Agregar la historia real más barata (del journal) |
| **Test de la entrevista:** ¿Virginia o Elías dirían ESTA frase tal cual, en voz alta, ante un juez? | Si no → reescribir hasta que sí |

## Reglas de flujo del repo

- Escribir el **ES fuente** en `docs/competencia/`; el inglés lo genera el
  pipeline de traducción. ⛔ NUNCA editar `en/` a mano (se pisa).
- Cifras vivas (tests, envs, costos): tomarlas del repo EN EL MOMENTO y fechar
  ("827 tests, medido 2026-06-12"). Cifras viejas desincronizadas = munición.
- Toda afirmación debe ser trazable a `journal/` o `research/` (regla del repo).

## Dónde estudiar a los campeones (fuentes reales, no inventar)

- Archivo curado oficial de TDPs/posters/repos de equipos top:
  github.com/robocup-junior/awesome-rcj-soccer
- RCJ archiva los pósters de cada mundial para los equipos futuros — el tuyo
  va a ser estudiado: escribí para ese lector también.
