---
title: "arqueromix — revert del movimiento de patrulla (medialuna izq) al estado que andaba mejor"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco)"
status: COMPILA · = comportamiento del tag arqueromix-bueno-2026-06-21-patrulla-lenta
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: revert
---

# arqueromix — revert del motion (medialuna)

## Por qué

Virginia (banco): el último programa (con mis fixes: AI_REAR 65 + sesgo forward 10 + T_SALIR 380 +
AVANCE 500) hacía una **MEDIALUNA hacia la izquierda** (sin corregir bien el rumbo / BNO). Pidió:
"el programa anterior dejalo como estaba antes, que andaba mejor".

## Qué se hizo (revert de SOLO el motion; doc y comentarios buenos quedan)

Vuelto a los valores del tag `arqueromix-bueno-2026-06-21-patrulla-lenta` (verificado con `git show`):
- `AMIX_AI_REAR_ENEG` 65 → **75**.
- `AMIX_T_SALIR_LINEA` 380 → **350**.
- `AMIX_T_INICIO_AVANCE_MIN` 500 → **400**.
- **Sesgo forward APAGADO por default** (`AMIX_FORWARD_BIAS_PWM`: default 0; opt-in con `-DARQMIX_FORWARD_BIAS`).
  Con el sesgo en 10 + el rumbo sin corregir bien hacía la medialuna. El mecanismo queda (dormido) por si se
  quiere experimentar.

El strafe (ad/aiproporcional) queda BYTE-equivalente al tag (bias=0 = no-op). Las mejoras de comentarios y
documentación (§5 diagrama 12 estados, etc.) NO se revierten — son independientes del comportamiento.

## Nota: el "usar los BNO"

Virginia marcó que la medialuna era "sin el control de los BNO". La causa de fondo (el rumbo/heading no
corrige bien la deriva del strafe) es un tema SEPARADO del heading del TOP, no de estas constantes. Queda
pendiente. La versión QUIETA que se está diseñando lo sortea (quieto = no patrulla = no medialuna).

## Verificación

- `pio run -e central_robot2_arqueromix` → **SUCCESS**.
- ⚠️ Compila ≠ anda. Lo cierra el equipo en banco.

## Próximo

Versión NUEVA "arquero QUIETO" (env separado `-DARQMIX_QUIETO`): homing → quieto esperando → sigue la
pelota lateral para enfrentarla → patea si cerca. En diseño (workflow). NO toca esta versión de patrulla.
