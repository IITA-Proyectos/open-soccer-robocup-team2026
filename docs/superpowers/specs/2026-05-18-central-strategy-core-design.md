---
title: "Diseño — CENTRAL strategy core (Plan 1): estrategia + decisión, pura y host-testeada"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [firmware, central-board, estrategia, fsm, decision, diseno]
robot: ambos
area: control
tipo: decision
related: [docs/firmware/CONTRATO-DATOS-CENTRAL.md, docs/firmware/CONTRATO-DATOS-TOP.md, docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md, docs/superpowers/plans/2026-05-18-down-line-sensing-core.md]
---

# CENTRAL strategy core — diseño aprobado

> Aprobado por Gustavo (2026-05-18). Mismo patrón que DOWN: lógica pura
> host-testeable en `src/shared/` como **única fuente de verdad**, glue HW en
> `src/central/`. Este es **Plan 1** (núcleo de estrategia/decisión). La
> integración HW real (motores Zircon, tuning PID en robot) es Plan 2.

## 1. Rol de CENTRAL

CENTRAL es el **master/decisor** (NO motor-server): consume `WORLD_SNAPSHOT`
de TOP (Serial) y `LINE_URGENT`/`LineStatusV2` de DOWN (bus de emergencia),
corre la estrategia/FSM/decisión y produce un comando de movimiento+kicker que
los PID/cinemática (ya existentes y testeados: `pids`, `kinematics`) ejecutan
sobre los motores del Zircon **localmente**. Corrige la doc obsoleta de
`config_central.h` que aún describe el modelo viejo "motor server".

## 2. Roles de juego (dipswitch)

- **Robot 1 — ARQUERO** (fijo): patrulla su arco; prioridad alta a interferir
  trayectorias de pelota hacia nuestro arco.
- **Robot 2 — JUGADOR DE CAMPO**: modo interno **ATAQUE / DEFENSA** que
  conmuta según posición+trayectoria de pelota.
  - ATAQUE: RUSH a la pelota → conducir/encarar → patear al arco rival.
  - DEFENSA: replegar a zona defensiva → interceptar → despejar/desviar al
    arco rival.

El rol se lee de un dipswitch (hoy `PIN_ROLE_DIPSWITCH` no se lee — ver
TASK-024; el glue HW lo cableará en Plan 2; el `strategy_core` recibe el rol
como parámetro y es testeable con ambos roles).

## 3. Módulos puros (src/shared/) — única fuente de verdad

Cada uno: una responsabilidad, interfaz clara, test host-native. Reemplazan la
réplica `strategy_transitions.cpp` (que la auditoría marcó como falsa
seguridad: testeaba una copia, no el código que corre).

| Módulo | Responsabilidad |
|--------|-----------------|
| `ball_trajectory` | Desde `WORLD_SNAPSHOT.ball_vx/vy` (+ pos): clasifica destino { ARCO_RIVAL, ARCO_PROPIO, OTRO }, magnitud de velocidad, "¿a mi alcance?" (umbral configurable). |
| `play_decision` | La regla pedida: si pelota a alcance y en movimiento delante del robot → ARCO_RIVAL ⇒ dejar circular; ARCO_PROPIO ⇒ interceptar trayectoria; OTRO ⇒ desviar hacia arco rival. |
| `strategy_core` | FSM unificada. Estados ARQUERO (patrol/intercept/clear + line-avoid) y JUGADOR (rush/seek/drive/kick/defend). Recibe rol, world, salida de play_decision. Devuelve **intención** de alto nivel. |
| `field_safety` | Reflejo de borde desde `LineStatusV2` (escape_angle, IMMINENT_EXIT, data_valid). **Máxima prioridad.** |
| `motion_target` | Traduce intención → objetivo de movimiento `{vx_mm_s, vy_mm_s, omega_centideg_s, kicker}` para los PID/cinemática existentes. |
| `central_decide` | Orquestador: aplica **precedencia** y produce el comando final. |

## 4. Precedencia (aprobada)

```
1. field_safety   — si borde inminente / data_valid line: ESCAPAR (escape_angle
                     de DOWN). Preempta TODO. "En todo momento no salir."
2. play_decision  — si pelota a alcance y en movimiento: circular/interceptar/desviar.
3. strategy_core  — FSM del rol (buscar pelota, patrullar, etc.).
```
Arranque rápido: al `START` del árbitro, JUGADOR en estado **RUSH** va directo
a la pelota a velocidad máxima si la ve (jugada inicial rápida).

Caso degradado (coherente con la safety lattice del diseño de comunicaciones):
si `LineStatusV2.data_valid==0` → modo conservador (no "ciego"); si
`WORLD_SNAPSHOT` stale → `motors_stop` (fail-safe existente, se conserva).

## 5. Contrato — extensión de `WORLD_SNAPSHOT`

Se agregan a `WorldSnapshot` (types.h): `int16_t ball_vx_mm_s;` y
`int16_t ball_vy_mm_s;` (velocidad de pelota en marco robot, mm/s; sentinela
N/A si no estimable). Tamaño nuevo cabe en `PROTO_MAX_PAYLOAD=32`
(actual ~23 B + 4 B = 27 B). Se agrega/incrementa el versionado de schema y
`static_assert(sizeof(WorldSnapshot)==N)`. Se actualizan
`CONTRATO-DATOS-CENTRAL.md` y `CONTRATO-DATOS-TOP.md`.

**Quién llena qué:** CENTRAL **consume** `ball_vx/vy` ya (host-testeado con
snapshots sintéticos). TOP **lo llena** en su propio plan (estimación en la
fusión de cámaras) — diferido, igual que OTOS fue diferido en DOWN. Hasta
entonces TOP enviará `ball_vx/vy = 0` o sentinela; `play_decision` degrada
elegante si la velocidad no es confiable (cae a comportamiento de solo-posición).

## 6. Tests (TDD host-native, sin hardware)

Suites nuevas en `test/`: `test_ball_trajectory`, `test_play_decision`
(los 3 casos), `test_strategy_core` (estados/transiciones arquero + jugador
ataque/defensa, incluye RUSH inicial), `test_field_safety` (preempción borde),
`test_central_decide` (precedencia integrada §4). Reusa `test_kinematics` y
`test_pids` existentes. `static_assert` + test de tamaño para el contrato.

## 7. Deferred honesto (no oculto)

- TOP llenando `ball_vx/vy` real → plan de TOP.
- Tuning de PID y mapeo de motores en Zircon, lectura real del dipswitch de
  rol (TASK-024), wiring UART (TASK-008) → Plan 2 CENTRAL (HW).
- Pose absoluta del robot sigue dependiendo de OTOS/cámaras (DOWN Plan 2/TOP):
  la estrategia se diseña para degradar si la pose no es confiable
  (decisiones relativas a pelota/arco visibles, no a pose global).

## 8. Decomposición / próximos planes

- **Plan 1 (este):** núcleo puro de estrategia/decisión + extensión de
  contrato + tests. Compila para CENTRAL (env central_robot1/2) sin hardware.
- **Plan 2:** integración HW (motores Zircon, dipswitch rol, tuning PID en
  robot, bring-up).

## 9. Quién decidió y cuándo

Diseño propuesto por Claude (Anthropic, Opus 4.7 1M), aprobado por Gustavo
Viollaz (@gviollaz) el 2026-05-18 vía brainstorming (3 decisiones clave: TOP
envía velocidad de pelota; arquero fijo + jugador de campo dinámico; unificar
en un módulo puro host-testeado único). Cada tarea se cierra con su prueba.
