---
title: "Resumen — Arquitectura del sistema 2025"
date: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: false
ai-tool: "Claude (Anthropic - Claude Opus 4.6)"
status: final
tags: [arquitectura, resumen, indice]
---

# Resumen — Arquitectura del Sistema 2025

> Documento índice. Análisis completo en: [`2026-02-21-arquitectura-sistema-2025.md`](2026-02-21-arquitectura-sistema-2025.md)

## Hallazgos principales

- **23 puntos de falla** identificados (6 críticos, 8 altos, 9 moderados)
- **6 bugs críticos**: giroscopio deshabilitado, condición de pateo invertida, thresholds LAB inválidos (L_min > L_max), código del arquero no compila, protocolo UART sin checksum ni resync
- **Recursos desperdiciados**: 8 sensores IR instalados pero no usados, dribbler sin activación automática
- **3 versiones incompatibles** del protocolo UART OpenMV→Teensy
- **2 baud rates** y **2 matrices de homografía** diferentes sin documentar

## Componentes del sistema

| Componente | Estado 2025 |
|------------|-------------|
| Teensy 4.1 + Zircon PCB | ✅ Hardware OK, software limitado |
| OpenMV H7 (visión) | ⚠️ Funcional pero thresholds inconsistentes |
| BNO055 (giroscopio) | ❌ Deshabilitado en código de competencia |
| 8× IR ball sensors | ❌ No usados en código final |
| 3× sensores de línea | ⚠️ Cobertura insuficiente (solo 3) |
| HC-SR04 (ultrasónico) | ⚠️ Lectura bloqueante |
| Dribbler | ❌ Sin activación automática |
| Communication Module | ❌ No existe (obligatorio 2026) |

## Top 3 prioridades inmediatas

1. **Habilitar giroscopio BNO055** — el código base ya existe
2. **Rediseñar protocolo UART** — con start byte, checksum, y resync
3. **Verificar ball-capturing zone 1.5 cm** — cambio de reglas 2026

## Documentos relacionados

- [Revisión de código 2025](2026-02-21-revision-codigo-2025.md)
- [Análisis de reglas 2026](2026-02-21-analisis-reglas-2026.md)
