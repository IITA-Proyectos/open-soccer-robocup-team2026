---
title: "Diseño — post-patada del arquero quieto: orientar/retroceder/centrar por GIROSCOPIO"
date: 2026-06-21
status: vivo (diseño; Fase 1 implementada, Fases 2-3 pendientes)
audiencia: "Quien siga el rediseño del post-patada del arqueromix (Virginia, Elías, 2027)"
author: "Claude Opus 4.8 (1M), vía Claude Code — pedido Virginia"
requested-by: "Virginia Viollaz"
analisis: "workflow paralelo 6 agentes (4 lectores + síntesis + crítica adversarial), verificado contra código real"
---

# Post-patada del arquero quieto, por GIROSCOPIO

## Problema (reportado en banco por Virginia)

Tras patear, el arquero debe quedar **mirando al arco del oponente** y **retroceder a su
arco manteniendo ese frente**, como un arquero parado fuera del área. El control actual
(`orientar_frente` + `retroceder_rumbo_opp`) **no corrige bien**: poca ganancia / cae en
zona muerta. Virginia pide usar el **giroscopio, no la cámara**, con un control parecido al
del strafe lateral (que SÍ funciona), y **movimientos correctos primero** (lento está bien).

## Diagnóstico (verificado en código real)

Dos causas que se suman:

1. **Señal equivocada — cámara, no giroscopio.** `orientar_frente` (`amix_fsm.cpp`) gira
   mirando `goal_opp_angle` (cámara TOP, ~4 Hz, se ensucia/smearea al moverse), y
   `PATEANDO_atras` llama `retroceder_rumbo_opp(goal_opp_angle, ...)` (`amix_motors.cpp:196`).
   El strafe que SÍ anda usa `heading_error_deg` (giroscopio sellado al arranque,
   `amix_io.h:51`) — estable y sin lag, **ya cableado**.
2. **Autoridad de rotación bajo piso.** `retroceder_rumbo_opp` clampea `rot` a
   `AMIX_ROT_MAX=30`; la trasera (piso ~107) recibe solo `rot` → no se mueve; la corrección
   sale solo del desbalance M1/M2 (~25%). Y cerca del objetivo el proporcional pide PWM <
   piso → no corrige (zona muerta). Es exactamente la trampa de `control-pid-zona-muerta`.

## La idea rectora

Giroscopio para el lazo, y **partir el movimiento en pasos simples donde cada motor trabaja
en su régimen bueno** (sin pelear la zona muerta). Tres fases, **una por vez, banco entre
cada una** (regla del repo: un cambio por flasheo).

### Fase 1 — Orientar al oponente por giroscopio  ✅ IMPLEMENTADA (2026-06-21)

`orientar_frente` gira para llevar `heading_error_deg → 0`. Control **BANG-BANG con banda
ANCHA**, NO proporcional fino:
- `|heading_error| > AMIX_TOL_ORIENTAR_DEG (8°)` → `girar()` al piso (`AMIX_GIRO_FRENTE_PWM=50`)
  en el sentido que reduce el error.
- `|heading_error| ≤ 8°` → `parar()`, mirando al oponente → `PATEANDO_atras`.
- Gateado por `heading_valid`: sin heading bueno → fallback recto (no gira a ciegas).

**Por qué bang-bang y no PID fino** (corrección de la crítica adversarial P0-2): cerca del
cero un proporcional da PWM < piso → no gira → serpentea (el síntoma original, ahora
alrededor del cero). Bang-bang "gira al piso o para" lo esquiva. Es lo que hace el delantero
para apuntar a la pelota (`apuntar_pelota_motores`, PWM fijo + tolerancia 15°), que anda.

**Por qué banda 8° y no fina:** a 4-6 Hz (latencia del heading del TOP) girando al piso el
robot rota varios grados por ciclo; ±2° haría overshoot y serpenteo. ±8° = tolerancia que el
resto del arquero ya usa (`AMIX_TOL_CENTRADO_DEG`).

### Fase 2 — Retroceder recto hasta la línea  ✅ 2a IMPLEMENTADA (2026-06-21) / 2b pendiente

**Fase 2a (hecha):** `PATEANDO_atras` (quieto) retrocede **recto**, **sin** la corrección por
cámara que lo desviaba (`retroceder_rumbo_opp`, sin uso ahora). Para en la primera línea blanca
(`linea()`) o safety 4 s. Como el robot ya quedó mirando al oponente (Fase 1), recto = derecho
hacia su arco → pisa el borde del área y frena. **Ajustes de banco (Virginia, segunda iteración):**
- **Velocidad propia y más lenta** del retroceso del quieto: `AMIX_ATRAS_QUIETO=80` (vs
  `AMIX_ATRAS=120` de la patrulla, que NO se toca) + primitiva `retroceder_quieto()`. A 120 se
  metía al área (cruzaba la línea por inercia + latencia); más lento = para más justo.
- **Seguridad "nunca salirse":** gate de **frescura del enlace DOWN**. La línea se lee cada tick
  (`amix_comm` la refresca antes del FSM); si `down_link_fresh==false` (enlace caído >500 ms), el
  arquero NO retrocede a ciegas → frena. Sin dato de línea confiable, mejor quieto que salirse.

**Consciencia de línea al BUSCAR la pelota (banco Virginia 2026-06-22):** el strafe lateral de
`esperar_quieto` (enfrentar la pelota descentrada) no miraba la línea → se metía al área de
costado. Fix: **si hay `linea()` (o DOWN no fresco) → `avanzar()`** al frente a buscarla en vez de
seguir lateral. El avance es "un poco más grande": al tocar línea arma una **ventana**
`AMIX_T_BUSCAR_AVANCE=400` ms (variable `s_buscar_avance_until_ms`) → sigue avanzando un toque más
allá del borde antes de volver al lateral (el avance corto quedaba pegado). Decisión táctica de
Virginia: aprovechar el borde para salir a buscar.

**Golpe consciente de línea (banco Virginia 2026-06-22):** `PATEANDO_adelante` (el golpe) AÚN
PATEANDO lee `linea()`; en modo quieto, si la detecta **corta el golpe** (→ pausa → orientar →
retroceder) para no salirse de la cancha. Patrulla intacta (gateado por `AMIX_QUIETO`).

**Pendiente de borde total:** queda `inicio_lateral_izq` (strafe izq 1.6 s a ciegas al arranque)
sin chequeo de línea. No priorizado; siguiente paso si se quiere borde 100 %.

**Fase 2b (pendiente, sólo si el recto curva en banco):** mientras retrocede, si el heading se
tuerce más de ~15° → **frenar, re-orientar (Fase 1 bang-bang), reanudar**. Safety 4 s sobre el
tiempo **total** (no reiniciar el timer en cada re-orientación — crítica P2-6).

**Por qué NO "corregir con la trasera"** (corrección de la crítica adversarial P0-1): meter
la trasera a base alta para corregir convierte el retroceso recto en **diagonal** (la trasera
empuja de costado) y el trim no tiene autoridad para compensar esa base. El retroceso es
traslación pura; su corrección correcta es **rotación pura** (parar y girar, donde el motor
gira bien con piso < 70), no mezclar. Desacoplar = cada motor en su régimen bueno. Reemplaza
`retroceder_rumbo_opp` (cámara + clamp 30). Safety 4 s sobre el tiempo **total** (no reiniciar
el timer en cada re-orientación — crítica P2-6).

### Fase 3 — Centrado lateral frente al arco  ⏸️ POSPUESTA (decisión Virginia 2026-06-21)

La única señal de "centrado frente a mi arco" es el **arco propio por la cámara trasera**,
que (a) **no está validado en banco** y (b) **justo cuando el arquero está pegado a su arco,
la cámara trasera no lo ve** → el centrado casi nunca se ejecutaría (crítica adversarial
P1-4). Construir sobre esa señal = código que se saltea siempre.

**Decisión:** posponer hasta validar la cámara trasera en banco → **TASK-224**. Según el
resultado: construir con arco propio, o centrar por otra señal (línea / simetría de tiempo).
Por ahora, tras el retroceso → `esperar_quieto` (como hoy): ya queda razonable.

## Trade-off honesto: "giroscopio, no cámara"

Correcto para el **lazo de movimiento** (la cámara se ensucia al girar). Pero el cero del
giroscopio se sella al arranque y **puede derivar** patada a patada; la cámara del arco es la
única referencia *absoluta* (crítica adversarial P1-3). **Mejora futura (no ahora):** re-anclar
el cero del giroscopio con `goal_opp` cuando el robot está **quieto** (no moviéndose, donde
smearea) — fusión odometría+landmark, skill `fusion-pose-odometria-landmarks`. Con banda 8° y
corrigiendo cada ciclo, la deriva de una patada es tolerable.

## Lo que NO se portó del delantero/GK-real (y por qué)

- **El delantero gira con bang-bang a PWM fijo (~40) + tolerancia 15°** → se adoptó la IDEA
  (no el código): independencia entre programas mantenida.
- **El PI de trim-de-trasera del GK real (`gk_strafe_hold.h`, `strategy.cpp` `GK_STRAFE_KP`
  etc.) existe y está validado para el GK real**, pero es para **strafe lateral** (la trasera
  empujando de costado, sobre su piso con holgura). NO aplica al **retroceso recto** (P0-1) →
  no se porta.

## Valores (Fase 1)

| Constante | Valor | Qué controla |
|---|---|---|
| `AMIX_TOL_ORIENTAR_DEG` | 8° | banda muerta de orientación (mirando al oponente) |
| `AMIX_GIRO_FRENTE_PWM` | 50 | PWM fijo del giro (bang-bang); titrado en banco |
| `AMIX_T_ORIENTAR_SAFETY` | 3000 ms | tope si no logra orientarse |
| `AMIX_GIRO_ALINEAR_SIGN` | +1 (flip `-DARQMIX_FLIP_GIRO_ALINEAR`) | sentido físico del giro (a confirmar banco) |

## Plan de banco (lo cierra el equipo — Claude NO marca done)

Fase 1, env `central_robot2_arqueromix_quieto`, mirar SOLO `orientar_frente` tras un despeje:
1. ¿Queda **mirando al oponente** (±8°) sin serpentear? Criterio de éxito.
2. ¿Gira para el lado **correcto**? Si no → `-DARQMIX_FLIP_GIRO_ALINEAR`.
3. ¿Serpentea alrededor del cero? → subir `AMIX_TOL_ORIENTAR_DEG` (8→10/12).
4. ¿Tironea? → subir `AMIX_GIRO_FRENTE_PWM` (50→55). ¿Muy rápido? → es el piso (no baja más en continuo).

Recién con Fase 1 validada → Fase 2 (retroceso). Fase 3 espera TASK-224.

## Referencias
- Código: `src/arqueromix/{amix_fsm.cpp, amix_config.h, amix_io.h}`.
- Análisis paralelo (6 agentes) + crítica adversarial: journal `2026-06-21-arquero-quieto-orientar-giroscopio.md`.
- Skills: `control-pid-zona-muerta`, `dinamica-omni-3-ruedas`, `fusion-pose-odometria-landmarks`.
