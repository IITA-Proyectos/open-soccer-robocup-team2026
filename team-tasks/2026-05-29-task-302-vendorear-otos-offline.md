---
id: TASK-302
title: "Vendorear SparkFun OTOS + Toolkit en lib/ para que [env:down] y [env:diag_down] compilen offline"
date_created: 2026-05-29
assigned: [gviollaz]
priority: P2
status: pending
estimated_hours: 0.5
blocks: [build reproducible de DOWN en máquinas con Avast/SSL]
blocked_by: [acceso de red a la lib OTOS UNA vez (excepción Avast TASK-025 o máquina con red sana)]
tags: [infra, build, platformio, down-board, vendoring, avast]
---

# TASK-302 — Vendorear OTOS + Toolkit (compile offline de DOWN)

## Resumen

`[env:down]` y `[env:diag_down]` son los **únicos firmwares del repo que NO
compilan offline**. Todos los demás (`top`, `central_*`, los `diag_top_*`)
tienen sus libs **vendoreadas en `lib/`** (Adafruit BNO055/BusIO/Unified
Sensor, STM32duino VL53L5/7/8, Adafruit VL53L7CX, Unity) — ver `lib/README.md`
— justamente para esquivar el problema Avast/SSL (`CRYPT_E_NO_REVOCATION_CHECK`)
documentado en TASK-025.

Pero `[env:down]` todavía tiene en `platformio.ini`:
```ini
lib_deps =
    sparkfun/SparkFun Qwiic OTOS Arduino Library
```
Eso obliga a PlatformIO a **bajarla del registry**. En una máquina con Avast
interceptando TLS, la descarga falla con `HTTPClientError` y el build se
cae ANTES de compilar nada. (Pasó el 2026-05-29 en la worktree DOWN.)

## Workaround usado el 2026-05-29 (temporal, NO commiteado)

Se copió el OTOS ya bajado del repo principal a la worktree:
```bash
cp -r ".../open-soccer-robocup-team2026/.../.pio/libdeps/down/SparkFun Qwiic OTOS Arduino Library" \
      ".../soccer-agente-down/.../.pio/libdeps/down/"
cp -r ".../.pio/libdeps/down/SparkFun Toolkit" ".../soccer-agente-down/.../.pio/libdeps/down/"
```
`.pio/` está gitignored → esto NO sobrevive a un `pio run -t clean` ni a otra
worktree/máquina. Es parche local, no solución.

## Solución propuesta (la del patrón del repo)

1. En una máquina con red sana (o con la excepción Avast de TASK-025 aplicada),
   dejar que PlatformIO baje la lib UNA vez: `pio pkg install -e down`.
2. Copiar `SparkFun Qwiic OTOS Arduino Library` **y** su dep transitiva
   `SparkFun Toolkit` desde `.pio/libdeps/down/` a `lib/` (podando ejemplos/docs
   como se hizo con las otras, ver `lib/README.md` para el criterio de poda).
3. Renombrar carpetas a slug sin espacios si hace falta (ej. `SparkFun_Qwiic_OTOS`,
   `SparkFun_Toolkit`) — chequear que el `library.json`/`.properties` siga válido.
4. En `platformio.ini`, **quitar** el bloque `lib_deps` de `[env:down]` y
   `[env:diag_down]` (PIO LDF detecta las libs de `lib/` solo, igual que con
   BNO055). Dejar un comentario como los de `[env:top]`/`[env:central_*]`.
5. `pio run -t clean -e down && pio run -e down` debe compilar 100% offline.
6. Idem `pio run -e diag_down`.
7. Commitear las libs vendoreadas + el cambio de `platformio.ini` + actualizar
   `lib/README.md` con la nueva entrada.

## ⚠️ Coordinación multi-agente

`platformio.ini` es archivo compartido (lo edita también el agente TOP — HAL
refactor). Antes de tocarlo: `git fetch && git log origin/main -10 -- "software/teensy/Soccer 2026/platformio.ini"`.
Idealmente lo hace Gustavo desde el repo principal, o el agente DOWN con la
coordinación explícita.

## Criterio de cierre

- [ ] `lib/` contiene OTOS + Toolkit podadas y commiteadas.
- [ ] `[env:down]` y `[env:diag_down]` sin `lib_deps` de registry.
- [ ] `pio run -t clean -e down` compila offline en una máquina con Avast.
- [ ] `lib/README.md` actualizado.

## Cambios de estado

- 2026-05-29: creada al detectar el gap durante la compilación de las mejoras
  de robustez DOWN (Claude Opus 4.7, requested-by Gustavo).
