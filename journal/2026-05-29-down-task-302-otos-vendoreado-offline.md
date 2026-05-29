---
title: "DOWN: TASK-302 — OTOS + SparkFun_Toolkit vendoreadas, [env:down]/[env:diag_down] compilan offline"
date: 2026-05-29
author: "Claude (Anthropic - Claude Opus 4.7)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7, Anthropic)"
status: final
tags: [down, infra, build, platformio, vendoring, otos, avast, offline]
robot: ambos
area: control
tipo: resultado
---

# DOWN — TASK-302: OTOS vendoreado, build offline restaurado

## Contexto

El journal previo de la misma sesión
(`journal/2026-05-29-down-robustez-audit-p0.2-p1.5-p1.6.md`) dejó como próximo
paso **TASK-302**: vendorear la lib OTOS. El problema de fondo: al activar OTOS
el 2026-05-24 (TASK-012) se agregó `lib_deps = sparkfun/...` a `[env:down]` y
`[env:diag_down]`, dejando a OTOS como la **única lib de firmware sin vendorear**.
Eso **rompió el invariante** "las 4 placas compilan offline" (documentado en
`lib/README.md` desde 2026-05-19): en una máquina con Avast interceptando TLS
(`CRYPT_E_NO_REVOCATION_CHECK`, TASK-025), PlatformIO no puede bajar OTOS del
registry y el build de DOWN se cae **antes de compilar nada**.

El workaround del journal previo (copiar OTOS desde el `.pio/` del repo principal)
era local y no sobrevivía a `pio run -t clean`. Gustavo pidió avanzar TASK-302.

## Qué se hizo

- **Vendoreadas dos libs en `lib/`** (mismo patrón que BNO055/VL53):
  - `lib/SparkFun_Qwiic_OTOS/` (49 KB)
  - `lib/SparkFun_Toolkit/` (110 KB, dependencia transitiva de OTOS)
  - Podadas: solo `src/` + `library.properties` + LICENSE. Sin `examples/`,
    `.github/`, `keywords.txt`, `.piopm`. **OTOS no trae firmware blob** (a
    diferencia de los VL53), por eso pesan poco.
- **`platformio.ini`**: quitado el bloque `lib_deps` de registry de `[env:down]`
  y `[env:diag_down]`, reemplazado por comentario "VENDOREADAS en lib/" igual que
  los envs `top`/`central_*`. La LDF de PIO detecta las libs de `lib/` sola.
- **Coordinación multi-agente (CLAUDE.md regla 5)**: `platformio.ini` es archivo
  compartido (lo toca también el agente TOP). `git fetch` + `git log origin/main
  -- platformio.ini`: última edición ajena `7db45e2` (HAL refactor, solo bloques
  `top`/`diag_top`) ya en mi base y confirmada **ancestro de HEAD**. Mis ediciones
  tocan solo los bloques `down`/`diag_down` → sin conflicto.

## Qué se midió/observó

- **Borrado `.pio/libdeps/down` entero** (incluido el workaround temporal) para
  forzar que `lib/` sea la única fuente posible.
- `pio run -t clean -e down && pio run -e down` → **SUCCESS**, FLASH code:33416 B
  — **idéntico** al build que usaba el workaround → es el mismo binario, la lib
  vendoreada es la correcta.
- `pio run -t clean -e diag_down && pio run -e diag_down` → **SUCCESS**,
  FLASH code:21960 B.
- **Post-build: `.pio/libdeps/down` NO se recreó** → PlatformIO no tocó el
  registry; toda la resolución fue por LDF desde `lib/`.
- Máquina **con Avast** (la firma `CRYPT_E_NO_REVOCATION_CHECK` se confirmó en la
  sesión previa probando el egress HTTPS) → es offline real, no "offline porque
  ya estaba cacheado".

## Conclusión

**Invariante restaurado**: las 4 placas (`top`, `down`, `central_robot1`,
`central_robot2`) + los `diag_*` vuelven a compilar 100% offline. TASK-302
**cerrada como build-verificada**.

Distinción importante con la regla 1 de CLAUDE.md ("Claude no cierra tasks de
hardware"): **TASK-302 no es una task de hardware**. Su criterio de cierre es
enteramente verificable en software (libs presentes, `lib_deps` quitado, compila
offline, README actualizado) y se verificó en esta misma máquina con Avast. No
afirma nada sobre el comportamiento del robot. Por eso el agente la puede cerrar
sin pasar por el equipo humano.

## Próximos pasos

- **TASK-301** (equipo, P1) sigue pendiente y es lo único que queda de la cadena
  de robustez DOWN: validar en banco P0.2 (power-cycle calib), P1.5 (all-white con
  luz real) y P1.6 (frames_dropped bajo carga). Depende de TASK-031 (UART real).
- Nada más bloquea el build de DOWN: cualquier integrante puede clonar y
  `pio run -e down` sin red ni excepción Avast.
- Commit en `agente/down`; merge a `main` lo hace Gustavo desde el repo principal
  (regla multi-agente: el agente no mergea).
