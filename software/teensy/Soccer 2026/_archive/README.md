# `_archive/` — código pure-only NO integrado al binario

> **No tocar antes de Incheon 2026.** Reactivar solo en revisión post-mundial.

## Por qué está acá

Esta carpeta es **deliberadamente** invisible al build de PlatformIO (no está en
`build_src_filter` de ningún env). Contiene módulos C++ puros + sus tests que
fueron creados por sesiones Claude descoordinadas (mayo 2026), tienen tests
host-native verdes, pero **nunca fueron conectados a `main_central.cpp`** ni a
ningún binario que corra en el robot.

Se archivan en lugar de borrarse para:
- Preservar el trabajo (regla del equipo: no se borra patrimonio).
- Dejarlos disponibles como punto de partida si en post-Incheon se decide
  refactor (estrategia 2027).

## Inventario

| Módulo | Origen | Tests | Por qué se archivó |
|---|---|---|---|
| `src/shared/strategy_core.{h,cpp}` | Sesión Claude 2026-05-18 | `_archive/test/test_central_strategy/` | FSM unificada arquero+jugador alternativa. `main_central.cpp` llama a `strategy.cpp` (la viva), nunca a esto. Duplica esfuerzo con `src/shared/strategy_transitions.{h,cpp}` (que sí caracteriza fielmente a la FSM viva). |
| `src/shared/play_decision.{h,cpp}` | Sesión Claude 2026-05-18 | `_archive/test/test_central_play/` | Lógica pura de decisión táctica (`circular/interceptar/desviar`). Nadie la llama. Pensada para que `strategy_core` la use — pero `strategy_core` tampoco está integrado. |
| `src/shared/field_safety.{h,cpp}` | Sesión Claude 2026-05-18 | `_archive/test/test_central_safety/` | Escape de borde como módulo puro. La lógica equivalente vive en `strategy.cpp` (estado `LINE_AVOID`) + `main_central.cpp` (EMERGENCY_LINE bypass con `motors_brake`). |

## Cómo reactivarlos (post-Incheon)

1. Decidir explícitamente cuál es la fuente única de la FSM (¿la `strategy.cpp`
   actual, refactorizada para usar estos módulos puros? ¿un rediseño nuevo?).
2. Mover los archivos de vuelta a `src/shared/` con `git mv`.
3. Conectar desde `strategy.cpp` (o sucesor) — el patrón está en el journal
   `2026-05-15-analisis-firmware-y-fsm-testeable.md` ("Conectar
   strategy.cpp → strategy_transitions" con preservación Moore).
4. Plan de prueba en hardware real (regla 1 CLAUDE.md).

Ver `docs/FUENTES-DE-VERDAD.md` para el contexto completo de qué es canónico.

## Fechas

- 2026-05-19: archivado por Claude bajo decisión de Gustavo (cleanup
  quirúrgico Nivel 2 del plan de salida del "coach-fábrica").
