---
id: TASK-034
title: "Decidir arquitectura de localización XY+heading para Incheon (5 alternativas)"
date_created: 2026-05-25
date_due: 2026-06-01
assigned: [gviollaz]
priority: P1
status: pending
estimated_hours: 0.5  # decisión + journal, no implementación
blocks: [implementacion-localizacion-tof-imu, scope-firmware-pose]
blocked_by: []
tags: [decision, top-board, localizacion, control, incheon, tof, imu]
---

# TASK-034 — Decidir arquitectura de localización XY+heading para Incheon

## Resumen

Definir qué algoritmo de localización se implementa para Incheon dado el
hardware disponible (4× VL53L7CX cardinales + 2× BNO055). 5 alternativas
analizadas en `research/in-progress/2026-05-25-localizacion-tof-imu-analisis.md`.
La decisión define el scope del firmware de pose para los próximos ~10
días (Sprint 1 + Sprint 2).

## Contexto

- El robot necesita pose absoluta `(x, y, θ)` en la cancha para que la
  FSM de strategy pueda decidir (defender, avanzar, patear).
- Sin esto el robot vuelve a ser puramente reactivo (solo persigue
  pelota), perdiendo el valor del diseño de 4 ToFs.
- Hardware confirmado (Gustavo, 2026-05-25): VL53L7CX (no L5CX), FoV
  60° (no 90°), multizona 8×8 (64 valores/sensor), montaje cardinal
  a 14 cm del piso.
- TASK-033 (¿2 ToFs sin rework vs 4 con bodge?) está acoplada a esta
  decisión — el algoritmo elegido condiciona o se condiciona por
  cuántos sensores hay.

## Decisión a tomar

¿Qué algoritmo de localización se implementa para Incheon?

## Opciones con trade-offs (resumen)

Análisis completo en `research/in-progress/2026-05-25-localizacion-tof-imu-analisis.md`.

| # | Alternativa | Precisión | CPU | Dev time | Robustez | Complejidad |
|---|---|---|---|---|---|---|
| 1 | Trilateración geométrica directa | ±2-3 cm | despreciable | 1 día | baja | ★ |
| 2 | Wall-follow + dead reckoning IMU | ±2-3 cm (degrada) | baja | 1-2 días | media | ★★ |
| 3 | EKF | ±0.5-1 cm | ~50 µs | 3-5 días | alta | ★★★★ |
| 4 | Particle Filter (MCL) | ±1 cm | ~500 µs | 4-7 días | muy alta | ★★★★★ |
| 5 | LUT matching v2 (multizona 8×8) | ±1 cm | ~500 µs | 4-5 días | muy alta | ★★★ |

## Recomendación del coach

**Sprint 1 (días 1-4): Alternativa 1 — trilateración geométrica directa.**

- Baseline funcional rápido (~3-4 días incluyendo banco).
- Valida que el hardware ToF cardinal + BNO entrega lo que dice.
- Si la precisión alcanza para la estrategia planeada, se puede
  diferir Sprint 2 e ir a Incheon con esto.

**Sprint 2 (días 5-10): Alternativa 5 v2 — LUT matching multizona.**

- Upgrade que aprovecha el 8×8 (que Alt 1 desperdicia).
- Robustez frente a obstáculos sin meter EKF/MCL.
- Decisión de ejecutar Sprint 2 se toma **después** de evaluar Sprint
  1 en cancha real.

**Total ~10 días.** Compatible con calendario Incheon (42 días).

### Por qué NO Alt 2 / Alt 3 / Alt 4

- **Alt 2 (dead reckoning)**: drift de IMU integrado prohibitivo.
  Útil solo como fallback dentro de un sistema más rico — no como
  solución única.
- **Alt 3 (EKF)**: dev time + tuning consume budget de TASK-014/015/
  016/022 (P0 pendientes). Precisión incremental sobre Alt 5 no
  justifica el esfuerzo para Incheon. Considerar para 2027.
- **Alt 4 (MCL)**: overkill para una cancha 1.83 × 2.43 m con 4
  paredes ortogonales. Brilla en ambigüedad alta — acá no aplica.

## Dependencia con TASK-033

Si TASK-033 resuelve "2 ToFs sin rework":
- Alt 1 sigue viable pero pierde redundancia.
- Alt 5 pierde la mitad de su ventaja (matching multizona con 2
  sensores en vez de 4).
- Recomendación con 2 ToFs: ir solo con Alt 1, usar el segundo BNO
  como ancla redundante de heading.

Si TASK-033 resuelve "4 ToFs con bodge":
- Recomendación coach completa (Sprint 1 + Sprint 2) aplica.

**Implicación: resolver TASK-033 primero o en paralelo.** Las dos
decisiones se toman juntas o en cascada (033 antes que 034).

## Pasos concretos

1. Gustavo lee el research note completo
   (`research/in-progress/2026-05-25-localizacion-tof-imu-analisis.md`).
2. Gustavo cruza con la decisión de TASK-033.
3. Gustavo decide: ¿qué alternativa para Sprint 1? ¿se planifica
   Sprint 2 desde ya o se decide post-Sprint 1?
4. Gustavo escribe **journal entry corto** con:
   - Qué alternativa(s) eligió.
   - Por qué (1 párrafo).
   - Plan de implementación: quién ejecuta, en qué orden, plazo.
5. Crear sub-task(s) de implementación con asignado (Virginia para
   firmware, Claude para skeleton + raycasting / LUT generator
   offline).
6. Mover el research a `research/completed/` con sección "Decisión
   final" al inicio.
7. Marcar este TASK como `done` con link al journal de la decisión.

## Criterio de cierre

- [ ] Decisión escrita en journal `journal/2026-05-2X-decision-localizacion-incheon.md`.
- [ ] Sub-task(s) de implementación creada(s) con asignado.
- [ ] Research note movido a `research/completed/` con decisión.
- [ ] Esta TASK marcada como `done` en este archivo + en
      `team-tasks/README.md`.

## Plazo

**2026-06-01** (7 días) — para arrancar Sprint 1 con tiempo de buffer
antes de la salida a Incheon (jun 30).

## Referencias

- Análisis técnico completo: `research/in-progress/2026-05-25-localizacion-tof-imu-analisis.md`
- TASK acoplada (hardware ToFs): `team-tasks/2026-05-25-task-033-decidir-cuantos-tofs-incheon.md`
- Journal forense del XSHUT: `journal/2026-05-25-top-xshut-no-routed-hallazgo-forense.md`
- Hardware ToF (estado actual): `software/teensy/Soccer 2026/src/top/sensors_tof.cpp`
- Hardware IMU (estado actual): `software/teensy/Soccer 2026/src/top/sensors_imu.cpp`

## Cambios de estado

- 2026-05-25: creada por Claude Opus 4.7 (Anthropic) al cerrar el
  análisis de 5 alternativas. Asignada a Gustavo (decisión arquitectural
  + budget de tiempo del equipo).
