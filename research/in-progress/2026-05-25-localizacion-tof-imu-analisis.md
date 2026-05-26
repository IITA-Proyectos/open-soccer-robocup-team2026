---
title: "Localización XY+heading con 4× VL53L7CX + 2× BNO055 — análisis de 5 alternativas"
date-start: 2026-05-25
date-target-close: 2026-06-05  # decisión necesaria antes de empezar Sprint 1 firmware
status: in-progress
owner: "Claude Opus 4.7 (Anthropic) — análisis; Gustavo Viollaz — decisión"
last-updated: 2026-05-25
priority: P1  # bloquea scope del firmware de localización para Incheon
robot: ambos
area: control
tipo: analisis
related:
  - journal/2026-05-25-top-xshut-no-routed-hallazgo-forense.md
  - research/in-progress/2026-05-25-top-board-rev-1.1-wishlist.md
  - team-tasks/2026-05-25-task-034-decidir-arquitectura-localizacion-incheon.md
  - software/teensy/Soccer 2026/src/top/sensors_tof.cpp
  - software/teensy/Soccer 2026/src/top/sensors_imu.cpp
---

# Localización XY+heading con 4× VL53L7CX + 2× BNO055 — análisis de 5 alternativas

> **TL;DR.** Para resolver pose `(x, y, θ)` del robot en la cancha RCJ Soccer
> Open con el hardware disponible (4× VL53L7CX en disposición cardinal +
> 2× BNO055), se evaluaron 5 alternativas algorítmicas con precisión,
> costo CPU, tiempo de implementación y robustez frente a obstáculos
> dinámicos (3 robots: 1 partner + 2 oponentes). **Recomendación coach:**
> Sprint 1 = Alternativa 1 (trilateración geométrica directa, ~3-4 días,
> ±2-3 cm). Sprint 2 = Alternativa 5 v2 (LUT matching multizona 8×8,
> ~5-7 días, ±1 cm). **NO** implementar Alt 2/3/4 — costo/beneficio no
> cierra para Incheon. Total Sprint 1+2: ~10 días, encaja en calendario
> (42 días al torneo). Decisión pendiente: ver TASK-034.

## Pregunta de investigación

Dado el hardware confirmado en TOP rev 1.0:

- **4× VL53L7CX** (no L5CX), FoV **60°** (no 90°), multizona **8×8 = 64
  valores por sensor**, montados a **14 cm de altura** del piso, en
  disposición **cardinal** (frontal, trasero, izquierdo, derecho del
  robot).
- **2× BNO055** (heading fusionado de magnetómetro + giro + acel),
  presumiblemente uno en TOP y otro en DOWN (o ambos en TOP — confirmar
  con Enzo, no crítico para este análisis).

¿Cuál es el algoritmo más adecuado para producir pose `(x, y, θ)` del
robot en cancha **antes** de cada `WorldSnapshot` (~30 Hz objetivo) sin
saturar la CPU del Teensy 4.0 ni inflar el dev-time más allá del
calendario Incheon?

## Por qué importa

- **Pose absoluta** es input crítico para la FSM de strategy (decidir
  cuándo avanzar, cuándo defender, cuándo patear).
- **Sin localización el robot vuelve a ser reactivo** (solo persigue la
  pelota), perdiendo el valor diferencial del diseño de 4 ToFs.
- La decisión de algoritmo se tiene que tomar **ahora** porque define:
  - Scope del firmware (qué módulos hay que escribir).
  - Calibración necesaria (qué se mide, dónde, cómo).
  - Dependencia con TASK-033 (¿2 ToFs sin rework vs 4 con bodge?) — si
    son 2 ToFs la mayoría de alternativas degradan precisión, y Alt 5
    pierde gran parte de su ventaja.

## Restricciones del proceso

- Análisis hecho por Claude, decisión por Gustavo (ver TASK-034).
- **No empezar implementación** hasta que la decisión esté escrita en
  el TASK + journal.
- Cuando se decida, mover este doc a `research/completed/` con sección
  "Decisión final" al inicio.

## Hardware disponible (recapitulación)

### TOFs VL53L7CX (×4)

- **FoV**: 60° (no 90° como originalmente asumido — confirmado por
  Gustavo el 2026-05-25).
- **Resolución multizona**: 8×8 = 64 sub-mediciones por frame, cada una
  cubre un sub-cono de ~7.5° × 7.5°.
- **Rango efectivo**: 0.04 m – 4.0 m (datasheet).
- **Frame rate**: 15-30 Hz según resolución.
- **Disposición**: 4 cardinales (frontal, trasero, izquierdo, derecho)
  centrados en el eje del robot.
- **Altura de montaje**: **14 cm** del piso. Crítico — significa:
  - La pelota (centro a 3.5 cm, tope a 7 cm) **NO interfiere** con el
    haz por geometría (el haz pasa por encima de la pelota).
  - Las **paredes** (altura 14 cm en RCJ Soccer Open según reglamento
    2026) **sí** son target válido — el sensor las mide casi a la
    altura del centro de la pared.
  - **Los 3 robots** (cuerpo típico 22 cm alto) **sí interfieren** — son
    obstáculos dinámicos que hay que detectar/filtrar.

### IMUs BNO055 (×2)

- Heading absoluto fusionado (magnetómetro + giro + acelerómetro).
- Drift bajo (~0.5°/h en condiciones nominales).
- Frecuencia configurable hasta 100 Hz.
- **Restricción Incheon**: el magnetómetro puede ser perturbado por
  motores cercanos / hierro del chasis. Calibración previa al partido
  obligatoria (sección 3 de skill `openmv-vision-tuning` — adaptable).

## Las 5 alternativas

Las 5 alternativas se diseñaron como un **espectro creciente de
sofisticación** desde "trigonometría a mano" (Alt 1) hasta "stack
probabilístico clásico de robótica móvil" (Alt 4). La Alt 5 es un
enfoque alternativo (table-lookup con multizona) que sacrifica
generalidad por velocidad y simpleza de debug.

---

### Alternativa 1 — Trilateración geométrica directa

**Idea.** Las 4 distancias de los TOFs son las distancias del robot a
las 4 paredes en las direcciones cardinales del *robot*. Si conocemos
`θ` (heading del BNO), las podemos rotar al frame de la cancha y
resolver `(x, y)` por geometría directa:

```
d_front = wall_north_y - robot_y  (si robot mira al norte de la cancha)
d_back  = robot_y - wall_south_y
d_left  = robot_x - wall_west_x
d_right = wall_east_x - robot_x
```

Heading `θ` viene directo del BNO. Sumar/restar pares de TOFs da
redundancia para detectar inconsistencias (e.g., si `d_front + d_back`
≠ largo de la cancha, hay un obstáculo entre el robot y una de las
paredes).

**Precisión esperada.** ±2-3 cm en condiciones limpias (sin
obstáculos). La precisión del L7CX a < 2 m es de ±15 mm según
datasheet — sumando geometría queda ~±20-30 mm.

**Costo CPU.** Despreciable (4 sumas/restas + 1 rotación por frame).

**Dev time.** **1 día**. Es matemática del secundario.

**Robustez Fase 2 (con obstáculos = otros robots).** **Baja**. Si un
robot oponente está entre nuestro robot y la pared, la lectura
correspondiente colapsa (mide al oponente, no a la pared). Sin
filtrado el método produce pose loca.

**Mitigaciones simples disponibles.**
- Usar consistencia entre pares: si `d_front + d_back > largo_cancha`,
  uno de los dos está midiendo un obstáculo → descartar el corto.
- Si solo hay 1 obstáculo, los otros 3 TOFs alcanzan a resolver pose
  (el problema queda sobre-determinado con 3, único con 2).

**Complejidad de implementación.** ★ (mínima).

**Cuándo usarla.** Sprint 1 / baseline funcional rápido. Comparar
contra Alt 5 cuando esté lista.

---

### Alternativa 2 — Wall-follow + dead reckoning IMU

**Idea.** No resolver pose absoluta continuamente. Cuando el robot se
acerca a una pared (cualquier TOF reporta < 30 cm), "anclar" la pose
contra esa pared y resetear el integrador IMU. Entre anclajes, integrar
velocidad estimada por IMU (acelerómetro doble integrado) + heading.

**Precisión esperada.** ±2-3 cm en el momento del anclaje, **degrada
rápidamente entre anclajes** (drift de IMU integrado es típicamente >
10 cm/s² de error → varios cm en pocos segundos).

**Costo CPU.** Baja (filtros simples sobre IMU + lógica de anclaje).

**Dev time.** **1-2 días**.

**Robustez Fase 2.** **Media**. Los obstáculos no engañan a la pose
cuando el robot está lejos de paredes (porque integra IMU). Pero el
drift hace que se vuelva inútil rápidamente.

**Complejidad.** ★★.

**Cuándo usarla.** **Nunca como solución única.** Útil solo como capa
fallback dentro de un sistema más rico (e.g., si todos los TOFs ven
obstáculos, integrar IMU hasta que vuelva a ver una pared). **No
recomendado para Incheon.**

---

### Alternativa 3 — Extended Kalman Filter (EKF)

**Idea.** Estado `[x, y, θ, vx, vy, ω]ᵀ`. Modelo de proceso simple
(velocidad constante con ruido). Observaciones: 4 TOFs + heading BNO.
Cada ciclo: predict (con modelo) + update (con observaciones).
Innovaciones filtradas para detectar outliers (obstáculos).

**Precisión esperada.** ±0.5-1 cm en condiciones limpias. **El filtrado
estocástico de innovaciones** detecta obstáculos automáticamente y los
rechaza si la innovación supera N·σ.

**Costo CPU.** ~50 µs por update en Teensy 4.0 con matrices 6×6
(estimación a banco; medir cuando se implemente).

**Dev time.** **3-5 días**. Incluye:
- Implementar EKF embebido (matrices float, sin libs grandes).
- Tunear matrices `Q` (proceso) y `R` (observación) — esto es lo que
  más tiempo se lleva, requiere iteración con datos reales.
- Diseñar test de validación (mover robot por trayectoria conocida).

**Robustez Fase 2.** **Alta**. EKF rechaza naturalmente lecturas
inconsistentes vía innovación. Reentra automáticamente cuando los
obstáculos despejan.

**Complejidad.** ★★★★.

**Cuándo usarla.** Si tuviéramos 6 meses, sería la respuesta correcta.
Para Incheon (42 días) la suma "5 días dev + tunear + validar" come
budget de otras P0. **No recomendado para Incheon, sí para 2027.**

---

### Alternativa 4 — Particle Filter (Monte Carlo Localization, MCL)

**Idea.** Representar la distribución de pose como N partículas
(~200-500 en embebido). Cada ciclo: propagar partículas con modelo de
proceso + ruido, pesar cada partícula por verosimilitud de las 4
mediciones TOF (rasterizar cada partícula contra mapa de cancha + ray
casting). Resampling cada K ciclos.

**Precisión esperada.** ±1 cm con N grande, **muy robusto** a
obstáculos (las partículas que predicen el obstáculo correctamente
ganan peso).

**Costo CPU.** ~500 µs por ciclo con N=200 partículas (estimación —
medir cuando se implemente). 30 Hz cabe holgado en Teensy 4.0 pero
puede comer ciclos de otros módulos.

**Dev time.** **4-7 días**. Incluye:
- Implementar raycasting eficiente (precalcular mapa).
- Implementar resampling sin sesgo (sistematic, no naive).
- Tunear N + sigma de proceso.

**Robustez Fase 2.** **Muy alta**. MCL es el estándar de la industria
para localización robusta con obstáculos.

**Complejidad.** ★★★★★.

**Cuándo usarla.** **Overkill para una cancha de 1.83 × 2.43 m con 4
paredes ortogonales**. MCL brilla en mapas complejos o features
escasos — acá la geometría es trivial. **No recomendado para Incheon
ni para 2027 — la cancha es demasiado simple para justificarlo.**

---

### Alternativa 5 — LUT matching v2 (con multizona 8×8)

**Idea.** Pre-computar offline una **lookup table (LUT)** que para cada
celda de la cancha (e.g., grilla de 5 cm) almacene el **patrón esperado
de los 4 sensores en sus 64 zonas cada uno** (256 valores por celda).
En runtime: capturar las 4 × 64 = 256 mediciones reales, comparar con
el patrón de cada celda, elegir la celda de menor error.

**v1** sería con valores escalares (1 distancia por sensor) y matching
trivial. **v2** aprovecha las 64 zonas del 8×8, lo que habilita:

- **Detectar pared plana vs objeto puntual** por varianza dentro del
  array (pared plana = baja varianza; objeto = alta).
- **Usar solo mitad superior** del array (zonas que apuntan más arriba
  del piso) para mejor SNR (evita rebotes del piso muy cerca).
- **Detectar esquinas** por gradiente en el array (transición pared-a-
  pared visible en una sola lectura).

**Precisión esperada.** ±1 cm con grilla fina (2 cm) y matching
multidimensional bien diseñado.

**Costo CPU.** ~500 µs por ciclo (con LUT ~5000 entradas × 256 valores
= 1.28 MB → necesita SRAM externa o flash; o bien grilla más gruesa
con interpolación final).

**Dev time.** **4-5 días** (v2). Incluye:
- Generar LUT offline (script Python, ~1 día).
- Implementar matching embebido eficiente (~2 días).
- Validar con datos reales en cancha (~1-2 días).

**Robustez Fase 2.** **Muy alta**. Los obstáculos se detectan por
varianza intra-array (un cuerpo de robot tiene firma muy distinta a
una pared lisa), y se descartan del matching.

**Complejidad.** ★★★ (más simple que EKF/MCL — todo el peso del
algoritmo está en el preprocessing offline).

**Cuándo usarla.** Sprint 2 / upgrade de Alt 1 cuando ya esté la
infraestructura funcional. **Recomendado para Incheon como upgrade.**

---

## Tabla resumen

| # | Alternativa | Precisión | CPU | Dev time | Robustez Fase 2 | Complejidad | Recomendación |
|---|---|---|---|---|---|---|---|
| 1 | Trilateración geométrica directa | ±2-3 cm | despreciable | 1 día | baja | ★ | **Sprint 1** |
| 2 | Wall-follow + dead reckoning IMU | ±2-3 cm (degrada) | baja | 1-2 días | media | ★★ | descartar |
| 3 | EKF | ±0.5-1 cm | ~50 µs | 3-5 días | alta | ★★★★ | descartar (2027) |
| 4 | Particle Filter (MCL) | ±1 cm | ~500 µs | 4-7 días | muy alta | ★★★★★ | descartar (overkill) |
| 5 | LUT matching v2 (multizona 8×8) | ±1 cm | ~500 µs | 4-5 días | muy alta | ★★★ | **Sprint 2** |

## Recomendación del coach

**Sprint 1 (días 1-4):** Alternativa 1 (trilateración geométrica directa).

- Baseline funcional rápido.
- Aprende el equipo a usar TOFs en código real.
- Valida que el hardware ToF cardinal + BNO entrega lo que dice (test
  de "robot quieto en cancha sin obstáculos → ¿qué reporta?").
- Si el resultado es "anda y suficientemente preciso para nuestra
  estrategia", **se puede ir a Incheon con esto** y diferir Sprint 2.

**Sprint 2 (días 5-10):** Alternativa 5 v2 (LUT multizona).

- Upgrade que aprovecha el 8×8 del L7CX (que Alt 1 desperdicia).
- Robustez frente a obstáculos sin meter EKF/MCL.
- Decisión de ir o no a Sprint 2 se toma **después** de evaluar Sprint
  1 en cancha real (no en banco).

**Total ~10 días.** Compatible con el calendario Incheon (42 días) con
margen amplio.

### Por qué NO Alt 2 / Alt 3 / Alt 4

- **Alt 2 (dead reckoning)**: drift de IMU integrado es prohibitivo
  para Incheon (decenas de cm en pocos segundos sin anclaje). Útil
  solo como fallback dentro de un sistema más rico — no como solución.
- **Alt 3 (EKF)**: dev time + tuning consume budget de otras P0
  (TASK-014/015/016/022). La precisión incremental sobre Alt 5 no
  justifica el esfuerzo para Incheon. **Considerar para 2027** cuando
  haya tiempo de tunear bien.
- **Alt 4 (MCL)**: overkill para una cancha 1.83 × 2.43 m con 4
  paredes ortogonales. Brilla en mapas complejos o ambigüedad alta —
  acá no aplica.

### Dependencia con TASK-033

Si se decide en TASK-033 ir con **2 ToFs sin rework** (vs 4 con
bodge):

- **Alt 1** sigue siendo viable pero pierde redundancia (no se puede
  detectar obstáculos por consistencia de pares).
- **Alt 5** pierde gran parte de su ventaja (la riqueza de matching
  multizona se reduce a la mitad de sensores).
- Recomendación si solo hay 2 ToFs: ir con Alt 1 y considerar agregar
  un BNO como ancla absoluta de yaw (que ya tenemos).

**Implicación:** la decisión de algoritmo está acoplada con la de
hardware. Resolver TASK-033 primero (o en paralelo) antes de
arrancar implementación.

## Riesgos / supuestos abiertos

1. **El BNO055 puede tener drift de heading en presencia de motores**
   — la magnetometría es sensible. Mitigación: usar el segundo BNO como
   referencia (si los dos disagree > 5°, sospechar). Validar en banco
   con motores corriendo. Bloquea Alt 1 y degrada Alt 5.
2. **El reglamento RCJ Soccer Open 2026 puede haber cambiado las
   paredes** (altura, color, reflectividad). Verificar reglamento
   vigente antes de calibrar. Si las paredes son < 14 cm el haz del
   TOF las pasa por arriba → catástrofe.
3. **La iluminación de Incheon** puede afectar mediciones IR-based del
   L7CX (datasheet menciona degradación con sunlight directo). La
   cancha es indoor — riesgo bajo, pero no nulo si hay ventanales.

## Próximos pasos

### Para cerrar este research (mover a `completed/`)

1. **Gustavo decide arquitectura en TASK-034.** Recomendación:
   "Sprint 1 Alt 1 + Sprint 2 Alt 5 v2 condicional".
2. **Resolver TASK-033** (¿2 vs 4 ToFs?) en paralelo — la decisión
   acopla con esta.
3. **Si se aprueba**, crear sub-tasks de implementación:
   - Sub-task Sprint 1: implementar trilateración en
     `src/top/localization.{h,cpp}` (módulo nuevo).
   - Sub-task Sprint 2: condicional al éxito de Sprint 1.
4. **Mover doc a `research/completed/`** con sección "Decisión final"
   al inicio (qué se eligió, por qué, link al PR de implementación).

### Decisiones que NO se toman hoy

- Qué grilla usar para LUT v2 (5 cm vs 2 cm) → decisión técnica al
  arrancar Sprint 2.
- Si el segundo BNO se usa como ancla redundante o se dedica al DOWN
  (heading del arquero) → decisión técnica al integrar.

## Referencias

- Datasheet VL53L7CX:
  `research/in-progress/um2884-a-guide-to-using-the-vl53l5cx-multizone-timeofflight-ranging-sensor-with-wide-field-of-view-ultra-lite-driver-uld-stmicroelectronics.pdf`
  (es L5CX pero la familia comparte arquitectura ULD/multizona).
- Reglamento RCJ Soccer Open 2026: pendiente subir a `research/references/`.
- Hardware actual TOP: `journal/2026-05-24-hardware-up-top-tof-frontal-resuelto.md`
  + `journal/2026-05-25-top-xshut-no-routed-hallazgo-forense.md`.
- TASK acoplada: `team-tasks/2026-05-25-task-033-decidir-cuantos-tofs-incheon.md`.

## Atribución

- **Hardware spec aclarado** (L7CX no L5CX, FoV 60° no 90°, altura 14 cm,
  disposición cardinal) — Gustavo Viollaz (@gviollaz), input verbal del
  2026-05-25.
- **Diseño del espectro de 5 alternativas + análisis de trade-offs +
  recomendación coach** — Claude Opus 4.7 (Anthropic), session
  2026-05-25.
- **Decisión final** — Gustavo Viollaz, a ejecutar en TASK-034.
