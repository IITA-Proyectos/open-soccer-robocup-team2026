---
title: "Verificar que la CENTRAL recibe TODO de TOP + DOWN (diag_central_rx_all)"
date: 2026-06-03
status: vivo
audiencia: "Virginia / Elías / Enzo — operativos en el banco"
firmware-source: software/teensy/Soccer 2026/src/diag/diag_central_rx_all.cpp
environment: "pio run -e diag_central_rx_all"
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
---

# `diag_central_rx_all` — ¿la CENTRAL recibe la info COMPLETA de TOP + DOWN?

## Para qué sirve

Programa de banco que se flashea en la **CENTRAL** (Teensy 4.1) y escucha **las
dos placas a la vez**, decodifica cada mensaje campo por campo, y cierra con un
**VEREDICTO DE COMPLETITUD** que responde de frente:

> **¿La CENTRAL está recibiendo TODA la información de TOP y DOWN?**

No mueve motores ni decide nada — es puro diagnóstico de recepción.

## Qué mensajes espera (info "completa")

| Placa | UART en CENTRAL | Mensaje | Tipo | Tamaño |
|---|---|---|---|---|
| **DOWN** | Serial1 (RX1 = **pin 0**) | `LineStatusV2` (LÍNEA) | 0x10 | 16 B |
| **DOWN** | Serial1 (RX1 = **pin 0**) | `Pose2D` (OTOS pose) | 0x11 | 7 B |
| **DOWN** | Serial1 (RX1 = **pin 0**) | `Velocity2D` (OTOS vel) | 0x12 | 7 B |
| **TOP** | Serial7 (RX7 = **pin 28**) | `WorldSnapshot` | 0x60 | 27 B |

**"Completo" = los 4 mensajes llegando frescos (<500 ms) + ambos enlaces sin
errores de CRC y sin huecos de SEQ.**

## Cableado (las 3 placas + GND común OBLIGATORIO)

| Origen | → | CENTRAL |
|---|---|---|
| DOWN TX1 (pin 1) | → | RX1 = **pin 0** (Serial1) |
| TOP TX4 (pin 17) | → | RX7 = **pin 28** (Serial7) |
| GND (las 3) | ↔ | GND común |

Baud: **230400** en ambos enlaces.

> ⚠️ **NO cablear nada al pin 7** — ahí va el motor 2 (U17). El link de DOWN es el
> **pin 0** (mapeo viejo "Serial2/pin 7" superado el 2026-05-31).

## Cómo correr

```bash
cd "software/teensy/Soccer 2026"
pio run -e diag_central_rx_all -t upload
pio device monitor -b 115200
```

Pre-requisitos: DOWN flasheado y **difundiendo** (broadcast: env `down` con `down_tx`;
OTOS alimentado desde **batería**, no USB) + TOP flasheado y emitiendo `WorldSnapshot`.

## Cómo leer el veredicto

El panel se redibuja cada 500 ms. El bloque final se ve así:

```
+--- VEREDICTO DE COMPLETITUD ----------------------------------+
 DOWN (Serial1):  enlace [OK]  (crc=0 seqGap=0)
   [OK]    LINEA      #1234  18.0 Hz  age 12ms
   [OK]    OTOS pose  #1230  18.0 Hz  age 20ms
   [FALTA] OTOS vel   #0     --       nunca
 TOP  (Serial7):  enlace [OK]  (crc=0 seqGap=0)
   [OK]    SNAPSHOT   #890   50.0 Hz  age 8ms
 ---------------------------------------------------------------
 >>> CENTRAL recibe TODO de DOWN+TOP:  NO   (falta: OTOS-vel ) <<<
+---------------------------------------------------------------+
```

| Marca | Significado |
|---|---|
| `[OK]` | ese mensaje llega FRESCO (<500 ms) |
| `[STALE]` | llegó alguna vez pero hace >500 ms que no → se cortó |
| `[FALTA]` | nunca llegó (`#0`) |
| `Hz` | tasa real del stream (confirma flujo continuo, no un frame suelto) |
| `enlace [OK]` | el UART decodifica frames, **0 CRC err** y **0 seqGap** |

**El LED de la CENTRAL queda FIJO solo si TODO está completo y fresco.** Si
parpadea, falta algo (mirar la línea `falta: ...`).

## Interpretación rápida (qué hacer según el veredicto)

| Síntoma | Causa probable | Acción |
|---|---|---|
| `bytes=0` en un enlace | cable/GND/baud | revisar TX→RX (pin 0 DOWN / pin 28 TOP), GND común, 230400 |
| `bytes` sube pero `frames=0` / `crc` alto | baud o TX/RX cruzados / masa flotante | revisar cableado y masa |
| `LINEA [OK]` pero `OTOS pose/vel [FALTA]` | **DOWN no difunde el OTOS** (env sin broadcast / OTOS sin batería) | **es DOWN, no la CENTRAL** — flashear `down` con `down_tx` + power-cycle con batería (ver ESTADO-ACTUAL "CÓMO ENCENDER LOS OTOS") |
| `SNAPSHOT [FALTA]` | TOP no emite / cable al pin 28 | confirmar TOP corriendo + cable TX4(pin17)→pin28 |
| `seqGap` sube | frames perdidos (buffer / backpressure) | anotar cuántos; ver `ANALISIS-COMM-DOWN-CENTRAL-2026-05-31.md` |
| todo `[OK]`, LED fijo | ✅ **CENTRAL recibe la info completa** | listo para integrar |

## Documentación esperada (journal post-test)

Entrada en `journal/YYYY-MM-DD-rx-all-<descripcion>.md` con: foto del setup de las
3 placas, captura del panel (el bloque del veredicto), y el veredicto final
(SÍ/NO + qué faltaba). Si faltaba algo, qué se hizo para resolverlo.

## Referencias

- Sketch: [`src/diag/diag_central_rx_all.cpp`](../../software/teensy/Soccer%202026/src/diag/diag_central_rx_all.cpp)
- Solo DOWN (panel detallado de la línea): [`DIAG-CENTRAL-COMM-DOWN.md`](DIAG-CENTRAL-COMM-DOWN.md)
- Protocolo: [`src/shared/proto.h`](../../software/teensy/Soccer%202026/src/shared/proto.h) · contratos: `types.h`, `line_view.h`, `pose_view.h`
- Análisis del link DOWN→CENTRAL: [`ANALISIS-COMM-DOWN-CENTRAL-2026-05-31.md`](ANALISIS-COMM-DOWN-CENTRAL-2026-05-31.md)
- TASK-031 (verificar UART DOWN/TOP→CENTRAL) · TASK-100 (validar ingest línea)

## Cambios

- 2026-06-03 — agregado el **veredicto de completitud** (`[OK]/[STALE]/[FALTA]` +
  Hz por mensaje + PASS/FAIL global + LED de completitud) al `diag_central_rx_all`
  existente, y creado este doc operativo. Compila `diag_central_rx_all` SUCCESS.
  NO validado en hardware (lo corre el equipo). Author: Claude Opus 4.8 (Anthropic).
  Requested-by: Viollaz.
