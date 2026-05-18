---
id: TASK-016
title: "Fail-safe de borde: OR-latch de imminent_exit + precedencia ante LOST conjunto"
date_created: 2026-05-18
assigned: [mariaviollaz, elias]
priority: P0
status: pending
estimated_hours: 6
blocks: [no salir de cancha en Incheon]
tags: [firmware, comunicacion, central-board, control, seguridad]
depends_on: []
---

# TASK-016 — Fail-safe de borde robusto

## Por qué importa (P0)

Dos problemas verificados que hacen que el robot **salga de cancha** (penalización
/ expulsión del juego en Incheon):

1. **V-C4:** `imminent_exit` es un flag de evento dentro de `LineStatus`
   (`types.h:45`). El patrón "quedarse con el último" lo **pisa**: si tras un
   stall llegan varios `LINE_URGENT`, el pulso de borde queda enterrado entre
   frames más nuevos y `world_model_imminent_exit()` (`main_central.cpp:95`)
   nunca lo ve.
2. **V-A1:** si el loop de CENTRAL se stallea, mundo y línea pasan a LOST a la
   vez; `motors_stop()` (por mundo-LOST) gana y el "modo borde conservador"
   (por línea-LOST) **nunca se ejecuta** → el P0 de protección de borde es
   inalcanzable justo en el fallo más común.

## Pasos concretos

1. Tratar `imminent_exit` como **latch OR** sobre TODOS los frames drenados en el
   loop (o procesar la emergencia DENTRO del while de drenado, no después). El
   latch se limpia solo cuando la acción de emergencia se consumió.
2. Definir explícitamente la **precedencia de acciones de seguridad** cuando
   varios enlaces caen juntos. Principio: la acción **más conservadora gana**;
   el modo borde conservador debe ser alcanzable aunque mundo esté LOST (p.ej.
   `motors_stop` ya cubre "no salir", pero hay que garantizar que NO se pase a
   "seguir jugando" por una evaluación de orden equivocado).
3. Definir en código qué es exactamente "modo borde conservador" (hoy es un
   handwave): velocidad limitada + vector prohibido hacia afuera + señalización.
4. Eliminar el "modo ciego de borde" (`main_central.cpp:13`): línea LOST nunca
   debe resultar en "seguir jugando sin protección".

## Criterio de cierre

- [ ] `imminent_exit` no se puede perder por coalescing (latch OR verificado con
      test de ráfaga).
- [ ] Precedencia de seguridad definida y testeada para: solo-mundo-LOST,
      solo-línea-LOST, ambos-LOST.
- [ ] No existe ningún path donde línea-LOST → seguir jugando sin protección.

## Plan de prueba en hardware real

1. **Setup:** robot jugando sobre cancha, cerca de la línea blanca.
2. **Ráfaga + stall:** forzar stall de CENTRAL y, al recuperar, inyectar varios
   `LINE_URGENT` con `imminent_exit` intermitente (1,0,1,0).
3. **Criterio medible:** el robot **no cruza** la línea en ningún caso;
   `imminent_exit` del frame con borde se respeta aunque venga seguido de frames
   sin borde.
4. **Desconexión:** cortar el bus DOWN→CENTRAL en juego → el robot entra en modo
   conservador (no "ciego"), no sale de cancha, lo señaliza.
5. **Regresión:** con todo sano, el comportamiento de borde normal no cambia.

## Notas / decisiones

_(completar al ejecutar)_

## Cambios de estado

- 2026-05-18: creada por Claude tras la verificación independiente, a pedido de
  Gustavo Viollaz.
