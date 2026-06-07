# r-d-2027 — Investigación y Desarrollo para versiones futuras (2027+)

> ⚠️ **ESTA CARPETA NO ES EL ROBOT DE INCHEON 2026.**
> Es un subproyecto **separado y experimental** del IITA Soccer Open con ideas,
> diseños y prototipos para versiones futuras (Nacional 2026 / Internacional
> 2027). **NO mezclar con el código de competencia.** No tocar nada de acá
> mientras se prepara Incheon — para eso está `software/`, `docs/`, `team-tasks/`.

## Por qué existe

El equipo va acumulando ideas que valen para "la próxima vuelta" pero que NO
deben distraer ni confundir al desarrollo principal:

- **Comunicación inter-robot** real (SuperTeam coordinado).
- **Telemetría inalámbrica estilo F1** (sensores en vivo durante entrenamiento
  para analizar y mejorar el software más rápido).
- **Bus CAN troncal** entre las 3 Teensy (TOP / CENTRAL / DOWN) reemplazando
  los UART punto-a-punto, con un ESP32 gateway que selectivamente puentea al
  exterior por WiFi y al robot compañero por ESP-NOW.

Estos temas YA estaban en el roadmap (`docs/competencia/MEJORAS-PENDIENTES.md`
items E4) pero estaban dispersos. Esta carpeta los junta y los elabora.

## Reglas duras

1. **Aislamiento total.** Nada acá modifica el firmware/software de competencia.
   El código de `r-d-2027/code/` se compila standalone (su propio `platformio.ini`
   o equivalente) y NO se incluye desde `software/teensy/Soccer 2026/`.
2. **Las TASKs de r-d-2027 viven acá** (no en `team-tasks/`) — para no inflar
   la lista de tareas activas de Incheon.
3. **La cadencia del repo principal manda.** Si esto distrae a alumnos del
   trabajo de Incheon, se pausa y listo. Es R&D, no compromiso.
4. **Nombres claros:** todo archivo en `r-d-2027/` empieza con la convención
   `YYYY-MM-DD-...` o tiene `r-d-2027` en la ruta para que un grep nunca lo
   confunda con producción.
5. **Decisiones a futuro NO se aplican retroactivamente** al diseño actual.
   Si un cambio de acá genera valor inmediato, se promueve con un decision
   record propio en `docs/decisions/` y se discute aparte.

## Contenido

| Archivo | Qué es |
|---|---|
| `README.md` | Este archivo (índice + reglas). |
| `roadmap.md` | Mapa de fases, dependencias, qué viene cuándo. |
| `decisions/2026-06-07-esp32-telemetry-bridge.md` | **Puente ESP32 telemetría WiFi v0** (decisión + por qué; algo simple que se podría hacer YA para acelerar puesta a punto, sin romper nada). |
| `decisions/2026-06-07-can-gateway-architecture.md` | **Arquitectura CAN troncal + ESP32 gateway** (decisión + por qué; el modelo grande para 2027). |
| `specs/esp32-telemetry-bridge-v0-spec.md` | Spec técnica detallada del puente WiFi v0. |
| `specs/can-gateway-full-v1-spec.md` | Spec técnica detallada del modelo CAN+gateway completo. |
| `code/esp32-bridge-firmware/` | Firmware Arduino/PlatformIO completo del ESP32 puente (compila standalone). |
| `code/teensy-glue-snippet/` | Snippet de ejemplo de cómo cablearlo a la telemetría existente (NO aplicar al firmware actual sin decisión). |
| `code/pc-udp-listener/` | Listener Python standalone para recibir el stream por UDP. |

## Cómo se "desarrolla" esto sin romper Incheon

- El código de `code/` compila aislado (no afecta los envs de `pio run -e top/down/central_robot1/central_robot2`).
- Si llega a probarse en un robot real, se hace en **branch separada**
  (`rd-2027-...`) y solo se mergea a `main` si pasa una decisión explícita
  del coach (Gustavo). Por ahora vive en `main` solo como documentación +
  carpeta de R&D — no entra a la compilación del binario de competencia.
- Cualquier cambio a archivos de `software/teensy/Soccer 2026/` que sea
  necesario para integrar esto es **propuesto** en los specs (con diff de
  ejemplo en `teensy-glue-snippet/`) pero NO aplicado.

## Dueño

Este subproyecto lo coordina Gustavo (coach). Los alumnos que quieran
contribuir lo hacen **explícitamente y fuera de su tiempo de Incheon**, con el
visto bueno del coach.

---
*Última actualización: 2026-06-07.*
