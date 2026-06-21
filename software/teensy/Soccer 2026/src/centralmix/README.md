# centralmix — port del DELANTERO 2025 sobre datos de TOP/DOWN (PRUEBA)

> **Qué es.** Una versión EXPERIMENTAL del delantero: toma como base el programa
> **delantero 2025** (su máquina de estados y su **manejo DIRECTO de motores**) y lo
> alimenta con los datos que llegan de **placa TOP y DOWN**, en vez de leer sensores
> directo. Objetivo: poder debuggear con la FSM que el equipo ya conoce. Es una
> prueba — **si sale, va; si no, se sigue con el stack actual** (`src/central/`).
>
> ⚠️ **Compila, pero NO está validado en banco.** Compilar ≠ andar.

## Aislamiento (no toca nada de lo actual)
- Todo vive en `src/centralmix/` (carpeta nueva) y en el namespace `iitasoccer::mix`.
- Env propio `central_robot1_mix` con `build_src_filter = +<centralmix/> +<shared/>`
  → **NO compila `src/central/`** ni ningún otro env. Aditivo puro.
- Flashear (R1 **CON gyro** — recomendado, BNO del TOP por snapshot):
  `pio run -e central_robot1_mix_bno -t upload`
- Flashear (BNO LOCAL en la CENTRAL — **no aplica a R1**, no tiene BNO local):
  `pio run -e central_robot1_mix -t upload`
- Volver a lo actual:  cualquier env de competencia de siempre (`central_robot1`, etc.).

## Flujo de datos (autocontenido, SIN world_model)
```
TOP (Serial7) ─┐                       ┌─ mix_fsm   (FSM 2025: 24 estados)
DOWN (Serial1) ┴─ mix_comm ─► g_io ───►┤
                  (decode proto)        └─ mix_motors (manejo DIRECTO 2025 → pines Zircon R1)
```
- `mix_io.h` — `struct MixIO g_io`: las **variables planas** estilo 2025 (pelota x/y +
  ángulo, arcos, heading + error, línea, match_running, OTOS). Es "lo disponible".
- `mix_comm.cpp` — único que toca Serial. Lee TOP/DOWN, decodifica con `shared/proto`,
  llena `g_io`. Heading (3 modos): **BNO LOCAL** en la CENTRAL por default (⚠️ R1 no tiene
  → no usar en R1); **BNO del TOP por snapshot** con `-DMIX_HEADING_SNAPSHOT` (env
  `central_robot1_mix_bno`, el de R1 con gyro); **OTOS** con `-DMIX_HEADING_OTOS` (sin gyro).
- `mix_fsm.cpp` — port FIEL del switch de 24 estados del 2025; lee `g_io`, llama motores.
- `mix_motors.cpp` — primitivas directas 2025 (`girar/avanzar/centrar/patear/...`) sobre
  los pines R1 actuales (M1=2/5/3, M2=8/7/6, M3=11/12/4), sin mixer.
- `mix_config.h` — pines + constantes 2025 + selector de heading.

## TODO de banco (lo que falta validar — el equipo cierra HW)
1. **Sentido de cada motor por rueda** — el 2025 usaba OTRO mapeo de pines; las primitivas
   pueden salir invertidas o laterales. Verificar `avanzar/girar/centrar/patear` en banco.
2. **Re-tuneo píxeles→mm** — umbrales `Xp<=50` (cercanía) y alineación con arco eran de
   cámara 2025 (píxeles); ahora son mm/grados. Re-tunear (`MIX_TOL_*` en `mix_config.h`).
3. **Línea 3-sensores → 1-ángulo** — los 3 `DETECTA_LINEA_*` del 2025 se mapean por sector
   angular (±30°) del dato de DOWN. Re-tunear el sector.
4. **Arco rival = `goal_opp`** (POR ROL, sin color). La polaridad propio/rival la resuelve la
   placa TOP (`goal_polarity`); el delantero apunta al arco que el TOP marca como rival. Ya no
   hay `-DMIX_ATTACK_BLUE` ni color en la CENTRAL. Confirmar en banco que el TOP da `goal_opp` bien.
5. **Heading source** — ✅ RESUELTO (coach 2026-06-21): R1 NO tiene BNO local → usa el
   **BNO del TOP por snapshot** (env `central_robot1_mix_bno`, `-DMIX_HEADING_SNAPSHOT`).
   El BNO del TOP de R1 quedó andando + validado en banco 2026-06-21 (fix `bno_left_en`).
   Falta validar el delantero COMPLETO con ese heading en banco → TASK-115.
6. **`match_running`** — se agregó el gate GO/STOP del árbitro (el 2025 no lo tenía).

Detalle de cada decisión: comentarios en los headers + journal 2026-06-19.
