---
date: 2026-06-06
status: diseño / análisis (próximo desarrollo PRIORITARIO)
tipo: diseño-sistema-monitoreo-telemetria
autor: Claude Opus 4.8 (1M) — pedido por Gustavo Viollaz
tareas: [TASK-304, TASK-305, TASK-205, TASK-206]
---

# Sistema de monitoreo / telemetría USB + apps PC de banco

## 1. El problema (lo que pidió el equipo)
Necesitamos **aplicaciones de PC** que permitan **monitorear el funcionamiento de las placas** en banco, de forma **SIMPLE e intuitiva** (no leer texto crudo del Serial Monitor):

- **App de BASE (DOWN) — PRIORITARIA, próximo desarrollo.** Trabaja con un **modo DEBUG + CALIBRACIÓN automática** del firmware. Flujo: poner el robot en la cancha, conectarlo por **USB a la Teensy de la placa base**, y **moviéndolo sobre las líneas** que permita:
  1. diagnosticar que **los 32 sensores de luz funcionan** (ninguno muerto/pegado),
  2. ver **qué está viendo de las líneas** (qué sensores ven blanco, ángulo/profundidad),
  3. ver **qué interpretación está enviando** el software a la CENTRAL y a la TOP (la LineStatusV2 real),
  4. **calibrar** los valores de luz (carpet/blanco/umbral por sensor) de forma asistida.
- **App de PARTE SUPERIOR (TOP).** Monitorear los sensores de arriba (cámaras, IMU, ToF) y el modelo del mundo fusionado que TOP manda a CENTRAL.

Clave: NO es un sketch de diag aparte — es un **MODO DEBUG del firmware de competencia**, que cuando está activo **emite datos por USB** que un programa con **interfaz gráfica** interpreta y muestra. Así se ve exactamente lo que el robot "piensa", no un test paralelo.

## 2. Por qué importa
- Hoy el diagnóstico es por texto en el Serial Monitor (diags `diag_down`, `diag_down_calibracion`, `diag_top_*`): sirve pero es lento, poco visual y propenso a error humano al leer 32 números.
- Una **app gráfica + telemetría estructurada** convierte el banco en algo rápido y a prueba de error: ver el anillo de 32 sensores de un vistazo, la línea detectada como flecha, y la interpretación que viaja a CENTRAL.
- Es el **precursor USB/cableado del roadmap de telemetría "estilo F1"** (TASK E4 / CANbus + ESP32 gateway): misma idea —ver sensores en vivo, registrar, analizar para mejorar el software— pero ya, por USB, sin esperar el hardware del año próximo. Acelera el ciclo de mejora (data-driven).

## 3. Arquitectura propuesta (2 capas + protocolo)

### 3.1 Capa firmware — MODO DEBUG/TELEMETRÍA (gateado, byte-idéntico OFF)
- Un **modo debug dentro del firmware de competencia** (`main_down.cpp` / `main_top.cpp`), **gateado** por flag de build (p.ej. `-DDOWN_DEBUG_TELEMETRY`) y/o activable por **comando por USB** (un byte/línea que enciende el stream). DEFAULT OFF → el binario de competencia queda **byte-idéntico**.
- Cuando está activo, emite **telemetría estructurada por el USB CDC del Teensy** (`Serial`, independiente de los UART inter-placa que ya usa el robot — no interfiere con el WorldSnapshot ni la LINE_URGENT). Tasa objetivo 20–50 Hz (USB CDC sobra para esto).
- **Qué emite la base (DOWN):** lo MISMO que el firmware computa para competencia, no datos inventados:
  - `raw[32]` (cuentas ADC crudas por sensor),
  - `white[32]` (booleano: ve blanco según umbral),
  - calibración vigente: `carpet[32]`, `white_cal[32]`, `threshold[32]`,
  - el dato procesado que VIAJA a CENTRAL: la **LineStatusV2** (line_angle, escape_angle, penetration, cross_track, line_present, sensors, quality, sample_age) — exactamente el frame que arma `dm_update`/`comm_central_send_line_urgent`.
- **Comandos por USB** (host→firmware): iniciar/parar stream, **calibrar carpet**, **calibrar blanco**, **modo auto-calib** (capturar min/máx por sensor mientras se pasa el robot), **guardar calibración a EEPROM**.
- Reusa lo que ya existe: `line_ring` (raw/white/calibrate_carpet/calibrate_white/set_calibration), `calib_storage`/EEPROM (`cs_serialize`, `ec_load_calibration`), y el framing de `proto.cpp` si conviene. NO duplicar la lógica — exponerla.

### 3.2 Capa host — APP PC con GUI (Python, cross-platform, offline)
- **Lenguaje sugerido:** Python (pyserial + una GUI: PyQt/Tkinter/Dear PyGui, o web local). Razón: cross-platform, rápido de iterar, sin compilar, y el equipo ya usa Python para `gen_figuras.py`/`actualizar-cifra.py`.
- **App BASE:** 
  - **Anillo de 32 sensores** dibujado en su geometría real (LUT del PCB DOWN): cada sensor coloreado por valor crudo / resaltado si ve blanco.
  - **Línea detectada** como flecha (ángulo) + profundidad + flag de salida inminente.
  - **Interpretación que viaja a CENTRAL** (LineStatusV2) mostrada en claro (cross_track mm, penetration, line_present).
  - **Diagnóstico de sensores muertos/pegados**: marca en rojo los que NO varían al pasar la línea (auto-detect por falta de rango).
  - **Calibración asistida**: botones carpet/blanco/auto + barra por sensor (min/máx capturado) + guardar a EEPROM. Guía en pantalla ("pasá el robot sobre las líneas").
- **App SUPERIOR (TOP):**
  - Cámaras: pelota/arcos detectados (posición relativa + velocidad), confianza.
  - IMU: heading + validez. ToF: 4 distancias.
  - **WorldSnapshot fusionado** (lo que TOP manda a CENTRAL): cancha con la pelota/arcos ubicados.
- La GUI se puede **desarrollar y testear SIN el robot** usando un stream grabado o un simulador (un archivo de telemetría de ejemplo) → no bloquea por hardware (mismo espíritu que el host-testing del firmware).

### 3.3 Protocolo de telemetría (a definir, simple)
- Framing simple y robusto: o bien líneas CSV/JSON (fácil de parsear y de loguear), o el `Frame` binario de `proto.cpp` con un MsgType nuevo de telemetría. Recomendado para v1: **líneas de texto** (CSV con cabecera o JSON-por-línea) — trivial de parsear en Python, fácil de grabar a archivo para análisis posterior. Migrar a binario solo si la tasa lo exige.
- Versionar el esquema (como los contratos de wire) para que GUI y firmware no se desincronicen.

## 4. Fases / prioridad
1. **FASE 1 (P0 — próximo desarrollo): BASE.** TASK-304 (firmware debug/telemetría + calib en DOWN) + TASK-305 (app PC de base). Es lo PRIORITARIO que pidió el equipo.
2. **FASE 2 (P1): SUPERIOR.** TASK-205 (firmware debug/telemetría en TOP) + TASK-206 (app PC superior).
3. **FASE 3 (futuro, roadmap E4):** migrar el transporte de USB a **CAN troncal + ESP32 gateway inalámbrico** → telemetría estilo F1 sin cable y robot-a-robot.

## 5. Restricciones / disciplina
- El modo debug **gateado OFF por default → binario de competencia byte-idéntico** (regla del proyecto). Verificable: con el flag off, el firmware no cambia.
- El firmware (Arduino) NO se compila en el gate host → la lógica PURA de telemetría (serializar el frame, parsear comandos) puede extraerse a un **módulo puro host-testeable**; el glue de Serial es Arduino (verificar con `pio`).
- La app PC es **host puro** (Python) → cero impacto en el gate de firmware; se versiona en el repo (`tools/` o `pc-apps/`).
- NO reinventar: exponer `line_ring`/`calib_storage`/`proto` existentes.

## 6. Entregables
- `tools/monitor-base/` (app PC base) + `tools/monitor-top/` (app PC superior).
- Modo debug/telemetría en `src/down/` y `src/top/` (gateado) + un `[env:down_debug_telemetry]` / `[env:top_debug_telemetry]`.
- Protocolo documentado (esquema versionado).
- Tareas: TASK-304/305 (base, P0), TASK-205/206 (superior, P1).
