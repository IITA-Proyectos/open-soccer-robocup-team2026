---
title: "DOWN: implementación de robustez del audit (P0.2 calib EEPROM, P1.5 all-white, P1.6 backpressure UART)"
date: 2026-05-29
author: "Claude (Anthropic - Claude Opus 4.7)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7, Anthropic)"
status: final
tags: [down, robustez, calib, eeprom, uart, linea, audit, firmware]
robot: ambos
area: control
tipo: resultado
---

# DOWN — robustez: P0.2 + P1.5 + P1.6 implementados en firmware

## Contexto

Sesión de desarrollo de la placa DOWN. El pedido: análisis independiente
de lo hecho, comparar con la funcionalidad descripta, encontrar puntos de
falla, y **en paralelo** seguir implementando y resolviendo problemas.

Ya existía un audit exhaustivo del 2026-05-29 (commit `02d08ae`,
`research/in-progress/2026-05-29-auditoria-exhaustiva-placa-down.md`).
Para NO duplicar (regla 4 de CLAUDE.md), la decisión fue **verificar el
audit contra el código e implementar los 3 hallazgos in-scope de firmware**,
no escribir un audit rival. Los 3: P0.2 (persistencia de calib), P1.5
(rechazo de saturación todo-blanco), P1.6 (backpressure UART).

## Qué se hizo

**P1.5 — rechazo "todo blanco" (shared, testeado host).**
- `lf_all_white(white, n, min_white_count)` en `line_filters.{h,cpp}`:
  predicado **puro stateless** (no el `_rejection` con mutación in-place que
  sugería el audit — la decisión vive en `down_model`, no en el filtro).
- En `dm_update`: si ≥7/8 sensores marcan blanco (umbral espejo del detector
  de "levantado"), se invalidan todos los sensores → la geometría da
  `line_present=0` sola, **se saltea la adaptación de calib** ese tick (clave:
  si no, el baseline de carpet se arrastra hacia el blanco — bug nuevo que
  hubiera introducido) y se marca `data_valid=0`. Señaliza por
  `EV_CALIB_SUSPECT` (el contrato de 16 B / 8 flags está lleno, sin bit libre).

**P0.2 — persistencia de calib en EEPROM (Arduino, compile-only).**
- El snippet del audit era **incorrecto**: asumía `g_model.calib` global en
  `main_down.cpp`. En realidad el `DownModel` vive en el namespace anónimo de
  `comm_central.cpp`. Integración real: nueva `comm_central_load_persisted_calib()`
  que carga de EEPROM y setea `g_dm_init=true` para **bloquear el lazy-init**
  (la EEPROM gana porque trae blanco real que el boot no tiene). El SAVE se
  dispara al completar el paso "blanco" del comando `CENTRAL_CALIB_LINE`.
- `main_down.cpp::setup()` la llama después de `line_ring_calibrate_carpet()`.

**P1.6 — backpressure UART (Arduino, compile-only).**
- Guard `availableForWrite()` en **ambos** emisores: `comm_central.cpp`
  (Serial1, 200 Hz) y `comm_top.cpp` (Serial5, 100 Hz) + contadores
  `..._get_frames_dropped()`. Sin el guard, `Serial.write()` con buffer lleno
  hace busy-wait y le roba ciclos al `line_ring` de 1 kHz.

## Qué se midió/observó

- **Tests host-native (g++, fallback de TASK-025 — `pio test` no resuelve Unity
  offline):** `test_line_filters` **39/39** (6 nuevos de saturación),
  `test_down_model` **7/7** (2 nuevos), 0 fallos, sin regresión.
- **Compilación firmware:** `pio run -e down` → **SUCCESS** (FLASH code 33416 B,
  linkea OK).
- **Bloqueador de build encontrado y destrabado:** `[env:down]` NO compilaba —
  PlatformIO intentaba bajar `sparkfun/SparkFun Qwiic OTOS` del registry y
  fallaba con `HTTPClientError`. Probé egress: TODO HTTPS muere con
  `CRYPT_E_NO_REVOCATION_CHECK` (Avast interceptando TLS — TASK-025), tanto el
  registry como GitHub. OTOS es la **única lib de firmware sin vendorear** (las
  demás están en `lib/`). Destrabado copiando el OTOS+Toolkit ya bajado del
  repo principal a `.pio/libdeps/down/` (artefacto local, gitignored).

## Conclusión

Los 3 hallazgos in-scope quedaron **implementados y compilando**. La lógica
pura (P1.5) está además testeada host. Los cambios Arduino-only (P0.2, P1.6)
son **compile-only**: no se pueden host-testear (dependen de Serial/EEPROM)
y **NO** se validaron en hardware. Por regla 1 de CLAUDE.md, Claude no cierra
tasks de hardware — la validación real es **TASK-301**.

Hallazgo infra colateral: `[env:down]`/`[env:diag_down]` son los únicos
firmware que no compilan offline → **TASK-302** (vendorear OTOS).

## Próximos pasos

- **TASK-301** (equipo): validar en banco los 3 criterios (power-cycle calib,
  all-white con luz real, frames_dropped bajo carga). Depende de TASK-031 (UART).
- **TASK-302** (Gustavo): vendorear OTOS+Toolkit en `lib/` y quitar el `lib_deps`
  de registry — cierra el gap de build offline.
- Fuera de scope DOWN (quedan en el audit): P0.4 (guard `data_valid` en
  `strategy.cpp` — toca CENTRAL), P0.3 (medir MP1584 — hardware).
