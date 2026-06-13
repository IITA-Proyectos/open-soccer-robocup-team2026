---
name: ia-educacion-no-trampa
description: Use when documentation, an argument, or a reply touches on the team's use of AI — methodology sections, USO-DE-IA, TDP/poster AI-use framing, attribution/transparency decisions, or answering anyone (judge, teacher, parent, skeptic) who calls AI use "cheating" or says "the robot isn't really theirs". Triggers - "¿no es trampa?", "usaron IA", defender el método, mostrar/declarar el uso de IA, transparencia de IA, "los chicos no aprendieron". The stance: AI use done with understanding + verification is EDUCATION, not cheating — show it and defend it, never hide or apologize. NOT for doing the AI-accelerated work (vibe-* skills) nor judging deliverables (rcj-deliverables-judge).
---

# La IA es educación, no trampa — mostrar y defender

## Principio central

**El uso de IA con entendimiento y verificación no es un atajo para evitar
aprender: es el contenido que hay que aprender.** En la documentación NO se
esconde ni se minimiza — se MUESTRA con orgullo y se DEFIENDE con argumentos
pedagógicos y fácticos. Pero la defensa solo es creíble si se apoya en evidencia
real (trazabilidad, gate de tests, validación de banco) y declara los límites.

**REQUIRED REFERENCE:** `references/arsenal-argumentos.md` — los argumentos
desplegables (reframe trampa-es-un-cómo, histórico, fáctico/laboral, esfuerzo,
transparencia) + rebatir objeciones + el guardarraíl de honestidad. Cargarlo
antes de escribir/defender.

## Cuándo NO usar

- Hacer el trabajo asistido por IA → skills `vibe-*`.
- Voz de estudiante en general → `rcj-doc-voz-estudiante` (complementa: esta
  skill da la POSTURA, esa da la VOZ).
- Juzgar entregables → `rcj-deliverables-judge`.

## Postura (la actitud, antes que las palabras)

1. **Orgullo, no disculpa.** El uso de IA es una VENTAJA competitiva y
   pedagógica, no algo a justificar entre paréntesis. Borrar todo residuo
   apologético: nada de "solo la usamos como apoyo", "todas las decisiones
   fueron nuestras igual" (concede que la IA es sospechosa). Se afirma, no se
   pide permiso.
2. **Confrontar la acusación de frente.** Si el contexto es "¿es trampa?", se
   NOMBRA y se DESARMA — no se rodea. La reframe central (trampa es un CÓMO, no
   un QUÉ) va explícita.
3. **Pasar a la ofensiva.** El argumento más fuerte no es "no es trampa" sino
   **"prohibirla es la verdadera falla pedagógica"**: deja a los chicos sin la
   competencia central de su campo. La defensa se vuelve tesis educativa.
4. **Mostrar es la jugada fuerte.** La transparencia (commits con coautoría,
   journal, repo público, gate) ES la prueba de que se usó bien. Esconder sería
   la confesión de que no se puede defender.

## Cómo se escribe (al redactar documentación)

- **Abrir con la tesis, no con la defensa:** "usamos IA como herramienta de
  ingeniería que aceleramos y verificamos" — afirmativo, no a la defensiva.
- **Desplegar 2-3 argumentos del arsenal** apropiados al lector (a un juez:
  histórico + transparencia + valor educativo; a un docente escéptico:
  fáctico/laboral + esfuerzo; a un par: la reframe del CÓMO).
- **Anclar CADA afirmación en evidencia del proyecto.** "Aprendimos más" sin el
  caso del I²C/giroscopio es palabrería. La evidencia convierte retórica en
  postura de ingeniería.
- **Declarar los límites en el mismo texto** (qué NO hace la IA, qué está
  validado vs roadmap). Es lo que vuelve la defensa imbatible — ver guardarraíl.
- **Distinguir legítimo de ilegítimo** explícitamente (la tabla del arsenal):
  marca que el equipo sabe dónde está la línea.
- **Cerrar con la tesis de futuro:** no es trampa, es la educación para el
  mundo laboral que viene.

## Errores comunes (todos observados en línea base)

| Error | Realidad |
|---|---|
| Tono apologético ("solo apoyo", "igual decidimos todo") | Concede que la IA es sospechosa. Afirmar con orgullo |
| Rodear la acusación de "trampa" sin nombrarla | Si el trigger es la acusación, se desarma de frente |
| Solo defender, nunca atacar | El argumento fuerte: prohibir es la mala praxis |
| Argumento solo pedagógico, sin lo fáctico/laboral | Falta la mitad: la IA ya es estándar profesional |
| Retórica sin evidencia del proyecto | "Aprendimos más" sin el caso real = palabrería |
| "Defender a muerte" = sobrevender lo que la IA hizo | Exagerar te derriba; declarar límites te blinda |
| Voz burocrática de disclosure | Voz de educador con convicción (sin inventar pasión) |

## Red flags — parás y recalibrás

- Tu texto pide perdón por usar IA en vez de defenderla.
- Mencionaste "trampa" como acusación pero no la desarmaste.
- Afirmaste valor educativo sin un solo ejemplo concreto del proyecto.
- Defendiste tan fuerte que ocultaste un límite (la IA "hizo" algo que en
  realidad un humano validó) → eso es la grieta que un escéptico explota.

**Coordina con:** `rcj-doc-voz-estudiante` (voz), `rcj-deliverables-judge`
(cómo lo lee un juez), y el documento maestro `docs/competencia/USO-DE-IA.md`.
