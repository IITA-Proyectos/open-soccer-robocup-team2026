# TASK-222 — Validar en banco el TOP de R1 con paridad RT (top_robot1_pri_rt)

- **Placa:** TOP (R1)
- **Asignado:** equipo (banco) — Gustavo / Enzo
- **Prioridad:** P1 (latencia del sensado/heading de R1 en partido; R1 = delantero usa OTOS+heading)
- **Estado:** abierta
- **Depende de:** TASK-216 (boot-check de los 2 BNO de R1) — hacerlo en la misma corrida.

## Por qué
Auditoría 2026-06-19 (verificada por `pio project config`): el env que el `.ini`
llamaba "competencia de R1" (`top_robot1_pri_fastbno`) **NO** tenía 5 flags RT que la
competencia de R2 (`top_robot2_pri`) sí: `SNAPSHOT_TIMER`, `HCSR04_ASYNC`, `TOF_SCHED`,
`BNO_SENTINEL`, `BNO_FREEZE_DETECT`. Se creó **`top_robot1_pri_rt`** (= `top_robot2_pri`
con `-DROBOT1`) para dar lecturas en paralelo + envío independiente (menos latencia),
como pidió Gustavo. Compila OK; **falta validación en HW de R1** (regla del repo: HW lo
cierra el equipo). Los comentarios del `.ini` que afirmaban paridad eran estale (ver journal).

## Cómo
1. Flashear: `pio run -e top_robot1_pri_rt -t upload` (power-cycle tras flashear).
2. Abrir monitor (`pio device monitor -b 115200` o la app) y verificar:
   - **Boot-check BNO (TASK-216):** los 2 BNO ackean 0x28 en sus buses; heading válido.
   - **Centinela @1 Hz:** el 2º BNO se lee (antes no se inicializaba). Cross-check L/R.
   - **Freeze-detect SIN falso-DEAD:** robot QUIETO 5 min → `heading_valid` NO cae a 0
     (la guarda-gyro debe evitarlo). Medir piso de ruido `|gyro_z|` quieto; ajustar
     `IMU_FREEZE_GYRO_MOTION_CDPS` si hace falta. Girar con BNO "clavado" → debe caer a DEAD.
   - **Snapshot @100 Hz desacoplado:** `seq` del WorldSnapshot avanza ~100/s parejo (el
     emisor va por IntervalTimer, no por el loop).
   - **Ultrasonido async:** lectura del HC-SR04 sin frenar el loop (sin pulseIn bloqueante).
   - **`loop_us`:** comparar avg/max vs `top_robot1_pri_fastbno` (esperado: más parejo,
     sin el pico de ~12 ms del pulseIn bloqueante).
3. Girar el robot: el `hdg` del snapshot trackea (no se clava en 0).

## Criterio de cierre
- Los 2 BNO bootean y el heading trackea al girar (cierra también TASK-216).
- Quieto 5 min sin falso-DEAD del freeze-detect.
- Snapshot @~100 Hz parejo + ultrasonido sin bloquear el loop.
- Si todo OK → `top_robot1_pri_rt` pasa a ser el flash de competencia de R1 (y se corrige
  el comentario estale del `.ini` que decía que `_fastbno` ya tenía los RT).

## Escape / rollback
`pio run -e top_robot1_pri_fastbno -t upload` (el flash conservador de R1, sin los 5 RT).
