---
title: "Arquitectura sensorial NO BLOQUEANTE del TOP — pizarra productor/consumidor para que ninguna lectura frene el WorldSnapshot a 100 Hz"
date: 2026-06-14
author: "Claude (Anthropic - Claude Opus 4.8 1M)"
status: vivo
tipo: diseno
scope: src/top
---

# Arquitectura sensorial NO BLOQUEANTE del TOP

> **Cómo leer este doc.** Es un **diseño + andamiaje gateado off-by-default**, NO
> una orden de tocar el firmware vivo. Cada pieza nueva vive detrás de un `#ifdef`
> que arranca **apagado**: con los flags sin definir, el binario de Incheon es
> **byte-idéntico** al de hoy. La integración en `main_top.cpp` es **post-Incheon,
> en banco**. La regla no negociable del repo sigue valiendo: el testing en
> hardware real lo cierra el **equipo humano**, no Claude. Acá se separa con
> honestidad lo que ya existe y es puro/host-testeable de lo que falta construir,
> y se marca **dónde el hardware pone un techo que ningún software levanta**.

---

## 1. Esencia en una frase

Hoy `build_snapshot()` corre **en el mismo hilo** (el superloop cooperativo de
`main_top.cpp:272-413`) que las lecturas de sensores que **bloquean** —`pulseIn`
del HC-SR04 (~12 ms, `sensors_tof.cpp:155`), `getRangingData` del ToF (~10-15 ms,
`sensors_tof.cpp:378-379`), reads I2C del BNO (`sensors_imu.cpp:392-394`)— así que
cuando un sensor se demora, el envío del `WorldSnapshot` a CENTRAL **se atrasa con
él**; el rediseño introduce una **PIZARRA** (slots productor/consumidor lock-free)
para que el snapshot se **arme leyendo RAM** y salga a **100 Hz clavados**, con
cada slot marcado **fresco o viejo**, **pase lo que pase con los buses**.

---

## 2. Diagrama de bloques de la pizarra

```
   PRODUCTORES (cada uno a SU tasa, en SU contexto)          SLOTS doble-buffer / seqlock
 ┌───────────────────────────────────────────────┐        ┌──────────────────────────────┐
 │ CÁMARA FRONTAL  Serial3  (RX-IRQ del core)     │ ─────► │ slot CAM_FRONT [seq|data|ts] │
 │ CÁMARA TRASERA  Serial5  (RX-IRQ del core)     │ ─────► │ slot CAM_BACK  [seq|data|ts] │
 │ HC-SR04         GPIO 4/3 (timer + pin-IRQ)     │ ─────► │ slot OBSTACLE  [seq|data|ts] │  ┐
 │ 4 ToF           Wire 18/19  (loop, troceado)   │ ─────► │ slot TOF[0..3] [seq|data|ts] │  ├─ comparten
 │ BNO heading     Wire/Wire2  (loop, SM corta)   │ ─────► │ slot HEADING   [seq|data|ts] │  ┘  el min_obst
 │ OTOS de DOWN    Serial1  (RX-IRQ/DMA + parseo) │ ─────► │ slot POSE/VEL  [seq|data|ts] │
 └───────────────────────────────────────────────┘        └───────────────┬──────────────┘
                                                                           │ read_latest()
                                                                           │ (no bloquea,
                  ╔════════════════════════════════════════════════════╗   │  reintenta si
                  ║   CONSUMIDOR / EMISOR @ 100 Hz                      ║◄──┘  choca escritura)
                  ║   reloj independiente (timer 10 ms o gate de loop) ║
                  ║   1) read_latest de CADA slot  (solo RAM)          ║
                  ║   2) FAIL-SAFE por frescura (slot viejo → sentinel)║
                  ║   3) arma WorldSnapshot (31 B, types.h:98-139)     ║
                  ║   4) TX no bloqueante a CENTRAL (drop si lleno)    ║
                  ╚════════════════════════════════════════════════════╝
                                                                           │
                                                                           ▼
                                                              Serial4 (16/17) → CENTRAL
                                                              (ya dropea si TX lleno,
                                                               comm_central.cpp:81-86)
```

La pizarra **NO crea paralelismo que el hardware no tiene** (sección 6). Lo que hace
es sacar la latencia variable de los buses **fuera del camino crítico** del snapshot:
el consumidor sólo copia RAM y compara timestamps; nunca dispara una transacción de bus.

---

## 3. El contrato de la pizarra (slot + seq + frescura, API pura)

El corazón es un **slot genérico** lock-free, **single-writer / N-readers**, que vive
en un header **PURO** (sin `Arduino.h`) para que entre al gate host-native (los **858
tests / 61 suites** del repo, medidos 2026-06-14). Archivo objetivo:
`src/shared/blackboard_slot.h` (**hoy NO existe** — verificado por grep; este doc lo
especifica).

### 3.1 Por qué SEQLOCK y no mutex

En el Cortex-M7 **single-core** del Teensy 4.0 no hay preempción de tareas (no hay
RTOS — confirmado: cero `TeensyThreads`/`FreeRTOS` en `src/` y `lib/`). La única
concurrencia posible es **una ISR interrumpe al loop** (nunca al revés). Un seqlock
cubre exactamente ese caso **sin deshabilitar interrupciones** y **sin que el
escritor (ISR) espere nunca al lector** → cero inversión de prioridad, requisito duro
para correr en ISR.

### 3.2 La estructura y las dos operaciones

```cpp
template <typename T>
struct BlackboardSlot {
    volatile uint32_t seq;       // PAR = estable; IMPAR = escritura en curso
    T                 data;      // payload POD del sensor (copiable por valor)
    volatile uint32_t stamp_ms;  // millis() de la última publicación (frescura)
    volatile bool     ever_pub;  // ¿hubo al menos una publicación?
};

// PRODUCTOR (ISR o loop). No bloquea, no espera:
template <typename T>
inline void bb_publish(BlackboardSlot<T>& s, const T& v, uint32_t now_ms) {
    s.seq = s.seq + 1;     // → IMPAR ("estoy escribiendo")
    bb_barrier();          // el ++seq se ve ANTES del cuerpo
    s.data = v; s.stamp_ms = now_ms; s.ever_pub = true;
    bb_barrier();          // el cuerpo se ve ANTES del 2.º ++seq
    s.seq = s.seq + 1;     // → PAR ("snapshot consistente, versión N")
}

// CONSUMIDOR (loop o ISR del emisor). Reintenta si chocó una escritura:
template <typename T>
inline bool bb_read_latest(const BlackboardSlot<T>& s, T& out, uint32_t now_ms,
                           uint32_t fresh_ms, uint32_t max_retries = 4) {
    for (uint32_t k = 0; k <= max_retries; ++k) {
        const uint32_t s0 = s.seq;
        if (s0 & 1u) { bb_relax(); continue; }   // IMPAR → escritura en curso
        bb_barrier();
        out = s.data;
        const uint32_t stamp = s.stamp_ms; const bool ever = s.ever_pub;
        bb_barrier();
        if (s.seq == s0) {                        // sin escritura en el medio
            if (!ever) return false;                          // nunca se publicó
            if ((now_ms - stamp) > fresh_ms) return false;    // VIEJO
            return true;                                      // fresco y consistente
        }
        // chocó: reintenta con la nueva versión
    }
    return false;   // techo de reintentos → trata como NO-disponible, NUNCA bloquea
}
```

- **`bb_barrier()`**: en firmware = `asm volatile("dmb":::"memory")` (precedente: el
  `asm volatile("dsb")` del watchdog en `main_top.cpp:95`); en host-test =
  `std::atomic_signal_fence(seq_cst)`. Se selecciona por `#ifdef` → el header queda puro.
- **Frescura wrap-safe**: la resta unsigned `(now_ms - stamp_ms)` es robusta ante el
  wrap de `millis()` a ~49,7 días — **idéntico patrón** al ya probado en
  `tof_fresh_or_no_reading()` (`sensors_tof.h:62`, función pura host-testeada).
- **Anti torn-read**: el lector sólo acepta la copia si `seq` es **PAR al entrar Y la
  misma versión al salir**. Si el productor publicó en el medio, descarta y reintenta.
  Nunca entrega medio-struct.

### 3.3 Invariante NO negociable

**UN solo escritor por slot.** Si dos ISR pudieran publicar el mismo slot, dos `++seq`
concurrentes corrompen el seqlock en silencio (versión PAR con data rota). El límite
físico del bus I2C único (`Wire` 18/19: BNO + 4 ToF) **refuerza esto naturalmente**:
esos sensores se serializan por una sola máquina de estados de bus → un solo agente
publica esos slots, no cuatro ISR peleando.

---

## 4. Los 6 productores

> **Convención de la tabla.** "ISR-safety" = qué corre realmente en contexto de
> interrupción. "Costo actual" cita el archivo:línea del bloqueo VIVO de hoy.

| # | Productor | Mecanismo propuesto | ISR-safety | Qué publica | Costo actual (VIVO) |
|---|-----------|---------------------|------------|-------------|---------------------|
| 1 | **Cámaras** frontal (Serial3) + trasera (Serial5) | **M1 recomendado**: parseo en el **RX-IRQ** del core (sin DMA). El RX de las LPUART **ya es interrupt-driven**; se engancha `feed()` por byte (parser puro) y al cerrar frame CRC8-OK se publica. DMA (M2) sólo si el banco muestra pérdida de frames. | Drenar FIFO + `CameraParser::feed()` + publicar + sello de ticks. **Sin** `millis()`/`Serial.print`/`malloc`/`float`. A 19200 baud ≈ 2 B/ms → ISR trivialmente corto. | slot CAM_FRONT / CAM_BACK: 9 campos (`ball_x/y`, `goal_yellow_x/y`, `goal_blue_x/y` + 3 flags visible) + ts | Drain en el loop con cota `MAX_BYTES_PER_TICK=64` (`cameras_runtime.cpp:182-198`); ring 256 B (`:156-159`). **NO bloquea hoy**, pero un stall del loop puede **desbordar el ring en silencio** y perder frames. |
| 2 | **HC-SR04** (TRIG=4, ECHO=3, GPIO puro) | FSM de 3 estados por **2 interrupciones**: `IntervalTimer` dispara TRIG cada ~60 ms; `attachInterrupt(ECHO, CHANGE)` cronometra el eco por flancos con `micros()`. **El candidato más fácil** — es GPIO, no toca ningún bus. | Captura de flanco (`micros()` → var volátil) + cálculo entero del ancho + publicar. Único "bloqueo": `delayMicroseconds(10)` del pulso TRIG (**1200× menor** que el `pulseIn` de hoy). **Sin** I2C. | slot OBSTACLE (parte HC-SR04): `dist_mm` + ts. **Mejora gratis**: hoy `g_hcsr04_mm` NO tiene frescura. | `pulseIn(ECHO, HIGH, 12000)` = **bloqueo de 0 a 12 ms** (`sensors_tof.cpp:155`), cada ~90 ms (`:414`). El comentario del WDT lo nombra como el stall que **obligó** al timeout de 1 s (`main_top.cpp:66-73`). |
| 3 | **BNO055** heading | **SM I2C registro-a-registro** sobre `Wire` síncrono (NO DMA: el payload son 6+6+1 B, el DMA no amortiza). Una transacción corta por pasada del loop; reparte el costo. **NO corre en ISR** (una transacción `Wire` en ISR cuelga). | Fase-1: **nada en ISR** — cooperativo en el loop; peor atraso = 1 transacción (~0,5 ms). Publica a la pizarra al cerrar un ciclo **COMPLETO** (estado DONE), no a media fase. | slot HEADING: `heading_deg`, `gyro_z_dps`, `valid` (=`fused_valid`), ts, seq | 3 reads I2C bloqueantes/tick (`sensors_imu.cpp:392-394`, ~1,5 ms pegados), inline en el loop (`main_top.cpp:318-321`). Band-aid 20 Hz (`sensors_imu.cpp:119`) por contención con ToF. |
| 4 | **4 ToF VL53L7CX** (round-robin, `Wire`) | (1) **reordenar** el loop para que el envío vaya PRIMERO; (2) opcional: **trocear** `getRangingData()` en chunks de ~1-2 ms (`-DTOP_TOF_CHUNKED`); (3) guarda de cadencia. **NO ISR** (I2C largo en bus compartido). | Cooperativo en el loop. La pizarra (`g_distances_mm`/`g_zones_mm`) se escribe/lee sólo desde el loop → single-threaded, sin sección crítica. Si se trocea, copia atómica al cerrar el frame. | slot TOF[i]: `dist_mm`, `zones[16]`, `last_ok_ms`, `ever_ok` (la pizarra **ya existe** como arrays module-static, `sensors_tof.cpp:42-49`). | `getRangingData()` ~10-15 ms (payload recortado; ~60 ms full), `sensors_tof.cpp:378-379`. Round-robin 1/tick evita los 4 juntos (~160 ms) pero **ese read cae antes del send** → jitter de ~10-15 ms. |
| 5 | **OTOS de DOWN** (odometría, Serial1) | **RX-IRQ + ring ampliado** (o DMA circular) + parseo diferido con el **mismo `FrameDecoder`** ya testeado. **Fallback recomendado**: `serialEvent1()` (lo llama el core tras `yield()`, **no en ISR dura**) → drena el ring sin restricciones. | Si va por timer/ISR: drain + `feed()` + publicar, sin I2C/`Serial.print`. Si va por `serialEvent1()`: contexto no-ISR, swap de índice atómico basta (sin seqlock). | slot POSE / VEL: `Pose2D` + `Velocity2D`, cada uno con ts. Accesores `comm_down_get_pose()`/`is_pose_fresh()` se re-implementan sobre la pizarra → `main_top.cpp` no cambia. | El parseo vive en el loop (`comm_down.cpp:100-112`); si un sensor bloqueante reina, **nadie drena Serial1** y el ring (64 B + 512 B extra, `:88-94` ≈ ~22 ms de aire a 230400) **desborda en silencio** → odometría perdida sin LOST. |
| 6 | **CONSUMIDOR/EMISOR** @100 Hz | Ver sección 5 — es el único **consumidor**, listado acá para cerrar el inventario de las 6 piezas de la pizarra. | — | Arma el `WorldSnapshot` y lo manda. | Emit en el loop (`main_top.cpp:338-342`); a horario hoy, pero **comparte hilo** con los reads bloqueantes. |

### Nota de honestidad sobre las cámaras (corrección de premisa, load-bearing)

El equipo decidió "interrupciones + DMA + doble-buffer". Para el **RX de cámaras** el
DMA es **en parte redundante**: el core **ya** drena el UART por IRQ a un ring; el
`while(available())` de hoy sólo vacía lo que el hardware **ya** puso, no espera al
cable. El cuello real **no** es que el RX bloquee, sino que si el loop se **demora**,
el ring desborde en silencio. Por eso **M1** (parseo en el RX-IRQ + pizarra) entrega
el ~90 % del beneficio —desacoplar la publicación del loop— con una **fracción del
riesgo** del DMA (canales eDMA, DMAMUX, coherencia de caché del M7). El DMA "mueve la
aguja" donde el bus **sí** bloquea de verdad: I2C de ToF/BNO — y ahí tampoco se puede
hacer en ISR (sección 6).

---

## 5. El consumidor/emisor @100 Hz + fail-safe por frescura

### 5.1 Reloj independiente

Hoy el emit vive **dentro** del superloop (`main_top.cpp:338-342`). Sale a ~10 ms,
pero comparte el hilo con los reads I2C: cuando el loop cayó a 6 Hz por los 4 ToF
juntos, el snapshot se atrasó con él. El rediseño **separa el reloj del emit de la
cadencia de los sensores**:

- **Opción A (recomendada)** — `IntervalTimer` @100 Hz (10000 µs). En la ISR del timer:
  `read_latest` de cada slot (solo RAM) → fail-safe de frescura → `proto_encode` →
  TX no bloqueante. La emisión queda **clavada a 100 Hz aunque el loop esté trabado
  leyendo un ToF**, porque el timer interrumpe el loop. **Clave**: la ISR del emit
  **sólo toca RAM**, nunca I2C/`pulseIn` → WCET acotado, sin I2C reentrante.
- **Opción B (mínimo cambio, menor garantía)** — mantener el emit en el loop con el
  gate `elapsedMillis` de hoy. Da jitter si el loop se atasca >10 ms. Es el fallback
  host-testeable.

### 5.2 Sobre el "DMA TX"

`Serial4.write(buf,n)` del core **no** es polling-bloqueante: copia al ring TX (64 B)
servido por la ISR del LPUART, y `availableForWrite()` reporta el espacio libre. Para
38 B a 230400 baud (~1,65 ms/frame) **eso ya es TX no bloqueante real, sin DMA**. El
precedente VIVO es `comm_central.cpp:81-86` (drop si `availableForWrite < n`). **Veredicto
honesto**: el "DMA TX" que pidió el equipo **se realiza con la cola TX interrupt-driven
del core + backpressure**, no montando `DMAChannel` para 38 B (más superficie de bug que
beneficio). Si el banco midiera que la ISR del LPUART roba demasiado, **entonces** DMA —
pero medir primero.

### 5.3 Fail-safe por frescura (el corazón del pedido)

El emit **evalúa** la frescura pero **no inventa** dato: cuando un slot está rancio,
**degrada a su sentinel**, no disfraza lo viejo de fresco. Por cantidad:

| Slot viejo | Umbral orientativo | Degradación |
|------------|--------------------|-------------|
| POSE | >~80 ms (~8 frames) | `my_pose_confidence=0`, x/y→0 (CENTRAL ya ignora pose con conf=0) |
| PELOTA | >~200 ms (= expiración de `ball_velocity`) | `ball_visible=0` (NO mandar la última posición como pelota fantasma) |
| ARCO | timeout de cámara | `goal_*_visible=0` (sentinel ya definido, `types.h:118-123`) |
| OBSTÁCULO | `TOF_STALE_TIMEOUT_MS` | `min_obstacle_mm=0xFFFF` (`TOF_NO_READING` = "libre", `types.h:126`) |
| HEADING | >~3× su período | baja `flags` bit4 `heading_valid` |
| ÁRBITRO | baja tasa, latcheado | mantener último comando (documentar) |

**Por qué 100 Hz clavados sin esperar a ningún sensor**: CENTRAL consume el snapshot
para su FSM; si el TOP deja de emitir (porque esperó a un ToF trabado), CENTRAL entra
en fail-safe y **frena el robot**. Un snapshot **a horario con un slot marcado "viejo"
es infinitamente mejor** que un snapshot tarde con todos los slots frescos: CENTRAL
puede decidir con datos parciales (sigue navegando con heading aunque la pose esté
vieja), pero no puede decidir con **nada**.

### 5.4 Pendiente wire-breaking (fuera de este alcance, se nombra)

Para que CENTRAL pueda **gatear por staleness del lado receptor**, el `WorldSnapshot`
debería llevar un `sample_age_ms` global (precedente: `LineStatusV2.sample_age_ms`,
`types.h:153`). Hoy el snapshot **NO lleva timestamp** (verificado: `struct
WorldSnapshot`, `types.h:98-139`, 31 B, sin campo de tiempo). Eso es **schema bump v4**
→ re-flashear TOP + CENTRAL **juntos** — encadenado con los campos de velocidad propia.
No está en este diseño.

---

## 6. Presupuesto de tiempo realista (qué solapa de verdad vs qué serializa el bus)

> Esta es la sección más importante para no gastar esfuerzo donde el hardware no deja.

### 6.1 Lo que SÍ solapa en paralelo real (periféricos distintos)

| Recurso | Periférico | Solapa con |
|---------|-----------|------------|
| Cámara frontal | LPUART (Serial3) | todo lo demás |
| Cámara trasera | LPUART (Serial5) | todo lo demás |
| OTOS de DOWN | LPUART (Serial1) | todo lo demás |
| TX a CENTRAL | LPUART (Serial4) | todo lo demás |
| HC-SR04 | GPIO 4/3 | **todo** (no toca ningún bus) |

Estos **corren en paralelo físico** porque son periféricos independientes con su
propia ISR de hardware. El DMA/RX-IRQ acá compra **robustez ante stalls**, no
throughput (a 19200-230400 baud el costo por byte es despreciable en el M7 @600 MHz).

### 6.2 Lo que SERIALIZA — el límite físico del bus I2C único

**Dos lecturas del MISMO bus I2C NO pueden solaparse: hay un solo par SDA/SCL.**

- **`Wire` (18/19)**: BNO secundario + **4 ToF** comparten este bus. El paralelismo
  real entre BNO y ToF es **CERO** — se turnan. Ningún DMA/ISR/seqlock lo cambia.
  - El bus está forzado a **100 kHz** (no 400 kHz) en runtime porque a más velocidad
    el read multi-byte del BNO se corrompe cuando los ToF rangean → **yaw congelado**
    (banco 2026-06-02/06-08; `sensors_tof.cpp:125,255`).
  - Por eso existe el **deconflict temporal** (`main_top.cpp:307-321`): el BNO sólo se
    lee si pasaron ≥ `TOP_BNO_TOF_GAP_MS` (8 ms) desde el último read de ToF. **Es
    load-bearing** — sacarlo recongela el yaw.
- **`Wire2` (24/25)**: en ROBOT2 el BNO **primario** vive **solo** acá, sin ToF →
  **aislado**. Acá sí se puede leer a 100 Hz sin contención (lo habilita `TOP_BNO_FAST`
  + `TOP_BNO_PRIMARY_ONLY`, `sensors_imu.cpp:113-117`). **Este es el único paralelismo
  I2C real que tiene el robot.**

**Consecuencia para el diseño**: la pizarra **desacopla el snapshot del bus, no
serializa menos el bus**. El throughput del BNO+ToF sigue topado por turnarse en
`Wire`. Lo que se gana es que ese turno ocurra **fuera del camino crítico** del
snapshot de 100 Hz: el loop publica/consume RAM mientras el bus avanza a su ritmo.

### 6.3 Veredicto de mecanismos del core (auditoría)

| Mecanismo | Veredicto | Nota |
|-----------|-----------|------|
| UART RX-IRQ + `addMemoryForRead` | **VIABLE YA** | El core llena el ring por ISR; `addMemoryForRead` (ya en uso, `comm_down.cpp:94`, `cameras_runtime.cpp:158`) lo amplía. Es la palanca, no el DMA. |
| `IntervalTimer` (4 PIT) | **VIABLE, nativo** | HOY **0 usos** en `src/` (verificado por grep) → este sería el **primero**, PIT libre. |
| `attachInterrupt` por flanco | **VIABLE, nativo** | Todos los GPIO del i.MX RT1062 soportan IRQ por flanco; latencia ~decenas de ns. |
| I2C async/DMA | **LÍMITE DURO** | `WireIMXRT` del core es **bloqueante**, sin API async ni `setWireTimeout()` (`main_top.cpp:75`). DMA-I2C no está expuesto. Opciones reales: trocear / round-robin / aceptar el slot. |
| `TeensyThreads`/`FreeRTOS` | **NO VIABLE** | No vendoreado (cero refs en `src/`+`lib/`); agregarlo dispara la descarga del registry que Avast bloquea (TASK-025). Además **no paraleliza un bus físico** → no resuelve el cuello. |
| DMA RX serial | **POSIBLE, no necesario** | Con el ring ampliado el RX ya no bloquea; DMA es trabajoso y sólo compra robustez extra ante stalls muy largos. |

---

## 7. Plan de implementación POR FASES (todo gateado off-by-default)

> **Regla maestra**: módulos PUROS host-testeables **primero**; integración en
> `main_top.cpp` **al final** y **en banco post-Incheon**. Con todos los flags
> apagados, el binario de competencia es **byte-idéntico** al de hoy.

### Fase 0 — El contrato puro (sin tocar firmware vivo)

1. Crear `src/shared/blackboard_slot.h` (sección 3): `BlackboardSlot<T>`,
   `bb_publish`, `bb_read_latest`, `bb_barrier`/`bb_relax` con `#ifdef` host/firmware.
2. Tests host (`test/test_blackboard_slot/`): publicar/leer, frescura, wrap de
   `millis()`, anti torn-read (un "escritor" que deja `seq` impar → el lector reintenta),
   techo de reintentos. **Entra al gate de los 858 tests.**

**Salida**: el contrato existe y está testeado en host. Cero cambios al binario.

### Fase 1 — Productores puros host-testeables (andamiaje, sin cablear)

Para cada productor que tenga lógica de FSM, extraer la **parte pura** a `src/shared/`
con su test host, **sin** conectarla al firmware todavía:

- `cam_blackboard.{h,cpp}` — alimenta bytes a `feed()` y verifica que un frame CRC8-OK
  publica y uno corrupto NO. (El parser `cameras.cpp` ya es puro — verificado: incluye
  solo `cameras.h`.)
- `bno_read_sm.{h,cpp}` — FSM de fases `{IDLE, REQ_EULER, WAIT_EULER, ... , DONE}`,
  pura, testeable igual que `imu_fusion`/`imu_freeze` lo son hoy.
- `hcsr04_async.{h,cpp}` — la lógica de estados + validación de ancho de eco, pura
  (la captura de flanco real es la única parte Arduino-only).

**Salida**: la inteligencia de cada productor está host-verificada antes de tocar una
sola ISR. Cero cambios al binario.

### Fase 2 — Andamiaje Arduino gateado, productor por productor (compila, OFF por default)

Cablear cada productor a la pizarra detrás de su flag, en **orden de relación
costo/beneficio** (del más fácil al más riesgoso):

1. `-DTOP_HCSR04_ASYNC` — el más fácil (GPIO puro, no toca ningún bus). Mata el peor
   bloqueo individual (`pulseIn` 12 ms).
2. `-DTOP_CAM_RX_ISR` — parseo en RX-IRQ (M1). Desacopla la publicación de cámaras del
   loop.
3. `-DTOP_TOF_REORDER` — envío antes del tick de ToF (la pieza 1 del ToF: ~80 % del
   beneficio, ~5 % del riesgo).
4. `-DTOP_BNO_PRODUCER_SM` — SM I2C del BNO (respeta el deconflict del bus).
5. `-DTOP_OTOS_DRAIN_DEFERRED` — `serialEvent1()` para el OTOS (fallback de menor riesgo).
6. `-DTOP_TOF_CHUNKED` — trocear `getRangingData` (alto esfuerzo / alto riesgo de
   regresión del BNO — **último**).

Con cada flag **sin definir**, ese módulo compila y corre **exactamente como hoy**.

### Fase 3 — El emisor por timer (gateado)

7. `-DTOP_ENABLE_SNAPSHOT_TIMER` — mover el emit a `IntervalTimer` @100 Hz leyendo la
   pizarra (sección 5). **Depende de que la pizarra + productores estén cableados**;
   sin ellos cae al fallback Opción B (emit en el loop, como hoy).

### Fase 4 — Integración en `main_top.cpp` vivo (POST-Incheon, EN BANCO)

Recién acá `build_snapshot()` deja de llamar getters que tocan bus y pasa a leer la
pizarra. **Esto NO se hace antes de Incheon** y **NO lo cierra Claude** — requiere
banco real (regla no negociable #1 del repo).

---

## 8. Riesgos + mitigaciones

| # | Riesgo | Mitigación |
|---|--------|------------|
| R1 | **Falso sentido de paralelismo**: el equipo asume que "ahora todo corre en paralelo" y sube la cadencia del BNO o lo re-acopla a los ToF → recongela el yaw (semanas de banco perdidas). | La pizarra **desacopla el snapshot del bus, no serializa menos el bus** (sección 6). La SM del BNO **hereda el deconflict temporal** (`main_top.cpp:307-321`); en `Wire` compartido NO corre más rápido que el band-aid de 20 Hz. |
| R2 | **Seqlock/barreras mal puestas** → torn-read que **pasa los 858 tests host pero falla SOLO en hardware** (el host single-thread "anda" aunque falten los DMB; el M7 reordena stores). | Test host valida la **lógica** (reintento, frescura, wrap). El **ordering real** se valida **en banco**: un `IntervalTimer` escribiendo a alta tasa mientras el loop lee, con un campo redundante/checksum dentro del struct que detecte inconsistencia. |
| R3 | **Single-writer no chequeado por el compilador**: si por error dos ISR publican el mismo slot, el seqlock se corrompe en silencio. | `assert` de debug (owner-ID por slot) + disciplina documentada. El bus I2C único refuerza el invariante (un solo agente publica BNO+ToF). |
| R4 | **Frescura mal calibrada por sensor**: `fresh_ms` muy corto borra datos buenos (1 frame perdido del ToF a 15 Hz es normal); muy largo propaga un sensor colgado como fresco. | Cada slot **hereda el timeout ya titrado de su sensor** (`TOF_STALE_TIMEOUT_MS`, `CAMERA_TIMEOUT_MS`, `DOWN_HEARTBEAT_TIMEOUT_MS`), nunca un valor global. |
| R5 | **WCET de la ISR del emit no medido**: el presupuesto (read_latest×6 + `proto_encode` + write) es µs **estimados**, no medidos en el Teensy real. | Pre-requisito de banco: instrumentar el WCET con el loop cargado (4 ToF + HC-SR04 + 2 BNO + 2 cámaras). Criterio: WCET << 10 ms con margen enorme (debería ser <50 µs). `loop_monitor.h` ya existe. |
| R6 | **DMA + coherencia de caché del M7** (sólo si se hace M2/DMA): el D-cache puede devolver datos viejos de lo que escribió el DMA. | Ubicar el buffer DMA en `DMAMEM` (región no cacheable) o `arm_dcache_delete` antes de leer. **M1 no tiene este problema** (no usa DMA). |
| R7 | **`onReceive()` puede no existir** en la versión de Teensyduino instalada → habría que override de `lpuartN_status_isr` (más invasivo, hay que conservar el clear de flags de overrun/framing del core). | Verificar la versión del core **antes** de elegir la vía de enganche del RX-IRQ. |
| R8 | **El WDT no debe enmascarar un loop muerto**: si el emit corre en ISR/timer, **no** alimentar el WDOG1 desde ahí. | El `watchdog_feed()` queda **solo en el loop** (`main_top.cpp:279`). Si el loop muere, el robot **debe** resetear aunque la ISR siga emitiendo snapshots viejos. |
| R9 | **ROBOT1 hereda código medido en ROBOT2** (round-robin ToF, deconflict BNO) sin re-verificar en su banco (`sensors_tof.cpp:373`). | Re-validar en el banco de ROBOT1 antes de confiar la cadencia. |
| R10 | **Activar cualquier flag sin banco** cambia el modelo de concurrencia de un módulo que alimenta a CENTRAL. | Off-by-default + integración en banco. **Claude NO marca ninguna TASK como done.** |

---

## 9. Plan de prueba en banco

> **Quién cierra**: el equipo humano con hardware real. Claude **no** cierra ninguna
> de estas TASKs (regla no negociable #1).

**Setup**: ROBOT2 (o ROBOT1 re-verificado) con los 4 ToF + HC-SR04 + 2 BNO + 2 cámaras
activos, CENTRAL conectada drenando el snapshot, monitor USB del TOP abierto.

**T1 — Contrato puro (host, lo puede correr Claude)**
- Correr `scripts/run-host-tests.sh`. Criterio: los nuevos `test_blackboard_slot` +
  productores puros pasan, total de tests **sube** sin romper los 858 existentes.

**T2 — HC-SR04 async (banco)**
- Medir distancia conocida (30/50/80/100/150 cm) con `-DTOP_HCSR04_ASYNC` ON vs el
  `pulseIn` de hoy. Criterio: error ≤ ±2 cm, **0 cuelgues**, y el panel `[TOP]` muestra
  el loop **sin** el escalón de 12 ms al disparar.

**T3 — Cámaras RX-IRQ (banco)**
- Con `-DTOP_CAM_RX_ISR` ON: `pkts_F/B` crecen, `resync`/`crc` **no explotan**
  (`main_top.cpp:411`). Inyectar un stall del loop (p.ej. un `delay` de prueba) y
  verificar que **ya NO se pierden frames** (vs el desborde silencioso del ring de hoy).

**T4 — BNO sin recongelar (banco) — el más crítico**
- Con la SM del BNO ON y los 4 ToF activos, **girar el robot a mano**. Criterio: el
  yaw **sigue el giro** (no se clava), `hdg` en el panel cambia suave. **0 freezes**
  en 5 min. Si se congela → el deconflict temporal se rompió, revertir.

**T5 — Snapshot a 100 Hz bajo carga (banco)**
- Con el emisor por timer ON, medir del **lado CENTRAL** que `snap_fresh=Y` estable y
  la tasa de snapshot **no cae** aunque el loop del TOP esté cargado. `g_frames_tx_dropped`
  (`comm_central.cpp:93`) ≈ 0 con CENTRAL drenando. **0 reentradas** del timer.

**T6 — Frescura / fail-safe (banco)**
- Desconectar en caliente un sensor (un ToF, una cámara) y verificar que su slot
  **expira a sentinel** (min_obst→0xFFFF, ball_visible→0) **sin** frenar el snapshot
  ni contaminar los otros slots.

**T7 — WDT (banco)**
- 30 min de marcha normal: **0 resets espurios**. Colgar el I2C (desconectar un sensor)
  → **auto-reset** por WDT (`WDOG1_WRSR` indica reset por watchdog). El emit por timer
  **no** debe enmascarar esto.

---

## Cierre

Este diseño no inventa paralelismo: lo que el hardware da (UARTs y GPIO en paralelo,
`Wire2` aislado) se aprovecha; lo que el hardware **no** da (un solo `Wire` para BNO +
4 ToF) se respeta y se documenta. El valor central de la pizarra es **sacar la latencia
variable de los buses fuera del camino crítico del snapshot de 100 Hz** y darle al
consumidor una **frescura honesta** por slot, sin que ninguna lectura pueda volver a
atrasar el envío a CENTRAL. Todo gateado, puro-primero, integración en banco
post-Incheon.
