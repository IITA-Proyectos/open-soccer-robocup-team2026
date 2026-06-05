# IITA - Open Soccer RoboCup Team 2026

> RoboCupJunior Soccer Open League — Temporada 2026  
> Equipo del [Instituto de Innovación y Tecnología Aplicada (IITA)](https://github.com/IITA-Proyectos), Salta, Argentina  
> **Campeones nacionales RoboCupJunior Soccer — Diciembre 2025, Buenos Aires**  
> **Clasificados para RoboCup 2026 — Incheon, Corea del Sur — 30 Jun - 6 Jul 2026**

## Sobre este repositorio

Este es el repositorio central de ingeniería del equipo IITA para la competencia RoboCupJunior Soccer Open League 2026. Contiene todo el trabajo técnico: software, hardware, investigación, testing y documentación.

El equipo 2026 se basa en el trabajo realizado por el equipo 2025, cuyo código original se preserva en [`legacy/2025-season/`](legacy/2025-season/) y en el [repositorio original](https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025).

## Estructura del repositorio

```
open-soccer-robocup-team2026/
├── docs/                    # Documentación técnica + índices operativos
│   ├── ESTADO-ACTUAL.md     #   ⭐ Estado vivo del robot (1ª lectura obligatoria)
│   ├── FUENTES-DE-VERDAD.md #   ⭐ Qué doc/módulo es canónico por tema
│   ├── ARQUITECTURA-3-PLACAS-2026.md  #  Arquitectura del robot 2026
│   ├── firmware/            #   Specs y contratos de datos por placa
│   ├── decisions/           #   Decisiones de diseño (fechadas)
│   └── superpowers/         #   Planes y specs de sprints
├── team-tasks/              # Tareas que requieren acción humana (TASK-NNN)
├── journal/                 # Diario de ingeniería (entradas cronológicas)
├── research/                # Pipeline de investigación (backlog → in-progress → completed)
├── testing/                 # Protocolos y resultados de pruebas
├── hardware/                # Todo lo físico del robot
│   ├── electronics/         #   PCB, sensores, esquemáticos + packs por subsistema
│   ├── electrical/          #   Baterías, motores, drivers, potencia
│   └── mechanical/          #   Impresión 3D, piezas manuales, ensamblaje
├── software/                # Todo el código
│   ├── teensy/Soccer 2026/  #   ⭐ Firmware VIVO 3 placas (PlatformIO) — src/{central,top,down,shared,diag}
│   ├── staging/             #   Sketches y experimentos por subsistema
│   ├── robot-arquero/       #   Material específico del arquero (ROBOT1)
│   ├── robot-delantero/     #   Material específico del delantero (ROBOT2)
│   ├── vision/              #   Código de cámara OpenMV
│   ├── communication/       #   Comunicación entre placas / robots
│   └── libraries/zirconLib/ #   Librería de la placa Zircon
├── skills/                  # Playbooks técnicos del dominio (knowledge base, no auto-invocables)
├── competition/             # Reglas, cronograma, poster técnico
│   ├── rules/
│   └── timeline.md
└── legacy/                  # Contenido migrado de temporadas anteriores (NO tocar)
    └── 2025-season/
```

## Equipo 2026

| Rol | Nombre | Notas |
|-----|--------|-------|
| Director del proyecto | Gustavo Viollaz (@gviollaz) | Coordinación IITA, dirección estratégica multi-temporada (Incheon 2026 → Nacional Nov 2026 → Mundial 2027) |
| Coach | Enzo Juarez (@enzzo195) | Revisión técnica y mentoreo del equipo |
| Competidora - Soccer Open | María Virginia Viollaz (@mariaviollaz) | Veterana temporada 2025, 18 años. Experiencia en visión artificial y trayectorias. Coach del equipo 2027 |
| Competidor - Soccer Open | Elías Cordero | Estudiante de robótica e Ing. Electromecánica (UNSa) |

Clasificados como **campeones nacionales** en la Roboliga Argentina (diciembre 2025, Buenos Aires).

## Arquitectura

El robot 2026 usa una **arquitectura distribuida de 3 placas** especializadas (CENTRAL Teensy 4.1 + ARRIBA Teensy 4.0 + ABAJO Teensy 4.0), conectadas por UART.

📐 **[Documento de arquitectura completo (lectura recomendada): `docs/ARQUITECTURA-3-PLACAS-2026.md`](docs/ARQUITECTURA-3-PLACAS-2026.md)** — qué hace cada placa, cómo se comunican, decisiones de diseño justificadas.

**Especificación funcional del firmware por placa** (`docs/firmware/`):
- [`FIRMWARE-PLACA-ABAJO.md`](docs/firmware/FIRMWARE-PLACA-ABAJO.md) — sensor de piso (32 sensores de luz + 2 OTOS odométricos)
- [`FIRMWARE-PLACA-ARRIBA.md`](docs/firmware/FIRMWARE-PLACA-ARRIBA.md) — cerebro sensorial (2 cámaras + 2 IMU + 4 ToF multizona + comm árbitros + partner ESP-NOW)
- [`FIRMWARE-PLACA-CENTRAL.md`](docs/firmware/FIRMWARE-PLACA-CENTRAL.md) — cerebro decisor + ejecutor (FSM táctica + 3 PIDs + cinemática inversa omni-3 + motores + encoders opcionales)

## 📦 Packs autocontenidos por subsistema

Para **programar, calibrar o diagnosticar** un subsistema específico del robot,
hay un **pack autocontenido** en `hardware/electronics/<subsistema>-pack/`
con TODO lo necesario en un solo lugar (docs + snapshot del firmware vivo +
tests + ground-truth):

| Pack | Para qué |
|---|---|
| 📦 [`hardware/electronics/down-board-pack/`](hardware/electronics/down-board-pack/) | Placa **DOWN** (32 sensores de línea + 2 OTOS) |
| 📦 [`hardware/electronics/central-board-pack/`](hardware/electronics/central-board-pack/) | Placa **CENTRAL** (Zircon, FSM + motores + PIDs) |
| 📦 [`hardware/electronics/top-board-pack/`](hardware/electronics/top-board-pack/) | Placa **TOP** (master de cámaras + IMU + ToF) |
| 📦 [`hardware/electronics/cameraFront-pack/`](hardware/electronics/cameraFront-pack/) | Cámara **OpenMV frontal** |
| 📦 [`hardware/electronics/cameraBack-pack/`](hardware/electronics/cameraBack-pack/) | Cámara **OpenMV trasera** |

👉 **Punto de entrada**: [`hardware/electronics/PACKS-INDEX.md`](hardware/electronics/PACKS-INDEX.md) — índice maestro con "qué pack abrir para qué tarea".

Cada pack tiene su propio `README.md` con un **índice "pregunta → doc"**. Si
vas a tocar un subsistema, abrir el pack antes que cualquier otro doc del
repo. Los packs son snapshots del 2026-05-24 — si contradicen al código vivo
en `software/teensy/Soccer 2026/src/...`, **gana el código vivo**.

## Cómo contribuir

Leer **[CONTRIBUTING.md](CONTRIBUTING.md)** antes de hacer cualquier cambio. Incluye reglas de atribución, uso de IA, y formato de commits.

## Para asistentes de IA

Si sos una IA trabajando en este repositorio, leé **[AI-INSTRUCTIONS.md](AI-INSTRUCTIONS.md)** primero. Contiene convenciones y reglas que debés seguir.

## Enlaces importantes

- **Reglas Soccer 2026 (DRAFT)**: https://robocup-junior.github.io/soccer-rules/2026-soccer-draft-rules/rules.html
- Reglas Soccer (versión estable): https://robocup-junior.github.io/soccer-rules/master/rules.html
- Reglas generales RoboCupJunior: https://junior.robocup.org/robocupjunior-general-rules/
- Especificaciones del campo: https://robocup-junior.github.io/soccer-rules/master/field_specification.html
- **Pelota IR nueva 2026 (42mm, open source)**: https://github.com/robocup-junior/ir-golf-ball
- Foro RoboCupJunior: https://junior.forum.robocup.org/
- Discord RoboCupJunior: https://robocup-junior.github.io/soccer-rules/discord/
- Awesome RCJ Soccer: https://github.com/robocup-junior/awesome-rcj-soccer
- Robot Soccer Kit (open source): https://github.com/robot-soccer-kit
- Recursos IITA - Robótica y Visión: https://github.com/IITA-Proyectos/resources-robotics-and-machine-vision-resources
- Roboliga Virtual Argentina: https://virtual.roboliga.ar/
- Reglas nacionales Argentina: https://docs.google.com/document/d/10VgMxnmTzKQiAOIgHjWHb9KzwLUDc66N0ZvooTC_tL8/edit?tab=t.0
- Repo Temporada 2025: https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025
- **RoboCup 2026 Incheon**: https://www.robocup.org/events/80

---

*Repositorio mantenido por IITA — Instituto de Innovación y Tecnología Aplicada, Salta, Argentina*

