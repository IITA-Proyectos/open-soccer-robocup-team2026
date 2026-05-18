---
title: "Estado del firmware del robot — evaluación crítica independiente (DOWN / CENTRAL / TOP / cámaras)"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [firmware, electronica, vision, analisis, resultado, ambos]
robot: ambos
area: control
tipo: resultado
related-tasks: [TASK-006, TASK-008, TASK-012, TASK-014, TASK-022, TASK-023, TASK-024]
---

# Estado del firmware del robot — evaluación crítica

> 4 auditorías **independientes** sobre el código real (no sobre docs): DOWN,
> CENTRAL, TOP, y cámaras+build. Sin endulzar. Excluye el armado físico
> (estimado terminado esta semana). Lenguaje accesible para el equipo.

## Resumen ejecutivo (leer esto)

**Las preguntas que hiciste, respondidas sin vueltas:**

- **¿Hay 3 programas completos candidatos a funcionar (DOWN, CENTRAL, TOP)?**
  **NO.** Hay **3 esqueletos bien estructurados que compilan**, pero **ninguno
  es "candidato a funcionar"**: cada uno tiene agujeros P0 que lo dejan
  no-funcional en una cancha real.
- **¿Hay 2 programas de cámara, uno por cámara?** **NO.** Hay **1 solo script
  genérico de demo/calibración**, con 2 bugs que rompen el protocolo y mala
  práctica de calibración. No es un sistema de 2 cámaras.

**Veredicto global:** el firmware es un **andamiaje honesto y prolijo, no un
robot que funcione**. Los ~95 tests verdes dan **falsa seguridad**: cubren solo
lógica pura de `src/shared/`, y los tests de la FSM prueban una **réplica**
(`strategy_transitions.cpp`), no el código que corre (`strategy.cpp`). Para
tener un robot que juegue un fútbol primitivo pero **legal**, falta trabajo de
banco con hardware: estimación honesta **~2–3 semanas** de foco. Incheon es
jun 30–jul 6: alcanza si se atacan los P0 ya, en orden, con la meta declarada
"aprendizaje, no podio".

## Estado por programa

| Programa | ¿Compila? | ¿Candidato a funcionar? | Lo que está REAL | El agujero que lo mata |
|---|---|---|---|---|
| **DOWN** (línea + odometría) | Sí (modo default) | **Parcial** | Anillo de línea (HW read + filtros, testeado), envío `LINE_URGENT` a CENTRAL | **OTOS = stub total** (pose siempre 0); línea **sin calibrar** y solo 8 de 32 sensores |
| **CENTRAL** (motores + decisión) | Sí (probable) | **Parcial** | Motores, cinemática inversa, 2 PIDs, FSM delantero/arquero conectada al loop | **UARTs cruzados** TOP↔CENTRAL↔DOWN → snapshot nunca llega → `motors_stop()` permanente |
| **TOP** (cerebro sensorial) | Sí (probable) | **Parcial** | Pipeline de cámaras (fusión, testeado), IMU dual real (heading OK) | Pose absoluta = **0 hardcodeado**; ToF = **stub**; distancias cámara **sin calibrar** |
| **Cámaras** OpenMV | (script corre) | **NO — es un demo** | `find_blobs` corre; thresholds LAB existen | **1 solo script** (no 2); sentinel "no veo pelota" **no concuerda** con el parser del TOP; auto-WB/gain ON |

## Los P0 que bloquean un robot operativo (convergentes entre auditorías)

1. **UARTs cruzados en código** (CENTRAL/TOP/DOWN). TOP manda el snapshot por
   `Serial2`; CENTRAL lo espera en `Serial1`. DOWN manda línea por `Serial1`;
   CENTRAL la espera en `Serial2`. Si el cableado físico no cruza, CENTRAL
   nunca recibe mundo fresco → motores parados siempre → **robot inerte**. Es
   el bloqueante #1. **Hay que MEDIR el wiring con osciloscopio antes de tocar
   código** (ligado a TASK-008 / `config_top.h:40-42` "NO CONFIRMADO").
2. **Cámara: sentinel "no detectado" roto.** Cuando no ve la pelota, la OpenMV
   manda `(0,100)`; el parser del TOP lo interpreta como **pelota visible en el
   origen** → el robot persigue una **pelota fantasma permanente**. Además la
   cámara **crashea** si una coordenada sale negativa (`bytearray`).
3. **OTOS = stub total** (DOWN): toda la odometría es 0; y TOP **ni la
   consume**; y la pose en el snapshot está hardcodeada a 0. **El robot no
   tiene mapa** — cero posición absoluta en todo el sistema.
4. **ToF = stub total** (TOP): sin evasión de obstáculos/robots. `min_obstacle`
   casi siempre vacío.
5. **Escala de distancia de cámara sin calibrar** (`CAMERA_UNIT_TO_MM=10.0`
   placeholder): las decisiones de approach/kick de CENTRAL usan distancias que
   no corresponden a la realidad.
6. **No arranca sin la placa COMM y sin fallback manual:** sin COMM mandando
   START, la FSM queda en WAIT_START para siempre (COMM = TASK-006, pendiente).
7. **Polaridad de arco hardcodeada** (`yellow=opp`, `strategy_set_attack_color`
   nunca se llama): según el sorteo de lado, el delantero **ataca su propio
   arco** ~50% de los partidos.
8. **Rol del robot nunca se lee** (`PIN_ROLE_DIPSWITCH` declarado, sin
   `digitalRead`): no se distingue arquero de delantero al boot.
9. **Cámara con auto-WB y auto-gain ENCENDIDOS:** los thresholds LAB se
   invalidan con la luz de Incheon (≠ lab IITA Salta). Falla #1 de novatos RCJ.
10. **Build/tooling:** `lib_deps` de OTOS (DOWN) y ToF (TOP) comentados/vacíos
    → esos binarios probablemente **no compilan** cuando se activen las libs;
    **sin doc de build/flasheo**, **sin CI**, `platform = teensy` sin pinear.
    Un alumno solo hoy **no puede** compilar/flashear las 4 placas.

## Lo que SÍ está bien (crédito honesto)

- `proto.h`: framing con CRC16 + resync, **sólido y testeado**.
- Pipeline de fusión de cámaras (cam-cam): **completo y testeado** (16 tests).
- IMU dual real: heading funciona, no bloquea el loop (solo el `setup()`).
- CENTRAL: motores + cinemática inversa + 2 PIDs + FSM delantero/arquero con
  estados sustantivos, **conectada al loop** (no es placeholder), aunque
  táctica primitiva y sin tunear.
- Anillo de línea: lectura HW + filtros reales y unit-testeados (lógica pura).
- Aislamiento de builds PlatformIO (`build_src_filter` por placa): bien hecho.

## Falsa seguridad a desarmar con el equipo

- **Tests verdes ≠ robot funciona.** Cubren `src/shared/` (lógica pura). El
  hardware (UART real, motores, IMU, OTOS, ToF, parser de cámara) **no se
  testea**. El bug P0 del sentinel de cámara está **fuera** de los tests.
- **Los 35 tests de FSM prueban `strategy_transitions.cpp`, que NO es lo que
  corre.** Si diverge de `strategy.cpp`, los tests pasan y el robot falla.
- `config_central.h` aún describe el modelo viejo "motor server TOP-master":
  quien lo lea para entender el sistema entiende lo contrario de lo que pasa.

## Camino crítico realista (excluye armado físico)

**Fase 1 — Destrabar (P0 sistema, ~3–5 días):** medir y alinear wiring UART
(TASK-008/014), fallback de START sin COMM o COMM operativa (TASK-006), arreglar
sentinel + crash de cámara (TASK-022), leer rol + polaridad de arco (TASK-024).
→ Resultado: un robot que recibe mundo, arranca, y no persigue fantasmas ni
auto-golea.

**Fase 2 — Percepción usable (P0/P1, ~4–6 días):** calibrar `CAMERA_UNIT_TO_MM`
+ thresholds LAB + exposición fija en cancha (TASK-022); OTOS real **o** decidir
explícitamente "sin pose absoluta" (TASK-012); ToF real **o** "sin evasión"
documentado; loop no-bloqueante (TASK-014).

**Fase 3 — Jugar (P1, ~3–5 días):** tunear PIDs en hardware, calibrar magic
numbers de estrategia y línea, validar LINE_AVOID (que no retroceda HACIA la
línea), modo degradado real si cae DOWN.

**Transversal (P0/P1, ~2 días):** doc de build/flasheo + CI + pinear platform +
resolver lib_deps (TASK-023). Sin esto el repo no sobrevive a quien lo armó.

## Tareas

Cubierto por tasks existentes: TASK-006 (COMM), TASK-008 (wiring UART),
TASK-012 (libs OTOS/ToF), TASK-014 (loop no-bloqueante). Nuevas creadas:

- **TASK-022 (P0)** — Cámara operativa: sentinel, crash bytearray, exposición/
  WB fija, calibración mm + LAB, 1 script por cámara.
- **TASK-023 (P0/P1)** — Build/tooling: doc de compilar+flashear, CI, pinear
  `platform`, resolver `lib_deps`, tests del parser de cámara.
- **TASK-024 (P0)** — Operabilidad de arranque: leer rol (dipswitch), polaridad
  de arco por árbitro, fallback de START sin COMM (verificar reglas RCJ).

## Conclusión

El equipo hizo un trabajo de **arquitectura** sólido y honesto, pero **el robot
no funciona todavía** y no está "casi listo". Lo más peligroso sería creer que
sí por los tests verdes. Con los P0 atacados en orden (destrabar → percepción →
jugar) hay tiempo para un robot que juegue un fútbol básico y legal en Incheon,
coherente con la estrategia "inversión en aprendizaje". Sin atacarlos, el robot
queda inerte (UART), o persigue fantasmas (cámara), o se autogolea (polaridad),
o sale de cancha (línea sin calibrar).

## Fuentes

Auditorías independientes sobre `software/teensy/Soccer 2026/src/{down,central,top,shared}/*`,
`software/vision/`, `platformio.ini`, `test/`. Citas archivo:línea en los
informes de cada auditoría (resumidas aquí). Relacionado:
`docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md`,
`journal/2026-05-18-estado-firmware-robot.md`.
