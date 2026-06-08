---
title: "Diagnóstico de motores de la placa CENTRAL (Zircon Rev v15)"
date: 2026-05-28
updated: 2026-05-29
status: vivo
audiencia: "Virginia / Elías / Enzo — operativos en el banco"
firmware-source: software/teensy/Soccer 2026/src/diag/diag_central_motors.cpp
environment: "pio run -e diag_central_motors  (o sketch standalone en Arduino IDE)"
author: "Claude Opus 4.7 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
---

> ✅ **Actualización 2026-05-31:** el "conflicto pines 7/8" (Serial2 vs motor 2) se
> RESOLVIÓ moviendo los UART de la CENTRAL — DOWN→CENTRAL pasó a **Serial1** (0/1) y
> TOP→CENTRAL a **Serial7** (28/29), dejando los pines 7/8 solo para el motor 2 (U17).
> Lo que sigue sobre "aislar 7/8 / migrar Serial2" quedó **superado**; este diag sigue
> siendo útil para **mapear motor N firmware → rueda física** y validar los 3 H-bridges.

# `diag_central_motors` — Test individual de los 3 motores del Zircon

## Disposición de motores ROBOT1 (VALIDADA)

> Validada en banco (María/Elías 2026-06-01, `diag_central_line_sweep_robot1`) y RE-CONFIRMADA 2026-06-06 (commit 8956d10) tras rearmar el robot. La fuente única del sentido por motor es `MOTOR_INVERT` en `config_central.h`.

| Motor (firmware) | Driver | Pines INA/INB/PWM | Sentido (MOTOR_INVERT) | Ángulo rueda | Posición física |
|---|---|---|---|---|---|
| Motor 1 | U5  | 2 / 5 / 3   | **+1** (normal)             | 330deg | Delantera **izquierda** ¹ |
| Motor 2 | U17 | 8 / 7 / 6   | **-1** (INVERTIDO por HW)   | 210deg | Delantera **derecha** ¹ |
| Motor 3 | U7  | 11 / 12 / 4 | **+1** (normal)             | 90deg | **Trasera** (centro) ² |

¹ El lado izquierda/derecha del par delantero quedó DEFINIDO con la disposición física real (CALIBRADO 2026-06-08, Gustavo, banco): M1 = delantera IZQUIERDA, M2 = delantera DERECHA. Ligado a `WHEEL_ANGLES_DEG = {330, 210, 90}`.
² M3 = trasera CONFIRMADO en banco: en lateral puro su aporte es 0 (rueda trasera kiwi). Fuente: journal `2026-06-03-banco-resultados-arbitro-strafe-y-bno-freeze.md`, journal 2026-06-01.

El sentido/identidad de los 3 motores está cerrado y la disposición/posición ya está DEFINIDA (CALIBRADO 2026-06-08). Lo que sigue pendiente de banco es SOLO el tuneo fino del lateral (que no rote) + confirmar el sentido de la traslación, y el ROBOT2/delantero — ver Pendientes.

## Para qué sirve

Sketch standalone para validar **en banco** que los 3 H-bridges del Zircon
Rev v15 funcionan, y para:

1. **Mapear** qué número de motor del firmware (1/2/3) corresponde a qué rueda
   física del robot.
2. **Calibrar el sentido de giro de cada rueda** vía el arreglo `MOTOR_DIR[]`
   (ver sección dedicada). El sentido que quede correcto en el banco es el que
   después va al firmware de producción.

Cada motor se prueba uno a la vez con una onda PWM senoidal (0 → 128 → 0
repetido), avanzando al siguiente **cuando se aprieta el botón** de la placa.

**Bonus crítico**: el mismo sketch resuelve empíricamente el conflicto P0
pines 7/8 documentado en
[`hardware/electronics/central-board-pack/01-pinout-y-hardware.md §8`](../../hardware/electronics/central-board-pack/01-pinout-y-hardware.md).
Si el motor 2 (driver U17, pines 7/8/6) **NO se mueve** cuando le toca el
turno, eso confirma que los pines 7/8 están cableados como `Serial2` hacia
DOWN y NO como motor — entonces no hay conflicto. Si el motor 2 **sí se
mueve**, los pines 7/8 son del motor y hay que migrar `Serial2` a otro UART
libre (ej. `Serial7` en pines 28/29) en el firmware CENTRAL de producción.

## Lo que NO es

- ❌ NO es firmware de competencia. Es diagnóstico de banco.
- ❌ NO depende de `config_central.h` ni del rol — los pines están
  hardcodeados en el sketch según `mapa-pines-teensy-ambos-robots.md` del
  2026-03-20. Si el cableado físico difiere, **editar la tabla `MOTORS[]`**
  arriba del sketch.

## Cómo se controla el sentido de giro (`MOTOR_DIR[]`)

Arriba del sketch hay un arreglo:

```cpp
constexpr int MOTOR_DIR[3] = { +1, +1, +1 };
//                              M1   M2   M3      (+1 horario, -1 antihorario)
```

- **Editás el signo del motor que gira al revés** de lo que querés, recompilás
  y resubís. Ej.: si el motor 2 gira al revés → `{ +1, -1, +1 }`.
- Hay además un multiplicador global `DIRECTION_SIGN` (invierte los 3 a la
  vez). En Arduino IDE dejalo en `+1` y controlá todo con `MOTOR_DIR[]`.
- El sentido efectivo de cada motor = `DIRECTION_SIGN × MOTOR_DIR[i]`. El
  Serial Monitor lo imprime al arrancar (`M1 M2 M3 → CW/CCW`) para confirmar
  **antes** de soltar el robot.

> ⚠️ **Importante (producción) — actualizado 2026-06-03:** el motor 2 (driver
> U17) tiene **INA/INB invertidos por hardware** (validado en banco por
> María/Elías con `diag_central_line_sweep`, donde `motor2()` cruza INA/INB y
> `ROT_M2=-1`). Producción ya lo honra: `config_central.h` define
> `MOTOR_INVERT[3] = {+1,-1,+1}` (ROBOT1) y `motors_zircon.cpp` multiplica el PWM
> firmado por ese signo antes de manejar INA/INB. **Fuente única del sentido por
> motor = `MOTOR_INVERT` en `config_central.h`** (no repetir el dato en otros
> docs; referenciarlo). ⚠️ ROBOT2 (delantero) NO testeado — ver el comentario de
> `MOTOR_INVERT` en la rama ROBOT2 de `config_central.h`.

## Procedimiento operativo

### Pre-requisitos

- Zircon Rev v15 con Teensy 4.1 enchufado.
- Cable USB al PC.
- Los 3 motores cableados a sus drivers del Zircon (U5, U17, U7).
- **Batería de potencia conectada y cargada** (el USB alimenta sólo al Teensy,
  no a los H-bridges).
- **Robot SUJETO al banco** o con **las ruedas al aire**. El sketch hace girar
  el motor a 50% de PWM máximo — el robot puede salir corriendo si está apoyado
  sobre las ruedas en el piso.

### Paso 1 — Compilar y flashear

**Opción A — PlatformIO (recomendada, usa el archivo del repo):**

```bash
cd "software/teensy/Soccer 2026"
pio run -t clean -e diag_central_motors
pio run -e diag_central_motors -t upload
```

**Opción B — Arduino IDE:** este repo es un proyecto PlatformIO; Arduino IDE
**no puede compilar el sketch desde dentro de `src/diag/`** porque esa carpeta
tiene ~15 sketches, cada uno con su `setup()/loop()` → error *"multiple
definition of setup()"*. Para usar Arduino IDE hay que aislar el sketch:

1. Crear una carpeta de sketch propia (en tu sketchbook), p. ej.
   `…/Arduino/diag_central_motors/`.
2. Copiar el contenido de `src/diag/diag_central_motors.cpp` adentro como
   `diag_central_motors.ino` (es código autónomo: sólo incluye `<Arduino.h>`).
3. En Arduino IDE: **Tools → Board → Teensy 4.1** (requiere Teensyduino),
   elegir el puerto, y **Upload**.

> El `.ino` es una **copia descartable** de banco. La fuente de verdad
> (versionada) es el `.cpp` del repo. Cuando descubras los signos correctos en
> el `.ino`, pasalos al `.cpp`. No commiteamos el `.ino` para no tener dos
> copias del mismo código en git.

### Paso 2 — Abrir Serial Monitor (115200)

PlatformIO: `pio device monitor -b 115200`. Arduino IDE: **Tools → Serial
Monitor** a 115200. Tenés que ver el banner inicial:

```
==============================================
 diag_central_motors  -  Zircon Rev v15
==============================================
 Apreta el BOTON (pin 9) para arrancar el test.
 (Backup: escribi algo + ENTER en el Serial Monitor.)
 ...
 Sentido efectivo (M1 M2 M3): CW CW CW
```

Si el banner no aparece: revisar el `-b 115200`, que el Teensy esté reconocido,
y que el LED parpadee a 2 Hz (heartbeat de `WAITING_START`).

### Paso 3 — Correr el test

Arrancá apretando el botón físico (pin 9). Como backup, escribir algo **+
ENTER** en el Serial Monitor también avanza (poné el desplegable de fin de
línea en **"Newline"** para que el ENTER funcione).

| Apretón # | Acción |
|---|---|
| 1 | Motor 1 (driver U5) arranca con onda creciente/decreciente |
| 2 | Motor 1 para, Motor 2 (driver U17) arranca |
| 3 | Motor 2 para, Motor 3 (driver U7) arranca |
| 4 | Motor 3 para, FIN. Reset del Teensy para repetir. |

Cada motor gira hasta el próximo apretón — no hay timeout. La onda completa un
ciclo cada 2 s. **El test espera el botón en CADA paso** (ver "Fixes de
robustez" abajo — antes saltaba solo y eso ya está arreglado).

### Paso 4 — Anotar mediciones

| Motor (firmware) | Pines | Driver | Rueda física | ¿Gira? | Sentido (MOTOR_INVERT) |
|---|---|---|---|---|---|
| Motor 1 | INA=2, INB=5, PWM=3 | U5 | Delantera **izquierda** (330deg, CALIBRADO 2026-06-08) | ☑ Sí | **+1** (normal) |
| Motor 2 | INA=8, INB=7, PWM=6 | U17 | Delantera **derecha** (210deg, CALIBRADO 2026-06-08) | ☑ Sí | **-1** (INVERTIDO por HW) |
| Motor 3 | INA=11, INB=12, PWM=4 | U7 | **TRASERA** (centro, 90deg, confirmado banco) | ☑ Sí | **+1** (normal) |

> **Nota:** izq/der del par delantero ya DEFINIDO (CALIBRADO 2026-06-08, Gustavo, banco). Datos de sentido validados en banco María/Elías 2026-06-01 (`diag_central_line_sweep_robot1`), re-confirmados 2026-06-06 (commit 8956d10). Pendiente de banco: SOLO el tuneo fino del lateral + confirmar el sentido de la traslación.

> **Por qué "visto de arriba" y no "de frente":** las 3 ruedas miran para lados
> distintos, así que el sentido propio de cada una no dice nada. Lo que importa
> es si la rueda empuja al **robot** a girar a favor o en contra del reloj
> (vista cenital). Las 3 tienen que coincidir.

## Interpretación de resultados

### Caso A — Los 3 motores giran

Cableado del firmware coincide con el hardware. **Pero ojo**: si el motor 2
giró, los pines 7/8 **son del motor** — entonces el conflicto pines 7/8 es
real y hay que **migrar `Serial2` a otro UART libre** (recomendado `Serial7`
en pines 28/29) en `src/central/comm_down.cpp` antes de probar comm
DOWN→CENTRAL. → TASK nueva + recablear el conector DOWN↔CENTRAL.

### Caso B — Motor 2 NO gira, motores 1 y 3 sí

Confirma que los pines 7/8 **no son del motor** — son sólo `Serial2` hacia
DOWN. El conflicto pines 7/8 **no existe** en el Zircon Rev v15. → cerrar la
TASK del conflicto como "no aplica al v15" y borrar el warning del §8 del
pinout.

### Caso C — Ningún motor gira

Batería sin conectar/descargada, drivers sin alimentación de potencia, o
pinout del sketch ≠ Zircon. → multímetro a la entrada de los drivers,
batería, y osciloscopio en el pin PWM activo.

### Caso D — Otro patrón inesperado

Anotar **literalmente** lo que pasó (luz rara, ruido, olor a quemado, motor que
tiembla pero no gira). Las sorpresas son los datos más valiosos.

## Resultados de calibración — robot 1 (2026-05-29)

Primer test de banco exitoso con el sketch ya robustecido. Robot físico **#1**
(rol arquero/delantero **aún sin asignar**; el diag usa el pinout del arquero /
`ROBOT1`).

- ✅ **Los 3 motores giran y esperan el botón en cada paso.**

> ⚠️ **CORREGIDO 2026-06-03.** La conclusión original de esta sección decía
> `MOTOR_DIR = {+1,+1,+1}` (sin inversiones). Quedó **superado**: el banco
> posterior (María/Elías, `diag_central_line_sweep_robot1`, 2026-06-01) mostró
> que el **motor 2 (driver U17) va INVERTIDO por hardware** (INA/INB cruzados;
> `ROT_M2=-1`). El sentido validado para ROBOT1 es **`{+1, -1, +1}`**, ya cargado
> en producción (`config_central.h` → `MOTOR_INVERT` + `motors_zircon.cpp`).
> La nota del viernes 2026-05-29 había dejado la tabla de sentidos **en blanco**;
> el dato real lo aportó el banco del arquero que SÍ anduvo.

> ⚠️ **A validar en el diag de avance/heading (NO en este):** en la cinemática
> (`src/shared/kinematics.cpp:14`), `+omega` gira las 3 ruedas en `+1`, o sea
> `+omega` = **horario físico**. La convención estándar es `+omega` =
> antihorario. Si el PID de heading + IMU asumen antihorario, el robot podría
> **girar para el lado contrario al corregir rumbo** (runaway). Plan de prueba:
> en `diag_central_drive`, comandar un giro chico y verificar que el robot
> corrige hacia el heading objetivo (no que se aleja). Si se aleja → invertir
> el signo del término `omega*R` en la cinemática (o el signo global de omega).

## Fixes de robustez aplicados (2026-05-29)

El sketch tenía dos bugs de banco, ya corregidos (host-verificados, compila):

1. **Avanzaba solo al arrancar (WAITING→M1→M2→M3).** Causa: avanzaba con
   *cualquier* byte por Serial, y el USB mete bytes de handshake al abrir el
   monitor. Fix: avanzar **sólo con ENTER** (`\n`/`\r`) + drenar el buffer y
   leer el botón real en `setup()`.
2. **No esperaba el botón después de arrancar un motor.** Causa: el **ruido
   eléctrico del motor** en el pin 9 generaba apretones falsos. Fix:
   **antirebote por estabilidad** (la lectura cruda debe mantenerse `DEBOUNCE_MS`
   = 50 ms para contar) + **anti-cascada** (`MIN_DWELL_MS` = 300 ms ignora
   disparos al inicio de cada estado).

## Pendientes que este test NO cubre

| # | Pendiente | Cómo se cierra |
|---|---|---|
| 1 | ~~**Veredicto pines 7/8 (TASK-036)**~~ → ✅ **RESUELTO 2026-05-31 por reasignación de UART** (no por aislar el motor): el link DOWN→CENTRAL se movió a **Serial1 (0/1)** y TOP→CENTRAL a **Serial7 (28/29)**, dejando los pines **7/8 exclusivos del motor 2 (U17)**. Ya no hay que decidir Serial2 vs Serial7 — el link es **Serial1**. | Cerrado. |
| 2 | ~~**Correspondencia geométrica exacta de la rueda física**~~ → ✅ **DEFINIDA** (CALIBRADO 2026-06-08): M1 = delantera IZQUIERDA, M2 = delantera DERECHA, M3 = trasera; `WHEEL_ANGLES_DEG = {330, 210, 90}`. Queda SOLO el tuneo fino del lateral (que no rote) + confirmar el sentido de la traslación. | Banco: `diag_central_strafe_robot1` para tunear el lateral y confirmar el sentido. |
| 3 | **Orientación / sentido por motor (`MOTOR_INVERT`)** | ROBOT1 validado: **M2 (U17) invertido → `{+1,-1,+1}`**, ya en `config_central.h` + `motors_zircon.cpp` (banco María/Elías 2026-06-01). **Falta ROBOT2/delantero**: correr este diag en el delantero y cargar su `MOTOR_INVERT` (ahí U17 es el motor 1). |
| 4 | Convención global de giro (`+omega` = horario/antihorario) | `diag_central_drive` + IMU/heading (ver caja ⚠️ arriba) |
| 5 | ~~Confirmar `PIN_KICKER_SOL` (TASK-011)~~ — **CANCELADO**: el robot no tiene kicker físico (empuja por inercia); TASK-011 cancelada. | — |
| 6 | Cinemática real — `WHEEL_ANGLES_DEG = {330, 210, 90}` **CALIBRADO 2026-06-08** en `config_central.h`; falta confirmar `WHEEL_RADIUS_MM` y el SENTIDO de la traslación en banco | `diag_central_strafe_robot1` (tuneo fino del lateral + confirmar sentido) |
| 7 | Saturación con los 3 motores simultáneos | Idem (movimientos vectoriales) |
| 8 | Encoders | Fuera de scope Incheon |

> ✅ **El #1 ya está resuelto (2026-05-31).** El link DOWN→CENTRAL es **Serial1
> (RX1 = pin 0)** — tanto en `diag_central_comm_down` como en el `comm_down.cpp` de
> producción (verificado en código 2026-06-03). Los pines **7/8 quedan exclusivos
> del motor 2 (U17)**. ⚠️ **NO cablear el link a pin 7** (era el mapeo viejo
> Serial2, ya superado — ahí ahora va señal de motor). Ver
> [`DIAG-CENTRAL-COMM-DOWN.md`](DIAG-CENTRAL-COMM-DOWN.md).

## Documentación esperada (journal entry post-test)

El equipo que ejecute este test debe dejar una entrada en
`journal/YYYY-MM-DD-diag-central-motors-<descripcion>.md` con: foto del setup,
tabla del Paso 4 completada, veredicto (Caso A/B/C/D), próxima TASK abierta, y
tiempo de la sesión.

## Referencias

- Sketch: [`software/teensy/Soccer 2026/src/diag/diag_central_motors.cpp`](../../software/teensy/Soccer%202026/src/diag/diag_central_motors.cpp)
- Environment: `[env:diag_central_motors]` en
  [`platformio.ini`](../../software/teensy/Soccer%202026/platformio.ini)
- Driver de producción: [`src/central/motors_zircon.cpp`](../../software/teensy/Soccer%202026/src/central/motors_zircon.cpp)
- Cinemática: [`src/shared/kinematics.cpp`](../../software/teensy/Soccer%202026/src/shared/kinematics.cpp) · config: [`src/central/config_central.h`](../../software/teensy/Soccer%202026/src/central/config_central.h)
- Pinout fuente: [`hardware/electronics/central-board-pack/01-pinout-y-hardware.md`](../../hardware/electronics/central-board-pack/01-pinout-y-hardware.md)
- Arduino IDE en este equipo: core `teensy:avr` 1.60.0 (verificado con `arduino-cli compile --fqbn teensy:avr:teensy41`).

## Cambios

- 2026-05-28 — creación inicial. Sketch + environment + doc. Author: Claude
  Opus 4.7 (Anthropic). Requested-by: Gustavo Viollaz (@gviollaz).
- 2026-05-29 — agregado `MOTOR_DIR[]` (sentido por motor); fixes de robustez
  (boot/serial + antirebote anti-ruido); vía Arduino IDE documentada;
  resultados de calibración de robot 1; notas de producción + convención de
  omega a validar. Author: Claude Opus 4.7 (Anthropic). Requested-by: Viollaz.
- 2026-06-07 — agregada tabla de disposición ROBOT1 (validada + re-confirmada 2026-06-06); tabla del Paso 4 completada con sentidos validados; pendiente #2 reformulado a 'geometría de rueda', no identidad. Author: Claude (coach).
