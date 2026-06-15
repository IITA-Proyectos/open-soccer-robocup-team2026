---
title: "Handoff — continuar la integración del análisis RT de las 3 placas"
date: 2026-06-15
author: "Claude (Anthropic - Claude Opus 4.8 1M) — coach"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: vivo
tipo: handoff
area: control
---

# Handoff — integración del análisis RT de las 3 placas

> **Para qué es este archivo.** La sesión que diseñó la arquitectura de tiempo real
> de las 3 placas se quedó sin contexto. Esto es el **punto de continuación**: pegalo
> (o leelo) al iniciar una sesión nueva para retomar el **desarrollo y la
> programación** sin re-derivar nada. El diseño está LISTO; falta la integración
> (cablear, gateado, host-testeable), que es trabajo post-Incheon.

---

## Contexto (qué hay hecho — ya está en `main`)

- **5 documentos de diseño** en `docs/firmware/`:
  - `ARQUITECTURA-SENSORIAL-TOP-NO-BLOQUEANTE.md` (TOP: pizarra + RX-IRQ + emisor por timer)
  - `ESTIMACION-FUSION-TOP.md` (TOP: capa estimador, 24 mejoras priorizadas)
  - `ARQUITECTURA-LAZO-CENTRAL-RT.md` (CENTRAL: 4 capas + FSM prolija)
  - `GUIA-DE-TUNING-CENTRAL.md` (CENTRAL: todos los parámetros tuneables, verificado 100 %)
  - `ARQUITECTURA-LAZO-DOWN-RT.md` (DOWN: dual-ADC + detección temprana + tolerancia a fallas)
- **5 módulos PUROS host-testeados, GATEADOS off-by-default, SIN cablear al firmware:**
  `src/shared/{sensor_slot, snapshot_assembler, state_timer, motor_slew, line_early_escape}.h` + sus tests.
- Journal: `journal/2026-06-15-arquitectura-rt-3-placas.md`. Gate host al día: **937 tests / 67 envs / 0 fallos**.
- **EL ROBOT HOY SIGUE CORRIENDO EL FIRMWARE ANTERIOR.** Todo lo nuevo está apagado.

## Tu misión

Integrar (cablear) los diseños **detrás de flags off-by-default**, host-testeado, **SIN
cambiar el binario de competencia** (byte-idéntico). Es trabajo **post-Incheon**; no
arriesgar el robot que funciona.

## Antes de empezar (protocolo del repo — obligatorio)

1. `git pull`
2. Leé `docs/ESTADO-ACTUAL.md` y `docs/FUENTES-DE-VERDAD.md`
3. Leé el doc de diseño de la placa que vayas a tocar

## Reglas duras (no negociables)

- TODO gateado **off-by-default** → el binario de competencia NO cambia hasta validar en banco.
- **NO reescribir** `strategy.cpp` / `main_central.cpp` / `main_top.cpp` / `main_down.cpp` vivos. Cablear módulos puros detrás de flags.
- **NO cerrar TASKs de hardware** como "done": eso lo hace el equipo con la placa. Vos programás host-testeable.
- Mecanismo ya decidido: **interrupciones + DMA + doble-buffer. NO RTOS, NO threads.**
- **Gate host** (no hay pio/g++ en PATH; usar el de Webots):
  `cd "software/teensy/Soccer 2026" && export PATH="/c/Program Files/Webots/msys64/mingw64/bin:$PATH" && bash scripts/run-host-tests.sh`
- Commitear en **rama feature + PR a main**. NO push directo a main salvo que Gustavo lo pida.
- Doc/módulo nuevo → fila en `FUENTES-DE-VERDAD.md` + journal en el mismo commit.

## Orden de trabajo (de menor a mayor riesgo)

### A) Quick-wins (firmware vivo, chicos, gateados — empezar acá)
1. **CENTRAL:** gatear el bloque de debug `Serial.print` (`main_central.cpp:341`) tras `-DCENTRAL_DEBUG_SERIAL` (default OFF) → saca un pico de jitter cada 500 ms.
2. **CENTRAL:** `Serial7.addMemoryForRead(buf,512)` en `comm_top_init` (hoy 64 B → el snapshot del TOP se pierde en silencio si una vuelta se alarga).

### B) TOP estimación (alto valor / bajo esfuerzo: cablear lo que YA existe)
3. Cablear, cada uno tras su flag default-OFF: `ball_sticky` (mata la pelota fantasma), `imu_freeze`, y `pose_fusion`+`pose_filter`.
   - **PRE-REQUISITO DURO:** el HEADING es la raíz. `pose_fusion` **NO debe compilar sin `imu_freeze`** activado (poné un `#error`). Un rumbo malo rota TODO el mapa.

### C) Integrar los ladrillos puros (placa por placa, tras flag, con gate verde)
4. **CENTRAL:** `state_timer.h` + `motor_slew.h` + `sensor_slot.h` (la pizarra).
5. **TOP:** `sensor_slot.h` + `snapshot_assembler.h`.
6. **DOWN:** `line_early_escape.h` + el ADC rápido (dual-ADC).

### D) Banco (lo hace el equipo, no vos)
- Medir **ruido de sensores + WCET + latencias ANTES** de tunear cualquier ganancia.

## Trampas que el review adversarial ya encontró (no repetirlas)

- **Pizarra seqlock:** la barrera debe ser `__DMB()` con clobber `:::"memory"`, NO el `dsb` pelado del watchdog. El seqlock es correcto por **single-writer + lector wait-free**, NO por "una ISR nunca interrumpe a otra" (FALSO en el Cortex-M7 por preempción anidada del NVIC).
- **α-β de pelota:** DEBE heredar la expiración de `ball_velocity` (200 ms) o vuelve la pelota fantasma volando recta.
- **CENTRAL:** los **reflejos** (STOP del árbitro, escape de línea) **NO** pasan por el slew/rampa de motores (si no, el "suave" demora la salida rápida).
- **DOWN:** NO meter `delayMicroseconds` dentro de una ISR; el OTOS bloqueante roba ~0.6 ms al barrido de línea (el IntervalTimer lo resuelve, es P2).
- El detalle por cantidad/mejora está en cada doc (todos tienen su review adversarial + backlog priorizado).

## Método por cada cambio

Leer el doc → flag off-by-default → módulo puro + test host primero → glue Arduino
después → correr el gate host (0 regresiones) → commit en rama + PR → journal.
**Mejora corta y bien documentada > ambiciosa y opaca.**
