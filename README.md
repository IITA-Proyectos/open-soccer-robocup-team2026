# IITA - Open Soccer RoboCup Team 2026

> RoboCupJunior Soccer Open League — Temporada 2026  
> Equipo del [Instituto de Informática y Tecnología Aplicada (IITA)](https://github.com/IITA-Proyectos), Salta, Argentina  
> **Campeones nacionales RoboCupJunior Soccer — Diciembre 2025, Buenos Aires**  
> **Clasificados para RoboCup 2026 — Incheon, Corea del Sur — 30 Jun - 6 Jul 2026**

## Sobre este repositorio

Este es el repositorio central de ingeniería del equipo IITA para la competencia RoboCupJunior Soccer Open League 2026. Contiene todo el trabajo técnico: software, hardware, investigación, testing y documentación.

El equipo 2026 se basa en el trabajo realizado por el equipo 2025, cuyo código original se preserva en [`legacy/2025-season/`](legacy/2025-season/) y en el [repositorio original](https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025).

## Estructura del repositorio

```
open-soccer-robocup-team2026/
├── journal/                 # Diario de ingeniería (entradas cronológicas)
├── research/                # Pipeline de investigación
│   ├── backlog/             #   Temas pendientes de investigar
│   ├── in-progress/         #   En análisis
│   ├── completed/           #   Análisis terminados con conclusiones
│   └── references/          #   Proyectos externos para estudiar
├── testing/                 # Protocolos y resultados de pruebas
│   ├── protocols/           #   Procedimientos de prueba
│   └── results/             #   Resultados con fecha
├── hardware/                # Todo lo físico del robot
│   ├── electronics/         #   PCB, sensores, esquemáticos
│   ├── electrical/          #   Baterías, motores, drivers, potencia
│   └── mechanical/          #   Impresión 3D, piezas manuales, ensamblaje
├── software/                # Todo el código
│   ├── robot-arquero/       #   Software del arquero
│   ├── robot-delantero/     #   Software del delantero
│   ├── vision/              #   Código de cámara OpenMV
│   ├── communication/       #   Comunicación entre robots
│   └── libraries/           #   Librerías compartidas
├── competition/             # Reglas, cronograma, poster técnico
│   ├── rules/
│   └── timeline.md
└── legacy/                  # Contenido migrado de temporadas anteriores
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

*Repositorio mantenido por IITA — Instituto de Informática y Tecnología Aplicada, Salta, Argentina*

