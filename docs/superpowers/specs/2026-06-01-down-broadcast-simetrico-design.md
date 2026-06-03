---
title: "Diseño — DOWN difunde datos completos (línea + OTOS) a CENTRAL y TOP (broadcast simétrico)"
date: 2026-06-01
status: aprobado-pendiente-implementacion
tipo: spec-diseño
area: firmware / contratos inter-placa
autor: "Claude (coach) + Gustavo Viollaz (director) — brainstorm 2026-06-01"
relacionado:
  - docs/firmware/CONTRATO-DATOS-DOWN.md
  - docs/firmware/CONTRATO-DATOS-CENTRAL.md
  - docs/firmware/ANALISIS-COMM-DOWN-CENTRAL-2026-05-31.md
  - docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md
---

# Broadcast simétrico de DOWN — diseño

> **Estado:** diseño aprobado por Gustavo (brainstorm 2026-06-01). Ejecución **por etapas**
> (Capa 1 primero, en paralelo con agentes). Este doc es la fuente de verdad del diseño;
> el plan de implementación granular sale de acá vía `writing-plans`.

## 1. Contexto y problema

Hoy la placa DOWN parte sus datos en **dos enlaces asimétricos**, cada uno con su payload:

- **DOWN → CENTRAL** (`Serial1` DOWN pin1 → `Serial1` CENTRAL pin0, 230400): **solo** la línea
  (`LINE_URGENT` / `LineStatusV2`, 16 B) @ 200 Hz. "Bus de emergencia".
- **DOWN → TOP** (`Serial5` DOWN pin20 → `Serial1` TOP pin0, 230400): **solo** odometría
  (`DOWN_OTOS_POSE` 0x11, 7 B + `DOWN_OTOS_VEL` 0x12, 7 B) @ 100 Hz.

Consecuencia: CENTRAL **nunca** ve el OTOS directo (su pose sale del WorldSnapshot del TOP), y
el TOP recibe el OTOS pero lo **descarta** (su `build_snapshot` usa ToF+IMU). El robot no puede
"patear derecho" con OTOS desde el cerebro, y el seguimiento lateral del arquero no tiene señal
perpendicular real.

**Objetivo del usuario:** que DOWN **difunda lo mismo a ambas placas** — línea (si hay y dónde),
señal para seguimiento lateral del arquero, y OTOS para patear derecho / usos futuros.

## 2. Objetivos / No-objetivos

**Objetivos:**
- DOWN difunde el **conjunto completo** (línea + pose OTOS + vel OTOS) a **CENTRAL y TOP**.
- CENTRAL **consume** OTOS directo para control de movimiento (drive-straight / patear derecho).
- El arquero obtiene una señal de **`cross_track` real** (distancia perpendicular a la línea)
  para desplazarse **paralelo a la línea lateral**.
- No romper los contratos vivos salvo donde el diseño lo requiera explícitamente (Capa 3).

**No-objetivos (por ahora):**
- El TOP **no** cambia su localización: el WorldSnapshot (ToF+IMU) sigue siendo la **pose
  autoritativa de cancha**. Fusionar OTOS en el TOP queda como opción futura (anotada).
- CENTRAL **no** deja de depender del TOP para la pose de cancha (el OTOS directo es **solo**
  para control de movimiento, no para localización absoluta).
- No se archiva ninguna de las dos cadenas de línea de DOWN (deuda intencional, decisión
  post-Incheon).

## 3. Decisiones de diseño (lockeadas en el brainstorm)

| # | Decisión | Valor elegido |
|---|----------|---------------|
| D1 | **Transporte** | **Opción A**: frames separados reusando los MsgType existentes (0x10/0x11/0x12), difundidos a **ambos** UART. Aditivo, sin cambiar tamaños de struct. |
| D2 | **Autoridad de pose** | El **WorldSnapshot del TOP** sigue autoritativo para la pose de cancha. CENTRAL usa OTOS **directo solo para control de movimiento**. |
| D3 | **TOP** | Recibe todo (incl. línea, arreglando el handler roto) pero **no** cambia su localización por ahora. |
| D4 | **Alcance** | Diseño completo en **3 capas**; **ejecución por etapas** (Capa 1 ya, en paralelo; 2 y 3 olas siguientes). |
| D5 | **GK / cross_track** | `cross_track` = **distancia perpendicular** del centro del robot a la línea blanca; el arquero la usa para **mantener distancia y avanzar paralelo** a la línea lateral. **Capa 3.** |
| D6 | **Schema** | **OK bumpear `LSV2_SCHEMA`** en Capa 3 (señaliza que `cross_track`/`penetration` pasan a ser valores reales). |

## 4. Estudio de impacto (síntesis del análisis multi-agente, 2026-06-01)

> Generado por 4 agentes read-only (consumidores CENTRAL, consumidores TOP, tests/contratos,
> documentación). Detalle completo en el historial de la sesión; acá el resumen accionable.

### 4.1 Realidad de consumidores HOY

- **CENTRAL** (`src/central/`): `comm_down.cpp` decodifica **solo** `LINE_URGENT` (vía
  `lsv2_from_frame`); todo otro tipo se ignora. `world_model` guarda `g_snap` (TOP) + `g_line`
  (DOWN); **no hay storage de OTOS**. La pose que expone (`world_model_get_my_*`) sale de
  `g_snap`. `strategy.cpp` lee línea en `:107,141,177,182,326,341,356-359,385-388` y heading en
  `:131,234,296,431,433`; **nunca** lee x/y ni velocidad. Bypass de freno de emergencia en
  `main_central.cpp:127`.
- **TOP** (`src/top/`): `comm_down.cpp` tiene `switch` con 0x10/0x11/0x12, pero el case de línea
  hace `memcpy` a `LineStatus` **legacy (~5 B)** ≠ `LineStatusV2` (16 B) → **aunque DOWN le mande
  la línea hoy, la descarta por tamaño**. La pose/vel OTOS se cachean pero **no se consumen**:
  `build_snapshot` usa `localization_runtime` (ToF+IMU). Únicos lectores: 2 prints de debug
  (`main_top.cpp:187,189`). **El TOP no tiene `world_model` ni FSM** → no hay quién use la línea.
- **CENTRAL ⇄ OTOS:** cero acceso hoy. Sumarlo es **aditivo** (handlers 0x11/0x12 + storage en
  `world_model`). No hay colisión de MsgType (0x11/0x12 ya existen, hoy ignorados).

### 4.2 "Gotchas" que condicionan el diseño

1. El handler de línea del TOP está **roto por contrato** (`LineStatus` 5 B vs `LineStatusV2`
   16 B) → migrar a `lsv2_from_frame` como CENTRAL.
2. **SEQ:** CENTRAL lleva **un** contador de SEQ para gap-detection asumiendo **un solo stream**.
   Si DOWN intercala 0x10/0x11/0x12 en el mismo UART, DOWN debe usar **un SEQ monótono único por
   enlace** (no por tipo) o el `frames_lost` se rompe.
3. El TOP recibe OTOS pero lo desperdicia: la odometría "muere" ahí (post-Incheon
   `TODO_DIFFERENTIAL_OTOS`).

### 4.3 Contratos, tests y `static_assert`

- Solo **2 `static_assert`** de tamaño: `types.h:126` (`WorldSnapshot==27`) y `types.h:143`
  (`LineStatusV2==16`). `scripts/run-host-tests.sh` linkea **toda** `src/shared` en cada suite →
  un assert desactualizado = **build roto global**.
- `test_down_encode`: fija el **frame byte-a-byte** (golden + CRC) y `sizeof(LineStatusV2)==16`.
- `test_central_line_ingest`: roundtrip + tests de **rechazo** (tamaño viejo, tipo equivocado,
  schema equivocado).
- `test_central_contract`: `sizeof(WorldSnapshot)==27` + campos `ball_vx/vy`.
- `test_proto`: framing genérico (overhead, `PROTO_MAX_PAYLOAD=32`, rechazo LEN>max).
- `test_central_motion` / `test_central_trajectory`: **no** incluyen `types.h` → no afectados.
- `down_encode.cpp` usa `sizeof` (se adapta si crece el struct); `line_view.h:28` exige tamaño
  **exacto** (fail-safe entre flasheos distintos).
- **Margen de payload (32 B):** línea 16 + pose 7 + vel 7 = 30 B si se combinara (no se combina).
  Como frames separados, cada uno entra holgado. **WorldSnapshot 27 B + cualquier OTOS > 32 B**
  ⇒ por eso NO se mete OTOS en el snapshot (justifica D1=A).

## 5. Arquitectura objetivo

```
                 ┌───────── DOWN (Teensy 4.0) ─────────┐
                 │  line_ring 1kHz   │   OTOS 100Hz     │
                 │        │ down_model→LineStatusV2     │
                 │        │          │ Pose2D/Velocity2D│
                 │   ┌────▼──────────▼────┐             │
                 │   │  down_tx (broadcast)│  SEQ/enlace │
                 │   └──┬───────────────┬─┘             │
                 └──────│───────────────│───────────────┘
        Serial1 (pin1)  │               │  Serial5 (pin20)
       línea+pose+vel   ▼               ▼  línea+pose+vel
        ┌───────────────────┐   ┌────────────────────────┐
        │ CENTRAL (T4.1)    │   │ TOP (T4.0)             │
        │ comm_down:        │   │ comm_down:             │
        │  0x10→line        │   │  0x10→line (FIX V2)    │
        │  0x11→otos_pose ★ │   │  0x11→pose (cache)     │
        │  0x12→otos_vel  ★ │   │  0x12→vel  (cache)     │
        │ world_model:      │   │ build_snapshot:        │
        │  línea + OTOS ★   │   │  ToF+IMU (autoritativo)│
        │ strategy:         │   │  (OTOS NO se fusiona)  │
        │  drive-straight ★ │   │                        │
        │  via OTOS directo │   │ WorldSnapshot ──Serial7─┼──► CENTRAL (pose de cancha)
        └───────────────────┘   └────────────────────────┘
   ★ = nuevo (Capas 1–2)
```

Cada enlace lleva la **unión**: `LINE_URGENT` @200 Hz + `DOWN_OTOS_POSE` @100 Hz +
`DOWN_OTOS_VEL` @100 Hz. Carga estimada por enlace: 200·23 B + 100·14 B + 100·14 B ≈ **7,4 kB/s**
≈ **32 %** de 23040 B/s (230400 baud). Holgado.

## 6. Diseño por capas

### 6.1 Capa 1 — Transporte (ola 1; ejecutar ya, paralelizable)

**Meta:** el dato DISPONIBLE y bien recibido en ambas placas. Sin consumirlo aún.

**DOWN** (`src/down/`):
- Introducir una abstracción de TX por enlace: `DownLink { Stream& uart; uint8_t seq; counters }`,
  con dos instancias (CENTRAL=`Serial1`, TOP=`Serial5`).
- `down_tx_broadcast(type, payload, len)`: codifica el frame **una vez por enlace** con el **SEQ
  propio de ese enlace** (monótono, compartido entre los 3 tipos) y escribe con el guard
  `availableForWrite` (backpressure). Mantener contadores `sent/dropped` por enlace.
- El scheduler de `main_down.cpp` queda igual en cadencia: línea @200 Hz, pose+vel @100 Hz —
  pero **cada send va a los dos enlaces**.
- Migrar `comm_central`/`comm_top` (DOWN) a usar `down_tx` (eliminar los SEQ duplicados).

**CENTRAL** (`src/central/`):
- `comm_down.cpp`: refactor `handle_frame` a `switch(f.type)` manteniendo `LINE_URGENT` primero
  (ruta crítica). Agregar `DOWN_OTOS_POSE`/`DOWN_OTOS_VEL` → `memcpy` con guard de tamaño →
  `world_model_apply_otos_pose/vel`.
- `world_model.{h,cpp}`: nuevos `Pose2D g_otos_pose`, `Velocity2D g_otos_vel`, `g_otos_last_ms`;
  `apply_otos_pose/vel` (set + timestamp); accessors de lectura + `world_model_otos_is_fresh()`
  (timeout propio); reset en `world_model_init`.
- **No** consumir en `strategy` todavía (eso es Capa 2).

**TOP** (`src/top/`):
- `comm_down.cpp`: arreglar el case `LINE_URGENT` → storage `LineStatusV2` + `lsv2_from_frame`
  (incluir `line_view.h`), igual que CENTRAL. Ajustar firma del getter a `const LineStatusV2&`.
  Mantener cache + frescura. (Sin consumidor: solo deja de descartarse.)

**Contratos:** **sin cambios de tamaño** (reusa 0x10/0x11/0x12). Cero `static_assert` tocado.

### 6.2 Capa 2 — Consumidores (ola 2)

**Meta:** que CENTRAL USE el OTOS para "patear derecho".

- `src/shared/pose_view.h` (nuevo, puro, testeable host): helpers de interpretación de
  `Pose2D`/`Velocity2D` (N/A handling, confianza, conversión de unidades), espejo de `line_view.h`.
- `world_model` expone pose/vel OTOS interpretadas.
- `strategy.cpp` / control de movimiento: usar el OTOS directo para **drive-straight** y
  **corrección al patear** (baja latencia, sin round-trip por el TOP). Definir el blend con el
  `HeadingPID` actual (p.ej. heading del snapshot para orientación de cancha + vx/vy OTOS para
  mantener trayectoria recta).
- **TOP:** fuera de scope (D3). Anotado como opción futura: fusionar OTOS en `localization_runtime`.

### 6.3 Capa 3 — Cómputo real de campos (ola 3)

**Meta:** seguimiento lateral REAL del arquero.

- **`cross_track_mm` real** en `down_model` (usando `sensor_geometry.h` / `SENSOR_POS[]`):
  distancia perpendicular del centro del robot (0,0) a la recta de la línea blanca detectada.
  Procedimiento: centroide de sensores en blanco = punto sobre la línea; `line_angle` = dirección
  de la recta; `cross_track` = distancia firmada de (0,0) a esa recta. **+** = línea hacia un lado,
  **−** = el otro (convención a fijar y documentar).
- **`penetration_mm` real** (mm, no conteo): profundidad de penetración del robot en la región de
  línea, desde la geometría de sensores.
- **Comportamiento GK** (consume Capa 3): el arquero mantiene `cross_track` ≈ setpoint y usa
  `line_angle` para quedar **paralelo**, pudiendo **trasladarse a lo largo** de la línea lateral
  (strafe). Reemplaza/complementa el PID actual basado en `depth`.
- **Schema:** bumpear `LSV2_SCHEMA` (D6). Como `cross_track_mm`/`penetration_mm` **ya existen** en
  `LineStatusV2` (offsets 6–9), **no cambia el `sizeof`** ni los `static_assert`. Lo que cambia es
  el **cómputo** (afecta `test_down_model`) y, si se sube schema, los asserts de `LSV2_SCHEMA==2`
  en `test_down_encode`/`test_central_line_ingest`/`test_down_model` (actualizar a la versión nueva).
  El **golden de `test_down_encode`** solo cambia si cambia el ejemplo (no la estructura).

## 7. Contrato de datos (resumen)

| Mensaje | Tipo | Tamaño | Rate | Destinos (nuevo) | Notas |
|---------|------|--------|------|------------------|-------|
| `LineStatusV2` | `LINE_URGENT` 0x10 | 16 B | 200 Hz | **CENTRAL + TOP** | `cross_track`/`penetration` reales en Capa 3 |
| `Pose2D` | `DOWN_OTOS_POSE` 0x11 | 7 B | 100 Hz | **CENTRAL + TOP** | x,y (int16 mm) + heading (int16 centideg) + flags(u8) |
| `Velocity2D` | `DOWN_OTOS_VEL` 0x12 | 7 B | 100 Hz | **CENTRAL + TOP** | vx,vy + omega + flags |

- **SEQ:** monótono **por enlace**, compartido entre los 3 tipos.
- **Frame proto:** START/LEN/TYPE/SEQ/payload/CRC16/END (overhead 7 B). Sin cambios.
- **Precisión OTOS:** ver `CONTRATO-DATOS-DOWN.md §4` (mm / centideg). No cambia en este diseño.

## 8. Plan de tests (host-native, `pio test -e test_native` + `run-host-tests.sh`)

- **Capa 1:**
  - `test_down_tx` (nuevo): el broadcast emite los 3 tipos a **ambos** enlaces con SEQ correcto
    por enlace.
  - `test_central_otos_ingest` (nuevo, espejo de `test_central_line_ingest`): decode 0x11/0x12 →
    `world_model` → accessors + freshness + rechazo de tamaño/tipo equivocado.
  - `test_top_line_ingest` (nuevo): el TOP acepta `LineStatusV2` por `lsv2_from_frame` y rechaza
    el tamaño viejo.
  - **No** tocar el golden de `test_down_encode`.
- **Capa 2:** `test_pose_view` (interpretación pura) + tests de drive-straight (lógica pura).
- **Capa 3:** `test_down_model` extendido (cross_track/penetration reales con geometría conocida);
  actualizar asserts de `LSV2_SCHEMA` si se sube.
- **Regla de oro:** cualquier cambio de tamaño/schema ⇒ actualizar `static_assert` (`types.h`) en
  el mismo commit, o **toda** la suite rompe en build.

## 9. Documentación a actualizar / contradicciones a NO tocar

**Actualizar (primarios):**
- `docs/firmware/CONTRATO-DATOS-DOWN.md` (§1 enlaces, §2 catálogo de mensajes: ambos destinos).
- `docs/firmware/CONTRATO-DATOS-CENTRAL.md` (§0 diagrama, §0.3 odometría → también CENTRAL, §8
  catálogo: pose/vel entrantes).
- `docs/firmware/CONTRATO-DATOS-TOP.md` (línea entrante ahora oficial, no "legacy/no debería").
- `docs/ARQUITECTURA-3-PLACAS-2026.md` (topología, outputs de ABAJO, tabla de mensajes, mapa de flujo).
- `hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md` (columnas "Para qué" de los 2 enlaces DOWN).
- `docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md` (§6 por enlace).
- `docs/firmware/ANALISIS-COMM-DOWN-CENTRAL-2026-05-31.md` (extender: timing del nuevo payload).
- `docs/ESTADO-ACTUAL.md` + `docs/FUENTES-DE-VERDAD.md` (filas que apuntan a docs cambiados).
- Packs (`*-board-pack/`): **snapshots**, re-sincronizar **después** del canónico.

**⚠️ NO homogeneizar (contradicciones intencionales):**
- WorldSnapshot **24 B vs 27 B** (ARQUITECTURA superado con banner vs CONTRATO canónico).
- **Doble cadena de línea** de DOWN (`line_ring` vs `down_model`) — decisión binaria post-Incheon.
- "Línea a TOP legacy / no debería llegar" — **este diseño la vuelve oficial**; actualizar con
  intención (no como 'fix' silencioso).
- Conflicto **7/8 resuelto vs pendiente** — registro histórico.
- `LineStatus` v1 (5 B) vs v2 (16 B) en contratos — drift ya trackeado (CONTRACT-02).

## 10. Paquetes de trabajo (paralelización para agentes)

**Ola 1 (paralela de entrada — placas/archivos disjuntos):**

| WP | Paquete | Toca (archivos) | Depende de | Salida verificable |
|----|---------|-----------------|-----------|--------------------|
| **1A** | DOWN broadcast (`down_tx`, SEQ/enlace, backpressure) | `src/down/comm_central.cpp`, `comm_top.cpp`, `main_down.cpp`, nuevo `src/down/down_tx.{h,cpp}` | contrato (fijo) | `pio run -e down` OK + `test_down_tx` |
| **1B** | CENTRAL ingest OTOS | `src/central/comm_down.{cpp,h}`, `world_model.{cpp,h}` | contrato (fijo) | `pio run -e central_robot1/2` + `test_central_otos_ingest` |
| **1C** | TOP fix ingest línea (V2) | `src/top/comm_down.{cpp,h}` | contrato (fijo) | `pio run -e top_robot1/2` + `test_top_line_ingest` |
| **1D** | Tests host-native (1A/1B/1C) | `test/test_down_tx/`, `test/test_central_otos_ingest/`, `test/test_top_line_ingest/` | interfaces de 1A/1B/1C | `run-host-tests.sh` verde |
| **1E** | Docs sync (sin tocar contradicciones) | `docs/`, `hardware/electronics/` | este spec | revisión humana |

1A/1B/1C/1E arrancan en paralelo (no comparten archivos). 1D se escribe contra el contrato y se
valida al integrar. **Regla:** las worktrees de agentes NO mergean a `main`; el coach integra con
`git fetch` + merge + gate (`pio test -e test_native` + builds de los envs tocados).

**Ola 2 (secuencial tras ola 1):** WP-2A CENTRAL consume OTOS (`pose_view.h` + strategy
drive-straight) + WP-2B tests.

**Ola 3 (secuencial tras ola 2):** WP-3A `cross_track`/`penetration` reales en `down_model` +
schema bump + WP-3B GK strafe paralelo + WP-3C tests.

## 11. Riesgos y mitigaciones

| Riesgo | Mitigación |
|--------|-----------|
| SEQ compartido mal → `frames_lost` basura | SEQ monótono **por enlace** en `down_tx`; test dedicado. |
| Backpressure: 400 frames/s/enlace roban ciclos al `line_ring` 1 kHz | Guard `availableForWrite` + drop con contador (ya es el patrón vigente). |
| Dos fuentes de pose en CENTRAL (snapshot vs OTOS) → ambigüedad | D2: snapshot = autoritativo de cancha; OTOS = **solo** control de movimiento. Documentar en accessors. |
| Cambiar `LineStatusV2` rompe build global | Capa 3 llena campos existentes (sin cambiar `sizeof`); actualizar `static_assert`/schema en el mismo commit. |
| Tocar contradicciones intencionales | Lista §9 explícita; revisión humana en WP-1E. |
| Validación en HW pendiente (enlaces no cableados) | Capa 1 es host-testeable; la validación de banco va con el bring-up de enlaces (sesión aparte). |

## 12. Plan de ejecución por etapas

1. **Ola 1 (Capa 1):** fan-out WP-1A..1E con agentes en worktrees. Coach integra a `main` con el
   gate. Resultado: dato disponible en ambas placas, host-tests verdes.
2. **Validación de banco** (humano, cuando estén cableados los enlaces): confirmar que CENTRAL ve
   pose/vel OTOS frescas y el TOP recibe línea V2.
3. **Ola 2 (Capa 2):** CENTRAL consume OTOS (drive-straight / patear derecho).
4. **Ola 3 (Capa 3):** `cross_track`/`penetration` reales + GK strafe paralelo + schema bump.

## 13. Criterios de aceptación

- **Capa 1:** `pio run` OK en `down`, `central_robot1/2`, `top_robot1/2`; `run-host-tests.sh` verde
  incluyendo los 3 tests nuevos; DOWN emite los 3 tipos a ambos enlaces con SEQ por enlace;
  CENTRAL expone pose/vel OTOS frescas; TOP acepta línea V2 (sin descartarla por tamaño).
- **Capa 2:** CENTRAL ejecuta drive-straight usando OTOS directo (validable en banco).
- **Capa 3:** `cross_track_mm` reporta distancia perpendicular real (validado con geometría
  conocida en test + banco); el arquero se desplaza paralelo a la línea lateral.
- **Docs:** contratos/arquitectura/mapa coherentes con el código; contradicciones intencionales
  intactas.
