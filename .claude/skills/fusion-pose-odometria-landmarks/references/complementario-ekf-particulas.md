# Filtro complementario vs EKF vs partículas — cuál, por qué, y receta mínima

> Referencia de `fusion-pose-odometria-landmarks`. Los tres filtros que podés usar
> para fusionar pose, ordenados de menos a más complejidad. Para CADA uno: la idea
> en una frase, cuándo gana, el costo real (que casi nunca es la CPU), y una receta
> mínima ejecutable en un Teensy. El veredicto de cuál para Incheon ya está en
> `localizacion-rcj-soccer`; acá está el "cómo".

Recordá el patrón único que todos comparten: **predecir con la odometría, corregir
con el landmark.** Lo que cambia es CÓMO representan la incertidumbre y CÓMO eligen
cuánto creerle a la corrección.

---

## A. Filtro complementario (ganancia fija) — el que ya tenés

**Idea.** Ganancia `K` constante. `pose = pose_predicha + K·(landmark − pose_predicha)`.
Es un EKF al que le congelaste la ganancia en vez de calcularla del ruido.

**Cuándo gana.** Cuando el ruido de los sensores es **más o menos estable** (no
cambia drásticamente entre ciclos) y querés el 80% del beneficio sin tuneo de
covarianza. **Es el caso de Incheon.**

**Costo real.** Casi nulo. Una multiplicación y una resta por eje. El único "tuneo"
es elegir K (un número), titulándolo en banco.

**Receta mínima** (esto es esencialmente lo que hace `pose_fusion.h`):

```
// Estado: x, y (mm). Heading viene aparte (no se fusiona acá).
// K en Q8: K_q8 = 26  ≈  0.10
on_odometry(dx, dy):          // del OTOS, frame cancha
    x += dx;  y += dy
on_landmark(lx, ly, valid):   // de localization.cpp
    if (!valid) return                       // gate de freshness
    if (abs(lx-x) > JUMP_MM || abs(ly-y) > JUMP_MM) return  // gate de outlier (rival tapando pared)
    x += (K_q8 * (lx - x)) >> 8
    y += (K_q8 * (ly - y)) >> 8
```

**Trampas.** (1) K alto → tiembla con el ruido del ToF. (2) Sin el gate de salto,
un outlier teletransporta la pose. (3) No fusiona heading: el heading tiene que
venir confiable de otro lado.

**Veredicto:** ✅ **Empezá acá.** Ya está escrito y testeado (`pose_fusion`). Cablealo.

---

## B. EKF de 3 estados (x, y, θ) — el "de libro"

**Idea.** Igual que el complementario, pero la ganancia `K` **se calcula sola** cada
ciclo a partir de la incertidumbre actual (covarianza `P`) y el ruido medido de
odometría (`Q`) y de la observación (`R`). Cuando estás muy seguro (P chica), le
crees poco al landmark; cuando estás perdido (P grande tras moverte mucho sin ver
nada), le crees mucho. Esa adaptación automática es la única ventaja sobre el
complementario.

**Cuándo gana.** Cuando el ruido **cambia con la situación** (a veces ves 3 ToF
buenos, a veces 1 ruidoso) y querés que el filtro ajuste su confianza solo. También
cuando querés una **medida de incertidumbre** explícita (la elipse `P`) para que la
FSM sepa "no confíes en la pose ahora".

**Costo real.** **NO es la CPU** (matrices 3×3, trivial en el M7 con FPU). Es: (1)
**tunear `Q` y `R`** — necesitás el ruido MEDIDO (`references/medir-ruido-sensores.md`);
(2) manejar la **no-linealidad del heading** (wrap ±180°, jacobianos del modelo de
observación de cada landmark); (3) **debuggear divergencia** (si Q/R están mal, la
P colapsa o explota y el filtro miente con confianza). Eso es lo que lo hace "1 día
de código, 1 semana de tuneo".

**Receta mínima (esqueleto — predict/update estándar):**

```
estado  x = [px, py, θ]ᵀ ;  covarianza P (3×3)
Q = ruido de proceso (de la deriva medida del OTOS)
R = ruido de medición (de la σ medida del landmark)

predict(Δ):                    // Δ = odometría OTOS
    x = f(x, Δ)                // suma el desplazamiento (rotado por θ)
    P = F·P·Fᵀ + Q             // F = jacobiano de f

update(z, h, H, R):            // z = observación de landmark; h(x)=predicción; H=jacobiano
    y = z − h(x)               // innovación  (¡wrap del ángulo si z es heading!)
    S = H·P·Hᵀ + R
    K = P·Hᵀ·S⁻¹               // ganancia ÓPTIMA (acá se calcula sola)
    x = x + K·y
    P = (I − K·H)·P
```

**Trampas.** Divergencia silenciosa por Q/R mal; olvidar el wrap del ángulo en la
innovación; alimentar una observación inválida (siempre gate antes del update).

**Veredicto:** 🟡 **2027.** Subí acá SOLO si en banco el complementario de ganancia
fija no alcanza (la deriva varía mucho con la situación). Para entonces ya tenés el
ruido medido y la experiencia del complementario. Decisión de diseño del repo:
diferido a 2027 (`research/in-progress/2026-05-25-localizacion-tof-imu-analisis.md`).

---

## C. Filtro de partículas / MCL — para creencia multimodal

**Idea.** En vez de UNA pose + elipse, mantenés **N hipótesis** (partículas) con
peso. Predict: movés todas con la odometría + ruido aleatorio. Update: a cada una le
calculás "¿qué tan bien explica mis mediciones?" (comparás el ToF/bearing que esa
partícula PREDICE contra el medido), repesás, y **resampleás** (las buenas se
reproducen, las malas mueren). La estimación es el promedio pesado de la nube.

**Cuándo gana.** Cuando la creencia es **multimodal / ambigua** y un gaussiano no la
puede representar:
- **Simetría de la cancha:** sin heading confiable, "estoy en (x,y) mirando al arco
  rival" y el espejo dan el MISMO patrón de paredes → dos modos. El EKF colapsa al
  promedio (un punto en el medio, equivocado); el MCL mantiene los dos hasta que un
  landmark asimétrico (el color de un arco) desempata.
- **Kidnapped robot:** te levantan y te reubican → esparcís partículas y re-converge.

**Costo real.** Proporcional a `N × costo_modelo_observación`. En un Teensy 4, ~100-300
partículas con un modelo barato (4 distancias predichas) entra a 20-50 Hz. Más caro
que A y B, pero viable. El tuneo es distinto: número de partículas, ruido de
predicción, estrategia de resampling (evitar "empobrecimiento" donde todas colapsan
a una).

**Receta mínima:**

```
init:  N partículas esparcidas (o en torno a la pose inicial)
predict(Δ):  por cada partícula:  p.pose = f(p.pose, Δ + ruido_aleatorio)
update(z):   por cada partícula:  p.w *= verosimilitud(z | p.pose)   // gaussiana sobre (z − predicción)
             normalizar pesos
             if (N_efectivo < N/2)  resample()   // sistemático/low-variance
estimación:  Σ p.w · p.pose
```

**Trampas.** Empobrecimiento de partículas (resampling agresivo → todas iguales →
no recupera); ruido de predicción mal calibrado (muy chico → no explora, muy grande
→ nube dispersa); el costo de RAM/CPU si N crece.

**Veredicto:** ⚠️ **Aprendelo (alto valor, Middle Size League / 2027), NO Incheon.**
Para este robot el camino barato a la ambigüedad de la cancha es **arreglar el
heading** (rompe la simetría) — no montar un MCL. Pero como ejercicio educativo y
como capital para 2027, es el filtro más rico para entender. Si querés un MCL mínimo
de juguete para enseñar, 100 partículas + el modelo de arriba sobre los 4 ToF corre
en el Teensy y se ve lindo en el monitor.

---

## Cómo elegir, en una decisión

```
¿el ruido es estable y la creencia es unimodal (tenés heading confiable)?
   sí → A. Complementario  (pose_fusion — ya está)
   no, el ruido varía mucho con la situación → B. EKF (2027, con ruido medido)
   no, la creencia es multimodal / ambigua / kidnapped → C. MCL (2027 / MSL)
```

Para Incheon, con heading arreglado: **A**. Todo lo demás es inversión 2027.
