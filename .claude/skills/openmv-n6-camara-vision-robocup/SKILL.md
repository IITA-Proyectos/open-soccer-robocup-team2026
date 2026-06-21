---
name: openmv-n6-camara-vision-robocup
description: Usar cuando una cámara OpenMV N6 (SoC STM32N657, sensor PAG7936 global shutter, módulo MicroPython `sensor` NO `csi`) del robot de soccer "no ve" o ve mal — pelota naranja por color LAB / arcos amarillo+azul que no detecta, falsos positivos, blob partido, la cámara reporta posición errada (homografía), una de las dos cámaras (frontal/trasera) muerta en el Teensy, o el packet UART al TOP no llega/desincroniza. Cubre el pipeline find_blobs LAB en CPU (lo que CORRE hoy, sin NPU), bloqueo de exposición/ganancia/WB, homografía atada a VGA, protocolo v2 de 11 bytes con CRC8, fusión front+back y rotación 180° de la trasera, el árbol de diagnóstico cámara→UART→fusión, y cuándo (y cuándo NO) saltar a ML/NPU. Triggers - "la cámara no ve la pelota / ve fantasmas", "calibrar color / LAB / threshold", "find_blobs", "bloquear exposición / white balance / ganancia", "la posición que reporta está mal / homografía", "una cámara no manda / Serial3 Serial5 / cam_front_en", "el packet de la cámara no llega / CRC / desincroniza", "OpenMV N6 / PAG7936 / STM32N6", "NPU / YOLO / FOMO / Edge Impulse / Roboflow", "global shutter vs rolling shutter", "recalibrar en Incheon". NO es para el firmware del Teensy que CONSUME los datos (vibe-robotics-coding), ni para fusionar la POSE XY del robot (fusion-pose-odometria-landmarks), ni para elegir la técnica de localización (localizacion-rcj-soccer), ni para el mount mecánico (vibe-mechanical-design). La skill openmv-vision-tuning (titulada "H7") está DESACTUALIZADA: el hardware real es N6 — ésta la reemplaza.
---

# Cámaras OpenMV N6 — visión por color, contrato cámara→TOP y diagnóstico

## Principio central — "el preview ve, el robot no"

La N6 sobra de hardware: sensor 1 MP global shutter (cientos de FPS crudos) + Cortex-M55 + NPU de
600 GOPS. **El cuello de botella no es el silicio: es la CALIBRACIÓN bajo la luz real y el camino de
datos cámara→UART→fusión.** La frase ancla:

> **Si el preview del OpenMV IDE muestra el blob bien pero el robot no reacciona, la cámara está
> SANA: el bug vive en la CALIBRACIÓN (LAB/exposición — depende de la LUZ, no del silicio), en la
> HOMOGRAFÍA (atada a VGA) o en el TRANSPORTE/FUSIÓN (UART v2, CRC8, gating `cam_*_en`, rotación
> 180° de la trasera) — casi nunca en el sensor.**

Modelo mental de **3 capas**, calcado del approach BNO ([[bno055-imu-heading-robocup]], plantilla de
oro): **(1) óptica/sensor**, **(2) software-en-cámara** = LAB + exposición/WB + homografía +
protocolo v2, **(3) transporte+fusión-en-Teensy** = UART + CRC + gating `cam_*_en` + rotación 180°.
Mirar el preview del IDE PRIMERO manda la culpa a la capa correcta antes de tocar lente, cable o luz.

Dos reglas duras:
- **"preview a color" ≠ "detecta el objeto"** (un threshold LAB malo da preview perfecto y cero blobs).
- **"el IDE dibuja el blob" ≠ "el TOP recibe la coord"** (el packet puede no salir, salir con CRC
  malo, o la homografía mapearlo mal). Confirmá el EFECTO end-to-end.

⚠️ **El robot ya tiene el patrón GEMELO del BNO en cámaras:** un flag de config
(`cam_front_en`/`cam_back_en`, `cameras_runtime.cpp:109-110`) deja una cámara SANA fuera de la
fusión → "no ve" que NO es óptico. El árbol DERIVA la causa, no la asume.

## Cuándo usar / cuándo NO

USAR: la cámara no detecta pelota/arco; calibrar LAB; bloquear exposición/ganancia/WB; falsos
positivos / blob partido; la posición reportada está mal (homografía); una de las 2 cámaras muerta
en el Teensy; el packet v2 no llega/desincroniza (CRC); decidir si pasar a ML/NPU.

NO usar (rutear):
- **Firmware del Teensy que CONSUME los datos** (parser ya escrito, FSM, world_model) →
  `vibe-robotics-coding`.
- **Fusionar la POSE XY** (OTOS+ToF+heading+bearings de arco) → [[fusion-pose-odometria-landmarks]];
  **elegir la técnica de localización** → [[localizacion-rcj-soccer]]. El bearing al arco que sale
  de acá es una ENTRADA de aquellas.
- **Mount mecánico / óptica física** → `vibe-mechanical-design`.
- **Timing del loop del TOP** (I/O bloqueante al drenar UART, jitter) → [[tiempo-real-determinismo]].

⚠️ **Frontera con `openmv-vision-tuning`:** esa skill está titulada "H7" y describe pelota IR pasiva
+ arcos cyan/magenta → **DESACTUALIZADA** (el hardware es N6, pelota naranja por color, arcos
amarillo/azul). **Ésta la reemplaza** como fuente operativa. Marcar la vieja como legacy (ver
"Skills relacionadas").

Esta skill termina en "el robot VE bien (pelota+arcos) y el packet v2 llega al TOP con coords
correctas".

## N6 vs H7 — qué cambió y por qué importa (NO confundir)

Specs completas + tabla comparativa → [references/n6-hardware-y-specs.md](references/n6-hardware-y-specs.md).

| | OpenMV **N6** (lo que hay) | OpenMV H7 (lo que dice la skill vieja) |
|---|---|---|
| SoC | STM32N657, Cortex-**M55** @800 MHz | Cortex-M7 @480 MHz |
| NPU | ST Neural-ART, ~**600 GOPS INT8** | **sin NPU** |
| Sensor | **PAG7936** 1 MP **GLOBAL shutter** | OV7725/OV5640 **ROLLING shutter** |
| RAM | 64 MB SDRAM + 4,2 MB SRAM | 1 MB interna |
| Módulo MicroPython | **`sensor`** (en esta placa `csi` daba preview NEGRO) | `sensor` |

- **Consecuencia #1:** los umbrales LAB del H7 **NO se transportan al N6** — sensor distinto
  (PAG7936) → recalibrar SIEMPRE. Nunca copiar thresholds entre placas/sensores.
- **Consecuencia #2:** el GLOBAL shutter elimina motion-blur/skew de la pelota rápida → permite
  exposiciones cortas SIN distorsión geométrica (ventaja real que el H7 rolling no tiene).
- ⚠️ **No citar como dato firme:** la NPU es **600 GOPS** (INT8, entero), no "600 GFLOPS" (algunos
  artículos de prensa lo dicen mal). Es un acelerador de redes neuronales, no de propósito general.
- **Honestidad:** HOY el robot **NO usa la NPU**. El pipeline de producción es `find_blobs` LAB en
  CPU. Toda la potencia de IA está ociosa — y está BIEN (ver "Cuándo el color no alcanza").

## El pipeline que CORRE hoy (`main.py` de producción — find_blobs LAB en CPU)

Fuente: `hardware/electronics/camaras-openmv/main.py` (va EN las 2 cámaras; los `cam-*-n6.py` están
**DEPRECADOS**, banner 2026-06-08 — NO flashear).

| Paso | Qué hace | Ancla `main.py` |
|---|---|---|
| 1 | `reset()` + `RGB565` + **VGA** (640×480) | :20-22 |
| 2 | `hmirror`+`vflip` (montaje 180°) | :24-25 |
| 3 | dejar correr autos → `skip_frames(2000)` → **fijar WB/gain/exposure hardcoded** (gain 12,04 dB, exposure 100328 µs) | :29-40 |
| 4 | máscara de 2 triángulos en esquinas superiores (falsos positivos fuera de cancha) | :51-56, 89-99 |
| 5 | ROI = 97% superior (recorta el robot abajo) | :118 |
| 6 | `find_blobs` LAB: naranja=pelota, amarillo+azul=arcos, `merge=True` | :120-122 |
| 7 | blob más grande → homografía + corrección de perspectiva `(h-r)/h` | :105, 71-86 |
| 8 | clamp + coded(+100) → **packet v2** con `crc8`+`END` | :140-145 |

- **LAB** = tupla `(L_min,L_max,A_min,A_max,B_min,B_max)`. Se prefiere sobre RGB porque separa
  luminosidad (L) de cromaticidad (A=verde-rojo, B=azul-amarillo) → más robusto a la luz.
  Thresholds de producción en `main.py:47-49`.
- `pixels_threshold`: pelota chica → **7**; arcos grandes → **600** (`main.py:120-122`). Sensibles a
  la resolución (atados a VGA).
- ⚠️ **INCONSISTENCIA de thresholds (marcar, no homogeneizar):** `main.py:47-49` tiene UN set;
  `CALIBRACION-VISION-N6.md:17-24` declara OTRO "final" (2026-06-09); `main-comunicacion-vieja.py`
  un tercero. Los tres difieren. Cuál está REALMENTE flasheado es estado de hardware → cerrar en
  banco. El doc dice que `main.py` es la fuente única.
- ⚠️ **El flag `BRING_UP` NO existe en `main.py`** (vivía en los `cam-*-n6.py` deprecados):
  `main.py` fija los autos con valores hardcoded y trae el bloque "COPIA ESTAS 3 LINEAS" COMENTADO
  (`main.py:149-156`). `BRING_UP` (autos ON para calibrar / OFF para competir) es buena práctica
  reusable, pero documentala como mejora P2 — **NO como lo que corre hoy.**

## Bloquear exposición / ganancia / WB (lo que más rompe el LAB)

Con los autos ON, la luz cambiante hace derivar A/B del LAB; además el auto-exposure de OpenMV es
conservador (ajusta poco la exposición y compensa subiendo GANANCIA → mete ruido). **En competencia
los autos NO van activos.**

Patrón correcto (firmas con confianza media-alta — verificadas contra el book de OpenMV + foro +
`main.py`, NO verbatim de `omv.sensor.html` que dio 404 al fetch):
```python
sensor.set_auto_whitebal(True); sensor.set_auto_gain(True)   # dejar aprender
sensor.skip_frames(time=2000)
# leer y FIJAR:
sensor.set_auto_gain(False, gain_db=...)            # get_gain_db()
sensor.set_auto_whitebal(False, rgb_gain_db=(...))  # get_rgb_gain_db()
sensor.set_auto_exposure(False, exposure_us=...)    # get_exposure_us()
```
Es exactamente lo que hace `main.py:29-40`; el bloque que IMPRIME esos 3 valores para pegarlos está
comentado (`main.py:149-156`) — descomentarlo es el flujo de re-calibración.

**Regla de oro Incheon:** re-bloquear bajo la luz REAL del venue 15 min antes. Los valores de Salta
no sirven si la luz difiere. Procedimiento completo →
[references/calibracion-y-cuando-ml.md](references/calibracion-y-cuando-ml.md).

## Homografía (XY) — atada a VGA, NO tocar el framesize sin recalibrar

`transformarcoordenadas()` (`main.py:71-86`) convierte (u,v) píxeles → (X,Y) cm relativos, con
corrección de perspectiva `(h-r)/h`. La H actual es la de Elías 2026-06-07 (ultra-wide, VGA).

- ⚠️ **ATADA a la resolución:** cambiar VGA→QVGA para ganar FPS **INVALIDA la homografía** (y hay
  que revisar `pixels_threshold` de área). No cambiar framesize sin recalibrar — gotcha load-bearing.
- ⚠️ **INCONSISTENCIA de altura (marcar, no resolver):** `main.py:44` usa `h=95.0`; los docs y
  `main-comunicacion-vieja.py:65` usan `CAM_HEIGHT_CM=18.7`. **95,0 vs 18,7 NO está reconciliado** —
  es parámetro load-bearing. Cuál corresponde al montaje real = banco, no se afirma desde el repo.
- **COLOR vs HOMOGRAFÍA — qué se rehace cuándo:** el COLOR (LAB) depende de la LUZ → rehacer en
  Incheon. La HOMOGRAFÍA depende del MONTAJE → la H de Salta sirve en Incheon **si el robot no se
  desarmó**. Tooling: `solve_homografia.py` (mínimos cuadrados, `--csv`/`--validate`, 2026-06-15).
- ⚠️ **Docs desactualizados:** varios dicen "pegar la H en `cam-*-n6.py`" (DEPRECADOS). La corrección
  HI-6 (`CALIBRACION-HOMOGRAFIA-XY-N6.md:18-24`) dice leer `main.py`, pero NO se propagó a todos lados.
- ⚠️ `CAMERA_UNIT_TO_MM=10.0` (`cameras_runtime.cpp:33-41`) es **PLACEHOLDER** (correcto solo si la H
  reporta cm) — validar con pelota a 30/50/80/100 cm (<10% error, **TASK-022**).

## Protocolo v2 cámara→TOP (11 bytes, CRC8) — el contrato que toca el wire

Layout byte-a-byte completo → [references/protocolo-v2-camara-top.md](references/protocolo-v2-camara-top.md).
**FUENTE DE VERDAD = `src/top/cameras.h:33-57`.**

| byte | contenido |
|---|---|
| 0 | `201` HEADER1 (pelota) |
| 1-2 | Xp, Yp coded (+100) |
| 3 | `202` HEADER2 (arco amarillo) |
| 4-5 | Xam, Yam coded |
| 6 | `203` HEADER3 (arco azul) |
| 7-8 | Xaz, Yaz coded |
| 9 | **CRC8** = XOR de los bytes 0..8 |
| 10 | `254` END |

- **Coords X e Y SIMÉTRICAS:** `coded = valor + 100`, valor ∈ [-100,100] → byte ∈ [0,200]. El TOP
  decodifica `valor = byte - 100` (`cameras.cpp:8-11`).
- **Sentinel "no detectado" = byte coded `255`** (INALCANZABLE porque las coords reales se clampean a
  [0,200]); objeto no-visible si X_coded O Y_coded == 255 (`cameras.h:69`, `main.py:104`).
- **CRC8 = XOR simple** de los 9 bytes de datos, NO polinómico (`cam_crc8`, `cameras.h:77-81`;
  `crc8()`, `main.py:64-68`). **Las dos implementaciones DEBEN coincidir** o todo frame se descarta.
- **Parser lado-TOP:** state machine de 11 estados (`cameras.cpp:41-139`, `WAIT_HEADER1..WAIT_END`);
  valida 3 headers en posición fija + END + CRC; si falla DESCARTA el frame y cuenta
  `crc_errors_`/`resync_events_`. Solo PUBLICA el packet si todo chequea (atómico).
- **v1 vs v2:** v1 (9 bytes, sin CRC ni END, X asimétrica) perdía la mitad izquierda del FOV y
  desincronizaba con un bit-flip (bug R6). `main-comunicacion-vieja.py` es la referencia v1 INSEGURA
  — NO flashear (`cameras.h:9-31`).
- **Transporte:** **Serial3** = FRONTAL (RX pin 15, conector U8, `cam_id=0`); **Serial5** = TRASERA
  (RX pin 21, SOLDADA — SWAP TASK-204, `cameras_runtime.cpp:49-54`). Ambos **19200 8N1**
  (`UART_CAMERA1/2_BAUD`, `pinout_common.h:61,63`). El lado cámara abre `UART(3, 19200)` (`main.py:6`).
  Colchón RX 256 B Arduino-only (`cameras_runtime.cpp:166-169`). Watchdog `CAMERA_TIMEOUT_MS=1000`
  (`cameras_runtime.cpp:31`).

## Fusión front+back y rotación 180° de la trasera

`src/shared/cameras_fusion.{h,cpp}` (PURA, host-testeada — `test_cameras_fusion`, 16 tests).

- **La cámara TRASERA NO rota sus propias coords** — la rotación 180° la hace el TOP
  (`cam_obs_to_robot_frame`, `cam_id==1` invierte signo de x e y; contrato en `cameras_fusion.h:52-63`,
  impl en `cameras_fusion.cpp`). Regla de diseño: cada cámara reporta en SU marco; el TOP unifica.
  ⚠️ Algunos docs citan `cameras_fusion.cpp:25-29` para esto — el código real está más abajo
  (la cabecera lo declara en `cameras_fusion.h:52-63`).
- **Pelota:** ambas ven → media simple, `confidence=95` (consenso); una ve → esa, conf 80; ninguna →
  invisible (`fuse_ball_dual`, `cameras_fusion.h:65-78`). Confianza FIJA porque el protocolo NO
  transporta área de blob (limitación conocida).
- **Arcos:** polar vía `atan2(x,y)` con +Y=frente, +X=DERECHA → **+90° = arco a la derecha**
  (`cameras_fusion.h:38-50`; el comentario "+90=izquierda" fue corregido 2026-05-31). El mapeo
  amarillo/azul → rival/propio se autodetecta por `goal_polarity` con latch anti-rebote
  (`main_top.cpp:234-267`).
- ⚠️ **Gating por config = la trampa del flag (gemela del BNO):** una cámara entra a la fusión SOLO
  si `cam_alive AND g_top_cfg.cam_front_en/cam_back_en` (`cameras_runtime.cpp:109-110`). Un
  `cam_*_en=false` persistido en EEPROM deja una cámara SANA fuera → "no ve" que NO es óptico.
  **Loguear los flags al boot.**
- ⚠️ **Riesgo multi-cámara:** hoy front y back comparten thresholds LAB idénticos (un solo
  `main.py`). Asume que dos PAG7936 responden igual — validar por unidad bajo la misma luz.

## Cuándo el color NO alcanza → ML/NPU (y cuándo NO meterse)

- **Para pelota naranja + arcos azul/amarillo por COLOR, `find_blobs` LAB es lo más rápido y
  confiable** — corre en CPU, sin entrenar, se calibra en minutos. Es el método de producción HOY.
  **NO cambiarlo por ML "porque la N6 puede".**
- **El ML (YOLO/FOMO) conviene SOLO cuando el color no resuelve:** distinguir por forma/textura,
  detectar robots adversarios, ambientes con muchos falsos positivos de color. Costo real:
  recolectar+etiquetar dataset + entrenar (Edge Impulse/Roboflow) — días, no minutos.
- Si se adopta: entrenar → exportar TFLite INT8 → **CONVERTIR para NPU** (OpenMV IDE → Tools →
  Machine Vision → Convert Model for NPU) → escribir a ROM → `ml.Model('/rom/modelo.tflite')`.
  Cargar el `.tflite` CRUDO desconecta/reconecta el dispositivo. Portar un FOMO de una OpenMV vieja
  (RT1062) NO es plug-and-play (requiere conversión).
- **Honestidad de alcance:** la integración `ml.Model` en la fw de las N6 del repo NO está probada
  (el repo no usa la NPU hoy). **Capacidad capitalizable a 2027, NO operativa para Incheon** —
  documentarla así. Detalle → [references/calibracion-y-cuando-ml.md](references/calibracion-y-cuando-ml.md).

## Árbol de diagnóstico — "la cámara no ve" / "la posición está mal" (el corazón)

Orden barato→caro. DERIVA la causa, no la asume.

- **FASE 0 — ¿qué síntoma EXACTO?** (a) cero blobs (LAB/exposición/ROI/máscara); (b) blob partido en
  2-3 (merge/area_threshold/lente); (c) falsos positivos (público naranja, líneas, otro robot →
  ROI/máscara/L más estricto); (d) detecta pero la POSICIÓN está mal (homografía/h/unit_to_mm); (e)
  una cámara entera muda (UART/`cam_*_en`/Serial). Síntomas DISTINTOS → causas distintas.
- **FASE 1 — ¿la óptica/sensor vive?** En el IDE: preview a color (módulo `sensor`, no `csi`) +
  Threshold Editor sobre la pelota → ¿hay blob? **Verificación: mové la pelota → ¿el blob la sigue?**
  Si sí → óptica SANA, bifurcá a transporte/fusión (Fase 3). Preview NEGRO → módulo `csi`/firmware.
- **FASE 2 — calibración (preview OK pero cero/mal blob):** LAB bajo la luz REAL (no la del lab) con
  `get_histogram()`/`get_statistics()` sobre un ROI; exposición/WB bloqueados; ROI 97% no recorta el
  objeto; máscara de esquinas no tapa la pelota. **Verificación: detección estable a 20/50/100/150 cm.**
- **FASE 3 — transporte+fusión (detecta en el IDE pero el robot no reacciona):** ¿sale el packet?
  (`print` en `main.py:146`) → ¿al Serial correcto? (Serial3 front / Serial5 back) → ¿CRC OK?
  (`crc_errors_` del parser) → ¿`cam_*_en=true`? (gating `cameras_runtime.cpp:109`) → ¿la rotación
  180° de la trasera tiene el signo correcto? → ¿la homografía mapea bien? (`h=95` vs `18.7`,
  `unit_to_mm`). El punto donde el número se vuelve sentinel/0/errado = la etapa rota.
- **FASE 4 — una variable por vez, descartá con DATOS** (diag de banco).
- **FASE 5 — gate de verificación:** con la pelota en una posición CONOCIDA, confirmá que la coord
  que llega al TOP cae donde está. **"El IDE dibuja el blob" / "compila" / "tests pasan host-native"
  NO lo prueban.** Esta regla la cierra el equipo humano, no Claude.

## Errores comunes

| Síntoma | Causa raíz real | Trampa (lo que parece) | Fix + verificación |
|---|---|---|---|
| cero blobs aunque el preview es a color | LAB calibrado en otra luz (o copiado del H7, sensor distinto) y/o exposición en auto | "la cámara está rota" | recalibrar LAB con `get_histogram`/`get_statistics` bajo la luz REAL; bloquear exposición/WB; estable a 20/50/100/150 cm |
| una cámara entera muda pese a mandar packets | `cam_front_en`/`cam_back_en=false` en EEPROM → fusión la trata como caída (`cameras_runtime.cpp:109-110`) | "la cámara/UART está rota" (gemelo de `bno_left_en`) | loguear los flags al boot; forzar enabled; revisar comandos de config previos |
| detecta en el IDE pero el robot no reacciona | packet no sale / Serial equivocado / CRC8 no coincide entre `main.py:64-68` y `cameras.h:77-81` | "la detección no anda" (falla el transporte) | `print` `main.py:146`; verificar Serial3=front(15)/Serial5=back(21); leer `crc_errors_`/`resync_events_` |
| la pelota se reporta en posición errada | homografía: framesize cambiado (atada a VGA), `h=95` vs `18.7`, `UNIT_TO_MM=10` placeholder | "el sensor está descalibrado" | no cambiar framesize sin recalibrar; reconciliar `h` en banco; calibrar UNIT_TO_MM con pelota a distancias conocidas (TASK-022) |
| la pelota se detecta como 2-3 blobs chicos | `merge` off / `area_threshold` bajo / lente desenfocada | "la cámara pierde la pelota" | `merge=True` (`main.py:120`) + thresholds razonables (atados a VGA) |
| falsos positivos (público naranja, líneas, otro robot) | ROI no excluye el horizonte / máscara mal dimensionada / L muy permisivo | "el threshold está mal" (a veces es geometría) | ROI 97% (`main.py:118`) + máscara de esquinas (`:89-99`); endurecer L; que la máscara NO tape la pelota |
| busco `BRING_UP` en producción y no está | vivía en `cam-*-n6.py` (DEPRECADOS); `main.py` fija los autos hardcoded (`:37-39`) | "producción no bloquea la exposición" (sí, hardcoded) | `main.py` sí fija exposición/gain/WB; implementar `BRING_UP` es mejora P2, no un bug |
| la cámara trasera reporta la pelota del lado contrario | la rotación 180° se hace en el TOP (`cam_id==1`), si la cámara rota internamente se duplica | "la trasera está al revés / mal montada" | la trasera NO rota sus coords; verificar `cam_id` (back=1) y que el script no invierta de más |
| front y back ven colores distintos / la fusión da fantasmas | comparten thresholds LAB idénticos asumiendo dos PAG7936 iguales | "la fusión está mal programada" | calibrar LAB (y H) POR cámara; la media simple de "ambas ven" es imposible para una pelota física → falso positivo (ver `TOP_CAM_STICKY`) |

**Anti-racionalizaciones:** "la cámara está rota" → si el preview ve y el blob sigue la pelota, la
óptica está sana; el bug es calibración o transporte. "es el sensor / la N6 falla" → status del flag
`cam_*_en` y CRC primero. "la N6 tiene NPU, usemos YOLO" → el color por LAB ya resuelve pelota+arcos
en CPU; el ML es para lo que el color no puede, y cuesta días. "los thresholds del H7 sirven" → NO,
sensor distinto (PAG7936) → recalibrar. "compila / el IDE dibuja el blob" → no prueba que el TOP
reciba la coord correcta; verificá end-to-end.

## Skills relacionadas

- ⚠️ **`openmv-vision-tuning` — DESACTUALIZADA** (dice "H7", pelota IR pasiva, arcos cyan/magenta;
  status "outline only"). El hardware real es N6, pelota naranja por color, arcos amarillo/azul.
  **Esta skill la reemplaza como fuente operativa.** Recomendación: reescribirla/deprecarla para no
  dejar dos skills de cámara contradictorias (mismo error que `cam-*-n6.py` vs `main.py`).
- **Firmware del Teensy que consume cámara** (parser, FSM, world_model) → `vibe-robotics-coding`.
- **Fusionar la POSE XY** (los bearings de arco son una entrada) → [[fusion-pose-odometria-landmarks]];
  **elegir la técnica** → [[localizacion-rcj-soccer]].
- **Plantilla de oro** (mismo patrón 3-capas + árbol + trampa-del-flag; además heading BNO y bearings
  de arco se cruzan en la fusión) → [[bno055-imu-heading-robocup]].
- **Mount mecánico** → `vibe-mechanical-design`; **timing del loop del TOP** →
  [[tiempo-real-determinismo]].
- Método de debug → `superpowers:systematic-debugging`; verificar antes de cerrar →
  `superpowers:verification-before-completion`. Test en banco → [[hardware-test-protocol]];
  documentar/reconciliar inconsistencias → [[engineering-journal]].

## Referencias (no inflar el inline)

- [references/n6-hardware-y-specs.md](references/n6-hardware-y-specs.md) — specs de la OpenMV N6
  (SoC/NPU/sensor/memoria/consumo) + tabla comparativa N6 vs H7, con las cifras marcadas por confianza.
- [references/protocolo-v2-camara-top.md](references/protocolo-v2-camara-top.md) — el contrato v2
  byte-a-byte, CRC8, sentinel, transporte (Serial/baud/pines), parser y la historia v1→v2.
- [references/calibracion-y-cuando-ml.md](references/calibracion-y-cuando-ml.md) — procedimiento de
  calibración LAB/exposición/homografía para banco e Incheon, las inconsistencias load-bearing
  (thresholds, `h=95` vs `18.7`, UNIT_TO_MM), y el veredicto honesto sobre ML/NPU (2027, no Incheon).
