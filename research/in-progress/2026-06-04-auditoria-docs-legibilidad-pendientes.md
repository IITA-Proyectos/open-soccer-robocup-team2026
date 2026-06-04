---
date: 2026-06-04
status: vivo
tipo: evaluacion
titulo: Auditoria consolidada — mejoras aplicadas, docs, legibilidad y triage de pendientes
base: main 4821f7e (worktree soccer-main)
gate_host: 547 tests / 40 envs / 0 fallos (re-verificado 2026-06-04, matchea baseline)
tracks: [firmware-audit, systems-audit, docs-architecture, firmware-contracts, readability x4, backlog-triage]
---

# Auditoria consolidada 2026-06-04 — para el coach

Cuatro frentes de revision corridos en paralelo contra el **codigo vivo** en
`C:/Users/violl/iitasoccer/soccer-main` (NO el cwd senuelo): (A) auditoria
adversarial de los 6 fixes de hoy, (B) revision de docs de arquitectura +
contratos, (C) legibilidad de comentarios/codigo, (D) triage de TODO el backlog
pre-Incheon. Este doc consolida los ~50 hallazgos en lo accionable.

---

## RESUMEN EJECUTIVO (para el coach)

- **Los 6 fixes de hoy estan bien.** Ninguno introduce regresion en el camino
  vivo de competencia. 4 son impecables (SC-01 guarda NaN, SI-03 reset decoder,
  SB-3 media simple, CC-01 contador badsz). 2 tienen cabos sueltos de BANCO, no
  de codigo: SI-02 (validar drop-rate ~0) y VIS-02 (confirmar UART frontal).
- **Riesgo sistemico #1 que puede costar un PARTIDO (no estaba destacado):** las
  DOS cadenas de defensa de borde (freno EMERGENCY_LINE + estado LINE_AVOID)
  gatean AMBAS sobre el mismo `line_data_fresh()` (ventana 500 ms del enlace
  DOWN→CENTRAL). Si ese enlace cae >500 ms con el robot a velocidad, **no hay
  freno independiente → el robot puede salir de cancha**. Es BANCO (medir tasa de
  perdida real antes de tocar el cerebro), pero es lo mas probable de costar el
  punto inmediato.
- **Riesgo sistemico #2:** el freno de borde `motors_brake` (HIGH/HIGH) **NO
  esta confirmado en el Zircon** — si el driver hace COAST en vez de short-brake,
  la unica defensa rapida de borde no frena. Tambien sale de cancha. BANCO,
  prioridad alta: medir brake-vs-coast con el datasheet/Enzo.
- **Riesgo sistemico #3 (degrada, rara vez termina):** un BNO muerto que lee
  0.000 queda DEGRADED (no DEAD) y **sigue arrastrando el heading fusionado hacia
  0° con peso 0.25**. Con 1 solo BNO sano hoy, sesga la orientacion activamente.
  El motor de failover ya existe; falta que el glue baje `present`. BANCO (riesgo
  de falso-DEAD apaga el unico heading).
- **El informe base 2026-06-04 esta PARCIALMENTE STALE:** su lista "Aplicar YA"
  (linea 250) incluye quick-wins que **ya estan en el arbol** (SC-01, CC-01,
  SI-02, SB-3, SI-03, VIS-02). Reaplicarlos a ciegas = re-trabajo sobre codigo
  vivo. Verificar con grep antes de aplicar cualquier item "de la lista".
- **Docs: el doc-arbitro (FUENTES-DE-VERDAD) esta stale en 2 filas de alto
  impacto.** Dice WorldSnapshot "v2=27B" cuando el `static_assert` vivo es
  **v3=31B**, y lista como deuda abierta "cameras_runtime no llena ball_vx/vy"
  que ya esta **RESUELTA y cableada** (verificado en cameras_runtime.cpp:143-154).
  Un indice arbitro impreciso es peor que ausente: la gente confia en el.
- **Docs: dos contratos desincronizados (HIGH).** CONTRATO-DATOS-CENTRAL describe
  LineStatus v1 (5B) como "codigo actual" cuando CENTRAL ya migro a LineStatusV2
  (16B); y CONTRATO-DATOS-CAMARAS tiene secciones v1 que **ensenan el sentinel
  viejo** (0,0)/(0,100), contradiciendo su propio changelog v2 y el parser real
  — peligroso porque guia la programacion de vision (bloqueante #1).
- **Legibilidad: el codigo esta excepcionalmente bien comentado.** Las lecciones
  de banco (I2C@100kHz+BNO@20Hz, omega clamp ≤327, U17 invertido, arbitro GPIO,
  OTOS frame freshness) estan intactas y se deben PRESERVAR. Los mejores wins son
  resumenes en lenguaje llano AGREGADOS arriba de los bloques densos. ~40
  hallazgos, casi todos solo-comentario, seguros de aplicar ya.
- **Bloqueante real #1 sigue siendo TASK-022** (recalibrar LAB + homografia de
  las 2 N6). Es 100% BANCO. Lo unico que un agente de escritorio puede aportar es
  `diag_cam_acceptance` (codigo nuevo de banco, riesgo de regresion NULO) para
  validar reportado-vs-esperado contra posiciones medidas.
- **Separacion neta:** ~10 cambios SEGUROS de escritorio (comentarios + 3-4
  codigo-puro host-verificable) vs ~15 items de BANCO/DATO-HUMANO. El plan de
  abajo prioriza por valor: empezar por diag_cam_acceptance + POSE-01 + las
  simplificaciones cero-riesgo + las correcciones de docs.

---

## (A) AUDITORIA CRITICA DE LAS MEJORAS

### A.1 — Que de lo aplicado HOY puede fallar todavia

Los 6 fixes del 2026-06-04 fueron auditados leyendo el codigo real. Veredicto:
**ninguno rompe el camino vivo.** Detalle por fix:

| Fix | Estado | Que puede fallar todavia | Accion |
|---|---|---|---|
| **SC-01** guarda NaN en `omega_degps_to_centideg` (`pids.cpp:35`) | Impecable | Solo teorico: si algun dia se compila con `-ffast-math`, el check `x==x` se desarma (habria que ir a chequeo por bits). No pre-Incheon. | Dejar. Opcional: comentar que la guarda asume NO `-ffast-math`. |
| **SI-03** `default: reset()` en `FrameDecoder::feed` (`proto.cpp:117`) | Correcto, defensivo, inalcanzable hoy | Deja un `return false;` muerto en la linea 125 (preexistente). | Dejar. Borrar el return muerto es cosmetico en modulo de comms muy testeado → diferir post-Incheon. |
| **SB-3** media simple en `fuse_ball_dual` (`cameras_fusion.cpp:47-48`) | Impecable para su alcance | Las 4 lineas de fusion **siguen sin clamp** (SB-4 pendiente). Hoy es no-op; si se sube `UNIT_TO_MM` sin clamp, el cast a int16 puede overflowear. | Cerrar SB-4 (clamp_to_i16) JUNTO con la recalibracion de homografia, NO suelto. |
| **CC-01** contador `badsz` (`comm_top.cpp` + `main_central.cpp:174`) | Impecable, cableado en los 3 puntos | Nada. | Ninguna. Observabilidad barata de pit (distingue "cable suelto" de "firmware desfasado"). |
| **SI-02** gate `availableForWrite` en TX del TOP (`comm_central.cpp:69-74`) | Correcto, aditivo | El drop es **silencioso salvo telemetria** (`g_frames_tx_dropped`). Si el contador NO se imprime en el DIAG del TOP, el fix pierde su valor. | BANCO: confirmar drop-rate ~0. ESCRITORIO: verificar que `tx_dropped` se imprima en `main_top.cpp` (si falta, add aditivo de 1 linea — ver doable_solo). |
| **VIS-02** `round()` vs `int()` en ambas camaras (`cam-frontal-n6.py:153`, `cam-trasera-n6.py:148`) | Correcto, aplicado en ambas | `round()` de MicroPython es **banker's rounding** (sesgo <0.5cm, irrelevante; solo importarlo al comparar al medio-cm en aceptacion). Cabo NO tecnico: `UART_PORT` de la frontal sigue sin confirmar (VIS-01, banco). | Dejar el fix. BANCO: confirmar UART frontal probando 1/2/3 (NO a ciegas). |

**Patron transversal (GEN-01):** hay 2 contadores de diagnostico nuevos
(`badsz`, `tx_dropped`). `badsz` ya se imprime (`main_central.cpp:174`).
**Confirmar/agregar la impresion de `tx_dropped` en `main_top.cpp`** es la unica
accion de escritorio que faltaria para que SI-02 rinda. Es aditivo (un
`Serial.print`); leer primero el print actual para no romper el parseo de un
script de telemetria.

### A.2 — Riesgos sistemicos que MAS probablemente rompan un partido

Ordenados por probabilidad de costar el punto inmediato (los dos primeros
terminan con el robot FUERA de cancha = perdida directa; el tercero degrada):

1. **Enlace DOWN caido → sin defensa de borde independiente (SYS-1, HIGH).**
   `EMERGENCY_LINE` (`main_central.cpp:127`) y `LINE_AVOID`
   (`strategy.cpp:376,617`) gatean AMBOS sobre el mismo `line_data_fresh()`
   (`LINE_TIMEOUT_MS=500`, `world_model.cpp:54`). `SAFE_NO_TOP` cubre la caida
   del enlace TOP, NO del DOWN. Si DOWN cae >500 ms mientras KICKOFF/SEARCH/
   INTERCEPT comandan avance → no hay freno ni retroceso. **BANCO:** medir tasa
   real de perdida del enlace DOWN (telemetria `down[crc=/lost=]` ya existe) y
   decidir politica fail-safe (degradar velocidad, no full-stop que regala
   posesion). Toca `strategy.cpp` → no a ciegas. Quick-win seguro independiente:
   contador/flag de "DOWN stale durante RUN" para cuantificar.

2. **Freno de borde sin confirmar en HW (CA-03/CC-02, HIGH).**
   `motors_brake` pone INA=INB=HIGH (`motors_zircon.cpp:81-91`), **sin confirmar
   que el Zircon Rev v15 frene** (header marcado SIN CONFIRMAR). Si hace COAST,
   la unica defensa rapida de borde no frena. **BANCO, prioridad alta:** medir
   brake-vs-coast (datasheet del driver con Enzo). Si COAST: NO improvisar pulso
   de reversa a ciegas (tira el robot afuera); primero dar mas margen en
   `imminent_exit` y/o freno por reversa calibrado en banco. Mientras no se mida,
   **asumir COAST y dar margen extra**.

3. **BNO muerto sesga el heading, no solo lo congela (IMU-1-REVISED, HIGH).**
   `present=g_ready[i]` (`sensors_imu.cpp:248`) nunca baja a false → un BNO
   congelado en 0 queda DEGRADED (no DEAD) y **sigue pesando 0.25 en el
   circular_mean**, arrastrando el heading hacia 0°. Con 1 solo BNO sano hoy,
   sesga activamente. El motor de failover (`dead_after_misses=5 → DEAD`) ya esta
   host-testeado; falta que el glue baje `present`. **BANCO:** detectar
   congelamiento EXACTO (gyro_z==0.000 Y heading sin cambio por N ticks) o
   `i2c_present(addr)` cada ~10-20 ticks (NO por tick — reintroduce la contencion
   BNO+ToF que congela el yaw, leccion 2026-06-02). Umbral agresivo → falso-DEAD
   apaga el unico heading. La guarda complementaria (SC-01) ya esta del lado
   control.

4. **Pelota stale hasta 1s (CAM-02-REVISED, MEDIUM).** `CAMERA_TIMEOUT_MS=1000`
   (`cameras_runtime.cpp:17`) y `world_model_ball_visible` no tiene gate propio
   de edad de pelota. La "mitigacion por frescura" que asume el informe NO existe
   como gate especifico. **BANCO (tune, acoplado a TASK-022):** evaluar bajar el
   timeout de la VISIBILIDAD a ~150-250ms tras medir la tasa real de packets de
   las N6 (sin LAB recalibrado probablemente <30Hz → parpadeo si se baja sin
   medir).

5. **Deploy wire-breaking piecemeal mata la cadena (AUDIT-2, MEDIUM).**
   Re-flashear camaras/TOP/CENTRAL por separado con contratos v2/v3 = sintoma
   identico a cable suelto. **PROCESO (seguro):** runbook de pit — "cualquier
   re-flasheo que toque un contrato wire = re-flashear las 5 unidades JUNTAS +
   correr diag_top_cameras + verificar fr>0". Opcional (toca glue TOP): contador
   de "bytes recibidos pero 0 packets decodificados por camara".

---

## (B) DOCS — imprecisiones doc↔codigo y donde simplificar

Todas las correcciones son **doc-only, behavior-neutral, safe_now=true** salvo
donde se indica. NO se toco `strategy.cpp` ni comportamiento.

### B.1 — Alta prioridad (el lector confia y se equivoca)

| Doc | Problema | Accion | safe_now |
|---|---|---|---|
| `docs/FUENTES-DE-VERDAD.md:30` | Fila WorldSnapshot dice **v2=27B**; vivo es v3=31B (`static_assert` types.h) | Actualizar celda a v3=31B; mover 27B a "superados" | ✅ |
| `docs/FUENTES-DE-VERDAD.md:67-68` | Deuda #3 "cameras_runtime no llena ball_vx/vy" ya esta RESUELTA (cableado en cameras_runtime.cpp:143-154 + main_top.cpp:131-132) | Marcar RESUELTA (2026-06-03) o eliminar, apuntar a `ball_velocity` | ✅ |
| `docs/firmware/CONTRATO-DATOS-CENTRAL.md` §2.2/§4.2/GAP-005/006 | Describe LineStatus **v1 (5B)** como "codigo actual"; CENTRAL ya consume **LineStatusV2 (16B)** via `lsv2_from_frame` | Actualizar a v2, marcar GAP-005/006 RESUELTOS, bumpear nota de estado | ✅ (reescritura larga → la revisa el dueno del doc) |
| `docs/firmware/CONTRATO-DATOS-CAMARAS.md` §2.1/§3/§8.1/§8.3 | Aun ensenan el sentinel **v1** (0,0)/(0,100) y citan codigo inexistente, contra su changelog v2 y el parser real | Rotular §2.1 "historia v1 (cerrada)"; reescribir §8.1/§8.3 a v2 (sentinel 255, 11B, CRC8+END=254). Minimo: aviso al tope. **Guia la programacion de vision (bloqueante #1)** | ✅ |
| `docs/firmware/CONTRATO-DATOS-DOWN.md` §5/§7 | Apunta a `src/down/comm_*.cpp` como "encoders actuales (v1)" | **Verificar primero** si DOWN ya emite v2 (16B). Si DOWN sigue en v1 mientras CENTRAL parsea v2 → enlace roto en banco | ❌ (requiere leer el emisor DOWN) |

### B.2 — Media/baja prioridad (confunde pero no descarrila)

| Doc | Problema | Accion | safe_now |
|---|---|---|---|
| `docs/ARQUITECTURA-3-PLACAS-2026.md:74` | Tabla FSM CENTRAL lista PUSH/KICK/GOALKEEPER_PATROL que **NO existen** en strategy.cpp | Reemplazar por estados reales por rol (verificado strategy.cpp:57-61); detalle canonico = strategy.cpp | ✅ |
| `docs/MAPA-DE-DATOS.md:41` | Paquete camara dice **9B**; contrato vivo v2=**11B** | Cambiar a 11B (v2), mencionar CRC8+END+sentinel | ✅ |
| `docs/firmware/CONTRATO-DATOS-CENTRAL.md` §5.1/§10 | `comm_top.cpp` listado en Serial1; usa **Serial7** | Corregir 3 menciones a Serial7 (resuelve contradiccion interna) | ✅ |
| `docs/ARQUITECTURA-3-PLACAS-2026.md:435-436` | cam1/cam2 rotuladas "proto viejo 9B" | Cambiar a "cam v2 11B (CRC8+END)" | ✅ |
| `docs/ARQUITECTURA-3-PLACAS-2026.md:389-394,407,432` | Banner/ASCII dicen v2/27B y "24B"; vivo v3/31B | Banner 27→31 (v3); opcional ASCII "(24B)"→"(31B v3)" | ✅ |
| `comm_arbiter.h:11` (TOP) | Lista `COMM_REFEREE_CMD` por UART (obsoleto desde TASK-039, arbitro=GPIO 5/6) | Marcar obsoleto, apuntar a lectura GPIO; status/partner siguen UART | ✅ |
| `cameras_runtime.cpp:19` (TOP) | Comentario de `CAMERA_UNIT_TO_MM` describe rango X del proto **v1** | Actualizar a convencion simetrica v2 ([-100,100] ambos ejes) | ✅ |
| `sensors_tof.cpp:24` (TOP) | Cabecera "Estado HW 2026-05-24" stale vs multi-ToF activo | Nota al pie: con `TOP_ENABLE_MULTI_TOF` corren los 4 ToF | ✅ |
| `FIRMWARE-PLACA-CENTRAL.md` §11.2/§15, `FIRMWARE-PLACA-ARRIBA.md` §13.1/§7.1 | Specs de diseno con snippets stale (24B/27B, LineStatus 5B) | Nota de vigencia en cabecera apuntando a contratos canonicos + types.h | ✅ |
| `CONTRATO-DATOS-CENTRAL.md` GAP-014 | "No hay WDT en CENTRAL" (cierto), no nota que TOP **ya tiene** WDT | Nota: portar `watchdog_init_1s/feed` de main_top.cpp; mantener GAP abierto | ✅ |

### B.3 — Donde simplificar (onboarding)

- `docs/README.md:11-15` — **no enlaza MAPA-DE-DATOS**, que es el mejor
  "start-here" para un recien llegado. Agregarlo como 3er item de los "indices
  vivos", antes de mandar a ARQUITECTURA (el mas largo y stale). ✅
- `docs/ARQUITECTURA-3-PLACAS-2026.md:285` — fila huerfana `DOWN_ODOM` incrustada
  en medio de "Asignacion fisica de UARTs". Borrar la duplicada (seguro); mover
  el resto a la tabla "Protocolo de mensajes". ✅
- **NO** falta un diagrama/resumen general: MAPA-DE-DATOS §1-2 ya lo es. Basta el
  cross-link del README.

**Nota de scope (regla doc-consistency):** NINGUNA de estas tocan las
contradicciones intencionales de FUENTES (la nota SEEK/DRIVE vs SEARCH/POSITION
es de naming, verificada — no cubre PUSH/KICK). Todo lo de arriba son
desincronizaciones reales doc↔codigo, no contradicciones intencionales.

---

## (C) LEGIBILIDAD — mejores wins de claridad

El codigo esta **excepcionalmente bien comentado** y carga lecciones de banco que
NO se deben perder. La estrategia ganadora es AGREGAR un resumen en lenguaje
llano arriba del bloque tecnico denso (un no-experto entiende el QUE antes de
vadear el POR-QUE), preservando cada hecho de banco verbatim.

### C.1 — Solo comentario, SEGUROS de aplicar ya (behavior_neutral, gate intacto)

| # | Archivo:linea | Win |
|---|---|---|
| R1 | `central/main_central.cpp:96` | Mapa llano de las 4 fases arriba de `loop()` |
| R2 | `central/main_central.cpp:1-10` | Linea llana del rol de la placa en el header |
| R3 | `central/main_central.cpp:123-134` | Aclarar por que `EMERGENCY_LINE` retorna a mitad de loop (preservar 15ms + active-brake) |
| R4 | `central/motors_zircon.h:24-31` | `motors_brake`: warning llano arriba, caveat HW-sin-confirmar verbatim debajo |
| R5 | `central/world_model.h:28-34` | Caveat "#16 accessor existe pero la FSM lo ignora" en una linea llana + lista + "verify con grep" |
| R6 | `central/world_model.cpp:1-5` | Header de 2 lineas: espejo read-side del snapshot TOP + linea/OTOS DOWN, watchdogs 500ms |
| R7 | `central/comm_down.cpp:22-25` | Conclusion llana arriba del bloque `frames_lost` (crib note de campo) |
| R8 | `central/comm_top.cpp:20-35` | Lead llano en el comentario CC-01 (badsz) |
| R9 | `central/strategy.cpp:240-298` | Resumen llano de 3 lineas arriba del doc-block de `gk_classify_intercept` (solo comentario, no toca codigo) |
| CMT-01 | `top/main_top.cpp:14` | Aclarar snapshot v3=31B + linea de "Seguridad: watchdog" |
| CMT-02 | `top/comm_arbiter.h:11` | (= B.2) marcar `COMM_REFEREE_CMD` obsoleto → GPIO |
| CMT-03 | `top/main_top.cpp:111` | Frase llana arriba del bloque "heading siempre del IMU" |
| CMT-04 | `top/sensors_imu.cpp:62` | Explicar "band-aid" y el por-que del 20Hz (choque ToF → yaw congelado) |
| CMT-05 | `top/cameras_runtime.cpp:19` | (= B.2) rango X a convencion simetrica v2 |
| CMT-06 | `top/sensors_tof.cpp:24` | (= B.2) nota multi-ToF activo |
| CMT-07 | `top/comm_central.cpp:22` | Separar "hoy solo cuenta" de "futuro reset/calib" |
| CMT-08 | `top/cameras_runtime.h:14` | Aclarar marco del robot (no de cancha) en coords |
| CMT-09 | `top/main_top.cpp:227` | Nombrar el ciclo del loop (drenar→sensar→fusionar→enviar) |
| CMT-10 | `top/sensors_imu.cpp:269` | Frase de proposito antes del despeje `offset=raw-target/SIGN` |
| DOWN-01 | `down/comm_central.cpp:120-127` | **Hacer EXPLICITA la "doble cadena de linea"** (deuda viva): esta funcion NO usa la salida de `line_ring`, reprocesa los raw con `DownModel` |
| DOWN-02 | `down/comm_central.h:26-27` | Precisar 200Hz fijo (no "100-200Hz") en 3 comentarios |
| DOWN-03..10 | `down/config_down.h`, `down_tx.cpp`, `otos.cpp`, `line_ring.{h,cpp}`, `main_down.cpp` | Aclaraciones llanas: scrambling de Enzo, "golden 23B", teleport-al-origen OTOS, colision LED/MUX pin13, bus de emergencia, ALS-PT19, filtro espacial |
| RD-01..10 | `shared/line_geometry.{cpp,h}`, `line_calib.cpp`, `down_model.h`, `down_encode.cpp`, `localization.cpp`, `line_filters.cpp` | Constantes magicas 55/125 → 90°±35°; pasos del corner-detection; EMA de `lc_adapt_carpet`; banner de `down_model.h`; etc. (todos doc/comentario) |

> Total solo-comentario seguro: ~38 wins. Conservan cada hecho de banco verbatim;
> el lenguaje llano se AGREGA, nunca sustituye al detalle tecnico.

### C.2 — Toca codigo/cerebro → DEFER (regla dura: no tocar strategy.cpp pre-Incheon)

| # | Archivo | Cambio | Por que defer |
|---|---|---|---|
| R10 | `central/strategy.cpp:90-91,554-555` | Rename `ATK_KICK_DIST_MM/ANGLE_DEG` → `ATK_PUSH_*` (no hay kicker fisico) | Behavior-neutral (constantes locales, mismos call-sites) PERO toca identificadores del cerebro. Dejar como propuesta, validar gate verde antes de mergear. Post-Incheon. |

---

## (D) PENDIENTES — triage completo clasificado

Clasificacion: **programable-solo-ahora** (escritorio, host-gate o pio-compile,
sin banco, sin tocar strategy.cpp) · **banco** (requiere hardware/medicion) ·
**dato-humano** (deliverables del equipo).

| Item | Clase | Severidad | Plan |
|---|---|---|---|
| **POSE-01** ejes invertidos en `pose_fusion.{h,cpp}` (header dice X=2430/Y=1820; canonico es X=1820 lateral / Y=2430 arco-a-arco) | programable-solo-ahora | medium | Corregir comentario L22-23 + defaults `field_width_mm=1820`, `field_height_mm=2430` (`pose_fusion.h:38-39`, `.cpp:55-56`). **Modulo NO cableado** → cero runtime. LEER `test_pose_fusion` primero (si hardcodea valores invertidos, actualizar en el mismo commit). Verify: `run-host-tests.sh test_pose_fusion` verde. |
| **POSE-02** knobs muertos `otos_stale_ms`/`tof_stale_ms` en `PoseFusionConfig` | programable-solo-ahora | low | NO borrar (test puede init por posicion). Agregar comentario "hoy no se consumen". |
| **POSE-03** rama muerta en tie-break `imu_fusion.cpp:238-241` (a<b siempre) | programable-solo-ahora | low | Simplificar a `else pick=a;` + comentario (LEFT = menor indice). Recompilar `test_imu_fusion`. Bajo valor; agrupar. |
| **TOF-1** path no-MULTI deja I2C a 400kHz (`sensors_tof.cpp:260`) | programable-solo-ahora | low | Cambiar 400000→100000 (iguala path MULTI). Rama no compilada en competencia → behavior-neutral; verify pio compile top_robot1. |
| **TOF-3** HC-SR04 sin filtro de frescura (`sensors_tof.cpp:368`) | programable-solo-ahora* | low | Darle timestamp propio (`g_hcsr04_last_ok_ms/ever_ok`) y pasarlo por `tof_fresh_or_no_reading`. *REQUIERE leer el sitio donde se setea `g_hcsr04_mm`; si no hay timestamp facil, queda pending. HC-SR04 off por default → behavior-neutral en competencia. |
| **TOPHAL-01** sin `static_assert` de `NUM_TOF` (`pinout_common.h`) | programable-solo-ahora | low | `static_assert(NUM_TOF == 4, ...)` apuntando a los arrays `tof_*[4]` de localization.h. Con NUM_TOF=4 el build no cambia. Verify pio compile top_robot1. |
| **CC-04** `comm_top_tick`/`comm_down_tick` devuelven int descartado | programable-solo-ahora | low | Cambiar firma a void (.h/.cpp ambos). Solo `main_central.cpp:100-101` los llama. Conservar usos internos del contador si los hay. Verify pio compile central_robot1. |
| **DC-2** `slip_estimate` cast a uint8 sin saturacion inferior (`down/comm_top.cpp:88`) | programable-solo-ahora | low | Agregar helper puro `sat_u8` en `telemetry_sat.h` (espejo de `sat_i16`) + 3-4 casos a `test_telemetry_sat`. Confirmar include path de telemetry_sat desde src/down. |
| **DIAG-CAM-ACCEPTANCE** crear sketch de aceptacion de vision | programable-solo-ahora (escribir+compilar) / banco (correr) | medium | Nuevo `src/diag/diag_cam_acceptance.cpp` + env, REUSA el parser de produccion (`cameras.cpp`), imprime reportado-vs-esperado mm/° con PASS/FAIL. Riesgo de regresion NULO (aislado en src/diag). Reduce el riesgo del bloqueante #1. La corrida real es banco. |
| **SI-02 print** exponer `tx_dropped` en DIAG del TOP | programable-solo-ahora* | low | *Leer el print de `main_top.cpp` primero (no romper parseo de telemetria), luego add de 1 `Serial.print`. |
| **AUDIT-1 / doc-estado** marcar quick-wins ya aplicados en el informe base | programable-solo-ahora (doc) | medium | Marcar SC-01/CC-01/SI-02/SB-3/SI-03/VIS-02 = APLICADOS en `2026-06-04-analisis-paralelo-modulos.md:250`. Evita re-trabajo. |
| **IMU-1** failover BNO muerto | banco | high | Detectar congelado EXACTO o `i2c_present` periodico (NO por tick). Validar robot quieto no da falso-DEAD. Toca I2C delicado del TOP. |
| **CC-02/CA-03** brake-vs-coast del Zircon | banco | high | Medir tiempo de parada; si COAST, margen extra en `imminent_exit` + freno por reversa calibrado. |
| **SYS-1** politica fail-safe DOWN-stale | banco | high | Medir tasa de perdida real del enlace DOWN; decidir degradar-velocidad vs full-stop. Toca strategy.cpp. |
| **TASK-022** recalibrar LAB + homografia + UART frontal + exposicion (2 N6) | banco | high | 100% calibracion en venue. Validar con diag_cam_acceptance. Bloqueante #1. |
| **CAM-02** tune `CAMERA_TIMEOUT_MS` de visibilidad | banco | medium | Medir tasa de packets N6, bajar timeout de visibilidad sin hacer parpadear ball_visible. Acoplado a TASK-022. |
| **VIS-01** confirmar UART frontal (1/2/3) | banco | low | Probar contra Serial del TOP, fijar `UART_PORT`. No a ciegas. |
| **SB-4** clamp en fusion de camaras | banco (junto a homografia) | low | `clamp_to_i16` en las 4 lineas, coordinado con el factor UNIT_TO_MM al recalibrar. |
| **BANK-TUNE** conductas dormidas (ball_predict, drive_straight OTOS, GK cross_track, OTOS sep) | banco | medium | Tunear gains/signos con robot armado. Modulos puros ya host-tested. NO tocar strategy.cpp pre-Incheon. |
| **DELIVERABLES** nombre equipo, costos BOM, fotos, video, roster (`MEJORAS-PENDIENTES.md`) | dato-humano | high | Equipo humano. Sub-items parcialmente automatizables en tarea APARTE: Fig.8 grafico de crecimiento de tests + propagar la cifra 547/40/0 a README/ESTADO-ACTUAL/MEMORY. |
| **POST-INCHEON-DEBT** doble cadena linea, RX duplicado, CRC duplicado, header tuning, archivar legacy | banco/post-Incheon | low | Diferir. Unificar CRC puede invalidar calib EEPROM; doble cadena rompe builds diag sin red de tests; strategy_transitions toca el cerebro. |

---

## PLAN DE EJECUCION

### Aplicar YA (seguro, verificable de escritorio)

**Codigo-puro host-verificable (gate 547 debe quedar verde):**
1. **POSE-01** — corregir ejes en `pose_fusion.{h,cpp}` (leer test primero). *El
   de mayor valor: bug latente real, fix seguro porque el modulo no corre.*
2. **POSE-03** — simplificar tie-break muerto en `imu_fusion.cpp`.
3. **DC-2** — `sat_u8` en `telemetry_sat.h` + casos de test.

**Codigo compile-only (pio, no entra al gate host):**
4. **TOPHAL-01** — `static_assert(NUM_TOF==4)`.
5. **CC-04** — `comm_*_tick` → void.
6. **TOF-1** — 400k→100k en path no-MULTI.
7. **TOF-3** — frescura HC-SR04 (SOLO si el sitio de seteo tiene timestamp facil;
   si no, dejar pending).

**Codigo nuevo aislado (riesgo de regresion nulo):**
8. **DIAG-CAM-ACCEPTANCE** — escribir + compilar el sketch (correrlo es banco).
   *Reduce el riesgo del bloqueante #1.*

**Docs (doc-only, behavior-neutral):**
9. **B.1** — FUENTES-DE-VERDAD filas v3/31B y deuda #3 resuelta; CONTRATO-CENTRAL
   v2; CONTRATO-CAMARAS aviso v1-historico.
10. **B.2/B.3** — resto de correcciones de tamano/Serial/banner + README cross-link.
11. **AUDIT-1** — marcar quick-wins ya aplicados en el informe base.

**Comentarios (C.1, ~38 wins, gate intacto):**
12. Aplicar en lote los resumenes en lenguaje llano (R1-R9, CMT-01..10,
    DOWN-01..10, RD-01..10). El de mas valor: **DOWN-01** (hacer explicita la
    doble cadena de linea).

### Dejar / NO aplicar ahora

- **Todo lo de BANCO** (IMU-1, CC-02/CA-03, SYS-1, TASK-022, CAM-02, VIS-01,
  SB-4, BANK-TUNE) — requiere hardware/medicion. Asumir COAST en el freno hasta
  medir; dar margen de borde.
- **R10** (rename en strategy.cpp) y **CS-3** (static oculto en PATROL) — tocan el
  cerebro; regla dura, post-Incheon con gate verde.
- **POST-INCHEON-DEBT** — alto riesgo / nulo beneficio competitivo a ~26 dias.
- **DELIVERABLES** (nombre, costos, fotos, video, roster) — dato humano.
- **SI-03 return muerto**, borrado de campos POSE-02 — cosmetico, riesgo de churn
  en modulos testeados; diferir.

### Honestidad sobre el estado

El gate host esta verde (547/40/0, re-verificado hoy). Los 6 fixes de hoy no
regresan nada. **Pero la competitividad no depende de mas codigo de escritorio:**
los 3 riesgos que pueden costar un partido (DOWN-stale, freno COAST, BNO sesgado)
y el bloqueante #1 (vision) son TODOS de banco. El mejor uso del escritorio es
preparar el terreno: `diag_cam_acceptance` para validar la vision recalibrada,
POSE-01 para que el modulo de pose este correcto cuando se cablee, y las
correcciones de docs para que el equipo no pierda tiempo persiguiendo pistas
stale en el pit.
