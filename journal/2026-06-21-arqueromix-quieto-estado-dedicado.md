---
title: "arqueromix MODO QUIETO — reescrito con estado dedicado esperar_quieto (no usa la patrulla)"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, banco Virginia (análisis línea por línea)"
status: COMPILA · NO validado en banco
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: fix-banco
---

# arqueromix quieto — estado dedicado (no la patrulla)

## Reporte (Virginia, banco) + reclamo

El quieto NO se quedaba quieto: iba atrás, tocaba la línea de fondo, salía con fuerza al frente, y
seguía **moviéndose para adelante sin parar**. Virginia (con razón): el enfoque de parchar la patrulla
con flags está mal; es un programa SIMPLE → borrar lo lateral si hace falta y hacerlo bien.

## Causa raíz (análisis línea por línea del amix_fsm.cpp REAL)

El quieto reusaba `moverce_*`, que tiene DOS cosas que generan movimiento parásito:
1. **PROFUNDIDAD (línea 194/249):** `if (AMIX_PROFUNDIDAD_POR_LINEA && goal_own_visible && linea()) →
   inicio_avanzar`. La cámara SÍ ve el arco propio + la cancha está llena de líneas → cada línea que
   pisa lo manda a avanzar al frente → vuelve a moverce → pisa otra línea → avanza → **camina para
   adelante sin parar**. ← ESTO era el "continúa moviéndose para adelante".
2. **REBOTE por línea** (ya gateado en el intento anterior): las líneas del piso lo hacían rebotar de costado.

Parchar `moverce_*` con flags no escala: arrastra toda la maquinaria de patrulla.

## Solución (estado dedicado, limpio)

El quieto deja de usar `moverce_*`. Estado nuevo **`esperar_quieto`** con SOLO 3 ramas:
```
esperar_quieto:
   pelota CERCA+al frente → PATEANDO_pausa_inicial (patea, reusa la secuencia)
   pelota DESCENTRADA     → strafe lateral hacia ella (ad/aiproporcional, corrección de rumbo)
   si no                  → parar()  (QUIETO)
```
Sin rebote, sin profundidad, sin fallback de línea → cero movimiento parásito.

Transiciones (gateadas por `AMIX_QUIETO`, constexpr):
- `inicio_avanzar` (homing) → `esperar_quieto` (quieto) / `moverce_derecha` (default).
- `avanzar_despues_de_patear` (post-despeje) → `esperar_quieto` (quieto) / `moverce_derecha` (default).

Además se LIMPIÓ `moverce_*`: se quitaron los gates de quieto del intento anterior → la patrulla
default volvió a su código limpio.

## Default byte-idéntico

Sin `-DARQMIX_QUIETO`, las transiciones van a `moverce_derecha` (constexpr resuelto) y `esperar_quieto`
nunca se entra. La patrulla normal NO cambia.

## Verificación

- `pio run -e central_robot2_arqueromix_quieto` → **SUCCESS**.
- `pio run -e central_robot2_arqueromix` (default) → **SUCCESS** (patrulla limpia).
- ⚠️ Compila ≠ anda. Banco.

## A probar (Virginia)

`pio run -e central_robot2_arqueromix_quieto -t upload`. Sin pelota: homing → debe quedar **QUIETO**
(ni de costado ni para adelante). Pelota descentrada → strafe hasta enfrentarla, frena al centrar.
Pelota cerca → patea y vuelve a quieto.

## Archivos

- `amix_fsm.h` (+estado `esperar_quieto`, 13 estados), `amix_fsm.cpp` (estado nuevo + 2 transiciones +
  limpieza de moverce_*), `DOCUMENTACION.md` (§17.5 reescrita).
