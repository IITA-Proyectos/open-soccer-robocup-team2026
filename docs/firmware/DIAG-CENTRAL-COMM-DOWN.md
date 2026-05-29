---
title: "Receiver de banco del link DOWN → CENTRAL (diag_central_comm_down)"
date: 2026-05-29
status: vivo
audiencia: "Virginia / Elías / Enzo — operativos en el banco"
firmware-source: software/teensy/Soccer 2026/src/diag/diag_central_comm_down.cpp
environment: "pio run -e diag_central_comm_down"
author: "Claude Opus 4.7 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
---

# `diag_central_comm_down` — Validar el link DOWN → CENTRAL

## Para qué sirve

**Primer paso del bring-up de la comunicación DOWN → CENTRAL** (el hito que
destraba la moratoria: "DOWN reportando línea por UART real a CENTRAL").

CENTRAL escucha el UART, corre el `FrameDecoder` del protocolo (`proto.h`), y
por cada frame imprime el **`LineStatusV2`** que DOWN ya manda + la **salud del
enlace** (bytes, frames OK, errores CRC, resyncs, huecos de SEQ).

**Filosofía:** probar que los **bytes fluyen con CRC OK** usando el payload que
DOWN ya emite, **antes** de sumar mensajes nuevos (los 32 sensores crudos son
un 2do paso — ver final). Si esto anda, el canal físico + el protocolo quedan
validados.

## ⚠️ UART: Serial7 (RX7 = pin 28), NO Serial2

El test `diag_central_motors` (2026-05-29) confirmó que el **motor 2 gira** con
los pines **8/7** (driver U17). Esos son exactamente los pines de **Serial2**
(RX2=7, TX2=8). Por lo tanto **los pines 7/8 son del motor — Serial2 NO se puede
usar** para DOWN→CENTRAL. Esto resuelve empíricamente la **TASK-036** (la cierra
el equipo, no Claude).

➡️ Se usa **Serial7 → RX7 = pin 28, TX7 = pin 29** (libres según el pinout).

**Cableado:**

| DOWN (Teensy 4.0) | → | CENTRAL (Teensy 4.1) |
|---|---|---|
| TX1 = **pin 1** | → | RX7 = **pin 28** |
| GND | ↔ | GND |

Baud: **230400** (igual que DOWN — `config_down.h` / `comm_central.cpp`).

> Pendiente relacionado: el firmware de **producción** `src/central/comm_down.cpp`
> todavía usa Serial2 — hay que migrarlo a Serial7. Flagueado, es scope CENTRAL.

## Procedimiento

### Pre-requisitos

- DOWN flasheado con su firmware (`pio run -e down -t upload`) y **enviando**
  (`comm_central_send_line_urgent` en su loop).
- Cableado de la tabla de arriba hecho (pin 1 DOWN → pin 28 CENTRAL, GND común).
- Ambas placas alimentadas.

### Correr

```bash
cd "software/teensy/Soccer 2026"
pio run -e diag_central_comm_down -t upload
pio device monitor -b 115200
```

El LED de CENTRAL queda **fijo** si recibe `LineStatusV2` fresco (<500 ms); si
no, **parpadea** (sin datos).

### Qué mirar (status cada 500 ms)

```
[COMM-DOWN] bytes=12345 frames=120 crc_err=0 resync=0 seq_gaps=0 otros=0 | LSV2 #120 hace 3ms valid=1 present=1 ang=45.0 pen=15 on_line=4/32 q=90 age=8ms flags=[IMM ]
```

| Campo | Qué significa |
|---|---|
| `bytes` | bytes crudos recibidos por Serial7 (si queda en 0 → no llega NADA: cable/pin/GND/baud) |
| `frames` | frames válidos decodificados (CRC OK + sync OK) |
| `crc_err` | frames con CRC malo (ruido / baud equivocado / mal cable) — debe ser ≈0 |
| `resync` / `seq_gaps` | resincronizaciones del decoder / frames perdidos |
| `valid` / `present` | `data_valid` y `line_present` del contrato |
| `ang` / `pen` / `on_line` | ángulo (°), penetración (mm), **cuenta** de sensores en línea (0–32) |
| `flags` | eventos: `IMM` (salida inminente), `LIFT` (levantado), `COR`, `END`, etc. |

## Interpretación rápida

- **`bytes=0` siempre** → no llega nada. Revisar: cable DOWN pin 1 → CENTRAL
  **pin 28** (¡no 7!), GND común, DOWN realmente enviando, baud 230400.
- **`bytes` sube pero `frames=0` / `crc_err` alto** → llega señal pero corrupta:
  baud equivocado, TX/RX cruzados mal, o masa flotante.
- **`frames` sube, `crc_err≈0`** → ✅ **link validado.** DOWN fuera de la línea:
  `present=0`. Sobre la línea: `present=1`, `on_line` sube, `ang/pen` coherentes.
  Levantado: `flags=[LIFT]`.

Esto alimenta la **TASK-100** (validación del ingest de línea DOWN→CENTRAL).

## Próximo paso — ver los 32 sensores individuales

Hoy DOWN manda solo el **resumen** (`LineStatusV2`), no los 32 sensores crudos.
Para verlos en CENTRAL hace falta un **mensaje nuevo** (`DOWN_LINE_RAW`, 32
bytes = 1/sensor, entra justo en el payload de 32 B del protocolo):

1. Definir `MsgType::DOWN_LINE_RAW` + encoder en `proto`/`shared`.
2. DOWN: emitir los 32 (lee `line_ring_get_raw(i)`, escala a 8 bits) — scope del
   **agente DOWN** (o coordinado).
3. CENTRAL: extender este diag para imprimir los 32 como barra/visualización.

→ Avisar cuando el link del resumen esté validado y lo armamos.

## Referencias

- Sketch: [`src/diag/diag_central_comm_down.cpp`](../../software/teensy/Soccer%202026/src/diag/diag_central_comm_down.cpp)
- Protocolo: [`src/shared/proto.h`](../../software/teensy/Soccer%202026/src/shared/proto.h) · contrato: [`src/shared/types.h`](../../software/teensy/Soccer%202026/src/shared/types.h) (`LineStatusV2`)
- Helpers de interpretación: [`src/shared/line_view.h`](../../software/teensy/Soccer%202026/src/shared/line_view.h)
- Emisor DOWN: [`src/down/comm_central.cpp`](../../software/teensy/Soccer%202026/src/down/comm_central.cpp)
- Contrato canónico: [`docs/firmware/CONTRATO-DATOS-DOWN.md`](CONTRATO-DATOS-DOWN.md)
- TASK-100 (validación ingest línea) · TASK-036 (pines 7/8, evidencia acá)

## Cambios

- 2026-05-29 — creación. Sketch receiver + env `diag_central_comm_down` + doc.
  Usa Serial7 (pines 7/8 = motor 2, confirmado por diag_central_motors).
  Author: Claude Opus 4.7 (Anthropic). Requested-by: Viollaz.
