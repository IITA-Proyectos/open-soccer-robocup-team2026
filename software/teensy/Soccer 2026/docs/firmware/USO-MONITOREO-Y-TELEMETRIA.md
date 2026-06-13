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

1. **¿Hay que reflashear la placa?** **SÍ, una vez.** La telemetría es un **modo del firmware
   que se elige al compilar** (un flag de build), no un interruptor en vivo. Para usar la app
   con el robot real flashás el env `*_debug_telemetry` de esa placa. (Una vez flasheado, el
   stream se puede pausar/reanudar por comando `STREAM OFF/ON`, pero eso NO lo apaga del
   binario — sólo deja de emitir.)

2. **¿Cómo recupero el firmware anterior (competencia)?** **Reflasheando el env de competencia.**
   No hay "backup" que restaurar: el **código fuente es la única fuente de verdad** y flashear es
   repetible. `pio run -e down -t upload` (o `-e top_robot1`) deja el binario de competencia
   **byte-idéntico** al de antes. **No se pierde nada** (ver punto sobre calibración abajo).

3. **¿Es un modo de diagnóstico del software o un software alternativo?** **Ninguno de los dos
   en el sentido habitual.** Es una **variante de compilación del MISMO firmware de competencia**:
   el código de telemetría vive *dentro* de los archivos de competencia pero está apagado con
   `#ifdef` salvo que compiles el env de telemetría. Es **distinto** de los sketches `diag_*`
   (esos sí son programas standalone aparte). Con el env de telemetría, **el robot se comporta
   exactamente igual que en competencia** (manda a la CENTRAL, lee sensores) y *además* emite
   los datos por USB. Por eso ves lo que el firmware REAL piensa, no un test paralelo.

> **Bonus clave:** **la calibración NO se pierde al reflashear.** Se guarda en **EEPROM**, que
> es una zona aparte del programa y **sobrevive a un reflasheo normal**. Flujo recomendado:
> calibrás con la app (env telemetría) → `CAL SAVE` → reflasheás competencia → el firmware de
> competencia **arranca con esa calibración** (la carga de EEPROM al boot).

---

## 1. El modelo mental (1 minuto)

Cada placa tiene **un mismo firmware** que se puede compilar de dos maneras:

| Querés… | Compilás/flasheás el env | Telemetría USB | Binario |
|---|---|---|---|
| **Jugar / competir** (placa ABAJO) | `down` | ✗ apagada | el de competencia |
| **Monitorear/calibrar** (placa ABAJO) | `down_debug_telemetry` | ✓ encendida | competencia **+** stream USB |
| **Jugar / competir** (placa ARRIBA) | `top_robot1` | ✗ apagada | el de competencia |
| **Monitorear** (placa ARRIBA) | `top_robot1_debug_telemetry` | ✓ encendida | competencia **+** stream USB |

- El flag que enciende el modo es `-DDOWN_DEBUG_TELEMETRY` / `-DTOP_DEBUG_TELEMETRY`. Los envs
  `*_debug_telemetry` lo pasan; los de competencia **no**.
- **Con el flag apagado** (envs de competencia) el binario es **byte-idéntico** al de antes:
  todo el código nuevo está dentro de `#ifdef`. O sea, activar esto **no cambió** lo que juega.
- El **mismo cable USB que usás para flashear** es el que la app lee (es el USB del Teensy).
- ⚠️ **Competencia vs banco:** el env de telemetría hace un poco de trabajo extra por loop
  (emitir el JSON + leer comandos), así que **NO es byte-idéntico**. Para **partidos oficiales,
  flasheá el env de competencia**. Para banco/práctica/calibración, usá el de telemetría.

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
# 2) Compilar + flashear el modo telemetría:
pio run -e down_debug_telemetry -t upload
# 3) (opcional) confirmar que streamea: abrir el monitor. El env de banco arranca con el
#    stream PRENDIDO e imprime "[DOWN-TELEM] v1 ready — stream ON desde boot (env debug)"
#    seguido del JSON (sin necesidad de la app). El binario de COMPETENCIA `down` en cambio
#    arranca DORMIDO y dice "[DOWN-MONITOR] dormido" — ese banner distingue cuál flasheaste.
pio device monitor -b 115200      # <-- CERRALO antes del paso 4 (un solo programa por COM)
# 4) Correr la app:
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
# 1) Hacé el Procedimiento A (flashear down_debug_telemetry + abrir la app).
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
```powershell
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"
pio run -e down -t upload          # placa ABAJO  -> binario de competencia (byte-idéntico)
pio run -e top_robot1 -t upload     # placa ARRIBA -> binario de competencia (byte-idéntico)
```
No hay nada que "restaurar": esto **es** recuperar el firmware anterior. La calibración en
EEPROM se mantiene.

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
> ⚠️ Igual que el resto de los comandos, la sintonía fina también viaja en el binario de
> **competencia** (`down`/`down_robot2`) vía el monitor USB dormido (`DOWN_USB_MONITOR`); el
> flujo canónico (reflashear `down_debug_telemetry` vs calibrar en vivo sobre `down`) se está
> reconciliando en **TASK-307**.

---

## 5. Diagnóstico de problemas (validado en escritorio)

| Síntoma | Causa probable | Solución |
|---|---|---|
| **Se queda en "iniciando…" / no arranca** (típico al conectar la batería) | Al conectar la batería la placa **se resetea y bootea** (~2 s en DOWN; la TOP ~40 s por los ToF) y el USB re-enumera | La app **ahora se reconecta sola** y **muestra lo que dice la placa** en la barra de estado (*«…la placa dice: [DOWN] calibrando carpet…»*). Esperá el boot; si se queda en una línea fija, esa línea te dice **dónde** se colgó — pasámela. |
| **Sin batería**: OTOS en `L:o-- R:o--`, línea `data_valid=0` / `CALIB_SUSPECT`, sensores en rojo | Los OTOS y los LEDs de los sensores se alimentan de la **batería, NO del USB** | **Alimentá el robot con la batería cargada** (>7,6 V). Con USB solo no arrancan los OTOS ni se separan verde/blanco. |
| Casi todos los sensores en **rojo "dead"** con el robot quieto | Falso positivo: un sensor "no varía" si **no lo movés** sobre una línea | **Ya corregido:** con el robot quieto la app dice *"robot QUIETO: movelo sobre la línea"* en vez de marcar muertos. Sólo marca DEAD si un sensor no varía **mientras otros sí** (robot moviéndose). |
| **No sé el COM** | — | `python -m monitor_base --list-ports` (marca el Teensy) o `--port auto`. |
| La ventana abre pero **no llegan datos** / "cross-track N/A" | La placa NO está flasheada con el env de telemetría, o COM equivocado, o **otro programa tiene el puerto** | Flashear `*_debug_telemetry`; cerrar `pio device monitor` / Serial Monitor del Arduino IDE / otra instancia de la app; confirmar COM |
| `pyserial no está instalado` | Falta la lib (sólo para `--port`) | `pip install pyserial` (no hace falta para `--sim`/`--replay`) |
| `schema de telemetría no soportado: v=X (la app entiende v=Y)` | El firmware y la app son de **versiones distintas** del repo | Reflashear la placa desde el **mismo checkout** que la app, o `git pull`. *(Validado: firmware v1 + app v2 → la app lo rechaza limpio, sin mostrar basura.)* |
| Aparecen líneas raras / `[DOWN] ...` mezcladas | Son prints de boot del firmware | **La app las ignora sola** (filtra lo que no es telemetría). *(Validado.)* |
| `could not open port COMx` | El puerto está ocupado o no existe | Un solo programa por COM: cerrar monitor/IDE; verificar el COM. *(Validado: puerto inexistente → la app lo reporta en la barra de estado, no crashea.)* |
| `pio run -e down_debug_telemetry` da **error de compilación** | No esperado: **verificado compilando OK el 2026-06-13** (`down`, `down_robot2` y `down_debug_telemetry` → los tres SUCCESS). Si falla, suele ser checkout desincronizado o Avast tocando el registry (TASK-025) | Pegar el error exacto y avisar. **El env de competencia `pio run -e down` NO se ve afectado.** |
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
  2026-06-13: `down` / `down_robot2` / `down_debug_telemetry` → SUCCESS). Lo que falta NO es
  compilación sino **validación de banco** de la sintonía fina v3: que la persistencia tras
  power-cycle y el deshabilitar-un-sensor surtan efecto sobre la placa real (ver TASK-306).
- El env de telemetría **no es byte-idéntico** (overhead del stream) → **partidos oficiales con
  el env de competencia**.
- La app conecta a **una placa a la vez** (un COM). Para ver ABAJO y ARRIBA juntas, abrí **dos
  instancias** (una con `--port`, otra con `--top --port`).
- La **orientación de los rayos ToF** en el radar es un reparto cardinal **aproximado** (los
  números del panel son la fuente fiel); afinarlo es trabajo futuro.
- Hoy es **por USB (cable)**. La telemetría **en tiempo real inalámbrica** (CANbus + ESP32) es
  roadmap del año próximo (ver TDP §3.10 / §4.6).
