---
title: "Implementación RT paralela del TOP — pizarra + emisor desacoplado @100 Hz"
date: 2026-06-16
author: "Claude (Anthropic — Opus 4.8 1M), sesión autónoma con Gustavo"
status: vivo
tipo: handoff-implementación
rama: agente/top-rt-paralelo
relacionada: ARQUITECTURA-SENSORIAL-TOP-NO-BLOQUEANTE.md (diseño), ESTIMACION-FUSION-TOP.md, HANDOFF-INTEGRACION-RT.md
---

# Implementación de la arquitectura TOP no-bloqueante — pizarra + emisor @100 Hz

> **Estado de madurez (honesto):** los módulos PUROS están **programados y host-verificados**
> (1098 tests g++ verde). El emisor de firmware está **gateado y compila** (ON y OFF), pero
> **NO está validado en hardware** — la ISR/IntervalTimer, el WCET, la reentrancia de Serial4 y
> el ordering de memoria de la pizarra **los cierra el equipo en banco** (regla #1 del repo:
> Claude no marca hardware como hecho). Todo lo gateado es **byte-neutro apagado** → el binario
> de competencia de hoy NO cambia.
>
> **Dónde vive:** rama `agente/top-rt-paralelo` (worktree aislada; no toca `main`).

## 1. Esencia (la idea organizadora)

El TOP es un **EMISOR de un modelo del mundo @100 Hz** hacia CENTRAL, alimentado por sensores de
**latencia heterogénea** (cámaras ~ms, ToF ~10-15 ms, pulseIn del HC-SR04 ~12 ms, BNO ~1,5 ms)
sobre **UN bus I²C Wire que NO se puede paralelizar** (BNO secundario + 4 ToF en un solo SDA/SCL).
El diseño **DESACOPLA el ritmo del emisor del ritmo impredecible de los sensores** vía una
**PIZARRA de frescura**, y **degrada cada dato a un sentinela honesto cuando envejece** — nunca
un dato viejo disfrazado de fresco.

**Cuello dominante:** el bus I²C Wire compartido. La pizarra **desacopla el snapshot del bus, NO
acelera el bus** (paralelismo BNO↔ToF = cero; ningún DMA/ISR/seqlock lo cambia). El **HEADING es
la raíz** del mapa (rota todo; la pose depende de él).

## 2. Las 3 capas (vista vertical) + el fail-safe (concern transversal)

```
  PRODUCTORES (cada sensor, a su ritmo)            [Fase 1 ✅ puro / Fase 2 ⬜ banco]
     hcsr04_async · bno_read_sm · tof_schedule · cámaras · OTOS
        |  publican a su SLOT con su timestamp
        v
  PIZARRA  (sensor_slot.h: seqlock doble-buffer + frescura)   [Fase 0 ✅]
        |  el emisor lee SOLO RAM (cero bus)
        v
  ENSAMBLADOR FAIL-SAFE  (snapshot_from_slots + snapshot_assembler)  [✅ puro]
        |  por slot: fresco→dato · viejo/never→SENTINELA honesto
        v
  EMISOR @100 Hz  (snapshot_emitter: IntervalTimer → assemble → TX no-bloqueante)  [Fase 3 🟡]
        |
        v   Serial4 → CENTRAL
```

**El fail-safe (concern transversal, el corazón de "super confiable"):** cada campo del
WorldSnapshot es **"dato fresco-y-válido" o "sentinela honesto"**. Un slot que vence colapsa a su
sentinela ya entendido por CENTRAL: pelota→`visible=0`, arco→`visible=0`, heading→limpia el bit4
`heading_valid`, pose→`confidence=0`, obstáculo→`0xFFFF` (=libre). **Nunca hold-last presentado
como fresco.** Default-to-safe: "nunca recibido" se trata igual que "vencido".

## 3. Qué se construyó esta sesión

### 3.1 Módulos PUROS (host-verificados — `bash scripts/run-host-tests.sh`, 1098 tests, 0 fallas)

| Módulo | Qué hace | Tests | Tipo |
|---|---|---|---|
| `src/shared/sensor_slot.h` (+capped) | pizarra seqlock + `slot_read_latest_capped()` (corta el `for(;;)` que colgaría la ISR del emisor; retorna `false`→sentinela en vez de girar) | 29 | mejora aditiva |
| `src/shared/freshness_policy.h` | política genérica FRESH/STALE/NEVER + `conf_gate` (el techo de frescura le gana a la confianza) | 10 | nuevo |
| `src/shared/snapshot_from_slots.h` | **la costura** pizarra→ensamblador: lee cada slot con UN solo `now`, decide frescura por umbral, arma `SnapshotInputs` | 14 | nuevo |
| `src/shared/snapshot_assembler.h` | ensamblador fail-safe (ya existía, validado) | 13 | existente |
| `src/shared/hcsr04_async.h` | FSM async del ultrasónico: trigger→eco→distancia/**timeout→N/A** (mata el `pulseIn` bloqueante de 12 ms) | 14 | nuevo |
| `src/shared/bno_read_sm.h` | scheduler del 2º BNO que **hereda el deconflict 8 ms** + cadencia; `cfg_valid` rechaza `{10ms,8,false}` (el caso que **recongela el yaw**) | 21 | nuevo |
| `src/shared/tof_schedule.h` | round-robin + **skip del caído** (byte-equivalente al `s_rr` actual; 4 caídos→`0xFF` sin colgar) | 15 | nuevo |
| `src/shared/pose_fusion.{h,cpp}` (anti-free-run) | **fix del bug**: >500 ms sin corrección ToF → `valid=false, confidence=0` (hoy reportaba `conf=10` divergiendo sobre el drift del OTOS = dato viejo disfrazado de fresco) | 19 | mejora |

### 3.2 Firmware GATEADO (`-DTOP_ENABLE_SNAPSHOT_TIMER`; compila ON y OFF; byte-neutro OFF — BANCO)

- `src/top/snapshot_emitter.{h,cpp}` — emisor @100 Hz por `IntervalTimer`. El loop llama
  `snapshot_emitter_publish()` (vuelca los reads a la pizarra); la ISR lee la pizarra
  (`inputs_from_slots`→`assemble_snapshot`) y manda no-bloqueante por Serial4.
- `src/top/main_top.cpp` — cableado **gateado** (4 sitios bajo `#ifdef`): include, `init` en setup,
  `publish` en el loop, y el emit-en-loop de hoy bajo `#else`. `build_snapshot` marcado
  `[[maybe_unused]]` (sin uso bajo el flag). **El WDT sigue alimentándose SOLO desde el loop.**
- `platformio.ini` — env `top_robot2_pri_snaptimer` (BANCO, no para partido).

**Verificado:** `pio run -e top_robot2_pri` (OFF) y `... _snaptimer` (ON) → ambos **SUCCESS**. Los 7
módulos puros **compilan en el firmware Teensy real** (no solo host), confirmando compatibilidad
Arduino.

## 4. Lo load-bearing (no romper)

1. **El bus I²C Wire es el cuello físico.** La pizarra desacopla el snapshot del bus; NO lo
   acelera. ⚠️ Trampa: creer "ahora todo es paralelo" y subir la cadencia del BNO → **recongela el
   yaw** (`bno_read_sm::cfg_valid` lo atrapa en software, pero el invariante es físico).
2. **El deconflict temporal 8 ms (BNO↔ToF) + el BNO a 20 Hz son LOAD-BEARING.** Sacarlos recongela
   el yaw (banco 2026-06-02/06-08). `bno_read_sm.h` los hereda como parámetros, no los supera.
3. **El WDT (`watchdog_feed`) queda SOLO en el loop** (`main_top.cpp:391`), **nunca en la ISR del
   emisor.** Si el emit lo alimentara, un loop muerto quedaría enmascarado.
4. **UN solo escritor por slot** (single-writer del seqlock). El `publish` corre solo desde el loop.
5. **`volatile` + barreras `__DMB()` en el seqlock** = glue de banco (R2). Los tests host pasan sin
   ellas (single-thread); el torn-read por reordenamiento del M7 **solo aparece en hardware** → se
   valida en banco con el timer escribiendo fuerte + un checksum redundante en el struct.

## 5. Plan de integración (dónde estamos / qué falta)

| Fase | Qué | Estado |
|---|---|---|
| **F0** | contrato puro de la pizarra (`sensor_slot.h`) | ✅ |
| **F1** | productores puros (`hcsr04_async`, `bno_read_sm`, `tof_schedule`) + política de frescura | ✅ host-verificado |
| **F2** | **cablear los productores: cada read real publica a su slot CON SU read-timestamp** | ⬜ **BANCO** — esto da la frescura POR-SENSOR |
| **F3** | emisor por `IntervalTimer` (`snapshot_emitter`) | 🟡 scaffold gateado compila; **banco valida ISR/WCET/reentrancia** |
| **F4** | integración viva + `sample_age_ms` en el snapshot (schema v4, re-flashear TOP+CENTRAL) | ⬜ post-Incheon |

> **⚠️ Alcance de la frescura HOY (honesto):** el `snapshot_emitter_publish()` actual **republica
> los getters cacheados cada loop con `now`**, así que la frescura del slot refleja la cadencia del
> LOOP, no la del SENSOR. Eso **YA cubre el LOOP-MUERTO** (si el loop se cuelga deja de publicar →
> todos los slots vencen → todo sentinela → CENTRAL frena, + el WDT resetea). **NO cubre todavía la
> muerte de UN sensor con el loop vivo** (el getter devuelve su último valor cacheado). Esa es la
> **Fase 2**: cada `sensors_tof_tick`/`sensors_imu_tick`/`cameras_tick` debe llamar `slot_publish`
> con el timestamp del read real (o usar el `age` que el sensor ya expone, p.ej.
> `comm_down_pose_age_ms`). Los módulos de frescura están listos; falta SOLO el read-time por sensor.

## 6. Plan de prueba de banco (lo cierra el equipo — regla #1)

Sobre el env `top_robot2_pri_snaptimer`, con el robot armado + batería:

- **T1 — Byte-identidad OFF:** `objdump`/hex de `top_robot2_pri` (sin flag) == binario de hoy. (El
  gateo `#ifdef` lo garantiza estructuralmente; confirmar.)
- **T2 — El snapshot sale @100 Hz bajo carga:** con los 4 ToF + cámaras + pulseIn activos, del lado
  CENTRAL `snap_fresh=Y` estable y la tasa NO cae aunque el loop esté trabado en un read (es el
  punto de todo: con el emit-en-loop caía a ~6 Hz).
- **T3 — Fail-safe por slot:** desconectar un sensor en caliente → su campo del snapshot pasa a su
  sentinela (pelota `visible=0`, obstáculo `0xFFFF`, etc.) **sin frenar el snapshot ni contaminar
  los otros slots.** (Requiere F2 para la frescura por-sensor; con F3-lite solo se ve en loop-muerto.)
- **T4 — Loop-muerto:** inducir un cuelgue del loop (I²C trabado) → el WDOG resetea a 1 s y, mientras,
  el emit sale TODO sentinela (CENTRAL frena). El emit NO enmascara el cuelgue.
- **T5 — WCET de la ISR:** `snapshot_emitter_isr_wcet_us()` acotado (la ISR no debe robarle al RX de
  cámaras/OTOS → 0 bytes perdidos; 0 reentradas en Serial4).
- **T6 — Deconflict del BNO:** girar el robot → el heading trackea sin congelarse con el emit por
  timer corriendo (el deconflict 8 ms sigue vigente).
- **T7 — Torn-read / ordering:** con `volatile`+`__DMB()` cableados, un campo de checksum redundante
  en el struct del slot NO detecta inconsistencias bajo el timer escribiendo a alta tasa.

## 7. Cómo usarlo

- **Correr los tests host de los módulos nuevos:** `cd software/teensy/Soccer\ 2026 && bash scripts/run-host-tests.sh` (todos) o `... test_hcsr04_async` (uno).
- **Compilar el firmware con el emisor:** `pio run -e top_robot2_pri_snaptimer` (ON) · `pio run -e top_robot2_pri` (OFF, competencia).
- **Reusar los módulos puros** (host-testeados, listos para cablear): `freshness_policy.h`,
  `hcsr04_async.h`, `bno_read_sm.h`, `tof_schedule.h`, `snapshot_from_slots.h`, `slot_read_latest_capped`.

## 8. Honestidad de madurez (regla del repo)

`pio SUCCESS` y `host-tests verde` prueban **lógica y compilación**, NO comportamiento en el Teensy
real. Todo lo que toca el binario vivo (F2-F4) o el hardware **lo cierra el equipo humano en banco**.
Esta sesión deja: la maquinaria de fail-safe **programada y verificada en host**, el emisor
**gateado y compilando**, y este plan para que el banco arranque sin re-investigar.
