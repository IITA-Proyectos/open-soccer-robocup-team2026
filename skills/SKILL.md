# ⚽ IITA RoboCupJunior Soccer Open — Skills Library

## Para IAs y desarrolladores

Esta biblioteca hace que cualquier IA sea un **programador experto en robots de fútbol RoboCup Junior Soccer Open**. Contiene skills específicos para Soccer + referencia a la biblioteca compartida de robótica de competencia en el repo hermano.

## Arquitectura del equipo IITA

```
Equipo: 2 robots autónomos
  Robot ARQUERO:  Teensy + OpenMV + BNO055 + motores omni
  Robot DELANTERO: Teensy + OpenMV + BNO055 + motores omni + kicker

Comunicación: WiFi/BT entre robots (ESP32)
Visión: OpenMV H7+ (detección de pelota naranja + arcos coloreados)
Campo: 182×243 cm, carpet verde, paredes negras 22cm, arcos cyan/magenta
Pelota: Golf ball naranja (42mm, sin IR para Soccer Open)
```

## Cuándo usar estos skills

Lee esta biblioteca SIEMPRE que el usuario pida ayuda con:
- Programación de robots de **fútbol RoboCup Junior**
- Estrategia de arquero o delantero
- Detección de pelota naranja (golf ball) con OpenMV
- Detección de arcos por color (cyan/magenta)
- Navegación en campo de soccer (líneas blancas, paredes negras)
- Comunicación entre arquero y delantero
- Control de dribbler y kicker

## Índice de Skills

### Soccer-Específicos (en este repo)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **⚽ Soccer Match FSM** | `skills/soccer-match-fsm.md` | **⭐⭐⭐ FSM completa: kickoff, attack, defend, search, penalty. Roles arquero/delantero. Comunicación de estado entre robots** |
| **🧤 Goalkeeper Strategy** | `skills/goalkeeper-strategy.md` | **⭐⭐ Movimiento lateral, predicción de tiro, blocking zone, clearing, reposicionamiento** |
| **⚡ Striker Strategy** | `skills/striker-strategy.md` | **⭐⭐ Behind-the-ball, orbit, dribble, shoot, approach angles, goal alignment** |
| **🎯 Soccer Ball Detection** | `skills/soccer-ball-detection.md` | **⭐⭐ Golf ball naranja con OpenMV LAB, arcos cyan/magenta, líneas blancas, confianza** |
| **🏠 Soccer Field Navigation** | `skills/soccer-field-navigation.md` | **⭐⭐ Campo 182×243cm, paredes negras, evitar líneas blancas, zonas del campo** |

### Biblioteca Compartida (repo hermano: `wro-2026-robosport-nacional-iita-salta`)

Estos skills son genéricos y aplican directamente a Soccer:

| Categoría | Skills disponibles | Referencia |
|-----------|-------------------|------------|
| **Fundamentos** | PID, FSM, RobustComm, Comm Diagnostics, Multi-Task Scheduler, **Parallel Sensing** | `skills/00-foundations/` |
| **IMU/Gyro** | BNO055 (básico, avanzado, non-blocking, field test), Dual-IMU, Heading Management | `skills/01-imu-gyroscope/` |
| **Movimiento** | Cinemática omni, drive diferencial, trayectorias | `skills/02-movement/` |
| **Fusión** | EKF, **Ball Tracking Avanzado** (Kalman+oclusión+predicción), ToF Array, Color Localization | `skills/03-sensor-fusion/` |
| **Visión** | OpenMV Pipeline | `skills/04-vision/` |
| **Multi-Robot** | Communication Protocol | `skills/06-multi-robot/` |
| **Operaciones** | Pre-Match Checklist | `skills/07-competition-ops/` |

> **Repo hermano:** https://github.com/IITA-Proyectos/wro-2026-robosport-nacional-iita-salta
> 
> Los skills de ese repo son AI-consumable y aplican directamente. Leer el `skills/SKILL.md` de ese repo para el índice completo.

## Diferencias clave Soccer vs RoboSports

| Aspecto | Soccer Open | RoboSports |
|---------|------------|------------|
| Campo | 182×243 cm | 2362×1143 mm |
| Paredes | Negras, 22 cm | Blancas, 100 mm |
| Pelota | 1 golf ball naranja (42mm) | 9 naranjas + 2 violetas ping-pong (40mm) |
| Objetivo | Meter goles | Empujar pelotas al otro lado |
| Arcos | Cyan y Magenta | No hay (rampa) |
| Roles | Arquero + Delantero (fijos) | 2 robots con roles dinámicos |
| Kicker | Sí (solenoide) | No (solo empujar) |
| Dribbler | Sí (spinner) | Opcional |
| Piso | Carpet verde oscuro | Mat impreso con líneas |
| Líneas | Blancas (bordes, centro, áreas) | Negras (grid, posiciones pelotas) |
| Duración | 10 min (2×5) | 1-2 min (aleatorio) |

## Plataformas

| Plataforma | Lenguaje | Uso |
|------------|----------|-----|
| **Teensy 4.1** | C++ | Control principal (PID, FSM, fusión) |
| **OpenMV H7+** | MicroPython | Visión (pelota, arcos, líneas) |
| **ESP32** | C++ | Comunicación entre robots |
| **PCB Zircon** | — | PCB custom del equipo IITA |

## Fuentes

- RoboCupJunior Soccer Rules 2026 (draft)
- IITA legacy 2025 season analysis (23 deficiencies, 12 recommendations)
- IITA RoboSports skills library (33 skills, 6 docs)
- CAMBADA, CMDragons, RoBorregos TDPs
