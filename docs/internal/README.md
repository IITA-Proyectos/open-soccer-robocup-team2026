---
title: "Documentación Interna - Equipo IITA Salta Soccer"
date: 2026-03-20
status: active
---

# Documentación Interna

Documentos técnicos de uso interno del equipo. No se presentan en competencia.

## Índice

### Estado del proyecto y planificación
- [**Limitaciones del robot actual (marzo 2026)**](limitaciones-robot-marzo-2026.md) — Las 8 limitaciones conocidas con severidad, impacto y decisiones. Incluye L1 (módulo árbitros, BLOQUEANTE).
- [**Roadmap de mejoras 2026**](roadmap-mejoras-2026.md) — Todas las mejoras organizadas en 4 horizontes (Mundial → Deseable → Roboliga → Futuro). 20 items con IDs para tracking.
- [**Flujo de trabajo software**](flujo-de-trabajo-software.md) — Ciclo staging → prueba → producción para desarrollo de código.

### Diseño de sistemas
- [**Sistema de posicionamiento y comunicación**](sistema-posicionamiento-y-comunicacion.md) — Arquitectura por capas, modelo del mundo, comunicación entre robots, implementación progresiva.
- [**Cinemática omnidireccional y movimientos**](cinematica-omnidireccional-movimientos.md) — Modelo cinemático, función moverRobot(), control PD de heading.
- [**Giróscopo BNO055 — análisis técnico**](giroscopo-bno055-analisis-tecnico.md) — IMUPLUS vs NDOF, inicialización, drift, referencias.

### Análisis de código
- [**Análisis programa delantero**](analisis-definitivo-delantero.md) — Bugs, confiabilidad, legibilidad, propuesta de refactoring.
- [**Análisis programa arquero**](analisis-definitivo-arquero.md) — Bugs, comparativa con delantero, oportunidades de mejora.
- [**Análisis código legacy 2025**](analisis-arquero-legacy.md) — Análisis del código heredado de la temporada anterior.
