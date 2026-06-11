---
title: "Patrulla lateral del arquero (diag_central_strafe) — OPEN-LOOP"
date: 2026-05-31
status: vivo
audiencia: "Virginia / Elías / Enzo — operativos en el banco"
firmware-source: software/teensy/Soccer 2026/src/diag/diag_central_strafe.cpp
environment: "pio run -e diag_central_strafe_robot1 (o _robot2, o _robot2_kick = + impulso + corte trasera)"
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
---

# `diag_central_strafe` — Patrulla lateral del arquero (OPEN-LOOP)

> **✅ Resultado de banco 2026-06-09 (Gustavo, ROBOT2, env `diag_central_strafe_robot2_kick`): "anda bien".**
> El strafe quedó validado con **3 técnicas juntas**: piso de PWM por rueda `{70, 70, 107}`, impulso inicial
> (kickstart) `{130, 130, 140}` PWM × 40 ms, y corte anticipado de la trasera 66 ms antes del fin del tramo.
> Ver la sección **"El env `_kick`"** abajo. ROBOT1 arranca de los mismos valores — **A VERIFICAR EN BANCO R1**.
> (El piso `{70,70,42}` que se menciona en las notas viejas de abajo quedó superado: era del banco R1
> 2026-06-08, con la trasera baja para que no rote en lazo abierto.)

> **🔬 Resultado de banco 2026-06-03** (corrido vía el hermano `diag_central_arbitro_strafe`,
> mismo path de motores): al moverse **solo gira el motor 1**. Causa: la cinemática vieja
> `{60,-60,180}` estaba **en el eje equivocado** (daba círculos) y subdimensionaba el par.
> Con la cinemática CALIBRADA 2026-06-08 (`WHEEL_ANGLES_DEG = {330, 210, 90}`), lateral puro
> da **M1=M2=+0.5·vx (mismo lado)** y **M3=−vx (la trasera es la que más empuja**, no 0).
> El piso de PWM ahora es **POR RUEDA** (`MOTOR_MIN_PWM[3] = {70, 70, 42}`: delanteras
> oblicuas 70 > trasera paralela 42) para que ninguna quede stalled por deadzone.
> Ver journal `2026-06-03-banco-resultados-arbitro-strafe-y-bno-freeze.md` + TASK-101.

## Para qué sirve

Test de banco de la **mitad inferior** (motores + placa CENTRAL + cinemática). El
robot se mueve **lateral** (perpendicular a donde mira la cámara frontal):
**~30 cm a la izquierda → pausa → ~30 cm a la derecha → pausa → loop**. Es la base
del **arquero**.

```
vx lateral → cinemática inversa omni-3 → motors_zircon (PWM a los 3 H-bridges)
```

## ⚠️ Es OPEN-LOOP, sin feedback de heading

El CENTRAL **no tiene BNO055** (el heading viene del TOP, que acá **no se usa**).
El **OTOS de la base ahora SÍ llega a CENTRAL** (broadcast simétrico desde DOWN,
2026-06-01), pero **este sketch v1 todavía no lo usa** (open-loop a propósito, para
validar primero el movimiento lateral simple). Entonces comanda **`omega = 0`**:

- Para un omni-3 con **ruedas parejas y bien calibradas**, `omega=0` es
  **traslación pura SIN rotar** → el robot "mira al frente" mientras se mueve de
  costado.
- Cualquier rotación residual es **deriva**, y **NO se corrige** (no hay sensor de
  heading en CENTRAL). Cuánto deriva es justamente uno de los datos que este test
  mide (FASE B).

> **Heading-hold activo = v2** (ver "Próximos pasos"): ahora es **viable sin TOP**
> porque el **OTOS ya llega a CENTRAL** (`pose_view.h` + `world_model`, broadcast
> 2026-06-01). Es un cambio **local en CENTRAL** (sumar un `HeadingPID` sobre el
> heading del OTOS) — ya NO requiere mensaje nuevo ni trabajo cross-board.

## ⚠️ Distancia OPEN-LOOP (aproximada)

CENTRAL no recibe odometría → la distancia se hace **por tiempo**: a `S` mm/s,
30 cm = 300 mm tarda `300/S` s. Los "30 cm" son **nominales** — medir con regla y
ajustar `-DDIAG_STRAFE_SPEED_MM_S` / `-DDIAG_STRAFE_DISTANCE_MM`.

## Convención (de `kinematics.h`)

`+X = derecha`, `+Y = frente`. Lateral = `vx` (con `vy = 0`).
**Izquierda = −vx**, **derecha = +vx**. Invertible con `-DDIAG_STRAFE_INVERT_LR`.

## Cómo correr

```bash
cd "software/teensy/Soccer 2026"
pio run -e diag_central_strafe_robot1 -t upload     # arquero
pio device monitor -b 115200
```

**Operativa** (botón pin 9, o **ENTER** por Serial Monitor):
- `WAITING` → (botón) arranca la patrulla `IZQUIERDA → PAUSA → DERECHA → PAUSA → …`
- Botón **durante** la patrulla = **STOP**.

Telemetría cada 250 ms: `state | vx`.

## El env `_kick` — las 3 técnicas del lateral (✅ banco ROBOT2 2026-06-09)

```bash
pio run -e diag_central_strafe_robot2_kick -t upload    # robot2 + impulso + corte trasera
```

Es el **mismo strafe** de `diag_central_strafe_robot2` pero con las 3 técnicas de control de bajo nivel
activas. Validado en banco por Gustavo el 2026-06-09: **"anda bien"**. Por decisión de Gustavo, estas 3
técnicas se usan **SIEMPRE que el robot se mueva lateralmente, en TODOS los programas** (no son exclusivas
de este diag).

### Las 3 técnicas y sus valores

1. **Piso de PWM por rueda** (`MOTOR_MIN_PWM[3]` en `config_central.h`): **`{70, 70, 107}`** — delanteras
   oblicuas 70 · trasera 107. La trasera se barrió 42→50→70→85→95→100→105→**107**. ¿Por qué la trasera es la
   ALTA? El PWM **no** es proporcional a la velocidad y es **distinto por rueda**: en el strafe la trasera
   debe girar al DOBLE que las delanteras (cinemática: fronts 0.5·vx, rear 1.0·vx), pero como rueda ALINEADA
   al movimiento (menos fricción que las oblicuas) lo logra con ~1.5× el PWM (107 vs 70), no 2×.
   Siempre activo (no depende de flag).

2. **Impulso inicial (kickstart)** — flag `-DCENTRAL_MOTOR_KICKSTART`, vive en `motors_zircon.cpp` (módulo
   puro `shared/motor_kickstart`): en la transición **parado→comando** de cada rueda, manda
   **`{130, 130, 140}` PWM durante 40 ms** y después deja pasar el PWM base. Implementado como factor ×9.9
   (`KICKSTART_FACTOR_X10=99`) + cap POR RUEDA (`KICKSTART_PWM_CAP[3]={130,130,140}`) → el factor satura
   siempre contra el cap = **impulso fijo**. La trasera necesitaba 140 porque con menos "se quedaba".

3. **Corte anticipado de la trasera** — flag `-DCENTRAL_REAR_BRAKE_LEAD`: el diag llama
   `motors_set_rear_cut(true)` cuando faltan **66 ms** para el fin del tramo → el mixer corta la TRASERA
   (idx 2) a 0 mientras las delanteras terminan solas. Sin esto, la inercia de la trasera **desacomoda el
   robot al frenar**. El corte se re-arma (`false`) en cada cambio de estado. La *decisión* de cuándo cortar
   es del caller (este diag conoce la duración del tramo); por eso hoy está cableado **SOLO acá** — llevarlo
   al lateral de la FSM del arquero es tema-a-analizar (la FSM no tiene el evento "fin del movimiento").

### Perillas de tuneo

| Perilla | Dónde | Default | Cuándo tocarla |
|---|---|---|---|
| `MOTOR_MIN_PWM[3]` | `config_central.h` (por-robot) | `{70, 70, 107}` | delanteras ↑ si no empujan en el piso; trasera ↓ GRADUAL (107→95→85…) si el robot ROTA en el strafe. ⚠️ NO pasar ~150 (motores 5V a 7,4 V se queman > ~70 %) |
| `KICKSTART_PWM_CAP[3]` | `motors_zircon.cpp` | `{130, 130, 140}` | rueda que "se queda" al arrancar → subir SU cap de a 5–10 (medir temperatura); tirón excesivo → bajar |
| `KICKSTART_WINDOW_MS` | `motors_zircon.cpp` | `40` | si el impulso no alcanza, antes de subir caps probar 40→60 |
| `-DDIAG_STRAFE_REAR_LEAD_MS=N` | build flag (este diag) | `66` | **subí** si la cola (trasera) sigue rodando al frenar; **bajá** si la trasera frena demasiado antes |

### Estado por robot

| | ROBOT2 | ROBOT1 |
|---|---|---|
| `{70,70,107}` + impulso `{130,130,140}` + lead 66 ms | ✅ **validado banco 2026-06-09** | ⚠️ mismos valores, **A VERIFICAR EN BANCO R1** — el `{70,70,42}` viejo era de su banco 2026-06-08 (trasera baja porque rotaba); si R1 rota con 107, bajar |

> ⚠️ **OJO build:** todo env con `build_src_filter` EXPLÍCITO que active `-DCENTRAL_MOTOR_KICKSTART` necesita
> `+<shared/motor_kickstart.cpp>` en el filtro (el env `_kick` ya lo tiene). Los envs que compilan todo
> `src/` no lo necesitan. pio/Teensy no se compila desde las sesiones Claude: **compila el equipo**.

## Plan de prueba en banco

> **Robot SUJETO o ruedas al aire** al principio (puede salir disparado). Batería
> cargada (los H-bridges NO van por USB).

> ⚠️ **LEER ANTES DE INTERPRETAR LA DERIVA (FASE B).** Este sketch mueve los
> motores vía `motors_apply_command()` → `inverse_kinematics()` con
> `WHEEL_ANGLES_DEG = {330, 210, 90}` (**CALIBRADO 2026-06-08** con la disposición física
> real — antes era `{60,-60,180}`, que estaba en el eje equivocado y daba CÍRCULOS).
> La inversión por motor se honra vía `MOTOR_INVERT` (`config_central.h`) que
> `motors_zircon.cpp` aplica; **desde el recableado 2026-06-11 es `{+1,+1,+1}` en ambos
> robots** (M2/U17 de R1 = +1; histórico: -1 con el cableado de fábrica PRE-reparación).
> Con la cinemática calibrada el lateral ya **no** sale en diagonal por la
> cinemática. **Pendiente de banco: SOLO el tuneo fino del lateral (que no rote) +
> confirmar el SENTIDO de la traslación** — si traslada al revés, sacar el +180 de los
> ángulos (el giro queda arreglado igual). Si todavía aparece deriva grande tras el tuneo,
> recién ahí decide si "hace falta v2" (heading-hold con OTOS).

**FASE A — Movimiento lateral + dirección.**
- Apretás el botón → el robot se mueve **de costado** (NO adelante/atrás), **izquierda primero**.
- Si va a la derecha → recompilar con `-DDIAG_STRAFE_INVERT_LR`.
- Si en vez de ir derecho de costado **se va en diagonal o rota fuerte** → la
  cinemática / el sentido de las ruedas no están bien: revisar `MOTOR_DIR`
  (`diag_central_motors`) y `WHEEL_ANGLES_DEG` (`config_central.h`).

**FASE B — Medir la DERIVA de heading (clave para el arquero).**
- Marcá la orientación inicial (una cinta/flecha en el piso). Dejá que patrulle
  varios ciclos izquierda↔derecha.
- **¿Cuánto rota el robot** respecto del frente después de, p. ej., 5 ciclos?
  - Deriva chica (pocos grados) → el open-loop alcanza para el arquero; seguimos así.
  - Deriva grande (gira notoriamente) → hace falta **heading-hold activo (v2 con OTOS)**.
- Anotar los grados de deriva (es el dato que decide si v2 es necesario).

**FASE C — Calibrar la distancia.**
- Medir con regla cuánto recorre cada tramo. Ajustar `-DDIAG_STRAFE_SPEED_MM_S`
  (o `-DDIAG_STRAFE_DISTANCE_MM`) hasta que los ~30 cm coincidan. Anotar el valor.

**Criterios de cierre (los confirma el equipo, no Claude):**
- [ ] Movimiento **lateral** correcto (FASE A), izquierda primero.
- [ ] **Deriva de heading medida** y anotada (FASE B) → decide si hace falta v2.
- [ ] Distancia por tramo ≈ 30 cm con el valor calibrado (FASE C).
- [ ] Velocidad anotada para el firmware del arquero.

## Flags

| Flag | Efecto |
|---|---|
| `-DDIAG_STRAFE_SPEED_MM_S=200` | velocidad lateral (default 150) |
| `-DDIAG_STRAFE_DISTANCE_MM=400` | distancia por tramo (default 300 = 30 cm) |
| `-DDIAG_STRAFE_INVERT_LR` | invierte izquierda/derecha |
| `-DCENTRAL_MOTOR_KICKSTART` | impulso inicial por rueda `{130,130,140}` PWM × 40 ms (ON en el env `_kick`; requiere `+<shared/motor_kickstart.cpp>` en el `build_src_filter`) |
| `-DCENTRAL_REAR_BRAKE_LEAD` | habilita el corte anticipado de la trasera vía `motors_set_rear_cut()` (ON en el env `_kick`) |
| `-DDIAG_STRAFE_REAR_LEAD_MS=N` | cuántos ms antes del fin del tramo corta la trasera (default 66) |

## Próximos pasos (para el arquero "de verdad")

1. **Heading-hold con OTOS (v2)** — si la FASE B muestra deriva grande. **Ya
   viable**: el OTOS llega a CENTRAL (`pose_view.h` + `world_model`, broadcast
   2026-06-01). Es un cambio **local en CENTRAL**: agregar un `HeadingPID` que
   corrige `omega` con el heading del OTOS (igual que `diag_central_drive_straight`,
   pero con la pose OTOS en vez del WorldSnapshot del TOP). Ya NO es cross-board.
2. **Evitar la línea mientras patrulla** (placa inferior) — integrar `comm_down` +
   `world_model_imminent_exit()` para frenar/rebotar en el borde (mismo patrón que
   `diag_central_drive_straight` con `-DDIAG_DRIVE_WITH_LINE`). Necesita el link
   DOWN→CENTRAL andando (ver `DIAG-CENTRAL-COMM-DOWN.md`).
3. **Seguir la pelota lateralmente** (arquero reactivo) — setpoint lateral derivado
   de la posición de la pelota (necesita TOP / visión).

## Referencias

- Sketch: [`src/diag/diag_central_strafe.cpp`](../../software/teensy/Soccer%202026/src/diag/diag_central_strafe.cpp)
- Cinemática: [`src/shared/kinematics.h`](../../software/teensy/Soccer%202026/src/shared/kinematics.h)
- Motores + `MOTOR_DIR`: [`DIAG-CENTRAL-MOTORS.md`](DIAG-CENTRAL-MOTORS.md)
- Hermano (avance recto +Y, con heading del TOP): [`DIAG-CENTRAL-DRIVE.md`](DIAG-CENTRAL-DRIVE.md)
- OTOS → CENTRAL (camino v2): `TODO_DIFFERENTIAL_OTOS` en `diag_central_drive_straight.cpp`

## Cambios

- 2026-06-09 — ✅ **banco ROBOT2 OK** (Gustavo, env `diag_central_strafe_robot2_kick`: "anda bien").
  Documentadas las 3 técnicas del lateral con sus valores finales: piso `MOTOR_MIN_PWM={70,70,107}`,
  kickstart `{130,130,140}` PWM × 40 ms (`-DCENTRAL_MOTOR_KICKSTART`), corte anticipado de la trasera
  66 ms (`-DCENTRAL_REAR_BRAKE_LEAD` + `-DDIAG_STRAFE_REAR_LEAD_MS`). ROBOT1 arranca de los mismos
  valores, A VERIFICAR EN BANCO R1. Editor: Claude Opus 4.8 (Anthropic). Requested-by: Viollaz.
- 2026-05-31 — creación. Sketch + envs `diag_central_strafe_robot1/2` + doc.
  **OPEN-LOOP** (omega=0): el CENTRAL no tiene BNO y los OTOS van DOWN→TOP, así que
  no hay feedback de heading. Heading-hold activo con OTOS queda como v2. Compila
  robot1 + robot2. NO validado en hardware.
  Author: Claude Opus 4.8 (Anthropic). Requested-by: Viollaz.
