---
title: "Arquero: patrulla SIMPLE izquierda↔derecha que ANDA (banco Gustavo) + fix clasificación marco-cancha + análisis de arquitectura del lazo"
date: 2026-06-18
author: "Claude (sesión coach — Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: ANDA en banco (validado Gustavo 2026-06-18, env central_robot2_arquero_pingpong); 2 mejoras PENDIENTES para mañana
tipo: implementacion + diagnostico
---

# Resumen

El arquero ahora **patrulla de lado a lado (izq↔derecha) de forma confiable** — validado en
banco por Gustavo el 2026-06-18 con `central_robot2_arquero_pingpong`. Se llegó después de
varias iteraciones de banco y un análisis de arquitecto del lazo de control. Todo detrás de
flags, apagado por defecto → los binarios de competencia (`central_robot2_arquero`) quedan
byte-idénticos. **Quedan 2 mejoras para mañana** (heading-hold por giróscopo + posición XY).

# Camino (qué falló y por qué)

1. Primer intento (`central_robot2_arquero_strafe_pid`): strafe continuo + heading-PID por la
   rueda trasera + escape de 2 s. **No andaba**: se detenía antes de la línea y retrocedía.
2. Análisis de arquitecto (workflow): se **descartaron** las hipótesis de bloqueo de serie —
   el lazo es un superloop libre (decenas de kHz), el RX es NO bloqueante (la ISR del UART
   buffea los bytes en un ring de 576 B; el loop solo drena + parsea, barato), control a 100 Hz.
   **El serie NO es el cuello.** (Ver §Hallazgos para no re-investigar.)
3. **Root cause REAL (confirmado adversarialmente):** la clasificación "¿la línea está atrás o
   al costado?" se hacía en **MARCO ROBOT**, sin corregir por el rumbo (`strategy.cpp` rebote
   del MOVE). Con el robot rotado, su propia línea de FONDO se confundía con una lateral →
   escapaba para el lado equivocado / contra la pared. Más el **churn de estados de la v3.3**
   (PULSO→STOP→REACQ): ante rumbo fuera de banda se frenaba a reorientar y RETROCEDÍA.
4. **Solución que ANDA:** FSM **SIMPLE ping-pong** (`-DGK_PINGPONG`) que reemplaza la patrulla
   v3.3: strafe continuo izq↔der, clasificación de línea en **MARCO CANCHA** (la + heading),
   y escape al opuesto **HASTA SALIR** de la línea (mín 0,7 s, tope 4 s). Sin pelota, sin
   retroceso, sin pararse a media patrulla.

# Qué se hizo (host-tested + compila; banco lo cerró Gustavo)

- **`src/shared/gk_strafe_hold.h`** (PURO, host 7/7): PI de rumbo → trim de la rueda TRASERA,
  con zona muerta + anti-windup + flag "fuera de banda". Es la idea de Gustavo de modular con
  pequeñas correcciones la trasera (que tiene holgura sobre su piso, a diferencia de las
  delanteras que se desbordan con ω — banco 2026-06-12).
- **`motors_set_rear_trim()`** en `motors_zircon` (gateado `-DCENTRAL_REAR_TRIM`): suma la
  corrección a la trasera (idx 2) post-floor-scale, cap térmico 150.
- **`strategy.cpp`**: `gk_pingpong_tick()` (FSM simple) + bypass en el router; `GK_STRAFE_PID`
  (versión continua previa); clasificación **marco cancha** (reusa el patrón ya validado de
  `GK_SIMPLE_STRAFE`, banco María 2026-06-14); escape **hasta-salir** (mín/máx).
- **`central_telemetry_serial.cpp`**: `Serial.write` del monitor gateado con `availableForWrite`
  → un host lento (la app va a ~9 Hz) ya NO puede colgar el control de motores. (= el A2 del
  análisis de arquitecto; en partido no hay host → no-op.)
- **Envs banco** (`platformio.ini`): `central_robot2_arquero_pulse` (debug pulsado),
  `central_robot2_arquero_strafe_pid` (continuo+PID), **`central_robot2_arquero_pingpong`**
  (el que ANDA). Competencia byte-idéntica.
- Doc: `docs/firmware/FSM-ARQUERO-ESTADOS.md` (los estados de la FSM del arquero).

# Validación de banco (Gustavo, 2026-06-18)

`central_robot2_arquero_pingpong`: el arquero hace izq↔derecha, toca la línea y escapa al
opuesto a máxima potencia hasta salir. **"Ahora sí anda."** Empezó en strafe PURO (sin trim de
rumbo), que es lo más confiable (banco previo: strafe puro derecho a 200 mm/s). Dato de planta
confirmado: la potencia del escape **ya está al máximo** (a ~590 mm/s la trasera llega al cap
de 150 PWM = límite de quemado de los motores 5 V @ 7,4 V); más mm/s no da más empuje.

# PENDIENTE para mañana (pedido Gustavo)

1. **Heading-hold con giróscopo modulando la rueda trasera.** El módulo (`gk_strafe_hold.h`) y
   el actuador (`motors_set_rear_trim`) YA están y compilan; falta **prenderlo** (agregar
   `-DCENTRAL_REAR_TRIM` al env pingpong) y **confirmar el signo** `GK_REAR_TRIM_SIGN` en banco
   (si corrige al revés, −1) + tunear `GK_STRAFE_KP/KI`. Objetivo: que el robot no derive de
   paralelo durante el strafe. ⚠️ Ojo: el heading del TOP llega a ~4 Hz / ~250 ms viejo (cuello
   real) → ver la rama `feat/top-heading-predict-gateado` de la otra sesión.
2. **Control de posición XY en cancha** con la pose del TOP (`world_model_get_my_x/y_mm` +
   `my_pose_confidence`): que el arquero se ubique respecto de su arco por XY (no solo por tocar
   la línea). Encaja con el spec `docs/superpowers/specs/2026-06-17-localizacion-tof-pose-xy-design.md`.

# Hallazgos de arquitectura (para NO re-investigar)

- El serie **no bloquea**: la ISR del UART buffea los bytes (ring 576 B); el parseo (~400
  frames/s) es trivial en el M7 a 600 MHz. El cuello NO es el serie.
- La latencia que importa es el **heading del TOP (~4 Hz)** — es el loop del TOP frenado por
  I²C de ToF, no el de la CENTRAL. Otra sesión lo ataca (`feat/top-heading-predict-gateado`).
- Mover el parseo a ISR/RTOS = optimización prematura para este robot (no lo necesita).

# Archivos
- `src/shared/gk_strafe_hold.h` + `test/test_gk_strafe_hold/`
- `src/central/{motors_zircon.h,motors_zircon.cpp,strategy.cpp,central_telemetry_serial.cpp}`
- `platformio.ini` (3 envs de banco del arquero)
- `docs/firmware/FSM-ARQUERO-ESTADOS.md`, este journal.

---

# Continuación 2026-06-18 (tarde) — heading-hold con signo confirmado + control de profundidad (Y)

Los 2 PENDIENTES de arriba se atacaron en banco con Gustavo. Estado:

## 1. Heading-hold por trim de la rueda trasera — ANDA, signo CONFIRMADO

- Env `central_robot2_arquero_pingpong_trim` (= pingpong + `-DCENTRAL_REAR_TRIM`).
- **Camino del signo (importante, para no repetir el error):** primero el robot describía una
  "media luna" y se estabilizaba mirando al arco OPUESTO. Diagnóstico equivocado inicial: creí que
  el signo estaba bien porque "controlaba". El error de razonamiento: **un lazo angular con el signo
  invertido también queda estable, pero en el rumbo a 180° del objetivo** (el antípoda es su
  equilibrio estable). Para discriminar se metió **captura del rumbo inicial** (el objetivo deja de
  ser un cero absoluto y pasa a ser el rumbo con que se lo coloca): como aun así divergía al opuesto,
  quedó probado que el signo estaba invertido. Fix: **`GK_REAR_TRIM_SIGN = -1`**, confirmado en banco.
  Se promovió `-1` al **default de `strategy.cpp`** (vale para todo env de R2; R1 puede diferir y se
  overridea con `-DGK_REAR_TRIM_SIGN=+1` en su env). Con `-1` + captura de rumbo, sostiene la
  orientación con la que se lo coloca (mira al amarillo y se queda).
- **Oscilación de ~3 s** (idas y vueltas) sigue presente: es el KI alto + latencia del heading (~250 ms),
  pendiente de tunear (bajar `GK_STRAFE_KI` 1.0→0.5 + heading-predict en el TOP). NO se tocó todavía.

## 2. Salida de línea — mejorada en banco

- **Margen post-salida** (`GK_LINE_ESCAPE_POST_CLEAR_MS = 400`): el escape ya no corta justo en el
  borde; empuja un toque más DESPUÉS de dejar de ver línea, para despegarse y no re-tocarla. Gustavo:
  "la salida quedó bien".
- **Reacción en el MISMO tick** (antes esperaba un tick): sirve el escape apenas detecta la línea.
- Velocidad de escape a **590** (techo útil; arriba la trasera satura en 150 PWM). Subir el número NO
  da más empuje. El "se sale de la cancha" restante se atribuye a la deriva hacia adelante (ver #3).

## 3. Control de PROFUNDIDAD (Y) — diseño Gustavo, COMPILA, FALTA validar en banco

- Env nuevo `central_robot2_arquero_pingpong_trim_yhold` (= trim + `-DGK_Y_HOLD`).
- **Causa de la deriva hacia adelante:** el heading-hold modula la rueda TRASERA, que es la que mueve
  en Y → corregir rumbo induce deriva en Y. 
- **Solución (idea de Gustavo):** mezclar un `vy` chico al strafe (pequeña diagonal hacia el fondo)
  para mantener Y ≈ objetivo. Usa la **Y de la pose del TOP** (`world_model_get_my_y_mm`),
  **condicionada a confianza ≥ 60** (si la localización no es confiable, `vy=0` y degrada a la
  patrulla del trim). EMA lenta (α=0.05, el tick corre ~100 Hz) + zona muerta 60 mm + `vy ≤ 20 mm/s`
  (límite medido antes de disparar la trasera por su piso). Correcciones lentas para no pelear con el
  heading-hold (más rápido). **Objetivo Y = 500** (validado contra el monitor por Gustavo: cerca del
  centro, un poco atrás = "defensor adelantado").
- **Fuentes de sensor evaluadas:** ToF traseros NO llegan a la CENTRAL (habría que cablear TOP→CENTRAL);
  `cross_track` solo sirve pegado a una línea. Por eso se fue por la Y de la pose del TOP.
- ⚠️ **NO validado en HW**: depende de que la localización XY del TOP dé Y/confianza sanos. Verificar
  primero en el monitor de la CENTRAL.

## 4. Observabilidad — pose en el monitor de la CENTRAL

- `central_telemetry_serial.cpp`: la línea de texto del monitor de la CENTRAL ahora imprime
  `pose x.. y.. conf.. hv..` (lo que la CENTRAL **recibe** del TOP). Antes solo salía en el frame
  binario para la app. Sirve para comparar TOP-calcula vs CENTRAL-recibe y para fijar el target de Y.
  Match-safe (solo emite con un humano en el monitor; se dropea si el buffer está lleno).

## Pendiente
- Validar en banco el Y-hold (que se quede en Y≈500 sin derivar; confirmar el signo de la corrección Y).
- Tunear la oscilación de ~3 s del heading (`GK_STRAFE_KI` + heading-predict en el TOP).
- Probar R1 (puede tener otro `GK_REAR_TRIM_SIGN`).

---

# Continuación 2026-06-18 (noche) — prior-art: corregir valores re-derivados + referenciar la guía canónica

Gustavo marcó (con razón) que varios de estos tuneos YA estaban hechos y documentados. Búsqueda
exhaustiva del repo (workflow de 7 agentes) → doc canónico **`docs/firmware/GUIA-DE-TUNING-CENTRAL.md`**
(+ `MOTION-CONTROL-ACTUAL.md`, `CONTROL-ARQUERO-LATERAL-Y-LATENCIAS.md`). Correcciones a lo re-derivado mal:

- **Escape: 590 → 470 mm/s.** Arriba de ~470 la trasera satura → huida DIAGONAL (el síntoma reportado).
  La distancia se saca con TIEMPO, no velocidad (María: 600→…→1700 ms). Desactivé la escala de trasera
  al arranque (`GK_ESCAPE_REAR_SCALE=1.0`): el "diagonal" era esa misma saturación, no fricción de arranque.
- **Y-hold vy: 30 → 19 mm/s** (tope físico documentado; arriba la trasera se dispara a 107 = patada).
- **Heading-hold: `GK_STRAFE_KP` 2.0 → 3.0** (coincide con el HeadingPID validado), **`GK_STRAFE_KI` 1.0 → 0.5**
  (baja el hunting integral de ~3 s). La latencia ya está al máximo: el BNO se lee a **100 Hz** en
  `top_robot2_pri` (`-DTOP_BNO_FAST` horneado 2026-06-16) + `hpredict` → la oscilación NO era latencia.
- **Kickstart vigente `{145,145,150}`** (el código manda): corregí skill `dinamica-omni-3-ruedas` y
  `FUENTES-DE-VERDAD.md` que aún decían `{130,130,140}`.

Referencias agregadas en lectura obligatoria: skills `dinamica-omni-3-ruedas` y `control-pid-zona-muerta`
+ `FUENTES-DE-VERDAD.md` ahora apuntan a la guía canónica y cargan los lemas (escape ≤470, ±19, kickstart,
burn cap 150, noise 5). Inconsistencia marcada (código manda): `CONTROL-ARQUERO-LATERAL-Y-LATENCIAS.md`
(2026-06-14) dice "BNO 20 Hz / fix=fastbno" → desactualizado, el código ya lee a 100 Hz (`fastbno` redundante).

**Pendiente de banco:** validar KP=3/KI=0.5 (oscilación), escape 470 (que salga recto y despegue), Y-hold ±19.

**2026-06-18 (banco, post-prior-art):** con KP=3/KI=0.5 la oscilación prácticamente desapareció. Quedaba
(a) deriva diagonal hacia adelante + a veces toca pared, (b) no se centraba. Pedido de Gustavo, SIMPLE y
SIN pulsar: **patrulla 200→140 mm/s** (−30%; control total aunque lento, ya probado que el strafe puro lo
banca) y **`GK_ESCAPE_FRONT_SCALE` 1.30→1.45** (más empuje SOLO en las delanteras durante el escape, para
enderezar la huida sin tocar la trasera que ya está al cap a 470; el burn_cap 150 lo limita). Falta validar.
