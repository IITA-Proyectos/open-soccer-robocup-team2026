# TEST-CARDS de banco — Placa DOWN (Teensy 4.0)

> ✅ **ACTUALIZADO 2026-06-06 — programas que antes faltaban YA EXISTEN** (creados aditivos; verificar con `pio run -e <env>`, NO compilan en el gate host):
> - **CARD DOWN-4 / DOWN-9 (tasa 1 kHz + carga por tick):** env **`diag_down_cpu`** + `src/diag/diag_down_cpu.cpp` ✅ creado.
> - **CARD DOWN-8 (ROBOT2 sin OTOS):** env **`down_robot2`** (= `down` + `-DDOWN_NUM_OTOS_CONNECTED=0`) ✅ creado.
> Donde abajo diga "NO existe / falta crear / BLOQUEADA" para estos, leer: **ya creado** (ver este banner).

> Tarjetas de prueba CORTAS (idealmente <5 min) para validar la placa DOWN en el
> banco, camino a Incheon. La placa DOWN lee 32 sensores de luz (4 muxes CD4051)
> + 2 SparkFun OTOS, procesa línea y odometría, y los difunde a CENTRAL (Serial1,
> línea urgente 200 Hz) y a TOP (Serial5, odometría 100 Hz).
>
> **Antes de empezar:**
> - Todos los comandos `pio` se corren DESDE `software/teensy/Soccer 2026/`:
>   `cd "software/teensy/Soccer 2026"` y desde ahí `pio run -e <env> -t upload`.
> - Monitor serie SIEMPRE a **115200** baud (`pio device monitor -b 115200`).
> - Cuando una card pida pegar serial de vuelta a la IA: pegá la **línea literal**
>   (copiá tal cual la imprime el monitor), no la parafrasees.
> - ⚠️ Robot1 byte-idéntico: estos envs gateados (`down_wdt`, `down_lean`) NO
>   cambian el binario de competencia `[env:down]` hasta que se PROMUEVAN. Las
>   cards de abajo son el gate de esa promoción.
> - Fuente de los criterios: `docs/RUNBOOK-BANCO-INCHEON.md` §2.3/§2.4 y
>   `docs/ESTADO-MADUREZ-FEATURES.md`.

---

## Subsistema 1 — Watchdog HW (WDOG1, 1 s)

Env: `[env:down_wdt]` (platformio.ini:982-986) = `[env:down]` + `-DDOWN_ENABLE_WDT`.
Código: `src/down/main_down.cpp:64-80` (init/feed), `:138-145` (arma al final de
setup), `:148-154` (feed como PRIMERA línea de loop). Default OFF.

### CARD DOWN-1: Watchdog NO resetea en boot

- **Objetivo:** confirmar que el WDT armado al final de `setup()` no se dispara
  durante el boot lento (otos_init ~0.5 s + calib carpet ~0.32 s). Si reseteara
  acá, la placa entraría en boot-loop y nunca arrancaría.
- **Placa:** DOWN (Teensy 4.0).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e down_wdt -t upload`
- **¿Existe el programa?:** SÍ. `[env:down_wdt]` en platformio.ini:982. El flag
  `DOWN_ENABLE_WDT` lo consume `src/down/main_down.cpp:64,138,149`. No hay que crear nada.
- **Setup físico:** placa DOWN alimentada, los 4 muxes + 2 OTOS conectados (placa
  fixeada), robot quieto sobre el carpet (la calib de boot mide el verde). USB al monitor.
- **Pasos:**
  1. Flashear con el comando de arriba.
  2. Abrir `pio device monitor -b 115200` INMEDIATAMENTE tras el upload.
  3. Mirar la secuencia de boot; dejar correr 60 s sin tocar nada.
- **Que esperar si PASA:** la secuencia de boot completa y aparece, UNA sola vez:
  `[DOWN] watchdog (WDOG1, 1 s) armado` (main_down.cpp:144), precedido por
  `[DOWN] listo: odometria a ARRIBA + linea urgente a CENTRAL` (main_down.cpp:136).
  El LED de status (pin 13) queda encendido. NO se repite la cabecera de boot
  (`[DOWN] line_ring init OK`).
- **Resultados posibles:**
  - A) Aparece `armado` una vez y NO se repite el boot en 60 s → PASS (boot limpio).
  - B) El boot se repite en bucle (vuelve a salir `[DOWN] line_ring init OK` cada
    ~1-2 s) → FAIL: el WDT se arma demasiado pronto o el boot tarda >1 s después
    de armarlo. Reportar; NO promover.
  - C) Nunca sale `armado` pero sí `listo` → el binario no trae el flag (env mal);
    re-flashear con `down_wdt` (no `down`).
- **Feedback a devolver a la IA:** pegá las últimas ~8 líneas del monitor desde el
  primer `[DOWN] line_ring init OK`, incluyendo si `[DOWN] watchdog (WDOG1, 1 s) armado`
  aparece 1 vez o se repite.
- **Tiempo estimado:** 3 min.

### CARD DOWN-2: Watchdog auto-resetea al colgar I²C

- **Objetivo:** confirmar que si el bus I²C de un OTOS se cuelga (loop trabado en
  `otos_tick`), el WDOG1 resetea la placa a ~1 s y recupera el LINE_URGENT a CENTRAL.
  Es el seguro real contra "el robot deja de frenar en el borde".
- **Placa:** DOWN (Teensy 4.0).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e down_wdt -t upload`
  (mismo binario que DOWN-1; correr DOWN-1 antes).
- **¿Existe el programa?:** SÍ (mismo env `down_wdt`). El feed es la primera línea
  de `loop()` (main_down.cpp:153); si el loop se traba, no se vuelve a alimentar.
- **Setup físico:** placa booteada y corriendo (post DOWN-1), monitor abierto.
  Tené a mano el conector de UNO de los OTOS (U5 en Wire / U6 en Wire1) para
  desconectarlo EN CALIENTE (sin cortar la alimentación de la placa).
- **Pasos:**
  1. Con la placa corriendo y el monitor abierto, desconectá en caliente el cable
     SDA/SCL (o el conector Qwiic) de UN OTOS.
  2. Cronometrá: contá ~1-2 s.
  3. Observá si la placa rebootea (vuelve la cabecera `[DOWN] line_ring init OK ...`).
- **Que esperar si PASA:** dentro de ~1-2 s de desconectar el OTOS, el monitor
  muestra el reboot completo (reaparece `[DOWN] line_ring init OK` y el resto de la
  secuencia de setup). El LED hace el ciclo de boot otra vez.
- **Resultados posibles:**
  - A) Rebootea en ~1 s → PASS (WDT funcionando).
  - B) NO rebootea y el monitor se congela (deja de imprimir) indefinidamente →
    FAIL: el WDT no está alimentándose/armado o el cuelgue no traba el loop.
  - C) NO rebootea y sigue imprimiendo normal → la desconexión no colgó el bus
    (el core hizo timeout HW); probá desconectando de otra forma o forzando el
    cuelgue. Reportar como inconcluso (no es FAIL del WDT).
- **Feedback a devolver a la IA:** decí "rebooteó en ~N segundos" (con N medido) o
  "no rebooteó, monitor congelado" / "no rebooteó, siguió normal". Pegá las 3-4
  líneas alrededor del evento.
- **Tiempo estimado:** 4 min.
- **PROMOCIÓN:** DOWN-1 (A) + DOWN-2 (A) PASS → mover `-DDOWN_ENABLE_WDT` al
  `[env:down]` y re-flashear competencia (RUNBOOK §2.3).

---

## Subsistema 2 — Lean line pipeline (ahorro de CPU)

Env: `[env:down_lean]` (platformio.ini:988-992) = `[env:down]` + `-DDOWN_LEAN_LINE_PIPELINE`.
Gate: `src/down/config_down.h:68-70` (apaga `LINE_RING_PROCESS`).
Invariante: `src/down/line_ring.cpp:142-144` — el muestreo CRUDO, `g_last_sample_us`
y `g_tick_count` corren SIEMPRE; el `LineStatusV2` lo arma `dm_update()` desde la
lectura cruda (`line_ring_get_raw`), NO desde el pipeline apagado. Por eso el wire
debe quedar idéntico.

### CARD DOWN-3: Lean — LineStatusV2 byte-idéntico

- **Objetivo:** confirmar que apagar el pipeline de filtros muerto NO cambia el
  dato de línea que sale a CENTRAL/TOP (line_present, ángulo, imminent_exit). Es la
  promoción más segura porque no toca el wire — pero hay que verlo.
- **Placa:** DOWN (Teensy 4.0) emitiendo + una placa receptora (CENTRAL o TOP) para
  leer el `LineStatusV2`.
- **Programa / env:**
  - DOWN: `cd "software/teensy/Soccer 2026" && pio run -e down_lean -t upload`
  - Receptor (elegí uno): CENTRAL `pio run -e diag_central_rx_all -t upload`
    o TOP `pio run -e diag_top_comm_down -t upload`.
- **¿Existe el programa?:** SÍ. `[env:down_lean]` (platformio.ini:988). Receptores:
  `diag_central_rx_all` (src/diag/diag_central_rx_all.cpp) y `diag_top_comm_down`
  (platformio.ini:956, src/diag/diag_top_comm_down.cpp). Nada que crear.
- **Setup físico:** DOWN cableada a la receptora (DOWN→CENTRAL = Serial1, pin1→pin0;
  DOWN→TOP = Serial5). Robot sobre el carpet con una línea blanca accesible para
  pasarla bajo el anillo.
- **Pasos:**
  1. Flashear DOWN con `down_lean` y la receptora con su diag.
  2. Abrir el monitor de la RECEPTORA (115200). Anotar line_present / ángulo /
     imm_exit con el anillo SOBRE el verde y luego SOBRE la línea blanca.
  3. Re-flashear DOWN con el env normal `[env:down]` (`pio run -e down -t upload`)
     y repetir el paso 2 con los mismos movimientos.
- **Que esperar si PASA:** los valores de línea leídos en la receptora son los
  MISMOS en `down_lean` y en `down` para las mismas posiciones (sobre verde:
  line_present=NO; sobre blanco: line_present=SI con un ángulo coherente y, al
  cubrir varios sensores, imminent_exit/borde activándose igual).
- **Resultados posibles:**
  - A) Mismo comportamiento de línea en ambos binarios → PASS (wire idéntico).
  - B) Difiere (p.ej. detecta línea en lean pero no en normal, o el ángulo salta)
    → FAIL: el lean cambió el wire. Reportar; NO promover.
  - C) La receptora no recibe nada → problema de cableado/UART, no del lean;
    revisar el enlace antes de juzgar.
- **Feedback a devolver a la IA:** pegá la línea de la receptora que muestra
  line_present/ángulo/imm_exit en las DOS condiciones (verde / blanco) para
  `down_lean` y para `down`, así la IA compara los 4 estados.
- **Tiempo estimado:** 5 min.

### CARD DOWN-4: Lean — headroom de CPU (duración del tick)

- **Objetivo:** medir que el lean libera CPU: la duración del tick de línea
  (`line_ring_get_last_tick_us`) debe BAJAR respecto al binario normal, sin afectar
  el muestreo de 1 kHz.
- **Placa:** DOWN (Teensy 4.0).
- **Programa / env:** NO existe hoy un diag que imprima `line_ring_get_last_tick_us`
  para la placa DOWN aislada. **Falta crear** (NO lo crees vos): un sketch de banco
  (p.ej. `src/diag/diag_down_cpu.cpp` + su `[env:diag_down_cpu]`) que en `loop()`
  llame `line_ring_tick()` a 1 kHz y cada 500 ms imprima
  `line_ring_get_last_tick_us()` y `line_ring_get_tick_count()`, compilable con y
  sin `-DDOWN_LEAN_LINE_PIPELINE`. La API ya está expuesta en
  `src/down/line_ring.h:53-54`. Mientras no exista, esta card queda BLOQUEADA.
- **Setup físico:** placa DOWN quieta sobre el carpet, USB al monitor.
- **Pasos (cuando exista el diag):**
  1. Compilar el diag SIN lean y anotar el `last_tick_us` promedio.
  2. Compilar el diag CON `-DDOWN_LEAN_LINE_PIPELINE` y anotar el mismo número.
  3. Comparar.
- **Que esperar si PASA:** el `last_tick_us` con lean es MENOR que sin lean
  (el pipeline de filtros ×32 ya no corre), y `tick_count` sigue avanzando ~1000/s
  en ambos (muestreo crudo intacto).
- **Resultados posibles:**
  - A) tick_us baja con lean y tick_count ~1000/s → PASS (hay headroom real).
  - B) tick_us igual o sube → el pipeline no era el costo dominante; reportar el
    número (igual el lean es seguro si DOWN-3 pasa).
  - C) tick_count cae bien por debajo de 1000/s → problema de muestreo, reportar.
- **Feedback a devolver a la IA:** "falta el diag de CPU, no se puede medir" O, si
  ya se creó, pegá los dos `last_tick_us` (sin lean / con lean) y los `tick_count`.
- **Tiempo estimado:** 4 min (una vez exista el diag).
- **PROMOCIÓN:** DOWN-3 (A) PASS es el criterio duro (wire); DOWN-4 es evidencia
  del beneficio. PASS → mover `-DDOWN_LEAN_LINE_PIPELINE` al `[env:down]` (RUNBOOK §2.4).

---

## Subsistema 3 — Anillo de 32 sensores de línea (censo / 0 muertos)

Diag de censo crudo: `[env:diag_down]` → `src/diag/main_diag_down.cpp`
(vuelca los 32 valores crudos + marca de blanco cada 300 ms, muestrea a 1 kHz).
Diag de calibración con census de sospechosos: `[env:diag_down_calibracion]` →
`src/diag/diag_down_calibracion.cpp` (comando `'m'` → "sensores sospechosos: N / 32").

### CARD DOWN-5: Censo de sensores — 0 muertos

- **Objetivo:** confirmar que los 32 sensores del anillo RESPONDEN (cada uno cambia
  su valor crudo al pasarle la línea blanca). Un sensor que nunca cambia = muerto o
  mal soldado → punto ciego en el anillo.
- **Placa:** DOWN (Teensy 4.0).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e diag_down -t upload`
- **¿Existe el programa?:** SÍ. `[env:diag_down]` (platformio.ini:174) compila
  `src/diag/main_diag_down.cpp` (4 muxes = 32 sensores vía `-DDOWN_NUM_MUXES_CONNECTED=4`).
- **Setup físico:** placa DOWN con los 4 muxes conectados, sobre el carpet verde,
  con una línea blanca (o cinta/papel blanco) para pasar bajo cada sensor. USB al monitor.
- **Pasos:**
  1. Flashear y abrir `pio device monitor -b 115200`.
  2. Esperar la cabecera + `Sensores de luz configurados : 32` y la calib de carpet.
  3. Pasá el blanco LENTO por todo el anillo. Mirá la fila `LUZ S0:.. S1:.. ... S31:..`:
     el valor del sensor tapado debe subir y aparecer un `*` al lado (`line_ring_get_white`).
- **Que esperar si PASA:** cabecera `Sensores de luz configurados : 32`
  (main_diag_down.cpp:67-68). Al barrer el blanco, CADA uno de S0..S31 muestra su
  número subiendo y un `*` en algún momento. Ningún sensor queda clavado (siempre el
  mismo valor, sin `*` nunca).
- **Resultados posibles:**
  - A) Los 32 reaccionan (todos llegan a `*`) → PASS, 0 muertos.
  - B) Uno o más nunca cambian / nunca dan `*` → esos están muertos o mal soldados.
    Anotar los índices SN. FAIL del anillo.
  - C) Un mux entero (8 consecutivos, p.ej. S8..S15) clavado en ~1023 o en 0 →
    falla de ese mux (ADC flotante / SEL mal). Reportar el rango.
- **Feedback a devolver a la IA:** decí "los 32 dan `*`" O listá exactamente qué SN
  no cambian (ej: "S14 y S27 clavados en 512, nunca `*`"). Pegá una fila `LUZ ...`
  con el blanco puesto sobre un sensor sospechoso.
- **Tiempo estimado:** 5 min.

### CARD DOWN-6: Calibración + censo de "sospechosos"

- **Objetivo:** calibrar verde/blanco real del campo y verificar que los 32 separan
  verde de blanco (margen ≥ 80 counts). Guarda la calib en EEPROM (la usa el
  firmware de competencia al boot).
- **Placa:** DOWN (Teensy 4.0).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e diag_down_calibracion -t upload`
- **¿Existe el programa?:** SÍ. `[env:diag_down_calibracion]` (platformio.ini:702)
  → `src/diag/diag_down_calibracion.cpp`. Comandos: `c` verde, `b` blanco, `m`
  sospechosos, `s` guardar EEPROM, `t` probar, `v` ver, `l` cargar, `x` borrar.
- **Setup físico:** robot sobre el VERDE real del campo; una LÍNEA BLANCA real para
  apoyar el anillo encima. USB al monitor.
- **Pasos:**
  1. Flashear, abrir monitor. Con el anillo sobre el verde, escribí `c` + Enter.
  2. Apoyá los sensores sobre la línea blanca (lo más cubierta posible), escribí
     `b` + Enter.
  3. Escribí `m` + Enter para el censo de sospechosos.
- **Que esperar si PASA:** tras `m`, la línea
  `[CAL] sensores sospechosos: 0 / 32` seguida de
  `[CAL] todos separan verde/blanco OK.` (diag_down_calibracion.cpp:136-140).
- **Resultados posibles:**
  - A) `sospechosos: 0 / 32` → PASS. Escribí `s` para guardar la calib en EEPROM.
  - B) `sospechosos: N / 32` con N>0 → esos sensores no separan verde/blanco
    (margen < 80). Pueden ser sombra/cobertura mala al capturar blanco (recapturá
    `b`) o sensor débil. Listar los SN.
  - C) Todos sospechosos / margen negativo → la captura de verde y blanco quedó
    invertida o el blanco no se apoyó bien. Recalibrar (`c` y `b` de nuevo).
- **Feedback a devolver a la IA:** pegá la línea `[CAL] sensores sospechosos: N / 32`
  y, si N>0, la salida del comando `m` con los `SN margen=...`. Confirmá si
  guardaste con `s` (`*** Calibracion GUARDADA en EEPROM ***`).
- **Tiempo estimado:** 5 min.

---

## Subsistema 4 — OTOS (odometría)

ROBOT1: 2 OTOS (U5→Wire/0x17, U6→Wire1/0x17). ROBOT2: SIN OTOS (ver MEMORY /
robot-variants). Diag: `[env:diag_down]` hace I²C scan + init + lectura en vivo
(main_diag_down.cpp:82-91,148-158). Firmware: `[env:down_debug]` muestra pose por USB.

### CARD DOWN-7: ROBOT1 — los 2 OTOS detectan y leen

- **Objetivo:** confirmar que ambos OTOS responden por I²C (U5 en Wire, U6 en Wire1)
  y que la pose fusionada cambia al mover el robot.
- **Placa:** DOWN (Teensy 4.0), ROBOT1.
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e diag_down -t upload`
- **¿Existe el programa?:** SÍ. `[env:diag_down]` (platformio.ini:174) corre el I²C
  scan en ambos buses + `otos_init()` + lectura en vivo (main_diag_down.cpp).
- **Setup físico:** placa DOWN ROBOT1 con los 2 OTOS conectados (U5 Wire SDA18/SCL19,
  U6 Wire1 SDA17/SCL16), apoyada sobre una superficie con textura (los OTOS son
  ópticos: necesitan ver el piso). USB al monitor.
- **Pasos:**
  1. Flashear, abrir monitor. Leer las líneas de I²C scan y de OTOS init.
  2. Deslizá la placa unos cm sobre la superficie.
  3. Mirá la línea `OTOS: x=.. y=.. hdg=..` cambiar.
- **Que esperar si PASA:** en el scan, `0x17` aparece en AMBOS buses (Wire y Wire1).
  `[diag] OTOS init: L=OK R=OK` (main_diag_down.cpp:87-91). Al mover la placa, la
  línea `OTOS: x=.. y=.. hdg=.. [L=OK R=OK]` (main_diag_down.cpp:148-158) muestra
  x/y cambiando.
- **Resultados posibles:**
  - A) `L=OK R=OK` y x/y cambian al mover → PASS (2 OTOS sanos).
  - B) `L=OK R=--` (o viceversa) → un OTOS no responde. Revisar bus Wire1
    (SDA17/SCL16) / pull-ups / soldadura del U6. Reportar cuál.
  - C) `L=-- R=--` / scan vacío en ambos buses → sin power 3V3 a los OTOS o sin
    pull-ups. Reportar el resultado del `[i2c-scan]`.
- **Feedback a devolver a la IA:** pegá las dos líneas `[i2c-scan] Wire ...` y
  `[i2c-scan] Wire1 ...`, la línea `[diag] OTOS init: L=.. R=..`, y una línea
  `OTOS: x=.. y=.. hdg=..` antes y después de mover.
- **Tiempo estimado:** 4 min.

### CARD DOWN-8: ROBOT2 — fallback sin OTOS no rompe

- **Objetivo:** confirmar que en ROBOT2 (que NO tiene OTOS) el firmware DOWN bootea
  y emite línea normalmente con 0 OTOS, sin colgarse ni spamear errores. ROBOT1
  byte-idéntico: NO se toca `[env:down]`; se necesita un env nuevo.
- **Placa:** DOWN (Teensy 4.0), ROBOT2 (sin OTOS soldados/conectados).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e down_robot2 -t upload`
- **¿Existe el programa?:** **NO existe.** Falta crear (NO lo crees vos) un env nuevo
  en platformio.ini, p.ej.:
  ```
  [env:down_robot2]
  extends = env:down
  build_flags = ${env:down.build_flags} -UDOWN_NUM_OTOS_CONNECTED -DDOWN_NUM_OTOS_CONNECTED=0
  ; (mantiene 4 muxes = 32 sensores; pone 0 OTOS para la placa ROBOT2)
  ```
  El firmware ya soporta `NUM_OTOS=0` limpio: `otos_init()` guarda con
  `if (NUM_OTOS >= 1)` / `>= 2` (otos.cpp:81,86) y retorna false sin tocar I²C;
  `otos_tick()` con ningún OTOS vivo cae en la rama de degradación (otos.cpp:162-165)
  y no actualiza pose; `line_ring`/`comm_central` no dependen de OTOS. Falta SOLO
  el env para flashearlo.
  > Alternativa de validación SIN crear el env (rápida, ROBOT1): desconectar
  > físicamente los 2 OTOS de una placa ROBOT1 con `[env:down_debug]` y verificar
  > que igual bootea y emite línea (degradación en caliente). No es lo mismo que
  > NUM_OTOS=0 en compilación, pero da señal temprana.
- **Setup físico:** placa DOWN ROBOT2 sin OTOS, los 4 muxes conectados, sobre el
  carpet. USB al monitor.
- **Pasos (cuando exista `down_robot2`, o con `down_debug` + OTOS desconectados):**
  1. Flashear y abrir monitor a 115200.
  2. Verificar que la secuencia de boot completa sin colgarse.
  3. (con `down_debug`) mover el robot sobre la línea blanca y ver la línea de debug.
- **Que esperar si PASA:** boot completo hasta
  `[DOWN] listo: odometria a ARRIBA + linea urgente a CENTRAL` (main_down.cpp:136).
  El OTOS reporta `L=- R=-` y `(NINGUNO — degradacion total)` (main_down.cpp:96-98)
  SIN colgar el boot. Con `down_debug`, la línea
  `[DOWN] LINEA=SI ang=.. cross=..mm | OTOS x=0.0 y=0.0 hdg=0.0 [L=-- R=--] | tx_ok=N drop=0`
  (comm_central.cpp:161-179): la línea se detecta y `tx_ok` sube; la pose OTOS queda
  en 0 y NO crece.
- **Resultados posibles:**
  - A) Bootea, `tx_ok` sube, línea se detecta, OTOS queda en 0/0/0 → PASS (fallback OK).
  - B) El boot se cuelga / no llega a `listo` → FAIL: algo asume OTOS presente.
    Reportar dónde se traba (última línea impresa).
  - C) Spamea errores I²C en loop → reportar; el scan/timeout no debería repetirse.
- **Feedback a devolver a la IA:** pegá la línea `[DOWN] OTOS: L=- R=- (NINGUNO ...)`
  + `[DOWN] listo: ...`, y (con down_debug) una línea `[DOWN] LINEA=... | OTOS x=0.0
  ... [L=-- R=--] | tx_ok=N drop=M`. Confirmá si `tx_ok` crece y si la pose queda en 0.
- **Tiempo estimado:** 5 min (o 3 min con la alternativa down_debug).

---

## Subsistema 5 — Muestreo 1 kHz del anillo

`line_ring_tick()` corre a 1 kHz (`LINE_TICK_INTERVAL_US=1000`, config_down.h:138;
loop main_down.cpp:161-163). Contadores SIEMPRE activos: `g_tick_count` y
`g_last_sample_us` (line_ring.cpp:142-144). El diag `diag_down` ya tickea a 1 kHz
(main_diag_down.cpp:104-108).

### CARD DOWN-9: Verificar tasa de muestreo de 1 kHz

- **Objetivo:** confirmar que el anillo se muestrea ~1000 veces/s (línea es el path
  de freno de borde: si el muestreo cae, el robot detecta el borde tarde).
- **Placa:** DOWN (Teensy 4.0).
- **Programa / env:** NO existe un diag que imprima la TASA medida de tick_count
  por segundo. **Falta crear** (NO lo crees vos): el mismo `diag_down_cpu` de la
  CARD DOWN-4 cubre esto si imprime `delta(line_ring_get_tick_count())` por segundo.
  La API ya existe: `line_ring_get_tick_count()` / `line_ring_get_last_sample_us()`
  (line_ring.h:53-58). Mientras no exista ese diag, esta card se valida de forma
  INDIRECTA (abajo) con `diag_down`.
- **Setup físico:** placa DOWN quieta sobre el carpet, USB al monitor.
- **Pasos (validación indirecta con `diag_down`, sin crear nada):**
  1. Flashear `pio run -e diag_down -t upload`, abrir monitor.
  2. El diag imprime cada 300 ms; la línea procesada (`linea: angulo=.. depth=..`)
     debe refrescar fluido y reaccionar de inmediato al pasar el blanco.
  3. Pasá el blanco RÁPIDO por un sensor: la marca `*` y el `depth` deben capturarlo
     sin "perder" el evento (a 1 kHz, un cruce de pocos ms se ve).
- **Que esperar si PASA (indirecto):** la reacción al blanco es instantánea (el `*`
  aparece en el mismo refresco que pasás el sensor); el sistema no "saltea" cruces
  rápidos. (Directo, con el diag a crear: ~1000 ± pocos % ticks/s reportados.)
- **Resultados posibles:**
  - A) Reacción inmediata, no se pierden cruces rápidos → PASS indirecto.
  - B) Hay que pasar el blanco lento o "se pierde" en cruces rápidos → el muestreo
    efectivo puede estar degradado (¿OTOS robando ticks? ver nota abajo). Reportar.
  - C) (con diag a crear) tick/s claramente < ~900 → FAIL del muestreo; reportar
    el número.
- **Nota de diseño:** en el firmware REAL, `otos_tick()` hace I²C bloqueante ~3-4 ms
  cada 10 ms y le roba ticks al muestreo de 1 kHz (comentado en main_down.cpp:176-179).
  El diag `diag_down` también tickea OTOS, así que la prueba dura de tasa conviene
  hacerla con el diag a crear, con y sin OTOS, para cuantificar el robo.
- **Feedback a devolver a la IA:** "reacción inmediata / se pierden cruces rápidos"
  O, si ya existe el diag de tasa, pegá los ticks/s medidos (con y sin OTOS).
- **Tiempo estimado:** 3 min (indirecto).

---

## Resumen de envs/diags

| Card | Env | ¿Existe? | Archivo |
|------|-----|----------|---------|
| DOWN-1, DOWN-2 | `down_wdt` | SÍ (platformio.ini:982) | src/down/main_down.cpp:64-154 |
| DOWN-3 | `down_lean` + `diag_central_rx_all`/`diag_top_comm_down` | SÍ (988 / 956) | config_down.h:68-70, line_ring.cpp:142-144 |
| DOWN-4 | `diag_down_cpu` | **NO — falta crear** | API en line_ring.h:53-54 |
| DOWN-5 | `diag_down` | SÍ (174) | src/diag/main_diag_down.cpp |
| DOWN-6 | `diag_down_calibracion` | SÍ (702) | src/diag/diag_down_calibracion.cpp |
| DOWN-7 | `diag_down` | SÍ (174) | src/diag/main_diag_down.cpp:82-158 |
| DOWN-8 | `down_robot2` | **NO — falta crear** (DOWN_NUM_OTOS_CONNECTED=0) | firmware ya soporta NUM_OTOS=0: otos.cpp:81,86,162 |
| DOWN-9 | `diag_down_cpu` (directo) / `diag_down` (indirecto) | parcial | line_ring.cpp:142-144 |

**Faltantes a crear (NO en esta tarea — son cards de "needs-firmware"):**
1. `[env:diag_down_cpu]` + `src/diag/diag_down_cpu.cpp` — imprime
   `line_ring_get_last_tick_us()` y la tasa de `line_ring_get_tick_count()`/s,
   compilable con/sin `-DDOWN_LEAN_LINE_PIPELINE`. Cubre DOWN-4 y DOWN-9 directo.
2. `[env:down_robot2]` — `extends = env:down` + `-DDOWN_NUM_OTOS_CONNECTED=0`.
   Cubre DOWN-8.
