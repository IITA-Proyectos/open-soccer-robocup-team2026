---
title: "Review del branch origin/Elias + auditoría del scratch de staging (giro/BNO + movimiento)"
date: 2026-06-03
status: final
audiencia: "Elías / Virginia / Enzo / Gustavo — equipo"
author: "Claude Opus 4.8 (Anthropic), vía Claude Code — review + 2 subagentes de análisis"
requested-by: "Gustavo Viollaz (@gviollaz)"
tipo: review
area: firmware
robot: ambos
tags: [review, branch, staging, bno055, giroscopo, cinematica, omni3, reuso, deuda]
---

# Review — branch `origin/Elias` + scratch de `software/staging/`

> **TL;DR.** El branch `origin/Elias` **no aporta código**: su único cambio neto vs `main`
> es un archivo vacío `software/staging/up_board/gyroscopes/gitkeep` (un scaffold que completa
> el patrón `up_board/{communication-module, tof, gyroscopes}`). **No hay bugs en el branch**;
> está 4 commits detrás de main. **Lo importante es hacia dónde apunta**: trabajo de
> giroscopio para la placa TOP. Y ahí ya existe un **stack de producción maduro y host-testeado**
> (`sensors_imu` + `imu_fusion` + 6 envs `diag_bno_*`). El scratch suelto de `staging/shared/`
> (sketches de giro/BNO y de movimiento) **repite bugs ya resueltos** (NDOF en vez de IMUPLUS,
> falta de la receta I²C 100 kHz / 20 Hz, signo de yaw sin invertir, una 3ª cinemática omni-3
> divergente) y **duplica** lógica. **Recomendación (línea del coach): no promover ni ampliar
> ese scratch; mejorar incrementalmente la producción y usar los `diag_*` que ya existen.**

---

## 1. Qué es el branch `origin/Elias` (factual)

- HEAD `c51af56` "Create gitkeep". Merge-base con main = `efa9bec`.
- **Diff neto `main...origin/Elias`: 1 archivo, +1 línea** → `software/staging/up_board/gyroscopes/gitkeep` (vacío).
- Está **4 commits detrás de main** (le faltan los avances M4 / arquero-anticipa / OTOS M2-M3 / docs del 2026-06-03).
- El resto del árbol de `staging/` es **idéntico** a main.

**Veredicto del branch en sí:** inofensivo y coherente con el scaffold existente. No hay nada que
"arreglar" ni bug que corregir **en el branch**. Mergearlo a main sería traer solo ese gitkeep
(se puede, pero primero conviene que el branch haga `merge origin/main` para no quedar atrás).

> El valor de esta review NO está en el branch (vacío), sino en **auditar el material de
> giro/movimiento que ese scaffold organiza**, que es lo que el equipo va a tocar. Eso es lo
> que sigue.

---

## 2. Coherencia con el repo y el robot

| Tema | Estado | Detalle |
|---|---|---|
| Naming `up_board` | ⚠️ menor | El repo llama a esa placa **TOP / "ARRIBA"** en todo el firmware y docs (`src/top/`, `FIRMWARE-PLACA-ARRIBA.md`). `up_board` es una variante inglesa que convive en `staging/`. No rompe nada, pero conviene unificar a TOP para no fragmentar vocabulario. |
| `software/staging/` | ✅ por diseño | El `README.md` lo declara scratch "pendiente de probar en el robot físico", con flujo de promoción. Es legítimo que NO sea producción y que esté desactualizado. |
| Sketches apuntan a firmware legacy | ⚠️ alto | Los `cambios-*.md` apuntan a `software/robot-delantero/definitivo-*` y `robot-arquero/*` (código **2025**, Arduino mono-placa). El robot 2026 corre el stack de **3 placas** en `software/teensy/Soccer 2026/`. Las recetas del scratch describen **1 BNO** y un robot mono-placa → no trasladan a la arquitectura actual sin reescritura. |
| Robot real vs scratch | ⚠️ alto | El BNO hoy vive en la **TOP** (no en el Teensy que mueve motores); el heading viaja por snapshot/OTOS. Varios sketches asumen **BNO local al Teensy de motores** → su conducta (autocal por heading local) **no es portable** a las 3 placas. |

---

## 3. Bugs encontrados (consolidado, priorizado)

> Todos en `software/staging/shared/` (scratch). Ninguno es de producción. Citas `file:line`.

### CRÍTICO
1. **Modo BNO = NDOF en vez de IMUPLUS** (usa magnetómetro → el campo de los motores DC
   contamina el yaw). `test-gyro-movimiento-basico/...ino:110`, `test-gyro-movimiento-lateral...ino:154`,
   `test-circulo.ino:216`, `test-4-movimientos.ino:251`. **Producción usa IMUPLUS a propósito**
   (`src/top/sensors_imu.h:13`, `sensors_imu.cpp:145`).
2. **Falta la receta de banco I²C 100 kHz + lectura BNO a ~20 Hz** en TODOS los sketches
   (sin `Wire.setClock(100000)`, heading leído por iteración con `delay()`). Es exactamente el
   patrón que **congela el yaw** cuando el BNO convive con los ToF en el bus (banco 2026-06-02).
   Producción: `sensors_imu.cpp:167` (clock) + `BNO_READ_INTERVAL_MS=50` (20 Hz, `:229-235`).
3. **3ª cinemática omni-3 divergente** en `test-movimiento-omnidireccional.ino:150-153`: ángulos
   de rueda **30/150/270** que no coinciden con `WHEEL_ANGLES_DEG={60,-60,180}` de producción
   (`src/central/config_central.h:72`, marcado **TENTATIVO**) ni con el control-directo validado en
   banco. Mismo riesgo de **"círculos"** que documentó María (`journal/2026-06-01-...:29-41`).

### ALTO
4. **Signo de yaw sin invertir** (PID corrige al lado equivocado salvo coincidencia de cableado):
   básico `:192`, lateral `:223`, imuplus `:123`, círculo `:298`. Producción fija `HEADING_SIGN=-1`
   (`sensors_imu.cpp:62`, ver `journal/2026-05-31-top-bno-verificacion-y-fix-signo.md`).
5. **`constrain(velBase±corr, 0, 255)` mata la rueda en vez de invertir** el sentido cuando la
   corrección supera la base — `test-4-movimientos.ino:442-443, 468-469`. Es el **bug v2 que el
   propio lateral dice haber arreglado** y reaparece acá. Producción lo resuelve con saturación
   proporcional (`src/shared/kinematics.cpp::saturate_wheels`).
6. **Convención de M2 invertido duplicada en 3 sitios** (`moverAdelante`/`moverAtras`/`motor2`,
   `test-4-movimientos.ino:435-482`) → se desincronizan fácil.
7. **`cambios-bno055-init.md:82-84` apunta a firmware obsoleto** (`definitivo-delantero/arquero`)
   y describe init de **1 solo BNO** → desalineado con el stack TOP dual actual (aunque su receta
   IMUPLUS/no-bloqueo/promedio-10 ya está **implementada** en `sensors_imu.cpp:143-157`).
8. **Resync de protocolo roto** en `cambios-uart-sincronizacion.md:125-167`: el `break` final, tras
   consumir 9 bytes de un falso-201, no reintenta → rompe el alineamiento; el comentario `:165`
   ("el while descartará los bytes") es **falso**.

### MEDIO
9. Lógica de init `bnoOK`/timeout **indefinida** — puede declarar el BNO OK con el sensor ausente
   (`test-movimiento-omnidireccional.ino:224-232`).
10. **Underflow unsigned** `TIEMPO_MOVIMIENTO - anticipacionReal` si la anticipación supera el tramo
    (`test-motores-lateral-simple.ino:623`, `test-4-movimientos.ino`). Defaults seguros, pero sin guard.
11. **Autocal/heading dependen del BNO local del Teensy** — ausente en la arquitectura 3 placas
    (sketches #1/#2/#3). Conducta no portable.
12. **`delay()` bloqueantes** (lateral `:449,570`; 4-mov `:594,609`) → incompatibles con el watchdog
    de comando de 200 ms de producción (`config_central.h:97`).
13. **Botón `INPUT` activo-HIGH sin pull** vs `INPUT_PULLUP` activo-LOW de producción
    (`diag_central_line_sweep.cpp:294`) → no portable + lecturas espurias.
14. **Clamp de coords 1–200 en OpenMV** (`cambios-uart-sincronizacion.md:189-200`) satura arcos
    lejanos → ángulos `atan2` sesgados. Es un workaround de un protocolo sin framing.
15. **Modo BNO + ganancias PID inconsistentes** entre sketches y entre código y README (NDOF vs
    IMUPLUS; `Kp 3/4`, `Ki 0.05/0.08/0.1`, etc.). Sin fuente única.

### BAJO
16. **Duplicados/ruido:** `test-gyro-movimiento-basico.ino` suelto es solo una nota de redirección
    (borrar); 5 funciones (`inicializarGyro`/`leerHeading`/`calcularCorreccionPID`/botón) están
    **copiadas casi idénticas** en 3 sketches → 3 lugares para el mismo bug.
17. `String` de Arduino en loop (`test-4-movimientos.ino:780+`), `dataLog[600]` ~30 KB RAM
    (`test-gyro-movimiento-lateral...:95`), comentarios que mienten ("cinemática omnidireccional
    correcta" cuando es control directo, `test-motores-lateral-simple.ino:6`).

---

## 4. Lo que SÍ está bien / vale rescatar

- **`test-motores-lateral-simple.ino`** es el sketch **alineado con el robot real**: control directo
  con **M2 invertido** (`motor2()` con INA/INB swapeados) + PID de heading repartido + freno
  anticipado de M3 + autocal. Esa lógica **ya fue portada a producción** en
  `src/diag/diag_central_line_sweep.cpp:50-58,141-218` (el arquero que anduvo en banco 2026-06-01).
  → La compensación de freno M3 + autocal es lo único que `line_sweep` aún no tiene; si se quiere,
  **se agrega a `line_sweep`**, no se mantiene el `.ino`.
- **`test-bno055-imuplus.ino`** es conceptualmente el más correcto del lote (IMUPLUS, sin motores),
  pero como **test** ya está cubierto por el env `diag_bno_left`.
- **Mapeo de pines de motores: SANO.** Los 3 sketches de movimiento usan el mismo pinout que
  `config_central.h` (ROBOT2: M1=8/7/6, M2=11/12/4, M3=2/5/3; ROBOT1 espejado). Sin discrepancia.
- **`evaluar-bohlebots-bno055.md`**: la decisión ya está tomada y **superada** — producción se quedó
  en Adafruit+IMUPLUS y **ya reimplementó** lo que se elogiaba de BohleBots: heading relativo
  (`sensors_imu.cpp:133-141`), calib en EEPROM (`:81-98`), anti-impacto/drift (`imu_fusion.h:7-18`).

---

## 5. Mapa de reúso — software ya desarrollado que debería usarse

> **Idea central: no subir sketches nuevos; colgar todo del stack tested que ya existe.**

| Necesidad (lo que el scratch intenta) | Módulo / diag de PRODUCCIÓN a usar | Tests |
|---|---|---|
| Init + lectura del BNO (IMUPLUS, 100 kHz, 20 Hz, offset, degradación) | `src/top/sensors_imu.{cpp,h}` (`sensors_imu_init`, `sensors_imu_tick`, `sensors_imu_get_heading_deg`) | — (Arduino) |
| Fusión de heading (dual-BNO, promedio circular, glitch/resync) | `src/shared/imu_fusion.{cpp,h}` | `test_imu_fusion` (17) |
| Probar 1 BNO standalone | env **`diag_bno_left`** | — |
| Probar BNO **conviviendo con ToF** (lo que importa de verdad) | envs **`diag_bno_tof` / `diag_bno_tof_slow`** | — |
| Probar 2 BNO + fusión / direcciones | **`diag_bno_dual_live`**, **`diag_bno_addr_check`** | — |
| Cinemática omni-3 (IK) | `src/shared/kinematics.{cpp,h}` + aplicación `src/central/motors_zircon.cpp` | `test_kinematics` (11) |
| PID de heading / lateral / approach | `src/shared/pids.{cpp,h}` (`HeadingPID`, `LateralPID`, `approach_velocity`) | `test_pids` (18) |
| Movimiento recto (adelante/atrás) en banco | envs **`diag_central_drive_robot1/2`** | — |
| Strafe lateral del arquero (M2 invertido, validado banco) | **`src/diag/diag_central_line_sweep.cpp`** + envs `diag_central_strafe_robot1/2` | — |
| Protocolo inter-placa robusto (framing + CRC) | `src/shared/proto.{cpp,h}` + `src/shared/crc16.{cpp,h}` | `test_proto` (13) |
| Parser OpenMV→Teensy (cámaras) | `src/top/cameras.cpp` + env `diag_top_cameras` | `test_cameras_fusion` (16) |
| Velocidad / predicción de pelota (recién agregado) | `src/shared/ball_velocity` + `src/shared/ball_predict` | `test_ball_velocity` (16), `test_ball_predict` (9) |

---

## 6. Oportunidades de mejora (incrementales, sobre lo que ya hay)

1. **Reconciliar la contradicción de M2** ANTES de promover cualquier movimiento lateral:
   `DIAG-CENTRAL-MOTORS.md` (2026-05-29) dice `MOTOR_DIR={+1,+1,+1}` (sin inversión), pero el banco
   2026-06-01 + `line_sweep` asumen **M2 invertido por HW**, y `motors_zircon.cpp` **no invierte
   ninguno**. Es la decisión de hardware que destraba toda la cinemática. → cerrar en banco (TASK-036/101).
2. **Calibrar `WHEEL_ANGLES_DEG`** con Enzo (hoy `{60,-60,180}` TENTATIVO) — eso es lo que falta para
   que la IK de producción ande, **no** escribir una 3ª cinemática.
3. **Folding selectivo:** llevar el **freno anticipado de M3 + autocal** del lateral-simple a
   `diag_central_line_sweep` si se valida útil (única pieza no portada aún).
4. **Adoptar framing+CRC** (`proto.h`/`crc16.h`) en cualquier enlace nuevo en vez del workaround de
   clamp 1–200 del legacy.
5. **Limpieza de staging:** borrar la nota-redirección suelta y deduplicar las 5 funciones repetidas;
   marcar `cambios-*.md` y `evaluar-bohlebots-*.md` como **históricos** (su target legacy ya no es la
   fuente de verdad y la mayoría ya está implementada en producción).
6. **Unificar vocabulario** `up_board` → `TOP` en staging.

---

## 7. Recomendación final (línea del coach)

- **No promover** los sketches de giro/movimiento de `staging/shared/` tal cual: arrastran NDOF,
  falta de receta I²C/20 Hz, signo de yaw y una cinemática divergente — todos **ya resueltos** en
  producción.
- **No crear** código nuevo de giroscopio bajo `up_board/gyroscopes/`: las pruebas que el equipo
  quiera ya existen como **envs `diag_bno_*`**, y cualquier maniobra debe colgar de
  `sensors_imu_get_heading_deg()` + `kinematics`/`pids`.
- **Sí** cerrar en banco los 2 desbloqueantes reales (polaridad de M2 + `WHEEL_ANGLES_DEG`), que es
  lo único que impide que el movimiento de producción ande — eso es la mejora incremental de mayor
  impacto.
- **Branch `origin/Elias`:** inofensivo. Si se quiere en main, que primero haga `merge origin/main`
  (está 4 atrás) y luego se mergea el gitkeep — o se descarta, ya que el scaffold no aporta sin el
  trabajo de arriba.

## Archivos de referencia
- Producción IMU: `src/top/sensors_imu.{cpp,h}`, `src/shared/imu_fusion.{cpp,h}`.
- Producción movimiento: `src/shared/kinematics.{cpp,h}`, `src/shared/pids.{cpp,h}`, `src/central/motors_zircon.{cpp,h}`.
- Diags de banco: envs `diag_bno_*`, `diag_central_*` en `platformio.ini`.
- Banco/contexto: `journal/2026-06-01-arquero-seguidor-linea-y-calibracion.md`, `journal/2026-06-03-verificacion-banco-mitad-inferior-y-review-gk.md`, `journal/2026-05-31-top-bno-verificacion-y-fix-signo.md`.
- Scratch auditado: `software/staging/shared/` (ver secciones 3–4).
