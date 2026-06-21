# Omni-3 — casos reales del robot IITA (evidencia + lecciones)

> Anclaje de lo VIVIDO en banco para que el árbol DERIVE la causa en vez de asumirla. Cada caso
> apunta a `archivo:línea` real y al diag oráculo. La verdad MANDA = el código + el banco.

## Caso A — los CÍRCULOS de `{60,-60,180}` (eje equivocado)

**Síntoma (banco 2026-06-03/08):** con `inverse_kinematics({60,-60,180})` el robot, al pedir
movimiento, **daba círculos** en vez de trasladar; además solo el motor 1 parecía responder a
`vx=150` y subdimensionaba el par.

**Causa raíz:** los ángulos `{60,-60,180}` estaban expresados respecto al eje **+Y**, pero la
fórmula `-vx·sinθ + vy·cosθ` asume ángulos desde **+X** (`kinematics.h:3-9`). Frame equivocado →
la combinación de proyecciones rotaba el vector en vez de trasladarlo.

**Fix (`config_central.h:153-163`):** calibrar los ángulos a la disposición física real
`{330,210,90}` (= `{150,30,270}` físicos + 180). Un lateral puro pasó a dar la firma correcta
`[+0.5,+0.5,−1]·vx` (M1=M2 mismo lado, trasera la que más empuja).

**Lección:** "da círculos al pedir traslación" = ángulos en el EJE equivocado, casi nunca "el
robot está roto". Verificá el patrón de ruedas de un movimiento PURO antes de tocar nada.

## Caso B — `OMEGA_SIGN`: la realimentación POSITIVA del lazo de rumbo (la trampa madre)

**Síntoma (banco robot2 2026-06-09):** con el gyro-hold ACTIVO, el heading no se corregía sino
que **se ESCAPABA**: `hdg −3.7° → −71.5°`, clavándose justo en el bail-out de seguridad de 45°.
Pasaba en ambos robots y retroactivamente explica parte de los "círculos" históricos de R1.

**Causa raíz (`config_central.h:165-174`, `motors_zircon.cpp:195-201`):** el `+180` de los
ángulos (Caso A) corrigió la TRASLACIÓN (cambia `sin/cos`) pero el término `ω·R` **no depende del
ángulo** → quedó sin corregir. Con motores que giran horario-desde-el-centro a comando positivo,
una `ω` positiva (CCW) del cerebro producía rotación física OPUESTA → el PID de rumbo, al ver el
error, comandaba ω en el sentido que lo AGRANDABA. **Realimentación positiva pura.**

**Fix:** `OMEGA_SIGN = -1` (`config_central.h:175`), aplicado en el mixer
(`motors_zircon.cpp:199`: `omega_rad_s = OMEGA_SIGN · cmd.omega · …`). Aplica a AMBOS robots
(los 3 motores horario validados en banco).

**Lección transferible (clave para 2027):** la **firma** de un lazo cerrado sobre el signo
equivocado de actuación es **error MONÓTONO creciente hasta el clamp**, NO oscilación. Si ves eso,
NO toques las ganancias — flip el signo. Traslación y rotación se validan POR SEPARADO: un robot
que traslada perfecto puede girar invertido.

## Caso C — el strafe 2:1 y por qué PWM ≠ velocidad

**Hecho cinemático:** en strafe puro la trasera debe ir al **doble de velocidad** que las
delanteras (matriz: fronts `0.5·vx`, rear `1.0·vx`). **Trampa:** "entonces mando 2× el PWM a la
trasera". En banco (R2 2026-06-09, `diag_central_strafe_robot2_kick`) la trasera SOSTIENE esa
relación 2:1 de velocidad con solo **~1.5× el PWM** (107 vs 70), porque va **alineada** al strafe
(menos fricción que las delanteras oblicuas a 60°).

**Por qué (web + planta):** el PWM no es proporcional a la velocidad — hay zona muerta (deadband,
hay que vencer fricción estática), back-EMF y carga distinta por rueda. La cinemática da la
velocidad de rueda IDEAL; cuánto PWM la logra es **planta medida** → [[dinamica-omni-3-ruedas]].

**Lección:** no traduzcas relaciones de la matriz directo a relaciones de PWM. La matriz vive en
velocidad; el PWM vive en la planta. Confundirlos hace tunear la rueda equivocada.

## Caso D — saturación: escalar el vector, NO clampear por rueda

**Síntoma (banco 2026-06-09):** el arquero, en patrulla lateral con corrección de rumbo, "perdía
el frente" — la corrección fina de gyro se diluía.

**Causa raíz:** el clamp **por rueda** (recortar cada PWM a su tope por separado) **rota el vector
de movimiento** → cambia la trayectoria y la velocidad relativa entre ruedas, comiéndose las
correcciones chicas.

**Fix (`motors_zircon.cpp:208-234`, `-DCENTRAL_FLOOR_SCALE`, `kinematics.cpp:27-39`
`saturate_wheels`):** escalar las 3 ruedas por **un factor común** `λ = v_max/v_mayor` → preserva
la DIRECCIÓN (la cinemática es lineal: escalar `v` = escalar el comando `s`), solo baja la
rapidez. La corrección fina sobrevive.

**Lección:** ante saturación, escalar (preserva trayectoria) NUNCA clampear por rueda (la
distorsiona). Como `w = M·s` es lineal, `λ·w` equivale a `λ·s` → mismo heading, menos velocidad.

## Archivos reales (punteros)

- `src/shared/kinematics.cpp:6-17` — `inverse_kinematics` (la fórmula); `:19-25` `wheel_speed_to_pwm`;
  `:27-39` `saturate_wheels` (escalado proporcional); `:41-51` `apply_pwm_floor` (piso/deadzone).
- `src/shared/kinematics.h:3-14` — convención de frame + fórmula textual; `:21-28` structs.
- `src/central/config_central.h:151` `WHEEL_RADIUS_MM=100`; `:163` `WHEEL_ANGLES_DEG={330,210,90}`;
  `:175` `OMEGA_SIGN=-1`; `:180` `MAX_PWM=255`; `:66/103` `MOTOR_MIN_PWM` por rueda (planta).
- `src/central/motors_zircon.cpp:32-36` arma `WHEELS[]` desde los ángulos; `:199-206` aplica
  `OMEGA_SIGN`, llama `inverse_kinematics` + `saturate_wheels`; `:94-115` PWM→motor con `MOTOR_INVERT`.
- `src/shared/gk_motion_speed.h:29` — derivación de la geometría del strafe con `{330,210,90}`.
- `src/shared/central_config.h:26-27` — nota: la cinemática/`OMEGA_SIGN` NO son calibrables por
  EEPROM (son geometría, no tuning).
- Diags oráculo: `diag_central_strafe(_robot1/2)` (strafe open-loop, ω=0),
  `diag_central_arbitro_strafe` (patrulla del arquero), `diag_central_drive_straight` (avance +Y
  con PID de rumbo — valida la cadena cinemática end-to-end).
