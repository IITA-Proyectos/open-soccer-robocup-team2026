---
id: TASK-101
title: "Banco mitad inferior (motores+CENTRAL+DOWN): reconciliar cinemática/M2 + decidir substrato del arquero (fork v2)"
date_created: 2026-06-03
assigned: [gustavo-viollaz, virginia-viollaz, elias, enzo]
priority: P1
status: pending
estimated_hours: 3
needs_decision_from: gustavo-viollaz
blocks: ["v2 heading-hold OTOS del arquero", "arquero que se mueva DERECHO en cancha"]
blocked_by: [placa Zircon Rev v15 + Teensy 4.1 + DOWN + batería cargada]
tags: [central-board, banco, arquero, cinematica, motores, control, decision, coach]
related:
  - journal/2026-06-03-verificacion-banco-mitad-inferior-y-review-gk.md
  - journal/2026-06-01-arquero-seguidor-linea-y-calibracion.md
  - team-tasks/2026-05-29-task-036-correr-diag-central-motors-en-banco.md
  - team-tasks/2026-05-29-task-037-correr-diag-central-drive-en-banco.md
  - team-tasks/2026-05-29-task-100-validar-ingest-linea-down-central.md
  - docs/firmware/DIAG-CENTRAL-STRAFE.md
---

# TASK-101 — Banco mitad inferior + decisión del substrato del arquero

> **Para el coach.** Este archivo es el resumen accionable de la verificación
> pre-banco del 2026-06-03 (sesión Claude, no-HW). El análisis completo está en
> [`journal/2026-06-03-…`](../journal/2026-06-03-verificacion-banco-mitad-inferior-y-review-gk.md).
> Acá: qué medir en banco, en qué orden, y **la decisión que hay que tomar**.

## Contexto en 3 líneas

- El árbol compila (`central_robot1` + diags) y el Serial está verificado en código
  (DOWN=Serial1 pin 0, TOP=Serial7 pin 28; diag y producción coinciden).
- Pero `diag_central_strafe` corre sobre **cinemática NO calibrada** (la que el
  2026-06-01 dio CÍRCULOS), así que la "deriva" de FASE B puede ser un falso positivo.
- Hay **dos arqueros** en el repo (producción `goalkeeper_tick` vs banco
  `diag_central_line_sweep`) → hay que decidir cuál evoluciona antes de escribir el v2.

## Temas a analizar (detalle en el journal)

| # | Tema | Prioridad | Dónde se cierra |
|---|---|---|---|
| 1 | `diag_central_strafe` usa `inverse_kinematics(WHEEL_ANGLES_DEG={60,-60,180}` TENTATIVO) → puede dar círculos; FASE B se contamina | P1 (P0 para que el arquero ande derecho) | Banco: FASE A primero |
| 2 | **Contradicción M2**: 2026-05-29 dijo `{+1,+1,+1}` sin inversión; 2026-06-01 dijo "M2 INA/INB invertido por HW". `motors_zircon` no tiene signo por motor | P1 | Banco: TASK-036 en ESTE robot |
| 3 | **Doble arquero** (producción cinemática vs banco control-directo+BNO-muerto) | P2 (decisión) | Decisión de Gustavo (abajo) |
| 4 | **Signo de omega (runaway)**: `+omega` = horario físico; CLEAR ya usa heading_pid sin validar HW | P1 (gate de v2 y de CLEAR) | Banco: TASK-037 giro chico |

## Secuencia de banco recomendada (ORDEN)

1. **TASK-036** (`diag_central_motors`) en **este** robot → cerrar Tema 2 (¿M2
   invertido sí/no? sentido definitivo de los 3). ⚠️ El conflicto 7/8 YA está
   resuelto (UART movido a Serial1) — ignorar la parte vieja de "migrar Serial2".
2. **Link DOWN→CENTRAL** (`diag_central_comm_down`, TASK-100) → cable al **pin 0**
   (NO pin 7 — ahí ahora va el motor 2). Mirar `frames↑ / CRC≈0`.
3. **`diag_central_strafe` FASE A** → ¿lateral limpio o círculos? Si da círculos,
   el dato útil **no es la deriva** sino "la cinemática genérica no sirve aún para
   este robot" (Tema 1). Solo si FASE A sale limpio tiene sentido medir FASE B.
4. **Signo de omega** (Tema 4): en `diag_central_drive` (TASK-037) comandar un giro
   chico y confirmar que corrige HACIA el rumbo (no que se aleja). Si se aleja →
   invertir el signo de omega ANTES de cualquier heading-hold.

## DECISIÓN que necesita el coach (Tema 3 / fork del v2)

¿Qué substrato de movimiento evoluciona el arquero?

- **Opción A — producción (`goalkeeper_tick`):** calibrar `WHEEL_ANGLES_DEG` con
  Enzo + heading-hold OTOS ahí. Más limpio/reusable (sirve también al delantero,
  que necesita vectores arbitrarios). Cuesta la calibración de cinemática.
- **Opción B — banco (`diag_central_line_sweep`):** mantener el control directo que
  ya anduvo y solo cambiar el BNO muerto (`imu_get_heading`, CENTRAL sin BNO) por el
  heading del OTOS (`world_model_get_otos_heading_deg`, ya local por broadcast). Más
  rápido para Incheon; no generaliza al delantero; deja la deuda de los dos arqueros.

**Recomendación coach:** Opción B ahora (desbloquea el semicírculo del 2026-06-01
con el OTOS local) + Opción A como deuda post-banco en `FUENTES-DE-VERDAD.md`. El v2
se escribe recién con Temas 2 y 4 cerrados en banco.

## Criterio de cierre

1. TASK-036 corrida en este robot con veredicto M2 (Tema 2) documentado en journal.
2. FASE A del strafe documentada: lateral limpio vs círculos (Tema 1).
3. Signo de omega confirmado en banco (Tema 4).
4. **Gustavo eligió Opción A o B** (Tema 3) — anotarlo en `FUENTES-DE-VERDAD.md`.
5. Recién entonces: abrir TASK de implementación del v2 heading-hold OTOS.

## Atribución

- Análisis + esta TASK: Claude Opus 4.8 (Anthropic), sesión 2026-06-03
  (requested-by Gustavo Viollaz @gviollaz).
- **La ejecución en banco y las decisiones las hace el equipo humano** — Claude NO
  cierra TASKs de hardware (regla 1 CLAUDE.md).
