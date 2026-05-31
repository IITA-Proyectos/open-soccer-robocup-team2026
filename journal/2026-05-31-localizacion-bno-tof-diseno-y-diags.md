---
title: "Diseño de localización (BNO+TOF → pose x,y,θ) + 2 diags de prueba"
date: 2026-05-31
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8, Anthropic) — workflow multi-agente"
status: final
tags: [top-board, localizacion, bno055, tof, vl53l7cx, pose, fusion, diags]
robot: robot1
area: firmware
tipo: diseño + herramientas de prueba
---

# Diseño de localización (BNO+TOF → pose x,y,θ) + diags de prueba

> **TL;DR.** Mientras el equipo suelda el 2º BNO, corrí un análisis multi-agente
> de cómo usar los 2 giroscopios + los 4 TOF para sacar pose (x,y,θ) del robot
> en la cancha, y escribí 2 programas de prueba listos para banco:
> **`diag_bno_dual_live`** (valida 2 BNO + fusión de heading, degrada a 1) y
> **`diag_pose_live`** (pose x,y,θ en vivo llamando al algoritmo REAL del repo).
> Ambos compilan limpio y offline. El diseño es de 3 capas; la Capa 1 (XY con 4
> TOF) **ya existe en el código** y solo hay que validarla + endurecerla — no
> rediseñar. No se tocó firmware de competencia todavía: esto es análisis +
> herramientas de validación.

## Cómo se hizo

Gustavo pidió, mientras soldaba la otra placa, preparar en paralelo el uso de
los 2 BNO + los TOF para localización, con análisis completo y programas de
prueba. Lancé un **workflow de 4 agentes en paralelo + síntesis**:
1. Auditoría del gap de pose actual.
2. Diseño de fusión dual-BNO.
3. Algoritmo BNO+TOF (incl. idea de usar el gradiente de zonas 8×8 para
   estimar ángulo de pared y corregir drift).
4. Diseño de los programas de prueba.

## Hechos de hardware que anclan el diseño (banco 2026-05-30/31)

- **4 TOF andando**, enumerados en bus único (LP {9,10,11,12}, dir 0x2A..0x2D).
  Mapeo: TOF0=frente(+Y,0°), TOF1=atrás(-Y,180°), TOF2=derecha(+X,270°),
  TOF3=izquierda(-X,90°). El izquierdo es de otro fabricante, rotado 180°
  (corrección de zonas ya aplicada).
- **1 solo BNO conectado** (LEFT, Wire 0x28). El 2º (RIGHT, Wire1 24/25) se está
  soldando ahora.
- Heading del BNO: chip CW-positivo, **el firmware ya invierte a CCW** con
  `HEADING_SIGN=-1` (sensors_imu.cpp). heading 0 = mira arco rival (+Y).

> **Corrección importante respecto a docs viejos:** `sensors_tof.cpp` (módulo
> vivo) y varios comentarios todavía dicen "solo el TOF frontal está
> instalado, los otros son stub". Eso quedó **OBSOLETO** — los 4 andan. Por eso
> la pose XY ya es alcanzable de forma robusta (2 TOF por eje). Pendiente:
> actualizar `sensors_tof.cpp` para enumerar los 4 (hoy solo inicializa el
> frontal a 0x29, 4×4). Ver "Pendiente" abajo.

## El diseño objetivo — 3 capas (de menor a mayor ambición)

### Capa 1 — Pose XY con los 4 TOF (YA existe, validar + endurecer)
`localization_compute()` (shared/localization.cpp) hace trilateración
geométrica directa y es **geométricamente correcta**. Da pose válida si hay ≥1
estimación en eje X **y** ≥1 en eje Y. Mejoras de bajo riesgo (host-testeables,
sin soldar nada), para más adelante:
- **Outlier rejection en el primer ciclo:** hoy solo actúa con `prev_valid`
  (localization.cpp:93, runtime:46). Con 2 TOF/eje conviene rechazar
  inconsistencias desde el ciclo 0.
- **Confidence graduado en vez de valid binario:** si un eje queda sin
  estimación (esquina, robot alineado a una pared), hoy cae a (0,0)/invalid y el
  CENTRAL lo lee como "robot en su esquina". Mejor: mandar "XY no disponible" con
  confidence baja.
- **Suavizado:** mediana móvil de 5–10 frames sobre (x,y) en el runtime. Quita
  el jitter de ±2–3 cm de los TOF (no corrige drift, solo ruido).

### Capa 2 — Heading (θ) + corrección de drift
Hoy el heading sale siempre del BNO LEFT crudo (offset al boot). En IMUPLUS
(sin magnetómetro) tiene **drift de 1–5°/min**. El desacople heading-vs-XY
actual es correcto y se mantiene. Mejora scope-Incheon (NO requiere 2º BNO ni
visión): usar las **paredes** como referencia absoluta de orientación para
frenar el drift — cuando un TOF mira una pared cuasi-perpendicular, el ángulo de
mínima distancia entre sus zonas da un ángulo absoluto; fusionar lento
(α≈0.02–0.05) solo cuando la lectura es confiable. **Viabilidad acotada:** a >1 m
la resolución 8×8 (~7.5°/zona) y el ruido del sensor hacen ruidoso el mínimo →
promediar 3–5 frames y aplicar solo a pared cercana/perpendicular. Si no llega a
tiempo, recalibrar entre partidos (power-cycle apuntando al arco) alcanza.

### Capa 3 — Fusión dual-BNO (cuando se suelde el 2º)
Aporta **redundancia + detección de impactos**, no precisión de heading per se.
Es opt-in y NO cambia la API actual:
- Helper `normalize_heading_diff` (resta circular en [-180,180]) — imprescindible
  para no romper en ±180.
- Promedio **circular** (atan2 de sumas de sin/cos), no aritmético.
- Restar el offset de cada BNO **antes** de promediar.
- Máquina de estados: BOTH_OK / LEFT_ONLY / RIGHT_ONLY / BOTH_DISAGREED /
  BOTH_FAIL, con histéresis (varianza del desacuerdo, no un solo ciclo).
- Detección de impacto: si el desacuerdo salta >15° en 1 ciclo. En un giro real
  ambos suben juntos; en impacto, uno salta.
- **Degradación:** con 1 BNO todo cae a LEFT_ONLY sin cambios de comportamiento.

> **OJO antes de implementar la fusión:** `sensors_imu.cpp` ya tiene
> `g_left`/`g_right`, `sensors_imu_get_disagreement_deg()` y fallback LEFT→RIGHT,
> pero **NO** tiene fusión circular ni detección de impacto. Reusar lo que hay,
> no duplicar.

### Qué NO hacer pre-Incheon (consenso)
Kalman 2D cinemático, LUT de firmas de 64 zonas, recalibración por visión,
gradiente sub-zona fino. Correctos pero caros (4–7 días c/u) y no bloquean
competir. → backlog 2027.

## Programas de prueba escritos (compilan limpio, offline)

### 1. `diag_bno_dual_live` (`src/diag/diag_bno_dual_live.cpp`)
Valida los 2 BNO + fusión de heading **sin** localización. Es el **primer test a
correr cuando se suelde el 2º BNO**. Muestra a 10 Hz: heading + gyroZ + calib de
cada sensor, heading FUSIONADO (promedio circular), desacuerdo, y flag de
impacto. Comandos: `z` (recapturar cero), `d` (drift 20 s). Degrada a 1 BNO
mostrando "1 BNO: degradado".
```
pio run -e diag_bno_dual_live -t upload ; pio device monitor -b 115200
```
**Criterio:** ambos ready; desacuerdo <5° en reposo; al girar a la IZQUIERDA el
heading SUBE en ambos; el flag impact salta solo ante un golpe, no ante giro.

### 2. `diag_pose_live` (`src/diag/diag_pose_live.cpp`)
Pose (x,y,θ) en vivo: enumera los 4 TOF (patrón de `diag_top_tof_quad_live`),
lee el BNO, y llama al **algoritmo REAL** `localization_compute()` de
`shared/localization.cpp` (compilado dentro del diag — testea el código que va a
competir, no una copia). Imprime las 4 distancias + pose + qué TOF aportó.
```
pio run -e diag_pose_live -t upload   # POWER-CYCLE despues de flashear
# apuntar robot a +Y, mandar 'z', luego:
pio device monitor -b 115200
```
**Criterio (test de aceptación Capa 1):** robot en posiciones medidas con cinta
(esquinas + centro 1215,910), pose dentro de ±50 mm en x,y y ±5° en heading. En
esquina con robot a 45° la pose debe reportarse INVÁLIDA (no (0,0) válido).

## Orden recomendado (priorizado)

1. **Validar lo que ya existe** antes de tocar: correr `diag_pose_live` en 5
   posiciones. Si la Capa 1 cumple ±50 mm/±5°, NO rediseñar.
2. **Endurecer Capa 1** en código puro host-testeable (outlier 1er ciclo,
   confidence graduado, mediana móvil). Cubrir con test host.
3. **Cuantificar el drift real** del BNO (correr `diag_bno_dual_live` con `d`, o
   un diag de drift con pared). El número decide si la Capa 2 es necesaria.
4. **Si el drift duele:** implementar corrección-por-pared (α chico + gating).
5. **Al soldar el 2º BNO:** correr `diag_bno_dual_live`, luego implementar la
   fusión sobre `sensors_imu.cpp` (reusar lo que hay).
6. **Integrar a CENTRAL** solo cuando los diags pasen (campos paralelos en
   WorldSnapshot; no cambiar el heading/offset existente).

## Preguntas abiertas (para el equipo / próxima sesión)

- ¿Los 4 TOF leen 8×8 dentro del presupuesto de 30 ms del loop, o algunos caen
  a 4×4 por latencia? Afecta la viabilidad de la corrección-por-pared.
- Orientación de zonas de los otros 3 TOF (solo el izquierdo está verificado).
  Para promedio (Capa 1) no importa; para ángulo-por-zona (Capa 2) sí → correr
  `diag_top_tof_zonemap` antes de confiar en ángulos por zona.
- ¿`world_model.cpp` (CENTRAL) distingue "pose XY no disponible" de "(0,0)"? El
  confidence graduado requiere coordinarlo.

## Pendiente humano

- Soldar el 2º BNO (en curso) → correr `diag_bno_dual_live`.
- Correr `diag_pose_live` en posiciones conocidas y anotar resultados (journal).
- **Firmware:** actualizar `sensors_tof.cpp` (módulo vivo) para enumerar los 4
  TOF — hoy solo inicializa el frontal. Es host-testeable y es lo que destraba
  que `localization` use los 4 en producción (no solo los diags). Lo hago la
  próxima sesión si confirmás.

## Archivos

- `src/diag/diag_bno_dual_live.cpp` — **nuevo**.
- `src/diag/diag_pose_live.cpp` — **nuevo** (reusa shared/localization.cpp).
- `platformio.ini` — envs `diag_bno_dual_live`, `diag_pose_live` (offline).
- Análisis completo (multi-agente): este journal lo resume; el detalle quedó en
  el resultado del workflow de la sesión.
