---
id: TASK-024
title: "Operabilidad de arranque: leer rol (dipswitch), polaridad de arco por árbitro, fallback de START"
date_created: 2026-05-18
assigned: [mariaviollaz, elias]
priority: P0
status: pending
estimated_hours: 8
blocks: [que el robot arranque y ataque el arco correcto en Incheon]
tags: [firmware, top-board, central-board, arbitros, operabilidad]
depends_on: []
---

# TASK-024 — Operabilidad de arranque (rol / polaridad / START)

## Por qué importa (P0)

La auditoría encontró 3 fallas que, aunque todo lo demás funcione, **hacen
perder partidos sí o sí**:

1. **Rol del robot nunca se lee.** `PIN_ROLE_DIPSWITCH` (`config_top.h:87`)
   declarado pero **sin ningún `digitalRead`**. TOP no sabe ni reporta si es
   arquero o delantero → CENTRAL no diferencia la FSM al boot.
2. **Polaridad de arco hardcodeada.** `main_top.cpp:61-68` `yellow=opp` fijo;
   `strategy_set_attack_color()` (`strategy.h:43`) **nunca se llama**. Según el
   sorteo de lado, el delantero **ataca su propio arco ~50% de los partidos**
   (autogoles sistemáticos).
3. **No arranca sin COMM y sin fallback.** `match_running` solo se setea con
   `START` de la placa COMM (`comm_arbiter.cpp:28`). Sin COMM operativa, la FSM
   queda en WAIT_START para siempre y no hay arranque manual.

## Pasos concretos

1. Leer `PIN_ROLE_DIPSWITCH` en `setup()` de TOP (verificar pull-up/polaridad
   en la placa real), propagar el rol en el snapshot/status y que CENTRAL elija
   FSM arquero vs delantero según ese rol.
2. Determinar la polaridad de arco por el lado asignado por el árbitro:
   mapear el comando/lado a `strategy_set_attack_color()` y llamarlo. Definir
   el mensaje con la placa COMM (coordinación con Enzo/TASK-006).
3. Fallback de START: **verificar primero el reglamento RCJ** (el start suele
   ser solo por árbitro). Si está permitido, agregar arranque manual seguro
   (botón/dipswitch) en OR con el START del árbitro, claramente diferenciado.
4. Señalizar por LED el rol y el arco objetivo elegidos (chequeo pre-partido).

## Criterio de cierre

- [ ] El dipswitch de rol cambia efectivamente arquero↔delantero (probado en placa).
- [ ] Con el robot en cada lado de cancha, ataca el arco correcto (no autogol).
- [ ] El robot arranca de forma reglamentaria; fallback (si aplica) verificado
      contra reglas RCJ y documentado.

## Plan de prueba en hardware real

1. Setear dipswitch arquero → el robot se comporta como arquero; cambiar a
   delantero → cambia. (Robot armado, sobre cancha.)
2. Poner el robot en lado A y lado B → en ambos ataca el arco rival, nunca el
   propio.
3. Simular árbitro START/STOP (placa COMM o fallback) → el robot arranca/para
   correctamente; sin la señal NO se mueve.

## Notas / decisiones

_(completar al ejecutar — registrar polaridad del dipswitch y decisión sobre el
fallback de START vs reglamento RCJ)_

## Cambios de estado

- 2026-05-18: creada por Claude tras la evaluación crítica del firmware, a
  pedido de Gustavo Viollaz.
