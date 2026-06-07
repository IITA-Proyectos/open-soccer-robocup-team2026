# Telemetría USB de la placa DOWN — contrato v1

**Estado:** v1 (2026-06-07). Módulo puro `src/shared/telemetry_down.{h,cpp}` (host-testeado,
gate `test_telemetry_down`). Glue Arduino `src/down/down_telemetry_serial.cpp` (GATEADO
`-DDOWN_DEBUG_TELEMETRY`, env `down_debug_telemetry`). App de PC: `tools/monitor-base/`.

Este es el contrato versionado entre el **firmware de DOWN** (emite) y la **app de PC**
(consume). Misma disciplina que los contratos de wire: si cambia el layout, subir
`TELEMETRY_DOWN_SCHEMA` y regenerar el golden de ambos lados.

> ⚠️ El modo telemetría es un **MODO DEBUG gateado** del firmware de competencia, NO un
> sketch aparte. Con el flag OFF (todos los envs de competencia: `down`, `down_lean`,
> `down_wdt`) el binario es **byte-idéntico** — todo el código nuevo vive dentro de
> `#ifdef DOWN_DEBUG_TELEMETRY`. Sólo `[env:down_debug_telemetry]` lo compila ON.

## 1. Transporte

- **Medio:** USB CDC del Teensy (`Serial`, 115200), independiente de los UART inter-placa
  (Serial5→TOP, Serial1→CENTRAL). NO interfiere con el WorldSnapshot ni la LINE_URGENT.
- **Framing:** **JSON Lines** — un objeto JSON por línea, terminado en `\n`. Trivial de
  parsear (`json.loads` por línea) y de loguear a archivo para análisis posterior.
- **Tasa:** 20–50 Hz (configurable por comando `RATE`). ~800 B/línea × 50 Hz ≈ 40 KB/s,
  muy por debajo del techo del USB CDC (~1 MB/s).
- **Dirección host→firmware:** comandos de texto, una línea por comando (ver §4).

## 2. Frame firmware→host (schema 1)

Un objeto JSON con esta forma (claves en orden de emisión; los `…` son los 32 valores):

```json
{"v":1,"seq":7,"t_ms":123456,
 "ring":{"n":32,"raw":[…32…],"white":196611,"carpet":[…32…],"white_cal":[…32…]},
 "line":{"schema":2,"valid":1,"present":1,"angle_cd":-1250,"escape_cd":16750,
         "pen_mm":42,"xtrack_mm":-8,"non":5,"flags":1,"q":88,"age_ms":3},
 "otos":{"n":2,"lok":1,"rok":0,"x":123.45,"y":-67.80,"hdg":12.34,
         "vx":5.00,"vy":-3.20,"w":0.123,"slip":1.50},
 "diag":{"lifted":0,"ltick":100000,"ltick_us":250}}
```

### Campos

| Ruta | Tipo | Unidad / rango | Significado |
|------|------|----------------|-------------|
| `v` | uint8 | =1 | `TELEMETRY_DOWN_SCHEMA`. La app rechaza otros valores. |
| `seq` | uint32 | — | Contador monotónico de frame (detectar pérdidas). |
| `t_ms` | uint32 | ms | `millis()` al emitir. |
| `ring.n` | uint8 | 1..32 | `NUM_LINE_SENSORS` (32 en la placa de competencia). |
| `ring.raw[i]` | uint16 | cuentas ADC | Lectura cruda del sensor i (`line_ring_get_raw`). Carpet verde ~100–300, blanco ~600–900. |
| `ring.white` | uint32 | bitmask | Bit i = el sensor i ve blanco (`line_ring_get_white`). |
| `ring.carpet[i]` | uint16 | cuentas ADC | Calib de carpet del sensor i (`line_ring_get_carpet_avg`). |
| `ring.white_cal[i]` | uint16 | cuentas ADC | Calib de blanco del sensor i (`line_ring_get_white_avg`). Umbral = (carpet+white_cal)/2. |
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

| Comando | Acción en el firmware |
|---------|-----------------------|
| `PING` | El firmware puede responder con un frame de ack (diagnóstico de enlace). |
| `STREAM ON` / `STREAM OFF` | Arranca / para la emisión del stream. |
| `RATE <hz>` | Cambia la tasa de emisión (hz > 0). |
| `CAL CARPET` | `line_ring_calibrate_carpet()` (robot sobre carpet verde, quieto). |
| `CAL WHITE` | `line_ring_calibrate_white()` (sensores sobre la línea blanca). |
| `CAL AUTO ON` / `CAL AUTO OFF` | Captura min/max por sensor mientras se pasa el robot; al cerrar, carpet=min, white=max. |
| `CAL SAVE` | Guarda la calibración vigente a EEPROM. |
| `CAL LOAD` | Carga la calibración de EEPROM. |
| `OTOS RESET` | `otos_reset()` (pone la pose acumulada en 0,0,0). |

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
pio run -e down_debug_telemetry -t upload      # Teensy 4.0 DOWN
python -m monitor_base --port COMx             # app de PC (ver tools/monitor-base/README)
```

Para desarrollar la app **sin el robot**, usar el simulador o un archivo grabado:
```
python -m monitor_base --sim
python -m monitor_base --replay grabacion.jsonl
```

> El glue Arduino NO se compila en el gate host (g++). Verificar con
> `pio run -e down_debug_telemetry`. El módulo puro y la app de PC sí se testean en
> escritorio (`bash scripts/run-host-tests.sh test_telemetry_down` y `pytest tools/monitor-base`).
