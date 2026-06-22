---
title: "ToF — bus I2C a 400 kHz detrás de flag (byte-neutro) + plan de pruebas de banco"
date: 2026-06-22
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
tipo: firmware-banco
toca-competencia: SÍ desde 2026-06-22 (400 kHz promovido a default de robot2; ver actualización al pie)
status: compila (default 400 + slowbus 100) · NO VALIDADO EN HARDWARE (lo cierra el equipo)
---

# Bus I2C de los ToF a 400 kHz — parche de banco flag-gated

## Contexto / pedido

Gustavo: ahora que el BNO055 primario se mudó a `Wire2` y en el bus de los ToF quedó
solo un **BNO centinela a 1 Hz**, preparar el cambio del bus a **400 kHz**, documentar
las pruebas de banco, modificar los programas de TOP, y dejar todo en el repo y en
GitHub. El equipo flashea y mide cuando se liberen los robots.

## Qué se hizo

**Diseño seguro (clave):** el bus base **sigue en 100 kHz** (BNO-safe); se sube a
400 kHz **solo durante el `getRangingData()` de cada ToF** (la transferencia grande,
~60 ms a 100 kHz) y se **restaura 100 kHz inmediatamente**. Así el centinela BNO
**siempre** lee a 100 kHz (el bus base no cambia) → **no se tocó `sensors_imu.cpp`**
(cero riesgo de regresión en la IMU). El superloop es de un solo hilo: nunca hay dos
transacciones I2C simultáneas, así que la ventana rápida solo cubre el bloque ToF.

**Cambios (todo detrás de `-DTOP_TOF_FAST_BUS`, default OFF):**
1. `src/top/sensors_tof.cpp`: constante `TOF_FAST_BUS_HZ = 400000` + bracket
   `setClock(400k) … getRangingData … setClock(100k)` en el camino MULTI, bajo `#ifdef`.
2. `platformio.ini`: env nuevo `top_robot2_pri_fastbus` (extiende `top_robot2_pri` +
   el flag).
3. `docs/firmware/TOF-BUS-400KHZ-PLAN-PRUEBAS.md`: el plan de banco T1–T7.

**Byte-neutralidad (flag OFF):** `TOF_FAST_BUS_HZ` se define una vez y se referencia
**solo** dentro de `#ifdef TOP_TOF_FAST_BUS` → con el flag apagado es una constante sin
uso que el compilador elimina; el resto del cambio es código bajo `#ifdef`. El binario
de competencia (`top_robot2_pri`) queda **byte-idéntico**.

## Verificación (host)
- `pio run -e top_robot2_pri -e top_robot2_pri_fastbus` → **2 succeeded** (default con
  flag OFF y variante con flag ON compilan).
- ⚠️ **NO probado en hardware.** El cierre real es el plan T1–T7 (distancias ToF
  correctas a 400 kHz, sin timeouts/NACK con 5 dispositivos + bodge, centinela BNO
  sano, y la mejora de loop rate que justifica el cambio).

## Por qué importa
Cada lectura de ToF a 100 kHz come ~60 ms del loop del TOP (causa conocida del panel
[TOP] cayendo a ~6 Hz con los 4 ToF). A 400 kHz baja a ~16 ms → libera presupuesto del
lazo y baja el lag del WorldSnapshot a la CENTRAL. Es P2 (capitalizable); sube a P1 si
el banco confirma que destraba el atraso de forma notable.

## Qué sigue (equipo, cuando se liberen los robots)
- Flashear `top_robot2_pri_fastbus` a un TOP de robot2 y correr T1–T7 del plan.
- Si pasa: decidir promover el flag a `top_robot2_pri` (re-validar como cambio de
  competencia). Si aparecen timeouts/caídas: descartar y volver a 100 kHz (rollback =
  flashear `top_robot2_pri`, byte-idéntico).
- Antes de usar en robot1: confirmar que su BNO primario también está fuera del bus de ToF.

## Actualización 2026-06-22 — promovido a DEFAULT de competencia (robot2)

Mismo día, Gustavo pidió que el bus **arranque a 400 kHz por default** (no quedar como opt-in de banco).
Hecho:

- `top_robot2_pri` (binario de competencia) ahora trae `-DTOP_TOF_FAST_BUS` → el bus a 400 es el
  default de robot2. **Ya NO es byte-idéntico** al binario previo (el frontmatter de arriba se
  actualizó a `toca-competencia: SÍ`).
- El gate en código pasó de opt-in a **opt-out**: macro `TOP_TOF_FAST_BUS_ACTIVE` = ON si está el flag
  y NO se forzó `-DTOP_TOF_BUS_SLOW`. Así existe un rollback/A-B limpio sin tocar el resto de flags.
- El env `top_robot2_pri_fastbus` se reemplazó por **`top_robot2_pri_slowbus`** (`-DTOP_TOF_BUS_SLOW`):
  la MISMA build de competencia pero a 100 kHz, para rollback de 1 flasheo y para medir el A/B de loop
  rate (T5).
- **Sólo robot2.** Robot1 NO lo trae (su BNO primario comparte el bus de ToF) → sigue a 100 kHz.
- Compila `pio run -e top_robot2_pri -e top_robot2_pri_slowbus` → **2 succeeded**.

⚠️ **Importante:** se adoptó como default por decisión de Gustavo, **NO está validado en hardware**.
El plan T1–T7 sigue siendo el gate para confiar en él en partido; el doc se actualizó en consecuencia.
Flasheo de competencia ahora es `pio run -t upload` (default); rollback `pio run -e top_robot2_pri_slowbus -t upload`.
