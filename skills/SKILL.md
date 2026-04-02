# ⚽ IITA RoboCupJunior Soccer Open — Skills Library (12 Skills)

## Para IAs y desarrolladores

12 skills específicos Soccer + 6 documentos técnicos + referencia a 33 skills compartidos del repo hermano. Incluye los **comportamientos REALES actualmente programados** en los robots IITA.

## Arquitectura del equipo IITA

```
Equipo: 2 robots autónomos (3 ruedas omni cada uno)
  Robot ARQUERO:  Teensy + OpenMV(s) + BNO055 + 3 omni + ESP32 + Módulo RCJ
    Comportamiento: patrulla línea de fondo → rush a pelota → retreat inmediato
  Robot DELANTERO: Teensy + OpenMV(s) + BNO055 + 3 omni + kicker + ESP32 + Módulo RCJ
    Comportamiento: recorre cancha → approach pelota → behind-the-ball → push a arco

Comunicación primaria: ESP-NOW propio (~10ms latencia, 10 Hz WorldModel)
Comunicación oficial: Módulo RCJ Soccer (start/stop árbitro, UART, SuperTeam)
Visión: OpenMV H7+ (1-4 cámaras: pelota, arcos, obstáculos)
Campo: 182×243 cm, carpet verde, paredes negras 22cm, arcos cyan/magenta
Pelota: Golf ball naranja (42mm, pasiva, Soccer Vision 2026)
```

## Cuándo usar estos skills

Lee esta biblioteca SIEMPRE que el usuario pida ayuda con:
- Programación de robots de **fútbol RoboCup Junior**
- **Comportamiento actual de los robots IITA** (arquero patrol+rush, delantero search+push)
- **Jugadas de salida (kickoff set plays)** preparadas
- Estrategias de juego (formaciones, coordinación, pases, adaptación al rival)
- Multi-cámara y World Model, módulo de comunicación
- Movimiento omnidireccional, detección de pelota, navegación

## Índice de Skills (12)

### Estrategia y Comportamiento (6)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **🏆 Game Strategy Playbook** | `skills/game-strategy-playbook.md` | **⭐⭐⭐ Formaciones dinámicas, empuje coordinado, pases, adaptación al rival** |
| **🏁 Kickoff Set Plays** | `skills/kickoff-set-plays.md` | **⭐⭐⭐ 5 jugadas ensayadas: A=disparo directo, B=robo centro, C=pase lateral, D=muro, E=robo agresivo (favorita IITA). Tabla de selección por situación, análisis de timing (300ms al centro)** |
| **⚽ Soccer Match FSM** | `skills/soccer-match-fsm.md` | **⭐⭐⭐ FSM, roles, comunicación, módulo árbitro 2026** |
| **🧤 Goalkeeper Strategy** | `skills/goalkeeper-strategy.md` | **⭐⭐⭐ COMPORTAMIENTO IITA ACTUAL: FSM 3 estados (PATROL línea de fondo → RUSH a pelota cuando cerca+despejado → RETREAT inmediato). Predicción de tiro Kalman, clearing inteligente, calibración de rush** |
| **⚡ Striker Strategy** | `skills/striker-strategy.md` | **⭐⭐⭐ COMPORTAMIENTO IITA ACTUAL: FSM 4 estados (SEARCH recorriendo cancha → APPROACH a pelota → POSITION behind-the-ball opuesto a arco → PUSH empujar a arco). Orbit si está del lado equivocado** |

### Comunicación (1)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **📡 RCJ Communication Module** | `skills/communication-module-integration.md` | **⭐⭐⭐ Start/stop obligatorio, UART inter-robot, SuperTeam** |

### Visión y Percepción (3)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **👁️ Multi-Camera World Model** | `skills/multi-camera-world-model.md` | **⭐⭐⭐ WorldModel: fusión multi-cámara, Kalman oclusión, WiFi, rivales** |
| **🎯 Soccer Ball Detection** | `skills/soccer-ball-detection.md` | **⭐⭐ OpenMV: golf ball naranja LAB, arcos cyan/magenta** |
| **📡 LiDAR vs ToF** | `skills/lidar-vs-tof-positioning.md` | **⭐⭐ VL53L1X vs RPLidar, reglas 2026 permiten IR** |

### Movimiento (2)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **🔄 OmniDriveBase** | `skills/omni3-drive-base.md` | **⭐⭐⭐ API DriveBase 3 omni: drive(), move(), turn(), go_to()** |
| **🏠 Soccer Field Navigation** | `skills/soccer-field-navigation.md` | **⭐⭐ Campo 182×243cm, líneas IR, zonas, heading** |

## Documentación técnica (6)

| Documento | Path | Resumen |
|-----------|------|---------|
| **🏆 Game Strategy Analysis** | `docs/game-strategy-analysis.md` | **⭐⭐⭐ Reglas (multiple defense, pushing, forcing), 4 formaciones, tácticas, adaptación rival** |
| **📡 Módulo Comunicación RCJ** | `docs/communication-module-analysis.md` | **⭐⭐⭐ Hardware ESP32+display, pinout, SuperTeam** |
| **👁️ Multi-Camera World Model** | `docs/multi-camera-world-model.md` | **⭐⭐⭐ 1/2/4 cámaras, fusión, oclusión, pipeline 100Hz** |
| **🔄 Sistema Omni 3 Ruedas** | `docs/omni3-drive-system.md` | **⭐⭐⭐ Hardware, 4 niveles control, calibración** |
| **📡 LiDAR/ToF/SLAM** | `docs/lidar-tof-slam-analysis.md` | **⭐⭐⭐ Reglas 2026, RPLidar vs VL53L1X, no SLAM** |

### Biblioteca Compartida (repo hermano)

33 skills genéricos en `wro-2026-robosport-nacional-iita-salta`:

| Categoría | Skills clave |
|-----------|-------------|
| **Fundamentos** | PID, FSM, **RobustComm**, Multi-Task, **Parallel Sensing** |
| **IMU/Gyro** | BNO055 (básico, avanzado, **non-blocking**, field test), Dual-IMU |
| **Fusión** | EKF, **Ball Tracking Avanzado** (Kalman+oclusión+predicción), ToF Array |

> **Repo hermano:** https://github.com/IITA-Proyectos/wro-2026-robosport-nacional-iita-salta

## Fuentes principales

**IITA 2025 season** (comportamientos actuales en competencia), RoboCupJunior Soccer Rules 2026, RCJA Rules 2026, RCJ Forum, `robocup-junior/soccer-communication-module`, PCBWay (4 cameras), CAMBADA (MSL), IITA RoboSports library (33 skills, 6 docs)
