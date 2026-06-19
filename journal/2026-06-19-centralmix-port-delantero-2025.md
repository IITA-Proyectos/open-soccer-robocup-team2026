# 2026-06-19 — centralmix: port del delantero 2025 sobre datos de TOP/DOWN (prueba aislada)

## Qué se hizo
Se creó **`src/centralmix/`** + env **`central_robot1_mix`**: una versión EXPERIMENTAL del
delantero que toma como base el **programa 2025** (su FSM y su manejo DIRECTO de motores) y
lo alimenta con los datos de **TOP/DOWN** vía variables planas (`g_io`), **sin world_model**.
Objetivo: debuggear con la FSM que el equipo ya conoce. Pedido de Gustavo. **Compila** (`pio
run -e central_robot1_mix` SUCCESS, FLASH ~23,5 KB). **Aislado:** no toca `src/central/` ni
ningún env existente (bloque aditivo en `platformio.ini`).

## Estructura (todo nuevo, namespace `iitasoccer::mix`)
- `main_centralmix.cpp` — setup/loop (comm → FSM → motores).
- `mix_io.h` — `struct MixIO g_io`: variables planas estilo 2025 (pelota x/y+ángulo, arcos,
  heading+error, línea, match_running, OTOS).
- `mix_comm.cpp` — único que toca Serial. Lee TOP (Serial7) + DOWN (Serial1), decodifica con
  `shared/proto` + `line_view`/`pose_view`, llena `g_io`. Heading: BNO por default; OTOS con `-DMIX_HEADING_OTOS`.
- `mix_fsm.cpp` — port FIEL del switch del delantero 2025: **24 estados** (el enum real tiene
  24, NO 28; se portó verbatim y se marcó la discrepancia). Regla nueva: gate `match_running` (GO/STOP).
- `mix_motors.cpp` — primitivas directas 2025 (`girar/avanzar/centrar/patear/...`) sobre pines
  Zircon R1 (M1=2/5/3, M2=8/7/6, M3=11/12/4), sin mixer.
- `mix_config.h` — pines + constantes 2025 + selector de heading. `README.md` — guía del equipo.

## Cómo se llegó (port, no copia)
El 2025 corría mono-placa leyendo sus sensores directo (cámara por UART 9 bytes en píxeles,
BNO local, 3 sensores de línea analógicos) y manejando motores inline. El port:
- **Entradas:** reemplazadas por `g_io`, que `mix_comm` llena desde el WorldSnapshot del TOP
  + LineStatusV2/OTOS de DOWN (mm/grados, no píxeles).
- **Motores:** mismas primitivas/valores 2025, retargeteados a los pines R1 actuales (mismo
  H-bridge INA/INB+PWM → portan directo).
- **FSM:** transiciones/umbrales/timers verbatim del 2025.

Construido con 2 workflows (entender + scaffold). Detalle de contratos: headers de `centralmix/`.

## Pendiente (NO validado en banco → TASK-113; lo cierra el equipo)
1. **Sentido de cada motor por rueda** (el 2025 usaba otro mapeo de pines → primitivas pueden
   salir invertidas/laterales). 2. **Re-tuneo píxeles→mm** (umbrales `MIX_TOL_*`). 3. **Línea
   3-sensores→1-ángulo** (sector ±30° del dato de DOWN). 4. **Arco rival** hardcodeado AMARILLO
   (`-DMIX_ATTACK_BLUE` invierte). 5. **Heading source**: el `mix_comm` quedó leyendo BNO local
   del CENTRAL; confirmar si R1 usa eso o el heading del snapshot del TOP (el BNO del TOP de R1
   está muerto, se cambia 2026-06-19 PM — ver journal arquero-pose-xy del día).

## Decisión de fondo
Es una PRUEBA paralela, no reemplaza nada. Si en banco anda → se sigue por acá; si no → se
continúa con el stack actual (`src/central/` + `strategy.cpp`). Cero riesgo para lo de hoy
(build aislado). Ver [[project-iita-soccer-2026-strategy]].
