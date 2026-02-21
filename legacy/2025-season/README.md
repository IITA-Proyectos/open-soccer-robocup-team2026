---
title: "Legado Temporada 2025"
date: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: true
status: final
tags: [legado, 2025, migracion]
---

# Legado - Temporada 2025

## Origen

Todo el contenido en esta carpeta proviene del repositorio original del equipo 2025:

**Repo original**: https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025

Este contenido fue migrado el 2026-02-21 por Claude (Anthropic) bajo supervisión de Gustavo Viollaz.

## IMPORTANTE: Esta carpeta es de solo lectura

**No modificar** los archivos en esta carpeta. Son referencia histórica del trabajo del equipo 2025. El código nuevo para 2026 va en `software/`.

## Equipo 2025

El equipo se dividió en subgrupos:

- **Movilidad**: @Vale15-tb, @mijaelb78, @vickyT049
- **OpenMV (visión)**: @3liasCo, @rafaelmoya2010, @roma1keta
- **Dribbler**: @lauri-an, @xio02, @Aczino593
- **Control**: @Leandro284-AR, @srmartbr17, @ASH101-CODE
- **Tutores**: @Martii3141, @sebalander

## Qué se migró

### Código (migrado como archivos de texto)

- `code/arquero/` — Código del robot arquero (Arduino/Teensy + zirconLib)
- `code/delantero/` — Código del robot delantero con máquina de estados
- `code/vision-openmv/` — Scripts MicroPython para OpenMV (detección de pelota, arcos, calibración)
- `code/control/` — Código de control (giro, seguidor de línea, integración)
- `code/libraries/zirconLib/` — Librería Arduino para la placa Zircon
- `code/misc/` — Scripts sueltos (sensores de línea, giroscopio, dribbler, etc.)

### Archivos binarios (referenciados por link, no migrados)

Los archivos binarios grandes (STL, GCode, XLSX, PDF, imágenes) permanecen en el repo original:

- **Diseños 3D (STL/SCAD/GCode)**: [Ver en repo 2025](https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025/tree/main/Dise%C3%B1o3D)
- **Reglas traducidas (PDF)**: [Ver en repo 2025](https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025/blob/main/SoccerRules-Robocup-2025%20%20Castellano.pdf)
- **Valores de sensores (XLSX)**: [Ver en repo 2025](https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025/blob/main/Control/valores%20de%20los%20sensores.xlsx)

## Arquitectura técnica del robot 2025

### Hardware
- **Microcontrolador**: Teensy (Arduino compatible)
- **Cámara**: OpenMV H7 / H7 Plus
- **Comunicación**: UART a 19200 baud entre OpenMV y Teensy
- **Motores**: 3 motores TT con drivers H-bridge
- **Sensores**: 3 sensores de línea (analógicos), giróscopo BNO055, ultrasonido
- **PCB custom**: "Zircon" (dos versiones: Mark1 y Naveen1)
- **Actuadores**: Dribbler (motor), solenoide (kick)

### Software
- **Protocolo UART**: 6 bytes - [header1=201][Xp][Yp][header2=202][Xa][Ya]
- **Visión**: Detección por color en espacio LAB, transformación homográfica
- **Comportamiento**: Máquina de estados (GIRANDO → AVANZANDO → CENTRANDO → PATEANDO)
