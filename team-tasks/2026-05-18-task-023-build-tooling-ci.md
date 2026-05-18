---
id: TASK-023
title: "Build/tooling: doc de compilar+flashear, CI, pinear platform, lib_deps, tests del parser cámara"
date_created: 2026-05-18
assigned: [mariaviollaz, enzzo195]
priority: P0
status: pending
estimated_hours: 16
blocks: [que el equipo pueda operar las 4 placas sin Gustavo/Claude]
tags: [build, tooling, ci, firmware, documentacion, 2027]
depends_on: []
---

# TASK-023 — Build / tooling / CI

## Por qué importa (P0/P1)

La auditoría encontró que **un alumno solo hoy NO puede compilar ni flashear**
las 4 placas de competencia (TOP, DOWN, CENTRAL-arquero, CENTRAL-delantero):

- **Cero documentación de build/flasheo** — la única guía son comentarios
  dentro de `platformio.ini`. No hay `software/teensy/README.md`.
- `lib_deps` de **OTOS (DOWN) vacío** y **ToF (TOP) comentado** → esos binarios
  probablemente **no compilan** cuando se activen las libs (P0 latente).
- `platform = teensy` **sin versión pinneada** → toolchain no reproducible
  (otra máquina = otro GCC = build roto).
- **Sin CI** → un commit que rompe el build pasa desapercibido hasta el venue.
- El parser `src/top/cameras.cpp` (donde está el bug P0 del sentinel) **no
  tiene tests**; `test_cameras_fusion` testea otra cosa (`shared/`).
- `default_envs = top` → `pio run` pelado compila solo TOP; falsa sensación de
  "compilé todo". `test/codigo_arquero/a.cpp` es basura en el árbol de tests.

Viola el principio del repo: "tiene que sobrevivir a Virginia / equipo 2027".

## Pasos concretos

1. `software/teensy/README.md`: instalar PlatformIO/Teensyduino, **mapa placa
   física → env**, comando exacto de upload por placa, orden de flasheo,
   cómo verificar que arrancó.
2. Resolver `lib_deps` reales de SparkFun OTOS y ToF (ST VL53L5CX/L7CX);
   compilar `-e down` y `-e top` limpios con las libs.
3. Pinear `platform = teensy@<versión probada>` en los 5 envs.
4. CI mínimo (GitHub Actions): `pio run -e top -e down -e central_robot1
   -e central_robot2` + `pio test -e test_native` en cada push.
5. Tests del parser `CameraParser` (mover/cubrir desde un env de test).
6. Limpiar: quitar `test/codigo_arquero/a.cpp`; aclarar en `platformio.ini`
   que CENTRAL son 2 binarios; decidir `default_envs`.

## Criterio de cierre

- [ ] `software/teensy/README.md` permite a un alumno nuevo compilar+flashear
      las 4 placas sin ayuda (probado con Virginia/Elías).
- [ ] `pio run` de las 4 envs compila limpio con lib_deps resueltas.
- [ ] `platform` pinneado; build reproducible en otra máquina.
- [ ] CI verde en push; tests del parser de cámara existen.

## Plan de prueba en hardware real

1. En una máquina limpia, un alumno sigue el README y flashea las 4 placas.
2. Criterio: las 4 placas arrancan; el alumno no necesitó ayuda externa.
3. Romper a propósito un build → CI lo detecta antes del merge.

## Notas / decisiones

_(completar al ejecutar — registrar versión de platform pinneada y libs)_

## Cambios de estado

- 2026-05-18: creada por Claude tras la evaluación crítica del firmware, a
  pedido de Gustavo Viollaz.
