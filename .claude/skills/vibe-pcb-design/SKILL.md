---
name: vibe-pcb-design
description: Use when designing custom PCBs for the robot (sensor breakouts, motor driver expansions, MCU base boards, IR sensor arrays) using AI-accelerated KiCad pipelines. From schematic intent to gerber-ready board with verification gates (DRC clean, footprint match, BOM sanity, JLCPCB-compatible). The robot already uses a custom "Zircon" board — this skill applies for new boards or revisions.
---

# Vibe PCB Design — Pipeline IA → Placa Fabricable

> **Status: outline only — content pending iteration.**

## When to use

- Diseñar una placa nueva (sensor breakout, expansión, daughter board).
- Revisar la placa Zircon (Rev v15 actual) para una revisión vXX.
- Auditoría de schematics existentes en `hardware/electronics/`.
- Diseñar un PCB de IR sensor array para detección de pelota / línea.

## When NOT to use

- Cambios al firmware sin tocar hardware — usar `vibe-robotics-coding`.
- Cambios mecánicos puros — usar `vibe-mechanical-design`.
- Decisiones de sourcing puras (sin diseño nuevo).

## Pipeline esperado (planned content)

[TODO: desarrollar con la placa Zircon como referencia base]

1. **Intent capture** — qué hace la placa:
   - Qué se conecta (lista de I/O, alimentación).
   - Restricciones de tamaño (¿cabe en el chasis?).
   - Costo target (Argentina / JLCPCB).
2. **Component selection** — IA propone components con sourcing:
   - Mouser / DigiKey (para componentes que se piden internacionales).
   - JLCPCB Parts Library (para assembly automático).
   - Sourceables en Argentina (cuando aplica).
3. **Schematic generation** — KiCad schematic estructurado:
   - Jerárquico si tiene sentido (módulos: power, sensors, comm, MCU).
   - Net naming consistente.
   - Bypass caps en cada IC.
4. **PCB layout** — IA orienta routing, IA verifica DRC.
5. **Verification gates antes de fabricar:**
   - DRC clean (sin errores de design rule).
   - Footprint match contra datasheets reales (no contra LCSC genéricos).
   - BOM sanity check (componentes sourceables, costo razonable, alternativas).
   - Compatibilidad con casa de fabricación (JLCPCB / PCBWay / local).
   - Test points presentes para debugging.
   - Power rail decoupling correcto.
   - Conectores con polarización (no se pueden enchufar al revés).
6. **Output** — schematics + Gerbers + BOM + pick&place en `hardware/electronics/board-name-revXX/`.

## Verification gates (no negociable)

- DRC clean antes de exportar gerbers.
- Cada IC con su decoupling cap (regla del pulgar: 100nF cerca de cada VCC pin).
- Cada conector con label de pinout (en silkscreen).
- BOM con part numbers reales y links.
- Para revisiones de Zircon: regression test contra firmware existente (`zirconLib`).
- Test point en cada power rail crítico (3.3V, 5V, motor V+).

## Context: placa Zircon

[TODO: capturar info concreta de Zircon Rev v15 desde `hardware/electronics/Zircon.pdf`]

Lo que ya sabemos:
- **MCU:** Teensy 4.1 (montado, no integrado).
- **Drivers de motor:** U5, U7, U17 (3 drivers H-bridge para 3 motores omni).
- **Pinout maestro:** documentado en `software/libraries/zirconLib/zirconLib.cpp` y `journal/2026-03-20-diferencias-pines-motores-arquero-delantero.md`.
- **Diferencia entre robots:** mismo PCB pero ROBOT1 (arquero) y ROBOT2 (delantero) usan pines lógicos diferentes — definido vía `#define ROBOT1` / `#define ROBOT2`.

Si se hace una revisión Zircon:
- Considerar uniformar pinout entre arquero y delantero (eliminar las divergencias documentadas el 2026-03-20).
- Considerar agregar lo que falta en Rev v15 (¿ESP32 onboard? ¿conector dedicado para módulo árbitro RCJ?).

## JLCPCB compatibility checklist

- [ ] Footprints en LCSC parts library (preferentemente "basic parts" para evitar setup fee).
- [ ] Min trace width / spacing dentro de capabilities (>= 6mil/6mil para 1-2 layer).
- [ ] Min via (>= 0.3mm drill).
- [ ] Componentes no en biblioteca → marcados como "manual assembly".
- [ ] Edge clearance >= 0.5mm.
- [ ] Pick&place con orientación correcta.

## References

- `hardware/electronics/Zircon.pdf` — schematic actual Rev v15.
- `software/libraries/zirconLib/zirconLib.cpp` — firmware library que asume el pinout.
- `journal/2026-03-20-diferencias-pines-motores-arquero-delantero.md` — divergencias entre robots.
- `AI-INSTRUCTIONS.md` sección 7.
