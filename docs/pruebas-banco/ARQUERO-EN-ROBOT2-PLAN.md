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
- [ ] **F0.a Velocidades GK a régimen fiel**: `GK_PATROL_SPEED` 150→420 · `GK_LINE_RETREAT_SPEED`
      250→420 · `GK_GOTO_LINE_VY_BACK` 350→420 · clamp mínimo 420 al vx de INTERCEPT.
      (Cota: `gk_motion_speed`; el strafe validado de robot2 ya corre a ese régimen efectivo.)
- [ ] **F0.b Anti-flapping PATROL↔LINE_AVOID**:
      (1) setpoint de patrulla "apenas pisando": `GK_CROSS_TRACK_SETPOINT_MM` 0 → +40 mm
      (el anillo toca el borde con 2-4 sensores, no centrado con 6+);
      (2) histéresis del disparo: entrar a LINE_AVOID con ≥8 sensores (o penetración honda),
      salir con ≤3 + cooldown 300 ms antes de re-disparar.
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

## FASE 3 — GOTO_LINE aislado (banco con cancha)
`central_robot2_arquero` (NO slow). Robot en el centro del área mirando al frente → START →
**recto hacia atrás** hasta pisar la línea → pasa a PATROL. **Criterio: trayectoria recta
(±10 cm en 1 m), se detiene/transiciona al tocar la línea, < 3 s.**

## FASE 4 — PATROL sin pelota (el corazón del arquero)
Con F0.a+F0.b aplicados. **Criterio: oscila izq↔der (~2 s por lado) "apenas pisando" el borde
de la línea, SIN entrar a LINE_AVOID** (monitorear `[CENTRAL] ... ev=` — el flag IMMINENT_EXIT
no debe parpadear), sin rotar (heading hold con el BNO dual de robot2).
Perillas: `GK_PATROL_SPEED` (velocidad), `GK_CROSS_TRACK_SETPOINT_MM` (cuánto pisa),
período de oscilación.

## FASE 5 — LINE_AVOID provocado
Empujar el robot HACIA ADENTRO de la línea de cancha (no la del área). **Criterio: retrocede
perpendicular a la línea y retoma PATROL, sin loop de escape.**

## FASE 6 — INTERCEPT + CLEAR con pelota (depende visión calibrada)
Pelota visible moviéndose lateral → el arquero la sigue en X sobre su línea; pelota a <250 mm
→ CLEAR (empuja) → vuelve. **Criterio: sigue la pelota sin perder la línea ni flapping.**

## Monitoreo durante TODAS las fases
USB a la CENTRAL: la línea `[CENTRAL] ... down[rx crc lost rsy valid ev]` cada 500 ms.
`crc` debe quedarse en 0 (sube = batería/cableado); `valid=Y`; `ev` muestra IMMINENT_EXIT/
LIFTED/MUX_DEAD en vivo.
