---
title: "Runbook de banco para Incheon — check-list con el robot en mano"
date: 2026-06-05
status: vivo
tipo: procedimiento-banco
audiencia: "equipo en el banco / Incheon (Virginia, Elías, Enzo, Gustavo)"
robot: ambos
author: "Claude Opus 4.8 (Anthropic), vía Claude Code"
requested-by: "Gustavo Viollaz (@gviollaz)"
relacionado: [TASK-022, TASK-101, TASK-301, IMU-1, R2]
---

# Runbook de banco — Incheon 2026

> **Qué es esto.** Un check-list accionable, con **comandos reales**, para hacer
> con el robot en mano en el banco / en el venue. NO es teoría: cada paso dice
> qué `env` flashear, qué comando correr, qué mirar y cuándo dar PASS.
>
> **Reglas de oro del banco (leer una vez):**
> 1. **ToF y cámaras: power-cycle SIEMPRE tras flashear.** Las direcciones I²C de
>    los VL53L7CX persisten entre resets; el bus arranca sucio si solo reseteás.
>    "Power-cycle" = cortar batería **y** USB, esperar ~10 s, reconectar.
> 2. **OTOS / `L=-- R=--` = problema de ALIMENTACIÓN, no de firmware.** Los OTOS
>    cuelgan del 3.3 V del MP1584 que viene de la **batería** (el USB NO los
>    alimenta). Batería cargada + switch ON + power-cycle completo. Si ves `0x64`
>    en el scan I²C, eso es brownout, no otro chip.
> 3. **Sujetá el robot / ruedas al aire** en todo diag que mueva motores: puede
>    salir disparado.
> 4. **Working dir de todos los comandos:**
>    `C:/Users/violl/iitasoccer/soccer-main/software/teensy/Soccer 2026`
>    (ahí vive `platformio.ini`). Abrí la terminal ahí antes de correr `pio`.
> 5. **Un puerto USB = una placa.** `pio run -e ... -t upload` flashea la Teensy
>    conectada. Si hay varias, conectá de a una o usá `--upload-port`.
> 6. **Monitor a 115200** salvo donde se indique otro baud.

**Cómo se compila / flashea (patrón único):**
```
pio run -t clean -e <ENV>          # opcional pero recomendado al cambiar de env
pio run -e <ENV> -t upload         # compila + sube a la Teensy conectada
pio device monitor -b 115200       # abre el monitor serie USB
```

> **OJO build (Avast):** los firmwares (top/down/central) y los diags compilan
> **100 % offline** (libs vendoreadas en `lib/`). Solo `pio test -e test_native`
> baja Unity del registry → si Avast lo bloquea, usá `scripts/run-host-tests.sh`
> (g++ offline) para el gate host. Esto NO afecta a flashear el robot.

---

## ÍNDICE

- **Sección 1 — Recalibración de visión (TASK-022, BLOQUEANTE #1).**
- **Sección 2 — Activar las features de confiabilidad GATEADAS.**
- **Sección 3 — Validaciones de banco pendientes (movimiento + pose + GK).**
- **Apéndice — Mapa de UART / pines (referencia rápida de cableado).**

---

# Sección 1 — Recalibración de visión (TASK-022) — BLOQUEANTE #1

> **Por qué es lo primero.** Las 2 cámaras OpenMV N6 muestran color y transmiten,
> pero **los thresholds LAB dependen de la luz** y hay que rehacerlos bajo la
> iluminación del venue. **Sin esto el robot no ve la pelota.** Objetivo: un
> proceso repetible de ~15 min por cámara. Doc fuente:
> `docs/firmware/CALIBRACION-VISION-N6.md` + `docs/firmware/DIAG-CAM-ACCEPTANCE.md`.

**Herramienta IDE:** OpenMV IDE (versión que liste "OpenMV N6") → USB-C a la
cámara → botón ▶ Run. Los scripts viven en:
- Frontal: `hardware/electronics/cameraFront-pack/firmware/openmv/{cam-frontal-n6.py, calib-lab-n6.py}`
- Trasera: `hardware/electronics/cameraBack-pack/firmware/openmv/{cam-trasera-n6.py, calib-lab-n6.py}`

> ⚠️ **Cada cámara se calibra por separado y MONTADA EN EL ROBOT** (mismo
> ángulo/altura/luz que en partido). Calibrar en la mano = thresholds que no sirven.

### 1.0 — Pre-requisito: contrato cámara→TOP v2 (WIRE-BREAKING)

El packet cámara→TOP es **v2 (11 bytes: 9 datos + CRC8 + END=254, X simétrico
`X_coded=X+100`, sentinel=255).** Es **wire-breaking** contra v1: una punta v1
contra otra v2 NO decodifica.

- [ ] **Deploy COORDINADO:** re-flashear **las 2 cámaras** (`cam-frontal-n6.py` +
      `cam-trasera-n6.py`, guardadas como `main.py`) **Y el TOP** (`top_robot1`)
      en el MISMO pasaje. Piecemeal = visión muerta.
- [ ] La homografía que calibres (Paso 1.4) debe alinearse a la convención
      simétrica `[-100,100]` (`+x = derecha`, `-x = izquierda`).

### 1.1 — Confirmar el enlace UART (¿le llega a la TOP?)

Objetivo: la TOP cuenta packets de cada cámara antes de calibrar color.

1. [ ] Flashear el TOP con el verificador de enlace y abrir monitor:
   ```
   pio run -e diag_top_cameras -t upload
   pio device monitor -b 115200
   ```
   (Frontal=Serial3/RX15/U8; Trasera=Serial5/RX21/U9; baud 19200.)
2. [ ] En la cámara: abrir su script de producción con `BRING_UP=True`, ▶ Run.
       En la consola del IDE tienen que salir packets `[201, X, Y, 202, ...]`.
3. [ ] En el monitor del TOP: el veredicto debe decir **FORMATO OK** y el
       contador de packets de esa cámara debe **subir**.
4. [ ] Si NO recibe: probar `UART_PORT = 1`, luego `2`, luego `3` en el `.py`
       (re-Run cada vez) hasta que suba el contador. **Anotar el valor que
       funcionó** en el script. (La trasera viene anotada UART3→Serial5.)
5. [ ] Si la imagen sale espejada/al revés: ajustar `HMIRROR` / `VFLIP` hasta que
       en el preview la pelota arriba esté arriba (puede diferir entre cámaras).

**PASS 1.1:** la TOP cuenta packets de AMBAS cámaras, FORMATO OK, `crc`/`rsy` en 0.

### 1.2 — Calibrar los 3 thresholds LAB (el corazón de TASK-022)

Herramienta: **`calib-lab-n6.py`** (corre de RAM, NO transmite, no toca el script
de competencia). NO guardarlo como `main.py`.

1. [ ] Abrir `calib-lab-n6.py` → ▶ Run. Aparece un **cuadro blanco central** (sonda).
2. [ ] Por cada color (`TARGET = "naranja"`, luego `"amarillo"`, luego `"azul"`; re-Run):
   - [ ] a. Poner el objeto (pelota / arco) **llenando el cuadro central**.
   - [ ] b. Leer en consola el **`TUPLE sugerido`** (LAB real + margen).
   - [ ] c. Pegar ese tuple en `THRESHOLDS[TARGET]` del mismo `calib-lab-n6.py`, re-Run.
   - [ ] d. Mirar el framebuffer: el **recuadro verde** rodea **SÓLO el objeto**,
            nada del fondo. Agarra fondo → achicar rangos; no agarra → ampliar.
   - [ ] e. Consola: "agarra 1 blob, mayor = N px" con N por encima de
            `PIXELS_MIN` (pelota ≥20, amarillo ≥600, azul ≥300).
3. [ ] Copiar los 3 tuples finales a `NARANJA_THRESHOLD` / `AMARILLO_THRESHOLD` /
       `AZUL_THRESHOLD` del **script de producción** de esa cámara.

> Alternativa: Threshold Editor del IDE (Tools → Machine Vision → Threshold
> Editor). El kit es más rápido (te da el tuple ya calculado).

### 1.3 — Fijar exposición para competencia

Con autos ON la luz cambia y rompe los LAB. Para partido va todo fijo.

1. [ ] En el script de **producción**, pasar `BRING_UP = False`.
2. [ ] Confirmar que el init tiene los 3 autos en OFF:
       `set_auto_whitebal(False)`, `set_auto_gain(False)`,
       `set_auto_exposure(False, exposure_us=VALOR)`.
3. [ ] Ajustar `EXPOSURE_US` (empezar ~37000, subir/bajar) hasta que la imagen
       quede tan buena como con autos. Re-Run y verificar que los 3 colores
       SIGUEN detectándose.
4. [ ] Si al fijar exposición cambió el color → re-tocar thresholds (Paso 1.2)
       **con `BRING_UP=False`**, así quedan para la condición real de partido.
5. [ ] **Anotar `EXPOSURE_US` en el journal** (foto del setup).

### 1.4 — Homografía (Y ≈ distancia) — si hay tiempo

Convierte pixeles a cm para que `Y` ≈ distancia a la pelota.

1. [ ] Poner 4 puntos de posición conocida en el suelo (al frente para la
       frontal, atrás para la trasera) → leer pixeles → calcular `H_MATRIX`
       (4 correspondencias). **Cada cámara tiene su propia H.**
2. [ ] Medir la altura real de la cámara → `CAM_HEIGHT_CM` (placeholder hoy 18.7).
3. [ ] Validar la escala: ajustar `CAMERA_UNIT_TO_MM` en el TOP
       (`src/top/cameras_runtime.cpp`, hoy placeholder = `10.0f`) contra la cancha.

> El Paso 1.4 es "calidad de distancia". Para que el robot **vea y persiga** la
> pelota alcanza con 1.1–1.3; priorizar esos si el tiempo aprieta.

### 1.5 — ACEPTACIÓN cuantitativa (cierra el bloqueante)

Test de aceptación con el parser de PRODUCCIÓN (`cameras.cpp`). NO recalibra: verifica.

1. [ ] Flashear y abrir:
   ```
   pio run -e diag_cam_acceptance -t upload
   pio device monitor -b 115200
   ```
2. [ ] Lectura en vivo (cada ~300 ms, una línea por cámara):
   ```
   FRONTAL pkts=1234 crc=0 rsy=0 | BALL vis=Y raw=(3,58) mm=(30,580) ang=3.0deg dist=581mm | YEL=N BLU=Y
   ```
   - `pkts` sube; `crc`/`rsy` se quedan en 0 (si suben: ruido UART o baud/contrato mal).
   - `BALL vis=N` con la pelota presente → recalibrar LAB / revisar luz / acercar.
3. [ ] **Modo PASS/FAIL:** poner la pelota a una distancia MEDIDA con cinta,
       centrada al frente. Escribir los **cm** + Enter (ej. `60`), elegir cámara
       con `f`/`b`, apretar **`p`**:
   ```
   ---- ACEPTACION ----
     Esperado=600mm  Reportado=581mm  err=19mm  tol=60mm  -> PASS
   ```
   - Tolerancia = la mayor entre ±5 cm y ±10 %.
   - [ ] Repetir a **30 / 60 / 90 cm** y a izquierda/derecha (mirar el ángulo).
   - Error crece con la distancia → revisar **homografía**; escalado parejo →
     ajustar `CAMERA_UNIT_TO_MM`.

### 1.6 — Guardar en la cámara

1. [ ] Con `BRING_UP=False` detectando bien: `Tools → Save open script to OpenMV
       Cam (as main.py)` en AMBAS cámaras.
2. [ ] Power-cycle de la cámara y confirmar que arranca y transmite **sin la IDE
       conectada**.

### CRITERIO DE ACEPTACIÓN — Sección 1 (TASK-022)

- [ ] La TOP cuenta packets de **ambas** cámaras (1.1).
- [ ] Con `BRING_UP=False`, las 3 detecciones (pelota/amarillo/azul) salen estables
      al mover el objeto; el recuadro verde rodea solo el objeto correcto.
- [ ] `diag_cam_acceptance`: la pelota se detecta, **X simétrica** (izquierda da
      negativo, no "al frente"), 3/3 PASS a 30/60/90 cm con ángulo coherente.
- [ ] `main.py` guardado en ambas N6; arrancan y transmiten tras power-cycle.

> Claude NO cierra TASK-022 (es banco). Cuando se cumple esto, lo cierra el equipo
> y se actualiza `docs/ESTADO-ACTUAL.md`.

---

# Sección 2 — Activar las features de confiabilidad GATEADAS

> **Concepto.** Hay 4 mejoras de robustez que están **OFF por default** en los
> binarios de competencia (cambio de binario CERO sin el flag). Cada una tiene un
> **env de banco aditivo** que la prende. **Procedimiento por feature: flashear el
> env de banco → validar los 2 criterios → si PASS, mover el `-D` al env de
> competencia** (`top_robot1`/`central_robot1`/`down`). Orden recomendado: primero
> los watchdogs (HW, alto valor, bajo riesgo), después BNO-freeze, después
> `down_lean` (puro ahorro de CPU).

> **Cómo "promover" un flag (lo mismo para las 4):** editar `platformio.ini` y
> agregar el `-D...` al `build_flags` del env de competencia correspondiente. Es
> un cambio de UNA línea. Re-flashear el binario de competencia. NO promover sin
> los 2 PASS de banco.

### 2.1 — `top_robot1_bnofreeze` (detector de BNO congelado, IMU-1)

**Qué arregla:** el robot corre con **1 solo BNO sano** (0x28). Si su yaw se
congela a mitad de partido (el chip sigue ackeando pero el heading queda clavado),
hoy pasa como VÁLIDO. Este detector lo baja a DEAD. (Único HIGH de la auditoría
2026-06-04.) Flag: `TOP_ENABLE_BNO_FREEZE_DETECT`. Defaults del detector
(`src/shared/imu_freeze.h`): **N = 40 lecturas bit-idénticas** + **T = 1500 ms**
(a 20 Hz ≈ 2 s); ambos umbrales deben cumplirse.

- [ ] Flashear el env de banco:
  ```
  pio run -e top_robot1_bnofreeze -t upload
  pio device monitor -b 115200
  ```
  (Power-cycle tras flashear; boot ~40 s por la carga de los 4 ToF + I²C 100 kHz.)
- [ ] **Criterio A — NO falsos-DEAD:** robot **QUIETO un rato largo** (varios
      minutos). El BNO NO debe caer a DEAD (un BNO vivo jitterea ≥1 LSB de
      centideg; no debería quedar bit-idéntico 40 lecturas + 1.5 s).
- [ ] **Criterio B — SÍ detecta:** congelar/simular el BNO clavado (forzar el
      yaw a no cambiar) → confirmar que cae a **DEAD**.
- [ ] Si hay falsos-DEAD con el robot quieto: subir `N`/`T` (tunear en
      `src/shared/imu_freeze.h`) y re-validar.
- [ ] **PROMOVER:** si A y B PASS, agregar `-DTOP_ENABLE_BNO_FREEZE_DETECT` al
      `build_flags` de `[env:top_robot1]` (y `top_robot2`) en `platformio.ini`,
      re-flashear competencia.

### 2.2 — `central_robot1_wdt` (WDOG1 hardware en CENTRAL)

**Qué arregla:** la CENTRAL es el **master de motores**. Si SU loop se cuelga, los
motores quedan con el último comando y el robot sigue a ciegas (el timeout de 500
ms del snapshot vive DENTRO del loop colgado → no salva). WDOG1 a **1 s** resetea
la placa. Flag: `CENTRAL_ENABLE_WDT`.

- [ ] Flashear:
  ```
  pio run -e central_robot1_wdt -t upload
  pio device monitor -b 115200
  ```
  (Al boot imprime `[CENTRAL] WDT de hardware ARMADO (CENTRAL_ENABLE_WDT, 1 s)`.)
- [ ] **Criterio A — 0 resets espurios:** **30 min de marcha normal** con ambos
      UARTs drenando (TOP→CENTRAL + DOWN→CENTRAL) + strategy corriendo. No debe
      reiniciarse solo.
- [ ] **Criterio B — auto-reset al colgar:** forzar un cuelgue del loop a
      propósito → la placa debe reiniciarse a ~1 s. Confirmar que `WDOG1_WRSR`
      indica reset por WDT.
- [ ] **PROMOVER:** A y B PASS → agregar `-DCENTRAL_ENABLE_WDT` al `[env:central_robot1]`
      (y `central_robot2` cuando se valide ese robot), re-flashear.

### 2.3 — `down_wdt` (WDOG1 hardware en DOWN)

**Qué arregla:** si el I²C de DOWN se traba, se corta `LINE_URGENT` a CENTRAL → el
robot deja de frenar en el borde. WDOG1 a 1 s recupera la cadena. Flag:
`DOWN_ENABLE_WDT`. **OJO timing de boot:** el WDT se arma al FINAL de `setup()`,
después de `otos_init`/calibración (~0.5 s) y `line_ring_calibrate_carpet` (~0.32 s)
— esas operaciones lentas NO deben auto-resetear.

- [ ] Flashear:
  ```
  pio run -e down_wdt -t upload
  pio device monitor -b 115200
  ```
- [ ] **Criterio A — boot sin reset + 30 min:** la placa bootea OK (otos_init y
      calib son lentos) y **0 resets en 30 min** de marcha con 4 muxes + 2 OTOS.
- [ ] **Criterio B — auto-reset al colgar el I²C:** **desconectar un OTOS en
      caliente** → la placa debe auto-resetearse a ~1 s.
- [ ] **PROMOVER:** A y B PASS → agregar `-DDOWN_ENABLE_WDT` al `[env:down]`, re-flashear.

### 2.4 — `down_lean` (apaga el pipeline de línea muerto → libera CPU)

**Qué hace:** apaga el pipeline de filtros de línea que **solo leen los diags**
(no la competencia) → libera CPU en DOWN. Flag: `DOWN_LEAN_LINE_PIPELINE`. El wire
es **IDÉNTICO** (el `LineStatusV2` sale de `dm_update`, no de ese pipeline).
⚠️ **No usar con los diags de línea** (ellos sí leen ese pipeline).

- [ ] Flashear:
  ```
  pio run -e down_lean -t upload
  pio device monitor -b 115200
  ```
- [ ] **Criterio — wire idéntico:** comparar el `LineStatusV2` que sale (vía
      `diag_central_rx_all` o `diag_top_comm_down`) contra el de `[env:down]`
      normal: **mismo comportamiento de línea** (line_present, ángulo, imm_exit)
      con menos carga. La detección de borde NO debe cambiar.
- [ ] **PROMOVER:** PASS → agregar `-DDOWN_LEAN_LINE_PIPELINE` al `[env:down]`,
      re-flashear. (Es la promoción más segura: no toca wire.)

> **Nota:** `central_robot1_wdt`, `down_wdt` y `down_lean` ya existen como envs en
> `platformio.ini` (auditoría 2026-06-05 batch 2). `top_robot1_bnofreeze` también
> (batch del 2026-06-04). No hay que crearlos.

---

# Sección 3 — Validaciones de banco pendientes

> Cada ítem: **síntoma → qué medir → comando/env**. Estas son las cosas que el
> firmware no puede cerrar (necesitan robot + medición). Sujetá el robot en todo
> lo que mueva motores.

### 3.1 — WHEEL_ANGLES / cinemática "da círculos"

- **Síntoma:** al mandar un lateral puro, el robot **gira en vez de trasladar**;
  o al moverse solo gira un motor.
- **Causa probable:** `WHEEL_ANGLES_DEG[3] = {60, -60, 180}`
  (`src/central/config_central.h:87`) es **tentativo**; y a velocidades bajas un
  lateral puro deja motores en deadzone (banco 2026-06-03: `vx=150`/`MAX_SPEED=1000`
  → PWM ~13 % → M1 raspa, M2 stalled). Detalle: TASK-101.
- **Qué medir:** que un comando de traslación pura (vx, omega=0) mueva el robot
  DERECHO sin rotar; ajustar `WHEEL_ANGLES_DEG` y/o subir la velocidad de prueba.
- **Comando:**
  ```
  pio run -e diag_central_strafe_robot1 -t upload          # patrulla lateral open-loop
  # arrancá con más velocidad para sacar los motores de deadzone:
  # flags: -DDIAG_STRAFE_SPEED_MM_S=600  -DDIAG_STRAFE_DISTANCE_MM=400  -DDIAG_STRAFE_INVERT_LR
  pio device monitor -b 115200
  ```
  (Doc: `docs/firmware/DIAG-CENTRAL-STRAFE.md`.) **No promover ningún cambio de
  `WHEEL_ANGLES_DEG` sin que ROBOT1 quede byte-idéntico hasta validar** —editar
  config es decisión del equipo con la medición.

### 3.2 — Motores ROBOT2 (sin testear) + mapeo motor↔rueda

- **Síntoma:** ROBOT2 (delantero) nunca se validó; `MOTOR_INVERT` de ROBOT2 está
  copiado de ROBOT1 (`{+1,-1,+1}`, "sin validar en delantero", config_central.h:62).
- **Qué medir:** que los 3 H-bridge del Zircon de ROBOT2 energicen y giren en el
  sentido correcto; mapear motor firmware 1/2/3 → rueda física + sentido.
- **Comando:**
  ```
  pio run -e diag_central_motors -t upload     # mismo sketch para ambos robots
  pio device monitor -b 115200
  ```
  Operativa: botón pin 9 avanza motor por motor (0→128→0). 4º apretón = fin.
  Flag opcional `-DDIAG_MOTORS_REVERSE`. Doc: `docs/firmware/DIAG-CENTRAL-MOTORS.md`.
- **Recordatorio ROBOT1 (no re-pisar):** M1=U5, M2=U17 (**invertido por HW** →
  `MOTOR_INVERT={+1,-1,+1}`), M3=U7 (validado banco). No volver al viejo `{+1,+1,+1}`.

### 3.3 — Freno vs COAST en el Zircon (`motors_brake`)

- **Síntoma:** al frenar el robot puede COAST (seguir de largo) en vez de frenar
  firme — abierto desde la auditoría (central-brake-coast).
- **Qué medir:** ¿`motors_brake()` (`src/central/motors_zircon.cpp:85`) frena de
  verdad (corto de bobina) o hace COAST como `motors_stop()`? Mandar el robot a
  velocidad y comparar la distancia de frenado de `motors_brake()` vs `motors_stop()`.
- **Comando:** usar un diag que mueva y frene, p.ej.
  ```
  pio run -e diag_central_drive_robot1 -t upload
  # flags: -DDIAG_DRIVE_SPEED_MM_S=300  -DDIAG_DRIVE_DURATION_MS=3000
  pio device monitor -b 115200
  ```
  (Doc: `docs/firmware/DIAG-CENTRAL-DRIVE.md`. Botón pin 9: FORWARD→PAUSE→REVERSE.)
  Si `motors_brake` no frena, es decisión del equipo si se cambia el modo del driver.

### 3.4 — Pose absoluta (ToF eje X + `TOF_OFFSET_MM`)

- **Síntoma:** la pose por trilateración no está validada en HW (TASK-035); hoy el
  pose se computa pero el heading del snapshot viene del BNO, no de la pose.
- **Qué medir:** poner el robot a mano en posiciones CONOCIDAS de la cancha y
  comparar (x,y) computado vs real. Verificar que los ToF cubren el eje X (no solo
  Y) y que `TOF_OFFSET_MM = 95` (`src/top/pinout_common.h:115`, radio del robot)
  corresponde a la geometría real.
- **Comandos (en orden):**
  ```
  # 1) los 4 ToF enumeran y miden (power-cycle tras flashear):
  pio run -e diag_top_tof_quad_live -t upload   ;  pio device monitor -b 115200
  # 2) pose en vivo con el algoritmo REAL localization_compute():
  pio run -e diag_pose_live -t upload           ;  pio device monitor -b 115200
  #    ('z' = fijar cero; power-cycle tras flashear)
  ```
  (También `diag_localization_live` para validar el módulo aislado en posiciones
  conocidas.) **No re-invertir ejes:** `+Y=arco-a-arco`, `+X=lateral`
  (`docs/CONVENCION-EJES-ROBOT.md`).

### 3.5 — Tune del GK (strafe / drive-straight / anticipación)

Las conductas nuevas del arquero están **code-complete pero duermen** hasta que
fluya el dato (OTOS/cross_track) y se tuneen los gains en banco. Tres tunes:

- **(a) Drive-straight (OTOS):** el ATK va/empuja derecho usando la odometría OTOS.
  - Medir: que vaya recto; tunear el PID de heading-hold.
  - Comando: `pio run -e diag_central_drive_robot1 -t upload` (con cadena TOP→CENTRAL
    viva; doc DIAG-CENTRAL-DRIVE). Para sumar la línea: flag `-DDIAG_DRIVE_WITH_LINE`.
- **(b) GK strafe paralelo a la línea (cross_track real):** el arquero se desplaza
  paralelo por `cross_track_mm`.
  - Medir: **confirmar eje/signo del strafe** (que vaya hacia donde corresponde) +
    tunear velocidad.
  - Comando: `pio run -e diag_central_strafe_robot1 -t upload`
    (flags `-DDIAG_STRAFE_SPEED_MM_S=...`, `-DDIAG_STRAFE_INVERT_LR` si el signo
    está al revés; doc DIAG-CENTRAL-STRAFE).
- **(c) Anticipación (ball_predict):** el GK INTERCEPT apunta a la X **predicha**
  de la pelota (`pos + v·lookahead`). Fallback automático = idéntico a hoy con
  pelota quieta.
  - Medir: con pelota en movimiento, que el arquero **se adelante** al cruce;
    tunear `lookahead_s` / `max_lead_mm`. Cambio de conducta → validar en banco.

- **Gatillado por árbitro (sanity de cadena completa):** confirmar que START/STOP
  del árbitro mueve/frena la conducta end-to-end (árbitro=GPIO pines 5/6 del TOP →
  flag MATCH_RUNNING → Serial4→CENTRAL):
  ```
  pio run -e diag_central_arbitro_strafe_robot1 -t upload
  pio device monitor -b 115200
  ```
  (Override de banco por serial: `s`=START manual, `x`=STOP. Doc:
  `docs/firmware/DIAG-CENTRAL-ARBITRO-STRAFE.md`.)

### 3.6 — Sanity de comunicación 3 placas (antes de cualquier partido)

- [ ] Con las 3 placas cableadas, flashear la CENTRAL con el receiver integral y
      confirmar que decodifica DOWN + TOP juntos:
  ```
  pio run -e diag_central_rx_all -t upload
  pio device monitor -b 115200
  ```
  Cableado: DOWN TX1(pin1)→CEN pin0 ; TOP TX4(pin17)→CEN pin28 ; GND común. Baud 230400.
- [ ] Verificar que llega el WorldSnapshot (TOP) y el LineStatusV2 + OTOS (DOWN),
      con CRC OK. (Receivers más finos: `diag_central_comm_down`, `diag_top_comm_down`.)

---

# Apéndice — Mapa de UART / pines (referencia de cableado)

> Numeración INTERNA (GPIO) del Teensy. El 4.0 (TOP/DOWN) **no expone Serial7
> (28/29) en el borde** (back-pads) → por eso TOP→CENTRAL va por Serial4.

**TOP (Teensy 4.0):**
- S1 ← DOWN · **S2 (7/8) ↔ COMM (árbitro)** · S3 ← cámara frontal (U8) ·
  **S4 (16/17) → CENTRAL** · S5 ← cámara trasera (U9, pin 21).
- Árbitro = **NIVEL GPIO en pines 5/6** (no UART): `match_running = pin5 OR pin6`,
  `INPUT_PULLDOWN`, 0=stop/1=go (fail-safe: cable suelto → ambos 0 → STOP).
- I²C: 1 BNO sano (0x28) + 4 ToF (0x2A..0x2D) en `Wire` (18/19) **@100 kHz**, BNO
  leído @20 Hz. ToF LP en pines {9,10,11,12} activo-ALTO. HC-SR04 en 4/3.
  ⚠️ El BNO derecho (0x29) es la unidad FALLADA; el robot corre con 1 BNO.

**CENTRAL (Teensy 4.1):**
- **S7 (pin 28) ← TOP** (cable del TOP pin17/TX4) · **S1 (pin 0) ← DOWN** ·
  pines **7/8 LIBRES para el motor 2** (conflicto 7/8 RESUELTO). Sin BNO local.
- Motores: M1=U5 (2/5/3), M2=U17 (8/7/6, **invertido HW**), M3=U7 (11/12/4).

**DOWN (Teensy 4.0):**
- S1 → CENTRAL (pin1/TX1 → CEN pin0) · S5 → TOP. Difunde línea+OTOS a ambas placas
  (`down_tx`). 4 muxes (32 sensores) + 2 OTOS (Wire+Wire1, 0x17).

**Baudios:** cámaras 19200 · inter-placa 230400 · monitor USB 115200 (salvo nota).

**Fuente única de cableado:** `hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md`.

---

## Cierre

Cuando se cumplan los criterios de aceptación de las Secciones 1 y 2, **actualizar
`docs/ESTADO-ACTUAL.md`** (qué TASK se cerró, qué flag se promovió) en el mismo
commit. El gate host (hoy **658 tests / 47 envs / 0 fallos**, medido 2026-06-05)
debe seguir verde tras cualquier cambio de config — correrlo con
`scripts/run-host-tests.sh`.
