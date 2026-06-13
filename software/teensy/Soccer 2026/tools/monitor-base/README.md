# monitor-base — App de PC de monitoreo/calibración de la placa base (DOWN)

App gráfica para **ver de forma simple lo que leen los sensores de la base** —los
32 sensores de luz **y** la odometría OTOS— y **calibrar** la línea, leyendo la
telemetría USB del firmware DOWN. Es la FASE 1 (P0) del sistema de monitoreo
(TASK-304/305); la FASE 2 (TOP) es TASK-205/206.

> Pensada para el banco: ponés el robot en la cancha, lo conectás por USB a la
> Teensy de la base, y moviéndolo sobre las líneas ves en vivo qué ve cada sensor,
> qué línea detecta, **qué le manda a la CENTRAL**, y la pose de los OTOS — sin
> leer 32 números crudos en el Serial Monitor.

> 📖 **¿Cómo se activa/desactiva y se usa en banco** (¿hay que reflashear?, ¿cómo vuelvo a
> competencia?, calibración que persiste, diagnóstico de problemas)? → guía paso a paso:
> [`docs/firmware/USO-MONITOREO-Y-TELEMETRIA.md`](../../docs/firmware/USO-MONITOREO-Y-TELEMETRIA.md).

## Qué muestra

- **Anillo de 32 sensores** en su **geometría real del PCB** (3 anillos, espejo de
  `src/shared/sensor_geometry.cpp`): cada sensor coloreado por su valor crudo,
  resaltado si ve blanco, en **rojo si está muerto/pegado/saturado**.
- **Línea detectada** como flecha (ángulo) + penetración + cross-track.
- **Interpretación que viaja a la CENTRAL**: el `LineStatusV2` real (válido,
  presente, ángulo, escape, penetración, cross-track, calidad, edad, eventos).
- **Odometría OTOS**: x/y/heading, velocidades, slip, OTOS izq/der sanos, levantado.
- **Calibración asistida**: botones carpet/blanco/auto/guardar/cargar + detección
  de sensores con margen bajo (no separan piso de blanco).
- **Sintonía fina (v3)**: slider de **sensibilidad global** al blanco (ver en vivo
  cómo reaccionan los sensores); grilla de **barras de cercanía al umbral** por sensor
  (barra a la derecha = ve blanco, izquierda = ve piso); **click en un sensor** (anillo o
  barra) abre el **inspector** (valores crudos/calib/umbral + botón habilitar/deshabilitar
  + slider de sensibilidad individual), y **doble-click** en el anillo o la barra
  habilita/deshabilita ese sensor directo. Todo se persiste con `CAL SAVE`.

## Correr

> El firmware de competencia (`down` / `down_robot2`, flag `-DDOWN_USB_MONITOR`)
> arranca con el **monitor USB dormido** (no manda nada). Esta app lo **activa sola**
> al conectarse (envía `STREAM ON`); ya **no hay un env de banco aparte** (el viejo
> `down_debug_telemetry` se eliminó). Para flashear: `pio run -e down -t upload`.

```bash
# Robot real (Teensy DOWN flasheada con el env de competencia down — la app
# activa la telemetría sola al conectar; antes había un env de banco aparte):
python -m monitor_base --port COM5          # Windows
python -m monitor_base --port /dev/ttyACM0  # Linux/Mac
python -m monitor_base --port auto           # autodetecta el Teensy
python -m monitor_base --list-ports          # lista los COM y cuál parece el Teensy

# Sin robot, para desarrollar/demostrar:
python -m monitor_base --sim
python -m monitor_base --sim --sim-dead 7,18   # inyecta sensores "muertos"

# Reproducir una grabación .jsonl:
python -m monitor_base --replay grabacion.jsonl

# Grabar la sesión a archivo mientras mirás (para replay/análisis offline):
python -m monitor_base --port COM5 --record sesion.jsonl

# Smoke headless (sin ventana — sirve en CI / para verificar que corre):
python -m monitor_base --selftest
```

### Vista de ARQUERO (seguidor de línea + OTOS)

Para **probar en banco el seguidor de línea del arquero** (se queda centrado sobre la línea
de su arco y barre lateralmente):

```bash
python -m monitor_base --arquero --sim       # sin robot
python -m monitor_base --arquero --port COM5  # robot real (env de competencia down; la app activa la telemetría sola)
```

Muestra: un **medidor de cross-track** (mm a la línea, objetivo 0), el **arco trasero** del
anillo resaltado (los sensores que ven la línea del arco) con la flecha de la línea, y la
**estela de la trayectoria por OTOS** con **cada OTOS izq/der por separado** (para ver el
diferencial / que avance derecho). Requiere telemetría **schema v2**.

### Vista del campo (placa TOP, FASE 2)

La misma app tiene una **vista TOP** (`--top`): un **radar robot-céntrico** (el robot al
centro, su frente hacia arriba) con la pelota, los arcos amarillo/azul, los ToF, una
brújula de heading, y el panel del **WorldSnapshot que TOP manda a la CENTRAL**. Botones
IMU ZERO / IMU SAVE / stream / rate.

```bash
python -m monitor_base --top --sim            # campo simulado (sin robot)
python -m monitor_base --top --port COM6       # placa TOP real (env top_robot1_debug_telemetry)
python -m monitor_base --top --replay top.jsonl
python -m monitor_base --top --selftest        # smoke headless
```

`--port` requiere `pip install pyserial`. El resto usa solo la stdlib de Python
(incluido tkinter, que viene con CPython en Windows).

## Comandos al firmware (botones de la GUI)

`CAL CARPET`, `CAL WHITE`, `CAL AUTO ON/OFF`, `CAL SAVE`, `CAL LOAD`, `OTOS RESET`,
`STREAM ON/OFF`, `RATE <hz>`.

**Sintonía fina (schema v3):**
- `SENS GLOBAL <pct>` — sensibilidad global al blanco, `pct ∈ [-100,+100]` (**+ = menos
  sensible**, sube el umbral; 0 = punto medio histórico). **Los extremos SATURAN: -100 →
  todos los sensores leen blanco; +100 → ninguno.** Slider "Sensibilidad global". (La app
  pinta "ve blanco" = `raw ≥ umbral efectivo`, así el slider barre el anillo de verdad.)
- `SENS SET <i> <pct>` — sensibilidad del sensor `i` (slider del inspector).
- `SENSOR <i> ON|OFF` — habilitar/deshabilitar el sensor `i` (botón del inspector). Un
  sensor OFF se excluye del centroide/penetración (como un sensor unhealthy).

`CAL SAVE` ahora persiste **también** la sensibilidad (global + por sensor) y el mapa de
habilitados. Ver el contrato completo en
[`docs/firmware/TELEMETRIA-DOWN.md`](../../docs/firmware/TELEMETRIA-DOWN.md).

> **Defaults = no-op:** todos los sensores habilitados, sensibilidad 0 → umbral en el
> punto medio = comportamiento de competencia histórico. Las perillas solo cambian la
> detección si las movés.

## Arquitectura

Núcleo PURO (testeable, sin GUI) + una vista delgada en Tkinter:

| Módulo | Rol |
|--------|-----|
| `protocol.py` / `protocol_top.py` | Parser del contrato JSON Lines v1 (espejo de `telemetry_down.h` / `telemetry_top.h`). |
| `geometry.py` | LUT de las 32 posiciones físicas (espejo de `sensor_geometry.cpp`). |
| `sensor_health.py` | Detector de sensores muertos/pegados/saturados (rango en ventana). |
| `calibration.py` | Asistente de auto-calib (min/max, umbral, margen, sospechosos). |
| `simulator.py` / `simulator_top.py` | Genera telemetría sintética DOWN / TOP (sin robot) por el formato real. |
| `sources.py` | Fuentes serial / replay / sim (DOWN y TOP, parser inyectable) + helpers puros. |
| `gui.py` / `gui_gk.py` / `gui_top.py` | Vista Tkinter base (anillo) / arquero (línea+OTOS) / campo TOP (radar). |
| `recorder.py` | Graba la telemetría entrante a `.jsonl` (`--record`). |
| `__main__.py` | CLI (`--sim/--port/--replay/--record/--selftest/--top`) + selftests headless. |

## Tests

```bash
cd tools/monitor-base
python -m pytest            # requiere: pip install pytest
```

El test `test_protocol.py` valida el parser contra **el mismo golden que emite el
firmware C++** (`tests/golden_frame_v1.jsonl`, generado por
`test/test_telemetry_down/`), garantizando el contrato cross-lenguaje. Si cambia
el schema, regenerá el golden (ver `docs/firmware/TELEMETRIA-DOWN.md` §5).
