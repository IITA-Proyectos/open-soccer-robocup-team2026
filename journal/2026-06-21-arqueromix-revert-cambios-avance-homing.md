---
title: "arqueromix — REVERT de mis 2 cambios del avance del homing (pedido Virginia)"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco)"
status: COMPILA · feature 'despeje al arco' del compañero INTACTA
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: revert
---

# arqueromix — revert del avance del homing (impulso dedicado + invertido)

## Por qué

Virginia (banco): mis 2 cambios sobre el avance del homing (`inicio_avanzar`) **rompieron algo que
funcionaba**; pidió **volver a la versión anterior a esos 2 cambios** porque la base estaba andando.
Además avisó que había una **versión nueva** del programa en el repo (la traje con `git pull`).

Commits revertidos:
- `675381e` — avance del homing = impulso momentáneo (primitiva dedicada `avanzar_inicio`, 400→200 ms, PWM 90).
- `f872a8a` — avance del homing: bajar fuerza (90→75) + invertir sentido (`AMIX_INICIO_AVANCE_SIGN=-1`).

## Qué se hizo (cirugía, no a lo bruto)

1. **`git pull --ff-only`** → trajo la versión nueva del compañero (`e6c2853`): feature **"despeje
   DIRIGIDO al arco rival"** (`ff3a55c`) = estado nuevo `ALINEAR_arco_opp` + primitiva `girar()` +
   arcos por ROL (`goal_opp`/`goal_own`, sin color). Esa feature está construida ENCIMA de mis 2 commits.
2. **Verificado** (no de memoria): la feature del arco **no usa NINGÚN símbolo** que yo agregué
   (`avanzar_inicio`/`AMIX_INICIO_AVANCE_PWM`/`AMIX_INICIO_AVANCE_SIGN` aparecían solo en el camino del
   homing). Son ortogonales → revertir lo mío es seguro para la feature.
3. **`git revert --no-commit f872a8a 675381e`** → aplicó SOLO el inverso de mis diffs.
   **Sin conflictos** (exit 0).

## Estado resultante (verificado)

- Mis símbolos: **0** referencias (fuera).
- `inicio_avanzar` vuelve a llamar **`avanzar()`** a PWM 100, `AMIX_T_INICIO_AVANCE = **400** ms`
  (exactamente como antes de mis cambios).
- Feature "despeje al arco" del compañero: **intacta** (26 refs; `girar`/`ALINEAR_arco_opp` presentes).
- `pio run -e central_robot2_arqueromix` → **SUCCESS**.
- 0 marcadores de conflicto en el código/doc.

## Lección (para mí)

No tocar lo que anda con cambios encadenados sin un paso de validación entre medio. Y **re-fetchear el
repo antes de cada cambio** — otras sesiones/compañeros pueden haber subido una versión nueva en paralelo.

## Archivos

- Revertidos a estado pre-mis-cambios: `amix_config.h`, `amix_motors.{h,cpp}`, `amix_fsm.cpp`, `DOCUMENTACION.md` (solo la parte del avance del homing).
- Borrados (journals de los cambios revertidos): `2026-06-21-arqueromix-impulso-avance-inicio-momentaneo.md`, `2026-06-21-arqueromix-avance-inicio-bajar-fuerza-e-invertir.md`.
- NO tocado: la feature del arco ni el binario de competencia (build aislado).
