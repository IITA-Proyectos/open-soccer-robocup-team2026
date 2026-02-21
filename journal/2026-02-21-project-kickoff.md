---
title: "Kickoff del proyecto temporada 2026"
date: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: true
ai-tool: "Claude (Anthropic)"
status: final
tags: [organizacion, kickoff, analisis]
---

# Kickoff del proyecto - Temporada 2026

## Objetivo

Crear la estructura organizativa del repositorio para la temporada 2026 de RoboCupJunior Soccer Open League, migrar el trabajo del equipo 2025 y realizar el primer análisis técnico.

## Trabajo realizado

### Estructura y organización
- Creación del repositorio `open-soccer-robocup-team2026` en cuenta personal
- Fork a la organización IITA-Proyectos
- Diseño de la estructura de carpetas:
  - `journal/` — Diario de ingeniería (cronológico)
  - `research/` — Pipeline de investigación (backlog → in-progress → completed)
  - `testing/` — Protocolos + resultados
  - `hardware/` — Electrónica, eléctrica, mecánica
  - `software/` — Por robot + visión + comunicación + librerías
  - `competition/` — Reglas, cronograma
  - `legacy/` — Preservación del trabajo 2025
- Creación de `CONTRIBUTING.md` con convenciones de atribución
- Creación de `AI-INSTRUCTIONS.md` para que cualquier IA opere correctamente

### Migración del legado 2025
- Migración del código del arquero (base .ino)
- Migración del código del delantero (máquina de estados)
- Migración de la librería zirconLib (.h + .cpp)
- Migración de scripts de visión OpenMV (coordenadas, calibración)
- Migración de código de control (giro, seguidor de línea)
- Documentación de archivos mecánicos (STL/GCode referenciados por link)
- Índice completo de archivos misceláneos del repo original

### Análisis técnico del código 2025
- Revisión completa de todos los archivos de código
- **6 bugs críticos/bloqueantes documentados**:
  1. Delantero: anguloRadArco calcula con datos de pelota en vez de arco
  2. Delantero: comparación de tiempo invertida en estado PATEANDO
  3. Arquero: variable `potencia` nunca definida (no compila)
  4. Arquero: readLine() llamado antes de InitializeZircon()
  5. Arquero: código duplicado con 2 setup()/loop()
  6. zirconLib: llave `}` extra al final
- Lista priorizada de mejoras P0/P1/P2 para 2026

### Planificación
- Cronograma detallado semana a semana (Feb-Jun 2026) con checkboxes
- 6 temas de research cargados en backlog (mejoras mecánicas, visión, electrónica, estrategia, UART)
- 1 research completado (revisión de código 2025)

## Decisiones tomadas

- **Separar journal de research**: Journal es cronológico, research es temático con pipeline de estados
- **Legacy como read-only**: Código 2025 preservado intacto, código nuevo en software/
- **Atribución obligatoria**: Todo commit indica quién (humano o IA) y cómo
- **Archivos binarios por referencia**: STL/GCode/PDF quedan en el repo 2025, solo se linkean
- **Nomenclatura kebab-case**: Nunca más espacios en nombres de archivos

## Próximos pasos

- [ ] Completar datos del equipo 2026 en README.md (nombres de alumnos)
- [ ] Inventario de hardware disponible del 2025
- [ ] Probar robots 2025 en cancha para evaluar estado actual
- [ ] Definir horarios de sesiones de trabajo
- [ ] Empezar a trabajar los items P0 del backlog de mejoras
