<!-- AUTO-GENERADO por el workflow analisis-motion-control (wf_39266dd8-484) el 2026-06-08. Analisis de 22/23 programas (test-circulo.ino no se pudo analizar -> revisar a mano). Valores citados = del codigo real con archivo:linea. Editable a mano. -->

# Control de movimiento — ESTRATEGIA ACTUAL 2026 (y gaps vs el histórico)

**Fecha:** 2026-06-08
**Alcance:** Cómo mueve el robot HOY (firmware 2026, placa CENTRAL/Zircon, omni KIWI 3 ruedas) y qué técnicas de control de movimiento tenían los robots históricos (delantero/arquero 2025 + tests de staging) que el firmware actual **NO** porta.
**Honestidad total:** hoy el único refinamiento de bajo nivel que existe es el **piso de PWM por rueda (deadzone)**. NO hay impulso inicial, NI delay de arranque de la trasera, NI freno anticipado de la trasera, NI freno por reversa (plugging), NI rampas de aceleración. Esto se dice claro y se justifica con evidencia abajo.

---

## Arquitectura actual

El movimiento 2026 es **continuo por velocidad**, no por PWM discreto. La FSM de estrategia (`strategy.cpp`) y los diags emiten un `MotorCommand{vx_mm_s, vy_mm_s, omega_centideg_s}` en marco robot (+X derecha, +Y frente, omega+ = CCW). Ese comando entra a un único pipeline:

```
MotorCommand (vx, vy, omega)
   │  [slow-mo gate: MOTION_SCALE=0.7 SOLO con -DCENTRAL_SLOW_MOTION; default 1.0]
   ▼
inverse_kinematics(vx, vy, omega_rad_s, WHEELS)        ← kinematics.cpp/.h
   │   v_i = -vx·sin(θ_i) + vy·cos(θ_i) + omega·R
   │   WHEEL_ANGLES_DEG = {330, 210, 90}  (M1 del-izq, M2 del-der, M3 trasera)
   │   WHEEL_RADIUS_MM = 100
   ▼
saturate_wheels(ws, MAX_SPEED_MM_S=1000)               ← saturación PROPORCIONAL (preserva dirección)
   ▼
wheel_speed_to_pwm(ws[i], 1000, MAX_PWM=255)           ← mapa lineal velocidad→PWM, clamp ±255
   ▼
apply_pwm_floor(pwm, MOTOR_MIN_PWM[i], NOISE_THRESH=5) ← PISO por rueda (deadzone); gate OFF si min<=0
   │   MOTOR_MIN_PWM = {70, 70, 42}  (ROBOT1) / {0,0,0} (ROBOT2)
   ▼
apply_pwm_to_motor(pwm · MOTOR_INVERT[i])              ← MOTOR_INVERT = {+1, -1, +1} (M2/U17 invertido HW)
   │   pwm>0 → INA=1/INB=0 · pwm<0 → INA=0/INB=1 (reversa de marcha) · pwm=0 → coast
   ▼
PWM a los 3 H-bridges del Zircon
```

Decisiones clave de la arquitectura actual:

- **El omega ya viene calculado.** El lazo de rumbo (HeadingPID + LateralPID, alimentado por el heading del TOP / OTOS de DOWN) vive en `strategy.cpp` y los diags, no en el mixer. `motors_zircon` solo convierte y reparte. Hay un **gate de heading**: si el BNO no convergió (`heading_valid=0`), omega se fuerza a 0 (`central_gate_heading_omega`).
- **Saturación que preserva la trayectoria.** Si una rueda excede `MAX_SPEED_MM_S=1000`, se escalan las 3 por el mismo factor — no se recorta una sola (eso deformaría la dirección).
- **El piso es por rueda, no global.** Las delanteras oblicuas (60° respecto al strafe) arrancan más duro → piso 70; la trasera paralela arranca más fácil → piso 42. Esto evita que la trasera "se adelante" y rote el robot en el strafe.
- **Cap de potencia ~150 PWM (~70%)** documentado (motores brushed 5V alimentados a 7.4V se queman por encima) pero **AÚN NO cableado** en `apply_pwm_to_motor` — pendiente de banco.

---

## Lo que el firmware actual SÍ hace (valores reales)

| Mecanismo | Qué hace hoy | Valor real |
|---|---|---|
| **Cinemática inversa KIWI** | Reparte vx/vy/omega a 3 ruedas | `WHEEL_ANGLES_DEG={330,210,90}`, `R=100mm` (el viejo `{60,-60,180}` daba círculos) |
| **Saturación proporcional** | Escala las 3 ruedas para no deformar dirección | clamp a `MAX_SPEED_MM_S=1000`, PWM a `±255` |
| **Piso de PWM por rueda (deadzone)** | Eleva PWM no-nulo hasta el piso conservando signo; apaga si `|pwm|≤5` | `MOTOR_MIN_PWM={70,70,42}` R1 / `{0,0,0}` R2; `NOISE_THRESH=5` |
| **Inversión por motor** | Corrige M2/U17 cableado al revés por HW | `MOTOR_INVERT={+1,-1,+1}` |
| **Stop (coast)** | Para normal: INA=INB=0, PWM=0 | `motors_stop()` |
| **Brake activo (short)** | Emergencia: INA=INB=HIGH, PWM=0 (corto del H-bridge) | `motors_brake()` — "solo emergencias, corriente alta" |
| **HeadingPID (rumbo)** | Mantiene el frente vía omega (en strategy/diags) | KP_HEADING ≈ 3.0 deg/s por grado; omega saturado a int16 |
| **LateralPID arquero** | Strafe del arquero por cross_track | `g_lateral_pid_gk` |
| **Drive-straight con OTOS** | Cancela deriva lateral midiendo con odometría | KP_HEADING=3.0, KP_LATERAL=0.5 (aditivo; fallback exacto si OTOS no fresco) |
| **Impulso de set-play (KICKOFF)** | Boost de **velocidad** (no PWM) al frente | `ATK_KICKOFF_SPEED=500mm/s` por `250ms` → luego SEARCH a 200mm/s |
| **Perfil de velocidad por distancia** | "Rampa" espacial (no temporal): frena al acercarse al target | APPROACH 50–500mm → 200–600mm/s; POSITION 80–500mm → 200–500mm/s |
| **Timeout de frescura** | Si el snapshot está viejo → para | `SNAPSHOT_TIMEOUT_MS=500` (en world_model) |

> Nota importante: el "impulso" del KICKOFF y las "rampas por distancia" operan a nivel **velocidad comandada (mm/s)**, NO a nivel PWM de rueda. No rompen inercia mecánica ni compensan stiction del motor. Son lógica de estrategia, no de control de motor.

---

## GAP vs el histórico — tabla honesta

| Técnica | Histórico (tenía: valor) | Actual 2026 (tiene / NO) | Impacto en cancha |
|---|---|---|---|
| **Impulso inicial (anti-inercia)** | **SÍ.** Delantero 2025: PWM 150 en las 3 ruedas por **70ms** antes de bajar a 30 (girar). Arquero 2025: factor **1.8×** (M1/M2=90, M3=153) por **40ms**; re-impulso **350ms** al cambiar de lado. | **NO.** Solo existe el **piso estático** de PWM por rueda (70/70/42). El comentario dice que "porta el IMPULSO_INICIAL del delantero 2025", pero es un **floor permanente**, no un pulso que sube y baja en ms. No hay `millis()` ni timers en el mixer. | El arquero arranca el strafe **lento/dudoso** desde parado: el piso ayuda a vencer stiction pero no da el "tirón" que rompe la inercia. En microdesplazamientos rápidos (tapar un tiro) puede haber **latencia de arranque** perceptible. |
| **Delay de arranque de la trasera** | **NO** (ni histórico ni actual). En el histórico la trasera simplemente iba a PWM 0 en avance recto (apagado, no retardo). | **NO.** Las 3 ruedas arrancan en el mismo `motors_apply_command`. La asimetría trasera/delantera se maneja con **piso más bajo** (42 vs 70), no con delay. | Ninguno por ausencia: nunca fue una técnica usada. No es un gap real. |
| **Freno anticipado de la trasera** | **SÍ, en staging.** `test-4-movimientos` y `test-lateral-simple`: en strafe, M3 (trasera, alta inercia) se apaga **~60ms ANTES** (`BASE_ANTICIPACION_MS=60`, escala con VEL_M3, auto-calibrado por drift de gyro >1.5°). | **NO.** En `motors_stop()` las 3 ruedas se frenan a la vez. No hay apagado escalonado por rueda. | Al terminar un tramo lateral, la trasera con inercia **rota/desvía** el robot al frenar (el drift que el staging corregía). El arquero puede **quedar torcido** tras cada barrido lateral → pierde encuadre con el arco. |
| **Freno por reversa (plugging)** | **SÍ.** Delantero 2025: tras patear/empujar, reversa asimétrica M1=250 / M2=170 por **200–300ms**; escape de línea reversa **400ms**. Frenado real y rápido. | **NO.** El "freno" es `motors_stop()` (coast) o `motors_brake()` (short, solo emergencias). Nunca se invierte el motor por un tiempo breve para frenar. LINE_AVOID retrocede como navegación, no como freno. | **Frenado lento.** El robot **se pasa de largo** (overshoot) al detenerse desde velocidad. Crítico en el arquero: para una intercepción, frenar en seco importa tanto como acelerar. Hoy depende solo del coast (rueda libre) → distancia de frenado larga. |
| **Rampas de aceleración (slew temporal)** | **Parcial.** Solo en el pateo del delantero 2025: 0→240 PWM en pasos de 5 cada 20ms (~960ms). El resto era PWM de golpe. No había rampa de **desaceleración** (frenaba por reversa). | **NO** (slew temporal). Hay **perfil por distancia** (APPROACH/POSITION, espacial) y **saturación proporcional** (clamp instantáneo), pero ningún limitador de tasa de cambio de PWM entre ciclos. | Cambios bruscos de comando → **tirones / patinaje** de las ruedas omni (pérdida de tracción) y pico de corriente. Menos crítico que reversa/impulso, pero suma desgaste y resta repetibilidad. |

---

## Cómo se portaría cada técnica (mapeo de alto nivel, NO plan)

> Todo esto cambia el binario de motor → **requiere banco** y queda gateado para no romper el binario de competencia hasta validar.

- **Impulso inicial (anti-inercia):** iría en `motors_apply_command()` de `motors_zircon.cpp`, **después** de `apply_pwm_floor` y **antes** de `apply_pwm_to_motor`. Detectar transición "parado → comando no-nulo" y aplicar un multiplicador por una ventana corta. Constantes nuevas: `MOTOR_KICKSTART_FACTOR` (ej. ~1.6–1.8) y `MOTOR_KICKSTART_MS` (ej. ~40–70ms), + un `last_command_zero_ms` para el timer. Es la primera vez que el mixer tendría estado temporal (`millis()`).

- **Freno anticipado de la trasera:** iría en la capa que decide el stop del strafe (en el arquero: `strategy.cpp` GK, o en el diag de strafe), no en el mixer genérico, porque depende de saber "qué rueda es la paralela al movimiento". Constante nueva: `BRAKE_LEAD_MS` por rueda (escalado por su velocidad), con auto-calibración por drift de heading como el staging. Alternativa más limpia hoy: dejar que el HeadingPID corrija el drift post-frenada (ya existe) y medir si alcanza.

- **Freno por reversa (plugging):** nuevo modo en `motors_zircon.cpp`, hermano de `motors_brake()`, ej. `motors_plug_brake(prev_cmd)` que aplica PWM en sentido **opuesto al último comando** durante una ventana corta, luego coast. Constantes: `PLUG_BRAKE_MS` (~150–300ms) y `PLUG_BRAKE_MAGNITUDE`. La FSM lo invocaría al pasar a STOP desde velocidad alta (no en cada parada). **Ojo cap 5V/7.4V:** la reversa es PWM alto → debe respetar el cap de ~70%.

- **Rampas de aceleración (slew):** un limitador de tasa de cambio sobre el PWM final (o sobre vx/vy/omega) en `motors_apply_command()`, guardando el PWM del ciclo anterior por rueda. Constante: `MAX_PWM_DELTA_PER_TICK`. Aditivo y gateable; con delta grande es no-op (binario idéntico).

---

## Prioridad para el arquero de Incheon (honesto)

El arquero vive del **strafe lateral corto y del arranque/frenado preciso**. Por eso los gaps que más le pegan son los de **inercia (arranque)** y **frenado**:

- **P1 — Freno por reversa (plugging).** Es el gap de mayor impacto real en el arquero. Sin él, el robot se pasa de largo al interceptar y al terminar cada barrido. Es lo que más diferencia "tapa el tiro" de "llega tarde". Requiere banco + respetar el cap de potencia. *risk-no-fix:* overshoot persistente, intercepciones perdidas. *risk-fix:* PWM alto en reversa puede quemar motor si no se capea; mal tuneo del tiempo → oscila.

- **P1 — Impulso inicial (anti-inercia).** El segundo más importante: reduce la latencia de arranque del strafe desde parado. El piso 70/70/42 ayuda pero no reemplaza el tirón. *risk-no-fix:* arranque lento, microajustes perezosos. *risk-fix:* tirón excesivo → el arquero "salta" y se pasa; introduce estado temporal nuevo en el mixer (más superficie de bug).

- **P2 — Freno anticipado de la trasera.** Deseable, pero **el HeadingPID actual ya corrige parte del drift post-frenada**. Conviene primero medir cuánto drift queda en banco antes de portarlo; puede ser innecesario si el PID alcanza. *risk-no-fix:* arquero queda levemente torcido tras cada barrido (corregible por PID). Capitalizable a 2027.

- **P2 — Rampas de aceleración.** Mejora de suavidad/repetibilidad y reduce patinaje, pero no decide partidos en el arquero. Útil sobre todo cuando lleguen mejores motores. Capitalizable a 2027.

**Ninguno es P0:** ninguno bloquea que el robot compita o desclasifique. El P0 real del arquero sigue siendo aguas arriba (visión sin recalibrar, hardware-up). Estos cuatro son refinamientos de control que mejoran el rendimiento en cancha, no habilitan la participación.

> **Regla del repo:** ninguno de estos cuatro puede marcarse `done` por Claude. Cambian el binario de motor → cierre solo por el equipo con la placa en banco, midiendo distancia de frenado / drift / latencia de arranque con criterio numérico y plan de prueba en hardware real.