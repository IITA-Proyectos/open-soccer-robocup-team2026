---
task: 208
titulo: "GUI del monitor TOP (gui_top.py): mostrar telemetría v2 (per-cámara + OTOS + vector de escape) + controles de config A2.1"
fecha: 2026-06-14
asignado: agente/equipo dueño de la app monitor-base (lado GUI)
prioridad: P1
pedido-por: Gustavo Viollaz
relacionada: TASK-205 (app monitoreo TOP), TASK-206 (config persistente), TELEMETRIA-TOP.md
estado: pending
---

# TASK-208 — GUI del monitor TOP: telemetría v2 + controles de config

> **Para quién:** el agente/persona que trabaja la app `tools/monitor-base/` (lado GUI). El
> **firmware y el parser ya están hechos y en `main`** (A1 = `4226382`, A2.1 = `a094c8a`); lo
> ÚNICO que falta del lado app es **DIBUJAR** lo nuevo en `gui_top.py` y agregar botones de config.
>
> **Límite de carril (no chocar):** esta TASK toca SOLO `tools/monitor-base/monitor_base/gui_top.py`
> (y, si hace falta, helpers de dibujo nuevos). **NO toca el firmware** (`src/`) ni el parser
> `protocol_top.py` ni el contrato — ya están listos. El agente del firmware (Claude/coach) **no
> va a tocar `gui_top.py`**, así que es todo tuyo.

## 1. Estado actual (qué YA está y qué NO)

✅ **Hecho (firmware + parser, en `main`):**
- El frame TOP es **schema v2**: agrega bloques `camf`/`camb` (detecciones POR CÁMARA) y
  `base`/`line` (OTOS + línea + vector de escape de la base DOWN). Contrato completo:
  [`docs/firmware/TELEMETRIA-TOP.md`](../software/teensy/Soccer%202026/docs/firmware/TELEMETRIA-TOP.md).
- El parser `protocol_top.py` **ya deserializa** todo eso: `TopFrame` ahora tiene `.camf`, `.camb`
  (dataclass `CamPer`), `.base` (dataclass `Base`), `.line` (dataclass `Line`). El simulador
  (`simulator_top.py`) emite v2. `pytest tools/monitor-base` = 82/82 verde.
- Config A2.1: el firmware acepta comandos `CAM/BNO/US/TOF/CFG` (abajo) y los persiste en EEPROM.

❌ **Falta (ESTA task):** `gui_top.py` HOY solo dibuja el **fusionado** (radar + paneles cámara/IMU/
ToF/snapshot). NO muestra `camf`/`camb`/`base`/`line` (los parsea y los ignora) ni tiene botones de
config. El objetivo es mostrarlos de forma INTUITIVA.

## 2. Lo que hay que DIBUJAR (datos que ya llegan parseados)

### 2.1 Per-cámara: front vs back vs fusionado (lo más valioso)

`f.camf` y `f.camb` son objetos `CamPer` (en `protocol_top.py`) con estos campos:

| Campo | Tipo | Significado |
|---|---|---|
| `ball_visible` | bool | la cámara ve la pelota |
| `ball_x_mm`, `ball_y_mm` | int | posición (marco robot: +x derecha, +y frente) |
| `yellow_visible` / `yellow_angle_deg` / `yellow_distance_mm` | bool/float/int | arco amarillo (polar) |
| `blue_visible` / `blue_angle_deg` / `blue_distance_mm` | bool/float/int | arco azul (polar) |

**Qué mostrar:** tres columnas/paneles lado a lado — **FRONTAL** (`camf`) · **TRASERA** (`camb`) ·
**FUSIONADO** (el `f.cam` que ya mostrás). El valor está en **ver el desacuerdo**: cuando ambas ven
pelota pero en posiciones distintas, el fusionado promedia (pelota fantasma, geométricamente
imposible). Sugerencia: marcar en ROJO si `camf.ball_visible and camb.ball_visible and` el delta
`sqrt((bxf-bxb)²+(byf-byb)²)` es grande (ej. > 200 mm). Eso le dice al operador **cuál cámara apagar**.

### 2.2 Base (OTOS de la placa DOWN): `f.base` (dataclass `Base`)

| Campo | Significado |
|---|---|
| `pose_fresh` (bool) | si `False` → **GRISEAR** todo el panel pose (dato viejo, no real) |
| `pose_x_mm`, `pose_y_mm`, `pose_heading_deg`, `pose_confidence` | pose por odometría OTOS |
| `vel_fresh` (bool) | ídem para velocidad |
| `vel_vx_mm_s`, `vel_vy_mm_s`, `vel_omega_deg_s`, `vel_slip` | velocidad; `slip > 50` = patinazo (indicador) |

**Qué mostrar:** un panel "ODOMETRÍA (base)" con pose + velocidad; griseado si `*_fresh` es False.

### 2.3 Línea + vector de escape: `f.line` (dataclass `Line`)

| Campo | Significado |
|---|---|
| `fresh` (bool) | dato vivo (< 500 ms) |
| `data_valid` (bool) | **COMPUERTA MAESTRA**: si False, NO usar la geometría (mostrar "INVALID") |
| `angle_deg` (float\|None) | ángulo de la línea; **None = N/A** |
| `escape_deg` (float\|None) | **dirección del VECTOR DE ESCAPE** (None = N/A) |
| `penetration_mm` (int\|None) | **magnitud del vector de escape** (None = N/A) |
| `cross_track_mm` (int\|None) | distancia al centro de la línea |
| `present` (bool), `sensors_on` (int 0-32), `quality` (int 0-100) | estado de la línea |
| `event_flags` (int) / `.event_names` (property → list[str]) | eventos EV_* (IMMINENT_EXIT, CORNER, LIFTED, …) |

**Qué mostrar:** una **flecha en el radar** apuntando a `escape_deg` con largo proporcional a
`penetration_mm` (clamp 0-300 mm) — solo si `data_valid and present`. + un panel con `present`,
`quality`, `event_names`. Tri-estado de frescura: fresh / stale (>500 ms) / N/A.

> El radar de `gui_top.py` ya tiene la convención de ángulos (`_polar_px`, 0°=frente, +90°=derecha) —
> reusala para la flecha de escape igual que para los rayos ToF.

## 3. Controles de CONFIG (A2.1) — botones que MANDAN comandos

El firmware ya ejecuta estos comandos (se mandan con `self.source.send("...")`, igual que los
botones IMU/STREAM que ya tenés en `_build_layout`). Mutan la config en RAM (efecto inmediato) y se
persisten con `CFG SAVE`:

| Botón | Comando a enviar |
|---|---|
| Cámara frontal ON/OFF | `CAM F ON` / `CAM F OFF` |
| Cámara trasera ON/OFF | `CAM B ON` / `CAM B OFF` |
| BNO izq/der ON/OFF | `BNO L ON\|OFF` / `BNO R ON\|OFF` |
| Ultrasonido ON/OFF | `US ON` / `US OFF` |
| ToF n ON/OFF | `TOF <n> ON` / `TOF <n> OFF` (n = 0..5) |
| ToF n ubicación | `TOF <n> POS FRONT\|BACK\|RIGHT\|LEFT` |
| Persistir / recargar / reset | `CFG SAVE` / `CFG LOAD` / `CFG RESET` |

> En `--sim` los comandos no se mandan al robot (el `_send` ya guarda contra eso). Probar el envío
> real es banco.

### 3.1 Mostrar el ESTADO de config (limitación a coordinar)

Hoy el estado de config (qué está on/off, bearing por ToF) **NO viene en el JSON** — solo en la
**línea `CFG` del texto humano** (modo ENTER del monitor crudo), p.ej.:
`CFG cam[F1 B0] bno[L1 R1] us1 | tof[0:en1@0 1:en1@180 ...]`.

Para que la GUI muestre el estado de forma máquina-legible hace falta un **bloque `cfg` en el JSON
(schema v2→v3)**, que es **firmware** (lo hace el agente del firmware, NO esta task). **Si lo
necesitás, pedilo** (ver TASK-206 / coordinar): el firmware agrega el bloque `cfg` + sube
`SCHEMA_VERSION_TOP` a 3 + regenera el golden, y vos lo dibujás. Mientras tanto, la GUI puede
mostrar el estado de los botones que el USUARIO tocó (optimista), o leer la línea CFG si parseás el
texto crudo.

## 4. Archivos y verificación

- **Editar SOLO:** `tools/monitor-base/monitor_base/gui_top.py` (+ helpers de dibujo nuevos si hace falta).
- **NO editar:** `protocol_top.py`, `simulator_top.py`, `src/` (firmware), el golden, el contrato.
- **Verificar sin robot/display:**
  - `python -m monitor_base --top --sim` → la ventana abre con datos sintéticos (el simulador ya
    emite v2: pelota en `camf`, arco azul en `camb`, OTOS y línea con escape en `base`/`line`).
  - `python -m monitor_base --top --selftest` → smoke headless (debe seguir verde).
  - `pytest tools/monitor-base` → 82/82 (si agregás tests de GUI, que no rompan los existentes).

## 5. Criterio de cierre

- [ ] La vista TOP muestra **front / back / fusionado** de la pelota y los 2 arcos, lado a lado, y
      resalta el desacuerdo entre cámaras (candidato a pelota fantasma).
- [ ] Panel de **odometría (base/OTOS)** con pose+velocidad, griseado si `*_fresh` es False.
- [ ] **Flecha de vector de escape** en el radar (dirección `escape_deg`, largo ∝ `penetration_mm`)
      + `event_names`, respetando `data_valid` (compuerta maestra).
- [ ] **Botones de config** A2.1 (CAM/BNO/US/TOF/CFG) que mandan los comandos correctos.
- [ ] Anda en `--sim` y `--selftest`; `pytest` sigue verde.
- [ ] (Si se agrega el bloque `cfg` JSON, coordinado) — mostrar el estado de config leído del frame.

## 6. Contexto / por qué

Esto cierra el "Monitor del Sistema de Posicionamiento" del lado visual: ver TODO lo que percibe la
TOP (cámaras por separado, OTOS, línea/escape) + apagar el sensor que miente con dato. El firmware
ya lo expone y persiste; falta que se VEA. Spec global:
[`research/in-progress/2026-06-13-diseno-monitor-general-top-config-persistente.md`](../research/in-progress/2026-06-13-diseno-monitor-general-top-config-persistente.md).
