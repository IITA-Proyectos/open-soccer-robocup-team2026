---
title: "¿Qué env flasheo hoy? — mapa vigente por robot y placa"
date: 2026-06-11
status: vivo
tipo: indice-operacional
---

# ¿Qué env flasheo HOY? (al 2026-06-11)

> **Para qué existe:** platformio.ini tiene ~80 envs y la respuesta a "¿qué flasheo?"
> vivía repartida en banners y guiones. Esta media página es LA respuesta.
> **Regla:** se actualiza en el mismo commit que cualquier cambio de env recomendado.
> Comando: `pio run -e <ENV> -t upload` desde `software/teensy/Soccer 2026/` con el
> USB en la placa correcta. ⚠️ Compilar SIEMPRE desde `C:\Users\violl\iitasoccer\soccer-main`
> (el clon `futbol2026\` es un señuelo viejo).

## Tabla vigente

| Robot | Placa | Env VIGENTE | Nota |
|---|---|---|---|
| **R1** | CENTRAL (partido, arquero) | ⚠️ **decisión pendiente** — hoy usar `central_robot1_arquero_demo` (o `_bb` con caja negra) | `central_robot1` "a secas" corre el clamp viejo SIN floor_scale (síntomas del banco 2026-06-09); crear `central_robot1_match` está en el backlog |
| R1 | CENTRAL (práctica delantero) | `central_robot1_delantero_practica_bb` (+`_obst_bb` = 2ª instancia anti-choque) | Sin gyro: eje por OTOS |
| R1 | TOP | **`top_robot2_pri`** (sí, "robot2": vale para AMBAS TOP desde el recableado 2026-06-11) | `top_robot1*` = cableado VIEJO, NO flashear |
| R1 | DOWN | `down` (a secas — CON OTOS) | `down_robot2` NO va en R1 |
| **R2** | CENTRAL (delantero partido) | `central_robot2` | banco/demo: `central_robot2_demo_bb` |
| R2 | CENTRAL (arquero) | `central_robot2_arquero` | banco: `_bb` / solo-patrulla: `_patrol_bb` |
| R2 | TOP | **`top_robot2_pri`** | `top_robot2_pri_sticky` = cámara pegajosa (promover tras validar) |
| R2 | DOWN | `down_robot2` (sin OTOS) | |
| ambos | DOWN (calibrar línea) | `diag_down_calibracion` (`c`→`b`→`v`→`s`, 32/32 margin ≥40) | al terminar, RE-flashear el down. ⚠️ **POSIBLE CAMINO MÁS CORTO sin reflashear:** el `down`/`down_robot2` de competencia lleva `-DDOWN_USB_MONITOR` → calibrar EN VIVO con la app `monitor-base`. El código lo soporta pero **3 docs se contradicen** (esta fila vs `platformio.ini` vs la guía `USO-MONITOREO-Y-TELEMETRIA.md`) → **TASK-307** lo resuelve/confirma en banco antes de fijar el flujo |
| ambos | Cámaras ×2 | `hardware/electronics/camaras-openmv/main.py` | NO los `cam-*-n6.py` de los packs (deprecados) |

## Lista negra (NO flashear sin leer el journal correspondiente)

| Env(s) | Por qué |
|---|---|
| `top_robot1`, `top_robot1_oscint`, `top_robot1_bno_wire2` | Cableado/hipótesis PRE-recableado 2026-06-11: buscan el BNO en el bus equivocado |
| `top_robot1_bnofreeze` | Su comentario dice "alias inocuo" — es FALSO: re-activa el detector de freeze QUITADO el 2026-06-08 por falsos-DEAD latcheantes. Solo banco de re-tune |
| `top_robot1_multitof`, `top_robot2_multitof` | Alias redundantes obsoletos (los 4 ToF son default desde 2026-06-01) |
| `top_robot1_debug_telemetry` | Extiende el cableado viejo; usar `top_robot2_pri_debug_telemetry` |
| `central_robot*_slow`, `*_wdt*`, `down_wdt`, `down_lean`, `diag_*` | De banco/diagnóstico — nunca para partido |
| `diag_central_line_sweep*` | El motor2() fue espejado tras el recableado del M2 — re-validar signos en banco antes de confiar |

## Teclas que siempre se confunden

- Envs de banco con juez-PC (`*_demo*`, `*_practica*`, `*_arquero*_bb`): **`g`=GO · `s`=STOP** (+ caja negra: `d`=volcar, `x`=borrar).
- `diag_central_arbitro_strafe_*`: **`s`=START · `x`=STOP** (¡al revés!).
