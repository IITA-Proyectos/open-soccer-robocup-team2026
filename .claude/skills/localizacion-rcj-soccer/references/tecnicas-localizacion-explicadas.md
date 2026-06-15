# Técnicas de localización — explicadas, con veredicto para RCJ Soccer Open

> Referencia de `localizacion-rcj-soccer`. Una sección por técnica que un alumno
> puede nombrar. Cada una: **qué es** (en una analogía), **cuándo gana**, **cuánto
> cuesta en CPU**, **veredicto para nuestro hardware** (OpenMV N6 + Teensy 4.x), y
> **qué pieza de nuestro robot la encarna**. La voz es de estudiante: jerga
> explicada, sin humo.

El hardware de referencia para todos los veredictos: **Teensy 4.0/4.1** (Cortex-M7
@600 MHz, FPU de simple precisión, ~1 MB RAM) para el control/fusión, y **OpenMV
N6** (otro M7, corriendo MicroPython, ~30-60 fps según resolución) para visión. NO
hay una Jetson, ni una Raspberry Pi, ni una GPU. Esa restricción decide casi todo.

---

## 1. Odometría (dead-reckoning) — "contar tus pasos"

**Qué es.** Estimás cuánto te moviste integrando tu propio movimiento. Versión
clásica: contar vueltas de rueda (encoders) y aplicar la cinemática. Versión
nuestra: el **OTOS** mira el piso con un sensor de flujo óptico (como el de un
mouse) y te da Δx, Δy, Δθ directo.

**Cuándo gana.** Siempre disponible, suave, alta frecuencia (100 Hz), no necesita
ver nada del entorno. Es la base sobre la que todo lo demás corrige.

**El defecto que define todo.** **El error se acumula (deriva).** Cada medición
tiene un error chico; al integrarlas, los errores se suman. En un omni que patina,
los encoders de rueda mienten mucho (la rueda gira pero el robot no avanza) — por
eso el **OTOS óptico es mejor** acá: mide el movimiento real del piso, no el de la
rueda. Aun así deriva: minutos de juego sin corrección y la pose es ficción.

**CPU.** Trivial. El OTOS hace el trabajo pesado en su propio chip.

**Veredicto RCJ.** ✅ **Imprescindible y ya lo tenés.** Es la "predicción" de
cualquier filtro. Nunca lo uses solo: siempre corregilo con landmarks.

**En tu robot:** `src/down/otos.*` + `src/shared/otos_position/otos_fusion/otos_health`.
El modelo de movimiento que lo respalda (cómo se mueve el omni): `dinamica-omni-3-ruedas`.

---

## 2. Visual Odometry (VO) — odometría con una cámara

**Qué es.** Odometría, pero estimando el movimiento propio del robot mirando cómo
se desplazan los puntos entre cuadros consecutivos de una cámara (seguís
"features" frame a frame y deducís cuánto te moviste).

**Cuándo gana.** Cuando no tenés encoders ni OTOS y SÍ tenés una cámara con
escena texturada (drones, autos, interiores con detalle). Es la base de muchos
sistemas de robótica móvil grande.

**CPU.** Cara: detección + matching de features por cuadro. Pesa en un M7 con
MicroPython.

**Veredicto RCJ.** ❌ **No.** Dos razones: (1) la cancha es verde lisa, **sin
textura** para trackear features de forma confiable; (2) **ya tenés odometría
óptica mejor** — el OTOS hace exactamente esto pero mirando al piso, en hardware
dedicado, sin gastar la CPU de visión que necesitás para ver la pelota y los
arcos. Nota conceptual linda para el equipo: **el OTOS ES "visual odometry"** (un
sensor de flujo óptico es un VO de una sola dirección, downward-facing). Cuando
alguien pida VO, ya lo tienen.

---

## 3. Localización por landmarks (balizas) — "leer carteles de calle"

**Qué es.** Conocés de antemano **dónde está cada landmark** (cartel) en el mapa.
Medís la **distancia y/o el ángulo** a uno o varios y resolvés geométricamente
dónde tenés que estar para que esas mediciones cierren.

- **Trilateración** = a partir de **distancias** a puntos/paredes conocidas.
- **Triangulación** = a partir de **ángulos** (bearings) a puntos conocidos.

**Cuándo gana.** Cuando el mapa es conocido y tenés landmarks medibles. **Es
exactamente el caso de RCJ.** Tus landmarks:
- **Las 4 paredes** → distancia por los 4 ToF → **trilateración** (lo que ya
  hacés). Geometría favorable: las barricadas de 10-22 cm las ve el ToF a 14 cm
  como pared continua.
- **Los 2 arcos** (cian/magenta) → ángulo (bearing) por la cámara → triangulación
  (futuro; hoy la cámara da pelota+arcos pero no entra a la pose).
- **Las líneas blancas** → el anillo de 32 sensores dice "estoy sobre/cerca de una
  línea" → corrección de borde.

**CPU.** Barata. Geometría cerrada (cosenos, proyecciones). Una LUT de coseno y
listo.

**Veredicto RCJ.** ✅ **Es el frame correcto para este robot.** "Localización en
mapa conocido por landmarks" es el nombre técnico de lo que tenés que hacer bien.

**En tu robot:** `src/shared/localization.{h,cpp}` (trilateración a paredes,
±2-3 cm en condiciones limpias). Frágil hoy porque con montaje cardinal hay **1
solo ToF por eje** → si ese ToF lo tapa un rival, perdés el eje. Mejora futura
(Sprint 2): usar las **64 zonas 8×8** de cada ToF para distinguir pared plana de
obstáculo (`LUT matching multizona`).

---

## 4. Monte Carlo Localization (MCL) / Filtro de partículas — "muchas apuestas a la vez"

**Qué es.** En vez de una sola estimación de pose, mantenés una **nube de cientos
de hipótesis** ("partículas"), cada una una pose posible con un peso. En cada
ciclo: (1) movés todas según la odometría + ruido, (2) a cada una le calculás
"¿qué tan bien explica lo que estoy midiendo?" (comparás el ToF/bearing que esa
partícula PREDICE contra el medido) y la repesás, (3) **resampleás** (las apuestas
buenas se reproducen, las malas mueren). La nube converge a dónde estás.

**Cuándo gana — y por qué es famoso en RoboCup.** Cuando la creencia es
**multimodal o ambigua**: la cancha es casi **simétrica** (si no sabés tu heading,
"estás en X,Y mirando al arco rival" y "estás en el espejo mirando al propio" dan
el MISMO patrón de paredes). Un filtro gaussiano (EKF) **no puede** representar
"podría estar en dos lugares"; un filtro de partículas sí. También resuelve el
**"kidnapped robot"** (te levantan y te reubican): esparcís partículas y la nube
re-converge sola.

**CPU.** Proporcional a `N_partículas × costo del modelo de observación`. En un
Teensy 4, con ~100-300 partículas y un modelo de observación barato (comparar 4
distancias predichas), es **viable a 20-50 Hz**. No es gratis, pero entra.

**Veredicto RCJ.** ⚠️ **Aprendelo, pero probablemente NO para Incheon.** La razón:
si tenés un **heading confiable** (el BNO/OTOS rompe la simetría de la cancha) y
los ToF a las paredes, tu creencia es **unimodal** → un filtro complementario o un
EKF es **mucho más barato y suficiente**. El MCL recién gana su lugar si: (a) el
heading NO es confiable (justo tu problema hoy con el BNO — pero la solución más
barata es arreglar el heading, no montar un MCL), (b) los landmarks son ambiguos,
o (c) te importa la recuperación de kidnapped-robot. **Es el tool correcto para
Middle Size League / 2027**, alto valor de aprendizaje, capitalizable. Para Incheon
el ROI está en arreglar el heading + cablear el filtro complementario que ya tenés.
Si igual querés un MCL mínimo, la receta está en `references/complementario-ekf-particulas.md`.

---

## 5. EKF (Extended Kalman Filter) — "una apuesta con margen de error que se ajusta solo"

**Qué es.** Mantenés **una** estimación de pose (x, y, θ) MÁS una **covarianza** (tu
incertidumbre, como una elipse de error). Dos pasos por ciclo:
- **Predicción:** movés la pose con la odometría y **agrandás** la elipse (te volvés
  menos seguro al moverte).
- **Corrección:** llega una observación de landmark; la usás para mover la pose hacia
  donde el landmark dice y **achicás** la elipse. El "Kalman gain" decide cuánto
  creerle a la observación según el ruido relativo (medido) de odometría vs landmark.

**Cuándo gana.** Creencia **unimodal** (un solo lugar plausible) con observaciones
continuas — el caso típico una vez que tenés heading. Es la respuesta "de libro"
para fusión de pose y la base del juego posicional en robótica móvil.

**CPU.** Para 3 estados (x, y, θ), son matrices 3×3 — **muy barato** en el Teensy
con FPU. El costo real no es la CPU: es **tunear las covarianzas** (necesitás el
ruido MEDIDO de cada sensor) y manejar la no-linealidad del heading (wrap ±180°).

**Veredicto RCJ.** 🟡 **El destino correcto, diferido a 2027 por decisión de
diseño.** Y con razón: un **filtro complementario** (que ya tenés escrito en
`pose_fusion.h`) es un EKF "pobre" de ganancia fija que da el **80% del beneficio
por el 20% del código y CERO tuneo de covarianza**. Camino sano: cableá el
complementario para Incheon → si en banco ves que una ganancia fija no alcanza,
subís a EKF en 2027 con el ruido ya medido. Receta y comparación:
`references/complementario-ekf-particulas.md`.

---

## 6. Visual SLAM — construir el mapa Y ubicarte, a la vez

**Qué es.** SLAM = *Simultaneous Localization And Mapping*. Te metés en un entorno
**desconocido** y, mientras te movés, construís el mapa Y te ubicás en él al mismo
tiempo (problema del huevo y la gallina: necesitás el mapa para ubicarte y la
ubicación para mapear). Visual SLAM lo hace con cámara(s) (ORB-SLAM, etc.).

**Cuándo gana.** Entornos **desconocidos y grandes**: un robot de rescate
explorando un edificio, un drone en un espacio nuevo, un auto autónomo.

**CPU.** Alta a muy alta (keyframes, bundle adjustment, loop closure). Pensado para
GPU / CPU de PC, no para un M7 con MicroPython.

**Veredicto RCJ.** ❌ **No, y es importante entender por qué** para no perder
semanas. La cancha RCJ está **completamente especificada por el reglamento**:
medidas, colores y posición de arcos, posición de líneas. **El mapa te lo
DAN.** Hacer SLAM es construir un mapa que ya tenés impreso — esfuerzo y CPU
tirados. Lo tuyo es el sub-problema más fácil: **localización en mapa conocido**
(secciones 3-5). Cuando alguien diga "usemos Visual SLAM", la respuesta de coach
es: "no tenemos un entorno desconocido; tenemos el mapa. Lo que querés es
localización por landmarks + fusión, y ya está a medio hacer".

> Matiz fino para una entrevista de jurado: lo que SÍ es defendible es decir
> "evaluamos Visual SLAM y lo descartamos porque el mapa es conocido; elegimos
> localización en mapa conocido (trilateración + fusión), que es la técnica
> apropiada para el problema y el hardware". Eso puntúa: muestra criterio, no
> ignorancia.

---

## 7. "Pose estimation" — el paraguas

No es una técnica: es el **nombre general** de estimar (x, y, θ). Todo lo de arriba
es pose estimation por distintos caminos. Cuando alguien lo use como término,
preguntá *cuál* de los caminos quiere y mandá a la sección que corresponde.

---

## Tabla resumen (para decidir rápido)

| Técnica | ¿Mapa conocido? | Creencia | CPU en M7 | Veredicto RCJ |
|---|---|---|---|---|
| Odometría (OTOS) | no aplica | — | trivial | ✅ base, ya está |
| Visual Odometry | no aplica | — | cara | ❌ (el OTOS ya lo hace mejor) |
| Landmarks (trilateración/triangulación) | **sí** | unimodal | barata | ✅ el frame correcto |
| MCL / partículas | sí | **multimodal** | media (100-300 part.) | ⚠️ 2027 / si el heading no es confiable |
| EKF | sí | unimodal | barata (3×3) | 🟡 destino; hoy alcanza el complementario |
| Visual SLAM | **mapa DESCONOCIDO** | — | muy alta | ❌ no tenés entorno desconocido |

**Regla de decisión para este robot:** mapa conocido + heading confiable →
**landmarks + filtro complementario/EKF**. El MCL y el VO/SLAM son para otros
problemas (o para 2027 / Middle Size League).
