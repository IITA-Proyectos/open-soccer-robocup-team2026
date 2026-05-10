---
name: rcj-judging-package
description: Use when preparing the RoboCupJunior Soccer Open judging deliverables for Incheon 2026 (BOM, poster A1, technical video, digital portfolio, technical interview prep). Ensures every deliverable hits the 2026 rubric and stays consistent across artifacts (innovation highlighted in video must appear in poster and portfolio).
---

# RCJ Judging Package — Deliverables que Puntúan

> **Status: outline only — content pending iteration with the team.**

## When to use

- Armar la BOM final del equipo.
- Diseñar el poster técnico A1 (60×84cm).
- Producir el video técnico.
- Compilar el portfolio digital.
- Preparar a Virginia y Elías para la entrevista técnica.
- Verificar coherencia entre los 5 deliverables.

## When NOT to use

- Trabajo técnico interno (no entregable a jueces) — eso va en `journal/`, `research/`, `docs/`.
- Documentación de proceso interno del repo.

## Deliverables (planned content)

[TODO: desarrollar cada uno con plantilla concreta]

### 1. BOM (Bill of Materials)

**Columnas requeridas según reglas RCJ 2026:**
- Componente.
- Proveedor / sourcing.
- Nuevo / reusado de temporadas anteriores.
- Kit / custom.
- Precio unitario.
- Cantidad.
- Precio total.

**Secciones recomendadas:**
- Mecánica (impresos 3D, perfiles, tornillería, ruedas omni).
- Electrónica (PCB Zircon, MCU, drivers, sensores, conectores).
- Comunicación (módulo árbitro RCJ, ESP32, antenas).
- Visión (OpenMV H7/H7+, lentes, mounts).
- Actuadores (motores TT, dribbler, solenoide).
- Energía (batería, BMS, cables, switch).
- Notas (alternativas, sourcing en Argentina si aplica).

**Trampa común:** olvidar componentes "invisibles" (cinta termocontraíble, conectores Dupont, JST, soldadura, PLA por kg).

### 2. Poster A1 (60×84cm vertical)

**Estructura recomendada:**
- **Header** (top 15%): equipo, país (🇦🇷), logo IITA, logo torneo, año.
- **Hero** (15%): foto del robot en cancha (no render, foto real).
- **Bloques principales** (55%): arquitectura sistema, mecánica, electrónica (Zircon), software (FSM + visión), estrategia (formaciones, set plays).
- **Innovación destacada** (10%): qué nos diferencia.
- **Footer** (5%): QR a portfolio digital, GitHub repo, contacto.

**Reglas de oro:**
- Inglés (es competencia internacional).
- Diagramas > párrafos. Cada bloque con 1 imagen mínimo.
- Tipografía legible a 1.5m de distancia (mínimo 24pt para body).
- Paleta consistente con identidad IITA.

### 3. Video técnico

**Estructura sugerida (2-3 min total):**
- **0:00-0:30** — equipo presentándose + saludo (cara).
- **0:30-1:30** — demo funcional: robot jugando (clips reales de partidos).
- **1:30-2:15** — proceso de diseño: timelapse impresión, montaje, debug en lab, calibración cámara.
- **2:15-2:45** — innovación destacada (algo que diferencia al equipo — debe coincidir con poster y portfolio).
- **2:45-3:00** — cierre + agradecimientos.

**Reglas de oro:**
- Subtítulos en inglés (audio puede ser español).
- Sonido limpio (lavalier o ambiente controlado).
- Resolución 1080p mínimo.
- Subir a YouTube unlisted + link en portfolio.

### 4. Portfolio digital

**Secciones:**
- Equipo y motivación (quiénes somos, por qué soccer).
- Hardware completo (mecánica + electrónica con schematics).
- Software completo (link a repo, descripción de subsystems).
- Procesos (cómo decidimos lo que decidimos — links a `research/completed/`).
- Innovación destacada (consistente con video y poster).
- Lecciones aprendidas (incluso de fracasos — los jueces lo valoran).
- Próximos pasos (post-Incheon: Nacional Nov 2026 + Mundial 2027).

### 5. Entrevista técnica

**Cada miembro con su rol técnico claro:**
- Virginia Viollaz: [TODO definir — probablemente visión + estrategia + integración]
- Elías Cordero: [TODO definir — probablemente electrónica + mecánica]
- Gustavo (coordinador): NO entrevistado por reglas RCJ — son los alumnos.

**Cada uno debe poder explicar (sin guion):**
- Qué hizo concretamente (yo personalmente diseñé X, yo programé Y).
- Por qué lo hizo así (decisión técnica con trade-offs).
- Qué intentó que no funcionó (humildad técnica + aprendizaje).
- Qué mejoraría si tuviera 6 meses más.

**Trampas comunes:**
- "Nosotros" en vez de "yo" — los jueces preguntan rol individual.
- Repetir el poster sin agregar contexto.
- No tener respuesta para "y si esto fallara, ¿qué pasaría?".

## Coherencia transversal (no negociable)

- La **innovación destacada** debe aparecer (con la misma redacción) en poster + video + portfolio.
- BOM debe coincidir con lo que se ve en el robot (no listar componentes que no están).
- Lo que el alumno dice en entrevista no puede contradecir poster/portfolio.
- Cualquier afirmación en deliverables tiene que tener trazabilidad a `journal/` o `research/completed/`.

## Hard rules

- **Inglés** para entregables internacionales (Incheon).
- **Documentación previa en el repo** justifica todo lo que aparece en deliverables.
- **Deadline real** ≠ deadline RCJ (siempre dejar buffer de 1 semana).

## References

- Reglas RCJ Soccer 2026 (DRAFT): https://robocup-junior.github.io/soccer-rules/2026-soccer-draft-rules/rules.html
- `competition/timeline.md` — deadlines.
- `competition/rules/` — reglas archivadas localmente.
- Posters de equipos top RCJ — agregar a `research/references/` para inspiración.
