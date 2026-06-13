# 2026-06-13 — Sintonía fina de calibración DOWN (sensibilidad + habilitar/deshabilitar) + verificación de producción

**Sesión:** Claude (coach) + Gustavo. Repo principal `soccer-main` (branch `main`).
**Tema:** 3 features de calibración fina en la app `monitor-base` + firmware DOWN, iteración
sobre feedback de banco, y verificación de que quedan "en producción" en los dos robots.

---

## Qué se hizo

Se agregaron 3 features pedidas por Gustavo para entender/ajustar mejor los 32 sensores de
línea de la placa DOWN desde la app, sin reflashear cada vez:

1. **Habilitar/deshabilitar sensores** individualmente (click → inspector + botón, o
   doble-click en anillo/barra). Un sensor OFF se excluye del cálculo de línea (como uno
   unhealthy) en `dm_update`.
2. **Sensibilidad global al blanco** (`SENS GLOBAL`, −100…+100, **+ = menos sensible**) con
   mapeo **saturante** (−100 → todo blanco, +100 → nada) y barra de cercanía al umbral por
   sensor.
3. **Sensibilidad por-sensor** (`SENS SET`). `CAL SAVE` persiste todo en **EEPROM v2**.

Todo **aditivo y no-op por default** (todos habilitados, sensibilidad 0 → umbral en el punto
medio = competencia histórica byte-idéntica).

### Commits (todos en `main`, pusheados)

- `f69bc0b` — feat: habilitar/deshabilitar + sensibilidad global y por-sensor (firmware + app).
- `0173999` — fix: saturación de sensibilidad + contraste/visual + **6 hallazgos de revisión
  adversarial** (workflow 17 agentes). El importante: `sens=0` NO era byte-idéntico
  (`lroundf` vs `mid()` floor → +1 en sumas carpet+white impares; los 55 tests no lo cazaron
  porque todas las calibs de test usan sumas pares). Fix: `total==0 → mid()` + test de paridad.
- `a4e8ae6` — fix: sensores deshabilitados en gris + X (no amarillo) en el anillo.
- `476bf2d` — feat: doble-click en anillo/barra habilita-deshabilita directo.
- `d27c9c1` — feat: la sensibilidad se aplica **al mover** el slider (no al soltar) — más
  intuitivo. Se envía en el callback de cambio del `ttk.Scale`, deduplicado por valor entero.

---

## Verificación (host — lo que Claude SÍ puede cerrar)

- `pio run -e down` (R1 competencia), `-e down_robot2` (R2) y `-e down_debug_telemetry` →
  los **tres SUCCESS** (27.7 / 7.5 / 7.3 s).
- 59 tests C++ vía g++ (Avast bloquea `pio test` — TASK-025) + 82 pytest de la app + import GUI.

### Confirmación clave: ¿está "en producción" en los dos robots? — SÍ (por lectura de código)

Gustavo pidió "ponerlo en producción y asegurarse que está en todos los programas base, R1 y
R2". Se verificó leyendo el firmware (no la doc):

- **Boot ungated:** `comm_central_load_persisted_calib()` (en `main_down.cpp:117`, SIN gate de
  telemetría) lee la EEPROM v2 y puebla `g_dm.calib[].enabled/.sensitivity` + `g_dm.global_sens`
  → aplica en `down`, `down_robot2` Y `down_debug_telemetry`.
- **Detección ungated:** `dm_update()` (la verdad que viaja a CENTRAL) consume esas perillas
  cada tick, también sin gate.
- **Comandos + frame v3** están detrás de `#if defined(DOWN_DEBUG_TELEMETRY) || defined(DOWN_USB_MONITOR)`,
  y **`[env:down]` define `-DDOWN_USB_MONITOR`** (TASK-306, monitor dormido en el binario de
  partido), que `down_robot2` hereda → la calibración en vivo + persistencia ya funciona en los
  binarios de **partido** de ambos robots.

**Conclusión:** no había gap de firmware. "En producción y en los dos robots" ya era cierto
por diseño (defaults no-op + gate doble + boot ungated). No hubo que portar nada.

---

## Validado en HARDWARE REAL (testimonio de Gustavo — el equipo cierra hardware, no Claude)

- **R1, placa DOWN física, 2026-06-13:** la app conectada por USB; los **sliders de
  sensibilidad aplican AL MOVER** (tuning en vivo OK sobre la placa real).

## Pendiente de banco (NO cerrado — regla no negociable: hardware lo cierra el equipo)

Trasladado a **TASK-306** (ampliación 2026-06-13) como criterios de cierre:

1. **Persistencia tras power-cycle:** `CAL SAVE` con perillas → apagar/encender → confirmar que
   la sintonía sigue aplicada (no solo carpet/white).
2. **Deshabilitar saca de la línea:** que un sensor OFF cambie de verdad `LineStatusV2`
   (centroide/`cross_track`) hacia CENTRAL, no solo el dibujo.
3. **Saturación sobre placa real:** mín → todos blanco, máx → ninguno (en `--sim` no se ve).
4. (opcional) Repetir el set en R2.

---

## Docs

- **Corregido un error factual** en `docs/firmware/USO-MONITOREO-Y-TELEMETRIA.md`: decía que
  `down_debug_telemetry` "no compila / no se verificó con pio / único pendiente conocido" —
  acabo de compilarlo (SUCCESS). Actualizada la fila de troubleshooting y la sección 7.
- **Agregados los comandos v3** (`SENS GLOBAL/SET`, `SENSOR ON/OFF`) a la tabla de comandos de
  esa guía, con flag a **TASK-307** (la reconciliación del flujo canónico reflasheo-vs-en-vivo
  la decide Gustavo; no la toqué unilateralmente).
- `README` de la app y `TELEMETRIA-DOWN.md` ya estaban en v3 (sesión previa).

---

## Notas / riesgos

- En `--sim` los sliders/toggle no tienen efecto (el simulador no aplica sensibilidad ni recibe
  comandos); la saturación/efecto solo se ve sobre la placa real.
- La EEPROM subió v1→v2: una calib guardada vieja (v1) se rechaza limpio → recalibrar una vez.
- Sigue abierto el **conector USB flojo de la DOWN** (TASK-306, banco 2026-06-12) — si la app
  se desconecta sola en banco, es eso, no la app.

---

## Verificación adversarial competencia↔debug (workflow 5+1 agentes, 2026-06-13)

Gustavo validó en banco (R1 real, `down_debug_telemetry`) que la persistencia (setup +
sensores deshabilitados) y el deshabilitar funcionan, y preguntó si puede confiar en que el
binario de **competencia** (`down`/`down_robot2`) se comporta igual. Se verificó
adversarialmente leyendo el código (5 verificadores con lentes distintas + síntesis):

**Veredicto: `minor_channel_differences_safe`, confianza alta, 0 divergencias de lógica.**

- Toda la sintonía fina está gateada por `#if defined(DOWN_DEBUG_TELEMETRY) || defined(DOWN_USB_MONITOR)`
  (un **OR**), y `[env:down]` define `DOWN_USB_MONITOR` → se compila en partido. El boot
  (`comm_central_load_persisted_calib`) y `dm_update` son **ungated** → corren en los 3 envs.
- **Hallazgo importante (corrige un supuesto mío):** `down_debug_telemetry` HEREDA
  `DOWN_USB_MONITOR` (hace `extends = env:down` + agrega solo `-DDOWN_DEBUG_TELEMETRY`). Las 14
  apariciones de `DOWN_DEBUG_TELEMETRY` en `src/` están TODAS dentro de un OR con
  `DOWN_USB_MONITOR`; ninguna rama de comportamiento depende solo de él. ⇒ el binario de banco
  **ya arranca dormido y con auto-off de 3 s, igual que competencia**: el banco ya ejercitó el
  wake/calibración de partido. `DOWN_DEBUG_TELEMETRY` es hoy casi no-op.
- **Diferencias reales = solo de CANAL, no de lógica:** monitor arranca dormido (app lo
  despierta con STREAM ON + PING@1s vs timeout 3s); el auto-off solo limpia `g_stream_on`, NO
  toca `g_dm` ni la EEPROM (un `CAL SAVE` hecho no se pierde).
- **Doc/comentario stale corregidos** (este commit): el header de `down_telemetry_serial.cpp`
  decía "debug = stream ON desde el boot" (falso) y la guía de uso esperaba el banner
  "[DOWN-TELEM] v1 ready" (muerto: ambos imprimen "[DOWN-MONITOR] dormido").

### Procedimiento de humo sobre el binario de competencia (pendiente equipo, TASK-306)

1. `pio run -e down -t upload` (NO down_debug_telemetry). El banner NO distingue los binarios
   (ambos "[DOWN-MONITOR] dormido") → confiar en el env del `pio run`, no en el serial.
2. `python -m monitor_base --port COMx` → la app manda STREAM ON + PING sola; confirmar que
   llegan frames (sin app no se ve nada = esperado, está dormido).
3. Anotar `threshold[]` / `enabled_bits` / `persensor_sens[]` iniciales.
4. `SENS GLOBAL 30` + `SENS SET 5 -40` → confirmar que cambian global y `persensor_sens[5]` +
   se mueve `threshold[5]`.
5. `SENSOR DISABLE 10` → confirmar bit 10 OFF Y que `cross_track`/`sensors_on_line` cambian al
   pasar la línea por ese sensor (efecto real de la exclusión, `down_model.cpp:180`).
6. `CAL SAVE` → **leer el ACK** `[DOWN] calib + sintonia persistida en EEPROM` (no confiar en
   el envío del comando).
7. Power-cycle (desenchufar ~5 s; de paso, a los 3 s el robot vuelve a modo partido solo).
8. Reabrir la app → confirmar que `threshold[]`/`enabled_bits` (sensor 10 OFF)/`persensor_sens`
   (sensor 5 = −40, global = 30) reportan lo guardado. Cierra la persistencia en competencia.
9. (Opcional R2) Ídem `pio run -e down_robot2 -t upload` (OTOS N/A; warnings de redefine de
   `DOWN_NUM_OTOS_CONNECTED` son esperados/inofensivos).

---

## Addendum — restaurado stream-desde-boot en `down_debug_telemetry` (2026-06-13)

Al abrir el monitor serie crudo sobre la base, Gustavo no veía valores. Diagnóstico: el binario
de banco había quedado **dormido** porque `down_debug_telemetry` hereda `DOWN_USB_MONITOR`
(TASK-306 lo agregó a `[env:down]` el 06-12) → el monitor crudo no lo despierta (solo la app, o
`STREAM ON\n` a mano). Eso fue un **retroceso del flujo de banco** (antes el debug streameaba
desde el boot).

**Fix (a pedido de Gustavo):** precedencia `DOWN_DEBUG_TELEMETRY` > `DOWN_USB_MONITOR` vía un
macro interno `DOWN_MONITOR_SLEEPY` (= `USB_MONITOR && !DEBUG_TELEMETRY`). Ahora:
- `down_debug_telemetry` (banco): stream **ON desde el boot**, sin auto-apagado; banner
  `[DOWN-TELEM] v1 ready — stream ON desde boot (env debug)`. El monitor crudo ve valores sin app.
- `down` / `down_robot2` (competencia): **dormido byte-idéntico** (toman las mismas ramas que
  antes; `DOWN_MONITOR_SLEEPY` se define igual que el viejo `#ifdef DOWN_USB_MONITOR`); banner
  `[DOWN-MONITOR] dormido`.
- **El banner ahora SÍ distingue** banco de competencia (antes ambos decían "dormido").

Verificado: `pio run -e down` y `-e down_debug_telemetry` → SUCCESS. `down` byte-idéntico por
construcción (no re-validar hardware). El efecto en banco (que el monitor crudo chorree valores)
lo confirma el equipo al abrir el monitor. Comentario header + guía de USO actualizados en el
mismo commit.

---

## Addendum 2 — wake por ENTER en competencia (2026-06-13, pedido de Gustavo)

Para poder monitorear la base con un monitor serie crudo SIN la app y SIN tipear `STREAM ON`,
y manteniendo el robot **match-safe** (boot dormido):

**Cambio (en `down_telemetry_serial.cpp`, path compartido → aplica a `down`/`down_robot2`):**
- `dispatch()`: **cualquier línea** del host (incluso un Enter vacío) renueva el latido y
  **PRENDE el stream**, EXCEPTO `PING` (latido puro: no re-prende si se pausó) y `STREAM OFF`.
- `pump_rx()`: **CR o LF** terminan la línea (antes solo LF) → un Enter despacha sea cual sea
  el fin-de-línea del monitor; la línea vacía también despacha (despierta).

**Conducta resultante (queda así):**
- Boot: dormido en competencia (sin datos) / prendido en banco — sin cambios.
- Competencia + monitor serie: **un ENTER → stream por 3 s**; repetir Enter lo mantiene. No
  hace falta `STREAM ON`.
- App: igual que antes (STREAM ON + PING cada 1 s la mantienen prendida; al desenchufar, 3 s
  → modo partido).
- `STREAM OFF` sigue pausando; `RATE <hz>` baja la tasa si 20 Hz es mucho para leer a ojo.

⚠️ **CAMBIA EL BINARIO DE COMPETENCIA** (`down`/`down_robot2`) — ya NO es byte-idéntico. Es
match-safe (en partido no hay USB conectado → nunca llega una línea → nunca despierta), pero
**lo valida el equipo en banco** (no lo cierro yo). No hay test host (es glue Arduino).

### Test plan — wake por Enter (equipo, banco)

**Subsistema:** comm/telemetría USB DOWN. **Robot:** R1 (`down`) y opcional R2 (`down_robot2`).
**Setup:** placa DOWN flasheada con `pio run -e down -t upload`; batería cargada (>7,6 V) para
ver valores reales; monitor serie con fin-de-línea LF, CR o CRLF (cualquiera).
**Pasos / criterio de aceptación:**
1. Abrir el monitor, apretar reset → ver `[DOWN-MONITOR] dormido`. **Sin tocar nada: NO llega JSON** (dormido OK).
2. Apretar **Enter** (línea vacía) → empieza a salir JSON. A los ~3 s sin tocar nada, **para solo**. ✔
3. Apretar Enter de nuevo → vuelve a salir 3 s. Mantener Enter cada <3 s → stream continuo. ✔
4. Tipear `STREAM OFF`+Enter → para; tipear `STREAM ON`+Enter → arranca. ✔
5. Abrir la app (`python -m monitor_base --port COMx`) → stream continuo + grabación, igual que antes. ✔
6. **Regresión partido:** sin USB conectado, el robot juega igual (la base manda LineStatusV2 a CENTRAL como siempre; el wake no dispara sin host).
**Doc esperada:** confirmar en el journal que los 6 pasos dan lo esperado; recién ahí queda cerrado.
