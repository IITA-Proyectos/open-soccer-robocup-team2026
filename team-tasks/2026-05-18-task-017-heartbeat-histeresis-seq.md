---
id: TASK-017
title: "Heartbeat explícito + máquina OK/STALE/LOST con histéresis + SEQ sin falsos"
date_created: 2026-05-18
assigned: [mariaviollaz, elias]
priority: P1
status: pending
estimated_hours: 10
blocks: [observabilidad y recuperación confiable de enlaces]
tags: [firmware, comunicacion, protocolo, robustez]
depends_on: [TASK-014]
---

# TASK-017 — Heartbeat + histéresis + SEQ

## Por qué importa (P1)

- No hay heartbeat explícito: "vivo" = "hubo datos" → no distingue emisor
  colgado de enlace muerto.
- **V-C3:** sin histéresis, una microcaída cerca del umbral hace que el robot
  oscile entre frenar y jugar (los motores del Zircon son ruido EMI en la misma
  placa que CENTRAL → lazo de fallo).
- **V-C5:** verificar SEQ tal como estaba propuesto da **falsos packet-loss**
  tras un resync legítimo (un glitch de CRC hace saltar el SEQ sin pérdida real).

> **Depende de TASK-014:** las ventanas T_OK/T_LOST se fijan a partir del
> período de loop **medido en hardware**, no inventado.

## Pasos concretos

1. Agregar `MsgType::LINK_HEARTBEAT` (payload 0 + contador incremental). Cada
   emisor lo manda si en `HB_TX_MS` no tuvo dato real que enviar.
2. Máquina por enlace **OK / STALE / LOST con histéresis**:
   - OK requiere **N frames buenos consecutivos** (no 1).
   - STALE/LOST con umbrales separados de entrada y salida (histéresis) para
     evitar flapping.
   - Definir en código el comportamiento de **STALE** en cada consumidor
     (strategy, fail-safe) — no dejarlo como "usar el último".
3. SEQ: contar gap **solo** si entre los dos frames NO hubo `crc_error` ni
   `resync` (usar los contadores de `proto.h:108-111`). Tolerar gap 1–2 sin
   acción. SEQ es **métrica de salud**, no gatillo directo de seguridad.
4. Fijar T_OK/T_LOST por enlace = f(período de loop medido en TASK-014), no los
   números inventados de la tabla §3.3 de la propuesta.

## Criterio de cierre

- [ ] `LINK_HEARTBEAT` implementado y observado en los enlaces inter-placa.
- [ ] Histéresis verificada: con pérdida intermitente cerca del umbral, el robot
      NO oscila frenar/jugar.
- [ ] SEQ no genera falsos LOST tras resync (test de inyección de CRC-error).
- [ ] Ventanas T_OK/T_LOST documentadas con su base = período medido (TASK-014).

## Plan de prueba en hardware real

1. **Setup:** robot completo, TASK-014 cerrada (loop no bloqueante, período
   conocido).
2. **Ruido EMI:** motores acelerando/frenando fuerte mientras se observa el
   estado de cada enlace.
3. **Criterio medible:** sin flapping OK↔STALE↔LOST con ruido normal de motores;
   el robot no tartamudea; el enlace pasa a LOST solo ante caída real y se
   recupera limpio (sin frame basura).
4. **Inyección de CRC-error:** verificar que un glitch aislado NO dispara LOST
   (solo sube el contador de salud).
5. **Regresión:** juego normal 5 min sin falsos LOST.

## Notas / decisiones

_(completar al ejecutar — registrar los T_OK/T_LOST elegidos y su justificación)_

## Cambios de estado

- 2026-05-18: creada por Claude tras la verificación independiente, a pedido de
  Gustavo Viollaz.
