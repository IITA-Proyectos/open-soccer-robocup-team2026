---
id: TASK-204
title: "Swap UART en TOP: cámara trasera → Serial5 (como está soldada) + link a CENTRAL → Serial7"
date_created: 2026-05-31
assigned: [agente TOP, Virginia, Elías]
priority: P1
status: pending
estimated_hours: 1.5
range: TOP (200-299)
flagged_by: "sesión CENTRAL (agente/central) — coordinación cross-placa. Ejecutar en la worktree soccer-agente-top."
tags: [top, uart, camaras, comunicacion, central, hardware-truth]
---

## Resumen

La **cámara trasera quedó soldada en los pines de Serial5 (RX5 = pin 21)** de la
placa TOP. El firmware actual la lee en **Serial7** y pone el link a CENTRAL en
**Serial5** → están **cruzados**. Hay que **swapearlos** en firmware + docs:
- cámara trasera → **Serial5** (donde está soldada),
- link TOP→CENTRAL (WORLD_SNAPSHOT) → **Serial7** (pines 28/29, el UART que queda libre).

Lo pidió Gustavo: *"la cámara ya está soldada en esos pines, acomodá los programas
y la doc; la comunicación con central y con abajo va en los pines que quedan."*

> ⚠️ Ejecutar **en la worktree TOP** (`soccer-agente-top`, branch `agente/top`).
> Esto es 100% placa TOP; la sesión CENTRAL NO lo toca para no pisar al agente TOP.

## Contexto / evidencia (banco 2026-05-31)

Con `diag_top_cameras` (escanea todos los UART) se vio:
- **Serial3 (pin 15)** [FRONTAL/U8] → `[DATOS] FORMATO OK`. ✅
- **Serial5 (pin 21)** [etiquetada "trasera vieja"] → `[DATOS] FORMATO OK`. ← **la trasera está acá** (se arregló un TX/RX cruzado del cable).
- **Serial7 (pin 28)** [etiquetada "trasera / U9"] → `0 bytes`. ← acá NO hay nada soldado.

El commit `695e7b0` (2026-05-29) movió la trasera Serial5→Serial7 y puso el link
a CENTRAL en Serial5, **asumiendo que la placa no estaba armada**. Se armó con la
trasera en Serial5 → hay que revertir ese movimiento (swap) y dejar la doc
consistente. Además quedó **medio migrado**: `pinout_common.h:46` y los
comentarios de `main_top.cpp:7,25,116` todavía dicen "cámaras = Serial3+Serial5"
(o sea, ya asumen lo correcto), mientras `cameras_runtime.cpp` y `comm_central.cpp`
quedaron en el estado nuevo (cruzado).

**Por qué importa (estado roto hoy):** la trasera (TX en pin 21) le mete bytes al
RX de Serial5, que `comm_central` lee como "comandos desde CENTRAL" → basura; y la
trasera no se lee (firmware escucha Serial7 = vacío). El arquero pierde la cámara
de atrás y el RX del link a CENTRAL queda contaminado.

## Mapa UART objetivo (Teensy 4.0, placa TOP)

| UART | Pines RX/TX | Rol | Cambio |
|------|-------------|-----|--------|
| **Serial1** | 0 / 1 | ← DOWN (odometría OTOS) | sin cambio |
| **Serial3** | 15 / 14 | ← cámara **FRONTAL** (U8) | sin cambio |
| **Serial4** | 16 / 17 | ↔ placa COMM (árbitros/ESP-NOW) | sin cambio |
| **Serial5** | 21 / 20 | ← cámara **TRASERA** (soldada acá) | **Serial7→Serial5** |
| **Serial7** | 28 / 29 | → **CENTRAL** (WORLD_SNAPSHOT) | **Serial5→Serial7** |

No usar **Serial2 (pin 7)**: conflicto con HC-SR04 ECHO. **Serial6 (pin 24/25)** = I²C
`Wire1` (no es UART). Por eso el link a CENTRAL va a **Serial7**.

## Cambios exactos

### Firmware (`src/top/`)
1. **`cameras_runtime.cpp`** — trasera `Serial7` → `Serial5`:
   - `cameras_init()` (≈L92): `Serial7.begin(...)` → `Serial5.begin(...)`.
   - drenado de la trasera (≈L115-119): `Serial7` → `Serial5`.
   - comentarios L33-38 (incl. el bloque "MOVIDA 2026-05-29"): reescribir → la
     trasera está en Serial5 (soldada); Serial7 ahora es el link a CENTRAL.
   - **`cameras_runtime.h`** L1,4,5: "Serial3 + Serial7" → "Serial3 + Serial5".
2. **`comm_central.cpp`** (TOP→CENTRAL) — `Serial5` → `Serial7`:
   - `comm_central_init()` (L34): `Serial5.begin` → `Serial7.begin`.
   - tick RX (L39): `Serial5.available/read` → `Serial7`.
   - TX del snapshot (L59): `Serial5.write` → `Serial7.write`.
   - comentarios L27-32: reescribir (link a CENTRAL = Serial7, TX7=pin 29, RX7=pin 28).
   - **`comm_central.h`** L3: stale "Serial2 (pines 7/8)" → "Serial7 (pines 28/29)".
3. **`pinout_common.h`** L43-46: reconciliar:
   - `UART_TO_ZIRCON_BAUD` (link a CENTRAL/Zircon): el comentario debe decir **Serial7**.
   - `UART_CAMERA2_BAUD`: el comentario "Serial5" **ya queda correcto** (trasera).
4. **`main_top.cpp`** comentarios: L14 y L119 ("WORLD_SNAPSHOT a CENTRAL (Serial2)")
   → **Serial7**. L7/L25/L116 ("cámaras Serial3+Serial5") **ya quedan correctos**.

### Diag (`src/diag/`)
5. **`diag_top_cameras.cpp`** — actualizar las etiquetas para que reflejen la
   realidad: la trasera es **Serial5** (no "trasera vieja"); **Serial7 ya no es
   una cámara** (es el link a CENTRAL, es TX desde TOP → en el escáner RX va a dar 0).
   Líneas ≈16, 55-56, 119-120, 126 + las etiquetas del escáner.

### Docs (consistencia en el mismo commit — regla de sesión)
6. `hardware/electronics/top-board-pack/01-pinout-y-hardware.md`, `02-funcionalidad.md`,
   `06-arquitectura-3-placas.md` — tabla de UART.
7. `docs/firmware/CONTRATO-DATOS-TOP.md` y `CONTRATO-DATOS-CAMARAS.md` — asignación de UART.
8. `hardware/electronics/cameraBack-pack/01-hardware-y-conexion.md` + `README.md` —
   trasera en Serial5 / pin 21.
9. `docs/ARQUITECTURA-3-PLACAS-2026.md`, `docs/firmware/FIRMWARE-PLACA-ARRIBA.md`,
   `docs/FUENTES-DE-VERDAD.md` (si canoniza el mapa UART), `docs/ESTADO-ACTUAL.md`
   (sección TOP).

## Lado CENTRAL (NO se toca firmware)

`comm_top` de CENTRAL recibe el snapshot en **Serial1 (pin 0)** — **no cambia**.
Lo único físico: el cable del link sale ahora por el **pin 29 (TX7) del TOP** (en
vez del pin 20) → sigue entrando al **pin 0 (RX1) del CENTRAL**. Solo se recablea
el extremo TOP.

## Criterio de cierre

- [ ] `pio run -e top_robot1` → SUCCESS (y `top_robot2`).
- [ ] `pio test -e test_native` → sigue 262/262 (el swap no toca lógica testeada).
- [ ] En `diag_top_cameras`: la **trasera da `[DATOS] FORMATO OK` en Serial5** (pin 21);
      Serial7 en 0 en el escáner (es TX del link, no RX). Frontal en Serial3 ✅.
- [ ] Con `main_top` real: `cameras_front_alive()` **y** `cameras_back_alive()` = true
      (las 2 cámaras fusionan).
- [ ] CENTRAL recibe `WORLD_SNAPSHOT` (`world_model_snapshot_is_fresh()`=true) con el
      cable del TOP en pin 29 → CENTRAL pin 0.
- [ ] Docs sin contradicciones (grep "Serial5"/"Serial7" en top-board-pack + contratos).

## Notas / decisiones

- Confirmar con **Enzo** a qué conector físico (¿U-cuál?) corresponde el pin 21 de
  Serial5, para rotular bien la doc de hardware (el diag lo llamaba "trasera vieja").
- Este swap **reconcilia** el firmware con lo que `pinout_common.h:46` + los
  comentarios de `main_top` ya asumían (cámaras en Serial3+Serial5). La migración
  de `695e7b0` quedó a medio aplicar; esto la termina al revés (correcto p/ la placa armada).
- **Claude no cierra esta TASK** — el equipo valida en hardware (cámaras vivas +
  snapshot a CENTRAL).

## Cambios de estado

- 2026-05-31 — creada por la sesión CENTRAL (análisis del banco + diag_top_cameras).
  Flagueada para el agente TOP. Author: Claude Opus 4.8 (Anthropic). Requested-by: Viollaz.
