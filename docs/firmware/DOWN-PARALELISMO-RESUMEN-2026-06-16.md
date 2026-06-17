---
title: "DOWN paralelo: resumen de estado + camino para que la luz no espere al OTOS (3 lecturas / 2 envíos / 200 Hz)"
date: 2026-06-16
author: "Claude Opus 4.8 (Anthropic) — requested-by Gustavo Viollaz"
status: resumen-para-decision
tipo: analisis
area: down / tiempo-real
scope: src/down
relacionado: docs/firmware/ARQUITECTURA-LAZO-DOWN-RT.md, docs/firmware/ARQUITECTURA-SENSORIAL-TOP-NO-BLOQUEANTE.md, team-tasks/TASK-309
---

# DOWN paralelo — resumen para decisión (sin tocar código)

> **Pedido de Gustavo:** llevar la DOWN a trabajo PARALELO de modo que **la lectura de los
> 32 sensores de luz NO se demore por la lectura SERIE/bloqueante de los 2 OTOS**. Visión:
> **3 lecturas en paralelo** (luz + OTOS-A + OTOS-B) y **2 envíos en paralelo** (a TOP y a
> CENTRAL), idealmente **todo a 200 Hz**. Este documento analiza la idea, el programa actual,
> los docs existentes, qué YA está hecho y qué falta para implementarlo. **Cero cambios de código.**

---

## 1. Veredicto en 30 segundos

1. **La visión es FACTIBLE** en el Teensy 4.0 — pero ojo: el M7 es **single-core**. "Paralelo"
   acá NO son threads: es **interrupciones (ISR) + DMA + periféricos corriendo solos** mientras
   el CPU hace otra cosa. Es exactamente lo que el **TOP ya implementó** (`snapshot_emitter`).
2. **El objetivo central ("la luz no espera al OTOS") tiene una solución INMEDIATA y de bajo
   riesgo que YA está programada**: F1 (ADC rápido) + F2 (OTOS rápido). El OTOS pasa de bloquear
   **~3-4 ms → ~0.5 ms** (menos de 1 tick de línea). Eso solo, **sin DMA ni ISR**, ya resuelve el
   99% del problema para Incheon. **Falta SOLO validarlo en banco** (TASK-309).
3. **El "paralelo real" literal** (los 2 OTOS leyéndose por DMA al mismo tiempo + la luz por
   timer-ISR) es **post-Incheon / 2027**: el blocker duro es que **el core Teensy no da I²C por
   DMA** (hay que escribir un driver a pelo). Alto esfuerzo, alto riesgo. No se paga antes de Incheon.
4. **200 Hz en todo entra de sobra en el UART** (se usa ~32% de la banda hoy). El cuello NO es el
   UART ni el envío — es el **OTOS bloqueante**. Resuelto F2, subir a 200 Hz es cambiar 2 constantes.

**Recomendación:** validar **F1 + F2 en banco YA** (eso da el objetivo para Incheon). El timer-ISR
y el DMA real, después del mundial.

---

## 2. Cómo está HOY (lo medido en el código)

La DOWN es un **superloop cooperativo puro, un solo hilo** (`main_down.cpp` loop):

```
cada vuelta:  RX (drain UART) → barrido luz (1 kHz) → SEND línea a CENTRAL (200 Hz)
              → otos_tick (100 Hz, BLOQUEANTE ~3-4 ms) → SEND pose/vel a TOP (100 Hz)
```

- **NO hay ISR de aplicación, NO hay DMA, NO hay IntervalTimer.** Todo corre en secuencia.
- **El cuello confirmado:** `otos_tick()` es **100% bloqueante** — 4 transacciones I²C
  (`getPosition`+`getVelocity` × 2 OTOS) a **100 kHz**, ~3-4 ms. Mientras corre, el barrido de
  luz de 1 kHz y el send de 200 Hz **se quedan parados** → la luz pierde 3-4 ticks de corrido =
  **ventana ciega de borde a velocidad alta.** Ese es el problema que Gustavo quiere matar.
- **Dato clave a favor:** los 2 OTOS **ya están en buses físicos distintos** (OTOS-A → `Wire`,
  OTOS-B → `Wire1`), pero **hoy se leen EN SERIE** (uno y después el otro, bloqueando). El hardware
  permite leerlos en paralelo; el firmware todavía no lo hace.
- **Frecuencias hoy:** línea **200 Hz** (a ambas placas), pose + vel **100 Hz** (a ambas placas).
- **Tiempos hoy (CALCULADOS, no medidos en banco):** barrido de luz **~717 µs**, OTOS **~3-4 ms**.
  > ⚠️ Estos números son cálculos del diseño, NO mediciones. **F0 (`down_loopmon`) es el instrumento
  > que cablé para MEDIRLOS** en banco. Sin ese dato, todo lo demás es estimación.

---

## 3. La visión de Gustavo, traducida al hardware del Teensy 4.0

| Lo que pide Gustavo | Cómo se hace en un M7 single-core | ¿Comparten recurso? |
|---|---|---|
| **Proceso 1: leer luz** | `IntervalTimer` (1 kHz) que dispara una ISR de barrido — solo GPIO + ADC | NO toca el bus de los OTOS → **puede preemptar al OTOS sin conflicto** |
| **Proceso 2: leer OTOS-A** | I²C-DMA en `Wire` (LPI2C1) | bus propio |
| **Proceso 3: leer OTOS-B** | I²C-DMA en `Wire1` (LPI2C3) | bus propio → **los 2 OTOS pueden converger por DMA en paralelo REAL** |
| **Envío 1: a TOP** | `Serial5` (LPUART8), TX por ISR del core, no-bloqueante | UART propio |
| **Envío 2: a CENTRAL** | `Serial1` (LPUART6), TX por ISR del core, no-bloqueante | UART propio → **ya son concurrentes** |
| **Todo a 200 Hz** | un timer-emisor lee la "pizarra" y manda a 200 Hz, clavado | el UART aguanta (ver §6) |

**La clave técnica que da la razón a Gustavo:** la luz (GPIO+ADC) y los OTOS (I²C) **no comparten
ningún recurso de hardware**. Por eso una ISR de línea por timer **puede interrumpir al OTOS a
mitad de su lectura** y barrer los sensores igual — el barrido no toca el bus I²C, no hay conflicto.
Ese es el corazón de "la luz no espera al OTOS".

---

## 4. Qué YA está hecho (no hay que inventarlo)

### 4.1. El TOP ya implementó el patrón EXACTO (es el template a copiar)
`src/top/snapshot_emitter.{h,cpp}` (gateado, host-testeado): el **loop publica** a una pizarra
(single-writer, sin bus) y un **`IntervalTimer` @100 Hz dispara una ISR que SOLO lee la pizarra
(RAM, cero bus) y manda no-bloqueante** — el envío sale **clavado** aunque el loop esté trabado
leyendo un sensor lento. DOWN clona esta estructura cambiando los tipos y **duplicando el TX**
(uno a `Serial1`, otro a `Serial5`) en vez de uno solo. **Invariantes que NO se rompen:** el
watchdog se alimenta SOLO en el loop (nunca en la ISR — un loop muerto debe resetear); la ISR no
toca ningún bus ni hace `Serial.print` ni float pesado.

### 4.2. La pizarra (seqlock) ya existe y es genérica
- `src/shared/sensor_slot.h` — el seqlock `SensorSlot<T>` (doble-buffer + contador `seq`,
  publish/read wait-free, frescura wrap-safe). **Puro, host-testeado, YA usado por el TOP.** DOWN
  lo usa **sin modificar**.
- `src/down/down_blackboard.h` — **la pizarra de DOWN YA ESTÁ ESCRITA**: `DownBlackboard { SensorSlot<LineStatusV2> line; SensorSlot<Pose2D> pose; SensorSlot<Velocity2D> vel; }`,
  gateada `-DDOWN_BLACKBOARD`, host-testeada (12 tests). **Falta SOLO cablearla al loop.**

### 4.3. Las mejoras de velocidad ya están programadas (gateadas, off-by-default)
| Gate / env | Qué hace | Estado |
|---|---|---|
| `-DDOWN_OTOS_FAST_I2C` / `down_otosfast` | **F2:** OTOS a 400 kHz + `getPosVelAcc` (4 transacciones → 2): **~3-4 ms → ~0.5 ms** | ✅ programado, compila, **falta banco** |
| `-DDOWN_ADC_FAST` + `-DDOWN_ADC_DUAL` / `down_adcfast`,`down_adcdual` | **F1:** averaging-1 + dual-ADC: barrido **~717 µs → ~126 µs** | ✅ programado, compila, **falta banco** |
| `-DDOWN_RX_HARDEN` / `down_rxharden` | **F5:** RX endurecido (no bloquea por calib ni ruido) | ✅ programado |
| `-DDOWN_LOOP_MONITOR` / `down_loopmon` | **F0:** mide el WCET real de la vuelta (incl. spike OTOS) | ✅ — es el instrumento de banco |
| `-DDOWN_RELIABLE_GATE`, `-DDOWN_EARLY_EVIDENCE` | F4 fail-safe + F3 detección temprana | ✅ programados |

**Competencia `[env:down]` es BYTE-IDÉNTICA** mientras no se prenda ningún flag.

---

## 5. Qué FALTA para la visión completa (lo que NO está programado)

| Pieza | Qué es | Esfuerzo | Riesgo | Cuándo |
|---|---|---|---|---|
| **Subir a 200 Hz** (OTOS+pose+vel) | bajar 2 constantes (`OTOS_TICK_INTERVAL_MS`, `COMM_SEND_INTERVAL_MS` 10→5) | **trivial** | bajo *pero* depende de F2 | tras validar F2 |
| **Timer-emisor** (2 envíos clavados a 200 Hz) | clonar `snapshot_emitter` con 2 TX (Serial1+Serial5) + cablear la pizarra | medio | medio (re-entrancia) | post-Incheon |
| **OTOS Nivel 1 — IntervalTimer de línea** | la luz pasa a una ISR @1 kHz que PREEMPTA al OTOS → **la luz NUNCA espera al bus** | medio | medio (`g_raw` ISR↔loop: volatile/seqlock) | post-Incheon |
| **OTOS Nivel 2 — I²C async/DMA** | los 2 OTOS leyéndose por DMA **en paralelo real** | **ALTO** | **ALTO** | **2027** (ver blocker #1) |
| **ADC-DMA continuo** (barrido ~50-60 µs) | pipeline del settle del CD4051 con DMA | alto | alto | 2027 |
| **Cablear la pizarra** (`-DDOWN_BLACKBOARD` al loop) | + las 2 barreras `__DMB()` obligatorias + `volatile` del `seq` | medio | medio | junto con Nivel 1 |

---

## 6. Los 3 blockers técnicos (por qué el "paralelo real" es 2027)

1. **I²C asíncrono/DMA del OTOS — el blocker MÁS DURO (confirmado por grep).** El core Teensy
   **no ofrece I²C por DMA**: `WireIMXRT.cpp` tiene **cero** referencias a DMA; `requestFrom()` es
   100% bloqueante con timeouts hardcodeados. La lib del OTOS (`sfTkArdI2C`) es bloqueante hasta el
   fondo. Para leer los 2 OTOS por DMA en paralelo **hay que escribir un driver LPI2C-DMA a pelo**
   (una subclase de `sfTkII2C` con ISR de completion). **Alto esfuerzo, alto riesgo, 2027.**
2. **ADC-DMA del barrido — el core SÍ lo tiene, el CD4051 lo complica.** La lib `pedvide/ADC`
   ofrece DMA continuo (`AnalogBufferDMA`), pero **no pausa para cambiar el SEL del mux + esperar
   el settle de 5 µs**. Hay que construir un pipeline manual. **El dual-ADC (F1) NO es blocker** —
   ya está programado y compila; es el DMA continuo (Capa 3) lo que falta. P2/2027.
3. **Re-entrancia luz-ISR vs `dm_update`-loop — HOY no existe, es prospectiva.** `g_raw[]` es un
   array plano no-`volatile`. En el superloop cooperativo de hoy **no hay race**. Cuando la luz
   pase a ISR (Nivel 1), aparece: hay que `volatile`-ar el doble-buffer o snapshotear con
   `noInterrupts()`, **y las 2 barreras `__DMB()` del seqlock son obligatorias** (sin ellas el
   M7 reordena memoria → torn-read que pasa los tests host y falla SOLO en hardware). El remedio
   está andamiado (`down_blackboard.h`), falta cablearlo.

---

## 7. Camino recomendado (impacto / riesgo)

### 🟢 INMEDIATO — para Incheon (bajo riesgo, ya programado, solo banco)
1. **Validar F2** (`down_otosfast`) en banco: medir el spike del OTOS antes/después (esperado
   ~3-4 ms → ~0.5 ms). 30 min de marcha, 0 errores I²C a 400 kHz (si hay, bajar a 100 kHz).
2. **Validar F1** (`down_adcfast` → `down_adcdual`): barrido 717→126 µs; que averaging=1 no
   flickee el umbral; comparar el delta carpet/blanco por ADC1 vs ADC2.
3. **Con F2 validado**, subir OTOS+pose+vel a 200 Hz (2 constantes) y medir.

> **Esto solo ya cumple el objetivo de Gustavo para Incheon:** con el OTOS bloqueando <0.5 ms
> (menos de 1 tick de línea) en vez de 3-4 ms, **la luz prácticamente deja de esperar al OTOS** —
> sin ISR ni DMA, sin tocar la arquitectura, con el binario de competencia byte-idéntico hasta
> que el banco lo promueva. Todo en **TASK-309**.

### 🔵 POST-INCHEON / 2027 — el "paralelo real" literal
4. **OTOS Nivel 1** (IntervalTimer de línea por ISR) + **cablear la pizarra** → la luz NUNCA
   espera al bus, ni siquiera el medio milisegundo de F2.
5. **Timer-emisor** (clon de `snapshot_emitter`, 2 TX) → envíos clavados a 200 Hz aunque un read
   se atrase.
6. **OTOS Nivel 2** (driver I²C-DMA) → los 2 OTOS en paralelo real. El más caro; capitaliza 2027.

---

## 8. Lo honesto (regla #1)
- **Nada de esto está probado en banco.** Los ~717 µs y ~3-4 ms son cálculos. El equipo con la
  placa cierra cada fase (TASK-309); Claude no marca hardware como `done`.
- **No hay que rehacer el diseño:** existe (`ARQUITECTURA-LAZO-DOWN-RT.md`). Este documento es el
  resumen ejecutivo de "qué falta para el paralelo" sobre ese diseño.
- **El TOP ya pavimentó el camino:** `snapshot_emitter` + `sensor_slot` + el patrón de invariantes
  son el molde probado. DOWN lo clona, no lo inventa.

## Referencias
- Diseño completo por fases: `docs/firmware/ARQUITECTURA-LAZO-DOWN-RT.md`
- Template no-bloqueante del TOP: `docs/firmware/ARQUITECTURA-SENSORIAL-TOP-NO-BLOQUEANTE.md` +
  `src/top/snapshot_emitter.{h,cpp}` + `src/shared/sensor_slot.h`
- Plan de banco DOWN: `team-tasks/2026-06-16-task-309-banco-reingenieria-rt-down.md`
