---
title: "Skills nuevas: localización / SLAM / odometría / fusión de pose (2 skills + 3 referencias)"
date: 2026-06-14
author: "Claude Opus 4.8 (1M context) (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M, Anthropic)"
status: completado
tags: [skills, localizacion, slam, odometria, pose, fusion, ekf, control, sensores, ambos]
area: control
robot: ambos
tipo: decision
---

# Skills de localización, SLAM, odometría y fusión de pose

## Qué se pidió

Gustavo pidió "los mejores skills" para resolver temas de posicionamiento,
localización, SLAM, odometría, visual odometry, visual SLAM, localización por
landmarks, MCL/filtro de partículas y pose estimation, y subirlos al repo para
que estén disponibles para todos.

## Decisión de coach (lo importante)

**No se creó una skill de "Visual SLAM".** Aterrizando el pedido en el hardware
real (OpenMV N6 + Teensy 4.x, sin GPU/Jetson) y en la cancha RCJ (un **mapa
conocido** que da el reglamento), el veredicto honesto es: el robot **no necesita
SLAM** (no hay entorno desconocido que mapear) ni Visual Odometry de cámara
frontal (el OTOS ya es odometría óptica). Lo que necesita es **localización en
mapa conocido por landmarks + odometría + heading, todo fusionado** — que es
exactamente lo que el robot tiene **a medio construir**.

Las skills enseñan TODAS las técnicas que se nombraron (con su veredicto de
factibilidad para que el equipo aprenda y para defenderlo ante un jurado), pero
empujan al ROI real para Incheon.

## Hallazgo de la exploración (estado real del stack de localización)

Verificado en el firmware vivo (`software/teensy/Soccer 2026/`):

- **VIVO:** trilateración `src/shared/localization.{h,cpp}` (4 ToF + heading BNO →
  `WorldSnapshot.my_x/y_mm`, conf 70/0). Frágil: necesita ≥1 ToF por eje.
- **VIVO:** odometría OTOS (2× óptico, DOWN) → Pose2D/Vel2D a CENTRAL. Deriva.
- **ESCRITO Y TESTEADO HOST, NO CABLEADO:** `pose_fusion` (filtro complementario
  ToF+OTOS, K≈0.10 Q8), `pose_filter` (EMA/mediana + gate de salto), `otos_fusion`
  (dual OTOS, heading circular + slip), `imu_fusion`/`imu_freeze`, `otos_health`.
- **Raíz de los problemas:** el heading del BNO **se congela** por contención I²C
  con los ToF (TASK-207) → la trilateración rota el mapa entero.
- **Diferido a 2027 por diseño:** EKF, LUT multizona 8×8, MCL (no existe en código).

Conclusión: el cuello de botella NO es "falta SLAM", es **(1) heading inestable**
y **(2) la capa de fusión ya existe y no está enchufada**.

## Lo que se creó

Par técnico (mismo patrón que `dinamica-omni-3-ruedas` + `control-pid-zona-muerta`):

- **`.claude/skills/localizacion-rcj-soccer/`** — LENTE: qué técnica y por qué.
  `SKILL.md` + `references/tecnicas-localizacion-explicadas.md` (deep-dive de cada
  técnica con analogía sostenida ciudad/pasos/carteles + tabla de veredictos).
- **`.claude/skills/fusion-pose-odometria-landmarks/`** — CÓMO: construir/cablear/
  tunear el estimador. `SKILL.md` + `references/complementario-ekf-particulas.md`
  (3 filtros + recetas mínimas) + `references/medir-ruido-sensores.md` (protocolo
  de banco para sacar los 3 números sin los que no se puede tunear nada).

Ambas con la disciplina del repo: P0/P1/P2, tema-a-analizar con risk-no-fix/
risk-fix/tiempo, plan de prueba en hardware obligatorio, voz accesible para los
alumnos, cross-refs a las skills vecinas y a `CONVENCION-EJES-ROBOT.md`.

## Índices actualizados (mismo commit)

- `CLAUDE.md` — lista de skills 16 → 18, las 2 nuevas bajo "Técnica específica".
- `docs/FUENTES-DE-VERDAD.md` — fila nueva "Localización / pose": las skills son la
  LENTE/metodología; la IMPLEMENTACIÓN canónica sigue siendo `localization.cpp` +
  el spec + `CONVENCION-EJES-ROBOT.md` (las skills apuntan, no reemplazan).

## Verificación

- Skills nuevas: solo docs (`.md`), no tocan firmware → no hay gate de compilación
  que correr. No se modificó código del robot.
- Coherencia: los módulos y constantes citados en las skills se verificaron contra
  el firmware vivo en esta sesión (no contra los packs snapshot, que son del 24-05).

## Pendiente (equipo — son TASKs de hardware, Claude no las cierra)

1. **P1 — estabilizar el heading** (BNO a bus propio Wire2 / heading del OTOS de
   respaldo). Es la raíz. Sin esto la pose absoluta es inusable.
2. **P1 — correr el protocolo de medición de ruido** (`medir-ruido-sensores.md`):
   deriva OTOS, σ del ToF, deriva/congelamiento del heading. ~2 h de banco.
3. **P2 — cablear `pose_fusion`** detrás de un flag default-OFF y validar en banco.
4. Medir el `TOF_OFFSET_MM` real (hoy placeholder 95 mm en `pinout_common.h`).

## Atribución

Author: Claude Opus 4.8 (1M context) (Anthropic)
Requested-by: Gustavo Viollaz (@gviollaz)
