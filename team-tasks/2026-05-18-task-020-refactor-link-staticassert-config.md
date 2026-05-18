---
id: TASK-020
title: "Refactor a módulo Link único + static_assert(sizeof) + corregir config/docs obsoletas"
date_created: 2026-05-18
assigned: [mariaviollaz, enzzo195]
priority: P2
status: pending
estimated_hours: 24
blocks: [mantenibilidad y herencia 2027]
tags: [firmware, comunicacion, refactor, deuda-tecnica, 2027]
depends_on: [TASK-014, TASK-016, TASK-017, TASK-018]
---

# TASK-020 — Refactor a módulo Link único (capitalizable 2027)

## Por qué importa (P2)

Hay 5 `comm_*.cpp` casi duplicados con baud hardcodeado 3 veces; corregir el
protocolo = tocar 6 archivos. **Pero V-A6/M1 advierten: NO son copia exacta** —
hay **3 patrones de receptor distintos** (coalesce-último / procesar-todos /
delegar-a-world-model), funciones emisoras duplicadas con colisión de nombres y
un emisor multi-frame. El refactor NO es mecánico y es el **peor momento** para
un desalineo silencioso de structs (V-C6: no hay `static_assert(sizeof)`;
comentario de `WorldSnapshot` errado, 23≠24 B).

> Se hace **después** de los P0/P1 (depende de TASK-014/016/017/018), con esos
> cambios ya estabilizados, usando `test/test_proto/` como red.

## Pasos concretos

1. Diseñar una clase/módulo `Link` que parametrice: UART, baud (desde
   `config_*.h`, NO hardcodeado), y respete explícitamente los **3 patrones de
   mensaje** del sistema definitivo: STREAM (coalesce al último), EVENTO
   (latch OR), COMANDO (procesar todos / idempotente).
2. `static_assert(sizeof(T) == N)` para CADA struct del protocolo (`types.h`),
   compilado en las 3 placas. Corregir el comentario de tamaño errado.
3. Migrar los 5 `comm_*.cpp` a `Link` preservando el comportamiento real de cada
   uno (no aplanar los 3 patrones a uno).
4. Corregir docs/config obsoletas: `config_central.h:1-15` (aún describe el
   modelo viejo "motor server TOP-master"; el código real es CENTRAL-master con
   motores locales) y la doc desincronizada de `comm_top.h`.

## Criterio de cierre

- [ ] Un solo módulo `Link`; baud solo desde `config_*.h` (cero hardcode).
- [ ] `static_assert(sizeof)` de todas las structs del protocolo, en las 3 placas.
- [ ] Los 3 patrones (stream/evento/comando) preservados y testeados.
- [ ] `config_central.h` y docs de comms reflejan la arquitectura real.
- [ ] `test/test_proto/` pasa + test de banco 10 min a 230400 sin pérdida.

## Plan de prueba en hardware real

1. **Banco:** los 3 enlaces inter-placa corriendo 10 min a 230400 con tráfico
   real + ruido EMI de motores; 0 desincronización, contadores de salud OK.
2. **Regresión:** comandos one-shot (RESET_OTOS/CALIB_LINE) siguen llegando
   (no coalescidos); emergencias (latch) no se pierden; streams coalescen al
   último.
3. **Cross-placa:** verificar que un mismatch de `sizeof` lo atrapa el
   `static_assert` en compilación (test negativo deliberado).

## Notas / decisiones

_(completar al ejecutar)_

## Cambios de estado

- 2026-05-18: creada por Claude tras la verificación independiente, a pedido de
  Gustavo Viollaz. P2: capitalizable a 2027, después de cerrar los P0/P1.
