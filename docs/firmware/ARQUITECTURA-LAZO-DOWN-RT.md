---
title: "Arquitectura del lazo de tiempo real de la placa DOWN — lectura veloz del anillo + detección temprana confiable + difusión sin demoras"
date: 2026-06-15
author: "Claude (Anthropic - Claude Opus 4.8 1M)"
status: vivo
tipo: diseno
area: sensores
scope: src/down
---

# Arquitectura del lazo RT de la placa DOWN

> **Disciplina, primero.** Este es un documento de **diseño + andamiaje gateado
> off-by-default**. NO reescribe el firmware DOWN vivo. Nada de acá está probado en
> hardware: la regla no negociable #1 de `CLAUDE.md` manda — **solo el equipo con la
> placa cierra una TASK de hardware como `done`**. Claude diseña, documenta y escribe
> módulos puros host-testeables; no marca nada como funcionando porque "compila" o
> "los tests host pasan". Todo lo nuevo entra detrás de un `#define` que por default
> deja el binario de competencia **byte-idéntico** al de hoy.

---

> **ESTADO DE IMPLEMENTACIÓN — 2026-06-16 (sesión RT DOWN).** El diseño de abajo pasó por
> validación adversarial (7 agentes) + se PROGRAMÓ el glue gateado + los módulos puros, todo
> host-tested + compilado (NO probado en banco — regla #1; lo cierra el equipo, **TASK-309**).
> Competencia `[env:down]` byte-idéntica (todo off-by-default). Estado por fase:
> - **F0** LoopMonitor cableado (`-DDOWN_LOOP_MONITOR`, env `down_loopmon`) — ✅ programado/compila.
> - **F1** averaging-1 (`-DDOWN_ADC_FAST`) + dual-ADC (`-DDOWN_ADC_DUAL`) — ✅ + `adc_scan_plan.h` (puro, 10 tests).
> - **F2** OTOS 400 kHz + `getPosVelAcc` (`-DDOWN_OTOS_FAST_I2C`) — ✅ programado/compila.
> - **F4** lectura confiable + sellado fail-safe (`-DDOWN_RELIABLE_GATE`) — ✅ + `line_reliable_gate.h` (puro, 13 tests).
> - **F5** RX harden: calib diferido + byte-budget + 512 B (`-DDOWN_RX_HARDEN`) — ✅ + `rx_calib_defer.h`/`rx_byte_budget.h` (puros, 11 tests).
> - **F3** detección temprana — módulos puros listos (`line_neighbors.h` 13 + `line_early_escape.h` refinado 20); cableado a `dm_update` POST-Incheon (F4 gobierna el camino vivo).
> - **F6** pizarra — `down_blackboard.h` (reusa `sensor_slot.h`, 12 tests); NO cableado al loop.
> Envs de banco nuevos: `down_loopmon`, `down_adcfast`, `down_adcdual`, `down_otosfast`,
> `down_reliable`, `down_rxharden`, `down_rt_all`. Revisión adversarial del glue: limpia (0 must-fix).

---

## 1. Esencia en 1 frase

**Leer los 32 sensores del anillo lo más rápido posible (dual-ADC + pipeline del
settle), avisar a CENTRAL apenas hay EVIDENCIA confiable de línea — con un vector de
escape ya calculado — y difundir ese aviso a CENTRAL y TOP sin que nada (ni el I2C del
OTOS, ni el serial, ni la calibración) le robe un solo barrido al camino de seguridad
de borde.**

Tres palancas, en orden de criticidad:

1. **Confiable** (lo más importante): ningún escape falso. Un sensor loco, un robot
   levantado, una calibración rota — nunca disparan un aviso de borde. La seguridad es
   el contrato.
2. **Temprano**: avisar con evidencia (≥2 sensores físicamente adyacentes) en vez de
   esperar los 6 sensores del `imminent_exit` actual.
3. **Veloz**: barrido de ~717 µs → ~126 µs (combo seguro) → ~50-60 µs (full), para
   dejar headroom y que el OTOS no estruje el muestreo.

---

## 2. Diagrama ASCII de capas

El lazo DOWN se piensa en cuatro capas. Las flechas son el flujo de datos; las cajas
con `[ya existe]` son módulos puros host-testeados que se REUSAN; `[NUEVO]` es lo que
falta escribir; `[refina]` es cambio gateado dentro de un módulo vivo.

```
 ┌───────────────────────────────────────────────────────────────────────────┐
 │  CAPA 0 — ADQUISICIÓN (productores, cada uno con su propio reloj/ISR)       │
 │                                                                            │
 │   ┌──────────────────────┐   ┌──────────────────┐   ┌──────────────────┐   │
 │   │ Anillo 32 sensores   │   │ 2× OTOS (I2C)    │   │ UART RX (S1/S5)  │   │
 │   │ 4× CD4051 + dual-ADC │   │ Wire / Wire1     │   │ comandos CENTRAL │   │
 │   │ [refina line_ring]   │   │ [refina otos]    │   │  + TOP           │   │
 │   │  settle pipelined    │   │ 400 kHz + burst  │   │ [ya: core ISR]   │   │
 │   │  IntervalTimer 1 kHz │   │ timer dedicado   │   │ ring + addMemFor │   │
 │   └──────────┬───────────┘   └────────┬─────────┘   └────────┬─────────┘   │
 └──────────────┼────────────────────────┼──────────────────────┼────────────┘
                │ g_raw[32]               │ pose/vel cruda       │ frames cmd
                v                         v                      v
 ┌───────────────────────────────────────────────────────────────────────────┐
 │  CAPA 1 — PIZARRA (blackboard, 1 escritor por slot, lecturas coherentes)    │
 │   [NUEVO sensor_slot.h — seqlock, gateado, hoy copia directa]               │
 │   g_slot_line<LineStatusV2>   g_slot_pose<Pose2D>   g_slot_vel<Velocity2D>  │
 └──────────────┬────────────────────────────────────────────────────────────┘
                │ raw cacheado + estado calib/salud
                v
 ┌───────────────────────────────────────────────────────────────────────────┐
 │  CAPA 2 — CÁLCULO CONFIABLE (dm_update — todo el cerebro de línea)          │
 │                                                                            │
 │   filtro temporal ─► umbral POR SENSOR (calib+hysteresis) ─► spatial       │
 │     [ya: line_filters]   [ya: line_calib + EEPROM]          [ya]           │
 │        │                                                                   │
 │        ├─► TOLERANCIA A FALLAS (compuerta de "lectura confiable")          │
 │        │     · sensor muerto/clavado/ruidoso → EXCLUIR  [ya: sensor_health]│
 │        │     · mux entero muerto → data_valid=0         [ya: MuxWatchdog]  │
 │        │     · robot levantado (todo oscuro) → RECHAZO  [ya: surface_mon]  │
 │        │     · saturado (todo blanco) → RECHAZO         [ya: lf_all_white] │
 │        │     · débiles excluidos uno a uno              [ya: lc_count_weak]│
 │        │                                                                   │
 │        ├─► GEOMETRÍA (centroide real, escape, cross_track)  [ya: line_geom]│
 │        │                                                                   │
 │        └─► DETECCIÓN TEMPRANA (vecino FÍSICO + vector escape) [NUEVO/refina]│
 │              evidencia (1..5) vs inminente (>=6) en sensors_on_line         │
 └──────────────┬────────────────────────────────────────────────────────────┘
                │ LineStatusV2 coherente + sellado de frescura
                v
 ┌───────────────────────────────────────────────────────────────────────────┐
 │  CAPA 3 — DIFUSIÓN (consumidor/difusor, no bloqueante, sin demoras)         │
 │   [ya: down_tx]  SEQ por enlace + backpressure (dropea, no espera)          │
 │   LineStatusV2 @200 Hz ─► CENTRAL (Serial1) + TOP (Serial5)                 │
 │   Pose2D/Velocity2D @100 Hz ─► CENTRAL + TOP                               │
 └───────────────────────────────────────────────────────────────────────────┘
```

La regla de oro del layout: **el OTOS (I2C lento) y la calibración (bloqueante) viven
FUERA del camino de la línea.** El barrido de línea solo toca GPIO + ADC; nunca espera
un bus.

---

## 3. Lectura rápida de los muxes

### 3.1. Lo que pasa HOY (medido en código, NO en banco)

`sample_all_sensors_hardware()` (`line_ring.cpp:43-61`) hace, por barrido:

- 8 iteraciones (una por canal lógico del CD4051);
- en cada iteración: 12 `digitalWrite` a los pines SEL de los 4 muxes
  (`line_ring.cpp:51-54`), **un** `delayMicroseconds(5)` de settle del CD4051
  (`line_ring.cpp:55`), y **4 `analogRead` secuenciales** (`line_ring.cpp:58`) sobre
  `PIN_MUX_OUT[4] = {A0, A1, A8, A9}` (`config_down.h:81`).

`analogReadResolution(10)` se setea (`line_ring.cpp:91`) pero el **averaging queda en
el default del core Teensy = 4 muestras promediadas por lectura**.

**Presupuesto ANTES (calculado, no medido):**

| Componente | Tiempo |
|---|---|
| `analogRead` 10-bit, avg=4 | ≈ 20.9 µs c/u |
| Por iteración: 12 digitalWrite + 5 µs settle + 4×20.9 | ≈ 89.6 µs |
| **Barrido completo (× 8)** | **≈ 717 µs** |

A 1 kHz (`LINE_TICK_INTERVAL_US = 1000`, `config_down.h:138`) eso consume **~72% del
tick**. Por eso el muestreo es frágil: cuando el OTOS hace su I2C bloqueante de 3-4 ms
(`main_down.cpp:183-190`), se pierden 3-4 ticks de línea seguidos → ventana ciega de
borde a velocidad alta.

> El diagnóstico `g_last_tick_us` existe (`line_ring.cpp:142`) pero **no está medido en
> banco** todavía. La CARD DOWN-4 de `docs/pruebas-banco/DOWN.md` debe crear el env que
> lo imprima. **El "ANTES" de 717 µs es un cálculo, no una medición** — el banco lo
> confirma o lo corrige.

### 3.2. Cómo se acelera — 3 capas acumulables, cada una gateada

Hecho de hardware load-bearing (verificado, forum PJRC 58387): en el Teensy 4.0
(IMXRT1062) los 4 pines de mux están **dual-mapeados a AMBOS ADCs** — A0=ADC1/ADC2_IN7,
A1=IN8, A8=IN13, A9=IN14. Por eso cualquier par de muxes se puede convertir EN PARALELO
(uno en ADC1, otro en ADC2). La lib `pedvide/ADC` ya viene DENTRO del core
(`framework-arduinoteensy/libraries/ADC/`), así que no hay vendoreo del registry.

| Capa | Mecanismo | Barrido | Prioridad |
|---|---|---|---|
| **1** — averaging 1 | `analogReadAveraging(1)`: 20.9→4.9 µs/lectura. El ALS-PT19 ya se filtra temporalmente aguas abajo (`lf_temporal_update`), no necesita el promedio HW. | 8×(1+5+4×4.9) ≈ **205 µs** | P1 |
| **2** — dual-ADC | `startSynchronizedSingleRead(pinA,pinB)` / `readSynchronizedSingle`: 2 muxes a la vez → 2 lecturas dobles en vez de 4 simples. | 8×(1+5+2×4.9) ≈ **126 µs** | P1 |
| **3** — pipeline + alta velocidad | ADC continuo/DMA + `ADC_CONVERSION_SPEED::HIGH_SPEED` (~1.5 µs/conv). Conmutar el SEL del PRÓXIMO grupo MIENTRAS el ADC convierte el actual: el settle de 5 µs se **solapa** con la conversión anterior en vez de sumarse. El barrido queda limitado por el settle, no por el ADC. | ≈ **50-60 µs** | P2 (2027) |

**Resultado: ~717 µs → ~126 µs (combo seguro 1+2) → ~50-60 µs (full). Reducción 6× a
13×.**

### 3.3. Por qué no demora / por qué es confiable

- `startSynchronizedSingleRead` arranca las 2 conversiones a la vez **en hardware** y
  `readSynchronized` cosecha ambas. El camino crítico se acorta porque 2 conversiones
  corren físicamente en paralelo — **no porque se "saltee" nada**.
- En Capa 3, el ADC continuo + DMA SACA el `analogRead` del camino: la conversión la
  hace el periférico en background y el doble-buffer entrega el barrido completo sin que
  el core espere. El muestreo deja de robar tiempo al loop.
- Bajar averaging a 1 NO degrada la detección: el ruido sube √4 = 2× por lectura cruda,
  pero el `lf_temporal_update` aguas abajo ya promedia. **Reversible con un `#define`.**
- Nada de esto toca el contrato de salida: `g_raw[]` sigue siendo el mismo buffer,
  `dm_update` lo lee crudo igual.

**Recomendación honesta de prioridad:** implementar **Capa 1+2** (alto impacto, bajo
riesgo); dejar **Capa 3 (DMA continuo)** como P2 capitalizable a 2027 — la complejidad
de DMA + doble-buffer no se paga hasta que el settle de 5 µs sea el cuello, y para eso
primero hay que **MEDIR el settle real del CD4051 en banco** (puede que 5 µs sea
conservador y se pueda bajar).

---

## 4. Detección temprana + vector de escape

### 4.1. El problema con lo de HOY

Hoy hay **un solo umbral de alarma y es TARDÍO**: `dm_update` prende `EV_IMMINENT_EXIT`
recién cuando `sensors_on_line >= imminent_depth` (= **6**, `comm_central.cpp:38`),
`down_model.cpp:316`. CENTRAL gatea TODO su fail-safe de borde con eso y solo eso (el
freno de borde y la entrada a `LINE_AVOID`).

Y lo más irónico: **el vector ya está calculado y nadie lo usa.**
`escape_angle_centideg = line_angle + 180°` se computa en `line_geometry` y viaja en
`LineStatusV2` (`types.h:146`), pero CENTRAL no tiene `world_model_get_escape_angle()`.

A ~1 m/s, esperar al 6º sensor significa que el robot recorre 3-4 anillos de penetración
(~11 mm entre anillos → decenas de mm DENTRO de la línea) antes de que CENTRAL se entere.

### 4.2. Criterio: "primera evidencia" vs "salida inminente"

DOS niveles de alarma sobre el MISMO frame, ambos con el vector disponible, calculados
DENTRO de `dm_update` (donde ya está todo el estado), **sin tocar `line_ring` ni el
wire-contract de 16 bytes**:

**(A) EVIDENCIA TEMPRANA** — nuevo, baja confianza, urgencia baja. El robot está
EMPEZANDO a pisar. Dispara apenas el primer sensor cruza su umbral CALIBRADO + histéresis
**Y tiene un VECINO FÍSICO también sobre umbral**.

> ⚠️ **No reusar `lf_spatial_filter`.** Exige vecino-DE-ÍNDICE, pero la geometría del PCB
> tiene 6 pares de índices contiguos que están físicamente LEJOS (`down_model.cpp:246`:
> idx 7↔8 = 141 mm; 23↔24 = 116 mm; 31↔0 = 102 mm). "Vecino de índice" ≠ "vecino
> físico". Hay que precomputar UNA VEZ una **LUT de vecinos físicos** desde `SENSOR_POS`
> (los 1-2 sensores más cercanos en mm a cada uno) y exigir 2 sensores **físicamente
> adyacentes** sobre umbral. Esto descarta el ruido de un sensor aislado sin esperar los 6.

Con ≥2 blancos físicamente adyacentes, `lg_compute_xy` da el centroide →
`escape_angle = centroide + 180°` (opuesto a los blancos = hacia adentro de cancha).

**Señalización SIN bit nuevo** (los 8 bits de `event_flags` están TODOS ocupados,
`types.h:162-169`): se reusa **`sensors_on_line`** (ya viaja, 0..32) como métrica de
confianza/profundidad. CENTRAL distingue **evidencia (1..5)** de **inminente (≥6)**
leyendo ese byte que YA recibe, y lee `escape_angle_centideg` que YA viaja. **Cero
cambio de wire.**

**(B) SALIDA INMINENTE** — existente, alta confianza, urgencia alta. Se mantiene
`EV_IMMINENT_EXIT` con `sensors_on_line >= 6` **intacto**. El freno duro de CENTRAL
sigue gateando ahí; **no se toca el fail-safe probado.**

### 4.3. Corregir el gate del vector

Para que el `escape_angle` exista en el rango temprano (1-5 sensores) hay un cambio
mínimo y gateado: hoy `line_present`/escape requiere el spatial-filter de índice. Con
1 solo blanco físicamente aislado, `line_present` sigue 0 (no inventamos vector de 1
punto); con ≥2 físicamente adyacentes, `line_present = 1` y `escape_angle` válido. Eso
es **estrictamente más temprano** que hoy.

### 4.4. Cómo lo consume CENTRAL (post-Incheon, off-by-default)

- Nuevo `world_model_get_escape_angle()` (espeja `get_cross_track_mm`).
- La fase ESCAPE del arquero usa ese vector MEDIDO en vez del "hacia adentro" hardcodeado.
- **Pre-freno SUAVE** (reducir velocidad hacia el borde, NO frenar a cero) en evidencia
  temprana; el **freno DURO** se reserva para inminente. Nunca latchear (lección
  anti-latch, `main_central.cpp:280-292`).

### 4.5. Latencia evidencia→CENTRAL (números reales)

| Etapa | Peor caso |
|---|---|
| raw del sensor (scan 1 kHz) | ≤ 1 ms |
| `dm_update` (puro) | < 50 µs |
| espera del slot de send (200 Hz) | ≤ 5 ms |
| framing/UART 16 B @230400 baud | ≈ 0.9 ms |
| **Total** | **~7 ms peor caso, ~3 ms típico** |

Hoy ese mismo ~7 ms **no arranca hasta el 6º sensor**. La detección temprana lo arranca
3-4 sensores antes → decenas de mm menos de penetración. (Si banco lo pide, el cuello del
slot de 5 ms se baja subiendo `LINE_URGENT` a 500 Hz.)

---

## 5. Calibración individual por sensor

### 5.1. Ya existe casi todo, y es bueno

Cada sensor tiene SU propio umbral. **No hay umbral global escondido.**

- **Estructura:** `SensorCalib { carpet, white, threshold, enabled, sensitivity }`
  (`line_calib.h`), array en `DownModel.calib[32]` (`down_model.h`). El threshold es el
  punto medio carpet/blanco POR SENSOR.
- **Cómo se calibra:** paso carpeta en `setup()` (`line_ring_calibrate_carpet`,
  `line_ring.cpp:168`) + paso blanco por comando UART desde CENTRAL (payload 0x21 →
  `line_ring_calibrate_white`, `line_ring.cpp:184`).
- **Cómo se persiste:** blob v2 de 201 B en EEPROM con magic + version + CRC16-CCITT
  (`calib_storage`), escrito con `EEPROM.update` (minimiza desgaste). Carga al boot:
  `comm_central_load_persisted_calib` (`comm_central.cpp:225`) — la EEPROM (referencia de
  BLANCO real) gana sobre la derivación boot-time que solo tiene carpet.
- **Cómo alimenta la detección:** `dm_update` calcula `th_eff = lc_threshold_with_sens(...)`
  POR SENSOR (`down_model.cpp:136`, `line_calib.cpp:9`). Con `sens=0` es **byte-idéntico**
  al histórico (truncación entera, no redondeo — `line_calib.cpp:14-19`).

### 5.2. Cuatro refinamientos (gateados, sin reescribir)

**A. Paso-blanco ACUMULATIVO por sensor.** Hoy `calibrate_white` promedia los 32 a la
vez asumiendo la franja COMPLETA bajo el robot — físicamente imposible (la franja toca
~12-15 sensores, no 32) → ensucia el white de los que nunca vieron blanco. Propuesta:
pasar el robot por la línea varias veces y quedarse, por sensor, con el **máximo (o
percentil alto)** observado, marcando `white_seen[i]`. Sensor sin `white_seen` → white
default + `enabled=0` (no vota). Da un white REAL por sensor.

**B. Margen/calidad por sensor en EEPROM (v3).** Agregar un flag `calibrated[i]` para
que al boot un sensor que históricamente nunca calibró bien arranque `enabled=0`. ⚠️
Bump de `CS_VERSION` invalida la calib v2 persistida → **NO bumpear cerca de
competencia**; B es post-Incheon.

**C. El umbral por sensor habilita bajar el conteo sin falsos.** La detección temprana
quiere disparar con MENOS sensores. Un `th_eff` bien ajustado individualmente (sensor
sucio = umbral propio más alto) hace que el PRIMER sensor que cruza sea señal confiable
y no ruido. La calib individual es exactamente lo que permite bajar el umbral de
evidencia.

**D. Fallback explícito en 3 niveles** (ya implementado, falta consolidarlo): (1) sin
EEPROM → carpet del boot + white default 800 (juega degradado, no ciego); (2) sensor sin
calib de blanco → `enabled=0`, excluido del centroide; (3) EEPROM corrupta (CRC) →
rechazo total + recalibración (`ec_erase_calibration` ya existe).

> **No demora el hot-loop:** toda la calibración es operación de BANCO/ADMIN con el robot
> QUIETO, disparada por comando, NUNCA en partido. El `th_eff` por tick es aritmética
> entera trivial. La adaptación viva del carpet (`lc_adapt_carpet`, α=0.02) es un MAC
> float por sensor.

---

## 6. Tolerancia a fallas — el "súper confiable"

Esta es la capa **más crítica**. La pregunta que responde: **¿cuándo es CONFIABLE una
lectura, antes de poblar `line_present` / `cross_track` / `escape_angle`?**

### 6.1. Lo que YA detecta y excluye/rechaza (vivo en `dm_update`)

| Falla | Detector | Acción |
|---|---|---|
| Sensor RUIDOSO (>20 transiciones/s) o STUCK (mismo raw >5 s) | `sensor_health` (`down_model.cpp:176-177`) | EXCLUIR del centroide |
| Mux entero muerto (8 canales clavados >100 ms) | `MuxWatchdog` (`down_model.cpp:280`) | `data_valid=0` + `EV_MUX_DEAD` |
| Sensor DÉBIL (\|white−carpet\| < 40) | `lc_count_weak` (`down_model.cpp:191-195`) | EXCLUIR uno a uno; >max → `data_valid=0` |
| Sensor deshabilitado a mano (`enabled=0`) | `down_model.cpp:180` | EXCLUIR |
| Robot LEVANTADO (≥28/32 oscuros >100 ms) | `surface_monitor` (`down_model.cpp:269`) | `EV_LIFTED` + `data_valid=0` |
| SATURADO (≥7/8 blancos) | `lf_all_white` (`down_model.cpp:205-208`) | zero `validated[]` + `data_valid=0` |
| Un sensor aislado | `lf_spatial_filter` (exige vecino) | nunca llega a `validated` |

**"Un sensor loco" tiene TRIPLE defensa hoy:** histéresis per-sensor ±20 counts + filtro
espacial (exige vecino) + `EV_IMMINENT_EXIT` exige ≥6. Para un escape falso harían falta
≥3 sensores contiguos ruidosos a la vez con vecino — físicamente improbable, y
`sensor_health` los marca en <1 s.

### 6.2. El agujero confirmado

`line_angle` / `escape_angle` / `cross_track` se POBLAN cada vez que `line_present`
(≥1 validado), **INDEPENDIENTE de `data_valid`** (`down_model.cpp:300-310`). Con
`data_valid=0`, DOWN igual manda números no-N/A. El contrato delega en CENTRAL no usarlos
— pero eso es confiar en que CENTRAL respete una regla escrita.

### 6.3. El criterio "LECTURA CONFIABLE" (compuerta explícita)

Todas deben cumplirse para emitir geometría útil:

1. NO levantado — `surface_monitor` ya lo da.
2. NO saturado todo-blanco — `lf_all_white` ya lo da.
3. NO mux muerto — `MuxWatchdog` ya lo da.
4. Calib utilizable (`n_weak ≤ max_weak_sensors`) — `lc_count_weak` ya lo da.
5. **SOPORTE MÍNIMO:** `validados ≥ MIN_SENSORS_FOR_VECTOR` (proponer 3, gateado,
   default = comportamiento actual). Es la "dosis de evidencia": tempranísima (anillo
   externo, 3 sensores) pero NO 1-2 ruidosos.

### 6.4. Tres acciones (todas en `dm_update`, cada una `-D` off-by-default)

**A. SELLAR la geometría cuando NO es confiable.** Si `data_valid==0`, forzar
`escape_angle`/`cross_track`/`penetration` a SENTINELA (`LSV2_NA_I16`/`LSV2_NA_U16`) en
vez del número crudo. Cambio: `if (g.line_present && data_valid) {...} else {N/A}`.
**Defensa en profundidad**: la fuente deja de mentir, no depende de que CENTRAL respete
la regla.

**B. Conteo de SALUD GLOBAL como 2º eje de `data_valid`.** Si `sh_healthy_count` cae bajo
un piso (proponer ≥24/32, gateado), tratar como geometría degradada → `data_valid=0`.
Hoy 10+ sensores enfermos NO bajan `data_valid` (solo se excluyen), y un centroide con 22
sensores sesgados puede mentir saliendo `valid=1`. El piso cierra ese caso.

**C. Gate de SOPORTE MÍNIMO para el vector.** `escape`/`cross` solo si
`validados ≥ MIN_SENSORS_FOR_VECTOR`.

> **No produce falsos y es auditable:** los detectores base ya son wrap-safe (overflow de
> `millis()`), auto-recovery y con debounce temporal independiente del rate. El criterio
> compuesto solo los AND-ea, no inventa estado. "Levantado" (todo-oscuro) y "saturado"
> (todo-blanco) son opuestos simétricos con umbrales 7/8 espejados; el piso de
> `healthy_count` cubre el medio. Todo host-testeable con datos sintéticos antes de banco.

⚠️ **Sin bit nuevo:** `event_flags` está LLENO. La señal de "no confiable" es
`data_valid=0` + el sellado a sentinela, que es lo que el contrato ya define como
compuerta maestra. Un `healthy_count`/`quality` real se reserva para `LineStatusV3` (hoy
`quality` es placeholder, `down_model.cpp:329`).

---

## 7. OTOS en paralelo — no roba ticks a la línea

### 7.1. Lo que pasa HOY

`otos_tick()` (`main_down.cpp:187`) corre cada 10 ms, **100% bloqueante**: 4
transacciones I2C separadas (`getPosition`+`getVelocity` × 2 OTOS), Wire/Wire1 a **100
kHz** (no hay `setClock` en `src/down`) → ~3-4 ms/tick. La lib mete `delayMs(3-5)`
hardcodeados. Eso roba 3-4 barridos de línea de corrido. El send ya va **antes** del
`otos_tick` (reorden audit #24, `main_down.cpp:174`) pero eso solo protege la latencia
del frame de ESE ciclo, no el robo de los 3-4 ms siguientes.

El send en sí NO bloquea: `comm_top_send_status` solo lee los globals cacheados
(`otos_get_x_mm()` etc.). Toda la latencia I2C está encerrada en `otos_tick()`.

### 7.2. Tres niveles, del más barato/seguro al más agresivo

**NIVEL 0 — recortar el bloqueo** (`-DDOWN_OTOS_FAST_I2C`), de ~3-4 ms a <~0.6 ms, SIN
cambiar la arquitectura superloop:
- (a) `Wire.setClock(400000)` / `Wire1.setClock(400000)` (Qwiic estándar Fast-mode) → 4×
  menos tiempo de bus.
- (b) `getPosVelAcc()` (ya en la lib, `sfDevOTOS.cpp:333`) lee pos+vel+acc en UN
  `readRegister` de 18 B en vez de 2 de 6 → de 4 transacciones a 2.
- Combinado: ~8× → bloqueo a ~0.4-0.6 ms, **por debajo de 1 tick de línea.** Casi cero
  riesgo, ganancia enorme.

**NIVEL 1 — cadencia que no pisa el barrido** (`-DDOWN_OTOS_ISR_PACING`): mover el
scheduling de la línea a un **`IntervalTimer` (ISR de hardware a 1 kHz)** que llama
`line_ring_tick()`, y dejar `otos_tick()` en el `loop()` de baja prioridad. La ISR de
línea PREEMPTA al OTOS: si el OTOS está a mitad de su ráfaga I2C cuando vence el tick de
línea, la ISR corre igual (el barrido es solo GPIO+ADC, no toca el bus del OTOS — no hay
conflicto de recurso). El jitter del muestreo pasa de "hasta 4000 µs" a "<1 µs". Patrón
precedente del repo: interrupciones, NO RTOS/threads.

> ⚠️ **Re-entrancia (riesgo medio):** `line_ring_tick` escribe `g_raw` desde la ISR y
> `dm_update` lo lee desde el loop → hay que `volatile`-ar el doble-buffer o snapshotear
> con `noInterrupts()` corto. La calib (bloquea 320 ms) NO debe correr desde la ISR —
> solo en modo admin con la ISR pausada.

**NIVEL 2 — I2C asíncrono/DMA + doble-buffer** (`-DDOWN_OTOS_ASYNC`, post-Incheon):
lanzar la lectura de 18 B por DMA (el LPI2C del Teensy 4.0 lo soporta); al completar, una
ISR deposita el frame en doble-buffer. `otos_tick()` pasa a máquina de estados:
"kick" (arranca y vuelve en µs) + "consume" (cuando el buffer está listo). El loop nunca
espera el bus. Es el espejo del patrón interrupciones+DMA+doble-buffer ya bendecido por
Gustavo para TOP/CENTRAL. Los 2 buses I2C físicos independientes (Wire/Wire1) permiten
leer los 2 OTOS en paralelo real.

> ⚠️ **El I2C async NO lo da el core** (`WireIMXRT.h` no tiene transfer background ni
> `setWireTimeout`). El Nivel 2 es trabajo ARQUITECTÓNICO (shim DMA bajo `sfTkII2C`),
> alto esfuerzo, alto riesgo — por eso post-Incheon. El `WDOG1` (1 s, gateado
> `-DDOWN_ENABLE_WDT`) es la red ante cuelgue de bus.

**Orden recomendado: Nivel 0 primero** (mayor ganancia, menor riesgo, banco corto), luego
Nivel 1, y Nivel 2 solo post-Incheon.

`otos_health` (salud por-OTOS, host-testeada) y `otos_fusion` NO cambian — solo cambia
QUIÉN/CUÁNDO llena los globals de entrada.

---

## 8. Serie por ISR + difusión (pizarra, tasas, frescura)

### 8.1. El RX ya es por ISR (lo del core)

El "serial por ISR" pedido **YA está cumplido** por el core Teensy 4: cada
`HardwareSerial` (Serial1=LPUART6, Serial5=LPUART8) recibe byte a byte en una ISR de UART
y lo deja en un ring (64 B default). `comm_central_tick()` / `comm_top_tick()`
(`main_down.cpp:164-165`) solo DRENAN ese ring en el loop. El parseo (`FrameDecoder`) es
una state machine O(1) por byte. **No escribir ISR propias** — solo agrega bugs. Lo que
falta es hacerlo robusto:

1. **Separar PARSEO de EJECUCIÓN.** El único trabajo largo que hoy puede colarse en el
   path RX es `CENTRAL_CALIB_LINE` (`comm_central.cpp:111`): `calibrate_white` (~320 ms)
   + EEPROM. Cambiar el handler para que setee `g_calib_pending` y RETORNE; el trabajo
   pesado corre FUERA del path RX (robot quieto, modo banco). WCET del tick RX baja de
   ~350 ms a **<5 µs siempre**, incluso si por error llega un 0x21 en vivo.
2. **Colchón RX por paridad:** `Serial1.addMemoryForRead(buf512)` + `Serial5...`,
   guardado con `#if defined(__IMXRT1062__)` (igual que TOP/CENTRAL `comm_down.cpp`). 512
   B = ~22 ms de aire: absorbe el bloqueo del I2C OTOS sin perder un comando.
3. **Límite duro de parseo:** `MAX_RX_BYTES_PER_TICK` (ej. 256) por si un cable ruidoso
   inyecta basura — el `FrameDecoder` ya resincroniza solo (CRC16-CCITT).

### 8.2. La PIZARRA (blackboard doble-buffer) — `sensor_slot.h` [REUSAR, ya existe]

> **CORRECCIÓN 2026-06-16 (verificado):** `src/shared/sensor_slot.h` **SÍ EXISTE** y el TOP
> (`snapshot_emitter.cpp`) ya lo usa. El "Estado real" anterior ("no existe") era un dato VIEJO.
> Por eso F6 NO crea un seqlock nuevo: se entregó `src/down/down_blackboard.h`, un wrapper
> delgado y gateado (`-DDOWN_BLACKBOARD`) que REUSA `SensorSlot<T>` de `sensor_slot.h` para los
> slots del DOWN (`line`/`pose`/`vel`), con su test host (`test_down_blackboard`, 12 tests). Es
> ANDAMIAJE: NO se cablea al loop en esta tanda (ver §8.2 deuda doble-buffer). Así se evita la
> divergencia cross-placa (síndrome coach-fábrica): una sola firma de seqlock para las 3 placas.

Hoy NO hay pizarra: el productor y el difusor están FUSIONADOS en una función síncrona
(`comm_central_send_line_urgent`, `comm_central.cpp:148`): re-lee crudos → `dm_update` →
sella `sample_age_ms` → `down_tx_broadcast_line`. **No hay race hoy** porque productor y
difusor corren en el MISMO hilo cooperativo del `loop()`.

La pizarra propuesta: **4 `SeqSlot<T>` independientes** (1 escritor por slot), puros,
host-testeables, gateados:

```cpp
template<typename T> struct SeqSlot { volatile uint32_t seq; T data; uint32_t stamp_ms; };
// publish: seq++ (impar); DMB; data=v; stamp=now; DMB; seq++ (par)
// read:    do { s=seq; if(s&1) reintento_acotado; DMB; out=data; DMB; } while(seq!=s);
```

Slots: `g_slot_line<LineStatusV2>`, `g_slot_pose<Pose2D>`, `g_slot_vel<Velocity2D>`. El
PRODUCTOR arma el frame y hace `slot_publish` en vez de difundir; el DIFUSOR nuevo
(`down_blackboard_flush()`, al final del loop) hace `slot_read_latest` y difunde solo la
última foto coherente. **Nunca sale un frame a medio armar.**

**Tasas (sin cambio vs contrato §2):** `LineStatusV2` @200 Hz, `Pose2D`/`Velocity2D`
@100 Hz — se conservan poniendo el gate de cadencia en el difusor.

**Frescura:** `sample_age_ms` ya viaja DENTRO del `LineStatusV2` (edad de la MUESTRA
FÍSICA, fix audit 2026-06-03 #2, `comm_central.cpp:173`). El slot agrega `stamp_ms` (edad
del PUBLISH) como 2º sello, para que un futuro consumidor por-ISR distinga "frame viejo en
el slot" de "frame nuevo". Hoy redundante (mismo hilo) pero gratis y a prueba de futuro.

> ⚠️ **Doble-buffer PREMATURO (P2):** hoy no hay race en el hilo cooperativo. El seqlock
> previene un torn-read que **todavía no existe**. Integrarlo al loop ANTES de que el
> productor/RX sea ISR/DMA es complejidad sin beneficio. **Entregar `sensor_slot.h` PURO +
> tests + gate OFF; integrar al loop vivo SOLO en el mismo paso que el barrido pase a
> ADC-DMA/ISR** (igual regla que CENTRAL F5).
>
> ⚠️ **Sin los 2 `DMB` el seqlock es decorativo (P1):** el M7 reordena memoria; el
> compilador puede mover la copia de `data` fuera de la ventana `seq` → torn-read
> silencioso. Las 2 barreras son obligatorias. Bajo el gate OFF se compila como copia
> directa byte-idéntica a hoy. (Precedente: `asm volatile("dsb")` ya vivo en
> `main_central.cpp:84`.)

### 8.3. La difusión (`down_tx`) ya es correcta

`down_tx` dropea con backpressure (`availableForWrite()` antes de escribir; si no entra,
dropea y cuenta — `down_tx.cpp:41`). Para un canal de ESTADO, dropear el frame viejo y
mandar el nuevo es lo correcto (el siguiente trae el estado fresco). Solo falta endurecer
la observabilidad: mirar `down_tx_get_dropped(link)` (ya existe) en banco — hoy no se
mira. No cambiar la lógica de drop.

---

## 9. Qué YA existe vs qué falta

### 9.1. YA existe — puro, host-testeado, se REUSA (honesto: esto es ~80% del trabajo)

| Módulo | Qué hace | Tests |
|---|---|---|
| `line_geometry` (`lg_compute_xy`) | centroide vectorial real con `SENSOR_POS`, line_angle, escape_angle | `test_down_geometry` (20) |
| `line_calib` + `calib_storage` + `eeprom_calib` | umbral por sensor + EEPROM v2 CRC | `test_down_calib` (5), `test_calib_storage` (19) |
| `sensor_health` | ruidoso/stuck/OOB por sensor + healthy_count | `test_sensor_health` (12) |
| `lf_all_white` / `surface_monitor` | saturado / levantado | `test_line_filters` (39), `test_down_surface` (5) |
| `line_filters` (temporal/hysteresis/spatial/MuxWatchdog) | filtros + mux muerto | `test_line_filters` (39) |
| `down_tx` | broadcast SEQ por enlace + backpressure | `test_down_tx` |
| `otos_health` / `otos_fusion` | salud por-OTOS + fusión | (host) |

### 9.2. Falta — lo NUEVO (el ADC rápido y la detección temprana son lo único nuevo de verdad)

| Item | Tipo | Prioridad |
|---|---|---|
| **ADC dual + averaging 1** (Capa 1+2 del §3) | refina `line_ring`, gateado | P1 |
| ~~Detección temprana + LUT vecino-físico + gate del vector~~ (§4) — ✅ **CABLEADO 2026-06-16** (`-DDOWN_EARLY_EVIDENCE`, host-tested; banco pendiente TASK-309) | refina `dm_update`, gateado | P1 |
| **Sellado a sentinela + criterio confiable + piso healthy_count** (§6.4) | refina `dm_update`, gateado | P1 |
| **OTOS Nivel 0** (400 kHz + burst) (§7) | refina `otos`, gateado | P1 |
| `world_model_get_escape_angle()` + consumo ESCAPE (CENTRAL) | nuevo, gateado | P1 (post-Incheon) |
| **`sensor_slot.h`** (pizarra seqlock) (§8.2) | NUEVO puro, no existe | P2 |
| **OTOS Nivel 1** (IntervalTimer línea) (§7) | refina scheduling | P2 |
| **ADC DMA continuo** (Capa 3) (§3) | refina `line_ring` | P2 (2027) |
| **OTOS Nivel 2** (I2C async/DMA) (§7) | nuevo shim DMA | P2 (2027) |
| `LoopMonitor` cableado en DOWN (F0) | nuevo, mide WCET | P1 (prerequisito de banco) |

---

## 10. Plan por fases gateado + plan de banco

Cada fase es un `#define` off-by-default. El binario de competencia NO cambia hasta que
banco lo valida. Orden por relación impacto/riesgo:

| Fase | Qué | Gate | Riesgo |
|---|---|---|---|
| **F0** | `LoopMonitor` + env diag que imprime `g_last_tick_us` (CARD DOWN-4). **MEDIR el "ANTES".** | — | nulo |
| **F1** | ADC averaging 1 + dual-ADC (§3 Capa 1+2) | `-DDOWN_ADC_FAST` | bajo/medio |
| **F2** | OTOS Nivel 0 (400 kHz + `getPosVelAcc`) | `-DDOWN_OTOS_FAST_I2C` | bajo |
| **F3** ✅glue 2026-06-16 | Detección temprana + LUT vecino-físico (§4) — CABLEADO en `dm_update` (unión por vecino físico a `validated[]`, ESTRICTAMENTE ADITIVA), host-tested (`test_down_early_evidence` 3, gate ON/OFF), env `down_earlyev`. Banco pendiente (TASK-309). | `-DDOWN_EARLY_EVIDENCE` | medio |
| **F4** | Sellado a sentinela + criterio confiable (§6.4 A/B/C) | `-DDOWN_RELIABLE_GATE` | bajo/medio |
| **F5** | RX: separar parseo/ejecución + addMemoryForRead (§8.1) | `-DDOWN_RX_HARDEN` | bajo |
| **F6** | OTOS Nivel 1 (IntervalTimer línea) + `sensor_slot.h` (§7-8) | `-DDOWN_OTOS_ISR_PACING` | medio (re-entrancia) |
| **F7** | ADC DMA continuo + OTOS Nivel 2 (post-Incheon/2027) | varios | alto |

### Plan de banco (lo cierra el EQUIPO, no Claude)

1. **F0:** medir `delta micros()` del barrido y `g_last_tick_us` con el anillo real.
   Confirmar/corregir el "ANTES" de 717 µs.
2. **F1:** medir el barrido DESPUÉS (esperado ~126 µs combo seguro). Leer el MISMO mux por
   ADC1 y por ADC2 y comparar el delta crudo carpet/blanco (riesgo offset por-ADC; la
   calib individual lo absorbe porque el umbral se mide con el MISMO ADC del runtime).
   Verificar que averaging=1 no hace flickear el umbral/histéresis.
3. **F2:** medir `delta micros()` alrededor de `otos_tick` antes/después (esperado 3-4 ms
   → <0.6 ms). 30 min de marcha: 0 fallos I2C espurios a 400 kHz (si hay, bajar reloj).
4. **F3:** robot a velocidad conocida cruzando la línea. Medir **penetración_mm al
   instante del aviso temprano vs inminente** + **0 falsos positivos** en marcha normal
   sobre carpet. Re-confirmar que `imminent_depth=6` da el mismo timing de freno duro
   (sintonizado 2026-06-14).
5. **F4:** titular `MIN_SENSORS_FOR_VECTOR` y el piso de `healthy_count` con el robot
   quieto sobre la línea contando `healthy_count` real. Robot levantado a mano + sensor
   tapado a propósito → verificar `data_valid=0` y geometría sellada a N/A.
6. **F5:** inyectar 0x21 en vivo → la cadencia de `LINE_URGENT` a CENTRAL no se
   interrumpe. Saturar RX con basura → 0 comandos perdidos, `resync` sube pero CRC no
   corrompe.
7. **F6:** medir jitter del período de muestreo (`g_last_sample_us`) antes/después →
   verificar que no hay gaps de >1.5 ms. Stress de RX por ISR → 0 campos cruzados de 2
   frames (torn-read).

> **Sin estos números en banco, cada fase queda en backlog.** Claude no cierra ninguna
> TASK de hardware como `done` (regla no negociable #1).

---

## Referencias cruzadas

- Contrato de datos DOWN: `docs/firmware/CONTRATO-DATOS-DOWN.md`
- Convención de ejes: `docs/CONVENCION-EJES-ROBOT.md`
- Precedente de pizarra/reflejos CENTRAL: `docs/firmware/ARQUITECTURA-LAZO-CENTRAL-RT.md`
- Deuda doble-cadena de línea: `FUENTES-DE-VERDAD.md` §deudas + `comm_central.cpp:155-159`
- Cards de banco DOWN: `docs/pruebas-banco/DOWN.md`
