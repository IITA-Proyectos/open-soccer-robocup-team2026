---
id: TASK-303
title: "Validar en banco las 24 correcciones del audit DOWN (2026-06-03) + lista de pendientes del audit"
date_created: 2026-06-03
assigned: [virginia-viollaz, elias, gviollaz]
priority: P1
status: pending
estimated_hours: 2.5
blocks: [confianza operativa DOWN en torneo]
blocked_by: [placa DOWN energizada en banco, TASK-031 (UART real) para los criterios end-to-end]
tags: [hardware, down-board, audit, otos, linea, calib, validacion]
---

# TASK-303 — Validar en banco las correcciones del audit DOWN + pendientes

## Resumen

El 2026-06-03 se implementaron **24 de los 28 hallazgos** del audit multi-agente
del firmware DOWN (`research/in-progress/2026-06-03-auditoria-multiagente-firmware-down.md`).
**Todas compilan (3 placas) y la lógica pura está testeada host (399 tests / 0
fallos vía `scripts/run-host-tests.sh`)**, pero las que tocan COMPORTAMIENTO no se
validaron en hardware real. Claude no cierra tasks de HW (regla 1) — este TASK lo
cierra el equipo con la placa.

Commits (rama `agente/down`): `5870308` docs-config · `32f80d0` line-comm ·
`131ec5b` model · `f560dc4` otos · `eb0f12b` main-timing · `1153d28` runner+journal.

## Criterios de cierre — validación en banco (por área)

### A. OTOS — salud + fusión (commit f560dc4)
- [ ] **#6/#15 (salud I²C real):** con los 2 OTOS andando (`[L=ok R=ok]`,
      `diag_central_rx_all` o `down_debug`), **desconectar UN OTOS en caliente** →
      confirmar que `confidence` cae a **60** (no se queda en 100) y que la pose NO
      se congela mintiendo. Desconectar el segundo → `confidence=0`.
- [ ] **#7 (heading dual):** girar el robot **más de 90°** (ej. 120°) → el heading
      reportado por OTOS sigue correcto y monótono (antes saturaba en ±90°).
- [ ] **#13 (recuperación en caliente):** desconectar un OTOS, reconectarlo, mandar
      `CENTRAL_RESET_OTOS` → confirmar que se **re-detecta SIN power-cycle** del Teensy.
- [ ] **#14 (omega clamp):** hacer girar el robot **rápido** (>5.7 rad/s al alinear)
      → confirmar que `omega` reporta el **signo correcto** (antes wrappeaba: derecha→izquierda).
- [ ] **#20 (slip):** con el robot quieto, `slip_estimate ≈ 0` sostenido (antes crecía por drift).

### B. Línea / calib (commits 131ec5b, 32f80d0)
- [ ] **#4 (calib no se contamina):** dejar el robot **parkeado sobre la línea
      blanca** ≥1–2 min y confirmar que el baseline de carpet NO se corrompe (la
      detección de línea sigue sana después). *Es el fix de mayor impacto-partido.*
- [ ] **#2 (sample_age):** en el monitor, confirmar que `sample_age_ms` reporta un
      valor real chico (no saturado en 255).
- [ ] **#25 (histéresis line_present):** pasar lento el borde de la franja →
      `line_present` estable, sin flicker tick-a-tick.

### C. Timing (commit eb0f12b)
- [ ] **#24 (línea antes de OTOS):** con OTOS andando, confirmar que la latencia /
      frecuencia del frame de línea NO se degrada por el bloqueo I²C (medir período
      del `comm_central_send`, o `tx_ok` estable a ~200 Hz).

> Las correcciones de docs/comentarios (#8/#12/#17/#22/#27) y de código
> muerto/init (#9/#10/#11/#18) NO requieren banco (cero cambio de conducta,
> cubiertas por tests host + compilación).

## Otras tareas PENDIENTES del audit (la lista completa)

| # audit | Qué falta | Owner | Estado / dónde |
|---|---|---|---|
| **#1** | UART DOWN→{CENTRAL,TOP} end-to-end por protocolo en banco (CRC/freq/latencia, 60 s) | equipo | **TASK-031** (pending) — el P0 de mayor riesgo |
| **#3** | Validar HW las 3 robustez viejas (calib EEPROM / all-white / backpressure) | equipo | **TASK-301** (pending) |
| **#5** | Unificar las 2 cadenas de línea (`line_ring` vs `DownModel`) — **decisión de diseño** (cuál queda) | **Gustavo** | deuda viva (post-Incheon, ver `FUENTES-DE-VERDAD.md`); Claude NO lo hace solo |
| **#23** | Frescura de `Velocity2D` cuelga del timestamp de `Pose2D` | **agente CENTRAL** | es `src/central/comm_down.cpp` (fuera de la placa DOWN) → derivar |

## No olvidar

- [ ] **Gustavo:** mergear `agente/down` → `main` (`git merge --no-ff agente/down`),
      6 commits limpios del audit + el runner de tests.
- [ ] Si los criterios A/B/C pasan: marcar los hallazgos como **validados HW** en el
      doc del audit y en `docs/ESTADO-ACTUAL.md`.

## Notas / cambios de estado

- 2026-06-03: creada al cerrar la implementación de la etapa 2 del audit DOWN
  (Claude Opus 4.7, requested-by María en la compu de Gustavo). Firmware listo +
  host-testeado (399 tests); HW pendiente.
