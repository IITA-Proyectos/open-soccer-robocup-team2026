---
title: "Pack autocontenido — placa TOP del robot Soccer 2026"
date: 2026-05-24
status: vigente
audiencia: "IA + humanos que programen la placa TOP"
fuente-canonica: "docs/FUENTES-DE-VERDAD.md"
---

# Pack autocontenido — placa TOP

## Para qué existe este directorio

Todo lo que hace falta saber para **programar, testear y diagnosticar** la
placa TOP del robot Soccer 2026, **en un solo lugar**.

TOP es el **cerebro sensorial**: 1 Teensy 4.0 master + 2 cámaras OpenMV + 2
BNO055 (IMU dual) + 4 ToF multizona (todavía stub) + 1 HC-SR04 + 4 UARTs
activos. Recibe odometría del DOWN, comandos del árbitro (vía COMM ESP32-C6),
fusiona todo con visión, y entrega un único `WorldSnapshot` al CENTRAL @ 100 Hz
para que decida la jugada.

## Regla de oro

> Si lo que está en este pack **contradice** algo del repo vivo
> (`software/teensy/Soccer 2026/src/top/...`), **gana el repo vivo**.
> Este pack es una **foto curada del 2026-05-24** — los `.cpp/.h` y los tests
> son copias snapshot, NO son la fuente compilable.

**Compilación y tests siguen viviendo en sus ubicaciones originales** del repo:
- Compilación: `pio run -e top` — apunta a `src/top/`.
- Tests host-native: `pio test -e native` — apunta a `test/test_cameras_fusion/` y `test/test_proto/`.

## Estructura del pack

```
top-board-pack/
├── README.md                         ← estás aquí
├── 01-pinout-y-hardware.md           ← Teensy 4.0 master + I²C dual + 5 UARTs
├── 02-funcionalidad.md               ← cerebro sensorial: IMU + cámaras + ToF + fusión
├── 03-contrato-datos-top.md          ← contrato binario WorldSnapshot v2 (27 B)
├── 04-contrato-datos-camaras.md      ← protocolo de las cámaras OpenMV (9 bytes/packet)
├── 05-protocolo-comunicaciones.md    ← diseño general de comm de las 3 placas
├── 06-arquitectura-3-placas.md       ← contexto general TOP/CENTRAL/DOWN
├── firmware/
│   ├── top/                          ← 16 archivos específicos del TOP
│   │   ├── main_top.cpp
│   │   ├── cameras.{h,cpp}           ← parser OpenMV (9 bytes/packet)
│   │   ├── cameras_runtime.{h,cpp}   ← wiring sobre Serial3 + Serial5
│   │   ├── sensors_imu.{h,cpp}       ← BNO055 dual con consistencia
│   │   ├── sensors_tof.{h,cpp}       ← HC-SR04 vivo + VL53 STUB
│   │   ├── comm_down.{h,cpp}         ← UART ← DOWN (Serial1)
│   │   ├── comm_central.{h,cpp}      ← UART → CENTRAL (Serial2)
│   │   ├── comm_arbiter.{h,cpp}      ← UART ↔ COMM ESP32-C6 (Serial4)
│   │   └── config_top.h
│   └── shared/                       ← 5 archivos shared usados por TOP
│       ├── cameras_fusion.{h,cpp}    ← fusión front+back, watchdog (16 tests)
│       ├── proto.{h,cpp}             ← frame protocol + CRC-16
│       └── types.h                   ← WorldSnapshot v2, etc.
├── tests/                            ← 2 suites host-native que cubren TOP
│   ├── test_cameras_fusion.cpp       (16 tests)
│   └── test_proto.cpp                (13 tests)
└── ground-truth/                     ← fuentes EasyEDA crudas (NO editar manualmente)
    ├── SCH_Roboliga2026_TOP_2026-04-12.json
    ├── PCB_PCB_Roboliga2026_TOP_2026-04-12.json
    ├── Schematic_Roboliga2026_TOP_2026-04-12.pdf
    └── BOM_Roboliga2026_TOP_2026-04-12.csv
```

## Índice por pregunta

| Pregunta | Doc del pack |
|---|---|
| ¿Qué MCU y placa usa TOP? | `01-pinout-y-hardware.md` §1 (Teensy 4.0 master + PCB custom TOP) |
| ¿Qué pines usa la UART hacia CENTRAL? | `01-pinout-y-hardware.md` §2.2 (Serial2, RX=7, TX=8 — ⚠️ tentativo + conflicto con HC-SR04 ECHO) |
| ¿Qué pines usa la UART hacia DOWN? | `01-pinout-y-hardware.md` §2.2 (Serial1, RX=0, TX=1) |
| ¿Qué pines usa la UART hacia COMM (ESP32-C6)? | `01-pinout-y-hardware.md` §2.2 (Serial4, RX=16, TX=17) |
| ¿Qué pines usan las cámaras OpenMV? | `01-pinout-y-hardware.md` §2.2 (cam 1: Serial3 RX=15 TX=14; cam 2: Serial5 RX=21 TX=20) |
| ¿Qué pines I²C usa cada BNO055? | `01-pinout-y-hardware.md` §2.1 (BNO055 izq: Wire 18/19; BNO055 der: Wire1 25/24 ⚠️ remap) |
| ¿Cuál es la dirección I²C de cada BNO055? | `01-pinout-y-hardware.md` §2.1 (ambos 0x28 — por eso buses separados) |
| ¿Qué pines XSHUT tienen los 4 ToF? | `01-pinout-y-hardware.md` §2.3 (pines 2, 3, 4, 5 — tentativos) |
| ¿Qué hace la placa TOP? | `02-funcionalidad.md` §1 |
| ¿Qué subsistemas están vivos y cuáles son stub/futuro? | `02-funcionalidad.md` §2 (tabla "vivo vs aspiracional") |
| ¿Cómo funciona el IMU dual? | `02-funcionalidad.md` §5 + `firmware/top/sensors_imu.cpp` |
| ¿Cómo funcionan las cámaras OpenMV? | `02-funcionalidad.md` §6 + `firmware/top/cameras.cpp` |
| ¿Cómo se fusionan las 2 cámaras (front + back)? | `02-funcionalidad.md` §6.2 + `firmware/shared/cameras_fusion.cpp` |
| ¿Por qué los ToF están en stub? | `02-funcionalidad.md` §7 (es Nivel 3+, no bloquea Incheon) |
| ¿Cómo se construye el WorldSnapshot? | `02-funcionalidad.md` §8 + `03-contrato-datos-top.md` |
| ¿Qué hace la placa COMM y cómo le hablamos? | `02-funcionalidad.md` §9 + `firmware/top/comm_arbiter.cpp` |
| ¿Cuáles son las latencias del loop? | `02-funcionalidad.md` §11 |
| ¿Qué watchdogs hay? | `02-funcionalidad.md` §10.2 |
| ¿Cómo regenero el pinout desde el schematic? | `01-pinout-y-hardware.md` §5 (apuntar `extract_pinout_from_schematic.py` a `ground-truth/`) |

## ⚠️ Pendientes humanos importantes (NO bloquean uso del pack)

**Los más urgentes — resolver antes de probar hardware:**

1. **Conflicto pin 7 (HC-SR04 ECHO vs Serial2 RX2)** — `config_top.h` línea 39
   asigna pin 7 a RX2 de Serial2 (UART hacia CENTRAL); línea 74 asigna el
   mismo pin 7 a HC-SR04 ECHO. **No pueden ser ambas cosas a la vez**.
   Opciones:
   - (A) Mover HC-SR04 a otro pin libre (9, 11, 12, 22, 23, 26–33).
   - (B) Mover Serial2 a otro UART (Serial7 está libre en pines 28/29).
   - (C) Si el HC-SR04 nunca se conectó físicamente, dejarlo así y documentarlo.

2. **Confirmar Wire1 remap a pines 24/25** (TASK-003) — si Enzo no lo
   confirma con multímetro, el bus I²C #1 no funciona → 1 BNO055 + 2 ToF
   muertos.

3. **Confirmar conector U1 → CENTRAL** (Serial2 RX_OUT/TX_OUT a pines 7/8) —
   el comentario del config_top.h dice "TENTATIVO". Si U1 va a otros pines,
   CENTRAL no recibe WORLD_SNAPSHOT y el robot no juega.

**Otros pendientes (no urgentes):**

4. **Recuperar BOM y Pick&Place del proyecto EasyEDA TOP** (TASK-013) — el
   paquete de fabricación solo tenía gerbers. Sin BOM no hay trazabilidad
   de componentes para repuestos en Incheon.
5. **Activar ToF VL53** — Nivel 3+. Vendorear lib SparkFun ST/Pololu y
   reemplazar el stub de `sensors_tof.cpp`. No bloquea Incheon.
6. **Llenar `ball_vx/vy` del WorldSnapshot v2** — los campos están definidos
   en `types.h` (con `static_assert` activo), pero `cameras_runtime` no los
   calcula → quedan en 0. CENTRAL no puede intercep tar por velocidad de
   pelota todavía. P2.
7. **Calibración LAB de las cámaras para iluminación Incheon** (TASK-022) —
   workflow en skill `openmv-vision-tuning` del repo.

Estos pendientes viven en `team-tasks/` y `journal/`, no se duplican al pack.

## Lo que NO está en este pack (y por qué)

| Categoría | Por qué no está |
|---|---|
| Journals (`journal/`) | Es historia. El pack sólo tiene info útil para programar. |
| TASKs (`team-tasks/`) | Son acciones humanas, no spec técnica. |
| Plans superpowers (`docs/superpowers/`) | Son planes ya ejecutados. Lo ejecutado vive en el código vivo. |
| `mapa-pines-placas-nuevas.md` | Superado por `01-pinout-y-hardware.md` del pack. |
| `2026-05-17-placa-top-analisis-gerbers.md` | Análisis histórico del 17-may con el gap de BOM detectado. La info útil ya está integrada al `01-pinout-y-hardware.md` (datos del PCB). |
| Código de cámaras OpenMV (Python, side del MV) | Vive en otro repo del equipo (firmware del OpenMV). Ver skill `openmv-vision-tuning`. |
| Código de la placa COMM (ESP32-C6, Arduino) | Vive en repo oficial RCJ `soccer-communication-module`. Ver `hardware/electronics/comm-board/`. |
| Código de TOP en sus formas legacy (2025) | Vive en `legacy/2025-season/` — no aplica al firmware nuevo. |
| `BackupProjects_*.zip` | Backup binario del proyecto EasyEDA — respaldo, no fuente de verdad. |

## Diferencias con los packs DOWN y CENTRAL

| Aspecto | DOWN | CENTRAL | TOP |
|---|---|---|---|
| Schematic / PCB JSONs | ✅ Sí | ❌ No (sólo Zircon.pdf binario) | ✅ Sí |
| BOM disponible | ✅ Sí | ❌ No | ⚠️ Parcial (falta Pick&Place — TASK-013) |
| Confianza pinout | Alta (extraído del SCH JSON automáticamente) | Media (de `config_central.h` con tentativos) | Media (de `config_top.h` con tentativos + Q3 a confirmar) |
| Robots cubiertos | 1 | 2 (ROBOT1 + ROBOT2 con pinout distinto) | 1 (rol se decide por dipswitch al boot) |
| Subsistemas vivos | Anillo línea + OTOS (lib pendiente) | FSM + PIDs + motores | IMU + cámaras + HC-SR04 + UARTs |
| Subsistemas STUB / futuro | OTOS lib (TASK-012) | Encoders (futuro) | **ToF VL53** + EKF + Kalman pelota + partner ESP-NOW |
| Tests | 8 suites (~138 tests) | 7 suites (79+ tests) | 2 suites (29 tests) |
| Conflictos abiertos | 0 | 1 (pines 7/8 motores vs Serial2) | 2 (pin 7 HC-SR04 vs Serial2; Wire1 remap Q3) |

## Cobertura completa: las 3 placas

Con este pack TOP ya están armados los 3 packs autocontenidos del robot:

| Pack | Path | Tamaño aprox |
|---|---|---|
| ✅ **DOWN** | `hardware/electronics/down-board-pack/` | 47 archivos, 2.4 MB |
| ✅ **CENTRAL** | `hardware/electronics/central-board-pack/` | 40 archivos, 362 KB |
| ✅ **TOP** | `hardware/electronics/top-board-pack/` | ~36 archivos, ~2 MB |

La placa **COMM** (ESP32-C6 separada) ya tiene su mini-pack en
`hardware/electronics/comm-board/` con procedimiento de flash y descripción
de componentes. No necesita ser empaquetada de la misma forma porque el
firmware vive en el repo oficial RCJ, no en este.

## Atribución

- Fuentes originales del repo (`docs/firmware/`, `hardware/electronics/`, `src/top/`, etc.) — equipo IITA Salta + sesiones Claude previas (ver journals).
- Curado y consolidación del pack — Claude Opus 4.7 (Anthropic), sesión 2026-05-24.
- Requested-by — Gustavo Viollaz (@gviollaz).
