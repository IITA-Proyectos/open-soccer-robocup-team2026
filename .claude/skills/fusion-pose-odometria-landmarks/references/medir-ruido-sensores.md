# Protocolo de banco — medir el ruido de los 3 sensores de pose

> Referencia de `fusion-pose-odometria-landmarks`. **No se puede tunear un filtro
> de fusión sin estos 3 números.** La ganancia del complementario, las matrices
> Q/R del EKF y el ruido de predicción del MCL salen TODOS de acá. Este protocolo
> lo ejecuta el equipo humano en banco (regla 1 de CLAUDE.md: Claude no cierra
> TASKs de hardware). Diseñá el setup completo con `hardware-test-protocol`.

## Por qué medir (la regla que casi nadie sigue)

Un filtro decide **cuánto creerle a cada sensor según su ruido relativo**. Si
inventás esos números, el filtro anda en el lab y falla en Incheon (donde el piso,
la luz y los rivales cambian el ruido). Medir una vez, en banco, con método, es la
diferencia entre "fusión de pose" y "promedio mágico que a veces anda".

Cada medición sale como **media (sesgo/bias)** + **desviación estándar (σ, ruido)**.
El bias se resta (calibración); la σ es lo que el filtro tiene que tolerar.

---

## Medición 1 — Deriva del OTOS (la odometría)

**Qué buscás:** cuántos mm/s se va la pose si SOLO integrás OTOS, sin corregir.

**Setup:**
1. Marcá un punto de inicio con cinta. Poné el robot ahí, encendé, reseteá la pose OTOS a (0,0,0).
2. **Prueba estática (bias):** dejá el robot quieto 60 s. Logueá la pose OTOS cada
   100 ms (caja negra / `diag` por USB). Idealmente debería quedar en (0,0,0).
   Lo que se mueva = **deriva en reposo** (ruido del sensor + integración).
3. **Prueba de cierre de lazo (deriva dinámica):** manejá el robot en un cuadrado
   o círculo de ~1 m y **volvé al punto de inicio marcado**. La pose OTOS NO va a
   marcar (0,0,0): el delta = **error de cierre**. Repetí 5 veces.

**Qué sacás:**
- Deriva en reposo: `mm` acumulados en 60 s → `mm/s`.
- Error de cierre: media y σ de los 5 deltas → "el OTOS se va ~X mm por cada
  Y metros recorridos". Esto alimenta `Q` (ruido de proceso) del EKF y el ruido de
  predicción del MCL.

> Ojo OTOS: se alimenta del 3.3 V de la batería (no del USB). Si da `L=-- R=--` o
> `0x64`, es brownout, no firmware — ver el callout de OTOS en `docs/ESTADO-ACTUAL.md`.

---

## Medición 2 — Ruido del ToF / trilateración (el landmark absoluto)

**Qué buscás:** la σ de la pose XY que da la trilateración con el robot QUIETO en
una posición conocida, y cuánto la degrada un obstáculo.

**Setup:**
1. Poné el robot QUIETO en una posición medida con metro (ej. centro: ~(910, 1215)),
   apuntando al arco rival (para que el heading-offset del boot sea 0).
2. Logueá la pose de `localization.cpp` (env `diag_localization_live`) durante 30 s
   sin tocar nada. El robot no se mueve → toda variación es **ruido del ToF**.
3. Repetí en 5 posiciones (centro + 4 zonas). El ruido cambia según qué paredes ve.
4. **Prueba de obstáculo:** con el robot quieto, alguien acerca la mano/un robot a
   30 cm de una pared. Verificá que (a) la pose XY no salta (outlier-rejection §5 de
   `localization.cpp`), (b) `source_flags` muestra un ToF menos.

**Qué sacás:**
- σ de x y de y por posición → alimenta `R` (ruido de medición) del EKF y la
  verosimilitud del MCL. Es el número que dice "cuánto NO creerle al ToF".
- Confirmás el `outlier_threshold_mm` (default 300): si la oclusión hace saltar la
  pose, el umbral está mal o el gate no corre.
- **Bonus:** medí el `TOF_OFFSET_MM` real (plano del sensor → centro del robot, con
  cinta) — hoy es un placeholder de 95 mm sin validar (`pinout_common.h`). Sin ese
  número, la trilateración tiene un sesgo constante.

---

## Medición 3 — Deriva y congelamiento del heading

**Qué buscás:** cuánto deriva el heading por minuto, y si se congela (el problema
conocido del BNO, TASK-207).

**Setup:**
1. Robot quieto, apuntando a una marca fija. Logueá el heading (BNO y, si está, el
   del OTOS) cada 100 ms durante 2-3 min.
2. **Deriva:** el heading debería quedar clavado. Lo que se mueva en 2 min = deriva
   en °/min. Repetí con los motores APAGADOS y con los motores girando (el campo EM
   de los motores afecta al magnetómetro del BNO — dato importante).
3. **Congelamiento:** mientras corre `main_top` real (no un diag aislado), girá el
   robot a mano y mirá si el heading sigue o se queda clavado en un valor (el síntoma
   de la contención I²C BNO+ToF). El detector `imu_freeze.h` existe para esto —
   validá que dispare sin falsos positivos con el robot quieto.

**Qué sacás:**
- Deriva en °/min con motores on/off → decide cada cuánto hay que recalibrar (hoy:
  apagar/encender entre partidos) y si vale el magnetómetro como referencia absoluta.
- Veredicto de congelamiento → es el P1 raíz: si el heading se congela, NO tunees el
  filtro XY todavía (ver `localizacion-rcj-soccer` → P1 heading; arreglo: BNO a bus
  propio, TASK-207, y/o heading del OTOS como respaldo).

---

## Salida del protocolo (lo que entregás para tunear)

Una tablita de 3 filas que va al journal:

| Sensor | Bias (a restar) | σ / deriva (MEDIDA) | Notas (motores on/off, posición, oclusión) |
|---|---|---|---|
| OTOS (odometría) | … mm en 60 s reposo | … mm de cierre por metro | … |
| ToF (trilateración) | offset real … mm | σx=… σy=… mm por posición | salta/no con oclusión |
| Heading (BNO/OTOS) | … ° | … °/min (motores on vs off) | ¿se congela? sí/no |

Con esas 3 filas, el K del complementario sale de la razón σ_odometría/σ_landmark,
las Q/R del EKF salen directo, y el MCL tiene su ruido de predicción. **Recién ahí
se tunea un filtro de verdad.** Antes, es adivinar.
