---
title: "arqueromix — patrulla por arco CON fallback a línea (para que no se vaya de largo)"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco)"
status: COMPILA · NO validado en banco
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: fix-banco
---

# arqueromix — patrulla por arco con fallback a línea

## Contexto

Virginia: "el programa de patrulla-por-arco no es el que está cargado, si lo perdiste hacelo de nuevo."

**Verificado: NO se perdió.** El env `central_robot2_arqueromix` no tiene flags, `AMIX_PATRULLA_POR_ARCO=true`,
y el rebote en `moverce_*` lo dispara `borde_arco_der()/izq()`. Es decir, la patrulla-por-arco ES el
default. PERO `borde_arco_*` sólo dispara con `goal_own_visible` → si la cámara NO ve el arco propio,
**no rebota nada** y, como la línea estaba sacada de la patrulla, el arquero **se va de largo** → parecía
que el programa no estaba cargado.

## Fix (lo que hace que el pedido funcione de verdad)

En `moverce_derecha`/`moverce_izquierda`, el borde pasa a ser:
`en_borde = goal_own_visible ? borde_arco_*() : linea()`.

→ **Rebota por el ARCO cuando la cámara lo ve** (pedido Virginia) **y por la LÍNEA cuando no lo ve**.
La patrulla SIEMPRE rebota; nunca se va de largo. El seguimiento del arco sigue siendo lo primario; la
línea es la red cuando la cámara pierde el arco.

**Diagnóstico incorporado:** si rebota ANGOSTO (al borde del arco) está usando la cámara; si rebota
ANCHO (recién en las líneas de cancha) la cámara no ve el arco y está en fallback. Así se ve a simple
vista si la cámara está agarrando el arco.

## Notas honestas (sin cambios)

`goal_own` NO está validado en banco; depende de la calibración LAB de las cámaras y de que el robot
ARRANQUE MIRANDO A LA CANCHA (si arranca girado, la polaridad del TOP queda invertida → el seguimiento
del arco se rompe). Por eso el fallback a línea es importante: sin cámara confiable, igual patrulla.

## Verificación

- `pio run -e central_robot2_arqueromix` → **SUCCESS**.
- ⚠️ Compila ≠ anda. Lo cierra el equipo en banco.

## Cómo verificar (Virginia)

1. Power-cycle el robot **mirando a la cancha** (para la polaridad del TOP). GO.
2. Debe patrullar y **rebotar SIEMPRE** (ya no se va de largo). Si rebota angosto (borde del arco) =
   la cámara ve el arco 👍. Si rebota ancho (líneas de cancha) = está en fallback (cámara no ve el arco).
3. Knobs: `AMIX_TOL_ARCO_OWN_DEG` (umbral del borde del arco), `-DARQMIX_FLIP_ARCO_OWN` (signo),
   `-DARQMIX_PATRULLA_LINEA` (patrulla SÓLO por línea).

## Archivos

- `amix_fsm.cpp` (fallback `goal_own_visible ? borde_arco : linea` en moverce_*), `DOCUMENTACION.md` (§17.2).
