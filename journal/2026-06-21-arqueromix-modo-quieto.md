---
title: "arqueromix — MODO QUIETO (versión de prueba): no patrulla, espera quieto la pelota"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco) + workflow paralelo"
status: COMPILA · NO validado en banco
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: feature-banco
---

# arqueromix — MODO QUIETO

## Pedido (Virginia)

Otra versión de prueba, SEPARADA (dejar la de patrulla como estaba, que andaba mejor). El arquero
quieto: homing igual (atrás hasta línea → se adelanta para salir) → se queda QUIETO buscando la pelota
→ si la ve LEJOS, se mueve lateral hasta enfrentarla → si está CERCA, se adelanta y patea (igual que
antes). Como el anterior pero simplificado: mientras espera, NO se mueve.

## Diseño (workflow paralelo: 2 lectores + síntesis)

Opción elegida: **reusar `moverce_*` con un gate**, sin estados nuevos ni primitivas nuevas (la más
simple y deja el default byte-idéntico). El seguimiento de pelota y la patada YA existen → se reusan.

## Implementación (3 archivos, mínima)

1. `amix_config.h`: `constexpr bool AMIX_QUIETO` (`#ifdef ARQMIX_QUIETO`).
2. `amix_fsm.cpp`: en `moverce_derecha`/`moverce_izquierda`, el `ad/aiproporcional(pd,error)` del tope
   se gatea:
   ```
   if (!AMIX_QUIETO || (haypelota && millis() >= s_commit_until_ms && !ball_alineada() && !ball_para_despejar()))
       adproporcional(pd, error);
   ```
   En quieto strafea SOLO si sigue una pelota descentrada; si no, queda quieto (por el `parar()` de las
   ramas de pelota). El resto del `if(haypelota)` no se toca (es transición de estado + pd, no movimiento).
3. `platformio.ini`: env `central_robot2_arqueromix_quieto` = base + `-DARQMIX_QUIETO` (patrón _retroflip).

**Se mantiene activo en quieto** (deliberado, seguridad): homing, rebote por arco/línea, profundidad por
línea, secuencia de despeje. Si el arquero se corrió siguiendo la pelota, el rebote por arco lo trae.

## Default byte-idéntico

Sin `-DARQMIX_QUIETO`, `AMIX_QUIETO=false` → el `||` corta y el strafe corre incondicional como antes.
La patrulla normal (`central_robot2_arqueromix`) NO cambia.

## Verificación

- `pio run -e central_robot2_arqueromix_quieto` → **SUCCESS**.
- `pio run -e central_robot2_arqueromix` (default patrulla) → **SUCCESS**.
- ⚠️ Compila ≠ anda. Lo cierra el equipo en banco (regla #1).

## Plan de banco (Virginia)

`pio run -e central_robot2_arqueromix_quieto -t upload`
1. Homing igual que el base. 2. Sin pelota → queda PARADO (no patrulla). 3. Pelota alineada+lejos →
quieto. 4. Pelota descentrada → strafe hasta enfrentarla, frena al centrarla. 5. Pelota cerca → patea
igual y vuelve a quieto. 6. Empujar atrás (al área) con el arco a la vista → avanza y sale.
Si tiembla por parpadeo de pelota → avisar (se agrega persistencia corta).

## Riesgo a mirar

Si la pelota parpadea (visible/no), podría alternar seguir↔quieto y temblar. No se incluyó histéresis
de entrada (YAGNI); es un knob chico a agregar si el banco lo pide.

## Archivos

- `amix_config.h` (AMIX_QUIETO), `amix_fsm.cpp` (gate en moverce_*), `platformio.ini` (env nuevo),
  `DOCUMENTACION.md` (§17.5). `amix_fsm.h`/`amix_motors.*` NO se tocan.
