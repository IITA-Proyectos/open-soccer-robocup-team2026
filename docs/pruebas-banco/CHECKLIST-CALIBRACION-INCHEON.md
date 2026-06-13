# CHECKLIST DE CALIBRACIÓN — Incheon (cancha nueva)

> **Para qué:** un procedimiento ORDENADO, para hacer con nervios, que mata el miedo
> *"no sabemos qué cambiamos ni qué quedó operativo"*. Se hace UNA vez al llegar a la
> cancha (luz, alto de pared y verde distintos del laboratorio de Salta), y se repite
> sólo lo que toque si algo se mueve.
>
> **Regla de oro de este doc:** después de CADA paso hay un bloque **"cómo confirmar
> que quedó OPERATIVO"** — esa frase es tu CERTEZA. No avances al siguiente paso hasta
> ver esa señal con tus propios ojos. Si no la ves, andá a **"si falla"**.
>
> **Hacelo EN ORDEN.** Cada paso depende del anterior: sin batería cargada nada calibra,
> sin línea no hay freno de borde, sin heading no hay ataque al arco, etc.
>
> **Cosas que NO se pierden:** la calibración de línea (DOWN) y el cero del heading (TOP)
> viven en **EEPROM** → **sobreviven al reflasheo**. Calibrás una vez, reflasheás
> competencia, y arranca con la calib puesta. (Ver `docs/firmware/USO-MONITOREO-Y-TELEMETRIA.md`.)
>
> **Herramientas:** todo desde `software/teensy/Soccer 2026/`. App de PC:
> `tools/monitor-base/` (`python -m monitor_base ...`). Monitor serie SIEMPRE a **115200**.
> Cámaras: OpenMV IDE + el kit de calibración LAB existente.

---

## TABLA RESUMEN (la foto de una sola mirada)

| # | Qué se calibra | Dónde se guarda | Cómo confirmar que quedó OPERATIVO |
|---|----------------|-----------------|-------------------------------------|
| 0 | **Batería cargada** (>7,6 V) | — (físico) | OTOS levantan, la línea separa verde/blanco, NO sale `CALIB_SUSPECT` |
| 1 | **Sensores de luz / línea** (DOWN) | **EEPROM** de la DOWN | Al re-bootear, USB dice `[DOWN] calib cargada de EEPROM (persistida)`; en la app `data_valid` pasa a **1** al cruzar línea |
| 2 | **Heading IMU** (TOP) | **EEPROM** de la TOP (perfil BNO) | Apuntando al arco rival, `imu.hdg ≈ 0`; `imu.valid=1`; al re-bootear el heading arranca coherente |
| 3 | **Cámaras (4: 2/robot)** umbrales LAB pelota+arcos | **flash/SD** de cada OpenMV | En la app `--top`: pelota `bvis=1` con `bconf` alto a ángulo/dist correctos; front y rear dan valores parecidos |
| 4 | **ToF / ubicación** (depende del ALTO de pared de Incheon) | config/firmware TOP | Tapando cada ToF de a uno cambia **el correcto** en `tof.d[]`; pose estable en 2-3 posiciones conocidas |
| 5 | **Parámetros de juego** (vel/acel/potencia) | config firmware (sólo si hace falta) | Anotado qué cambió + el robot no se quema ni patina ni vuelca |

> **Atajo mental:** pasos 0→1→2 son **OBLIGATORIOS y rápidos** (batería, línea, heading).
> El 3 (cámaras) es el más lento y el bloqueante real (visión sin recalibrar = #1).
> El 4 depende de la pared de Incheon. El 5 sólo si algo anda mal.

---

## PASO 0 — Batería primero (sin esto NADA calibra)

- **Qué se calibra:** nada todavía — es la **precondición**. Con USB solo, los OTOS y los
  LEDs de los sensores de luz **no se alimentan** (van de la batería), así que la odometría
  no arranca y el verde ≈ blanco → la línea da `CALIB?`/`data_valid=0`/`CALIB_SUSPECT`.
  Una batería floja (~7,60 V) hace que el robot ni se mueva y degrada la línea.
- **Cómo:**
  1. Medí la batería con tester (o por el indicador del robot). Debe leer **> 7,6 V**.
  2. Si está por debajo: **cargá o cambiá la batería ANTES de tocar cualquier otra cosa.**
     No debuguees con batería baja — vas a perseguir fantasmas.
  3. Conectá la batería y dejá que la placa **bootee** (DOWN ~2 s; TOP ~40 s por los ToF/BNO).
- **Cómo confirmar que quedó OPERATIVO (la certeza):**
  - Batería **> 7,6 V** con tester.
  - En la app de monitoreo: los OTOS NO están en `L:o-- R:o--`; los sensores no salen todos
    en rojo; la línea NO dice `CALIB_SUSPECT` permanente.
- **Si falla:**
  - Sigue `CALIB_SUSPECT`/`data_valid=0` con batería supuestamente OK → re-medí; una batería
    "a media carga" igual degrada la línea. Cambiala y reintentá.
  - El robot no se mueve → casi siempre batería baja (ver `MEMORY` → batería mínima).

---

## PASO 1 — Sensores de luz / línea (placa DOWN)

- **Qué se calibra:** el umbral verde/blanco de los 32 sensores del anillo contra el VERDE
  y el BLANCO reales de la cancha de Incheon (distintos del lab). Es lo que decide el freno
  de borde, así que es crítico.
- **Cómo:**
  1. Flashear competencia (el monitor USB viaja DORMIDO en `down`) y abrir la app de la BASE:
     ```powershell
     cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"
     pio run -e down -t upload
     cd tools\monitor-base
     python -m monitor_base --port COMx        # COMx = puerto de la DOWN (--port auto si no lo sabés)
     ```
     > La app despierta sola la telemetría al conectarse (manda STREAM ON + PING). Ya no hay
     > un env de banco aparte para la DOWN: se calibra sobre el mismo binario de competencia.
  2. Robot sobre el VERDE de la cancha → botón **"Auto-calib ON"**.
  3. Pasá el robot **lento** por las líneas blancas unos segundos (captura min/max por sensor).
  4. Botón **"Auto-calib OFF"** (fija carpet=min, white=max).
  5. Botón **"Guardar EEPROM"**.
  6. (Alternativa manual: **"Calibrar CARPET"** sobre verde + **"Calibrar BLANCO"** sobre una línea.)
- **Cómo confirmar que quedó OPERATIVO (la certeza):**
  1. **El test de la persistencia (el que mata el miedo):** botón **"Cargar EEPROM"** y luego
     **apagá/encendé la placa (power-cycle)**. Al re-bootear, por USB tiene que aparecer
     **`[DOWN] calib cargada de EEPROM (persistida)`** — y **NO** `EEPROM sin calib valida`.
     Esa línea ES tu certeza de que la calib quedó guardada y la usará el firmware de competencia.
  2. **El test en vivo:** cruzá una línea blanca bajo el anillo → en la app **`data_valid` pasa
     a 1** y el panel `LineStatusV2` marca present=SÍ con ángulo coherente; sobre verde vuelve a NO.
  3. Los 32 sensores reaccionan (cambian de color al ver blanco), **ninguno queda en rojo**.
  4. **La calib PERSISTE al reflashear:** volvé a competencia con `pio run -e down -t upload`
     y NO se pierde (vive en EEPROM, zona aparte del programa).
- **Si falla:**
  - Re-bootea y dice **`EEPROM sin calib valida`** → el "Guardar EEPROM" no quedó. Repetí
    Auto-calib ON/OFF + **Guardar EEPROM** y volvé a power-ciclar.
  - `data_valid=0` siempre / `CALIB_SUSPECT` → casi siempre **batería baja** (volvé al PASO 0)
    o el blanco se capturó con poca cobertura: recapturá el BLANCO apoyando bien el anillo.
  - Quedan sensores **sospechosos / margen bajo** → recapturá blanco con más cobertura; si
    insisten unos pocos, anotá los índices SN (pueden ser sensores débiles, no bloquea).
  - 8 sensores consecutivos en rojo → un mux caído (HW), reportar el rango.

---

## PASO 2 — Heading IMU (placa TOP)

- **Qué se calibra:** el **cero del heading** (a dónde es "0°" = de frente al arco rival) y,
  opcionalmente, el perfil de calib del BNO. Sin esto, el ataque al arco apunta mal.
- **Cómo:**
  1. Flashear telemetría TOP y abrir la app en vista TOP:
     ```powershell
     cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"
     pio run -e top_robot1_debug_telemetry -t upload
     cd tools\monitor-base
     python -m monitor_base --top --port COMx     # COMx = puerto de la TOP (otro COM distinto del DOWN)
     ```
  2. **Apuntá el frente del robot exactamente al arco RIVAL.**
  3. Comando/botón **`IMU ZERO`** (re-captura el heading inicial, `sensors_imu_recalibrate_zero()`).
  4. Comando/botón **`IMU SAVE`** para persistir el perfil del BNO en EEPROM.
- **Cómo confirmar que quedó OPERATIVO (la certeza):**
  1. Apuntando al arco rival, en el panel IMU **`hdg ≈ 0°`** (`imu.hdg`), y **`imu.valid=1`**
     (heading_valid). Girá el robot 90° a mano → `hdg` cambia ~90° en el sentido esperado.
  2. **`imu.disagree` chico** (idealmente < 5°): los BNO concuerdan. (Hoy el robot corre con
     **1 solo BNO sano**, `0x28`; si ves `lok=1 rok=0` es esperado — basta con que el bueno lea.)
  3. **Persistencia:** power-cycle de la TOP → el heading arranca coherente y `valid=1` tras el
     boot (la TOP tarda ~40 s por ToF/BNO; esperá el boot completo).
- **Si falla:**
  - `imu.valid=0` o `hdg` congelado → el BNO no está leyendo. Verificá I²C (Wire 18/19, 100 kHz)
    y que el BNO se lea a ~20 Hz (`BNO_READ_INTERVAL_MS`); a 400k o leyéndolo fuerte el yaw se
    congela contra los ToF (ver `MEMORY` → UART↔pin + I²C map).
  - `hdg` apunta al revés cuando girás → revisar signo del BNO (pendiente de banco conocido).
  - Tras power-cycle el cero se perdió → el `IMU SAVE` no quedó; repetí `IMU ZERO` + `IMU SAVE`.

---

## PASO 3 — Cámaras (4 en total: 2 por robot)

- **Qué se calibra:** los **umbrales LAB** (OpenMV usa **LAB, no HSV**) de la **pelota** y de
  los **arcos** (amarillo/azul), contra la luz de Incheon. Y, clave: **igualar las 2 cámaras
  de cada robot** (front y rear) para que el mismo objeto dé valores parecidos en las dos.
  *(Visión sin recalibrar es el bloqueante real #1 para Incheon — TASK-022.)*
- **Cómo:**
  1. Abrí cada cámara en **OpenMV IDE** y corré el kit de calibración LAB existente
     (script de calib LAB del repo de visión, el que usa `sensor` + `pyb.UART` en las N6).
  2. **Lockeá la exposición** bajo la luz de Incheon (no dejes auto-exposición saltando) y
     ajustá los thresholds LAB de la pelota (golf naranja / pelota IR pasiva 2026) y de cada arco.
  3. **Igualá las 2 cámaras del robot:** poné el MISMO objeto delante de la front y de la rear
     y ajustá hasta que los valores LAB detectados sean **parecidos** en ambas.
  4. **Persistí en la flash/SD de CADA cámara** (las 4: 2 por robot). Respetá la convención
     **X simétrica `[-100,100]`** del contrato cámara→TOP v2 (la homografía debe alinearse a eso).
- **Cómo confirmar que quedó OPERATIVO (la certeza):**
  1. En la app **`python -m monitor_base --top --port COMx`**, poné la pelota en el campo:
     el panel `cam` muestra **`bvis=1`** con **`bconf` alto** y **`bx`/`by`** coherentes con la
     posición real (marco robot: +x derecha, +y frente). Mové la pelota → los números siguen.
  2. **Front y rear consistentes:** el mismo objeto detectado por la cámara de adelante y la de
     atrás da posición/confianza parecidas (no una "ve" y la otra "no ve" lo mismo). `fok=1` y
     `bok=1` (ambas cámaras vivas, watchdog OK).
  3. **Arcos:** apuntando al arco rival, `gy_*`/`gb_*` (amarillo/azul) marcan `vis=1` con ángulo
     y distancia razonables; y en `snap` el arco mapea a `opp_*` (rival) según el color de equipo.
  4. **Enlace sano:** `cam.crc` y `cam.resync` no se disparan (pocos errores CRC8 cámara→TOP).
- **Si falla:**
  - `bvis=0` aunque la pelota esté ahí → thresholds LAB mal para esta luz; reajustá con la
    exposición LOCKEADA (auto-exposición es la causa #1 de "anda en el lab y no en la cancha").
  - Front detecta y rear no (o valores muy distintos) → no quedaron igualadas; repetí el paso 3
    con el mismo objeto delante de las dos.
  - `fok=0`/`bok=0` → esa cámara no está viva (cable/UART/firmware de la cámara); ojo: en las N6
    usar `sensor` + `pyb.UART` (no `csi`, no `machine.UART`, no `pyb.LED` → crashean en fw 4.8.1).
  - `cam.crc`/`cam.resync` suben rápido → desajuste de contrato; **re-flashear las 2 cámaras y la
    TOP JUNTAS** (el contrato v2 es wire-breaking; piecemeal = visión muerta).

---

## PASO 4 — ToF / ubicación (depende del ALTO DE PARED de Incheon)

- **Qué se calibra:** las distancias de referencia de los ToF contra la **altura de pared real
  de Incheon** (si la pared es más baja/alta que el lab, las lecturas y la pose cambian).
  **IMPORTANTE: cada ToF se verifica POR SEPARADO** — el ToF **izquierdo es de otro fabricante**
  y puede leer distinto, y un ToF puede estar apuntando unos grados arriba/abajo.
- **Cómo:**
  1. Con la TOP en telemetría (`--top`), poné el robot en una **posición conocida** (p.ej.
     pegado a una pared a distancia medida con cinta métrica).
  2. **Verificá cada ToF individualmente — el test del dedo:** **tapá UN ToF a la vez** con la
     mano y mirá en la app qué rayo cambia. Confirmá que cambia **el que corresponde** a esa
     dirección (no otro). Repetí con los 4.
  3. Compará la lectura `tof.d[i]` (mm) contra la distancia medida con cinta. Si un ToF lee
     consistentemente corto/largo o "ve" el piso/el borde de la pared, está mal apuntado
     (unos grados de inclinación) → ajustá el montaje o anotá el offset.
  4. Verificá la pose en **2-3 posiciones conocidas** distintas del campo.
- **Cómo confirmar que quedó OPERATIVO (la certeza):**
  1. **Mapeo correcto:** al tapar cada ToF, en `tof.d[]` cambia **el índice correcto** (el ToF
     izquierdo, derecho, etc. — cada uno el suyo). Ninguno cruzado.
  2. **Sin lecturas perdidas:** `tof.d[i]` ≠ `65535` (65535 = sin lectura / TOF_NO_READING) en
     condiciones normales; `tof.n=4` (los 4 presentes).
  3. **Pose estable:** en cada posición conocida, `snap.x`/`snap.y`/`snap.hdg_cd` se quedan
     **quietos y coherentes** con la posición real (no saltan), y `snap.conf` razonable.
- **Si falla:**
  - Tapo un ToF y cambia **otro** → están cruzados en el mapeo; reportar cuál con cuál.
  - Un ToF lee siempre corto/largo o `65535` intermitente → mal apuntado o de otro fabricante
    con rango distinto (el izquierdo); ajustar montaje/offset y re-verificar contra la cinta.
  - La pose salta en una posición fija → un ToF metiendo ruido; identificá cuál tapándolos de
    a uno y, si hace falta, bajá su peso en la fusión / corregí el offset.

> **Nota:** la pose absoluta fusionada hoy **no está cableada para mover el robot**; este paso
> es sobre todo para **confiar en los ToF** (freno/obstáculo) y verificar que la pose que la TOP
> calcula es sana. Si la pose fusionada no se usa para navegar, basta con (1) y (2).

---

## PASO 5 — Parámetros de juego (SÓLO si hace falta)

- **Qué se calibra:** velocidad / aceleración / potencia de motores, si en la cancha de Incheon
  el robot patina, vuelca, va lento, o **se acerca a quemar motores**.
- **Cómo:**
  1. **No toques nada si el robot anda bien.** Esto es el último recurso, no un paso obligatorio.
  2. Si hace falta, cambiá UN parámetro a la vez en la config del firmware y reflashea.
  3. ⚠️ **Cap de potencia OBLIGATORIO:** los motores 2026 son brushed 5V alimentados a 7,4V →
     **NO pasar ~70% de potencia o se queman** (ver `MEMORY` → motores 5V). No subas la potencia
     por encima del cap "para que ande más rápido".
- **Cómo confirmar que quedó OPERATIVO (la certeza):**
  - **Anotá EXACTAMENTE qué cambiaste** (parámetro, valor viejo → valor nuevo, por qué). Esa
    anotación ES la certeza contra el miedo "no sé qué cambiamos".
  - El robot se mueve sin patinar/derrapar al arrancar/frenar, no vuelca en los giros, y los
    motores **no se calientan** tras unos minutos.
- **Si falla:**
  - Patina al acelerar → bajá aceleración antes que velocidad punta (transferencia de peso).
  - Motores calientes → bajá el cap de potencia ya (riesgo de quemarlos); volvé al valor seguro.
  - Si dudás, **volvé al valor anterior** (por eso lo anotaste) y dejá el robot como venía.

---

## CIERRE — antes de jugar

1. Reflasheá los **envs de COMPETENCIA** en las dos placas (el binario es **byte-idéntico** al
   de banco con el flag OFF; **la calibración en EEPROM se mantiene**):
   ```powershell
   cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"
   pio run -e down -t upload            # placa ABAJO  -> competencia
   pio run -e top_robot1 -t upload      # placa ARRIBA -> competencia
   ```
2. **Power-cycle final** y mirá el boot por USB de cada placa: confirmá los banners de calib
   cargada (DOWN: `[DOWN] calib cargada de EEPROM (persistida)`; IMU `valid=1` en TOP).
3. Repaso de 30 segundos contra la **tabla resumen**: cada fila con su certeza ✔.

---

> ## NOTA FINAL (tu certeza, leéla siempre)
>
> **Ante la duda, cada placa al bootear te DICE qué calibración cargó — leelo, es tu certeza.**
> No adivines: la DOWN imprime `[DOWN] calib cargada de EEPROM (persistida)` (o, si NO,
> `EEPROM sin calib valida`), la TOP arranca con `imu.valid=1` y el heading coherente, y la
> app `--top` te muestra `bvis`/`tof.d[]`/`snap` en vivo. Si la línea de boot dice que cargó,
> **cargó**. Si dice que no, **calibrá de nuevo (PASO 1/2)** — no salgas a la cancha con la duda.
