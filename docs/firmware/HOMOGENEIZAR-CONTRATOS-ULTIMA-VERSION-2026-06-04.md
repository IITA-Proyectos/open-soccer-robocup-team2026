---
title: "Homogeneizar TODAS las ramas a la ÚLTIMA versión de contratos + firmware (2026-06-04)"
date: 2026-06-04
author: "Claude Opus 4.8 (Anthropic) — agente TOP"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8, Anthropic)"
status: accion-requerida
dirigido-a: "agente coach (repo principal) — coordinar el merge + homogeneización"
tags: [contrato, firmware, merge, homogeneizar, wire-breaking, multi-agente, incheon]
robot: ambos
area: comunicacion
tipo: instruccion-de-merge
---

# ⚠️ ACCIÓN PARA EL AGENTE COACH — homogeneizar todo a la ÚLTIMA versión

> **Para el agente coach (el que opera en el repo principal y mergea a `main`).**
> Gustavo pidió que, tras mergear `agente/top`, **todas las ramas y todo el
> firmware queden en la MISMA y ÚLTIMA versión de contratos** — nada de placas
> hablando versiones distintas. Este doc te dice exactamente qué versión es la
> buena y qué hay que verificar en cada rama.

## TL;DR — la última versión (estado al 2026-06-04, rama `agente/top`)

| Contrato | Versión | Tamaño | Dónde se define |
|---|---|---|---|
| **WorldSnapshot** (TOP→CENTRAL) | **v3** (`contract-schema: 3`) | **31 bytes** | `src/shared/types.h` |
| **Cámara** (OpenMV→TOP) | **v2** (`contract-schema: 2`) | **11 bytes/packet** | `src/top/cameras.{h,cpp}` |
| **Proto inter-placa** (frame envelope) | estable | overhead 7 B | `src/shared/proto.h` |
| **LineStatusV2 + Pose2D/Velocity2D** (DOWN→{CENTRAL,TOP}) | broadcast simétrico | — | `src/shared/types.h` + `down_tx` |

> ⚠️ **Ambos contratos de robot (cámara v2 y snapshot v3) son `WIRE BREAKING`.**
> Una placa con firmware viejo NO puede hablar con una con firmware nuevo: el
> número de bytes cambió y el CRC no validará. **Hay que flashear las 3 placas
> (TOP, CENTRAL, OpenMV ×2) con la MISMA versión, todas juntas.**

---

## 1. WorldSnapshot v3 (TOP → CENTRAL) — 31 bytes

Cambios vs v2 (27 B): **+`heading_valid` (flags bit 4)** y **+ángulo/distancia del
arco propio** (`goal_own_angle_centideg` + `goal_own_distance_mm`, antes solo
viajaba `goal_own_visible`).

### Layout exacto (`src/shared/types.h`, `static_assert(sizeof == 31)`)

| Off | Campo | Tipo | Nota |
|----:|-------|------|------|
| 0 | `my_x_mm` | i16 | pose X (trilateración TOF+IMU; válida si confidence>0) |
| 2 | `my_y_mm` | i16 | pose Y |
| 4 | `my_heading_centideg` | i16 | heading del BNO; **válido sólo si flags bit4=1** |
| 6 | `my_pose_confidence` | u8 | 0-100; `pose.valid?70:0` |
| 7 | `ball_x_mm` | i16 | |
| 9 | `ball_y_mm` | i16 | |
| 11 | `ball_visible` | u8 | 0/1 |
| 12 | `ball_confidence` | u8 | 0-100 |
| 13 | `ball_vx_mm_s` | i16 | velocidad pelota (0,0 = N/A) |
| 15 | `ball_vy_mm_s` | i16 | |
| 17 | `goal_opp_angle_centideg` | i16 | |
| 19 | `goal_opp_distance_mm` | i16 | |
| 21 | `goal_opp_visible` | u8 | 0/1 |
| 22 | `goal_own_visible` | u8 | 0/1 |
| **23** | **`goal_own_angle_centideg`** | **i16** | **← v3**; válido sólo si goal_own_visible=1 |
| **25** | **`goal_own_distance_mm`** | **i16** | **← v3**; válido sólo si goal_own_visible=1 |
| 27 | `min_obstacle_mm` | u16 | 0xFFFF = sin lectura |
| 29 | `referee_cmd` | u8 | 0=stop 1=start 2=halftime 3=reset |
| 30 | `flags` | u8 | ver bits abajo |

### Bits de `flags` (offset 30)
- bit 0 = `in_own_penalty_area`
- bit 1 = `partner_alive`
- bit 2 = `partner_sees_ball`
- bit 3 = `match_running`
- **bit 4 = `heading_valid`** ← **v3** (1 = el heading del BNO es válido; si 0, CENTRAL NO debe usar `my_heading_centideg`)
- bits 5-7 = reservados

Frame completo TOP→CENTRAL: `0xAA LEN(=0x1F=31) TYPE(WORLD_SNAPSHOT=0x60) SEQ PAYLOAD(31) CRC16 0x55` = **38 bytes**.

---

## 2. Cámara v2 (OpenMV → TOP) — 11 bytes/packet

Cambios vs v1 (9 B): **+sentinel 255 explícito** (en vez de la heurística
`(0,-100)` que daba falsos negativos), **+byte END (0xFE)** y **+CRC8**.

### Layout exacto (`src/top/cameras.{h,cpp}`, `CAM_PACKET_LEN = 11`)

| Byte | Valor | Significado |
|----:|-------|-------------|
| 0 | `201` (`CAM_HEADER1`) | sync pelota |
| 1 | ball_x coded | `coded = valor + 100`; **255 = no detectado** |
| 2 | ball_y coded | idem |
| 3 | `202` (`CAM_HEADER2`) | sync arco amarillo |
| 4 | yellow_x coded | 255 = no visible |
| 5 | yellow_y coded | |
| 6 | `203` (`CAM_HEADER3`) | sync arco azul |
| 7 | blue_x coded | 255 = no visible |
| 8 | blue_y coded | |
| 9 | **CRC8** (`cam_crc8` sobre los 9 bytes previos) | integridad |
| 10 | `254` (`CAM_END_BYTE`, 0xFE) | fin de trama |

Constantes: `CAM_SENTINEL=255`, `CAM_COORD_OFFSET=100`, `CAM_END_BYTE=254`.

> **El OpenMV (ambas cámaras, frontal y trasera) tiene que emitir ESTE formato
> de 11 bytes con CRC8 + END.** Si el script de la OpenMV todavía manda el de 9
> bytes (v1), el parser del TOP lo rechaza por CRC → cámara muda. Flashear el
> OpenMV junto con el TOP.

---

## 3. Proto inter-placa (envelope, estable)

`0xAA │ LEN │ TYPE │ SEQ │ PAYLOAD │ CRC16-BE │ 0x55`. START=0xAA, END=0x55,
CRC-16/CCITT-FALSE sobre LEN+TYPE+SEQ+PAYLOAD. SEQ wrap 0-255. No cambió, pero
TOP ahora **sí consume SEQ** (cuenta `frames_lost` por enlace) — eso es aditivo,
no rompe el wire.

---

## 4. Qué hacer en cada rama (checklist para el coach)

> Ramas en el remoto: `agente/central`, `agente/down`, `agente/top`,
> `agente/vision`, + `main`. El **origen de verdad de los structs compartidos es
> `src/shared/types.h` + `src/shared/proto.h`** — al mergear `agente/top` a `main`,
> esos structs quedan en v3/v2. Las demás ramas deben **rebasar sobre el `main`
> actualizado** para heredar la misma `types.h`.

### Paso 1 — mergear `agente/top` a `main`
```
cd ~/iitasoccer/open-soccer-robocup-team2026
git fetch
git merge --no-ff agente/top      # trae WorldSnapshot v3 + cámara v2 + mejoras
git push origin main
```
Gate verificado en `agente/top` antes de pedir el merge: **545 tests host / 40
envs / 0 fallos**, `top_robot1` compila SUCCESS.

### Paso 2 — homogeneizar las otras ramas (cada agente, al inicio de su próxima sesión)
Para `agente/central`, `agente/down`, `agente/vision`:
```
git fetch
git rebase main          # hereda types.h v3 + proto + cámara v2
# resolver conflictos en src/shared/ a favor de main (es la versión canónica)
pio run -e <su_env>      # confirmar que compila contra el contrato nuevo
bash "software/teensy/Soccer 2026/scripts/run-host-tests.sh"   # 0 fallos
```
Puntos de atención por rama:
- **`agente/central`**: `world_model.{h,cpp}` debe parsear los 31 B (leer
  `goal_own_angle/distance` + gatear el heading por `flags bit4`). El commit
  `24bd417` ya lo dejó hecho en TOP-branch; verificar que CENTRAL no tenga una
  copia divergente de `types.h`.
- **`agente/down`**: no consume WorldSnapshot, pero comparte `types.h`/`proto.h`.
  Rebasar para no divergir el shared. LineStatusV2 + Pose2D/Velocity2D no cambian.
- **`agente/vision`**: es la que más importa para la cámara — el script OpenMV
  debe emitir el **packet de 11 B (v2)** con CRC8 + END + sentinel 255.

### Paso 3 — flasheo coordinado (equipo humano, con placa en mano)
**Las 4 piezas a la MISMA versión, todas juntas:**
1. **TOP** (Teensy 4.0): `pio run -e top_robot1 -t upload` (o `top_robot2`).
2. **CENTRAL** (Teensy 4.1): `pio run -e central_robot1 -t upload` (o `_robot2`).
3. **OpenMV frontal**: script de cámara v2 (11 B).
4. **OpenMV trasera**: script de cámara v2 (11 B).

> Si se flashea solo una y las otras quedan viejas: **el enlace se rompe en
> silencio** (CRC falla → la placa receptora descarta todo). Síntoma: `rx=0` o
> `crc_errors` subiendo. Por eso: **flashear las 4 juntas o ninguna.**

---

## 5. Cómo VERIFICAR que quedó todo homogéneo (criterio de cierre)

- [ ] `main` tiene `static_assert(sizeof(WorldSnapshot)==31)` y `CAM_PACKET_LEN==11`.
- [ ] Cada `agente/*` rebasado sobre `main` **compila** su env y pasa la suite host (0 fallos).
- [ ] No hay copias divergentes de `src/shared/types.h` / `proto.h` entre ramas
      (todas apuntan al mismo blob tras el rebase — `git diff main agente/x -- src/shared/types.h` vacío).
- [ ] En banco: TOP→CENTRAL con `diag_central_rx_all` muestra los 31 B decodificados
      sin CRC-error; cámara→TOP con `crc_errors=0`.
- [ ] Los 3 docs canónicos coinciden: `CONTRATO-DATOS-TOP.md` y `-CENTRAL.md` en
      `contract-schema: 3`, `CONTRATO-DATOS-CAMARAS.md` en `contract-schema: 2`.

## 6. Referencias (fuente de verdad)
- `src/shared/types.h` — WorldSnapshot v3 (struct + static_assert).
- `src/top/cameras.{h,cpp}` — packet cámara v2 (CAM_* constants + parser).
- `src/shared/proto.h` — envelope inter-placa.
- `docs/firmware/CONTRATO-DATOS-TOP.md` (schema 3) · `-CENTRAL.md` (schema 3) ·
  `-CAMARAS.md` (schema 2).
- Commits clave en `agente/top`: `d230de5` (cámara v2), `24bd417` (snapshot v3),
  `b5f14e5` (debounce+SEQ+RX ring), `d802403` (ToF stale + WDT), `f574ce2`
  (bt_classify + CRC telemetría cámara).

## Cambios de estado
- 2026-06-04: creado por el agente TOP (Claude Opus 4.8) a pedido de Gustavo,
  para que el agente coach homogeneíce todas las ramas + firmware a la última
  versión tras mergear `agente/top`.
