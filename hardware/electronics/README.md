# Electrónica del robot Soccer 2026

Diseño y documentación de las placas PCB, sensores y cámaras del robot.

## 👉 Para programar un subsistema: usar los packs

El recurso de entrada principal de este directorio son los **5 packs
autocontenidos** (uno por subsistema programable):

📦 **[PACKS-INDEX.md](PACKS-INDEX.md)** — índice maestro de los 5 packs con
"qué pack abrir para qué tarea".

| Pack | Para programar / calibrar |
|---|---|
| [`down-board-pack/`](down-board-pack/) | placa DOWN (32 sensores luz + 2 OTOS) |
| [`central-board-pack/`](central-board-pack/) | placa CENTRAL (Zircon, FSM + motores + PIDs) |
| [`top-board-pack/`](top-board-pack/) | placa TOP (master de cámaras + IMU + ToF) |
| [`cameraFront-pack/`](cameraFront-pack/) | cámara OpenMV frontal |
| [`cameraBack-pack/`](cameraBack-pack/) | cámara OpenMV trasera |

Cada pack tiene su propio `README.md` con un **índice "pregunta → doc"** que
responde sin ambigüedad qué archivo abrir para cada cosa. **Si vas a
programar, abrir el pack antes que cualquier otro doc del repo**.

## Otros contenidos del directorio

### Schematic + PCB crudos (ground-truth)

```
pcb_design/
├── down_board/      ← SCH JSON + PCB JSON + PDF + BOM de la placa DOWN
└── top_board/       ← SCH JSON + PCB JSON + PDF + BOM de la placa TOP
```

> Los JSONs son la **fuente de verdad** del pinout. El script
> `software/teensy/Soccer 2026/scripts/extract_pinout_from_schematic.py`
> parsea automáticamente cualquiera de ellos. Los packs DOWN y TOP incluyen
> una copia de estos archivos en su carpeta `ground-truth/`.

### Placa COMM (ESP32-C6, separada)

```
comm-board/
├── 2026-05-17-placa-comm-componentes-y-circuito.md     ← descripción
├── 2026-05-17-procedimiento-flash-firmware-c6.md       ← cómo flashear
└── RECURSOS-Y-ENLACES.md                                ← repos y docs externos
```

La placa COMM (módulo BLE para árbitros) tiene su propio mini-pack acá. Su
firmware vive en el repo oficial RCJ (`soccer-communication-module`), no en
este repo, por eso no tiene un pack del mismo tipo que los otros 5.

### Docs históricos (NO usar como guía de programación — ver packs)

| Doc | Estado | Reemplazado por |
|---|---|---|
| [`2026-05-17-placa-base-down-componentes-y-circuito.md`](2026-05-17-placa-base-down-componentes-y-circuito.md) | Histórico | `down-board-pack/01-pinout-y-posiciones.md` (más completo y actualizado) |
| [`2026-05-17-placa-top-analisis-gerbers.md`](2026-05-17-placa-top-analisis-gerbers.md) | Histórico | `top-board-pack/01-pinout-y-hardware.md` (integra esta info) |
| [`2026-05-19-pinout-down-extraido-schematic.md`](2026-05-19-pinout-down-extraido-schematic.md) | Vigente, pero también incluido como copia en `down-board-pack/01-pinout-y-posiciones.md` | — (es la misma fuente) |
| [`mapa-pines-placas-nuevas.md`](mapa-pines-placas-nuevas.md) | **Superado** — decía "A/B/C compartidas" lo cual es incorrecto | Pack DOWN y pack TOP (ambos extraen del SCH JSON real) |
| [`mapa-pines-teensy-ambos-robots.md`](mapa-pines-teensy-ambos-robots.md) | Histórico (2026-03-20), única referencia textual del pinout del Zircon | `central-board-pack/01-pinout-y-hardware.md` (cura esta info + marca conflicto pines 7/8) |

### Placa Zircon (legado 2025)

El equipo 2025 desarrolló una PCB custom llamada "Zircon" (la que el equipo
2026 reutiliza como **placa CENTRAL**) con librería Arduino propia
(`zirconLib`). El código original del 2025 se preserva en
`legacy/2025-season/code/libraries/`. El firmware nuevo del 2026 NO usa
`zirconLib` — implementa su propio driver de motores en
`software/teensy/Soccer 2026/src/central/motors_zircon.{h,cpp}` (vivo, incluido
en el pack CENTRAL).

## Cómo mantener este directorio prolijo

- **Cualquier doc nuevo de electrónica**: agregar al índice arriba o al
  `PACKS-INDEX.md`.
- **Cualquier doc viejo que quede superado**: agregar fila a "Docs históricos"
  con el reemplazo, y en el doc viejo pegar un banner que apunte acá.
- **Cualquier pack nuevo**: agregar fila a la tabla de packs + actualizar
  `PACKS-INDEX.md` + actualizar `docs/FUENTES-DE-VERDAD.md`.
