---
title: "Estado del arte: localización + sensado en cancha (equipos RoboCup 2024/25 + landscape ToF/LIDAR)"
date: 2026-06-23
author: "Claude Opus 4.8 (Anthropic) — workflow paralelo 13 agentes (~1.1M tokens)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
tipo: research-completed
status: investigación COMPLETA · referencias al repo VERIFICADAS por el main loop · web marcada [verif]/[no-verif] · mediciones de banco PENDIENTES (ver Parte 3)
ancla: "falla real — los 4 ToF VL53L7CX no detectan la pared a >1.5m (leen piso)"
---

# Estado del arte: localización + sensado en cancha conocida

> Generado por un workflow paralelo (recon repo → 9 agentes de investigación web → 2 verificadores
> adversariales → síntesis). **Verificación del main loop (2026-06-23):** las referencias internas que
> cita la síntesis EXISTEN en el repo (`journal/2026-06-23-tof-modo-maxrange-paredes-negras.md`, env
> `top_robot2_pri_tofmaxrange` con `-DTOP_TOF_MAXRANGE`, `docs/lidar-tof-slam-analysis.md`, spec pose-XY
> F5). Las afirmaciones web están marcadas [verif]/[no-verif] por los verificadores; la Parte 3 lista lo
> que falta confirmar y medir en banco.
>
> **⚠️ Antes de invertir en hardware, confirmar verbatim contra el PDF oficial 2026:** (1) altura real de
> la pared (¿140 o 220 mm?), (2) legalidad de ToF/LiDAR en Open/Vision (la prohibición sería SOLO de
> Lightweight). Ambas cambian la estrategia.

## PARTE 1 — INFORME EJECUTIVO

### 1.1 Qué usan los MEJORES equipos (localización en cancha conocida)

| Equipo | Liga / 2024-25 | Cómo localiza | Sensores clave | Fuente |
|---|---|---|---|---|
| **chamburr / Raffles** (Singapur) | Open, fue a Worlds | Pared con **LiDAR 1D** + heading, gating por signal strength + consistencia geométrica | 4× LiDAR 1D (clase TF-Luna, por firma I2C), OpenMV H7, IMU 9-DOF | código [verif] |
| **Omicron / BBC** (Australia) | Open, referente | **Líneas blancas** (cámara omni) + optimización vs mapa + odometría → 1.5 cm en <2 ms | cámara omni IMX290, mouse óptico PMW3360 | sitio [verif] |
| **B-Human** (SPL, multicampeón) | Mayor | UKF-por-hipótesis + PF, **solo 12 partículas**; mide líneas/círculo | visión (no ToF) | docs [verif] |
| **LNX Robots** (Eslovaquia) | Open, 2° mundial + Best Poster | Visión frontal + heading, **sin ToF/LiDAR** | Arducam, BNO055, RPi4+Teensy 4.1 | sitio [verif] |
| **RoBorregos** (México) | Open | Visión + IMU: bearing a arcos por color | OpenMV, BNO055 | repo [verif] |

**Dos lecciones load-bearing:**
1. **Los mejores NO confían el "¿dónde estoy?" a ranging activo contra paredes.** Anclan a **visión de
   líneas/arcos + fusión (EKF/MCL con POCAS partículas: B-Human=12, otro=20)**. El ranging es complemento
   de corto alcance, no la columna vertebral. (Refuerza nuestra `localizacion-rcj-soccer`.)
2. **El equipo MÁS parecido (chamburr) eligió LiDAR 1D, NO ToF multizona VL53, para medir paredes** — con
   gating: descartar por signal strength, distancia mínima, salto brusco, y validar contra el ancho
   conocido de la cancha (`|front+back − largo| < 14 cm`). Constantes verbatim en `coordinate.rs`:
   `LIDAR_SIGNAL_MIN=200`, `LIDAR_DIST_MIN=20`, `FIELD_LENGTH_TOLERANCE=14`.

### 1.2 Landscape de sensores de rango — criterios de selección

| Sensor | Rango (90%) | Rango target NEGRO ~10% | Luz amb. | FoV | Multizona | Interfaz | Veredicto pared baja >1.5 m |
|---|---|---|---|---|---|---|---|
| **VL53L7CX** (el nuestro) | 350 cm (88% **en oscuro**) | **No publicado** | Severa | 60°×60° | Sí 4×4/8×8 | I2C | **NO confiable** — FoV ancho ve piso |
| VL53L8CX | 400 cm (oscuro) | No publicado (~2.8 m reflectante @5klux) | — | 65° diag | Sí | I2C+SPI | Mejor en luz; SPI acelera boot |
| VL53L4CX (single) | **600 cm** (oscuro) | No publicado | — | **18°** | No | I2C | FoV angosto = apunta mejor; apuesta media |
| **TF-Luna** (LiDAR 1D) | 0.2–8 m | **0.2–2.5 m @10% negro** | Inmune 70 klux | **2°** | No | UART/I2C | SÍ, marginal a 2.5 m |
| **TFmini-S** (LiDAR 1D) | 0.1–12 m | **0.1–7 m @10% negro** | Robusto | **2°** | No | UART/I2C | **SÍ — ganador** rango largo a negro |
| Ultrasonido HC-SR04 | 2–400 cm | Igual (no depende de color) | Inmune | Ancho | No | Trig/Echo | Negro OK, haz ancho/lento |

**Criterios (el oficio que faltaba):** ① ¿target oscuro? → mirá la columna "negro", NO el headline. ②
¿apuntar a un blanco angosto sin tocar piso? → FoV angosto gana (2° >> 60°). ③ ¿luz fuerte? → el headline
(medido en oscuro) miente, el rango útil cae a la mitad. ④ ¿necesitás dónde-está-la-pared 2D? → multizona/
LiDAR-scan, pero pagás FoV ancho.

### 1.3 Causa raíz VERIFICADA de la falla de 1.5 m

Dos mecanismos combinados (ambos del datasheet):
1. **PRIMARIA — link budget IR contra pared NEGRA.** El "350 cm" del VL53L7CX es solo blanco-88%-en-oscuro
   (DS13865); el datasheet NO publica rango a baja reflectancia. La pared **negra mate** (reglamento RCJ,
   para contraste de cámaras) absorbe el VCSEL IR → retorno bajo umbral → `target_status` inválido → zona
   descartada. Banco Virginia 2026-06-23: movió/inclinó el robot y la pared no aparece en NINGUNA zona (si
   fuera geometría pura, alguna fila pegaría).
2. **SECUNDARIA — FoV 60° + montaje alto.** Sensor ~170 mm, FoV ±30° → el borde inferior toca piso a ~0.3 m;
   la alfombra cercana devuelve más fotones que la pared baja lejana. (Alturas SIN medir con regla — TASK-225.)

> **Un especialista habría predicho esto con un cálculo de link budget (reflectancia 5-10% + 1/d² + SNR)
> ANTES de montar.** Ninguna de las 5 skills actuales modela esto.

**⚠️ Cambia la estrategia:** la spec oficial Open 2024 diría **pared 220 mm matte black** (no 140). Si es
220, la hipótesis "el ToF mira por encima" se cae (quedaría por debajo del borde) y queda solo el link
budget IR. **MEDIR la altura real.**

**Qué la resuelve (por costo):**
- **Config (gratis, ya implementado):** modo MAX-RANGE (`top_robot2_pri_tofmaxrange`) → medir hasta dónde
  recupera la pared negra. Si ~0.9-1.2 m → localización por paredes CERCANAS viable.
- **Montaje (barato):** bajar el sensor a la altura de la pared.
- **Sensor (medio):** **TFmini-S** (0.1-7 m a negro, haz 2°) — lo que sugiere chamburr (LiDAR 1D).
- **Modalidad (alto, capitaliza 2027):** localizar por **visión de líneas blancas + fusión** (Omicron/
  B-Human). Ya tenemos OpenMV.

**Regulatorio que LIBERA la decisión:** la prohibición de ToF/LiDAR/IR sería SOLO de Lightweight (pelota IR
activa); en Open/Vision NO. Nuestros VL53L7CX serían legales y se podría agregar LiDAR. ⚠️ Confirmar contra
el PDF oficial.

## PARTE 2 — PLAN DE SKILLS NUEVAS (priorizado)

- **#1 (P0) `sensado-rango-link-budget-seleccion`** — física de detección (link budget IR, reflectancia,
  FoV vs apuntado, degradación por luz) + selección ToF/LiDAR/IR/ultrasonido/visión. **La que habría
  predicho la falla.** Gap: ninguna skill modela la física de detección.
- **#2 (P1) `inteligencia-competitiva-rcj-localizacion`** — qué/cómo localizan los mejores (visión-de-líneas,
  ToF/LiDAR-de-pared con gating, fusión con pocas partículas), con el patrón de chamburr verbatim. Gap: no
  hay skill auto-invocable del estado del arte.
- **#3 (P1) `procesamiento-senal-rango-paredes`** — entre el sensor crudo y el corrector de pose: gating en
  cascada, mapeo zona→ángulo, line-fitting RANSAC/Hough, separar pared de rival, degradar con gracia. Gap:
  `tof-vl53l7cx` llega al promedio; el spec F5 nombra el gap zona→ángulo como pendiente.
- **#4 (P2, condicional) `lidar-2d-integracion-rcj`** — modalidad LiDAR 2D si entra hardware (plano de
  escaneo, por qué NO hace falta ICP en cancha conocida). Capitaliza 2027.

**No duplicar:** `fusion-pose-odometria-landmarks` (filtro), `localizacion-rcj-soccer` (elección de técnica),
`tof-vl53l7cx-...` (uso del chip) y la familia timing ya están. Las nuevas se insertan ANTES (selección
física #1), AL COSTADO (inteligencia #2), ENTRE sensor-y-filtro (#3) y como modalidad futura (#4).

## PARTE 3 — RIESGOS / HUECOS (no-verificado, requiere banco/PDF)

**Mediciones de banco pendientes (P0):**
1. Altura real de la pared (¿140 o 220 mm?) con regla → gatea cuál hipótesis sigue viva. TASK-225.
2. Altura real del sensor con regla.
3. Barrido MAX-RANGE: 0.5/1.0/1.5/2.0 m a pared negra real × `target_status` por zona, por sensor.
4. Re-medir con la LUZ de Incheon (no la de Salta) — el alcance cambia con klux.
5. A/B contra un TFmini-S prestado: ¿devuelve 1.5-2.0 m estables donde el VL53 falla?

**No-verificado de la investigación web (declarado):**
- Tablas rango-por-reflectancia del VL53L1X (no extraíbles, timeout TLS).
- Modelos exactos de IMU de algunos equipos (familia confirmada, modelo por dirección I2C).
- TDPs de campeones japoneses (Crescent, Munako) y varios en Drive con auth: no verificables en fuente
  primaria (NO se inventaron técnicas).
- chamburr = TF-Luna: por firma I2C (0x10, reg 0x26), el código no nombra el modelo (inferencia).
- Precios TF-Luna/TFmini-S (~US$20-40): de memoria de mercado.
- Geometría del cono 60°: matemática propia desde el FoV (verificar en banco).
- **Legalidad ToF/LiDAR en Open (regla 6.2.2):** leída vía HTML, NO del PDF oficial verbatim. **Confirmar
  palabra por palabra antes de invertir — es la base de todo.**
