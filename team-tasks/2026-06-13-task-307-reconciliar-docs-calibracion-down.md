---
task: 307
titulo: "Reconciliar 3 fuentes que se contradicen sobre cómo calibrar la línea (¿reflashear o no?)"
fecha: 2026-06-13
asignado: equipo (Gustavo decide el flujo canónico; alumno valida en banco)
prioridad: P2
pedido-por: Gustavo Viollaz (2026-06-13) — disparado por un error de Claude que siguió un doc viejo
relacionada: TASK-306 (app monitoreo), TASK-304/305 (telemetría/app), FUENTES-DE-VERDAD
estado: pending
---

# TASK-307 — Tres fuentes se contradicen sobre la calibración de línea

> **Origen:** el 2026-06-13 Claude recomendó calibrar flasheando
> `diag_down_calibracion` + reflashear, siguiendo `QUE-FLASHEO-HOY.md`. Gustavo
> recordó que la calibración estaba integrada al firmware de partido (sin
> reflashear). Al verificar el código, Gustavo tenía razón — pero **tres docs
> dicen tres cosas distintas**.

## La contradicción (verificada)

| Fuente | Qué dice | ¿Al día? |
|---|---|---|
| **Código** (`main_down.cpp` + `down_telemetry_serial.cpp`) | El stream + dispatch de comandos (CAL CARPET/WHITE/SAVE) compila bajo `#if defined(DOWN_DEBUG_TELEMETRY) \|\| defined(DOWN_USB_MONITOR)`. **`[env:down]` define `-DDOWN_USB_MONITOR`** → el binario de **competencia** lleva el monitor DORMIDO. | **Es la verdad** |
| `platformio.ini [env:down]` (TASK-306, 12-jun) | "monitor dormido en binario de partido; enchufás USB + app → calibrás EN VIVO **sin reflashear**; sacás cable → 3 s → modo partido". | ✅ coincide con el código |
| `docs/firmware/USO-MONITOREO-Y-TELEMETRIA.md` (guía canónica de uso) | **"¿Hay que reflashear? SÍ, una vez"** + "el env de competencia tiene el flag apagado, binario byte-idéntico". **No menciona `DOWN_USB_MONITOR`.** | ❌ viejo/contradice al código |
| `docs/pruebas-banco/QUE-FLASHEO-HOY.md` línea 29 (fuente canónica de ENVS) | `diag_down_calibracion` (`c`→`b`→`v`→`s`) + "al terminar RE-flashear el down". | ❌ omite el camino en vivo |

**Causa:** TASK-306 agregó `-DDOWN_USB_MONITOR` a `platformio.ini` pero **no
actualizó** ni la guía de uso ni `QUE-FLASHEO-HOY.md` (se rompió la regla "doc
canónico se actualiza en el MISMO commit que el cambio").

## Qué hay que decidir/confirmar (humano + banco)

1. **¿El camino en vivo sobre competencia REALMENTE anda?** El código lo compila,
   pero falta confirmar en banco el handshake: ¿la app `monitor-base` despierta el
   monitor dormido del binario `down`/`down_robot2` (manda PING/STREAM_ON) y
   `CAL SAVE` persiste bien? Probarlo end-to-end. *(TASK-306 se llamaba "app
   CONFIABLE": chequear si la conclusión fue usar `down_debug_telemetry` por
   confiabilidad, o si el camino en vivo ya quedó validado.)*
2. **Definir el flujo canónico** (en vivo sobre `down` vs reflashear
   `down_debug_telemetry`) y **alinear las 3 fuentes en UN commit**: guía de uso,
   `QUE-FLASHEO-HOY.md` línea 29, y el comentario de `platformio.ini`.
3. **De paso:** TASK-306 bug #1 (calib no se aplica hasta reboot) parece **ya
   resuelto** en el código (`comm_central_invalidate_calib()` se llama en los casos
   CAL_* de `down_telemetry_serial.cpp`). Verificar y actualizar el estado de
   TASK-306 si corresponde.

## Criterio de cierre

Un commit que deja las 3 fuentes diciendo **lo mismo** (lo confirmado en banco), y
que cualquiera que pregunte "¿cómo calibro la línea?" obtiene UNA respuesta, sin
contradicción. Mientras tanto, `QUE-FLASHEO-HOY.md` queda con un flag que apunta a
esta tarea.

## Nota de proceso (por qué pasó y cómo evitarlo)

- El cambio de comportamiento de un env (TASK-306) tocó `platformio.ini` pero no
  los docs que describen el flujo → quedaron en contradicción. **Reforzar:** todo
  cambio de env/flag actualiza, en el mismo commit, `QUE-FLASHEO-HOY.md` y la guía
  de uso afectada.
- **Chequeo barato para auditorías:** cruzar lo que dice `QUE-FLASHEO-HOY.md` /
  la guía contra los `build_flags` reales del env en `platformio.ini`. Una
  auditoría de consistencia doc↔código habría detectado este desfase.
