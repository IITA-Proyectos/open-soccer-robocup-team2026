# TASK-121 — Validar en banco el ANTI-CHOQUE ("frenar y esperar") del arquero (arqueromix)

- **Placa:** CENTRAL (R2, arquero — María/Virginia). Depende del TOP `top_robot2_pri` (manda `min_obstacle_mm`). DOWN `down_robot2`.
- **Asignado:** equipo (banco) — María / Virginia / Gustavo
- **Prioridad:** P2 (mejora; el arquero validado —`_retrofreno`/`_quieto`— NO se toca hasta cerrar esto).
- **Estado:** abierta — **código listo + revisión adversarial + fixes aplicados, SIN banco.** Compila SUCCESS; `_quieto`/`_kickcorto`/`_retrofreno` byte-idénticos verificados. Falta hardware.
- **Build (banco):** `pio run -e central_robot2_arqueromix_evita -t upload`
- **Escape / rollback:** `pio run -e central_robot2_arqueromix_quieto -t upload` (o `_retrofreno` para el retroceso validado).
- **Flag:** `-DARQMIX_AVOID_OBSTACLE`.

## Qué hace (decisión María)
Si hay un robot **cerca al frente** (`obstacle_mm ≤ ARQMIX_OBST_STOP_MM`, 15 cm) → el arquero **FRENA y ESPERA**
(no se mueve hacia el rival, **incluido el despeje**). Cuando el obstáculo se aleja/desaparece (`0xFFFF`), sigue
normal solo. **"Frenar y esperar", NO esquivar/retroceder.** Guard arriba del switch (después del árbitro + re-homing).
Kill-switch: `ARQMIX_OBST_STOP_MM = 0`.

**Dónde vive (todo gateado `-DARQMIX_AVOID_OBSTACLE`):** campo `obstacle_mm` en `amix_io.h`; copia `s.min_obstacle_mm`
en `amix_comm.cpp`; umbral en `amix_config.h`; helper `obstaculo_cerca()` + guard en `amix_fsm.cpp`. NO toca el TOP.

## ⚠️ El SENSOR y el supuesto (BLOQUEANTE — leer)
`min_obstacle_mm` del snapshot **NO es sólo el ultrasonido**: es `min(4 ToF + HC-SR04)` (`sensors_tof_get_min_distance_mm`;
`top_robot2_pri` trae `-DTOP_ENABLE_MULTI_TOF`). El HC-SR04 va **alto → no ve la pelota**, pero **los ToF van más
bajo y SÍ podrían verla**. El anti-choque asume que en R2 es **efectivamente sólo el ultrasonido** — eso vale
**SÓLO si los ToF están deshabilitados en la EEPROM del TOP** (`g_top_cfg.tof[i].enabled=false`).
- **Chequeo #1 (BLOQUEANTE, antes de confiar):** con `top_robot2_pri`, acercar **SÓLO la pelota** (sin robot) al
  frente y leer `snap_min_obstacle_mm` por telemetría. Si baja de ~150 mm con la pelota → el freno se dispara con
  la pelota → **el arquero NO despejaría (gol en contra)**. Mitigación (en el TOP, NO desde acá): deshabilitar o
  enmascarar las zonas bajas de los ToF frontales (`g_top_cfg.tof[i].enabled/zone_mask`), o correr el TOP sin MULTI_TOF.

## Revisión adversarial 2026-07-01 (4 lentes + verificación) — 4 hallazgos, TODOS atendidos
- **#1 GRAVE (CORREGIDO):** el guard congelaba sin pausar `millis_inicio_estado` → si congelaba durante el golpe
  (`PATEANDO_adelante`, 450 ms) más que su timeout, al reanudar el timer ya había vencido + `parar()` reseteaba la
  rampa → **saltaba el despeje entero (no pateaba)**. FIX: `millis_inicio_estado = millis()` en el guard (pausa el
  reloj del estado → al reanudar corre su duración completa).
- **#2 MEDIO (CORREGIDO, mismo fix):** igual con `ALINEAR_arco_opp` (300 ms) → despejaba sin apuntar al arco.
- **#3 GRAVE (CORREGIDO):** el freno preemptaba `frenar_patada` (contra-empuje ACTIVO 200 PWM que mata la inercia
  del golpe); como `parar()` es RUEDA LIBRE, la inercia del golpe empujaba al arquero **HACIA** el obstáculo. FIX:
  el guard **excluye `frenar_patada`** (`estado != Estado::frenar_patada`) → deja completar el freno activo.
- **#4 GRAVE (CORREGIDO = doc + chequeo):** el comentario decía "en R2 los ToF están sin usar" como HECHO — es
  falso a nivel firmware (ver arriba). Comentarios corregidos en `amix_io.h`/`amix_config.h`/`platformio.ini`; el
  supuesto queda como chequeo BLOQUEANTE de banco (arriba).
- Descartado (no es bug): "queda congelado sobre la línea" — es la consecuencia directa del diseño "frenar y
  esperar" (no cruza la línea, sólo espera); titrar en banco si molesta.

## Cómo validar (en orden)
1. **🔴 Chequeo #1 (arriba): que la PELOTA no dispare el freno.** Si lo dispara, parar acá y arreglar la config del TOP.
2. **Que frene con un robot:** algo ALTO (otro robot / la mano a la altura del ultrasonido) a <15 cm al frente →
   FRENA y queda quieto. Sacarlo → sigue normal.
3. **Despeje con obstáculo (fixes #1/#3):** interponer un objeto alto al frente DURANTE el despeje >0,5 s y soltar;
   confirmar que al soltar (a) hace un golpe COMPLETO (no lo saltea) y (b) el freno del golpe (`frenar_patada`)
   aplica el contra-empuje, no coastea contra el obstáculo.
4. **A/B / kill-switch:** `_quieto` o `ARQMIX_OBST_STOP_MM=0` = sin anti-choque.
5. **NO-REGRESIÓN:** homing, seguimiento de pelota y secuencia de despeje, iguales al validado cuando NO hay obstáculo.

## Criterio de cierre (humano)
- La PELOTA NO dispara el freno (chequeo #1). Frena con un robot a <15 cm y sigue al alejarse. El despeje con
  obstáculo NO se saltea y NO lo empuja al obstáculo. Umbral titrado. Sin regresión. Repetible.
- **Decisión:** si anda → combinar con el retroceso validado (`-DARQMIX_RETRO_BRAKE_ON_LINE` + `-DARQMIX_AVOID_OBSTACLE`)
  y evaluar promover; si no → quedarse con `_retrofreno`/`_quieto`.

## Perilla
- `ARQMIX_OBST_STOP_MM` (150 mm) en `amix_config.h` (gateado). Subir = frena más lejos; bajar = deja acercarse más; 0 = apagado.

## Relación
- Feature del arquero anti-"salirse"/anti-choque: `_kickcorto` (TASK-119), `_retrofreno` (TASK-120, validado + checkpoint
  tag `arquero-retrofreno-checkpoint-2026-07-01`), y este `_evita`. Combinables por flags.
