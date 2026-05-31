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

## ⚠️ UART: Serial2 (RX2 = pin 7) — el enlace validado en banco

Se usa **Serial2 (RX2 = pin 7, TX2 = pin 8)** porque es el **cableado ya
validado**: el 2026-05-29 el par mínimo `diag_down_send1` / `diag_central_recv1`
confirmó que el `'1'` llega por **pin 7**. Es además el mismo UART que usan
`diag_central_recv1` y el `comm_down.cpp` de producción → **no hay que recablear
entre tests**.

**Sobre el conflicto 7/8 (motores):** el motor 2 (driver U17) usaría los pines
**8/7** = los de Serial2. PERO el conflicto **solo aparece manejando motores**, y
este receiver **NO los maneja** → pin 7 es seguro acá. El veredicto (¿el motor de
U17 realmente gira con 7/8?) **quedó PENDIENTE de aislar** (Gustavo 2026-05-29) →
**TASK-036 sigue abierta**. Solo si se confirma 7/8 = motor **y** se corren
**motores + comunicación juntos**, recién ahí migrar a **Serial7 (28/29)** (acá y
en `comm_down.cpp`). El veredicto 7/8 y demás pruebas de motores quedan pendientes
— ver [`DIAG-CENTRAL-MOTORS.md`](DIAG-CENTRAL-MOTORS.md).

**Cableado:**

| DOWN (Teensy 4.0) | → | CENTRAL (Teensy 4.1) |
|---|---|---|
| TX1 = **pin 1** | → | RX2 = **pin 7** |
| GND | ↔ | GND |

Baud: **230400** (igual que DOWN — `config_down.h` / `comm_central.cpp`).

## Procedimiento

### Pre-requisitos

- DOWN flasheado con su firmware (`pio run -e down -t upload`) y **enviando**
  (`comm_central_send_line_urgent` en su loop).
- Cableado de la tabla de arriba hecho (pin 1 DOWN → pin 7 CENTRAL, GND común).
- Ambas placas alimentadas.

### Correr

```bash
cd "software/teensy/Soccer 2026"
pio run -e diag_central_comm_down -t upload
pio device monitor -b 115200
```

El LED de CENTRAL queda **fijo** si recibe `LineStatusV2` fresco (<500 ms); si
no, **parpadea** (sin datos).

### Qué mirar (panel cada 500 ms)

El display es un **panel legible**, no una línea densa de números:

```
+=========== LINEA  DOWN -> CENTRAL ===========+
 ESTADO   : >>> SOBRE LA LINEA <<<
 DIRECCION: > DERECHA  (45 grados)
 SENSORES : 04/32 [######------------------]
 PENETRA. :  15 mm [#####-----------]
 CALIDAD  : 90/100
 EVENTOS  : SALIDA-INMINENTE
 CAMPOS   : schema=2 valid=1 present=1 on_line=4 q=90 age=8ms ev=0x1
            angle=45.00deg escape=N/A pen=15mm cross=-3mm
 ----------------------------------------------
 ENLACE   : 120 frames | 0 CRC err | 0 perdidos | hace 8 ms
+==============================================+
```

| Línea | Qué significa |
|---|---|
| **ESTADO** | `fuera de linea` / `SOBRE LA LINEA` / `SALIDA INMINENTE` / `ROBOT LEVANTADO` / `DATOS NO VALIDOS` |
| **DIRECCION** | flecha + palabra + grados (0° = frente; el signo izq/der se confirma en banco) |
| **SENSORES** | **CUENTA** de sensores en línea (0–32) — no las posiciones (eso necesita el mensaje de 32 crudos, ver abajo) |
| **PENETRA. / CALIDAD / EVENTOS** | campos del contrato `LineStatusV2` |
| **CAMPOS** | vista **cruda decodificada**: TODOS los campos del `LineStatusV2` campo por campo (lo que sale del decoder), con `N/A` donde el contrato usa sentinel |
| **ENLACE** | salud del link: `frames` sube + `CRC err ≈ 0` = **link OK**. `[STALE!]` = no llegan datos hace >500 ms |

Si todavía no llegó ningún frame, el panel muestra **"ESPERANDO datos de DOWN"** con la checklist de cableado.

## Interpretación rápida

- **`bytes=0` siempre** → no llega nada. Revisar: cable DOWN pin 1 → CENTRAL
  **pin 7** (RX2), GND común, DOWN realmente enviando, baud 230400.
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
- TASK-100 (validación ingest línea) · TASK-036 (pines 7/8, veredicto pendiente de aislar)

## Cambios

- 2026-05-29 — creación. Sketch receiver + env `diag_central_comm_down` + doc.
  Usa Serial7 para esquivar el posible conflicto 7/8 con el motor 2 (veredicto sin aislar — TASK-036).
  Author: Claude Opus 4.7 (Anthropic). Requested-by: Viollaz.
- 2026-05-29 (b) — **unificado el link de banco en Serial2 / pin 7** (el enlace
  validado por `diag_down_send1`/`diag_central_recv1`; mismo UART que el
  `comm_down.cpp` de producción → no se recablea entre tests). El Serial7
  (motor-safe) queda documentado para cuando se corran **motores + comm juntos**
  y se cierre el veredicto 7/8. Veredicto 7/8 + mapeo/orientación de motores:
  **pendientes** (TASK-036 — ver `DIAG-CENTRAL-MOTORS.md`).
  Author: Claude Opus 4.8 (Anthropic). Requested-by: Viollaz.
