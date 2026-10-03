---
title: "arqueromix #10: empuje con la cámara trasera + TOP con prioridad delantera (gateados)"
date: 2026-10-03
author: "Claude (sesión coach, pedido del equipo arquero)"
status: hecho-sin-banco
---

# arqueromix #10 — empuje con la trasera + prioridad delantera

## Pedido

> "Cuando vea la pelota con la cámara trasera (mientras no vea arco) la empuje hacia atrás hasta
> detectar la línea, y que la prioridad sea siempre la delantera: si ambas cámaras detectan pelota,
> que tome la delantera."

## Cómo funcionaba antes (el mecanismo)

- **TOP** (`cameras_fusion.cpp`): da vuelta (180°) lo que ve la trasera y junta las dos cámaras en UNA
  pelota. Si ambas ven → **promedio**. Como las cámaras miran a lados opuestos, una pelota física nunca
  está en las dos: el promedio inventa una pelota en el medio.
- **CENTRAL** (`amix_fsm.cpp`, `esperar_quieto`): sólo recibe (x, y) fusionado; no sabe qué cámara la
  vio. Con la pelota atrás (|áng| > 90°) no despeja (exige ±30°) y hace strafe hacia el signo del
  ángulo. Nunca retrocede.

## Qué se hizo (todo gateado)

**TOP — `-DTOP_BALL_FRONT_PRIORITY` (env `top_robot2_pri_frontprio`).** Función pura nueva
`fuse_ball_front_priority`: si la delantera ve, la trasera no entra a la fusión. Reusa `fuse_ball_dual`
(pasándole `back_alive && !front_ok`), así que fuera del caso "ambas ven" es idéntica a la clásica.
Host: 5 tests nuevos, `test_cameras_fusion` 32/32 verde.

**CENTRAL — `-DARQMIX_EMPUJE_TRASERA` (env `central_robot2_arqueromix_empuje` = #9 + flag).**
Estado nuevo `empujar_atras`:

| Paso | Condición |
|---|---|
| Entra (desde `esperar_quieto`) | pelota visible, \|áng\| > 90°, `goal_own_visible` = falso, DOWN fresco, pasó el cooldown |
| Acción | `retroceder_empuje()`: MISMA potencia que el golpe del despeje (rampa 0 → `AMIX_KICK_VEL_FINAL`, 191 con +6%), con el sentido del homing (`AMIX_INICIO_RETRO_SIGN`, ya validado). Sin el trim del pateo (está medido sólo hacia adelante). |
| Sale a `inicio_avanzar` | ve la línea · DOWN no fresco · 3 s sin línea |
| Sale a `esperar_quieto` | aparece el arco propio · la pelota pasa al frente |

### Decisiones de diseño (y por qué)

1. **"La vio la trasera" = \|ángulo\| > 90°.** La CENTRAL no recibe qué cámara vio la pelota y no se
   tocó el contrato del snapshot (wire-breaking). Con FOVs opuestos, atrás sólo puede venir de la
   trasera. **Esto sólo es válido con la TOP de prioridad delantera**: con el promedio el ángulo mezcla
   las dos cámaras. Por eso los dos envs van juntos.
2. **"Mientras no vea arco" = arco PROPIO** (`goal_own_visible`). Si la trasera ve el arco propio,
   empujar hacia atrás es empujar hacia el propio arco. No se usó "ningún arco": la delantera ve
   seguido el arco rival y el empuje nunca se dispararía. **A confirmar con el equipo** (TASK-124 paso 2):
   si desde la posición del arquero el arco propio se ve siempre, el empuje no se dispara cerca del arco.
3. **No corta por perder la pelota.** Pegada a la cola, la trasera puede dejar de verla (zona ciega bajo
   la cámara); el pedido es empujar hasta la línea.
4. **Cooldown 2 s.** Al despegarse de la línea la pelota queda atrás de nuevo → sin cooldown entraría en
   loop línea↔adelante.
5. **No empuja con DOWN viejo.** Sin línea no hay condición de corte; el homing sí lo tolera (tiene
   safety de 50 s), acá no.

## Verificación

- `bash scripts/run-host-tests.sh test_cameras_fusion` → 32/32 OK.
- `g++ -std=gnu++17 -fsyntax-only -Wall -Wextra` de `amix_fsm.cpp` en 5 combinaciones de flags (base,
  base+flag, #9, #9+flag, #9+flag+RETRO_BRAKE/ORIENT_ESC/RETRO_BY_TIME) → sin errores ni warnings.
- Preprocesado (`g++ -E -P`) de `amix_fsm.cpp` SIN el flag nuevo **idéntico** al de `HEAD` en 3 combinaciones
  (incluida la #9). `cameras_runtime.cpp` sin el flag: sólo suma la DECLARACIÓN de la función nueva.
- `pio project config` resuelve los dos envs con los flags esperados.
- ⚠️ **NO** se pudo correr `pio run`: el registry de PlatformIO está bloqueado por la política de red de la
  sesión cloud. Pendiente en la PC del taller (TASK-124 paso 0, con md5 de los binarios viejos).

## Ajuste posterior (mismo día): potencia del empuje = la del pateo

Pedido: "que el empuje hacia atrás lo haga con la misma potencia con la que patea hacia adelante". Primitiva
nueva `retroceder_empuje()` en `amix_motors.cpp` (gateada): copia la rampa de `avanzar_patear()` (comparte su
estado; `parar()` la cierra al entrar/salir) pero con el patrón de retroceso. **Riesgo nuevo:** a 191 de PWM
(antes 106) la inercia al ver la línea es mayor y `parar()` es rueda libre → puede pasarse de la línea con la
pelota. Si pasa en banco: freno activo corto, como `frenar_patada`. Sin el flag, `amix_motors.cpp` y
`amix_fsm.cpp` siguen con preprocesado idéntico al anterior.

## Pendiente

Banco completo → [`team-tasks/2026-10-03-task-124-banco-arqueromix-empuje-trasera.md`](../team-tasks/2026-10-03-task-124-banco-arqueromix-empuje-trasera.md).
Compilar ≠ anda.
