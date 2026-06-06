# TEST-CARDS de banco — Placa TOP (Teensy 4.0)

> Cerebro sensorial del robot. Lee 1 BNO055 (0x28) + 4 ToF VL53L7CX + HC-SR04 + 2 camaras
> OpenMV + arbitro GPIO, fusiona pose y manda `WorldSnapshot` a CENTRAL.
>
> **Como usar estas cards (loop banco <-> IA):**
> 1. Elegi la card, flashea el env, abri el monitor.
> 2. Segui los pasos y mira "Que esperar si PASA".
> 3. Copia y pega la linea de serial / numero medido / comportamiento en el campo
>    **Feedback a devolver a la IA** y mandalo de vuelta para que la IA decida el siguiente paso.
>
> **Repo:** `software/teensy/Soccer 2026/` (todos los `cd` son relativos a ahi).
> **Baud del monitor:** 115200 salvo que la card diga otra cosa.
> **Comando monitor estandar:** `pio device monitor -b 115200`
>
> ⚠️ **ToF y power-cycle:** las direcciones I2C de los VL53L7CX PERSISTEN entre resets
> del Teensy; un re-flasheo NO las limpia. Donde la card lo pida, **corta y repone la
> energia de la placa** (no alcanza apretar reset) antes de abrir el monitor.

---

## Subsistema: VISION (camaras OpenMV)

### CARD TOP-1: Aceptacion de vision recalibrada (TASK-022)

- **Objetivo:** confirmar que, ya recalibradas LAB+homografia, la camara reporta la distancia de la pelota dentro de tolerancia. Es el bloqueante #1 para Incheon.
- **Placa:** TOP (Teensy 4.0).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e diag_cam_acceptance -t upload`
- **¿Existe el programa?:** SI. Env `[env:diag_cam_acceptance]` en `platformio.ini:79-84`; fuente `src/diag/diag_cam_acceptance.cpp`. Usa el parser de PRODUCCION (`src/top/cameras.cpp`, CameraParser v2, 11 bytes). Doc: `docs/firmware/DIAG-CAM-ACCEPTANCE.md`.
  - ⚠️ **Pre-requisito real:** las 2 camaras tienen que estar YA recalibradas (TASK-022). El contrato camara->TOP v2 es WIRE-BREAKING: hay que re-flashear las **2 camaras (.py) Y el TOP/diag JUNTOS** (piecemeal = vision muerta). La homografia debe alinearse a la convencion simetrica X en [-100,100].
- **Setup fisico:** robot quieto; las 2 OpenMV alimentadas y corriendo su `main.py` (FRONTAL->Serial3/pin15, TRASERA->Serial5/pin21, baud 19200). Pelota naranja a una distancia MEDIDA con cinta (p.ej. 60 cm), centrada al frente. Luz de cancha.
- **Pasos:**
  1. Flashea, abri el monitor. Veras un panel `FRONTAL ... | TRASERA ...` cada ~300 ms.
  2. Apoya la pelota a una distancia conocida y centrada; escribi esos cm + Enter (p.ej. `60`).
  3. Elegi camara con `f` (frontal) o `b` (trasera) y apreta `p` para el chequeo PASS/FAIL.
- **Que esperar si PASA:** linea de aceptacion del tipo
  `  Esperado=600mm  Reportado=58Xmm  err=NNmm  tol=60mm  -> PASS`
  y `  Angulo reportado=~0.0deg` con la pelota centrada. La linea periodica muestra `BALL vis=Y ... dist=~600mm`. Tolerancia = la mayor entre ±5 cm y ±10 % (`diag_cam_acceptance.cpp:50-51`).
- **Resultados posibles:**
  - A) `-> PASS` con angulo ~0 -> homografia OK, vision aceptada para esa distancia. Repetir a 30/60/120 cm.
  - B) `-> FAIL` con err grande -> homografia mal escalada; recalibrar / revisar UNIT_TO_MM.
  - C) `FAIL: la camara NO ve la pelota` -> LAB mal calibrado o poca luz; recalibrar color.
  - D) Panel con `pkts=0` o `crc>0` creciendo / `BALL vis=N` siempre -> enlace roto: camara no flasheada a v2, baud distinto, o cable cruzado (ver CARD TOP-6).
- **Feedback a devolver a la IA:** pega la **linea completa de aceptacion** (`Esperado=... Reportado=... err=... tol=... -> PASS/FAIL`) + la **linea del angulo** + una linea periodica `FRONTAL pkts=... crc=... | BALL vis=...`. Indica a que distancia REAL (cm) estaba la pelota y que camara (f/b).
- **Tiempo estimado:** 5-8 min (mas el tiempo de recalibrar, que es aparte).

### CARD TOP-6: Salud del enlace camara -> TOP

- **Objetivo:** verificar que el formato que mandan las camaras es el que el TOP espera (bytes/paquetes/resyncs), ANTES de pelear con la calibracion. Aisla "no ve" (color) de "no llega" (enlace).
- **Placa:** TOP (Teensy 4.0).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e diag_top_cameras -t upload`
- **¿Existe el programa?:** SI. Env `[env:diag_top_cameras]` en `platformio.ini:812-820`; fuente `src/diag/diag_top_cameras.cpp`. Compila el parser de produccion `src/top/cameras.cpp`.
- **Setup fisico:** robot quieto; 2 OpenMV alimentadas y corriendo `main.py`. FRONTAL=Serial3 (RX15, U8), TRASERA=Serial5 (RX21). Baud 19200.
- **Pasos:**
  1. Flashea, abri el monitor.
  2. Mira el veredicto por camara que imprime el diag.
  3. Mové la pelota frente a cada camara y observa si cambian los valores.
- **Que esperar si PASA:** por cada camara un veredicto `FORMATO OK` con bytes recibidos > 0 y paquetes decodificados subiendo, resyncs bajos/estables.
- **Resultados posibles:**
  - A) `FORMATO OK` ambas, paquetes suben -> enlace sano; el problema (si lo hay) es de calibracion -> ir a TOP-1.
  - B) `SIN BYTES` -> camara apagada, no corre `main.py`, baud != 19200, o TX/RX cruzado.
  - C) `FORMATO RARO` / resyncs disparados -> camara en contrato viejo (v1, 9 bytes); re-flashear la camara a v2.
- **Feedback a devolver a la IA:** pega el **veredicto literal de cada camara** + la linea con bytes/paquetes/resyncs (frontal y trasera). Deci si la camara estaba alimentada y corriendo su `main.py`.
- **Tiempo estimado:** 3-4 min.

---

## Subsistema: IMU / HEADING (BNO055)

### CARD TOP-2: Activar y validar BNO freeze-detect (IMU-1)

- **Objetivo:** validar el failover del BNO congelado: con el robot QUIETO no debe dar falso-DEAD, y al CONGELAR el heading debe caer a DEAD (heading deja de ser valido). Unico HIGH de la auditoria 2026-06-04.
- **Placa:** TOP (Teensy 4.0).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e top_robot1_bnofreeze -t upload`
- **¿Existe el programa?:** SI. Env `[env:top_robot1_bnofreeze]` en `platformio.ini:590-592` (= `top_robot1` + `-DTOP_ENABLE_BNO_FREEZE_DETECT`). Corre el firmware VIVO completo (`src/top/main_top.cpp`). El detector vive en `src/shared/imu_freeze.h` (PURO; N=`IMU_FREEZE_MIN_SAMPLES`=40, T=`IMU_FREEZE_MIN_MS`=1500 ms, `imu_freeze.h:50-51`) y se aplica en `src/top/sensors_imu.cpp:282-298` (baja `present`->false -> imu_fusion lo marca DEAD). El flag esta OFF en los envs de competencia: este es el UNICO que lo compila ON.
  - ⚠️ Este env manda snapshots reales a CENTRAL. Para banco aislado podes dejar CENTRAL desconectada: solo mira el USB.
- **Setup fisico:** robot quieto sobre la mesa, BNO (0x28) conectado en Wire (18/19). No hace falta nada mas. Para forzar el congelamiento: tene a mano una forma de simularlo (desconectar el BNO en caliente, o tapar el bus) o pedile a la IA un override de banco si lo agrega.
- **Pasos:**
  1. Flashea, power-cycle, abri el monitor. Dejalo QUIETO ~3-5 min.
  2. Anota: el `hdg=` jitterea (±0.x) y `imu_L=Y` se mantiene (NO debe pasar a N por estar quieto).
  3. Forza el congelamiento (desconecta el BNO del bus en caliente, o el metodo que la IA indique) y observa `imu_L`.
- **Que esperar si PASA:**
  - Quieto largo: la linea `[TOP] loop=NNN hdg=XX.X imu_L=Y imu_R=N ...` (impresa cada 500 ms, `main_top.cpp:257-264`) mantiene `imu_L=Y` todo el tiempo (sin falso-DEAD).
  - Al congelar: tras ~N=40 lecturas idénticas y >=1.5 s (`imu_freeze.h:130`), `imu_L` cae a `N` (DEAD) y el snapshot deja de marcar heading_valid.
- **Resultados posibles:**
  - A) Quieto: `imu_L=Y` estable + al congelar `imu_L=N` -> failover OK, listo para mover el flag a competencia.
  - B) Quieto: `imu_L` parpadea a `N` solo -> FALSO-DEAD; subir N/T en `imu_freeze.h` y reportar cada cuanto pasa.
  - C) Al congelar `imu_L` queda en `Y` -> el detector no dispara; bajar N/T o revisar que el valor quede realmente bit-identico.
- **Feedback a devolver a la IA:** pega **2 lineas `[TOP] loop=...`**: una del periodo QUIETO (mostrando `imu_L=Y`) y una de DESPUES de congelar (mostrando el valor de `imu_L`). Indica cuanto tiempo estuvo quieto sin falso-DEAD y como forzaste el congelamiento.
- **Tiempo estimado:** 5-8 min (incluye el rato quieto).

### CARD TOP-4: Heading/BNO sano + signo de giro

- **Objetivo:** confirmar que el BNO (0x28) entrega heading vivo, que el yaw CAMBIA al girar y con el SIGNO correcto (giro horario vs antihorario), y que el gyro calibra. Pendiente de banco recurrente (signo BNO).
- **Placa:** TOP (Teensy 4.0).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e diag_bno_left -t upload`
- **¿Existe el programa?:** SI. Env `[env:diag_bno_left]` en `platformio.ini:857-865`; fuente `src/diag/diag_bno_left.cpp`. Lee el BNO 0x28 en crudo (lib Adafruit, modo IMUPLUS) y duerme los 4 ToF (LP low) para limpiar el bus. SEGURO con 1 solo BNO. (Para el panel integrado con el resto de sensores, ver CARD TOP-7 con `diag_top_all`.)
- **Setup fisico:** robot sobre la mesa, BNO en Wire (18/19). Vas a tener que GIRARLO a mano, asi que dejalo libre para rotar ~90-180 grados.
- **Pasos:**
  1. Flashea, abri el monitor. Dejalo **QUIETO ~5 s** (para que calibre el gyro).
  2. Anota el `yaw=` de reposo. Gira el robot ~90 grados en SENTIDO HORARIO (visto desde arriba) y para.
  3. Anota el nuevo `yaw=`. Volve a la posicion inicial y gira ~90 grados ANTIHORARIO.
- **Que esperar si PASA:** lineas
  `yaw=XX.X roll=.. pitch=.. | gyro(z)=Y.YYdps (x,y)=.. | calib sys/g/a/m=.../.../.../... | T=NNC`
  (`diag_bno_left.cpp:82-89`). Al girar, `yaw` y `gyro(z)` cambian; `calib g` (gyro) llega a 3 tras unos segundos quieto; `T` es una temperatura razonable (~20-40 C).
- **Resultados posibles:**
  - A) yaw cambia al girar y `calib g`=3 -> BNO sano. Anota el SIGNO: horario sube o baja yaw (define la convencion para el HeadingPID).
  - B) yaw FIJO pero `gyro(z)` cambia -> la fusion no integra; problema de modo/lib, no del chip.
  - C) yaw FIJO y `gyro(z)~0` al girar -> chip/lectura muerta.
  - D) `calib g` siempre 0 -> no se dejo quieto al boot; reiniciar y NO moverlo 5 s.
  - E) `T` absurda (p.ej. -128 o 200) -> lectura I2C rota.
- **Feedback a devolver a la IA:** pega **3 lineas `yaw=...`**: una en reposo, una despues de girar HORARIO ~90, y una despues de girar ANTIHORARIO ~90. Asi la IA fija el signo. Aclara cuanto giraste (grados aprox) y en que sentido.
- **Tiempo estimado:** 4-5 min.

---

## Subsistema: ToF (VL53L7CX x4)

### CARD TOP-3: Censo de los 4 ToF + medir TOF_OFFSET_MM

- **Objetivo:** confirmar que los 4 ToF MIDEN, mapear cada direccion (frente/atras/der/izq), y medir el offset geometrico real (centro del robot a la cara del sensor) para validar `TOF_OFFSET_MM`.
- **Placa:** TOP (Teensy 4.0).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e diag_top_tof_quad_live -t upload`
- **¿Existe el programa?:** SI. Env `[env:diag_top_tof_quad_live]` en `platformio.ini:251-259`; fuente `src/diag/diag_top_tof_quad_live.cpp`. Enumera por LP {9,10,11,12} -> dir {0x2A,0x2B,0x2C,0x2D}, lee los 4 simultaneos. El valor a validar es `TOF_OFFSET_MM = 95` en `src/top/pinout_common.h:115` (lo usa la localizacion via `localization_runtime.cpp:43`). El mapeo de INDICE a posicion de produccion (0=FRENTE 1=ATRAS 2=DER 3=IZQ) esta en el diag integral `diag_top_all.cpp:38` -> cruzar para etiquetar.
  - ⚠️ Este diag enumera por su cuenta a 0x2A..0x2D, **no** mide TOF_OFFSET solo: el offset se mide con cinta (paso 4).
- **Setup fisico:** robot quieto, los 4 ToF montados, frente a una pared plana a una distancia conocida (p.ej. 30 cm de la CARA de un sensor). Tene una cinta metrica.
- **Pasos:**
  1. Flashea. **Corta y repone la energia de la placa** (las direcciones I2C persisten). Abri el monitor; el boot tarda ~10-15 s (carga firmware de los 4 ToF).
  2. Lee la linea de resumen `ToF MIDIENDO: N de 4.` Si N<4, anota cuales `(off)`.
  3. Tapa cada sensor con la mano de a uno: el que cae a ~50 mm es ese -> anota que `ToF#k@0xXX` corresponde a frente/atras/der/izq.
  4. **Medir offset:** poné el robot con un sensor mirando una pared a una distancia MEDIDA desde el CENTRO del robot (p.ej. centro a pared = 300 mm). El sensor reportara `~300 - TOF_OFFSET`. `TOF_OFFSET_MM = (distancia centro->pared) - (lectura del sensor)`. Compara contra 95 mm.
- **Que esperar si PASA:**
  - `ToF MIDIENDO: 4 de 4.`
  - lineas tipo `ToF#0@0x2A=312mm(v16)  ToF#1@0x2B=----(v0)  ...` (`diag_top_tof_quad_live.cpp:191-201`); `(vNN)` = zonas validas.
  - el offset calculado cae cerca de 95 mm (±~10 mm aceptable).
- **Resultados posibles:**
  - A) 4/4 miden y offset ~95 mm -> ToF OK y `TOF_OFFSET_MM` confirmado.
  - B) <4 miden (`! no aparecio en 0x29` o `begin() fallo`) -> ese LP/sensor no responde; revisar cableado LP o power-cycle de nuevo.
  - C) offset medido != 95 mm por >10 mm -> reportar el numero medido para que la IA actualice `TOF_OFFSET_MM` (no lo edites vos).
  - D) lecturas erráticas / `(v0)` permanente en un sensor -> ese ToF mira al vacio o esta tapado.
- **Feedback a devolver a la IA:** pega la linea **`ToF MIDIENDO: N de 4.`** + una linea de medicion de los 4 (`ToF#0@0x2A=...mm(vNN) ...`) + el **mapeo direccion->indice** que descubriste tapando + el **TOF_OFFSET calculado** (distancia centro->pared medida y lectura del sensor, ambos en mm).
- **Tiempo estimado:** 6-8 min (boot lento + medicion con cinta).

---

## Subsistema: SISTEMA / LOOP

### CARD TOP-5: Periodo del loop del TOP (frecuencia real)

- **Objetivo:** medir la frecuencia real del loop del firmware TOP en marcha (debe estar holgadamente por encima de 100 Hz, la cadencia del snapshot). Si cae, algun sensor esta bloqueando el loop.
- **Placa:** TOP (Teensy 4.0).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e top_robot1 -t upload`
- **¿Existe el programa?:** SI para el metodo via serial (NO hay un diag dedicado de "loop period"). El firmware VIVO `src/top/main_top.cpp` mantiene `g_loop_count` (`main_top.cpp:42,219`) y lo imprime cada 500 ms en la linea `[TOP] loop=NNNN ...` (`main_top.cpp:255-258`). El periodo se DERIVA: `frecuencia = (loop_count_2 - loop_count_1) / 0.5 s`.
  - **Variante osciloscopio (opcional, requiere codigo nuevo — NO crear sin pedirlo):** togglear un pin GPIO libre cada loop y medir el periodo con osciloscopio daria la medida directa. HOY ese toggle NO existe en `main_top.cpp`; si se quiere, pedirle a la IA que agregue un toggle GATED detras de un flag de diag (no toca el binario de competencia). Por ahora, usar el conteo serial de abajo.
- **Setup fisico:** robot encendido con todos los sensores conectados como en partido (BNO + 4 ToF + camaras + arbitro). Para el caso realista, las camaras alimentadas y corriendo. Solo se mira el USB; el robot puede estar quieto.
- **Pasos:**
  1. Flashea, power-cycle (por los ToF), abri el monitor; espera a que pasen los ~10-15 s de boot.
  2. Captura DOS lineas `[TOP] loop=NNNN ...` consecutivas (estan separadas 500 ms).
  3. Resta los dos `loop=` y dividi por 0.5 s para obtener Hz.
- **Que esperar si PASA:** dos lineas como
  `[TOP] loop=12345 hdg=... imu_L=Y imu_R=N min_obst=... cam_F/B=Y/Y ... resync=...`
  con un delta tal que la frecuencia sea claramente >100 Hz (idealmente varios cientos a >1 kHz). El comentario del firmware dice "El loop normal corre a >100 Hz" (`main_top.cpp:62`).
- **Resultados posibles:**
  - A) delta grande -> frecuencia >>100 Hz -> loop sano, snapshot a 100 Hz con margen.
  - B) delta chico -> frecuencia cercana o por debajo de 100 Hz -> algun tick bloquea (lectura I2C trabada del BNO o ToF); reportar para investigar (el WDOG1 de 1 s resetea si se cuelga del todo).
  - C) el contador `loop=` NO sube entre lineas o no aparece la linea -> el loop esta colgado / no arranco; revisar boot de los ToF.
- **Feedback a devolver a la IA:** pega **DOS lineas `[TOP] loop=...` consecutivas** (tal cual, con su `loop=NNNN`). La IA calcula la frecuencia del delta. Si el robot estaba quieto vs con sensores activos, aclaralo.
- **Tiempo estimado:** 3-4 min.

---

## Subsistema: INTEGRAL (panel todo-en-uno, opcional)

### CARD TOP-7: Panel integral de la TOP (sanity rapido)

- **Objetivo:** chequeo rapido de TODOS los sensores de la TOP en un solo panel (BNO + 4 ToF + HC-SR04 + camaras + arbitro) antes de cada sesion de banco. Util como "todo verde?" inicial.
- **Placa:** TOP (Teensy 4.0).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e diag_top_all -t upload`
- **¿Existe el programa?:** SI. Env `[env:diag_top_all]` en `platformio.ini:61-64`; fuente `src/diag/diag_top_all.cpp`. Reusa los modulos VIVOS (sensors_tof, sensors_imu, cameras_runtime, comm_arbiter). NO manda snapshot ni mueve nada.
- **Setup fisico:** robot quieto con todo conectado (BNO, 4 ToF, HC-SR04, 2 camaras alimentadas + corriendo `main.py`, arbitro en pines 5/6). Frente a una pared/objeto para que los ToF lean algo.
- **Pasos:**
  1. Flashea. **Corta y repone energia** (ToF). Abri el monitor; boot ~10-15 s.
  2. Lee el panel `============== TEST INTEGRAL TOP ==============` que sale cada 500 ms.
  3. Mové la mano frente a cada ToF/camara para ver que reaccionan.
- **Que esperar si PASA:**
  - `BNO    : hdg=XX.X deg | L=Y(..) R=N(..) desac=...deg` (heading vivo, L presente).
  - `ToF    : F=..mm A=..mm D=..mm I=..mm | min=..mm [ready YYYY]` (4 ready).
  - `CAM    : F=Y B=Y | ball vis=...` y `pkts F/B` subiendo.
  - `REFEREE: pin5=0 pin6=0 | AND=STOP OR=STOP` (en reposo) — cambia a GO al dar START.
- **Resultados posibles:**
  - A) BNO hdg vivo + ToF `[ready YYYY]` + CAM F/B=Y -> placa sana, seguir con las cards especificas.
  - B) `[ready` con alguna `N` -> ToF caido -> ir a CARD TOP-3.
  - C) `L=N` -> BNO no inicializo -> ir a CARD TOP-4.
  - D) `CAM F=N`/`B=N` o `pkts` no suben -> enlace de camara -> ir a CARD TOP-6.
- **Feedback a devolver a la IA:** pega **un bloque completo del panel** (las lineas BNO/ToF/HC-SR04/CAM/REFEREE entre los `====`). Indica si las camaras estaban alimentadas.
- **Tiempo estimado:** 3-4 min.
