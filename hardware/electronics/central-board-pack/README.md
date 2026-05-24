---
title: "Pack autocontenido — placa CENTRAL del robot Soccer 2026"
date: 2026-05-24
status: vigente
audiencia: "IA + humanos que programen la placa CENTRAL"
fuente-canonica: "docs/FUENTES-DE-VERDAD.md"
---

# Pack autocontenido — placa CENTRAL

## Para qué existe este directorio

Todo lo que hace falta saber para **programar, testear y diagnosticar** la
placa CENTRAL del robot Soccer 2026, **en un solo lugar**.

CENTRAL es **el cerebro motor**: recibe percepción ya digerida (`WORLD_SNAPSHOT`
del TOP + `LINE_URGENT` del DOWN), decide qué hacer con la FSM táctica, calcula
PIDs y mueve los motores. Es una placa con **3 capas** (FSM → PIDs → motores)
que se ejecutan a 100 Hz, más un **canal de emergencia** que bypassa la FSM en
~2 ms si el robot está por salir de la cancha.

## Regla de oro

> Si lo que está en este pack **contradice** algo del repo vivo
> (`software/teensy/Soccer 2026/src/central/...`), **gana el repo vivo**.
> Este pack es una **foto curada del 2026-05-24** — los `.cpp/.h` y los tests
> son copias snapshot, NO son la fuente compilable.

**Compilación y tests siguen viviendo en sus ubicaciones originales** del repo:
- Compilación: `pio run -e zircon_robot1` (arquero) o `pio run -e zircon_robot2` (delantero) — apunta a `src/central/`.
- Tests host-native: `pio test -e native` — apunta a `test/test_strategy_transitions/`, `test/test_central_*`, etc.

## Estructura del pack

```
central-board-pack/
├── README.md                         ← estás aquí
├── 01-pinout-y-hardware.md           ← Zircon Rev v15 + Teensy 4.1 (ROBOT1 + ROBOT2)
├── 02-funcionalidad.md               ← 3 capas (FSM + PIDs + motores) + EMERGENCY
├── 03-contrato-datos.md              ← contrato binario WorldSnapshot v2 (27 B)
├── 04-protocolo-comunicaciones.md    ← diseño general de comm de las 3 placas
├── 05-arquitectura-3-placas.md       ← contexto general TOP/CENTRAL/DOWN
├── firmware/
│   ├── central/                      ← 14 archivos específicos del CENTRAL
│   │   ├── main_central.cpp
│   │   ├── strategy.{h,cpp}          ← FSM viva (cerebro)
│   │   ├── motors_zircon.{h,cpp}     ← driver de los 3 motores
│   │   ├── imu_zircon.{h,cpp}        ← BNO055 wrapper
│   │   ├── world_model.{h,cpp}       ← espejo del WorldSnapshot
│   │   ├── comm_top.{h,cpp}          ← UART ← TOP (Serial1)
│   │   ├── comm_down.{h,cpp}         ← UART ← DOWN (Serial2, emergencia)
│   │   └── config_central.h          ← pinout + constantes (ROBOT1/ROBOT2)
│   └── shared/                       ← 13 archivos compartidos usados por CENTRAL
│       ├── strategy_transitions.{h,cpp}  ← caracterización pura de la FSM (35 tests)
│       ├── behind_ball.{h,cpp}       ← positioning detrás de la pelota (16 tests)
│       ├── pids.{h,cpp}              ← PID heading/lateral/distance (17 tests)
│       ├── kinematics.{h,cpp}        ← cinemática inversa omni-3 (11 tests)
│       ├── motion_target.{h,cpp}     ← targets de movimiento
│       ├── proto.{h,cpp}             ← frame protocol + CRC-16
│       └── types.h                   ← WorldSnapshot v2, MotorCommand, etc.
└── tests/                            ← 7 suites host-native
    ├── test_strategy_transitions.cpp (35 tests)
    ├── test_behind_ball.cpp          (16 tests)
    ├── test_pids.cpp                 (17 tests)
    ├── test_kinematics.cpp           (11 tests)
    ├── test_central_contract.cpp
    ├── test_central_trajectory.cpp
    └── test_central_motion.cpp
```

> **Nota**: NO hay carpeta `ground-truth/` (a diferencia del pack DOWN). La
> placa CENTRAL es el "Zircon Rev v15" del 2025 y el repo no tiene
> schematic/PCB JSON disponibles (solo el `Zircon.pdf` fuera de este pack).
> El pinout vive en `firmware/central/config_central.h` y en el doc histórico
> `hardware/electronics/mapa-pines-teensy-ambos-robots.md`.

## Índice por pregunta

| Pregunta | Doc del pack |
|---|---|
| ¿Qué MCU y placa usa CENTRAL? | `01-pinout-y-hardware.md` §1 (Teensy 4.1 + Zircon Rev v15) |
| ¿Qué pin del Teensy va a qué motor? | `01-pinout-y-hardware.md` §3 (ROBOT1) o §4 (ROBOT2) |
| ¿Cuál es la diferencia de pinout entre arquero y delantero? | `01-pinout-y-hardware.md` §5 |
| ¿Qué pines usa la UART hacia TOP? | `01-pinout-y-hardware.md` §2.1 (Serial1, RX=0, TX=1) |
| ¿Qué pines usa la UART hacia DOWN? | `01-pinout-y-hardware.md` §2.1 (Serial2, RX=7, TX=8 — ⚠️ conflicto pendiente) |
| ¿Qué pines usa el BNO055 y qué dirección I²C? | `01-pinout-y-hardware.md` §2.2 (Wire, SDA=18, SCL=19, 0x28) |
| ¿Cómo es la cinemática del robot? | `01-pinout-y-hardware.md` §6 + `02-funcionalidad.md` §5.1 + `firmware/shared/kinematics.{h,cpp}` |
| ¿Qué hace la placa CENTRAL? | `02-funcionalidad.md` §1, §3 |
| ¿Cuáles son las 3 capas (FSM/PIDs/motores)? | `02-funcionalidad.md` §2 |
| ¿Qué modos de operación tiene? | `02-funcionalidad.md` §4 |
| ¿Cómo es la FSM del DELANTERO? | `02-funcionalidad.md` §7.1 + `firmware/central/strategy.cpp` |
| ¿Cómo es la FSM del ARQUERO? | `02-funcionalidad.md` §7.2 + `firmware/central/strategy.cpp` |
| ¿Cómo funciona behind-the-ball? | `02-funcionalidad.md` §7.3 + `firmware/shared/behind_ball.cpp` |
| ¿Qué PIDs hay y cuándo se activan? | `02-funcionalidad.md` §6 + `firmware/shared/pids.{h,cpp}` |
| ¿Cómo se calcula la cinemática inversa omni-3? | `02-funcionalidad.md` §5.1 + `firmware/shared/kinematics.{h,cpp}` |
| ¿Cómo funciona el modo de EMERGENCIA (línea inminente)? | `02-funcionalidad.md` §8 (latencia ~2 ms) |
| ¿Cuáles son los watchdogs y timeouts? | `02-funcionalidad.md` §10 |
| ¿Cuál es la latencia decisión → motor? | `02-funcionalidad.md` §11.2 (~13 ms) |
| ¿Qué tramas recibe y de quién? | `02-funcionalidad.md` §9 + `03-contrato-datos.md` |
| ¿Cómo está armado el WorldSnapshot v2 (27 B)? | `03-contrato-datos.md` |
| ¿Por qué hay un bus de emergencia separado a DOWN? | `04-protocolo-comunicaciones.md` (latencia <15 ms) |
| ¿Cuál es el rol de CENTRAL en el contexto de 3 placas? | `05-arquitectura-3-placas.md` |
| ¿Cómo se selecciona arquero vs delantero? | `01-pinout` §3/§4 + `02-funcionalidad` §4 (compile-time `#define ROBOT1` / `ROBOT2`) |
| ¿El kicker en qué pin está? | `01-pinout-y-hardware.md` §4 (⚠️ tentativo, pin 23 a confirmar — TASK-011) |

## ⚠️ Pendientes humanos importantes (NO bloquean uso del pack)

**El más urgente — resolver pronto antes de probar hardware:**

1. **Conflicto pines 7/8** — el doc histórico del 2026-03-20 (`mapa-pines-teensy-ambos-robots`)
   asigna pin 7 (INB) y pin 8 (INA) a Motor 2 (ROBOT1) o Motor 1 (ROBOT2). Pero el firmware
   nuevo (`config_central.h` + `comm_down.h`) los usa para **RX2 y TX2 de Serial2** (UART
   hacia DOWN). **No pueden ser ambas cosas a la vez**. Tres opciones:
   - (A) El doc del 2026-03-20 está mal (era del firmware del 2025) → confirmar con Enzo.
   - (B) Cambiar firmware nuevo a OTRO UART (Serial3/4/5 — el Teensy 4.1 tiene 8).
   - (C) El motor que usaba 7/8 ya no se conecta físicamente → documentarlo.

**Otros pendientes (no urgentes):**

2. **Confirmar `PIN_KICKER_SOL`** — ¿es el pin 23? (TASK-011 en `team-tasks/`)
3. **Confirmar cinemática física** — `WHEEL_ANGLES_DEG` y `WHEEL_RADIUS_MM` con regla en el robot armado.
4. **Sensores legacy del Zircon 2025** (8 IR pelota + 3 línea analógicos) están físicamente
   en el PCB pero el firmware nuevo no los lee — confirmar antes de liberar esos pines en una futura rev.
5. **Encoders magnéticos** (AS5600) — opción futura para closed-loop por motor.

Estos pendientes viven en `team-tasks/`, no se duplican en el pack.

## Lo que NO está en este pack (y por qué)

| Categoría | Por qué no está |
|---|---|
| Journals (`journal/`) | Es historia. El pack sólo tiene info útil para programar. |
| TASKs (`team-tasks/`) | Son acciones humanas, no spec técnica. |
| Plans superpowers (`docs/superpowers/`) | Son planes ya ejecutados. Lo ejecutado vive en el código vivo. |
| `Zircon.pdf` | Esquemático binario de 2025, pesado, fuera del scope del pack. Vive en `hardware/electronics/`. |
| `software/libraries/zirconLib/` | Librería del 2025 — el firmware nuevo NO la usa. |
| `software/robot-arquero/` y `software/robot-delantero/` | Código del equipo 2025 (estilo monolítico), reemplazado por arquitectura 3-placas. Vive en `legacy/`. |
| Código de TOP, DOWN, COMM | No es responsabilidad de CENTRAL. Para TOP/DOWN ver sus packs (DOWN ya existe en `down-board-pack/`). |
| Libs Adafruit_BNO055 | Vendoreadas en `lib/` del repo, no copiadas al pack — son sólo para compilar, no tienen valor de doc. |

## Diferencias con el pack DOWN

| Aspecto | DOWN | CENTRAL |
|---|---|---|
| Schematic / PCB JSONs | ✅ Sí (incluidos en `ground-truth/`) | ❌ No disponibles (solo Zircon.pdf binario) |
| Confianza del pinout | Alta (extraído del SCH JSON automáticamente) | Media (de `config_central.h` + doc histórico, con cosas tentativas marcadas) |
| Cantidad de robots cubiertos | 1 (la placa DOWN) | 2 (ROBOT1 arquero + ROBOT2 delantero, mismo Zircon distinto cableado de motores) |
| Capas funcionales | 1 (sensor inteligente puro) | 3 (FSM + PIDs + motores) |
| Tests | 8 suites (138 tests aprox) | 7 suites (79+ tests confirmados) |
| Conflictos abiertos | 0 | 1 (pines 7/8 motores vs Serial2) |

## Atribución

- Fuentes originales del repo (`docs/firmware/`, `hardware/electronics/`, `src/central/`, etc.) — equipo IITA Salta + sesiones Claude previas (ver journals).
- Curado y consolidación del pack — Claude Opus 4.7 (Anthropic), sesión 2026-05-24.
- Requested-by — Gustavo Viollaz (@gviollaz).
