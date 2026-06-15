# IDE de monitoreo de la placa TOP — diseño de vistas + actuación sobre ToF

> **Estado:** análisis (in-progress). Pedido por María (banco, 2026-06-15). Producido con
> workflow multi-agente (3 lectores anclados en el código real + diseño + chequeo adversarial),
> lente de ingeniero SLAM/tiempo-real + coach RCJ. **Nada de esto está implementado.**
> Toda cita `archivo:línea` es contra `soccer-main` (main).

## 0. Encuadre por capas

El IDE se organiza por capas de abstracción del robot:
**(1) sensor crudo** (zonas ToF, cámaras, HC-SR04) → **(2) objetos** (pelota/arcos + velocidad)
→ **(3) mundo** (cancha XY, mapa de ocupación) → **(4) salud + actuación** (vetar zonas,
ubicar/rotar/invertir ToF). Las 4 vistas que pidió María + 3 que agrego se mapean a estas capas.

**Regla de oro del diseño:** toda vista nueva **hereda** el pipeline existente del `monitor-base`
(`FrameSource` hilo+cola → parser inyectable `parse_line_top` → `GUI._tick/poll` → `_render`,
`sources.py:101-146`) y **reusa** las piezas puras ya host-testeadas. NO abre serial ni parsea
JSON por su cuenta. **No se rehace nada: se extiende.**

## 1. Las 7 vistas

| # | Vista | Capa | Factibilidad HOY | Prioridad |
|---|---|---|---|---|
| V5 | **Timeline del snapshot a CENTRAL** (caja-negra TOP en vivo) | salud/RT | ✅ 0 firmware | **P1 — mejor ROI** |
| V6 | **Salud de fusión de cámara** (front vs back, anti-pelota-fantasma) | salud/objetos | ✅ 0 firmware | **P1 — bug real** |
| V1 | **Polar robot-céntrica** (conos ToF/cámara/US + objetos + velocidad) | sensor+objetos | ✅ parcial | P1 |
| V3 | **Tira 360 de los 4 ToF + cámaras** | sensor crudo | ✅ con caveat | P1 |
| V2 | **Cancha X-Y + estela** | mundo | ⚠️ partida en 2 | P1 estela / bloqueada objetos |
| V4 | **Mapa tipo-SLAM por rayos** | mundo | 🔶 degradado ya / rico = firmware | P2 / 2027 |
| V7 | **Panel de actuación ToF** (req 6) | salud/actuación | 🔶 visual ya / real = firmware | P1 visual / P2 firmware |

### V5 — Timeline / caja-negra del snapshot (AGREGADO, P1)
Tira temporal (~10 s) de lo que el TOP **manda a CENTRAL**: `snap.valid`, `ball_vis`,
`heading_valid`, `opp/own arco vis`, `min_obstacle`, `referee_cmd`, `flags`. Permite ver
flapping de visibilidad, caídas de `heading_valid`, y correlacionar "el robot hizo X" con "qué
veía". Equivalente TOP de la caja-negra CENTRAL. **Todo viaja hoy** (`telemetry_top.cpp:145-160`);
la app acumula el histórico (deque). Único gap menor: `loop_hz` no viaja, se infiere de
`Δt_ms/Δseq`. **Factible-ya, costo bajo, 0 firmware.**

### V6 — Salud de fusión de cámara (AGREGADO, P1)
Comparador lado-a-lado `camf` vs `camb` vs `cam` (fusión): pelota y arcos de cada cámara + el
resultado fusionado, **resaltando en rojo cuando el delta front↔back es grande** (la pelota
fantasma del promedio que motivó `top_robot2_pri_sticky`). Tablero directo para validar la
cámara pegajosa. **Todo viaja en v2** (`telemetry_top.cpp:87-106`). **Factible-ya.**

### V1 — Polar robot-céntrica (PEDIDA 1, P1)
Radar robot-céntrico: conos de los 4 ToF (sectores en su bearing de montaje {0,180,270,90},
apertura ~60° **hardcodeada**), 2 cámaras como sectores wide, HC-SR04 como sector frontal
angosto; pelota/arcos sobre el radar; **vector de velocidad de la PELOTA** (`bvx/bvy`).
Reusa `gui_top._to_px/_polar_px`. **Limitaciones honestas:** la **velocidad de los ARCOS no
existe** en telemetría (solo `ang/dist/vis`); el **ancho de cono exacto** depende de `TOF_FOV_DEG`
que hoy **no viaja** (R1 no lo expone) → se dibuja ~60° como aproximación declarada.

### V3 — Tira 360 de los 4 ToF + cámaras arriba (PEDIDA 3, P1)
4 grillas de zonas 4×4 lado a lado en orden físico: **izquierdo | FRONTAL(centro) | derecho |
trasero**; heatmap por zona. Arriba del frontal: cámara frontal con ID pelota/arcos; arriba del
trasero: cámara trasera. Reusa `zones.ZoneGrid` + `_render_zones`. **CAVEATS (ver §2.2):** el ToF
izquierdo viaja **rotado 180°** y la telemetría manda zonas **crudas** → hay que portar
`tof_zone_orient.h` a la app; y el mapeo **zona→azimut (~90°) está sin escribir** → la tira
alinea los 4 sensores **entre sí pero NO al frente del robot** (avisarlo en la UI).

### V2 — Cancha X-Y + estela (PEDIDA 2) — PARTIDA EN DOS
- **V2a — Estela del OTOS (P1, honesta HOY):** robot dibujado como triángulo con estela de poses
  previas (deque, auto-encuadre), **usando SOLO `base.px/py/phdg` del OTOS** (odometría DOWN
  confiable). Reusa el patrón de estela del arquero (`gui_gk.py:253-298`).
- **V2b — Objetos en cancha (BLOQUEADA hasta TASK-022):** dibujar pelota/arcos en coordenadas
  absolutas **NO** hasta calibrar la homografía. Ver §2.1.

### V4 — Mapa tipo-SLAM por rayos (PEDIDA 4) — ver §4
### V7 — Panel de actuación ToF (AGREGADO, soporta req 6) — ver §3

## 2. Las correcciones honestas (del chequeo adversarial)

### 2.1 🔴 La cancha X-Y absoluta dibuja BASURA si se usa tal cual
La pose `snap.x/y` del robot y la proyección de objetos a absoluto dependen de la **homografía
sin calibrar**: `UNIT_TO_MM = 10.0f` es un **placeholder plano** (multiplica coords de cámara ×10
→ es *píxel×10*, NO distancia métrica; `cameras_runtime.cpp`, TASK-022). Un objeto dibujado "en
la cancha" en una posición **plausible-pero-falsa** es **peor que no dibujarlo**, porque invita a
tunear sobre un dato mentiroso. **Regla dura:** para la pose del robot en el display usar **SOLO
OTOS** (`base.px/py`), **nunca** `snap.x/y`, mientras `UNIT_TO_MM` sea placeholder. `snap.x/y` y
`base.px/py` **NO** son intercambiables.

### 2.2 🟠 La tira 360 alinea sensores entre sí, pero no al robot
Dos hechos: (a) el ToF izquierdo (idx 3) tiene las zonas **rotadas 180°** (otro fabricante,
montado mirando abajo, `tof_zone_orient.h:7-14`) y la corrección `tof_raw_zone_for_canonical`
**solo se aplica en el diag, no en la telemetría viva** → las zonas viajan **crudas** → hay que
replicar la corrección **en la app** (port de `tof_zone_orient.h`, ~20 líneas). (b) El mapeo
**zona→azimut (~90° canónico) está SIN escribir** (`tof_zone_orient.h:18-20`). Consecuencia: la
tira corrige el 180° del izquierdo y alinea los 4 sensores **entre sí**, pero **todos quedan
rotados ~90° vs el frente del robot** hasta escribir ese mapeo. **La UI debe avisar:** "arriba en
la grilla ≠ adelante del robot" (hoy no es cierto). Si no, confunde justo en lo que pretende
aclarar.

### 2.3 🟠 La actuación ToF "en vivo" es un síntoma mal ubicado
Rotar/invertir un ToF es una **constante de montaje** (no cambia entre partidos) → botones de
toggle = "resolver en runtime algo fijo". Peor: `TOF POS` **ya no se aplica en vivo** (efecto
pleno tras `CFG SAVE` + reinicio, `top_telemetry_serial.cpp:295-298`) → una UI "en vivo" para eso
**miente**. **La movida correcta:** la corrección de orientación (180° + ~90°) se hace **una vez,
visual en la app** (espejo de `tof_zone_orient.h`) para *interpretar bien las lecturas* — sin
sliders. El único control en vivo con sentido: **vetar un sensor entero** (`TOF n OFF`, ya existe)
y, si hace falta, **vetar una zona muerta puntual**.

### 2.4 🟡 El cuello de botella no es el baudio, es el ToF
El enlace de cámara a 19200 es **upstream** (no toca el panel). El panel lee USB a 115200 (frame
JSON ~1350 B a 20 Hz ≈ 27 KB/s, trivial). El límite real: el **round-robin de los ToF refresca
cada sensor a ~8 Hz** (`sensors_tof.cpp:370-372`), pero el JSON emite a 20 Hz → la misma zona
**repetida 2-3×**. Para el SLAM (V4) eso es un **bug**: acumular log-odds sobre lecturas repetidas
**falsea** "la pared se consolidó". **Fix:** deduplicar por lectura nueva (≥120 ms), y renderizar
Tkinter a **10-15 Hz**, no 20 (la grilla 4×4×4 + conos + estela en Canvas a 20 Hz lagea).

## 3. Actuación sobre los ToF (requisito 6)

**Ya existe** (parser verificado, `telemetry_top.h:174-194`):
- `TOF <n> ON/OFF` — habilita/deshabilita el **sensor entero**.
- `TOF <n> POS FRONT|RIGHT|BACK|LEFT` — setea el bearing de montaje (aplica **en boot**, no en vivo).
- `CFG SAVE/LOAD/RESET` — persiste a EEPROM.

**Falta firmware** (los campos existen en `top_config.h:46-49` pero **sin comando ni apply-point**, son no-op A2.2):
- `TOF <n> ZONE <z> ON|OFF` — vetar zona individual (campo `zone_mask` uint64).
- `TOF <n> ROT 0|90|180|270` — rotar grilla (campo `zone_rotation_deg`).
- `TOF <n> FLIP X|Y|OFF` — invertir arriba↔abajo / izq↔der (campo `flip`).

**Dónde vive la transformación (decisión):**
- **Corto plazo / banco:** veto+rotación+flip **solo-visual en la app** (Python, espejo de
  `tof_zone_orient.h`). Cero riesgo de firmware; sirve YA para *interpretar las lecturas* (el
  objetivo declarado del req 6). El operador ve la grilla bien orientada sin tocar el binario.
- **Mediano plazo:** llevar la transformación al **firmware** (apply-points) **solo si** las zonas
  corregidas tienen que **alimentar lógica del robot** (evasión/SLAM), no solo el display. Ahí:
  comandos nuevos + persistencia (ya lista) + exponer la config en el JSON (hoy solo en la línea
  CFG modo-humano) — con `host-tests` del reordenamiento + banco con objeto a distancia conocida.
- **REGLA:** la matemática vive en **UNA** fuente de verdad (`tof_zone_orient.h`); la app la
  espeja, el firmware la aplica. **Si difieren, el banco miente** → test de paridad obligatorio
  (objeto a 20 cm de un sensor, resultado esperado hardcodeado, corre contra el .h y el port).

## 4. Mapa tipo-SLAM por rayos (V4)

**Pipeline:** por cada ToF *i*, por cada zona *z* válida (`z[i][z] != 65535`): rayo desde el robot
en azimut `A(i,z)` a distancia `z[i][z]` mm → marca celda OCUPADA en una grilla de ocupación
(5 cm/celda, robot-céntrica 2,5 m = 100×100). Acumular con **log-odds** (hit +Δ, celdas del camino
−Δ por ray-cast Bresenham) para consolidar estructura estable y desvanecer ruido.

`A(i,z) = bearing_montaje(i) + offset_zona(z)`. `bearing_montaje` existe ({0,180,270,90}).
**`offset_zona(z)` es el hueco:** ni el **FOV** (`TOF_FOV_DEG`, ausente en R1) ni el mapeo
**columna→ángulo** existen (`tof_zone_orient.h:18-20`).

- **V4-DEGRADADO (factible-ya, 0 firmware):** 1 rayo por sensor al bearing de montaje, distancia =
  min/promedio de su frente → **4 rayos = mapa grueso de 4 sectores**. Útil para "algo cerca a la
  derecha/atrás", NO para perfilar paredes.
- **V4-RICO (necesita 3 piezas de firmware):** 16 rayos/sensor (64 total) con FOV + columna→ángulo
  + corrección de orientación → recién ahí una pared aparece como segmento recto.

**Pose:** usar **OTOS** (`base.px/py/phdg`, deriva acotada), **NO** `snap.x/y` (homografía). Sin
loop-closure, el mapa es **memoria corta** (últimos segundos), no SLAM global. Honesto:
**occupancy-grid local con odometría = "lidar pobre"**, no SLAM. Para Incheon alcanza para evasión.

**Heurística oponente-vs-pared:** segmento largo/recto y quieto = **pared** (gris); cluster compacto
(≤25 cm), separado del borde, que **se desplaza** entre frames = candidato **oponente** (rojo); la
pelota se descarta del mapa cruzándola con la cámara. Sin pose absoluta confiable, degrada a
"segmento largo recto = estructura".

## 5. Reuse vs build

**Se EXTIENDE (reusar tal cual):** el pipeline `FrameSource`→parser→`_tick/poll`→`_render`;
`gui_top._to_px/_polar_px` (V1/V4); `zones.ZoneGrid`+`zone_color`+`_render_zones` (V3/V7);
`gui_gk.trail`+`_render_trail` (estela V2a/V3/V5); `geometry.SENSOR_POS` (anillos); `health.evaluate`
(semáforos); `self.source.send` (actuación); el patrón `flag+branch` en `__main__` (una vista =
un flag); `pack_propagate(False)` (anti-parpadeo, lección TASK-209).

**Se CONSTRUYE NUEVO (app, sin firmware):** port Python de `tof_zone_orient.h`; proyección
relativo→absoluto (V2b, gateada); acumulador occupancy-grid + ray-cast (V4); buffers históricos de
objetos (V3/V5); comparador `camf/camb/cam` (V6).

**Se CONSTRUYE NUEVO (firmware, solo si actuación real / SLAM rico):** comandos `TOF ZONE/ROT/FLIP`
+ apply-points; exponer config en JSON (schema v3); FOV + azimut-por-zona; velocidad de arcos.

## 6. Plan por fases + prioridad

- **FASE 0 (Incheon, ~1-1,5 días, 0 firmware): V5 + V6.** Atacan bugs reales que ya tenés
  (flapping de heading, pelota fantasma). Cero riesgo sobre el binario de competencia.
- **FASE 1 (post-Incheon o post-TASK-022, 0 firmware): V3** (tira 360, orientación display-only),
  **V1** (polar), **V2a** (estela OTOS), **V7-visual**.
- **FASE 2 (post-TASK-022): V2b** (objetos en cancha confiables), **V4-degradado** (4 rayos).
- **FASE 3 (2027): V4-rico** + comandos `TOF ZONE/ROT/FLIP` + apply-points + FOV/azimut-por-zona +
  velocidad de arcos → el **lidar-360 / SLAM** del Mundial. No es Incheon.

**Prioridad honesta: P1 solo para V5+V6. ⚠️ Ningún ítem de este IDE debe desplazar a TASK-022
(calibrar cámaras) en la cola de Incheon** — es el bloqueante #1 real; este panel es soporte, no
sustituto. En un equipo de 2 personas a 2 semanas de Incheon, 3 días de IDE **es** desplazamiento.

**`risk-no-fix`:** seguir debuggeando el TOP a ciegas (línea CFG + JSON crudo), lento y propenso a
malinterpretar zonas mal orientadas (el izquierdo a 180° engaña a ojo). Fricción de desarrollo, no
desclasificante.
**`risk-fix`:** bajo en Fase 0/1 (solo app Python, binario de competencia intacto). Medio en Fase 3
(tocar `sensors_tof.cpp` apply-points puede degradar el `min_obstacle` real → defaults no-op
byte-idénticos + host-tests + banco antes de confiar).

## 7. Preguntas abiertas (a resolver en banco)

1. **FOV real por sensor en R1** (`robot2.h:148` asume {60,60,60,40} pero flagueado "ausente/confirmar").
2. **Mapeo columna-de-zona → ángulo** dentro del FOV (sin escribir; ~90° canónico pendiente).
3. **ToF de R2 "rotados ~90°" SIN confirmar** (`robot2.h:131-136`): ¿índice→posición y mount angles
   iguales a R1? Correr `diag_top_tof_census` en R2 con power-cycle.
4. ¿Veto/rot/flip se quiere **solo para display** o también para **alimentar lógica** (define si
   Fase 3 firmware es necesaria)?
5. ¿Exponer la config TOP en el JSON (schema v3) o alcanza con la línea CFG modo-humano?
6. ¿Velocidad de **arcos** derivada en el TOP, o solo velocidad de pelota? (alcance de V1).

---

**Atribución:** análisis producido por Claude Opus 4.8 (workflow multi-agente), pedido de María
Viollaz. Pendiente de decisión del equipo qué fase entra antes de Incheon. **Verdad del código
manda:** las citas se verificaron contra `soccer-main` (main); el chequeo adversarial corrigió las
imprecisas (la fusión de arco vive en `cameras.cpp`/`cameras_runtime.cpp`, no en un
`cameras_fusion.cpp`).
