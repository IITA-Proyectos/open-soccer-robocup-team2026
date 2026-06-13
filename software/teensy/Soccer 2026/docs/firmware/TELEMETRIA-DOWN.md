# Telemetría USB de la placa DOWN — contrato v3

**Estado:** v3 (2026-06-13). Módulo puro `src/shared/telemetry_down.{h,cpp}` (host-testeado,
gate `test_telemetry_down`). Glue Arduino `src/down/down_telemetry_serial.cpp` (GATEADO
`-DDOWN_USB_MONITOR`, envs `down` / `down_robot2`).
App de PC: `tools/monitor-base/`.

**Historial de schema:** v1 = anillo 32 + LineStatusV2 + OTOS fusionado. **v2** = agrega las
lecturas por-OTOS izq/der sin fusionar (`lx/ly/lh/rx/ry/rh`) para la **vista de arquero**.
**v3 (2026-06-13)** = agrega **sintonía fina por sensor** (`ring.threshold[]` umbral efectivo,
`ring.enabled_bits`, `ring.global_sens`, `ring.persensor_sens[]`) + comandos `SENS GLOBAL`,
`SENS SET`, `SENSOR ON|OFF`. La app acepta v2 y v3 (en v2 deriva los campos nuevos).

Este es el contrato versionado entre el **firmware de DOWN** (emite) y la **app de PC**
(consume). Misma disciplina que los contratos de wire: si cambia el layout, subir
`TELEMETRY_DOWN_SCHEMA` y regenerar el golden de ambos lados.

> 📖 **Cómo activar/desactivar y usar en banco** (reflasheo, recuperar competencia,
> calibración que persiste, troubleshooting): [`USO-MONITOREO-Y-TELEMETRIA.md`](USO-MONITOREO-Y-TELEMETRIA.md).
> Este doc es el **contrato técnico**; el de USO es el **procedimiento operativo**.

> ⚠️ El modo telemetría es un **MODO DEBUG gateado** del firmware de competencia, NO un
> sketch aparte. NO hay un env de banco aparte para la DOWN: la telemetría la compila el
> flag `-DDOWN_USB_MONITOR`, presente en los envs de competencia `down` (robot1) y
> `down_robot2` (robot2). El binario de partido arranca **DORMIDO** (no emite nada) y la
> telemetría se activa sola al conectar la app o apretar Enter (ver §4). Sin el flag, todo
> el código nuevo vive dentro de `#ifdef DOWN_USB_MONITOR` y el binario es byte-idéntico.

## 1. Transporte

- **Medio:** USB CDC del Teensy (`Serial`, 115200), independiente de los UART inter-placa
  (Serial5→TOP, Serial1→CENTRAL). NO interfiere con el WorldSnapshot ni la LINE_URGENT.
- **Framing:** **JSON Lines** — un objeto JSON por línea, terminado en `\n`. Trivial de
  parsear (`json.loads` por línea) y de loguear a archivo para análisis posterior.
- **Tasa:** 20–50 Hz (configurable por comando `RATE`). ~800 B/línea × 50 Hz ≈ 40 KB/s,
  muy por debajo del techo del USB CDC (~1 MB/s).
- **Dirección host→firmware:** comandos de texto, una línea por comando (ver §4).

## 2. Frame firmware→host (schema 2)

Un objeto JSON con esta forma (claves en orden de emisión; los `…` son los 32 valores):

```json
{"v":2,"seq":7,"t_ms":123456,
 "ring":{"n":32,"raw":[…32…],"white":196611,"carpet":[…32…],"white_cal":[…32…]},
 "line":{"schema":2,"valid":1,"present":1,"angle_cd":-1250,"escape_cd":16750,
         "pen_mm":42,"xtrack_mm":-8,"non":5,"flags":1,"q":88,"age_ms":3},
 "otos":{"n":2,"lok":1,"rok":0,"x":123.45,"y":-67.80,"hdg":12.34,
         "vx":5.00,"vy":-3.20,"w":0.123,"slip":1.50,
         "lx":120.10,"ly":-65.20,"lh":11.50,"rx":126.80,"ry":-70.40,"rh":13.18},
 "diag":{"lifted":0,"ltick":100000,"ltick_us":250}}
```

### Campos

| Ruta | Tipo | Unidad / rango | Significado |
|------|------|----------------|-------------|
| `v` | uint8 | =3 | `TELEMETRY_DOWN_SCHEMA`. La app acepta v2 y v3. |
| `seq` | uint32 | — | Contador monotónico de frame (detectar pérdidas). |
| `t_ms` | uint32 | ms | `millis()` al emitir. |
| `ring.n` | uint8 | 1..32 | `NUM_LINE_SENSORS` (32 en la placa de competencia). |
| `ring.raw[i]` | uint16 | cuentas ADC | Lectura cruda del sensor i (`line_ring_get_raw`). Carpet verde ~100–300, blanco ~600–900. |
| `ring.white` | uint32 | bitmask | Bit i = el sensor i ve blanco (`line_ring_get_white`). |
| `ring.carpet[i]` | uint16 | cuentas ADC | Calib de carpet del sensor i (`line_ring_get_carpet_avg`). |
| `ring.white_cal[i]` | uint16 | cuentas ADC | Calib de blanco del sensor i (`line_ring_get_white_avg`). |
| `ring.threshold[i]` | uint16 | cuentas ADC | **(v3)** Umbral EFECTIVO del sensor i (punto medio ± sensibilidad global+propia). Es el que usa `dm_update`. |
| `ring.enabled_bits` | uint32 | bitmask | **(v3)** Bit i = sensor i HABILITADO. Un sensor en 0 se excluye del centroide/penetración. Default todos en 1. |
| `ring.global_sens` | int8 | % [-100,100] | **(v3)** Sensibilidad global al blanco. + = menos sensible (umbral sube); 0 = punto medio histórico. |
| `ring.persensor_sens[i]` | int8 | % [-100,100] | **(v3)** Sensibilidad propia del sensor i (se suma a la global, mismo signo). |
| `line.schema` | uint8 | =2 | `LSV2_SCHEMA` (informativo). |
| `line.valid` | 0/1 | — | **Compuerta maestra**. Si 0, NADA del bloque `line` es confiable. |
| `line.present` | 0/1 | — | Hay línea presente (con histéresis). |
| `line.angle_cd` | int16 | centideg | Ángulo de la línea (0=frente, horario+). **N/A = -32768.** ÷100 = grados. |
| `line.escape_cd` | int16 | centideg | Ángulo de escape (opuesto). **N/A = -32768.** |
| `line.pen_mm` | uint16 | mm | Penetración en la línea. **N/A = 65535.** |
| `line.xtrack_mm` | int16 | mm | Distancia perpendicular firmada centro→línea (+ adelante). **N/A = -32768.** |
| `line.non` | uint8 | 0..32 | Sensores sobre la línea (`sensors_on_line`). |
| `line.flags` | uint8 | bitfield | Eventos `EV_*` OR-eados (ver §3). |
| `line.q` | uint8 | 0..100 | Calidad de la medición. |
| `line.age_ms` | uint8 | 0..255 | Edad de la muestra. 255 = N/A. |
| `otos.n` | uint8 | 0/1/2 | OTOS configurados (`NUM_OTOS`). |
| `otos.lok` / `otos.rok` | 0/1 | — | OTOS izq (U5/Wire) / der (U6/Wire1) sano. |
| `otos.x` / `otos.y` | float | mm | Pose fusionada (centro del robot). +X derecha, +Y adelante. |
| `otos.hdg` | float | grados | Heading fusionado, CCW+. |
| `otos.vx` / `otos.vy` | float | mm/s | Velocidad (marco robot). |
| `otos.w` | float | rad/s | Velocidad angular. |
| `otos.slip` | float | mm/s | Estimación de slip diferencial (0 = sin slip; sólo 2-OTOS). |
| `otos.lx`/`otos.ly`/`otos.lh` | float | mm,mm,° | **(v2)** Pose del OTOS IZQUIERDO (U5) sin fusionar. |
| `otos.rx`/`otos.ry`/`otos.rh` | float | mm,mm,° | **(v2)** Pose del OTOS DERECHO (U6) sin fusionar. El diferencial izq/der confirma que el robot avanza derecho sobre la línea. |
| `diag.lifted` | 0/1 | — | Robot levantado (datos de línea no confiables). |
| `diag.ltick` | uint32 | — | `line_ring_get_tick_count` (cuenta de muestreos). |
| `diag.ltick_us` | uint32 | µs | Duración del último tick = carga de CPU del muestreo. |

Todos los campos son lo que el firmware **YA computa para competencia** (no datos
inventados): `ring.*` y `diag.*` salen de `line_ring`; el bloque `line` es el
`LineStatusV2` exacto que `comm_central_send_line_urgent()` mandó a la CENTRAL;
`otos.*` es la pose fusionada de los OTOS.

## 3. Flags de evento (`line.flags`)

Espejo de `EV_*` en `src/shared/types.h`:

| Bit | Valor | Constante | Significado |
|-----|-------|-----------|-------------|
| 0 | 0x01 | `EV_IMMINENT_EXIT` | Salida de cancha inminente. |
| 1 | 0x02 | `EV_CORNER` | Esquina detectada. |
| 2 | 0x04 | `EV_LINE_END` | Fin de línea. |
| 3 | 0x08 | `EV_LIFTED` | Robot levantado. |
| 4 | 0x10 | `EV_CALIB_SUSPECT` | Calibración sospechosa (sensor que no separa piso/blanco). |
| 5 | 0x20 | `EV_MUX_DEAD` | Un mux no responde. |
| 6 | 0x40 | `EV_DEGRADED_GEOMETRY` | Geometría degradada (modo parcial). |
| 7 | 0x80 | `EV_SENSOR_NOISY` | Sensor ruidoso. |

## 4. Comandos host→firmware (texto, una línea)

Case-insensitive, tokens separados por espacios o comas. El parser puro
`td_parse_command()` los mapea a un enum; el glue Arduino los ejecuta.

> **Despertar el stream (binario `down`/`down_robot2` arranca DORMIDO):** el binario de
> partido no emite nada hasta que el host lo despierta. Cualquier línea recibida del host
> —incluso un **Enter vacío** (CR o LF)— prende el stream por `DOWN_MONITOR_HOST_TIMEOUT_MS`
> (3 s) y renueva ese plazo; **NO hace falta `STREAM ON`**. La app `tools/monitor-base` lo
> hace sola al conectar (manda `STREAM ON` + `PING`). Excepciones: `PING` es latido puro
> (no re-prende si el host pausó con `STREAM OFF`) y `STREAM OFF` apaga.

| Comando | Acción en el firmware |
|---------|-----------------------|
| `PING` | El firmware puede responder con un frame de ack (diagnóstico de enlace). |
| `STREAM ON` / `STREAM OFF` | Arranca / para la emisión del stream. |
| `RATE <hz>` | Cambia la tasa de emisión (hz > 0). |
| `CAL CARPET` | `line_ring_calibrate_carpet()` (robot sobre carpet verde, quieto). |
| `CAL WHITE` | `line_ring_calibrate_white()` (sensores sobre la línea blanca). |
| `CAL AUTO ON` / `CAL AUTO OFF` | Captura min/max por sensor mientras se pasa el robot; al cerrar, carpet=min, white=max. |
| `CAL SAVE` | Guarda a EEPROM la calib vigente **+ la sintonía** (sensibilidad global/por-sensor + habilitados). |
| `CAL LOAD` | Carga calib **+ sintonía** de EEPROM (en vivo). |
| `OTOS RESET` | `otos_reset()` (pone la pose acumulada en 0,0,0). |
| `SENS GLOBAL <pct>` | **(v3)** Sensibilidad global al blanco, `pct ∈ [-100,100]` (clampeado). + = menos sensible. Mapeo SATURANTE: -100 → umbral 0 (todos leen blanco), +100 → por encima de white (ninguno). |
| `SENS SET <i> <pct>` | **(v3)** Sensibilidad propia del sensor `i`. |
| `SENSOR <i> ON` / `SENSOR <i> OFF` | **(v3)** Habilita/deshabilita el sensor `i` (deshabilitado = excluido del centroide). |

## 5. Versionado y golden

El frame canónico de prueba (ver `make_golden_frame` en
`test/test_telemetry_down/test_main.cpp`) produce un string **byte-idéntico** que se
commitea como fixture de la app en `tools/monitor-base/tests/golden_frame_v1.jsonl`.
El test C++ (`test_td_serialize_golden_exact`) y el test de Python validan **el mismo
string** → contrato cross-lenguaje. **Si cambiás el schema:**

1. Subí `TELEMETRY_DOWN_SCHEMA` en `telemetry_down.h`.
2. Actualizá el golden en el test C++ y regenerá `golden_frame_v1.jsonl`
   (recompilá el arnés o copiá la salida del serializador).
3. Subí la versión que la app de PC acepta.

## 6. Build / banco

```
pio run -e down -t upload                       # Teensy 4.0 DOWN (robot1; robot2 = down_robot2)
python -m monitor_base --port COMx             # app de PC (ver tools/monitor-base/README)
```

Para desarrollar la app **sin el robot**, usar el simulador o un archivo grabado:
```
python -m monitor_base --sim
python -m monitor_base --replay grabacion.jsonl
```

> El glue Arduino NO se compila en el gate host (g++). Verificar con
> `pio run -e down`. El módulo puro y la app de PC sí se testean en
> escritorio (`bash scripts/run-host-tests.sh test_telemetry_down` y `pytest tools/monitor-base`).
