---
title: "Decisión — ESP32 Telemetry Bridge v0 (puente UART→WiFi)"
date: 2026-06-07
status: propuesta (R&D 2027 — NO aplicada al firmware de competencia)
scope: r-d-2027
related: [r-d-2027/specs/esp32-telemetry-bridge-v0-spec.md, docs/firmware/TELEMETRIA-DOWN.md, docs/decisions/2026-05-17-comunicacion-inter-robot-superteam.md]
---

# Decisión — ESP32 Telemetry Bridge v0

> Documento de R&D 2027. **No aplica al robot de Incheon 2026 hasta que el
> coach lo apruebe explícitamente.**

## 1. Contexto

Hoy (Incheon 2026) el equipo tiene un sistema de **telemetría USB** ya
construido y funcionando:

- **Firmware:** modo debug gateado por `-DDOWN_DEBUG_TELEMETRY`/`-DTOP_DEBUG_TELEMETRY`
  que emite JSON Lines @115200 por USB CDC.
  Lógica pura en `src/shared/telemetry_down.cpp` + `telemetry_top.cpp` (host-testeada,
  golden frame versionado v1).
- **App PC:** `tools/monitor-base/` (Python) que recibe por serie y dibuja el
  anillo de 32 sensores, línea detectada, LineStatusV2 que viaja a CENTRAL,
  + calibración asistida (carpet/blanco/auto/save).
- **Protocolo:** **JSON Lines schema v1**, bidireccional (firmware↔PC), versionado.
  Documentado en `docs/firmware/TELEMETRIA-DOWN.md` y `TELEMETRIA-TOP.md`.

Limitación: requiere **cable USB** durante puesta a punto. Eso:
- Limita los lugares donde se puede observar (alcance del cable, gestión de USB
  hub, alimentación cuando hay 2 robots y 1 sola laptop).
- Obliga a parar el robot/desconectarlo para "ir a mirarlo".
- No facilita registrar datos en pleno entrenamiento dinámico.

El roadmap `docs/competencia/MEJORAS-PENDIENTES.md` E4 ya prevé migrar a
**CAN troncal + ESP32 gateway inalámbrico** ("telemetría F1") como objetivo
2027.

## 2. Decisión propuesta

**Construir un puente ESP32 standalone que recibe por UART el MISMO stream
JSON Lines que hoy va por USB y lo reenvía a la laptop por UDP/WiFi.**

```
   ┌──────────────────────────┐                       ┌─────────────────┐
   │  Teensy CENTRAL/TOP/DOWN │                       │     Laptop       │
   │  (firmware competencia)  │                       │   (monitor-base) │
   │                          │                       │                 │
   │   telemetry stream ──────┼─USB CDC──────────────►│  --serial mode  │
   │   (JSON Lines v1)        │      (lo que YA anda)  │  (igual que hoy)│
   │           │              │                       │                 │
   │           └──Serial2─────┼──►┌──────────┐         │                 │
   │                          │   │  ESP32   │──WiFi──►│  --udp mode    │
   │                          │   │  bridge  │   UDP   │  (nuevo)        │
   │                          │   │          │◄───UDP──│  comandos       │
   │           ◄──Serial2─────┼───┤          │         │  bidireccional  │
   │                          │   └──────────┘         └─────────────────┘
   └──────────────────────────┘
```

## 3. Por qué (vs otras opciones)

### Por qué NO modificar la telemetría existente

Cero. La telemetría USB ya anda, está testeada, tiene golden, y es lo que se
está usando para terminar la puesta a punto de Incheon. **Tocarla es riesgo,
no valor.**

### Por qué SÍ un ESP32 separado

- **Reutiliza el schema v1 al 100%**. El ESP32 no parsea ni interpreta — solo
  copia bytes de UART a UDP. La app PC recibe exactamente la misma estructura
  que hoy. Mismo golden, mismo parser, mismas pruebas.
- **No toca el binario de competencia.** Si el flag `-DENABLE_TELEMETRY_ESP32`
  está OFF (default), el firmware del Teensy es **byte-idéntico** al de hoy.
  Con el flag ON, el único cambio es duplicar el `Serial.print(json)` a un
  `Serial2.print(json)` adicional → cero riesgo lógico.
- **Es el primer paso del E4.** El ESP32 que hoy hace UART→UDP es **literalmente
  el mismo nodo gateway** que en 2027 hará CAN→UDP. Cambia solo el puerto de
  entrada. El firmware del ESP32 (UDP forwarder + ESP-NOW pair stub) es
  reutilizable.

### Alternativas consideradas

| Alternativa | Por qué no |
|---|---|
| Bluetooth Classic (SPP) desde el Teensy | Teensy no tiene radio. Habría que poner un módulo BT externo igual; gasto similar pero peor (BT classic latente, sin multi-cliente, no escala a inter-robot). |
| Conectar el COMM (ESP32-C6) del árbitro a la telemetría también | El COMM corre firmware oficial RCJ certificado. Modificarlo arriesga la homologación y mezcla seguridad (start/stop árbitro) con debug. **Se conserva COMM intocable.** Mejor un ESP32 dedicado. |
| WiFi vía un dongle USB en el Teensy | No existe driver práctico de Teensy para dongles USB WiFi. |
| Telemetría sobre el cable USB existente (mejorar la app actual) | Sigue requiriendo cable. No resuelve el problema. |
| Saltar directo a la FASE C (CAN + gateway 2027) | Requiere HW de CAN, software de gateway con 3 listas blancas, mapeo CAN-ID, etc. — 2+ semanas y bloquea otras prioridades de Incheon. La FASE A entrega 70% del valor con 10% del esfuerzo. |

## 4. Consecuencias

### Lo que ganamos

- Telemetría inalámbrica EN VIVO durante entrenamiento (sin cable).
- **Calibración remota** (la app manda `CAL CARPET/WHITE/AUTO/SAVE` por WiFi,
  el ESP32 los reenvía por UART, el firmware existente los procesa).
- Stub ESP-NOW listo para inter-robot (vacío, lista blanca configurable).
- Validación temprana del flujo *sensor → red → app PC* que la FASE C necesita.
- Costo bajísimo: $5-10 HW, 2-3 días SW, riesgo prácticamente cero.

### Lo que sacrificamos

- Otro chip que mantener (HW + un binario más).
- WiFi en cancha de torneo puede ser ruidoso/conflictivo (no usar durante
  partido oficial — solo entrenamiento).
- 1 UART extra del Teensy ocupado. Hoy Teensy 4.x tiene varios libres en
  CENTRAL/TOP — verificar disponibilidad (spec §4).

### Lo que NO cambia

- Comportamiento del robot con telemetría OFF (gate cumple regla byte-idéntico).
- Protocolo JSON Lines schema v1 (el ESP32 es transparente al nivel app).
- La app `monitor-base` sigue funcionando idéntico por USB; le agregamos
  `--udp` como modo adicional.

## 5. Reglas de uso

- **NUNCA en partido oficial.** Solo entrenamiento/banco. En partido la red WiFi
  del venue puede ser problemática (latencia, interferencias) y no aporta nada
  al juego — el robot juega autónomo.
- **NUNCA enviar comandos críticos** por UDP. La FASE A solo permite comandos
  de telemetría/calibración (idempotentes y reversibles). No `START`/`STOP`/
  movimientos por WiFi.
- WiFi credentials del ESP32 viven en `code/esp32-bridge-firmware/include/config.h`
  (gitignored, no se commitea). Plantilla en `config.example.h`.

## 6. Plan

1. **Spec técnica detallada** — `r-d-2027/specs/esp32-telemetry-bridge-v0-spec.md`.
2. **Firmware ESP32 completo** en `r-d-2027/code/esp32-bridge-firmware/`
   (PlatformIO + Arduino, standalone, ~200-300 líneas).
3. **Snippet de glue Teensy** en `r-d-2027/code/teensy-glue-snippet/`
   (ejemplo, NO aplicado al firmware de competencia).
4. **PC UDP listener standalone** en `r-d-2027/code/pc-udp-listener/`
   (Python, plug-and-play, también sirve como referencia para integrar en
   `monitor-base`).
5. Cuando el coach decida aplicarlo: branch `rd-2027-telemetry-bridge`,
   prueba en banco, decisión explícita para mergear o pausar.

## 7. Quién decide y cuándo

- **Propuesta:** Claude (Anthropic) a pedido de Gustavo, 2026-06-07.
- **Aprobación para construir el código stub en `r-d-2027/code/`:**
  Gustavo (autorizó "elaborar todo").
- **Aprobación para aplicarlo al robot real (Teensy glue):** pendiente,
  decisión del coach + revisión del equipo (Virginia/Elías). Hasta entonces
  vive solo como R&D documentado.
