# Arquitectura del Sistema de Robots — Temporada 2025

## Análisis Técnico Completo para Planificación 2026

**Equipo**: IITA RoboCup Junior Soccer Open (ahora Soccer Vision)
**Autor del análisis**: Claude (Anthropic — Claude Opus 4.6)
**Supervisión**: Gustavo Viollaz (@gviollaz)
**Fecha**: 21 de febrero de 2026
**Fuentes**: Repos `IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025` y `IITA-Proyectos/open-soccer-robocup-team2026`

---

## 1. Resumen Ejecutivo

El sistema 2025 consiste en dos robots (arquero y delantero) construidos sobre la placa controladora Zircon (diseño propio en dos variantes: Mark1 y Naveen1) con microcontrolador Teensy 4.1, cámara OpenMV H7 como sensor de visión primario, giroscopio BNO055 para orientación, sensores de línea reflectivos para detección de límites, sensores IR para detección de pelota, y un sensor ultrasónico HC-SR04 para distancia en el arquero.

El equipo ganó el campeonato nacional en diciembre 2025 con este sistema. Sin embargo, el análisis revela **problemas estructurales significativos** en software que deben resolverse antes de competir internacionalmente en Incheon (junio-julio 2026).

Se identificaron **23 puntos de falla** categorizados en: bugs de software (8), deficiencias de diseño (7), vulnerabilidades de protocolo (5), y riesgos por cambios de reglas 2026 (3).

Ver documento completo en: `research/completed/2026-02-21-arquitectura-sistema-2025.md`
