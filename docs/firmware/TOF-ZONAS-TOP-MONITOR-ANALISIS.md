---
title: "ToF VL53L7CX de la placa TOP: bloqueo de zonas, rotación, EEPROM — y por qué el monitor no coincidía con la TOP"
date: 2026-06-21
author: "Claude Opus 4.8 (Anthropic) — workflow de 7 lectores + verificación adversarial"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
status: vivo (análisis + referencia)
tipo: investigacion-y-referencia
fidelidad: "Escrito leyendo el CÓDIGO ejecutable (no comentarios). Las 10 divergencias TOP↔monitor se verificaron leyendo AMBOS lados del código + platformio.ini. Nada testeado en hardware (lo cierra el equipo)."
audiencia: "Virginia, Elías, quien tenga que HABILITAR el bloqueo de zonas de los ToF"
---

# Cómo funcionan los 4 ToF VL53L7CX de la placa TOP (y por qué el bloqueo de zonas no coincidía con el monitor)

> **Para quién es esto:** Virginia, Elías y cualquiera que tenga que *habilitar* el bloqueo de zonas de los ToF y entender por qué históricamente "el monitor mostraba un bloqueo y la TOP usaba otro". Está escrito fiel al código (cito `archivo:línea`), no a los comentarios. Donde el código y el comentario se contradicen, lo marco.
>
> **Glosario rápido (jerga explicada):**
> - **ToF** = *Time of Flight*. Sensor de distancia que mide cuánto tarda un pulso de luz infrarroja en ir y volver. El VL53L7CX no da *un* número: da una **matriz** de distancias (una por "zona").
> - **Zona** = cada celda de esa matriz. El nuestro está en **4×4 = 16 zonas** (NO 8×8, ver §2).
> - **Máscara (mask)** = un número binario donde cada bit dice "esta zona la uso (1) / la anulo (0)".
> - **Firmware** = el programa en C++ que corre dentro de la Teensy de la placa TOP.
> - **Monitor** = el programa Python (`tools/monitor-base`) que corre en la notebook y muestra/edita los sensores por USB.
> - **Crudo / canónico / display**: tres "marcos de referencia" de la grilla de zonas. Lo explico en §4; es **la clave del problema**.

---

## 1. Resumen ejecutivo

Hoy, en competencia, la TOP corre el env `top_robot2_pri` (es el `default_envs`, `platformio.ini:25,372`) en **ambos** robots. Cada uno de los 4 ToF se lee en **round-robin** (uno por tick) para no hundir el loop, se filtran sus 16 zonas crudas por `status` válido, y se reducen a **una** distancia promediando las zonas habilitadas. El "bloqueo de zonas" es una **máscara** de 16 bits por sensor que vive en RAM (efecto inmediato) y se persiste en EEPROM con `CFG SAVE`.

**Por qué fallaba ("el monitor muestra una cosa, la TOP usa otra"):** hay **dos causas reales**, ninguna es un bug de la máscara en sí.

1. **El firmware NUNCA le devuelve al monitor qué zonas tiene vetadas.** La telemetría (campo `"z"`) manda las 16 zonas **crudas y sin enmascarar** (`telemetry_top.cpp:127-141`). El monitor pinta el veto **desde su propio archivo `.json` local**, no desde la máscara real de la EEPROM. Si el `.json` de la notebook y la EEPROM de la placa están desincronizados (editaste en la app y no bajaste, bajaste y no hiciste `CFG SAVE`, o hubo `CFG RESET`/reboot), **ves un veto que la TOP no está aplicando**. No hay *readback*.

2. **Quién rota la grilla está partido en dos interruptores que deben coincidir a mano** y nada lo verifica: `FIRMWARE_OWNS_ROTATION` en Python (`tof_layout.py:52`, default `False`) y el flag de compilación `-DTOP_ENABLE_TOF_ROT` en el firmware (default **apagado**). Hoy ambos están en su default compatible → coinciden. Pero si flasheás `top_robot2_pri_zonas` o `_tofrot` (que **sí** traen `-DTOP_ENABLE_TOF_ROT`) sin poner la app en `True`, para rotaciones 90°/270° el veto cae en **zonas físicas opuestas** (doble rotación). Para 0°/180° "parece andar" por casualidad → el bug es intermitente y confunde.

**Veredicto a la pregunta transversal de Gustavo:** la *representación* (numeración de zona, convención de bits de la máscara, matemática de rotación) **SÍ coincide** entre firmware y monitor y tiene tests de paridad. Lo que **diverge** es (a) el monitor pinta su `.json`, no la EEPROM real, y (b) el acople "quién rota" no está enforced. Esos dos puntos son toda la historia del síntoma. Hay además un **bug der/izq** latente en el bearing (§4.1) que es ortogonal al veto pero hay que conocer antes de tocar la app.

---

## 2. Capacidades del VL53L7CX (lo relevante para zonas)

- **Matriz de distancias, no un número.** Cada lectura ("frame") trae una grilla de zonas, cada una con su distancia en mm y su `target_status`.
- **Resolución: 4×4 = 16 zonas.** `TOF_RESOLUTION_ZONES = 16` en el firmware. **El "8×8" es un mito**: aparece en un doc viejo y en el diag de banco `diag_top_tof_zonemap.cpp` (que corre 64 zonas, `RES=64`), pero el firmware vivo y todo el path de máscara usan 16. Esto importa: si alguna verificación de orientación se hizo en 8×8, **no se traslada directo** al 4×4 real.
- **Frecuencia.** 4×4 admite hasta 60 Hz; 8×8 hasta 15 Hz. El robot usa `TOF_RANGING_FREQ_HZ = 15`.
- **FoV (campo de visión): 90° en diagonal** (≈ 60°×60° en el cuadrado). Cada zona ve una porción angular del FoV.
- **`target_status` por zona** (qué tan confiable es esa celda): `5` = 100% válida; `6` ≈ 50% (típico del primer frame); `9` ≈ 50%. El robot acepta **`5 || 6 || 9`** (filtro permisivo) y mm en `[0, TOF_MAX_RANGE_MM=4000]`. Cualquier otra cosa → la zona se marca `TOF_NO_READING = 0xFFFF`.
- **Alcance vs luz.** Cae fuerte con iluminación: ~3 m en oscuridad → ~0,5 m a 5 klux. Relevante para Incheon (cancha más iluminada que el lab de Salta).
- **Clock I2C.** Carga de firmware del chip (~84 KB, ~9,6 s a 1 MHz) en el init; **runtime obligatorio a 100 kHz** porque a más velocidad el read multibyte del BNO055 secundario se corrompe (`sensors_tof.cpp:166-173,380`).
- **Round-robin.** Leer los 4 ToF juntos hundía el loop a ~6 Hz; por eso se lee **1 ToF por tick**.

---

## 3. Cómo la TOP lee los ToF (pipeline real)

Trazado de `sensors_tof_tick()` (`sensors_tof.cpp:432-499`), que es lo que corre de verdad cada tick:

1. **Round-robin: un solo ToF por tick.** Con `-DTOP_ENABLE_TOF_SCHED` (que `top_robot2_pri` **sí** trae, `platformio.ini:390`) el índice sale de `tof_sched_next()` (`tof_schedule.h:104-117`), que salta los sensores caídos y devuelve `0xFF` si los 4 están caídos. Sin el flag, fallback byte-equivalente `s_rr=(s_rr+1)%4`.
2. **Leer solo si hay dato.** `isDataReady()` + `getRangingData()` son *polling no bloqueante*. `getRangingData()` trae un bloque grande por I2C (decenas de ms) — por eso uno por tick y no los cuatro.
3. **`fill_zones()`** (`sensors_tof.cpp:227-235`) vuelca las 16 zonas **crudas** a `g_zones_mm[i][0..15]` (orden nativo del chip), poniendo `0xFFFF` en cada zona cuyo `status` no sea `5/6/9` o cuyo mm caiga fuera de `[0,4000]`. **`g_zones_mm` queda siempre en marco crudo, sin orientar ni enmascarar.**
4. **Aplicar la máscara → distancia.** Lee `zmask = g_top_cfg.tof[i].zone_mask` (`sensors_tof.cpp:477`) y reduce las 16 zonas a **una** distancia `g_distances_mm[i]` con `tof_zone_masked_mean()` (default) o `_robust` (con flag). Ver §5 y §6.
5. **Frescura (stale).** Si hubo frame OK, sella `g_last_ok_ms[i]=now`. Si `getRangingData()` falla, **no toca la frescura**: el valor cacheado sobrevive hasta `TOF_STALE_TIMEOUT_MS = 250 ms` (~3-4 frames a 15 Hz), después el getter lo expira a `NO_READING`.
6. **El consumidor lee** vía `sensors_tof_get_distance_mm(idx)` (`sensors_tof.cpp:583`), que chequea `enabled` + frescura. Si el sensor está deshabilitado o venció, devuelve `NO_READING` sin inventar nada.

> **Dato importante de frescura:** como cada ToF se refresca cada ~4 ticks (round-robin), un sensor "stale" reporta `NO_READING` en el escalar. Y el getter de zonas crudas (`sensors_tof_get_zone_mm`) está **gateado por la misma frescura del escalar** (`sensors_tof.cpp:598`): si el escalar venció, **todas** las zonas crudas de ese sensor se reportan `NO_READING` aunque el frame en memoria sea reciente.

---

## 4. Ubicación y rotación de los sensores

Acá viven **dos cosas distintas que NO son lo mismo** y conviene no confundir:

### 4.1. Posición / "bearing" (para localización)

- El **índice** del ToF es fijo por hardware (cableado XSHUT + dirección I2C). Convención: **`[0]=FRENTE`, `[1]=ATRÁS`, `[2]=DERECHA`, `[3]=IZQUIERDA`**.
- El **ángulo de montaje** (`mount_bearing_deg`) se usa **solo** para trilateración: `classify_wall()` (`localization.cpp:96-106`) hace `world_angle = heading + mount_angle` y decide a qué pared apunta cada rayo. **No rota la grilla de zonas.**
- Se aplica **solo en el boot** (`localization_runtime.cpp:100-102`). Cambiar `POS` por comando **no tiene efecto hasta `CFG SAVE` + apagar/prender**. Esto es fácil de confundir con "no anda el bloqueo".

> ⚠️ **CONTRADICCIÓN REAL DER/IZQ (bug latente, y NO hipotético — ya pasó una vez).** El default de hardware es `TOF_MOUNT_ANGLE_DEG = {0,180,270,90}` (`pinout_common.h:110`) y `kDefaultBearing = {0,180,270,90}` (`top_config.cpp:42`) → idx2 (DERECHA) = **270°**, idx3 (IZQUIERDA) = **90°**. PERO **TRES fuentes** del monitor/comentarios usan la convención OPUESTA:
> 1. El comentario `top_config.h:45` dice `90=der,270=izq`.
> 2. El parser del comando `POS` mapea **`RIGHT→90`, `LEFT→270`** (`telemetry_top.cpp:451-453`).
> 3. **`robot_geometry.py:39-40`** (el dibujo top-down del robot en el monitor) usa **bearing 90=derecha / 270=izquierda** → el monitor muestra los rayos der/izq **invertidos** respecto a cómo `classify_wall` clasifica las paredes de verdad.
>
> O sea: si bajás "`TOF 2 POS RIGHT`" al ToF derecho, grabás bearing **90**, que es el ángulo de la **izquierda** según el hardware → la trilateración mandaría ese rayo a la pared equivocada. **Esto YA OCURRIÓ una vez:** `pinout_common.h:102-105` documenta que el valor viejo `{0,180,90,270}` cruzaba der/izq y se **corrigió en banco el 2026-05-30**. Es un rastrillo ya pisado, no un riesgo teórico. **Mientras nadie toque `POS` desde la app**, el firmware usa su default correcto `{0,180,270,90}` y no pasa nada. El peligro aparece al bajar `POS` o al confiar en el dibujo del monitor. (Esto es ortogonal al bloqueo de zonas, pero hay que saberlo antes de tocar la app.)

### 4.2. Rotación / espejo de la grilla 4×4 (para zonas)

- Cada sensor tiene `zone_rotation_deg` (0/90/180/270, **horaria**) y `flip` (bit0 = espejo X/columnas, bit1 = espejo Y/filas).
- **El ToF izquierdo (idx 3) está físicamente montado mirando 180°** → necesita una corrección de 180° para que "arriba en la grilla" sea consistente con los otros. La app lo trae como `DEFAULT_ROTATION = {0:0, 1:0, 2:0, 3:180}` (`tof_layout.py:43`). El firmware/EEPROM, en cambio, tiene `zone_rotation_deg = 0` para **todos** por default (`top_config.cpp`). Esto **solo** importa en el modo "firmware-dueño" (§9); en el modo default la app pliega la rotación dentro de la máscara y el firmware no rota, así que no choca.

### 4.3. Numeración de zona, antes y después de rotar

- **Indexado:** `idx = row*4 + col` (*row-major*). `idx=0` = fila 0 / columna 0 = arriba-izquierda en pantalla; `idx=15` = abajo-derecha. Idéntico en firmware (`tof_zone_mask_orient.h`, grid_w=4) y app (`tof_layout._idx`).
- **Permutación al rotar** (`tof_zone_mask_orient.h:35-52`): para `(r,c)=(idx/4, idx%4)`:
  - 90°: `(r,c) → (c, 3-r)`
  - 180°: `(r,c) → (3-r, 3-c)` (equivale a `idx → 15-idx`)
  - 270°: `(r,c) → (3-c, r)`
  - luego se aplica el `flip`.
- **Esta matemática es idéntica en ambos lados** y tiene test de paridad. De hecho hubo un bug histórico (90↔270 invertido, daba `90→3/270→12`) que **ya está corregido** (`test/test_tof_zone_mask_orient/test_main.cpp:22-27` lo documenta) — prueba de que esta convención ya dio dolores de cabeza.

> **Los tres marcos de referencia** (memorizá esto, es la raíz de todo):
> - **Crudo:** como sale del chip, sin tocar. Es lo que vive en `g_zones_mm` y lo que viaja por telemetría.
> - **Canónico / display:** ya orientado (rotado/espejado) para que "arriba = arriba del robot". Es lo que el operador **ve y clickea** en el monitor.
> - La máscara **debe terminar en crudo** para que el veto caiga en las zonas físicas correctas. La conversión display→crudo (`raw_zone_mask`, "plegar") la hace **o** la app **o** el firmware, **nunca los dos ni ninguno**.

---

## 5. Bloqueo / anulación de zonas (la máscara)

- **Tipo:** `uint64_t zone_mask` (`top_config.h:48`). 64 bits disponibles, pero **solo se usan los 16 bajos** (4×4).
- **Convención de bit:** `bit i = 1` → zona `i` **se usa**; `bit i = 0` → zona `i` **se anula**. Default `~0` (todas activas = no-op, *fail-safe*).
- **Layout:** `bit (row*4+col)`, row 0 = fila superior, col 0 = izquierda.
- **Cómo se aplica** (`tof_zone_mask.h:51-66`): `tof_zone_masked_mean()` recorre las 16 zonas y hace `if (!((mask>>i)&1)) continue;` → la zona con bit 0 **no entra al promedio**. Promedia (entero, `suma/count`) las zonas con bit 1 **y** lectura válida. Si tras enmascarar **no queda ninguna** → devuelve `NO_READING` (nunca inventa una distancia).
- **Efecto inmediato:** `zone_mask` se lee **cada tick** desde `g_top_cfg`. Un comando `TOF n ZONEMASK xxxx` o `TOF n ZONE OFF z` cambia la distancia **en el tick siguiente**, sin reboot. La persistencia (a EEPROM) recién ocurre con `CFG SAVE`.

> ⚠️ **Truncado a 16 bits.** El comando `ZONEMASK` enmascara con `& 0xFFFF` en dos lugares (`telemetry_top.cpp:468`, `top_telemetry_serial.cpp:334`). Hoy inocuo (4×4=16). Pero `ZONE ON/OFF` acepta zona `0..63` (`telemetry_top.cpp:456-462`, `z<64` en `:459`) aunque solo existan `0..15`: podés "encender" bits 16..63 que nunca se leen → ruido confuso en un futuro *readback*. Si algún día se sube a 8×8, `ZONEMASK` no podría setear los 64 bits → deuda latente.

---

## 6. Promedios y descarte de outliers

Hay **tres reductores** zona→distancia conviviendo. Cuál corre depende del flag/env:

| Reductor | Dónde | Qué hace |
|---|---|---|
| **`tof_zone_masked_mean`** (default delantero) | `tof_zone_mask.h:51` | Promedio entero de zonas con bit=1 y válidas. Sin descarte de outliers. Con `mask=~0` es byte-idéntico al viejo `mean_valid_zones`. |
| **`tof_zone_masked_robust`** (opt-in `-DTOP_ENABLE_TOF_ROBUST`) | `tof_zone_mask.h:88` | Además del veto por máscara (también lo respeta, `:96`), descarta (1) zonas > `TOF_ROBUST_FIELD_MAX_MM=2430` mm (rayo fuera de cancha) y (2) con ≥3 zonas válidas, las que están < `TOF_ROBUST_LOW_KEEP_PCT=70%` de la **mediana** (rebote en otro robot). Con <3 válidas no rechaza outliers. |
| **`keeper_wall_dist_mm`** (solo arquero, `-DTOP_KEEPER_XY_WALLS`) | `keeper_xy_walls.h:51-83` | **Mediana + recorte simétrico ±35%**. NO usa `zone_mask` de EEPROM ni el robust. |

Aparte, el **delantero** tiene un **segundo** descarte de outliers, pero **entre ejes, no entre zonas** (`localization.cpp:166-208`): recibe las 4 distancias ya reducidas, descarta `d>2430 || d<10` (`localization.cpp:131`), y si dos estimaciones del mismo eje difieren > `outlier_threshold_mm`, tira la **más lejana de la pose previa** — pero **solo si hay pose previa** (`prev_valid`). En el primer ciclo no rechaza nada. El promedio final es media aritmética entera. `localization.cpp` **nunca ve las 16 zonas**; todo el enmascarado ya ocurrió en `sensors_tof.cpp`.

> ⚠️ **El arquero ignora la máscara por completo.** `compute_keeper_xy_pose` (`localization_runtime.cpp:48-84`) lee zonas crudas con su propio mediana+recorte y **no consulta `zone_mask`**. Si vetás zonas desde el monitor con el robot en modo arquero-XY-walls, **el veto no tiene ningún efecto** en la pose. Mismatch silencioso total. (Ojo con el nombre: el env de la TOP es `top_robot2_arquero_xywalls`, pero el comentario de `platformio.ini:423-424` aclara que el arquero "de verdad" es de la CENTRAL `central_robot2_arquero`. Verificá en banco qué binario tiene cada placa antes de asumir.)

---

## 7. Persistencia en EEPROM

- **Qué se graba:** un blob `TopConfig` v1 de **93 bytes** = magic `0x7C` + version `1` + 5 enables (cam/bno/us) + `tof[6]` (4 reales + 2 futuros) × 14 bytes + CRC16.
- **Layout por ToF (14 bytes, little-endian byte-a-byte):** `enabled(1) | mount_bearing_deg(2) | zone_rotation_deg(2) | flip(1) | zone_mask(8)`. La serialización es byte-a-byte (no `memcpy` del struct) para que el host x86 y la Teensy ARM produzcan el **mismo** blob (`top_config.cpp:61-115`).
- **Offset:** `TOP_CONFIG_EEPROM_OFFSET = 368`, rango `[368,460]` (`top_eeprom_config.h:5-17`). **No pisa** la calib del BNO en `[320,367]`.
- **CRC:** CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`), sobre el payload `[2..90]` (no cubre magic/version).
- **Carga al boot:** `main_top.cpp` llama `top_config_load()` **lo primero**, antes de inicializar sensores. Pone defaults (todo ON, `mask=~0`) y solo los sobreescribe si **magic + version exacta + CRC** validan. EEPROM en blanco (`0xFF`) → CRC falla → defaults → **competencia byte-idéntica**.
- **Quién aplica qué:**
  - `zone_mask` → **en vivo cada tick** (efecto inmediato).
  - `mount_bearing_deg` → **solo en boot** (`localization_runtime_init`).
  - `zone_rotation_deg` / `flip` → se **guardan** siempre, pero solo se **aplican** con `-DTOP_ENABLE_TOF_ROT`.
- **Quién escribe:** **solo** `CFG SAVE` (`top_config_save`, `EEPROM.update` byte a byte). Todos los demás comandos solo mutan RAM.

> ⚠️ **Trampa de "se evaporó el veto".** Si `CFG SAVE` falló, o el blob quedó corrupto, o cambió la version, `top_config_deserialize` cae a defaults (`mask=~0`) **sin aviso ruidoso** → al rebootear, todas las zonas vuelven a contar → "no se bloquean las zonas tras un power-cycle". Y el flasher de Teensy **no borra** la EEPROM emulada → una config vieja sobrevive a un re-flash (la rechaza el CRC solo si cambió el formato).

> ⚠️ **`g_top_cfg` es `static` → arranca en CEROS = todo DESHABILITADO** (`top_eeprom_config.cpp:14-16`). Depende 100% de que `main_top` llame `load()` (que primero pone defaults) antes de leer cualquier sensor. Hoy es correcto, pero cualquier diag que inicialice sensores sin llamar `load()` tendría todos los ToF apagados en silencio.

---

## 8. El programa de monitoreo (Python)

Flujo de la UI al cable (`tof_layout.py`, `panel_tof_setup.py`, `sources.py`):

1. El operador edita un `TofLayout`: por sensor, `position`, `rotation_deg`, `flip`, `sensor_enabled`, y `zone_enabled[idx]` (16 bool en **marco display**, lo que ve y clickea). Se guarda en un `.json` **por placa**, keyed por N° de serie del Teensy (`config_path_for_serial`, `tof_layout.py:325`) → R1 y R2 nunca cruzan config.
2. "Bajar a la placa" → `_push_to_fw()` (`panel_tof_setup.py:321`) llama `to_firmware_commands()` (`tof_layout.py:274`), que genera por sensor (verificado en código):
   - `TOF n POS <dir>` (siempre, `supported_now=True`)
   - `TOF n ON|OFF` (siempre, `supported_now=True`)
   - `TOF n ZONEMASK <4 hex>` (**siempre, `supported_now=True`** → **el veto SÍ baja**)
   - Según `FIRMWARE_OWNS_ROTATION`: si `True`, además `TOF n ROT`/`FLIP` reales + máscara **canónica**; si `False` (default), `ROT`/`FLIP` van como informativos `supported_now=False` (**no se mandan**) y la máscara va **plegada** (`raw_zone_mask`).
   - Al final: `CFG SAVE` (una vez).
3. `_push_to_fw` manda solo los `supported_now=True`. Cada uno se envía como línea ASCII + `\n` por el puerto (`sources.py:395`), ej. `TOF 0 ZONEMASK FFF0\n`.
4. En el firmware, `tt_parse_command` (`telemetry_top.cpp:403`) tokeniza (case-insensitive) y `dispatch()` (`top_telemetry_serial.cpp:244`) muta `g_top_cfg`. `ZONEMASK` → efecto inmediato; `POS` → solo en boot; `ROT/FLIP` → guardados (aplican con el flag); `CFG SAVE` → EEPROM.

> ⚠️ **El warning del `_push_to_fw` es engañoso/stale** (`panel_tof_setup.py:328-332`): dice que `ROT/FLIP/ZONE` están "pendientes de firmware / aplicados en el display, bajarán cuando el firmware los soporte". Es **falso hoy**: el firmware **ya** parsea y aplica `ZONE`, `ZONEMASK`, `ROT` y `FLIP` (`top_telemetry_serial.cpp:319-361`), y la **`ZONEMASK` sí baja**. Peor: el `ZONE` que menciona el warning es **doblemente espurio** — `to_firmware_commands()` **nunca emite** un comando `ZONE ON/OFF` (solo `ZONEMASK`, `tof_layout.py:307`), así que ni está pendiente ni se manda. El texto puede hacerte creer que vetaste zonas que "solo afectan el display" cuando en realidad la máscara se aplicó en el robot (o al revés).

---

## 9. ⚠️ Comparación TOP ↔ MONITOR (la sección clave)

| Aspecto | Firmware (TOP) | Monitor (Python) | ¿Coinciden? |
|---|---|---|---|
| **Numeración de zona** | `idx = row*4 + col`, row-major, 16 zonas | `idx = r*4 + c`, row-major, 16 zonas | ✅ **Sí** |
| **Convención de máscara** | `uint64`, bit=1 usar / 0 anular, default `~0` | `mask:04X`, bit=1 usar / 0 anular | ✅ **Sí** |
| **Formato del comando** | `strtol(hex) & 0xFFFF` | `TOF n ZONEMASK FFFF` (4 hex) | ✅ **Sí** |
| **Matemática de rotación** | `tof_zone_mask_orient` (90/180/270 horaria + flip) | `zone_source_map` (misma) | ✅ **Sí** (test de paridad) |
| **Validez de zona** | filtra `status 5/6/9` en `fill_zones` | recibe ya filtrado (`65535→None`); no re-filtra | ✅ **Sí** (por construcción) |
| **Posición/bearing por defecto** | `{0,180,270,90}` por índice (der=270, izq=90) | `POS` mapea RIGHT=90/LEFT=270; `robot_geometry.py` dibuja der=90/izq=270 | ⚠️ **Contradicción der/izq (3 fuentes)** (§4.1) |
| **Qué viaja por telemetría** | zonas **crudas, sin enmascarar** (`"z"`) | pinta el veto desde su `.json` **local** | ❌ **Divergen** |
| **Quién aplica la rotación** | solo con `-DTOP_ENABLE_TOF_ROT` (default OFF) | `FIRMWARE_OWNS_ROTATION` (default False) | ⚠️ **Acople frágil sin enforcement** |
| **Las DOS vistas 360 del monitor** | — | `gui_tof_360` pinta el veto; `panel_tof_360` **no** | ❌ **Divergen entre sí** |
| **Veto en modo arquero-XY** | `keeper_wall_dist_mm` **ignora** `zone_mask` | igual pinta zonas vetadas | ❌ **Divergen (total)** |

**En prosa, los puntos donde divergen (= por qué "el monitor mostraba algo y la TOP usaba otra cosa"):**

1. **La telemetría no devuelve el veto real.** El campo `"z"` son las 16 zonas crudas, sin máscara (`telemetry_top.cpp:131-141`). La máscara solo afecta el **escalar** `g_distances_mm`, que el monitor no recibe por-zona. El monitor pinta tachadas las zonas de **su `.json`** (`panel_tof_setup._render_card`, `panel_tof_setup.py:343-367`, lee `self.cfg.zone_is_enabled`), no las de la EEPROM. Si están desincronizados → ves un veto fantasma. **No hay readback** que confirme paridad.

2. **El acople "quién rota" no está enforced.** `FIRMWARE_OWNS_ROTATION` (Python) y `-DTOP_ENABLE_TOF_ROT` (build) deben ir juntos. Hoy ambos en su default (`False` / sin flag) → la app pliega y el firmware aplica crudo → **coinciden**. Pero `top_robot2_pri_zonas` y `_tofrot` traen `-DTOP_ENABLE_TOF_ROT`: con esos, la app **debe** estar en `True` o el veto 90°/270° se **rota doble** → cae en zonas opuestas. Para 0°/180° coincide por casualidad → bug intermitente.

3. **Dos vistas del monitor discrepan entre sí.** `panel_tof_360._render_strip` (`panel_tof_360.py:128-147`) llama `oriented_grid_for_sensor(...)` **sin `cfg`** y **nunca** consulta `zone_is_enabled` → **nunca pinta el veto**. `gui_tof_360._render_strip` (`gui_tof_360.py:276-290`) **sí** pasa `cfg` y apaga las zonas vetadas. Misma herramienta, dos comportamientos: en una vista el veto se ve y en la otra no.

4. **El arquero ignora la máscara** (§6): si el robot corre `-DTOP_KEEPER_XY_WALLS`, vetar zonas en el monitor no hace nada.

5. **El bearing der/izq tiene 3 fuentes con la convención invertida** (§4.1): comentario + parser `POS` + `robot_geometry.py`. El dibujo top-down del monitor muestra los rayos der/izq al revés de cómo la TOP clasifica las paredes. Ya se cruzó una vez (corregido 2026-05-30).

---

## 10. Robot1 (delantero) vs Robot2 (arquero)

- **El código de lectura de ToF es idéntico** para R1/R2 (mismo `sensors_tof.cpp`; el rol entra por `#define ROBOT1/ROBOT2` vía `pinout_robotN.h`, no por lógica de lectura). Ambas TOP corren la familia `top_robot2_pri`.
- **No hay diferencia de EEPROM** entre R1/R2: mismo offset (368), mismo formato, mismos defaults. La separación es por **datos**: cada Teensy tiene su propia EEPROM, y el monitor guarda un `.json` por N° de serie → imposible cruzar config.
- **La diferencia real es de flags/rol:**
  - **Delantero**: `localization_compute` (media + outlier-reject entre ejes).
  - **Arquero**: `-DTOP_KEEPER_XY_WALLS` → `compute_keeper_xy_pose` (mediana+recorte sobre zonas izq/der/atrás, **sin máscara**).
- **Aviso del código** (`sensors_tof.cpp:450`): el round-robin 1-ToF/tick se validó en banco de **ROBOT2**; ROBOT1 hereda el mismo código pero está **a verificar** en su banco. Y `robot2.h:136-138` advierte que `TOF_MOUNT_ANGLE_DEG` aún vive en `pinout_common.h` (común) y para R2 la rotación ~90° está **"a confirmar"** → deuda abierta que puede hacer que R2 interprete direcciones distinto.

---

## 11. Hallazgos y qué hace falta para habilitar (formato coach)

### Lo que está bien (no tocar)
- La convención de zona, máscara, rotación y el formato del comando **coinciden** y tienen tests de paridad. **El bloqueo de zonas funciona** en el modo default (`top_robot2_pri` + app `FIRMWARE_OWNS_ROTATION=False`). El default `~0` es fail-safe.

### Temas a analizar (priorizados)

**P0 — Acordar UN modo de rotación y verificarlo en banco antes de habilitar.**
- **Qué pasa:** `FIRMWARE_OWNS_ROTATION` (Python) y `-DTOP_ENABLE_TOF_ROT` (build) deben coincidir y nada lo chequea. Es el mecanismo exacto del síntoma histórico.
- **`risk-no-fix`:** volvés a pisar el bug: flasheás `_zonas`/`_tofrot` sin tocar la app → veto 90°/270° en zonas opuestas. Intermitente (0/180 "parece andar").
- **`risk-fix`:** ninguno si elegís el modo default. El riesgo es operativo (acordarse).
- **Recomendación:** usar el modo **default** (`top_robot2_pri` SIN `-DTOP_ENABLE_TOF_ROT` + app en `False`). NO usar `top_robot2_pri_zonas`/`_tofrot` salvo que pongas la app en `True` **a la vez** y hagas `CFG RESET` antes.
- **Plan de banco:** con `top_robot2_pri`, la mano a una distancia conocida frente al ToF izquierdo (idx 3, el de 180°): vetá la fila superior en la app, `CFG SAVE`, reboot, y confirmá que el escalar `tof_mm[3]` cambia como esperás (la zona física correcta queda anulada). Repetir para 90°/270° en otro sensor. **Solo el equipo con la placa puede cerrar esto** (no lo puedo marcar `done`).

**P1 — Falta readback de la máscara real (firmware→monitor).**
- **Qué pasa:** el monitor pinta su `.json`, no la EEPROM. No hay forma de confirmar en pantalla qué veto usa la TOP de verdad.
- **`risk-no-fix`:** seguís ciego ante la desincronía `.json`↔EEPROM. Es la causa #1 del síntoma.
- **`risk-fix`:** bajo. Agregar un comando tipo `TOF n ZONEMASK?` que devuelva `g_top_cfg.tof[n].zone_mask` por telemetría, y que el monitor lo lea tras `CFG SAVE` y avise si difiere de lo mostrado.
- **`tiempo`:** ~0,5-1 día (firmware: emitir el valor; app: leerlo y comparar).
- **Plan de banco:** bajar una máscara, `CFG SAVE`, pedir readback, verificar que coincide; reboot, readback otra vez (detecta blob corrupto/no-guardado).

**P1 — Corregir el warning stale de `_push_to_fw` y la contradicción der/izq (3 fuentes).**
- **`_push_to_fw`** (`panel_tof_setup.py:328-332`): el texto "ROT/FLIP/ZONE pendientes de firmware" es falso (la `ZONEMASK` baja y el firmware la aplica), y el `ZONE` que nombra ni se emite.
- **Bearing der/izq** (§4.1): comentario `top_config.h:45` + parser `RIGHT→90/LEFT→270` (`telemetry_top.cpp:451-453`) + `robot_geometry.py:39-40` contradicen el hardware `{0,180,270,90}` (der=270, izq=90). **Tres fuentes** con la convención invertida; ya se cruzó una vez (corregido 2026-05-30, `pinout_common.h:102-105`).
- **`risk-no-fix`:** si alguien baja `POS RIGHT/LEFT`, la trilateración del delantero manda rayos a la pared equivocada; y el operador malinterpreta el dibujo del robot.
- **`risk-fix`:** medio — hay que decidir **una** convención y alinear comentario + parser + `robot_geometry.py` + el montaje físico real. Tocar `src/shared/` requiere `git fetch` antes (regla multi-agente).
- **`tiempo`:** ~0,5 día código + verificación de montaje físico en banco.

**P2 — Documentación/código desalineado (capitalizable a 2027).**
- Comentario de `top_config.h:40-48` dice que `rotation/flip/zone_mask` están "RESERVADOS / apply en A2.2", pero el código (`sensors_tof.cpp:478-495`) **ya** los aplica → induce a pensar que el bloqueo "no hace nada".
- Mito del **8×8**: `diag_top_tof_zonemap.cpp` corre 64 zonas; el firmware vivo es 16. La verificación de orientación del izquierdo se hizo en 8×8 → revalidar a 4×4.
- El arquero ignora la máscara (§6): documentar que vetar zonas no afecta al arquero-XY.
- `ZONEMASK` trunca a 16 bits: deuda si se sube a 8×8.
- El criterio de status `{5,6,9}` está **DUPLICADO** en `fill_zones` (`sensors_tof.cpp:230`) y `mean_valid_zones` (`sensors_tof.cpp:213`) sin constante compartida → cambiar el criterio en el futuro obliga a tocar dos lugares (fácil olvidar uno).

### Gaps honestos (lo que NO verifiqué en hardware)
- **No puedo confirmar qué env está flasheado HOY en cada placa física** ni si la EEPROM tiene una `zone_mask` custom o defaults. Eso decide si hay mismatch real ahora. **Requiere leer la placa** (log de boot "config cargada de EEPROM" vs "EEPROM vacía", o `CFG LOAD` + dump). Lo que **sí** confirmé en código: `default_envs = top_robot2_pri` y que ese env **NO** trae `-DTOP_ENABLE_TOF_ROT` (`platformio.ini:25,390`), y que la app está en `FIRMWARE_OWNS_ROTATION=False` (`tof_layout.py:52`) → **en el estado del repo, los dos lados coinciden**.
- No corrí los tests host de paridad (`test_tof_zone_mask_orient`, `test_tof_layout.py`); los leí. Convendría correrlos cubriendo el caso `TOP_ENABLE_TOF_ROT=ON` para 90°/270°.
- No verifiqué si algún `.json` de placa real quedó con una rotación ≠ default o si alguien parcheó `FIRMWARE_OWNS_ROTATION=True` localmente (es lo primero a chequear en la notebook de competencia).

---

**Archivos clave (rutas relativas al repo):**
- Firmware lectura/máscara: `software/teensy/Soccer 2026/src/top/sensors_tof.cpp` (`432-499`, `476-496`, `583-600`)
- Máscara + reductores: `src/shared/tof_zone_mask.h` (`51-124`), `src/shared/tof_zone_mask_orient.h` (`35-73`)
- Config/EEPROM: `src/shared/top_config.h` (`43-62`), `src/shared/top_config.cpp` (`42`, `61-115`), `src/top/top_eeprom_config.h` (`5-17`)
- Parser comandos: `src/shared/telemetry_top.cpp` (`442-490`), `src/top/top_telemetry_serial.cpp` (`244-374`)
- Telemetría `"z"` cruda: `src/shared/telemetry_top.cpp` (`125-141`)
- Localización/arquero: `src/shared/localization.cpp` (`96-106`, `126-208`), `src/shared/keeper_xy_walls.h` (`51-83`), `src/top/localization_runtime.cpp` (`48-102`)
- Monitor: `tools/monitor-base/monitor_base/tof_layout.py` (`36-52`, `240-309`), `panel_tof_setup.py` (`318-367`), `zones.py`, `panel_tof_360.py`, `gui_tof_360.py`, `robot_geometry.py` (`39-40`)
- Envs: `platformio.ini` (`25`, `372-435`: `top_robot2_pri` y variantes `_zonas`/`_tofrot`)

---

*Análisis con apoyo de Claude (workflow de 7 lectores en paralelo + pasada adversarial que verificó las 10 divergencias leyendo ambos lados del código). NO autoriza tocar firmware — espera evaluación de Gustavo. Nada testeado en hardware: el cierre lo hace el equipo. Atribución según `AI-INSTRUCTIONS.md`.*
