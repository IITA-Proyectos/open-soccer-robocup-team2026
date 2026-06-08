<!-- AUTO-GENERADO por el workflow analisis-motion-control (wf_39266dd8-484) el 2026-06-08. Analisis de 22/23 programas (test-circulo.ino no se pudo analizar -> revisar a mano). Valores citados = del codigo real con archivo:linea. Editable a mano. -->

# Control de movimiento — ESTRATEGIAS Y VALORES HISTORICOS (robots 2025 + sketches de prueba)

> **Fecha:** 2026-06-08
> **Cómo se generó:** análisis automático de código **deprecado 2025** (`software/_deprecated-2025/`) + sketches de prueba en `software/staging/shared/` (STAGING CONGELADO, NO flashear). Todos los valores de PWM, milisegundos y ganancias citados acá **salieron literalmente del código** (con referencia `archivo:línea`). Lo que el código no tenía, se marca como **"no encontrado"** y no se inventa.
> **Para qué sirve:** es un catálogo de "qué técnicas usaban los robots del 2025 y con qué números", para decidir cuáles portar al firmware 2026 sin reinventar desde cero. NO es código vivo ni canónico.

---

## Glosario rápido (para no perderse)

- **KIWI / 3 ruedas omni:** robot con 3 ruedas a 120° entre sí. En el delantero 2025 el reparto era M1 y M2 **delanteras-laterales** y M3 **trasera, paralela al avance**.
- **PWM:** "potencia" del motor, de 0 a 255 (8 bits). En el código a veces está escalada (ej. `100*g` con `g=0.3` = PWM 30).
- **Coast (rueda libre):** apagar el motor poniendo `PWM=0` y los dos pines de dirección en bajo (`INA=INB=0`). El motor se frena solo por rozamiento.
- **Short-brake (freno activo):** poner `INA=INB=1` para cortocircuitar el motor y frenar fuerte. **Ningún programa del 2025 lo usaba.**
- **Plugging / freno por reversa:** frenar metiéndole reversa breve al motor. Sí se usaba.
- **Deadzone / piso de PWM:** mínimo de PWM por debajo del cual el motor no arranca (zumba pero no gira). Compensarlo = forzar ese mínimo.
- **Heading / PID de rumbo:** mantener la orientación del robot fija leyendo el giroscopio BNO055 y corrigiendo con un lazo P / PD / PID.

---

## 1) Impulso inicial anti-inercia

**Qué es:** un pulso de PWM alto y breve para "romper" el rozamiento estático al arrancar un giro o un movimiento, que después baja al PWM de régimen (más bajo). Sustituye a una rampa de aceleración.

| Programa | Valor(es) reales | Nota |
|---|---|---|
| `definitivo-delantero.cpp` | Giro: **PWM 150 × 70 ms** en las 3 ruedas → baja a `100*g=30`. Arranque arquero: **factor 1.8 → M1=M2=90, M3=153 × 40 ms** → baja a `pd*50=50 / pd*89=89`. Re-impulso arquero al cambiar de dirección: **350 ms**. Órbita: `ic=0.55` → M1=M2=33, M3=99, **× 500 ms (antihorario) / 300 ms (horario)** → baja a `c=0.4` (24 / 72). | Múltiples impulsos discretos en vez de rampa. (`:395-399`, `:1034-1038`, `:674-678`, `:744-749`) |
| `delantero-sin-zirconLib.cpp` | Giro: **PWM 150 × 70 ms** → `100*g=30`. Órbita (cambio de sentido): `60*ic / 180*ic` **× 500 ms (antihorario) / 300 ms (horario)**. Además `AVANCE_INICIO` ejecuta `avanzar_patear()` (rampa hasta 240) **× 700 ms** al boot. | (`:425-429`, `:704-708`, `:775-779`) |
| `test-4-movimientos.ino` | **No encontrado** — el PWM arranca directo en su objetivo (150 adelante/atrás; 55/55/100 lateral), sin pulso. | — |
| `test-motores-lateral-simple.ino` | **No encontrado** — arranca directo a VEL base (55/55/100). La asimetría de este sketch está solo en el FRENADO. | — |
| `test-movimiento-omnidireccional.ino` | **No encontrado** — aplica directo el PWM de la cinemática. | — |
| `test-gyro-movimiento-basico.ino` | **No encontrado** — arranca directo en `VELOCIDAD_BASE=150`. | — |
| `test-gyro-movimiento-lateral.ino` | **No encontrado** — aplica directo el PWM de régimen. | — |
| `test-bno055-imuplus.ino` | **No aplica** — no controla motores. | — |
| `zirconLib.cpp` / `.h` | **No encontrado** — HAL crudo, `motorN()` escribe el PWM directo sin pico. | — |

**Lectura:** el impulso anti-inercia es una técnica **exclusiva de los robots de competencia 2025** (delantero). El número estrella es **150 × 70 ms para giro** y **factor 1.8 × 40 ms** para arranque del arquero. Ningún sketch de prueba lo implementa.

---

## 2) Delay de arranque de la rueda trasera

**Qué es:** arrancar la rueda trasera (M3, la paralela al avance) un rato **después** que las delanteras, para que no empuje de más al inicio.

| Programa | Valor(es) reales | Nota |
|---|---|---|
| **Todos** | **No encontrado en ningún programa.** | Ningún código del 2025 retrasa temporalmente el arranque de M3. |

**Aclaración importante (no confundir):** en avance recto, el delantero 2025 y los tests de avance/atrás **apagan** la trasera (`PWM3=0`) **todo el tiempo** — pero eso es **apagado permanente**, NO un delay temporizado. En los movimientos donde M3 sí participa (órbita, lateral), arranca **simultáneamente** con las laterales. El "delay de arranque de la trasera" como técnica **no existe en el histórico**.

---

## 3) Freno anticipado de la rueda trasera

**Qué es:** apagar la rueda trasera M3 unos milisegundos **antes** de terminar un movimiento lateral, para que su inercia no desvíe el rumbo al final. Es la técnica más sofisticada del histórico.

| Programa | Valor(es) reales | Nota |
|---|---|---|
| `test-4-movimientos.ino` | **`BASE_ANTICIPACION_MS=60` ms nominal**, escalado `× VEL_M3/100` → con VEL_M3=100 da **60 ms**. Rango 0–300 ms. Auto-cal por gyro paso **5 ms**; manual ±10 ms por Serial (`+`/`-`). Umbral de drift **1.5°**. Aplica SOLO a laterales. | M3 frena en `tiempoTranscurrido >= 3000 - anticipacionReal`. (`:115-120`, `:198-200`, `:519-521`, `:847/:920`) |
| `test-motores-lateral-simple.ino` | **Idéntico: `BASE_ANTICIPACION_MS=60` ms**, dinámico `× VEL_M3/100`, rango 0–300, auto-cal ±5 ms si drift > **1.5°**. | Núcleo del sketch (v3/v4). (`:100`, `:108-111`, `:155-157`, `:413-415`, `:632`, `:486-506`) |
| `definitivo-delantero.cpp` | **No encontrado.** `parar()` apaga las 3 ruedas a la vez. | — |
| `delantero-sin-zirconLib.cpp` | **No encontrado.** | — |
| `test-movimiento-omnidireccional.ino` | **No encontrado.** `parar()` apaga las 3 a la vez. | — |
| `test-gyro-movimiento-basico.ino` | **No aplica** — M3 siempre apagada. | — |
| `test-gyro-movimiento-lateral.ino` | **No encontrado** — apaga las 3 simultáneas. | — |
| `test-bno055-imuplus.ino` / `zirconLib` | **No aplica.** | — |

**Lectura:** técnica **exclusiva de los dos sketches de prueba lateral** (`test-4-movimientos` y `test-motores-lateral-simple`), ambos con el mismo número: **60 ms a VEL_M3=100, auto-calibrable por giroscopio** (mide el drift del heading tras frenar y ajusta ±5 ms para minimizarlo). Es lo más interesante para portar al strafe del 2026.

---

## 4) Freno por reversa / electromagnético (plugging)

**Qué es:** frenar metiéndole reversa breve al motor en vez de dejarlo en coast. Frena mucho más rápido. El "electromagnético" (short-brake `INA=INB=1`) **no se usó nunca**.

| Programa | Valor(es) reales | Nota |
|---|---|---|
| `definitivo-delantero.cpp` | **Reversa tras patear:** `retroceder_patear()` → **M1=250, M2=170** (asimétrico), M3=0. Duración: PATEANDO_atras **200 ms**, PATEANDO_corto_atras **300 ms**. Escape de línea: `retroceder1/2/3` combinaciones de 100/0, **400 ms**. Arquero: hasta detectar línea (sin tiempo fijo). | `patadM1=250 / patadM2=170` iguales en ROBOT1 y ROBOT2. (`:205-207`, `:30-31`, `:967-969`, `:878-880`) |
| `delantero-sin-zirconLib.cpp` | **Igual:** `retroceder_patear()` **M1=250, M2=170**, M3=0. Pateo corto-atrás **300 ms**, pateo largo-atrás **200 ms**. Escape de línea **400 ms** a PWM 100. | (`:220-222`, `:33-34`, `:908-915`, `:997-1004`) |
| Todos los `test-*.ino` | **No encontrado** — frenan por coast, nunca invierten el motor. | El único "freno" de los tests laterales es apagar M3 antes (coast anticipado), no reversa. |
| `zirconLib.cpp/.h` | **No encontrado** — la librería puede poner reversa (`direction` bool) pero no implementa ninguna secuencia de freno. | — |

**Lectura:** el plugging es **exclusivo de los robots de competencia 2025**. Número clave: **reversa asimétrica M1=250 / M2=170 durante 200–300 ms** (la asimetría compensa diferencias mecánicas entre las dos ruedas delanteras). **Short-brake activo (`INA=INB=1`): no encontrado en ningún programa.**

---

## 5) Deadzone / piso de PWM mínimo

**Qué es:** un mínimo de PWM forzado para que el motor arranque (vencer la zona muerta). Lo opuesto a un techo/clamp superior.

| Programa | Valor(es) reales | Nota |
|---|---|---|
| **Todos** | **No encontrado en ningún programa.** | Ningún código del 2025 implementa un piso/deadzone de arranque. |

**Detalle de lo que SÍ había (techos, no pisos):**
- `zirconLib.cpp`: solo **techo** `motorLimit=100` (`power = min(power, motorLimit)`, `:9`, `:174/:195/:215`). Cualquier power bajo se manda tal cual.
- Tests: solo `constrain(abs(vel), 0, 255)` y techos `MOTOR_MAX=200`. El límite inferior siempre es **0**.
- Delantero: los PWM más bajos usados (`100*g=30`, `60*c=24`) salen de factores de escala (g, a, c, ic, pd), **no** de una constante de deadzone.

**Lectura:** **el manejo de zona muerta nunca se implementó**. Es un gap conocido: con correcciones PID grandes, una rueda puede caer hasta PWM 0 y "morir" (ej. `150-80=70`, o `150 - correccion` si la corrección supera 150).

---

## 6) Rampas de aceleración (slew)

**Qué es:** subir/bajar el PWM gradualmente en vez de saltar de golpe, para no patinar ni golpear la mecánica.

| Programa | Valor(es) reales | Nota |
|---|---|---|
| `definitivo-delantero.cpp` | **Solo en el pateo:** `avanzar_patear()` rampa **0 → 240** en pasos de **5 cada 20 ms** (~960 ms al tope). Sin rampa en `avanzar()`, `girar()` ni movimientos del arquero. | No hay rampa de DESaceleración (la frenada es por reversa/coast). (`:181-200`, `:70-73`) |
| `delantero-sin-zirconLib.cpp` | **Solo en el "pateo"/empuje:** `avanzar_patear()` rampa **0 → 240**, paso **5 cada 20 ms**. Resto sin rampa. | (`:196-216`, `:76-80`) |
| `test-4-movimientos.ino` | **No encontrado** — PWM salta directo. La única suavidad es el PID de heading (no controla velocidad de avance) y el coast anticipado de M3 (que no es rampa). | — |
| `test-motores-lateral-simple.ino` | **No encontrado** — salta directo. | — |
| `test-movimiento-omnidireccional.ino` | **No encontrado** — la única "escala" es saturación proporcional instantánea (limita magnitud, no rampa). | — |
| `test-gyro-movimiento-basico.ino` | **No encontrado** — salta de 0 a 150 y de 150 a 0. | — |
| `test-gyro-movimiento-lateral.ino` | **No encontrado** — step instantáneo. | — |
| `zirconLib` / `test-bno055` | **No encontrado / no aplica.** | — |

**Lectura:** la única rampa real del histórico es la **rampa de pateo 0→240 (paso 5 / 20 ms)** de los dos delanteros. **No existe rampa de desaceleración en ningún lado.**

---

## 7) Freno general (coast / short-brake / reversa)

**Qué es:** cómo detiene el robot. Tres opciones posibles: **coast** (libre), **short-brake** (`INA=INB=1`, freno activo) o **reversa** (plugging).

| Programa | Tipo de freno | Nota |
|---|---|---|
| `definitivo-delantero.cpp` | **coast + reversa (plugging)** | `parar()` = coast puro (`PWM=0`, `INA=INB=0`). Frenado rápido real = reversa (`retroceder_patear` 250/170) + pausas con `parar()` (500–1000 ms) para disipar inercia antes de cambiar de estado. **Sin short-brake.** (`:153-157`) |
| `delantero-sin-zirconLib.cpp` | **coast + reversa (plugging)** | Igual: `parar()` coast, frenado por reversa breve (200–300 ms). **Sin short-brake.** (`:155-158`) |
| `test-4-movimientos.ino` | **coast** | `parar()/motorX(0,0)` → `INA=INB=0`. Única sofisticación: coast **anticipado** de M3. **Sin reversa, sin short-brake.** (`:399-402/:429-431`) |
| `test-motores-lateral-simple.ino` | **coast** | `parar()` y apagado de M3 → coast. **Sin reversa, sin short-brake.** (`:381-385`) |
| `test-movimiento-omnidireccional.ino` | **coast** | `parar()` apaga las 3 a la vez. **Sin reversa, sin short-brake.** (`:108-112`) |
| `test-gyro-movimiento-basico.ino` | **coast** | `parar()` coast en las 3. (`:300-312`) |
| `test-gyro-movimiento-lateral.ino` | **coast** | `parar()` coast. (`:391-395`) |
| `zirconLib.cpp` | **coast (implícito)** | No hay rutina de freno; `motorN(0,...)` deja PWM=0 (coast). |
| `zirconLib.h` | **ninguno** | El header no implementa nada. |
| `test-bno055-imuplus.ino` | **ninguno** | No controla motores. |

**Lectura:** **el short-brake activo (`INA=INB=1`) NO aparece en NINGÚN programa.** Todos paran por **coast**; solo los dos delanteros agregan **reversa (plugging)** para frenadas rápidas. Esto deja una puerta abierta para 2026: el short-brake es una técnica que el equipo nunca probó.

---

## 8) Heading / giroscopio + PID

**Qué es:** mantener la orientación del robot fija con el BNO055. Importa distinguir si el heading se usa como **feedback continuo a los motores** (lazo cerrado real) o solo como **gate / condición** de transición de estados.

| Programa | Sensor + ganancias | Tipo | Cómo se usa |
|---|---|---|---|
| `definitivo-delantero.cpp` | BNO055 0x28, **kp=0.3** (solo P, sin Ki/Kd) | **P** | **Gate, NO feedback a motores.** `correccion=error*kp` se calcula pero NO se aplica a `analogWrite`. Sirve de condición: avanza si `|error|<=50`, patea corto si `<=80`, centra si `<=1`. (`:81`, `:362-365`, `:448`) |
| `delantero-sin-zirconLib.cpp` | BNO055 0x28, **kp=0.3** (solo P) | **P** | **Gate, NO feedback.** Idéntico: `correccion` nunca llega a los motores. `|error|<=50` avanzar, `<=1` patear, `<=80` corto vs reorbitar. (`:83-88`, `:392-395`, `:478`) |
| `test-4-movimientos.ino` | BNO055 0x28. **Básico (adel/atrás):** Kp=3.0 Ki=0.08 Kd=0.8, clamp ±80, integral ±50. **Lateral:** Kp=3.0 Ki=0.05 Kd=0.5, clamp ±50, integral ±40 | **PID completo** | **Feedback real a motores.** En adel/atrás la corrección suma/resta a las delanteras; en lateral entra como rotación `× FACTOR_ROTACION=0.5`. Anti-windup + derivada por dt real. (`:92-96`, `:99-103`, `:363-385`) |
| `test-motores-lateral-simple.ino` | BNO055 0x28. **Kp=3.0 Ki=0.05 Kd=0.5**, clamp ±50, integral ±40 | **PID completo** | **Feedback real.** Setpoint=0, reparte a los 3 motores con signos `ROT_M={+1,-1,+1}` y peso 0.5. Fallback sin gyro: se mueve igual con heading=0. (`:83-88`, `:402-405`) |
| `test-movimiento-omnidireccional.ino` | BNO055 0x28 (IMUPLUS). **Kp_heading=2.0 Kd_heading=0.05** (sin Ki) | **PD** | **Feedback real.** `omega = error*Kp + deriv*Kd`, saturado ±OMEGA_MAX=100, entra a las 3 ruedas como `L_ROTACION(0.6)*omega`. (`:65-66`, `:151-152`) |
| `test-gyro-movimiento-basico.ino` | BNO055 0x28. **Kp=3.0 Ki=0.08 Kd=0.8**, clamp ±80, integral ±50 | **PID completo** | **Feedback real.** `error=0-heading`, corrección `+/-` simétrica entre las 2 ruedas delanteras (diferencial puro, no toca velocidad media). (`:66-68`, `:252-256`) |
| `test-gyro-movimiento-lateral.ino` | BNO055 0x28. **Kp=4.0 Ki=0.1 Kd=0.8**, clamp ±60, integral ±40 | **PID completo** | **Feedback real.** Corrección solo a M1/M2 con `*0.5`; M3 (rueda principal) NO recibe corrección. (`:76-81`, `:365-366`) |
| `test-bno055-imuplus.ino` | BNO055 0x28 (IMUPLUS) | **Solo lectura** | **No hay PID.** Solo lee e imprime heading por Serial. Modo IMUPLUS para evitar el offset ~35° del magnetómetro. (`:60`, `:131-132`) |
| `zirconLib.cpp/.h` | BNO055 (lectura) | **Ninguno** | `readCompass()` lee `orientation.x` pero `CalibrateCompass()` está **COMENTADA** → `compassCalibrated=false` → devuelve **0**. Heading ni siquiera operativo. (`:46`, `:62-92`, `:94-104`) |

**Lectura clave (muy importante para 2026):**
- Los **robots de competencia 2025** (delanteros) usaban el giroscopio **solo como gate** (P con kp=0.3, corrección calculada pero **nunca aplicada a los motores**).
- Los **sketches de prueba** son los que tienen el **PID real cerrado sobre los motores**. Las ganancias más maduras y repetidas: **Kp=3.0, Ki=0.08, Kd=0.8** (adelante/atrás) y **Kp=3.0, Ki=0.05, Kd=0.5** (lateral), con clamps de corrección **±50 a ±80** e integral **±40 a ±50**.
- **Detalle no negociable:** todos calibran offset al boot (promedio de **10 lecturas**), esperan `gyro>=3`, y normalizan a **±180°**. Varios usan **IMUPLUS** (sin magnetómetro) para evitar el offset de ~35°.

---

## TABLA GRANDE — PWM por movimiento

PWM en escala 0–255. "Delanteras" = M1/M2 (laterales del KIWI); "Trasera" = M3 (paralela al avance). En los tests omni puros no hay distinción física delantera/trasera (se anota la mezcla cinemática).

| Programa | Movimiento | PWM delanteras (M1/M2) | PWM trasera (M3) | Nota |
|---|---|---|---|---|
| `definitivo-delantero` | adelante | **100 / 100** | **0** (apagada) | trasera OFF en avance recto (`:159-161`) |
| `definitivo-delantero` | adelante-pateo | rampa **0→240** (paso 5/20 ms) | 0 | rampa de aceleración (`:197-199`) |
| `definitivo-delantero` | rotar (girar) | `100*g` = **30 / 30** | **30** | las 3 mismo sentido, g=0.3 (`:148-150`) |
| `definitivo-delantero` | rotar (apuntar) | `100*a` = **40 / 40** | **40** | a=0.4 (`:543-552` análogo) |
| `definitivo-delantero` | lateral (centrado) | `pd*50` = **50 / 50** | `pd*89` = **89** | pd=1 normal, 1.5 si desalineada (`:213-231`) |
| `definitivo-delantero` | círculo (órbita) | `60*c` = **24 / 24** | `180*c` = **72** | trasera 3× las laterales, c=0.4 (`:615-617`) |
| `definitivo-delantero` | círculo-impulso | `60*ic` = **33 / 33** | `180*ic` = **99** | ic=0.55, 500/300 ms (`:674-678`) |
| `definitivo-delantero` | atrás (escape línea) | combinaciones **100 / 0** | **0** o **100** | retroceder1/2/3 según sensor, 400 ms (`:165-177`) |
| `definitivo-delantero` | reversa-pateo (freno) | **250 / 170** | **0** | plugging asimétrico, 200–300 ms (`:205-207`) |
| `definitivo-delantero` | stop | **0 / 0** | **0** | coast (`:153-157`) |
| `delantero-sin-zirconLib` | adelante | **100 / 100** | **0** | trasera OFF (`:160-164`) |
| `delantero-sin-zirconLib` | rotar (girar) | `100*g`=**30 / 30** | **30** | g=0.3 (`:149-152`) |
| `delantero-sin-zirconLib` | rotar (apuntar) | `100*a`=**40 / 40** | **40** | a=0.4 (`:543-552`) |
| `delantero-sin-zirconLib` | círculo (órbita) | `60*c`=**24 / 24** | `180*c`=**72** | c=0.4 (`:645-647`) |
| `delantero-sin-zirconLib` | círculo-impulso | `60*ic`=**33 / 30** | `180*ic`=**99 / 90** | ic=0.55 R2 / 0.5 R1 (`:704-708`) |
| `delantero-sin-zirconLib` | atrás (escape línea) | **100** en 2 ruedas | **0** o **100** | retroceder1/2/3, 400 ms (`:166-180`) |
| `delantero-sin-zirconLib` | "pateo" (empuje) | rampa **0→240** | **0** | empuje por inercia (no hay kicker) (`:204-214`) |
| `delantero-sin-zirconLib` | reversa-pateo (freno) | **250 / 170** | **0** | plugging asimétrico (`:220-222`) |
| `delantero-sin-zirconLib` | stop | **0 / 0** | **0** | coast (`:155-158`) |
| `test-4-movimientos` | adelante | **150 ± corr PID** | **0** | solo 2 ruedas traccionan (`:444-465`) |
| `test-4-movimientos` | atrás | **150 ∓ corr PID** | **0** | corrección invertida (`:474-475`) |
| `test-4-movimientos` | lateral-der | **55 / 55** (DIR={-1,+1}) + rot | **100** (frena 60 ms antes) | freno anticipado M3 (`:843-891`) |
| `test-4-movimientos` | lateral-izq | **55 / 55** (dir=-1) + rot | **100** (frena 60 ms antes) | (`:916-964`) |
| `test-4-movimientos` | rotar puro | — | — | **no encontrado** (solo rotación como corrección PID) |
| `test-4-movimientos` | stop | **0 / 0** | **0** | coast (`:435-439`) |
| `test-motores-lateral-simple` | lateral-der | **55 / 55** + rot PID | **100** (frena 60 ms antes) | M3 = rueda lateral principal (`:398-400`) |
| `test-motores-lateral-simple` | lateral-izq | **55 / 55** + rot PID | **100** (frena 60 ms antes) | dir=-1 (`:629`) |
| `test-motores-lateral-simple` | stop | **0 / 0** | **0** | coast (`:381-385`) |
| `test-movimiento-omnidireccional` | adelante (vel=60) | m1=**+52**, m2=**-52** | m3=**0** | KIWI: m3=vx=0 (`:156-162`) |
| `test-movimiento-omnidireccional` | atrás (vel=60,dir=180) | m1=**-52**, m2=**+52** | m3=**0** | (`:193`) |
| `test-movimiento-omnidireccional` | lateral-der (dir=90) | m1=**-30**, m2=**-30** | m3=**+60** | (`:194`) |
| `test-movimiento-omnidireccional` | lateral-izq (dir=-90) | m1=**+30**, m2=**+30** | m3=**-60** | (`:195`) |
| `test-movimiento-omnidireccional` | diagonal (dir=45) | m1=**+15**, m2=**-52** | m3=**+42** | (`:196`) |
| `test-movimiento-omnidireccional` | rotar puro (vel=0) | `L_ROTACION*omega` (3 ruedas igual signo) | igual | omega saturado ±100 (`:197-198`) |
| `test-movimiento-omnidireccional` | stop | **0 / 0** | **0** | coast (`:108-112`) |
| `test-gyro-movimiento-basico` | adelante | **150 ± corr PID** (constrain 0-255) | **0** (siempre apagada) | solo 2 ruedas (`:252-256`) |
| `test-gyro-movimiento-basico` | atrás | **150 ∓ corr PID** | **0** | (`:278-279`) |
| `test-gyro-movimiento-basico` | stop | **0 / 0** | **0** | coast (`:300-312`) |
| `test-gyro-movimiento-lateral` | lateral-der | `100*0.5`=**50 / 50** ± corr·0.5 | `100*1.0`=**100** | M3 sin corrección PID (`:359-385`) |
| `test-gyro-movimiento-lateral` | lateral-izq | **50 / 50** ± corr·0.5 | **100** | signos invertidos (`:380-385`) |
| `test-gyro-movimiento-lateral` | stop | **0 / 0** | **0** | coast (`:391-395`) |
| `zirconLib.cpp/.h` | (sin movimientos de alto nivel) | — | — | solo `motorN(power,dir)` con techo **100** |
| `test-bno055-imuplus` | (no controla motores) | — | — | solo lee/imprime heading |

---

## Apéndice por programa

### A) `definitivo-delantero.cpp` — delantero 2025 DEFINITIVO (competencia)
`software/_deprecated-2025/robot-delantero/definitivo-delantero.cpp`
FSM grande (~30 estados) que separa comportamiento DELANTERO (busca/apunta/centra/orbita/patea) y ARQUERO (strafe proporcional + impulsos). Avance recto = 2 laterales a 100, trasera apagada. Órbita = trasera 3× las laterales. "Pateo" = empuje por inercia (rampa a 240) + freno por reversa asimétrica (250/170). Heading P (kp=0.3) usado como **gate**, no como feedback. Coast + reversa, sin short-brake, sin deadzone.
**Evidencia:** `:159-161` avance (100/100/0) · `:148-150` girar (30) · `:395-399` impulso 150×70 ms · `:1034-1038` impulso arquero 1.8× · `:615-617` órbita 24/72 · `:674-678` impulso órbita ic=0.55 · `:205-207` reversa-pateo 250/170 · `:30-31` patadM1/M2 · `:181-200` rampa pateo 0→240 · `:81/:362-365` kp=0.3 (P) · `:448` gate `|error|<=50` · `:153-157` parar() coast.

### B) `delantero-sin-zirconLib.cpp` — delantero 2025 sin librería
`software/_deprecated-2025/robot-delantero/delantero-sin-zirconLib.cpp`
Gemelo del DEFINITIVO sin usar zirconLib. Mismos números (100/100/0 avance, 30 girar, 24/72 órbita, 150×70 ms impulso, reversa 250/170, rampa pateo 0→240, kp=0.3 P como gate). Definía `aiproporcional()/adproporcional()` (avances proporcionales con corrección de rumbo, PWM 40–100) **pero NO las llama en la FSM** del loop.
**Evidencia:** `:160-164` avance · `:149-152` girar · `:425-429` impulso 150×70 ms · `:204-214` rampa pateo · `:220-222` reversa 250/170 · `:645-647` órbita 24/72 · `:704-708`/`:775-779` impulsos órbita 500/300 ms · `:392-395` correccion calculada no aplicada · `:478` gate `|error|<=50` · `:226-272` proporcionales definidas-no-usadas.

### C) `zirconLib.cpp` / `zirconLib.h` — HAL de la placa Zircon
`software/_deprecated-2025/zirconLib/zirconLib.{cpp,h}`
Librería de **bajo nivel**: solo `motor1/2/3(power, direction)` con **techo único `motorLimit=100`** y lectores de sensores (compass/ball/button/line). **NO** tiene cinemática KIWI, ni impulso, ni freno, ni heading PID. Toda la lógica de movimiento vive en el sketch que la usa. `CalibrateCompass()` comentada → heading no operativo. Detecta variante de placa por pin 32 (Mark1 vs Naveen1). Tiene una `}` sobrante en `:355` (código deprecated/malformado).
**Evidencia:** `:9` `motorLimit=100` · `:174/:195/:215` clamp superior (sin floor) · `:46/:62-92` CalibrateCompass comentada · `:94-104` readCompass devuelve 0 si no calibrado · `:236-244` pines motores Mark1.

### D) `test-4-movimientos.ino` — ciclo adelante/atrás/der/izq (3 s c/u)
`software/staging/shared/test-4-movimientos/test-4-movimientos.ino`
**El sketch más rico en técnica de freno.** Adelante/atrás solo 2 ruedas a 150 con PID; laterales con 3 ruedas (55/55/100) + **freno anticipado de M3 (60 ms, auto-calibrado por drift del gyro, paso 5 ms, umbral 1.5°)** + ajuste en vivo por Serial. Saturación **proporcional** (escala los 3 si max>255). `resetPID()` en cada transición. Constantes de signo calibradas a mano (`DIR_M={-1,+1,+1}`, "calibradas por María").
**Evidencia:** `:67` BASE 150 · `:70-72` 55/55/100 · `:92-96`/`:99-103` PID básico/lateral · `:115-120` freno anticipado 60 ms/1.5°/5 ms · `:519-521` frena M3 · `:524-530` saturación proporcional · `:564-585` auto-cal · `:399-402` coast.

### E) `test-motores-lateral-simple.ino` — strafe puro der/izq con freno anticipado
`software/staging/shared/test-motores-lateral-simple/test-motores-lateral-simple.ino`
Versión depurada (v3/v4) del freno anticipado: **M3 (rueda lateral principal) frena ~60 ms antes que M1/M2**, anticipación **dinámica** (`BASE × VEL_M3/100`) + **auto-calibración por gyro** (mide drift 100 ms post-freno, ajusta ±5 ms). PID heading Kp=3.0/Ki=0.05/Kd=0.5 reparte a los 3 con `ROT={+1,-1,+1}`×0.5. Saturación proporcional. Fallback sin gyro (heading=0). Tuning por Serial.
**Evidencia:** `:68-70` 55/55/100 · `:83-88` PID · `:100` BASE 60 ms · `:155-157` anticipación dinámica · `:413-415` frena M3 · `:632` condición de freno · `:486-506` auto-cal ±5 ms.

### F) `test-movimiento-omnidireccional.ino` — primitiva `moverRobot()` (cinemática inversa)
`software/staging/shared/test-movimiento-omnidireccional/test-movimiento-omnidireccional.ino`
**La primitiva omni más completa.** `moverRobot(velocidad, direccionGrados, headingObj)` traslada en cualquier dirección con cinemática inversa KIWI (ruedas a 30/150/270°): `m1=-0.5*vx+0.866*vy+0.6*omega`, `m2=-0.5*vx-0.866*vy+0.6*omega`, `m3=1.0*vx+0.6*omega`. Marco robot: 0=adelante, 90=derecha, 180=atrás, -90=izquierda (`vx=vel*sin(dir)`, `vy=vel*cos(dir)`). **PD** de heading (Kp=2.0, Kd=0.05, omega ±100). Saturación proporcional a MOTOR_MAX=200. Corre 9 tests de 3 s.
**Evidencia:** `:65-69` ganancias · `:151-152` omega · `:156-157` polar→cartesiano · `:160-162` cinemática inversa · `:165-171` saturación · `:96-106` aplicarMotor sin piso · `:108-112` coast.

### G) `test-gyro-movimiento-basico.ino` — adelante/atrás recto con PID
`software/staging/shared/test-gyro-movimiento-basico/test-gyro-movimiento-basico.ino`
Solo línea recta (adelante 5 s / atrás 5 s) con **2 ruedas** (M1/M2 a 150) y M3 **siempre apagada**. PID heading completo (Kp=3.0/Ki=0.08/Kd=0.8, clamp ±80, integral ±50, setpoint 0). Corrección `+/-` simétrica (diferencial puro, no toca la velocidad media). Avance por botón. Coast.
**Evidencia:** `:59` 150 · `:66-68` PID · `:70-71` clamps · `:252-256` reparto ±corr · `:266-268` M3=0 · `:222` setpoint 0 · `:300-312` coast.

### H) `test-gyro-movimiento-lateral.ino` — strafe con factores fijos + CSV
`software/staging/shared/test-gyro-movimiento-lateral(No probar en este)/test-gyro-movimiento-lateral.ino`
**NO PROBAR (marcado en el nombre).** Strafe der/izq con cinemática de **factores FIJOS no calibrados** (`FACTOR_M1=0.5, M2=0.5, M3=1.0`, comentados "ajustar experimentalmente" — nunca se completó). PID heading **Kp=4.0/Ki=0.1/Kd=0.8** (clamp ±60, integral ±40) aplicado solo a M1/M2 (M3 sin corrección). Loggea a CSV en RAM (600 muestras / 50 ms).
**Evidencia:** `:58` VEL 100 · `:69-71` factores fijos · `:76-81` PID · `:359-366` reparto · `:391-395` coast.

### I) `test-bno055-imuplus.ino` — test de banco del IMU (sin motores)
`software/staging/shared/test-bno055-imuplus/test-bno055-imuplus.ino`
**No controla motores.** Detecta el BNO055 por I2C, espera calibración del gyro (hasta 3 s, `gyro>=3`), captura offset (promedio de 10 lecturas) e imprime el heading por Serial mientras se gira a mano. Modo **IMUPLUS** (sin magnetómetro) para evitar el offset de ~35°. Cristal externo. Normaliza ±180°. LED como indicador.
**Evidencia:** `:38` BNO 0x28 · `:60` IMUPLUS · `:77` cristal externo · `:86-99` espera calibración · `:106-114` offset 10 lecturas · `:135-136` normalización ±180.

---

## Cierre — valores candidatos a portar al 2026

Estos son **los números más representativos hallados en el histórico real** (no inventados). Cada uno se debe **re-validar en banco** con la mecánica 2026 antes de confiar, y respetando el **cap de potencia ~70%** de los motores 5V actuales (ver memoria de hardware — no pasar ~70% o se queman).

| Técnica | Valor candidato | De dónde sale |
|---|---|---|
| **Impulso anti-inercia (giro)** | PWM **150 × 70 ms** → baja a régimen | delantero (`:395-399`) |
| **Impulso anti-inercia (arranque arquero)** | factor **1.8 × 40 ms** | delantero (`:1034-1038`) |
| **Freno anticipado trasera (lateral)** | **60 ms** a VEL_M3=100, escalado `× VEL_M3/100`, auto-cal por gyro (paso 5 ms, umbral 1.5°) | test-4-mov / test-lateral-simple |
| **Freno por reversa (plugging)** | **M1=250 / M2=170 × 200–300 ms** (asimétrico) | delantero (`:205-207`) |
| **Rampa de aceleración (empuje)** | **0→240, paso 5 cada 20 ms** | delantero (`:181-200`) |
| **PID heading adelante/atrás** | **Kp=3.0 Ki=0.08 Kd=0.8**, clamp corr ±80, integral ±50 | test-gyro-basico / test-4-mov |
| **PID heading lateral** | **Kp=3.0 Ki=0.05 Kd=0.5**, clamp ±50, integral ±40 | test-lateral-simple |
| **PD heading omni** | **Kp=2.0 Kd=0.05**, omega ±100, `L_ROTACION=0.6` | test-omni (`:65-66`) |
| **Cinemática inversa KIWI** | `m1=-0.5vx+0.866vy+0.6ω · m2=-0.5vx-0.866vy+0.6ω · m3=vx+0.6ω` | test-omni (`:160-162`) |
| **Calibración heading al boot** | promedio **10 lecturas**, esperar `gyro>=3`, modo **IMUPLUS**, normalizar **±180°** | test-bno-imuplus / todos |
| **PWM avance recto** | delanteras **100**, trasera **0** | delantero (`:159-161`) |
| **PWM órbita** | laterales **24**, trasera **72** (trasera 3×) | delantero (`:615-617`) |

**Gaps del histórico que 2026 podría cubrir (NO existían en ningún programa):**
- **Deadzone / piso de PWM:** **no encontrado** en ningún programa. Con PID grande una rueda puede caer a 0 y "morir". Candidato a agregar.
- **Short-brake activo (`INA=INB=1`):** **no encontrado.** Todos frenan por coast (+ reversa los delanteros). Técnica nunca probada por el equipo.
- **Delay de arranque de la trasera:** **no encontrado** (la trasera se apaga, no se retrasa). No es una técnica que el histórico tenga.
- **Rampa de desaceleración:** **no encontrada** (solo rampa de aceleración en el pateo).
- **Heading como feedback continuo en los robots de competencia:** los delanteros 2025 usaban el gyro **solo como gate** (kp=0.3, corrección nunca aplicada a motores); el PID real cerrado sobre motores **solo vive en los sketches de prueba**. Portar ese PID a la FSM de juego es la mejora más jugosa.