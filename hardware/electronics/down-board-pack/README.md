---
title: "Pack autocontenido — placa DOWN del robot Soccer 2026"
date: 2026-05-24
status: vigente
audiencia: "IA + humanos que programen la placa DOWN"
fuente-canonica: "docs/FUENTES-DE-VERDAD.md"
---

# Pack autocontenido — placa DOWN

## Para qué existe este directorio

Todo lo que hace falta saber para **programar, testear y diagnosticar** la
placa DOWN del robot Soccer 2026, **en un solo lugar**.

El repo tiene la información dispersa entre `hardware/electronics/`,
`docs/firmware/`, `software/teensy/Soccer 2026/src/down/`,
`software/teensy/Soccer 2026/src/shared/`, `software/teensy/Soccer 2026/test/`,
`software/teensy/Soccer 2026/scripts/`, `docs/decisions/`, etc. Eso obliga a
la IA (y a los humanos nuevos) a saltar entre 8 directorios distintos para
entender una placa.

Este pack consolida **solo lo correcto y vigente** para la placa DOWN. No
contiene historia, journals, planes ya ejecutados, ni docs superados.

## Regla de oro

> Si lo que está en este pack **contradice** algo del repo vivo
> (`software/teensy/Soccer 2026/src/down/...`), **gana el repo vivo**.
> Este pack es una **foto curada del 2026-05-24** — los `.cpp/.h` y los
> tests son copias snapshot, NO son la fuente compilable.

**El código compilable y los tests siguen viviendo en sus ubicaciones
originales** del repo:
- Compilación: `pio run -e down` (apunta a `src/down/`).
- Tests host-native: `pio test -e native` (apunta a `test/test_down_*`).
- Diag de hardware: `pio run -e diag_down` (apunta a `src/diag/`).

**Si actualizás el código original**, sincronizá este pack o avisalo al
empezar la próxima sesión.

## Estructura del pack

```
down-board-pack/
├── README.md                         ← estás aquí
├── 01-pinout-y-posiciones.md         ← fuente única del pinout + posiciones físicas
├── 02-funcionalidad.md               ← qué hace el firmware y cómo
├── 03-contrato-datos.md              ← contrato binario byte-a-byte de las tramas
├── 04-protocolo-comunicaciones.md    ← diseño general de comm de las 3 placas
├── firmware/                         ← código fuente vivo del firmware DOWN
│   ├── down/                         ← módulos específicos de la placa DOWN
│   │   ├── main_down.cpp
│   │   ├── line_ring.{h,cpp}
│   │   ├── otos.{h,cpp}
│   │   ├── comm_top.{h,cpp}          ← UART → TOP (Serial5, U10)
│   │   ├── comm_central.{h,cpp}      ← UART → CENTRAL (Serial1, U11)
│   │   └── config_down.h
│   └── shared/                       ← módulos compartidos usados por DOWN
│       ├── down_model.{h,cpp}
│       ├── down_encode.{h,cpp}
│       ├── line_geometry.{h,cpp}
│       ├── line_tracker.{h,cpp}
│       ├── line_calib.{h,cpp}
│       ├── line_filters.{h,cpp}
│       ├── surface_monitor.{h,cpp}
│       ├── proto.{h,cpp}             ← frame protocol + CRC-16
│       └── types.h                   ← LineStatus, Pose2D, Velocity2D
├── tests/                            ← tests host-native que cubren DOWN
│   ├── test_down_calib.cpp
│   ├── test_down_encode.cpp
│   ├── test_down_geometry.cpp
│   ├── test_down_model.cpp
│   ├── test_down_surface.cpp
│   ├── test_down_tracker.cpp
│   ├── test_line_filters.cpp
│   └── test_proto.cpp
├── diag/                             ← diagnóstico de hardware standalone
│   ├── main_diag_down.cpp            ← binario diag_down (toggle por mux + dump)
│   ├── diag_capture.py               ← captura COM + veredicto OK/sospechoso/muerto
│   └── extract_pinout_from_schematic.py  ← regenera 01-pinout desde SCH+PCB JSONs
└── ground-truth/                     ← fuentes EasyEDA crudas (NO editar manualmente)
    ├── SCH_Roboliga_2026_Futbol_2026-04-12.json   ← schematic JSON (290 KB)
    ├── PCB_PCB_Roboliga_2026_Futbol_2026-04-12.json  ← PCB layout JSON (1.6 MB)
    ├── Schematic_Roboliga_2026_Futbol_2026-04-12.pdf  ← schematic legible
    └── BOM_Roboliga_2026_Futbol_2026-04-12.csv    ← BOM (UTF-16 LE)
```

## Índice por pregunta

Cada pregunta apunta al doc que la responde sin ambigüedad:

| Pregunta | Doc del pack |
|---|---|
| ¿Qué pin del Teensy va a qué pin del CD4051? | `01-pinout-y-posiciones.md` §3, §4 |
| ¿Dónde está físicamente el sensor S17 en el robot? (x, y mm) | `01-pinout-y-posiciones.md` §5b |
| ¿Cómo está el sistema de referencia? (0,0 = centro, +X = ?, +Y = ?) | `01-pinout-y-posiciones.md` §5b |
| ¿Qué hace la placa DOWN? | `02-funcionalidad.md` §1, §3 |
| ¿Qué modos de operación tiene? | `02-funcionalidad.md` §4 |
| ¿Cómo lee el anillo de 32 sensores? | `02-funcionalidad.md` §5 + `firmware/down/line_ring.cpp` |
| ¿Cómo calcula el ángulo de la línea? | `02-funcionalidad.md` §5.3 + `firmware/shared/line_geometry.cpp` |
| ¿Cómo detecta "robot levantado"? | `02-funcionalidad.md` §5.5 + `firmware/shared/surface_monitor.cpp` |
| ¿Cómo lee los 2 OTOS? | `02-funcionalidad.md` §6 + `firmware/down/otos.cpp` |
| ¿Qué pines usa la UART hacia TOP? | `01-pinout-y-posiciones.md` §7.1 (Serial5, RX=21, TX=20, conector U10) |
| ¿Qué pines usa la UART hacia CENTRAL? | `01-pinout-y-posiciones.md` §7.2 (Serial1, RX=0, TX=1, conector U11) |
| ¿Qué pines usa cada OTOS para I²C? | `01-pinout-y-posiciones.md` §6 (U5: Wire 18/19; U6: Wire1 17/16) |
| ¿Qué dirección I²C tiene cada OTOS? | `01-pinout-y-posiciones.md` §6 (ambos 0x17 — por eso buses separados) |
| ¿Qué tramas envía DOWN y cada cuánto? | `02-funcionalidad.md` §7 + `03-contrato-datos.md` |
| ¿Cuál es el contrato binario exacto de las tramas? | `03-contrato-datos.md` |
| ¿Por qué hay 2 UARTs (no 1)? | `04-protocolo-comunicaciones.md` §3 (bus de emergencia <15 ms) |
| ¿Qué multiplexor atiende qué sensores? | `01-pinout-y-posiciones.md` §5 (S1–S8 → U1, S9–S16 → U2, S17–S24 → U3, S25–S32 → U4) |
| ¿Qué hay que cambiar en `config_down.h`? | `01-pinout-y-posiciones.md` §10 |
| ¿Cómo verifico el hardware de la placa? | `02-funcionalidad.md` §11.3 + `diag/main_diag_down.cpp` |
| ¿Cómo regenero el pinout si cambia el PCB? | Correr `diag/extract_pinout_from_schematic.py` |

## Lo que NO está en este pack (y por qué)

| Categoría | Por qué no está |
|---|---|
| Journals (`journal/`) | Es historia. El pack sólo tiene info útil para programar. |
| TASKs (`team-tasks/`) | Son acciones humanas, no spec técnica. |
| Plans superpowers (`docs/superpowers/`) | Son planes ya ejecutados. Lo ejecutado vive en el código vivo. |
| Research in-progress (`research/`) | Análisis históricos. La conclusión vive en los docs canónicos. |
| `mapa-pines-placas-nuevas.md` | Superado por `01-pinout-y-posiciones.md` (decía cosas incorrectas como "A/B/C compartidas"). |
| `2026-05-17-placa-base-down-componentes-y-circuito.md` | Tiene "open items" ya resueltos por el doc 19-may. Si querés la parte BOM/alimentación se podría extraer aparte. |
| `FIRMWARE-PLACA-ABAJO.md` (versión vieja) | Está marcado "PARCIALMENTE SUPERADO" en el repo. La versión curada vive en `02-funcionalidad.md` de este pack. |
| Libs vendoreadas (`lib/Adafruit_*`) | Son del BNO055 de CENTRAL/TOP, no de DOWN. |
| Código de TOP, CENTRAL, COMM | No es responsabilidad de DOWN. Si necesitás entender la **otra punta** del cable, mirar `software/teensy/Soccer 2026/src/{top,central}/comm_*`. |

## Pendientes humanos (NO bloquean uso del pack)

Algunas decisiones de hardware requieren validación física por Enzo o
Virginia/Elías antes de que el pack se considere 100% confirmado:

1. **Cotejo schematic ↔ multímetro**: 3 nets críticas (pin Arduino 13 ↔ pin 11
   de CD4051 U1, etc.). Plan en `01-pinout-y-posiciones.md` §12.
2. **Orientación del montaje del PCB en el chasis**: asumido +Y = adelante;
   si rota, se rota la LUT `SENSOR_POS[]` trivial. Ver §5b.
3. **Ambos OTOS poblados** (U5 y U6 soldados). Ver §6.
4. **Aplicar el pinout corregido a `config_down.h`** una vez validado.
5. **Vendorear la lib SparkFun OTOS** para activar el bloque `TODO_OTOS_LIB`
   en `firmware/down/otos.cpp`.

Estos pendientes viven en el repo como `team-tasks/2026-05-19-task-026-...md`
+ `team-tasks/2026-05-15-task-012-activar-libs-otos-tof.md`. NO se duplican
acá para no contaminar el pack con tareas operativas.

## Atribución

- Fuentes originales del repo (`docs/firmware/`, `hardware/electronics/`, `src/down/`, etc.) — equipo IITA Salta + sesiones Claude previas (ver journals).
- Curado y consolidación del pack — Claude Opus 4.7 (Anthropic), sesión 2026-05-24.
- Requested-by — Gustavo Viollaz (@gviollaz).
