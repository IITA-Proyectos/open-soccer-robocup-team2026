# Guía de USO — Software de monitoreo / telemetría (app de PC)

**Para qué:** ver de forma visual y calibrar lo que leen las placas en banco (anillo de 32
sensores, línea, OTOS, cámaras, ToF) usando la app `tools/monitor-base/`.

Este documento responde, simple y al pie de la letra: **¿hay que reflashear? ¿cómo vuelvo al
firmware de competencia? ¿es un modo de diagnóstico o un software aparte?** — y deja los
**procedimientos exactos** para activar, usar y desactivar. Contratos técnicos:
[`TELEMETRIA-DOWN.md`](TELEMETRIA-DOWN.md) / [`TELEMETRIA-TOP.md`](TELEMETRIA-TOP.md).
App: [`tools/monitor-base/README.md`](../../tools/monitor-base/README.md).

---

## TL;DR — las 3 dudas, resueltas

1. **¿Hay que reflashear la placa?** **Depende de la placa.**
   - **ABAJO: NO.** El binario de competencia `down` (o `down_robot2`) ya lleva un **monitor
     USB dormido**: arranca callado y se **enciende solo** al conectar la app (o con un Enter en
     un monitor serie). Mismo binario juega y monitorea — no hay env de banco aparte.
   - **ARRIBA: SÍ, una vez.** Ahí la telemetría sí es un **modo que se elige al compilar** (flag
     de build): para usar la app con la TOP real flashás el env `top_*_debug_telemetry`. (Una
     vez flasheado, el stream se pausa/reanuda por comando `STREAM OFF/ON`, pero eso NO lo apaga
     del binario — sólo deja de emitir.)

2. **¿Cómo recupero el firmware anterior (competencia)?** **Reflasheando el env de competencia.**
   No hay "backup" que restaurar: el **código fuente es la única fuente de verdad** y flashear es
   repetible. `pio run -e down -t upload` (o `-e top_robot1`) deja el binario de competencia
   **byte-idéntico** al de antes. **No se pierde nada** (ver punto sobre calibración abajo).

3. **¿Es un modo de diagnóstico del software o un software alternativo?** **Ninguno de los dos
   en el sentido habitual.** La telemetría vive *dentro* del **MISMO firmware de competencia**.
   Es **distinta** de los sketches `diag_*` (esos sí son programas standalone aparte). En
   **ABAJO** el binario de competencia `down` lleva el stream integrado (dormido hasta que la
   app/Enter lo despiertan); en **ARRIBA** el código está apagado con `#ifdef` salvo que
   compiles el env `top_*_debug_telemetry`. En ambos casos **el robot se comporta exactamente
   igual que en competencia** (manda a la CENTRAL, lee sensores) y *además* emite los datos por
   USB. Por eso ves lo que el firmware REAL piensa, no un test paralelo.

> **Bonus clave:** **la calibración NO se pierde al reflashear.** Se guarda en **EEPROM**, que
> es una zona aparte del programa y **sobrevive a un reflasheo normal**. Flujo recomendado
> (ABAJO): calibrás con la app sobre `down` → `CAL SAVE` → el mismo `down` **arranca con esa
> calibración** (la carga de EEPROM al boot), reflashees o no.

---

## 1. El modelo mental (1 minuto)

| Querés… | Compilás/flasheás el env | Telemetría USB | Binario |
|---|---|---|---|
| **Jugar / competir Y monitorear/calibrar** (placa ABAJO) | `down` (R1) / `down_robot2` (R2) | ✓ **dormida**, se despierta sola | el de competencia |
| **Jugar / competir** (placa ARRIBA) | `top_robot1` | ✗ apagada | el de competencia |
| **Monitorear** (placa ARRIBA) | `top_robot1_debug_telemetry` | ✓ encendida | competencia **+** stream USB |

- **Placa ABAJO: hay UN solo env, `down`** (el de competencia). El mismo binario que juega
  **también monitorea y calibra**: lleva un **monitor USB dormido** (flag `-DDOWN_USB_MONITOR`)
  que arranca **callado** (no manda nada → no afecta el partido) y se **enciende solo** cuando
  conectás la app `tools/monitor-base` (manda `STREAM ON`) o apretás **Enter** en un monitor
  serie crudo. Ya **no existe** un env de banco aparte para ABAJO.
- La placa ARRIBA sí conserva su modo de banco aparte (`top_*_debug_telemetry`, flag
  `-DTOP_DEBUG_TELEMETRY`); los envs `top_*` de competencia **no** lo pasan.
- El **mismo cable USB que usás para flashear** es el que la app lee (es el USB del Teensy).
- ⚠️ **Placa ARRIBA — competencia vs banco:** el env de telemetría hace un poco de trabajo
  extra por loop (emitir el JSON + leer comandos), así que **NO es byte-idéntico**. Para
  **partidos oficiales de ARRIBA, flasheá el env de competencia** (`top_robot1`); para
  banco/práctica usá el de telemetría. La placa ABAJO no tiene este dilema: juega y monitorea
  con el mismo `down`.

---

## 2. Antes de empezar (una sola vez)

1. **PC con Python 3** (ya instalado). La app no necesita nada raro para `--sim`/`--replay`.
2. Para conectar al **robot real** (`--port`): `pip install pyserial`.
3. **PlatformIO** (`pio`) para compilar/flashear el Teensy (lo que ya usás para el firmware).
4. **Saber el puerto COM** del Teensy. La forma más fácil: `python -m monitor_base --list-ports`
   (lista los COM y marca cuál parece el Teensy). O usá directamente **`--port auto`** y la app
   lo detecta sola. A mano: Windows → *Administrador de dispositivos* → *Puertos (COM y LPT)* →
   "USB Serial Device (COMx)". (Si conectás ABAJO y ARRIBA a la vez, son **dos COM distintos**.)

---

## 3. PROCEDIMIENTOS (copy-paste)

> Reemplazá `COMx` por tu puerto (ej. `COM5`). En PowerShell o CMD da igual. Las rutas con
> espacios van **entre comillas**.

### A) Activar el monitoreo de la BASE (placa ABAJO)
```powershell
# 1) Conectá por USB el Teensy de la placa de ABAJO.
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"
# 2) Compilar + flashear el binario de competencia (lleva el monitor USB dormido):
pio run -e down -t upload          # robot 2: pio run -e down_robot2 -t upload
# 3) (opcional) confirmar el binario: abrir el monitor. La placa arranca DORMIDA (no manda
#    nada) y dice "[DOWN-MONITOR] dormido" — ese banner confirma que flasheaste `down`.
#    Para despertar el stream desde el monitor crudo: apretá ENTER (cualquier línea) → manda
#    el JSON por 3 s; repetí Enter para que siga. La app lo hace sola (ver paso 4).
pio device monitor -b 115200      # <-- CERRALO antes del paso 4 (un solo programa por COM)
# 4) Correr la app (al conectar manda STREAM ON + PING → despierta la telemetría sola):
cd tools\monitor-base
python -m monitor_base --port COMx              # vista BASE (anillo + línea + OTOS + calib)
python -m monitor_base --arquero --port COMx     # vista ARQUERO (cross-track + OTOS izq/der)
```

### B) Activar el monitoreo de ARRIBA (placa TOP)
```powershell
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"
pio run -e top_robot1_debug_telemetry -t upload
cd tools\monitor-base
python -m monitor_base --top --port COMx          # radar del campo (pelota/arcos/heading)
```

### C) Calibrar la línea y que quede para competencia (lo más útil)
```powershell
# 1) Hacé el Procedimiento A (flashear down + abrir la app, que despierta la telemetría sola).
# 2) En la app (botones):
#    - Poné el robot sobre el carpet verde -> "Auto-calib ON"
#    - Pasá el robot por las líneas blancas unos segundos -> "Auto-calib OFF"
#    - "Guardar EEPROM"
#    (alternativa: "Calibrar CARPET" sobre verde + "Calibrar BLANCO" sobre una línea)
# 3) Para confirmar que persiste: "Cargar EEPROM" + apagar/encender la placa.
# 4) Volvé a competencia (la calibración QUEDA en EEPROM):
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"
pio run -e down -t upload
```

### D) Desactivar / volver al firmware de COMPETENCIA
**Placa ABAJO:** no hay nada que reflashear — el binario de competencia `down` es el que ya
estás corriendo. Para volver a modo partido, cerrá la app (o dejá de mandar Enter): si el host
se calla 3 s, el stream se duerme solo. (Re-flashear `down` sólo hace falta si cambiaste el
firmware.)

**Placa ARRIBA:** sí hay que reflashear el env de competencia para salir del modo telemetría:
```powershell
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"
pio run -e top_robot1 -t upload     # placa ARRIBA -> binario de competencia (byte-idéntico)
```
La calibración en EEPROM se mantiene en ambas placas.

### E) Sin el robot (desarrollar / mostrar / analizar)
```powershell
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026\tools\monitor-base"
python -m monitor_base --sim                 # base simulada
python -m monitor_base --arquero --sim        # arquero simulado
python -m monitor_base --top --sim            # campo simulado
python -m monitor_base --port COMx --record sesion.jsonl   # grabar una sesión real
python -m monitor_base --replay sesion.jsonl  # reproducirla después
python -m monitor_base --selftest             # chequeo sin ventana (debe decir [selftest] OK)
```

---

## 4. Comandos en vivo (dentro del modo telemetría)

Son botones de la app (o se pueden tipear en un monitor serie). **Sólo funcionan con el env de
telemetría flasheado**; no encienden ni apagan el modo, sólo lo operan:

| Comando | Qué hace | Dónde |
|---|---|---|
| `STREAM ON` / `STREAM OFF` | Reanuda / pausa el envío de datos | ambas |
| `RATE <hz>` | Cambia la frecuencia (1–200) | ambas |
| `CAL CARPET` / `CAL WHITE` | Calibra verde / blanco | base |
| `CAL AUTO ON` / `CAL AUTO OFF` | Captura min/max pasando el robot por la línea | base |
| `CAL SAVE` / `CAL LOAD` | Guarda / carga calibración de EEPROM (incluye la sintonía fina v3) | base |
| `OTOS RESET` | Pone la odometría en (0,0,0) | base |
| `IMU ZERO` / `IMU SAVE` | Re-cero del heading / guarda calib del BNO | top |

> **Despertar el stream de la placa ABAJO (`down`/`down_robot2`, monitor dormido):** NO hace
> falta tipear `STREAM ON`. En un monitor serie crudo, **un ENTER** (cualquier línea, incluso
> vacía; CR o LF) arranca el envío por **3 s**; repetí Enter para que siga. La app lo hace sola
> (manda `STREAM ON` al conectar + `PING` cada 1 s como latido). Si el host se calla 3 s, el
> stream se apaga solo → vuelve a modo partido **sin reflashear**. Es el **único** flujo: la
> telemetría de ABAJO vive en el binario de competencia `down`; ya no hay un env de banco aparte.

**Sintonía fina (schema v3, app monitor-base — banco 2026-06-13):**

| Comando | Qué hace | Dónde |
|---|---|---|
| `SENS GLOBAL <pct>` | Sensibilidad global al blanco, `−100…+100` (**+ = MENOS sensible**, sube el umbral; 0 = punto medio histórico). **Los extremos SATURAN** (−100 → todo blanco, +100 → nada). En la app: slider que se aplica **al mover** | base |
| `SENS SET <i> <pct>` | Sensibilidad del sensor `i` por separado (slider del inspector) | base |
| `SENSOR <i> ON` / `SENSOR <i> OFF` | Habilita / **deshabilita** el sensor `i` → un sensor OFF se **excluye del cálculo de línea** (como uno unhealthy). En la app: botón del inspector o **doble-click** en el anillo/barra | base |

> Defaults no-op: todos los sensores habilitados, sensibilidad 0 → umbral en el punto medio =
> comportamiento de competencia histórico. `CAL SAVE` persiste estas perillas en EEPROM y el
> firmware las **aplica al boot en TODOS los envs** (competencia incluida).
>
> La sintonía fina, como el resto de los comandos, viaja en el binario de **competencia**
> (`down`/`down_robot2`) vía el monitor USB dormido (`DOWN_USB_MONITOR`). **Hay un solo flujo
> canónico:** calibrás en vivo sobre `down` (conectando la app o despertando con Enter),
> `CAL SAVE` persiste en EEPROM y el mismo `down` arranca con esa calibración. No hay que
> reflashear ningún env de banco.

---

## 5. Diagnóstico de problemas (validado en escritorio)

| Síntoma | Causa probable | Solución |
|---|---|---|
| **Se queda en "iniciando…" / no arranca** (típico al conectar la batería) | Al conectar la batería la placa **se resetea y bootea** (~2 s en DOWN; la TOP ~40 s por los ToF) y el USB re-enumera | La app **ahora se reconecta sola** y **muestra lo que dice la placa** en la barra de estado (*«…la placa dice: [DOWN] calibrando carpet…»*). Esperá el boot; si se queda en una línea fija, esa línea te dice **dónde** se colgó — pasámela. |
| **Sin batería**: OTOS en `L:o-- R:o--`, línea `data_valid=0` / `CALIB_SUSPECT`, sensores en rojo | Los OTOS y los LEDs de los sensores se alimentan de la **batería, NO del USB** | **Alimentá el robot con la batería cargada** (>7,6 V). Con USB solo no arrancan los OTOS ni se separan verde/blanco. |
| Casi todos los sensores en **rojo "dead"** con el robot quieto | Falso positivo: un sensor "no varía" si **no lo movés** sobre una línea | **Ya corregido:** con el robot quieto la app dice *"robot QUIETO: movelo sobre la línea"* en vez de marcar muertos. Sólo marca DEAD si un sensor no varía **mientras otros sí** (robot moviéndose). |
| **No sé el COM** | — | `python -m monitor_base --list-ports` (marca el Teensy) o `--port auto`. |
| La ventana abre pero **no llegan datos** / "cross-track N/A" | COM equivocado, **otro programa tiene el puerto**, o (placa ARRIBA) no está flasheado el env de telemetría | **ABAJO:** la app despierta el stream sola (no hay que reflashear); revisá COM y que ningún otro programa tenga el puerto. **ARRIBA:** flashear `top_*_debug_telemetry`. En ambas, cerrar `pio device monitor` / Serial Monitor del Arduino IDE / otra instancia de la app; confirmar COM |
| `pyserial no está instalado` | Falta la lib (sólo para `--port`) | `pip install pyserial` (no hace falta para `--sim`/`--replay`) |
| `schema de telemetría no soportado: v=X (la app entiende v=Y)` | El firmware y la app son de **versiones distintas** del repo | Reflashear la placa desde el **mismo checkout** que la app, o `git pull`. *(Validado: firmware v1 + app v2 → la app lo rechaza limpio, sin mostrar basura.)* |
| Aparecen líneas raras / `[DOWN] ...` mezcladas | Son prints de boot del firmware | **La app las ignora sola** (filtra lo que no es telemetría). *(Validado.)* |
| `could not open port COMx` | El puerto está ocupado o no existe | Un solo programa por COM: cerrar monitor/IDE; verificar el COM. *(Validado: puerto inexistente → la app lo reporta en la barra de estado, no crashea.)* |
| `pio run -e down` da **error de compilación** | No esperado: **verificado compilando OK el 2026-06-13** (`down` y `down_robot2` → SUCCESS). Si falla, suele ser checkout desincronizado o Avast tocando el registry (TASK-025) | Pegar el error exacto y avisar. |
| **El robot se mueve** mientras flasheo/monitoreo | El env de telemetría corre el firmware **REAL** → reacciona al árbitro y sensores | Ruedas al aire / robot sujeto, o no alimentar los motores, durante el banco |
| Quiero volver a competencia y "no sé si quedó bien" | — | `pio run -e down -t upload` deja el binario **byte-idéntico**; nada que restaurar |

---

## 6. ¿Por qué vale la pena? (utilidad)

- Reemplaza "leer 32 números crudos en el Serial Monitor" por un **anillo/campo visual**.
- **Calibración asistida** que **persiste a competencia** (EEPROM) — el flujo del Procedimiento C.
- Mostrás **exactamente lo que viaja a la CENTRAL** (`LineStatusV2` / `WorldSnapshot`) → cazás
  bugs de datos "silenciosos".
- Diferencial **OTOS izquierdo/derecho** para probar el seguidor de línea del arquero.
- **Corre sin robot** (simulador + replay) → desarrollás y mostrás cuando quieras.
- Todo el núcleo está **host-testeado** (gate + pytest) y el **binario de competencia no cambia**.

## 7. Límites honestos / a tener en cuenta

- El **glue Arduino** (lo que prende el stream en el Teensy) **compila con `pio`** (verificado
  2026-06-13: `down` / `down_robot2` → SUCCESS). Lo que falta NO es
  compilación sino **validación de banco** de la sintonía fina v3: que la persistencia tras
  power-cycle y el deshabilitar-un-sensor surtan efecto sobre la placa real (ver TASK-306).
- **Placa ARRIBA:** el env de telemetría **no es byte-idéntico** (overhead del stream) →
  **partidos oficiales con el env de competencia** (`top_robot1`). La placa ABAJO juega y
  monitorea con el mismo `down` (monitor USB dormido, sin overhead mientras está callado).
- La app conecta a **una placa a la vez** (un COM). Para ver ABAJO y ARRIBA juntas, abrí **dos
  instancias** (una con `--port`, otra con `--top --port`).
- La **orientación de los rayos ToF** en el radar es un reparto cardinal **aproximado** (los
  números del panel son la fuente fiel); afinarlo es trabajo futuro.
- Hoy es **por USB (cable)**. La telemetría **en tiempo real inalámbrica** (CANbus + ESP32) es
  roadmap del año próximo (ver TDP §3.10 / §4.6).
