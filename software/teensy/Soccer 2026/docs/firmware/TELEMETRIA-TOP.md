# Telemetría USB de la placa TOP — contrato v1 (FASE 2)

**Estado:** v1 (2026-06-07). Módulo puro `src/shared/telemetry_top.{h,cpp}` (host-testeado,
gate `test_telemetry_top`). Glue Arduino `src/top/top_telemetry_serial.cpp` (GATEADO
`-DTOP_DEBUG_TELEMETRY`, env `top_robot1_debug_telemetry`). App de PC: `tools/monitor-base/`.

Hermano del contrato de la base ([`TELEMETRIA-DOWN.md`](TELEMETRIA-DOWN.md)). Mismo
transporte (JSON Lines por USB CDC `Serial`@115200) y misma disciplina: con el flag OFF
(envs `top_robot1`/`top_robot2`) el binario de competencia es **byte-idéntico** (todo el
código nuevo en `#ifdef TOP_DEBUG_TELEMETRY`).

## 1. Frame firmware→host (schema 1)

```json
{"v":1,"seq":3,"t_ms":5000,
 "cam":{"fok":1,"bok":0,"bvis":1,"bx":-120,"by":340,"bconf":77,"bvx":-15,"bvy":200,
        "gy_vis":1,"gy_ang":4500,"gy_dist":1200,"gb_vis":0,"gb_ang":-9000,"gb_dist":2500,
        "crc":2,"resync":5},
 "imu":{"hdg":42.50,"left":42.10,"right":42.90,"disagree":0.80,"lok":1,"rok":1,"valid":1},
 "tof":{"n":4,"d":[150,800,65535,1200],"hc":300,"min":150},
 "snap":{"valid":1,"x":100,"y":-200,"hdg_cd":4250,"conf":80,"bx":-118,"by":338,"bvis":1,
         "bconf":77,"bvx":-15,"bvy":200,"opp_ang":4500,"opp_dist":1200,"opp_vis":1,
         "own_vis":0,"own_ang":0,"own_dist":0,"obst":150,"ref":1,"flags":24},
 "diag":{"frames_sent":1234}}
```

### Bloques

| Bloque | Campos | Significado |
|--------|--------|-------------|
| raíz | `v`(=1), `seq`(uint32), `t_ms`(uint32) | schema, contador de frame, `millis()`. |
| `cam` | `fok`/`bok` | cámara frontal / trasera viva (watchdog). |
| `cam` | `bvis`,`bx`,`by`,`bconf`,`bvx`,`bvy` | pelota fusionada: visible, pos (mm, marco robot +x der/+y frente), confianza 0-100, velocidad mm/s. |
| `cam` | `gy_vis`/`gy_ang`/`gy_dist` | arco **amarillo**: visible, ángulo (centideg), distancia (mm). |
| `cam` | `gb_vis`/`gb_ang`/`gb_dist` | arco **azul**: idem. |
| `cam` | `crc`,`resync` | integridad del enlace cámara→TOP (errores CRC8 / resyncs, agregados front+back). |
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
| `diag` | `frames_sent` | snapshots enviados a CENTRAL. |

`snap.flags`: bit0 `in_own_penalty_area`, bit1 `partner_alive`, bit2 `partner_sees_ball`,
bit3 `match_running`, bit4 `heading_valid`.

`cam` = lo que ven las cámaras (crudo fusionado, amarillo/azul); `snap` = lo que TOP
**interpreta y manda a la CENTRAL** (arcos ya mapeados a propio/rival por color de equipo).

## 2. Comandos host→firmware (texto, una línea)

| Comando | Acción |
|---------|--------|
| `PING` | emite un frame inmediato (ack de enlace). |
| `STREAM ON` / `STREAM OFF` | arranca / para el stream. |
| `RATE <hz>` | cambia la tasa (1..200). |
| `IMU ZERO` | `sensors_imu_recalibrate_zero()` (re-captura el heading inicial; apuntar al arco rival). |
| `IMU SAVE` | `sensors_imu_save_calibration()` (persiste el perfil de calib del BNO en EEPROM). |

## 3. Build / banco

```
pio run -e top_robot1_debug_telemetry -t upload    # Teensy 4.0 TOP
python -m monitor_base --top --port COMx           # app de PC (vista TOP)
python -m monitor_base --top --selftest            # smoke headless (sin robot)
```

> El glue Arduino NO se compila en el gate host. Verificar con
> `pio run -e top_robot1_debug_telemetry` + confirmar `pio run -e top_robot1` byte-idéntico.
> El módulo puro y el parser de la app sí se testean en escritorio
> (`bash scripts/run-host-tests.sh test_telemetry_top` y `pytest tools/monitor-base`).

## 4. Versionado / golden

Golden byte-idéntico en `test/test_telemetry_top/test_main.cpp` ⇄ fixture
`tools/monitor-base/tests/golden_top_v1.jsonl` (contrato cross-lenguaje). Si cambia el
schema, subir `TELEMETRY_TOP_SCHEMA` y regenerar ambos.
