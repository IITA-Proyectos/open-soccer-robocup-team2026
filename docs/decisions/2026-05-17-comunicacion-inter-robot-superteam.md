---
title: "Decisión — Comunicación inter-robot en SuperTeam: NO implementar para Incheon (Opción A)"
date: 2026-05-17
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [decision, comunicacion, comm-board, superteam, incheon, esp-now]
robot: ambos
area: comunicacion
tipo: decision
related-tasks: [TASK-006]
related: [journal/2026-05-17-analisis-3-placas-y-correccion-firmware-c6.md, research/completed/2026-05-17-firmware-comm-rcj-branch-esp32-c6.md]
---

# Decision Record — Comunicación inter-robot en SuperTeam

## Contexto

Surgió al cargar el firmware oficial en la placa COMM (ESP32-C6): ¿cómo se
comunican los robots entre sí en SuperTeam, donde robots de distintos equipos
coordinan acciones? Se necesitaba decidir si el equipo construye un enlace de
datos robot-a-robot para Incheon 2026.

Dos verificaciones hechas (no suposiciones):

1. **Firmware oficial v0.91 (branch `esp32-c6`, commit `ffb4e3c`)** — revisado el
   código: usa solo `BLEDevice` para el árbitro. **Cero ESP-NOW, cero código
   inter-robot, cero "peer".** Solo: árbitro → módulo (BLE) → robot (nivel en
   OUT1/OUT2). Detalle en `journal/2026-05-17-analisis-3-placas-y-correccion-firmware-c6.md`.

2. **Reglas oficiales RCJ Soccer 2026** (`robocup-junior/soccer-rules`, branches
   `2026-soccer-draft-rules` y rama WIP del specsheet SuperTeam), citas literales:
   - *"The use of remote control of any kind is not allowed during the match.
     Robots must be controlled autonomously."* (`rules.adoc`, Control & Communication)
   - *"...Communications Module is required for referees to control the robots...
     interface with this module using a **single 2.54mm GPIO pin at present to
     start and stop the robots**... extending this to using UART for more complex
     applications **in future years**."* (`rules.adoc`, International Competition)
   - *"...make communication between multiple robots in a SuperTeam easier **in
     the future**... single 2.54mm GPIO pin **at present**... UART or I²C... **in
     future years**."* / *"Use of the communications modules will be mandatory
     for SuperTeam games."* (`superteam_rules.adoc`, Communication)
   - SuperTeam: coordinación = estrategia humana previa + posiciones (arquero,
     defensores, mediocampistas, delanteros) + IDs de robot (ej. `A-2`) para el
     árbitro + capitán ↔ árbitro. **No hay enlace de datos robot-a-robot definido.**

**Conclusión fáctica:** las reglas NO requieren ni proveen hoy comunicación de
datos robot-a-robot en SuperTeam. Lo obligatorio del módulo es **start/stop**.
El enlace inter-robot está explícitamente diferido al futuro por el comité.

## Opciones consideradas

### Opción A — NO implementar inter-robot para Incheon ✅ ELEGIDA
- **Pros:** cumple el reglamento (módulo start/stop ya es lo único obligatorio);
  riesgo cero de homologación; foco en robot autónomo honesto y en aprendizaje
  (alineado con la estrategia Incheon = inversión en aprendizaje, no podio);
  nada que mantener/depurar bajo presión en torneo.
- **Contras:** sin coordinación táctica automática entre robots del SuperTeam
  (se compensa con estrategia humana previa + roles/posiciones, que es
  justamente lo que el reglamento espera).

### Opción B — ESP32 dedicado aparte por robot, solo ESP-NOW
- **Pros:** no toca el módulo de árbitro (no arriesga su función obligatoria);
  ESP-NOW es el estándar de facto inter-robot en RCJ; reutilizable a 2027.
- **Contras:** ~2-3 días de trabajo + hardware extra por robot; sin valor para
  Incheon dado que SuperTeam no lo exige; sistema propio a validar contra reglas
  de interferencia/RF.

### Opción C — Modificar el firmware oficial y agregar ESP-NOW junto al BLE
- **Pros:** aprovecha el mismo ESP32-C6.
- **Contras:** coexistencia BLE + 802.15.4/WiFi en C6 más compleja; hay que
  garantizar que la función de árbitro (obligatoria) siga intacta; mayor
  superficie de fallo; sin retorno para Incheon.

## Decisión

**Se adopta la Opción A: el equipo NO implementa comunicación inter-robot para
Incheon 2026.** El módulo COMM se usa exclusivamente para su función
reglamentaria obligatoria: recibir start/stop del árbitro (BLE) y entregarlo al
robot por OUT1/OUT2. La coordinación en SuperTeam se resuelve como el
reglamento lo plantea: estrategia y roles/posiciones acordados entre los
equipos antes del partido, IDs para el árbitro, y el capitán como canal con la
mesa.

## Consecuencias

- **Lo que ganamos:** cumplimiento reglamentario garantizado, cero riesgo de
  homologación, foco y tiempo del equipo en lo que sí da puntos en Incheon
  (robot autónomo confiable + entregables de competencia), captura de
  aprendizaje documentada.
- **Lo que sacrificamos:** no hay jugadas coordinadas automáticas robot-a-robot
  en SuperTeam (aceptable: el reglamento no las exige y las difiere al futuro).
- **Capitalizable a 2027:** si el comité libera la extensión oficial
  (UART/I²C del módulo) o si se decide invertir, **Opción B** queda como camino
  recomendado y este registro deja la base técnica + reglamentaria lista. La
  arquitectura concreta de la Opción B (gateway ESP32-S3 colgado del bus CAN,
  con gate de modo por el `match_running` del árbitro) está detallada en
  `docs/decisions/2026-06-03-bus-can-general-y-flasheo-por-can.md` §7-bis (es
  forward-looking; NO cambia esta decisión: inter-robot sigue diferido).
- `TASK-006` "próximos pasos" (evaluar ESP-NOW post-mundial) queda subordinado a
  esta decisión: no se trabaja para Incheon.
- El playbook `skills/communication-module-integration.md` describe un
  `TeamMsg`/ESP-NOW que es **aspiracional y NO está en el firmware oficial** —
  tratarlo como diseño futuro, no como capacidad actual.

## Quién decidió y cuándo

- **Decisión tomada por:** Gustavo Viollaz (@gviollaz, coach) — 2026-05-17.
- **Análisis y registro:** Claude (Anthropic, Claude Opus 4.7 1M) a pedido de Gustavo.
- **Revisión recomendada:** antes del Nacional Argentina (nov 2026) y al
  publicarse las reglas SuperTeam finales 2026 (seguir `robocup-junior/soccer-rules`).

## Fuentes

- Reglas: `github.com/robocup-junior/soccer-rules` — `rules.adoc`,
  `superteam_rules.adoc` (branch `2026-soccer-draft-rules` y rama
  `dbscoach/add-superteam-comms-module-specsheet`).
- Firmware: `github.com/robocup-junior/soccer-communication-module` branch
  `esp32-c6` @ `ffb4e3c` (revisión de código: sin ESP-NOW/inter-robot).
- Internos: `journal/2026-05-17-analisis-3-placas-y-correccion-firmware-c6.md`,
  `research/completed/2026-05-17-firmware-comm-rcj-branch-esp32-c6.md`,
  `hardware/electronics/comm-board/`.
