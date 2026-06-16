# 2026-06-16 — Reingeniería RT de la placa DOWN: diseño validado + programado (gateado, host-tested)

## Pedido

Gustavo (4 h autónomas): "analizá si existe una definición funcional de mejoras de la placa DOWN;
si no, hacé la reingeniería basándote en el TOP. Validá el diseño de cada módulo, programá en
paralelo cada uno con mínima latencia sensor→envío (a CENTRAL y a TOP) + SUPER confiable / fail-safe
con degradación coherente. Trabajá en workflows paralelos, validá, programá, compilá, probá, dejá
todo listo."

## Hallazgo: la definición funcional YA existía

`docs/firmware/ARQUITECTURA-LAZO-DOWN-RT.md` (2026-06-15) es un diseño RT completo de 4 capas
(adquisición → pizarra → cálculo confiable → difusión), fases F0–F7 gateadas, con plan de banco y
~80% de módulos puros ya host-tested. **No rehíce la reingeniería: validé el diseño y lo
implementé** (lo que el pedido indica para el caso "si la encontrás").

## Cómo trabajé (3 workflows paralelos + glue propio)

1. **Validación adversarial** (7 agentes, 1 por fase + síntesis de arquitecto). Confirmó el diseño
   contra el código actual y cazó correcciones clave: `sensor_slot.h` YA existe (TOP lo usa) → F6
   reusa, no recrea; los 3 campos nuevos de `DownModelCfg` deben ir TRAILING + default in-class
   (initializer posicional → byte-identidad); F4 antes que F3 sobre `down_model.cpp`; CERO cambios
   de `types.h`/wire-contract.
2. **Módulos puros en paralelo** (5 agentes, archivos nuevos sin conflicto, self-test host):
   `adc_scan_plan.h`(10), `line_neighbors.h`(13)+`line_early_escape.h` refinado(20),
   `line_reliable_gate.h`(13), `rx_calib_defer.h`(5)+`rx_byte_budget.h`(6), `down_blackboard.h`(12).
   **79 tests nuevos, todos verdes.**
3. **Glue gateado** (lo hice yo, secuencial por los archivos compartidos, cada paso compila):
   - **F0** LoopMonitor cableado en `main_down.cpp` (mide el WCET de la vuelta REAL, incl. spike OTOS).
   - **F1** `line_ring.cpp`: averaging-1 (`-DDOWN_ADC_FAST`) + dual-ADC (`-DDOWN_ADC_DUAL`, reusa
     `adc_scan_plan.h`). 717µs→~126µs.
   - **F2** `otos.cpp`: `Wire.setClock(400000)` + `getPosVelAcc` burst (4 transacciones → 2). 3-4ms→<0.6ms.
   - **F4** `down_model.{h,cpp}`: compuerta de lectura confiable (reusa `line_reliable_gate.h`) +
     SELLADO fail-safe de la geometría a sentinela cuando `data_valid=0` — la capa "súper confiable".
     NO sella `line_present`/`sensors_on_line` → el freno duro de CENTRAL (`EV_IMMINENT_EXIT`) sobrevive.
   - **F5** `comm_central.{cpp,h}` + `comm_top.cpp` + `main_down.cpp`: calib diferido fuera del path
     RX (`rx_calib_defer.h`) + presupuesto de bytes/tick (`rx_byte_budget.h`) + colchón `addMemoryForRead` 512B.
4. **Revisión adversarial del glue** (5 agentes): **0 must-fix, byte-identidad OK, fail-safe OK** en
   las 5 fases. Apliqué las mejoras low/med que sugirió: gate simétrico en F0, config ADC no
   redundante en F1, y documenté la semántica del sellado F4 + el GAP-5 de F5 (ver TASK-309).

## Verificación

- **Full host gate: 84 envs, 1162 tests, 0 fails** (79 nuevos + cero regresión por el cambio gateado
  de `down_model.cpp`).
- **Compila** todo: `down` (competencia, byte-idéntica) + 9 envs de banco nuevos (`down_loopmon`,
  `down_adcfast`, `down_adcdual`, `down_otosfast`, `down_reliable`, `down_rxharden`, `down_rt_all`...)
  + `down_rt_all` prueba que F0+F1+F2+F4+F5 coexisten.

## Lo que NO hice (honestidad, regla #1)

- **NADA probado en banco.** Es firmware embebido para una placa que no tengo; lo cierra el equipo
  → **TASK-309** (plan de banco por fase). Claude no marca TASKs de hardware como `done`.
- **F3** (detección temprana) y **F6** (pizarra): módulos puros entregados + host-tested, pero el
  CABLEADO al loop vivo queda POST-Incheon (decisión del diseño: F4 gobierna el camino vivo; la
  pizarra seqlock solo se integra cuando el productor pase a ISR/DMA).
- **GAP-5**: la 2ª boca de calib (USB monitor) no se difirió — aceptable (USB = admin/robot quieto,
  nunca en partido); anotado en TASK-309 como deuda opcional.

## Atribución
Validación + módulos puros + glue + docs + esta entrada: Claude Opus 4.8 (Anthropic), 2026-06-16
(requested-by Gustavo Viollaz). Sobre el diseño previo de la misma sesión-IA (2026-06-15).
