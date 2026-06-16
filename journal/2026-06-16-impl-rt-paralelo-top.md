# 2026-06-16 — Implementación RT paralela del TOP (pizarra + emisor desacoplado)

> Sesión autónoma (Gustavo durmiendo, mandato: validar diseño + programar en paralelo los
> módulos del TOP, mínima latencia + super confiable/fail-safe, dejar todo listo). Rama
> aislada `agente/top-rt-paralelo` (worktree, no toca `main`).

## Qué se hizo

Validación de diseño (lente arquitecto RT, 4 áreas en paralelo) + implementación en paralelo
(7 agentes TDD) de la arquitectura no-bloqueante del TOP diseñada en
`docs/firmware/ARQUITECTURA-SENSORIAL-TOP-NO-BLOQUEANTE.md`.

**Módulos PUROS programados y host-verificados** (`bash scripts/run-host-tests.sh` = **78 envs /
1098 tests / 0 fallas**):
- `freshness_policy.h` (10), `hcsr04_async.h` (14), `bno_read_sm.h` (21), `tof_schedule.h` (15),
  `snapshot_from_slots.h` (14) — NUEVOS.
- `sensor_slot.h` +`slot_read_latest_capped` (29), `pose_fusion` fix anti-free-run (19) — MEJORAS.

**Firmware GATEADO** (`-DTOP_ENABLE_SNAPSHOT_TIMER`, compila ON y OFF, **byte-neutro apagado**):
- `src/top/snapshot_emitter.{h,cpp}` (emisor @100 Hz por IntervalTimer desde la pizarra) +
  cableado gateado en `main_top.cpp` + env `top_robot2_pri_snaptimer`.
- `pio run -e top_robot2_pri` (OFF) y `top_robot2_pri_snaptimer` (ON) → ambos SUCCESS.

## Lo importante (fail-safe / degradación con gracia)

Cada campo del snapshot es "fresco-y-válido" o **sentinela honesto** — nunca dato viejo disfrazado
de fresco. Fix central: `pose_fusion` ya no reporta `valid=true/conf=10` divergiendo sin ToF
(>500 ms → `valid=false`). El `slot_read_latest_capped` corta el `for(;;)` que colgaría la ISR.

## Lo que NO se cerró (banco — regla #1)

La frescura POR-SENSOR (cada read publica con su timestamp) es **Fase 2**; hoy el `publish`
republica los getters por loop → cubre loop-muerto, no muerte-de-un-sensor. La ISR/IntervalTimer +
WCET + reentrancia Serial4 + ordering `volatile/__DMB()` de la pizarra **los cierra el equipo en
banco** (T1-T7). Host-green ≠ validado en hardware.

## Para seguir

- Handoff completo (arquitectura + plan F0-F4 + plan de banco + invariantes):
  `docs/firmware/IMPL-PIZARRA-Y-EMISOR-TOP-2026-06-16.md`.
- Merge: la rama `agente/top-rt-paralelo` la revisa/mergea Gustavo. Todo es aditivo + gateado
  (binario de competencia byte-idéntico con los flags apagados).

## Atribución
Validación + programación + esta entrada: Claude Opus 4.8 (Anthropic), requested-by Gustavo
Viollaz, 2026-06-16. Banco lo cierra el equipo humano.
