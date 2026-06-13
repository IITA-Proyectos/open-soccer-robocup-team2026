---
name: arquitectura-robotica-topdown
description: Use when conceptualizing, explaining, reviewing or synthesizing a WHOLE robotic system as an integrated mechatronic system — top-down view, abstraction layers, how mechanical/electrical/power/control/vision/localization/comms fit together, what is load-bearing or critical, simplifying a complex multi-domain system. Triggers - "vista de arquitectura", "explicá el sistema completo", "top-down", "por capas", "cómo se integra todo", "qué es lo crítico", "sintetizá/simplificá el sistema", system-level review across domains. NOT for tuning one subsystem (control-pid-zona-muerta), drawing the figure (rcj-diagramas-poster), writing prose (rcj-doc-voz-estudiante), or P0/P1/P2 student feedback (rcj-soccer-coach).
---

# Arquitectura de sistemas robóticos — síntesis top-down por capas

## Principio central

**El valor del arquitecto NO es describir las partes — es destilar qué sostiene
al sistema y dónde están las costuras donde un dominio rompe a otro.** Síntesis
ANTES que detalle. Un robot integra mecánica + eléctrica + potencia + control +
software, y **falla en las interfaces entre dominios**, no dentro de uno.

**REQUIRED REFERENCE:** `references/lentes-por-dominio.md` — qué mira primero un
senior en control/estabilidad, potencia, localización/SLAM, visión, comms,
sensores, fail-safe, velocidad. Cargarla al entrar a cualquier dominio.

## Cuándo NO usar

- Ajustar un subsistema puntual → su skill (`control-pid-zona-muerta`, etc.).
- Dibujar la figura → `rcj-diagramas-poster` (esta skill produce el MODELO que
  la figura dibuja). Escribir la prosa → `rcj-doc-voz-estudiante`.

## El método (en orden — el orden ES la disciplina)

1. **Esencia primero (1 frase, ANTES de decomponer).** ¿Qué ES el sistema,
   reducido a su idea organizadora? No "tiene 3 placas" sino "son 2 módulos
   unidos por un contrato". Si no podés escribirla, todavía no entendiste el
   sistema — no decompongas hasta tenerla. Al final, verificá que la
   descomposición SIRVE a esta frase.
2. **DOS vistas separadas (el error #1 es mezclarlas):**
   - **Vista en CAPAS (vertical):** cada capa usa la de abajo y **oculta su
     complejidad**. Misión → decisión → modelo del mundo → percepción → control
     → … Regla dura: cada capa es **completa y detenible** — el lector puede
     parar al final de cualquiera sabiendo algo cerrado, sin necesitar la de
     abajo. Si una capa "se asoma" al detalle de otra, la frontera está mal.
   - **CONCERNS transversales (horizontal):** potencia, comunicaciones, timing,
     seguridad/fail-safe. **NO son capas** — cortan TODAS las capas. Tratarlos
     como capa (lo hace un generalista) confunde el modelo. Cada concern se
     describe como una vista propia que atraviesa el stack.
3. **Caminar las COSTURAS de integración** (lo que distingue al arquitecto).
   Recorrer explícitamente las fronteras entre dominios y preguntar "¿qué de un
   lado rompe al otro?": mecánica↔sensor (vibración→IMU), potencia↔todo
   (brownout→sensor→localización→control), software↔eléctrica (contrato roto→
   cadena muda), timing↔control (latencia en el lazo→inestabilidad). Las fallas
   reales viven acá.
4. **Nombrar la RESTRICCIÓN dominante.** ¿Qué le pone el ritmo al sistema:
   percepción, cómputo, potencia o control? Optimizar/explicar lo no-limitante
   es ruido. Una sola frase: "el sistema está limitado por X".
5. **Identificar lo LOAD-BEARING** (test: ¿si esto cae, hay sistema?).
   - **Fuentes de verdad únicas** que hay que cuidar como oro (el contrato de
     datos, la arteria principal, la red de tests).
   - **Single points of failure** y su modo de degradación.
   - Separar **"lo que sostiene"** de **"lo que está en riesgo"** (esto último
     priorizado; si el repo usa P0/P1/P2, alinearse — ver `rcj-soccer-coach`).
6. **Modos de degradación.** Para los componentes críticos: ¿el sistema degrada
   con gracia o muere? Default-to-safe. Un buen diseño degrada por capas.
7. **Cerrar con la síntesis** — la frase del paso 1, ahora cargada: qué es +
   qué lo sostiene + qué lo pone en riesgo hoy.

## Reglas de simplificación (síntesis = tachar, no comprimir)

- **Foco por relevancia, no por completitud.** Un mapa no muestra cada piedra.
  Lo incidental se omite explícitamente, no se mete "por las dudas".
- **Cada afirmación de arquitectura gana su lugar:** o explica una decisión, o
  marca un riesgo, o nombra una frontera. Si solo describe, va al detalle, no a
  la vista.
- **El nivel de zoom correcto** depende de la pregunta: no metas SLAM completo
  si el sistema resuelve con trilateración directa. Sobre-arquitecturar es un
  error tan grave como no arquitecturar.
- **Honestidad de madurez:** distinguir "diseñado/code-complete" de "validado en
  hardware". Un diagrama que pinta como real lo no-probado miente.

## Errores comunes (todos observados en línea base con un modelo fuerte)

| Error | Realidad |
|---|---|
| Poner comms/potencia como "capa" | Son CONCERNS transversales, cortan todas las capas. Vista aparte |
| Capas que se asoman al detalle de otras | Rompe el ocultamiento. Cada capa completa y detenible |
| Listar riesgos sin caminar las COSTURAS | El generalista lista; el arquitecto recorre las fronteras entre dominios |
| No nombrar la restricción dominante | Sin el cuello, no sabés qué importa optimizar/explicar |
| Síntesis al final como resumen | La esencia va PRIMERO y guía la descomposición |
| Lente de control = "tiene PID" | Aplicar régimen/autoridad/latencia/estabilidad (ver referencia) |
| Bottom-up: "estas son nuestras partes" | Top-down: misión → función → subsistema → componente |
| Meter SLAM/Kalman porque suena pro | Usar el nivel mínimo que resuelve; sobre-arquitecturar es falla |

## Red flags — parás y volvés al método

- No podés escribir la frase-esencia → no entendiste el sistema aún.
- Tu vista tiene "capas" que en realidad son concerns (potencia, comms).
- Describiste 6 subsistemas pero ninguna costura entre ellos.
- Todo te parece crítico → no separaste load-bearing de incidental.
- Pintaste como real algo que solo está code-complete.

**Esta skill produce el MODELO.** Para dibujarlo → `rcj-diagramas-poster`; para
escribirlo con voz de estudiante → `rcj-doc-voz-estudiante`; para juzgar si
suma puntaje → `rcj-deliverables-judge`.
