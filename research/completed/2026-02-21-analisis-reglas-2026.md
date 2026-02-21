---
title: "Análisis completo de reglas Soccer 2026"
date: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
status: completed
priority: alta
tags: [reglas, competencia, 2026, analisis]
---

# Análisis completo de reglas RoboCupJunior Soccer 2026

Fuente: https://robocup-junior.github.io/soccer-rules/2026-soccer-draft-rules/rules.html  
Fecha de las reglas: 2026-01-21

## Cambios específicos 2025 → 2026

| Cambio | Antes (2025) | Ahora (2026) | Impacto para nosotros |
|--------|-------------|-------------|----------------------|
| Nombre liga | Soccer Open | Soccer Vision | Solo nombre |
| Zona captura pelota | 3.0 cm | **1.5 cm** | **CRÍTICO** — revisar dribbler |
| Peso IR league | 1400g | 1500g | No nos afecta (Vision) |
| Pelota IR | 74mm | 42mm | No nos afecta (ya usamos golf ball) |
| Pushing + multiple defense | No especificado | Se resuelve pushing primero | Regla de juego |
| Test kicker | Pedir muestra de kick | Testear potencia del kicker | Más formal |

## Restricciones físicas del robot (Soccer Vision)

| Parámetro | Límite |
|----------|--------|
| Diámetro máximo | 18.0 cm |
| Altura máxima | 18.0 cm |
| Peso máximo | Sin límite |
| Zona de captura de pelota | **1.5 cm** |
| Voltaje máximo | 48V DC / 25V AC RMS |
| Comunicación entre robots | 2.4 GHz, máx 100 mW EIRP |

## Communication Module — OBLIGATORIO en internacional

- Interfaz: **un pin GPIO de 2.54mm** para start/stop
- Alimentación: directo a batería (5.3V-25V) sin switch, 500mA mínimo
- Siempre encendido durante el partido
- No cuenta para peso ni altura
- Más info: https://github.com/robocup-junior/soccer-communication-module

## Documentación obligatoria

- BOM (con precios), Poster A1, Video técnico, Portfolio digital, Código compartido en GitHub
- Entrevistas: cada miembro debe explicar su rol técnico
- La documentación es parte del scoring

## Acciones requeridas

1. **URGENTE**: Verificar dribbler cumple zona captura 1.5cm
2. **IMPORTANTE**: Integrar Communication Module
3. **IMPORTANTE**: Preparar handle con 5cm clearance + top marker 4cm
4. **NECESARIO**: Empezar BOM con precios
5. **PREPARAR**: Poster, video, portfolio, entrevista
