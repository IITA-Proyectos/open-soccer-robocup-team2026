# Plan paso-a-paso: arquero en robot2 — corregir, calibrar y validar

> **Fecha:** 2026-06-09 · **Origen:** banco de Gustavo (síntomas: círculos en GOTO_LINE,
> movimiento errático a gran velocidad al detectar la línea, inicio muy lento) + revisión
> crítica de 3 lentes (matemática del pipeline · lógica de la FSM · sensores/timing).
> **Robot:** arquero corriendo en el CUERPO de robot2 (`central_robot2_arquero`).
> Robot1 en reparación.

## Diagnóstico (resumen de la revisión)

### CAUSA #1 — CONFLICTO DE DISEÑO: patrullar SOBRE la línea dispara la huida (confianza >95%)
La FSM patrulla con **setpoint cross_track = 0** (centrado SOBRE la línea) pero
`EV_IMMINENT_EXIT` se dispara con **≥6 sensores en blanco** — que es exactamente lo que pasa
centrado sobre la línea. Resultado: `PATROL` (pisa línea) → `imminent_exit` → `LINE_AVOID`
(escape a 250 mm/s en dirección del line_angle, encima distorsionada por los pisos de PWM)
→ sale → `PATROL` → vuelve a la línea → **loop violento** = "se mueve a gran velocidad hacia
cualquier lado". No hay histéresis, y `LineStatusV2` no distingue línea del ÁREA (mi puesto)
de línea de CANCHA (peligro). Evidencia: `strategy.cpp:633,682` + `down_model.cpp` (depth≥6).

### CAUSA #2 — Velocidades GK bajo los pisos de PWM: TODA dirección no-recta sale distorsionada
Los pisos {70,70,107} (≈ 275-420 mm/s equivalentes) aplastan los comandos GK de 150-250 mm/s:
PATROL 150 → proporción 0.5:0.5:1.0 queda 1:1:1.5 (×3.7 de amplificación; ×5.4 con slow-mo);
LINE_AVOID diagonal → sale recto-para-cualquier-lado; el término ω (hasta 327°/s = 571 mm/s
en rueda) DOMINA sobre vx chico → giros. **Velocidad mínima FIEL: ~420 mm/s** (la que pone el
PWM crudo de la rueda dominante sobre su piso). El GOTO_LINE diagonal viejo (180,180) daba
CÍRCULOS por esto — ya corregido a recto-atrás 350.

### CAUSA #3 — Inicio lento: el boot del TOP tarda ~14 s
BNO ~10 s + carga de los 4 ToF ~3.4 s + 0.5 s de frescura en CENTRAL ≈ 14.5 s hasta poder
moverse. El arquero casi no usa los ToF → un env de banco sin ToF ahorra 3.4 s.

---

## FASE 0 — Fixes de escritorio (Claude, con OK de Gustavo; gate + banco después)
- [x] **GOTO_LINE recto atrás** (vx=0, vy=350) — APLICADO 2026-06-09 (orden de Gustavo).
- [x] **F0.a Velocidades GK** (cerrado DISTINTO en banco 2026-06-09/10): `GK_LINE_RETREAT_SPEED`=420 ✅
      · `GK_GOTO_LINE_VY_BACK`=420 ✅ · `GK_PATROL_SPEED` queda en **200 A PROPÓSITO** (con
      `CENTRAL_FLOOR_SCALE` el strafe bajo es fiel; más rápido el arquero se pasa de largo —
      NO subir sin banco) · clamp mínimo al vx de INTERCEPT: descartado (FLOOR_SCALE escala uniforme).
- [x] **F0.b Anti-flapping** — SUPERADO por la patrulla v3.2/v3.3 (2026-06-10): ya no hay PID de
      cross_track ni LINE_AVOID alcanzable para el GK; la respuesta a línea es por ÁNGULO
      (atrás→avanzar, costado→rebotar) con debounce+cooldown en el router. Ver FASE 4/5.
- [ ] **F0.c Slow-mo + pisos = incompatible**: documentar/avisar que `central_*_slow` con pisos
      altos distorsiona ×5.4 — para observar lento, bajar VELOCIDAD comandada manteniendo ≥régimen
      fiel, no escalar.
- [ ] **F0.d Env de banco `top_robot2_fastboot`** (sin ToF): boot ~10 s en vez de ~14.

## FASE 1 — Calibrar el anillo de línea EN LA CANCHA (equipo, 10 min)
```
pio run -e diag_down_calibracion -t upload   (placa DOWN)
pio device monitor -b 115200
```
`c` (robot sobre VERDE) → `b` (sensores sobre BLANCO) → `v` (verificar: los 32 con margin ≥40;
sospechosos → revisar con `m`) → `t` (test en vivo cruzando la línea: el conteo cambia suave)
→ `s` (GUARDAR en EEPROM). **Criterio: 32/32 sensores margin ≥ 40, conteo estable en `t`.**
⚠️ Batería cargada (>7,8 V): batería floja degrada el margen (lección 2026-06-06).

## FASE 2 — Validar el DATO de línea en el CENTRAL (5 min)
Re-flashear `down_robot2` + `diag_central_rx_all` en CENTRAL. Cruzar la línea A MANO (robot
empujado): `LINEA` debe mostrar `present=1`, `on_line` subiendo/bajando suave, `angle/escape/
cross/pen` con NÚMEROS estables (no N/A, no saltos ±30°). **Criterio: angle estable con ≥3
sensores pisando; cross_track con signo coherente (adentro + / afuera −).**

## Juez desde la PC (cuando la app del juez no funciona — agregado 2026-06-10)
Los envs de banco del arquero (`central_robot2_arquero`, `_slow`, `_patrol`) llevan
`-DCENTRAL_ENABLE_MANUAL_START`: **el monitor serie de la CENTRAL ES el juez**.
```
pio device monitor -b 115200        (en el puerto USB de la CENTRAL)
g   (o ENTER)  →  GO    el arquero arranca su delay de 2 s y corre la secuencia completa
s              →  STOP  la FSM vuelve a WAIT_START y los motores paran;
                        re-acomodás el robot y mandás 'g' de nuevo = ciclo desde cero
```
También sigue valiendo el pulsador a GND en el pin 9 de la CENTRAL (= GO), si está cableado.
⚠️ En COMPETENCIA el flag NO se define: el GO/STOP real llega de la **app del juez** por los
pines 5/6 del TOP (nivel GPIO, OR fail-safe) → snapshot → CENTRAL. Los envs de competencia
(`central_robot1`, `central_robot2`) quedan SIN este flag — arrancar sin árbitro desclasifica.

## FASE 3 — GOTO_LINE aislado (banco con cancha)
`central_robot2_arquero` (NO slow). Robot en el centro del área mirando al frente → GO
(`g` en el monitor, o app del juez) →
**recto hacia atrás** hasta pisar la línea → pasa a PATROL. **Criterio: trayectoria recta
(±10 cm en 1 m), se detiene/transiciona al tocar la línea, < 3 s.**

## FASE 4 — PATROL sin pelota — ACTUALIZADO a v3.3 "pegada a la línea" (2026-06-10)
`central_robot2_arquero_patrol` (ignora la pelota). La patrulla es SEGMENTADA y PEGADA A LA
LÍNEA: tramo lateral puro (1,2 s, ω=0) → freno → pulso(s) de frente SOLO si quedó >35° chueco
→ re-enganche si no ve la línea (retrocede suave ≤0,7 s hasta re-verla) → tramo. La línea
blanca es LA guía: línea LATERAL (<135°) → rebota el sentido; línea ATRÁS → sigue de largo.
Sub-fase visible en el panel: `GK_PATROL_MOVE/STOP/PULSE/SETTLE/REACQ`.

**Checklist de CIERRE de la patrulla (banco, 2026-06-10):**
1. **[P0]** 2 min de patrulla continua: el cuerpo **nunca** cruza la línea del área hacia el
   arco (vale rozarla con el anillo; no vale que la línea pase del centro del robot).
2. **[P0]** Sensado caído (tapar 2-3 sensores de línea, o batería ~7,6 V): tras 2 re-enganches
   vacíos **deja de retroceder** (guard anti-caminar-al-arco) y patrulla donde está.
3. **[P0]** GO arrancando ya pisando la línea / medio adentro del área → avanza a despegarse
   y patrulla afuera.
4. **[P1]** ≥4 idas y vueltas seguidas rebotando en AMBOS costados; nunca >3 tramos (~70 cm)
   en un mismo sentido sin rebote.
5. **[P1]** Cada `REACQ` dura <0,7 s y retrocede ≤15 cm; debe ser ocasional (si aparece en
   TODAS las paradas → el margen de avance quedó largo: perilla `GK_ADVANCE_MS`).
6. **[P1]** Frente contenido: nunca peor que ±45° en 2 min (±35° es lo esperable con el TOP
   a ~4 Hz — ver nota abajo; mejor que eso requiere el fix del TOP).
7. **[P1]** 3 ciclos STOP→GO (app del juez o `g`/`s`): secuencia completa cada vez + probar
   con batería recién cargada y a media carga.

## FASE 5 — Respuesta a línea por ÁNGULO (reemplaza el viejo "LINE_AVOID provocado")
LINE_AVOID quedó INALCANZABLE para el arquero (v2+; el router decide por ángulo de línea).
Probar las dos respuestas: empujar el robot HACIA su línea de atrás → **avanza** a despegarse
(~3 cm tras dejar de verla); empujarlo contra una línea LATERAL → **rebota** el sentido de
patrulla. **Criterio: ninguna respuesta es un giro violento ni lo mete al área.**

## FASE 6 — INTERCEPT + CLEAR con pelota (depende visión calibrada)
`central_robot2_arquero` (la variante SIN `-DGK_IGNORE_BALL`). Pelota visible moviéndose
lateral → el arquero la sigue en X sobre su línea (clamp por pose); pelota a <250 mm →
CLEAR (empuja hacia ADELANTE) → vuelve. **Criterio: sigue la pelota sin perder la línea ni
flapping.** ⚠️ Esperar al fix del TOP lento (abajo) para exigirle agilidad.

## ✅ RESUELTO (banco 2026-06-10 tarde): el TOP lento — snapshot de vuelta a 100 Hz
Se midió con Δ`loop=` del panel `[TOP]`: **~6 Hz → ~190.000 vueltas/s** tras el fix doble
(round-robin de ToF `a6c0366` + payload del VL53L7CX recortado con `-DVL53L7CX_DISABLE_*`).
Validado por Gustavo: hdg trackea giro a mano, ToF dinámicos, `resync=0`. En la CENTRAL,
`top[fr]` ahora debe subir ~+50 por línea de panel (antes +2) — verificarlo de paso en el
próximo banco. **Perillas re-apretables ahora que el heading llega fresco** (próximo banco
con cancha, NO sin probar): `GK_REORIENT_ENTER_DEG` 35→20 · `GK_REORIENT_SETTLE_MS` 700→400
· `GK_REORIENT_MAX_PULSES` 2→3. ⚠️ ROBOT1 hereda los fixes del TOP — A VERIFICAR al volver.

## Monitoreo durante TODAS las fases
USB a la CENTRAL: la línea `[CENTRAL] ... down[rx crc lost rsy valid ev]` cada 500 ms.
`crc` debe quedarse en 0 (sube = batería/cableado); `valid=Y`; `ev` muestra IMMINENT_EXIT/
LIFTED/MUX_DEAD en vivo.
