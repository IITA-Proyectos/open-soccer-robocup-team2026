# ⚽ IITA RoboCupJunior Soccer Open — Skills Library (7 Skills)

## Para IAs y desarrolladores

Esta biblioteca hace que cualquier IA sea un **programador experto en robots de fútbol RoboCup Junior Soccer Open**. 7 skills específicos Soccer + referencia a 33 skills compartidos del repo hermano.

## Arquitectura del equipo IITA

```
Equipo: 2 robots autónomos (3 ruedas omni cada uno)
  Robot ARQUERO:  Teensy + OpenMV + BNO055 + 3 motores omni
  Robot DELANTERO: Teensy + OpenMV + BNO055 + 3 motores omni + kicker

Comunicación: WiFi/BT entre robots (ESP32)
Visión: OpenMV H7+ (detección de pelota naranja + arcos coloreados)
Campo: 182×243 cm, carpet verde, paredes negras 22cm, arcos cyan/magenta
Pelota: Golf ball naranja (42mm, sin IR para Soccer Open)
```

## Cuándo usar estos skills

Lee esta biblioteca SIEMPRE que el usuario pida ayuda con:
- Programación de robots de **fútbol RoboCup Junior**
- **Movimiento omnidireccional con 3 ruedas (OmniDriveBase)**
- Estrategia de arquero o delantero
- Detección de pelota naranja (golf ball) con OpenMV
- Detección de arcos por color (cyan/magenta)
- Navegación en campo de soccer (líneas blancas, paredes negras)
- Comunicación entre arquero y delantero

## Índice de Skills

### Soccer-Específicos (en este repo)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **🔄 OmniDriveBase** | `skills/omni3-drive-base.md` | **⭐⭐⭐ API estilo Pybricks DriveBase para 3 ruedas omni. drive(), move(dir,dist), turn(), go_to(x,y,h). Traslación+rotación independientes, field-centric con gyro, odometría, corrección ToF. Clase C++ completa** |
| **⚽ Soccer Match FSM** | `skills/soccer-match-fsm.md` | **⭐⭐⭐ FSM: kickoff, attack, defend, search. Roles arquero/delantero. Comunicación inter-robot. Módulo árbitro 2026** |
| **🧤 Goalkeeper Strategy** | `skills/goalkeeper-strategy.md` | **⭐⭐ Tracking lateral, predicción de tiro (Kalman), clearing, zona de operación** |
| **⚡ Striker Strategy** | `skills/striker-strategy.md` | **⭐⭐ Behind-the-ball, orbit, shoot, detección de arcos, patrón de búsqueda** |
| **🎯 Soccer Ball Detection** | `skills/soccer-ball-detection.md` | **⭐⭐ Golf ball naranja OpenMV LAB, arcos cyan/magenta, calibración en venue** |
| **🏠 Soccer Field Navigation** | `skills/soccer-field-navigation.md` | **⭐⭐ Campo 182×243cm, sensores de línea IR, zonas del campo, heading** |

### Documentación técnica (en este repo)

| Documento | Path | Resumen |
|-----------|------|---------|
| **🔄 Sistema Omni 3 Ruedas** | `docs/omni3-drive-system.md` | **⭐⭐⭐ Por qué 3 omni, hardware típico, 4 niveles de control, calibración paso a paso, consumo por dirección, problemas comunes** |

### Biblioteca Compartida (repo hermano: `wro-2026-robosport-nacional-iita-salta`)

33 skills genéricos que aplican directamente a Soccer:

| Categoría | Skills disponibles |
|-----------|-------------------|
| **Fundamentos** | PID, FSM, RobustComm, Comm Diagnostics, Multi-Task Scheduler, **Parallel Sensing** |
| **IMU/Gyro** | BNO055 (básico, avanzado, **non-blocking**, field test), Dual-IMU, Heading Mgmt |
| **Movimiento** | Cinemática omni (genérica), drive diferencial, trayectorias |
| **Fusión** | EKF, **Ball Tracking Avanzado** (Kalman+oclusión+predicción), ToF Array, Color Localization |
| **Visión** | OpenMV Pipeline |
| **Multi-Robot** | Communication Protocol |
| **Operaciones** | Pre-Match Checklist |

> **Repo hermano:** https://github.com/IITA-Proyectos/wro-2026-robosport-nacional-iita-salta

## Plataformas

| Plataforma | Lenguaje | Uso |
|------------|----------|-----|
| **Teensy 4.1** | C++ | Control principal (PID 1kHz, OmniDriveBase, FSM) |
| **OpenMV H7+** | MicroPython | Visión (pelota, arcos, líneas) |
| **ESP32** | C++ | Comunicación entre robots |
| **PCB Zircon** | — | PCB custom del equipo IITA |

## Fuentes

RoboCupJunior Soccer Rules 2026, IITA legacy 2025 analysis, IITA RoboSports library (33 skills), Oliveira et al. (CMU 2008), Pybricks DriveBase, Modern Robotics (Northwestern), CAMBADA, CMDragons, RoBorregos
