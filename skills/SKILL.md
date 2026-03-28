# ⚽ IITA RoboCupJunior Soccer Open — Skills Library (9 Skills)

## Para IAs y desarrolladores

9 skills específicos Soccer + 4 documentos técnicos + referencia a 33 skills compartidos del repo hermano.

## Arquitectura del equipo IITA

```
Equipo: 2 robots autónomos (3 ruedas omni cada uno)
  Robot ARQUERO:  Teensy + OpenMV(s) + BNO055 + 3 motores omni + ESP32
  Robot DELANTERO: Teensy + OpenMV(s) + BNO055 + 3 motores omni + kicker + ESP32

Comunicación: ESP-NOW (peer-to-peer, ~10ms latencia, 10 Hz)
Visión: OpenMV H7+ (1-4 cámaras: pelota, arcos, obstáculos)
Campo: 182×243 cm, carpet verde, paredes negras 22cm, arcos cyan/magenta
Pelota: Golf ball naranja (42mm, pasiva, Soccer Vision 2026)
```

## Cuándo usar estos skills

Lee esta biblioteca SIEMPRE que el usuario pida ayuda con:
- Programación de robots de **fútbol RoboCup Junior**
- **Multi-cámara y World Model** (fusión de visión, oclusión, comunicación de equipo)
- Movimiento omnidireccional con 3 ruedas (OmniDriveBase)
- Estrategia de arquero o delantero
- Detección de pelota naranja con OpenMV, arcos cyan/magenta
- Navegación en campo de soccer, posicionamiento con ToF/LiDAR
- Comunicación entre arquero y delantero

## Índice de Skills (9)

### Visión y Percepción (3)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **👁️ Multi-Camera World Model** | `skills/multi-camera-world-model.md` | **⭐⭐⭐ WorldModel C++ class: fusión multi-cámara (resolver duplicados por proximidad en campo), Kalman predict bajo oclusión, comunicación WiFi con compañero (TeamMessage 30B/10Hz), arcos como landmarks, detección de rivales (obstáculo ≠ compañero), queries estratégicas (am_i_closer, ball_approaching_goal, rival_blocking). Loop completo** |
| **🎯 Soccer Ball Detection** | `skills/soccer-ball-detection.md` | **⭐⭐ OpenMV pipeline: golf ball naranja LAB, arcos cyan/magenta, calibración venue** |
| **📡 LiDAR vs ToF** | `skills/lidar-vs-tof-positioning.md` | **⭐⭐ Cuadro de decisión, VL53L1X para Soccer, RPLidar A1 con localización por paredes, detección de rivales. Reglas 2026: ToF/LiDAR ahora permitidos en Vision league** |

### Movimiento (2)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **🔄 OmniDriveBase** | `skills/omni3-drive-base.md` | **⭐⭐⭐ API DriveBase para 3 omni: drive(), move(dir,dist), turn(), go_to(x,y,h). Field-centric, PID gyro, odometría, corrección ToF** |
| **🏠 Soccer Field Navigation** | `skills/soccer-field-navigation.md` | **⭐⭐ Campo 182×243cm, sensores IR línea, zonas, heading** |

### Estrategia (4)

| Skill | Archivo | Resumen |
|-------|---------|--------|
| **⚽ Soccer Match FSM** | `skills/soccer-match-fsm.md` | **⭐⭐⭐ FSM completa, roles arquero/delantero, comunicación, módulo árbitro 2026** |
| **🧤 Goalkeeper Strategy** | `skills/goalkeeper-strategy.md` | **⭐⭐ Tracking lateral, predicción tiro Kalman, clearing, zona operación** |
| **⚡ Striker Strategy** | `skills/striker-strategy.md` | **⭐⭐ Behind-the-ball, orbit, shoot, búsqueda** |

## Documentación técnica (4)

| Documento | Path | Resumen |
|-----------|------|---------|
| **👁️ Multi-Camera World Model** | `docs/multi-camera-world-model.md` | **⭐⭐⭐ Configuraciones 1/2/4 cámaras (PCBWay usa 4), fusión multi-cámara, manejo de oclusión con Kalman, comunicación de equipo extendida, arcos como landmarks, detección de rivales, escenarios de decisión, pipeline 100Hz completo** |
| **🔄 Sistema Omni 3 Ruedas** | `docs/omni3-drive-system.md` | **⭐⭐⭐ Hardware, 4 niveles control, calibración, problemas comunes** |
| **📡 LiDAR/ToF/SLAM Analysis** | `docs/lidar-tof-slam-analysis.md` | **⭐⭐⭐ CAMBIO REGLAS 2026: ToF/LiDAR permitidos en Vision. RPLidar vs VL53L1X, por qué no SLAM, qué usan equipos top** |

### Biblioteca Compartida (repo hermano)

33 skills genéricos en `wro-2026-robosport-nacional-iita-salta`:

| Categoría | Skills clave |
|-----------|-------------|
| **Fundamentos** | PID, FSM, **RobustComm**, Multi-Task, **Parallel Sensing** |
| **IMU/Gyro** | BNO055 (básico, avanzado, **non-blocking**, field test), Dual-IMU |
| **Fusión** | EKF, **Ball Tracking Avanzado** (Kalman+oclusión+predicción+multi-pelota), ToF Array |
| **Visión** | OpenMV Pipeline |

> **Repo hermano:** https://github.com/IITA-Proyectos/wro-2026-robosport-nacional-iita-salta

## Fuentes principales

RoboCupJunior Soccer Rules 2026, PCBWay team (4 cameras, 3rd world 2022), CAMBADA (cooperative world model, MSL champions), ESP-NOW, Oliveira et al. (CMU 2008), Pybricks DriveBase, RoBorregos 2024, IITA RoboSports library (33 skills, 6 docs)
