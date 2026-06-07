---
title: "Spec — ESP32 Telemetry Bridge v0 (UART↔WiFi)"
date: 2026-06-07
status: R&D 2027 (no aplicado)
scope: r-d-2027
related: [r-d-2027/decisions/2026-06-07-esp32-telemetry-bridge.md, docs/firmware/TELEMETRIA-DOWN.md, docs/firmware/TELEMETRIA-TOP.md]
---

# Spec — ESP32 Telemetry Bridge v0

> Spec técnica del puente ESP32 que extiende la telemetría USB existente con
> un canal WiFi bidireccional. Documento de R&D 2027 — no aplicado al
> firmware de competencia.

## 1. Objetivo

Permitir que la app `tools/monitor-base` (y futuras app TOP/CENTRAL) reciban
y envíen telemetría/comandos **por WiFi**, reutilizando al 100% el protocolo
JSON Lines schema v1 que hoy va por USB CDC, sin modificar el binario de
competencia.

## 2. Diagrama lógico

```
   ┌──────────────────────┐                                    ┌──────────────────┐
   │  Teensy CENTRAL/TOP  │                                    │   Laptop          │
   │                      │                                    │   monitor-base    │
   │ telemetry_emit_line()│                                    │                  │
   │       │              │                                    │                  │
   │       ├──► Serial    │── USB CDC ─JSON Lines─────────────►│ --serial COMx    │
   │       │              │                                    │                  │
   │       └──► Serial2   │── 115200 ─JSON Lines────►┌────────┐│                  │
   │                      │                          │        ││                  │
   │            Serial2◄──┤──── comandos ────────────┤ ESP32  ││                  │
   │                      │   (CAL CARPET, etc.)    │ bridge ││                  │
   └──────────────────────┘                          │        ││                  │
                                                      │        ││ UDP             │
                                                      └─tx/rx──┘│ ───────────────►│
                                                                │                  │
                                                                │  --udp 192.168...│
                                                                │                  │
   ┌──────────────────────┐                                    └──────────────────┘
   │ Robot compañero      │
   │ ESP32 bridge         │◄── ESP-NOW (stub vacío hasta que el     ┌──────────┐
   │                      │     equipo decida compartir cosas)──────┤peer MAC  │
   └──────────────────────┘                                          └──────────┘
```

## 3. Hardware

### 3.1 Componentes por robot
- **1 ESP32** — DevKit cualquier variante con WiFi:
  - **Recomendado**: ESP32-DevKitC (WROOM-32) — barato, WiFi 4 + BT classic, en cajón de cualquier estudiante.
  - **Alternativa moderna**: ESP32-C3 SuperMini — más chico ($3), WiFi 4 + BLE 5, RISC-V.
  - **No usar el ESP32-C6 del COMM** (es el del árbitro y NO se toca).
- **4 cables dupont** (M-F si el Teensy tiene headers macho): TX, RX, 3V3, GND.
- Opcional: cinta velcro o tornillo para fijar el ESP32 al chasis cerca del Teensy.

### 3.2 Conexionado (Teensy ↔ ESP32)

| Teensy (CENTRAL/TOP/DOWN) | ESP32 (DevKit) | Nota |
|---|---|---|
| GND | GND | masa común OBLIGATORIA |
| 3V3 | 3V3 (Vin) | alimentar ESP32 desde el rail 3V3 del Teensy (si capacidad eléctrica lo permite — ESP32 pico WiFi puede consumir ~250 mA, mejor desde Vin con regulador propio) |
| TX (UART libre, e.g. Serial2 = pin 7) | RX (pin GPIO 16 ESP32-WROOM o GPIO 9 ESP32-C3) | Teensy es 3.3 V → directo, sin divisor |
| RX (UART libre, e.g. Serial2 = pin 8) | TX (GPIO 17 / GPIO 10) | ESP32 es 3.3 V → directo, sin divisor |

**Importante:** el rail 3V3 del Teensy 4.0/4.1 puede no bancar el pico de
consumo del ESP32 al asociarse a WiFi (~500 mA pico). Recomendación: alimentar
el ESP32 desde **5V (Vin)** del USB o de la batería del robot via su propio
regulador. NUNCA conectar el VIN de ambos cruzado.

### 3.3 Qué UART usar en cada Teensy

Antes de elegir, **revisar `hardware/electronics/mapa-pines-teensy-ambos-robots.md`**
para confirmar qué UART está libre en cada placa. Sugerencia inicial:

| Teensy | UART libre típico | Pin TX | Pin RX |
|---|---|---|---|
| CENTRAL | Serial2 | 7 | 8 |
| TOP | Serial4 | 16 | 17 |
| DOWN | Serial3 | 14 | 15 |

(Confirmar contra el mapa real antes de cablear — esta tabla es propuesta, no
verdad.)

## 4. Protocolo (REUTILIZA schema v1 existente)

### 4.1 Capa baja (Teensy ↔ ESP32 por UART)
- Baud: **115200** 8N1 (mismo que USB CDC para consistencia).
- Contenido: **JSON Lines schema v1** EXACTO, byte por byte, mismo que el USB.
  Una línea por mensaje, terminada en `\n`. El ESP32 NO interpreta el JSON
  — solo copia bytes hasta `\n` y reenvía.
- Dirección: bidireccional.
  - Teensy → ESP32: stream de telemetría a la tasa que ya emite por USB
    (configurable con `RATE`).
  - ESP32 → Teensy: comandos recibidos por WiFi (`CAL CARPET`, `OTOS RESET`,
    `RATE 50`, etc.) reenviados tal cual.

### 4.2 Capa WiFi (ESP32 ↔ Laptop)
- **Modo principal: UDP broadcast** (sin estado, simple, sin broker).
  - ESP32 envía cada línea a `255.255.255.255:8765` (o IP/puerto configurables).
  - Laptop escucha con un socket UDP. Cualquier app PC del repo (`monitor-base`,
    o el listener standalone de `code/pc-udp-listener/`) lo recibe.
- **Comandos laptop → ESP32**: UDP unicast desde el laptop al ESP32 al puerto
  `8764`. ESP32 los reenvía por UART al Teensy.
- **Opcional MQTT** (futuro): compilar con `-DENABLE_MQTT` y publicar a un
  broker. No se necesita para v0.

### 4.3 Convención de encabezado JSON (compatibilidad)
Cada línea es un JSON object con (al menos) `{"v": 1, "src": "down", ...}`.
El ESP32 NO toca esos campos — son los que el firmware ya emite hoy.

### 4.4 Filtro de línea
El ESP32 acepta líneas hasta `MAX_LINE_BYTES = 4096` (default). Líneas más
largas se descartan (defensa contra corrupción). Hoy el JSON Lines de DOWN
en peor caso es ~1.5 KB (anillo 32 sensores + calibración).

## 5. Firmware ESP32 (resumen)

Completo en `r-d-2027/code/esp32-bridge-firmware/`. Modos:
- **boot**: lee `config.h` (WiFi SSID/pass, IP destino, puerto, peer MAC para ESP-NOW).
- **loop**: drena UART → UDP; drena UDP → UART. ESP-NOW pair-stub (idle por default).
- **diagnóstico**: LED del DevKit:
  - apagado: sin WiFi.
  - parpadeo lento (1 Hz): WiFi conectado, sin tráfico.
  - parpadeo rápido (5 Hz): tráfico UART activo.
- **OTA opcional**: campos en config.h para activar ArduinoOTA (push update
  inalámbrico sin desarmar). Default OFF.

## 6. Glue Teensy (NO aplicado al firmware actual)

Ejemplo en `r-d-2027/code/teensy-glue-snippet/central_telemetry_esp32_glue.example.cpp`.

Pasos para integrarlo (cuando el coach decida):
1. Agregar en `platformio.ini` un env nuevo, p.ej. `central_robot1_telem_esp32`,
   con `-DCENTRAL_DEBUG_TELEMETRY -DENABLE_TELEMETRY_ESP32`.
2. Inicializar `Serial2.begin(115200)` en el setup gateado por
   `#ifdef ENABLE_TELEMETRY_ESP32`.
3. En cada lugar donde hoy `telemetry_emit_line(line)` hace `Serial.print(line)`,
   agregar `Serial2.print(line)` adicional gateado.
4. En el loop, leer `Serial2.read()` hasta `\n` y pasar la línea al parser de
   comandos existente (`td_parse_command`).

**Sin el flag `ENABLE_TELEMETRY_ESP32`, el binario de competencia es byte-idéntico.**

## 7. App PC

### 7.1 Listener standalone
`r-d-2027/code/pc-udp-listener/udp_listener.py`: script Python ~80 líneas que
escucha UDP y vuelca a stdout. Sirve para:
- Diagnóstico inmediato (ver si llegan datos).
- Como pipe para `monitor-base` si se le agrega `--stdin` (alternativamente
  el listener puede reemitir a un puerto serial virtual).

### 7.2 Integración a `monitor-base`
Sugerido (no aplicado): agregar a `tools/monitor-base/` un modo `--udp PORT`
que use el mismo parser de `monitor_base.py`. Una sola línea de cambio en el
loop principal (decidir entre `serial.readline()` vs `socket.recvfrom()`),
el resto idéntico. Se hace cuando se apruebe la integración real.

### 7.3 Calibración remota
Una vez bidireccional, los botones de la GUI (`CAL CARPET`, `WHITE`, `AUTO`,
`SAVE`) mandan UDP en lugar de Serial. Cero cambio en la API ni en el
firmware (los comandos son los mismos strings).

## 8. Seguridad y operación

- **Sin partido oficial**: la red WiFi del venue es impredecible. El bridge
  se apaga (build flag OFF) en el binario de competición.
- **Solo comandos de calibración/telemetría** por WiFi. **NUNCA**
  START/STOP/movimiento por WiFi (no aporta y es riesgo).
- **WPA2 al menos** en la red usada (no AP abierto).
- **WiFi credentials NO se commitean**. Plantilla en `config.example.h`,
  copia local a `config.h` ignorada por git.

## 9. Tests

### 9.1 Firmware ESP32
- Test de carga: ESP32 + USB-UART loopback → la PC manda un JSON conocido y
  espera recibirlo por UDP. Confirma path completo.
- Test de stress: 50 Hz de líneas de 1 KB → verificar 0 pérdida durante 5 min.

### 9.2 Reutilización del golden
El golden frame v1 de `test_telemetry_down/test_main.cpp::test_td_serialize_golden_exact`
DEBE seguir pasando — el ESP32 es transparente al schema. Se puede agregar un
test E2E en el listener Python que reciba el golden por UDP y verifique
byte a byte.

### 9.3 Integración (banco)
- Robot encendido + ESP32 cableado + laptop en la misma red.
- App PC en modo `--udp` muestra el anillo de 32 sensores en vivo igual que
  por USB.
- Botón `CAL AUTO` desde la app → robot ejecuta calibración → respuesta
  vuelve por WiFi → app muestra "calibración OK".

## 10. Métricas de éxito

- Latencia laptop→robot por UDP: ≤ 30 ms (medible en banco).
- Latencia robot→laptop: ≤ 30 ms.
- Tasa sostenida 20–50 Hz sin pérdida.
- 0 efecto sobre el binario de competencia con flag OFF.

## 11. Roadmap evolutivo desde acá

- **v0** (este doc): UART⇄UDP, schema v1 transparente, JSON Lines.
- **v1**: ESP-NOW pair entre dos robots con `PartnerSnapshot` real.
- **v2**: MQTT opcional para grabar entrenamientos a TimescaleDB.
- **FASE C** (ver `roadmap.md`): mismo ESP32, leer de CAN en lugar de UART.
