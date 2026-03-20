---
title: "Documentación - IITA Salta Soccer Open 2026"
date: 2026-03-20
status: active
---

# Documentación del Proyecto

Toda la documentación del equipo IITA Salta para RoboCup Junior Soccer Open 2026.

## Estructura

```
docs/
├── official/          # Documentación para presentar en competencia
│   ├── es/            # Versiones en español
│   └── en/            # Versiones en inglés (para internacional)
├── internal/          # Documentación técnica interna del equipo
└── README.md          # Este archivo
```

## Documentación Oficial (`official/`)

Documentos que se presentan a los jueces y organizadores de la competencia. Se mantienen en español e inglés.

| Documento | Descripción | Estado |
|-----------|------------|--------|
| TDP (Team Description Paper) | Descripción técnica del robot y estrategia | Pendiente |
| Poster | Poster de presentación del equipo | Pendiente |
| Engineering Journal Summary | Resumen del journal para jueces | Pendiente |

## Documentación Interna (`internal/`)

Documentos técnicos de uso interno del equipo. No se presentan en competencia pero son fundamentales para el desarrollo.

| Documento | Descripción | Estado |
|-----------|------------|--------|
| [Análisis código arquero legacy](internal/analisis-arquero-legacy.md) | Análisis a fondo del código 2025, bugs y oportunidades de mejora | ✅ Completo |
| [Arquitectura sistema 2025](ARQUITECTURA-SISTEMA-2025.md) | Arquitectura del sistema legacy | ✅ Referencia |
| [Setup protección](SETUP-PROTECCION.md) | Configuración de protecciones del repo | ✅ Referencia |

## Convenciones

- **Idioma**: Docs internos en español. Docs oficiales en ambos idiomas.
- **Formato**: Markdown con frontmatter YAML (title, date, status, tags).
- **Nombres de archivo**: kebab-case, descriptivos. Ej: `analisis-arquero-legacy.md`
- **Estado**: `draft`, `review`, `final`
- **AI-assisted**: Marcar `ai-assisted: true` en frontmatter si fue generado/asistido por IA.
