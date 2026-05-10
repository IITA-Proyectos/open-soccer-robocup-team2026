---
name: vibe-robotics-coding
description: Use when writing or modifying embedded robotics firmware for the IITA soccer robots (Teensy 4.1 + OpenMV cameras + custom Zircon PCB + BNO055 IMU + IR sensors + motors). AI-accelerated coding with hard verification gates: compiles, fits in MCU, hardware-tested, no regressions on working subsystems. Specialised for the C++/Arduino-Teensy stack and MicroPython OpenMV stack.
---

# Vibe Robotics Coding — Firmware Embedded Acelerado con IA

> **Status: outline only — content pending iteration.**

## When to use

- Modificar código del robot en `software/robot-arquero/`, `software/robot-delantero/`, `software/libraries/`.
- Escribir código de visión OpenMV en `software/vision/`.
- Implementar protocolos de comunicación (UART OpenMV↔Teensy, ESP-NOW inter-robot, módulo árbitro).
- Agregar features (Kalman filter ball, FSM states, kicker control, dribbler PWM).

## When NOT to use

- Cambios de hardware sin firmware — usar `vibe-mechanical-design` o `vibe-pcb-design`.
- Cambios sólo a docs / playbooks — edición directa.
- Refactor cosmético sin justificación funcional.

## Verification gates (no negociable)

Toda modificación de firmware debe pasar:

1. **Compila** sin warnings (o con warnings explicados en el commit message).
2. **Cabe en el MCU** — flash/RAM usage check para Teensy 4.1 (1MB RAM, 8MB flash).
3. **Loop time check** — main loop sigue dentro del budget (objetivo: < 10ms para 100Hz, < 5ms si hay visión).
4. **No rompe subsistemas vecinos** — regression test sobre módulos que tocaba antes (motores, IMU, comm).
5. **Test en hardware real** documentado en `journal/`:
   - Setup (qué robot, qué condiciones, qué firmware version).
   - Criterio de aceptación medible (no "anda bien" — "el robot se posiciona behind-the-ball en menos de 2s desde STR_APPROACH").
   - Resultado observado.
   - Antes/después (video, log, foto cuando aplica).
6. **Atribución de IA** en commit (ver `AI-INSTRUCTIONS.md` sección 1).

## Pipeline esperado (planned content)

[TODO: desarrollar con el código real del robot]

1. **Intent capture** — qué tiene que hacer el código, en qué condiciones, qué interactúa.
2. **Search existing code** — el robot ya hace cosas; antes de generar nuevo, leer qué hay (incluye `playbooks/`/`skills/` para el patrón mental).
3. **Generación incremental** — IA propone diff pequeño, no rewrite. Excepción: cuando el código existente está fundamentalmente roto (raro).
4. **Static checks** — compilación, sintaxis, headers correctos, linker.
5. **Mental simulation** — paso a paso por cada estado de la FSM, valores límite (ball_dist = 0, ball_visible = false, partner_alive = false).
6. **Hardware test plan** — antes de flashear, plan concreto de validación.
7. **Hardware test execution** — alumno corre el plan, captura resultados con video/log.
8. **Journal entry** — qué se cambió, qué se midió, qué se aprendió, qué quedó pendiente.

## Stack actual

- **MCU principal:** Teensy 4.1 (ARM Cortex-M7, 600MHz, 1MB RAM, 8MB flash).
- **Visión:** OpenMV H7 / H7 Plus (MicroPython, ~30 FPS golf ball detection).
- **IMU:** BNO055 (deshabilitado en 2025; integración pendiente — ver Fase 2).
- **Sensores línea:** IR analógicos (3 sensores, A11 / A12 / A13).
- **Sensores pelota IR:** array IR pasivo para fallback (8 sensores, pines 14-23) — útil si la cámara pierde la pelota.
- **Comunicación:**
  - UART OpenMV↔Teensy (pines 0/1).
  - ESP-NOW inter-robot (a través de ESP32 externo).
  - Módulo árbitro RCJ obligatorio (UART, ver communication-module-integration playbook).
- **Actuadores:**
  - 3 motores TT con drivers H-bridge en placa Zircon (pinout depende de robot — ver journal del 2026-03-20).
  - Dribbler.
  - Solenoide kicker (solo delantero).

## Anti-patterns (qué NO hacer)

- ❌ Magic numbers sin explicación. Si ves `if (ball_dist < 250)`, agregá comentario o constante (`#define BALL_APPROACH_THRESHOLD_MM 250`).
- ❌ Cambios que tocan motores sin verificar que el robot no se va contra una pared.
- ❌ Asumir que un playbook describe el comportamiento actual sin verificar contra el código.
- ❌ Subir código sin compilar localmente primero.
- ❌ Test plan que dice "probar en cancha" sin criterio de aceptación.
- ❌ Refactor + feature en el mismo commit (separar siempre).

## Bugs conocidos a tener presente

[TODO: poblar a medida que se auditen los playbooks en Fase 1]

- `striker-strategy.md` (playbook) — behind-the-ball asume `goal_y = FIELD_LENGTH` fijo; no maneja polaridad de campo según qué arco defendemos.
- `multi-camera-world-model.md` (playbook) — `correct_position_from_goal()` tiene álgebra incorrecta.
- BNO055 — no funcionaba en 2025; revisar I2C + initialization sequence + magnetic interference.

## References

- `software/libraries/zirconLib/` — librería custom de la placa.
- `software/robot-arquero/definitivo-arquero_6-9-2026/` — código arquero actual.
- `software/robot-delantero/definitivo-delantero/` — código delantero actual.
- `journal/2026-03-20-diferencias-pines-motores-arquero-delantero.md` — gotcha conocido del pinout.
- `legacy/2025-season/` — código histórico (NO modificar; sólo lectura).
