# r-d-2027 — Roadmap de fases

> **BANNER DE CORRECCIÓN (2026-07-30).** La **Fase B (bus CAN)** de este roadmap comparte los
> defectos de [`specs/can-gateway-full-v1-spec.md`](specs/can-gateway-full-v1-spec.md) — ver su
> banner. En una línea: elige **CAN-FD** contra la decisión congelada del equipo (**CAN clásico
> 1 Mbps**) y da por hecho que el **TWAI del ESP32 hace CAN-FD**, lo cual es **falso**. El doc
> canónico del bus es
> [`2026-06-03-bus-can-general-y-flasheo-por-can.md`](../docs/decisions/2026-06-03-bus-can-general-y-flasheo-por-can.md).
> Además el `WorldSnapshot` figura con 27 B cuando el código manda **31 B** (`src/shared/types.h:139`).


> Mapa de cómo evoluciona la comunicación entre placas y al exterior, desde lo
> que hay HOY (Incheon 2026) hasta el objetivo 2027. Pensado para que cada paso
> agregue valor sin obligar a hacer el paso siguiente — se puede frenar en
> cualquier fase sin perder lo construido.

## TL;DR

```
HOY (Incheon 2026)         FASE A (opcional 2026)        FASE B (2026/27)        FASE C (2027)
─────────────────────      ──────────────────────        ──────────────         ──────────────
UART punto-a-punto         + ESP32 puente WiFi            Bus CAN troncal         CAN + ESP32 gateway
proto.h frames             para telemetría/calib           reemplaza UART          + inter-robot ESP-NOW
Telemetría USB CDC         (reutiliza schema USB           (3 Teensy + 1 ESP32     + telemetría F1
JSON Lines v1              actual, byte-idéntico OFF)      sobre CAN-FD)           (MQTT/Grafana)
```

Cada fase **conserva** lo que la anterior construyó. La FASE A es un
**escalón cómodo y barato** que entrega valor inmediato y deja el nodo ESP32
que mañana será el gateway de la FASE C.

## Fase 0 — Estado actual (Incheon 2026)

- UART entre TOP/CENTRAL/DOWN con `proto.h` (CRC16, frame `START|LEN|TYPE|SEQ|PAYLOAD|CRC|END`).
- Contratos: `LineStatusV2` (DOWN→CENTRAL, 16 B), `WorldSnapshot` v2 (TOP→CENTRAL, 27 B con `ball_vx/vy`).
- Telemetría USB CDC con JSON Lines schema v1, gateado byte-idéntico OFF
  (`telemetry_down.cpp` puro + golden test + `down_telemetry_serial.cpp` glue + `tools/monitor-base`).
- COMM = ESP32-C6 con firmware oficial RCJ para árbitro (BLE) — **NO se toca**.
- Sin comunicación robot-a-robot.

**Decisión vigente:** `docs/decisions/2026-05-17-comunicacion-inter-robot-superteam.md`
→ Opción A (NO implementar inter-robot para Incheon, robot autónomo + módulo árbitro).

## Fase A — ESP32 Telemetry Bridge v0 (OPCIONAL, ejecutable en semanas)

**Qué hace:** un ESP32 chico ($5) por robot escucha por UART el MISMO stream
JSON Lines que hoy va por USB y lo reenvía por **UDP broadcast a la subred
WiFi del laptop**. Bidireccional: el laptop también manda comandos por UDP
que el ESP32 reenvía por UART (calibración remota, etc.).

**Qué entrega:**
- **Telemetría sin cable** durante puesta a punto / entrenamiento.
- **Calibración remota** (carpet/blanco/auto/save) sin estar al lado del robot.
- **Stub ESP-NOW** pair-to-pair listo para inter-robot (vacío hoy, listas
  blancas se llenan cuando se decida compartir).

**Qué NO toca:**
- Cero cambio en `telemetry_down.cpp` puro (mismo schema, mismo golden).
- Cero cambio en `monitor_base` Python (le agregamos `--udp` adicional al `--serial`).
- Cero cambio en el binario de competencia (gate `-DENABLE_TELEMETRY_ESP32` OFF
  por default).

**Esfuerzo:** ~2-3 días de software + $5-10 HW por robot.

**Spec:** `specs/esp32-telemetry-bridge-v0-spec.md`.
**Código stub:** `code/esp32-bridge-firmware/` + `code/pc-udp-listener/`.

**Riesgo:** muy bajo. Si no funciona, se desenchufa el ESP32 y el robot vuelve
a estar idéntico.

**Por qué ESTO ANTES que la FASE B/C:**
- 100% reutilizable como nodo gateway del CAN futuro (solo cambia de qué LEE).
- Valida el flujo *sensor → red → laptop* sin tocar el bus interno del robot.
- Acelera la puesta a punto del software de Incheon SIN comprometer el binario.

## Fase B — CAN troncal interno (objetivo 2026/27)

**Qué cambia:** el transporte entre TOP/CENTRAL/DOWN deja de ser UART
punto-a-punto y pasa a ser **CAN-FD a 1 Mbps**.

**Por qué CAN-FD:**
- Bus multi-drop natural (3 Teensy en el mismo cable) → menos cableado.
- Arbitración por prioridad de ID en hardware → la LineStatus URGENT
  literalmente se gana al snapshot en el medio (no hay starvation).
- Tolerancia a ruido EMI mucho mayor que UART (los motores meten ruido).
- CAN-FD soporta payload hasta 64 B → `LineStatusV2` (16 B) cabe en 1 frame,
  `WorldSnapshot` (27 B) cabe en 1 frame. Cero fragmentación.

**Hardware:** Teensy 4.0/4.1 tienen FlexCAN integrado (CAN3 en 4.1 es FD).
1 transceptor por placa (MCP2562 o SN65HVD230, ~$0.50 c/u). Resistor 120Ω
en cada extremo del bus. 2 hilos (CAN_H/CAN_L) + GND.

**Software:** las capas puras (`comm_*` decoders, contratos, frames) son
transport-agnostic. Solo cambia el glue de bajo nivel:
```
hoy:    Serial1.write(proto_encode(frame))
mañana: can_send(can_id_from_msgtype(frame.type), frame.payload, frame.payload_len)
```

**Decisión que requiere antes de ejecutar:** mapeo `MsgType` ↔ `CAN_ID`
documentado y aprobado (ver spec).

**Esfuerzo:** ~1 semana firmware + 1 día electrónica.

**Spec:** `specs/can-gateway-full-v1-spec.md` §3.

## Fase C — ESP32 Gateway completo (objetivo 2027)

**Qué agrega:** el ESP32 de la FASE A se conecta también al bus CAN como 4º
nodo. Mantiene **3 listas blancas** independientes:

```
                       ┌─ filter_intra:     CAN → firmware propio (raro)
ESP32 escucha CAN ─────┼─ filter_telemetry: CAN → WiFi (UDP/MQTT al laptop)
                       └─ filter_partner:   CAN → ESP-NOW (al otro robot)
```

**Inter-robot (ESP-NOW P2P):**
- Pareo por MAC al boot, 2 robots simétricos.
- `PartnerSnapshot` compacto (~24 B): mi pose + pelota relativa + intención
  FSM + confianza + timestamp.
- 20 Hz, latencia ~3-5 ms, sin AP.
- CENTRAL ya tiene `partner_alive`/`partner_sees_ball` en `WorldSnapshot.flags`
  → integración trivial.

**Telemetría F1:**
- UDP broadcast → laptop (cero broker, lo que ya hace FASE A).
- Opcional: MQTT → broker → **Grafana + TimescaleDB** para grabar
  entrenamientos y comparar runs.
- Listas blancas modificables en runtime (`SUB <id>` / `UNSUB <id>` desde la
  app PC) para diagnosticar más o ahorrar budget en partido.

**Esfuerzo:** ~2 semanas SW + tarjeta gateway custom (o ESP32-DevKit con
breakout CAN).

**Spec:** `specs/can-gateway-full-v1-spec.md` §4-§6.

## Dependencias y orden recomendado

```
Fase A (ESP32 bridge)  ──┐
                         ├──► Fase C (ESP32 gateway completo)
Fase B (CAN troncal)   ──┘
```

- **A y B son independientes.** Se pueden hacer en cualquier orden o en
  paralelo. A no necesita CAN. B no necesita ESP32.
- **C necesita A + B** (es la unión: el ESP32 ya gateway-listo + el bus CAN ya
  funcionando).
- Si solo se hace A: hay telemetría WiFi y opcional ESP-NOW, pero el bus
  interno sigue siendo UART.
- Si solo se hace B: el bus interno es más robusto, pero la telemetría sigue
  por USB CDC.
- A+B+C: arquitectura completa 2027.

## Qué NO entra en r-d-2027 (otros temas)

- Hardware mecánico (chasis 2027, dribbler, kicker activo): otro proyecto, no
  acá.
- Visión avanzada (depth cameras, ML): otro proyecto, no acá.
- Estrategia avanzada / coordinación SuperTeam táctica (¿quién va por la
  pelota?, asignación dinámica de roles): se construye SOBRE el canal
  inter-robot de la FASE C, pero su diseño es otro paquete.

---
*Última actualización: 2026-06-07.*
