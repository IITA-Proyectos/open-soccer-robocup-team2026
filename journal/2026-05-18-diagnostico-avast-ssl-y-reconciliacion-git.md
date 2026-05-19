---
title: "2026-05-18 — Diagnóstico causa raíz Avast/SSL en PlatformIO + reconciliación de divergencia git"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [infra, platformio, ssl, avast, git, proceso, leccion]
robot: ambos
area: infraestructura
tipo: diagnostico+proceso
related-tasks: [TASK-025, TASK-023]
---

# Diagnóstico Avast/SSL en PlatformIO + reconciliación de divergencia git

## Contexto

Sesión larga de análisis de firmware. Dos cosas a documentar honestamente:
un diagnóstico de infra valioso y un error de proceso con git que se corrigió.

## 1. Diagnóstico: por qué fallaba PlatformIO (causa raíz real)

Durante toda la sesión `pio test` falló con `HTTPClientError`. Primero lo
atribuí a "no hay red" — **mal, conclusión sin verificar**. El usuario lo
cuestionó ("¿qué es eso de que no hay red? si hay red"). Diagnóstico real:

- `git push`/`WebFetch` funcionaron toda la sesión → hay red.
- `curl api.registry.platformio.org` → error 35 `CRYPT_E_NO_REVOCATION_CHECK`.
- `curl --ssl-no-revoke ...` → **HTTP 200**.
- Env var `SSLKEYLOGFILE=\\.\aswMonFltProxy\...` → driver `aswMonFlt` = **Avast**.
- **Causa raíz**: Avast hace MITM/SSL-scanning, reemplaza certificados; los
  suyos no tienen revocación comprobable → schannel (Windows TLS, que
  PlatformIO 6.1.x usa vía `truststore`) corta. `git` usa OpenSSL → inmune.

El doc del equipo `SETUP-ENTORNO-BUILD-WINDOWS.md` (TASK-023) tenía el mismo
modelo mental erróneo ("sin red") y daba como fix pre-cachear offline.
Correcto pero incompleto: apenas se necesite una lib NO cacheada (activar
OTOS/ToF de TASK-023) el cache no alcanza. **Fix superior**: excepción de
`*platformio.org*` en Avast → PlatformIO funciona CON red. Documentado en
**TASK-025** + callout agregado al doc del equipo (sin pisarlo).

El usuario eligió la opción de excepción Avast. Pendiente que la aplique y
avise para correr la suite host-native de verdad.

## 2. Error de proceso: divergencia git (39 commits)

Esta sesión trabajó sobre una base **desactualizada**: no se hizo `git pull`
al empezar. Mientras tanto, sesiones del 17–18 (strategy-core TDD,
WorldSnapshot v2 +ball_vx/vy, ball_trajectory, corrección firmware C6,
TASK-014..024) avanzaron **39 commits** en el remoto.

- Los commits de esta sesión `c7affd5` (FSM testeable) y `9246f5c` (specs
  sync) **sí** se habían pusheado a tiempo → están en el remoto.
- El commit `4c09e99` (diagnóstico Avast, TASK-013) quedó local y su
  `TASK-013` **colisionaba** con el `TASK-013` del equipo (BOM placa TOP).
- Al `git pull --rebase` saltó conflicto en `team-tasks/README.md`.

**Manejo (sin nada destructivo sin autorización):**
1. `git rebase --abort` (estado seguro).
2. `git branch wip-avast-ssl-diagnostic 4c09e99` (respaldo del trabajo).
3. Verificado que el equipo NO había diagnosticado Avast (solo workaround
   offline) → el aporte vale, no es redundante.
4. Con autorización explícita del usuario: `git reset --hard origin/main`
   (mi commit salvo en el branch backup; nada perdido).
5. Re-aporte limpio sobre la base nueva: **TASK-025** (renumerada desde 013
   colisionante) + complemento al doc del equipo + este journal.

El branch `wip-avast-ssl-diagnostic` queda como respaldo hasta confirmar que
el re-aporte cubre todo; después se puede borrar.

## Lección para el frame (no repetir)

1. **"No hay red" es una conclusión, no una observación.** Verificar la pila
   (qué cliente, qué TLS, error exacto) ANTES de afirmar la causa. Aplicado
   tarde — el usuario tuvo que empujar el diagnóstico correcto.
2. **`git pull` al EMPEZAR toda sesión de trabajo en este repo.** El repo
   tiene múltiples sesiones Claude + el equipo trabajando en paralelo;
   asumir que el local está al día genera divergencias y colisiones de
   numeración de TASK. Incorporar como primer paso de cualquier sesión de
   código acá.

## Estado al cierre

- `main` local = `origin/main` (582534b) + este re-aporte limpio encima.
- Trabajo del equipo (39 commits) intacto.
- Diagnóstico Avast preservado y re-aportado como TASK-025.
- Pendiente usuario: aplicar excepción Avast → correr suite host-native
  (las suites `strategy_transitions` (35), `cameras_fusion` (16),
  `behind_ball` (16) de mis commits previos + las del equipo) y registrar
  resultado real (hoy solo verificadas por lectura cruzada).
