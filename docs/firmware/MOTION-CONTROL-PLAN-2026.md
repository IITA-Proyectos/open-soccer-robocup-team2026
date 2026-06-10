# Plan de control de movimiento 2026 — capas, gating y secuencia de banco

> **Fecha:** 2026-06-08
> **Autor del draft:** Claude Opus 4.8 (Anthropic). **Solicitado por:** Gustavo Viollaz.
> **Scope:** plan de portado del control de movimiento del arquero 2025 al pipeline 2026 (CENTRAL / Zircon Teensy 4.1).

---

## ⚡ ACTUALIZACIÓN 2026-06-09 — resultado de banco ROBOT2 (lee esto primero)

Banco de Gustavo con `diag_central_strafe_robot2_kick` (**"anda bien"**). Estado real de las capas:

| Capa | Estado 2026-06-09 |
|---|---|
| **Capa 1 — piso de PWM por rueda** | ✅ **VALIDADA EN BANCO ROBOT2** con `MOTOR_MIN_PWM = {70, 70, 107}` (el banco REFUTÓ la hipótesis ×1.1 de este plan — ver nota en la Capa 1) |
| **Capa 2a — impulso inicial (kickstart)** | ✅ **VALIDADA EN BANCO ROBOT2**: impulso fijo `{130, 130, 140}` PWM × 40 ms |
| **Capa 2b — freno anticipado de la trasera** | ✅ **cableado y validado en los diags de strafe** (corte de la trasera en los últimos 66 ms del tramo). Es la mitad del 2b |
| **Capa 2b — plugging (reversa)** | ⏳ **PENDIENTE** (sin implementar ni validar) |
| **Capa 3 — cascada de heading** | ⏳ **PENDIENTE** (sin cambios) |

**Política (decisión de Gustavo 2026-06-09):** las 3 técnicas validadas (piso + impulso + freno anticipado)
se usan **SIEMPRE que el robot se mueva LATERALMENTE, en TODOS los programas**. Eso significa que los flags
`-DCENTRAL_MOTOR_KICKSTART` y `-DCENTRAL_REAR_BRAKE_LEAD` se prenden **también en envs de producción**
(cambia el binario de competencia a propósito — supera el "todos default OFF" original de este plan para esas
dos técnicas; el resto sigue gateado OFF). ⚠️ OJO build: todo env con `build_src_filter` EXPLÍCITO que active
`-DCENTRAL_MOTOR_KICKSTART` necesita `+<shared/motor_kickstart.cpp>` en el filtro (los envs que compilan todo
`src/` no lo necesitan — verificar el filtro real de cada env antes de tocar).

### Valores FINALES por robot

| Qué (dónde vive) | ROBOT2 — ✅ validado banco 2026-06-09 | ROBOT1 — mismos valores, ⚠️ A VERIFICAR EN BANCO R1 |
|---|---|---|
| Piso de PWM por rueda `MOTOR_MIN_PWM[3]` (`config_central.h`) | `{70, 70, 107}` — delanteras oblicuas 70 · trasera 107 (barrido de la trasera: 42→50→70→85→95→100→105→107) | `{70, 70, 107}` — el `{70,70,42}` viejo de R1 era de SU banco 2026-06-08 (la trasera se bajó porque rotaba en el strafe); **si R1 rota con 107, bajar gradualmente** |
| Impulso inicial por rueda (kickstart, `motors_zircon.cpp`, flag `-DCENTRAL_MOTOR_KICKSTART`) | `{130, 130, 140}` PWM × **40 ms** en la transición parado→comando de cada rueda (implementado como factor ×9.9 + cap POR RUEDA = impulso fijo; la trasera necesitaba 140 porque "se quedaba") | igual `{130, 130, 140}` × 40 ms |
| Freno anticipado de la trasera (`motors_set_rear_cut()` en el mixer, flag `-DCENTRAL_REAR_BRAKE_LEAD`) | corta la TRASERA (idx 2) a 0 en los últimos **66 ms** del tramo (tunable `-DDIAG_STRAFE_REAR_LEAD_MS`) mientras las delanteras terminan — hoy cableado SOLO en `diag_central_strafe.cpp` | igual, 66 ms |

**Física aprendida (hallazgo de Gustavo, confirmado en banco):** el PWM **NO es proporcional a la velocidad**
y es **DISTINTO por rueda**. En el strafe la trasera debe girar al DOBLE de velocidad que las delanteras
(cinemática: fronts 0.5·vx, rear 1.0·vx), pero como rueda ALINEADA al movimiento (menos fricción que las
oblicuas) lo logra con **~1.5× el PWM (107 vs 70), no 2×**.

El resto del documento se conserva como estaba el 2026-06-08 (es el plan original); donde el banco lo superó,
hay notas fechadas en cada capa.

---

## Advertencia (leer antes de tocar nada)

1. **TODO lo que propone este plan está GATEADO por flags de build con default OFF.** Con los flags
   apagados el binario de competencia es **byte-idéntico** al actual. Nada de esto entra a un robot que
   compite hasta que un flag se prende a propósito, en un env de banco, y se valida.
2. **Claude NO cierra TASKs de hardware.** Todo lo que toca PWM, motores, corriente o freno **solo lo
   cierra el equipo humano con la placa, la batería cargada y el robot real.** Que compile, que pasen los
   tests host-native y que el binario sea idéntico con el flag OFF **no** es evidencia de que funcione en
   cancha. Es la regla no negociable del repo.
3. **Los módulos nuevos son PUROS** (sin `Arduino.h`, sin `Wire`, sin `millis()` adentro): el tiempo y el
   estado entran como **parámetros**. Eso los hace host-testeables con `g++` y deja al *caller* como dueño
   del cronómetro y de la transición de estado. La equivalencia numérica con el 2025 se ancla en tests
   Unity host; el comportamiento físico se valida en banco.
4. **El cap de potencia manda.** Los motores 2026 son DC brushed 5V comunes alimentados a **7,4 V**: por
   encima de **~70 % de duty (~150–178 PWM)** se queman. Todas las constantes portadas del 2025 con el
   factor ×1.1 (robot más pesado) que **superan el cap se recortan a ~150**. La seguridad del motor gana
   sobre cualquier asimetría o magnitud histórica. El ×1.1 es una **hipótesis de arranque por masa, no un
   número medido.**

---

## Arquitectura en capas

El pipeline 2026 **no manda PWM directo**: manda **velocidad** (`vx`, `vy`, `omega`) y la cinemática
inversa la reparte a las 3 ruedas. Las capas de este plan **envuelven** ese pipeline sin reemplazarlo:
eligen las velocidades de comando (Capa 1), agregan un impulso/freno temporizado alrededor del PWM ya
calculado (Capa 2), y eligen la fuente de feedback que mantiene el rumbo derecho (Capa 3).

```
                        ┌──────────────────── FSM ARQUERO (strategy.cpp) ────────────────────┐
                        │  decide qué movimiento: PATROL / INTERCEPT / CLEAR / GOTO_LINE      │
                        └───────────────┬───────────────────────────────────┬────────────────┘
                                        │ vx/vy (velocidad de comando)      │ omega (rumbo)
                                        ▼                                   ▼
   ┌─ CAPA 1 ─ valores PWM ─────────────────────┐        ┌─ CAPA 3 ─ cascada de heading ──────────────┐
   │ elige vx/vy para caer en los PWM 2025 ×1.1 │        │ select_heading_source():                   │
   │ (delanteras ~55 / trasera ~110) y CLAMPEA   │        │  BNO → OTOS → cámara/ToF → LAZO ABIERTO    │
   │ al cap. 95% CONFIG + helper puro            │        │ da la fuente + el current_heading; el ω    │
   │ gk_motion_speed.h                           │        │ fino lo pone el HeadingPID (clamp ≤327)    │
   └───────────────┬─────────────────────────────┘        └───────────────┬────────────────────────────┘
                   │ vx/vy elegidas y capeadas                            │ omega corregido (o 0)
                   ▼                                                       ▼
   ┌──────────────────────── MotorCommand {vx_mm_s, vy_mm_s, omega_centideg_s} ───────────────────────────┐
   │                                  PIPELINE 2026 (NO se toca)                                            │
   │   inverse_kinematics(vx,vy,omega, WHEEL_ANGLES{330,210,90})  →  wheel_speed[3]                         │
   │   wheel_speed_to_pwm(MAX_SPEED→255)  →  apply_pwm_floor(MOTOR_MIN_PWM)  →  pwm_base[3]                  │
   └───────────────────────────────────────────┬───────────────────────────────────────────────────────────┘
                                                │ pwm_base[3] (régimen, por rueda)
                                                ▼
   ┌─ CAPA 2a ─ IMPULSO (kickstart) ────────────────────────┐   estado: cronómetro del arranque (caller)
   │ motor_kickstart.h: si está dentro de la ventana (~40ms) │
   │ desde parado→comando, ×1.8 al pwm_base, capeado a 153.  │
   └───────────────┬─────────────────────────────────────────┘
                   ▼
   ┌─ CAPA 2b ─ FRENO (plugging + lead trasera) ────────────┐   estado: timestamp del STOP + last_pwm[3]
   │ motor_brake.h: al pasar a STOP desde velocidad alta,    │
   │ REVERSA breve (~250ms, cap 150) en vez de coast; +      │
   │ apaga la trasera con lead-time antes del fin del strafe. │
   └───────────────┬─────────────────────────────────────────┘
                   ▼
           apply_pwm_to_motor(i, pwm)  → MOTOR_INVERT → analogWrite   (+ motor_power_cap final, si se cablea)
```

**Dónde entra cada capa, en una línea:**

| Capa | Entra en | Qué decide | Naturaleza |
|---|---|---|---|
| **1 — Valores PWM** | `strategy.cpp` (constantes GK) + `config_central.h` (piso) | qué `vx/vy` comanda cada movimiento del arquero | 95 % CONFIG + helper puro |
| **2a — Impulso** | `motors_zircon.cpp` `motors_apply_command()` | pulso de PWM alto y breve al arrancar | módulo puro + estado en caller |
| **2b — Freno** | `motors_zircon.cpp` `motors_plug_brake()` + `main_central.cpp` | reversa breve al frenar + apagado anticipado de M3 | módulo puro + estado en caller |
| **3 — Cascada heading** | `strategy.cpp` `goalkeeper_tick()` (PATROL/INTERCEPT) | qué sensor corrige la deriva del strafe | módulo puro de selección |

---

## Capa por capa

### Capa 1 — Valores de PWM por movimiento (base = arquero 2025 ×1.1)

> **✅ Estado 2026-06-09 — el PISO POR RUEDA quedó VALIDADO EN BANCO ROBOT2:** `MOTOR_MIN_PWM = {70, 70, 107}`
> (delanteras oblicuas 70 · trasera 107; barrido 42→50→70→85→95→100→105→107). El banco **refutó** la hipótesis
> ×1.1 de la tabla de abajo (piso `{77,77,46}` con trasera BAJA): la trasera necesita piso **ALTO** porque debe
> girar al doble que las delanteras y el PWM no es proporcional a la velocidad (ver "Física aprendida" arriba).
> ROBOT1 arranca de los mismos `{70,70,107}` — **A VERIFICAR EN BANCO R1** (si rota, bajar la trasera).
> Las **velocidades GK** de esta capa (PATROL 430, INTERCEPT 588, etc.) siguen siendo plan, **NO validadas**.

**Qué hace.** El insight central: con los ángulos actuales `{330, 210, 90}` un **strafe puro** ya produce
el ratio del arquero 2025 — delanteras `0.5·vx`, trasera `1.0·vx`, es decir **2:1 (trasera fuerte)**, igual
que el 2025. **No hace falta tocar la cinemática ni el mixer.** Alcanza con **elegir** las velocidades de
comando del arquero para que el mapa velocidad→PWM caiga en los PWM 2025 ×1.1 (delanteras ~55 / trasera
~98–110) y **acotarlas** para que el peor caso por rueda no pase ~150 PWM. Es **95 % CONFIG** (subir
`GK_PATROL_SPEED` de 150 a ~430 mm/s, etc.) + un helper PURO opcional que hace la cuenta inversa
"PWM objetivo → vx/vy" y clampea al cap, para poder host-testear la equivalencia.

**Archivos puros nuevos.**
- `software/teensy/Soccer 2026/src/shared/gk_motion_speed.h` (+ `.cpp`) — cuenta inversa
  "PWM objetivo de rueda dominante → velocidad de comando" + clamp al cap. Sin Arduino, host-testeable.
- `software/teensy/Soccer 2026/test/test_gk_motion_speed/test_main.cpp` — Unity host que ancla la
  equivalencia: `gk_strafe_speed(55 PWM front) ≈ 430 mm/s`, el strafe a 430 reparte `{55,55,110}` por la
  cinemática real, y el clamp recorta el ×1.5 a 588 (trasera = 150).

**Constantes.**

| Nombre | Valor 2026 | Origen 2025 |
|---|---|---|
| `GK_PATROL_SPEED_MM_S` | **430.0f** (strafe front 55 / rear 110 PWM = 50×1.1) | strafe delanteras 50 / trasera 89; hoy **150.0f** (`strategy.cpp:115`) — demasiado bajo, el piso domina |
| `GK_INTERCEPT_SPEED` (o `patrol×1.5` clamped) | **588.0f** (×1.5=646 daría rear 165 > cap → clamp a 588, rear=150) | strafe acelerado `pd=1.5` al ver pelota: 75 / ~133 ×1.1 |
| `GK_CLEAR_SPEED_MM_S` | **500.0f** (avance vy: front 110 = avanzar 100 ×1.1, trasera 0) | avanzar 100 / 0; hoy **500.0f** ya correcto (`strategy.cpp:143`). Tope seguro 679 (front 150) |
| `GK_LINE_RETREAT_SPEED` | **275.0f** (retroceso despeje 150 ×1.1 a nivel velocidad) | retroceso 150/150/0; hoy **250.0f** (`strategy.cpp:118`) |
| `MAX_SPEED_MM_S` | **1000.0f — NO CAMBIA** (es la *pendiente* del mapa) | N/A (constante del pipeline 2026, `config_central.h:119`) |
| `MOTOR_MIN_PWM[3]` (piso) | **{77,77,46}** (= {70,70,42}×1.1) — **fallback balanceado de lazo abierto** | piso {70,70,42} (`config_central.h:53`); trasera BAJA a propósito sin lazo de heading |

**Integración + flag.** Punto: `strategy.cpp` (`goalkeeper_tick()`, ramas PATROL/INTERCEPT/CLEAR/GOTO_LINE)
+ `config_central.h:53` para el piso. **Flag `-DGK_PWM_2025_TUNE` (default OFF).** Con OFF el binario es
idéntico (PATROL=150, RETREAT=250, piso {70,70,42}). Con ON entran PATROL=430, INTERCEPT clamp 588,
RETREAT=275 y piso {77,77,46}. **El piso ×1.1 se gatea en la misma rama `#ifdef`**, NO pisando
`config_central.h` directo (para no cambiar el byte default). En INTERCEPT se agrega un clamp aditivo
`gk_clamp_strafe_speed_to_cap(...)` que es **no-op** si la velocidad ya está bajo el cap → fallback exacto.

**Caveats.**
- **NO es un módulo nuevo de control: es 95 % CONFIG.** El helper existe para host-testear la equivalencia
  y aplicar el clamp; no reemplaza el pipeline. Si el equipo quiere cero código nuevo, basta con las
  constantes gateadas.
- **La trasera-fuerte (rear 2× front) SOLO es segura CON lazo de heading activo.** Hoy el BNO está roto
  (`heading_valid=0` → omega gateado a 0). En lazo abierto sin BNO, subir PATROL a 430 puede hacer **rotar**
  el robot en el strafe. Por eso el piso queda como fallback balanceado {77,77,46} (trasera baja) y **el
  flag NO se activa en competencia hasta que el BNO esté sano** (TASK-207).
- **El piso actúa DESPUÉS del mapa velocidad→PWM y puede LEVANTAR un PWM bajo.** A PATROL=430 el strafe da
  front 55 / rear 110, pero 55 < piso 77 → el piso sube las delanteras a 77 y **rompe el ratio 2:1**. Es el
  comportamiento actual y por qué el strafe no es proporcional con BNO roto. El ratio 2025 real (front 55 /
  rear 98) solo se recupera con BNO sano + piso desactivado o muy bajo.
- **NO se baja `MAX_SPEED_MM_S`** a propósito: `wheel_speed_to_pwm` mapea `MAX_SPEED→255` siempre, así que
  bajarlo reescalaría TODAS las velocidades del repo (drive_straight, attacker, diags). El cap se logra
  acotando las velocidades de comando + el clamp.
- **El intercept queda en ×1.37, no ×1.5**, por el cap (646 → 588). Documentado para que no parezca bug;
  con mejores motores el cap sube y vuelve el ×1.5 completo.
- **Todos los valores salen de 2025 ×1.1; el ×1.1 es hipótesis por peso, NO medición.** Cierre = banco.

---

### Capa 2a — Impulso inicial anti-inercia (kickstart) en CENTRAL

> **✅ Estado 2026-06-09 — VALIDADA EN BANCO ROBOT2** (env `diag_central_strafe_robot2_kick`, "anda bien").
> Valores finales: impulso **FIJO por rueda `{130, 130, 140}` PWM × 40 ms** en la transición parado→comando.
> La implementación real difiere del draft de abajo: factor `×9.9` (`KICKSTART_FACTOR_X10=99`) + **cap POR
> RUEDA** `{130,130,140}` — el factor satura siempre contra el cap, así que en la práctica es un impulso fijo
> (no el ×1.8 con cap único 153 del plan). La trasera necesitaba 140 porque con menos "se quedaba".
> Por decisión de Gustavo, `-DCENTRAL_MOTOR_KICKSTART` va ON en TODOS los programas con movimiento lateral
> (también producción). ROBOT1: mismos valores, **A VERIFICAR EN BANCO R1**.

**Qué hace.** Reproduce el "impulso inicial" del arquero 2025: al detectar la transición **parado→comando**,
multiplica el PWM base de cada rueda por un factor (~1.8) durante una ventana corta (~40 ms) para vencer el
rozamiento estático (stiction), y al cerrarse la ventana deja pasar el PWM base sin tocar. **No es una rampa:
es un escalón temporizado.** La función es PURA: `ms_desde_arranque` y `pwm_base` entran como parámetros; el
dueño del cronómetro y de la transición es el caller. El boost respeta SIEMPRE el cap (~150–153 PWM).

**Archivos puros nuevos.**
- `src/shared/motor_kickstart.h` (+ `.cpp`) — `motor_kickstart_pwm(pwm_base, ms_since_start, window_ms, factor_x10, cap_abs)`.
- `test/test_motor_kickstart/test_main.cpp` — Unity host: boost dentro de la ventana, passthrough fuera,
  cap recorta, signo conservado, gates no-op (window≤0, factor≤10, base=0), bordes (ms=0, ms=window-1,
  ms=window, INT_MIN sin UB).

**Constantes.**

| Nombre | Valor 2026 | Origen 2025 |
|---|---|---|
| `KICKSTART_FACTOR` (`MOTOR_KICKSTART_FACTOR_X10`) | **1.8** (entero ×10 = 18; ROBOT2 = 10 = no-op) | factor 1.8 hardcodeado (M1/M2=1.8·50=90, M3=1.8·85=153) |
| `KICKSTART_WINDOW_MS` | **40** (es tiempo, NO se escala ×1.1) | 40 ms hardcodeado, "durante 40 ms" |
| `KICKSTART_PWM_CAP` | **153** (= M3 boosteado real 2025, 1.8·85; ~60 % de 255) | M3 = 153, el pico más alto que el robot 2025 usó sin quemar |

> El **×10 % por robot más pesado va al PWM BASE** (el caller manda el base 10 % más alto), **NO al factor
> 1.8** ni a la ventana.

**Integración + flag.** Punto: `motors_zircon.cpp` `motors_apply_command()`, la línea del boost va **después
de `apply_pwm_floor`** y **antes de `apply_pwm_to_motor`**. **Flag `-DCENTRAL_MOTOR_KICKSTART` (default OFF)**,
env nuevo `[env:central_robot1_kickstart]` que extiende `central_robot1`. El estado (millis del último
arranque + flag "parado") es **local de archivo, gateado**: solo existe con el flag → sin el flag, NO se
agrega ni estado ni llamada → binario idéntico. `moving = (vx≠0 || vy≠0 || omega≠0)`; al pasar parado→moving
se re-arma el reloj.

**Caveats.**
- **El módulo es puro pero el impulso real NECESITA estado** (cronómetro + detección de transición), que vive
  en el caller. Si la FSM nunca manda exactamente 0 entre movimientos (jitter), el impulso podría no re-armarse
  en cada arranque → a verificar en banco. Mitigación futura: re-armar también al cambiar de signo/dirección
  (el 2025 re-impulsaba 350 ms al cambiar de sentido).
- **El cap 153 es ligeramente > 150.** Se dejó en 153 porque es un valor que el robot 2025 usó sin quemarse;
  si el equipo prefiere el 150 estricto, `KICKSTART_PWM_CAP=150` (el M3 pierde 3 PWM, despreciable).
- **El ×10 % va al base, NO al kickstart.** Hoy el "base" es lo que sale de `wheel_speed_to_pwm` +
  `apply_pwm_floor` (piso {70,70,42} ya tuneado en banco 2026-06-08). Si esos pisos ya absorben el peso extra,
  el ×10 % explícito puede ser redundante. Decisión a confirmar con el equipo.
- **No combinar con `CENTRAL_SLOW_MOTION`** al medir el kickstart: el `MOTION_SCALE=0.7` baja el base antes
  de la cinemática y los números del banco no coincidirían con competencia.
- **ROBOT2 — SUPERADO 2026-06-09 (banco):** pines NO rotados (iguales a R1),
  `MOTOR_INVERT={+1,+1,+1}`, `MOTOR_MIN_PWM={70,70,107}` y `MOTOR_EFF_X100={100,100,131}`
  validados en banco (config_central.h, rama ROBOT2). El kickstart de R2 usa impulso FIJO
  con cap por rueda {130,130,140} (motors_zircon.cpp).

---

### Capa 2b — Freno: plugging (reversa) + freno anticipado de la trasera

> **Estado 2026-06-09 — PARCIAL.** El **freno anticipado de la trasera quedó ✅ cableado y validado en los
> diags de strafe**: `motors_set_rear_cut()` en el mixer (gateado `-DCENTRAL_REAR_BRAKE_LEAD`) corta la
> TRASERA (idx 2) a 0 en los últimos **66 ms** del tramo (tunable `-DDIAG_STRAFE_REAR_LEAD_MS`) mientras las
> delanteras terminan — sin esto la inercia de la trasera desacomoda el robot al frenar. Hoy está cableado
> **SOLO en `diag_central_strafe.cpp`**. El **plugging (reversa) sigue PENDIENTE** (sin implementar ni validar).
>
> **Tema-a-analizar (NO implementado a propósito): llevar el corte anticipado al lateral de la FSM del arquero**
> (`strategy.cpp`, PATROL/INTERCEPT — zona prohibida sin análisis previo). El corte necesita saber **cuándo
> TERMINA el movimiento**; en el control continuo de la FSM ese evento no existe (el strafe termina por evento
> asíncrono — pisó línea / interceptó — no por tiempo). Es glue futuro, mismo punto que ya marca el caveat
> "movimientos de duración conocida" más abajo.

**Qué hace.** Dos primitivas de freno puras portadas del 2025:
1. **Plugging (freno por reversa).** Dado el último comando de rueda y los ms desde el STOP, devuelve un PWM
   de **reversa** durante una ventana corta (200–300 ms) y luego coast. Frena mucho más rápido que el coast y
   **no depende del short-brake** del Zircon (que sigue sin confirmarse).
2. **Freno anticipado de la trasera.** Dado si la rueda va paralela al movimiento (M3 en strafe) y la
   velocidad, devuelve el **lead-time (ms)** con que hay que apagarla **antes** del fin del movimiento para que
   su inercia no desvíe el rumbo.

**Archivos puros nuevos.**
- `src/shared/motor_brake.h` (+ `.cpp`) — `plug_brake_compute(in, window_ms, cap_abs)` y
  `rear_brake_lead_ms(is_parallel, vel_pct, base_ms, max_ms)`.
- `test/test_motor_brake/test_main.cpp` — Unity host: reversa dentro / coast fuera, signo opuesto al último
  comando, cap 150 sobre crudas 275/187 (asimetría perdida), gate OFF (cap≤0 = passthrough), lead-time
  escalado ×VEL/100 con clamp 0–300, bordes.

**Constantes.**

| Nombre | Valor 2026 | Origen 2025 |
|---|---|---|
| `PLUG_PWM_M1_RAW` | **275** (250×1.1) → **CAPEADO a 150** | M1=250 (`retroceder_patear`) |
| `PLUG_PWM_M2_RAW` | **187** (170×1.1) → **CAPEADO a 150** ⚠️ pierde asimetría | M2=170 (asimétrico para compensar mecánica) |
| `PLUG_WINDOW_MS` | **250** (medio del rango; es tiempo, NO ×1.1) | 200 ms / 300 ms |
| `BRAKE_LEAD_BASE_MS` | **66** (60×1.1 a VEL=100; ×VEL/100; clamp 0–300) | `BASE_ANTICIPACION_MS=60`, auto-cal ±5 ms si drift>1.5° |
| `PLUG_PWM_CAP` | **150** (~70 %) — **el cap manda** | no existe en 2025 (gap: 250/170 se mandaban crudos) |

**Integración + flag.** Modo nuevo **hermano** de `motors_brake()`:
`motors_plug_brake(const int last_wheel_pwm[3], int ms_since_stop)` en `motors_zircon.cpp`, gateado por
**`-DCENTRAL_PLUG_BRAKE` (default OFF)**, env `[env:central_robot1_plugbrake]`. La FSM lo invoca en
`main_central.cpp` al pasar a STOP desde velocidad alta (en vez de `motors_stop()` directo). El M3 (trasera)
lleva `raw=0` → sin reversa (coherente con el histórico). El caller guarda `g_last_wheel_pwm[3]` (static
gateado) + el timestamp del STOP y pasa `ms_since_stop` como parámetro (el módulo no lee `millis()`). Sin el
flag: `motors_plug_brake()` cae a `motors_stop()` (coast normal) → binario idéntico.

**Caveats.**
- **El cap rompe la asimetría.** La gracia del plugging 2025 era M1=250 / M2=170 asimétrico para compensar las
  dos delanteras. Con cap 150 ambas quedan en 150 → el robot **podría desviarse al frenar**. Si el banco lo
  muestra, capear cada rueda por separado conservando la proporción (p.ej. M1=150, M2=102=150·170/250), no
  compartir 150. Lo dirime el banco.
- **REVERSA = corriente alta a motores 5V al límite.** El criterio (d) de la test-card (sin sobrecalentar tras
  ciclos) es **obligatorio** antes de competencia. **NO lo cierra Claude.**
- **`motors_brake()` (short-brake) sigue sin confirmarse en el Zircon** (puede quedar en coast según el driver).
  El plugging no depende de eso. Conviene medir las **tres** opciones (coast / short / plug) en la misma
  test-card para tener el comparativo.
- **El freno anticipado de la trasera necesita saber CUÁNDO termina el movimiento.** En 2025 los movimientos
  eran temporizados (fin predecible). En la FSM 2026 el strafe termina por **evento** (pisó línea /
  interceptó), no por tiempo → el lead-time no se puede aplicar "antes del fin" si el fin es asíncrono.
  `rear_brake_lead_ms()` queda **lista para movimientos de duración conocida**; para los reactivos el
  mecanismo correcto es el plugging (actúa DESPUÉS del STOP).
- **Las crudas 275/187/66 son hipótesis de escalado por masa, no medidas.**

---

### Capa 3 — Lazo de control en cascada (fuente de rumbo/deriva del strafe del arquero)

> **Estado 2026-06-09: PENDIENTE, sin cambios.** Sigue siendo plan.

**Qué hace.** Implementa la **cascada de prioridad** de feedback de rumbo para corregir la deriva del strafe.
Dos funciones puras: `select_heading_source(...)` elige la mejor fuente disponible, y
`compute_heading_correction(...)` devuelve la ω de corrección (deg/s) o cae a ω=0. La gracia es la
**degradación elegante**: si la mejor fuente no está, baja a la siguiente en vez de caer de golpe a lazo
abierto.

**Archivos puros nuevos.**
- `software/teensy/Soccer 2026/src/shared/heading_source.h` (+ `.cpp`) — enum `HeadingSource`, structs de
  inputs/cfg/correction, `select_heading_source()` + `compute_heading_correction()` + `heading_cascade()`.
- `software/teensy/Soccer 2026/test/test_heading_source/test_main.cpp` — Unity host: cada fallback en orden,
  ley P (signo del error), OPEN_LOOP/TOF_WALLS → ω=0, wrap de angdiff.

**Constantes.**

| Nombre | Valor 2026 | Origen 2025 |
|---|---|---|
| `HS_OPEN_LOOP_IMPULSE_PWM` | **~165** (150×1.1) → **EXCEDE cap → clamp 150** | impulso 150 PWM / 70 ms (delantero); 1.8× (arquero) |
| `HS_GK_STRAFE_BASE_MM_S` | **165** (150×1.1) | `GK_PATROL_SPEED_MM_S=150` (`strategy.cpp:115`) |
| `HS_KP_HEADING_DEG_S_PER_DEG` | **3.0** (ganancia, sin ×1.1) | `HeadingPID.kp=3.0` (`pids.h:49`) |
| `HS_KP_OTOS_LATERAL` | **0.5** (ganancia, sin ×1.1) | `DriveStraightCfg.kp_lateral=0.5` |

> Las constantes de **impulso/velocidad NO viven en `heading_source`** (es un módulo puro de **selección**,
> no toca PWM). Se documentan acá porque la pregunta lo pide; su lugar es `config_central.h` / el caller.

**Integración + flag.** Punto: `strategy.cpp` `goalkeeper_tick()`, estados **PATROL e INTERCEPT** (donde hoy
se computa `cmd.omega_centideg_s = gk_own_goal_orient_omega(now_ms)`). **Flag `-DCENTRAL_HEADING_CASCADE`
(default OFF)**, env `[env:central_robot1_heading_cascade]`. Sin el flag: byte-idéntico (sigue llamando
`gk_own_goal_orient_omega()`). Con el flag: arma los inputs del world_model y llama la cascada.

> **Variante recomendada de integración:** que la cascada **solo elija la FUENTE + el current_heading**, y que
> el ω final lo siga calculando el `g_heading_pid` del caller — para **no perder el anti-windup ni el clamp
> ≤327** del `HeadingPID` (crítico: el bug de sign-flip int16 del 2026-06-03, donde `omega·100` desbordaba el
> int16 y giraba invertido). El draft también trae una ley P directa, pero la magnitud fina la debe poner el
> PID. Decidir cuál de las dos en la sesión pio; ambas gateadas, ambas idénticas con el flag OFF.

**Caveats** (ver detalle en la sección siguiente): de las 4 ramas, **solo BNO y OTOS tienen dato de rumbo
real y fresco hoy**; la rama cámara existe pero depende de visión sin recalibrar (TASK-022); la rama ToF-pared
**no se construye** y queda dormida. Depende del **BNO sano** como fix de fondo. **No host-verificado en este
entorno** (cwd greenfield).

---

## La cascada de heading en detalle

**Orden de prioridad (de mejor a peor):**

```
   (1) BNO            heading absoluto del snapshot (flags bit4 heading_valid)   ── CABLEADO, consumido hoy
        │  no disponible →
   (2) OTOS del piso  heading de la odometría que DOWN difunde, si está fresco   ── CABLEADO, dato real
        │  no disponible →
   (3) CÁMARA-OBJETO  ángulo al ARCO PROPIO como referencia de rumbo             ── CABLEADO, dato DUDOSO
        │  no disponible →
  (3.5) TOF-PAREDES   [RESERVADO — NO cableado, nunca se elige hoy]              ── DORMIDA
        │  no disponible →
   (4) LAZO ABIERTO   sin corrección (solo impulso + freno; ω = 0)               ── = conducta de hoy
```

**Qué dato existe HOY (de las fuentes de datos disponibles):**

| Rama | Fuente concreta | Estado |
|---|---|---|
| **(1) BNO** | `WorldSnapshot.my_heading_centideg` + flags bit4 → `world_model_heading_valid()` / `world_model_get_my_heading_deg()` | **CABLEADO y consumido hoy** (gate `central_gate_heading_omega`) |
| **(2) OTOS** | `world_model_otos_is_fresh()` + `world_model_get_otos_heading_deg()` | **CABLEADO hoy** (usado por drive_straight en KICKOFF/APPROACH). Esta rama **sí tiene dato real** |
| **(3) Cámara** | `WorldSnapshot.goal_own_visible` + `goal_own_angle_centideg` → `world_model_goal_own_visible()` / `world_model_get_goal_own_angle_deg()` | **CABLEADO** y consumido por `gk_own_goal_orient` para ORIENTAR. ⚠️ **depende de visión recalibrada (TASK-022, bloqueante real #1)** → el dato fluye pero su calidad hoy es dudosa |
| **(3.5) ToF-pared** | — | **FALTA CABLEAR.** El snapshot solo trae `min_obstacle_mm` **escalar** (un único mínimo de 4 ToF + HC-SR04), **sin vectores de pared por dirección** → no alcanza para derivar un rumbo. La rama queda **RESERVADA** en el enum; `select_heading_source` **nunca la elige** (`tof_walls_available` siempre false). Cablear requeriría que el TOP exporte distancias ToF por dirección en el snapshot (campo inexistente) |

**Ramas que DUERMEN hasta cablear:** la cascada se construye **completa** (4 niveles), pero **2 de 4 ramas
duermen**: la de cámara existe con dato de calidad dudosa hasta TASK-022, y la de ToF-pared no se construye
hasta que haya vectores de pared en el snapshot. **BNO y OTOS son las únicas operativas hoy.**

**Complemento, no fuente de heading:** existe `world_model_get_cross_track_mm()` / `cross_track_valid()` y el
arquero ya lo usa para el **PID lateral** (distancia perpendicular a la línea). Es deriva **lateral**, NO
**rumbo** → es complementario a esta cascada (corrige posición, no orientación), no una fuente de heading.

**Fallback exacto:** si **ninguna** fuente está disponible → `source=OPEN_LOOP`, `valid=false`, `ω=0`. Eso es
**exactamente** la conducta de hoy con el BNO roto (el gate fuerza ω=0). El módulo **no inventa corrección sin
dato.**

---

## Estrategia de gating

Cada capa tiene su flag, su env y su binario. **Todos default OFF → competencia byte-idéntica.**

> **Actualización 2026-06-09:** este "todos default OFF" quedó **superado para 2 flags** por decisión de
> Gustavo: `-DCENTRAL_MOTOR_KICKSTART` y `-DCENTRAL_REAR_BRAKE_LEAD` se prenden **también en envs de
> PRODUCCIÓN** (las técnicas van siempre que el robot se mueva lateralmente; cambia el binario a propósito).
> En los comentarios del `platformio.ini`, R1 queda marcado **A VERIFICAR EN BANCO R1**. El resto de los
> flags (`GK_PWM_2025_TUNE`, `CENTRAL_PLUG_BRAKE`, `CENTRAL_HEADING_CASCADE`) sigue default OFF.

| Flag de build | Env de banco | Capa | Toca el binario solo si | Estado HW |
|---|---|---|---|---|
| `-DGK_PWM_2025_TUNE` | `central_robot1_gk_pwm_tune` | 1 | ON | requiere BNO sano para PATROL alto |
| `-DCENTRAL_MOTOR_KICKSTART` | `central_robot1_kickstart` | 2a | ON | medir stiction + temperatura |
| `-DCENTRAL_PLUG_BRAKE` | `central_robot1_plugbrake` | 2b | ON | medir corriente/temperatura (reversa) |
| `-DCENTRAL_HEADING_CASCADE` | `central_robot1_heading_cascade` | 3 | ON | medir drift por fuente |

**Cuáles se activan juntos.**
- Cada flag se valida **aislado primero** (un binario, una variable).
- **`GK_PWM_2025_TUNE` + `CENTRAL_HEADING_CASCADE` van de la mano:** el PWM alto del strafe (Capa 1) **solo es
  seguro con el lazo de heading activo** (Capa 3 con BNO sano). Activar Capa 1 sin Capa 3 con BNO roto = riesgo
  de que el robot rote. **No subir PATROL a 430 en competencia hasta tener BNO sano + cascada.**
- **NO combinar ningún flag con `CENTRAL_SLOW_MOTION`** al medir: el `MOTION_SCALE=0.7` distorsiona los PWM y
  los números del banco no coincidirían con competencia.
- **Orden recomendado de los caps en el path final** (si además se cablea `motor_power_cap`): floor →
  kickstart → power_cap → invert → analogWrite (el power_cap es la **red final** que garantiza ≤ cap).

**Orden de bring-up.**
1. **Host primero, siempre.** `bash scripts/run-host-tests.sh test_<modulo>` (o `pio test -e test_native -f
   test_<modulo>`) verde **antes** de tocar hardware.
2. **Diff de binario con flag OFF.** `pio run -e central_robot1` (sin flag) y comparar el `.elf/.hex` contra el
   build previo → **byte-idéntico**. Confirma que el módulo shared no contaminó el binario de competencia.
3. **Flag ON, aislado, en banco.** Un flag por vez, ruedas en el aire primero, luego en el piso.

---

## Secuencia recomendada (la que pidió Gustavo)

### Paso 1 — Valores de PWM ×1.1 (Capa 1)

- **Host-testeable (lo verifica Claude):** la equivalencia "PWM 2025 ×1.1 ↔ velocidad de comando" y el clamp
  del cap, anclados en `test_gk_motion_speed` (strafe 55 PWM ≈ 430 mm/s; 430 → `{55,55,110}`; ×1.5 clampeado a
  588 con rear=150).
- **Necesita banco (lo cierra el equipo):** el PWM efectivo por rueda medido con tacómetro, el **drift en lazo
  abierto** (¿rota mientras strafea con BNO roto?), corriente/temperatura tras 30 s, y la latencia de arranque
  vs el binario default.

### Paso 2 — Impulso + freno (Capa 2a + 2b)

- **Host-testeable (lo verifica Claude):** la lógica de los dos módulos puros — boost dentro/fuera de la
  ventana, signo, cap, gates no-op, bordes sin UB (`test_motor_kickstart`); reversa dentro/coast fuera, signo
  opuesto, cap 150, pérdida de asimetría, lead-time ×VEL/100 con clamp (`test_motor_brake`).
- **Necesita banco (lo cierra el equipo):** ¿el kickstart **rompe la inercia** al primer comando? (arranques
  limpios / 5); ¿el plugging **reduce la distancia de frenado** sin retroceder de más?; ¿la **reversa quema**
  los motores tras ciclos?; ¿el robot se **desvía al frenar** por la asimetría perdida? Nada de esto es
  host-verificable: **es corriente, inercia y temperatura reales.**

### Paso 3 — Cascada de heading (Capa 3)

- **Host-testeable (lo verifica Claude):** la lógica de **selección** (cada fallback en orden; ToF nunca se
  elige hoy) y la **corrección** (ley P proporcional al error; OPEN_LOOP/ToF → ω=0; fallback exacto)
  (`test_heading_source`).
- **Necesita banco (lo cierra el equipo):** el **drift del chasis** medido tras 3 barridos por cada fuente
  (BNO / OTOS / cámara / lazo abierto), y la confirmación de que con el flag OFF el comportamiento es idéntico.
  La calidad real de la rama cámara **depende de TASK-022 (visión recalibrada)**.

> **Por qué este orden:** Capa 1 da el *qué* tan rápido va el arquero; Capa 2 lo hace *arrancar y frenar* bien;
> Capa 3 lo mantiene *derecho*. Sin Capa 3 (lazo) la Capa 1 a velocidad alta es riesgosa → por eso el PWM alto
> espera al BNO sano. Mejora corta y bien documentada > mejora ambiciosa y opaca.

---

## Cartas de banco

> **Precondición común a las 4:** batería **CARGADA (>7,9 V)**. Con <7,6 V el robot no se mueve, la línea se
> degrada y el banco mide cualquier cosa — cargar/cambiar **antes** de debuggear. Placa: **CENTRAL (Zircon
> Teensy 4.1) del ARQUERO = ROBOT1**. Ruedas en el aire primero, luego en el piso de la cancha.
> **Ninguna de estas TASKs la cierra Claude — solo el equipo con la placa.**

### Carta 1 — Capa 1 (valores PWM)
- **ENV:** `pio run -e central_robot1_gk_pwm_tune` (= `central_robot1` + `-DGK_PWM_2025_TUNE`).
- **Pasos:** flashear → forzar PATROL (bench, sin pelota) → medir el PWM efectivo por rueda en strafe puro
  (tacómetro óptico / conteo de vueltas/seg, o telemetría DOWN/monitor-base).
- **Qué medir:** (a) PWM front vs rear en régimen; (b) ¿rota mientras strafea en lazo abierto? grados de drift
  por metro; (c) corriente/temperatura tras 30 s; (d) latencia de arranque desde parado.
- **Criterio numérico:** trasera **≤ 150 PWM siempre** (incl. intercept ×1.5 — si supera, el clamp falló);
  delanteras **50–60** y trasera **95–115** en PATROL régimen; **drift en lazo abierto < 10°/m** (si es mayor,
  bajar la trasera del piso o NO subir PATROL hasta tener BNO sano); ninguna rueda **> 50 °C** tras 30 s; el
  arranque debe ser **≥30 % más rápido** que el default.
- **Feedback a devolver:** tabla `{movimiento, PWM front, PWM rear, drift°/m}`; si rota mucho en lazo abierto →
  confirmar si {77,77,46} alcanza o hay que bajar más la trasera; si quema → bajar `GK_PWM_CAP`.

### Carta 2a — Capa 2a (impulso/kickstart)
- **ENV:** `pio run -e central_robot1_kickstart -t upload`. Baseline: confirmar que `central_robot1` (sin
  kickstart) ya mueve el robot.
- **Pasos:** (1) `central_robot1` normal: strafe desde parado en frío **5 veces**, anotar arranques limpios vs
  zumbido/arranque tardío/desvío. (2) `central_robot1_kickstart`: repetir las mismas 5. (3) Comparar.
- **Qué medir:** arranques limpios / 5, tiempo subjetivo hasta moverse, temperatura de los 3 motores al tacto
  tras **10 arranques** seguidos.
- **Criterio numérico:** con kickstart, **≥4 de 5** arranques en frío rompen inercia al primer comando, **sin**
  que ningún motor pase de "tibio" tras 10 ciclos (proxy de no superar el cap 153). Si arranca igual de mal →
  subir `KICKSTART_WINDOW_MS` 40→60 o factor 18→20 (**nunca** subir el cap sin medir temperatura). Si da un
  "tirón" que descalibra el rumbo → bajar la ventana 40→30.
- **Feedback a devolver:** arranques-limpios/5 (con y sin flag), si hubo tirón, si calentaron, y el
  window/factor/cap final.

### Carta 2b — Capa 2b (freno/plugging)
- **ENV:** `pio run -e central_robot1_plugbrake -t upload`. Batería **>8,0 V** (la reversa exige más).
- **Pasos:** (1) Host: `test_motor_brake` **12/12 PASS**. (2) Diff `.elf/.hex` de `central_robot1` (sin flag) →
  **byte-idéntico**. (3) Avance a PWM alto ~1 s → STOP; filmar con cámara cenital **120+ fps** (o slow-mo del
  celular): marcar posición en el instante del STOP y donde queda detenido.
- **Qué medir:** **distancia de deslizamiento** tras STOP (mm) — plugging ON vs coast (`motors_stop()`) vs short
  (`motors_brake()`); **desvío de heading** (grados, del snapshot del TOP) entre STOP y reposo, con y sin freno
  anticipado de la trasera en strafe.
- **Criterio numérico:** (a) plugging reduce la distancia de frenado **≥30 %** vs coast **sin** que el robot
  retroceda más de **~20 mm** pasada la parada (si retrocede, bajar `PLUG_WINDOW_MS` 250→200 o cap 150→120);
  (b) desvío de heading post-STOP en strafe **≤1,5°** (mismo umbral que la auto-cal 2025; si >1,5°, subir
  `BRAKE_LEAD_BASE_MS` en pasos de 5 ms: 66→71→76); (c) **ningún** motor supera 150 PWM en la reversa (leer por
  telemetría/serial); (d) tras **20 ciclos** de freno, los motores **no** se calientan anormalmente al tacto.
- **Feedback a devolver:** tabla `{coast, short, plug} → {distancia mm, desvío°}`; si se desvía al frenar →
  capear las delanteras por separado conservando la proporción.

### Carta 3 — Capa 3 (cascada de heading)
- **ENV:** `pio run -e central_robot1_heading_cascade -t upload`. TOP mandando snapshot por Serial7, DOWN por
  Serial1. Robot sobre la línea del arco, referencia recta (cinta) en el piso.
- **Pasos:** (1) `central_robot1` NORMAL: START de árbitro, PATROL ~30 s, **filmar de arriba** y medir el
  ángulo que rota el chasis tras 3 barridos. (2) `central_robot1_heading_cascade`: idéntico. (3) Provocar cada
  rama: BNO sano (heading_valid=Y) → usa BNO; tapar/desconectar BNO con OTOS fresco → sigue derecho por OTOS;
  sin BNO ni OTOS pero viendo el arco propio → orienta por cámara; sin nada → LAZO ABIERTO (ω=0).
- **Qué medir:** grados de **rotación del chasis (drift)** tras 3 barridos, por fuente.
- **Criterio numérico:** con cascada + BNO sano, **drift < 5°** tras 3 barridos (hoy en lazo abierto rota
  visiblemente); en degradación a OTOS, **drift < 10°**; en LAZO ABIERTO el comportamiento debe ser
  **byte-idéntico** al `central_robot1` normal (mismo drift que hoy → confirma el fallback exacto).
- **Feedback a devolver:** drift por fuente; confirmar que con flag OFF el arquero se comporta exactamente como
  antes.

---

## Riesgos

- **Cap de potencia en la reversa (P0 de seguridad).** El plugging mete **reversa de corriente alta** a
  motores 5V ya al límite (7,4 V). El cap 150 lo limita, pero **solo el banco con termómetro/tacto** confirma
  que no se queman tras ciclos. Es el riesgo más caro: un motor quemado en Incheon deja al robot fuera. El
  criterio (d) de la Carta 2b es **obligatorio**.
- **El cap rompe la asimetría del freno.** 275/187 → ambas a 150 → el robot puede **desviarse al frenar**.
  Mitigación: capear por rueda conservando la proporción, si el banco lo muestra.
- **Estado temporal nuevo en el mixer.** Las Capas 2a/2b agregan **estado** (cronómetro de arranque, timestamp
  de STOP, `last_wheel_pwm[3]`) en `motors_zircon.cpp` / `main_central.cpp`. Aunque gateado, es lógica de
  tiempo que **antes no existía** en el path de motores. Riesgos: que la transición parado→comando no se detecte
  por jitter del comando (el impulso no re-arma), o que el `ms_since_stop` se desincronice. **Mantener el estado
  gateado** para que el binario sin flag siga idéntico, y validar la detección de transición en banco.
- **Ramas de cascada sin dato.** 2 de las 4 fuentes de heading **duermen**: cámara (calidad dudosa hasta
  TASK-022) y ToF-pared (no construida — el snapshot solo trae `min_obstacle_mm` escalar). La cascada **no
  falla** por esto (cae al siguiente nivel), pero **no hay que vender 4 niveles de redundancia cuando hoy solo
  2 son reales**. El fix de fondo del rumbo sigue siendo **BNO a un bus aparte** (TASK-207), no este módulo.
- **PWM alto sin lazo (interacción Capa 1 × Capa 3).** Subir PATROL a 430 con BNO roto puede hacer **rotar** el
  robot en el strafe (trasera-fuerte sin corrección). **No activar `GK_PWM_2025_TUNE` en competencia hasta
  tener BNO sano + cascada.** Mientras tanto, el piso balanceado {77,77,46} es el puente.
- **No host-verificado en este entorno.** El cwd es el repo greenfield (`futbol2026/`), no el real
  (`iitasoccer/soccer-main/`). Los drafts compilan contra `src/shared` por construcción (Arduino-free), pero el
  equipo **debe** correr `run-host-tests.sh` y `pio run -e <env>` antes de cablear. El gate default-OFF
  garantiza binario idéntico, pero **eso no sustituye el test en hardware real.**
