---
title: "Kickoff del proyecto temporada 2026"
date: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: true
ai-tool: "Claude (Anthropic - Claude Opus 4.6)"
status: final
tags: [organizacion, kickoff, migracion, reglas, arquitectura]
---

# Kickoff del proyecto - Temporada 2026

## Contexto

Equipo IITA clasificó como campeón nacional (Roboliga Argentina, dic 2025). Representará a Argentina en RoboCup 2026, Incheon, Corea del Sur (30 jun - 6 jul 2026). Equipo Soccer Vision: Virginia Viollaz + Elías Cordero. Tutor: Gustavo Viollaz.

## Trabajo realizado

### Sesión 1 — Estructura y reglas

- Repositorio creado con estructura profesional (journal, research, testing, hardware, software, competition, legacy)
- Código 2025 migrado a legacy/
- Revisión completa del código 2025: 8 bugs identificados → `research/completed/2026-02-21-revision-codigo-2025.md`
- Análisis completo de reglas 2026: zona captura 1.5cm, comm module, BOM → `research/completed/2026-02-21-analisis-reglas-2026.md`
- Referencias documentadas: TDPs, equipos top (ducc), simulador
- Backlog de investigación con 6+ temas prioritarios
- Branch protection y CODEOWNERS configurados
- Colaboradores: @gviollaz, @enzzo19, @mariaviollaz, @3liasCo

### Sesión 2 — Análisis de arquitectura

- Análisis exhaustivo del sistema 2025: hardware, software, protocolo, sensores
- **23 puntos de falla** documentados (6 críticos, 8 altos, 9 moderados)
- Análisis del protocolo UART: 3 versiones incompatibles, sin checksum
- Tablas completas de pinout Zircon Mark1 vs Naveen1
- Inventario de código no migrado del repo original 2025
- Arquitectura de software propuesta para 2026 (modular, con PID y fusión sensorial)
- Documento completo → `research/completed/2026-02-21-arquitectura-sistema-2025.md`

## Cambio crítico: zona captura 3.0cm → 1.5cm

Verificar dribbler físicamente.

## Próximos pasos

1. Evaluar hardware físico (Zircon Mark1 vs Naveen1)
2. Verificar dribbler con regla de 1.5cm
3. Investigar e integrar Communication Module
4. Resolver BNO055 (arreglar zirconLib o wrapper directo)
5. Migrar código faltante del repo 2025 a legacy/
6. Rediseñar protocolo UART
7. Gestionar financiamiento viaje
8. Definir roles técnicos para entrevista
