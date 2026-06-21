# TASK-115 — Validar en banco el centralmix con heading del BNO del TOP (snapshot)

- **Placa:** CENTRAL (R1, delantero) + depende de TOP (R1) con el BNO andando.
- **Asignado:** equipo (banco) — Gustavo / Elías
- **Prioridad:** P2 (prueba paralela; el stack actual `src/central/` no se toca)
- **Estado:** abierta
- **Build:** `pio run -e central_robot1_mix_bno -t upload` (⚠️ **sin compilar por Claude** — la
  shell de la máquina de la sesión estaba rota; **compilar primero**). Escape: cualquier env de competencia.

## Por qué
El delantero "mix" (`src/centralmix/`) ahora puede tomar el heading del **BNO del TOP** por el
WorldSnapshot (Serial7) en vez de un BNO local que **R1 no tiene**. Modo nuevo
`-DMIX_HEADING_SNAPSHOT` (env `central_robot1_mix_bno`). Resuelve el pendiente #5 de TASK-113.
Ver journal `2026-06-21-centralmix-bno-del-top-snapshot.md`.

## Pre-requisito (hardware)
- TOP de R1 flasheada con **`top_robot1_pri_rt`** (trae el fix `bno_left_en` → BNO primario
  habilitado, `sensors_imu.cpp:279`). El heading del BNO de R1 ya se validó en banco 2026-06-21
  (reposo −15.6 → girado 101.4). Si la TOP de R1 NO tiene ese firmware, este test no aplica.

## Cómo validar (en orden)
1. **Compila**: `pio run -e central_robot1_mix_bno` → SUCCESS. (Confirma también que `central_robot1_mix`
   y `central_robot1_mix` + `-DMIX_HEADING_OTOS` siguen compilando — no se rompió nada.)
2. **Heading llega y es vivo** (robot quieto, luego girado a mano): por telemetría/serial confirmar
   que `g_io.heading_deg` / `heading_error_deg` SIGUEN el giro y `heading_valid=true` mientras el
   snapshot del TOP trae heading válido. En reposo el `error` debe quedar ~0; al girar ~90° debe
   marcar ~±90. **Make-or-break:** si `heading_valid` queda en false o el error no se mueve →
   revisar que la TOP esté mandando `heading_valid` (bit4) y el firmware correcto.
3. **A/B contra OTOS** (opcional, diagnóstico): `otos_heading_deg` queda disponible siempre. Comparar
   BNO-del-TOP vs OTOS girando el robot a mano (¿coinciden en sentido y magnitud?).
4. **Resto del delantero** (hereda de TASK-113, no se re-hace acá si ya se cerró): primitivas de
   motor por rueda, re-tuneo píxeles→mm, sector de línea, arco rival. Con heading vivo, ahora SÍ se
   ejercen las condiciones de rumbo del FSM (`|error|<=1` para patear, `<=50/80`).

## Criterio de cierre
- `central_robot1_mix_bno` compila.
- Con la TOP de R1 (BNO andando), `g_io` muestra heading del BNO vivo y `heading_valid=true`.
- El delantero alinea/patea usando ese heading (el ciclo del FSM responde al rumbo, no solo a visión).
- **Decisión:** si anda → este es el delantero "mix" con gyro para R1; si no → `-DMIX_HEADING_OTOS`
  (sin gyro) o se sigue con `src/central/` + `strategy.cpp`.

## Escape / rollback
`central_robot1_mix` con `-DMIX_HEADING_OTOS` (mismo delantero, sin gyro) o cualquier env de
competencia (`central_robot1` / `central_robot1_delantero_practica`). El cambio es aditivo: no toca
ningún env existente.

## Relación con otras tasks
- **TASK-113** (validar centralmix general): esta TASK-115 cierra concretamente su paso 1 (heading source).
- **TASK-216 / TASK-223** (BNO de R1 / freeze por ToF): el heading del TOP del que depende esta task
  ya quedó validado en banco 2026-06-21 (TASK-223 = pista falsa).
