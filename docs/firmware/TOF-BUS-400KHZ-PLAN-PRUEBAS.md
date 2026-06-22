# ToF — Bus I2C a 400 kHz (parche de banco) + plan de pruebas

> **Estado:** firmware LISTO y COMPILA (default + variante). **NO VALIDADO EN HARDWARE.**
> El cierre de este doc lo hace el equipo flasheando un TOP y corriendo el plan T1–T7.
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

**Archivos tocados:** solo `src/top/sensors_tof.cpp` (constante `TOF_FAST_BUS_HZ` +
el bracket bajo `#ifdef`) y `platformio.ini` (env nuevo `top_robot2_pri_fastbus`).

**Alcance:** el bracket está en el camino **MULTI** (los 4 ToF, que es el de
competencia). El camino de un solo ToF frontal queda a 100 kHz (sin acelerar, pero
seguro). Aplica a robot2; **antes de usarlo en robot1, confirmar que el BNO primario
de robot1 también esté fuera del bus de ToF** (en `Wire2`).

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
- **escape (rollback):** flashear `top_robot2_pri` (byte-idéntico a competencia).
  Si algo se porta raro, se vuelve en 1 flasheo.

---

## 5. Plan de pruebas en hardware real

**Setup:** TOP de robot2, monitor USB despierto (la app de `tools/monitor-base`).
Tener a mano el binario de escape (`top_robot2_pri`).

**Flasheo del binario a probar:**
```
pio run -e top_robot2_pri_fastbus -t upload
```

| # | Prueba | Cómo | Criterio de PASA |
|---|--------|------|------------------|
| **T1** | Arranca | Flashear fastbus, encender, abrir monitor. | El TOP bootea y el monitor despierta (cámaras/IMU/ToF/WorldSnapshot llegan). |
| **T2** | **Distancias ToF correctas** *(make-or-break)* | Poner pared/mano a distancias conocidas frente a **cada uno** de los 4 ToF. Comparar contra lo que daba a 100 kHz (`top_robot2_pri`). | Las 4 distancias coinciden con la realidad y con el baseline. **Sin** ceros espurios, basura ni saltos. |
| **T3** | Sin caídas de sensor | Mirar la frescura por-sensor / `ever_ok` / si el round-robin saltea alguno, durante ~2–3 min. | Ningún ToF entra en stale ni desaparece del turnero por NACK. |
| **T4** | **Centinela BNO sano** | Ver el heading del 2º BNO (centinela, 1 Hz) y compararlo con el primario (Wire2). | El centinela da heading coherente (no salta, no se congela), dentro de tolerancia del primario. |
| **T5** | **Loop más rápido** *(el objetivo)* | Medir loop rate del TOP / tasa de WorldSnapshot **antes** (`top_robot2_pri`) y **después** (fastbus). | El loop/WorldSnapshot sube de forma medible (se libera el ~60→~16 ms por lectura). Anotar los números. |
| **T6** | Soak quieto | Robot quieto 3–5 min con fastbus. | Sin falso-CONGELADO del freeze-detector, sin caída de ToF, heading estable. |
| **T7** | Con movimiento | Ruedas al aire o jugada corta: seguir paredes / distancias bajo motores andando. | Las distancias siguen trackeando; sin regresión vs 100 kHz. |

**Qué anotar (para decidir si se adopta):**
- T2: distancia real vs leída por cada ToF, a 100 kHz y a 400 kHz.
- T5: loop rate / Hz del WorldSnapshot, antes y después (el número que justifica el cambio).
- Cualquier timeout/NACK/caída observada en T3–T4.

**Decisión:**
- Si T2–T4 **PASAN** y T5 muestra mejora → candidato a adoptar (cómo, en §6).
- Si aparecen timeouts/caídas en T3 o el centinela se ensucia en T4 → **descartar**
  fastbus, volver a 100 kHz, y la próxima palanca de tiempo-real es bajar la frecuencia
  de round-robin o recortar payload del ToF, no subir el bus.

---

## 6. Si pasa: cómo se adopta (NO hacer hasta validar)

Hoy el cambio vive **solo** en el env de banco `top_robot2_pri_fastbus`. Si el equipo
valida T1–T7 en hardware, recién ahí se decide promoverlo al binario de competencia
agregando `-DTOP_TOF_FAST_BUS` a `top_robot2_pri` (eso **cambia** el binario de
partido → se re-valida en banco como cualquier cambio de competencia). Mientras tanto,
competencia sigue byte-idéntica.

**⚠️ Esta TASK la cierra el equipo humano con la placa. Claude no la marca `done`.**
