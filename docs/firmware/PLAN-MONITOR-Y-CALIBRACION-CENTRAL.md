<!-- Generado 2026-06-15 por un workflow de 6 agentes (5 auditorias en paralelo + sintesis), verificado contra el codigo (refs file:linea). PROPUESTA para arrancar en una proxima sesion. Revisado por el coach. -->

# Plan de implementación — Monitorear y calibrar la placa CENTRAL desde monitor-base

> **Estado:** PROPUESTA para arrancar en la próxima sesión. No requiere re-investigar: cada afirmación está verificada contra el código (rutas `file:línea` abajo).
> **Repo real:** `C:/Users/violl/iitasoccer/soccer-main/` (rama `main`, HEAD `9acf9dd` al momento de auditar). ⚠️ El cwd `futbol2026/open-soccer-robocup-team2026` es el repo greenfield casi vacío — NO trabajar ahí (es la "repo location trap").
> **Base de código:** todas las rutas son relativas a `software/teensy/Soccer 2026/` salvo que se diga "raíz del repo".
> **TASK asociada:** TASK-105 (rango CENTRAL 100-199). Ver §8 — este plan REEMPLAZA el diseño de la TASK-105 en dos puntos (nombre de flag y binario único vs aparte); hay que actualizar la TASK, no crear una nueva.

---

## 1. Resumen ejecutivo

**Qué se construye, en dos etapas:**

- **FASE 1 (lo que Gustavo quiere primero — monitorear/visualizar/loguear):** que la placa CENTRAL hable el mismo "idioma" (JSON Lines por USB) que ya hablan TOP y DOWN, para que la app `monitor-base` la muestre como **tercera placa**. La CENTRAL es el cerebro: decide la jugada (FSM), recibe el mundo del TOP, recibe línea/odometría del DOWN, y manda PWM a los 3 motores. Hoy todo eso solo se ve como **texto crudo** difícil de leer con el robot moviéndose, o queda en la **caja negra CSV offline**. La Fase 1 lo pone **en vivo, visual, con semáforos de salud y log para cazar datos espurios** — sin tocar la conducta del robot (todo gateado, binario de competencia byte-idéntico).

- **FASE 2 (calibrar parámetros de juego SIN reflashear):** hoy *cada* perilla del robot (potencia mínima de arranque, offset por motor, potencia de empuje, andar-derecho, umbrales de la FSM) es `constexpr` — cambiarla = recompilar + reflashear + esperar. La Fase 2 le da a la CENTRAL una **EEPROM + comandos `SET`** (copiando el patrón ya probado en DOWN y TOP), para barrer valores en cancha en segundos. Default de cada parámetro = valor de hoy = binario byte-neutro.

**Por qué en este orden:** la Fase 1 es **habilitante** de la Fase 2 — para calibrar a ciegas hace falta primero VER el efecto (PWM real por rueda, deriva, estado de la FSM). El propio análisis lo marca como riesgo: "exponer en telemetría los valores activos ANTES de hacerlos editables cierra el loop ver→calibrar y reduce el riesgo de un SET a ciegas". Y la Fase 1 es lo que pidió Gustavo primero.

**Lo que NO se construye (ya existe):** la **caja negra CSV** de la CENTRAL ya graba sense-decide-act a 50 Hz × 90 s y la vuelca en RUN→STOP. La Fase 1 **reusa sus mismos getters** — no la reemplaza. La diferencia: la caja negra es *offline y post-mortem*; el monitor es *en vivo*, y además expone **salud de enlaces** (frames/crc/resync), que la caja negra **no graba como números**.

**Esfuerzo honesto:** el 80% del trabajo y todo el riesgo de Fase 1 vive en el **firmware** (host-compilable, pero lo cierra el equipo en banco — jitter del USB, filtrado de `STREAM`/`PING`). La receta de la app es mecánica (la maquinaria multi-placa ya es genérica). Estimación: Fase 1 ≈ 3-5 días; infraestructura de Fase 2 (EEPROM + parser) ≈ 2 días; cada parámetro calibrable después ≈ horas.

---

## 2. Estado actual verificado (qué ya existe)

### 2.1 Caja negra de CENTRAL — YA IMPLEMENTADA (no es propuesta)

`src/central/blackbox.cpp`, gate `-DCENTRAL_BLACKBOX`:
- Ring en RAM2 (DMAMEM), `~50 Hz × 90 s = 4500 registros` (`blackbox.cpp:17-22`).
- Graba estado FSM + flags + hdg + ball + `cmd_vx/vy/w` + **PWM real por motor** (`motors_get_applied_pwm`, `blackbox.cpp:84,103-105`) + línea + OTOS + flag de emergencia.
- Volcado CSV automático en flanco RUN→STOP, o manual con `'d'`/`'x'` (`blackbox.cpp:70-71,145-147`).
- Doc: `docs/pruebas-banco/CAJA-NEGRA.md` (raíz del repo). Envs `central_robot2_demo_bb`, `central_robot1_arquero_demo_bb` (`platformio.ini:329-335`).
- **Lo que NO graba:** salud de enlaces TOP/DOWN como contadores (frames/crc/resync/lost). Solo va al texto del debug.

### 2.2 Monitor USB de CENTRAL — NO EXISTE (solo texto)

- La CENTRAL emite **solo texto humano** por USB: `[CENTRAL] loop=N role=ATK state=... top[rxB=..] down[rx=..] match=RUN hdg=.. otos=.. loop_us(max/avg)=..` cada 500 ms (`main_central.cpp:401-467`), gateado por `-DCENTRAL_DEBUG_SERIAL`.
- Ese flag se define en el env base `central_robot1` (`platformio.ini:180`) y se hereda → **hoy es byte-idéntico** en competencia. Comentario explícito de quick-win RT: para sacar el jitter hay que **borrar** ese flag tras validar en banco (`platformio.ini:185-188`).
- El handler de comandos USB lee **chars sueltos**: `'g'/'G'/'\n'/'\r'`=GO, `'s'/'S'`=STOP, `'d'/'x'`=caja negra (`main_central.cpp:265-275`). **NO** hay parser de líneas, **NO** responde `STREAM ON`/`PING`.
- ⚠️ **Trampa de la 'S' confirmada:** `c == 's'` dispara STOP (`main_central.cpp:268`). La app manda `STREAM ON\n` al conectar (`sources.py:400`) → la **'S' de STREAM frenaría al robot** si no se filtra primero en firmware. Mismo problema con cada `PING`.
- **NO existe** `telemetry_central.h` ni `protocol_central.py` (glob sin resultados; en `src/shared/` solo hay `telemetry_down.h`, `telemetry_top.h`, `telemetry_sat.h`).

### 2.3 El patrón a clonar — TOP (validado en banco 2026-06-14)

`src/top/top_telemetry_serial.cpp` + módulo puro `src/shared/telemetry_top.h`:
- **Dormido por default** (match-safe): 3 estados ASLEEP / MACHINE / HUMAN (`top_telemetry_serial.cpp:45-50`).
- `STREAM ON`/`PING` → JSON Lines 20 Hz; **auto-off a los 3 s** de silencio del host (`top_telemetry_serial.cpp:368-372`). En partido (sin USB) **nunca despierta**.
- Schema versionado `TELEMETRY_TOP_SCHEMA = 2` (`telemetry_top.h:19`); parser de comandos **modular puro** host-testeado (`tt_parse_command`, `telemetry_top.h:202`); enum tipado `TtCmd` (`telemetry_top.h:174-194`).
- `fill_frame()` arma el snapshot desde getters ya existentes (`top_telemetry_serial.cpp:61-189`).
- Contrato: `docs/firmware/TELEMETRIA-TOP.md` (existe).

### 2.4 Contratos de datos que la CENTRAL ya tiene en RAM

- **Estado FSM:** `strategy_get_state_name()` (`strategy.cpp:1843`), `strategy_get_role()` (`strategy.cpp:1840`).
- **PWM real por rueda:** `motors_get_applied_pwm(i)` (`motors_zircon.cpp:249`) — el mismo que usa la caja negra.
- **Salud TOP→CENTRAL:** `comm_top_get_bytes_received/frames_received/crc_errors/resync_events/snapshot_size_rejects` (`comm_top.cpp:78-82`).
- **Salud DOWN→CENTRAL:** `comm_down_get_frames_received/crc_errors/frames_lost/resync_events` (`comm_down.cpp:129-132`). ⚠️ `frames_lost` **se solapa con crc**: huecos reales ≈ `lost − crc` (`comm_down.cpp:27-46`). No sumar lost+crc en la app.
- **Mundo recibido del TOP:** WorldSnapshot v3 aplicado por `comm_top_tick()` al world_model (`main_central.cpp:247`), legible por `world_model_get_*`.
- **OTOS / heading:** `world_model_otos_is_fresh()` + `world_model_get_otos_heading_deg()` (`main_central.cpp:452-457`). En R1 sin BNO, el yaw OTOS **es** el rumbo del delantero.
- **Loop-time:** `g_loop_monitor.max_us` / `.ema_us` (`main_central.cpp:463-465`).
- ⚠️ **`my_x`/`my_y` del snapshot = 0** (GAP-002 del TOP: heading sí, pose no). Documentar en la app para no confundir con bug del stream.

### 2.5 La app `monitor-base` — maquinaria multi-placa YA genérica

`tools/monitor-base/monitor_base/`:
- **Sniff por línea:** `auto_parse()` mira `"ring"` → DOWN, `"cam"` → TOP, sino `ProtocolError` (`sources.py:112-121`). Falta una 3ª rama para CENTRAL con **clave exclusiva**.
- **Handshake ya genérico:** `SerialSource` manda `STREAM ON` una vez (`sources.py:400`) + `PING` cada 1 s (`sources.py:412`), **independiente de la placa** → reusable sin tocar la app.
- **`board_of()` binario:** `"down" if DownFrame else "top"` (`gui_shell.py:40-42`). Hay que agregar la 3ª rama `central`.
- **Dicts hardcodeados a (top,down):** `BOARD_LONG`/`BOARD_SHORT` (`gui_shell.py:36-37`), `last_by_board`/`metrics_by` (`gui_shell.py:91-93`). Agregar `'central'` a los 4.
- **Registro de paneles:** `_registry()` lista tuplas `(modname, clsname)` con import perezoso tolerante a fallos (`gui_shell.py:45-67`); la nav agrupa sola por `board`.
- **Molde de panel:** `panel_base.py:58` (`board = "down"`), declara su `board` + render(frame).
- **Parser de referencia:** `protocol_top.py:245-330` (`parse_obj_top`/`parse_line_top`, valida `"v"`, rechaza schema desconocido).
- **Precedente reciente:** TASK-209 agregó la vista `--top-salud` **dentro de** `monitor-base` (no un binario aparte) — `gui_top_health.py`, `panel_health.py`. Es el patrón a seguir.

### 2.6 Parámetros calibrables de Fase 2 — hoy todos `constexpr`

- `MOTOR_MIN_PWM[3] = {70,70,107}` R1 (`config_central.h:66`) / R2 (`config_central.h:103`). Comentario pide tunear en banco; ⚠️ tope ~150 (motores 5V@7,4V se queman).
- `MOTOR_EFF_X100[3] = {100,100,131}` R1 (`config_central.h:74`) / `{100,100,115}` R2 (`config_central.h:122`). Comentario documenta el A/B 131→115 de banco para enderezar el strafe.
- `ATK_PUSH_SPEED_MM_S = 700` (`strategy.cpp:195`), `ATK_KICK_DIST_MM = 80` (`strategy.cpp:187`), `ATK_SEARCH_SPIN_PWM = 40` (`strategy.cpp:161`).
- `GK_GOTO_LINE_VX_TRIM_MM_S = 0` (`strategy.cpp:280`, máx ±19), `GK_GOTO_LINE_HEADING_TRIM_DEG = 0` (`strategy.cpp:286`), `GK_PFM_KD_RATE = 0.30` (`strategy.cpp:1236`).
- `GK_CLEAR_TRIGGER_MM = 250` (`strategy.cpp:412`).
- **CENTRAL no usa EEPROM en ningún archivo** (grep `EEPROM.` solo da hits en TOP y DOWN). El patrón a copiar: `src/down/eeprom_calib.h` (`ec_load_calibration`/`ec_save_calibration`, save **solo manual** para no matar la flash, `eeprom_calib.h:24-49`) y `top_eeprom_config.h`.

---

## 3. FASE 1 — Monitoreo / salud / visualización / log

> Objetivo: ver en vivo, con la app, lo que la CENTRAL decide y la salud de sus enlaces; loguear para cazar datos espurios. **Cero cambio de conducta** (todo gateado/dormido).

### Decisión de diseño base (resolver primero, ver §8)

1. **Flag:** usar **`-DCENTRAL_USB_MONITOR`** (clon del `TOP_USB_MONITOR`, dormido+match-safe), **no** el `-DCENTRAL_DEBUG_TELEMETRY` que propone la TASK-105 (ese nombre sugiere "siempre encendido" estilo el viejo `DEBUG_SERIAL`). Documentar el cambio en la TASK-105.
2. **Binario único, dormido + wake** (como DOWN/TOP), no un binario aparte: el monitor viaja EN los envs `*_bb` existentes (mismos que la caja negra) y arranca dormido. Un solo flujo USB convive con `d`/`x`/`g`/`s`.
3. **Vista DENTRO de `monitor-base`** (como TASK-209 `--top-salud`), no `tools/monitor-central/` aparte.
4. **Clave de sniff exclusiva:** top-level **`"central":1`** en el JSON (ni TOP `"cam"` ni DOWN `"ring"` la tienen). Requisito, no detalle.
5. **JSON Lines** (no binario): el USB CDC sobra a 20-50 Hz; reusa toda la cadena de parsers.

### Features (cada una: firmware / app / dato / esfuerzo / prioridad / validación)

| # | Feature | Firmware | App | Dato | Esf. | Prio | Se valida contra |
|---|---------|----------|-----|------|------|------|------------------|
| F1.1 | **Stream JSON v1 + handshake + filtrar STREAM/PING** (la pieza habilitante) | NUEVO `src/shared/telemetry_central.{h,cpp}` (POD + serializador + parser, puro) + glue `src/central/central_telemetry_serial.cpp` gateado `-DCENTRAL_USB_MONITOR`, dormido+auto-off 3 s, clonado de `top_telemetry_serial.cpp`. Rutear líneas al parser **antes** de los chars `g/s/d/x` para que `STREAM`/`PING`/`STREAM OFF` no caigan como STOP. Init en `setup()`, tick en `loop()`. | `protocol_central.py` (parser puro) + 3ª rama en `auto_parse` (`"central"`) | Ninguno nuevo (todo por getter) | **L** | **P0** | `top_telemetry_serial.cpp` (banco 2026-06-14); `sources.py:112-121,400-412`; `main_central.cpp:265-275` |
| F1.2 | **Estado FSM + comando aplicado + PWM real por rueda** | `fill_frame`: `state=strategy_get_state_name()`, `role=strategy_get_role()`, `cmd` = el `MotorCommand` **aplicado** del último tick (cachearlo del `cmd` de `main_central.cpp:385`), `pwm[i]=motors_get_applied_pwm(i)`, `spin_pwm` | Panel "Cerebro CENTRAL": estado FSM resaltado + barras de PWM por rueda + cmd vx/vy/ω | El `MotorCommand` aplicado (hoy local del loop) cacheado al monitor | **S** | **P0** | `blackbox.cpp:84,103-105`; `strategy.cpp:1840,1843`; `motors_zircon.cpp:249` |
| F1.3 | **Salud de enlaces TOP/DOWN + frescura + OTOS** (la mitad que la caja negra NO graba) | `fill_frame`: contadores `comm_top_get_*` y `comm_down_get_*`, `age_ms = millis()−last_rx`, `snap_fresh`/`line_fresh`/`valid`, `loop_us(max/avg)`, OTOS gateado por frescura | Panel con **semáforos** fresh/stale por enlace + Hz (calculado en PC) + `lost` y `crc` **separados** + dial yaw OTOS | Ninguno nuevo | **S** | **P1** | `comm_top.cpp:78-82`; `comm_down.cpp:129-132`; `main_central.cpp:401-467` |
| F1.4 | **Cancha que la CENTRAL RECIBE del TOP** (eco del WorldSnapshot) | `fill_frame`: copiar el `g_snap` aplicado (pelota relativa, arco propio/rival, referee_cmd, flags) | Reusar `gui_field.py`/`panel_field.py` alimentado con el snap de CENTRAL (o panel propio) | Ninguno (snap ya en RAM) | **M** | **P1** | `types.h` WorldSnapshot v3; ⚠️ `my_x/y=0` (GAP-002) |
| F1.5 | **Eco de línea/OTOS del DOWN** (lo que la CENTRAL recibe de la base) | `fill_frame`: `data_valid`, ángulo, penetración, cross_track, event_flags de la línea + OTOS, **gateados por frescura** | Indicador línea válida/borde + vector de escape | Ninguno | **S** | **P1** | `comm_down.cpp`; `top_telemetry_serial.cpp:174-188` (mismo patrón) ⚠️ línea DOWN hoy v1 5B (GAP-005): campos v2 en N/A hasta migrar |
| F1.6 | **Log para cazar datos espurios** | — (firmware ya manda todo) | Rama `central` en `_log_anomalies` (`gui_shell.py:302-336`): detectar PWM-sin-cmd, hdg congelado, snap stale, flapping de estado, naranja fantasma. Reusar detectores de `analizar_corrida.py` | Ninguno | **S** | **P1** | `gui_shell.py:302-336`; `tools/blackbox/analizar_corrida.py` |
| F1.7 | **Routing de 3ª placa en la app** | — | `board_of()` 3 ramas (`gui_shell.py:40-42`); `'central'` en `BOARD_LONG`/`BOARD_SHORT`/`last_by_board`/`metrics_by` (`gui_shell.py:36-37,91-93`); registrar paneles en `_registry()` | La clave de sniff | **S** | **P0** | `gui_shell.py:36-42,45-67,91-93` |
| F1.8 | **registrar_placa --board central + seed** | — | `choices=('top','down','central')` (`registrar_placa.py:38`) + label CENTRAL; sembrar los 2 seriales USB de los Teensy CENTRAL | N° de serie USB de cada CENTRAL (se lee con la placa enfrente) | **S** | **P2** | `registrar_placa.py:31-38`; `robot_registry.py:28-33,49-62` |
| F1.9 | **SimulatorCentral + golden para CI** | — | `simulator_central.py` (análogo a `simulator_top.py`) + `golden_central_v1.jsonl` + extender `gui_shell.smoke` a 3 vías | El schema CentralFrame | **M** | **P2** | `gui_shell.py:451-499`; `simulator_top.py` |

**Quick-wins de salud (opcionales, ya parcialmente cableados, deciden con banco — TASK-104):** A1 = borrar `-DCENTRAL_DEBUG_SERIAL` del env de competencia (saca el jitter de 500 ms; en cancha no hay USB); A2 = `-DCENTRAL_TOP_RX_BIGBUF`. Validados en `docs/firmware/ARQUITECTURA-LAZO-CENTRAL-RT.md`. No son parte de este track pero el monitor los hace observables (`loop_us`, `rsy`).

---

## 4. FASE 2 — Calibración de parámetros de juego (sin reflashear)

> Objetivo: barrer perillas en cancha en segundos. **Aterrizar primero lo que pidió Gustavo:** offset por motor, potencia mínima de arranque, andar-derecho, potencia de pateo.

### F2.0 — Infraestructura EEPROM + comando SET (habilitante, P0 de Fase 2)

NUEVO `src/central/central_eeprom_config.{h,cpp}` (glue Arduino) + `src/shared/central_config.{h,cpp}` (módulo **puro**: struct + CRC + parser, host-testeable). Espejo EXACTO de `down/eeprom_calib.h` + `top/top_eeprom_config.h`:
- `struct CentralConfig { magic; version; ...campos...; crc; }`. **Default = los `constexpr` de hoy = binario byte-neutro.**
- `central_config_load()` en `setup()`; EEPROM vacía/inválida → cae a defaults (como `top_config_load`).
- `central_config_save()` **SOLO manual** (botón de la app), NUNCA en el tick (mata la flash — `eeprom_calib.h:24-26`).
- Parser tipado (estilo `TtCmd` de TOP/DOWN, no genérico): `SET FLOOR <i> <v>`, `SET EFF <i> <v>`, etc. **Clamps en el módulo puro** (host-testeable). Cambiar los `constexpr` candidatos a variables leídas del struct.
- ⚠️ Confirmar el **offset libre de la EEPROM emulada** del Teensy 4.1 antes de fijarlo (CENTRAL hoy no usa EEPROM → presumiblemente todo libre, pero documentarlo como hizo TOP con su EEPROM-MAP).

| # | Parámetro | Firmware | App | Esf. | Prio | Clamp de seguridad |
|---|-----------|----------|-----|------|------|--------------------|
| F2.1 | **Potencia mínima de arranque** `MOTOR_MIN_PWM[3]` | `constexpr`→`CentralConfig`, leído en `motors_zircon.cpp` (floor) | 3 sliders "piso motor 1/2/3" | S | **P1** | **[0,149]** (queman >~150) |
| F2.2 | **Offset de potencia por motor** `MOTOR_EFF_X100[3]` (endereza el strafe) | →`CentralConfig`, leído al armar `FloorScaleCfg.eff_x100` | 3 sliders "eff motor 1/2/3 (×100)" | S | **P1** | rango razonable (ej. 50-200) |
| F2.3 | **Andar derecho sin odometría** `GK_PFM_KD_RATE` + `VX_TRIM` + `HEADING_TRIM` | sacar de `#ifdef`/namespace a `CentralConfig` | 3 sliders al lado de la caja negra | S/M | **P1** | **VX_TRIM ±19 mm/s** |
| F2.4 | **Potencia de pateo/empuje** `ATK_PUSH_SPEED/MS` + back + (opc.) kickstart | →`CentralConfig` (solo delantero) | sliders velocidad/duración empuje y retroceso | S | **P2** | kickstart **≤150** si se expone |
| F2.5 | **Cablear cap 70% + hacerlo calibrable** (SEGURIDAD) | cablear `motor_power_cap.h` (ya testeado, NO cableado, `motors_zircon.cpp:29-36`) en `apply_pwm_to_motor`, `%` desde EEPROM | 1 slider "% cap potencia" | S | **P1** | default que NO recorte hasta validar banco |
| F2.6 | **Umbrales tácticos FSM** (CLEAR, KICK_DIST, SEARCH_PWM, factores amenaza) | bloque `constexpr`→`CentralConfig` por rol | vista "táctica" con sliders agrupados + presets | M/L | **P2** | coherencia trigger<release en el SET |

⚠️ **Trampa de config compartida:** `MOTOR_MIN_PWM`/`MOTOR_EFF_X100` de R2 son los mismos para arquero y delantero (rol = compile-time). El `SET` calibra **por robot físico**, no por rol. Documentarlo.

⚠️ **No crear 3ª fuente de verdad:** `src/shared/robot_config/robot2.h` (seed NO compilado) ya duplica MOTOR_MIN_PWM/EFF/WHEEL_ANGLES. La EEPROM calibrable y el robot-def deben converger — coordinar con `FUENTES-DE-VERDAD.md`.

---

## 5. Plan de trabajo concreto (orden de archivos)

### Sprint A — Firmware Fase 1 (lo cierra el equipo en banco)

1. **`src/shared/telemetry_central.h`** — schema v1. POD `CentralTelemetryFrame` (campos abajo) + `enum TcCmd` tipado + `tc_serialize_jsonl` + `tc_parse_command` + `tc_frame_init`. Clave raíz `"central":1`.
2. **`src/shared/telemetry_central.cpp`** + **`test/test_telemetry_central.cpp`** (host-native, genera golden).
3. **`src/central/central_telemetry_serial.{h,cpp}`** — glue, clon de `top_telemetry_serial.cpp` (ASLEEP/MACHINE/HUMAN, auto-off 3 s, `fill_frame` desde getters). Gate `-DCENTRAL_USB_MONITOR`.
4. **`main_central.cpp`** — `central_telemetry_init()` en `setup()` (tras comm init); `central_telemetry_tick()` en `loop()`; **rutear líneas al parser antes** del bloque `while(Serial.available())` de chars `g/s/d/x` (resolver el conflicto 'S').
5. **`platformio.ini`** — agregar `-DCENTRAL_USB_MONITOR` a los envs `*_bb` (NO a `central_robot1`/`robot2` de competencia). ⚠️ aquí está la **única colisión real con la rama RT** (ver §6).

**Contrato JSON v1 (esquema exacto, top-level `"central":1`):**
```
{"central":1,"v":1,"seq":N,"t_ms":N,
 "fsm":{"role":"ATK|GK","state":"...","match":0|1},
 "cmd":{"vx":mm/s,"vy":mm/s,"w":cdeg/s,"spin":pwm},
 "pwm":[p1,p2,p3],                          // motors_get_applied_pwm, signed
 "top":{"rxB":N,"fr":N,"crc":N,"rsy":N,"badsz":N,"fresh":0|1,"age":ms},
 "down":{"rx":N,"crc":N,"lost":N,"rsy":N,"badsch":N,"line_fresh":0|1,"valid":0|1,"ev":hex,"age":ms},
 "otos":{"fresh":0|1,"hdg":deg},
 "snap":{...pelota/arcos/referee/flags...},  // eco del WorldSnapshot (my_x/y=0, GAP-002)
 "loop":{"max_us":N,"ema_us":N}}
```
Reglas: `v` y `central` arriba (lo exige `is_telemetry_line`: empieza con `{` y trae `"v":`). Campos aditivos = no bump. Un bump = reflashear firmware + app + golden juntos.

### Sprint B — App Fase 1 (host-testeable, NO necesita banco)

6. **`monitor_base/protocol_central.py`** — `CentralFrame` dataclass (con `.seq` y `.t_ms`, los usa `_pump`) + `parse_obj_central`/`parse_line_central`, valida `"v"`, rechaza schema desconocido. Patrón `protocol_top.py:245-330`.
7. **`tools/.../tests/golden_central_v1.jsonl`** + `test_protocol_central.py`.
8. **`sources.py`** — 3ª rama en `auto_parse`: `if '"central"' in line: return parse_line_central(line)` + import. Test en `test_sources.py`.
9. **`gui_shell.py`** — `board_of()` 3 ramas; `'central'` en los 4 dicts; tuplas de paneles en `_registry()`; rama `central` en `_log_anomalies`.
10. **`monitor_base/panel_central.py`** (Cerebro: FSM + PWM + cmd) y **`panel_central_health.py`** (semáforos enlaces + OTOS + loop_us). `board="central"`. Molde `panel_base.py`.
11. **`registrar_placa.py`** + `robot_registry.py` — `--board central` + seed de los 2 seriales.
12. **`simulator_central.py`** + extender `gui_shell.smoke` (P2, para CI).

### Sprint C — Fase 2 (después de validar Fase 1 en banco)

13. `src/shared/central_config.{h,cpp}` + `test/test_central_config.cpp` (struct+CRC+parser+clamps).
14. `src/central/central_eeprom_config.{h,cpp}` (glue EEPROM).
15. `main_central.cpp` — `central_config_load()` en setup; extender parser de comandos con `SET *`.
16. Migrar parámetros `constexpr`→variables, uno por commit, default byte-neutro, en el orden F2.5 (seguridad) → F2.1 → F2.2 → F2.3 → F2.4 → F2.6.
17. App: sliders + botón "guardar a EEPROM" (reusa el transporte serial existente).

---

## 6. Riesgos

1. **NO pisar la app de competencia (TASK-306):** `monitor-base` es la app que el equipo usa en Incheon (María: "la tenemos que usar en Corea"). Toda vista CENTRAL nueva es **aditiva** (tuplas en `_registry()`, paneles con `board="central"`); no toca DOWN/línea. Riesgo bajo si se respeta la maquinaria genérica. Correr la suite pytest del monitor antes de cerrar.
2. **El banco lo cierra el equipo, no Claude (regla dura del repo):** todo el firmware de Fase 1/2 es host-compilable, pero "pio SUCCESS" / tests verdes **no prueban** que ande. Lo que SOLO valida el equipo: jitter del USB bajo el stream, filtrado de `STREAM`/`PING` (que no frene el robot), 3ª placa hot-swapeando en vivo, cada parámetro calibrable, y el cableado del cap 70%. Claude NO marca esas TASKs `done`.
3. **Colisión con la rama RT — ACOTADA, no bloqueante (verificado):** la rama `feat/integracion-rt-gateada-2026-06-15` toca 50 archivos, pero de CENTRAL **NO toca `main_central.cpp`, `strategy.cpp` ni `motors_zircon.cpp`** — solo `diag_central_blink.cpp` y **quita 13 líneas de `platformio.ini`** (envs). La **única colisión real** es en `platformio.ini` (donde van los envs nuevos). Mitigación: antes de tocar `platformio.ini` o `src/shared/`, `git fetch && git log origin/main -10 -- <archivo>`; coordinar el merge del env nuevo con quien lleve la rama RT. El miedo genérico "la rama RT reescribe la arquitectura" **no se confirma** sobre los archivos que toca este plan.
4. **Trampa de la 'S' (STOP espurio):** confirmada (`main_central.cpp:268` vs `sources.py:400`). **Filtrar `STREAM`/`PING` en firmware ANTES de exponer el stream**, o el monitor frena al robot al conectarse. Es requisito de la F1.1, no opcional.
5. **Jitter del USB:** el `DEBUG_SERIAL` se gateó porque ~30 prints/500 ms metían jitter en el loop de motores 100 Hz. Un stream a más Hz puede reintroducirlo → medir en banco; mantener gateado/dormido/byte-neutro en competencia.
6. **Byte-neutralidad (Fase 2):** cada default de `CentralConfig` = EXACTAMENTE el `constexpr` de hoy; EEPROM en blanco → defaults (como `top_config_load`). Verificar con build host + comparación de binario (como ya se hace en el repo). Un default mal puesto = regresión silenciosa.
7. **Clamps de seguridad NO negociables en el SET** (en el módulo puro): PWM ≤~149, VX_TRIM ±19. Un SET sin clamp puede freír un motor.
8. **EEPROM se desgasta:** `central_config_save()` SOLO manual (botón), nunca por slider-move.
9. **3ª fuente de verdad:** `robot2.h` seed duplica parámetros; la EEPROM debe converger con el robot-def, no contradecirlo.

---

## 7. Plan de prueba en hardware real (criterios medibles)

> Diseñado para el equipo (Virginia/Elías). Protocolo: `.claude/skills/hardware-test-protocol`. Lo cierra el equipo, no Claude.

**T1 — Byte-neutralidad (gate antes de todo):** `pio run -e central_robot1` y `-e central_robot2` (competencia, flags OFF) producen binario **byte-idéntico** al de HEAD. ✅ si `cmp` da 0 diferencias.

**T2 — Stream no frena el robot (la 'S'):** flashear un env `*_bb` con `-DCENTRAL_USB_MONITOR`. Conectar la app. ✅ si el robot **NO entra en STOP** al conectar (el `STREAM ON` no dispara la 'S') y el estado FSM sigue corriendo.

**T3 — Dormido en partido:** con el monitor flasheado, **sin** app conectada (USB desconectado o app cerrada). ✅ si por USB no sale **nada** y el robot juega normal (match-safe).

**T4 — Stream estable + jitter:** app conectada, `STREAM ON`. ✅ si llegan frames a tasa estable (≥15 Hz sin drops en la métrica de la app) **y** `loop_us(max)` no sube más de ~X% sobre el baseline sin monitor (medir baseline primero). ❌ si el jitter degrada el loop de motores.

**T5 — 3ª placa hot-swap:** con CENTRAL+TOP+DOWN registradas, mover el cable USB entre las 3. ✅ si la app cambia de vista sola (auto_parse rutea) sin reconfigurar.

**T6 — Salud refleja realidad:** desconectar el cable TOP→CENTRAL. ✅ si el semáforo TOP pasa a stale/rojo y `fr` deja de subir; reconectar → vuelve verde.

**T7 (Fase 2) — Calibrar sin reflashear:** `SET FLOOR 0 90` por la app, sin reflashear. ✅ si el PWM real del motor 0 (leído por el propio monitor, F1.2) cambia el comportamiento de arranque en vivo. Guardar a EEPROM, power-cycle, confirmar persistencia.

**T8 (Fase 2) — Clamps:** `SET FLOOR 0 200`. ✅ si el firmware clampea a 149 y la app lo refleja (no se manda PWM que queme).

---

## 8. Preguntas abiertas para Gustavo

1. **Nombre del flag:** ¿confirmás `-DCENTRAL_USB_MONITOR` (clon dormido del TOP) en vez del `-DCENTRAL_DEBUG_TELEMETRY` que dice la TASK-105? El primero es match-safe por diseño; el segundo arrastra la semántica "siempre encendido" del viejo `DEBUG_SERIAL`. Si aceptás, **actualizo la TASK-105** (no creo TASK nueva — la 105 ya cubre este track, P2, rango CENTRAL).
2. **Binario único vs aparte:** ¿vista CENTRAL **dentro de `monitor-base`** (patrón TASK-209 `--top-salud`, que recomiendo) o `tools/monitor-central/` separado (lo que literalmente dice la TASK-105)? El precedente reciente fue extender monitor-base.
3. **Prioridad real:** la TASK-105 es **P2** (el texto + caja negra alcanzaron para la práctica). ¿La subimos a P1 para antes de Incheon, o queda capitalizable a 2027? Define cuánto esfuerzo invertir ahora.
4. **Cap 70% (F2.5):** ¿cablear con default **0 (passthrough, byte-neutro)** o default **70 (protege ya, pero cambia el binario)**? Es decisión de seguridad + banco. El backlog (P0-6) dice que el delantero hoy sale SIN cap → riesgo de quemar motores en Incheon con PUSH a 700 mm/s.
5. **Seriales USB de los 2 Teensy CENTRAL** para el seed del registry (R1/R2): ¿los tenés a mano o se registran con `registrar_placa` con la placa enfrente?
6. **`SET` tipado vs genérico:** recomiendo **tipado** (`SET FLOOR/EFF/PUSH`, como el `TdCmd`/`TtCmd` de DOWN/TOP) porque valida rangos por grupo. ¿De acuerdo?
7. **Coordinación rama RT:** el único punto de roce es `platformio.ini`. ¿Mergeamos primero la rama RT y partimos de ahí, o agrego los envs en paralelo y resolvemos el conflicto de `platformio.ini` en el merge?

---

**Archivos clave (rutas absolutas, para arrancar la próxima sesión):**
- Patrón a clonar: `C:/Users/violl/iitasoccer/soccer-main/software/teensy/Soccer 2026/src/top/top_telemetry_serial.cpp` + `src/shared/telemetry_top.h`
- A extender (firmware): `src/central/main_central.cpp` (setup 158-211, loop 213-468, handler 265-275, debug 401-467), `src/central/config_central.h` (66/74/103/122), `src/central/strategy.cpp` (161/187/195/280/286/412/1236)
- A extender (app): `tools/monitor-base/monitor_base/{sources.py,gui_shell.py,protocol_top.py,panel_base.py,registrar_placa.py,robot_registry.py}`
- Patrón EEPROM: `src/down/eeprom_calib.h`, `src/top/top_eeprom_config.h`
- Ya existe (reusar): `src/central/blackbox.cpp`, `tools/blackbox/analizar_corrida.py`, `docs/pruebas-banco/CAJA-NEGRA.md`, `docs/firmware/TELEMETRIA-TOP.md`
- TASK a actualizar: `team-tasks/2026-06-12-task-105-central-telemetria-y-app-monitoreo-visual.md`