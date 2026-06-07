# monitor-base — App de PC de monitoreo/calibración de la placa base (DOWN)

App gráfica para **ver de forma simple lo que leen los sensores de la base** —los
32 sensores de luz **y** la odometría OTOS— y **calibrar** la línea, leyendo la
telemetría USB del firmware DOWN. Es la FASE 1 (P0) del sistema de monitoreo
(TASK-304/305); la FASE 2 (TOP) es TASK-205/206.

> Pensada para el banco: ponés el robot en la cancha, lo conectás por USB a la
> Teensy de la base, y moviéndolo sobre las líneas ves en vivo qué ve cada sensor,
> qué línea detecta, **qué le manda a la CENTRAL**, y la pose de los OTOS — sin
> leer 32 números crudos en el Serial Monitor.

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

## Correr

```bash
# Robot real (Teensy DOWN flasheada con env down_debug_telemetry):
python -m monitor_base --port COM5          # Windows
python -m monitor_base --port /dev/ttyACM0  # Linux/Mac

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
`STREAM ON/OFF`, `RATE <hz>`. Ver el contrato completo en
[`docs/firmware/TELEMETRIA-DOWN.md`](../../docs/firmware/TELEMETRIA-DOWN.md).

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
| `gui.py` / `gui_top.py` | Vista Tkinter de la base (anillo) / del campo TOP (radar). |
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
