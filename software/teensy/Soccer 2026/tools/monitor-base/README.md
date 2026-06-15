# monitor-base — Monitor y calibración del robot (IITA Soccer Open)

**Una sola aplicación** para **monitorear y calibrar** todo el robot: la placa
**SUPERIOR (TOP)** y la placa **BASE (DOWN)**. Se conecta sola al Teensy que
encuentre, detecta qué placa es por los datos, y si **movés el USB de una placa a
la otra** cambia sola — **sin perder la historia** de la otra.

> La guía completa también está **dentro de la app** (botón **Ayuda** / botón **?**
> de cada vista) y por consola con **`python -m monitor_base --readme`**. Este
> README es el resumen; la fuente del texto vive en `monitor_base/help_text.py`.

## Cómo se corre

```bash
python -m monitor_base                  # autodetecta puerto y placa, abre la consola
python -m monitor_base --sim            # sin robot (datos simulados, para probar la UI)
python -m monitor_base --port COM7      # forzar un puerto
python -m monitor_base --replay x.jsonl # reproducir una grabación
python -m monitor_base --readme         # muestra la guía completa y sale
python -m monitor_base --list-ports     # lista los COM y cuál parece el Teensy
```

Requisitos: `pip install pyserial` (para el robot real; el simulador no lo necesita).

## Hot-swap de placa

Con la app abierta podés **desconectar el USB de la placa superior y conectarlo a la
placa base**: la app lo detecta, reconecta sola y muestra las vistas de la base (y
deja calibrar). La historia/estelas de la placa superior **no se pierden**. La barra
superior indica a qué placa estás conectado y el estado del enlace.

## Identidad de robot (R1/R2) y anti-cruce de config

La barra superior muestra un **chip con el robot conectado** (`▣ Robot 1 · TOP`),
identificado por el **N° de serie USB** del Teensy (la telemetría no trae ID de robot).
Con eso:

- **La config de ToF se guarda POR PLACA** (`tof_layout_<serial>.json`): R1 y R2 **no
  comparten archivo** → imposible aplicarle a uno la config del otro.
- **Toda escritura a la EEPROM del robot** (`CFG SAVE`, `CAL SAVE`, `IMU SAVE`, `CFG
  RESET`) **pide confirmación nombrando el robot** conectado.
- Un Teensy **desconocido** se muestra con su serial (`⚠ Robot ? · serial …`) — igual no
  se cruza (cada serial su archivo). Para ponerle nombre, agregá su serial a
  `monitor_base/robot_registry.json` (`{"<serial>": {"robot": 2, "board": "top", "name":
  "Robot 2 · TOP"}}`) o al seed de `robot_registry.py`.

La calibración de línea (verde/blanco, sensibilidad) **vive en la EEPROM de cada robot**,
no en la app → no se cruza. Persistir en la EEPROM del TOP la config de ToF
(posición/zonas) y un offset fino de cámara es trabajo de firmware: ver
`team-tasks/2026-06-15-task-214-*` (+ TASK-206).

## Vistas

**Placa SUPERIOR (TOP):**
- **Cancha** — robot ubicado (pose del snapshot a CENTRAL) con orientación + estela,
  pelota con estela, overlay OTOS opcional para cross-check.
- **Polar** — cenital robot-céntrica: conos de ToF con profundidad por zona + conos
  de cámara + pelota/arcos.
- **ToF 360** — las 4 grillas de zonas (izq | frente | der | atrás) + cámaras arriba.
- **Salud** — semáforo por sensor + zonas + config (apagar sensores, posición ToF, EEPROM).
- **Cámara** — fusión frontal/trasera (anti-pelota-fantasma).
- **Timeline** — caja-negra en vivo del snapshot + flapping.
- **Config ToF** — ubicar/rotar/espejar ToF y vetar zonas por altura de pared + cancha.

**Placa BASE (DOWN) — calibración:**
- **Base** — anillo de 32 sensores + línea + OTOS + **calibración** (capturar
  verde/blanco, sensibilidad, habilitar sensores, guardar EEPROM).
- **Arquero** — cross-track + anillo trasero + estela OTOS izq/der (banco del seguidor de línea).

**General:** **Logs** (eventos + anomalías + datos, con filtros y export) · **Ayuda**.

## Calidad / honestidad

- Es herramienta de monitoreo/calibración: **no toca el binario de competencia**.
- Lógica pura host-testeada (≈200 tests `pytest`). Smoke headless del shell.
- Lo que dependa de la homografía de cámara sin calibrar (posición absoluta de
  pelota/arcos) se muestra como **estimado** hasta cerrar **TASK-022**; el FOV/azimut
  de los ToF los expone el firmware en paralelo (conos del polar aproximados hasta entonces).

## Tests

```bash
cd tools/monitor-base
python -m pytest            # requiere: pip install pytest
python -m monitor_base --monitor --selftest   # smoke headless del shell (construye todos los paneles)
```
