---
name: fusion-pose-odometria-landmarks
description: Use when BUILDING, wiring or tuning the estimator that fuses the IITA robot's position sensors into one pose — odometry (OTOS) + absolute landmarks (ToF trilateration, goal bearings) + heading (BNO). Triggers - "fusionar sensores de posición", "combinar ToF y OTOS", "el OTOS deriva / la odometría se va", "la pose salta / da saltos", "filtro complementario", "Kalman / EKF / filtro de partículas", "cablear pose_fusion", "el heading deriva o se congela", "tunear la fusión de pose", "qué le creo, al ToF o a la odometría". Covers the predict-from-odometry / correct-from-landmarks pattern, the pose_fusion/pose_filter modules that already exist (and aren't wired), how to MEASURE sensor noise before tuning, gating freshness/outliers, and the bench test plan. NOT for CHOOSING which technique (use localizacion-rcj-soccer), NOT for the omni motion model itself (dinamica-omni-3-ruedas), NOT for camera landmark detection (openmv-vision-tuning).
---

# Fusión de pose — odometría + landmarks + heading (cómo construir el estimador)

## Principio central

**Predecí con lo rápido-pero-derivante, corregí con lo lento-pero-absoluto.** Ese
es TODO el secreto de la fusión de pose, y no cambia entre un filtro complementario
de 20 líneas y un EKF:

```
cada ciclo:
  1. PREDICCIÓN:  pose += Δodometría (OTOS)        ← suave, 100 Hz, pero DERIVA
  2. CORRECCIÓN:  si llegó un landmark válido y consistente:
                    pose += K · (landmark − pose)   ← lo jala hacia la verdad absoluta
```

`K` (la ganancia) es **cuánto le creés a la corrección**. Mucho K → sigue al
landmark ruidoso y tiembla. Poco K → suave pero tarda en sacarse la deriva. El
valor correcto de K depende del **ruido relativo MEDIDO** de odometría vs landmark
(por eso medir el ruido es obligatorio — `references/medir-ruido-sensores.md`).

> **REQUIRED:** la elección de filtro (complementario vs EKF vs partículas) y por
> qué para este robot está en `localizacion-rcj-soccer`. Esta skill asume que ya
> decidiste **fusionar** y querés CONSTRUIR/tunear.

## Las 3 señales de ESTE robot (sus defectos, que el filtro tiene que compensar)

| Señal | Módulo | Rol en la fusión | Su defecto (lo que el filtro corrige/evita) |
|---|---|---|---|
| **OTOS** (×2, óptico al piso, DOWN) | `otos_position`, `otos_fusion` | **predicción** (Δx, Δy, Δθ @100 Hz) | deriva acumulativa; patina poco (óptico) pero se va con el tiempo |
| **ToF → paredes** (trilateración) | `localization.{h,cpp}` | **corrección absoluta** XY | intermitente (`valid=false` si <1 ToF/eje); un rival tapando una pared mete un outlier |
| **Heading** (BNO055 + IMU del OTOS) | `sensors_imu`, `imu_fusion`, `otos_fusion` | **rota el mapa** de la trilateración | BNO **se congela** por I²C con los ToF (TASK-207); deriva lenta del giroscopio |

**El heading es la raíz.** Si el heading está mal, la trilateración rota TODO el
mapa: la pose sale *coherente pero equivocada*, no ruidosa. Estabilizá el heading
ANTES de pelearte con el filtro XY (ver `localizacion-rcj-soccer` → P1 heading).

## Lo que YA existe en el repo (no reescribir — cablear y tunear)

> Verificado 2026-06-14. Estos módulos son PUROS, host-testeados, y **NO están
> enchufados al firmware vivo**. Cablearlos detrás de un flag default-OFF es el
> trabajo, no escribirlos de cero.

- **`src/shared/pose_fusion.{h,cpp}`** — **filtro complementario ToF+OTOS**, que es
  literalmente el patrón de arriba: predice con el Δ del OTOS, corrige hacia el ToF
  cuando es válido y consistente, ganancia `K` en punto fijo Q8 (~0.10). **NO fusiona
  heading** (lo pasa de largo). **NO cableado.** ← el candidato #1 a enchufar.
- **`src/shared/pose_filter.{h,cpp}`** — suavizado temporal: EMA o mediana + **gate
  de salto** (rechaza saltos > ~400 mm = teletransporte por outlier) + hold-last
  cuando la pose es inválida. Es la "red de seguridad" que va DESPUÉS de cualquier
  fuente de pose. **NO cableado.**
- **`src/shared/otos_fusion.h`** — combina los 2 OTOS: heading por **promedio
  circular** (atan2 de senos/cosenos sumados, cubre ±180°), slip por diferencia de
  velocidad menos la rotación esperada. **NO cableado.**
- **`src/shared/imu_fusion.{h,cpp}` / `imu_freeze.{h,cpp}`** — fusión de 2 BNO +
  detector de congelamiento (gateado OFF, pendiente de validar en banco que no dé
  falsos positivos con el robot quieto).
- **`src/shared/otos_health.h`** — máquina de estados de salud (presente/vivo/conf).

La trilateración (`localization.cpp`) SÍ corre y llega al WorldSnapshot
(`my_x/y_mm`, conf 70/0). La capa que falta es **fusionarla con el OTOS** para
tapar los huecos donde `valid=false`.

## El método para cablear/tunear (en orden — el orden ES la disciplina)

1. **Medí el ruido de las 3 señales en banco.** Sin esto, K se tunea a ojo y el
   filtro no es reproducible. Protocolo medible: `references/medir-ruido-sensores.md`.
   Salís con 3 números: deriva del OTOS (mm/s), σ del ToF (mm), deriva del heading (°/min).
2. **Estabilizá el heading primero** (no es esta skill — es P1 de `localizacion-rcj-soccer`).
   Un filtro XY sobre un mapa que rota no se puede tunear.
3. **Cableá `pose_fusion` detrás de un flag default-OFF** (patrón del repo: igual
   que `CENTRAL_MOTOR_KICKSTART`). El binario de competencia queda byte-idéntico
   hasta que el flag se prende. Verificá el gate host + que los envs de producción
   compilen sin cambiar conducta.
4. **Poné el gate de freshness/outlier ANTES de corregir:** no corrijas con un ToF
   `valid=false`, ni con uno que saltó > umbral (eso es el rival tapando la pared,
   no tu movimiento). `pose_fusion` ya tiene el gate de salto; `localization.cpp §5`
   ya tiene el outlier-rejection por inconsistencia entre ToF del mismo eje.
5. **Titulá K en banco** (ver `control-pid-zona-muerta` para la disciplina de
   titración): arrancá con K chico (suave, confía en odometría), subilo hasta que
   la pose siga a la verdad sin temblar. Elegí el K más chico que saque la deriva
   en el tiempo de un punto muerto típico.
6. **Validá con ground-truth real** (metro/cinta en posiciones marcadas) — la regla
   1 de CLAUDE.md: solo el equipo cierra una TASK de hardware. Plan: `hardware-test-protocol`.

Comparación de los 3 filtros + recetas mínimas (complementario, EKF 3-estados, MCL):
**`references/complementario-ekf-particulas.md`**.

## Plan de prueba en hardware (obligatorio — sin esto queda en backlog)

- **Banco estático:** robot en 5 posiciones marcadas con cinta (centro + 4) →
  comparar pose fusionada vs metro. Criterio: ±3 cm con todos los ToF visibles.
- **Banco con oclusión:** alguien tapa una pared con la mano a 30 cm → la pose XY
  **no debe saltar** (el outlier-rejection + gate de salto lo absorben); `source_flags`
  debe mostrar que se usaron menos ToF.
- **Banco de deriva:** mover el robot en círculo 30 s y volver al punto de inicio →
  con SOLO OTOS, medir el error de cierre (la deriva); con fusión ON, el error debe
  bajar marcadamente. Esto demuestra que la corrección funciona.
- **Regresión:** con el flag OFF, la conducta de competencia es idéntica (gate host
  + un env de producción que no cambió de tamaño).

## Errores comunes

- **Tunear sin medir el ruido** → K mágico que anda en el lab y falla en Incheon.
- **Fusionar con el heading congelado** → la pose es coherente pero rota; parece un
  bug del filtro y es el sensor. Arreglá el heading primero.
- **Corregir con un landmark inválido o saltado** (rival tapando la pared) → la pose
  da un salto y la FSM toma una decisión basada en ficción. Gate de freshness/outlier
  SIEMPRE antes de la corrección.
- **Doble conteo del heading:** si el OTOS ya trae heading y el BNO también, fusionar
  los dos sin cuidado los cuenta dos veces. Elegí uno como primario (ver `otos_fusion`/`imu_fusion`).
- **Reescribir `pose_fusion`/`pose_filter` de cero** sin notar que ya existen y están
  testeados. Cablealos.
- **K demasiado alto** porque "queremos que reaccione rápido" → la pose tiembla con
  el ruido del ToF y el control aguas abajo (que cuantiza, ver `control-pid-zona-muerta`)
  lo amplifica.

## Skills relacionadas

- **Qué técnica elegir y por qué (entrada):** `localizacion-rcj-soccer`.
- **Modelo de movimiento que alimenta la predicción:** `dinamica-omni-3-ruedas`.
- **Disciplina de titración de la ganancia + actuador cuantizado:** `control-pid-zona-muerta`.
- **Ver los landmarks (arcos por cámara):** `openmv-vision-tuning`.
- **Diseñar/ejecutar el test de banco:** `hardware-test-protocol`.
- **Frame de ejes canónico:** `docs/CONVENCION-EJES-ROBOT.md`.
