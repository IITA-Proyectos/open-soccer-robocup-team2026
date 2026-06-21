---
title: "arqueromix — bajar SOLO la velocidad del avance del homing (banco Virginia)"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco)"
status: COMPILA · NO validado en banco
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: fix-banco
---

# arqueromix — avance del homing: bajar SOLO la velocidad

## Contexto

Tras revertir mis 2 cambios anteriores (que invertían sentido + acortaban), Virginia confirmó en
banco que el movimiento "sin leer los sensores luego de detectar la línea" (`inicio_avanzar`) **anda
de golpe / fuerte** y pidió **bajarle la velocidad**.

Verificado (2 veces, en disco): el revert fue completo (0 refs a mis símbolos viejos). El "de golpe"
NO es resto mío — es el base: `avanzar()` salta a PWM 100 sin rampa. Mis cambios revertidos de hecho
lo BAJABAN; al volver al base, quedó más fuerte.

## Cambio (mínimo, SOLO velocidad — una cosa por vez)

`inicio_avanzar` pasa de `avanzar()` (PWM `AMIX_AVANZAR=100`) a `avanzar_inicio()`, IDÉNTICO salvo la
velocidad: usa `AMIX_INICIO_AVANCE_PWM=75`. **MISMO sentido** (M1=+, M2=−, M3=0), **mismos 400 ms**.
NO se invierte nada, NO se acorta. La única diferencia vs el base es el PWM. Se decopla del despeje:
`avanzar()` (post-patada) sigue a 100.

Diferencia con mis cambios revertidos: aquellos invertían el sentido y hacían el avance momentáneo
(200 ms) → rompían. Este NO: respeta el base que funcionaba y baja solo la velocidad.

## Verificación

- `pio run -e central_robot2_arqueromix` → **SUCCESS** (~16 KB).
- Feature "despeje al arco" del compañero: intacta (no se tocó).
- ⚠️ Compila ≠ anda. Lo cierra el equipo en banco.

## Cómo verificar (Virginia)

1. Flashear `central_robot2_arqueromix`. Al GO: retrocede → detecta línea → avanza **más suave** (PWM 75).
2. Si **stuttea / no arranca** el avance → PWM muy bajo (cerca del piso 70), subir `AMIX_INICIO_AVANCE_PWM`
   hacia 85.
3. Si **todavía va "de golpe"** aunque más lento → el tema NO es el PWM sino que arranca SIN rampa
   (salta al PWM de una). Avisame y le agrego una rampa de arranque suave (como tiene la patada), que
   es la herramienta correcta para el "de golpe". (No la meto ahora: un cambio por vez.)

## Archivos

- `amix_config.h` — nuevo `AMIX_INICIO_AVANCE_PWM=75` (velocidad del avance del homing).
- `amix_motors.{h,cpp}` — primitiva `avanzar_inicio()` (= avanzar pero con esa velocidad; mismo sentido).
- `amix_fsm.cpp` — `inicio_avanzar` llama `avanzar_inicio()`.
- `DOCUMENTACION.md` — §16 + Tunear.
