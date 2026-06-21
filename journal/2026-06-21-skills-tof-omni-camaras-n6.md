---
title: "Tres skills expertas nuevas — ToF VL53L7CX, cinemática omni-3, cámaras OpenMV N6"
date: 2026-06-21
author: "Claude Opus 4.8 (1M context)"
requested-by: "Gustavo Viollaz (@gviollaz)"
tipo: skill-authoring
toca-firmware: NO (solo .claude/skills/ + docs/)
---

# Sesión 2026-06-21 — Tres conjuntos de skills expertas

## Qué se pidió

Crear TRES skills expertas en `.claude/skills/`, usando la skill `bno055-imu-heading-robocup` como
**plantilla de oro** (mismo formato: frontmatter con triggers, "Principio central" en blockquote,
tablas, árbol de diagnóstico, "Errores comunes", "Skills relacionadas", `references/` para tablas
pesadas). Método: por cada skill, un **workflow de investigación paralela** = agente de
best-practices web (WebSearch/WebFetch) + agente de lectura de código real anclado a `archivo:línea`
+ síntesis a outline. Regla dura: **TODO verificado contra el CÓDIGO REAL**, honestidad sobre
trampas de diagnóstico, **marcar inconsistencias en vez de homogeneizarlas**.

## Qué se entregó (solo markdown — NO se tocó firmware)

1. **`tof-vl53l7cx-lectura-calibracion-posicionamiento/`** (SKILL.md + 3 references) — el sensor ToF
   multizona: resolución 4x4/8x8, modo continuo vs autónomo, filtro por `target_status`, recorte de
   payload (`VL53L7CX_DISABLE_*`), coexistencia BNO+ToF (el band-aid 100 kHz), carga del firmware
   ~84 KB, xtalk/offset, y de la matriz a la pose por paredes. Ancla a `src/top/sensors_tof.cpp`,
   `src/shared/localization.{h,cpp}`, `tof_zone_mask{,_orient}.h`. COMPLEMENTA `localizacion-rcj-soccer`
   y `fusion-pose-odometria-landmarks`.
2. **`cinematica-omni-3-120/`** (SKILL.md + 2 references) — la TEORÍA del omni-3: la fórmula
   `v_i=-vx·sinθ+vy·cosθ+ω·R`, la matriz con `{330,210,90}`, el `+180` que corrige traslación, la
   trampa `OMEGA_SIGN` (el +180 NO toca la rotación → realimentación positiva del lazo de rumbo),
   saturación por escalado. Ancla a `src/shared/kinematics.{h,cpp}`, `src/central/config_central.h`,
   `motors_zircon.cpp`. COMPLEMENTA `dinamica-omni-3-ruedas` (planta MEDIDA) y `control-pid-zona-muerta` (lazo).
3. **`openmv-n6-camara-vision-robocup/`** (SKILL.md + 3 references) — visión por color N6: pipeline
   `find_blobs` LAB en CPU (lo que CORRE, sin NPU), bloqueo de exposición/WB, homografía atada a VGA,
   protocolo v2 11 bytes/CRC8, fusión front+back + rotación 180° de la trasera, árbol de diagnóstico,
   y veredicto ML/NPU. Ancla a `cameras.{h,cpp}`, `cameras_runtime.cpp`, `cameras_fusion.{h,cpp}`,
   `camaras-openmv/main.py`. **REEMPLAZA a `openmv-vision-tuning`** (que describía H7) → banner de
   desactualización agregado a la vieja.

Decisión de diseño (cámaras): **1 skill madre, NO madre + sub-skills.** Justificación de la síntesis
(coincide con el patrón de la plantilla de oro): el pipeline es coherente y secuncial (un mismo
síntoma "no ve la pelota" cruza LAB + exposición + ROI + homografía + UART), partirlo rompe el árbol
unificado que es el corazón del valor. La NPU/ML no se usa hoy → se cubre como UNA sección honesta +
una reference, no como sub-skill (sería documentar capacidad no-cableada como operativa). Si en 2027
el ML entra a producción, ahí conviene escindir `openmv-n6-ml-npu`.

## Hallazgos / correcciones de la investigación (la verdad MANDA = el código)

- **ToF — FoV:** el VL53L7CX es **90° DIAGONAL (60×60)**, NO ~60-65° (eso es el VL53L5CX hermano).
  El supuesto del enunciado era erróneo; la investigación lo corrigió contra el datasheet ST.
- **ToF — `target_status`:** ST cuenta 5 = 100%, 6/9 ≈ 50%, resto < 50%. El robot usa el filtro
  permisivo `5||6||9` (`sensors_tof.cpp:208-235`); para trilateración fina conviene endurecer a {5}
  (tema-a-analizar, banco).
- **Omni — convención:** la fórmula del robot (`-vx·sinθ+vy·cosθ+ω·R`) es la convención **FIRGELLI
  (ω positivo)**, NO la de ros2_control (ω negativo). Que el robot necesite `OMEGA_SIGN=-1` es
  justamente el síntoma de que su convención no coincide con el sentido físico real de sus motores
  → se reconcilia en UN lugar (el mixer), no mezclando convenciones. Matriz verificada numéricamente
  (strafe → ruedas `[+100,+100,−200]`, firma 1:1:−2; `det≠0`).
- **Cámaras — H7 vs N6:** el hardware real es **N6** (STM32N657, sensor PAG7936 global shutter,
  NPU Neural-ART ~600 GOPS INT8 — no "GFLOPS"). La skill `openmv-vision-tuning` decía "H7", pelota
  IR pasiva y arcos cyan/magenta — todo desactualizado; el código de producción detecta naranja por
  color LAB y arcos amarillo/azul.

## Inconsistencias doc-vs-código marcadas (NO homogeneizadas), verificadas contra ambos lados

- `docs/firmware/CONTRATO-DATOS-TOP.md:457,493,523,553` describe los 4 ToF como **"stub"** /
  `min_obstacle=0xFFFF` siempre → el código usa el driver Adafruit real desde 2026-05-24.
- `docs/CONVENCION-EJES-ROBOT.md:153,158` dice grilla **"8×8"** y corrección `(7-fila,7-col)` → el
  código usa **4x4=16** (`TOF_RESOLUTION_ZONES=16`) → corrección `(3-fila,3-col)`, GW=4.
- Cámaras: **3 sets de thresholds LAB divergentes** (`main.py:47-49` vs `CALIBRACION-VISION-N6.md:17-24`
  vs `main-comunicacion-vieja.py`) y altura de cámara **`h=95.0` (`main.py:44`) vs `18.7` (docs)** —
  load-bearing de la homografía, sin reconciliar. **Cierre = banco**, no desde el repo.
- `cameras.h:5` referencia los scripts `cam-*-n6.py` (DEPRECADOS); producción = `camaras-openmv/main.py`.

Estas inconsistencias quedan REGISTRADAS en las skills (sección "Inconsistencias a NO homogeneizar")
y no se "arreglaron" en silencio — cerrarlas (sobre todo las de banco) es del equipo humano.

## Verificación

- Cada `archivo:línea` load-bearing fue leído por mí en esta sesión (sensors_tof.cpp, localization.*,
  kinematics.*, config_central.h, motors_zircon.cpp, cameras.{h,cpp}, cameras_runtime.cpp,
  cameras_fusion.h, main.py) + corroborado por el agente de código del workflow. La matriz omni se
  verificó numéricamente con Python. Anchors secundarios (keeper_xy_walls, tof_schedule, payload
  `platformio.ini:620-622`) verificados con grep.
- **NO se compiló nada porque NO se tocó firmware** — los binarios de competencia son byte-idénticos
  (trivial: el cambio es 100% markdown en `.claude/skills/` + `docs/`).
- Docs canónicos actualizados en el MISMO commit: `FUENTES-DE-VERDAD.md` (3 filas nuevas +
  `openmv-vision-tuning` marcada superada) + `ESTADO-ACTUAL.md` (banner).

## Pendiente (equipo / banco — Claude NO cierra TASKs de hardware)

- Reconciliar en banco los 3 sets de thresholds LAB y `h=95` vs `18.7` (cámaras).
- Calibrar `CAMERA_UNIT_TO_MM` (placeholder 10.0) con pelota a distancias conocidas (TASK-022).
- Endurecer (o no) el filtro de `target_status` a {5} para trilateración fina (ToF).
- Confirmar el `+180` de `WHEEL_ANGLES_DEG` con `diag_central_strafe` (si traslada al revés → sacarlo).
- Propagar HI-6 a los docs que aún citan `cam-*-n6.py`. Decidir si reescribir/deprecar formalmente
  `openmv-vision-tuning` (hoy queda con banner).
