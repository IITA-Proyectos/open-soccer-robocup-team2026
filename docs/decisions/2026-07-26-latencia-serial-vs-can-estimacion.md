---
title: "Latencia peor-caso DOWN→TOP→CENTRAL: serial (UART) actual vs CAN — estimación"
date: 2026-07-26
author: "Claude (Anthropic - Claude Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M, Anthropic)"
status: propuesta
tags: [comunicacion, latencia, can, uart, serial, tiempo-real, wcet, analisis, ambos]
robot: ambos
area: comunicacion
tipo: analisis
related: [docs/decisions/2026-06-03-bus-can-general-y-flasheo-por-can.md, docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md, docs/MAPA-DE-DATOS.md]
---

# Latencia peor-caso DOWN→TOP→CENTRAL — serial actual vs CAN

> ⚠️ **BANNER — qué es y qué NO es este documento.**
> Es una **estimación analítica** construida sobre **constantes reales del
> firmware** (verificadas archivo:línea, ver §1), **NO una medición de banco**.
> Los números de cadencia, baud y tamaño de mensaje son **hechos del código**;
> los tiempos de *procesamiento* y el *WCET del loop* son **estimaciones** —
> están marcados como tales. **Nada de esto está medido en hardware**: el plan
> para medirlo está en §7. Compañero de
> [`2026-06-03-bus-can-general-y-flasheo-por-can.md`](2026-06-03-bus-can-general-y-flasheo-por-can.md)
> (propuesta de bus), que sigue siendo **propuesta no adoptada**.

## 0. Respuesta corta (el titular, que sorprende)

Sobre el camino que preguntó Gustavo — *dato se lee en DOWN → se manda al TOP →
TOP procesa → lo manda a CENTRAL*:

| Escenario | Peor caso estimado | vs hoy |
|---|---|---|
| **HOY (UART, con relay por TOP)** | **≈ 41 ms** | — |
| CAN, mismos gates, dato que **necesita** al TOP | ≈ 38 ms | **−6 %** |
| CAN, mismos gates, dato que **NO** necesita al TOP (sin relay) | ≈ 25 ms | −39 % |
| CAN + publicación por **evento** | ≈ 15 ms | −63 % |
| CAN + evento + CENTRAL consume por evento | ≈ 5 ms | −88 % |

**El titular:** si el dato de verdad tiene que pasar por el TOP, **cambiar UART
por CAN mejora ~6 %** (41 → 38 ms). Casi nada. Porque **el cable no es el
problema**:

```
Descomposición del peor caso de HOY (≈41 ms):
  Gates de reloj (esperas)   ██████████████████████████████  30.0 ms   73%  ← el problema
  Adquisición I2C del OTOS   ████                             4.0 ms   10%
  Proceso/parse (3 placas)   ████                             4.0 ms   10%
  Tiempo EN CABLE            ███                              2.9 ms    7%  ← lo único que CAN ataca
```

CAN ataca ese **7 %** (lo baja a ~1,6 %) y **elimina el relay** cuando el salto
por TOP es innecesario. **No toca los gates, que son el 73 %.**

> **Consecuencia honesta:** la mayor parte de la mejora de latencia **no requiere
> CAN**. Requiere (a) sacar el relay y (b) publicar por evento en vez de por
> gate. Eso se puede hacer **hoy, sobre el UART que ya existe**. CAN se justifica
> por **escala de pines, robustez a EMI y determinismo** (ver el doc del bus),
> **no por latencia**. Coherente con el análisis previo: *CAN no es más rápido*.

---

## 1. Parámetros de entrada (verificados contra el firmware)

Extraídos y **verificados adversarialmente** del árbol vivo
`software/teensy/Soccer 2026/src/` (commit `9a56923`).

### 1.1 Transporte
| Parámetro | Valor | Fuente |
|---|---|---|
| Baud **todos** los enlaces inter-placa | **230400** 8N1 | `down/config_down.h:128`, `top/pinout_common.h:55`, `top/comm_central.cpp:26`, `central/comm_top.cpp:27` |
| Tiempo por byte | 10 bits/B → **43,4 µs/B** | 10/230400 |
| Overhead de frame `proto.h` | **7 B** (START+LEN+TYPE+SEQ+CRC16+END) | `shared/proto.h:77` |
| ACK / retransmisión | **NO HAY** (best-effort, CRC16+SEQ solo detectan) | `proto.cpp:109-111` |
| Backpressure TX | **drop-on-full**, no bloquea | `down_tx.cpp:39-47`, `top/comm_central.cpp:81-86` |

### 1.2 Mensajes (payload → cable → tiempo)
| Mensaje | Payload | En cable | Tiempo @230400 | Fuente |
|---|---|---|---|---|
| `Pose2D` (0x11) | 7 B | 14 B | **0,61 ms** | `types.h:23-28` |
| `Velocity2D` (0x12) | 7 B | 14 B | **0,61 ms** | `types.h:35-40` |
| `LineStatusV2` (0x10) | 16 B *(static_assert)* | 23 B | **1,00 ms** | `types.h:156` |
| `WorldSnapshot` (0x60) | **31 B** *(static_assert, v3)* | 38 B | **1,65 ms** | `types.h:139` |

### 1.3 Cadencias — **todas fixed-rate por gate, CERO event-driven**
Este es el dato que domina todo el análisis: **ningún hop envía "apenas hay
dato"**; todos esperan a que venza un gate de reloj.

| Gate | Período | Fuente |
|---|---|---|
| DOWN: muestreo de línea | 1 kHz (1 ms) | `config_down.h:138` |
| DOWN → CENTRAL+TOP: línea (`LINE_URGENT`) | **200 Hz (5 ms)** | `main_down.cpp:45,206` |
| DOWN: `otos_tick` (I2C **bloqueante**) | 100 Hz (10 ms) | `config_down.h:139` |
| DOWN → TOP: pose+vel | **100 Hz (10 ms)** | `config_down.h:140`, `main_down.cpp:221` |
| TOP → CENTRAL: `WorldSnapshot` | **100 Hz (10 ms)**, por **ISR** `IntervalTimer` | `snapshot_emitter.cpp:158` |
| CENTRAL: tick de strategy → motores | **100 Hz (≥10 ms, con jitter)** | `main_central.cpp:418` |

### 1.4 Modelo de ejecución
- Las **3 placas** son **superloop free-running** (sin RTOS), auto-gateadas con
  `elapsedMillis`/`elapsedMicros`. El gate de CENTRAL es **`>=10 ms`**, o sea
  período **no garantizado**: si una vuelta se alarga, el tick corre más tarde.
- **Determinante #1 del jitter:** en el binario de competencia (`[env:down]`, sin
  `-DDOWN_OTOS_FAST_I2C`) el `otos_tick` hace **4 transacciones I²C secuenciales
  a 100 kHz** ≈ **3–4 ms bloqueantes cada 10 ms** (`otos.cpp:125-135`). Roba
  ticks al muestreo de línea de 1 kHz. *(El "3–4 ms" es estimación de los
  comentarios del código, **no medición de banco** — `main_down.cpp:62-64`.)*
- Colchones RX en competencia: CENTRAL←TOP **512 B** (`CENTRAL_TOP_RX_BIGBUF`
  activo en ambos envs), TOP←DOWN **512 B**. DOWN sin `RX_HARDEN` (ring 64 B).

### 1.5 ⚠️ Corrección de topología (importante, contradice el supuesto del pedido)
La consigna asumía que el dato **siempre** hace DOWN→TOP→CENTRAL. **El firmware
real hace otra cosa**, y esto es una buena noticia:

- **La LÍNEA no pasa por el TOP para llegar a CENTRAL.** `down_tx` **difunde a
  los 2 enlaces a la vez** (`down_tx.cpp:18-21`): `Serial1`→CENTRAL **directo** y
  `Serial5`→TOP. El freno de borde usa el enlace **directo**
  (`down/comm_central.cpp:246`, `top/comm_down.cpp:40-43`).
- Por eso hay **dos caminos con perfiles de latencia muy distintos** (§2 y §3).
- **Caveat de competencia:** el OTOS **no se fusiona** al `WorldSnapshot` en el
  binario de partido (`POSE_FUSION` no está activo). El camino DOWN→TOP→CENTRAL
  **existe a nivel de transporte** (pose+vel viajan a 100 Hz), pero hoy el TOP
  no lo reenvía fusionado. Se modela igual porque es **la arquitectura** que
  Gustavo preguntó y la que se usaría al cablear `pose_fusion`.

---

## 2. CAMINO A — DOWN → TOP → CENTRAL (el que se preguntó)

### 2.1 HOY (UART) — peor caso ≈ **41 ms**

| # | Etapa | Peor caso | Tipo |
|---|---|---:|---|
| 1 | Adquisición OTOS (I²C bloqueante, 4 tx @100 kHz) | 4,0 ms | *estimado* |
| 2 | **Espera al gate de envío DOWN→TOP (100 Hz)** | **10,0 ms** | **gate** |
| 3 | Cable pose+vel (28 B @230400) | 1,2 ms | hecho |
| 4 | TOP: drenado RX + parse (free-running, sin cota) | 1,0 ms | *estimado* |
| 5 | TOP: procesamiento/fusión hasta entrar al snapshot | 2,0 ms | *estimado* |
| 6 | **Espera a la ISR emisora de snapshot (100 Hz)** | **10,0 ms** | **gate** |
| 7 | Cable `WorldSnapshot` (38 B) | 1,65 ms | hecho |
| 8 | CENTRAL: drenado RX | 0,5 ms | *estimado* |
| 9 | **Espera al gate de strategy (100 Hz)** | **10,0 ms** | **gate** |
| 10 | `strategy_tick` → `motors_apply` (mismo tick) | 0,5 ms | hecho |
| | **TOTAL** | **≈ 40,9 ms** | |

**Sumas por categoría:** gates **30,0 ms (73 %)** · I²C 4,0 ms (10 %) ·
proceso 4,0 ms (10 %) · **cable 2,85 ms (7 %)**.

> **Peor caso peor:** si el dato tuviera que pasar por el tick de
> `localization` del TOP (gate de **33 ms**, `main_top.cpp:493`), sumar hasta
> **+33 ms** → **≈ 74 ms**. Y el spike I²C del OTOS puede atrasar el loop de
> DOWN otros **+3–4 ms**.

### 2.2 Con CAN — tres escenarios

**Tiempos CAN @1 Mbps** (frame estándar 11-bit, 8 B datos ≈ 108 bits nominal,
~127 bits con bit-stuffing peor caso ≈ **0,13 ms/frame**):
`Pose2D` 7 B → **1 frame** · `LineStatusV2` 16 B → **2 frames** ·
`WorldSnapshot` 31 B → **4 frames**. Arbitraje peor caso presupuestado
**0,5 ms** (frame en curso + prioridades superiores, con bus al ~14 %).

**(A2) El dato SÍ necesita la fusión del TOP** — CAN no puede saltear el hop:

| # | Etapa | ms |
|---|---|---:|
| 1 | Adquisición OTOS (I²C — **no cambia**, no es el bus) | 4,0 |
| 2 | **Gate DOWN (100 Hz)** | **10,0** |
| 3 | Arbitraje + cable (2 frames) | 0,6 |
| 4 | TOP: RX por **mailbox/ISR** (sin parsear byte-stream) | 0,1 |
| 5 | TOP: procesamiento | 2,0 |
| 6 | **Gate emisión snapshot (100 Hz)** | **10,0** |
| 7 | Arbitraje + cable (4 frames) | 1,0 |
| 8 | CENTRAL: RX mailbox | 0,1 |
| 9 | **Gate strategy (100 Hz)** | **10,0** |
| 10 | → motores | 0,5 |
| | **TOTAL** | **≈ 38,3 ms** → **−6 %** |

**(A1) El dato NO necesita al TOP** (CAN es broadcast: CENTRAL lo oye **directo**,
se eliminan las etapas 4-7):

`4,0 + 10,0 (gate) + 0,6 + 0,1 + 10,0 (gate) + 0,5` = **≈ 25,2 ms → −39 %**

**(A1-evento) CAN + publicar apenas hay dato** (sin gate de emisión):

`4,0 + 0,6 + 0,1 + 10,0 (gate strategy) + 0,5` = **≈ 15,2 ms → −63 %**

**(A1-evento-full) + CENTRAL reacciona al RX, no al gate:**

`4,0 + 0,6 + 0,1 + 0,5` = **≈ 5,2 ms → −88 %**

### 2.3 El control: ¿y si hago lo mismo SIN CAN?
Sacar el relay y publicar por evento **sobre el UART directo que YA existe**
(`Serial1` DOWN→CENTRAL): `4,0 + 0 + 1,2 + 0,5 + 0,5` ≈ **6,2 ms**.

> **Esto es lo más importante del documento.** Se llega a **~6 ms** sin tocar
> el hardware. La ganancia de latencia viene de **arquitectura de software**
> (sin relay + event-driven), no del tipo de bus. CAN, con los mismos cambios,
> llega a ~5,2 ms: **~1 ms mejor**, es decir **irrelevante para latencia**.

---

## 3. CAMINO B — la línea (seguridad) — hoy YA es directo

Es el camino crítico real (freno de borde) y **ya está bien diseñado**: directo,
sin relay, gate rápido a 200 Hz, y CENTRAL lo chequea en **cada vuelta** del
loop (no al tick de 100 Hz) — `main_central.cpp:391-409`.

| Etapa | HOY (UART) | Con CAN |
|---|---:|---:|
| Muestreo de línea (1 kHz) | 1,0 ms | 1,0 ms |
| **Gate de envío (200 Hz)** | **5,0 ms** | **5,0 ms** |
| Cable (23 B / 2 frames CAN) | 1,0 ms | 0,26 ms |
| CENTRAL: RX | 0,5 ms | 0,1 ms |
| Chequeo de borde (cada vuelta) | 0,5 ms | 0,5 ms |
| **TOTAL** | **≈ 8,0 ms** | **≈ 6,9 ms** (**−14 %**) |

Consistente con el comentario del código: freno "<15 ms" (`main_central.cpp:371`).

> **Conclusión:** en el camino que **más importa para la seguridad**, CAN aporta
> **~1 ms**. Porque ese camino ya hizo lo correcto: **eliminó el relay**. Es la
> prueba empírica interna de la tesis de §2.3.

---

## 4. Carga de bus: ¿entra todo en un CAN de 1 Mbps?

| Tráfico | Frames/s |
|---|---:|
| Línea 200 Hz × 2 frames | 400 |
| Pose 100 Hz × 1 | 100 |
| Vel 100 Hz × 1 | 100 |
| `WorldSnapshot` 100 Hz × 4 | 400 |
| **Total** | **≈ 1.000 f/s** |

1.000 f/s × 0,135 ms ≈ **13,5 % de ocupación**. Entra holgado — queda ~86 % de
margen para las 4 cámaras y el bridge de telemetría del doc del bus.

*Comparación:* hoy `Serial1` (DOWN→CENTRAL) va al **32 %** de su capacidad
(7.400 B/s de 23.040 B/s) y `Serial4` (TOP→CENTRAL) al **16,5 %**.

---

## 5. Por qué CAN NO baja la latencia (el mecanismo)

1. **Los gates dominan.** 30 de 41 ms son esperas de reloj. Un bus más rápido
   no acorta una espera de reloj. Es como cambiar la ruta por una autopista
   cuando el 73 % del viaje es esperar el semáforo.
2. **El cable ya es barato.** 2,85 ms de 41. Aunque el cable fuera
   *instantáneo*, el peor caso bajaría solo a ~38 ms.
3. **CAN clásico es 1 Mbps; el UART está a 230,4 kbps** — sí, CAN es ~4× más
   rápido en señalización, pero con ~50 % de eficiencia de payload y **compartido
   entre todos los nodos**, mientras cada UART es **dedicado**. Por enlace, la
   diferencia real es chica (ver el análisis previo: *CAN no es más rápido*).

**Lo que CAN sí aporta acá** (y no es latencia):
- **Elimina el relay estructuralmente**: es broadcast, todos oyen todo → ya no
  hay que rebotar un dato por el TOP para que llegue a CENTRAL.
- **RX por mailbox/ISR** en vez de parsear byte-stream → menos I/O en el lazo,
  **menos jitter** (ataca el problema real de tiempo real).
- **Peor caso ACOTADO por arbitraje de prioridad en hardware**: no la latencia
  *más baja*, sino la *peor* **garantizada** bajo carga.
- Escala de pines, inmunidad a EMI de motores, CRC+reintento por HW.

---

## 6. Temas a analizar (formato coach, escala MODO APRENDIZAJE)

### T1 — Los gates son el 73 % de la latencia, no el bus · **P1**
**Qué observo.** Toda la cadena es fixed-rate por gate; nadie publica por evento.
Tres gates de 10 ms en serie = 30 ms de latencia pura de cuantización.
- `risk-no-fix`: se migra a CAN esperando latencia y se obtiene **−6 %**;
  la deuda vuelve a morder en el Nacional (nov-2026) con un robot que reacciona
  tarde y nadie sabe por qué.
- `risk-fix`: publicar por evento sube la tasa de mensajes (hay que rehacer el
  presupuesto de bus) y toca los 3 firmwares; riesgo de saturar el UART actual.
- `tiempo`: 1–2 días para un hop (medir primero), ~1 semana los tres.

### T2 — El relay por TOP es evitable para datos que no fusiona · **P1**
**Qué observo.** La línea **ya** va directa DOWN→CENTRAL y por eso tarda 8 ms en
vez de 41 ms. El mismo patrón aplica a cualquier dato que CENTRAL consuma sin
necesitar fusión del TOP.
- `risk-no-fix`: se paga ~13 ms de peaje por dato relayado, sin motivo.
- `risk-fix`: duplicar emisores complica el modelo de frescura/heartbeat
  (¿quién es la fuente de verdad de un dato que llega por 2 caminos?).
- `tiempo`: 0,5 día por dato (el transporte `down_tx` broadcast **ya existe**).

### T3 — El I²C bloqueante del OTOS (~4 ms) es el 10 % y todo el jitter · **P1**
**Qué observo.** En competencia corre a 100 kHz sin burst. La mitigación
(400 kHz + `getPosVelAcc` burst → ~0,4–0,6 ms) **ya está escrita** pero
**gateada a un env de banco** (`-DDOWN_OTOS_FAST_I2C`), no al binario de partido.
- `risk-no-fix`: sigue robando ticks al muestreo de línea de 1 kHz — el camino
  de seguridad.
- `risk-fix`: 400 kHz en un bus con EMI de motores puede dar errores I²C; hay
  que validar en banco.
- `tiempo`: 2 h (el código existe; es activar el flag y medir).

### T4 — Estos números NO están medidos · **P0 (bloquea el aprendizaje)**
**Qué observo.** Todo §2 y §3 es aritmética sobre constantes. El WCET real del
loop **nunca se midió**. Sin eso no se puede decidir nada con fundamento.
- `risk-no-fix`: se decide una migración de bus sobre estimaciones; **es
  exactamente el error que el frame del repo prohíbe** (no presentar hipótesis
  como hecho).
- `risk-fix`: ninguno (es instrumentación, no cambia conducta).
- `tiempo`: medio día. Ver §7.

---

## 7. Plan de prueba en hardware real (obligatorio antes de creerle a §2)

**Objetivo:** medir la latencia end-to-end real y el WCET del loop, para
reemplazar las estimaciones por hechos.

**Setup mínimo (sin osciloscopio):**
1. **Loop period real por placa.** `LoopMonitor` ya existe en CENTRAL
   (`main_central.cpp:271`). Loguear **máx/p99**, no promedio. En DOWN usar el
   env `down_loopmon`. *Aceptación:* histograma de período con el peor caso.
2. **Latencia end-to-end por marca de tiempo.** Agregar un campo `stamp_ms` al
   `Pose2D` en DOWN (o reusar `sample_age_ms`, que **ya existe** en
   `LineStatusV2` — `down_encode.cpp:15-20`). En CENTRAL, al consumir, loguear
   `millis() - stamp`. *Aceptación:* distribución de latencia; comparar el p99
   contra los ~41 ms estimados.
3. **Con osciloscopio (más fino):** GPIO en alto en DOWN al muestrear, GPIO en
   alto en CENTRAL al aplicar el comando de motor → medir el delta en 2 canales.
   *Aceptación:* peor caso sobre ~1000 eventos.
4. **Prueba de estrés:** repetir **con los 3 motores girando** (EMI real) y
   batería cargada. *Aceptación:* que el p99 no se degrade y `crc_errors` ≈ 0.

**Criterio de decisión:** si el p99 medido se parece a los ~41 ms → T1/T2 son
reales y **conviene atacar gates+relay ANTES que el bus**. Si es mucho peor →
hay un bloqueo no modelado (buscar I/O en el lazo, no cambiar de bus).

---

## 8. Recomendación

1. **No migrar a CAN buscando latencia.** No la da (−6 % en el caso relevante).
2. **Primero medir** (§7, T4) — hoy no hay ni un número de banco.
3. **Después atacar, en orden de retorno:** (a) sacar el relay donde no hace
   falta (T2, patrón que la línea **ya demuestra**), (b) publicar por evento
   (T1), (c) destrabar el I²C del OTOS (T3). **Las tres son gratis en hardware**
   y llevan el peor caso de ~41 ms a **~6 ms**.
4. **CAN sigue justificado** — pero por lo que dice su propio doc: **pines,
   robustez a EMI, escala a 4 cámaras + bridge, determinismo del peor caso**.
   **Nunca por velocidad.** Si además se migra, el peor caso queda en ~5 ms
   *y* con arbitraje acotado bajo carga, que es la propiedad de tiempo real que
   hoy no existe.

## 9. Fuentes

- Firmware vivo (commit `9a56923`): `software/teensy/Soccer 2026/src/{down,top,central,shared}/`
  + `platformio.ini` — parámetros de §1, todos con archivo:línea.
- Propuesta de bus: `docs/decisions/2026-06-03-bus-can-general-y-flasheo-por-can.md`
- Diseño de comunicaciones vigente (capas fail-safe): `docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md`
- Mapa de datos: `docs/MAPA-DE-DATOS.md`
- CAN: ISO 11898-2 (1 Mbps, frame estándar, bit-stuffing).
- Extracción de parámetros: workflow multi-agente (5 lectores + 1 verificador
  adversarial), 2026-06-23; correcciones aplicadas (topología de la línea a
  `Serial5`, `CENTRAL_TOP_RX_BIGBUF` activo en competencia, "6 Hz" del TOP
  superado).
