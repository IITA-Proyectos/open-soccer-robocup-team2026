---
id: TASK-018
title: "Cotar drenado de RX (MAX_BYTES_PER_TICK) y NO introducir Serial.clear()"
date_created: 2026-05-18
assigned: [mariaviollaz]
priority: P1
status: pending
estimated_hours: 4
blocks: [latencia de emergencia acotada]
tags: [firmware, comunicacion, central-board, timing]
depends_on: []
---

# TASK-018 — Drenado acotado, sin Serial.clear()

## Por qué importa (P1)

- **V-A2:** `comm_down_tick`/`comm_top_tick` drenan el RX **sin cota** de bytes
  por tick (a diferencia de `cameras_tick` que sí tiene `MAX_BYTES_PER_TICK=64`,
  `cameras_runtime.cpp:29`). Tras un stall, dos colas llenas inflan la latencia
  del chequeo de emergencia por encima del presupuesto (<15 ms).
- **V-C2:** el `Serial.clear()` que la propuesta original sugería **agrega** un
  fallo que hoy no existe (descarta frames válidos en recuperación) y contradice
  que el `FrameDecoder` ya resincroniza solo. **No se debe introducir.**

## Pasos concretos

1. Cotar `comm_*_tick()` de CENTRAL/TOP/DOWN con un `MAX_BYTES_PER_TICK` (mismo
   patrón que cámaras). Dimensionarlo con el período de loop medido (TASK-014).
2. Garantizar que el chequeo de emergencia de CENTRAL se evalúe **antes** de
   drenar colas largas o, mejor, que la emergencia se procese dentro del drenado
   (coordinar con TASK-016: latch OR).
3. **NO** agregar `Serial.clear()` en init ni en recuperación. La recuperación
   se apoya en el resync byte-a-byte del `FrameDecoder` (ya existe y está
   testeado en `test/test_proto/`).

## Criterio de cierre

- [ ] Todos los `comm_*_tick` con cota de bytes/tick documentada.
- [ ] Cero llamadas a `Serial.clear()` en el firmware (verificado por grep).
- [ ] Latencia de emergencia medida post-stall < 15 ms.

## Plan de prueba en hardware real

1. **Setup:** robot, stall de CENTRAL forzado, colas RX llenas al recuperar.
2. **Criterio medible:** desde que aparece `imminent_exit` hasta `motors_brake`
   < 15 ms, incluso post-stall con colas llenas.
3. **Regresión:** sin pérdida de frames en reconexión (contadores de salud).

## Notas / decisiones

_(completar al ejecutar)_

## Cambios de estado

- 2026-05-18: creada por Claude tras la verificación independiente, a pedido de
  Gustavo Viollaz.
