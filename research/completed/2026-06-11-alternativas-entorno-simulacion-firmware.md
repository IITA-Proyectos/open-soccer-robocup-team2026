---
title: "Alternativas de entorno de simulación para probar los programas de fútbol sin robot"
date: 2026-06-11
author: "Claude (Anthropic - Claude Fable 5)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: completed
tipo: research
tags: [simulacion, firmware, central, top, down, testing, webots, gazebo]
---

# Alternativas de entorno de simulación — informe de viabilidad

**Pregunta de Gustavo (2026-06-11):** ¿qué entorno de simulación nos permite
probar los programas de fútbol? Ideal: correr los programas de las 3 placas
(CENTRAL, TOP, DOWN) simulando delays, comunicación entre placas y ruido en
las comunicaciones. De mínima: simular la física del robot + el programa de
la CENTRAL solamente. Candidatos a investigar: Gazebo, Webots, MathWorks u
otros. Entregable: informe de alternativas y viabilidad a corto plazo.

**TL;DR (recomendación del coach):** no empezar por un simulador "grande".
El repo ya tiene el 60% de un simulador propio construido sin saberlo:
la FSM de la CENTRAL compila host-native con un shim de 5 líneas, los
contratos UART tienen encoders host-testeados, y la caja negra da ground
truth real para validar. La ruta de mejor relación costo/valor es
**incremental**: (A) gemelo 2D host-native de la CENTRAL en días →
(B) hardware-in-the-loop con la CENTRAL real y mundo simulado por PC →
(D) las 3 placas lógicas con UARTs virtuales. **Webots queda como inversión
2027** (sinergia directa con el proyecto Rescate Simulado/Erebus, que ya es
Webots). Gazebo y MATLAB/Simulink: descartados para este equipo y este plazo.

---

## 1. Lo que ya tenemos (y cambia toda la ecuación)

Un informe genérico compararía simuladores en abstracto. Acá lo decisivo es
el inventario de activos del repo — cada uno reduce semanas del camino:

| Activo | Dónde | Qué aporta a un simulador |
|---|---|---|
| 58 suites de test host-native (798 asserts, Unity, `pio test -e test_native`) | `test/` | La separación lógica-pura / hardware **ya está hecha** para `src/shared/` |
| FSM táctica completa (arquero v3.3 + delantero, INTERCEPT/CLEAR, behind-ball) | `src/central/strategy.cpp` | Su única dependencia Arduino es `millis()` (2 llamadas) → **compila en PC con un stub trivial**. Entrada = `world_model`, salida = `MotorCommand`. Interfaz de inyección natural. |
| Cinemática inversa host-testeada | `src/shared/kinematics.{h,cpp}` | La **cinemática directa** (ruedas→movimiento del chasis) es invertir esa misma matriz 3×3 — ~50 líneas, verificable contra los 11 tests existentes |
| Encoders/decoders de los contratos UART host-native | `src/shared/{down_encode,proto,crc16}` + parsers | Generar tráfico **binario válido** (WorldSnapshot v3 31 B, LineStatusV2) desde una PC es gratis; corromperlo a propósito (ruido) también |
| Caja negra 50 Hz (estado FSM + snapshot + cmd + PWM real) + `tools/blackbox/analizar_corrida.py` | firmware CENTRAL + `tools/blackbox/` | **Ground truth de corridas reales** para validar el simulador + el mismo formato CSV sirve de salida del sim → los detectores (flapping, empuje torcido…) corren igual sobre corridas simuladas |
| App PC monitor-base (Python, con tests; dibuja cancha, anillo, OTOS, LineStatusV2) | `software/teensy/Soccer 2026/tools/monitor-base/` | Visualizador reutilizable: el sim puede emitir la misma telemetría y verse en la app que el equipo ya conoce |
| Geometría de línea/cancha host-testeada | `src/shared/{line_geometry,line_view,localization}` | El "sensor de línea simulado" se construye con los mismos módulos que interpretan la línea real |
| Proyecto Rescate Simulado 2026 (Erebus sobre Webots, Python) | repo aparte | El equipo **va a aprender Webots sí o sí** para Roboliga nov-2026 → toda inversión Webots en soccer se amortiza doble en 2027 |

Conclusión del inventario: **el costo de entrada a un simulador propio es
inusualmente bajo para este repo**, y el costo de validarlo contra la
realidad también (caja negra). Eso invierte la decisión típica
"comprar simulador grande vs construir": acá construir chico gana.

## 2. Alternativas evaluadas

### A. Gemelo 2D host-native de la CENTRAL (SIL liviano) — ⭐ recomendada como primer paso

**Qué es.** Un ejecutable de PC (mismo `pio` con un env `native` nuevo, o
CMake simple) que linkea `strategy.cpp` + `src/shared/` reales contra:
un stub de `millis()`, una física 2D mínima (cinemática directa omni de 3
ruedas + pelota con fricción + paredes + líneas de la cancha con las
dimensiones de `pinout_common.h`), y generadores de `WorldSnapshot` /
`LineStatusV2` sintéticos calculados desde la pose simulada. Delay y ruido
se inyectan como colas con latencia configurable + corrupción/elisión de
campos. Salida: CSV en formato caja negra (lo lee `analizar_corrida.py`
sin cambios) y/o telemetría para monitor-base.

Es **exactamente el "de mínima"** que pide la pregunta.

- **Cubre:** la FSM completa de ambos roles, tuning de parámetros (pulsos GK
  35°→20°, settle 700→400 se pueden pre-explorar acá antes del banco),
  regresión automática (100 partidos simulados por cambio de firmware, en CI),
  delays de snapshot (reproduce el bug "TOP a 4 Hz" del 2026-06-10 en sim),
  ruido a nivel de **datos** (campos corruptos, edad del snapshot).
- **NO cubre:** parsing UART real (el ruido se inyecta a nivel de structs, no
  de bytes — eso lo cubre B), dinámica real de motores (PWM no lineal por
  rueda, slip — documentado en FUENTES-DE-VERDAD; mitigable calibrando el
  modelo contra CSVs reales de la caja negra), colisiones realistas robot-robot.
- **risk-no-fix:** seguimos dependiendo 100% de banco físico para CADA ajuste
  de FSM; cada hora de cancha se gasta en cosas que un sim filtraría gratis
  (regresiones lógicas, signos, timeouts).
- **risk-fix:** la trampa clásica — **creerle al sim más de lo que merece**.
  Un sim sin validar contra caja negra produce confianza falsa ("en sim
  andaba"). Mitigación: el plan de prueba de §4 es obligatorio antes de usar
  el sim para decidir nada. Segundo riesgo: tiempo de coach/sesión que
  compite con preparar Incheon.
- **Tiempo honesto:** v1 jugable (arquero patrullando con línea simulada +
  delantero persiguiendo pelota): **2–4 días de sesión Claude + revisión**.
  Con visualización en monitor-base: +1–2 días. Para alumnos solos sería
  2–3 semanas — este es un caso legítimo de acelerador IA (`vibe-robotics-coding`).

### B. Hardware-in-the-loop: CENTRAL real + mundo simulado desde la PC

**Qué es.** La placa CENTRAL **real**, con el binario **real** de
competencia, conectada por 2 adaptadores USB-UART (3,3 V) a una PC que se
hace pasar por TOP y DOWN: les habla los contratos reales (WorldSnapshot v3
por el lado de Serial7, LineStatusV2/OTOS por Serial1), cierra el lazo
leyendo el PWM aplicado vía telemetría USB/caja negra, y mueve la física
simulada en consecuencia. Reusa los encoders host-native para generar
frames válidos… y para generar frames **inválidos a propósito**.

- **Cubre lo que A no puede:** parsing UART real, timing real del loop de
  100 Hz, watchdogs, y **ruido de comunicaciones DE VERDAD** — bytes
  perdidos, CRC corruptos, frames partidos, latencia inyectada en el cable.
  Es la única alternativa de la lista que ejercita los caminos RX reales del
  firmware (donde viven los bugs de `seqGap`/`STALE`/resync que ya vimos en
  banco). También sirve al revés: una fake-CENTRAL para probar la TOP real.
- **NO cubre:** motores físicos (el robot está "en el aire" lógicamente);
  la física sigue siendo la del modelo PC (la misma de A — se comparte).
- **risk-no-fix:** los bugs de robustez de comunicación solo aparecen en
  cancha, donde son carísimos de diagnosticar (ya pasó: artefacto `lost`
  enorme con `crc=0` del 2026-06-01 sigue abierto — con esto se reproduce
  en escritorio).
- **risk-fix:** bajo. Hardware barato (2 adaptadores USB-TTL ~US$3 c/u —
  verificar stock en el labo; cualquier CP2102/FT232 3,3 V sirve). El riesgo
  es de alcance: tentación de simular "todo" por acá; mantener el objetivo
  en robustez de comms.
- **Tiempo honesto:** v1 (fake-DOWN + fake-TOP estáticos con escenarios
  guionados + inyector de ruido): **~1 semana** apoyándose en la física de A.
  Depende de A para el lazo cerrado completo.

### C. Webots + adaptación de rcj-soccersim — la inversión 2027

**Qué es.** Webots ([Cyberbotics](https://cyberbotics.com), open source,
**Windows nativo**, física ODE 3D) + el simulador oficial de RoboCupJunior
[rcj-soccersim](https://github.com/robocup-junior/rcj-soccersim) (estable
con Webots R2025b) como punto de partida para el campo y el árbitro
automático.

Matices importantes que salieron de la investigación:

1. Los robots de rcj-soccersim son **diferenciales de 2 ruedas con
   controladores Python** (es la liga Soccer *Simulation*, no un gemelo de
   Soccer Open). Usarlo "as-is" implicaría **portar la FSM a Python = dos
   implementaciones que divergen** → trampa mortal, descartado.
2. El camino correcto: campo/árbitro de rcj-soccersim + **PROTO custom del
   robot omni de 3 ruedas** (Webots trae un sample "omni wheels" en
   `samples/howto`; hay papers de robots 3WD omni en Webots) + **controlador
   C++ que linkea `strategy.cpp` + `src/shared/` reales** con un shim
   Webots→`world_model` / `MotorCommand`→motores. Mismo principio que A,
   pero con física 3D de verdad (colisiones, pelota rodando, empujes).
3. La comunicación entre placas acá no se simula naturalmente — se modela
   como delay/jitter en el shim (igual que A). Para ruido de comms real,
   B sigue siendo el camino.

- **Cubre:** física 3D honesta (empujes robot-robot, pelota con rebotes,
  vuelco), partidos completos 2v2 con árbitro, visual potente para alumnos
  y para el video técnico/judging.
- **risk-no-fix:** ninguno para Incheon; para 2027 se pierde la oportunidad
  de entrenar estrategia en sim como hacen los equipos top de ligas mayores.
- **risk-fix:** curva de aprendizaje Webots (PROTO, física, tuning de
  fricción de ruedas omni — notoriamente delicado en sim); riesgo de
  hundir semanas pre-Incheon en una herramienta que no llega a tiempo.
- **Tiempo honesto:** **1,5–3 semanas** para un mundo jugable con el robot
  omni + FSM real linkeada. NO entra antes de Incheon sin canibalizar banco.
  **Sí entra** entre Incheon y el Nacional de noviembre, y la sinergia con
  Erebus (mismo Webots, alumnos 2027) es real.

### D. SIL completo: las 3 placas lógicas + UARTs virtuales — el "ideal" por etapas

**Qué es.** La evolución natural de A: un orquestador host-native que corre
la **lógica** de las 3 placas (gran parte ya vive en `src/shared/`:
`imu_fusion`, `cameras_fusion`, `line_tracker`, `down_model`,
`ball_predict`…) conectadas por UARTs virtuales — colas de **bytes** con
latencia, jitter, drop y corrupción configurables, usando los
encoders/parsers reales. Los `main_top.cpp`/`main_down.cpp` reales no se
linkean tal cual (tocan drivers I²C: BNO, VL53L7CX, OTOS) — se reconstruye
el pipeline lógico placa por placa, empezando por lo ya host-testeado.

- **Cubre:** el pedido "ideal" completo — 3 programas, delays, comms y ruido
  a nivel de bytes — sin hardware. Reproduce en PC fenómenos como el bug del
  soft-resync del `imu_fusion` (2026-06-11) inyectando un secundario
  congelado sintético. Regresión de integración en CI.
- **NO cubre:** los drivers reales (el freeze del bus Wire bajo carga NO se
  reproduce acá — eso es físico) ni el timing real del Cortex-M7 (eso es B).
- **risk-fix:** es el de mayor mantenimiento: cada cambio de contrato (ya
  hubo 2 wire-breaking) obliga a tocar el sim. Mitigación: el sim usa los
  MISMOS encoders del firmware, así que se rompe en compilación, no en
  silencio.
- **Tiempo honesto:** **+2–3 semanas sobre A**, incremental (primero
  CENTRAL+DOWN lógico, después TOP). Post-Incheon.

### E–H. Descartadas para el corto plazo (con motivo)

| Alternativa | Por qué NO ahora |
|---|---|
| **Gazebo** (gz-sim Ionic/Harmonic) | Windows sigue **experimental** (mantenedores dixit; bugs WSLg conocidos); ecosistema ROS-céntrico que el equipo no usa; cero assets RCJ. Todo el valor que daría lo da Webots con menos fricción en las PCs Windows del equipo. |
| **MATLAB/Simulink** (MathWorks) | Licencia paga; el modelo se escribe en otro lenguaje → **divergencia garantizada** con el firmware C++; correr el firmware real adentro (S-functions/Embedded Coder) es posible pero es un proyecto en sí. Útil en ligas mayores con pipelines model-based; no para este equipo en este plazo. |
| **CoppeliaSim** | Técnicamente comparable a Webots (Windows nativo, edu gratuita), pero sin rcj-soccersim ni sinergia Erebus → Webots domina en todos los criterios del repo. |
| **Renode** (emular el binario i.MX RT1062) | Fascinante: correría el **binario real** de las 3 Teensy en PC. Pero el soporte i.MX RT es parcial y habría que modelar periféricos (LPUART×8, LPI2C, FlexPWM, ADC) — proyecto de investigación de meses. Anotado para r-d-2027 si alguien quiere una tesis. |

## 3. Recomendación y priorización (criterios del repo: aprendizaje / reuso 2027 / documentación)

**Ruta incremental A → B → (D y/o C)**, con prioridades honestas:

| Paso | Prioridad | Cuándo | Por qué |
|---|---|---|---|
| **A. Gemelo 2D CENTRAL** | **P2 alta** | Puede arrancar ya (sesión Claude dedicada) | Único con payoff plausible PRE-Incheon: pre-tunear pulsos GK y probar INTERCEPT/CLEAR sin cancha. Días, no semanas. Reusa todo. |
| **B. HIL CENTRAL real** | **P2** | Post-práctica 06-12, si hay banco libre / post-Incheon | Único que prueba robustez REAL de comms (el `lost`/`crc=0` abierto). Hardware ~US$10. |
| **D. SIL 3 placas** | **P2 (2027)** | Post-Incheon | El "ideal" pedido; base de regresión de integración para el equipo 2027. |
| **C. Webots** | **P2 (2027)** | Jul–oct 2026 (antes del Nacional) | Física 3D + partidos 2v2 + sinergia Erebus + material de judging. |

**Advertencia del coach (importante):** nada de esto es P0/P1. A 19 días de
Incheon, **una hora de banco real vale más que diez de simulador** — la
moratoria de fábrica-de-papel existió por algo. La forma sana de hacer A es
UNA sesión acotada que termina con el plan de prueba de §4 ejecutado, no un
proyecto paralelo que compita con la práctica de alumnos y las verificaciones
pendientes de R1. Si no hay tiempo, A entera se difiere a julio sin culpa:
el informe queda, los activos no se vencen.

## 4. Plan de prueba en hardware real (obligatorio — sin esto el sim no se usa para decidir)

La validación de un simulador ES contra el hardware: **replay de caja negra**.

1. **Setup.** Tomar 3 CSVs de caja negra de corridas reales conocidas y ya
   analizadas (ej.: patrulla v3.3 del banco 2026-06-10; una corrida de
   delantero de la práctica 2026-06-12; una corrida con EMERGENCY_LINE).
   Extraer de cada CSV la secuencia de entradas (snapshot, línea, OTOS) y
   alimentarla al sim **tal cual** (modo replay, sin física).
2. **Criterio de aceptación medible.** Para las mismas entradas, la FSM
   simulada reproduce ≥90% de los estados FSM del CSV real (comparados a
   50 Hz, tolerancia ±2 ticks en las transiciones) y los `MotorCommand`
   coinciden en signo y dentro de ±15% en magnitud. Si no pasa, el shim
   está mintiendo y el sim NO se usa para tunear nada hasta arreglarlo.
3. **Validación de física (fase 2).** Modo lazo cerrado sobre el escenario
   patrulla: la trayectoria simulada debe rebotar en las mismas líneas y con
   el mismo sentido de patrulla que el video/journal del banco 2026-06-10;
   el detector "EMPUJE TORCIDO" de `analizar_corrida.py` corrido sobre el
   CSV simulado no debe disparar donde el real no dispara.
4. **Regresión sobre vecinos.** El harness del sim vive FUERA de `src/`
   (o como shims aditivos gateados off-by-default): se verifica que el gate
   host (`pio test -e test_native`) y los `pio run -e` de los envs de
   competencia quedan **byte-idénticos** antes/después.

## 5. Fuentes externas consultadas

- rcj-soccersim (oficial RCJ, Webots): https://github.com/robocup-junior/rcj-soccersim y docs https://robocup-junior.github.io/rcj-soccersim/ — estable con Webots R2025b; robots diferenciales, controladores Python, árbitro automático incluido.
- Webots samples omni: mundo `samples/howto/omni_wheels` + tutorial https://cyberbotics.com/doc/guide/samples-howto; robot 3WD omni en Webots: IEEE https://ieeexplore.ieee.org/document/9612576; PROTO Robotino 3 (omni comercial): https://www.cyberbotics.com/doc/guide/robotino3
- Estado de Gazebo en Windows (experimental, junio 2025): https://www.mcguirerobotics.com/blog/2025/06/02/ros-2-across-the-windows-verse/ y issue WSLg https://github.com/gazebosim/gz-sim/issues/2670
- Renode (emulación Cortex-M): https://interrupt.memfault.com/blog/intro-to-renode — soporte i.MX RT parcial, periféricos Teensy no cubiertos.

## 6. Decisión pendiente (del equipo, no de este informe)

- ¿Se autoriza UNA sesión acotada para A antes de Incheon, o A entera pasa a julio?
- Si va A: ¿salida CSV-caja-negra sola (más rápido) o también telemetría a monitor-base (más visual para alumnos)?
- Para B: confirmar si hay 2 adaptadores USB-UART 3,3 V en el labo o comprar.
