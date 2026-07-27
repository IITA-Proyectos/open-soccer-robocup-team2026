# Guía de contribución

## Reglas de atribución

Todo cambio en este repositorio **debe indicar quién lo hizo y cómo**. Esto es obligatorio para mantener la trazabilidad del proyecto.

### Tipos de autoría

| Tipo | Descripción | Formato en commit |
|------|-------------|-------------------|
| **Humano solo** | Trabajo hecho 100% por una persona | `Author: Nombre Apellido (@github-user)` |
| **Humano + IA** | Trabajo hecho con asistencia de IA | `Author: Nombre Apellido` + `AI-assisted-by: NombreIA (Proveedor)` |
| **IA supervisada** | Trabajo hecho por IA bajo supervisión humana | `Author: NombreIA (Proveedor)` + `Requested-by: Nombre Apellido` |

### Formato de mensajes de commit

```
tipo(alcance): descripción breve

Descripción más larga si es necesario.

Author: Nombre Apellido (@github-user)
AI-assisted-by: Claude (Anthropic)     # si aplica
Requested-by: Nombre Apellido           # si aplica
```

### Tipos de commit

- `feat`: Nueva funcionalidad o contenido
- `fix`: Corrección de bug o error
- `docs`: Documentación
- `test`: Pruebas o resultados de testing
- `research`: Investigación y análisis
- `refactor`: Reestructuración sin cambio funcional
- `hardware`: Cambios en diseño mecánico, electrónico o eléctrico
- `journal`: Entrada de diario de ingeniería

### Ejemplos

```
feat(vision): agregar detección de arcos por color

Implementación del filtro de color para detectar ambos arcos
usando thresholds calibrados en LAB.

Author: Juan Pérez (@juanperez)
AI-assisted-by: ChatGPT (OpenAI)
```

```
research(motors): análisis comparativo de motores TT vs N20

Se compararon torque, velocidad y consumo de ambos tipos.
Ver research/completed/2026-03-15-motores-tt-vs-n20.md

Author: Claude (Anthropic - Claude Opus 4.6)
Requested-by: Gustavo Viollaz (@gviollaz)
```

## Nombres de archivo

- Usar **kebab-case** (minúsculas con guiones): `mi-archivo.md`
- Para entradas con fecha: `YYYY-MM-DD-descripcion.md`
- Código fuente: mantener convenciones del lenguaje (`.ino`, `.py`, `.cpp`, `.h`)
- **Nunca usar espacios** en nombres de archivo

## Frontmatter para archivos Markdown

Todo archivo `.md` de contenido debe comenzar con:

```yaml
---
title: "Título descriptivo"
date: 2026-02-21
author: "Nombre o IA"
ai-assisted: true/false
ai-tool: "Claude (Anthropic)"  # si aplica
status: draft/review/final
tags: [vision, openmv, calibracion]
---
```

## Reglas para uso de IA

1. **Siempre declarar** qué IA se usó y para qué parte del trabajo
2. **Revisar** todo output de IA antes de hacer commit
3. **No hacer commit de código generado por IA sin probarlo** en el robot real o simulador
4. **Documentar** qué prompts o instrucciones se le dieron a la IA si el resultado fue significativo
5. La IA es una **herramienta**, la responsabilidad final es del miembro del equipo

## Flujo de trabajo con branches

- `main`: versión estable e integrada. Los merges a `main` los hace **Gustavo** desde el repo principal (`git merge --no-ff agente/<placa>`).
- `agente/central`, `agente/top`, `agente/down`: ramas de trabajo, **una por placa**. Cada agente (humano o IA) edita y commitea solo en la suya. *(Hasta 2026-07-26 cada una vivía en su propia git worktree; hoy el repo es un clon único y se cambia de rama con `git switch` — ver [`CLAUDE.md`](CLAUDE.md).)*
- Ramas puntuales para experimentos o features cruzados: nombre descriptivo en kebab-case.

**Reglas de trabajo (no negociables):** el repo es **compartido** → `git fetch` + `git merge origin/main` **antes** de pushear a `main`; `git rebase` y `git reset --hard` sobre ramas ya pusheadas están **prohibidos**; y el staging se hace **archivo por archivo** (nunca `git add -A`) para no arrastrar trabajo ajeno al commit. El detalle completo (ubicación del repo, rangos de TASK por placa, cómo mergear) está en **[`CLAUDE.md`](CLAUDE.md) → "Ubicación del repo y trabajo en paralelo"**.
