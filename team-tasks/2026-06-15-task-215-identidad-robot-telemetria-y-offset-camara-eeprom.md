---
task: 215
titulo: "TOP firmware (sesión RT): ID de robot en telemetría (Opción 3) + offset de cámara persistente (Idea B) + cerrar lazo de validación de config ToF (Idea A = TASK-206 A2.2 + echo cfg)"
fecha: 2026-06-15
asignado: equipo — sesión que reescribe el firmware TOP en real-time (banco lo cierra el equipo)
prioridad: P1
pedido-por: Gustavo Viollaz (2026-06-15)
relacionada: TASK-206 (config persistente TOP), TASK-022 (homografía cámara), TASK-205/208 (monitor app), monitor-base
estado: backlog (firmware) — el lado APP (Opción 1 identidad por-serial + Opción 2 guard de escritura) YA está hecho y verificado (monitor-base, 2026-06-15)
---

# TASK-215 — Identidad de robot + ajuste fino de cámara + validación de config ToF (lado firmware)

> **Contexto (Gustavo, 2026-06-15):** la app de PC no podía saber si miraba R1 o R2 (la
> telemetría NO trae ID de robot) → riesgo de cruzar config de ToF / escribir a la EEPROM del
> robot equivocado. Se resolvió el lado APP; lo de acá es el lado FIRMWARE, que pertenece a la
> sesión que está reescribiendo el TOP en RT. **No tocar el firmware desde la sesión de la app**
> (colisión). Esta task es el contrato para que esa sesión lo implemente coherente (ambos lados).

## Ya HECHO (lado app, monitor-base, 2026-06-15) — para que NO se reimplemente

- **Opción 1 — identidad por N° de serie USB del Teensy.** `robot_registry.py` (serial→robot/placa,
  sembrado con R1 TOP `19810740`), chip de identidad en la barra del shell (`▣ Robot 1 · TOP` /
  `⚠ Robot ? · serial …`), y **config keyed por serial** (`tof_layout_<serial>.json`) → R1 y R2
  **no comparten archivo**. `sources.serial_for_port()`, `tof_layout.config_path_for_serial()`.
- **Opción 2 — guard de escritura a EEPROM.** `safety.is_destructive_write()` + diálogo que NOMBRA
  el robot conectado antes de `CFG SAVE`/`CAL SAVE`/`IMU SAVE`/`CFG RESET`.
- La app YA genera los comandos `TOF n POS/ROT/FLIP/ZONE` EXACTOS del contrato de TASK-206
  (`tof_layout.to_firmware_commands()`), listos para bajar.

## Lo que falta (FIRMWARE — esta task)

### 1) Opción 3 — ID de robot en la telemetría (chico, alto valor)
- Emitir en el stream (bloque `cfg` o campo nuevo de `telemetry_top.h`): **`robot` (1|2)** + **`board`
  ("top")**. Así la app muestra la identidad que el robot **DECLARA**, no la que infiere del serial.
- Belt-and-suspenders: sobrevive incluso si se swapean placas Teensy entre robots (el serial seguiría
  al chip; el `robot` declarado sigue al binario `-DROBOT1/2`). Si difieren serial↔declarado, la app
  lo marca (mismatch) — señal de placa intercambiada.
- **Criterio de cierre (banco):** flashear R1 → la app muestra "Robot 1" por el campo declarado;
  flashear R2 → "Robot 2"; sin recompilar la app.

### 2) Idea A — config de ToF PERFECTA, conocida por el firmware y VALIDADA por la GUI
> **Por qué es FUNDAMENTAL (Gustavo):** el posicionamiento XY por ToF usa las distancias de las
> zonas válidas; si la ubicación/orientación de cada ToF y el veto de zonas no son **exactos y
> conocidos por el firmware**, el robot se posiciona mal aunque el monitor se vea bien. La GUI tiene
> que **validar** que lo que el firmware aplica == lo que el operador configuró.

- **= TASK-206 A2.2 + bloque `cfg` echo.** A2.1 ya persiste enable + posición/bearing por ToF. Falta:
  - (a) **Aplicar** rotación/flip/veto-de-zonas al cómputo de `mean_valid_zones` / al posicionamiento XY.
  - (b) **Persistir** zonas/rotación/flip en EEPROM (A2.1 ya persiste pos/enable; extender el struct).
  - (c) **ECHO** la config ACTIVA en el bloque `cfg` de la telemetría (pos, rot, flip, máscara de
        zonas por ToF) → la GUI dibuja "config del firmware" al lado de "config del operador" y marca
        verde si coinciden / rojo si no (lazo de validación). El lado app de la validación se agrega
        cuando exista el echo.
- **Criterio de cierre (banco):** `TOF n ZONE OFF` en filas altas + `CFG SAVE` + power-cycle → ese ToF
  deja de ver el techo Y la GUI muestra esa zona vetada leída del firmware (no solo del .json local).

### 3) Idea B — offset fino de cámara persistente (ajuste de competencia)
> **Por qué (Gustavo):** corregir un sesgo de ubicación de cámara EN COMPETENCIA sin recalibrar la
> matriz de distancias (homografía). Un ajuste rápido, no una recalibración.

- **EEPROM:** `cam_front_offset_x/y`, `cam_back_offset_x/y` (4× int16, mm). Default 0 = no-op.
- **Comandos:** `CAM F OFFSET <x> <y>` · `CAM B OFFSET <x> <y>` (persisten con `CFG SAVE`).
- **Aplicar:** sumar el offset a la posición de pelota/arco derivada de ESA cámara **antes** de
  construir el `WorldSnapshot` (en `cameras_runtime`/`build_snapshot`). Echo en `cfg`.
- **Viabilidad / límite honesto:** un offset CONSTANTE corrige un **sesgo sistemático** (todo
  corrido N mm), NO un error **dependiente de la distancia** (la homografía es no-lineal: el error
  suele crecer con la distancia). Es un ajuste fino de primer orden, complementa —no reemplaza— la
  homografía (TASK-022 / `docs/firmware/CALIBRACION-HOMOGRAFIA-XY-N6.md`). Igual es valioso: barato,
  reversible, y ataca el caso común (cámara levemente desalineada → todo corrido un poco).
- **Criterio de cierre (banco):** poner una pelota a distancia conocida; `CAM F OFFSET 0 50` → la
  Y estimada sube 50 mm en el snapshot (y en la vista Cancha); `CFG SAVE` + power-cycle → persiste.

## Lado app cuando el firmware exponga lo de arriba (queda para monitor-base)
- Validación de config ToF: dibujar "config del firmware (echo `cfg`)" vs "config del operador" con
  semáforo verde/rojo.
- Panel de offset de cámara: 4 sliders (F/B × X/Y), valores en pantalla, manda `CAM F|B OFFSET`,
  cacheado en el config por-serial; la EEPROM del robot es la verdad.
- Mismatch serial↔`robot` declarado → aviso en la barra.

## Cierre
Lo cierra el equipo en banco (regla no negociable). La sesión de firmware programa host-testeable +
glue; NO marca `done` el comportamiento en hardware. El lado app (Opción 1+2) ya está y no se re-hace.
