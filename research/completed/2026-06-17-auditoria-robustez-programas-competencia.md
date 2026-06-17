---
title: "Auditoría de robustez de los programas de competencia — bugs/inestabilidad + mejoras posibles (ciclo completo con auditor independiente)"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: análisis completado — mejoras POSIBLES, NO implementadas (cierre = banco)
tipo: research / auditoría
metodologia: "4 agentes de detección (paralelo) → análisis multi-ángulo → propuestas → 1 auditor independiente adversarial → documentación"
---

# Auditoría de robustez — programas de competencia (Incheon 2026)

> **Qué es esto.** Revisión adversarial de los binarios que van a Incheon, buscando bugs e
> inestabilidad que podrían fallar EN PARTIDO. Pasó por un ciclo completo: detección paralela →
> análisis multi-ángulo (simplicidad/coherencia/performance/robustez/timing/momento) → propuestas →
> **auditoría independiente** → este documento. **NO se implementó nada**: son MEJORAS POSIBLES;
> el cierre de todas es banco (regla no-negociable #1).

## Binarios auditados
TOP: `top_robot2_pri` (R2) / `top_robot1_pri_fastbno` (R1) · CENTRAL: `central_robot1` (arquero) /
`central_robot2` (delantero) · DOWN: `down` (R1, 2 OTOS) / `down_robot2` (R2, 0 OTOS).

## Lo que está BIEN (verificado, no tocar)
Contratos de datos coinciden exacto (`static_assert` 31 B / 16 B, packed, mismo ABI ambos lados;
desfase de versión se rechaza por tamaño). CRC-16/CCITT + resync byte-a-byte robusto. Fail-safe
TOP-muerto correcto (snapshot stale 500 ms → `motors_stop`; árbitro GPIO `INPUT_PULLDOWN` +
debounce → cable suelto = STOP). Timeouts coherentes (emisores 100 Hz, timeouts 500 ms). PID con
anti-windup + clamp + guardas NaN/overflow. Seqlock del TOP correcto (barreras `__DMB`, reader
acotado en la ISR). R2-sin-OTOS degrada limpio. Enlaces al ~16 % de utilización.

## Patrones transversales detectados
1. **Watchdog ausente en CENTRAL y DOWN** (solo TOP lo usa en competencia).
2. **R1 (el ARQUERO) corre una config inferior a R2** (el delantero) en varios puntos — siendo el rol crítico.
3. **Fail-safe de borde/línea es "fail-passive"**: ante pérdida de datos degrada a "no sé / no frena", no a "modo seguro".

---

## Hallazgos + análisis multi-ángulo + veredicto del auditor

### P-A — Watchdog ausente en CENTRAL y DOWN (P0)
**Qué.** `central_robot1/2` y `down`/`down_robot2` NO definen `*_ENABLE_WDT`. Si el loop se cuelga
(ring UART trabado, I2C del OTOS colgado en DOWN-R1), no hay recuperación: CENTRAL deja los motores
en el último PWM; DOWN deja de mandar la línea. El WDOG1 está implementado y validado en TOP.
**Multi-ángulo.** Simplicidad: alta (flag ya existe, envs `*_wdt` listos). Coherencia: alta (igual
que TOP). Robustez: caza el peor caso. Timing/momento: bueno, pero requiere validar 0 resets
espurios (el WDT se arma al final del setup, tras `otos_init` lento).
**Veredicto auditor: ACEPTAR-CON-CAMBIOS — banco PRIMERO** (30 min sin reset + hang-test que
confirme `WDOG1_WRSR`), luego promover el flag a competencia. **La de mayor valor/riesgo si pasa banco.**
→ Ya cubierto por TASK-110/112; esta auditoría lo eleva a sistémico (las 3 placas).

### P-B — R1 corre config inferior a R2 (P0/P1)
**Qué.** `top_robot1_pri_fastbno` (R1) NO trae `HCSR04_ASYNC` (corre `pulseIn` bloqueante de 12 ms →
loop a ~6 Hz cuando el US no ve eco), ni `SNAPSHOT_TIMER` (sin frescura por-sensor), ni `TOF_SCHED`.
`central_robot1` NO trae `CENTRAL_TOP_RX_BIGBUF` (ring 64 B → descarta snapshots bajo jitter → el
arquero se "congela a tirones"). Además OMEGA_SIGN/pisos de R1 sin validar en banco.
**Multi-ángulo.** Coherencia: el hallazgo más fuerte — R1 es el arquero (rol crítico) con la peor
config. Simplicidad: casi pura config. Robustez: alinear baja loop-6Hz y descarte de snapshots.
Momento: mixto — algunos flags son fail-safe puro, otros riesgosos.
**Veredicto auditor (clasificación clara):**
- **ACEPTAR sin banco (fail-safe puro):** `CENTRAL_TOP_RX_BIGBUF` (solo agranda el ring) +
  `HCSR04_ASYNC` (saca el `pulseIn`; race ISR↔loop ya cerrado con `noInterrupts()`) + `TOF_SCHED`
  (byte-equivalente con 4 ToF vivos).
- **RECHAZAR para competencia (van por banco / `top_robot1_pri_xval`):** `BNO_FREEZE_DETECT`
  (riesgo de falso-CONGELADO, no validado en R1), `SNAPSHOT_TIMER` (ISR/Serial4 no host-testeable),
  `BNO_FAST` (depende del boot-check del BNO de R1, TASK-216).

### P-C — CLEAR del arquero sin guarda de arco propio → autogol (P1)
**Qué.** `GkState::CLEAR` (strategy.cpp ~1761-1803) empuja DERECHO a la pelota sin chequear dónde
está el arco propio → en un rebote dentro del área puede meterla adentro.
**Hallazgo del auditor (clave).** El módulo **`src/shared/clear_aim.{h,cpp}` YA EXISTE**, host-testeado
(12 tests en `test/test_clear_aim/`), compila en los binarios, con fallback exacto (arco no visible →
empuje derecho idéntico a hoy). **Pero NO está cableado** en CLEAR (verificado: 0 referencias en
`src/central/`). Entonces no es "escribir lógica", es "cablear lo ya escrito".
**Multi-ángulo.** Simplicidad: alta (cablear función pura). Coherencia: alta (mismo patrón que
behind_ball). Robustez: **acá el riesgo real** — `goal_own_angle` viene de la cámara trasera, que el
equipo YA desconfía (`GK_CAMERA_ORIENT_ENABLED` gateado OFF por la "J/U" del retroceso, 2026-06-09).
El fallback protege "no veo el arco", NO "lo veo MAL" → podría empujar al eje equivocado.
**Veredicto auditor: ACEPTAR-CON-CAMBIOS** — cablear GATEADO (`#ifdef GK_CLEAR_DIRECTIONAL`, OFF por
default) y prender SOLO tras validar en banco que `goal_own_angle` de la cámara trasera es confiable
(deuda abierta desde 2026-06-09). Sin esa validación → queda fuera del binario, sin regresión.
**→ CABLEADO 2026-06-17** (gateado `-DGK_CLEAR_DIRECTIONAL`, default OFF = byte-idéntico): en
`GkState::CLEAR` (strategy.cpp) se llama `clear_aim` y, si el arco propio es visible, empuja hacia
la banda; si no, fallback exacto al empuje derecho. Env de banco: `central_robot2_arquero_cleardir_bb`
(con caja negra). `test_clear_aim` 13/13, competencia byte-idéntica. **Banco obligatorio antes de
promover: confirmar que el ángulo del arco propio (cámara trasera) no manda a despejar al lado equivocado.**

### P-D — Pérdida silenciosa del freno de borde si DOWN muere (P1)
**Qué.** `edge_now = imminent_exit && line_is_fresh()`. Si DOWN se cuelga, `line_is_fresh→false` → el
freno **nunca dispara** y el robot sigue jugando ciego de borde (fail-passive). No degrada a modo seguro.
**Multi-ángulo / veredicto auditor: SOLO-DOCUMENTAR.** Un "modo conservador" sin pose absoluta es
difícil (sin saber dónde está el borde, "no acercarse" no es accionable; solo bajar vmax global, que
penaliza ante glitches transitorios de DOWN). Meter lógica de degradación nueva en el cerebro a 13
días, con cierre 100 % banco, no compensa. **Documentar como limitación conocida.** El watchdog de
DOWN (P-A) ataca la causa raíz (que DOWN no se cuelgue) mejor que un parche en CENTRAL.

### P-E — Fail-safe de línea degrada a "no sé", no a "peligro" (P1)
**Qué.** Saturación "todo blanco" → `data_valid=0` (no frena); calib persistida de otra cancha puede
cegar/falsear. **Veredicto:** parte se mitiga recalibrando en sede (TASK-022, banco) — no es código.
La parte "saturación no emite peligro" es un tema de diseño del contrato → SOLO-DOCUMENTAR + revisar
post-Incheon (cambiar el fail-safe a "conservador" toca el wire-contract).

### P-F — freeze-detector BNO falso-CONGELADO en forcejeo (P1, R2)
**Qué.** En empuje sostenido (rotación real + heading casi-clavado) puede apagar `heading_valid` →
CENTRAL pierde el rumbo en disputa. Ya marcado "banco CRÍTICO, no validado en HW" en el repo.
**Veredicto: BANCO** (ya es una TASK conocida) — no código nuevo; validar el umbral del gyro-guard.

### P-G — `GK_START_DELAY_MS = 2000` (P2)
**Qué.** El arquero queda quieto 2 s tras el GO en cada saque (arco descubierto). El comentario dice
"bajar a 0 para competencia". **Auditor verificó:** es puro tiempo de banco (acomodar el robot), sin
razón oculta de estabilización. **Veredicto: ACEPTAR-CON-CAMBIOS — poner ~200 ms** (no 0 estricto,
para que el primer snapshot tras el GO tenga `heading_valid` para el gyro-hold del retroceso).
El cambio más barato y seguro de todos.

### P-H — `REAR_BRAKE_LEAD` no-op en competencia (P2)
**Qué.** El flag está en ambos envs pero `motors_set_rear_cut()` solo se llama en diags → el freno
anticipado de la trasera NO actúa. No rompe; contradice la intención documentada. **Veredicto:
P2 cosmético** — o cablearlo a la FSM (necesita evento fin-de-tramo, no trivial) o sacar el flag
para no confundir. Post-Incheon.

### Pendiente sin cerrar (independiente de esta auditoría)
`motors_brake()` (INA=INB=HIGH, corto del puente) — FALTA confirmar en HW que frena y no queda en
COAST. Es banco, ya documentado en el código.

---

## Recomendación priorizada (veredicto del ciclo completo)

**Antes de Incheon (alto valor / riesgo acotado):**
1. **P-A Watchdog** (CENTRAL+DOWN) — banco primero (2 tests), luego promover. Mayor valor/riesgo.
2. **P-G Delay arquero** → ~200 ms. Trivial, 1 corrida de banco.
3. **P-B subconjunto seguro** — `BIGBUF` + `HCSR04_ASYNC` + `TOF_SCHED` en R1 (fail-safe puro).

**Si sobra banco:**
4. **P-C CLEAR direccional** — cablear `clear_aim` GATEADO, solo tras validar el arco trasero en banco.

**Después de Incheon:**
5. **P-B flags riesgosos** en R1 (`FREEZE_DETECT`/`SNAPSHOT_TIMER`/`BNO_FAST`) — vía banco.
6. **P-D / P-E** (fail-safe conservador) — documentados como limitación; tocan el cerebro/contrato.
7. **P-F** (freeze-detector umbral), **P-H** (rear_brake_lead), **motors_brake COAST** — banco / cosmético.

## Honestidad central
Ninguna de estas mejoras se implementó. **Ninguna cierra en código** — todas terminan en banco o
calibración, que solo cierra el equipo humano (regla #1). El valor de esta auditoría es el MAPA
priorizado de qué llevar a banco y en qué orden, con el riesgo de cada una analizado desde varios
ángulos y validado por un auditor independiente. Las 3 "antes de Incheon" son las que más bajan el
riesgo de una falla en partido con el menor costo.
