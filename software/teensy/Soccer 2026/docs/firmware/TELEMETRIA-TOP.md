# Telemetría/monitoreo USB de la placa TOP — contrato v2

**Estado:** **frame v2** (2026-06-14, monitor de posicionamiento A1) + monitor dormido en
competencia (2026-06-13, TASK-205, espejo de DOWN/TASK-306). Módulo puro
`src/shared/telemetry_top.{h,cpp}` (host-testeado, gate `test_telemetry_top`: 19 tests). Glue
Arduino `src/top/top_telemetry_serial.cpp`. App de PC: `tools/monitor-base/` (vista TOP).

> **v2 (A1) agrega** sobre el frame v1: bloques **`camf`/`camb`** (detecciones POR CÁMARA
> front/back, además del fusionado `cam`) y **`base`/`line`** (OTOS + línea + **vector de
> escape** que la TOP recibe de la base DOWN). El monitor pasa a ser el del **sistema de
> posicionamiento**: ve TODO lo que converge en la TOP + lo que sale a CENTRAL (`snap`).
> **WIRE-BREAKING v1→v2:** re-flashear firmware **y** actualizar la app JUNTOS (la app v1
> rechaza el frame v2 y viceversa, fail-safe; mismo precedente que WorldSnapshot v2→v3).

Hermano del contrato de la base ([`TELEMETRIA-DOWN.md`](TELEMETRIA-DOWN.md)). Mismo transporte
(JSON Lines por USB CDC `Serial`@115200).

## 0. Modo de operación — DORMIDO en competencia (igual que DOWN)

Desde 2026-06-13 el monitor viaja **EN el binario de competencia** (`top_robot2_pri` y sus
derivados, flag `-DTOP_USB_MONITOR`) pero **arranca DORMIDO**. Tres estados:

| Estado | Cómo se entra | Qué emite |
|--------|---------------|-----------|
| **DORMIDO** (default/boot) | — | **nada** (silencio total por USB). |
| **MÁQUINA** | la app manda `STREAM ON` (+ `PING` cada 1 s como latido) | **JSON Lines** continuo (§1), 20 Hz, para la app. |
| **HUMANO** | un **ENTER** (línea vacía o texto no reconocido; CR o LF) en un monitor serie crudo | **bloque de texto legible** (§1-bis), 2 Hz, sin app ni comandos. |

- **Auto-off**: si el host se calla > **3 s** (la app deja de mandar `PING`; nadie aprieta
  Enter), vuelve solo a DORMIDO. Sacar el cable = modo partido.
- **Match-safe**: en partido **no hay USB conectado** → nunca llega una línea → el monitor
  nunca despierta. Banner de boot: `[TOP-MONITOR] dormido - esperando app (STREAM ON / PING) o ENTER`.
- ⚠️ **El binario de competencia YA NO es byte-idéntico** al de antes de 2026-06-13 (ahora
  lleva el monitor dormido). Es match-safe por diseño, pero **lo valida el equipo en banco**.
- El flag legacy `-DTOP_DEBUG_TELEMETRY` (envs `top_*_debug_telemetry`) enciende el MISMO
  código (gate OR) y se comporta igual (dormido + wake).

> 📖 **Cómo usarlo en banco** (app, calibración IMU, recuperar competencia):
> [`USO-MONITOREO-Y-TELEMETRIA.md`](USO-MONITOREO-Y-TELEMETRIA.md). Este doc es el **contrato
> técnico** (qué campos, qué tipos); el de USO es el **procedimiento operativo**.

## 1. Frame firmware→host (schema 2)

Orden de bloques (la golden es byte-exacta): `cam` → **`camf`** → **`camb`** → `imu` → `tof` →
`snap` → **`base`** → **`line`** → `diag`.

```json
{"v":2,"seq":3,"t_ms":5000,
 "cam":{"fok":1,"bok":0,"bvis":1,"bx":-120,"by":340,"bconf":77,"bvx":-15,"bvy":200,
        "gy_vis":1,"gy_ang":4500,"gy_dist":1200,"gb_vis":0,"gb_ang":-9000,"gb_dist":2500,
        "crc":2,"resync":5},
 "camf":{"bvis":1,"bx":-118,"by":338,"gy_vis":1,"gy_ang":4500,"gy_dist":1200,
         "gb_vis":0,"gb_ang":0,"gb_dist":0},
 "camb":{"bvis":0,"bx":0,"by":0,"gy_vis":0,"gy_ang":0,"gy_dist":0,
         "gb_vis":1,"gb_ang":-9000,"gb_dist":800},
 "imu":{"hdg":42.50,"left":42.10,"right":42.90,"disagree":0.80,"lok":1,"rok":1,"valid":1},
 "tof":{"n":4,"d":[150,800,65535,1200],"hc":300,"min":150},
 "snap":{"valid":1,"x":100,"y":-200,"hdg_cd":4250,"conf":80,"bx":-118,"by":338,"bvis":1,
         "bconf":77,"bvx":-15,"bvy":200,"opp_ang":4500,"opp_dist":1200,"opp_vis":1,
         "own_vis":0,"own_ang":0,"own_dist":0,"obst":150,"ref":1,"flags":24},
 "base":{"pfresh":1,"px":1500,"py":2000,"phdg_cd":9000,"pconf":75,
         "vfresh":1,"vx":120,"vy":-30,"omega":4500,"slip":12},
 "line":{"fresh":1,"schema":2,"valid":1,"angle_cd":4500,"escape_cd":-13500,"pen_mm":80,
         "cross_mm":-32768,"present":1,"sensors":7,"events":1,"quality":88,"age_ms":3},
 "diag":{"frames_sent":1234}}
```

### Bloques

| Bloque | Campos | Significado |
|--------|--------|-------------|
| raíz | `v`(=2), `seq`(uint32), `t_ms`(uint32) | schema, contador de frame, `millis()`. |
| `cam` | `fok`/`bok` | cámara frontal / trasera viva (watchdog). |
| `cam` | `bvis`,`bx`,`by`,`bconf`,`bvx`,`bvy` | pelota **fusionada**: visible, pos (mm, marco robot +x der/+y frente), confianza 0-100, velocidad mm/s. |
| `cam` | `gy_vis`/`gy_ang`/`gy_dist` | arco **amarillo** fusionado: visible, ángulo (centideg), distancia (mm). |
| `cam` | `gb_vis`/`gb_ang`/`gb_dist` | arco **azul** fusionado: idem. |
| `cam` | `crc`,`resync` | integridad del enlace cámara→TOP (errores CRC8 / resyncs, agregados front+back). |
| **`camf`** | `bvis`,`bx`,`by` | **(v2)** pelota vista por la cámara **FRONTAL** (pre-fusión, marco robot). |
| `camf` | `gy_vis`/`gy_ang`/`gy_dist`, `gb_vis`/`gb_ang`/`gb_dist` | arcos amarillo/azul vistos por la frontal (polar). |
| **`camb`** | (mismos campos que `camf`) | **(v2)** lo que ve la cámara **TRASERA** (ya rotada 180°). El **delta** `camf↔camb` delata la pelota fantasma del promedio de `cam` → decidir `CAM F\|B OFF`. |
| `imu` | `hdg`,`left`,`right` | heading fusionado y de cada BNO (grados, CCW+). |
| `imu` | `disagree` | desacuerdo entre los 2 BNO (grados; >5° = problema). |
| `imu` | `lok`/`rok`/`valid` | BNO izq/der listos; `valid` = heading_valid del snapshot (bit4). |
| `tof` | `n`,`d[]` | cantidad de ToF (4) y sus distancias (mm). **`65535` = sin lectura** (TOF_NO_READING). |
| `tof` | `hc`,`min` | HC-SR04 (mm) y el mínimo de los 4 ToF + HC-SR04. |
| `snap` | `valid` | 0 si todavía no se difundió ningún WorldSnapshot. Si 0, ignorar el resto del bloque. |
| `snap` | `x`,`y`,`hdg_cd`,`conf` | **pose propia fusionada** (lo que va a CENTRAL): mm, centideg, confianza. |
| `snap` | `bx`,`by`,`bvis`,`bconf`,`bvx`,`bvy` | pelota en el snapshot. |
| `snap` | `opp_*` / `own_*` | arco **rival** / **propio** (mapeo por color de equipo): ángulo/dist/visible. |
| `snap` | `obst`,`ref`,`flags` | obstáculo más cercano (mm; 65535=libre), comando árbitro (0=stop/1=start/2=halftime/3=reset), flags. |
| **`base`** | `pfresh`,`px`,`py`,`phdg_cd`,`pconf` | **(v2)** **OTOS** que llega de la base (DOWN): `pfresh`=dato vivo (<500 ms); pose x/y (mm), heading (centideg), confianza. Si `pfresh=0`, GRISEAR (no es real). |
| `base` | `vfresh`,`vx`,`vy`,`omega`,`slip` | velocidad OTOS (mm/s, omega centideg/s) + `slip` (0-255; >50 = patinazo). |
| **`line`** | `fresh`,`schema`,`valid` | **(v2)** línea de la base (DOWN). `valid`=**compuerta maestra** (0 → ignorar la geometría). |
| `line` | `angle_cd`,`escape_cd`,`pen_mm`,`cross_mm` | ángulo de línea + **VECTOR DE ESCAPE** (`escape_cd`=dirección, `pen_mm`=magnitud) + cross-track. **N/A**: `-32768` (int16) / `65535` (`pen_mm`). |
| `line` | `present`,`sensors`,`events`,`quality`,`age_ms` | línea presente (0/1), sensores en línea (0-32), `events` (EV_* OR-eados), calidad, edad de muestra. |
| `diag` | `frames_sent` | snapshots enviados a CENTRAL. |

`snap.flags`: bit0 `in_own_penalty_area`, bit1 `partner_alive`, bit2 `partner_sees_ball`,
bit3 `match_running`, bit4 `heading_valid`.

`cam` = lo que ven las cámaras (crudo fusionado, amarillo/azul); `snap` = lo que TOP
**interpreta y manda a la CENTRAL** (arcos ya mapeados a propio/rival por color de equipo).

## 1-bis. Bloque de TEXTO humano (modo ENTER)

En modo HUMANO el firmware emite, en vez del JSON, un bloque de **texto legible** (función
pura `tt_format_human`, ASCII, mismo dato que el JSON). Es para leer de un vistazo en un
monitor serie crudo, sin la app. Los ángulos van en grados (`a<deg>`); `--` = no visible /
sin lectura. Ejemplo:

```
[TOP] seq 7 t=123456ms frames=4567
  CAM F:OK B:OK | ball(120,340) c85 v(10,-5) | GY a12.0 d800 | GB --
  CAMF ball(118,338) GY a12.0 d800 GB --
  CAMB ball -- GY -- GB a-178.0 d2400
  IMU hdg 47.50 VALID L47.25 R47.75 dis0.50 (L:ok R:ok)
  ToF n=4 [490,512,25,600] hc=1200 min=25 mm
  SNAP x120 y340 hdg47.50 c80 | ball(120,340) c85 | opp a12.0 d800 VIS | own -- | obst25 ref1 flags0x18
  BASE pose x1500 y2000 hdg90.00 c75 | vel vx120 vy-30 w45.0 slip12
  LINE FRESH present1 sensors7 | angle a45.0 esc a-135.0 pen80mm q88 ev0x01
```

`CAMF`/`CAMB` = lo que ve cada cámara por separado (sin fusionar). `BASE` muestra `STALE` si el
OTOS no está fresh; `LINE` muestra `STALE`/`INVALID` y `--` en los campos N/A.

## 2. Comandos host→firmware (texto, una línea)

| Comando | Acción |
|---------|--------|
| `PING` | emite un frame inmediato (ack de enlace). |
| `STREAM ON` / `STREAM OFF` | arranca / para el stream. |
| `RATE <hz>` | cambia la tasa (1..200). |
| `IMU ZERO` | `sensors_imu_recalibrate_zero()` (re-captura el heading inicial; apuntar al arco rival). |
| `IMU SAVE` | `sensors_imu_save_calibration()` (persiste el perfil de calib del BNO en EEPROM). |

## 3. Build / banco

El monitor ya viaja en el env de **competencia** (no hay que reflashear un env aparte):

```
pio run -e top_robot2_pri -t upload                # Teensy 4.0 TOP (AMBOS robots) — monitor dormido a bordo
python -m monitor_base --top --port COMx           # app de PC (vista TOP): manda STREAM ON + PING → despierta
python -m monitor_base --top --selftest            # smoke headless (sin robot)
# o, sin app, en un monitor serie crudo: apretar ENTER → bloque de texto 3 s (modo humano).
```

> El glue Arduino NO se compila en el gate host. Verificado con `pio run -e top_robot2_pri`
> (SUCCESS 2026-06-14) + **banco** pendiente (el binario de competencia cambió: validar
> dormido/wake/match-safe + datos DOWN reales — no es byte-idéntico, lo cierra el equipo). El
> módulo puro y el parser de la app sí se testean en escritorio
> (`bash scripts/run-host-tests.sh test_telemetry_top` → 19/0 y `pytest tools/monitor-base` → 82/0).

## 4. Versionado / golden

Golden byte-idéntico en `test/test_telemetry_top/test_main.cpp` (constante `GOLDEN`) ⇄ fixture
`tools/monitor-base/tests/golden_top_v2.jsonl` (contrato cross-lenguaje; verificado sha256). Si
cambia el schema: subir `TELEMETRY_TOP_SCHEMA` **y** `SCHEMA_VERSION_TOP` (protocol_top.py),
regenerar la `GOLDEN` C++ + el `.jsonl`, y reflashear firmware + actualizar app JUNTOS (v2 es
wire-breaking). El bloque de **texto humano** (`tt_format_human`) NO está versionado (es para leer
a ojo, no para parsear). El `golden_top_v1.jsonl` quedó obsoleto (v1 retirado).
