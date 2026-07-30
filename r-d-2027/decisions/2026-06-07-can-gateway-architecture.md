---
title: "Decisión — Arquitectura CAN troncal + ESP32 gateway (objetivo 2027)"
date: 2026-06-07
status: propuesta R&D 2027 (no aplicada)
scope: r-d-2027
related: [r-d-2027/specs/can-gateway-full-v1-spec.md, r-d-2027/decisions/2026-06-07-esp32-telemetry-bridge.md, r-d-2027/roadmap.md, docs/competencia/MEJORAS-PENDIENTES.md]
---

> **BANNER DE CORRECCIÓN (2026-07-30).** Este documento comparte los defectos de
> [`can-gateway-full-v1-spec.md`](../specs/can-gateway-full-v1-spec.md) — ver su banner. En una línea: elige **CAN-FD** contra la
> decisión congelada del equipo (**CAN clásico 1 Mbps**) y afirma que el **TWAI del ESP32-C6/S3
> hace CAN-FD**, lo cual es **falso** (es CAN 2.0B). El doc canónico del bus es
> [`2026-06-03-bus-can-general-y-flasheo-por-can.md`](../../docs/decisions/2026-06-03-bus-can-general-y-flasheo-por-can.md). Además el `WorldSnapshot` figura con
> 27 B cuando el código manda **31 B** (`src/shared/types.h:139`).

# Decisión — Arquitectura CAN troncal + ESP32 gateway

> R&D 2027. No aplica al robot de Incheon 2026.

## 1. Contexto

El robot actual usa **UART punto-a-punto** entre las 3 placas Teensy
(TOP↔CENTRAL, DOWN↔CENTRAL, DOWN↔TOP) con `proto.h` (CRC16, frame
`START|LEN|TYPE|SEQ|PAYLOAD|CRC|END`). Funciona y está testeado.

Limitaciones conocidas al crecer:
- **Cableado N×(N-1)/2** crece cuadrático: hoy 3 placas = 3 enlaces, OK; con
  4 placas serían 6.
- **No hay arbitración de prioridad nativa**: si llega un snapshot largo y
  enseguida una `LINE_URGENT` por el mismo cable, esta espera. Hoy se mitiga
  separando cables (LINE_URGENT por su propio UART), pero es ad-hoc.
- **Ruido EMI de motores**: los UART son sensibles a EMI; CAN está diseñado
  específicamente para entornos ruidosos (auto/industrial).
- **Sin canal externo**: para telemetría inalámbrica + inter-robot hay que
  bolt-on un ESP32 (FASE A).

Roadmap E4 (`docs/competencia/MEJORAS-PENDIENTES.md`) prevé:
**Bus CAN troncal + ESP32 gateway** para resolver todo lo de arriba de una.

## 2. Decisión propuesta

Migrar el transporte entre TOP/CENTRAL/DOWN de **UART → CAN-FD**, manteniendo
los **contratos de aplicación intactos** (`LineStatusV2`, `WorldSnapshot`,
`Pose2D`, etc. siguen siendo las mismas structs), y agregar un **4º nodo
ESP32 gateway** que puente selectivamente:
- al **laptop** (telemetría WiFi UDP / MQTT — estilo F1).
- al **robot compañero** (inter-robot via ESP-NOW).

```
                                  ┌─ ESP-NOW ─►  Robot compañero (gateway peer)
   ┌─────┐    ┌─────────┐    ┌────┴────┐
   │ TOP │────┤         ├────┤  ESP32  │── UDP / MQTT ─►  Laptop / Grafana
   └──┬──┘    │  CAN-FD │    │ gateway │
      │       │ 1 Mbps  │    └─────────┘
   ┌──┴───┐   │bus 2 hilos│
   │CENTRAL├──┤+ GND común├──── 120Ω terminator en cada extremo
   └──┬───┘   └─────────┘
      │
   ┌──┴──┐
   │ DOWN│
   └─────┘
```

## 3. Por qué CAN-FD (y no otras alternativas)

| Tecnología | Por qué descartada vs CAN-FD |
|---|---|
| UART punto-a-punto (lo de hoy) | Cableado N², sin arbitración por prioridad, sensible a EMI. Funciona para 3 placas, no escala. |
| RS-485 multidrop | Multidrop sí, pero no tiene arbitración por prioridad ni framing estándar; habría que reimplementar todo lo que CAN te da gratis. |
| SPI multi-slave | Requiere CS por slave → cableado N+1 desde un master. No es bus de pares. Bueno para sensores rápidos, mal para mensajería simétrica. |
| I²C | Bus, sí; pero 100/400 kbps típico (rápido sería 1 Mbps), pull-ups frágiles, sin CRC nativo, sin prioridades. |
| Ethernet (10/100) | Excesivo (procesador, PHY, latencia stack); demasiado HW/SW para 3 nodos. |
| CAN 2.0B clásico (8 B payload, 1 Mbps) | OK pero `LineStatusV2` (16 B) y `WorldSnapshot` (27 B) requieren fragmentar (ISO-TP). Complica el glue. |
| **CAN-FD (64 B payload, 1-5 Mbps data)** | **Elegido.** Soporta `LineStatusV2`/`WorldSnapshot` en **1 frame** sin fragmentación. Teensy 4.1 CAN3 lo tiene nativo. ESP32-C6/S3 con CAN-FD por TWAI. Transceptor TJA1051/MCP2562FD ($1). Arbitración por ID en HW, CRC fuerte, robusto a EMI. |

## 4. Mapa MsgType → CAN-ID (prioridades)

CAN arbitra por ID: menor ID gana. Asignación propuesta:

| Rango CAN-ID | Categoría | Origen → Destino | Notas |
|---|---|---|---|
| `0x010-0x01F` | **Safety / emergencia** | DOWN → ALL | `LineStatus URGENT` (16 B en 1 frame). |
| `0x020-0x02F` | Comando árbitro / override | gateway → ALL | Si el COMM se conecta al bus en lugar de UART. Opcional. |
| `0x030-0x03F` | World / sensores fusionados | TOP → CENTRAL | `WorldSnapshot` (27 B en 1 frame CAN-FD). |
| `0x040-0x04F` | Odometría / pose | DOWN → ALL | `Pose2D` / `Velocity2D`. |
| `0x050-0x05F` | Comando motor / estado robot | CENTRAL → ALL | Para que TOP/DOWN/gateway puedan "ver" qué hace. |
| `0x100-0x1FF` | **Telemetría sensores brutos** | TOP/DOWN → gateway | Anillo de 32 sensores, IMU raw, cámaras blobs. Baja prio. |
| `0x200-0x2FF` | **Inter-robot mensaje compartido** | gateway → exterior | Espejo de la `PartnerSnapshot`. |
| `0x300-0x3FF` | Diagnóstico / debug | cualquiera → gateway | Logs, stats de enlace, ping. |
| `0x7E0-0x7EF` | Bring-up / config | gateway → nodo específico | Cambiar parámetros en runtime. |

**El LineStatus URGENT está a `0x010-0x01F` (la prio más alta)** porque es
seguridad: si pasa al mismo tiempo que un WorldSnapshot, la línea ya está
arbitrando en el cable antes de que el snapshot termine. Eso es exactamente
lo que el robot necesita en una situación de borde.

## 5. Selectividad en el ESP32 gateway (las 3 listas blancas)

El usuario pidió explícitamente: **"no todo lo que se trafica en el bus tiene
que salir por el ESP32"**. La respuesta es **listas blancas configurables**:

```
ESP32 escucha CAN
    │
    ├─ filter_intra:     (vacía por default — el ESP32 no consume CAN)
    ├─ filter_telemetry: (qué reenvía a WiFi: por default 0x030, 0x040, 0x100-0x1FF)
    └─ filter_partner:   (qué reenvía por ESP-NOW: por default solo 0x200-0x2FF)
```

- **`LineStatus URGENT (0x010)` NUNCA sale del bus interno.** Es seguridad
  doméstica; no necesita ir al laptop ni al otro robot.
- **`PartnerSnapshot` viaja en `0x200-0x2FF`** — ese rango se publica al bus
  CAN, gateway lo captura y lo manda por ESP-NOW al robot compañero.
- **Las listas blancas se pueden modificar en runtime** desde la app PC
  (`SUB 0x101` / `UNSUB 0x101`). Para diagnóstico se abre más; para partido
  se cierra al mínimo.
- **Budget de tasa por ID**: cada ID tiene un `max_rate_hz` configurable. El
  gateway descarta lo que excede (el bus interno no, ahí pasa todo a su
  tasa real).

## 6. Inter-robot ESP-NOW (P2P)

- 2 robots, 2 ESP32 gateways pareados por MAC (`peer_mac` en config).
- Mensaje `PartnerSnapshot` ≈ 24 B: `{my_x, my_y, my_heading_cdeg, ball_x,
  ball_y, ball_vx, ball_vy, ball_conf, fsm_intent, timestamp_ms}`.
- 20 Hz nominal (latencia ~3-5 ms).
- CENTRAL ya tiene en `WorldSnapshot.flags`: `partner_alive`,
  `partner_sees_ball` → fusión inmediata cuando llegue.
- **Sin AP intermedio** (ESP-NOW es P2P al nivel del radio): no depende de la
  red del venue.

## 7. Telemetría "estilo F1" externa

- **UDP broadcast al laptop** (lo que hace la FASE A — se conserva tal cual).
- **Opcional MQTT** → broker + **Grafana + TimescaleDB**:
  - El gateway compila con `-DENABLE_MQTT` y publica a `iita/robot1/...`.
  - Permite **grabar entrenamientos enteros**, comparar runs ("la PID nueva
    es mejor o peor que la anterior"), ver trends.
  - Esto es el "data-driven loop" que acelera el ciclo de mejora del software
    al estilo Fórmula 1.

## 8. Consecuencias

### Ganamos
- Cableado interno simplificado (1 bus, 4 nodos).
- Arbitración por prioridad en HW → seguridad determinística.
- Tolerancia a EMI de motores.
- Canal externo unificado (telemetría + inter-robot).
- Selectividad fina (3 listas blancas + budget).

### Sacrificamos
- HW nuevo en cada placa (transceptor CAN + cable + terminador).
- Refactor del glue de comms (la lógica pura no cambia, pero el glue sí).
- Una decisión más por cada `MsgType`: ¿qué `CAN_ID` le doy? (resuelto con
  la tabla §4).

### Compatibilidad con la FASE A
- El ESP32 de la FASE A → en la FASE C **es el mismo ESP32**. Solo agrega un
  puerto CAN (TWAI o transceptor externo) en lugar de leer el UART de un
  Teensy. Su firmware UDP/MQTT/ESP-NOW se conserva.

## 9. Riesgos y mitigaciones

| Riesgo | Mitigación |
|---|---|
| El equipo nunca ha trabajado con CAN-FD en Teensy | Hacer prototipo banco con 2 Teensys + CAN simple (8 B) antes de comprometer al refactor. |
| Bug en el mapeo CAN-ID rompe la prioridad | Documentar la tabla en `specs/`. Test de banco que dispare `LineStatus URGENT` mientras se manda WorldSnapshot → verificar arbitración. |
| Listas blancas mal configuradas inundan el WiFi | Budget de tasa por ID + el gateway tiene un `MAX_OUT_HZ` global. |
| WiFi del venue mata la telemetría | No usar en partido (solo entrenamiento). |

## 10. Plan

1. Spec técnica detallada — `specs/can-gateway-full-v1-spec.md`.
2. Prototipo de banco (2 Teensy + CAN, mensaje simple) — fuera del repo
   principal, en `r-d-2027/code/can-prototype/` cuando se llegue.
3. Cuando se valide: branch `rd-2027-can-migration`, refactor del glue
   `comm_*.cpp` para usar CAN en lugar de Serial, mantener los contratos
   intactos.
4. Integración ESP32 gateway al bus.
5. Inter-robot y telemetría MQTT como features post-funcionamiento básico.

Esfuerzo total honesto: **3-6 semanas de equipo** (no 1 alumno solo). No es
trivial. **No hacerlo durante Incheon 2026 — solo después.**

## 11. Quién decide y cuándo

- **Propuesta:** Claude (Anthropic) a pedido de Gustavo, 2026-06-07.
- **Aprobación de la documentación R&D (este archivo + specs + stubs):**
  Gustavo, 2026-06-07.
- **Aprobación para empezar prototipo banco:** pendiente — necesita finalizar
  Incheon primero.
- **Aprobación para migrar el robot real:** pendiente — decisión post-Incheon
  con resultados del prototipo.
