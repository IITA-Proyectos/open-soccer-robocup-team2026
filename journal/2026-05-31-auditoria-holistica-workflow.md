---
title: "Auditoría holística — Soccer 2026 (workflow multi-agente)"
date: 2026-05-31
status: vigente
tipo: auditoria
generado-por: "workflow `auditoria-holistica-iita` (17 agentes, 4 lentes + verificación + síntesis) orquestado por Claude (coach)"
alcance: "read-only: coherencia de docs, oportunidades de mejora, líneas de comunicación inter-placa, estado del ultrasonido"
---

> Generado por un workflow multi-agente (4 lentes en paralelo → verificación de falsos
> positivos → síntesis). Las inconsistencias de documentación marcadas REALES fueron
> corregidas en el commit que acompaña a este journal. Las oportunidades de mejora de
> firmware son **recomendaciones para el equipo** (no se aplicaron unilateralmente).

# Auditoría holística — Soccer 2026 (2026-05-31)

> ⚠️ **NOTA POSTERIOR (2026-05-31, después de esta auditoría): el UART del CENTRAL se
> reasignó.** Donde la auditoría dice "TOP→CENTRAL en Serial1/pin0" y "DOWN→CENTRAL en
> Serial2/pin7" (tablas y checklist de §4) y trata el **conflicto 7/8 (F8/TASK-036) como
> abierto** (§2, §6), el código vivo + el mapa canónico ya usan: **TOP→CENTRAL = CENTRAL
> Serial7 (RX7 = pin 28)**, **DOWN→CENTRAL = CENTRAL Serial1 (RX1 = pin 0)**, con **Serial2
> (7/8) LIBRE para el motor 2 → F8/TASK-036 RESUELTO**. Para cablear seguí
> [`MAPA-CONEXIONES-3-PLACAS.md`](../hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md). El
> resto de la auditoría (visión, F1/F2/F3/F6/F4, ultrasonido) sigue vigente.

## 1. Resumen ejecutivo

- **El firmware compila y la arquitectura es sólida** (FSM dual + PIDs en CENTRAL, fusión sensorial en TOP, anillo de línea + OTOS en DOWN, 23 suites de tests host), pero **el robot todavía NO juega**: hay un cluster de gaps código-vs-hardware-real que lo dejan sin localizar, viendo mal la línea, o sin arrancar.
- **Los 3 bloqueantes P0 de firmware son de deploy/driver, no de lógica**: (F1) el driver de ToF lee 1 de 4 sensores → pose nunca válida; (F2) el build de competencia de DOWN sale con 1 mux = 8 de 32 sensores de línea; (F3) no hay arranque manual → si la placa COMM no manda START el robot queda clavado en WAIT_START. Los tres se arreglan con cambios chicos y de alto impacto.
- **La visión no es usable en cancha**: faltan las 3 calibraciones que dependen del sensor PAG7936 de la N6 (V1 thresholds LAB, V2 exposición fija + `BRING_UP=False`, V3 homografía por cámara). El pipeline parser→fusión→snapshot ya está cableado y compila; lo que falta es 100% calibración bajo la luz de Incheon.
- **Solo 1 de 3 enlaces UART está CONFIRMADO en banco**: DOWN→CENTRAL (línea, 2026-05-29, sin motores). TOP↔CENTRAL y DOWN→TOP tienen firmware listo en ambas puntas pero **no están cableados** (placas recién montadas). El swap del 2026-05-31 (cámara trasera soldada en Serial5 → link a CENTRAL movido a Serial7) propagó bien al código.
- **Único riesgo eléctrico real abierto**: conflicto pin 7/8 en CENTRAL (Serial2 RX2=pin7 vs driver motor U17, TASK-036). La línea anduvo en banco SIN motores; falta validarla con los 3 motores girando antes de declarar L3 confirmado-con-motores.
- **Documentación**: el swap propagó bien a los docs de mayor jerarquía, pero quedaron **8 inconsistencias reales** (sobre todo docs canónicos/packs con banner nuevo y cuerpo viejo "Serial2/Serial7"). Ninguna es bloqueante de juego; se descartaron 2 (1 falso-positivo, 1 nota meta) y NO se tocaron las 3 contradicciones intencionales.

---

## 2. Oportunidades de mejora priorizadas

Fusionados los duplicados entre lentes (F1≡causa de I4; F5≡V5; F8≡L3/F-comm; F11/V-kicker; F4-evasión; F6-histéresis).

| # | Sev | Área | Oportunidad | Evidencia (archivo:línea) | Acción |
|---|-----|------|-------------|---------------------------|--------|
| F1 | **P0** | Localización / ToF | Driver vivo inicializa **1 de 4 ToF** (en 0x29) → pose **nunca válida**, CENTRAL navega en (0,0) e ignora XY. Mata toda la estrategia posicional (área chica, kickoff posicional, `in_own_penalty_area`). | `src/top/sensors_tof.cpp:130-159`; `src/top/pinout_common.h:30-44`; `src/shared/localization.cpp:123`; `src/top/main_top.cpp:51-64` | Reescribir `sensors_tof_init()` portando la enumeración de `diag_top_tof_quad_live` (dormir todos los LP, despertar uno a uno, `setAddress()` a 0x2A..0x2D, ranging por sensor). Validar con `diag_pose_live`. |
| F2 | **P0** | Deploy / línea | `[env:down]` sin `-DDOWN_NUM_MUXES_CONNECTED` → default **1 mux = 8 de 32 sensores**; un solo cuadrante del anillo, arcos ciegos en 3/4 del perímetro. Fuerza `EV_DEGRADED_GEOMETRY`. | `platformio.ini:55-77`; `src/down/config_down.h:5-6,29-38`; `src/shared/down_model.cpp:54-78,123` | Agregar `-DDOWN_NUM_MUXES_CONNECTED=4 -DDOWN_NUM_OTOS_CONNECTED=2` a `[env:down]` (placa ya validada en banco 2026-05-24). **Una línea.** |
| F3 | **P0** | FSM / robustez | **No hay arranque manual**: `match_running` solo se setea por START de la placa COMM (Serial4). Si COMM falla/no manda START → robot estático todo el partido. | `src/top/comm_arbiter.cpp:21-34`; `src/central/strategy.cpp:134-155,324-337` | Botón fail-safe en CENTRAL (reusar `PIN_BUTTON=9` con debounce de `diag_central_motors.cpp:173-189`) que fuerce `match_running=true`, o override por dipswitch. |
| V1 | **P0** | Visión / calibración | Thresholds LAB son **placeholders del sensor H7**, no del PAG7936 de la N6. "SIN ESTO NO DETECTA" (comentario del propio script). | `cameraFront-pack/firmware/openmv/cam-frontal-n6.py:62-65`; `cameraBack-pack/.../cam-trasera-n6.py:60-63` | Recalibrar los 6 sliders LAB por color y **por cámara** bajo la luz de Incheon (OpenMV IDE Threshold Editor). Prioridad #1 de visión. |
| V2 | **P0** | Visión / calibración | Ambos scripts en **`BRING_UP=True`** (auto-WB + auto-gain ON) → invalida cualquier LAB calibrado al competir. | `cam-frontal-n6.py:41,79-87`; `cam-trasera-n6.py:37,77-83` | Tras V1: medir exposición real en cancha, fijar `EXPOSURE_US`, poner `BRING_UP=False` en ambos para congelar WB+gain+exposición. |
| V3 | **P0** | Visión / calibración | `H_MATRIX` placeholder e **idéntica** en frontal y trasera → coordenadas físicas erróneas (peor en la trasera, que necesita 4 puntos detrás). | `cam-frontal-n6.py:53-59`; `cam-trasera-n6.py:50-57`; `cameraFront-pack/04-calibracion...md:114-154` | Por cámara montada: cuadrado medible en el suelo, anotar (u,v) de 4 esquinas, calcular H con OpenCV, pegar. Medir `CAM_HEIGHT_CM`. Validar a 30/50/80/100 cm, error <10%. |
| I1 | **P0** | Integración / cableado | Cable **TOP→CENTRAL** (snapshot) movido a Serial7 pin29 por el swap; falta confirmar EN BANCO que el cable pin29→pin0 está hecho y que CENTRAL ve `snap_fresh=Y`. | `src/top/comm_central.cpp:27-35,49-62`; `src/central/comm_top.cpp:20,29-31`; `types.h:126` | Banco: cablear TOP pin29(TX7)→CENTRAL pin0(RX1) + GND; verificar `snap_fresh=Y` y heading siguiendo al rotar (ver §4). |
| F8 | **P1** | Comm / pinout | Conflicto **pin 7/8 en CENTRAL** (Serial2 RX2=pin7 vs INA/INB motor U17) bloquea pose OTOS DOWN→CENTRAL y arriesga pisar motor 2 ↔ RX2 con motores corriendo (TASK-036). | `src/central/config_central.h:29-31,37-39`; `src/central/comm_down.cpp:43-45`; `src/down/comm_central.cpp:147-149`; `platformio.ini:643-647` | Validar en banco motor 2 + Serial2 activos a la vez. Si se pisan: migrar L3 a Serial7 del CENTRAL (libre) y recablear DOWN pin1→CENTRAL pin28. **Bloqueante para FSM+línea+3 motores juntos.** |
| F6 | **P1** | Estrategia / visión | FSM conmuta con `ball_visible` **binario**, ignora `ball_confidence` → flicker SEARCH↔APPROACH y arquero saliendo por un destello. Probable con N6 recién calibradas y luz variable. | `src/central/strategy.cpp:193,216,270,372,398,410`; `src/central/world_model.cpp:52` | Histéresis temporal + umbral: pelota "vista" solo si `visible && confidence>=C` por N ms; "perdida" tras M ms. Lógica pura, testeable en host. |
| F4 | **P1** | Estrategia / juego | **Cero evasión** de obstáculos/rival: `min_obstacle_mm` llega a CENTRAL pero `strategy.cpp` nunca lo consulta → embiste rivales y paredes. | `src/top/main_top.cpp:84`; `src/central/world_model.cpp:60`; `strategy.cpp` (0 hits de min_obstacle) | Tras F1: gate reactivo en APPROACH — si `min_obstacle_mm<~150mm` y no es la pelota, bajar velocidad y sesgar lateral. No hace falta planner. |
| I2 | **P1** | Integración / estrategia | Mapeo arco color→lado **hardcodeado** (yellow=opp, blue=own) sin comando de lado del árbitro → si defendemos el amarillo, apunta al arco equivocado. | `src/top/main_top.cpp:74-81` | Definir cómo se setea el lado (dipswitch / comando árbitro / config por partido). Mínimo: flag que invierta opp/own. |
| F11 | **P1** | Actuador / kicker | `PIN_KICKER_SOL=23` es **placeholder sin confirmar** → el delantero podría no patear o togglear un GPIO equivocado (lógica de pulso ya es sólida). | `src/central/config_central.h:98-105`; `src/central/motors_zircon.cpp:37-78` | Confirmar con Enzo el GPIO real del solenoide en el Zircon; probar `kicker_fire=1` en banco (sujetar). Barato, desbloquea ataque (R2). |
| F7 | **P1** | Hardware / pinout | Conflicto **pin 10**: `PIN_TOF_XSHUT[1]=10` (LP ToF trasero) vs `PIN_ROLE_DIPSWITCH=10`. Latente hoy (rol por `#define`), pero se activa apenas F1 use los 4 LP. | `src/top/pinout_robot2.h:26-31,55-59`; `src/central/main_central.cpp:38-48` | Decisión HW (Enzo): reubicar dipswitch a otro pin o eliminar `PIN_ROLE_DIPSWITCH` del header y dejar el rol por build. Comentar "NO conectar dipswitch a pin 10". |
| V4 | **P1** | Visión / integración | `CAMERA_UNIT_TO_MM=10.0` **sin medición**; CENTRAL usa esos mm directo para umbrales approach/kick → escala mal toda la aproximación si H no reporta en cm. | `src/top/cameras_runtime.cpp:21-25` | Tras V3: medir pelota a 30/50/80/100 cm y ajustar el factor al valor medido (~30 min). |
| F5 / V5 | **P2** | Contrato / predicción | `ball_vx/vy` del WorldSnapshot v2 **nunca se setean** (viajan 0) pero `ball_trajectory.cpp` los consume → predicción de pelota muerta. (NO es la contradicción doc 24B/27B.) | `src/top/main_top.cpp:67-71`; `src/shared/types.h:104-105`; `src/shared/ball_trajectory.cpp:16-24` | Incheon: aceptar 0 y no usar `ball_trajectory` en el path activo, **o** calcular velocidad en TOP por diferencia finita (dt=10ms) + EMA. Documentar la elección. |
| I3 | **P2** | Contrato | `goal_own` solo tiene bit de visibilidad (sin ángulo/distancia); la fusión del arco azul se computa y se descarta → arquero no se orienta a su arco. | `src/shared/types.h:108-111`; `src/top/cameras_runtime.cpp:141-142`; `src/central/world_model.cpp:56-58` | Si el arquero lo necesita: extender snapshot con `goal_own_angle/distance` (rompe 27B→+4B, re-validar static_assert). Si no, deuda consciente. |
| F10 | **P2** | Línea / latencia | Umbral de freno efectivo en cancha es `imminent_depth=6` (down_model), no el `=3` de line_ring (deuda dual-chain intencional). Con n=8 (F2) "≥6" es casi inalcanzable → freno de borde podría no dispararse. | `src/central/main_central.cpp:95-102`; `src/down/comm_central.cpp:26-33`; `src/down/line_ring.cpp:40,126` | Tras F2 (n=32): validar en banco el umbral=6 vs velocidad real de cruce, objetivo latencia <15ms. No archivar ninguna cadena. |
| F12 | **P2** | Tests / cobertura | Sin test de la enumeración multi-ToF, del bypass de freno en `main_central`, ni de la banda 0-80mm de approach (`approach_velocity`=0 bajo 50mm pero kick a 80mm → riesgo de "plantarse"). | `test/` (23 suites, sin estos paths); `src/shared/pids.cpp:115-127`; `src/central/strategy.cpp:78,86` | Extraer decisión imminent→freno a función pura testeable; test de approach en banda 0-80mm; multi-ToF se valida en banco. |
| F9 / U1 | **P2** | Loop / ultrasonido | HC-SR04 con `pulseIn` bloqueante (hasta 25ms) está **OFF por flag** (`TOP_ENABLE_HCSR04` ausente de todo env) → riesgo neutralizado; el ToF U2 cubre la distancia frontal. | `src/top/sensors_tof.cpp:55-82,179-188`; `platformio.ini` (flag ausente) | Mantener OFF para Incheon. No reactivar hasta hacerlo no bloqueante (interrupción + `micros()`). |

---

## 3. Coherencia de documentación

El swap UART (TASK-204) y el bodge de ToF propagaron bien a los docs de mayor jerarquía (ESTADO-ACTUAL, FUENTES-DE-VERDAD, ARQUITECTURA mapa de flujo, CONTRATO-DATOS-TOP banner+cuerpo, banners de los packs) y coinciden con el código vivo. HC-SR04 4/3, ToF {9,10,11,12}→0x2A-0x2D, Wire1-liberado, baud 230400 y DOWN Serial5 (20=TX/21=RX) están **consistentes**. Quedan **8 inconsistencias reales** (ninguna bloqueante de juego — son riesgo de que alguien cablee/parsee el Serial equivocado o busque el BNO en el bus vacío):

| # | Sev | Inconsistencia | Evidencia | Fix concreto |
|---|-----|----------------|-----------|--------------|
| **D1** | P1 | **`CONTRATO-DATOS-CAMARAS.md` (canónico) quedó en el mapeo PRE-swap**: dice trasera=Serial7/RX28 y CENTRAL=Serial5 — al revés del HW y del código. Su gemelo del pack SÍ se corrigió. | `docs/firmware/CONTRATO-DATOS-CAMARAS.md:71-83` vs `cameras_runtime.cpp:92` (Serial5 trasera) + `comm_central.cpp:34` (Serial7 CENTRAL) | Reemplazar §1.1: trasera **Serial5/RX21,TX20**; poner el banner TASK-204. |
| **D2** | P1 | **Recableado dual-BNO está en el código pero en NINGÚN doc de prosa**: 3 docs siguen diciendo "2 BNO en buses separados Wire+Wire1, ambos 0x28". | `pinout_common.h:19-27` + `sensors_imu.cpp:10-12,34-35` vs `ARQUITECTURA:124`, `FIRMWARE-PLACA-ARRIBA:103,105`, `top-board-pack/01:42,80,93-96` | "ambos BNO en Wire (18/19): LEFT=0x28, RIGHT=0x29 (ADR a 3V3); Wire1 libre para DOWN". |
| **D3** | P1 | **`top-board-pack/01` contradicción INTERNA**: la tabla §1 dice "CENTRAL=Serial5, 20/21" mientras banner+tabla UART+resumen dicen Serial7. | `top-board-pack/01:47` vs `:23`, `:115`, `:213` | `:47` → "UART (Serial7, pines 28/29)". |
| **D4** | P2 | **`top-board-pack/03` banner OK pero cuerpo entero dice Serial2→CENTRAL**. | `top-board-pack/03:222,408,413,437,451,492` | Sincronizar el cuerpo con el canónico (Serial7). |
| **D5** | P2 | **`top-board-pack/06` desactualizado SIN banner**: "Serial2→CENTRAL", "TOP Serial2 NO CONFIRMADO". | `top-board-pack/06:259,388,411,421` | Re-copiar del canónico o banner TASK-204 + Serial7. |
| **D6** | P2 | **`top-board-pack/README` contradicción interna**: índice Serial7 pero árbol y pendiente #3 dicen Serial2/7-8. | `top-board-pack/README:52,106,151` | `:52`→Serial7; resolver pendiente #3; aclarar que 7/8 es del CENTRAL. |
| **D7** | P2 | **`ARQUITECTURA` topología ToF vieja**: "4 ToF (2 en cada bus I2C)" — el bodge los puso todos en Wire. | `ARQUITECTURA:125,141` | "4 ToF en bus único Wire, LP {9,10,11,12}→0x2A..0x2D". |
| **D9/D10** | P3 | Residuos en canónicos: `CONTRATO-DATOS-TOP:437` "Serial5"; G-TOP-01 `:451` abierto; `FIRMWARE-PLACA-ARRIBA:753,795,939` "Serial2" en el cuerpo. | (citas) | Actualizar a Serial7; marcar G-TOP-01 RESUELTO. |

**Drift de copias de firmware del pack de cámaras** (P2-P3): `cameraFront/Back-pack/firmware/teensy/config_top.h` es el monolítico viejo (HC-SR04 6/7, XSHUT {2,3,4,5}, ambos BNO 0x28, Serial7 "expansión"); `cameraFront-pack/01-hardware-y-conexion.md:13-16` dice cámara H7/OV5640 cuando es N6/PAG7936; `platformio.ini:690` (comentario de `diag_top_cameras`) sigue diciendo trasera=Serial7/RX28. La verdad de pines/UART vive en `src/top/pinout_common.h` + `pinout_robotN.h` + `comm_central.cpp`.

**Descartados (2):**
- **D8** (falso-positivo): `ARQUITECTURA:264` "Serial2 → recibe del ABAJO" está bajo la sección *Placa CENTRAL* (es el Serial2 del CENTRAL recibiendo del DOWN); consistente.
- **Contradicciones intencionales** (no se tocan): WorldSnapshot 24B vs 27B, doble cadena de línea de DOWN, y conflicto 7/8 del CENTRAL — correctamente etiquetadas en FUENTES-DE-VERDAD.

---

## 4. Líneas de comunicación inter-placa (ítem a cerrar)

Los 3 enlaces corren a **230400 baud** con el protocolo de `proto.h` (START/CRC16/SEQ/END). Cada enlace usa nombres de Serial distintos en cada punta (chips distintos en placas separadas; lo único que importa es el **pin físico**). Hay dos "Serial5" (DOWN→TOP vs cámara trasera del TOP) y dos "Serial1 RX en pin 0" (TOP recibe de DOWN; CENTRAL recibe de TOP) que **no colisionan** por estar en placas distintas.

| Enlace | Mensaje | TX (placa / Serial / pin) | RX (placa / Serial / pin) | Baud | Estado |
|--------|---------|---------------------------|---------------------------|------|--------|
| **TOP→CENTRAL** | WORLD_SNAPSHOT (27 B) @100Hz | TOP / Serial7 / **TX7 = pin 29** | CENTRAL / Serial1 / **RX1 = pin 0** | 230400 | **ASUMIDO** (no cableado) |
| **DOWN→TOP** | OTOS Pose2D + Velocity2D @100Hz | DOWN / Serial5 / **TX5 = pin 20** | TOP / Serial1 / **RX1 = pin 0** | 230400 | **ASUMIDO** (no cableado) |
| **DOWN→CENTRAL** | LINE_URGENT / LineStatusV2 (16 B) @200Hz | DOWN / Serial1 / **TX1 = pin 1** | CENTRAL / Serial2 / **RX2 = pin 7** | 230400 | **CONFIRMADO** (banco 2026-05-29, sin motores) |

*Retornos (admin/comandos):* CENTRAL→TOP por CENTRAL TX1=pin1 → TOP RX7=pin28. TOP→DOWN por TOP TX1=pin1 → DOWN RX5=pin21. CENTRAL→DOWN por CENTRAL TX2=pin8 → DOWN RX1=pin0.
*Único conflicto físico real:* CENTRAL Serial2 RX2=pin7 comparte pin con el driver del motor 2 (U17) — ver F8/TASK-036.

### CHECKLIST DE BANCO (confirmar cada enlace)

**Antes de empezar — marcar con cinta para no cruzar:** el **pin 21 del TOP** (cámara trasera soldada, Serial5) y el **pin 20 del DOWN** (TX a TOP).

**L1 — TOP→CENTRAL (snapshot):**
1. Cable señal: **TOP pin 29 (TX7) → CENTRAL pin 0 (RX1)**.
2. **GND común TOP↔CENTRAL obligatorio.**
3. (Opcional retorno admin) CENTRAL pin 1 (TX1) → TOP pin 28 (RX7).
4. Verificar que el cable sale por pin 29, **NO** por 20/21 (ahí está la cámara trasera).
5. Diag: flashear `top` + `central_robotN`, abrir monitor USB del CENTRAL @115200, mirar `main_central.cpp:128-129`.
6. **Esperado:** `snap_fresh=Y` estable + heading (`hdg=`) siguiendo al rotar el robot. Si `snap_fresh=N`: revisar TX29↔RX0 cruzado y GND.

**L2 — DOWN→TOP (odometría OTOS):**
1. Cable señal: **DOWN pin 20 (TX5) → TOP pin 0 (RX1)**.
2. **GND común DOWN↔TOP obligatorio.**
3. (Opcional retorno) TOP pin 1 (TX1) → DOWN pin 21 (RX5).
4. **OJO:** este enlace usa el **Serial1 del TOP (pin 0)**, NO el Serial5 del TOP (pin 21 = cámara). En DOWN sale del **pin 20** (Serial5 del DOWN).
5. Diag: flashear `down` (idealmente `-DDOWN_NUM_OTOS_CONNECTED=2`) + `top`; instrumentar `comm_down_get_frames_received()` del TOP o `main_down.cpp` con `-DDOWN_DEBUG_SERIAL`.
6. **Esperado:** `frames_received` del TOP crece a ~200 Hz y `is_pose_fresh()=true`.

**L3 — DOWN→CENTRAL (línea — cable ya validado, falta CON MOTORES):**
1. Cable señal: **DOWN pin 1 (TX1) → CENTRAL pin 7 (RX2)**.
2. **GND común DOWN↔CENTRAL obligatorio.**
3. (Opcional retorno) CENTRAL pin 8 (TX2) → DOWN pin 0 (RX1).
4. Diag de protocolo: flashear `diag_central_comm_down` en CENTRAL + `down` real; monitor USB CENTRAL @115200 → panel "LINEA DOWN→CENTRAL" con frames creciendo, CRC err=0.
5. **⚠️ VALIDACIÓN PENDIENTE CRÍTICA (TASK-036):** el cable anduvo SIN motores. Correr el firmware completo (`motors_init()` activo) y verificar simultáneamente que **(a)** el motor 2 gira bien **y (b)** el LineStatusV2 sigue llegando con CRC OK. Si se rompe cualquiera → migrar este enlace a **Serial7 del CENTRAL (pin 28)** y recablear DOWN pin1→CENTRAL pin28.
6. **Esperado:** motor 2 OK + `line_fresh=Y` en `main_central.cpp:131`.

---

## 5. Ultrasonido HC-SR04 — estado y cierre

**Estado actual:**
- **Pinout confirmado**: TRIG=pin 4 / ECHO=pin 3 (el diag T4/E3 lee, T3/E4 no → coincide con `pinout_common.h:60-61`). El viejo "conflicto pin 7" ya no existe.
- **Compilado FUERA del firmware de competencia**: todo bajo `#ifdef TOP_ENABLE_HCSR04`, flag ausente de todo env. Hoy el firmware NO toca pines 4/3 ni llama `pulseIn`. La distancia frontal la cubre el **ToF U2**. Riesgo de loop-blocking neutralizado.
- **Anomalía de banco sin cerrar**: lectura **intermitente** y en un momento **ambas orientaciones dieron la misma distancia** → sospecha de **(a)** puente/corto de soldadura entre pin 3 y pin 4, y/o **(b)** el HC-SR04 clásico no emite a 3.3V (necesita **Vcc=5V** con **divisor obligatorio en ECHO**, 1k+2k→3.3V).

**Checklist exacto restante para darlo por CONFIRMADO:**
1. **PUENTE:** sensor DESCONECTADO, multímetro en continuidad entre pin 3 ↔ pin 4; si pita → corto, reparar.
2. **IDLE:** `diag_top_ultrasonic` → `idle[p4 p3]`: ECHO sano idlea en 0.
3. **ALIMENTACIÓN:** Vcc = **5V** + **divisor 1k+2k en ECHO** antes del pin 3 (NUNCA 5V directo).
4. **BARRIDO:** mover la mano 5-50 cm; **SOLO** T4/E3 debe variar. Si ambas varían → sigue el puente.
5. **GND común** sensor ↔ Teensy.
- **Criterio de cierre:** T4/E3 varía monotónicamente con la distancia y T3/E4 queda en "sin eco". **No bloqueante para Incheon** (ToF U2 cubre la distancia frontal).

---

## 6. Próximos pasos sugeridos (orden recomendado)

**Se puede hacer YA (FIRMWARE/SOFTWARE — sin banco):**
1. **F2** — agregar `-DDOWN_NUM_MUXES_CONNECTED=4 -DDOWN_NUM_OTOS_CONNECTED=2` a `[env:down]` (1 línea, 32 sensores). ⚠️ confirmar antes que la placa de competencia es la fixeada (4 muxes), no la 04-12 vieja.
2. **F1** — reescribir `sensors_tof_init()` portando la enumeración ya validada de `diag_top_tof_quad_live` (desbloquea pose). *Riesgo bajo, alto impacto.*
3. **F3** — arranque manual fail-safe por botón en CENTRAL.
4. **F6** — histéresis + umbral de `ball_confidence` en la FSM (lógica pura, testeable).
5. **F4** — gate reactivo de evasión en APPROACH (después de F1).
6. **F12 / F5** — tests del path imminent→freno y banda 0-80mm; decidir alcance de `ball_vx/vy`.
7. **Docs** — D1-D10 (corregidos en el commit de este journal).

**Requiere BANCO (HARDWARE — humano):**
8. **Cablear y validar los 3 enlaces UART** (§4): L1 TOP→CENTRAL y L2 DOWN→TOP; reconfirmar L3. Primer hito end-to-end.
9. **F8 / TASK-036** — validar motor 2 + Serial2 simultáneos en CENTRAL; si se pisan, migrar L3 a Serial7. *Bloqueante para FSM + línea + 3 motores juntos.*
10. **Calibración de visión en orden:** V1 (LAB) → V2 (exposición + `BRING_UP=False`) → V3 (homografía) → V4 (`CAMERA_UNIT_TO_MM`). *DESPUÉS del hito 8.*
11. **F7** (dipswitch pin 10) y **F11** (GPIO kicker) — decisiones de Enzo.
12. **I2** — mapeo de arco por lado del árbitro.
13. **U2** — HC-SR04 (§5), opcional, no bloqueante.

**Regla de oro del bring-up:** cerrar el hito 8+9 (cables + conflicto 7/8) ANTES de invertir las horas de calibración fina de visión (10). El robot puede empezar a jugar con **heading + pelota relativa** (sin pose absoluta, que es Nivel 2).
