---
name: cinematica-omni-3-120
description: Usar cuando hay que entender, derivar, escribir o DEPURAR la cinemática de un robot omnidireccional de 3 ruedas a 120° — la matriz inversa (vx,vy,ω → 3 ruedas), las convenciones de signo/ejes, el término de giro ω·R, la saturación, o por qué el robot "da círculos en vez de trasladar", "traslada al revés", o "todo PID de rumbo amplifica el error en vez de corregirlo". Es la TEORÍA/matemática del omni-3; remite a la planta MEDIDA y al lazo para tuning. Triggers - "cinemática omni / inverse kinematics", "3 ruedas a 120°", "la matriz de las ruedas", "WHEEL_ANGLES / ángulos de rueda", "descomponer vx/vy/ω", "el robot da círculos / no traslada", "traslada para el lado equivocado", "el giro sale invertido / OMEGA_SIGN", "ω·R / término de rotación", "saturar las ruedas / clamp por rueda vs escalar", "convención +X/+Y, CW/CCW del robot", "convertir velocidad de rueda a PWM". NO es para tunear ganancias del lazo (control-pid-zona-muerta), NO es la planta medida —pisos PWM, deriva, regímenes— (dinamica-omni-3-ruedas), NO es el heading del IMU (bno055-imu-heading-robocup).
---

# Cinemática omni-3 a 120° — la matriz, los signos y sus trampas

## Principio central — la cinemática es geometría EXACTA; el desastre vive en los SIGNOS

La parte numérica de un omni-3 es trivial y cerrada: cada rueda solo "siente" la
**proyección** de la velocidad del cuerpo sobre su dirección de rodado, más un término
de giro igual para todas (`ω·R`). No hay nada que tunear ahí. La frase ancla:

> **Una matriz de cinemática puede estar PERFECTA en su aritmética y aun así mandar el robot
> en círculos, al revés, o haciendo que el control de rumbo amplifique el error — porque el bug
> NO está en la fórmula sino en una CONVENCIÓN DE SIGNO/EJE (qué es +X, qué giro es +, en qué
> marco están los ángulos). Verificá CADA signo a mano en banco ANTES de cerrar cualquier lazo.**

Modelo mental: separá SIEMPRE **(1) la fórmula** (proyección + ω·R, idéntica para todos),
**(2) las 3 convenciones** (ejes del cuerpo, sentido de ω, marco de los ángulos de rueda) y
**(3) la planta física** (qué PWM hace girar cada rueda y cuánto — eso NO es cinemática, es
[[dinamica-omni-3-ruedas]]). Casi todo "el robot se mueve mal" de un omni que ya camina es
capa (2), no (1) ni (3).

Regla dura: **la traslación y la rotación tienen convenciones INDEPENDIENTES.** Arreglar una
NO arregla la otra. En este robot eso costó días (ver árbol y casos reales): el `+180°` de los
ángulos corrige la TRASLACIÓN pero deja la ROTACIÓN invertida, que se tapa aparte con
`OMEGA_SIGN`.

## Cuándo usar / cuándo NO

USAR: derivar/leer/escribir la matriz inversa de un omni-3 a 120°; entender la descomposición
vx/vy/ω; elegir/depurar convenciones de signo y ejes; decidir cómo saturar; mapear "rueda i →
motor físico → ángulo geométrico"; diagnosticar círculos / traslación invertida / realimentación
positiva de rumbo.

NO usar (rutear):
- **Pisos de PWM, deriva parásita, regímenes de velocidad, mínimos físicos MEDIDOS** →
  [[dinamica-omni-3-ruedas]] (la planta del robot, NO la teoría).
- **Tunear el lazo** (heading-hold PFM/deadband/anti-windup, ganancias) →
  [[control-pid-zona-muerta]]; realización en tiempo real (discretización, dt) →
  [[control-embebido-tiempo-real]].
- **Heading del IMU** (de dónde sale ω/θ) → [[bno055-imu-heading-robocup]].
- **Fusión de pose XY** → [[fusion-pose-odometria-landmarks]].

Esta skill termina en "tenés velocidades de rueda correctas"; el PWM→velocidad real y el tuning
del lazo son de las otras.

## La fórmula (lo que MANDA es el código, no la memoria)

Ground-truth: `src/shared/kinematics.cpp:12-14` (función PURA, host-testeada — `test_kinematics`,
11 tests). Para cada rueda `i` con ángulo `θ_i` (posición desde +X, CCW) y radio `R`:

```
v_i = -vx·sin(θ_i) + vy·cos(θ_i) + ω·R
```

- `(-sin θ_i, cos θ_i)` es el versor de **rodado** de la rueda (perpendicular a su radio): la
  rueda omni solo transmite la componente de (vx,vy) en ESA dirección; la perpendicular la
  absorben los rodillos libres. Por eso es una **proyección**.
- `ω·R` es idéntico para las 3: una rotación pura del cuerpo hace que las 3 ruedas rueden
  tangencialmente a la misma velocidad lineal.
- Convención del frame (cabecera `kinematics.h:1-14`): **+X = derecha**, **+Y = frente**,
  **ω = + es CCW** (antihorario visto desde arriba).

La matriz completa con los ángulos REALES del robot `{330,210,90}`, su derivación, la cinemática
DIRECTA (inversa de la matriz) y la verificación numérica → [references/cinematica-matriz-y-derivacion.md](references/cinematica-matriz-y-derivacion.md).

## Los ángulos REALES del robot (config_central.h) y el `+180` a verificar

`src/central/config_central.h:163` → `WHEEL_ANGLES_DEG = {330, 210, 90}`,
`:151` → `WHEEL_RADIUS_MM = 100`. El índice alinea
`Motor_i ↔ MOTOR_PINS[i] ↔ WHEEL_ANGLES_DEG[i] ↔ posición física`
(`motors_zircon.cpp:29-36`):

| idx | Motor | Posición física | θ físico | θ en el código | rol |
|---|---|---|---|---|---|
| 0 | M1 / U5 | delantera IZQUIERDA | 150° | **330°** | proyección strafe +0.5·vx |
| 1 | M2 / U17 | delantera DERECHA | 30° | **210°** | proyección strafe +0.5·vx |
| 2 | M3 / U7 | TRASERA | 270° | **90°** | la que MÁS empuja en strafe (−1.0·vx) |

⚠️ **El `+180°` (de `{150,30,270}` físicos a `{330,210,90}`) NO es un error: es una corrección
de convención** (`config_central.h:158-162`). Los 3 motores giran **horario visto desde el
centro** (= comando positivo), opuesto a la fórmula que asume rodado CCW → sumar 180° a cada
ángulo invierte `sin/cos` y convierte vx/vy en **traslación** en vez de círculos. El viejo
`{60,-60,180}` estaba expresado en eje +Y (la fórmula usa +X) → **daba CÍRCULOS** (superado).
**Pendiente de banco (honesto): si traslada AL REVÉS, sacar el +180 → `{150,30,270}`** (el giro
ya queda bien igual; esto es solo dirección de traslación). Ver `diag_central_strafe_robot1`.

## La trampa madre: `OMEGA_SIGN` (el +180 NO toca la rotación)

`src/central/config_central.h:175` → `OMEGA_SIGN = -1`, aplicado en
`src/central/motors_zircon.cpp:199-201`.

El `+180` de arriba arregla la traslación porque cambia `sin/cos`. **Pero el término `ω·R` NO
depende del ángulo** → quedó SIN corregir. Con motores horario-desde-el-centro, una `ω` positiva
(CCW) del cerebro producía rotación física OPUESTA. Consecuencia documentada
(`config_central.h:165-174`): **todo PID de rumbo AMPLIFICABA el error** (realimentación
positiva) — banco: `hdg −3.7 → −71.5` con gyro-hold activo, clavándose justo en el bail-out de
45° (la firma del lazo invertido). Fix: el mixer invierte ω (`omega_rad_s = OMEGA_SIGN · …`).
Aplica a AMBOS robots.

**Lección transferible:** en un omni, traslación y rotación se validan por SEPARADO. Un robot
que traslada perfecto puede girar invertido, y un lazo de rumbo cerrado sobre ω invertido no
"oscila por mal tuneo": **diverge monótono hasta el clamp**. Esa firma (error que crece derecho
y se clava en el límite) = signo de ω, NO ganancias.

## Saturación: escalar el VECTOR, no clampear por rueda

`src/shared/kinematics.cpp:27-39` → `saturate_wheels()`. Si alguna rueda excede el máximo,
**escala las 3 por el mismo factor** → preserva la DIRECCIÓN del movimiento, solo baja la
rapidez alcanzable. El clamp por-rueda (recortar cada una a su tope) **rota el vector** y
arruina la trayectoria — peor aún, **se come las correcciones finas de rumbo** a velocidad de
patrulla (banco 2026-06-09: el arquero "perdía el frente" con clamp por-rueda → se migró a
escalado proporcional, `motors_zircon.cpp:208-234`, `-DCENTRAL_FLOOR_SCALE`).

⚠️ **El piso de PWM (deadzone) NO es saturación.** `apply_pwm_floor()` (`kinematics.cpp:41-51`)
ELEVA todo PWM no nulo por debajo de `MOTOR_MIN_PWM[i]` hasta ese piso — para sacar la rueda de
la zona muerta del motor. Es POR RUEDA y POR ROBOT (`config_central.h:66/103`) y pertenece a la
PLANTA → su tuning vive en [[dinamica-omni-3-ruedas]] / [[control-pid-zona-muerta]], no acá.

## De velocidad de rueda a PWM (NO es lineal)

`wheel_speed_to_pwm()` (`kinematics.cpp:19-25`) hace un mapeo **lineal** `speed→PWM` con
saturación a `MAX_PWM=255` (`config_central.h:180`, `MAX_SPEED_MM_S=1000`). ⚠️ **Eso es una
aproximación de diseño, no la física.** El PWM real NO es proporcional a la velocidad y es
DISTINTO por rueda (las oblicuas a 60° ven más fricción que la trasera alineada). La cinemática
te da la velocidad de rueda IDEAL; cuánto PWM hace falta para LOGRARLA es planta medida →
[[dinamica-omni-3-ruedas]]. No confundas "la matriz pide rear = 2× front" con "mandá rear con 2×
el PWM": en el strafe la trasera lo logra con ~1.5× el PWM (107 vs 70) por ir alineada.

## Árbol de diagnóstico — "el omni se mueve mal" (ya camina, pero feo)

Orientado a un robot que YA gira ruedas (si NO gira ninguna → es planta/driver, no cinemática).

- **FASE 0 — ¿qué movimiento falla, traslación o rotación?** Probalos SEPARADOS con un diag
  open-loop (`diag_central_strafe`, `diag_central_drive_straight`): comando vx puro / vy puro /
  ω puro, ω y vy en cero respectivamente. **Traslación y rotación tienen bugs independientes.**

- **FASE 1 — "da círculos cuando pido traslación":** los ángulos están en el EJE equivocado
  (vx/vy mapeados como si +X fuera +Y) — el caso `{60,-60,180}` histórico. Verificación: con
  vx>0, vy=0, ω=0 las 3 ruedas deben dar el patrón de strafe (en este robot `[+0.5, +0.5, −1.0]·vx`,
  ver matriz). Si en cambio dan algo simétrico que rota → ángulos mal puestos.

- **FASE 2 — "traslada pero PARA EL LADO EQUIVOCADO":** signo global de traslación = el `+180`.
  Sacar/poner el +180 a los 3 ángulos (`{330,210,90} ↔ {150,30,270}`). NO toca la rotación.

- **FASE 3 — "el giro sale invertido" o "el lazo de rumbo se va clavando al límite":** signo de
  `ω` (capa rotación, independiente). Flip `OMEGA_SIGN`. La firma del lazo invertido es error
  MONÓTONO creciente hasta el clamp (no oscilación) — no lo confundas con mal tuneo del PID.

- **FASE 4 — "una rueda no alcanza / la trayectoria se tuerce a alta velocidad":** saturación.
  ¿Estás clampeando por rueda en vez de escalar el vector? → `saturate_wheels` proporcional.

- **FASE 5 — "se mueve a alta velocidad pero no arranca lento / raspa":** zona muerta del motor
  → NO es cinemática, es piso de PWM / planta. Rutear a [[dinamica-omni-3-ruedas]].

- **FASE 6 — gate de verificación:** confirmá A MANO en banco, open-loop, los 3 movimientos
  puros (vx, vy, ω) en el sentido correcto **antes** de cerrar cualquier lazo. Que compile / que
  la matriz "esté bien" / que el host-test pase NO prueba el signo físico.

## Errores comunes

| Síntoma | Causa raíz real | Trampa (lo que parece) | Fix + verificación |
|---|---|---|---|
| da círculos al pedir strafe/avance | ángulos en el eje equivocado (+Y vs +X), `{60,-60,180}` | "cinemática mal / robot roto" | ángulos de POSICIÓN desde +X CCW; verificar patrón strafe `[+0.5,+0.5,−1]·vx` |
| traslada al revés (izq↔der / frente↔atrás) | signo global de traslación = el `+180` de los 3 ángulos | "motor invertido" | togglear +180 (`{330,210,90}↔{150,30,270}`); NO toca el giro |
| el giro físico es opuesto al comando ω | `ω·R` no se corrige con el +180 (no depende del ángulo) | "PID mal tuneado" | `OMEGA_SIGN=-1` en el mixer; validar giro puro a mano |
| el lazo de rumbo DIVERGE monótono hasta el clamp (45°) | realimentación positiva por ω invertida | "ganancias altas / oscila" | flip `OMEGA_SIGN`; la firma es error creciente, no oscilación |
| la trayectoria se tuerce al saturar | clamp POR RUEDA (rota el vector) | "una rueda floja" | `saturate_wheels` escala las 3 igual (preserva dirección) |
| el arquero "pierde el frente" en patrulla | clamp por-rueda se come las correcciones finas | "el gyro no corrige" | escalado proporcional (`-DCENTRAL_FLOOR_SCALE`) |
| "la matriz pide rear 2× → mando 2× PWM" y rota | PWM≠velocidad y es distinto por rueda | "cinemática mal" | la trasera alineada logra 2× velocidad con ~1.5× PWM → planta, no matriz |
| no arranca lento / raspa pero a full anda | zona muerta del motor (piso de PWM) | "cinemática débil" | piso por rueda — es PLANTA → [[dinamica-omni-3-ruedas]] |

**Anti-racionalizaciones:** "la matriz está bien, el problema es otro" → en un omni que ya camina,
el 90% de "se mueve mal" es un SIGNO/convención, no la aritmética. "subo/bajo ganancias y no
arregla" → si el error DIVERGE hasta el límite, es signo de ω, ninguna ganancia lo arregla.
"compila y el host-test pasa" → el host-test valida la fórmula, NO el signo físico de tu robot
(verificá a mano en banco). "mando más PWM a la rueda que pide más velocidad" → PWM≠velocidad,
es planta.

## Skills relacionadas

- **Planta MEDIDA** (pisos PWM, deriva parásita, regímenes, mínimos físicos, el strafe 2:1 real)
  → [[dinamica-omni-3-ruedas]]. **Par obligatorio:** esta skill da la velocidad de rueda IDEAL;
  esa da cuánto PWM la logra.
- **Tunear el lazo** (heading-hold, PFM, deadband, anti-windup) → [[control-pid-zona-muerta]];
  realización discreta/tiempo real → [[control-embebido-tiempo-real]].
- **Heading/ω del IMU** → [[bno055-imu-heading-robocup]]; **fusión de pose XY** →
  [[fusion-pose-odometria-landmarks]].
- Método de debug genérico → `superpowers:systematic-debugging`; verificar antes de cerrar →
  `superpowers:verification-before-completion`. Test en banco → [[hardware-test-protocol]];
  documentar → [[engineering-journal]].

## Referencias (no inflar el inline)

- [references/cinematica-matriz-y-derivacion.md](references/cinematica-matriz-y-derivacion.md) —
  la matriz 3×3 completa con `{330,210,90}`, derivación de la proyección, verificación numérica
  (strafe/avance/giro), cinemática DIRECTA (forward) y por qué 3 ruedas a 120° es no-singular.
- [references/casos-reales-omni.md](references/casos-reales-omni.md) — los casos ancla del robot
  (los CÍRCULOS de `{60,-60,180}`, la realimentación positiva de `OMEGA_SIGN`, el strafe 2:1) con
  punteros a `archivo:línea` y a los diags oráculo.
