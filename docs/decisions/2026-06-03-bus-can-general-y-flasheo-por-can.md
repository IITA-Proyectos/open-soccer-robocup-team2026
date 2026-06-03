---
title: "Bus general CAN + protocolo de flasheo de firmware por CAN (propuesta)"
date: 2026-06-03
author: "Claude (Anthropic - Claude Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M, Anthropic)"
status: propuesta
tags: [comunicacion, can, can-fd, bus, flasheo, firmware, flasherx, openmv, telemetria, electronica, propuesta, ambos]
robot: ambos
area: comunicacion
tipo: propuesta
related: [docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md, docs/decisions/2026-05-17-comunicacion-inter-robot-superteam.md, hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md, docs/MAPA-DE-DATOS.md]
---

# Bus general CAN + flasheo de firmware por CAN — PROPUESTA

> ⚠️ **BANNER — leer antes de usar esto como guía.**
> Este documento es una **PROPUESTA de arquitectura objetivo (candidata post-Incheon)**,
> **NO** describe lo que corre hoy. El transporte inter-placa **vivo y canónico**
> sigue siendo el de
> [`2026-05-18-diseno-comunicaciones-robusto-definitivo.md`](2026-05-18-diseno-comunicaciones-robusto-definitivo.md)
> (UART punto-a-punto con `proto.h`). **Nada de este doc se implementa antes de
> Incheon.** La migración a CAN es **aditiva** y **no rompe** el sistema UART:
> las **capas fail-safe 0–6** de aquel diseño **se preservan tal cual**; CAN solo
> reemplaza la **capa 1 (física)** y la **capa 2 (framing)**.

## 0. Alcance y motivación

**Qué resuelve.** El transporte actual es UART **punto a punto** (mapa de pines
S1…S7 por placa). Eso es frágil de escalar: con 3 placas ya hay un mapa de pines
delicado, y **sumar 4 cámaras OpenMV + un bridge de telemetría + (opcional) LiDAR
requiere más UARTs de los que físicamente existen** (Teensy 4.0 = 7 serials,
4.1 = 8, ya casi todos usados). Un **bus** rompe esa explosión combinatoria:
todos los nodos cuelgan de **2 hilos** y cada uno es un nodo con ID.

**Qué NO cambia.** La filosofía *fail-safe by default*, la taxonomía
STREAM/EVENTO/COMANDO, el heartbeat + FSM con histéresis y la escalera de
recuperación (WDT → reset por comando → reset por HW). Todo eso vive **encima**
del transporte y se mantiene idéntico.

**Contexto de competencia (RoboCup Jr Soccer Open).** El reglamento 2026
(reglas 1.3.1 y 3.2) permite comunicación **robot↔robot** solo en **2.4 GHz a
≤100 mW EIRP** y **prohíbe remote control** en partido. ⇒ **La telemetría a PC
es una herramienta de banco/práctica, no una feature de partido.** El robot debe
ser 100 % autónomo con la RF apagada. El bridge RF (ESP32-S3) debe ser
**desconectable por software** y el flasheo por bus **nunca** ocurre en partido.

---

## 1. Decisión propuesta (resumen)

1. **Bus físico:** un **CAN 2.0B clásico @ 1 Mbps**, 2 hilos diferenciales
   (CANH/CANL) + GND, terminado en **120 Ω** en los dos extremos.
2. **Nodos:** TOP, CENTRAL, DOWN (FlexCAN nativo de las Teensy 4.x), 4× OpenMV
   (CAN ≤1 Mbps por su Arduino Interface Library), y un **bridge ESP32-S3**
   (TWAI nativo) que esnifa todo el bus y telemetrea por WiFi/ESP-NOW.
3. **LiDAR (opcional):** UART dedicado a UNA placa; esa placa publica al bus solo
   el **dato derivado** (obstáculo/pared), no la nube de puntos cruda.
4. **Conectores:** **JST-GH 1.25 mm de 4 pines** (CANH, CANL, GND, +V) en
   daisy-chain. Transceiver **SN65HVD230** (3.3 V) por nodo.
5. **Flasheo por el bus:** protocolo propio sobre CAN (§9) con **FlasherX** del
   lado Teensy, gateado por modo mantenimiento, con integridad en 3 capas
   (CRC de frame por HW + CRC16 por bloque + CRC32 de imagen) y **USB siempre
   como recovery**.
6. **Upgrade futuro (no v1):** backbone **CAN-FD** solo entre Teensy (CAN3, 64 B
   por frame, 2–5 Mbps) para meter el `WorldSnapshot` en **un** frame sin
   segmentar. Las cámaras quedan en su bus clásico.

**Por qué CAN y no otra cosa** (resumen; el estudio completo de alternativas
—RS-485, SPI, I²C, 10BASE-T1S, Ethernet, EtherCAT— está en el journal asociado):

- **SIMPLE:** un bus de 2 hilos para todo; agregar un nodo = enchufarlo.
- **CONFIABLE:** diferencial (inmune al ruido de los motores), con **arbitraje,
  CRC y reintento por hardware** y aislamiento de errores (bus-off) — el nodo que
  falla se auto-desconecta sin tumbar el bus.
- **POTENTE:** CAN es **broadcast** → un solo nodo bridge captura **TODO** el
  tráfico del bus para telemetría, sin tocar a nadie.
- **Reusa tu hardware:** las Teensy ya traen los controladores CAN; solo agregás
  un transceiver de ~1 USD por placa.

---

## 2. Diseño físico del bus

### 2.1 Topología
```
        ┌──────── BUS CAN clásico 2.0B @ 1 Mbps (CANH/CANL trenzados + GND) ────────┐
     [120Ω]                                                                      [120Ω]
   ┌───┴───┐ ┌───────┐ ┌───────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌────────────┐
   │ TOP   │ │CENTRAL│ │ DOWN  │ │CAM 1 │ │CAM 2 │ │CAM 3 │ │CAM 4 │ │ ESP32-S3   │
   │ id=1  │ │ id=2  │ │ id=3  │ │ id=4 │ │ id=5 │ │ id=6 │ │ id=7 │ │ BRIDGE id=8│
   │+xcvr  │ │+xcvr  │ │+xcvr  │ │+xcvr │ │+xcvr │ │+xcvr │ │+xcvr │ │+xcvr (TWAI)│
   └───────┘ └───┬───┘ └───────┘ └──────┘ └──────┘ └──────┘ └──────┘ └─────┬──────┘
                 │ (UART dedicado, opcional)                                │ WiFi/ESP-NOW
              ┌──┴───┐                                                      ▼
              │LiDAR │ id=9 (gateway) → publica obstáculo derivado al bus  PC (dongle)
              └──────┘
```
- **Terminación 120 Ω solo en los DOS extremos físicos** de la línea, no en cada
  nodo. Los nodos del medio cuelgan con **stubs cortos (<30 cm)**.
- **GND común** referenciado entre todas las placas (ya existe en el robot).
- **Trenzar el par CANH/CANL** es lo que da el rechazo de ruido — no es opcional.

### 2.2 Componentes concretos
| Rol | Parte | Nota |
|---|---|---|
| Controlador CAN (Teensy) | **FlexCAN** nativo (CAN1/CAN2 clásico; CAN3 FD-capaz) | lib `FlexCAN_T4` o `ACAN-T4` |
| Controlador CAN (bridge) | **TWAI** del ESP32-S3 | clásico, perfecto para 1 Mbps |
| Transceiver (todos) | **SN65HVD230** (3.3 V, clásico, ≤1 Mbps) | uniforme para Teensy/ESP32/OpenMV |
| (si CAN-FD futuro) | TCAN332 / MCP2562FD / TJA1051 | FD-rated en la fase de datos |
| Conector | **JST-GH 4 pin** (CANH/CANL/GND/+V) | estándar drones, con traba, anti-vibración |
| Cable | par trenzado fino + GND | trenzado obligatorio |

---

## 3. Mapa de IDs CAN — tráfico normal (11-bit, estándar)

En CAN **menor ID = mayor prioridad** (gana el arbitraje). Se reservan rangos por
criticidad. Esto reemplaza el `TYPE` de `proto.h` por el **ID de arbitraje** de CAN
(y permite que la seguridad gane siempre la pelea por el bus):

| Rango ID | Clase | Prioridad | Ejemplos (mensajes vivos hoy) |
|---|---|---|---|
| `0x000–0x00F` | **Seguridad / emergencia** | máxima | `EMERGENCY_STOP` (broadcast), árbitro STOP |
| `0x010–0x0FF` | **Eventos de borde (EVENTO)** | muy alta | `imminent_exit` (DOWN→todos, latch) |
| `0x100–0x1FF` | **Comandos** | alta | `CENTRAL_RESET_OTOS`, `CENTRAL_CALIB_LINE`, `RESET_TOP` |
| `0x200–0x2FF` | **Streams de control** | media | `WORLD_SNAPSHOT` (TOP), `LINE_STATUS_V2`/`POSE2D`/`VEL2D` (DOWN) |
| `0x300–0x3FF` | **Visión** | media-baja | blobs de cada cámara (`0x300`+cam_id) |
| `0x400–0x4FF` | **Heartbeat / salud** | baja | `HEARTBEAT` por nodo (`0x400`+node_id), contadores |
| `0x500–0x6FF` | **Telemetría no crítica** | baja | snapshots de debug agregados para el bridge |
| **IDs extendidos (29-bit)** | **Flasheo de firmware** | **mínima** | §9 — siempre cede ante cualquier frame estándar |

> **Por qué el flasheo usa IDs extendidos:** un frame extendido pierde el
> arbitraje contra cualquier frame estándar con base equivalente ⇒ el flasheo
> **jamás** roba ancho de banda al control. (Además, en la práctica se flashea con
> el robot en **mantenimiento**, sin tráfico de control.)

### 3.1 Segmentación de mensajes > 8 bytes
CAN clásico transporta **8 B por frame**. El `WorldSnapshot` v2 son **27 B**
(ver `CONTRATO-DATOS-CENTRAL.md`), así que se parte en **multi-frame**:

- Convención mínima: **primer byte de cada frame = `(seq<<4)|total)`** (seq 0..N,
  total de frames), 7 B de payload útil ⇒ 27 B → **4 frames**.
- El receptor reensambla por `(ID, seq)`; si falta un seq antes de completar, se
  descarta el snapshot (es STREAM: el siguiente lo reemplaza — misma política que
  hoy). **No** se agrega ACK: a tasa de stream, el próximo cubre la pérdida.
- **Alternativa limpia (upgrade):** **CAN-FD** en el backbone Teensy mete los 27 B
  en **un solo frame de 64 B** → cero segmentación. Las cámaras (clásico) quedan
  en bus aparte o se mantiene todo clásico con multi-frame. Para v1 se recomienda
  **todo clásico con multi-frame** (más simple de cablear); CAN-FD es fase 2.

---

## 4. Integración de visión (OpenMV) y LiDAR

- **OpenMV** exponen **CAN ≤1 Mbps** (Arduino Interface Library). Cada cámara es
  **un nodo más** que publica **coordenadas/blobs** (pelota, arcos, líneas) en
  `0x300`+cam_id — **nunca frames JPEG**. La imagen cruda solo se usa para *debug
  visual* y va por **USB (banco)** o **WiFi del bridge (práctica)**, fuera del bus.
- **Regla de oro:** por el bus viaja la **conclusión** de la visión, no la imagen.
  El procesamiento LAB lo hace la cámara a bordo (para eso es inteligente).
- **LiDAR**: UART dedicado a una placa (firehose de ~20–50 KB/s); esa placa
  filtra y publica al bus solo el **obstáculo derivado**. Nunca el scan crudo.

---

## 5. Cómo se preservan las capas fail-safe (0–6)

El [diseño vivo](2026-05-18-diseno-comunicaciones-robusto-definitivo.md) define 7
capas. CAN **cambia 2 y conserva 5**:

| Capa | Hoy (UART) | Con CAN | Cambia |
|---|---|---|---|
| 0. Loop no-bloqueante | igual | igual | NO |
| **1. Física** | trenzado/GND, ruteo lejos de PWM | **diferencial CAN + 120 Ω** (mejor) | **SÍ** |
| **2. Framing** | `proto.h` (`0xAA…CRC16…0x55`) | **frame CAN nativo** (CRC15 HW) + multi-frame | **SÍ** |
| 3. Enlace (heartbeat+FSM histéresis) | igual | igual (`HEARTBEAT` ahora es un ID CAN) | NO |
| 4. Taxonomía STREAM/EVENTO/COMANDO | igual | igual (mapea a rangos de ID) | NO |
| 5. Estado seguro / arranque / observabilidad | igual | igual | NO |
| 6. Recuperación (WDT→cmd→HW) | igual | igual (+ bonus: flasheo por bus, §9) | NO |

El módulo `Link` (TASK-020) que implementa las 3 políticas de recepción sigue
siendo el mismo; cambia solo su *driver* de transporte (de `Serial` a un wrapper
CAN). El CRC16 de `proto.h` se **reusa** como CRC de bloque en el flasheo (§9.7).

---

## 6. Presupuesto de ancho de banda y latencia

- **Techo CAN clásico 1 Mbps:** ~7.700 frames de 8 B/s ⇒ ~60–70 KB/s de payload.
- **Uso estimado** (4 cámaras @ 60 fps coordenadas + control inter-placa + HB):
  ~1.000–2.000 frames/s ⇒ **15–25 % del bus**. Latencia del frame de mayor
  prioridad: **<1 ms**. Holgado.
- **WorldSnapshot** 27 B @ ~50–100 Hz = 4 frames × (50–100) = 200–400 frames/s.
- Si alguna vez queda corto ⇒ **CAN-FD** (5–8 Mbps, 64 B/frame) en el backbone.

---

## 7. Protocolo de flasheo de firmware por CAN  ⭐ (núcleo del pedido)

> **Objetivo:** reprogramar cualquier Teensy del bus **desde una sola conexión**
> (el bridge ESP32-S3, alimentado por la laptop vía WiFi/USB), sin desarmar el
> robot — y opcionalmente **inalámbrico**. Robusto, abortable, **anti-brick**, con
> **USB siempre como recovery**.

### 7.1 Principios y precondiciones de seguridad (no negociables)
1. **Solo en MANTENIMIENTO.** Un nodo acepta flasheo **únicamente** si:
   `match_running == false` **Y** motores deshabilitados **Y** recibió un
   `UNLOCK` con clave mágica (gate anti-accidente; p.ej. dip-switch o secuencia).
   En cualquier otra condición responde `NAK(BUSY/UNSAFE)`.
2. **Nunca en partido.** El flasheo vive en IDs extendidos de mínima prioridad y
   bajo el gate anterior. No puede arrancar con el robot jugando.
3. **USB es el recovery.** Un flasheo fallido puede dejar la placa muda hasta
   reflashear por USB (HalfKay). El USB de cada placa debe quedar **accesible**
   (pigtail/hub). El bus *no reemplaza* al USB: elimina su uso **cotidiano**.
4. **El que recupera no depende del que falla** (mismo principio que la capa 6).
5. **Unicast.** Se flashea **un** nodo por vez (broadcast solo para `QUERY`).

### 7.2 Direccionamiento — IDs extendidos (29-bit)
```
 bits [28:21] (8)  = 0xFA            ; "magic" de flasheo (alto ⇒ baja prioridad)
 bits [20:16] (5)  = target_node_id  ; 1..31 (0 = broadcast solo para QUERY)
 bits [15: 8] (8)  = source_node_id  ; el programmer (bridge = 8)
 bits [ 7: 0] (8)  = opcode          ; tabla §7.3
```
`NODE_ID`: 1=TOP, 2=CENTRAL, 3=DOWN, 4..7=CAM1..4, 8=BRIDGE/PROGRAMMER, 9=LIDAR_GW.

### 7.3 Opcodes / tipos de frame

**Programmer → Target** (`0x01`–`0x7F`):
| Op | Nombre | Payload (≤8 B) | Significado |
|---|---|---|---|
| `0x01` | `QUERY` | — | ¿estás? versión de app / board (broadcast ok) |
| `0x05` | `UNLOCK` | `key[4]` | habilita modo mantenimiento (gate anti-accidente) |
| `0x10` | `BEGIN` | `size[4]`, `crc32[4]` | inicia sesión: tamaño total + CRC32 de la imagen |
| `0x21` | `BLOCK_BEGIN` | `blk_idx[2]`, `blk_len[2]` | empieza bloque (≤256 B = ≤32 frames DATA) |
| `0x22` | `DATA` | `bytes[1..8]` | datos crudos del bloque, en orden |
| `0x23` | `BLOCK_END` | `blk_idx[2]`, `crc16[2]` | cierra bloque: CRC16/CCITT del bloque |
| `0x30` | `END` | `n_blocks[2]` | fin de transferencia: pide verificar CRC32 y commitear |
| `0x3F` | `ABORT` | `reason[1]` | cancela: descartar buffer, volver a estado seguro |

**Target → Programmer** (`0x80`–`0xFF`):
| Op | Nombre | Payload | Significado |
|---|---|---|---|
| `0x80` | `STATUS` | `state[1]`, `code[1]`, `app_ver[…]` | respuesta a `QUERY` |
| `0x81` | `READY` | `free_flash[4]` | buffer borrado, listo para recibir |
| `0x82` | `ERASING` | `progress[1]` | keepalive durante el borrado (puede tardar s) |
| `0x83` | `BLOCK_ACK` | `blk_idx[2]` | bloque recibido y CRC16 OK |
| `0x84` | `BLOCK_NAK` | `blk_idx[2]`, `reason[1]` | reenviar el bloque (CRC/overflow/orden) |
| `0x85` | `END_ACK` | — | CRC32 de imagen OK → va a commitear |
| `0x86` | `REBOOTING` | — | aplicando (FlasherX) y reiniciando |
| `0x8F` | `ERROR` | `code[1]` | error fatal (unsafe/oom/crc32-fail/timeout) |

### 7.4 Handshake (camino feliz)
```
PROGRAMMER                                   TARGET (Teensy + FlasherX)
   │  UNLOCK(key) ───────────────────────────►│ valida match=off, motores off
   │  ◄───────────────────────────── STATUS   │
   │  BEGIN(size, crc32) ────────────────────►│ reserva buffer en flash alto
   │  ◄──────────────── ERASING(%)  (xN) ──────│ borra buffer (puede tardar ~s)
   │  ◄───────────────────────────── READY    │
   │  ┌── por cada bloque b (256 B) ──────────┐│
   │  │ BLOCK_BEGIN(b,len) ───────────────────►│
   │  │ DATA × ⌈len/8⌉ ───────────────────────►│ escribe al buffer FlasherX
   │  │ BLOCK_END(b, crc16) ──────────────────►│ verifica CRC16 del bloque
   │  │ ◄──────────────── BLOCK_ACK(b)         │   (BLOCK_NAK ⇒ reintenta, máx R)
   │  └────────────────────────────────────────┘
   │  END(n_blocks) ─────────────────────────►│ verifica CRC32 de TODA la imagen
   │  ◄───────────────────────────── END_ACK  │   (si falla ⇒ ERROR, NO commitea)
   │  ◄───────────────────────────── REBOOTING│ FlasherX: erase app + copy + boot
   │  QUERY (re-confirma) ───────────────────►│ (ya con firmware nuevo)
   │  ◄───────────────── STATUS(app_ver nuevo)│
```

### 7.5 FSM del target
```
 APP_RUN ──UNLOCK(ok)──► MAINT ──BEGIN(safe)──► ERASING ──erase ok──► RECEIVING
    ▲                      │ (NAK si unsafe)        │                    │
    │                      ▼                        │                    │ END
    │  ◄── ABORT / timeout / ERROR ◄────────────────┴───────◄────────────┤
    │                                                                    ▼
    └───────────────────────────────── (CRC32 fail) ◄──── VERIFY ──(ok)──► COMMIT ─► REBOOT
                                         keep old fw                       (FlasherX)
```
- **`COMMIT`** = FlasherX: `firmware_buffer_init()` durante `RECEIVING`,
  `flash_write()` por bloque, y en `END_ACK` el `flash_move()` (erase app + copy
  + reboot). Es el **único** instante crítico (ver §7.7).
- Cualquier timeout/aborto ⇒ **descartar buffer y volver al firmware viejo intacto.**

### 7.6 FSM del programmer
`QUERY → UNLOCK → BEGIN → [espera READY, tolerando ERASING] → ∀bloque{ BLOCK_BEGIN
→ DATA× → BLOCK_END → espera BLOCK_ACK (reintenta en NAK, máx R) } → END → espera
END_ACK/REBOOTING → re-QUERY para confirmar versión nueva.`

### 7.7 Integridad en 3 capas (defensa en profundidad)
1. **CRC de frame (HW):** los **15 bits de CRC** de cada frame CAN + reintento
   automático cubren errores de bit en el cable.
2. **CRC16/CCITT por bloque** (mismo de `proto.h`): cubre **pérdida/orden** de
   frames dentro de un bloque (si un RX se desborda). `BLOCK_NAK` ⇒ reenvío.
3. **CRC32 de la imagen completa** (enviado en `BEGIN`, **re-verificado** por el
   target antes de commitear): **gate final**. Si no matchea ⇒ `ERROR`, **no se
   commitea**, queda el firmware viejo. Es lo que hace el flasheo *anti-brick por
   imagen corrupta*.

### 7.8 Anti-brick y rollback
- **Imagen corrupta** ⇒ cubierta por el CRC32 (§7.7.3): nunca se escribe app mala.
- **Corte de energía durante `flash_move`** (erase app→copy): único caso
  irrecuperable por bus ⇒ **mitigación**: flashear con **alimentación estable de
  banco** + **USB recovery** siempre disponible.
- **App nueva que arranca pero está rota:** patrón *confirm-or-rollback* — la app
  nueva debe **auto-confirmarse** en el primer boot (setear un flag / "patear" un
  watchdog post-update) en < N segundos; si no confirma, no hay banco B en la
  config básica de FlasherX ⇒ recovery por USB.
- **Upgrade futuro (A/B / dual-bank):** la Teensy 4.1 tiene **8 MB de flash**;
  alcanza para **dos imágenes**. Un bootloader A/B que arranque la imagen válida
  da rollback automático real sin USB. Es trabajo extra ⇒ **fase posterior**, no v1.

### 7.9 Tiempos y reintentos (a fijar; defaults razonables)
| Parámetro | Default propuesto | Nota |
|---|---|---|
| `T_BLOCK_ACK` | 200 ms | espera de `BLOCK_ACK` antes de reintentar |
| `R_BLOCK` | 3 | reintentos por bloque antes de `ABORT` |
| `T_KEEPALIVE` | 1500 ms | sin `ERASING`/respuesta ⇒ sesión muerta |
| `T_SESSION` | 60 s | techo global de una sesión de flasheo |
| Tamaño de bloque | 256 B (32 frames) | balance overhead/latencia |

> A diferencia de los timeouts de **partido** (que exigen medición en HW por
> seguridad, TASK-014), estos son de **banco** y no afectan el juego: los defaults
> sirven para arrancar y se afinan con la velocidad real medida.

### 7.10 Estimación de duración
Imagen típica Teensy 4.x ~64–256 KB. A 1 Mbps clásico (~60 KB/s útil con
overhead de bloque/ACK): **~64 KB ≈ 1–2 s**, **256 KB ≈ 4–8 s**. Con CAN-FD o
WiFi, más rápido. Aceptable para la pit.

### 7.11 Cámaras (OpenMV) y bridge (ESP32) — fuera de este protocolo
- **OpenMV** corre **MicroPython**: no se "flashea firmware" para tu código, se
  **reemplaza `main.py`** (un archivo). Camino normal: **USB / OpenMV IDE**. Un
  *script-push* por CAN (escribir el `.py` al filesystem de la cámara) es posible
  como extensión futura, pero **no** es parte del protocolo de firmware Teensy.
  El **firmware** de la OpenMV se actualiza por **USB DFU**.
- **ESP32-S3 bridge:** trae **OTA por WiFi nativo** (`ArduinoOTA`/`esp_ota`) ⇒ se
  flashea inalámbrico directo, sin pasar por CAN.

### 7.12 La arquitectura "sueño": bridge como hub de programación
```
Laptop ──WiFi OTA──► ESP32-S3 (bridge/programmer) ──CAN flash (§7)──► Teensy destino
                                                                          │ self-flash
                                                                       FlasherX → reboot
```
Empujás el `.hex`/`.bin` desde la pit a la red WiFi → el bridge lo direcciona por
CAN a la placa elegida (por `node_id`) → esa Teensy se reprograma sola.
**Flasheo inalámbrico de todo el robot desde la laptop** (siempre con USB de
respaldo y bajo el gate de mantenimiento).

---

## 7-bis. Extensión: el gateway como nodo inter-robot (SuperTeam, post-Incheon)

> Analiza si los **dos robots** del SuperTeam pueden compartir datos usando el
> **mismo gateway** del bus CAN. **NO cambia** la decisión vigente
> [`2026-05-17-comunicacion-inter-robot-superteam.md`](2026-05-17-comunicacion-inter-robot-superteam.md)
> (Opción A: NO implementar inter-robot para Incheon). Es la **arquitectura
> concreta de la Opción B**, para cuando el equipo decida invertir o el comité
> libere la extensión oficial.

### 7-bis.1 Principio: CAN no cruza entre robots
CAN es cableado y **se corta en el borde del robot**. Dos robots que se mueven no
pueden compartir un bus físico ⇒ **el inter-robot es inherentemente wireless**. El
gateway **no** extiende el bus: es un **puente selectivo CAN↔aire**.

Patrón correcto (**NO** tunelizar todo el bus — el bus local hace miles de
frames/s y B no debe ver los comandos de motor de A):
- GW-A **lee** del CAN local un set chico y curado (`TeamShare`: pelota, mi pose,
  rol, intención) y lo manda por **ESP-NOW** a GW-B.
- GW-B **inyecta** eso como un **mensaje CAN local** nuevo (`TEAMMATE_STATE`,
  ID `0x230`). Los Teensy consumen al compañero **como un CAN frame más**.

```
ROBOT A bus CAN ──TeamShare──► GW-A (ESP-NOW 2.4GHz ≤100mW) ≈≈► GW-B ──inyecta 0x230──► bus CAN ROBOT B
```
⇒ *Localmente* "todo es un mensaje CAN"; el **salto entre robots** es best-effort.
**Nunca control hard cross-robot**: se comparte **percepción e intención**, no
comandos de motor (justo lo que el reglamento espera).

### 7-bis.2 El reglamento separa partido y banco (los 3 roles son exclusivos)
| Rol del gateway | ¿Partido? | ¿Banco? | Canal |
|---|---|---|---|
| **Inter-robot** (`TeamShare`) | ✅ sí (robot↔robot) | ✅ | ESP-NOW 2.4 GHz **≤100 mW EIRP** |
| **Telemetría a PC** | ❌ no | ✅ | ESP-NOW / WiFi |
| **Flasheo** (§7) | ❌ no | ✅ | WiFi → CAN |

Como **solo el inter-robot es de partido** y telemetría/flasheo son **solo de
banco**, los tres roles son **temporalmente exclusivos** ⇒ sin contención
real-time. Un **mismo gateway** puede hacer los tres, con un **gate de modo**:
- `match_running = true` (**PARTIDO**) ⇒ **solo** TX inter-robot ≤100 mW;
  telemetría y flasheo **hard-OFF** (compile-gate + runtime assert + LED). El
  `match_running` ya viaja en el `WorldSnapshot` (origen: árbitro→COMM→GPIO TOP)
  ⇒ el gateway lo lee del bus, sin cableado nuevo.
- `match_running = false` (**BANCO**) ⇒ todo habilitado.
- Refuerzo físico: el gateway es **plug-in** ⇒ para partidos oficiales se puede
  **desenchufar** (el robot es autónomo sin él).

### 7-bis.3 NO usar la placa COMM (ESP32-C6) para esto
La COMM es el **módulo oficial OBLIGATORIO** del árbitro (BLE → OUT_1/OUT_2). No
expone CAN y sobrecargarla = la **Opción C ya rechazada** en
`2026-05-17-...superteam.md` (riesgo de romper la función obligatoria +
homologación). **La COMM queda dedicada al árbitro.** El gateway es un
**ESP32-S3 separado** colgado del bus CAN.

### 7-bis.4 Compartir un gateway vs radio dedicada
| | Gateway compartido (3 roles) | Radio inter-robot dedicada |
|---|---|---|
| Peso / partes | ✅ menos (1 board, 1 antena) | ❌ más (3 radios con la COMM) |
| **Compliance** | ⚠️ depende del gate de modo (un bug = violación) | ✅ blindado por separación física |
| Robustez | ⚠️ punto único (pero inter-robot es best-effort) | ✅ aislado |

### 7-bis.5 Mensaje `TeamShare` / `TEAMMATE_STATE` (boceto)
Payload ESP-NOW curado (≤ ~32 B), inyectado en el CAN del receptor como `0x230`:

| Campo | Bytes | Nota |
|---|---|---|
| `sender_id` | 1 | qué robot |
| `seq` | 1 | salud del enlace |
| `ball_seen` | 1 | flag |
| `ball_x, ball_y` | 4 | pelota en marco de cancha (cm) |
| `self_x, self_y` | 4 | mi pose (cm) |
| `self_heading` | 2 | grados |
| `role` | 1 | arquero / defensor / delantero |
| `intent` | 1 | ej. "voy a la pelota" / "cubro" |

A 10–30 Hz ⇒ **<1 KB/s** por aire. Best-effort (STREAM): si se pierde, el
siguiente reemplaza. Inyectado como CAN `0x230`, la FSM táctica lo trata como
otro stream (misma taxonomía STREAM/EVENTO/COMANDO del diseño vivo).

### 7-bis.6 Recomendación
1. **COMM intocable** (árbitro).
2. **Construir hoy el gateway bench-only** (telemetría + flasheo), radio-OFF /
   desenchufado en partido. **Ya compatible** con la decisión vigente (Opción A).
3. **Inter-robot: diferido.** Cuando se active (Opción B): diseñar el gateway
   **capaz** de inter-robot pero como **modo aislado y hard-gated** que arranca
   apagado; si la homologación preocupa, ir a **radio dedicada**. Coordinación
   **best-effort by design** (percepción + intención, no control).

---

## 8. Plan de implementación por fases (aditivo, sin romper lo vivo)

> CAN entra **en paralelo** al UART actual. En cada fase el robot queda
> competitivo y el cambio es reversible. Encaja con la metodología de capas ya
> usada (transporte → conductas → real).

- **F0 — PoC CAN (1 tarde):** 2 Teensy + 2 SN65HVD230, contador a 1 Mbps,
  **con motores andando al lado** (test de ruido real, no solo desconectar cable).
- **F1 — Backbone 3 placas (espejo):** publicar `WorldSnapshot`/línea **también**
  por CAN; UART sigue como **fallback exacto**. Comparar — debe ser idéntico.
- **F2 — Cortar UART:** tras N prácticas con paridad, desactivar enlaces UART y
  **liberar pines**. Acá se cobra la simplicidad.
- **F3 — Cámaras al bus:** cada OpenMV publica coordenadas por CAN (sin pines
  UART nuevos).
- **F4 — Bridge RF:** ESP32-S3 al bus → ESP-NOW/WiFi → dashboard que muestra
  **todo el tráfico** en vivo. Flag "modo partido / modo banco".
- **F5 — Flasheo por CAN (este §7):** primero `QUERY`/`UNLOCK`/`STATUS`, luego
  transferencia de bloques contra **una** Teensy de prueba, **siempre con USB a
  mano**. Recién con eso sólido, OTA vía bridge.
- **F6 (post) — CAN-FD backbone** y/o **A/B dual-bank**: solo si hace falta.

**Regla dura (heredada):** no tocar el fail-safe de motores ni las ventanas de
partido por esto. El flasheo es de banco.

---

## 9. Riesgos y contras honestos

1. **Trabajo de firmware real:** portar el `Link` a un driver CAN + implementar
   §7 no es trivial (pero es *una vez*, y el `proto.h`/CRC16 se reusan).
2. **8 B/frame** ⇒ `WorldSnapshot` se segmenta (4 frames) o se va a CAN-FD.
3. **OpenMV en CAN es clásico (1 Mbps)** ⇒ fija el bus en clásico; FD va en par
   aparte solo-Teensy.
4. **Flasheo:** corte de energía en `flash_move` = brick hasta USB ⇒ banco con
   alimentación estable + USB accesible **obligatorio**. Sin A/B no hay rollback
   automático en v1.
5. **Reglamento:** la telemetría/flasheo **no** son features de partido. El robot
   gana con la RF apagada.

---

## 10. Decisión, autoría y fecha

- **Estudio, diseño del bus y del protocolo de flasheo:** Claude (Anthropic,
  Opus 4.8 1M) a pedido de Gustavo Viollaz (@gviollaz), **2026-06-03**.
- **Status:** **propuesta** — *no* adoptada como arquitectura vigente ni validada
  en banco. Candidata **post-Incheon**. La adopción la decide el equipo, fase por
  fase, cada una con prueba en hardware real (inyección de ruido EMI de motores).

## 11. Fuentes

- Diseño de comunicaciones VIVO (capas 0–6 que se preservan):
  `docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md`
- Mapa de datos / contratos: `docs/MAPA-DE-DATOS.md`,
  `docs/firmware/CONTRATO-DATOS-CENTRAL.md` (WorldSnapshot v2 = 27 B)
- Conexiones físicas: `hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md`
- Journal de esta sesión: `journal/2026-06-03-bus-can-y-flasheo-propuesta.md`
- Verificado externamente (jun-2026): Teensy 4.x FlexCAN (CAN3 = CAN-FD, 5–8 Mbps)
  · OpenMV Arduino Interface Library (CAN/UART/SPI/I²C, CAN ≤1 Mbps) · FlasherX
  (auto-flasheo Teensy desde cualquier stream) · RoboCup Jr Soccer Rules 2026
  (1.3.1 / 3.2: robot↔robot 2.4 GHz ≤100 mW, no remote control).
