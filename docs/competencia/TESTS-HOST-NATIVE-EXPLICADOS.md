---
title: "Qué son los 858 tests host-native — guía para entender el número"
date: 2026-06-14
author: "Claude Opus 4.8 (Anthropic) — pedido de Gustavo Viollaz (@gviollaz)"
status: vivo
tipo: explicativo
related:
  - docs/competencia/TDP.md  (§3 Software — resumen del que esto es la versión larga)
  - docs/competencia/ENTREVISTA-PREP.md
  - software/teensy/Soccer 2026/scripts/run-host-tests.sh  (el runner que produce la cifra)
  - docs/competencia/assets/gen_figuras.py  (Fig. 8 — crecimiento de la cobertura)
---

# Qué son los 858 tests host-native

> **Por qué existe este documento.** En el TDP, el póster y el video decimos
> *"858 tests / 61 suites / 0 fallos"*. Es un número que impresiona, pero por sí
> solo **no comunica nada**: un juez (o un compañero nuevo del equipo) puede
> pensar que "test" significa "lo probamos 858 veces en la cancha", o que es
> relleno para inflar una cifra. Este documento traduce el número: **qué es un
> test, qué tipos hay, cuántos de cada tipo, qué errores reales atajaron antes de
> llegar a la cancha, y — sobre todo — qué NO prueban.** Si alguien pregunta
> *"¿de qué se tratan esos 800 tests?"*, la respuesta completa está acá.
>
> **Cifra viva.** 858 es la medición del **2026-06-14** (`scripts/run-host-tests.sh`,
> g++ de Webots). La suite crece cada sesión; el día de grabar/imprimir hay que
> re-medir. Todos los números de este doc salen de esa corrida real, no de memoria.

---

## 1. Qué es UN test host-native (en una frase)

> **Un test es una afirmación chequeable sobre la lógica del robot, que una PC
> verifica en milisegundos sin tocar la placa.**

Ejemplo de afirmación: *"si el error de rumbo es de 170°, el comando de giro NO
debe invertirse de signo"*. El test arma esa situación con números, llama a la
función real del robot, y compara el resultado contra lo esperado. Si coincide:
**PASS**. Si no: **FAIL**, y el problema salta en la PC, no en la cancha.

### La analogía: probar la receta antes del servicio

Pensá el "cerebro" del robot como un recetario. Cada **módulo puro**
(`src/shared/*.cpp`: cinemática, PIDs, FSM, filtros de línea…) es una receta:
entran ingredientes (números de los sensores), sale un plato (un comando para los
motores). Un **test host-native** es **probar la receta en una cocina de prueba
(la PC)** antes del servicio (el partido), **sin encender la cocina real** (la
placa Teensy con sus sensores).

| En la cocina | En el robot |
|---|---|
| La receta | El módulo puro (`src/shared/`) |
| La cocina de prueba / probar a cuchara | La PC corriendo el test con g++ |
| La cocina real, el horno que calienta de más | La placa Teensy + sensores + cancha |
| Probar y decir "le falta sal" | El `assert`: comparar resultado vs esperado |
| El comensal real | El partido en Incheon |

**Dónde se rompe la analogía (importante).** Probar a cuchara es subjetivo; un
`assert` de software es **exacto** (compara byte a byte o número a número, sin
opinión). Y la cocina de prueba **no puede** decirte si el horno real calienta de
más: eso solo lo descubrís encendiéndolo. Por eso los tests host **no
reemplazan** la prueba en banco — la complementan. Ver §4.

---

## 2. Cómo se corre (la receta técnica, reproducible)

Un comando, sin placa, sin internet, en segundos:

```bash
cd "software/teensy/Soccer 2026"
bash scripts/run-host-tests.sh
# ... tabla por suite ...
# Envs: 61  |  OK: 61  |  FAIL: 0  |  build-errors: 0
# Tests: 858  |  Failures: 0
```

Qué hace por dentro (Fig. 8 / TDP §3.1):

```
   módulo PURO (src/shared/*.cpp)  ──┐
   test (test/test_X/test_main.cpp) ─┤
   Unity vendoreado (lib/Unity)    ──┤
                                     ▼
        g++ -std=gnu++17 -I src/shared -I lib/Unity/src \
            unity.c  src/shared/*.cpp  test_main.cpp  -o test_X
                                     ▼
        ./test_X   →   PASS/FAIL en la PC (sin placa, sin red, sin Avast)
                                     ▼
        gate verde (0 fallos)  →  recién entonces se mergea
```

- **`g++` (compilador de C++)** compila la lógica pura para la PC. No hace falta
  la placa porque los módulos `src/shared/` **no incluyen `Arduino.h`** (ni
  `Wire`, ni `Serial`): son C++ portátil.
- **Unity** es el framework de tests (mini-librería, viene incluida en el repo en
  `lib/Unity/` para no depender de internet — la red del taller + Avast bloquean
  la descarga, por eso `pio test` no servía y nació este runner).
- **Sin red, sin Avast, en segundos**: ese es el punto. El ciclo "cambio código →
  sé si rompí algo" dura lo que tarda g++, no lo que tarda flashear y armar el
  robot.

> **Glosario rápido.** *Suite/env* = una carpeta `test/test_X/` con sus tests
> (hoy 61). *Test* = una función `void test_...()` con uno o más `assert` (hoy
> 858 en total). *assert* = la línea que compara "lo que dio" contra "lo que
> debía dar". *Módulo puro* = lógica sin hardware, testeable en la PC.
> *byte-idéntico / golden* = el resultado coincide exactamente con un valor de
> referencia guardado.

---

## 3. Cuántos tests hay y de qué (las 61 suites por subsistema)

Los 858 tests cubren **diez subsistemas** del robot. Esta tabla es el mapa de
"de qué se tratan":

| Subsistema | Qué verifica | Suites | Tests |
|---|---:|---:|---:|
| **Línea (placa DOWN + ingestión en CENTRAL)** | leer el anillo de 32 sensores, filtrar, calibrar, geometría de borde, encodear y que CENTRAL lo decodifique | 10 | **151** |
| **Control de movimiento (CENTRAL)** | cinemática omni-3, PIDs, pisos de PWM, impulso/freno de motores, trayectorias | 13 | **143** |
| **Localización / pose / IMU / OTOS** | trilateración 2D, fusión de heading, odometría OTOS, congelar IMU si se traba | 10 | **141** |
| **Estrategia / FSM táctica** | transiciones del cerebro (arquero/delantero), empuje sin kicker, jugar sin giróscopo | 4 | **92** |
| **Visión (cámaras OpenMV)** | parsear el frame de la cámara, fusionar 2 cámaras, identificar arco, apuntar al hueco | 4 | **71** |
| **Telemetría / monitoreo** | armar las tramas de debug, semáforo de salud de sensores, saturación | 4 | **69** |
| **Comunicaciones / contratos de datos** | frame UART con CRC, resincronización, WorldSnapshot, salud del enlace | 5 | **65** |
| **Config / persistencia / modo banco** | guardar/leer calibración en EEPROM, config de sensores, modo de prueba | 4 | **51** |
| **Pelota (modelo)** | velocidad por diferencias finitas, anticipación del arquero, "pegado" a la pelota | 3 | **38** |
| **ToF / ultrasonido (distancia)** | mantener distancia, descartar lecturas viejas, orientar zonas, back-off del HC-SR04 | 4 | **37** |
| **TOTAL** | | **61** | **858** |

### Detalle suite por suite (las 61, con su conteo real del 2026-06-14)

<details><summary>Línea (DOWN + ingestión) — 151</summary>

| Suite | Tests | Qué cubre |
|---|---:|---|
| `test_line_filters` | 39 | filtro temporal + histéresis + centroide + saturación todo-blanco |
| `test_down_model` | 25 | modelo completo de la placa DOWN (estado de línea armado) |
| `test_down_geometry` | 20 | geometría: ángulo de línea, penetración, ángulo de escape |
| `test_down_calib` | 19 | calibración de umbrales por sensor |
| `test_central_line_ingest` | 11 | DOWN→CENTRAL: encode→decode→interpretar (ver §6) |
| `test_down_encode` | 11 | serialización de `LineStatusV2` (16 B) |
| `test_down_tx` | 10 | transmisión / framing de la línea |
| `test_line_view` | 8 | helpers de lectura con compuerta `data_valid` |
| `test_down_surface` | 5 | monitor de superficie (detección de levantado) |
| `test_down_tracker` | 3 | seguimiento del borde de línea |
</details>

<details><summary>Control de movimiento (CENTRAL) — 143</summary>

| Suite | Tests | Qué cubre |
|---|---:|---|
| `test_pids` | 36 | PID heading/lateral/distancia, anti-windup, clamp anti-overflow |
| `test_kinematics` | 16 | cinemática inversa omni-3 + saturación proporcional de ruedas |
| `test_central_trajectory` | 15 | planificación de tramos de movimiento |
| `test_motor_brake` | 12 | freno anticipado de la rueda trasera |
| `test_drive_straight` | 9 | avance recto con PID de rumbo |
| `test_motor_kickstart` | 9 | impulso inicial parado→comando |
| `test_central_motion` | 8 | armado del comando de movimiento |
| `test_motor_floor_scale` | 8 | escalado del piso de PWM por rueda |
| `test_motor_power_cap` | 8 | tope de potencia |
| `test_pfm_heading` | 8 | modulación PFM del giro |
| `test_gk_motion_speed` | 5 | velocidad de patrulla del arquero |
| `test_central_gk_cross_track` | 5 | strafe del arquero por cross-track |
| `test_central_otos_ingest` | 4 | ingestión de odometría OTOS en CENTRAL |
</details>

<details><summary>Localización / pose / IMU / OTOS — 141</summary>

| Suite | Tests | Qué cubre |
|---|---:|---|
| `test_pose_targeting` | 22 | apuntar a una pose objetivo |
| `test_imu_fusion` | 20 | fusión de IMU |
| `test_localization` | 16 | trilateración 2D con 4 ToF + heading |
| `test_otos_fusion` | 14 | fusión de odometría OTOS |
| `test_pose_filter` | 14 | filtrado de pose |
| `test_imu_freeze` | 13 | detectar y congelar IMU trabado |
| `test_otos_health` | 11 | salud del sensor OTOS |
| `test_pose_fusion` | 11 | fusión de pose |
| `test_otos_position` | 10 | posición integrada del OTOS |
| `test_localization_f1a_f1b_geometria` | 10 | casos geométricos de trilateración |
</details>

<details><summary>Estrategia / FSM táctica — 92</summary>

| Suite | Tests | Qué cubre |
|---|---:|---|
| `test_strategy_transitions` | 39 | caracterización de la FSM viva (arquero + delantero) |
| `test_atk_nogyro` | 19 | delantero jugando sin giróscopo |
| `test_behind_ball_abs` | 18 | ponerse detrás de la pelota (pose absoluta) |
| `test_behind_ball` | 16 | ponerse detrás de la pelota (relativo) + empuje al arco |
</details>

<details><summary>Visión (cámaras OpenMV) — 71</summary>

| Suite | Tests | Qué cubre |
|---|---:|---|
| `test_cameras_fusion` | 27 | fusión de las 2 cámaras (frontal + trasera) |
| `test_cameras_parser` | 20 | parsear el frame que manda la cámara (CRC + fin de trama) |
| `test_clear_aim` | 13 | apuntar al hueco libre del arco rival |
| `test_goal_polarity` | 11 | identificar cuál arco es cuál (polaridad de color) |
</details>

<details><summary>Telemetría / monitoreo — 69</summary>

| Suite | Tests | Qué cubre |
|---|---:|---|
| `test_telemetry_top` | 20 | trama de telemetría USB del TOP (frame v2) |
| `test_telemetry_down` | 19 | trama de telemetría de la línea |
| `test_sensor_health` | 16 | semáforo OK/REVISAR/FALLA/SIN DATO por sensor |
| `test_telemetry_sat` | 14 | saturación de telemetría |
</details>

<details><summary>Comunicaciones / contratos de datos — 65</summary>

| Suite | Tests | Qué cubre |
|---|---:|---|
| `test_proto` | 30 | frame UART `[0xAA·LEN·TYPE·SEQ·PAYLOAD·CRC16·0x55]` + decoder resincronizante |
| `test_link_health` | 17 | salud del enlace (gaps de secuencia, CRC) |
| `test_snapshot_v3_consume` | 8 | consumir el WorldSnapshot v3 (31 B) en CENTRAL |
| `test_central_contract` | 6 | contrato de datos visto desde CENTRAL |
| `test_snapshot_roundtrip` | 4 | WorldSnapshot serializa→deserializa byte-idéntico |
</details>

<details><summary>Config / persistencia / modo banco — 51</summary>

| Suite | Tests | Qué cubre |
|---|---:|---|
| `test_calib_storage` | 21 | guardar/cargar calibración en EEPROM (round-trip + CRC) |
| `test_top_config` | 12 | config persistente de sensores del TOP |
| `test_bench_mode` | 10 | modo banco (overrides de prueba) |
| `test_loop_monitor` | 8 | monitor del período del loop principal |
</details>

<details><summary>Pelota (modelo) — 38</summary>

| Suite | Tests | Qué cubre |
|---|---:|---|
| `test_ball_velocity` | 17 | velocidad de la pelota por diferencias finitas + EMA |
| `test_ball_sticky` | 12 | mantener la pelota "pegada" |
| `test_ball_predict` | 9 | arquero que anticipa la X futura de la pelota |
</details>

<details><summary>ToF / ultrasonido (distancia) — 37</summary>

| Suite | Tests | Qué cubre |
|---|---:|---|
| `test_hcsr04_backoff` | 11 | back-off del ultrasonido HC-SR04 |
| `test_tof_stale` | 10 | descartar lecturas ToF viejas |
| `test_tof_distance_hold` | 9 | mantener distancia con ToF |
| `test_tof_zone_orient` | 7 | orientar las zonas del ToF |
</details>

---

## 4. El límite honesto: qué NO prueban estos tests

> **Un test host-native verifica la LÓGICA del robot, no el hardware.**
> Que la suite esté en verde significa *"el cerebro decide bien con estos
> números"*. **No** significa *"el robot ya funciona en la cancha"*.

Esto es regla del equipo, no opcional (ver `CLAUDE.md` → *Reglas no negociables*):

> *Testing en hardware real para todo cambio de código del robot. Solo el equipo
> humano que tiene la placa puede cerrar una TASK de hardware como `done`.*

Por eso en el TDP cada módulo lleva un **estado de madurez** honesto:

| Estado | Qué quiere decir |
|---|---|
| **VALIDADO EN BANCO** | pasa los tests host **y** ya se movió en banco (motores, árbitro, línea, snapshot) |
| **VERIFICADO EN HOST** | pasa los tests, pero todavía no se confirmó en el robot |
| **VERIFICADO EN HOST + DORMIDO POR FALLBACK** | pasa los tests y, por diseño, produce el **mismo comando** que la conducta vieja hasta que el dato nuevo fluya en banco (no introduce regresión) |

Lo que el test host **sí** atrapa: errores de lógica, de signo, de unidades, de
overflow, de contrato de datos, de manejo de casos inválidos. Lo que **no**
puede ver: que un sensor lee corrido, que un motor está cableado al revés, que el
I²C se cuelga, que el loop tarda más de lo previsto. Eso es banco.

---

## 5. Qué TIPOS de chequeo hacen (7 categorías)

Además de "por subsistema", los tests se pueden clasificar por **qué clase de
afirmación verifican**. Estas son las 7 que aparecen una y otra vez:

1. **Round-trip / contrato de datos** — serializar un mensaje y volver a leerlo
   tiene que dar **byte-idéntico**; el tamaño se fija con `static_assert`; el CRC
   y la resincronización funcionan. *Ej.:* `test_snapshot_roundtrip`, `test_proto`.

2. **Property / invariante numérica** — clamps, saturación, rangos, no-overflow,
   monotonía. *Ej.:* `kinematics::saturate_wheels()` escala las 3 ruedas sin
   cambiar la dirección; el clamp del PID nunca pasa de 327 (ver §6).

3. **Lógica de decisión / FSM** — las transiciones del cerebro disparan cuando
   deben y solo cuando deben. *Ej.:* `test_strategy_transitions` (39 tests
   caracterizan la máquina viva).

4. **Fail-safe / dato inválido** — `data_valid=0`, valores N/A, lecturas viejas
   (stale), saturación "todo blanco", sensor caído → el robot **desconfía** en vez
   de actuar con basura. *Ej.:* `test_tof_stale`, `test_sensor_health`,
   las compuertas de `test_central_line_ingest`.

5. **Fallback byte-idéntico** — una feature nueva (arquero anticipa, drive con
   OTOS, strafe por cross-track) produce **exactamente el mismo comando** que la
   conducta previa cuando el dato nuevo no está → se puede integrar "dormida" sin
   riesgo. *Ej.:* `test_ball_predict` (con velocidad 0, apunta a la pelota quieta).

6. **Regresión de bug real (guard)** — un test que reproduce un bug ya arreglado
   y **vuelve a fallar si alguien revierte el fix**. *Ej.:*
   `test_old_size_payload_rejected` (ver §6).

7. **Calibración / persistencia** — guardar config en EEPROM y recuperarla intacta
   (round-trip + CRC), con defaults que no cambian nada (no-op). *Ej.:*
   `test_calib_storage`, `test_top_config`.

---

## 6. Un test mirado de cerca: `test_central_line_ingest`

Para que se vea cómo es un test por dentro, este es uno de los más ilustrativos.
Cubre el camino DOWN→CENTRAL que **estuvo roto** (ver §7) y mezcla varias de las
categorías de §5. Sus 11 tests:

- **Round-trip real (no atajos):** arma un `LineStatusV2`, lo encodea como lo hace
  la placa DOWN, y lo decodifica **byte a byte por la misma máquina de estados**
  que usa CENTRAL al leer el puerto serie. Verifica que el ángulo, la penetración
  y los sensores activos llegan intactos.
- **Fail-safe (compuerta `data_valid`):** si el dato viene marcado como inválido,
  el ángulo devuelve 0 y la penetración devuelve 0 — **nada** de la trama se
  confía. Si no fuera así, `LINE_AVOID` retrocedería a una dirección basura y el
  PID lateral del arquero strafearía con datos inventados.
- **Regresión del P0 (guard):** `test_old_size_payload_rejected` manda un payload
  del **tamaño viejo** (5 bytes, `LineStatus`) y exige que sea **rechazado**. El
  bug original aceptaba ese tamaño; este test falla si alguien reintroduce el bug.
- **Versionado de esquema:** una trama del tamaño correcto (16 B) pero con
  `schema_version` incompatible se rechaza, no se reinterpreta como basura
  (escenario: DOWN y CENTRAL flasheados con versiones distintas).

Es decir: un solo archivo de test demuestra que la cadena de datos de línea
funciona, que desconfía de lo inválido, y que el bug que la rompió no puede volver.

---

## 7. Errores reales que atajaron antes de la cancha

Esto es lo que de verdad justifica el número. Estos bugs **existieron** en el
código y la suite host los reprodujo, los arregló y ahora los **guarda contra
regresión** — todo en la PC, antes de que arruinaran un partido. Cada uno está
documentado en journals del repo:

| Bug | Síntoma en cancha si no se atajaba | Fix | Test guardián |
|---|---|---|---|
| **Overflow `int16` del giro (sign-flip)** | `omega_centideg = omega·100`; un clamp de 360 → 36000 > 32767 → **wrap de signo** → el robot giraba **a fondo al revés** justo al saturar (error de rumbo grande) | clamp del PID 360 → **327** (327·100 = 32700 < 32767) + helper `omega_degps_to_centideg()` cableado en los 5 sitios | `test_pids` |
| **Anti-windup ausente** | tras una excursión grande de rumbo, el integral quedaba saturado y el robot **sobrepasaba** el setpoint al volver | conditional integration en heading + lateral PID | `test_pids` (incluye 2 regresiones no-saturadas bit-idénticas) |
| **CENTRAL ciego a la línea** | `comm_down.cpp` decodificaba `LineStatus` viejo (5 B) y **descartaba el 100 %** de los `LineStatusV2` reales (16 B) → CENTRAL no veía la línea, e invisible en telemetría | aceptar el tamaño/contrato nuevo | `test_central_line_ingest` (`test_old_size_payload_rejected`) |
| **`data_valid` ignorado en penetración** | el fallback del PID lateral del **arquero strafeaba con dato inválido** | `lsv2_penetration_u8` honra `data_valid` | `test_central_line_ingest` |
| **`data_valid` ignorado en ángulo** | **`LINE_AVOID` retrocedía hacia una dirección basura** | `lsv2_line_angle_deg` honra `data_valid` | `test_central_line_ingest` |

**Matiz honesto:** varios de estos se encontraron **leyendo el código en una
auditoría** (no es que la suite "los descubrió sola"). Pero la disciplina fue
siempre la misma — **TDD**: primero se escribe el test que **falla** reproduciendo
el bug (RED), después el fix mínimo, y queda **GREEN** como guardián permanente.
Y el segundo matiz, igual de importante: el `int16` sign-flip *pasa host*, pero
el **sentido de giro** se confirma en banco — el test evita el overflow, no
reemplaza la validación física (ver §4).

> Fuentes: `journal/2026-06-03-etapa2-tier1-fixes-central-host-tested.md` (los 7
> fixes Tier-1 con su patrón RED→GREEN), `journal/2026-05-29-fix-contrato-linea-central.md`
> (CENTRAL ciego a la línea), TDP §3.2/§3.3.

---

## 8. Cómo crecieron (la cifra viva)

La cobertura se construyó sesión a sesión, siempre con **0 fallos**:

```
246 → 262 → 324 → 354 → 403 → 470 → 545 → 658 → 834 → 858
May29  May29  Jun03 Jun03 Jun03 Jun03  Jun04 Jun05 Jun13 Jun14
```

Ver **Fig. 8** (`docs/competencia/assets/fig8_test_growth.png`, generada por
`gen_figuras.py`). El número es **vivo por diseño**: corremos la suite cada
sesión, así que para Incheon será mayor. Por eso siempre se publica con la
**fecha de medición**, y antes de grabar/imprimir se re-mide con
`scripts/run-host-tests.sh` y se re-propaga la cifra (hay un helper:
`docs/competencia/assets/actualizar-cifra.py`).

---

## 9. En una frase, para la entrevista

> *"858 no es '858 veces en la cancha'. Es 858 afirmaciones sobre la lógica del
> robot que una PC verifica en segundos sin la placa: contratos de datos, PIDs,
> la máquina de estados, el manejo de sensores caídos. Atraparon bugs concretos
> —un giro que se invertía por overflow, la central que quedó ciega a la línea—
> antes de que costaran un partido. Lo que NO prueban es el hardware: eso lo
> validamos en banco, y lo decimos con todas las letras."*
