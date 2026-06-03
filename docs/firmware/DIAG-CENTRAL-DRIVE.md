---
title: "Diagnóstico de avance recto del CENTRAL con PID heading"
date: 2026-05-29
status: vivo
audiencia: "Virginia / Elías / Enzo — operativos en el banco"
firmware-source: software/teensy/Soccer 2026/src/diag/diag_central_drive_straight.cpp
environments: ["diag_central_drive_robot1", "diag_central_drive_robot2"]
author: "Claude Opus 4.7 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
hermanos: [DIAG-CENTRAL-MOTORS.md]
---

> ✅ **Actualización 2026-05-31:** el "conflicto pines 7/8" (Serial2 vs motor 2) está
> **RESUELTO** — la CENTRAL reasignó sus UART: snapshot del TOP en **Serial7** (pin 28) y
> línea del DOWN en **Serial1** (pin 0), dejando 7/8 solo para el motor 2. Donde abajo
> diga "conflicto 7/8 / bypass por Serial2 bloqueado", leerlo como superado.

# `diag_central_drive_straight` — Avance recto con PID heading

## Para qué sirve

Validar end-to-end (en banco o cancha despejada) que la **cadena completa de
control de movimiento** del CENTRAL funciona:

```
TOP -> WorldSnapshot (Serial7, pin 28)
        \
         world_model
              \
               HeadingPID  -> omega correctivo
                \           /
                 kinematics inversa omni-3
                       \
                        motors_zircon -> 3 H-bridges del Zircon Rev v15
```

El sketch hace una secuencia simple controlada por botón:

| Apretón | Acción |
|---|---|
| 1 | Captura el heading actual como setpoint, arranca **FORWARD** (+Y, 3 s) |
| en FORWARD: 2 | Aborta forward → entra a PAUSED (1 s) |
| (auto post-PAUSED) | **REVERSE** (−Y, 3 s) manteniendo el mismo setpoint heading |
| en REVERSE: 3 | Aborta reverse → FIN |
| automático | Cualquier dirección termina por timeout 3 s |

Durante FORWARD/REVERSE el `HeadingPID` corrige desvíos: si el robot empieza
a girar parásita-mente, omega correctivo lo vuelve a alinear.

## Lo que SÍ valida

1. **Cadena TOP→CENTRAL**: que el `WorldSnapshot` llega por Serial7 (pin 28), con
   watchdog 500 ms. Si falla → motor stop automático.
2. **Cinemática inversa omni-3**: que `WHEEL_ANGLES_DEG` y `WHEEL_RADIUS_MM`
   en `config_central.h` matchean el robot armado. Si no matchean, el robot
   se mueve en una dirección distinta a +Y o rota.
3. **HeadingPID con ganancias reales**: por Serial USB cada 250 ms imprime
   heading, setpoint, error, omega — perfecto para tunear Kp/Ki/Kd contra
   un robot que se va de costado.
4. **Watchdogs**: snapshot perdido / línea inminente / botón abort.
5. **TASK-036 (motores) sanidad cruzada**: si un motor no responde, el robot
   no avanza recto y el error del PID crece sin compensación.

## Lo que NO hace (todavía)

- ❌ **PID DIFERENCIAL con 2 OTOS crudas**. Hoy CENTRAL recibe SOLO la pose
  fusionada del TOP (`my_x/my_y/my_heading` en `WorldSnapshot v2`), no las 2
  lecturas OTOS por separado. Para hacer el diferencial hay 2 caminos:
    1. **Ampliar contrato a v3**: agregar 2 poses crudas OTOS al snapshot
       (+12 B). Toca `types.h` + firmware TOP + comm_top en CENTRAL + romper
       `static_assert` de v2.
    2. **Bypass DOWN→CENTRAL**: que DOWN mande las 2 OTOS por Serial2 además
       del `LINE_URGENT`, con nuevo `MsgType` y handler en `comm_down.cpp`.
       Bloqueado por conflicto pines 7/8 + TASK-031.

  Recomendación coach: hacer (1) post-Incheon. Para "ir derecho" el heading
  fusionado alcanza. El diferencial OTOS daría redundancia + detección de
  slip, no precision adicional.
- ❌ **PID lateral** (mantener X=0 mientras avanza). El sketch sólo corrige
  rotación. Si el robot se desvía lateralmente pero sigue mirando al norte,
  no lo corrige. Versión futura.
- ❌ **Movimientos vectoriales arbitrarios** (lateral, diagonal). Solo +Y/−Y.
- ❌ **Calibración de cinemática**. Si `WHEEL_ANGLES_DEG` está mal, este
  sketch lo evidencia (robot se mueve raro) pero no calibra automático.

## Pre-requisitos OPERATIVOS (orden estricto)

1. **TASK-036 cerrada** (`diag_central_motors` ejecutado y los 3 motores
   validados, mapeo motor↔rueda conocido, conflicto pines 7/8 resuelto).
   Sin esto los resultados del PID son basura.
2. **PLACA TOP corriendo** y enviando `WorldSnapshot` por Serial1 a 230400
   baud. Sin esto el sketch queda en `WAITING_SNAPSHOT` para siempre
   (timeout 5 s).
3. **Cable UART** TOP→CENTRAL conectado físicamente al **pin 28 del CENTRAL**
   (Serial7 RX7); el cable sale del **TOP pin 17 (TX4)**. ⚠️ NO a los pines 0/1
   (esos son el link de DOWN, Serial1).
4. **Batería cargada** — los H-bridges NO se alimentan por USB.
5. **Robot SUJETO al banco**, con **ruedas al aire**, O en **cancha
   despejada** con al menos 1.5 m de espacio (a 300 mm/s × 3 s = 900 mm).
6. **Excepción Avast aplicada** (TASK-025) si PlatformIO se traba al
   compilar. El sketch sí usa libs externas (Adafruit_BNO055 vendoreada,
   no debería bajar nada).

## Procedimiento

### Paso 1 — Compilar y flashear

```bash
cd "software/teensy/Soccer 2026"
pio run -t clean -e diag_central_drive_robot1     # arquero
# o
pio run -t clean -e diag_central_drive_robot2     # delantero
pio run -e diag_central_drive_robot{1,2} -t upload
```

Cuidado con flashear el binario correcto al robot correcto — el pinout de
motores difiere entre ROBOT1 y ROBOT2.

### Paso 2 — Abrir Serial Monitor

```bash
pio device monitor -b 115200
```

Banner esperado:

```
==================================================
 diag_central_drive_straight  -  Zircon Rev v15
==================================================
 Build robot: ROBOT2 (delantero)     ← según el binario
 Velocidad de avance: 300 mm/s
 Duracion por direccion: 3000 ms
...
 Apreta el boton para arrancar.
```

### Paso 3 — Apretar el botón

Si la cadena TOP→CENTRAL anda, **inmediatamente** captura heading y
arranca FORWARD. Si no, entra a `WAITING_SNAPSHOT` y espera hasta 5 s; si
no llega snapshot, vuelve a `WAITING_START` con error explícito.

Durante FORWARD/REVERSE, el LED queda fijo. En estados de espera,
parpadea a 2 Hz.

### Paso 4 — Observar la telemetría

Cada 250 ms el sketch imprime:

```
[t=12345] state=FORWARD snap=FRESH hdg=42.3 sp=40.0 err=-2.3
```

| Campo | Qué es |
|---|---|
| `state` | Estado actual de la FSM del sketch |
| `snap` | FRESH = recibido en últimos 500 ms; STALE = timeout |
| `hdg` | Heading actual del robot (grados, -180 a +180), del WorldSnapshot |
| `sp` | Setpoint del PID (heading al que se intenta volver) |
| `err` | Diferencia (con wrap a ±180°). Idealmente oscila < ±5° |

Si está compilado con `-DDIAG_DRIVE_WITH_LINE` también imprime estado
de línea (DOWN).

### Paso 5 — Diagnosticar lo observado

| Síntoma | Diagnóstico probable | Acción |
|---|---|---|
| Robot se mueve diagonal en lugar de +Y | `WHEEL_ANGLES_DEG` mal configurado | Verificar con regla la posición física de las ruedas; ajustar `config_central.h §65-69` |
| Robot avanza pero rota constantemente | PID heading no compensa rápido | Aumentar `Kp` (default 3.0) en `pids.h:31`; recompilar |
| Robot oscila izq/der constantemente | PID heading con `Kp` o `Kd` mal | Bajar `Kp` o subir `Kd`; recompilar |
| Robot no avanza, ruedas zumban | Saturación o PWM bajo | Aumentar `DIAG_DRIVE_SPEED_MM_S` (cuidado!) o revisar `MAX_SPEED_MM_S` |
| Solo gira o solo se mueve 1 rueda | Motor caído o mal cableado | Vuelve a TASK-036 y repetir `diag_central_motors` |
| Snapshot STALE permanente | Cable TOP→CENTRAL, firmware TOP, baud, Serial1 | Verificar TOP con `pio device monitor` en otro USB |
| `err` no baja de ±20° aun al final del trayecto | Setpoint cambia silenciosamente, o sensor con drift fuerte | Revisar drift del BNO055 del TOP, o cambiar la fusión |

## Tuning del PID — referencia

Defaults en `src/shared/pids.h:30-32`:

```cpp
float kp = 3.0f;
float ki = 0.05f;
float kd = 0.5f;
```

Heurística clásica:
1. **`ki = 0, kd = 0`**. Subir `kp` hasta que apenas oscile en respuesta a un
   empujón manual. Anotar ese valor (`kp_crit`). Setear `kp = 0.6 * kp_crit`.
2. **Sumar `kd`** poco a poco hasta amortiguar el overshoot.
3. **Sumar `ki` muy poco** (factor 10 menor que `kp`) si hay deriva
   permanente. Cuidado con windup — el `integral_clamp = 50.0f` lo limita
   pero ojo.

Después de tunear acá, llevar las ganancias **al `pids.h` vivo** así las
usa también `strategy.cpp` (TASK separada).

## Pendientes que este test NO cubre

| # | Pendiente | Cómo se cierra |
|---|---|---|
| 1 | Calibración fina de `WHEEL_ANGLES_DEG` y `WHEEL_RADIUS_MM` | Mediciones con regla en robot armado + ajuste iterativo |
| 2 | PID lateral (mantener X=0 mientras avanza) | Sketch v2 — agregar `LateralPID` con measurement del `my_x_mm` del snapshot |
| 3 | PID diferencial OTOS (con 2 lecturas crudas) | Cambio de contrato v3 + bypass DOWN→CENTRAL. Post-Incheon. |
| 4 | Validación con cancha real (no banco) | Una vez calibrado en banco, prueba en cancha de pretemporada |
| 5 | Test con perturbaciones (empujones manuales) | Variante del sketch que registre máx error tras un push |

## Documentación esperada (journal entry post-test)

Entrada `journal/YYYY-MM-DD-diag-central-drive-<descripcion>.md` con:

- Foto del setup (robot, posición inicial, espacio disponible).
- Video corto del trayecto si es posible.
- Captura del Serial Monitor durante FORWARD + REVERSE (mostrar evolución
  del `err`).
- Veredicto: ¿anduvo recto? ¿qué ganancias del PID quedaron? ¿qué
  pendientes nuevos surgieron?
- TASKs nuevas: nuevas calibraciones que haya que hacer.

## Referencias

- Sketch: [`software/teensy/Soccer 2026/src/diag/diag_central_drive_straight.cpp`](../../software/teensy/Soccer%202026/src/diag/diag_central_drive_straight.cpp)
- Environments: `[env:diag_central_drive_robot1]` y `[env:diag_central_drive_robot2]`
  en [`platformio.ini`](../../software/teensy/Soccer%202026/platformio.ini)
- Hermano (motores 1×1): [`DIAG-CENTRAL-MOTORS.md`](DIAG-CENTRAL-MOTORS.md)
- HeadingPID: `src/shared/pids.h` + `src/shared/pids.cpp` (+ 17 tests)
- Kinematics: `src/shared/kinematics.h` + 11 tests
- Contrato WorldSnapshot v2: [`CONTRATO-DATOS-CENTRAL.md`](CONTRATO-DATOS-CENTRAL.md)
- Hito 24-may DOWN (anillo + OTOS validados en banco):
  `journal/2026-05-24-down-board-passing-tests-cierre.md`

## Cambios

- 2026-05-29 — creación inicial. Sketch + 2 environments (robot1/robot2) +
  doc + actualización de `ESTADO-ACTUAL.md` y `FUENTES-DE-VERDAD.md` +
  TASK-037 humana.
  Author: Claude Opus 4.7 (Anthropic). Requested-by: Gustavo Viollaz (@gviollaz).
