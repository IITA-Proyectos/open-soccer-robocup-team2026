---
name: localizacion-rcj-soccer
description: Use when deciding HOW the IITA RCJ Soccer Open robot should know where it is and which way it faces on the field — choosing/explaining a localization technique, or mapping academic terms to this robot. Triggers - "dónde está el robot", "localización", "pose", "saber la posición en la cancha", "SLAM", "visual SLAM", "odometría visual / visual odometry", "particle filter / filtro de partículas", "MCL / Monte Carlo", "localización por landmarks / balizas", "trilateración", "EKF", "Kalman", "pose estimation", "el robot no sabe dónde está". Teaches every technique with an honest feasibility verdict for OpenMV+Teensy compute, and maps each to the module that ALREADY exists in this repo. NOT for BUILDING/tuning the estimator that fuses sensors (use fusion-pose-odometria-landmarks), NOT for tuning the cameras that see the landmarks (openmv-vision-tuning), NOT for the omni motion model (dinamica-omni-3-ruedas).
---

# Localización en RCJ Soccer Open — qué técnica y por qué (lente de coach)

## Principio central

**Localizarte NO es un solo algoritmo: es elegir qué señales creés y cómo las
combinás.** Hay dos familias de señal, con defectos opuestos, y toda localización
seria es pegarlas:

| Señal | Qué te dice | Defecto |
|---|---|---|
| **Odometría** (cuánto te moviste) | movimiento relativo, suave, rápido, siempre disponible | **el error se ACUMULA** — a los pocos segundos la pose es basura |
| **Referencia absoluta** (landmarks: paredes, arcos, líneas) | dónde estás de verdad, sin deriva | **intermitente y ruidosa** — a veces no ves ningún landmark, a veces ves uno falso |

> **Analogía sostenida (usala con los alumnos).** Ubicarte en la cancha es como
> caminar por una ciudad: la **odometría** es contar tus pasos (sabés cuánto
> avanzaste, pero a las dos cuadras ya te equivocaste de medio bloque); los
> **landmarks** son leer un cartel de calle conocido (te reubica exacto, pero
> solo de a ratos). Tu cerebro **fusiona**: caminás contando pasos y cada tanto
> mirás un cartel para corregir. **SLAM** sería dibujar el mapa de la ciudad
> mientras caminás — pero en RCJ **el reglamento ya te dio el mapa**, así que
> dibujarlo de nuevo es quemar CPU al pedo.

## El veredicto honesto para ESTE hardware (leer antes de entusiasmarte con SLAM)

La cancha RCJ Soccer Open está **100% especificada por el reglamento**: medidas,
colores y posición de los arcos, posición de las líneas. **El mapa te lo dan.**
Por eso:

- **Visual SLAM → NO.** SLAM resuelve "entorno desconocido". No tenés uno.
  Construir un mapa que ya tenés es la inversión equivocada en un OpenMV/Teensy.
- **Visual Odometry (cámara frontal) → NO.** Cancha verde sin textura + costo de
  CPU en OpenMV + ya tenés odometría óptica mejor (los OTOS miran al piso).
- **Lo correcto es _localización en mapa conocido_** = landmarks (paredes por
  ToF, arcos por cámara, líneas por el anillo) + odometría (OTOS) + un **heading**
  que rompa la simetría de la cancha (BNO/OTOS), todo **fusionado** con un filtro
  liviano. Eso es lo que el robot YA tiene a medio construir.

Detalle técnica-por-técnica (qué es, cuándo gana, cuánto cuesta en CPU, veredicto
RCJ): **`references/tecnicas-localizacion-explicadas.md`**. Cargala cuando alguien
pregunte por VO, SLAM, MCL/partículas, EKF o landmarks.

## El mapa: técnica académica → módulo REAL de este repo

> Frame de la cancha (canónico): **`docs/CONVENCION-EJES-ROBOT.md §2`**. Origen =
> esquina pared propia-izquierda; **+Y = arco rival (largo, 2430 mm)**, **+X =
> derecha (corto, 1820 mm)**; heading 0 = mirando al arco rival.

| Técnica que nombran | En tu robot es… | Módulo | ¿Corre hoy? |
|---|---|---|---|
| Dead-reckoning / wheel odometry | **OTOS** (2× óptico al piso, en DOWN) — mejor que encoders en un omni que patina | `src/down/otos.*`, `src/shared/otos_*` | ✅ llega a CENTRAL (Pose2D/Vel2D); **deriva** |
| Visual Odometry | el OTOS ES odometría óptica (flujo, mirando abajo) | OTOS | ✅ (no hay VO de cámara frontal, ni hace falta) |
| Landmark localization | trilateración: 4 ToF → 4 paredes (+ arcos por cámara, futuro) | `src/shared/localization.{h,cpp}` | ✅ **vivo** en el WorldSnapshot (`my_x/y_mm`, conf 70/0); **frágil** (necesita ≥1 ToF por eje) |
| Heading / referencia angular | BNO055 (giroscopio) + IMU interna del OTOS | `sensors_imu`, `imu_fusion`, `otos_fusion` | ⚠️ BNO **se congela** por contención I²C con los ToF (TASK-207); OTOS-heading no cableado |
| Pose estimation / sensor fusion (EKF) | filtro complementario ToF (absoluto) + OTOS (deriva) | `src/shared/pose_fusion.{h,cpp}` | ❌ **escrito y testeado host, NO cableado** ← la pieza que falta |
| Suavizado temporal (mediana/EMA + anti-salto) | gate de salto > 400 mm + EMA/mediana | `src/shared/pose_filter.{h,cpp}` | ❌ escrito, NO cableado |
| MCL / Particle Filter | — | — | ❌ no existe en el código (y para Incheon **probablemente no lo necesites** — ver referencia) |
| EKF "de libro" | — | — | ❌ diferido a 2027 por decisión de diseño (el complementario da el 80% por el 20% del código) |

**Conclusión de lectura:** el cuello de botella NO es "nos falta SLAM". Es que la
**capa de fusión ya existe (`pose_fusion`) y no está enchufada**, y que el
**heading absoluto es inestable** (BNO se congela). Ahí está el ROI.

## Prioridades (formato coach — cada tema lo cierra el equipo en banco)

- **P1 — Heading confiable.** Sin un heading que no derive ni se congele, TODA la
  pose absoluta se desvía (la trilateración rota el mapa). Es la raíz. Caminos:
  BNO a bus propio (TASK-207) y/o usar el heading del OTOS como respaldo.
  `risk-no-fix`: la pose absoluta es inusable en partido → la FSM no puede hacer
  juego posicional. `risk-fix`: tocar el bus I²C del TOP puede romper los ToF.
- **P1 — Caracterizar el ruido de los 3 sensores en banco** (deriva del OTOS en
  mm/s, varianza del ToF, deriva del heading). **Sin esos números no se puede
  tunear NINGÚN filtro.** Protocolo: `fusion-pose-odometria-landmarks` →
  `references/medir-ruido-sensores.md`. `risk-no-fix`: cualquier fusión queda
  tuneada a ojo. `tiempo`: ~2 h de banco.
- **P2 — Cablear `pose_fusion`** (complementario ToF+OTOS) detrás de un flag,
  default OFF, validar que la conducta de competencia no cambia hasta probarlo.
  `risk-fix`: bajo (gateado). Capitaliza a 2027.
- **P2 (2027) — EKF de 3 estados / MCL.** Alto valor de aprendizaje y el tool
  correcto para Middle Size League, pero NO es el camino a Incheon.

## Errores comunes

- Pedir "implementemos Visual SLAM" sin notar que el mapa **ya está dado** →
  semanas de CPU tiradas resolviendo un problema que no existe.
- Tunear un filtro de fusión **sin haber medido el ruido** de cada sensor (los
  números de covarianza son MEDIDOS, no inventados).
- Confiar en la trilateración con el **heading congelado**: la pose sale
  perfectamente equivocada (mapa rotado), no "ruidosa" — más peligroso.
- Creerle al ToF cuando un robot rival está a 30 cm tapando la pared (por eso el
  outlier-rejection del §5 de `localization.cpp` existe — entenderlo antes de tocarlo).
- Olvidar que la pose absoluta **se calibra al boot apuntando al arco rival**: si
  el equipo enciende el robot mal orientado, todo el partido va con offset.

## Skills relacionadas (no reimplementar acá)

- **REQUIRED para construir/tunear el estimador:** `fusion-pose-odometria-landmarks`
  (filtro complementario / EKF / partículas + protocolo de medición de ruido).
- **Modelo de movimiento del omni** (la "física" que predice la odometría):
  `dinamica-omni-3-ruedas`.
- **Ver los landmarks (arcos/pelota con la cámara):** `openmv-vision-tuning`.
- **Lazo de control que CONSUME la pose** (cuantización del actuador):
  `control-pid-zona-muerta`.
- **Validar en hardware real (obligatorio):** `hardware-test-protocol`.
- **Frame de ejes canónico:** `docs/CONVENCION-EJES-ROBOT.md`.
