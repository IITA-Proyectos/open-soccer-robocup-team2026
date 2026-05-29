---
title: "Auditoría independiente de la placa TOP pre-Incheon — verificación, tests offline y 3 fixes de firmware"
date: 2026-05-29
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8, Anthropic)"
status: final
tags: [top-board, auditoria, tests, firmware, heading, hc-sr04, pre-incheon]
robot: ambos
area: firmware
tipo: auditoria
---

# Auditoría independiente de la placa TOP pre-Incheon

> **TL;DR.** Verificación independiente completa de la placa TOP (cerebro
> sensorial, Teensy 4.0). **(1)** Corrí la suite host-native **punta a punta
> por primera vez** — antes estaba bloqueada porque `pio test` baja Unity del
> registry y Avast lo corta (TASK-025). La destrabé compilando los tests
> directo con g++ contra el Unity vendoreado: **246 tests / 19 envs / 0 fallos**,
> 100% offline. Lo dejé como script reusable (`scripts/run-host-tests.sh`).
> **(2)** Encontré y arreglé **3 riesgos de mal funcionamiento** en firmware
> host-testeable: el heading que el TOP manda al CENTRAL era **siempre 0**
> (CENTRAL navegaba ciego de orientación), el HC-SR04 **bloqueaba el loop
> 25 ms** pisando el pin del uplink, y una función muerta. **(3)** El resto de
> los hallazgos (dependientes de hardware o de otras placas) van como
> **temas-a-analizar** abajo, con su prioridad — la mayoría ya tienen TASK.
>
> **Lo aplicado son cambios de firmware que compilan limpio en ambos robots,
> pero NO están validados en hardware.** Eso lo cierra el equipo: **TASK-200**.

## Contexto y mandato de la sesión

Gustavo pidió, para la placa TOP, lo mismo que se hizo con DOWN (commit
`02d08ae`): *"verificación independiente completa de lo que está hecho y lo
que falta, testear todo lo desarrollado buscando puntos de falla, y en
paralelo desarrollar lo faltante y resolver cualquier inconveniente que
genere riesgos de mal funcionamiento"*.

Recordatorio de qué es el TOP (de `main_top.cpp`): es el **cerebro
sensorial**. Percibe el mundo (2 cámaras OpenMV, 2 IMU BNO055, ToF + HC-SR04,
odometría OTOS desde DOWN, comm árbitro) y a 100 Hz arma un `WorldSnapshot`
que manda al CENTRAL por Serial2. **No decide táctica ni mueve motores.**

## 1. Verificación: ¿qué está hecho y funciona?

### Compila
Ambos robots compilan limpio tras los fixes:
`pio run -e top_robot1` y `-e top_robot2` → **SUCCESS, 0 warnings**.

### Tests host-native — destrabados y verdes

**El problema histórico (TASK-025).** El env `test_native` de `platformio.ini`
usa `test_framework = unity`, que **siempre** resuelve `throwtheswitch/Unity`
desde el registry de PlatformIO. Avast + la red del taller cortan esa
descarga, así que `pio test -e test_native` da `[ERRORED]` en los 19 envs
**sin compilar una sola línea de test**. Por eso `ESTADO-ACTUAL.md` decía que
la suite nunca se había corrido punta a punta. Lo confirmé: el `pio test`
corre, tarda ~72 s por env y todos ERRORean en "Installing Unity".

**La solución.** PlatformIO no es necesario para correr estos tests: todos los
módulos de `src/shared/` son **host-safe** (verificado: ninguno incluye
`Arduino.h`) y el env `test_native` linkea SOLO contra `src/shared`
(`build_src_filter = +<shared/>`). Así que compilo cada `test_main.cpp` +
`src/shared/*.cpp` + `lib/Unity/src/unity.c` directo con g++. Cero red.

**Resultado (corrido hoy, ambos verificados):**

```
TEST ENV                        TESTS    FAILS      IGN   RESULT
test_behind_ball                   16        0        0   OK
test_calib_storage                 19        0        0   OK
test_cameras_fusion                16        0        0   OK
test_central_contract               2        0        0   OK
test_central_motion                 9        0        0   OK
test_central_trajectory             7        0        0   OK
test_down_calib                     5        0        0   OK
test_down_encode                    3        0        0   OK
test_down_geometry                 20        0        0   OK
test_down_model                     5        0        0   OK
test_down_surface                   5        0        0   OK
test_down_tracker                   3        0        0   OK
test_kinematics                    11        0        0   OK
test_line_filters                  33        0        0   OK
test_localization                  14        0        0   OK
test_pids                          18        0        0   OK
test_proto                         13        0        0   OK
test_sensor_health                 12        0        0   OK
test_strategy_transitions          35        0        0   OK
Envs: 19  |  OK: 19  |  FAIL: 0
Tests: 246  |  Failures: 0
```

Lo dejé como **`scripts/run-host-tests.sh`** (reusable, documentado, alineado
con TASK-023 build/tooling CI y con el principio de que el repo sobreviva a
2027). Uso: `bash scripts/run-host-tests.sh` (o con un prefijo para uno solo).
Actualicé la tabla de tests y el "Estado" en `ESTADO-ACTUAL.md` con los
números reales (la tabla vieja decía "≥130 estimado" y faltaban 3 suites).

> **Qué NO prueban estos tests.** Son host-native sobre lógica pura
> (`src/shared`). **No prueban hardware** (cámaras, ToF, IMU, UARTs reales) ni
> el firmware integrado del TOP corriendo en el Teensy. Verde acá ≠ el robot
> percibe bien. Eso es hardware test, y lo cierra el equipo.

### Módulos del TOP — estado leído del código

| Módulo | Estado | Nota |
|---|---|---|
| `main_top.cpp` | vivo | loop no bloqueante tras el fix #2 (ver abajo) |
| `sensors_imu` (2× BNO055) | vivo | heading real; ahora **sí** llega al snapshot |
| `sensors_tof` (VL53L7CX U2) | vivo parcial | **solo el frontal U2** está instalado y enumerado |
| `cameras` + `cameras_runtime` | vivo | fusión front+back; **sin CRC** (tema abajo) |
| `comm_central` (→ snapshot) | vivo | Serial2 @ 230400; baud hardcoded (tema) |
| `comm_down` (← OTOS) | vivo parcial | recibe pose/vel pero **TOP no las usa** (tema) |
| `comm_arbiter` (↔ COMM) | vivo | referee_cmd + partner flags |
| `localization` (trilateración) | vivo, **inerte en HW actual** | nunca `valid` con TOFs solo en eje Y |

## 2. Lo que resolví en esta sesión (firmware host-testeable)

Tres temas que clasifiqué como **riesgo de mal funcionamiento** y eran
arreglables sin tocar hardware. Los tres compilan limpio en ambos robots.
**Faltan validar en hardware (TASK-200).**

### Tema A (RESUELTO en firmware) — El heading al CENTRAL era siempre 0

**Categoría:** control / percepción · **Robot afectado:** ambos · **Prioridad: P1**

**Qué observaba.** `build_snapshot()` (`main_top.cpp`) cargaba
`s.my_heading_centideg = pose.heading_centideg`, donde `pose` viene de
`localization_runtime_get_pose()`. Pero `localization_compute()`
(`src/shared/localization.cpp:130`) escribe `heading_centideg` **solo dentro
del bloque `if (x_count > 0 && y_count > 0)`** (línea 118). Con el hardware
actual hay TOFs **solo en el eje Y** (el único instalado es el frontal U2), así
que `x_count` es siempre 0, la pose **nunca es `valid`**, y
`pose.heading_centideg` se queda en **0 permanente**. Resultado: el CENTRAL
recibía heading=0 fijo. Y lo consume **sin gatearlo por confidence**
(`world_model.cpp:49` lo divide /100 directo, solo con freshness de 500 ms),
así que navegaba creyendo que el robot apunta al arco rival **siempre**.

**Por qué importa.** El robot SÍ conoce su orientación — tiene 2 BNO055
funcionando. El bug es de cableado de datos: se mandaba la orientación de un
módulo (localization) que está inerte, en vez del IMU. Cualquier maniobra del
CENTRAL que dependa del heading (ir derecho, orientar al arco, el propio
`diag_central_drive` de TASK-037) operaba con un valor falso.

**Fix aplicado.** `s.my_heading_centideg = sensors_imu_get_heading_centideg()`.
El heading queda **desacoplado** de la validez de la POSICIÓN: la posición
sigue gateada por `confidence` (cae a 0 si la pose no es válida), pero la
orientación va directa del IMU, que es la fuente correcta. Estrictamente más
correcto y no rompe nada (`#include "sensors_imu.h"` ya estaba).

**Risk-no-fix.** CENTRAL navega con heading falso → no orienta al arco, el
`diag_central_drive` (TASK-037) mide contra un heading mentiroso.
**Risk-fix.** Bajísimo: se cambia una fuente de dato por otra ya disponible y
testeada. El único matiz es que `sensors_imu` y `localization` calibran su
offset en el **mismo** instante de boot (robot apuntando a +Y), así que no hay
doble-cero ni salto. **Tiempo:** 0 (aplicado).

**Plan de prueba en hardware (TASK-200, parte a).**
1. Robot encendido apuntando al arco rival. Monitor serie del CENTRAL.
2. Girar el robot a mano 90°/180°; verificar que `world_model_get_my_heading_deg()`
   sigue el giro real (±5°).
3. Criterio: heading reportado ≈ orientación física. Antes del fix quedaba
   clavado en ~0 sin importar el giro.

### Tema B (RESUELTO en firmware) — HC-SR04 bloqueaba el loop 25 ms sobre el pin del uplink

**Categoría:** firmware / timing · **Robot afectado:** ambos · **Prioridad: P0** (ya trackeado por TASK-014)

**Qué observaba.** `sensors_tof.cpp` llamaba
`pulseIn(PIN_HCSR04_ECHO, HIGH, 25000UL)` cada ~3 ticks. `PIN_HCSR04_ECHO = 7`
(`pinout_common.h`), que en Teensy 4.0 **es Serial2 RX2** — el UART por el que
el TOP manda el `WorldSnapshot` al CENTRAL. Como el CENTRAL nunca le transmite
al TOP (verificado en `comm_top.cpp`: solo lee), esa línea idlea en HIGH y
`pulseIn` **espera el timeout completo de 25 ms** en cada lectura. Dos daños:
(1) roba 25 ms a un loop que corre a 100 Hz (10 ms de presupuesto) → mata la
cadencia del uplink; (2) lee basura → `min_obstacle_mm` contaminado → puede
disparar evasión espuria en el CENTRAL o enmascarar un obstáculo real.

**Fix aplicado.** Gateé todo el HC-SR04 tras `#ifdef TOP_ENABLE_HCSR04`
(**OFF por default**): la función `read_hcsr04()`, los `pinMode()` de init, y la
llamada en el tick. Sin el flag, el módulo **no toca los pines 6/7** ni llama a
`pulseIn`, y `sensors_hcsr04_get_distance_mm()` devuelve `TOF_NO_READING`
(queda naturalmente excluido del `min`). El VL53L7CX frontal U2 ya cubre la
distancia frontal, así que el HC-SR04 es **redundante** hoy. Dejé un bloque de
comentario grande explicando el conflicto de pin 7 para que nadie lo reactive
sin mover antes el ECHO a un pin libre.

**Risk-no-fix.** Loop colgado 25 ms + overflow del buffer RX (~23 ms) que
corrompe la odometría DOWN→TOP **en silencio** (lo documenta TASK-014).
**Risk-fix.** Se pierde el HC-SR04 como sensor de respaldo frontal — aceptable
porque el ToF frontal ya da esa medida; reactivable con el flag tras mover el
pin. **Tiempo:** 0 (aplicado). **Esto resuelve el lado firmware de TASK-014**;
queda pendiente la decisión de hardware (reasignar el ECHO) + medir el período
real del loop con osciloscopio (eso sigue siendo del equipo).

**Plan de prueba en hardware (TASK-200, parte b + TASK-014).** Ver TASK-014:
instrumentar el loop con toggle de GPIO + osciloscopio, confirmar que el peor
caso ya no tiene el escalón de 25 ms, y que la odometría DOWN→TOP no se corrompe
(contador CRC = 0 durante 5 min).

### Tema C (RESUELTO) — Función muerta en comm_down

**Categoría:** firmware / limpieza · **Robot afectado:** ambos · **Prioridad: P2**

`comm_down.cpp` tenía `void send_empty(MsgType)` en el namespace anónimo, sin
ningún caller → warning "defined but not used". La removí. `g_send_seq` sigue
en uso por `comm_down_send_reset_otos`/`_send_calib_line`, así que no aparece
warning nuevo. **Tiempo:** 0 (aplicado).

## 3. Temas-a-analizar (NO los toqué — dependen de hardware u otras placas)

Los presento en formato coach. **La mayoría ya tienen TASK**; los referencio
en vez de duplicar el plan.

### Tema D — Solo 1 ToF enumerado; localization está inerte
**Categoría:** percepción / localización · **Robot afectado:** ambos · **Prioridad: P1**

**Qué observo.** `sensors_tof_init()` solo inicializa el frontal U2; los slots
U3/U5/U17 devuelven `TOF_NO_READING`. La trilateración necesita al menos un
TOF en el eje X **y** otro en el eje Y para ser `valid`. Con un solo TOF
frontal (eje Y), `localization` **nunca produce pose válida** — corre pero su
salida se descarta (confidence=0). Es la causa raíz del Tema A.

**Estado.** Ya trackeado: **TASK-033** (cuántos ToFs para Incheon), **TASK-034**
(arquitectura de localización), **TASK-035** (validar trilateración en HW),
**TASK-038** (pines XSHUT del bodge). Bloqueante de fondo: TOP rev 1.0 no
rutea los XSHUT (journal `2026-05-25`), máximo 2 ToFs sin rework.
**Risk-no-fix.** El robot no tiene auto-localización absoluta en Incheon (juega
solo con heading + visión, que para el plan "robot honesto" puede alcanzar).
**Tiempo:** depende de la decisión de TASK-033/034 (bodge de Enzo).

### Tema E — Enlace de cámara sin CRC ni fin-de-trama robusto
**Categoría:** visión / comunicación · **Robot afectado:** ambos · **Prioridad: P1**

**Qué observo.** La cadena OpenMV→Teensy (`cameras*`) no valida integridad con
CRC. Un byte corrupto en la ráfaga puede meter una pelota fantasma en el
snapshot. **Estado:** ya trackeado por **TASK-015**. **Risk-no-fix.** Detección
de pelota/arco intermitente o fantasma bajo ruido eléctrico de motores.

### Tema F — Polaridad de arco hardcoded (yellow=opp, blue=own)
**Categoría:** estrategia / percepción · **Robot afectado:** ambos · **Prioridad: P1**

**Qué observo.** `main_top.cpp:78` mapea amarillo=rival / azul=propio fijo. La
polaridad real depende del lado de cancha que asigna el árbitro al inicio.
**Estado:** ya trackeado por **TASK-024** (arranque rol/polaridad). Hay un TODO
explícito en el código (pendiente Enzo). **Risk-no-fix.** Si el árbitro nos
asigna el lado contrario, el robot ataca su propio arco. **P1 dura.**

### Tema G — Odometría OTOS recibida pero ignorada por el TOP
**Categoría:** percepción / fusión · **Robot afectado:** ambos · **Prioridad: P2**

**Qué observo.** `comm_down` decodifica y cachea `DOWN_OTOS_POSE`/`_VEL` (con
getters `comm_down_get_pose/_velocity` + freshness), pero `build_snapshot()`
**no los usa**. La odometría de las 2 OTOS del DOWN no aporta ni a la pose ni a
la velocidad propia del snapshot. **Risk-no-fix.** Se desperdicia una fuente de
posición/velocidad relativa que podría complementar la localización inerte
(Tema D). **Risk-fix.** Requiere decidir el marco de referencia y fusión — es
diseño, cae bajo la moratoria. **Tiempo:** 0.5–1 día de diseño + test.
Candidato a research backlog post-hardware-up.

### Tema H — Velocidad de pelota siempre 0 en el snapshot
**Categoría:** percepción · **Robot afectado:** ambos · **Prioridad: P2**

**Qué observo.** `WorldSnapshot` tiene `ball_vx_mm_s`/`ball_vy_mm_s`
(`types.h:104-105`) pero `build_snapshot()` no los completa → quedan en 0 (por
`WorldSnapshot s{}`). El CENTRAL no puede anticipar la trayectoria de la pelota.
**Estado:** es la deuda #3 conocida. **Risk-no-fix.** El robot persigue la
posición instantánea de la pelota, no la lidera; pierde duelos a pelota en
movimiento. Capitalizable a 2027.

### Tema I — Baud del uplink hardcoded; comandos CENTRAL→TOP NO-OP
**Categoría:** comunicación · **Robot afectado:** ambos · **Prioridad: P2**

**Qué observo.** (1) `comm_central.cpp` arranca `Serial2.begin(UART_BAUD)` con
valor hardcoded; si CENTRAL y TOP no coinciden, el enlace falla silencioso —
conviene una sola fuente de verdad del baud. (2) `comm_central` tiene
`handle_frame` NO-OP: si el CENTRAL alguna vez manda comandos al TOP (reset
OTOS, calib), hoy se ignoran. **Risk-no-fix.** Bajo hoy (CENTRAL solo lee del
TOP). Queda como nota para cuando se quiera comando bidireccional.

## 4. Qué falta para Incheon (resumen TOP)

- **P0/P1 con TASK abierta:** ToFs/localización (TASK-033/034/035/038), CRC de
  cámara (TASK-015), polaridad de arco (TASK-024), medir loop en HW (TASK-014),
  **validar los 2 fixes de esta sesión (TASK-200, nueva)**.
- **P2 capitalizable a 2027:** fusión OTOS (Tema G), velocidad de pelota
  (Tema H), baud único + comando bidireccional (Tema I).
- **Bloqueantes globales (no del TOP):** COMM sin flashear (TASK-006), cámaras
  sin recalibrar para Incheon (TASK-022).

## 5. Pendiente humano

**TASK-200** (nueva, rango TOP): validar en hardware los 2 fixes de
comportamiento — (a) el heading del IMU llega al CENTRAL y el robot orienta
bien, (b) el loop ya no se cuelga 25 ms con el HC-SR04 apagado. Detalle y
criterios en el archivo de la task. **Yo no puedo cerrar esta TASK** — la cierra
quien tiene la placa en la mano.

## Archivos tocados

- `src/top/main_top.cpp` — heading del IMU (Tema A).
- `src/top/sensors_tof.cpp` — HC-SR04 gateado tras `TOP_ENABLE_HCSR04` (Tema B).
- `src/top/comm_down.cpp` — removida `send_empty()` muerta (Tema C).
- `scripts/run-host-tests.sh` — **nuevo**, runner host-native offline.
- `docs/ESTADO-ACTUAL.md` — tabla de tests real (246/19/0) + avance 2026-05-29.
- `team-tasks/2026-05-29-task-200-*.md` — **nueva**, validación HW de los fixes.
- `team-tasks/2026-05-18-task-014-*.md` — nota: lado firmware resuelto.
