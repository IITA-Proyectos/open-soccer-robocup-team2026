---
id: TASK-022
title: "Cámara operativa: sentinel, crash bytearray, exposición fija, calibración mm+LAB, 1 script por cámara"
date_created: 2026-05-18
assigned: [mariaviollaz]
priority: P0
status: MATRICES-HOMOGRAFIA-PER-CAMARA-HECHAS-(Elias-2026-06-14)-falta-recalib-LAB-en-sede-Incheon
estimated_hours: 24
blocks: [percepción de pelota/arcos en Incheon]
tags: [vision, camara, openmv, firmware, calibracion]
depends_on: []
---

# TASK-022 — Cámara operativa

## ✅ Actualización 2026-06-14 (Elías) — matrices de homografía PER-CÁMARA hechas

**Reportado por Gustavo (2026-06-15):** Elías **completó el 2026-06-14 la calibración de
las 4 cámaras, cada una con SU matriz de homografía propia.** Esto **cierra el paso 6**
("1 script/H por cámara") y **supera** el estado provisional anterior (la "misma H para
las 4 cámaras" del 2026-06-07, que era una decisión temporal). Ahora cada cámara reporta
su XY con su propia H calibrada.

- **Qué queda de TASK-022 (banco/sede, NO Claude):** la **recalibración de LAB de color
  bajo la luz de Incheon** (la luz del venue invalida los thresholds de Salta) + lock de
  exposición/WB en sede + medir fps@VGA + validar distancia con regla. Es inherente a
  cualquier mundial: los colores se reajustan en cancha.
- **Tooling listo para esa recalibración (2026-06-15):** `solve_homografia.py` ahora acepta
  `--csv` + `--validate` + guardas anti-calibración-mala (ver
  `journal/2026-06-15-task022-tooling-solver-homografia.md`); la H se pega en
  `camaras-openmv/main.py` (producción), no en `cam-*-n6.py` (deprecado).
- **Deuda menor (HI-5, opcional):** copiar las 4 matrices reales de Elías (de los `main.py`)
  al baseline versionado de `robot_config/robot2.h` (hoy tiene placeholders genéricos), para
  que el robot-def refleje el último-bueno. Lo hace quien tenga los valores (Elías/equipo).

> Nota de proceso: Claude **registra** este hito sobre el reporte de Gustavo; la ejecución y
> la validación son de Elías/el equipo (Claude no cierra TASKs de hardware por sí mismo).

## Por qué importa (P0)

La auditoría independiente (`research/completed/2026-05-18-estado-firmware-robot-evaluacion-critica.md`)
encontró que **no hay 2 programas de cámara**: hay **1 script genérico de demo**
(`software/vision/enviar coordenadas 2 arcos y pelota`) con fallas P0:

- **Sentinel roto:** sin pelota, la OpenMV manda `(0,100)`; el parser del TOP
  (`src/top/cameras.cpp:14-16`) lo interpreta como **pelota visible en el
  origen** → el robot persigue una pelota fantasma permanente.
- **Crash:** `bytearray()` con coordenada negativa (homografía) → la cámara
  muere en partido (`enviar...:155`).
- **Auto-WB y auto-gain ENCENDIDOS** (`:31-32`): thresholds LAB se invalidan
  con la luz de Incheon.
- Homografía hardcodeada marcada "(ajustar)"; `CAMERA_UNIT_TO_MM=10.0`
  placeholder; `pixels_threshold=7` (ruido).

## Pasos concretos

1. Definir UN sentinel "no detectado" único y alinear OpenMV ↔ `cameras.cpp`
   (ej. OpenMV manda `Y_coded=0`→`Y=-100`, que es lo que el parser espera).
2. Clampear TODAS las coords a `[0,255]` antes de `bytearray` (anti-crash).
3. `set_auto_whitebal(False)`, `set_auto_gain(False)`,
   `set_auto_exposure(False, exposure_us=…)` con valor medido en cancha.
4. Calibrar thresholds LAB de pelota 2026 + arcos en la cancha de Incheon
   (ver skill `openmv-vision-tuning`); subir `pixels_threshold` a un valor sano.
5. Calibrar `CAMERA_UNIT_TO_MM` midiendo pelota a 30/50/80/100 cm con cada cámara.
6. **2 scripts (o 1 parametrizado) — uno por cámara**: homografía/mirror/FOV
   propios de frontal y trasera (hoy hay 1 genérico).
7. Quitar `print()` del loop; medir fps real.

## Criterio de cierre

- [ ] Cámara tapada → TOP marca `ball_visible=false` (sentinel OK, sin fantasma).
- [ ] Sin crash con coords extremas (test de inyección).
- [ ] Exposición/WB/gain fijos; detección estable bajo luz de Incheon.
- [ ] Distancias en mm verificadas contra regla (error < ~10%).
- [ ] Script por cámara con su homografía; fusión 2 cámaras correcta.

## Plan de prueba en hardware real

1. **Setup:** robot armado, 2 cámaras montadas, cancha + pelota 2026.
2. Tapar cada cámara → verificar `cameras_front/back_alive` y `ball_visible`.
3. Pelota a distancias conocidas → comparar mm reportados vs regla.
4. Variar iluminación (apagar/encender luces) → detección no se cae.
5. Regresión: fusión dual da posición de pelota coherente moviéndola por la cancha.

## Notas / decisiones

_(completar al ejecutar — registrar exposure_us y thresholds calibrados + foto)_

### 2026-06-03 — Avance de banco (reporte de Gustavo). NO cierra la TASK.

Probado hoy con las cámaras montadas:

- ✅ **Detecta pelota + arcos.** Colores calibrados: **azul, amarillo y naranja**
  (arco propio / arco rival / pelota).
- ✅ **Lente ULTRA-WIDE colocado y "anduvo muy bien"** — más campo de visión,
  buena detección.
- ⚠️ **NO verificado todavía:** estabilidad de la detección (no perderla en
  10 min, sin falsos positivos), lock de exposición/WB/gain, y prueba bajo
  iluminación tipo Incheon.

Progreso contra el "Criterio de cierre":

- [ ] Cámara tapada → `ball_visible=false` (sentinel) — **sin verificar hoy**.
- [ ] Sin crash con coords extremas — **sin verificar**.
- [~] Exposición/WB/gain fijos; detección estable — **detección OK, falta lock
      de exposición + prueba de estabilidad y luz Incheon**.
- [ ] Distancias en mm verificadas — **NO** (ver ⚠️ del lente abajo).
- [~] Script por cámara con homografía/FOV — **el ultra-wide cambió el FOV**.

> ### ⚠️ El lente ultra-wide vuelve OBLIGATORIA la recalibración de distancia
> El ultra-wide introduce **distorsión de barril**: el factor `CAMERA_UNIT_TO_MM`
> y el mapeo de ángulos del firmware son de OTRO lente y **ya no valen**. Con el
> ultra-wide, los bordes del FOV mienten más en distancia/ángulo que el centro.
> → El paso 5 (calibrar mm a 30/50/80/100 cm) **se hace con el ultra-wide puesto**,
> y conviene verificar el ángulo del arco en varios puntos del FOV, no solo al
> centro. Esto está trackeado como **P0-CALIB** en
> `research/in-progress/2026-06-03-backlog-mejoras-firmware-top.md`.

**Lo que falta para cerrar TASK-022:** (1) verificar estabilidad + sentinel
(cámara tapada → no fantasma); (2) lock de exposición/WB/gain con valor medido;
(3) calibrar distancia mm con el ultra-wide; (4) idealmente, prueba bajo luz de
Incheon. **La cierra el equipo humano** (Claude no cierra TASKs de hardware).

### 2026-06-07 — ✅ Calibración de distancia HECHA (Elías) + integrada a v2

**Elías calibró en banco la homografía de distancias de la cámara FRONTAL del ROBOT1**
(lente ultra-wide, resolución **VGA**). Entregó `vision-frontal-calibrada.py`.

- ✅ **Homografía calibrada** → `H_MATRIX` real (ver
  `docs/firmware/CALIBRACION-HOMOGRAFIA-XY-N6.md` §Resultado + journal del día).
- ✅ **Integrada a producción v2:** la H se portó a `cam-frontal-n6.py` + `cam-trasera-n6.py`
  (mismo H para las 4 cámaras, decisión provisoria de Gustavo) a framesize **VGA**; el
  artefacto v1 de Elías se subió como prueba (NO se flashea: protocolo viejo). `py_compile` OK.
- ⚠️ **Distancias desde el CENTRO DEL LENTE, no del robot** → falta medir y restar el offset.

Progreso del "Criterio de cierre":
- [x] **Distancias calibradas** (homografía real con el ultra-wide @VGA) — *falta verificar
      contra regla con la H ya portada a v2*.
- [x] **Script por cámara con su homografía PROPIA** — ✅ HECHO por Elías 2026-06-14: las 4
      cámaras calibradas, cada una con SU matriz (ya NO la H compartida provisoria del 06-07).
- [ ] Cámara tapada → `ball_visible=false` (sentinel v2) — verificar tras el deploy.
- [ ] Lock de exposición/WB/gain (la calib se hizo con autos ON) + estabilidad + luz Incheon.

**Lo que queda para cerrar (banco, lo hace el equipo):** (1) **deploy coordinado** —
re-flashear las 2 cámaras (v2 @VGA con la H nueva) **+ el TOP** juntos y ver CRC OK / sin
fantasma; (2) **medir el offset lente→centro** y aplicarlo; (3) **fps a VGA**; (4) lock de
exposición + estabilidad + luz Incheon; (5) distancias vs regla. **Claude no cierra TASKs de
hardware** — el deploy y la validación los confirma el equipo.

## Cambios de estado

- 2026-05-18: creada por Claude tras la evaluación crítica del firmware, a
  pedido de Gustavo Viollaz.
- 2026-06-03: avance de banco — detección de pelota/arcos OK + colores
  (azul/amarillo/naranja) + lente ultra-wide. Sigue `pending`: falta
  estabilidad, lock de exposición y calibración de distancia (con el ultra-wide,
  que vuelve obligatoria la recalibración). Nota agregada por Claude (Opus 4.8)
  a pedido de Gustavo.
- 2026-06-07: ✅ calibración de **DISTANCIA hecha por Elías** (homografía frontal,
  ultra-wide, VGA) → **portada a producción v2** (`cam-*-n6.py`, misma H para las 4
  cámaras, decisión provisoria de Gustavo). Status → `calib-distancia-HECHA-falta-deploy-v2`.
  Queda: deploy coordinado cámaras+TOP, offset lente→centro, lock exposición, validación
  de banco. Integración + docs por Claude (Opus 4.8), a pedido de Gustavo.
- 2026-06-08: ✅ **CALIBRACIÓN CERRADA** (Gustavo confirma en banco: **colores LAB**
  calibrados + **distancia/homografía** de Elías). ⚠️ Importante: el script de PRODUCCIÓN
  que junta TODO lo que anda (colores calibrados + **LED indicador de detección** + **recorte
  inferior** ROI 3% + homografía de Elías @VGA + **comunicación v2 con CRC**) **NO es** el
  `cam-*-n6.py` de los packs (esa reescritura no tenía LED ni colores ni recorte → no detectaba,
  quedó DEPRECADA con banner). **El de producción, que va en las 2 cámaras, es:**
  **`hardware/electronics/camaras-openmv/main.py`** (derivado del `mainopenmvcomvieja.py` que
  anda + el contrato v2; README al lado). **Lo que queda de TASK-022 NO es calibración** → es
  deploy + validación de banco: (1) re-flasheo coordinado 2 cámaras + TOP juntos (CRC OK / sin
  fantasma con cámara tapada), (2) offset lente→centro, (3) lock de exposición/WB/gain, (4)
  distancias vs regla (<10%), (5) fps a VGA + estabilidad bajo luz Incheon. **Esos los cierra el
  equipo en banco** (Claude no cierra TASKs de hardware). Edición por Claude (Opus 4.8) a pedido de Gustavo.
