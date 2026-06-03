# Referencias Externas — IITA Salta Soccer Open 2026

> Centralizado de links y recursos externos del proyecto.

## Normas y estándares IITA

| Recurso | Link | Descripción |
|---------|------|-------------|
| **ICRS Playbook** | [iita-competition-playbook](https://github.com/IITA-Proyectos/iita-competition-playbook) | Políticas y estándares para repos de competencia IITA |
| **ICRS Repo Policy** | [iita-competition-repo-policy.md](https://github.com/IITA-Proyectos/iita-competition-playbook/blob/master/docs/es/policies/iita-competition-repo-policy.md) | Estándar ICRS v1.1 — estructura, flujos, documentación |
| **AI Ethics & Use** | [ai-ethics-and-use.md](https://github.com/IITA-Proyectos/iita-competition-playbook/blob/master/docs/es/policies/ai-ethics-and-use.md) | Política de uso de IA en equipos IITA |
| **Third Party & Sources** | [third-party-content-and-sources.md](https://github.com/IITA-Proyectos/iita-competition-playbook/blob/master/docs/es/policies/third-party-content-and-sources.md) | Política de fuentes y contenido de terceros |
| **Visibility & Security** | [visibility-and-security.md](https://github.com/IITA-Proyectos/iita-competition-playbook/blob/master/docs/es/policies/visibility-and-security.md) | Política de visibilidad y seguridad |
| **Competition Template** | [iita-competition-template](https://github.com/IITA-Proyectos/iita-competition-template) | Repo template ICRS para crear nuevos repos de equipo |

## Repos hermanos IITA 2026

| Equipo | Repo | Categoría |
|--------|------|-----------|
| IITA Salta Rescue Line | [rcj-2026-rescue-line-iita-salta-robocup](https://github.com/IITA-Proyectos/rcj-2026-rescue-line-iita-salta-robocup) | RCJ Rescue Line |

## Competencia

| Recurso | Link | Tipo |
|---------|------|------|
| Reglas Soccer Open 2025 (castellano) | [PDF en repo 2025](https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025/blob/main/SoccerRules-Robocup-2025%20%20Castellano.pdf) | Reglamento |
| RoboCup Junior Soccer Rules (oficial) | [robocupjunior.org](https://junior.robocup.org/rcj-soccer/) | Reglamento |

## Hardware y datasheets

> Arquitectura completa: [`docs/ARQUITECTURA-3-PLACAS-2026.md`](docs/ARQUITECTURA-3-PLACAS-2026.md). Specs de firmware por placa en [`docs/firmware/`](docs/firmware/).

| Componente | Link | Uso en el robot |
|-----------|------|-----------------|
| Teensy 4.1 (PJRC) | [pjrc.com/teensy](https://www.pjrc.com/teensy/) | Micro de la placa **CENTRAL** (FSM táctica + PIDs + motores) |
| Teensy 4.0 (PJRC) | [pjrc.com/teensy](https://www.pjrc.com/teensy/) | Micro de placas **TOP** y **DOWN** (2× por robot) |
| ESP32-C6 (Espressif) | [espressif.com/.../esp32-c6](https://www.espressif.com/en/products/socs/esp32-c6) | Placa **COMM**: señales de árbitro RCJ (START/STOP) + partner |
| OpenMV H7 / H7 Plus | [openmv.io](https://openmv.io/) | Cámaras de visión frontal + trasera (MicroPython) |
| BNO055 (Adafruit) | [Adafruit BNO055](https://learn.adafruit.com/adafruit-bno055-absolute-orientation-sensor) | IMU / giróscopo (orientación, placa TOP) |
| SparkFun OTOS (Qwiic) | [Arduino Library](https://github.com/sparkfun/SparkFun_Qwiic_OTOS_Arduino_Library) | 2× odometría óptica (placa DOWN) |
| ST VL53L7CX (ToF multizona) | Ver [`docs/firmware/FIRMWARE-PLACA-ARRIBA.md`](docs/firmware/FIRMWARE-PLACA-ARRIBA.md) | Distancia/obstáculos multizona (placa TOP) |
| Anillo 32 sensores de línea | Ver [`hardware/electronics/down-board-pack/`](hardware/electronics/down-board-pack/) | Detección de línea de borde (placa DOWN) |
| PCB Zircon (custom) | Ver [`hardware/electronics/central-board-pack/`](hardware/electronics/central-board-pack/) | Placa custom CENTRAL del equipo (+ `zirconLib`) |

## Equipos referencia Soccer Open

| Equipo | Link | Notas |
|--------|------|-------|
| _(Pendiente investigación)_ | | Analizar TDPs de equipos top 2024-2025 |

---

**Instrucciones:** Mantener actualizado. Agregar links a medida que se descubren recursos útiles.
