---
title: "Integración RT del TOP — seqlock endurecido + frescura por-sensor + env RT (etapas 1/2/3/4)"
date: 2026-06-16
author: "Claude (Anthropic — Opus 4.8 1M), sesión con Gustavo"
status: vivo
tipo: journal-implementación
rama: feat/top-rt-wiring
relacionada: IMPL-PIZARRA-Y-EMISOR-TOP-2026-06-16.md, ARQUITECTURA-SENSORIAL-TOP-NO-BLOQUEANTE.md, HANDOFF-INTEGRACION-RT.md, CONTRATO-DATOS-TOP.md
---

# Integración RT del TOP — cableado de los faltantes (etapas 1/2/3/4)

## Por qué (pedido de Gustavo)

Síntoma real en cancha: **el robot oscila sin control al usar el lazo de rumbo (giróscopo).**
Hipótesis de Gustavo: demoras entre lectura del BNO y el reporte a CENTRAL, y/o demoras en
el lazo de CENTRAL. Pedido: terminar de programar la arquitectura sensorial no-bloqueante
del TOP para que funcione **mucho más rápido y confiable, SIN cambiar contratos de datos**.

Antes hubo una **auditoría RTOS independiente** (workflow multi-agente, 95 agentes) que
encontró, entre otros: (D1-1) el lector del seqlock corría SIN cota dentro de la ISR del
emisor → deadlock + reset por WDT; (D1-2) seqlock sin `volatile`/barreras → torn-read bajo
-O2; (D-FS-2) heading congelado-pero-válido se mandaba con `heading_valid=1`; y que los
módulos no-bloqueantes nuevos estaban **escritos pero sin cablear** (huérfanos).

## Qué se hizo (todo en rama `feat/top-rt-wiring`, gateado, contrato intacto)

### Etapa 1 — Endurecer el seqlock (commit `aff4cce`)
- `sensor_slot.h`: `seq` → `volatile` + barreras `IITA_SEQLOCK_FENCE()` (`dmb 0xF` con
  clobber `:::"memory"` en Cortex-M7, barrera de compilador en host) en `slot_publish` y en
  ambos readers. Receta exacta del review adversarial (HANDOFF). Cierra D1-2.
- `snapshot_from_slots.h`: `inputs_from_slots()` (corre EN LA ISR) pasa de `slot_read_latest`
  (`for(;;)` sin cota) a `slot_read_latest_capped` con fail-safe a sentinela. Cierra D1-1.
- Test nuevo: slot con `seq` impar atascado → NO cuelga + sale no-fresco (antes colgaba).

### Etapa 2 — Frescura POR-SENSOR (read_stamp) en el emisor (commit `eab526d`)
- `publish_decision.h` (nuevo, puro): decide `alive_now` por slot desde las señales que cada
  fuente ya expone (heading/pose por `valid_src`; ball/goal por cámara-viva AND visible;
  obstáculo always-alive porque su liveness vive en el VALOR).
- `snapshot_emitter.cpp`: `snapshot_emitter_publish()` estampa cada slot con
  `read_stamp_tick(alive_now)` en vez de `now` plano → un sensor congelado-pero-vivo (BNO
  clavado) envejece su slot → colapsa a sentinela (heading limpia bit4). Cierra D-FS-2. (El
  módulo puro `read_stamp.h` ya estaba en `272c7ec`.)
- Test nuevo (7): end-to-end del heading congelado que envejece, contrastado con el bug de
  Fase 1 (`now` plano = fresco para siempre).

### Etapa 4 — Env RT paraguas + test de paridad de contrato (commit `917c2ba`)
- `platformio.ini`: env `top_robot2_pri_rt` (BANCO) = `top_robot2_pri` +
  `-DTOP_ENABLE_SNAPSHOT_TIMER` (emisor seguro + frescura por-sensor) +
  `-DTOP_BNO_FAST` (BNO primario @100 Hz → latencia de rumbo ~50 ms → ~10 ms; el fix #1
  de la oscilación). `top_robot2_pri` queda como fallback byte-idéntico.
- `test_contract_parity` (nuevo, 5): prueba BYTE-A-BYTE que el camino de la pizarra
  (`inputs_from_slots → assemble_snapshot`) produce el mismo WorldSnapshot de 31 B que el
  contrato + `sizeof==31`. **Garantía de que mover al emisor NO cambia el cable.**

## Verificación (lo que SÍ se probó)

- **Host (g++):** suite completa 79 envs / 1107 tests / 0 fallas. Tests nuevos verdes
  (test_snapshot_from_slots +1, test_publish_decision 7, test_contract_parity 5).
- **Compilación Teensy (pio):** `top_robot2_pri_rt` SUCCESS, `top_robot2_pri_snaptimer`
  SUCCESS (barreras `dmb` compilan en ARM), `top_robot2_pri` (competencia) SUCCESS (byte-neutro).

### Etapa 3 — HC-SR04 no-bloqueante + round-robin con skip del caído (commit `aa2af2d`)
- `sensors_tof.cpp` (gateado `-DTOP_ENABLE_HCSR04_ASYNC`): cablea `hcsr04_async.h` — ISR de
  ECHO (CHANGE) → `hcsr04_on_edge`; el tick dispara (`hcsr04_due` + pulso TRIG 10 µs +
  `on_trig_sent`) y cosecha por `hcsr04_poll`. **Elimina el spike de 12 ms del `pulseIn`.** La
  **race loop↔ISR** (que el borrador del workflow tenía sin resolver) se cierra con
  `noInterrupts()` alrededor de TODO acceso del loop a la FSM (la FSM NO es volatile: la
  sección crítica serializa la ISR de ECHO y actúa de barrera — sin el `const_cast`-sobre-
  `volatile` del borrador). `read_hcsr04` bloqueante queda como `#elif` → competencia byte-idéntica.
- `sensors_tof.cpp` (gateado `-DTOP_ENABLE_TOF_SCHED`): cablea `tof_schedule.h` — round-robin
  que SALTEA el ToF caído; byte-equivalente con los 4 vivos.
- `platformio.ini`: `top_robot2_pri_rt` ahora agrupa los **4** flags.
- Verificado: `pio top_robot2_pri_rt` SUCCESS (glue Arduino) + `top_robot2_pri` SUCCESS
  (byte-neutro). Módulos puros ya host-testeados (test_hcsr04_async, test_tof_schedule).

## Lo que NO se hizo / pendiente

- **bno_read_sm scheduler:** deliberadamente NO cableado. La cadencia real del BNO la fija
  `BNO_READ_INTERVAL_MS` (que `TOP_BNO_FAST` ya pone en 10 ms), no el gate de main_top → el
  scheduler sería *polish*, no acelera (lo confirmó la revisión adversarial). La latencia ya
  baja con `TOP_BNO_FAST` (en el env rt). Queda como mejora futura (deconflict tested).

## ⚠️ Lo cierra el equipo en BANCO (regla #1 — nada de esto está validado en hardware)

1. **DIAGNÓSTICO BARATO Y URGENTE (cero código nuevo):** flashear `top_robot2_pri_fastbno` y
   ver si la oscilación mejora. Si sí → la causa era la latencia del rumbo en TOP. Si no → el
   peso está en el **lazo de CENTRAL** (otra placa) y hay que medirlo allá.
2. `top_robot2_pri_rt`: correr T1-T7 del IMPL doc (byte-identidad OFF, snapshot @100Hz bajo
   carga, fail-safe por slot, loop-muerto, WCET de la ISR, deconflict BNO, torn-read/ordering).
3. **Medir latencia rumbo→CENTRAL antes/después** del fast-BNO con osciloscopio/timestamp.
4. Solo tras validar: promover `top_robot2_pri_rt` a `default_envs`.

## Nota de proceso

Un agente del workflow de specs commiteó `read_stamp.h` (`272c7ec`) en el working tree
compartido (tenían Bash). Salió limpio (solo ese módulo + atribución correcta), pero confirma
el riesgo de git paralelo del CLAUDE.md: en adelante, los agentes de workflow deben ser
read-only (no commitear).
