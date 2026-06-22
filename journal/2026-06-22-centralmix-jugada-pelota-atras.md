# 2026-06-22 — centralmix: jugada "pelota atrás" (cámara trasera) — giro-encare sobre el piso

## Qué se pidió (Elías)
La cámara trasera detecta la pelota cuando queda DETRÁS del robot, pero "esa implementación de qué
sirve" si la FSM no hace nada bueno con eso. Pedido: usar el dato para hacer la **mejor jugada** y
**implementarla**.

## Diagnóstico (workflow multi-agente: 10 agentes, entender→diseñar→sintetizar→red-team)
- Confirmado en código: la cámara trasera se rota 180° (`cameras_fusion.cpp` `cam_obs_to_robot_frame`,
  cam_id==1 → x=-x, y=-y) → pelota detrás = **`ball_y_cm < 0`** → `angulo_pelota_deg ≈ ±180`.
- Conducta ACTUAL con pelota atrás: `APUNTAR_PELOTA` llama `apuntar_pelota_motores()` que gira a
  `100*MIX_A = 35` PWM. **35 está por debajo del piso del motor `{70,70,107}`** (config_central
  `MOTOR_MIN_PWM`) → el robot **zumba sin girar** (zona muerta). Encima `atan2` **salta ±180↔-180**
  con el ruido lateral → el sentido se invierte cada tick. Resultado: el delantero queda **clavado
  ~10 s** hasta el timeout. Riesgo de gol en contra: **BAJO** (CENTRANDO ya orbita y alinea al arco
  rival antes de patear) → la jugada NO necesita ser goal-aware, solo **desclavar rápido**.

## Qué se implementó (guard en APUNTAR_PELOTA — cero estados nuevos)
Si la pelota está atrás, **girar EN EL LUGAR sobre el piso** hasta que quede apuntada. Claves de
robustez (todas salidas del red-team, ver abajo):
1. **Se ENTRA por `ball_y_cm < -MIX_ATRAS_Y_ENTRA` (6 cm)** — señal MONÓTONA, no por el ángulo que salta.
2. **El sentido se LATCHEA una sola vez** (`s_giro_atras_dir`, static de archivo) → no dithera en ±180.
3. **Se gira a `MIX_ATRAS_PWM` (120, sobre el piso) en TODO el arco** 180→15, hasta `|angulo|<MIX_TOL_APUNTADO`
   → recién ahí cae a `AVANZANDO`. NO entrega al apuntado de 35 a mitad de camino (eso re-clavaba).
4. **Giro PURO** (3 ruedas mismo signo, como `girar()`) → NO traslada la pelota a ningún arco.
5. **El latch se RESETEA a 0 en mix_fsm_init + las 5 salidas** del case (else→AVANZANDO, pierde-pelota
   500ms, timeout 10s, 3 escapes de línea) → no queda pegado entre visitas.
6. **`MIX_ATRAS_DIR_SIGN` (+1)**: el signo físico de +pwm está `<RE-VERIFICAR EN BANCO>`; si encara por
   el lado LARGO, poner −1. Peor caso de signo mal = ~340° de giro (NO gol en contra: gira en el lugar).
7. **Kill-switch:** `MIX_ATRAS_Y_ENTRA = 9999` → nunca dispara → FSM idéntica a hoy.

NO se tocó: `apuntar_pelota_motores` (apuntado fino de pelota al frente, intacto), AVANZANDO,
CENTRANDO_*, la **patada** (`avanzar_patear`/`MIX_KICK_*`, lo de ayer queda intacto), kickoff, escapes.
Diagnóstico nuevo: `g_io.giro_atras_dir` en el debug USB (para validar el anti-dither: debe quedar FIJO).

## Lo que el red-team encontró (y se corrigió ANTES de implementar)
- **Gap de zona muerta (ALTA, 2 agentes):** el diseño original entregaba a `apuntar(35)` cuando
  `|angulo|<140` → se re-clavaba en 140→15. **Fix aplicado:** girar a 120 hasta `|angulo|<15`, gate de
  entrada por `ball_y` → el giro cubre TODO el arco. (De paso elimina la histéresis angular y la del
  borde y=-6: el latch persiste hasta apuntado.)
- **Signo no garantizado camino corto (ALTA):** el latch usa `sign(angulo)` (convención del apuntado,
  tuneada para ángulos chicos) → cerca de ±180 puede ser el lado largo. **Fix:** `MIX_ATRAS_DIR_SIGN`
  (banco) + nota; no es gol en contra (giro puro).
- **Latch persistente entre estados (ALTA/MEDIA):** static + reset en init NO alcanza. **Fix:** reset en
  las 5 salidas (obligatorio, no opcional).
- **PWM 110 marginal vs piso M3=107 (MEDIA):** **Fix:** arrancar en **120** (13 de margen) + titular.
- **Churn APUNTAR↔GIRANDO si la cámara parpadea (MEDIA):** mitigado por reset-on-exit (re-entra y
  re-latchea limpio); si en banco hay churn, subir el umbral de pérdida para el caso atrás (anotado).

## Compilación
`pio run -e central_robot1_mix_bno` y `central_robot1_mix` → **SUCCESS**. NO validado en banco
(regla #1) → **TASK-118** (signo físico, piso/zumbido, anti-dither, no-regresión de la patada).

## Archivos
`mix_config.h` (constantes MIX_ATRAS_*), `mix_fsm.cpp` (latch + guard en APUNTAR_PELOTA + reset),
`mix_io.h` (diag `giro_atras_dir`), `main_centralmix.cpp` (debug). Ver [[project-iita-soccer-2026-strategy]].

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
