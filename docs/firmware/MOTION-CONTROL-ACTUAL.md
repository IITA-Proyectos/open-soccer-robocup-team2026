<!-- AUTO-GENERADO por el workflow analisis-motion-control (wf_39266dd8-484) el 2026-06-08. Analisis de 22/23 programas (test-circulo.ino no se pudo analizar -> revisar a mano). Valores citados = del codigo real con archivo:linea. Editable a mano. -->

# Control de movimiento — ESTRATEGIA ACTUAL 2026 (y gaps vs el histórico)

**Fecha:** 2026-06-08
**Alcance:** Cómo mueve el robot HOY (firmware 2026, placa CENTRAL/Zircon, omni KIWI 3 ruedas) y qué técnicas de control de movimiento tenían los robots históricos (delantero/arquero 2025 + tests de staging) que el firmware actual **NO** porta.
**Honestidad total (actualizado 2026-06-09):** hoy existen TRES refinamientos de bajo nivel: el **piso de PWM por rueda (deadzone)**, el **impulso inicial (kickstart)** y el **freno anticipado de la trasera**. Los dos últimos fueron portados del histórico y ✅ **validados en banco ROBOT2 el 2026-06-09** (Gustavo, env `diag_central_strafe_robot2_kick`, "anda bien"); en ROBOT1 arrancan con los mismos valores, **A VERIFICAR EN BANCO R1**. Siguen FALTANDO: delay de arranque de la trasera (nunca existió, no es gap real), freno por reversa (plugging) y rampas de aceleración. Detalle del banco: addendum 2026-06-09 al final.

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
   │   MOTOR_MIN_PWM = {70, 70, 107}  (R2 ✅ banco 2026-06-09 / R1 mismos valores, A VERIFICAR EN BANCO R1)
   ▼
[kickstart — gateado -DCENTRAL_MOTOR_KICKSTART]        ← impulso fijo por rueda {130,130,140} PWM × 40 ms
   │   en la transición parado→comando de cada rueda    (✅ banco R2 2026-06-09)
   ▼
[corte trasera — gateado -DCENTRAL_REAR_BRAKE_LEAD]    ← motors_set_rear_cut(): trasera (idx2) a 0 en los
   │   últimos 66 ms del tramo lateral                  últimos ms (hoy lo arma SOLO diag_central_strafe)
   ▼
apply_pwm_to_motor(pwm · MOTOR_INVERT[i])              ← MOTOR_INVERT = {+1, -1, +1} (M2/U17 invertido HW)
   │   pwm>0 → INA=1/INB=0 · pwm<0 → INA=0/INB=1 (reversa de marcha) · pwm=0 → coast
   ▼
PWM a los 3 H-bridges del Zircon
```

Decisiones clave de la arquitectura actual:

- **El omega ya viene calculado.** El lazo de rumbo (HeadingPID + LateralPID, alimentado por el heading del TOP / OTOS de DOWN) vive en `strategy.cpp` y los diags, no en el mixer. `motors_zircon` solo convierte y reparte. Hay un **gate de heading**: si el BNO no convergió (`heading_valid=0`), omega se fuerza a 0 (`central_gate_heading_omega`).
- **Saturación que preserva la trayectoria.** Si una rueda excede `MAX_SPEED_MM_S=1000`, se escalan las 3 por el mismo factor — no se recorta una sola (eso deformaría la dirección).
- **El piso es por rueda, no global — y la trasera es la ALTA (banco 2026-06-09).** Delanteras oblicuas 70 · trasera 107. La intuición vieja ("la trasera paralela arranca más fácil → piso bajo 42") quedó refutada en banco: en el strafe la trasera debe girar al **DOBLE** de velocidad que las delanteras (cinemática: fronts 0.5·vx, rear 1.0·vx) y el PWM **no** es proporcional a la velocidad — como rueda alineada (menos fricción que las oblicuas) lo logra con ~1.5× el PWM (107 vs 70), no 2×. El `{70,70,42}` viejo era del banco R1 2026-06-08 (la trasera se bajó porque rotaba); si R1 rota con 107, se baja gradualmente.
- **Cap de potencia ~150 PWM (~70%)** documentado (motores brushed 5V alimentados a 7.4V se queman por encima) pero **AÚN NO cableado** en `apply_pwm_to_motor` — pendiente de banco.

---

## Lo que el firmware actual SÍ hace (valores reales)

| Mecanismo | Qué hace hoy | Valor real |
|---|---|---|
| **Cinemática inversa KIWI** | Reparte vx/vy/omega a 3 ruedas | `WHEEL_ANGLES_DEG={330,210,90}`, `R=100mm` (el viejo `{60,-60,180}` daba círculos) |
| **Saturación proporcional** | Escala las 3 ruedas para no deformar dirección | clamp a `MAX_SPEED_MM_S=1000`, PWM a `±255` |
| **Piso de PWM por rueda (deadzone)** | Eleva PWM no-nulo hasta el piso conservando signo; apaga si `|pwm|≤5` | `MOTOR_MIN_PWM={70,70,107}` (✅ banco R2 2026-06-09; R1 mismos valores, A VERIFICAR EN BANCO R1); `NOISE_THRESH=5` |
| **Impulso inicial (kickstart)** | Pulso fijo por rueda en la transición parado→comando; rompe la inercia y baja al PWM base al cerrar la ventana | `{130,130,140}` PWM × **40 ms** (factor ×9.9 + cap por rueda = impulso fijo); gateado `-DCENTRAL_MOTOR_KICKSTART`; ✅ banco R2 2026-06-09 |
| **Freno anticipado de la trasera** | `motors_set_rear_cut()` en el mixer: corta la TRASERA (idx2) a 0 en los últimos ms del tramo lateral mientras las delanteras terminan | lead **66 ms** (tunable `-DDIAG_STRAFE_REAR_LEAD_MS`); gateado `-DCENTRAL_REAR_BRAKE_LEAD`; hoy lo arma SOLO `diag_central_strafe.cpp`; ✅ banco R2 2026-06-09 |
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
| **Impulso inicial (anti-inercia)** | **SÍ.** Delantero 2025: PWM 150 en las 3 ruedas por **70ms** antes de bajar a 30 (girar). Arquero 2025: factor **1.8×** (M1/M2=90, M3=153) por **40ms**; re-impulso **350ms** al cambiar de lado. | **✅ YA NO ES GAP (2026-06-09).** Portado como kickstart en `motors_zircon.cpp` (gateado `-DCENTRAL_MOTOR_KICKSTART`): impulso **fijo por rueda `{130,130,140}` PWM × 40 ms** en la transición parado→comando (factor ×9.9 + cap por rueda = impulso fijo; la trasera necesitaba 140 porque "se quedaba"). ✅ Validado en banco ROBOT2 (`diag_central_strafe_robot2_kick`); R1 mismos valores, A VERIFICAR EN BANCO. | **Gap cerrado.** El strafe rompe la inercia al primer comando ("anda bien" en banco R2). Por decisión de Gustavo, se usa SIEMPRE que el robot se mueva lateralmente, en todos los programas. |
| **Delay de arranque de la trasera** | **NO** (ni histórico ni actual). En el histórico la trasera simplemente iba a PWM 0 en avance recto (apagado, no retardo). | **NO.** Las 3 ruedas arrancan en el mismo `motors_apply_command`. La asimetría trasera/delantera se maneja con **piso más bajo** (42 vs 70), no con delay. | Ninguno por ausencia: nunca fue una técnica usada. No es un gap real. |
| **Freno anticipado de la trasera** | **SÍ, en staging.** `test-4-movimientos` y `test-lateral-simple`: en strafe, M3 (trasera, alta inercia) se apaga **~60ms ANTES** (`BASE_ANTICIPACION_MS=60`, escala con VEL_M3, auto-calibrado por drift de gyro >1.5°). | **✅ YA NO ES GAP en los diags (2026-06-09).** Portado como `motors_set_rear_cut()` en el mixer (gateado `-DCENTRAL_REAR_BRAKE_LEAD`): corta la TRASERA (idx2) a 0 en los últimos **66 ms** del tramo (tunable `-DDIAG_STRAFE_REAR_LEAD_MS`) mientras las delanteras terminan. ✅ Validado en banco ROBOT2. Hoy lo arma SOLO `diag_central_strafe.cpp` — llevarlo al lateral de la FSM del arquero es **tema-a-analizar** (el corte necesita saber cuándo TERMINA el movimiento; en el control continuo de la FSM no existe ese evento). | **Gap cerrado para movimientos de duración conocida** (diags): sin el corte, la inercia de la trasera desacomodaba el robot al frenar. En la FSM (fin por evento asíncrono) sigue abierto el glue. |
| **Freno por reversa (plugging)** | **SÍ.** Delantero 2025: tras patear/empujar, reversa asimétrica M1=250 / M2=170 por **200–300ms**; escape de línea reversa **400ms**. Frenado real y rápido. | **NO.** El "freno" es `motors_stop()` (coast) o `motors_brake()` (short, solo emergencias). Nunca se invierte el motor por un tiempo breve para frenar. LINE_AVOID retrocede como navegación, no como freno. | **Frenado lento.** El robot **se pasa de largo** (overshoot) al detenerse desde velocidad. Crítico en el arquero: para una intercepción, frenar en seco importa tanto como acelerar. Hoy depende solo del coast (rueda libre) → distancia de frenado larga. |
| **Rampas de aceleración (slew temporal)** | **Parcial.** Solo en el pateo del delantero 2025: 0→240 PWM en pasos de 5 cada 20ms (~960ms). El resto era PWM de golpe. No había rampa de **desaceleración** (frenaba por reversa). | **NO** (slew temporal). Hay **perfil por distancia** (APPROACH/POSITION, espacial) y **saturación proporcional** (clamp instantáneo), pero ningún limitador de tasa de cambio de PWM entre ciclos. | Cambios bruscos de comando → **tirones / patinaje** de las ruedas omni (pérdida de tracción) y pico de corriente. Menos crítico que reversa/impulso, pero suma desgaste y resta repetibilidad. |

---

## Cómo se portaría cada técnica (mapeo de alto nivel, NO plan)

> Todo esto cambia el binario de motor → **requiere banco** y queda gateado para no romper el binario de competencia hasta validar.

- **Impulso inicial (anti-inercia): ✅ PORTADO Y VALIDADO (2026-06-09), exactamente acá.** Vive en `motors_apply_command()` de `motors_zircon.cpp`, después de `apply_pwm_floor` y antes de `apply_pwm_to_motor`, gateado `-DCENTRAL_MOTOR_KICKSTART`. Valores finales: `KICKSTART_WINDOW_MS=40`, `KICKSTART_FACTOR_X10=99` (×9.9) + `KICKSTART_PWM_CAP[3]={130,130,140}` por rueda → en la práctica un **impulso fijo** {130,130,140}. ✅ Banco ROBOT2; R1 A VERIFICAR.

- **Freno anticipado de la trasera: ✅ PORTADO Y VALIDADO en los diags (2026-06-09).** El mecanismo (`motors_set_rear_cut()`) vive en el mixer gateado `-DCENTRAL_REAR_BRAKE_LEAD`, pero la **decisión** de cuándo cortar vive en el caller que conoce la duración del tramo — hoy SOLO `diag_central_strafe.cpp` (corta la trasera 66 ms antes del fin, tunable `-DDIAG_STRAFE_REAR_LEAD_MS`). Llevarlo al lateral de la FSM del arquero (`strategy.cpp` GK) queda como **tema-a-analizar**: el corte necesita saber cuándo TERMINA el movimiento, y en el control continuo de la FSM ese evento no existe (fin por evento asíncrono, no por tiempo).

- **Freno por reversa (plugging):** nuevo modo en `motors_zircon.cpp`, hermano de `motors_brake()`, ej. `motors_plug_brake(prev_cmd)` que aplica PWM en sentido **opuesto al último comando** durante una ventana corta, luego coast. Constantes: `PLUG_BRAKE_MS` (~150–300ms) y `PLUG_BRAKE_MAGNITUDE`. La FSM lo invocaría al pasar a STOP desde velocidad alta (no en cada parada). **Ojo cap 5V/7.4V:** la reversa es PWM alto → debe respetar el cap de ~70%.

- **Rampas de aceleración (slew):** un limitador de tasa de cambio sobre el PWM final (o sobre vx/vy/omega) en `motors_apply_command()`, guardando el PWM del ciclo anterior por rueda. Constante: `MAX_PWM_DELTA_PER_TICK`. Aditivo y gateable; con delta grande es no-op (binario idéntico).

---

## Prioridad para el arquero de Incheon (honesto)

El arquero vive del **strafe lateral corto y del arranque/frenado preciso**. Por eso los gaps que más le pegan son los de **inercia (arranque)** y **frenado**:

- **P1 — Freno por reversa (plugging).** Es el gap de mayor impacto real en el arquero. Sin él, el robot se pasa de largo al interceptar y al terminar cada barrido. Es lo que más diferencia "tapa el tiro" de "llega tarde". Requiere banco + respetar el cap de potencia. *risk-no-fix:* overshoot persistente, intercepciones perdidas. *risk-fix:* PWM alto en reversa puede quemar motor si no se capea; mal tuneo del tiempo → oscila.

- **~~P1~~ ✅ HECHO (2026-06-09) — Impulso inicial (anti-inercia).** Portado y validado en banco ROBOT2: impulso fijo {130,130,140} PWM × 40 ms, gateado `-DCENTRAL_MOTOR_KICKSTART`. R1 con los mismos valores, A VERIFICAR EN BANCO. Por decisión de Gustavo se usa siempre que el robot se mueva lateralmente.

- **~~P2~~ ✅ HECHO en los diags (2026-06-09) — Freno anticipado de la trasera.** Portado y validado en banco ROBOT2: corte de la trasera 66 ms antes del fin del tramo (`motors_set_rear_cut()`, gateado `-DCENTRAL_REAR_BRAKE_LEAD`). Lo que queda abierto es el **glue a la FSM del arquero** (tema-a-analizar: el corte necesita el evento "fin del movimiento", que la FSM continua no tiene).

- **P2 — Rampas de aceleración.** Mejora de suavidad/repetibilidad y reduce patinaje, pero no decide partidos en el arquero. Útil sobre todo cuando lleguen mejores motores. Capitalizable a 2027.

**Ninguno es P0:** ninguno bloquea que el robot compita o desclasifique. El P0 real del arquero sigue siendo aguas arriba (visión sin recalibrar, hardware-up). Estos cuatro son refinamientos de control que mejoran el rendimiento en cancha, no habilitan la participación.

> **Regla del repo:** ninguno de estos cuatro puede marcarse `done` por Claude. Cambian el binario de motor → cierre solo por el equipo con la placa en banco, midiendo distancia de frenado / drift / latencia de arranque con criterio numérico y plan de prueba en hardware real.

---

## Addendum 2026-06-08 — el arquero 2025 DEDICADO (lo más relevante para el strafe de HOY)

Al excavar el histórico apareció un archivo **sin extensión** que el análisis automático había saltado: `software/_deprecated-2025/robot-arquero/definitivo-arquero_6-9-2026` — el **arquero 2025 real**. Sus valores de strafe son los más directamente aplicables a lo que se está tuneando ahora. Detalle completo en `MOTION-CONTROL-HISTORICO.md` (apéndice arquero). Lo crítico:

### Contradicción de ratio delantera/trasera (¡clave para el tuneo!)

| | Delanteras (M1/M2) | Trasera (M3) | Lazo de rumbo |
|---|---|---|---|
| **Arquero 2025 (strafe)** | **50** | **89–100** (la MÁS fuerte) | **gyro continuo** (ramas por `error`) |
| **2026 hasta el 2026-06-08 (piso strafe)** | **70** | **42** (la más DÉBIL) | **NO** (BNO roto → omega gateado a 0) |
| **2026 desde el 2026-06-09 (piso strafe, ✅ banco R2)** | **70** | **107** (de nuevo la MÁS fuerte) | **NO** (sigue open-loop; anda bien con kickstart + corte trasera) |

Son **ratios opuestos**. Y no es que uno esté "mal": el 2025 le ponía a la trasera ~2× las delanteras porque **geométricamente la trasera hace ~2× la velocidad en un strafe** (es la paralela al movimiento), y **el drift que eso genera lo corregía en vivo con el giróscopo** (subía/bajaba M3 entre 40/89/100 según `error`). El 2026, con el **BNO roto**, no tiene ese lazo, así que para que no rote en lazo abierto hubo que **bajar la trasera** (42) — un parche, no la solución de fondo.

### La lección para 2026

- El parche `{70,70,42}` **sirve para sobrevivir con el BNO roto** (lazo abierto, que no rote). Está bien como puente.
- La **solución de fondo** es el **strafe con corrección giroscópica continua** del arquero 2025: subir M3 a lo geométrico (~89) y dejar que el lazo de rumbo corrija el drift. Eso **depende de tener el BNO sano** (ver TASK-207, BNO→Wire2). Con BNO sano + ese lazo, la trasera vuelve a poder ser la rueda fuerte.
- **Valores de arranque concretos para tunear el strafe 2026** (del arquero 2025, a re-validar en banco, respetando el cap ~70%): delanteras **50** / trasera **89** centrado; `error>0` → trasera **100**; `error<0` → delanteras **65/40** + trasera **40**; impulso de arranque **×1.8 por 40 ms**; anti-trabado en el borde = **350 ms** de strafe forzado; velocidad **×1.5** al ver la pelota.

> En una frase: **el arquero 2025 YA resolvía el strafe — con la trasera fuerte y el giróscopo corrigiendo. El camino del 2026 es recuperar ese lazo (BNO sano), no pelearse con el piso en lazo abierto.**

---

## Addendum 2026-06-09 — banco ROBOT2: la contradicción se resolvió (trasera FUERTE, como el 2025)

Banco de Gustavo con `diag_central_strafe_robot2_kick` (**"anda bien"**). Tres resultados que cambian la tabla de arriba:

1. **Piso de PWM por rueda FINAL:** `MOTOR_MIN_PWM = {70, 70, 107}` — delanteras oblicuas 70 · trasera 107.
   La trasera se barrió 42→50→70→85→95→100→105→**107**. La trasera vuelve a ser la rueda fuerte, como en 2025,
   pero ahora **sin lazo de gyro**: alcanza con el piso correcto + las otras dos técnicas.
2. **Impulso inicial portado y validado:** `{130, 130, 140}` PWM × **40 ms** en la transición parado→comando
   de cada rueda (gateado `-DCENTRAL_MOTOR_KICKSTART`; factor ×9.9 + cap por rueda = impulso fijo). La trasera
   necesitaba 140 porque con menos "se quedaba".
3. **Freno anticipado portado y validado (en los diags):** `motors_set_rear_cut()` corta la trasera a 0 en los
   últimos **66 ms** del tramo (gateado `-DCENTRAL_REAR_BRAKE_LEAD`, tunable `-DDIAG_STRAFE_REAR_LEAD_MS`) —
   sin esto la inercia de la trasera desacomoda el robot al frenar.

**Física aprendida (hallazgo de Gustavo, confirmado en banco):** el PWM **NO es proporcional a la velocidad**
y es **DISTINTO por rueda**. En el strafe la trasera debe girar al DOBLE que las delanteras (cinemática:
fronts 0.5·vx, rear 1.0·vx), pero como rueda ALINEADA al movimiento (menos fricción que las oblicuas) lo logra
con **~1.5× el PWM (107 vs 70), no 2×**.

**Política (decisión de Gustavo 2026-06-09):** las 3 técnicas (piso por rueda + kickstart + corte anticipado
de la trasera) se usan **SIEMPRE que el robot se mueva lateralmente, en TODOS los programas** → los flags van
ON también en envs de producción.

**ROBOT1:** arranca de los MISMOS valores que R2 (`{70,70,107}` + impulso `{130,130,140}` + lead 66 ms),
con leyenda **A VERIFICAR EN BANCO R1**. Honestidad: el `{70,70,42}` viejo de R1 era de SU banco 2026-06-08
(la trasera se bajó porque el robot rotaba en el strafe) — **si R1 rota con 107, bajar la trasera gradualmente**.

**Lo que sigue faltando:** el plugging (freno por reversa), las rampas de aceleración, la cascada de heading
(plan 2026), y el **glue para llevar el corte anticipado al lateral de la FSM del arquero** (tema-a-analizar:
el corte necesita saber cuándo TERMINA el movimiento; en el control continuo de la FSM no existe ese evento).