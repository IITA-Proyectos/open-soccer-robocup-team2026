---
title: "arqueromix MODO QUIETO — fix: no quedaba quieto (las líneas de la cancha lo hacían rebotar de costado)"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, banco Virginia"
status: COMPILA · NO validado en banco
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: fix-banco
---

# arqueromix quieto — fix del rebote por líneas

## Reporte (Virginia, banco)

Probó `central_robot2_arqueromix_quieto` SIN pelota en la cancha: "no se queda quieto, va hacia atrás,
luego adelanta, y se pone a mover hacia los costados."

## Diagnóstico (verificado contra el código)

- El flag `-DARQMIX_QUIETO` **SÍ se aplica** (verificado con `pio run -v`: `DARQMIX_QUIETO` en el compile).
- El strafe del tope de `moverce_*` **estaba bien gateado** (no patrulla).
- PERO el **rebote lateral** (`en_borde → salir_linea_*`) NO estaba gateado. En cancha vacía hay **líneas
  blancas** (marcas del piso); como la cámara probablemente NO ve el arco propio (`goal_own_visible=0`),
  el borde cae al **fallback por línea** (`en_borde = linea()`). Al pisar una línea → `salir_linea_*` →
  strafe de costado (a ciegas, NO gateado por quieto) → vuelve a `moverce_*` → re-detecta línea → rebota
  de nuevo → **oscila de costado**. Eso era el "se mueve hacia los costados".

## Fix (mínimo)

En `moverce_derecha`/`moverce_izquierda`, el **rebote lateral** ahora se gatea igual que el strafe: en
modo quieto SOLO dispara si está **siguiendo la pelota** (`haypelota && !ball_alineada()`). Si está
quieto (sin pelota / pelota alineada) → NO rebota → queda quieto. La **profundidad por línea** (empuje
al frente si derivó al área) SIGUE activa (es forward, no causa el costado). El default (patrulla)
queda byte-idéntico (el `!AMIX_QUIETO` corta el gate).

```
if (millis() >= s_commit_until_ms && en_borde &&
    (!AMIX_QUIETO || (haypelota && !ball_alineada()))) { ... salir_linea ... }
// + el fallback else if (linea() && (!AMIX_QUIETO || ...))
```

## Verificación

- `pio run -e central_robot2_arqueromix_quieto` → **SUCCESS**.
- `pio run -e central_robot2_arqueromix` (default) → **SUCCESS** (byte-idéntico).
- ⚠️ Compila ≠ anda. Banco.

## A probar (Virginia)

`pio run -e central_robot2_arqueromix_quieto -t upload`. Sin pelota: tras el homing debe quedar
**QUIETO** (no moverse de costado). Con pelota descentrada: strafe a enfrentarla. Cerca: patea.
Nota: tras el homing puede quedar sobre/cerca de una línea y, si la cámara ve el arco, dar un toque al
frente (profundidad) — eso es forward, no costado, y es la seguridad de no meterse al área.

## Archivos

- `amix_fsm.cpp` (gate del rebote en moverce_* x2), `DOCUMENTACION.md` (§17.5).
