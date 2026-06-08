# 2026-06-08 — Cinemática del arquero: WHEEL_ANGLES calibrados + piso de PWM por rueda

**Sesión:** Gustavo (banco, ROBOT1 arquero) + Claude (coach).
**Tema:** resolver que el arquero **giraba sobre su propio eje** en vez de trasladarse, y dejar la **disposición de motores DEFINIDA** y propagada.

> Distinto del journal `2026-06-08-bno-contencion-bus-debug-y-arquero-sin-bno.md` (ese fue el BNO/I²C). Acá: cinemática y motores.

## Punto de partida

El arquero, al patrullar, **rotaba sobre su eje** en lugar de moverse de costado. Síntoma clásico de cinemática mal expresada.

## Qué se encontró y se resolvió (en orden)

### 1. `WHEEL_ANGLES_DEG` estaba en el eje equivocado → daba círculos
- Estaba en `{60, -60, 180}`, expresado pensando en el eje **+Y (frente)**, pero `inverse_kinematics()` usa el ángulo de **posición de cada rueda desde +X**. Eso convertía un comando de traslación en giro.
- **Disposición física REAL (confirmada por Gustavo mirando el robot):** 3 ruedas omni a 120°:
  - **M1 = delantera IZQUIERDA** (driver U5, pines 2/5/3)
  - **M2 = delantera DERECHA** (driver U17, 8/7/6, **invertido por HW**)
  - **M3 = TRASERA / centro** (driver U7, 11/12/4)
  - Los 3 motores giran **horario** mirados desde el centro (comando positivo).
- Cuenta: posición desde +X → M1:150°, M2:30°, M3:270°. Como los motores giran horario (opuesto al antihorario de la fórmula), se suma 180° → **`{330, 210, 90}`**. (commit `3734880`)
- Verificado en banco: con `{330,210,90}` la rueda **trasera (M3) ahora sí se mueve** en el strafe → la cinemática quedó bien.

### 2. Deadzone del strafe: las delanteras no arrancaban
- En un strafe (vx), la geometría da a las **delanteras la MITAD** de velocidad que a la trasera. A vx=150 eso son ~19 PWM en las delanteras → **bajo el stiction del motor → no arrancaban** (la trasera sí, a ~38).
- Se activó el **piso de PWM** (`MOTOR_MIN_PWM`, estaba en 0/OFF).

### 3. El piso tiene que ser POR RUEDA (hallazgo de Gustavo)
- Con piso único alto (70), la **trasera** (que es más eficiente, va paralela al movimiento) se **adelantaba** a las delanteras → el robot rotaba.
- **Insight de Gustavo:** las ruedas **delanteras trabajan OBLICUAS (a 60°)** → sus rodillos pasivos ruedan de costado → **mucha más fricción** → menos velocidad por PWM. La **trasera va PARALELA** al strafe → más eficiente. **El PWM NO es proporcional a la velocidad real: es distinto por rueda.**
- Solución: **piso de PWM por rueda** `MOTOR_MIN_PWM[3] = {70, 70, 42}` (delanteras oblicuas más alto, trasera paralela más bajo). Reemplaza el piso único escalar + una ganancia por rueda (`MOTOR_GAIN`) que se probó y se descartó por redundante. (commits `0fdcf67` → `97b5658` → `2424e56`)

## Estado

- ✅ **Disposición, sentido y ubicación de los 3 motores: DEFINIDA.**
- ✅ Cinemática `{330,210,90}` correcta (las 3 ruedas se mueven en la dirección correcta).
- ⏳ **Pendiente de banco (tuneo, no diseño):** ajuste fino del lateral para que **no rote** (perilla = `MOTOR_MIN_PWM[idx]` por rueda) + **confirmar el sentido** de traslación. Con el robot levantado las 3 giran bien; sobre el piso falta torque/balance fino en las delanteras.
- ⚠️ Estos motores son **brushed 5V alimentados a 7,4V** → no pasar ~150 PWM (se queman > ~70%). Rango útil angosto → el lateral lento es difícil; fix de fondo = mejores motores (roadmap).

## Propagación a todo el código + docs (commit `41c8202` + siguiente)

Como la disposición quedó definida, se propagó a **todos los archivos de programa** y docs:
- **Código:** `config_central.h` (canónico) + `motors_zircon.cpp` + diags `diag_central_strafe`, `diag_central_arbitro_strafe` (caveat viejo de "círculos" → RESUELTO), `diag_central_motors`, `diag_central_line_sweep` (etiquetas físicas de cada rueda) + seed `robot2.h`.
- **Docs:** `FUENTES-DE-VERDAD.md` (fila motores), + barrido de `{60,-60,180}`→`{330,210,90}` y de los rótulos físicos (varios docs tenían M1=derecha/M2=izquierda **al revés**).
- ⚠️ **NO se tocaron los `DIR_M*` de `diag_central_line_sweep`** (calibración directa de marzo, no la cinemática) — reconciliar en banco, no asumir.

## Para el TDP (capitalizable)

Buen material de ingeniería para el portfolio: por qué el PWM no es proporcional a la velocidad en un omni de 3 ruedas (fricción oblicua vs paralela según la dirección), y cómo se modela con un piso de deadzone por rueda. El matiz de que "paralela vs oblicua" depende de la **dirección del movimiento** (en strafe la trasera es paralela; en avance se invierte) está documentado en `config_central.h`.
