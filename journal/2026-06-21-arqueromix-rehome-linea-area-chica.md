---
title: "arqueromix — re-homing al detectar la línea del área chica en la patrulla + tag de checkpoint"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco)"
status: COMPILA · NO validado en banco
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: feature-banco
---

# arqueromix — re-homing por línea del área chica

## Checkpoint guardado (pedido Virginia)

Antes de este cambio se tagueó la versión que anda bien como punto de retorno:
**`arqueromix-ok-patrulla-arco-2026-06-21`** (commit `965af1f`, pusheado a origin).
Para volver: `git checkout arqueromix-ok-patrulla-arco-2026-06-21 -- "software/teensy/Soccer 2026/src/arqueromix/"`
(o `git revert` del commit de este cambio).

## Pedido (Virginia)

Durante la patrulla por cámara, si DOWN detecta blanco con la parte trasera (= línea del ÁREA CHICA,
porque derivó hacia atrás metiéndose al arco), que NO se siga metiendo: que re-haga "eso que ya
funcionaba" (va para atrás hasta detectar blanco → avanza sin leer) y luego vuelva a leer la cámara
del arco.

## Implementación

- `amix_config.h`: `AMIX_REHOME_ON_LINE` (default true; `-DARQMIX_NO_REHOME_LINE` desactiva).
- `amix_fsm.cpp`: al inicio de `moverce_derecha`/`moverce_izquierda`, si
  `AMIX_REHOME_ON_LINE && AMIX_PATRULLA_POR_ARCO && linea()` → `parar()` + `estado = inicio_retroceder`.
  Reúsa la secuencia de homing tal cual (retrocede→blanco→avanza ciego→reanuda patrulla por cámara).
  Sólo en patrulla-por-arco (en fallback por línea, la línea ya es el rebote).

## ⚠️ Riesgo marcado (honestidad)

El re-home entra a `inicio_retroceder`, que arranca yendo HACIA ATRÁS hasta ver blanco. Si está
sólido sobre la línea → detecta en ~1 tick y avanza (bien). Pero si la línea PARPADEA al entrar,
podría retroceder de más (metiéndose al arco) hasta el safety de 50 s (TEMPORAL). Depende de la
geometría del área (si retroceder mantiene blanco o lo cruza) que no puedo verificar sin banco.
**Alternativa si en banco retrocede demasiado:** ir directo a `inicio_avanzar` (avanzar para SALIR
de la línea, sin retroceder) — es un renglón. Por ahora se respeta el pedido literal (homing completo).
Mitigación adicional: bajar `AMIX_T_INICIO_RETRO_SAFETY` de 50 s a ~4 s (ya pendiente).

## Verificación

- `pio run -e central_robot2_arqueromix` (default) → **SUCCESS**.
- `-DARQMIX_NO_REHOME_LINE -DARQMIX_PATRULLA_LINEA` (fallbacks) → **SUCCESS**.
- ⚠️ Compila ≠ anda. Lo cierra el equipo en banco.

## Cómo verificar (Virginia)

1. Flashear `central_robot2_arqueromix`. Patrullando, empujá al arquero hacia atrás (hacia el arco)
   hasta que DOWN vea la línea del área: debe **dejar de meterse**, re-homing (retrocede a blanco →
   avanza a ciegas) y volver a la patrulla por cámara.
2. ¿Retrocede DEMASIADO hacia el arco al tocar la línea? → avisame: lo cambio a "avanzar sin
   retroceder" (más seguro), o bajamos el safety del retroceso.
3. ¿No reacciona a la línea? → revisar que DOWN esté reportando línea (monitor).

## Archivos

- `amix_config.h` (`AMIX_REHOME_ON_LINE`), `amix_fsm.cpp` (re-home en moverce_*), `DOCUMENTACION.md` (§17.3).
- Tag: `arqueromix-ok-patrulla-arco-2026-06-21`.
