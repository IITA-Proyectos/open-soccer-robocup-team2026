---
title: "Mapa de datos end-to-end — qué corre en cada placa y cómo se mandan los datos"
date: 2026-06-03
status: vivo
tipo: indice-referencia
audiencia: "todo el equipo — entrar acá para entender el sistema de un vistazo"
author: "Claude Opus 4.8 (Anthropic), vía Claude Code"
requested-by: "Gustavo Viollaz (@gviollaz)"
---

# Mapa de datos — qué corre y cómo viaja la información

> **Para qué es este doc.** Un solo lugar para entender, de una pasada, **qué hace
> cada placa** y **qué dato le manda a quién, cómo y por dónde**. No reemplaza a los
> contratos byte-a-byte ni al mapa de cableado — los **indexa**. Si algo acá
> contradice al **código** (`src/shared/types.h`, `proto.h`) o al **cableado**
> (`hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md`), gana esa fuente.

## 1. Qué corre en cada placa

| Placa | Teensy | Rol | Qué hace |
|---|---|---|---|
| **TOP** | 4.0 | **Maestra de percepción** | Lee 2 cámaras + 2 BNO + 4 ToF, lee el árbitro (GPIO), **fusiona todo** y arma el `WorldSnapshot`. |
| **CENTRAL** | 4.1 | **Cerebro táctico** | Recibe el `WorldSnapshot` (NO fusiona), corre la FSM (`strategy.cpp`) y maneja los 3 motores omni. |
| **DOWN** | 4.0 | **Piso** | Anillo de 32 sensores de línea + 2 OTOS (odometría). Difunde `LineStatusV2` + pose/vel OTOS. |
| **COMM** | ESP32-C6 | **Árbitro + partner** | Recibe el START/STOP del árbitro RCJ y lo entrega al TOP por **nivel GPIO** (no UART). Partner ESP-NOW. |
| **2× Cámara** | OpenMV N6 | **Visión** | Detectan pelota naranja + arcos amarillo/azul por color LAB; mandan blobs al TOP. |

## 2. Mapa de mensajes (la data y cómo se manda)

> Transporte = **placa · Serial · pin (dirección)**. Todos los UART entre placas a
> **230400** salvo cámaras (**19200**) y COMM (**115200**). Framing de los UART
> inter-placa: `proto.h` (`FrameDecoder` + CRC16, ~7 B de overhead por frame).

| Mensaje | Tipo / tamaño | De → A | Transporte | Freq | Lo llena | Lo consume | Contrato canónico |
|---|---|---|---|---|---|---|---|
| **WorldSnapshot** | `0x60` · **27 B** (schema v2) | **TOP → CENTRAL** | TOP `Serial4` pin17 (TX4) → CEN `Serial7` pin28 (RX7) | 100 Hz | `main_top.cpp::build_snapshot` | `comm_top`→`world_model`→`strategy.cpp` | `CONTRATO-DATOS-CENTRAL.md` / `CONTRATO-DATOS-TOP.md §3` |
| **LineStatusV2** | `0x10` · **16 B** | **DOWN → CENTRAL y TOP** (broadcast) | DOWN `Serial1` pin1 → CEN `Serial1` pin0 · y DOWN `Serial5` pin20 → TOP `Serial1` pin0 | ~alta | `comm_central.cpp` (DOWN) vía `down_tx` | CEN: `comm_down`→`world_model`. TOP: cacheado (aún no consumido) | `CONTRATO-DATOS-DOWN.md` |
| **Pose2D (OTOS)** | `0x11` · **7 B** | **DOWN → CENTRAL y TOP** (broadcast) | (igual que LineStatusV2) | ~alta | DOWN `down_tx` | CEN: control de movimiento (Capa 2). TOP: cacheado | `CONTRATO-DATOS-DOWN.md` |
| **Velocity2D (OTOS)** | `0x12` · **7 B** | **DOWN → CENTRAL y TOP** (broadcast) | (igual que LineStatusV2) | ~alta | DOWN `down_tx` | idem Pose2D | `CONTRATO-DATOS-DOWN.md` |
| **Blobs cámara** | **9 B** · headers `201`=pelota / `202`=arco amarillo / `203`=arco azul | **Cámara → TOP** | frontal `Serial3` pin15 (U8) · trasera `Serial5` pin21 (U9) | ~30 Hz | script OpenMV (`cam-*-n6.py`) | `cameras`→`cameras_fusion`→`build_snapshot` | `CONTRATO-DATOS-CAMARAS.md` |
| **Árbitro START/STOP** | **NIVEL GPIO** (no es un frame) | **COMM → TOP** | TOP pines **5 y 6** (`INPUT_PULLDOWN`); `match_running = pin5 OR pin6` | continuo | COMM (ESP32-C6) | `comm_arbiter.cpp` → flag `MATCH_RUNNING`/`referee_cmd` del `WorldSnapshot` | §1.1 de `MAPA-CONEXIONES-3-PLACAS.md` (TASK-039) |
| **Partner / status** | ESP-NOW | **TOP ↔ COMM** | TOP `Serial2` pines 7/8 | — | — | partner (no crítico) | — |

### Qué lleva el WorldSnapshot (resumen)
Pose propia (x/y/heading/conf), pelota (x/y relativos + velocidad vx/vy + visible/conf),
arco rival (ángulo+distancia) y visibilidad del propio, obstáculo más cercano
(`min_obstacle_mm`), comando del árbitro (`referee_cmd`), y `flags`
(in_penalty / partner_alive / partner_sees_ball / **match_running**).
Struct exacto: `src/shared/types.h` (`struct WorldSnapshot`, 27 B, con `static_assert`).

## 3. El árbitro NO viaja por UART (importante)

El START/STOP del árbitro entra al TOP como **nivel digital en los pines 5/6** (0 = STOP,
3.3 V = PLAY), no como mensaje serial. En PLAY el COMM sube **uno solo** de los dos pines,
por eso el firmware usa `match_running = pin5 OR pin6` (fail-safe: cable desconectado →
ambos en 0 → STOP). Después ese estado **viaja dentro del WorldSnapshot** (campo
`referee_cmd` / flag `MATCH_RUNNING`) hacia la CENTRAL, que es quien decide. Detalle:
`MAPA-CONEXIONES-3-PLACAS.md §1.1` (TASK-039, banco 2026-06-02).

## 4. Estado de cada enlace (al 2026-06-03)

| Enlace | Estado |
|---|---|
| DOWN → CENTRAL | ✅ cable validado en banco (2026-06-02) |
| TOP → CENTRAL | 🔧 mapeo corregido 2026-06-02 (TOP `Serial4`, no `Serial7`); cablear TOP pin17 → CEN pin28 |
| DOWN → TOP | ⚠️ sin cablear todavía |
| Cámaras → TOP | ✅ formato OK; **falta calibrar color LAB** (TASK-022, bloqueante #1) |
| Árbitro (COMM → TOP) | ✅ GPIO validado en banco (TASK-039) |

## 5. Cómo VERLO en vivo (herramientas de banco)

| Quiero ver… | Herramienta | Cómo |
|---|---|---|
| Todo lo que la CENTRAL recibe de TOP **y** DOWN, decodificado + veredicto | `diag_central_rx_all` | `pio run -e diag_central_rx_all -t upload` · `pio device monitor -b 115200` |
| Solo el link DOWN→CENTRAL | `diag_central_comm_down` | ídem, env `diag_central_comm_down` |
| Lo que DOWN transmite (sin CENTRAL) | `diag_down_debug` / `down_debug` | por USB del DOWN |
| Lo que ve cada cámara | script OpenMV (`cam-*-n6.py`, `BRING_UP=True`) | OpenMV IDE |

## 6. Fuentes canónicas (la verdad detrás de este índice)

- **Structs de los mensajes:** `src/shared/types.h` (`WorldSnapshot`, `LineStatusV2`, `Pose2D`, `Velocity2D`).
- **Framing / CRC:** `src/shared/proto.h`.
- **Contratos byte-a-byte:** `docs/firmware/CONTRATO-DATOS-TOP.md`, `CONTRATO-DATOS-CENTRAL.md`, `CONTRATO-DATOS-DOWN.md`, `CONTRATO-DATOS-CAMARAS.md`.
- **Cableado físico (Serial/pin/dirección):** `hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md`.
- **Qué doc manda por tema:** `docs/FUENTES-DE-VERDAD.md`.
- **Estado vivo del repo:** `docs/ESTADO-ACTUAL.md`.
