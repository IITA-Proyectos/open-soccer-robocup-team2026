---
title: "Spec — CAN troncal + ESP32 gateway (v1)"
date: 2026-06-07
status: R&D 2027 (no aplicado)
scope: r-d-2027
related: [r-d-2027/decisions/2026-06-07-can-gateway-architecture.md, r-d-2027/specs/esp32-telemetry-bridge-v0-spec.md]
---

> ⛔ **BANNER DE CORRECCIÓN (2026-07-30) — NO USES ESTA SPEC PARA COMPRAR NI PARA RUTEAR.**
> Esta spec quedó **SUPERADA en tecnología**. El doc canónico del bus es
> [`docs/decisions/2026-06-03-bus-can-general-y-flasheo-por-can.md`](../../docs/decisions/2026-06-03-bus-can-general-y-flasheo-por-can.md):
> **CAN 2.0B clásico @ 1 Mbps**, transceptor **SN65HVD230 (3,3 V)**, mapa de IDs de su §3,
> flasheo por IDs de 29 bits.
>
> **Qué está mal y por qué:**
> - **El §1.3 es irrealizable.** Pone **CAN1 en TOP** (`:47`) y **CAN2 en DOWN** (`:49`) sobre un
>   bus que declara **CAN-FD** (`:11`, `:17`). En las Teensy 4.x (i.MX RT1062) **solo CAN3 hace
>   CAN-FD**; CAN1 y CAN2 son clásico. Dos de los tres nodos no pueden hablar el protocolo del
>   bus que esta spec define.
> - **El gateway tampoco es FD.** El **TWAI** de los ESP32 que propone (`:50`, `:152`:
>   WROOM-32 / C3 / C6) es CAN clásico, no FD.
> - **Las cámaras no están en el modelo.** Acá el bus tiene 4 nodos y la palabra "OpenMV" no
>   aparece **ni una vez**. Las OpenMV hablan **CAN clásico ≤1 Mbps** y por eso **fijan todo el
>   bus en clásico**. Un nodo clásico en un bus FD tira error frames hasta el bus-off.
> - **Derivados:** "16 B cabe en 1 frame, sin fragmentación" (`:137`) es falso en clásico y esta
>   spec **no define fragmentación**; el `WorldSnapshot` figura con **27 B** (`:84`) cuando el
>   código manda **31 B** (`src/shared/types.h:139`); el mapa de IDs del §2.1 **choca con el
>   canónico** (`0x100-0x1FF` y `0x300-0x3FF` con la semántica invertida); y prohibir los IDs de
>   29 bits (`:60-61`) deja **sin espacio al flasheo por CAN**, que es el núcleo del canónico.
>
> **Qué NO hacer:** ⛔ no comprar transceptores **CAN-FD** (MCP2562FD / TJA1051) — el bus va a
> ser clásico y el transceptor uniforme es el **SN65HVD230 de 3,3 V**. ⛔ No rutear el PCB 2027
> con el §1.3. ⛔ No soldar los pads traseros de una Teensy 4.0 para "llegar a CAN3": igual no
> va a haber FD.
>
> ⚠️ **Pendiente que NINGÚN doc resuelve (verificado contra el código, 2026-07-30):** en la placa
> **DOWN no queda ningún controlador CAN de borde libre** — CAN2 (pines 0/1) es el `Serial1`
> vivo a CENTRAL, y CAN1 (pines 22/23) son **A8/A9, las salidas de los muxes U3/U4**
> (`src/down/config_down.h:81`) → usarlos pierde 16 de los 32 sensores de línea. Queda solo
> CAN3 (30/31), que en la 4.0 son **pads traseros**. Decidirlo **antes** de cualquier PCB.
>
> **Lo que SÍ se salva:** el **§4** (gateway ESP32: listas blancas, budget por ID, UDP/MQTT,
> ESP-NOW) sobrevive entero al cambio a clásico. Los §6-§8 también, salvo la mención a Incheon
> (`:254`), que es historia.

# Spec — CAN troncal + ESP32 gateway v1

> Spec técnica del bus CAN-FD interno entre las 3 Teensy + 4º nodo ESP32 gateway
> al exterior. Objetivo 2027. Documento de R&D — no aplicado al firmware actual.

## 1. Capa física

### 1.1 Tecnología
- **CAN-FD** (CAN Flexible Data-Rate, ISO 11898-1:2015):
  - Arbitration bitrate: **500 kbps – 1 Mbps**.
  - Data bitrate: **2 – 5 Mbps** (durante el campo de datos, fuera de arbitración).
  - Payload máximo: **64 B/frame** (vs 8 B de CAN 2.0B).
- Tasa elegida v1: **1 Mbps arb / 2 Mbps data**. Conservador, sobra para 3-4 nodos.

### 1.2 Topología y cableado
```
                                        120Ω
                              ┌──────────────────┐
   ┌─────┐    ┌─────────┐    ┌────┴────┐    ┌─────────┐
   │ TOP │────┤ CENTRAL ├────┤  DOWN   ├────┤ ESP32 GW│
   └──┬──┘    └────┬────┘    └────┬────┘    └────┬────┘
   CAN_H/L      CAN_H/L         CAN_H/L        CAN_H/L
                  + GND común a TODO el bus
                              ─────┬─────
                                  120Ω
```

- Topología bus lineal, ramificaciones cortas (≤ 30 cm) si no se puede evitar.
- 2 hilos: **CAN_H + CAN_L** + GND común. Twisted pair recomendado para
  rechazo CMRR.
- **Resistor terminador 120 Ω** en CADA EXTREMO del bus (no en los nodos
  intermedios).
- Longitud total típica robot: ≤ 1 m → sin requisitos especiales.

### 1.3 Transceptores por placa

| Placa | Chip de CAN/TWAI | Transceptor recomendado | Costo unitario |
|---|---|---|---|
| TOP (Teensy 4.0) | FlexCAN (CAN1) | MCP2562FD | $1 |
| CENTRAL (Teensy 4.1) | FlexCAN3 (CAN-FD nativo) | MCP2562FD | $1 |
| DOWN (Teensy 4.0) | FlexCAN (CAN2) | MCP2562FD | $1 |
| ESP32 gateway | TWAI (ESP32 / C6) | MCP2562FD o TJA1051 | $1 |

**Alternativa más barata** (sin FD): MCP2551 / SN65HVD230 ($0.50 c/u) → CAN
2.0B clásico a 1 Mbps, payload 8 B → requiere fragmentación ISO-TP en SW para
mensajes >8 B. **No recomendado** salvo limitación HW.

## 2. Capa de mensajes — mapeo `MsgType` ↔ `CAN_ID`

### 2.1 Tabla de IDs

Convención: 11-bit standard ID (rango `0x000-0x7FF`). 29-bit extended NO se
usa en v1 (innecesario).

| Rango | Categoría | Frecuencia típica | Origen |
|---|---|---|---|
| `0x000-0x00F` | **reservado** (no asignar) | — | — |
| `0x010-0x01F` | safety / emergencia | hasta 200 Hz | DOWN |
| `0x020-0x02F` | árbitro / override | evento | gateway (vía COMM) |
| `0x030-0x03F` | mundo / fusión sensorial | 100 Hz | TOP |
| `0x040-0x04F` | pose / odometría | 100 Hz | DOWN |
| `0x050-0x05F` | comando motor / estado robot | 100 Hz | CENTRAL |
| `0x060-0x0FF` | reservado para nuevas categorías | — | — |
| `0x100-0x1FF` | telemetría sensores brutos | 20-50 Hz cada uno | TOP/DOWN |
| `0x200-0x2FF` | inter-robot espejado | 20 Hz | gateway |
| `0x300-0x3FF` | diagnóstico / debug | bajo | cualquiera |
| `0x400-0x7DF` | reservado | — | — |
| `0x7E0-0x7EF` | bring-up / config en runtime | evento | gateway |
| `0x7F0-0x7FF` | reservado | — | — |

### 2.2 IDs concretos propuestos (v1)

| CAN_ID | MsgType equivalente | Payload | Comentario |
|---|---|---|---|
| `0x010` | `LINE_URGENT` (LineStatusV2) | 16 B | bus emergencia, máxima prioridad |
| `0x030` | `WORLD_SNAPSHOT` | 27 B | TOP→CENTRAL principalmente |
| `0x040` | `DOWN_OTOS_POSE` (Pose2D) | 7 B | con padding hasta 8 B |
| `0x041` | `DOWN_OTOS_VEL` (Velocity2D) | 7 B | |
| `0x050` | `MOTOR_CMD` | 8 B | CENTRAL→motor server |
| `0x051` | `ROBOT_STATE` (FSM state, intent) | 8 B | espejo CENTRAL |
| `0x100` | telemetría DOWN raw (anillo 32 sensores) | 64 B (5 IDs si CAN 2.0) | banda baja |
| `0x101` | telemetría DOWN calib | 64 B | |
| `0x110` | telemetría TOP IMU raw | 24 B | |
| `0x111` | telemetría TOP cámaras blobs | 32 B | |
| `0x200` | `PartnerSnapshot` (inter-robot) | 24 B | gateway→exterior |
| `0x7E0` | config update (set RATE/FILTER) | variable | runtime config |

### 2.3 Garantías de prioridad

Por arbitración CAN: si dos nodos transmiten simultáneamente, **gana el de
menor ID**. Concretamente:
- `0x010` (LineStatus URGENT) **siempre** gana sobre `0x030` (WorldSnapshot)
  → seguridad determinística en HW.
- `0x100+` (telemetría) jamás bloquea a un mensaje de control.

Esto **resuelve por hardware** un problema que hoy se mitiga con UART
separados.

## 3. Glue por nodo

### 3.1 Lógica pura (no cambia)
Los módulos puros host-testeables (`comm_*_decoder` lógicos, contratos
`types.h`, decisiones de strategy/safety) **no se tocan**. Siguen siendo la
única fuente de verdad host-testeada.

### 3.2 Glue de transporte (cambia)
La capa de transporte cambia de `Serial.write(proto_encode(frame))` a
`can_send(can_id, payload, len)`.

Ejemplo conceptual (DOWN → CENTRAL emitiendo `LineStatusV2`):

**Hoy (UART)**:
```cpp
// down/comm_central.cpp (versión actual, NO tocar)
void comm_central_send_line_urgent_v2() {
    LineStatusV2 s = dm_update(...);
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = down_encode_line(s, seq++, buf, sizeof buf);
    Serial1.write(buf, n);
}
```

**Mañana (CAN)** (concepto, NO aplicado):
```cpp
// down/comm_central_can.cpp (FASE B/C)
void comm_central_send_line_urgent_v2_can() {
    LineStatusV2 s = dm_update(...);
    can_send_fd(CAN_ID_LINE_URGENT_V2, (uint8_t*)&s, sizeof s);
    // 16 B cabe en 1 frame CAN-FD; sin fragmentación.
}
```

CRC ya viene en HW de CAN — `proto.h`'s CRC16 ya no es necesario al nivel CAN
(redundante; opcionalmente se conserva una versión "envoltorio" para
robustez extra).

### 3.3 Recepción
Cada nodo se suscribe a los CAN_IDs que le interesan vía filtro de hardware
(FlexCAN/TWAI tienen filtros nativos: máscara + ID). Reduce carga de CPU.

## 4. ESP32 gateway

### 4.1 Hardware
- ESP32-WROOM-32 / C3 / C6 con transceptor CAN externo (MCP2562FD).
- WiFi (incorporado).
- ESP-NOW (incorporado, comparte radio con WiFi).
- 4 LEDs de diagnóstico: link CAN, link WiFi, ESP-NOW peer, actividad.

### 4.2 Software — núcleo de filtrado
3 listas blancas independientes con budget por ID:

```cpp
struct FilterEntry {
    uint16_t can_id;
    uint16_t max_rate_hz;  // 0 = sin límite
    uint32_t last_tx_ms;
};
FilterEntry filter_telemetry[N_MAX];  // → WiFi
FilterEntry filter_partner[N_MAX];    // → ESP-NOW
FilterEntry filter_intra[N_MAX];      // → uso interno del ESP32 (raro)
```

Loop:
```cpp
while (can_rx(&frame)) {
    if (matches(filter_telemetry, frame.id) && rate_ok(...)) {
        wifi_udp_send(frame);    // o mqtt_publish
    }
    if (matches(filter_partner, frame.id) && rate_ok(...)) {
        espnow_send(peer, frame);
    }
    if (matches(filter_intra, frame.id)) { /* uso local */ }
}
```

### 4.3 Configuración en runtime
- `SUB 0x100 RATE 20`: agregar `0x100` a `filter_telemetry` con 20 Hz max.
- `UNSUB 0x100`: quitar.
- `LIST`: mostrar listas blancas vigentes.
- Comandos por UDP desde la app PC, o por CAN (ID `0x7E0`) desde otro nodo.

### 4.4 Telemetría WiFi modos
- **UDP broadcast (default)**: cero infraestructura. Cualquier laptop en la
  red lo recibe. El payload es el frame CAN serializado a un struct simple
  `{can_id, len, data}` empaquetado.
- **MQTT (opcional, `-DENABLE_MQTT`)**: publish a `iita/robot{1,2}/<can_id_hex>`.
  El cliente recibe y graba.

### 4.5 Inter-robot ESP-NOW

```cpp
typedef struct __attribute__((packed)) {
    int16_t my_x_mm, my_y_mm, my_heading_centideg;
    int16_t ball_x_mm, ball_y_mm;
    int16_t ball_vx_mm_s, ball_vy_mm_s;
    uint8_t ball_confidence;
    uint8_t fsm_intent;
    uint32_t timestamp_ms;
} PartnerSnapshot;  // ~24 B
```

- Pareo por MAC al boot: `peer_mac` en `config.h`.
- TX: 20 Hz (al recibir un `0x200-0x2FF` del bus CAN).
- RX: cuando llega un `PartnerSnapshot`, el gateway lo publica al bus CAN
  con un CAN_ID dedicado (`0x210` "partner snapshot recibido") que CENTRAL
  consume.

## 5. Migración desde la FASE A

El ESP32 de la FASE A es el MISMO chip que el gateway de la FASE C. Solo se
le agrega:
- Conexión CAN (transceptor MCP2562FD + 2 pines TWAI).
- Firmware del bus CAN (driver TWAI) en lugar/además del UART.

Todo lo que la FASE A ya construyó (WiFi UDP, comandos PC, ESP-NOW stub) se
conserva. La transición es **incremental, no disruptiva**.

## 6. Plan de implementación (cuando se decida)

| Paso | Quién | Esfuerzo |
|---|---|---|
| 1. Prototipo CAN básico: 2 Teensy + MCP2562FD + mensaje "hello" | Virginia o Elías | 2-3 días |
| 2. Mapeo `MsgType` ↔ `CAN_ID` documentado y aprobado | Coach | 1 día |
| 3. Glue `comm_*_can.cpp` por nodo | Virginia | 1 semana |
| 4. Cableado real en el robot | Elías | 1 día |
| 5. Banco: bus completo (3 Teensy hablando) | equipo | 2-3 días |
| 6. ESP32 gateway con CAN + WiFi + filtros | Virginia | 1 semana |
| 7. ESP-NOW pair entre 2 gateways | Virginia | 2-3 días |
| 8. MQTT + Grafana opcional | post-funcional | 1 semana |

**Total honesto: ~3-6 semanas de equipo.** No es un fin de semana.

## 7. Tests

- Banco: dos Teensy + un osciloscopio en CAN_H/L verifican el diferencial.
- Software: el gate host actual no testea CAN (no aplica al gate de
  competencia), pero los **decoders/encoders de payload** sí son los mismos
  módulos puros que hoy testean los frames `proto.h`. Cero deuda nueva.
- Stress: 1 Mbps arb / 2 Mbps data sostenido con 5 IDs a 100 Hz + 10 IDs a
  20 Hz → 0 errores en 5 min.

## 8. Riesgos

| Riesgo | Mitigación |
|---|---|
| Aprender CAN-FD desde cero antes de Incheon | NO. Solo después. |
| Conflicto de IDs entre nodos | Tabla §2 documentada + autoridad: ningún nodo asigna IDs sin actualizar el doc. |
| Bus saturado | Budget por ID + tasas conservadoras (100 Hz top). |
| ESP-NOW colisiona con WiFi | Ambos comparten radio 2.4 GHz, pero ESP-NOW es resistente. Si hay problemas, considerar ESP32-C6 que tiene 802.15.4 separado para inter-robot. |
| El equipo prefiere quedarse con UART | Es válido. CAN no es obligatorio; UART funciona. CAN da escalabilidad y robustez para crecer. |

## 9. Decisión

Esta spec describe el **estado objetivo**. Su ejecución requiere una
**decisión explícita del coach** post-Incheon, con prototipo de banco
validado antes de comprometer al refactor.
