# TASK-119 — Validar en banco el delantero REACTIVO estilo "Edge" (rodeo) del centralmix

- **Placa:** CENTRAL (R1, delantero) + depende del TOP (cámaras: pelota + arco rival) y DOWN (línea + OTOS).
- **Asignado:** equipo (banco) — Elías
- **Prioridad:** P1 (apunta a que el delantero se posicione RÁPIDO y llegue a una pelota en movimiento;
  el mix 2025 actual es lento. NO bloquea Incheon: el mix 2025 sigue como fallback sin el flag).
- **Estado:** abierta
- **Build:** `pio run -e central_robot1_mix_edge -t upload`
  (⚠️ NO compilado por Claude en Teensy — sin toolchain en la sesión. El núcleo PURO `mix_edge` SÍ pasa
  11/11 host. El equipo confirma que el env Teensy compila ANTES de banco.)

## Por qué
El delantero "mix" (port 2025) es lento: estados secuenciales (apuntar→avanzar→orbitar) + órbita a
24/72 PWM + no anticipa. El **Team Edge** (campeón mundial Lightweight 2024) se pone detrás de la pelota
con UNA fórmula reactiva a full velocidad. Se portó ese rodeo (curva de amplificación de ángulo + giro
que mira al arco + empuje por inercia). Diseño y derivación de la cinemática: journal
`2026-06-23-centralmix-rodeo-estilo-edge.md` y `docs/firmware/CINEMATICA-OMNI-R1-DERIVACION.md`.

## Debug USB (115200, USB de la CENTRAL)
`pio device monitor -b 115200`. Campo nuevo: `EDGE=` (estado: KICKOFF/BUSCAR/RODEAR/EMPUJAR/RETROCEDER/
LINEA_*). Mirar también `ang` (ángulo pelota), `d` (distancia cm), `goalOpp vis/ang`, `hdg/herr`.

## Cómo validar (en orden — UNA perilla por vez; titular de menor a mayor)
1. **SENTIDO de `mix_mover_vector` (robot LEVANTADO, ruedas al aire).** Es lo PRIMERO: todo el rodeo
   depende de esto. Forzar comandos conocidos (o ver en RODEAR con la pelota quieta al frente):
   - pelota al FRENTE (`ang≈0`) → debe ir al FRENTE (`go_ang≈0`): las ruedas deben empujar como `avanzar`.
   - pelota a la DERECHA (`ang≈+60`) → el robot debe salir hacia adelante-derecha CURVANDO (no derecho a
     la pelota). Si sale al revés/lateral raro → anotar; el sentido está anclado a `avanzar` pero hay que
     confirmarlo con el pinout real.
2. **SIGNO de `MIX_EDGE_FACE_KP` (giro al arco) — robot LEVANTADO.** Poné el arco rival a la vista a un
   costado (`goalOpp vis=1`, `ang≠0`). El robot debe **girar ACERCANDO el frente al arco** (que `goalOpp
   ang` tienda a 0). Si gira ALEJÁNDOSE → poné `MIX_EDGE_FACE_KP = -1.5` en `mix_config.h`. (Es el mismo
   tipo de perilla que la patada del OTOS: confirmar SIGNO antes que magnitud.)
3. **RODEO en cancha (pelota QUIETA), velocidad media.** Pelota al costado del robot. Debe **rodearla
   por detrás** (no chocarla de frente) y terminar detrás mirando al arco. Cronometrar pelota-al-costado
   → detrás-y-encarado: objetivo MUCHO menor que el centrar 2025 (4–6 s). Si orbita "por afuera" y la
   pelota se escapa → subir `MIX_EDGE_K_SIDE` (rodea más cerrado). Si la "abraza" demasiado y la toca al
   pasar → bajar `MIX_EDGE_K_SIDE`.
4. **EMPUJE (gol por inercia).** Al quedar detrás + pelota al frente + arco alineado debe pasar a
   `EMPUJAR` (USB: `EDGE=EMPUJAR`) y empujar RECTO al arco (reusa `avanzar_patear` con heading-hold del
   OTOS — TASK-117). Ajustar `MIX_EDGE_PUSH_DIST_CM` (cm REALES: medir a qué `d` la pelota está "pegada")
   y `MIX_EDGE_PUSH_ALIGN_DEG`. Si empuja antes de estar detrás → bajar dist/align. Si nunca empuja →
   subir, o revisar que `goalOpp` se vea (si se ve y está desalineado, no empuja a propósito).
5. **VELOCIDAD.** Subir `MIX_EDGE_SPEED` 200→220→240 hasta el máximo que no haga brownout del regulador
   ni pierda la pelota por inercia. Vigilar temperatura de motores (rodeo sostenido).
6. **PELOTA EN MOVIMIENTO.** Tirar la pelota cruzada y ver si el rodeo la **alcanza** (sin el feedforward
   de velocidad de Edge —no cableado— no la va a anticipar, pero el ser reactivo + full velocidad debería
   mejorar mucho vs el mix 2025). Anotar si "llega a hacer gol" con pelota en movimiento (la pregunta
   original). Si NO alcanza → candidato a cablear la velocidad de pelota de DOWN a `g_io` (mejora futura).
7. **LÍNEA.** Durante el rodeo a full, ¿el escape de línea (`retroceder1/2/3`, sectores ±60°) frena a
   tiempo sin salir de cancha? El rodeo es más rápido que el mix 2025 → puede necesitar re-tuneo de los
   sectores o un margen. Si pisa línea seguido → avisar y dibujamos la geometría de los sensores de luz
   para afinar.
8. **A-B / FALLBACK.** Flashear `central_robot1_mix_bno` (mismo robot, FSM 2025) y comparar tiempo de
   posicionamiento y goles. Es la prueba de que el rodeo MEJORA (o no) sobre lo de hoy.
9. **NO-REGRESIÓN del resto:** kickoff-medialuna, árbitro RCJ (no moverse antes del GO), patada recta
   (TASK-117) deben seguir igual (se reusan tal cual).

## Criterio de cierre (humano)
- El delantero **se pone detrás de la pelota más rápido** que el mix 2025 (medido, A-B) y **empuja al
  arco rival** sin meterla al propio, **5/5** repeticiones con pelota quieta. Sentido de `mover_vector`
  y signo de `FACE_KP` confirmados y dejados como default. Sin brownout. Reporte de si **alcanza la
  pelota en movimiento** (sí/no — define si se cablea la velocidad de pelota).

## Escape / rollback
Cualquier env de competencia, o `central_robot1_mix_bno` (mismo robot, FSM 2025 — el flag `-DMIX_ATTACK_EDGE`
NO está → vuelve a lo de hoy). El rodeo vive 100% detrás del flag: no toca el mix 2025.

## Mejora futura (2027 / si hace falta — NO ahora)
- **Feedforward de velocidad de pelota** (lo mejor de Edge): cablear `Velocity2D` de DOWN → `g_io` →
  `EdgeIn` y sumar al `go_ang` cuando la pelota se mueve. Anticiparía la pelota en movimiento.
- **Curva por interpolación suave** (Edge tenía un cúbico) en vez de 3 tramos lineales, si el rodeo se
  siente "quebrado" en los breakpoints.

## Relación
- TASK-113/115/116/117/118 (misma rama centralmix). Reusa el empuje (TASK-117) y NO toca la jugada
  pelota-atrás (TASK-118) ni el kickoff (TASK-116).
