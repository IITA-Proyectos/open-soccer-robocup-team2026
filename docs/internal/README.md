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

### Transcripción FIEL del código 2025 (para recrear el comportamiento exacto en 2026)
> Estos 3 son una **transcripción línea por línea, 100% fiel** (no análisis de bugs): capturan TODA la lógica de juego, PWM, tiempos, timeouts y umbrales del Nacional 2025 (Buenos Aires, campeones) para reconstruir el comportamiento EXACTO con los sensores nuevos. Generados por workflow de 14 agentes + crítico de completitud (2026-06-18).
- [**Análisis FIEL — Arquero 2025**](ANALISIS-FIEL-ARQUERO-2025.md) — `definitivo-arquero` (ROBOT1). FSM, constantes por robot, lógica paso a paso, tabla exhaustiva (153 ítems). `cobertura_ok=TRUE`.
- [**Análisis FIEL — Delantero 2025**](ANALISIS-FIEL-DELANTERO-2025.md) — `definitivo-delantero` (ROBOT2). Ídem (152 ítems) + apéndice de completitud.
- [**Análisis FIEL — zirconLib**](ANALISIS-FIEL-ZIRCONLIB-2025.md) — primitivas de motor `motor1/2/3`, mapeo a PWM (Mark1/Naveen1), pines, `motorLimit=100`.
> ⚠️ Hallazgo: ambos `definitivo-*` son el MISMO firmware unificado arquero+delantero; cambian `#define ROBOT1/2` y el estado inicial.
