---
title: "Runbook sesión de prueba 2026-05-29 — banco DOWN + CENTRAL (TOP en construcción)"
date: 2026-05-29
author: "Claude Opus 4 (Anthropic), vía Claude Code"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus, Anthropic)"
status: final
tags: [coach, runbook, sesion-prueba, banco, down, central, prioridades, incheon]
robot: ambos
area: control
tipo: runbook
---

# Runbook sesión de prueba 2026-05-29 — banco DOWN + CENTRAL

> **TL;DR para los 3 (Enzo, Elías, Virginia).** Hoy tienen armadas **DOWN** y
> **CENTRAL**; **TOP está en construcción** (no operativa). Arranquen **2 tracks
> en paralelo e independientes**: **Enzo → placa DOWN** (`diag_down`: anillo +
> OTOS) y **Virginia+Elías → placa CENTRAL** (`diag_central_motors`). El test de
> motores es el **keystone**: además de validar los 3 motores, **resuelve el
> conflicto de pines 7/8** (motor-2 vs Serial2 hacia DOWN). Recién **cuando los
> dos tracks cierran y el veredicto de pines 7/8 dice "Serial2 vivo"** pueden
> juntar las placas y probar el **ingest de línea DOWN→CENTRAL (TASK-100)**.
> **NO pierdan tiempo** con drive-straight / cámara / rol-arco / COMM: todo eso
> necesita TOP o la placa COMM, que hoy no existen operativas.

---

## 0. Reality check (hardware HOY)

| Placa | Estado físico | Qué se puede probar hoy |
|---|---|---|
| **DOWN** (Teensy 4.0) | ✅ armada, validada 05-24 | anillo 32 sensores línea + 2 OTOS odometría |
| **CENTRAL** (Teensy 4.1, Zircon v15) | ✅ armada | 3 motores omni + recepción UART de DOWN |
| **TOP** (Teensy 4.0) | 🔨 **en construcción** | nada en banco hoy (cámaras/IMU/ToF/localización dependen de TOP) |
| **COMM** (ESP32-C6) | ❌ no construida | nada (START/STOP árbitro) |

**Cableado UART relevante** (cada placa nombra su propio puerto):
- CENTRAL `Serial1` (pines 0/1) ← WorldSnapshot de **TOP** *(no hay TOP hoy)*
- CENTRAL `Serial2` (**pines 7/8**) ← `LineStatusV2` de **DOWN** *(= Serial1 del lado DOWN)*
- ⚠️ **Pines 7/8 de CENTRAL los comparte el driver de Motor 2 (U17).** Hay que
  resolver empíricamente si son motor o Serial2 → eso lo decide `diag_central_motors`.

---

## 1. El runbook — qué es independiente y qué depende

### 🟢 INDEPENDIENTE — arrancan YA, en paralelo (2 tracks)

#### TRACK A — placa DOWN  ·  dueño: **Enzo** (apoyo Elías para OTOS/mecánica)
Todo standalone: solo la placa DOWN + USB. No necesita ninguna otra placa.

- **A1 · `diag_down` — anillo de línea + OTOS.**
  - Flashear `[env:diag_down]` y abrir el monitor serie.
  - **PASA si:** el I²C scan ve los **2 OTOS en 0x17** + los 4 mux; los **32
    sensores responden** (0 muertos en el sweep); la **pose OTOS cambia** al
    mover la placa a mano. *(Esto recapitula el hito 05-24; sirve de smoke-test
    de que DOWN sigue sana antes de juntarla con CENTRAL.)*
- **A2 · (opcional, 15 min) TASK-026 — multímetro en el mux.** Ya está validado
  empíricamente (05-24); es solo el **cierre formal con multímetro** si Enzo lo
  tiene a mano. No bloquea nada.
- **A3 · TASK-301 criterio A — calib persiste al power-cycle.** Calibrar la
  línea (paso blanco/negro), **apagar y encender** la placa, confirmar que la
  calib **sobrevive** (no re-calibra de cero). Standalone DOWN.

#### TRACK B — placa CENTRAL  ·  dueño: **Virginia + Elías**
Standalone: placa CENTRAL + 3 motores en banco + batería. No necesita DOWN ni TOP.

- **B1 · ⭐ TASK-036 · `diag_central_motors` — KEYSTONE.**
  - Flashear `[env:diag_central_motors]`. El **botón (pin 9)** dispara una onda
    PWM (limitada al 50%) a un motor por vez.
  - **PASA si:** los **3 motores giran** al pulsar; queda documentado el mapa
    **"motor N del firmware → rueda física"**.
  - 🔑 **VEREDICTO PINES 7/8 (lo más importante de la sesión):**
    - Si el **motor 2 (driver U17) NO gira** → pines 7/8 son **Serial2** →
      **el link DOWN→CENTRAL está vivo**, sigan a TASK-100.
    - Si el **motor 2 SÍ gira** → pines 7/8 son **motor** → hay que **migrar
      Serial2 a Serial7 (pines 28/29 libres)** antes de poder recibir la línea.
      En ese caso **TASK-100 queda bloqueada hoy** (es cambio de firmware + recableado).
  - **Este veredicto gatea TASK-100.** Por eso B1 va antes que cualquier prueba
    de línea sobre CENTRAL.

> Tracks A y B **no dependen entre sí** → córranlos **al mismo tiempo**.

### 🔴 DEPENDIENTE — solo después de A1 + B1 (y veredicto "Serial2 vivo")

#### CONVERGENCIA — DOWN + CENTRAL juntas  ·  Virginia lee debug de CENTRAL, Enzo/Elías mueven la placa sobre la línea

- **C1 · TASK-100 — ingest de línea DOWN→CENTRAL + frenado.**
  - **Pre-requisitos:** A1 OK (DOWN sana) **y** B1 con veredicto **"pines 7/8 =
    Serial2"**. Si el veredicto fue "motor", **NO se puede** hasta migrar el UART.
  - Conectar DOWN `Serial1` ↔ CENTRAL `Serial2`. Flashear CENTRAL normal (no diag).
  - **PASA si:** CENTRAL incrementa `frames_received` a ~100 Hz, `crc_errors ≈ 0`;
    al **cruzar la placa sobre la línea**, `imminent_exit` dispara y (si los
    motores están conectados) **frena**.
- **C2 · TASK-301 criterio C — backpressure bajo carga.** Con el link C1 vivo,
  confirmar `frames_dropped ≈ 0` con el uplink corriendo (no se degrada el loop
  de 1 kHz del anillo). Va **después** de C1.
- **C3 · TASK-301 criterio B — rechazo "todo blanco".** Necesita un **rig de luz
  fuerte** (saturación). Si lo tienen: bajo luz extrema, `line_present = 0` +
  flag `EV_CALIB_SUSPECT`. Independiente del link; háganlo si sobra tiempo y hay rig.

### ⛔ BLOQUEADO HOY — necesita TOP / COMM (no abrir en esta sesión)

| Tarea | Por qué no hoy |
|---|---|
| **TASK-037** drive-straight closed-loop | necesita **TOP** mandando WorldSnapshot (heading) por Serial1. La dirección de giro de los motores ya la cubre TASK-036 hoy. |
| **TASK-200** (heading IMU + loop sin stall) | valida fixes de firmware **en la placa TOP** |
| **TASK-022** (cámara operativa) | cámaras viven en **TOP** |
| **TASK-024** (rol + polaridad de arco) | necesita **TOP** (dip/rol) + **COMM** (START árbitro) |
| **TASK-038** (pines XSHUT bodge) | **TOP** + decisión TASK-033 pendiente |
| **TASK-006** (flashear COMM) | placa **COMM no construida** |

---

## 2. Orden recomendado para las 2 horas

1. **t0 (paralelo):** Enzo → **A1 `diag_down`**; Virginia+Elías → **B1
   `diag_central_motors`**. *(Las dos placas a la vez; ~30–45 min cada una.)*
2. **t0+45:** anotar el **veredicto pines 7/8** y el **mapa motor→rueda**.
   Enzo cierra **A3** (calib power-cycle) en paralelo.
3. **Si veredicto = "Serial2 vivo":** juntar placas y correr **C1 (TASK-100)**
   ingest+frenado (~45 min) → si va, **C2** backpressure (~15 min).
4. **Si sobra tiempo y hay rig de luz:** **C3** all-white.
5. **Si veredicto = "motor":** **no** se prueba línea hoy; documentar y dejar
   TASK-100 bloqueada por "migrar Serial2 a Serial7". Usar el tiempo restante
   en cerrar A3 + A2 + repetir el mapa de motores con más cuidado.

---

## 3. Análisis estratégico — hecho vs. falta (a 32 días de Incheon)

**Hecho / hardware-up:**
- **DOWN operacional**: anillo 32 sensores (05-24) + 2 OTOS validados
  *cuantitativamente* (280/300 mm = 6.5% error, dentro de tolerancia).
- **CENTRAL firmware vivo**: FSM ATK+GK (Nivel 2) + 3 PIDs + cinemática omni-3 +
  motores; sketches de banco (`diag_central_motors`, `diag_central_drive`) listos.
- **Pipeline de línea DOWN→CENTRAL mergeado** (contrato `LineStatusV2` 16 B) +
  **262 tests host verdes / 20 envs** (`pio test -e test_native`).
- **TOP firmware** adelantado (HAL Sprint A, localización Sprint 1 trilateración,
  ToF frontal vivo) — pero **falta la placa física**.

**Falta — la tensión real:** lo que decide "¿compite o no?" está **todo del lado
de TOP/COMM**, que es justo lo que no está armado:
- **TOP hardware** = ve la pelota (cámara), heading (IMU), distancias (ToF),
  localización. **Es el cuello de botella #1.**
- **COMM flasheada** = START/STOP de árbitro = **homologación**. Bloqueante #2.
- **Rol + polaridad de arco** (TASK-024) = riesgo de autogol.

---

## 4. Prioridades para "mañana" (post-sesión)

### 🔴 P0 — destraban "¿compite o no?"
- **Terminar la placa TOP.** Es el desbloqueo de mayor palanca: habilita cámara,
  heading real al CENTRAL, ToF, localización, y las TASK-200/037/024. Sin TOP el
  robot no ve ni se orienta.
- **Construir + flashear COMM (TASK-006).** Sin START/STOP no homologa.

### 🟠 P1 — cerrar lo que deje la sesión de hoy
- Si el **veredicto pines 7/8 = "motor"** → **migrar Serial2 a Serial7 (28/29)**
  en firmware CENTRAL + recablear, y recién ahí TASK-100.
- Cerrar lo que quede a medias de **TASK-100 / TASK-301** (línea + robustez DOWN).

### 🟡 P2 / decisión de coach
- **TASK-033** (2 vs 4 ToFs para Incheon) — *Gustavo*. Destraba TOP rev / Sprint B.
  Recomendación de coach registrada: **2 ToFs sin rework** para Incheon.

---

## 5. Documentación — inconsistencias

**Corregido en esta sesión (este commit):**
- `docs/ESTADO-ACTUAL.md` — tabla de tests host reconciliada con la realidad
  post-merge de los 3 agentes: total **246/19 → 262/20**, fila nueva
  `test_central_line_ingest` (8), `test_down_model` 5 → 7. Número verificado con
  `pio test -e test_native`.

**Flag — NO tocado (estado humano-propietario / contradicción intencional):**
- `team-tasks/README.md` — encabezado "al 2026-05-15", le faltan ~14 task files
  (027–032, 035–038) y la sección "TASKs activas (al 2026-05-19)" de
  ESTADO-ACTUAL replica esa lista vieja. **Reconciliar es trabajo del humano que
  conoce el estado real de cada TASK** (no lo invento).
- Posible desfase **TASK-034** (arquitectura de localización): ESTADO-ACTUAL la
  da como decidida (Sprint 1 trilateración aprobado 05-25) pero `team-tasks/README.md`
  la lista `pending`. Confirmar con Gustavo y cerrar la TASK si corresponde.
- Docs marcados superados en `FUENTES-DE-VERDAD.md` (WorldSnapshot 24 B vs v2 27 B,
  ejes invertidos, SEEK/DRIVE vs SEARCH/POSITION): contradicciones **intencionales**,
  no homogeneizar.

---

## 6. Atribución
- **Análisis estratégico, runbook de prueba (orden + dependencias + gating de
  hardware), prioridades y reconciliación de la tabla de tests** — Claude Opus 4
  (Anthropic), vía Claude Code, sesión 2026-05-29.
- **Encuadre de coach, realidad de hardware (DOWN+CENTRAL armadas, TOP en
  construcción) y dirección estratégica** — Gustavo Viollaz (@gviollaz).

## 7. Referencias
- Estado vivo: `docs/ESTADO-ACTUAL.md`
- Canónico por tema: `docs/FUENTES-DE-VERDAD.md`
- Diag motores: `docs/firmware/DIAG-CENTRAL-MOTORS.md` + `src/diag/diag_central_motors.cpp`
- Diag drive: `docs/firmware/DIAG-CENTRAL-DRIVE.md` (necesita TOP)
- Hito DOWN: `journal/2026-05-24-hardware-up-down-anillo-linea.md`
- Audit DOWN (origen P0.2/P1.5/P1.6 → TASK-301): `research/in-progress/2026-05-29-auditoria-exhaustiva-placa-down.md`
- Conflicto pines 7/8: ESTADO-ACTUAL §"Avance 2026-05-28 — diag_central_motors"
