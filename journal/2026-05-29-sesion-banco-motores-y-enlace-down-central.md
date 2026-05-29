---
title: "Sesión de banco 2026-05-29 — motores CENTRAL validados + enlace físico DOWN↔CENTRAL OK (falta protocolo)"
date: 2026-05-29
author: "Claude Opus 4 (Anthropic), vía Claude Code — redacción"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus, Anthropic)"
status: final
tags: [banco, hardware-up, motores, central, down, uart, protocolo, incheon]
robot: ambos
area: control
tipo: hito
---

# Sesión de banco 2026-05-29 — motores + enlace DOWN↔CENTRAL

> **TL;DR.** Dos validaciones de hardware en banco:
> 1. **Motores del CENTRAL andan** — se identificaron motor 1/2/3 y se definió la
>    orientación (horario / antihorario) de cada uno (`diag_central_motors`).
> 2. **El enlace físico DOWN↔CENTRAL funciona** — el test mínimo "mandar un 1"
>    confirma que el cable + UART transmiten. **Lo que falta**: que la
>    comunicación use el **protocolo establecido** (`proto.h` → `LineStatusV2`)
>    para leer los datos decodificados correctamente end-to-end. La herramienta
>    para eso ya existe (`diag_central_comm_down`), falta correr el stream real.

## 1. Motores del CENTRAL (Zircon Rev v15 + Teensy 4.1)

**Qué se probó:** `diag_central_motors` en banco (sketch agnóstico al rol, PWM en
onda controlada por botón pin 9, tope 50%). Cada motor se energiza de a uno.

**Resultado:** ✅ los motores responden. Se identificó qué número de motor del
firmware corresponde a qué rueda física y se definió el sentido de giro de cada
uno (horario / antihorario).

### 1.1 Datos a registrar (de las notas de banco — completar)

> Estos valores los tiene el equipo de las anotaciones del banco. Los dejo
> explícitos para cargarlos al repo (no se inventan). Una vez confirmados,
> actualizar esta tabla + `src/central/config_central.h` (`WHEEL_ANGLES_DEG`,
> y si hace falta invertir un sentido, el signo del PWM por motor).

| Motor firmware | Driver Zircon | Pines (INA/INB/PWM) | Rueda física | Sentido "+" (horario/antihorario) |
|---|---|---|---|---|
| Motor 1 | U5  | 2 / 5 / 3   | _(completar)_ | _(completar)_ |
| Motor 2 | U17 | 8 / 7 / 6   | _(completar)_ | _(completar)_ |
| Motor 3 | U7  | 11 / 12 / 4 | _(completar)_ | _(completar)_ |

*(Pinout para ROBOT1/arquero; en ROBOT2/delantero el mismo driver es otro número
de motor — ver `central-board-pack/01-pinout-y-hardware.md` §5.)*

### 1.2 Veredicto pendiente del conflicto pines 7/8 (CENTRAL) — IMPORTANTE

El motor 2 del firmware usa el **driver U17 en pines 7/8**, que son **también
Serial2** (el UART que el firmware de competencia usa para recibir la línea de
DOWN, `comm_down.cpp`). Por eso `diag_central_motors` sirve de juez:

- **Si el motor de U17 (pines 7/8) GIRÓ** → los pines 7/8 son del motor →
  **hay que migrar el Serial2 de CENTRAL a otro UART** para el link de DOWN
  (candidato: **Serial7**, pines 28/29 — de hecho el receiver de protocolo de
  hoy ya se programó en Serial7, ver §2.2).
- **Si NO giró** → 7/8 son Serial2 y no hay conflicto.

> **A registrar:** ¿giró el motor de U17? _(completar)_. Este es el dato que
> cierra (o no) el conflicto 7/8 del CENTRAL, distinto del de TOP→CENTRAL que ya
> se resolvió hoy (ese pasó a Serial5, ver journal del mismo día).

## 2. Comunicación DOWN → CENTRAL

### 2.1 Enlace físico — VALIDADO ✅ (test mínimo "mandar un 1")

Para aislar el **cable** del **protocolo** (pedido de María), se corrió el par:
- `diag_down_send1` (DOWN, Teensy 4.0): manda el carácter `'1'` por **Serial1
  (TX1 = pin 1)** a 1 Hz.
- `diag_central_recv1` (CENTRAL, Teensy 4.1): recibe por **Serial2 (RX2 = pin 7)**,
  lo imprime y parpadea el LED 13.

Cableado: **DOWN pin 1 (TX1) → CENTRAL pin 7 (RX2) + GND común, 230400 baud.**
El receiver NO llama `motors_init()`, así que **esquiva el conflicto 7/8** (el
conflicto solo aparece al manejar motores) y deja el pin 13 libre como LED.

**Resultado:** ✅ el byte llega. El cable + los UARTs transmiten correctamente.

### 2.2 Lectura por protocolo — FALTA (la herramienta ya existe)

Lo que sigue es que la comunicación se haga **con el protocolo establecido** para
leer los datos decodificados, no bytes crudos. La herramienta ya está hecha:

- `diag_central_comm_down` (CENTRAL, en **Serial7**): receiver de banco que usa
  los decoders reales del repo — `FrameDecoder` de `proto.h` + el struct
  `LineStatusV2` + los helpers de `line_view.h` — y vuelca **todos los campos
  decodificados, campo por campo**, con `N/A` donde el contrato usa sentinel:

  ```
  CAMPOS : schema=2 valid=1 present=1 on_line=4 q=90 age=8ms ev=0x1
           angle=45.00deg escape=N/A pen=15mm cross=-3mm
  ```

**Lo que falta para cerrar la comunicación end-to-end:**
1. Que **DOWN emita frames reales** `LineStatusV2` por el protocolo (encode con
   `proto.h` + CRC16) sobre el enlace ya validado, en vez del `'1'` crudo.
2. Que **CENTRAL los decodifique** con `diag_central_comm_down` y se vea la tabla
   de CAMPOS con datos coherentes (ángulo de línea, penetración, flags) al pasar
   el robot sobre una línea blanca.
3. **Decidir en qué Serial queda el link DOWN→CENTRAL en CENTRAL**: el test
   mínimo usó Serial2 (pin 7), el receiver de protocolo usó Serial7 (28/29). La
   decisión depende del veredicto del conflicto 7/8 (§1.2). El firmware de
   competencia (`comm_down.cpp`) hoy abre **Serial2** — si 7/8 son del motor, hay
   que cambiarlo a Serial7 ahí también.

## 3. Estado de tareas (las cierra el humano que probó, no Claude)

| TASK | Tema | Estado tras hoy |
|---|---|---|
| TASK-036 | Bench de motores (`diag_central_motors`) | Motores andan + mapeo/orientación definidos. **Cargar los valores (§1.1) y el veredicto 7/8 (§1.2) para cerrarla.** |
| TASK-031 | UART real DOWN↔CENTRAL / DOWN↔TOP | **Lado físico DOWN→CENTRAL validado** (§2.1). Falta el stream por protocolo (§2.2). DOWN↔TOP sigue pendiente (TOP no armado). |
| TASK-100 | Validar ingest de línea DOWN→CENTRAL + frenado | Avanzada: link físico OK + receiver de protocolo listo. Falta el end-to-end (§2.2) y el frenado. |

## 4. Próximo paso concreto (firmware de competencia)

Cuando se confirme el veredicto 7/8 y el Serial definitivo del link DOWN→CENTRAL:
- Si migra a Serial7: cambiar `src/central/comm_down.cpp` (`Serial2` → `Serial7`)
  y `src/down/comm_central.cpp` queda igual (DOWN manda por su Serial1). Reusar
  exactamente el path de `diag_central_comm_down` (ya probado).
- Validar el frenado de borde con `imminent_exit` real (TASK-100).

## 5. Atribución

- **Sketches de banco** (`diag_central_motors`, `diag_central_comm_down`,
  `diag_down_send1`, `diag_central_recv1`) — desarrollados por las sesiones de
  los agentes central/down (ver `agente/central`, `agente/down`), pedidos de
  Gustavo y María.
- **Ejecución de las pruebas en banco + identificación de motores, orientaciones
  y validación del enlace** — equipo IITA (Enzo / Elías / Virginia / Gustavo).
- **Redacción de este journal** — Claude Opus 4 (Anthropic), vía Claude Code.

## 6. Referencias

- Motores: `docs/firmware/DIAG-CENTRAL-MOTORS.md` + `src/diag/diag_central_motors.cpp`.
- Receiver de protocolo: `src/diag/diag_central_comm_down.cpp` (rama `agente/central`).
- Test mínimo de enlace: `src/diag/diag_down_send1.cpp` + `diag_central_recv1.cpp` (rama `agente/down`).
- Contrato de línea: `docs/firmware/CONTRATO-DATOS-DOWN.md` (`LineStatusV2`) + `src/shared/line_view.h`.
- Conflicto 7/8 CENTRAL: `central-board-pack/01-pinout-y-hardware.md` §8.
