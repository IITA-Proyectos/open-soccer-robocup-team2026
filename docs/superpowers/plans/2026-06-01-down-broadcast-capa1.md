# DOWN Broadcast Simétrico — Capa 1 (Transporte) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Que la placa DOWN difunda los 3 frames que ya existen (`LineStatusV2` 0x10 + `Pose2D` 0x11 + `Velocity2D` 0x12) a **ambas** placas (CENTRAL por Serial1 y TOP por Serial5), que CENTRAL los **ingiera** (línea ya; OTOS nuevo), y que TOP **reciba bien la línea** (hoy la descarta por tipo equivocado). Sin consumir el OTOS en strategy todavía (eso es Capa 2).

**Architecture:** Transporte = frames separados reusando los MsgType existentes, difundidos a ambos UART con un módulo nuevo `down_tx` que lleva **SEQ monótono por enlace**. CENTRAL gana storage + accessors de OTOS en `world_model` (el WorldSnapshot del TOP sigue siendo la pose autoritativa de cancha; el OTOS directo es solo para control de movimiento en Capa 2). Helper puro nuevo `pose_view.h` (espejo de `line_view.h`) para decodificar pose/vel, host-testeable.

**Tech Stack:** C++17, PlatformIO (Teensy 4.0/4.1, Arduino), protocolo `src/shared/proto.h` (START/CRC16/SEQ/END), Unity host-native (`pio test -e test_native` y `scripts/run-host-tests.sh` offline).

**Spec fuente:** `docs/superpowers/specs/2026-06-01-down-broadcast-simetrico-design.md` (este plan implementa SOLO la Capa 1).

---

## Scope

Este plan cubre **Capa 1 (transporte)** únicamente. Capa 2 (CENTRAL consume OTOS para drive-straight) y Capa 3 (`cross_track`/`penetration` reales + GK strafe) son planes futuros (su propio ciclo spec→plan). Capa 1 produce software que compila, pasa tests host, y deja el dato disponible y bien recibido en ambas placas — testeable de forma independiente.

## File Structure

| Archivo | Responsabilidad | WP |
|---------|-----------------|----|
| `src/down/down_tx.h` (crear) | Interfaz de broadcast TX de DOWN a ambos enlaces | 1A |
| `src/down/down_tx.cpp` (crear) | 2 enlaces (Serial1=CENTRAL, Serial5=TOP), SEQ por enlace, backpressure, contadores | 1A |
| `src/down/comm_central.cpp` (modificar) | Enviar la línea vía `down_tx` (broadcast) en vez de su propio encode/Serial1 | 1A |
| `src/down/comm_top.cpp` (modificar) | Enviar pose/vel vía `down_tx` (broadcast) en vez de `send_typed`/Serial5 | 1A |
| `src/shared/pose_view.h` (crear) | Helpers puros `pose_from_frame`/`vel_from_frame` (espejo de `line_view.h`) | 1B |
| `src/central/world_model.h` (modificar) | Declarar apply/accessors de OTOS | 1B |
| `src/central/world_model.cpp` (modificar) | Storage `g_otos_pose/vel` + freshness + accessors | 1B |
| `src/central/comm_down.cpp` (modificar) | `handle_frame` ingiere 0x11/0x12 → `world_model` | 1B |
| `test/test_central_otos_ingest/test_main.cpp` (crear) | Test host del chain encode→decode→extract de pose/vel | 1B |
| `src/top/comm_down.cpp` (modificar) | `g_line` → `LineStatusV2`; case `LINE_URGENT` usa `lsv2_from_frame` | 1C |
| `src/top/comm_down.h` (modificar) | Firma del getter de línea → `const LineStatusV2&` | 1C |
| docs varios (modificar) | Sync contratos/arquitectura/mapa al broadcast simétrico | 1E |

## Mapa de paralelización (para el fan-out de agentes)

WP-1A (toca `src/down/`), WP-1B (toca `src/shared/pose_view.h` + `src/central/`), WP-1C (toca `src/top/`), WP-1E (toca `docs/`) tocan **archivos disjuntos** → corren en paralelo, cada uno en su worktree. `types.h`/`proto.h` se leen pero NO se modifican (Capa 1 no cambia contratos de tamaño). El coach integra a `main` con el gate (`pio test -e test_native` + builds + `run-host-tests.sh`). Tarea final = integración + gate completo.

**Nota de testabilidad (honesta):** WP-1A (`down_tx`) y WP-1C (wiring TOP) son **Arduino-glue** → verificación = compila + banco (siguiendo el patrón del repo: lógica pura en `src/shared` host-testeada, glue Arduino compile-only). El framing ya está cubierto por `test_proto` + `test_down_encode`. WP-1B SÍ tiene test host (la interpretación pura vive en `pose_view.h`).

---

### Task WP-1A: `down_tx` — broadcast de DOWN a ambos enlaces

**Files:**
- Create: `software/teensy/Soccer 2026/src/down/down_tx.h`
- Create: `software/teensy/Soccer 2026/src/down/down_tx.cpp`
- Modify: `software/teensy/Soccer 2026/src/down/comm_central.cpp`
- Modify: `software/teensy/Soccer 2026/src/down/comm_top.cpp`

- [ ] **Step 1: Crear `down_tx.h`**

```cpp
// down_tx.h — Capa de transmisión BROADCAST de la placa DOWN.
//
// Difunde cada mensaje a AMBOS enlaces de salida:
//   • Enlace 0 = CENTRAL (Serial1, TX1 = pin 1)
//   • Enlace 1 = TOP     (Serial5, TX5 = pin 20)
// Cada enlace lleva su PROPIO SEQ monótono (compartido entre los 3 tipos) para que
// la detección de pérdida por SEQ del receptor sea correcta al intercalar tipos.
// Backpressure por enlace (availableForWrite): si el buffer está lleno, dropea el
// frame y lo cuenta (no bloquea el line_ring de 1 kHz). El Serial.begin() de ambos
// UART lo hacen comm_central_init()/comm_top_init(); este módulo solo escribe.
#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

void down_tx_broadcast_line(const LineStatusV2& s);   // 0x10 a CENTRAL + TOP
void down_tx_broadcast_pose(const Pose2D& p);         // 0x11 a CENTRAL + TOP
void down_tx_broadcast_vel(const Velocity2D& v);      // 0x12 a CENTRAL + TOP

// Telemetría por enlace: 0 = CENTRAL (Serial1), 1 = TOP (Serial5).
uint32_t down_tx_get_sent(uint8_t link);
uint32_t down_tx_get_dropped(uint8_t link);

}  // namespace iitasoccer
```

- [ ] **Step 2: Crear `down_tx.cpp`**

```cpp
#include "down_tx.h"
#include "proto.h"
#include <Arduino.h>
#include <string.h>

namespace iitasoccer {
namespace {

struct DownLink {
    HardwareSerial* uart;
    uint8_t  seq;
    uint32_t sent;
    uint32_t dropped;
};

// Enlace 0 = CENTRAL (Serial1), enlace 1 = TOP (Serial5).
DownLink g_links[2] = {
    { &Serial1, 0, 0, 0 },
    { &Serial5, 0, 0, 0 },
};

void send_on_link(DownLink& lk, MsgType type, const void* payload, size_t len) {
    Frame f{};
    f.type = type;
    f.seq = lk.seq++;                       // SEQ propio del enlace
    f.payload_len = static_cast<uint8_t>(len);
    memcpy(f.payload, payload, len);

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n == 0) return;
    if (lk.uart->availableForWrite() >= static_cast<int>(n)) {
        lk.uart->write(buf, n);
        lk.sent++;
    } else {
        lk.dropped++;                       // backpressure: dropear, no bloquear
    }
}

void broadcast(MsgType type, const void* payload, size_t len) {
    send_on_link(g_links[0], type, payload, len);   // CENTRAL
    send_on_link(g_links[1], type, payload, len);   // TOP
}

}  // namespace

void down_tx_broadcast_line(const LineStatusV2& s) { broadcast(MsgType::LINE_URGENT,    &s, sizeof(s)); }
void down_tx_broadcast_pose(const Pose2D& p)       { broadcast(MsgType::DOWN_OTOS_POSE, &p, sizeof(p)); }
void down_tx_broadcast_vel(const Velocity2D& v)    { broadcast(MsgType::DOWN_OTOS_VEL,  &v, sizeof(v)); }

uint32_t down_tx_get_sent(uint8_t link)    { return link < 2 ? g_links[link].sent    : 0; }
uint32_t down_tx_get_dropped(uint8_t link) { return link < 2 ? g_links[link].dropped : 0; }

}  // namespace iitasoccer
```

- [ ] **Step 3: Modificar `comm_central.cpp` — enviar la línea vía `down_tx`**

Agregar el include arriba (junto a los otros `#include`):
```cpp
#include "down_tx.h"
```

En `comm_central_send_line_urgent()`, reemplazar el bloque actual de serialización (el que hace `down_encode_line(...)` + `Serial1.availableForWrite()` + `Serial1.write(...)`) por:
```cpp
    // Difundir la línea a AMBAS placas (CENTRAL + TOP) con SEQ por enlace.
    down_tx_broadcast_line(s);
```

Redirigir los getters de telemetría de envío al enlace CENTRAL (link 0) de `down_tx` (reemplazar los cuerpos actuales que devuelven `g_frames_sent`/`g_frames_dropped`):
```cpp
uint32_t comm_central_get_frames_sent()    { return down_tx_get_sent(0); }
uint32_t comm_central_get_frames_dropped() { return down_tx_get_dropped(0); }
```

En el bloque `#ifdef DOWN_DEBUG_SERIAL`: **borrar** el re-envío manual por `Serial5` (el bloque que recodifica `s` con `down_encode_line` y hace `Serial5.write(buf5, nb5)`) — ahora la línea ya se difunde al TOP vía `down_tx_broadcast_line`. **Conservar** el cómputo del centroide `cross_track_mm` y el print USB de bring-up (no tocar). Quitar las variables locales `g_send_seq`/`g_frames_sent`/`g_frames_dropped` que queden sin uso.

- [ ] **Step 4: Modificar `comm_top.cpp` — enviar pose/vel vía `down_tx`**

Agregar el include:
```cpp
#include "down_tx.h"
```

En `comm_top_send_status()`, dejar el armado de `Pose2D pose{...}` y `Velocity2D vel{...}` igual, pero reemplazar las dos llamadas `send_typed(MsgType::DOWN_OTOS_POSE, pose)` / `send_typed(MsgType::DOWN_OTOS_VEL, vel)` por:
```cpp
    down_tx_broadcast_pose(pose);
    down_tx_broadcast_vel(vel);
```

Borrar la función `send_typed<T>` (queda sin uso) y la variable `g_send_seq`. Redirigir los getters de envío al enlace TOP (link 1):
```cpp
uint32_t comm_top_get_frames_sent()    { return down_tx_get_sent(1); }
uint32_t comm_top_get_frames_dropped() { return down_tx_get_dropped(1); }
```
(Conservar `comm_top_init()` con su `Serial5.begin(UART_TOP_BAUD)` y `comm_top_tick()` — el RX no cambia.)

- [ ] **Step 5: Compilar DOWN**

Run (desde `software/teensy/Soccer 2026`): `pio run -e down -e down_debug`
Expected: ambos `SUCCESS`.

- [ ] **Step 6: Commit**

```bash
git add "software/teensy/Soccer 2026/src/down/down_tx.h" "software/teensy/Soccer 2026/src/down/down_tx.cpp" "software/teensy/Soccer 2026/src/down/comm_central.cpp" "software/teensy/Soccer 2026/src/down/comm_top.cpp"
git commit -m "feat(down): WP-1A broadcast linea+OTOS a ambos enlaces (down_tx, SEQ por enlace)"
```

---

### Task WP-1B: CENTRAL ingiere OTOS (pose_view.h + world_model + comm_down)

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/pose_view.h`
- Create: `software/teensy/Soccer 2026/test/test_central_otos_ingest/test_main.cpp`
- Modify: `software/teensy/Soccer 2026/src/central/world_model.h`
- Modify: `software/teensy/Soccer 2026/src/central/world_model.cpp`
- Modify: `software/teensy/Soccer 2026/src/central/comm_down.cpp`

- [ ] **Step 1: Escribir el test que falla — `test/test_central_otos_ingest/test_main.cpp`**

```cpp
// test_central_otos_ingest — corre con: pio test -e test_native -f test_central_otos_ingest
//
// Capa 1 del broadcast simétrico (2026-06-01): DOWN difunde Pose2D (0x11) +
// Velocity2D (0x12) también a CENTRAL. Probamos el chain encode→decode→extract con
// los helpers puros de pose_view.h (los mismos que usará comm_down.cpp del CENTRAL).
#include <unity.h>
#include <string.h>
#include "types.h"
#include "proto.h"
#include "pose_view.h"
using namespace iitasoccer;

void setUp(void) {}
void tearDown(void) {}

// Encodea un payload tipado como frame proto y lo redecodifica byte-a-byte.
template <typename T>
static bool encode_then_decode(MsgType type, const T& payload, Frame& out) {
    Frame f{};
    f.type = type;
    f.seq = 0x07;
    f.payload_len = sizeof(T);
    memcpy(f.payload, &payload, sizeof(T));
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n == 0) return false;
    FrameDecoder dec;
    bool got = false;
    for (size_t i = 0; i < n; ++i) if (dec.feed(buf[i])) got = true;
    if (got) out = dec.get_frame();
    return got;
}

void test_pose_roundtrip(void) {
    Pose2D p{};
    p.x_mm = 1234; p.y_mm = -567; p.heading_centideg = 9000; p.confidence = 100;
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(MsgType::DOWN_OTOS_POSE, p, f));
    Pose2D got{};
    TEST_ASSERT_TRUE(pose_from_frame(f, got));
    TEST_ASSERT_EQUAL_INT16(1234, got.x_mm);
    TEST_ASSERT_EQUAL_INT16(-567, got.y_mm);
    TEST_ASSERT_EQUAL_INT16(9000, got.heading_centideg);
    TEST_ASSERT_EQUAL_UINT8(100, got.confidence);
}

void test_vel_roundtrip(void) {
    Velocity2D v{};
    v.vx_mm_s = 300; v.vy_mm_s = -120; v.omega_centideg_s = 4500; v.slip_estimate = 7;
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(MsgType::DOWN_OTOS_VEL, v, f));
    Velocity2D got{};
    TEST_ASSERT_TRUE(vel_from_frame(f, got));
    TEST_ASSERT_EQUAL_INT16(300, got.vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(-120, got.vy_mm_s);
    TEST_ASSERT_EQUAL_INT16(4500, got.omega_centideg_s);
    TEST_ASSERT_EQUAL_UINT8(7, got.slip_estimate);
}

// pose_from_frame rechaza un frame de tipo equivocado (no es pose).
void test_pose_wrong_type_rejected(void) {
    Pose2D p{};
    Frame f{};
    TEST_ASSERT_TRUE(encode_then_decode(MsgType::LINE_URGENT, p, f));
    Pose2D got{};
    TEST_ASSERT_FALSE(pose_from_frame(f, got));
}

// vel_from_frame rechaza un payload de tamaño equivocado.
void test_vel_wrong_size_rejected(void) {
    Frame f{};
    f.type = MsgType::DOWN_OTOS_VEL;
    f.payload_len = 3;  // != sizeof(Velocity2D) (7)
    Velocity2D got{};
    TEST_ASSERT_FALSE(vel_from_frame(f, got));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_pose_roundtrip);
    RUN_TEST(test_vel_roundtrip);
    RUN_TEST(test_pose_wrong_type_rejected);
    RUN_TEST(test_vel_wrong_size_rejected);
    return UNITY_END();
}
```

- [ ] **Step 2: Correr el test y verificar que FALLA (build error: falta `pose_view.h`)**

Run (desde `software/teensy/Soccer 2026`): `bash scripts/run-host-tests.sh test_central_otos_ingest`
Expected: build-error / FAIL — `pose_view.h: No such file or directory`.

- [ ] **Step 3: Crear `src/shared/pose_view.h`**

```cpp
// pose_view.h — interpretación pura de Pose2D / Velocity2D (DOWN → CENTRAL/TOP).
//
// Espejo de line_view.h: helpers SIN estado y SIN Arduino, compartidos entre el
// firmware y los tests host-native. Pose2D/Velocity2D NO tienen schema_version,
// así que se valida solo tipo + tamaño exacto (un payload del tamaño/tipo
// equivocado se RECHAZA en vez de reinterpretarse como basura).
#pragma once
#include <stdint.h>
#include <string.h>   // memcpy
#include "types.h"
#include "proto.h"

namespace iitasoccer {

inline bool pose_from_frame(const Frame& f, Pose2D& out) {
    if (f.type != MsgType::DOWN_OTOS_POSE) return false;
    if (f.payload_len != sizeof(Pose2D)) return false;
    memcpy(&out, f.payload, sizeof(Pose2D));
    return true;
}

inline bool vel_from_frame(const Frame& f, Velocity2D& out) {
    if (f.type != MsgType::DOWN_OTOS_VEL) return false;
    if (f.payload_len != sizeof(Velocity2D)) return false;
    memcpy(&out, f.payload, sizeof(Velocity2D));
    return true;
}

}  // namespace iitasoccer
```

- [ ] **Step 4: Correr el test y verificar que PASA**

Run: `bash scripts/run-host-tests.sh test_central_otos_ingest`
Expected: `test_central_otos_ingest  4  0  0  OK`.

- [ ] **Step 5: Agregar storage + accessors de OTOS en `world_model.h`**

Agregar antes del cierre `}  // namespace iitasoccer`, después de la sección de línea:
```cpp
// === OTOS directo de DOWN (Capa 1 broadcast) ===
// La pose de cancha AUTORITATIVA sigue siendo la del WorldSnapshot del TOP
// (world_model_get_my_*). El OTOS directo es SOLO para control de movimiento
// (drive-straight / patear derecho), que se cablea en Capa 2.
void world_model_apply_otos_pose(const Pose2D& pose);
void world_model_apply_otos_vel(const Velocity2D& vel);
bool world_model_otos_is_fresh();

float   world_model_get_otos_x_mm();
float   world_model_get_otos_y_mm();
float   world_model_get_otos_heading_deg();
float   world_model_get_otos_vx_mm_s();
float   world_model_get_otos_vy_mm_s();
float   world_model_get_otos_omega_deg_s();
uint8_t world_model_get_otos_slip();
uint8_t world_model_otos_pose_confidence();
```

- [ ] **Step 6: Implementar en `world_model.cpp`**

En el namespace anónimo (junto a `g_snap`/`g_line`), agregar:
```cpp
Pose2D     g_otos_pose{};
Velocity2D g_otos_vel{};
uint32_t   g_otos_last_ms = 0;
constexpr uint32_t OTOS_TIMEOUT_MS = 500;
```

En `world_model_init()`, agregar al final (antes del cierre):
```cpp
    g_otos_pose = Pose2D{};
    g_otos_vel  = Velocity2D{};
    g_otos_last_ms = 0;
```

Agregar las definiciones (antes del cierre del namespace `iitasoccer`):
```cpp
void world_model_apply_otos_pose(const Pose2D& pose) { g_otos_pose = pose; g_otos_last_ms = millis(); }
void world_model_apply_otos_vel(const Velocity2D& vel) { g_otos_vel = vel; }  // freshness va con la pose (llegan juntas a 100 Hz)
bool world_model_otos_is_fresh() { return g_otos_last_ms > 0 && (millis() - g_otos_last_ms) < OTOS_TIMEOUT_MS; }

float   world_model_get_otos_x_mm()         { return static_cast<float>(g_otos_pose.x_mm); }
float   world_model_get_otos_y_mm()         { return static_cast<float>(g_otos_pose.y_mm); }
float   world_model_get_otos_heading_deg()  { return g_otos_pose.heading_centideg / 100.0f; }
float   world_model_get_otos_vx_mm_s()      { return static_cast<float>(g_otos_vel.vx_mm_s); }
float   world_model_get_otos_vy_mm_s()      { return static_cast<float>(g_otos_vel.vy_mm_s); }
float   world_model_get_otos_omega_deg_s()  { return g_otos_vel.omega_centideg_s / 100.0f; }
uint8_t world_model_get_otos_slip()         { return g_otos_vel.slip_estimate; }
uint8_t world_model_otos_pose_confidence()  { return g_otos_pose.confidence; }
```

- [ ] **Step 7: Ingerir 0x11/0x12 en `comm_down.cpp` (CENTRAL)**

Agregar el include (junto a `#include "line_view.h"`):
```cpp
#include "pose_view.h"
```

En `handle_frame`, reemplazar el bloque final actual:
```cpp
    LineStatusV2 ls{};
    if (lsv2_from_frame(f, ls)) {
        world_model_apply_line(ls);
    }
```
por:
```cpp
    LineStatusV2 ls{};
    if (lsv2_from_frame(f, ls)) { world_model_apply_line(ls); return; }
    Pose2D pose{};
    if (pose_from_frame(f, pose)) { world_model_apply_otos_pose(pose); return; }
    Velocity2D vel{};
    if (vel_from_frame(f, vel)) { world_model_apply_otos_vel(vel); return; }
```
(El conteo de SEQ/`g_frames_lost` de arriba queda intacto: cuenta los 3 tipos, que es lo correcto para la salud del enlace.)

- [ ] **Step 8: Compilar CENTRAL**

Run: `pio run -e central_robot1 -e central_robot2`
Expected: ambos `SUCCESS`.

- [ ] **Step 9: Commit**

```bash
git add "software/teensy/Soccer 2026/src/shared/pose_view.h" "software/teensy/Soccer 2026/test/test_central_otos_ingest/test_main.cpp" "software/teensy/Soccer 2026/src/central/world_model.h" "software/teensy/Soccer 2026/src/central/world_model.cpp" "software/teensy/Soccer 2026/src/central/comm_down.cpp"
git commit -m "feat(central): WP-1B ingesta OTOS directo (pose_view.h + world_model + comm_down) + test"
```

---

### Task WP-1C: TOP recibe bien la línea (`LineStatusV2`)

**Files:**
- Modify: `software/teensy/Soccer 2026/src/top/comm_down.cpp`
- Modify: `software/teensy/Soccer 2026/src/top/comm_down.h`

**Por qué:** hoy el TOP guarda la línea en `LineStatus` (5 B) y el case `LINE_URGENT` exige `payload_len == sizeof(LineStatus)`, pero DOWN manda `LineStatusV2` (16 B) → el TOP la **descarta en silencio**. Con WP-1A el TOP ahora recibe la línea; hay que aceptarla bien. (Nadie consume `comm_down_get_line_status()` en el TOP hoy, así que cambiar el tipo es seguro.)

- [ ] **Step 1: Migrar el storage y el handler en `comm_down.cpp` (TOP)**

Agregar el include (junto a `#include "proto.h"`):
```cpp
#include "line_view.h"
```

Cambiar la declaración del estado de línea:
```cpp
LineStatus  g_line{};
```
por:
```cpp
LineStatusV2 g_line{};
```

Reemplazar el `case MsgType::LINE_URGENT:` actual por:
```cpp
        case MsgType::LINE_URGENT: {
            LineStatusV2 ls{};
            if (lsv2_from_frame(f, ls)) {     // valida tipo + tamaño (16) + schema
                g_line = ls;
                g_line_last_rx_ms = millis();
            }
            break;
        }
```

Cambiar la firma del getter:
```cpp
const LineStatus& comm_down_get_line_status() { return g_line; }
```
por:
```cpp
const LineStatusV2& comm_down_get_line_status() { return g_line; }
```

- [ ] **Step 2: Actualizar la firma en `comm_down.h` (TOP)**

Cambiar:
```cpp
const LineStatus& comm_down_get_line_status();
```
por:
```cpp
const LineStatusV2& comm_down_get_line_status();
```
(Actualizar también el comentario de cabecera que dice "LineStatus (ángulo línea...)" para que diga `LineStatusV2`.)

- [ ] **Step 3: Compilar TOP**

Run: `pio run -e top_robot1 -e top_robot2`
Expected: ambos `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add "software/teensy/Soccer 2026/src/top/comm_down.cpp" "software/teensy/Soccer 2026/src/top/comm_down.h"
git commit -m "fix(top): WP-1C recibir la linea como LineStatusV2 (lsv2_from_frame), no LineStatus viejo"
```

---

### Task WP-1E: Sync de documentación al broadcast simétrico

**Files (modificar):**
- `docs/firmware/CONTRATO-DATOS-DOWN.md`
- `docs/firmware/CONTRATO-DATOS-CENTRAL.md`
- `docs/firmware/CONTRATO-DATOS-TOP.md`
- `docs/ARQUITECTURA-3-PLACAS-2026.md`
- `hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md`
- `docs/ESTADO-ACTUAL.md`

- [ ] **Step 1: `CONTRATO-DATOS-DOWN.md`** — En §2 (catálogo de mensajes), cambiar la columna "Dir" para que `LINE_URGENT`(0x10), `DOWN_OTOS_POSE`(0x11) y `DOWN_OTOS_VEL`(0x12) listen destino **CENTRAL + TOP** (los 3 a ambas). En §1 (enlace físico), aclarar que cada UART de DOWN (Serial1→CENTRAL, Serial5→TOP) lleva ahora la **unión** (línea @200 Hz + pose/vel @100 Hz). Agregar nota: SEQ monótono por enlace (`down_tx`).

- [ ] **Step 2: `CONTRATO-DATOS-CENTRAL.md`** — En §0.1 (diagrama) agregar `DOWN → CENTRAL: Pose2D + Velocity2D`. En §0.3 cambiar la fila de odometría a "DOWN → TOP **y** CENTRAL". En §8 (catálogo) agregar las filas de `DOWN_OTOS_POSE`/`DOWN_OTOS_VEL` entrantes. Aclarar que el OTOS directo es **solo para control de movimiento** (Capa 2); la pose de cancha autoritativa sigue siendo el WorldSnapshot del TOP.

- [ ] **Step 3: `CONTRATO-DATOS-TOP.md`** — En §2/§5 marcar que `LINE_URGENT`/`LineStatusV2` ahora es un entrante **oficial** del TOP (ya no "legacy / no debería llegar"). Aclarar que el TOP la recibe y cachea pero todavía no la consume (no hay world_model en TOP).

- [ ] **Step 4: `ARQUITECTURA-3-PLACAS-2026.md`** — En "PLACA ABAJO → Outputs", "Topología" y "Protocolo de mensajes": reflejar que **ambos** enlaces de DOWN llevan línea + OTOS. **NO TOCAR** el "Mapa de flujo de datos" que dice WorldSnapshot **24 B** con su banner (contradicción intencional 24/27 B).

- [ ] **Step 5: `MAPA-CONEXIONES-3-PLACAS.md`** — En §1 (tabla DOWN) y §2 (enlaces), cambiar las columnas "Para qué": Serial1→CENTRAL y Serial5→TOP llevan ahora "línea + odometría OTOS". El cableado físico (pines) NO cambia.

- [ ] **Step 6: `ESTADO-ACTUAL.md`** — En la sección "DOWN", actualizar la nota "DOWN→CENTRAL lleva SOLO la línea" → ahora difunde línea + OTOS a ambas (Capa 1 del broadcast). Mencionar el spec/plan. Actualizar `last-updated-by`.

- [ ] **Step 7: NO TOCAR (contradicciones intencionales).** Verificar que NO se modificaron: WorldSnapshot 24 B vs 27 B, doble cadena de línea de DOWN, conflicto 7/8 resuelto-vs-pendiente (registro histórico en el cuerpo de ESTADO-ACTUAL), `LineStatus` v1 (5 B) vs v2 (16 B) en los contratos. Si un cambio las roza, parar y consultar al coach.

- [ ] **Step 8: Commit**

```bash
git add docs/firmware/CONTRATO-DATOS-DOWN.md docs/firmware/CONTRATO-DATOS-CENTRAL.md docs/firmware/CONTRATO-DATOS-TOP.md docs/ARQUITECTURA-3-PLACAS-2026.md hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md docs/ESTADO-ACTUAL.md
git commit -m "docs: WP-1E sync contratos/arquitectura/mapa al broadcast simetrico DOWN (Capa 1)"
```

---

### Task WP-1F: Integración + gate completo (coach)

**Files:** ninguno nuevo — verificación de todo lo integrado en `main`.

- [ ] **Step 1: Integrar las worktrees de WP-1A/1B/1C/1E a `main`** (lo hace el coach; las worktrees de agentes no pushean a main).

- [ ] **Step 2: Build de los 5 envs tocados**

Run (desde `software/teensy/Soccer 2026`): `pio run -e down -e down_debug -e top_robot1 -e top_robot2 -e central_robot1 -e central_robot2`
Expected: todos `SUCCESS`.

- [ ] **Step 3: Suite host-native completa**

Run: `bash scripts/run-host-tests.sh`
Expected: `FAIL: 0`, incluyendo `test_central_otos_ingest ... OK`. (Total esperado ≥ 291 tests = 287 previos + 4 nuevos.)

- [ ] **Step 4: `git fetch` + merge `origin/main` + push** (coach, con el gate ya verde).

```bash
git fetch origin && git merge origin/main && git push origin main
```

---

## Self-Review

**1. Spec coverage (vs §6.1 Capa 1 del spec):**
- "DOWN difunde los 3 frames a ambos UART con SEQ por enlace" → WP-1A (`down_tx`). ✓
- "CENTRAL agrega handlers 0x11/0x12 + storage en world_model + accessors" → WP-1B. ✓
- "TOP arregla el handler de línea (LineStatusV2 vía lsv2_from_frame)" → WP-1C. ✓
- "Tests host: ingest OTOS en CENTRAL" → WP-1B Step 1-4 (`test_central_otos_ingest`). ✓
- "Sin cambios de tamaño de contrato / cero static_assert tocado" → ningún WP modifica `types.h`. ✓
- "Docs sync sin tocar contradicciones intencionales" → WP-1E (con Step 7 de guardia). ✓
- Gap conocido y aceptado: `down_tx` y el fix del TOP son Arduino-glue (compile-only + banco), no host-test — declarado en "Nota de testabilidad".

**2. Placeholder scan:** sin "TBD"/"TODO"/"add error handling". Todo step de código muestra el código. Los steps de docs (WP-1E) dan la instrucción concreta por archivo/sección. ✓

**3. Type consistency:** `down_tx_broadcast_line/pose/vel` y `down_tx_get_sent/dropped(link)` se usan igual en `down_tx.{h,cpp}` y en `comm_central`/`comm_top`. `pose_from_frame`/`vel_from_frame` (definidos en `pose_view.h` Step 3) se usan en el test (Step 1) y en `comm_down.cpp` (Step 7). `world_model_apply_otos_pose/vel`, `world_model_get_otos_*` declarados en `world_model.h` (Step 5) = definidos en `world_model.cpp` (Step 6) = llamados en `comm_down.cpp` (Step 7). `comm_down_get_line_status()` cambia a `const LineStatusV2&` en `.h` y `.cpp` (WP-1C). ✓

## Dependencias entre tareas
- WP-1A, WP-1B, WP-1C, WP-1E: **independientes** (archivos disjuntos) → paralelas.
- WP-1F: **después** de integrar 1A/1B/1C/1E.
- Validación de banco (TOP↔CENTRAL ve `snap_fresh` + OTOS fresco; DOWN→TOP línea V2): sesión de hardware aparte (no en este plan).
