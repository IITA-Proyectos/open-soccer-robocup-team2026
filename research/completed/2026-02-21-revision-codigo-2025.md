---
title: "Revisión completa del código 2025"
date: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
status: completed
priority: alta
tags: [software, revision, legado, analisis]
---

# Revisión completa del código 2025

## Resumen ejecutivo

Se revisó todo el código del repositorio 2025. El software está en estado de prototipo funcional con varios bugs y código incompleto. La arquitectura básica (OpenMV → UART → Teensy → motores) es correcta. La máquina de estados del delantero es la pieza más completa.

## Bugs y problemas identificados

| # | Severidad | Descripción | Ubicación |
|---|-----------|-------------|----------|
| 1 | CRÍTICO | Variable `potencia` no definida en arquero | arquero-base.ino |
| 2 | CRÍTICO | Dos bloques setup()/loop() en el mismo archivo | arquero-base.ino |
| 3 | ALTO | Bug lógico en estado PATEANDO (`<=` debería ser `>=`) | perseguir-pelota.ino |
| 4 | ALTO | Sin detección de línea en delantero (se sale de cancha) | perseguir-pelota.ino |
| 5 | ALTO | Giróscopo BNO055 deshabilitado en todo el código | zirconLib + todos |
| 6 | MEDIO | Llave extra al final de zirconLib.cpp | zirconLib.cpp |
| 7 | MEDIO | Thresholds de visión hardcodeados | scripts OpenMV |
| 8 | BAJO | Nombres de archivos con espacios y sin extensión | Todo el repo 2025 |

## Conclusiones

1. **El delantero es la base más sólida** para iterar. La máquina de estados funciona conceptualmente.
2. **El arquero necesita reescritura** — el código actual no compila.
3. **La visión funciona** pero depende de calibración manual y thresholds fijos.
4. **El giróscopo nunca se integró exitosamente** — está comentado en todas partes.
5. **La placa Zircon es funcional** pero necesita evaluación física.
6. **Falta comunicación entre robots** (arquero-delantero no se coordinan).

Ver análisis detallado en gviollaz/open-soccer-robocup-team2026 (versión extendida).
