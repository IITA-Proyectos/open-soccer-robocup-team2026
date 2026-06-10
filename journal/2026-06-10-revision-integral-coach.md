# 2026-06-10 — Revisión integral del repo (coach, multi-agente)

> Pedida por Gustavo. 4 revisores en paralelo (firmware CENTRAL · firmware
> TOP/DOWN/shared · documentación · proceso) + verificación ADVERSARIAL de cada
> bug P0/P1 (un agente independiente intentó refutar cada uno con código).
> ~1M tokens, 188 lecturas. Las inconsistencias de docs se corrigieron en esta
> misma sesión (commit de hoy); los bugs se REPORTAN para decisión del equipo.

## Veredicto general del coach

El proyecto está en su mejor momento de la campaña: firmware estructuralmente
sólido (gates de frescura, clamps, fallbacks documentados), journal vivo,
docs/competencia muy por encima del estándar junior, y la deuda estructural más
grande (loop del TOP a 6 Hz) se midió y resolvió HOY. **Los riesgos que quedan
a ~20 días de Incheon son: 2 bugs de integración en CENTRAL, 1 riesgo de bus en
TOP, y — el más serio — gestión de deadline de deliverables.**

## 🐛 BUGS CONFIRMADOS (verificación adversarial: "real") — a decidir con Gustavo

### B1 [P1] El freno de emergencia de main anula la respuesta a línea de la FSM — robot puede quedar CONGELADO sobre la línea
**Dónde:** `main_central.cpp:252` vs `strategy.cpp:851` (GK) y `:610` (ATK).
**Qué pasa:** el loop principal frena y hace `return` ante `imminent_exit` ANTES
de que la FSM tickee — con el MISMO predicado que usan las ramas de la FSM que
responden a la línea. Resultado: el "avanzar a despegarse" del arquero y el
LINE_AVOID del delantero son **código muerto**; la respuesta real es frenar y
esperar. Como `imminent` es por ESTADO (≥6 sensores sobre blanco), un robot que
queda parado sobre la línea **no sale nunca** (y el `return` saltea el panel
serie: el robot además se "calla"). En banco se enmascara porque lo corremos
con la mano.
**Risk-no-fix:** robot clavado sobre la línea en partido hasta lack-of-progress;
los puntos P0 #3 y FASE 5 del checklist de patrulla van a fallar confuso.
**Risk-fix:** es EL path de seguridad anti-salirse — relajarlo mal = robot afuera
de la cancha a 600 mm/s. Exige banco con línea real.
**Opciones:** (a) freno solo si la FSM no está ya en estado de escape;
(b) freno de UN tick (frenar, y al tick siguiente dejar que la FSM comande);
(c) para el rol GK, quitar el freno de main y confiar en el router por ángulo.
**Tiempo:** 2-3 h código + 1-2 h banco (colgarlo de la FASE 5 ya planificada).

### B2 [P1] `GK_START_DELAY_MS = 2000` queda compilado en COMPETENCIA
**Dónde:** `strategy.cpp:159` (comentario "para COMPETENCIA bajar a 0" — pero no
hay flag que lo haga). El arquero espera 2 s tras CADA GO del árbitro (kickoff,
goles, restarts) — contra equipos que patean al primer segundo es gol regalado.
**Fix propuesto (15 min):** `#ifdef CENTRAL_ENABLE_MANUAL_START → 2000 / #else →
0` — los envs de banco ya definen ese flag, los de competencia no. + línea en el
checklist pre-competencia.

### B3 [P1] `top_robot2` sin `TOP_BNO_TOF_DECONFLICT`: el BNO SECUNDARIO comparte `Wire` con los 4 ToF
**Dónde:** `platformio.ini` (env top_robot2) + `sensors_imu.cpp:54`.
**Qué pasa:** el deconflict que salvó a robot1 del yaw congelado solo está en
top_robot1; se omitió en robot2 con la justificación (hoy corregida en docs) de
que "el BNO está en bus aparte" — cierto para el PRIMARIO (Wire2), falso para el
SECUNDARIO. Si el secundario se congela, la fusión puede promediar un heading
muerto. Mitigante: el primario (el que manda) no sufre contención, y el banco de
hoy mostró hdg sano. **Decisión a tomar:** extender el deconflict a robot2 (con
cuidado: con el round-robin nuevo el ToF corre en CASI todas las pasadas → un
deconflict naïve dejaría el IMU sin leer; necesita diseño de slots) o bajar el
secundario a lectura de salud a 1 Hz. **Tiempo:** 1-2 h código + banco corto.

## ⚠️ P1 DE PROCESO (no son código)

- **P-1 [P0] Nadie sabe cuándo cierra el form del TDP/video** → TASK-041 (creada
  hoy, asignada a Gustavo, 1 h). Si cierra a mitad de junio, TODO el plan de
  deliverables cambia de escala de tiempo.
- **P-2 [P1] `central_robot1` (env de competencia del arquero) NO lleva
  `-DCENTRAL_FLOOR_SCALE`** — todo el tuning v3.2/v3.3 se hizo con el flag. Robot1
  volvería de reparación a los síntomas ya diagnosticados del 2026-06-09 →
  TASK-042 (creada hoy: checklist completo de re-validación de robot1).
- **P-3 [P1] PR #18 de traducción automática sin mergear** → los deliverables EN
  en main tienen 5 días de atraso. Mergear o cerrar (decisión Gustavo).
- **P-4 [P1] team-tasks desactualizado** — tareas ya resueltas en banco siguen
  `pending` (ej. TASK-202 signos cámara/BNO, vencía hoy). Pasada de 30 min de
  alguien del equipo tildando contra los journals.
- **P-5 [P1] La reparación de robot1 no estaba trackeada** → cubierta por TASK-042.

## 🔧 MEJORAS (P2, backlog honesto)

1. Estáticos del sub-FSM de PATROL (pphase/direction/reacq_dry/x_center) NO se
   resetean al re-entrar al estado (STOP→GO arranca "donde quedó"). Inocuo en
   banco, prolijo de arreglar en `transition_gk`.
2. PULSE rota a ciegas si `heading_valid` cae a mitad del pulso (hdg_err=0 →
   signo fijo +1, sin corte en vivo). Gate de 1 línea.
3. El espejo host-testeable `strategy_transitions.cpp` quedó atrás de v3.2/v3.3
   (los tests fijan transiciones que ya no existen tal cual) — el corazón del
   arquero hoy NO tiene test host que lo cubra.
4. Capa-3 del arquero (cross_track + LateralPID) quedó muerta tras v2: ~80
   líneas + constantes sin uso. Borrar o marcar MUERTO (confunde al que entra).
5. `TOF_STALE_TIMEOUT_MS=250` quedó justo con el round-robin (cada sensor se
   refresca cada ~120 ms → 2 polls fallidos = blackout fugaz). Subir a ~350 ms
   es gratis y mantiene el sentido del stale.
6. Envs de telemetría USB solo existen para robot1; el banco actual es robot2.
7. `research/in-progress/` acumula 20 archivos, ≥4 muertos desde el 10/05 con su
   tema cerrado → moverlos a completed/ con 2 líneas de cierre.
8. Ramas remotas ya mergeadas (`virginia`, `mantener-el-heading`,
   `avanzar-derecho-a-cierto-angulo`) sin borrar — ruido para el que clona.
9. Falsos positivos de pelota secuestran PATROL→INTERCEPT (1 tick de
   ball_visible basta) — mitigado en banco por GK_IGNORE_BALL; para partido,
   exigir N ticks consecutivos de pelota antes de salir (debounce).

## 📝 DOCS CORREGIDAS EN ESTA SESIÓN (14 archivos, mismo commit)

CLAUDE.md (worktrees: mergear en `soccer-main`, NO en el dir histórico que quedó
en `agente/vision` — trampa de pérdida de trabajo) · ESTADO-ACTUAL ("lee 1 ToF"
→ lee 4; HAL Sprint B → resuelto) · RUNBOOK-BANCO-INCHEON §3.2 (motores R2 ya
validados; el MOTOR_INVERT citado era el de R1) · MAPA-CONEXIONES (cámaras 9→11
bytes v2; OUT2 no es espejo, es OR) · MOTION-CONTROL-PLAN (R2 "sin calibrar" →
superado) · DOWN.md CARD-6 (margen 80→40) · ENTREVISTA-PREP ({70,70,42}→
{70,70,107} — los chicos hubieran dicho un valor viejo a los jueces) · README
(árbol de software/ con 4 carpetas inexistentes) · REFERENCIAS-POR-ROBOT
(down_robot2 SÍ existe) · MEJORAS-PENDIENTES + RUBRICA ("cero imágenes" → 4
figuras ya generadas; faltan las FOTOS) · ARQUERO-PLAN (F0.a/F0.b cerrados como
quedaron de verdad) · journal de hoy (§5 contradecía al §4) · comentarios
Wire1→Wire2 en main_top.cpp / platformio.ini / pinout_robot2.h ·
ROBOT_HAS_OTOS invertido entre pinout_robot1/2.h (define sin uso en código —
verificado — corregido como doc).

## Lo que el coach pide para esta semana (orden sugerido)

1. **TASK-041** (deadline form) — 1 h de Gustavo, manda sobre todo lo demás.
2. Decisión sobre **B1** (freno vs FSM) + **B2** (gate del delay) — 1 sesión.
3. Banco con cancha: checklist patrulla + re-apriete de pulsos + FASE 5 (que
   ahora además valida el fix de B1).
4. Pasada de team-tasks (P-4) + merge/cierre de PR #18 (P-3).
5. Fotos B1-B6 del póster — sigue siendo el cap más duro de la rúbrica.
