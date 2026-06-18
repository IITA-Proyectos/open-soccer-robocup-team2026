---
title: "FSM del arquero — estados cuando cuida el arco moviéndose de lado"
date: 2026-06-17
status: vivo — verificado contra src/central/strategy.cpp (HEAD del 2026-06-17)
tipo: referencia-firmware
fuente: "strategy.cpp goalkeeper_tick (env de competencia central_robot2_arquero, SIN GK_SIMPLE_STRAFE)"
---

# Cómo se mueve el arquero (su máquina de estados)

> **FSM = máquina de estados finitos:** el robot está siempre en UN estado, hace
> una sola cosa, y salta a otro estado cuando se cumple una condición. Nada de
> "hace todo a la vez": una cosa por vez, con reglas claras de cuándo cambia.

**La esencia en una frase:** el arquero se pega a su línea de fondo y patrulla de
lado a lado tapando el arco con un patrón **mover → parar → enderezar → mover**; si
ve la pelota sale a interceptarla y despejarla, y si toca una línea blanca rebota
para el otro lado.

Esto sale de `goalkeeper_tick()` en `strategy.cpp`. El env que corre en R2 es
`central_robot2_arquero` (NO define `GK_SIMPLE_STRAFE`, así que corre la **patrulla
v3.3 segmentada**, no el arquero simple de María).

## Mapa rápido

El arquero tiene **6 estados grandes** (`enum GkState`, strategy.cpp:90-95):
`WAIT_START`, `GOTO_LINE`, `PATROL`, `INTERCEPT`, `CLEAR`, `LINE_AVOID`.

⚠️ **`LINE_AVOID` es código muerto en este build.** El enum lo tiene, pero **nadie
lo activa nunca** (no hay un solo `transition_gk(LINE_AVOID)` en todo el archivo).
El reflejo de borde se resuelve por otro lado (ver más abajo). Lo documento para que
nadie pierda tiempo buscando por qué "no entra".

El estado donde de verdad **se mueve de lado cuidando el arco es `PATROL`**, y adentro
tiene su propio **sub-FSM de 5 fases** (`pphase`): `MOVE`, `STOP`, `PULSO`, `SETTLE`,
`REACQ`. Más **2 mecanismos** que no son estados pero cambian el movimiento: el
**rebote por línea** (dentro de MOVE) y el **freno de borde** (un reflejo que pisa
todo).

```
            START del árbitro
                  │
   WAIT_START ────► GOTO_LINE(retroceso) ──► GOTO_LINE(avance) ──► PATROL
                          ▲                                          │  ▲
                          │ (router: borde inminente)                │  │ pierde pelota
                          └──────────────────────────────────────┐  ▼  │
                                                            ve pelota → INTERCEPT ⇄ CLEAR
   PATROL (sub-FSM lateral):
       MOVE ──(fin tramo 1200ms / rebote / límite pose)──► STOP
       STOP ──(300ms)──► PULSO  (si quedó chueco)
                       └► MOVE   (si derecho y ve línea)
                       └► REACQ  (si derecho y NO ve línea)
       PULSO ──(40-80ms)──► SETTLE ──(700ms)──► STOP   (lazo de enderezado)
       REACQ ──(ve línea / 700ms)──► MOVE
   Reflejo (pisa cualquier estado): FRENO DE BORDE, se suelta solo a 350ms
```

---

## A. Antes de patrullar (llegar y pegarse al arco)

### `WAIT_START` — esperando el árbitro
- **Qué hace:** quieto, motores en cero. Es el estado inicial.
- **Sale cuando:** el árbitro da GO (`match_running`) **y** pasaron 200 ms de
  margen (`GK_START_DELAY_MS`).
- **Espera:** la señal de START del árbitro.
- **Próximos:** `GOTO_LINE`.
- **Watchdog:** no (los 200 ms son un delay de arranque, no un timeout de fallo).

### `GOTO_LINE` — fase 0: retroceso al arco
- **Qué hace:** retrocede recto hacia su arco (vy = −420 mm/s), manteniendo el
  frente con el giroscopio.
- **Sale cuando:** los sensores de piso (DOWN) detectan la línea del arco **o**
  vence el timeout.
- **Espera:** tocar su línea de fondo.
- **Próximos:** `GOTO_LINE` fase 1 (avance).
- **Watchdog:** **4000 ms** (`GK_GOTO_LINE_TIMEOUT_MS`) → pasa igual al avance
  (asume que ya llegó).

### `GOTO_LINE` — fase 1: avance / despegue
- **Qué hace:** avanza ~10 cm (vy = +300 mm/s) para no patrullar pisando la línea
  (pegado, el control de borde saturaba y trompeaba).
- **Sale cuando:** deja de ver la línea por 100 ms seguidos (`GK_ADVANCE_MS`).
- **Espera:** despegarse limpio de la línea.
- **Próximos:** `PATROL` (y auto-captura el centro de patrulla si la pose es
  confiable).
- **Watchdog:** **1500 ms** (`GK_ADVANCE_TIMEOUT_MS`) → pasa igual a `PATROL`.

> Este `GOTO_LINE` fase-1 es **también** a donde manda el "router" cuando detecta
> borde inminente desde cualquier estado (ver §B, reflejos).

---

## B. La patrulla lateral (el núcleo: moverse de lado cuidando el arco)

`PATROL` es un contenedor: no comanda movimiento directo, sino que corre su sub-FSM
de fases. Sale de `PATROL` hacia arriba cuando **ve la pelota** (→ `INTERCEPT`) o
cuando el router fuerza `WAIT_START` (STOP) o `GOTO_LINE` (borde).

> **Strafe = moverse de costado** sin girar (las 3 ruedas omni combinadas dan
> traslación pura). En MOVE el robot hace strafe puro.

### `PATROL/MOVE` (pphase 0) — el tramo de strafe
- **Qué hace:** se desplaza de lado a velocidad fija (vx = ±200 mm/s,
  `GK_PATROL_SPEED_MM_S`), vy = 0, **ω = 0 a propósito** (mezclar giro con strafe
  en estos motores degenera en bandazos).
- **Sale cuando:** (1) **rebote por línea** — ve línea lateral fresca → invierte el
  sentido; (2) **límite por pose** — `my_x` cruza el borde de la ventana
  [centro ± 350 mm]; (3) **fin de tramo** por tiempo.
- **Espera:** llegar al borde de su recorrido (por línea, por pose, o por tiempo).
- **Próximos:** `PATROL/STOP`.
- **Watchdog:** **1200 ms** (`GK_PATROL_SEG_MS`) → `STOP`.

### `PATROL/STOP` (pphase 1) — frená y decidí
- **Qué hace:** frena (cmd en 0) y mide el rumbo quieto para decidir el próximo
  movimiento.
- **Sale cuando:** pasan 300 ms. Ahí decide:
  - si quedó **chueco** (|error de rumbo| > 35° y le quedan pulsos) → `PULSO`;
  - si está **derecho y ve la línea** → `MOVE`;
  - si está **derecho y NO ve la línea** → `REACQ`.
- **Espera:** 300 ms quieto para leer un rumbo fresco.
- **Próximos:** `PULSO`, `MOVE` o `REACQ`.
- **Watchdog:** **300 ms** (`GK_PATROL_STOP_MS`) → al destino que decidió.

### `PATROL/PULSO` (pphase 2) — enderezarse
- **Qué hace:** giro puro corto hacia el frente (ω = ±40°/s) para corregir el rumbo
  que el strafe descuadró.
- **Sale cuando:** el error de rumbo baja a ≤ 25° (corte en vivo, anticipa la
  inercia) **o** se cumple la duración del pulso.
- **Espera:** quedar derecho.
- **Próximos:** `SETTLE`.
- **Watchdog:** **40 a 80 ms** (duración dinámica ∝ error, `GK_REORIENT_PULSE_MIN/MAX_MS`)
  → `SETTLE`.

### `PATROL/SETTLE` (pphase 3) — asentar
- **Qué hace:** quieto, para que la inercia del giro termine y lleguen 2 muestras
  frescas de rumbo (el snapshot llega lento, ~4 Hz).
- **Sale cuando:** pasan 700 ms.
- **Espera:** que se asiente el giro.
- **Próximos:** `STOP` (re-evalúa si quedó derecho — máximo 2 pulsos por parada).
- **Watchdog:** **700 ms** (`GK_REORIENT_SETTLE_MS`) → `STOP`.

### `PATROL/REACQ` (pphase 4) — re-enganche de la línea
- **Qué hace:** retrocede despacio (vy = −200 mm/s) para volver a ver la línea (la
  patrulla vive pegada a la línea; sin verla, los rebotes no la guían).
- **Sale cuando:** vuelve a ver la línea **o** vence el tiempo.
- **Espera:** re-ver la línea blanca de referencia.
- **Próximos:** `MOVE`.
- **Watchdog:** **700 ms** (`GK_PATROL_REACQ_MAX_MS`) → `MOVE` (y cuenta 2 "secas"
  seguidas como guard anti-caminar-al-arco).

### Mecanismo: REBOTE por línea lateral (dentro de MOVE, no es un estado)
- **Qué hace:** si DOWN ve línea fresca, NO está "atrás" (|ángulo| < 135°) y pasó el
  cooldown → **invierte el sentido** del strafe para alejarse.
- **Cooldown:** **800 ms** (`GK_PATROL_BOUNCE_COOLDOWN_MS`) entre rebotes = ~16 cm a
  200 mm/s antes de poder rebotar de nuevo.
- ⚠️ Si el robot está rotado, la línea lateral puede aparecer a ~±135° y caer como
  "atrás" → **no rebota** (limitación conocida).

### Reflejo: FRENO DE BORDE (preempta cualquier estado)
- **Qué hace:** vive en `main_central.cpp` (fuera de la FSM); si hay borde inminente
  (≥6 sensores en blanco) frena. El "router" de `goalkeeper_tick` además manda a
  `GOTO_LINE` fase-1 para despegarse.
- **Watchdog:** se **auto-suelta a 350 ms** (`GK_EDGE_BRAKE_MAX_MS`, anti-clavado).
- **Antes de disparar:** debounce 150 ms + cooldown 400 ms (`GK_LINE_AVOID_*`).

---

## C. Cuando ve la pelota

### `INTERCEPT` — seguir la pelota de lado
- **Qué hace:** sigue la X predicha de la pelota (vx ∝ error, vy = 0), acotado por la
  pose a ±350 mm del centro.
- **Sale cuando:** la pelota se acerca a < 250 mm → `CLEAR`; o desaparece → `PATROL`.
- **Próximos:** `CLEAR`, `PATROL` (o `GOTO_LINE`/`WAIT_START` por el router).
- **Watchdog:** no.

### `CLEAR` — salir a despejar
- **Qué hace:** va derecho a la pelota (500 mm/s) y la empuja lejos (no hay pateador;
  empuja por inercia).
- **Sale cuando:** la pelota se aleja a > 400 mm → `INTERCEPT` (histéresis); o
  desaparece → `PATROL`.
- **Próximos:** `INTERCEPT`, `PATROL` (o router).
- **Watchdog:** no.

---

## D. Estado muerto

### `LINE_AVOID` — **inalcanzable en este build**
- Existe en el enum y tiene su `case`, pero **nadie lo activa** (0 transiciones a él
  en todo `strategy.cpp`). Su trabajo (huir de la línea a 420 mm/s) lo cubren el
  router (→ `GOTO_LINE`) y el rebote del MOVE. No lo busques en banco: no entra.

---

## Tabla resumen de watchdogs (timeouts de tiempo)

| Estado | Watchdog | Va a |
|---|---|---|
| WAIT_START | 200 ms (delay de arranque) | GOTO_LINE |
| GOTO_LINE retroceso | **4000 ms** | GOTO_LINE avance |
| GOTO_LINE avance | **1500 ms** | PATROL |
| PATROL/MOVE | **1200 ms** | PATROL/STOP |
| PATROL/STOP | **300 ms** | PULSO / MOVE / REACQ (lo que decida) |
| PATROL/PULSO | **40–80 ms** (∝ error) | PATROL/SETTLE |
| PATROL/SETTLE | **700 ms** | PATROL/STOP |
| PATROL/REACQ | **700 ms** | PATROL/MOVE |
| INTERCEPT | — | — |
| CLEAR | — | — |
| Freno de borde (reflejo) | **350 ms** (auto-suelta) | (libera, sigue la FSM) |

Otros tiempos que no son watchdog de estado pero gobiernan el movimiento:
rebote cooldown **800 ms**; router de borde debounce **150 ms** + cooldown **400 ms**.

---

## Notas honestas (para no perseguir fantasmas)

- **`LINE_AVOID` no entra nunca** en este build. El "escape de línea lateral" real
  es solo el **rebote** del MOVE, que **invierte el sentido a la misma velocidad
  cuantizada** (no hay un retroceso enérgico para el costado). Por eso, si la pose
  falla y la línea se ve tarde, el robot se puede ir de cancha.
- **ω = 0 en MOVE** es a propósito, pero deja sin corregir la deriva parásita
  (~80°/s) durante el tramo; el enderezado solo ocurre **parado** (PULSO). Ese es el
  precio de la patrulla segmentada.
- **Bajar `GK_PATROL_SPEED_MM_S` no frena** el strafe: con los pisos de PWM
  {70,70,107} + FLOOR_SCALE, 200 mm/s ya cae en el mínimo continuo. Para ir más
  lento (debug) hay que **pulsar** (env `central_robot2_arquero_pulse`).
- Verificado contra `strategy.cpp` (`goalkeeper_tick`, líneas ~1414-1858) el
  2026-06-17. Si se editan esas constantes, re-chequear esta tabla.
