---
title: "Índice — Documentos de competencia RoboCupJunior Soccer Open 2026"
date: 2026-06-04
status: índice
idioma: español-rioplatense (los DELIVERABLES finales — Poster + TDP + Video + Entrevista — van en INGLÉS)
rubrica: RoboCupJunior Soccer 2026 (total 56 pts)
---

# Documentos de competencia — RoboCupJunior Soccer Open 2026

Carpeta de trabajo del equipo IITA para los entregables **documentales** de RoboCupJunior Soccer Open 2026 (Incheon, 30 jun – 6 jul 2026). Cubre los componentes de la rúbrica que dependen de documentación (**26 pts + 2 bonus**, 100% bajo control del equipo); el Gameplay (30 pts) se juega en cancha y queda fuera de esta carpeta.

> ⚠️ **TRADUCIR ANTES DE ENTREGAR:** todos los documentos están en **español de trabajo**. El **Poster, el TDP, el Video (subtítulos) y el material de Entrevista FINALES deben estar en INGLÉS** (requisito duro de rúbrica RCJ). Mantener la versión ES solo como borrador interno.

---

## Archivos de esta carpeta

| Archivo | Qué es | Rúbrica que alimenta |
|---|---|---|
| [`POSTER.md`](POSTER.md) | Maqueta de trabajo del poster A1 apaisado (grilla 12 columnas, 6 zonas mapeadas 1:1 a los 6 criterios, 5 elementos obligatorios, Fig.1-11). | Poster Design & Presentation (5 pts) |
| [`TDP.md`](TDP.md) | Technical Documentation Paper, 4 secciones (Electrical/Mechanical/Software/Presentation) en formato Decisión→Por qué→Dato + §5 bonus open-source + §6 gaps. | TDP (7 pts + 2 bonus) · Documentation & Community (5 pts) |
| [`VIDEO-GUION.md`](VIDEO-GUION.md) | Guion shot-by-shot (<3 min) del Short Form Video TDP. Feature: testing host-native de firmware embebido. | Short Form Video TDP (1 pt) |
| [`ENTREVISTA-PREP.md`](ENTREVISTA-PREP.md) | Cheat-sheet de la Group Team Interview: Show&Tell + Teamwork-Task + banco de preguntas por categoría + protocolo de pase de palabra. | Group Team Interview (5 pts) |
| [`BOM.md`](BOM.md) | Bill of Materials de componentes mayores (part number, proveedor, nuevo/reusado, kit/custom, costo). Insumo del Poster y el TDP. | Poster Method/Design · TDP Electrical · Documentation |
| [`RUBRICA-COBERTURA.md`](RUBRICA-COBERTURA.md) | Matriz componente×criterio de los 56 pts: dónde se cubre cada punto, nivel apuntado, estimado hoy y qué falta. Para encontrar cada punto en 5 s. | Todos |
| [`MEJORAS-PENDIENTES.md`](MEJORAS-PENDIENTES.md) | Backlog priorizado (P0/P1/P2) de datos a conseguir, fotos/diagramas a producir, mejoras por deliverable y acciones de competencia. | Todos |

---

## Estrategia de puntaje (5 bullets)

- **Replicabilidad = estándar de oro.** Escribir para que OTRO equipo reconstruya el robot: pinout doble-punta, recetas de bring-up con los fallos reales, costos y specs. Es lo que más premia RCJ.
- **Iteración con datos.** Mostrar el ciclo testeo→evaluación→modificación con **modificaciones MAYORES hechas POR el testing** (OTOS lámina/A4/cartón, I²C 400k→100k, árbitro AND→OR), en gráficos/tablas, no solo narrado.
- **El cuello de botella es EJECUCIÓN, no contenido.** El texto ya apunta a Excellent en casi todo; faltan **imágenes (hoy hay 0 en el repo)**, **maquetar el A1**, **grabar el video**, **cerrar costos** y **traducir a inglés**. Software del TDP y los bonus open-source ya están sólidos.
- **Honestidad calibrada.** Distinguir "verificado en banco" de "verificado en host", y "dato medido" de "objetivo de diseño". Un juez adversarial premia la honestidad y castiga datos inventados; sobrevender el 403/33 (visión sin calibrar) es un riesgo.
- **Consistencia entre deliverables.** Una sola cifra de tests (correr `scripts/run-host-tests.sh` el día previo — hoy **624/44/0**), un solo nombre de equipo, un solo nombre de la organización (hoy LICENSE dice "Innovación" y README "Informática"). Cifras o nombres contradictorios = munición para el juez.

> 🇬🇧 **Recordatorio final:** antes de imprimir/enviar, **traducir Poster + TDP + subtítulos del Video + material de Entrevista al INGLÉS** y correr corrector ortográfico EN. Sin esto, varios criterios no pueden puntuar Excellent por más bueno que sea el contenido.

---

## Por dónde empezar

1. Leé [`RUBRICA-COBERTURA.md`](RUBRICA-COBERTURA.md) para ver el estado de cada uno de los 56 pts y dónde estamos parados.
2. Seguí el **camino crítico** de [`MEJORAS-PENDIENTES.md`](MEJORAS-PENDIENTES.md): datos duros → fotos/gráficos → cerrar BOM → traducir → maquetar/grabar → unificar → ensayar.
