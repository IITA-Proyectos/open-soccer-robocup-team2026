# Omni-3 a 120° — la matriz, su derivación y la verificación numérica

> Tabla de consulta + matemática. La verdad que MANDA es `src/shared/kinematics.cpp:6-17` (la
> implementación PURA host-testeada) + la geometría física del robot, NO la memoria. Toda fórmula
> de acá se reproduce corriendo `inverse_kinematics()` o el snippet de Python del final.

## 1. La fórmula (qué hace cada término)

Para cada rueda `i` montada en el ángulo de posición `θ_i` (desde +X, CCW) a radio `R`:

```
v_i = -vx·sin(θ_i) + vy·cos(θ_i) + ω·R
```

- **`(-sin θ_i, cos θ_i)`** = versor de **rodado** de la rueda (tangente al círculo de ruedas,
  perpendicular al radio). La rueda omni solo transmite la **proyección** (producto punto) de
  `(vx,vy)` sobre ese versor; la componente perpendicular la absorben los rodillos libres.
- **`ω·R`** = término de giro, **idéntico para las 3** porque las 3 están sobre un círculo de
  radio `R` y sus direcciones de rodado son tangentes a ese círculo → una rotación pura del
  cuerpo mueve cada superficie de rueda a velocidad tangencial `R·ω`. `R` es el **brazo de
  momento** de la rotación.

Forma matricial `v = M · [vx, vy, ω]ᵀ`, con cada fila `[-sin θ_i, cos θ_i, R]`.

## 2. La matriz REAL del robot — `θ = {330°, 210°, 90°}`, `R = 100 mm`

(`config_central.h:163,151`). Numéricamente:

| Rueda (idx) | θ | −sin θ (coef vx) | cos θ (coef vy) | R (coef ω) |
|---|---|---|---|---|
| M1 (0) del-IZQ | 330° | **+0.500** | **+0.866** | +100 |
| M2 (1) del-DER | 210° | **+0.500** | **−0.866** | +100 |
| M3 (2) trasera | 90° | **−1.000** | **0.000** | +100 |

```
        ⎡ +0.5   +0.866   100 ⎤ ⎡ vx ⎤
v = M·s = ⎢ +0.5   −0.866   100 ⎥ ⎢ vy ⎥
        ⎣ −1.0    0.000   100 ⎦ ⎣ ω  ⎦
```

`det(M) = −259.8 ≠ 0` → **no-singular** (omni real, 3 DOF controlables). Ver §5.

## 3. Verificación numérica (movimientos puros) — la firma para cazar errores de signo

| Comando | Resultado ruedas `[M1, M2, M3]` mm/s | Lectura |
|---|---|---|
| **strafe +X** (vx=200, vy=0, ω=0) | `[+100, +100, −200]` | fronts iguales 0.5·vx; **trasera al DOBLE y opuesta** (ratio 1:1:−2) |
| **avance +Y** (vy=200, vx=0, ω=0) | `[+173, −173, 0]` | fronts opuestas (±0.866·vy); trasera quieta |
| **giro CCW** (ω=1 rad/s) | `[+100, +100, +100]` | las 3 IGUALES (= R·ω). Firma del término de giro |

La **firma del strafe `1:1:−2`** (una rueda invierte mientras el par simétrico empuja igual) es
el mejor primer chequeo de banco para un error de signo: cualquier asimetría = bug de signo/eje.
Coincide con el banco del robot (ESTADO-ACTUAL: "lateral puro da M1=M2=+0.5·vx, M3=−vx").

## 4. El `+180°`: por qué `{150,30,270}` físicos viven como `{330,210,90}` en el código

Posición FÍSICA de cada rueda desde +X CCW: M1=150°, M2=30°, M3=270° (`config_central.h:155-157`).
Pero los 3 motores giran **horario visto desde el centro** a comando positivo — opuesto al rodado
CCW que asume la fórmula. Sumar **180°** a cada ángulo invierte `sin` y `cos`, lo que reescribe
el rodado al sentido físico real → `{330,210,90}`. Con eso `vx/vy` salen como **traslación** en
vez de círculos (el viejo `{60,-60,180}` estaba en eje +Y → daba CÍRCULOS).

⚠️ **El +180 NO toca el término `ω·R`** (no depende del ángulo) → la rotación queda invertida y
se corrige aparte con `OMEGA_SIGN` (ver casos-reales). **Pendiente de banco honesto:** si traslada
AL REVÉS, sacar el +180 → `{150,30,270}` (solo cambia dirección de traslación; el giro ya está
bien). Oráculo: `diag_central_strafe_robot1`.

## 5. Cinemática DIRECTA (forward) y no-singularidad

- **Forward** (de velocidades de rueda a `(vx,vy,ω)`): para 3 ruedas `M` es 3×3 y, si es de rango
  completo, **invertible** → `[vx,vy,ω]ᵀ = M⁻¹ · [v1,v2,v3]ᵀ`. Sirve para ODOMETRÍA por ruedas.
  ⚠️ **Este robot NO usa forward kinematics** (no tiene encoders en las ruedas; la odometría sale
  del OTOS — ver [[fusion-pose-odometria-landmarks]]). Se documenta por completitud teórica.
- **Pseudo-inversa** (Moore-Penrose `M⁺`): solo NECESARIA para >3 ruedas (4-wheel/mecanum), donde
  `M` es n×3 (sobre-determinada) y no hay inversa exacta. Para n=3, `M⁺ = M⁻¹`.
- **No-singularidad:** 3 ruedas omni a 120° dan 3 direcciones de rodado linealmente
  independientes → `M` de rango 3 → 3 DOF (2 traslaciones + rotación) = holónomo. Ángulos
  duplicados o un error de tipeo pueden volverla singular y la inversa explota (Modern Robotics,
  Lynch & Park §13.2).

## 6. Convenciones que DIVERGEN en la literatura (marcar, no homogeneizar)

El patrón de signos difiere entre fuentes reputadas — **no se mezclan filas de dos fuentes:**

| Fuente | Fila de la matriz | término ω | ángulos de ejemplo |
|---|---|---|---|
| ros2_control / REP-103 | `[ sin θ, −cos θ, −R ]` | NEGATIVO | +X adelante, +Y izquierda |
| **FIRGELLI (= la del robot)** | `[ −sin θ, +cos θ, +R ]` | POSITIVO | `{90,210,330}` |

➡️ **El código del robot usa la forma FIRGELLI** (`-vx·sinθ + vy·cosθ + ω·R`, `kinematics.cpp:12-14`),
con frame +X=derecha/+Y=frente/ω=CCW (`kinematics.h:3-13`). Las dos formas son internamente
consistentes; difieren en (a) qué sentido de giro de rueda es "positivo", (b) CW vs CCW de ω,
(c) dónde se pone el offset γ. **Que el robot necesite `OMEGA_SIGN=-1` es exactamente el síntoma
de que su convención (FIRGELLI, ω positivo) NO coincide con el sentido físico real de sus motores
→ se reconcilia en un solo lugar (el mixer), no mezclando convenciones.**

El mismo set de ángulos se parametriza de muchas formas (`[0,120,240]`, `[30,150,270]`,
`[90,210,330]`, `[330,210,90]`…): **NO es contradicción, es elección de frame.** Lo que IMPORTA
es que el offset del primer ángulo coincida con el ángulo FÍSICO al que está atornillada la rueda
#1 y con el +X asumido. Copiar números entre papers sin re-derivar el offset = bug silencioso.

## 7. Snippet de verificación (reproducible, sin libm en el target)

```python
import math
ang = [330.0, 210.0, 90.0]; R = 100.0      # config_central.h:163,151
def wheels(vx, vy, w):
    return [(-vx*math.sin(math.radians(t)) + vy*math.cos(math.radians(t)) + w*R) for t in ang]
print(wheels(200,0,0))   # strafe +X -> [100, 100, -200]   (firma 1:1:-2)
print(wheels(0,200,0))   # avance +Y -> [173.2, -173.2, 0]
print(wheels(0,0,1.0))   # giro      -> [100, 100, 100]
```

En el firmware la `cos/sin` se evalúa con `std::sin/std::cos` (float) en `kinematics.cpp`; la
trilateración usa una LUT Q12 entera aparte (`localization.cpp`) para ser idéntica host/target —
son módulos distintos, no confundir.
