# ⚽ IITA RoboCupJunior Soccer Open — Skills Library (10 Skills)

## Para IAs y desarrolladores

10 skills específicos Soccer + 5 documentos técnicos + referencia a 33 skills compartidos del repo hermano.

## Arquitectura del equipo IITA

```
Equipo: 2 robots autónomos (3 ruedas omni cada uno)
  Robot ARQUERO:  Teensy + OpenMV(s) + BNO055 + 3 omni + ESP32 + Módulo RCJ
  Robot DELANTERO: Teensy + OpenMV(s) + BNO055 + 3 omni + kicker + ESP32 + Módulo RCJ

Comunicación primaria: ESP-NOW propio (~10ms latencia, 10 Hz WorldModel)
Comunicación oficial: Módulo RCJ Soccer (start/stop árbitro, UART inter-robot, SuperTeam)
Visión: OpenMV H7+ (1-4 cámaras: pelota, arcos, obstáculos)
Campo: 182×243 cm, carpet verde, paredes negras 22cm, arcos cyan/magenta
Pelota: Golf ball naranja (42mm, pasiva, Soccer Vision 2026)
```

## Cuándo usar estos skills

Lee esta biblioteca SIEMPRE que el usuario pida ayuda con:
- Programación de robots de **fútbol RoboCup Junior**
- **Módulo de comunicación oficial** (start/stop, UART, SuperTeam)
- Multi-cámara y World Model (fusión de visión, oclusión, comunicación de equipo)
- Movimiento omnidireccional con 3 ruedas (OmniDriveBase)
- Estrategia de arquero o delantero
- Detección de pelota naranja con OpenMV, arcos cyan/magenta
- Navegación, posicionamiento con ToF/LiDAR

## Índice de Skills (10)

### Comunicación (1)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **📡 RCJ Communication Module** | `skills/communication-module-integration.md` | **⭐⭐⭐ Módulo oficial: start/stop obligatorio (GPIO), UART inter-robot via wireless del módulo (4 canales), TeamMsg 14B con checksum, SuperTeam protocol (11B/5Hz), wiring diagram, canales A0/A1, decisión módulo vs ESP-NOW (usar ambos)** |

### Visión y Percepción (3)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **👁️ Multi-Camera World Model** | `skills/multi-camera-world-model.md` | **⭐⭐⭐ WorldModel class: fusión multi-cámara, Kalman oclusión, WiFi compañero, arcos landmarks, rivales, queries estratégicas** |
| **🎯 Soccer Ball Detection** | `skills/soccer-ball-detection.md` | **⭐⭐ OpenMV: golf ball naranja LAB, arcos cyan/magenta, calibración** |
| **📡 LiDAR vs ToF** | `skills/lidar-vs-tof-positioning.md` | **⭐⭐ VL53L1X vs RPLidar, reglas 2026 permiten IR en Vision** |

### Movimiento (2)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **🔄 OmniDriveBase** | `skills/omni3-drive-base.md` | **⭐⭐⭐ API DriveBase 3 omni: drive(), move(), turn(), go_to()** |
| **🏠 Soccer Field Navigation** | `skills/soccer-field-navigation.md` | **⭐⭐ Campo 182×243cm, líneas IR, zonas, heading** |

### Estrategia (4)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **⚽ Soccer Match FSM** | `skills/soccer-match-fsm.md` | **⭐⭐⭐ FSM, roles, comunicación, módulo árbitro 2026** |
| **🧤 Goalkeeper Strategy** | `skills/goalkeeper-strategy.md` | **⭐⭐ Tracking lateral, predicción tiro, clearing** |
| **⚡ Striker Strategy** | `skills/striker-strategy.md` | **⭐⭐ Behind-the-ball, orbit, shoot** |

## Documentación técnica (5)

| Documento | Path | Resumen |
|-----------|------|---------|
| **📡 Módulo Comunicación RCJ** | `docs/communication-module-analysis.md` | **⭐⭐⭐ Hardware ESP32+display, pinout completo, start/stop, UART inter-robot (4 canales), penalizaciones, SuperTeam (5 robots Big Field), nadie usa world state compartido aún, futuro 2027+, recomendaciones** |
| **👁️ Multi-Camera World Model** | `docs/multi-camera-world-model.md` | **⭐⭐⭐ 1/2/4 cámaras, fusión, oclusión Kalman, pipeline 100Hz** |
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

RoboCupJunior Soccer Rules 2026, `robocup-junior/soccer-communication-module` (GitHub), SuperTeam Rules 2026, PCBWay team (4 cameras, 3rd world 2022), CAMBADA (MSL), ESP-NOW, IITA RoboSports library (33 skills, 6 docs)
