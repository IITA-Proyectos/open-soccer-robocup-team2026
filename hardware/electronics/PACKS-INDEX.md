---
title: "Índice de packs de programación — Robot Soccer 2026"
date: 2026-05-24
status: vigente
audiencia: "IA + humanos que programen cualquier subsistema del robot"
fuente-canonica: docs/FUENTES-DE-VERDAD.md
---

# Índice de packs de programación

## Para qué existe este índice

El robot Soccer 2026 tiene **5 subsistemas programables** (3 placas Teensy + 2
cámaras OpenMV). Para cada uno hay un **pack autocontenido** en
`hardware/electronics/<subsistema>-pack/` con TODO lo necesario para
programarlo, calibrarlo y diagnosticarlo en un solo lugar.

**Usar este índice como punto de entrada** cuando vayas a trabajar sobre un
subsistema específico. En vez de saltar entre 8 directorios del repo
(`hardware/`, `docs/firmware/`, `software/teensy/.../src/`, `test/`, etc.),
abrir el pack del subsistema y empezar por su `README.md`.

## Los 5 packs

| Subsistema | Pack | Hardware | Archivos | Tamaño | Estado |
|---|---|---|---|---|---|
| **DOWN** — sensores piso | [`down-board-pack/`](down-board-pack/) | Teensy 4.0 + 32 ALS-PT19 + 4 CD4051 + 2 SparkFun OTOS | 47 | 2.4 MB | Pinout completo extraído del SCH JSON automáticamente |
| **CENTRAL** — cerebro motor | [`central-board-pack/`](central-board-pack/) | Teensy 4.1 + Zircon Rev v15 + 3 motores omni + kicker (ROBOT2) — **sin BNO** (está en el TOP) | 40 | 362 KB | Sin schematic JSON disponible, 1 conflicto pendiente |
| **TOP** — cerebro sensorial | [`top-board-pack/`](top-board-pack/) | Teensy 4.0 master + 2 BNO055 + 4 ToF + HC-SR04 + 5 UARTs | 34 | 725 KB | Con SCH JSON, 2 conflictos detectados |
| **Cámara FRONTAL** | [`cameraFront-pack/`](cameraFront-pack/) | OpenMV H7 / H7 Plus (mira +Y del robot) | 16 | 152 KB | Hay 1 script genérico actual; el objetivo es split en `cam_frontal.py` |
| **Cámara TRASERA** | [`cameraBack-pack/`](cameraBack-pack/) | OpenMV H7 / H7 Plus (mira −Y del robot) | 16 | 156 KB | Hay 1 script genérico actual; el objetivo es split en `cam_trasera.py` |

**Total**: 5 packs, 153 archivos, ≈3.8 MB.

## Qué pack abrir según la tarea

| Tarea | Pack a abrir |
|---|---|
| Programar la lectura de los 32 sensores de línea | `down-board-pack/` |
| Programar el filtrado de ruido de la línea | `down-board-pack/` |
| Calibrar / activar OTOS odométricos | `down-board-pack/` |
| Cambiar el formato de las tramas LINE_URGENT o OTOS_POSE | `down-board-pack/` |
| Programar la FSM táctica (estrategia del partido) | `central-board-pack/` |
| Tunear PIDs (heading, lateral, approach) | `central-board-pack/` |
| Modificar la cinemática inversa omni-3 | `central-board-pack/` |
| Configurar pinout de motores (arquero vs delantero) | `central-board-pack/` |
| Programar el kicker (solenoide) | `central-board-pack/` |
| Programar el watchdog de motor / EMERGENCY_LINE | `central-board-pack/` |
| Programar el parser de cámaras del lado Teensy | `top-board-pack/` |
| Programar el IMU dual (BNO055 × 2) | `top-board-pack/` |
| Activar los ToF VL53 multizona | `top-board-pack/` |
| Programar el HC-SR04 frontal | `top-board-pack/` |
| Programar la fusión cameras + IMU + ToF | `top-board-pack/` |
| Construir el `WorldSnapshot` que va al CENTRAL | `top-board-pack/` |
| Calibrar / programar la cámara que mira hacia adelante | `cameraFront-pack/` |
| Calibrar / programar la cámara que mira hacia atrás | `cameraBack-pack/` |
| Ajustar thresholds LAB de los colores | el pack de la cámara correspondiente |
| Calibrar homografía de una cámara | el pack de la cámara correspondiente |

## Estructura común de los packs

Cada pack tiene la **misma forma** para que la IA y los humanos sepan dónde
buscar sin importar el pack:

```
<subsistema>-pack/
├── README.md                        ← punto de entrada con "índice por pregunta"
├── 01-<...>.md                      ← hardware / pinout / hardware-y-conexion
├── 02-funcionalidad.md              ← qué hace y cómo
├── 03-<...>.md                      ← contrato de datos / protocolo
├── 04+ docs específicos             ← calibración, comunicaciones, arquitectura
├── firmware/                        ← snapshot del código vivo
│   ├── <subsistema>/                ← específico
│   └── shared/ (cuando aplica)      ← módulos compartidos
├── tests/                           ← tests host-native que cubren este subsistema
└── ground-truth/ (cuando aplica)    ← SCH JSON / PCB JSON / PDF / BOM crudos
```

> 📌 No todos los packs tienen `ground-truth/`: solo los que tienen el
> schematic JSON disponible en el repo (DOWN y TOP). CENTRAL no tiene
> (Zircon Rev v15 del 2025, solo PDF) y las cámaras no aplica (módulo OpenMV
> comercial).

## Regla de oro de los packs

> **Si lo que está en un pack contradice al repo vivo
> (`software/teensy/Soccer 2026/src/...` y `software/vision/`), gana el repo
> vivo.**
>
> Los packs son **snapshots del 2026-05-24** — los `.cpp/.h/.py` son copias
> snapshot, NO son la fuente compilable/ejecutable. La compilación y los
> tests siguen viviendo en sus ubicaciones originales.

## Cuándo usar el repo vivo vs un pack

| Acción | Repo vivo o pack |
|---|---|
| Lectura/entendimiento del subsistema completo | **Pack** (todo en un lugar) |
| Búsqueda rápida de cómo funciona algo | **Pack** (índice "pregunta → doc") |
| Modificar el código del firmware | **Repo vivo** (`software/teensy/.../src/...`) |
| Correr tests | **Repo vivo** (`pio test -e native`) |
| Compilar y flashear | **Repo vivo** (`pio run -e down`, etc.) |
| Calibrar la cámara OpenMV | **Repo vivo** (`software/vision/`) flasheado desde el OpenMV IDE |
| Generar documentación nueva | **Repo vivo** (`docs/` o `hardware/electronics/`) |

## Cómo mantener los packs actualizados

Los packs son snapshots del 2026-05-24. Si el repo vivo cambia
significativamente (refactor del firmware, nuevo schematic, etc.), conviene
**regenerar los packs**:

1. **Para DOWN y TOP** (los que tienen ground-truth): correr
   `software/teensy/Soccer 2026/scripts/extract_pinout_from_schematic.py`
   apuntado al JSON nuevo. Esto actualiza el doc `01-pinout-y-posiciones.md`
   automáticamente.
2. **Para todos los packs**: re-copiar los archivos `firmware/` y `tests/`
   con `cp` desde sus ubicaciones originales en `src/<subsistema>/` y
   `test/test_<subsistema>_*/`.
3. **Curado de docs**: si el firmware ganó una funcionalidad nueva, leer el
   `02-funcionalidad.md` del pack y actualizarlo a mano.
4. **Commit + push**: dejar el snapshot consolidado en el repo con la fecha
   actualizada en el frontmatter de cada doc del pack.

> 📌 **Esto NO es automático todavía**. Si querés un script `regenerate-packs.sh`
> que haga el copy-snapshot de los archivos vivos a los packs, se puede hacer
> en una sesión posterior. Por ahora la regeneración es manual.

## Lo que NO está en los packs (y por qué)

| Categoría | Por qué |
|---|---|
| Journals (`journal/`) | Son historia. Los packs sólo tienen info útil para programar. |
| TASKs (`team-tasks/`) | Son acciones humanas, no spec técnica. |
| Plans / specs / decisions (`docs/superpowers/`, `docs/decisions/`) | Son trabajo de coordinación. Lo decidido y aplicado vive en el código vivo o en los docs canónicos copiados al pack. |
| Schematic Zircon (PDF) | Binario pesado del 2025, fuera del scope del pack CENTRAL (no se incluye, se referencia). |
| Código legacy (`legacy/2025-season/`, `software/robot-arquero/`, `software/robot-delantero/`) | Es del equipo 2025. El firmware nuevo del 2026 vive en `software/teensy/Soccer 2026/`. |
| Docs superados del repo | Cualquiera de los `mapa-pines-placas-nuevas.md`, `2026-05-17-placa-*-analisis-gerbers.md`, etc. — fueron consolidados / curados en los packs. |

## Referencias

- **`docs/FUENTES-DE-VERDAD.md`** — tabla canónica del repo. Cada pack está registrado como fuente.
- **`docs/ESTADO-ACTUAL.md`** — snapshot del estado del robot. Menciona los packs como recurso de entrada.
- **`docs/ARQUITECTURA-3-PLACAS-2026.md`** — visión arquitectónica general (incluida en los packs CENTRAL y TOP).
- **`CLAUDE.md`** — protocolo de sesión Claude en el repo.
