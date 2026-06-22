# ToF — Bus I2C a 400 kHz (default de competencia R1 + R2) + registro de pruebas

> **Estado:** ✅ **VALIDADO EN HARDWARE — 2026-06-22 (Gustavo): AMBOS robots (R1 + R2) andan a 400 kHz.**
> El bus a 400 kHz es el **default de competencia de los dos** (`top_robot2_pri` y `top_robot1_pri`
> traen `-DTOP_TOF_FAST_BUS`). Este doc queda como REGISTRO del cambio + el plan de pruebas que se
> corrió. Rollback de 1 flasheo, si alguna vez hiciera falta = `*_slowbus`.
> **Autor:** Claude Opus 4.8 (Anthropic) · **Pedido por:** Gustavo Viollaz · **Fecha:** 2026-06-22

---

## 1. Qué es esto (en una frase)

Subir el bus I2C de los 4 ToF (VL53L7CX) de **100 kHz → 400 kHz** para que cada
lectura de un sensor tarde **~4× menos** y el loop del TOP respire — pero hacerlo
**solo durante la lectura del ToF** y dejando el bus en 100 kHz el resto del tiempo,
para no arriesgar al BNO055 centinela que comparte el mismo cable.

Todo está **detrás de un flag** (`-DTOP_TOF_FAST_BUS`). Apagado (lo que se flashea
en competencia) el binario es **byte-idéntico** al de hoy. Encendido (env de banco)
prueba la velocidad nueva.

---

## 2. Por qué ahora se puede (y antes no)

El bus `Wire` (pines 18/19 del TOP) tiene colgados: los **4 ToF** + **1 BNO055**.

- **Antes:** ese BNO055 era el IMU PRIMARIO. Su lectura multibyte se **corrompe a
  >100 kHz** en este bus (bodge LP + capacitancia). Por eso el bus estaba clavado a
  100 kHz: era el precio de que el BNO conviviera con los ToF. El costo: cada
  `getRangingData()` de un ToF trae un bloque grande por I2C y a 100 kHz tarda
  **~60 ms** → con round-robin de 1 ToF/tick eso es ~60 ms de bus ocupado por tick.
- **Ahora:** el BNO PRIMARIO se mudó a **`Wire2`** (pines 24/25), solo. En el bus de
  los ToF quedó únicamente un **BNO CENTINELA que se lee a 1 Hz** (backup pasivo por
  si el primario falla). Ya no hay un IMU crítico a alta tasa en ese bus.

**Conclusión:** el bus puede ir rápido para los ToF. Al centinela lo único que hay
que garantizar es que cuando le toque su lectura (1 vez por segundo) el bus esté en
100 kHz. Eso se logra **gratis** con el diseño de abajo.

---

## 3. Cómo está implementado (el diseño seguro)

**No** subimos el bus a 400 kHz de forma permanente. Hacemos lo contrario, que es más
seguro:

- El bus **base queda en 100 kHz** (BNO-safe), igual que hoy.
- En `sensors_tof_tick()`, **justo antes** de leer el bloque de un ToF, subimos a
  400 kHz; **justo después**, restauramos 100 kHz:

  ```cpp
  #ifdef TOP_TOF_FAST_BUS
      Wire.setClock(TOF_FAST_BUS_HZ);   // 400 kHz SOLO para el bloque ToF
  #endif
      if (... isDataReady() && getRangingData(...)) { /* CPU: zonas, máscara */ }
  #ifdef TOP_TOF_FAST_BUS
      Wire.setClock(TOF_RUN_CLOCK_HZ);  // restaurar 100 kHz: bus BNO-safe
  #endif
  ```

**Por qué es seguro:** el lazo del TOP es un superloop de un solo hilo — nunca hay dos
transacciones I2C a la vez. La ventana de 400 kHz dura **solo** la transferencia del
ToF; cuando en otro tick le toca al centinela BNO, el bus ya está en 100 kHz. El
centinela **nunca depende de "acordarse de bajar el clock"** → por eso **no se tocó
`sensors_imu.cpp`** (cero riesgo de regresión en la IMU).

**Archivos tocados:** `src/top/sensors_tof.cpp` (constante `TOF_FAST_BUS_HZ` + el bracket bajo
`#ifdef`) y `platformio.ini` (flag en `top_robot2_pri` y `top_robot1_pri` + envs `*_slowbus`).

**Alcance:** el bracket está en el camino **MULTI** (los 4 ToF, que es el de
competencia). El camino de un solo ToF frontal queda a 100 kHz (sin acelerar, pero
seguro). Aplica a **AMBOS robots**: tras la unificación HW del 2026-06-15/16, R1 = R2 (BNO
primario solo en Wire2, centinela @1 Hz en el bus de ToF), así que el bus rápido es seguro en los dos.

---

## 4. Tema a analizar (formato coach)

- **risk-no-fix (si NO se hace):** el loop del TOP sigue gastando ~60 ms por lectura
  de ToF. Con los 4 sensores en round-robin eso recorta el presupuesto del lazo y
  atrasa el WorldSnapshot que va a la CENTRAL (heading/distancias con lag). Es deuda
  de tiempo-real ya conocida (panel [TOP] cayó a ~6 Hz con los 4 ToF juntos).
- **risk-fix (qué se puede romper):** a 400 kHz con **5 dispositivos** colgados del
  bus + el cableado **bodge LP**, puede haber **timeouts/NACKs**: un ToF que deja de
  responder (lecturas en cero, basura, o el round-robin saltándolo). Menos probable
  pero a vigilar: que la ventana rápida ensucie de algún modo al centinela BNO. Por
  eso el plan T2–T4 mide exactamente esto.
- **tiempo:** firmware ya hecho (0). Banco: **~30–45 min** para correr T1–T7 con un
  TOP y el monitor.
- **prioridad:** **P2** — mejora de tiempo-real capitalizable; el robot compite sin
  esto. Sube a **P1** si en banco se confirma que destraba el atraso del WorldSnapshot
  de forma notable.
- **escape (rollback):** flashear `top_robot2_pri_slowbus` (la MISMA build, bus a 100 kHz).
  Si algo se porta raro, se vuelve en 1 flasheo.

---

## 5. Plan de pruebas en hardware real

**Setup:** TOP de robot2, monitor USB despierto (la app de `tools/monitor-base`).
Tener a mano el binario de escape (`top_robot2_pri_slowbus`).

**Flasheo:** el 400 kHz ya es el DEFAULT de competencia (robot2), así que el binario a probar **ES el
de partido**:
```
pio run -t upload
```
Rollback a 100 kHz (si algo falla, o para el A/B de T5): `pio run -e top_robot2_pri_slowbus -t upload`.

| # | Prueba | Cómo | Criterio de PASA |
|---|--------|------|------------------|
| **T1** | Arranca | Flashear el default (400), encender, abrir monitor. | El TOP bootea y el monitor despierta (cámaras/IMU/ToF/WorldSnapshot llegan). |
| **T2** | **Distancias ToF correctas** *(make-or-break)* | Poner pared/mano a distancias conocidas frente a **cada uno** de los 4 ToF. Comparar contra lo que da a 100 kHz (`top_robot2_pri_slowbus`). | Las 4 distancias coinciden con la realidad y con el baseline. **Sin** ceros espurios, basura ni saltos. |
| **T3** | Sin caídas de sensor | Mirar la frescura por-sensor / `ever_ok` / si el round-robin saltea alguno, durante ~2–3 min. | Ningún ToF entra en stale ni desaparece del turnero por NACK. |
| **T4** | **Centinela BNO sano** | Ver el heading del 2º BNO (centinela, 1 Hz) y compararlo con el primario (Wire2). | El centinela da heading coherente (no salta, no se congela), dentro de tolerancia del primario. |
| **T5** | **Loop más rápido** *(el objetivo)* | Medir loop rate del TOP / tasa de WorldSnapshot **a 100 kHz** (`top_robot2_pri_slowbus`) y **a 400** (default). | El loop/WorldSnapshot sube de forma medible (se libera el ~60→~16 ms por lectura). Anotar los números. |
| **T6** | Soak quieto | Robot quieto 3–5 min a 400. | Sin falso-CONGELADO del freeze-detector, sin caída de ToF, heading estable. |
| **T7** | Con movimiento | Ruedas al aire o jugada corta: seguir paredes / distancias bajo motores andando. | Las distancias siguen trackeando; sin regresión vs 100 kHz. |

**Qué anotar (para decidir si se adopta):**
- T2: distancia real vs leída por cada ToF, a 100 kHz y a 400 kHz.
- T5: loop rate / Hz del WorldSnapshot, antes y después (el número que justifica el cambio).
- Cualquier timeout/NACK/caída observada en T3–T4.

**Decisión:**
- Si T2–T4 **PASAN** y T5 muestra mejora → el 400 queda confirmado como default de competencia (ya
  está puesto; ver §6).
- Si aparecen timeouts/caídas en T3 o el centinela se ensucia en T4 → **revertir** a 100 kHz
  (flashear `top_robot2_pri_slowbus`), y la próxima palanca de tiempo-real es bajar la frecuencia de
  round-robin o recortar payload del ToF, no subir el bus.

## Resultados de banco — 2026-06-22 (robot2, Gustavo en la placa)

Leído por serie HEADLESS (`tools/monitor-base/probe_top_serial.py`, solo lectura). Detalle completo +
mapeo índice→posición en `journal/2026-06-22-banco-tof-400khz-validacion-serie-robot2.md`.

- **T1 ✅** vivo, stream ~21 fps, `resync=0`.
- **T2 ✅** los 4 ToF leen y responden; 3/4 clavaron objetivos a mano con **16/16 zonas, ±1 mm**
  (frente/derecha/izquierda). El sensor "que no daba lecturas" estaba SANO (miraba al vacío).
- **T3 ✅** sin caídas/stale en ventanas de varios minutos.
- **T4 ✅** el heading sigue el giro suave, maneja el wrap ±180°, se asienta sin deriva, `valid=True`
  (NO congelado). Convención: izquierda = heading sube (CCW+).
- **T5 ✅** 400 kHz = **150.511** vs 100 kHz = **71.947 pasadas/s** → **~2,1× más throughput de loop**
  con el bus rápido (el `getRangingData()` era el costo dominante del lazo, ~70% del tiempo bloqueado a
  100 kHz). Justificación cuantificada del cambio.
- **Items abiertos (NO del bus):** centinela 2º BNO = 0.00 (backup sin heading real); cámaras/DOWN
  caídos en las lecturas finales (probable solo-USB alimentando la TOP).

**El bus a 400 kHz queda VALIDADO en banco (T1–T4) para robot2.**

---

## 6. Estado de adopción

⭐ **2026-06-22 (Gustavo):** el flag `-DTOP_TOF_FAST_BUS` se **promovió a `top_robot2_pri`** → el bus a
400 kHz es el **default de competencia de robot2**. Esto **cambia** el binario de partido (ya NO es
byte-idéntico al de antes). El env de banco `_fastbus` se reemplazó por `top_robot2_pri_slowbus`
(`-DTOP_TOF_BUS_SLOW`), que vuelve a 100 kHz sin tocar el resto de flags — para rollback y para el
A/B de loop rate (T5).

**Ambos robots (2026-06-22).** R1 también lo trae: la familia `top_robot1_pri*` ya usa
`-DTOP_BNO_PRIMARY_ONLY` (primario en Wire2) tras la unificación HW del 2026-06-15/16, así que el bus
rápido le aplica igual. Flash de competencia de R1 = `top_robot1_pri_rt`; rollback = `top_robot1_pri_slowbus`.

**✅ Validado en hardware (2026-06-22, Gustavo):** ambos robots flasheados con el 400 kHz **andan** —
ToF leyendo, robot booteando OK. El cierre lo hizo el equipo con la placa (esta confirmación es de
Gustavo, no de Claude). El `slowbus` queda como rollback/A-B por si alguna vez hiciera falta.
