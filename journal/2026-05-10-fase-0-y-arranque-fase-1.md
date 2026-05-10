---
title: "Sesión 2026-05-10 — Cierre Fase 0, arranque Fase 1, descubrimiento arquitectura 3 placas"
date: 2026-05-10
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [coaching, organizacion, arquitectura, firmware, decisiones, kickoff]
robot: ambos
area: organizacion
tipo: decision
---

# Sesión 2026-05-10 — Cierre Fase 0, arranque Fase 1

## Contexto

Primera sesión larga con frame coach senior activo. Se cerró la Fase 0 (setup del sistema de trabajo con skills locales) y se arrancó la Fase 1 (auditoría de playbooks), que pivoteó a "diseño de firmware nuevo para las 3 placas chinas que llegaron".

## Lo hecho

### Fase 0 completa

- `CLAUDE.md` raíz (frame coach + estrategia multi-temporada Incheon-Nacional-Mundial 2027).
- `.claude/skills/` con 8 skills Claude reales:
  - `rcj-soccer-coach`, `rcj-judging-package`
  - `vibe-mechanical-design`, `vibe-pcb-design`, `vibe-robotics-coding`
  - `hardware-test-protocol`, `engineering-journal`, `openmv-vision-tuning`
- Spec del plan en `docs/coach-system/2026-05-10-fase-0-setup-design.md`.
- Memorias estratégicas guardadas en `~/.claude/projects/.../memory/`.

### Auditoría striker-strategy (Fase 1, ítem 1)

- Archivo: `research/in-progress/2026-05-10-auditoria-striker-strategy.md`.
- 8 temas-a-analizar detectados (2× P0, 4× P1, 2× P2).
- **Honestidad coach:** ~60% solapa con `docs/internal/analisis-definitivo-delantero.md` que ya existía desde marzo 2026 y yo no había abierto.
- Hallazgos originales: T2 (Xp==0 == "no pelota"), T4 (playbook desconectado del código real), T8 (ángulos arcos sin uso).
- **Bugs P0 ya documentados en marzo que necesitan verificarse**: gap mortal del arquero (`3 < |Yp| < 5` → congelado), uso de `currentYaw` raw vs `error` normalizado.

### Descubrimiento arquitectura 3 placas nuevas

- BOMs y schematics decodificados de `hardware/electronics/pcb_design/{top,down}_board/`.
- Documentos nuevos:
  - `hardware/electronics/mapa-pines-placas-nuevas.md` — pinout decodificado (inferido del schematic visual, marcado como borrador).
  - `research/in-progress/2026-05-10-diseno-firmware-3-placas.md` — diseño preliminar de firmware.

### Decisiones confirmadas con coach (Q1-Q7)

- **Q1 — Motores:** Zircon Rev v15 + Teensy 4.1 sigue activo como **motor server** (recibe `MotorCommand` por UART del TOP). La placa DOWN tiene forma irregular para dejar espacio físico a los motores que vienen del Zircon.
- **Q2 — COMM:** copia 100% del módulo oficial RCJ (ESP32 + OLED + acelerómetro + 2 botones). Firmware oficial RCJ a cargar.
- **Q4 — ToF:** VL53L5CX comprados pero pendientes de arribo. VL53L7CX posiblemente en stock. Hito 3 arranca sin ToF.
- **Q5 — OTOS:** uno a cada costado del robot para análisis diferencial (forzar "avanzar derecho al patear" detectando rotación inesperada).
- **Q6 — Protocolo OpenMV:** se mantiene el viejo (9 bytes 201/202/203) inicialmente, con parser robusto en TOP.
- **Q7 — Rol arquero/delantero:** dipswitch fijo en TOP al setup. Cambio dinámico de rol es objetivo post-Mundial.

### Decisión pendiente (Q3)

- **Conflicto pines 16/17 del Teensy 4.0 TOP** (I2C1 vs Serial4). Resolver con multímetro o consulta a `enzzo195`.

### Hito 1 arrancado: cimientos del firmware nuevo

- `software/teensy/Soccer 2026/platformio.ini` reorganizado a **multi-environment**:
  - `[env:teensy41_legacy]` — código del nacional 2025 intacto.
  - `[env:top]` — placa TOP (Teensy 4.0).
  - `[env:down]` — placa DOWN (Teensy 4.0).
- `src/shared/` creado con:
  - `types.h` — structs compartidos (`Pose2D`, `Velocity2D`, `LineStatus`, `MotorCommand`, `ZirconStatus`, `BallObservation`).
  - `crc16.{h,cpp}` — CRC-16/CCITT-FALSE sin tabla (~1µs por 16 bytes en Teensy 4.0).
  - `proto.{h,cpp}` — encoder + `FrameDecoder` con state machine, CRC validation, contadores de packet loss y resyncs.

## Aprendizajes / observaciones

1. **El equipo IITA ya tiene mucha documentación interna** (`docs/internal/` con 9 archivos densos del 2026-03-20) que en sesiones futuras conviene revisar **antes** de cualquier auditoría. Saltar este paso lleva a redescubrir lo ya documentado.
2. **La auditoría adicional contra código viejo tiene rendimiento decreciente.** Lo que falta es **ejecutar los fixes**, no descubrirlos.
3. **El roadmap interno (`docs/internal/roadmap-mejoras-2026.md`) tenía las placas HW-012 y HW-013 marcadas como "Horizonte 3 post-mundial".** El equipo decidió adelantarlas para Incheon — agresivo pero coherente con la filosofía "aprender > podio".
4. **Arquitectura final de 4 procesadores** (Zircon Teensy 4.1 motores + TOP Teensy 4.0 master + DOWN Teensy 4.0 sensores piso + COMM ESP32 árbitros) es modular y limpia. Permite paralelizar el trabajo entre alumnos (Virginia trabaja en TOP, Elías en DOWN, por ejemplo).

## Plan A vs Plan B (decisión a tomar pronto)

- **Plan A (ambicioso):** firmware nuevo en las 3 placas + Zircon como motor server. Robot nuevo a Incheon. Plan de 8 hitos semanales mayo 11 → junio 28.
- **Plan B (seguro):** Robot viejo (Zircon + Teensy 4.1 + código nacional 2025 con bugs P0 fixeados) va a Incheon. Las placas nuevas se debuggean en post-mundial. Bugs P0 documentados en marzo (`docs/internal/analisis-definitivo-*.md`) que **deben verificarse y fixearse**: gap del arquero, `currentYaw` raw, `velocidadActualPateo` no se resetea, `PATEANDO_atras_arquero` sin timeout.

Recomendación: **Arrancar Plan A**. A 4 semanas de Incheon, evaluar si pivotamos a Plan B. Las skills `engineering-journal` y `hardware-test-protocol` aseguran que sepamos en cada momento dónde estamos.

## Archivos creados/modificados en esta sesión

**Frame y skills:**
- `CLAUDE.md` (raíz)
- `.claude/skills/{8 skills}/SKILL.md`
- `docs/coach-system/2026-05-10-fase-0-setup-design.md`

**Auditorías y análisis:**
- `research/in-progress/2026-05-10-auditoria-striker-strategy.md`
- `research/in-progress/2026-05-10-diseno-firmware-3-placas.md`
- `hardware/electronics/mapa-pines-placas-nuevas.md`

**Firmware nuevo (Hito 1):**
- `software/teensy/Soccer 2026/platformio.ini` (refactor multi-env)
- `software/teensy/Soccer 2026/src/shared/types.h`
- `software/teensy/Soccer 2026/src/shared/crc16.h`
- `software/teensy/Soccer 2026/src/shared/crc16.cpp`
- `software/teensy/Soccer 2026/src/shared/proto.h`
- `software/teensy/Soccer 2026/src/shared/proto.cpp`

**Memorias globales (fuera del repo):**
- `~/.claude/projects/.../memory/project_iita_soccer_2026_strategy.md`
- `~/.claude/projects/.../memory/feedback_iita_soccer_coach_frame.md`

## Próximos pasos inmediatos

1. **Verificar Q3** (conflicto pines 16/17 TOP) con multímetro o `enzzo195`.
2. **Verificar bugs P0 del código viejo** (¿gap del arquero arreglado? ¿`currentYaw` raw arreglado?). Esto es **Plan B insurance**.
3. **Cargar firmware oficial RCJ en placa COMM** — independiente del trabajo TOP/DOWN.
4. **Compilar `[env:top]` y `[env:down]`** vacíos (solo con shared/) para verificar que el setup PlatformIO funciona.
5. **Tests unitarios del protocolo UART**: encode + decode loopback en host (puede ser PC con `pio test`, no requiere hardware).
6. **Próxima sesión: Hito 2** — implementar `down/line_ring.{h,cpp}` y `down/otos.{h,cpp}`.

## Notas del coaching

- Honestidad sobre overlap con docs internos previos fue importante para no inflar el valor de la auditoría.
- El pivot a "diseño de firmware" llegó por información del usuario (las 3 placas llegaron) — no estaba planeado en el spec inicial. **El plan se adapta a la realidad del equipo.**
- A 7 semanas de Incheon, **el ritmo importa**. Las decisiones rápidas (Q1-Q7 confirmadas en una sola pasada del coach) destrabaron mucho.
