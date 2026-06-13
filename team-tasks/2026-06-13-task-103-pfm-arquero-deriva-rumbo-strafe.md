---
task: 103
titulo: "Arquero strafe v6: el frente deriva ±37° durante el barrido (PFM débil) → choca la pared"
fecha: 2026-06-13
asignado: equipo (firmware CENTRAL — Gustavo + alumno)
prioridad: P1
pedido-por: Gustavo Viollaz (banco en casa, cancha, 2026-06-13)
relacionada: control-pid-zona-muerta (skill), pfm_heading.h, central_arquero_fsm (memoria)
estado: pending
---

# TASK-103 — Deriva de rumbo del arquero strafe (PFM no mantiene el frente)

> **Hallazgo de banco (cancha en casa, 2026-06-13), con dato duro de caja negra.**
> Env `central_robot2_arquero_strafe_bb` (build Jun 12 2026, R2, GK, invert 1/1/1,
> pisos 70/70/107). La FSM v6 funciona (GO → `GK_SIMPLE_MOVE`/`GK_SIMPLE_ESCAPE`,
> el escape de línea dispara y revierte la dirección), PERO el frente al arco NO se
> mantiene.

## El dato (caja negra de la corrida 2)

- `hdg_deg` arrancó en **0°** y derivó hasta **−36°** en una pasada; al revertir,
  derivó hasta **+38°** en la otra. **Swing ~±37°**, muy por encima del criterio
  **±10°**.
- El PFM **sí corrige y con el signo correcto** (`cmd_w_dps` pulsa **+100** cuando
  `hdg` es negativo), pero la corrección es **demasiado débil**: el strafe induce
  más rotación de la que el PFM alcanza a frenar entre escape y escape.
- **Por qué choca la pared (causa raíz):** al derivar el frente ~37°, el vector de
  strafe rota con él → el robot deja de ir paralelo a la línea y se mete hacia
  adelante. El ToF `min_obst` cae de **~490 mm a 25 mm** al final de la pasada =
  está tocando la pared. (En la corrida 1 chocó; ahí se suma que con la
  iluminación nueva no vio bien la línea — eso es calibración, tema aparte.)

## Tema a analizar (no "bug a fixear")

**Qué pasa si NO se hace (`risk-no-fix`):** el arquero strafe se va de frente y
choca la pared / se desorienta; inutilizable en partido. Mitigación existente: la
**patrulla v3.3** (pulsos pegada a la línea, NO strafe continuo) no tiene este
problema y es el fallback seguro para jugar.

**Qué se rompe al hacerlo (`risk-fix`):** subir la corrección de rumbo de más
puede meter oscilación/temblor alrededor del frente (sobre-corrección). Por eso se
tunea **midiendo**, no a ojo.

**Tiempo (honesto):** 1 sesión de banco (2-4 h) de tune iterativo con caja negra.

## Fix propuesto (firmware — `pfm_heading.h` / `pfm_heading_default_cfg()`)

Aplicar la skill **`control-pid-zona-muerta`**. Hipótesis a probar, en orden:
1. **Subir la ganancia/duty de la corrección** (kp y/o ki del PFM, o pulsos más
   frecuentes / `settle` más corto) para que la corrección siga el ritmo de la
   rotación inducida por el strafe.
2. **Bajar el `deadband`** si el frente se va sin que el PFM reaccione.
3. **Atacar la causa en origen:** ¿por qué el strafe "puro" induce rotación? Puede
   ser desbalance de PWM entre ruedas / `floor_scale` / mismatch de motores. Si el
   strafe no rotara, el PFM no tendría que pelear. Vale revisar la cinemática del
   strafe antes de subir ganancias.
- Cambiar estos parámetros = **recompilar + reflashear**. NO tunear a ciegas:
  medir con caja negra en cada iteración.

## Plan de prueba en hardware real (obligatorio)

**Subsistema:** control de movimiento (lazo PI+PFM de rumbo durante strafe) ·
**Robot:** arquero (R2) · **Firmware:** `central_robot2_arquero_strafe_bb`.

1. En la cancha, robot mirando al arco rival, las 3 placas a batería (>7,8 V).
2. `g` (GO), corrida de 10-20 s. `s` (STOP) → vuelca CSV de caja negra.
3. Analizar la columna `hdg_deg` con `tools/blackbox/analizar_corrida.py`.

**Criterio de aceptación (medible):**
- `|hdg_err| < 10°` **sostenido** durante toda la pasada (no solo al arrancar).
- `min_obst` **no** cae a valores de pared (el strafe se mantiene paralelo a la
  línea, no se mete de frente).
- Sin oscilación/temblor nuevo alrededor del frente (que el fix no genere el
  problema opuesto).

## Cierre

Lo cierra el **equipo humano** tras validar el criterio en banco. Claude NO marca
esta TASK como `done` (regla no negociable: testing en hardware real lo cierra el
equipo).
