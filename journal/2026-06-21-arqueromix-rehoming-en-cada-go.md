---
title: "arqueromix — re-hacer el homing del área en CADA go (no solo el primero) + sentido del retroceso validado"
date: 2026-06-21
author: "Claude Opus 4.8 (1M context)"
requested-by: "Virginia Viollaz (banco)"
tipo: firmware-bugfix
toca-competencia: NO (solo src/arqueromix/, build aislado)
---

# Sesión 2026-06-21 (banco, 7ª iteración) — re-homing en cada GO

## Reporte de Virginia
El programa BASE (`central_robot2_arqueromix`) funcionó: el homing va para atrás bien (el sentido
del retroceso era el correcto → NO hizo falta `_retroflip`). PERO el homing lo hace **una sola vez**:
solo el primer GO. Si lo paran (STOP) y le dan GO de nuevo SIN apagar la batería, ya no retrocede —
patrulla, pero no va para atrás. Pide: que SIEMPRE, después de cada GO, vuelva a hacer el homing.

## Causa raíz
El estado del FSM es estático y `amix_fsm_init()` (que setea `estado=inicio_retroceder`) solo corre
una vez en `setup()`. El gate del árbitro en `amix_fsm_tick` solo hacía `parar()` cuando
`match_running=false` — NO reiniciaba el estado. Entonces, tras un STOP, el FSM quedaba en
`moverce_*` (patrullando), y el siguiente GO continuaba desde ahí, sin re-homing.

## Fix
Detección de flanco STOP→GO en `amix_fsm_tick` (`static bool s_was_running`):
- `match_running==false` → `parar()` + `s_was_running=false`.
- primer tick con `match_running==true` tras un STOP (o el primer GO) → reinicia el FSM:
  `estado=inicio_retroceder`, `millis_inicio_estado=millis()`, `pd=base`.
Así CADA GO re-arranca el homing al área chica. (El kick-ramp ya se resetea en `parar()`.)

## Verificación
- **Compila SUCCESS** `central_robot2_arqueromix`.
- Competencia byte-idéntica (build aislado). NO validado en HW (pero el cambio es chico y directo).

## Estado del arqueromix (resumen del día)
Ciclo del arquero funcionando en banco (Virginia): homing al área (va atrás, validado) → patrulla
siguiendo la pelota por ángulo → despeje con rampa simétrica → vuelve a patrullar; ahora re-homing
en cada GO. Signo de rumbo -1 (validado). Pendiente fino: bajar el safety del retroceso de 50 s a
~4 s; tunear `AMIX_T_INICIO_AVANCE` / `AMIX_KICK_VEL_FINAL` / `AMIX_TOL_*` a gusto. Cierre = TASK-114.

## Referencias
- `src/arqueromix/DOCUMENTACION.md §16`. Journals del día: `2026-06-21-arqueromix-*`.