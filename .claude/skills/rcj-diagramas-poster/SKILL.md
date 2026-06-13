---
name: rcj-diagramas-poster
description: Use when creating or reviewing explanatory visuals for RoboCupJunior deliverables — poster figures, system architecture diagrams, dataflow/FSM diagrams, iteration figures, photo overlays, SVG drawings for the A1 poster or TDP. Triggers - "armá/dibujá el diagrama", "la figura del póster", "explicá el sistema con un dibujo", "diagrama de bloques/flujo", "hacé el SVG", a figure that must be legible at poster distance, or any visual meant to score Photos & Graphics / Method / Data rubric points.
---

# Diagramas explicativos y figuras de póster (RCJ)

## Principio central

**Un diagrama responde UNA pregunta.** El póster es una FAMILIA de figuras
coordinadas, no una mega-figura que lo dice todo. Cada figura se diseña para su
distancia de lectura (titular a 3 m, cuerpo a 1,5 m) con matemática de
impresión hecha ANTES de dibujar — y **no se entrega sin haberla VISTO
renderizada**.

**REQUIRED BACKGROUND:** estructura/zonas del póster → `rcj-judging-package` y
`docs/competencia/POSTER.md` (paleta y tipografías en su Zona Pie). Puertas de
rúbrica → `rcj-deliverables-judge`. Texto que acompaña → `rcj-doc-voz-estudiante`.

## Proceso

1. **Pregunta y zona.** Escribir la pregunta que la figura responde en una
   frase. Leer el plan de zonas de `POSTER.md`: ¿dónde vive esta figura y qué
   contenido ya cubren las OTRAS zonas? Prohibido duplicar una zona vecina
   dentro de la figura.
2. **Elegir el tipo** (tabla abajo). Si la pregunta pide dos tipos, son dos
   figuras.
3. **Presupuesto visual: ≤7 unidades.** Una unidad = caja, flecha con
   etiqueta, panel, glifo. ¿Necesita más? → partir en familia con zoom
   (sistema → placa → módulo), cada una con su pregunta.
4. **Matemática de impresión ANTES de dibujar.**
   `mm_por_px = ancho_impreso_mm / ancho_viewBox_px`. Regla del póster
   (POSTER.md): cuerpo ≥24 pt (≈8,5 mm), títulos ≥48 pt (≈17 mm).
   Ejemplo real: figura de 420 mm con viewBox 1800 → 0,233 mm/px ⇒ texto
   mínimo **36 px**, títulos **72 px**. Nada por debajo del mínimo — si no
   entra, sobran unidades (volver al paso 3).
5. **Dibujar el SVG** con las reglas de abajo.
6. **Renderizar y MIRAR (obligatorio).** En Windows sin Inkscape:
   `& "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe" --headless --disable-gpu --screenshot=out.png --window-size=ANCHO,ALTO "file:///ruta.svg"`
   (probado en la máquina del equipo 2026-06-12). Abrir el PNG y chequear:
   solapes, texto cortado, contraste, y la prueba del pulgar — achicado a
   miniatura, ¿se entiende la historia? (simula los 3 m).
7. **Entrega disciplinada.** Drafts a `docs/competencia/assets/drafts/` y
   AVISAR siempre qué archivos se crearon — el equipo decide qué se commitea.
   Antes de imprimir: cifras re-medidas, texto a curvas (Inkscape: Path >
   Object to Path), export PNG 300 dpi.

## Tipos de diagrama (elegir por pregunta)

| Pregunta | Tipo | Nota |
|---|---|---|
| ¿Cómo está organizado el sistema? | Bloques por RESPONSABILIDAD | Función, no cableado: el esquemático va en el repo, no en el póster |
| ¿Qué viaja entre módulos? | Dataflow con flechas etiquetadas | Etiqueta = dato + tamaño @ frecuencia (ej. "31 B @ 100 Hz") |
| ¿Cómo se comporta? | FSM (estados + transiciones) | UN comportamiento estrella, no todos |
| ¿Cómo mejoró con el testing? | Figura de iteración v1→v2→v3 con datos | Puerta Excellent "testing→evaluation→modification" |
| ¿Cómo es de verdad? | FOTO real + overlay de etiquetas | La rúbrica pide fotos etiquetadas Y citadas |
| ¿Qué dicen los números? | Gráfico de datos | Eje, unidad, fecha de medición |

## Reglas del SVG

- **Paleta del póster** (POSTER.md): azul=TOP, naranja=CENTRAL, verde=DOWN,
  gris=neutro (+ rojo SOLO para emergencia/seguridad). El color ES información.
- 1-2 familias tipográficas máximo (las de la Zona Pie).
- Más de un tipo de flecha ⇒ leyenda dentro de la figura.
- **Caption obligatoria:** "Fig.N — [el TAKEAWAY, no 'diagrama del sistema']"
  + crédito ("Original diagram by the team — CC BY 4.0") + fuente citada si
  hay material externo.
- **Honestidad visual:** objetivos de diseño no medidos llevan asterisco
  ("*design target"); cifras vivas (tests, errores) se miden ESE día y se
  fechan en la figura.
- Evitar texto rotado salvo necesidad (es lo primero que se rompe entre
  renderers).

## Errores comunes (todos observados en línea base)

| Error | Realidad |
|---|---|
| Texto de 13 px "porque entra" | A 420 mm son ~10 pt impresos — viola el ≥24 pt del propio póster. La matemática va ANTES de dibujar |
| Mega-figura de 12+ unidades | Compite con las otras zonas del póster y nadie la lee en 30 s. Familia de figuras |
| Entregar el SVG sin renderizar | Los anchos de texto y rotaciones se estiman mal a ciegas. Render + mirar, siempre |
| Cifra de tests/datos de memoria | Se desincroniza con los demás entregables = munición de juez. Medir y fechar |
| Dropear archivos en `assets/` sin avisar | Drafts a `assets/drafts/` + avisar; el equipo decide qué entra al repo |
| Dibujar el cableado (pines, UARTs) | Eso es el esquemático. El póster explica FUNCIÓN; el detalle eléctrico se linkea al repo |
