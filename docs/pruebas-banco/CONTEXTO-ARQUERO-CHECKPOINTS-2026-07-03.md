Soy María, equipo IITA Soccer (RoboCup Open 2026, Incheon). Trabajo el **ARQUERO = ROBOT2, placa CENTRAL**.
Repo: `C:\Users\violl\iitasoccer\soccer-main` (rama `main`). Familia de programa: **`arqueromix`** (`src/arqueromix/`).
Antes de tocar nada: `git pull` y leer `docs/ESTADO-ACTUAL.md`, `docs/FUENTES-DE-VERDAD.md`,
`docs/pruebas-banco/QUE-FLASHEO-HOY.md`. Regla del repo: el testing en hardware lo cierra el equipo — NO marcar
tareas de hardware como hechas por "compila".

Vengo armando **7 CHECKPOINTS** del arquero, cada uno gateado detrás de flags, sin romper los anteriores.

## REGLA DE ORO (cómo se construyó todo — respetar SIEMPRE)
- El arquero DEFINITIVO = `central_robot2_arqueromix_quieto` (md5 `8D0168CF81F7CDAF347B0AB89E030E59`),
  byte-idéntico e INTOCABLE.
- CADA checkpoint = el base + FLAGS `-D…` gateados (aditivo). Todo lo nuevo va bajo `#ifdef`.
- Después de CUALQUIER cambio: **compilar y verificar que los checkpoints anteriores queden BYTE-IDÉNTICOS**
  (Get-FileHash md5 del `firmware.hex` vs los de abajo). Si un md5 cambió, el gating no fue hermético.
- Nada se promueve a competencia sin banco.

## CÓMO FUNCIONA EL ARQUERO (arqueromix)
- FSM SOLO QUIETO (sin patrulla). Flujo: `inicio_lateral_izq` → `inicio_retroceder` (homing: atrás hasta la
  línea) → `inicio_avanzar` → `esperar_quieto` (espera / sigue la pelota de costado / despeja) → secuencia de
  despeje (`PATEANDO_pausa_inicial` → `ALINEAR_arco_opp` → `PATEANDO_adelante` → `frenar_patada` → `PATEANDO_pausa`
  → `orientar_frente` → `PATEANDO_atras` → `acomodar_linea` → `acomodar_orientar` → `esperar_quieto`).
- Recibe TODO por serie de TOP/DOWN en variables planas `g_aio` (`amix_io.h`): pelota/arcos/heading del SNAPSHOT
  del TOP; línea (`line_present`/`line_depth`/`line_angle`) del DOWN; `min_obstacle_mm` (ToF + ultrasonido) del
  snapshot. NO hay BNO local (el heading viene del TOP).
- Motores DIRECTOS (`amix_motors.cpp`). Archivos: `amix_config.h` (constantes/knobs), `amix_fsm.{cpp,h}`,
  `amix_io.h`, `amix_comm.cpp`, `amix_motors.{cpp,h}`.

## LOS 7 CHECKPOINTS (flashear: `pio run -e <env> -t upload` desde `…\Soccer 2026`; TOP=`top_robot2_pri`, DOWN=`down_robot2`)

1. **`central_robot2_arqueromix_quieto`** — DEFINITIVO/base. Homing → espera → sigue pelota de costado → despeja.
   Flags: `-DARQMIX_QUIETO`. md5 `8D0168CF81F7CDAF347B0AB89E030E59`. Estado: validado ("anda bastante bien").

2. **`central_robot2_arqueromix_kickcorto`** — + pateo CORTO cuando el despeje arranca sobre la línea (menos envión).
   Flags: `-DARQMIX_KICK_SHORT_ON_LINE`. md5 `72F2516E30D1FC41DECE7B1F80C9A9D0`. Estado: banco parcial (TASK-119).

3. **`central_robot2_arqueromix_retrofreno`** — retroceso post-pateo va HASTA la línea y luego AVANZA para
   despegarse (espejo del homing). + corte de seguridad si el enlace DOWN muere.
   Flags: `-DARQMIX_RETRO_BRAKE_ON_LINE`. md5 `F41B8D1D7D98708F4AD0415B5FBE24D2`. Estado: **María "andando perfecto"**;
   pendiente el chequeo de seguridad DOWN-muerto (TASK-120). Tag `arquero-retrofreno-checkpoint-2026-07-01`.

4. **`central_robot2_arqueromix_evita`** — ANTI-CHOQUE: si el ultrasonido (min_obstacle_mm) ve un robot a <15 cm
   al frente, FRENA y ESPERA (no se mueve hacia él, incluido el despeje); cuando se aleja, sigue.
   Flags: `-DARQMIX_AVOID_OBSTACLE`. md5 `FDD6667AE1371230C1944114B29C6C4A`. Estado: anda (María); **falta el
   chequeo BLOQUEANTE de la pelota (TASK-121, ver abajo)**. Tag `arquero-evita-checkpoint-2026-07-01`.

5. **`central_robot2_arqueromix_retrotiempo`** — los 2 retrocesos que dependían de la línea pasan a TIEMPO FIJO
   (NO leen línea): homing = 0,4 s (`AMIX_T_INICIO_RETRO_FIXED`), post-pateo = 1,3 s (`AMIX_T_ATRAS_FIXED`).
   Para cancha donde la línea no se detecta. Flags: `-DARQMIX_RETRO_BY_TIME`.
   md5 `B389BD33827354AAC1D06CFA5EC58524`. Estado: en titración. Tag `arquero-retrotiempo-checkpoint-2026-07-03`.

6. **`central_robot2_arqueromix_evita_lejos`** — copia del #4 + golpe MÁS LARGO (`AMIX_T_PAT_ADELANTE` 450→550 ms,
   la pelota va más lejos) + escape al buscar MAYOR (`AMIX_T_BUSCAR_AVANCE` 400→500 ms).
   Flags: `-DARQMIX_AVOID_OBSTACLE -DARQMIX_KICK_FAR -DARQMIX_BUSCAR_AVANCE_LARGO`.
   md5 `D81386B6A40EF35D3FF66294641D9D49`. Estado: en titración. Tag `arquero-evitalejos-checkpoint-2026-07-03`.

7. **`central_robot2_arqueromix_evita_lejos_rehome`** — copia del #6 + RE-HOMING: si pasa 15 s
   (`AMIX_T_REHOME_NO_BALL`) sin ver la pelota, vuelve una vez atrás HASTA la línea + escape (reusa el homing) y
   sigue esperando. Flags: `#6 + -DARQMIX_REHOME_NO_BALL`. md5 `4BF4C64F8D975CA0F883F86EB0CC0937`. Estado: sin
   banco. Tag `arquero-rehome-checkpoint-2026-07-03`.

8. **`central_robot2_arqueromix_centrado_fino`** — copia del #7 + CENTRADO FINO + POTENCIA +6%.
   (a) Banda muerta angular del seguimiento `AMIX_TOL_CENTRADO_DEG` 8°→5° (a 8° el corrimiento lateral
   aceptado ≈14% de la distancia → pelota a 1,5 m quedaba hasta ~21 cm corrido; a 5° ≈9%). NO arregla el
   "queda torcido" (nada re-orienta en la espera; lo mitiga el re-homing heredado). (b) TODAS las potencias
   del camino vivo +6% proporcional (`AMIX_POWER_SCALE`=1.06: seguir pelota 75/133→79/141, avanzar 100→106,
   homing 100/75→106/80, golpe 180→191, retro post-pateo 80→85, freno 200→212, giros 90/50→95/53).
   Riesgos a mirar: oscilar con pelota lejana (micro-strafes que no terminan → subir a 6°) y que el retro
   post-pateo a 85 no cruce la línea. Flags: `#7 + -DARQMIX_CENTRADO_FINO + -DARQMIX_POWER_106`.
   md5 `076BACD2C76A0199B95118EE550B4196`. Estado: banco parcial 2026-07-04 (María): **velocidad +6% "anda
   bastante bien"**; resto del checklist TASK-122 pendiente. Hallazgo del banco: el despeje se desvía siempre
   a la IZQUIERDA → #9.
   Tags: `arquero-centradofino-checkpoint-2026-07-03` (versión SOLO 5°, md5 `8C25AC3EDBA84AAC0E17A1C35E98CB0B`)
   y `arquero-centradofino-106-checkpoint-2026-07-04` (esta, 5° + potencia +6%).

9. **`central_robot2_arqueromix_recto_cortaretro`** — copia del #8 + PATEO RECTO + CORTE DE RETROCESO:
   (a) `-DARQMIX_KICK_TRIM` — el despeje se iba SIEMPRE a la izquierda (banco María 2026-07-04); PWM extra en
   la delantera IZQUIERDA (`AMIX_KICK_TRIM_PWM`=15 en el pico, escala con la rampa; sigue a la izq → subir de
   a 5, tira a la der → bajar). Mismo remedio que el `MIX_KICK_FWD_TRIM` del delantero. (b)
   `-DARQMIX_RETRO_CUT_BALL` — si mientras retrocede tras el despeje VE la pelota, corta el retroceso y vuelve
   a `esperar_quieto` (la sigue / se posiciona / despeja). NO toca el homing del GO (la pelota del centro
   siempre se ve → habría matado el homing). Knob `AMIX_RETRO_CUT_DIST_MM`=9999 (cualquier pelota; si queda
   adelantado bajar a ~800-1000). (c) `-DARQMIX_REHOME_RAPIDO` — el re-homing heredado baja 15 s → 5 s
   (`AMIX_T_REHOME_NO_BALL`) y SU retroceso corta si ve la pelota (solo el del re-homing, marca
   `s_rehome_activo`; el homing del GO nunca corta). Flags: `#8 + -DARQMIX_KICK_TRIM +
   -DARQMIX_RETRO_CUT_BALL + -DARQMIX_REHOME_RAPIDO`.
   md5 `1881F751C79507B9E70759FFEE7B39A1` (con trim=8, en titración: 0=izq, 15=der, probando 8).
   Estado: en titración de banco (TASK-123). Tag `arquero-rectocortaretro-checkpoint-2026-07-04` = la
   versión inicial (trim=15, sin rehome rápido, md5 `F1C6CA9EEA2231EE5733E232C8DCE421`).

(Tags git: `git tag -l "arquero-*checkpoint*"`. Volver a uno: `git checkout <tag>`. Los #1 y #2 son commits base,
sin tag dedicado.)

## KNOBS DE TUNING (en `amix_config.h`, cada uno bajo su `#ifdef`)
- Golpe: `AMIX_KICK_VEL_FINAL`=180 (fuerza, máx 255), `AMIX_T_PAT_ADELANTE`=450 (dur.; 550 con `-DARQMIX_KICK_FAR`).
- Anti-choque: `ARQMIX_OBST_STOP_MM`=150 (0=off). Escape al buscar: `AMIX_T_BUSCAR_AVANCE`=400 (500 con flag).
- Retrocesos por tiempo: `AMIX_T_INICIO_RETRO_FIXED`=400, `AMIX_T_ATRAS_FIXED`=1300. Re-homing: `AMIX_T_REHOME_NO_BALL`=15000.
- Centrado: `AMIX_TOL_CENTRADO_DEG`=8 (5 con `-DARQMIX_CENTRADO_FINO`; oscila→6, corrido→4). Despeje: `AMIX_TOL_KICK_DEG`=30, `AMIX_TOL_CERCANIA_MM`=250.
- Potencia global: `AMIX_POWER_SCALE`=1.0 (1.06 con `-DARQMIX_POWER_106`) — escala TODOS los PWM del camino vivo en proporción (se aplica en primitivas + `pd`, NO en `AMIX_PROP_*`).
- Pateo recto: `AMIX_KICK_TRIM_PWM`=15 (con `-DARQMIX_KICK_TRIM`) — PWM extra de M1 (del. izquierda) en el pico del golpe; sigue a la izq → subir de a 5, tira a la der → bajar. Corte retro: `AMIX_RETRO_CUT_DIST_MM`=9999 (con `-DARQMIX_RETRO_CUT_BALL`) — bajar a ~800-1000 si queda adelantado.

## TEMAS ABIERTOS (importantes)
1. **La LÍNEA no se detecta confiable en esta cancha.** Los sensores de luz de DOWN leen MUY bajo (~20-57) vs su
   calibración de verde guardada (~120-280) → la DOWN da **`lifted`=1** (cree que el robot está levantado) → IGNORA
   la línea. NO se toca la calibración del blanco (decisión María). Por eso existe el checkpoint #5 (`_retrotiempo`,
   sin línea). Para recalibrar sin cuadrado blanco: **auto-calibración por barrido en la DOWN** por el monitor USB
   (115200): `STREAM OFF` (calma el flood) → `CAL CARPET` con el robot sobre el verde (baja el `lifted`) → `CAL SAVE`;
   o `CAL AUTO ON` → pasear el robot por la línea → `CAL AUTO OFF` → `CAL SAVE`. Comando alternativo: `SENS <0..100>`
   sube la sensibilidad. Hay 2 sensores muertos (leen 0) que rompen la auto-cal. Ojo altura del anillo/iluminación.
2. **Anti-choque vs PELOTA (BLOQUEANTE, TASK-121):** `min_obstacle_mm` = `min(4 ToF + HC-SR04)`. El ultrasonido va
   alto (no ve la pelota) pero los ToF podrían verla. Confirmar en banco (telemetría `snap_min_obstacle_mm`,
   acercar SOLO la pelota) que el freno NO se dispara con la pelota; si se dispara, deshabilitar/enmascarar los ToF
   frontales en la EEPROM del TOP. Si no, el arquero no despejaría (gol en contra).

## DOCS/TAREAS
`team-tasks/2026-06-29-task-119-*` (pateo corto), `…task-120-*` (retrofreno), `…task-121-*` (anti-choque).
Journals `journal/2026-06-29-*` y `2026-07-01-*` (arquero). Marcador: el BNO de R2 falló una vez el 2026-07-01
(sin acción; `journal/2026-07-01-bno-fallo-observado-marcador.md`).

---
Con esto quiero seguir trabajando el arquero. Decime si arranco con algún checkpoint o querés que resumas el estado.
