---
title: "Auditoría multi-agente del firmware de la placa CENTRAL — bugs + mejoras (Etapa 1: análisis)"
date: 2026-06-03
author: "Claude Opus 4.8 (Anthropic) — workflow de 6 agentes + verificación adversarial"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
status: in-progress
tipo: auditoria-riesgo
robot: ambos (CENTRAL = Teensy 4.1 / Zircon Rev v15)
horizonte: "Incheon 2026"
related-files:
  - software/teensy/Soccer 2026/src/central/*
  - software/teensy/Soccer 2026/src/shared/{kinematics,pids,behind_ball,drive_straight,proto,line_view,pose_view,types}.*
related-docs:
  - research/in-progress/2026-05-29-auditoria-exhaustiva-placa-down.md  (gemela, placa DOWN)
  - journal/2026-06-03-banco-resultados-arbitro-strafe-y-bno-freeze.md
  - team-tasks/2026-06-03-task-101-banco-mitad-inferior-cinematica-y-fork-arquero.md
tags: [audit, riesgo, firmware-central, fsm, control, comunicacion, motion, pre-incheon]
---

# Auditoría del firmware CENTRAL — Etapa 1 (análisis)

> **Esto es Etapa 1: ANÁLISIS.** La Etapa 2 (decidir qué implementar) es del coach + equipo.

> **✅ ACTUALIZACIÓN Etapa 2 — 2026-06-03.** Se implementaron los ítems Tier-1
> **host-testeables y sin intervención humana** (workflow TDD + integración central,
> commit `f545810`): **#9** (helper saturante de omega, anti sign-flip), **#29**
> (anti-windup real), **#5** y **#13** (gates `data_valid` en `line_view.h`), **#25**
> (surface `resync_events`), **#15** (constante muerta) y **#17** (pines UART
> informativos). Verificado: **suite host 322 tests / 0 fallos** + `central_robot1/2`
> compilan. **PENDIENTE DE BANCO** (regla 1): #9 y #29 cambian conducta → validar que
> el robot no gira al revés / no sobrepasa → **TASK-102**. Detalle:
> `journal/2026-06-03-etapa2-tier1-fixes-central-host-tested.md`.
> El resto (Tier-2 + #1 cross-board + #21/#23 sin test host) sigue pendiente — ver
> "Recomendación para Etapa 2" abajo.

## Cómo se hizo

Workflow multi-agente (6 revisores en paralelo, uno por dimensión: `fsm`,
`motion`, `control`, `comm`, `mainloop`, `arch`) sobre el código real de
`src/central/` + los módulos `src/shared/` que CENTRAL consume. Cada hallazgo
P0/P1 (y los de baja confianza) pasó por un **verificador adversarial** que
intentó refutarlo leyendo el código. Resultado: **52 hallazgos → 46 confirmados,
6 descartados** (32 agentes, ~2M tokens). Los descartados están al final (honestidad).

## Resumen ejecutivo

**Riesgo global del firmware CENTRAL: MEDIO.** El cerebro está **bien diseñado**
(FSM modular, PIDs en librería pura host-testeable, watchdogs de freshness, fail-safe
de borde). El problema no es el diseño sino **robustez de integración**: gating de
validez incompleto, control de heading con varios modos de fallo latentes, y la capa
de motores sin calibrar.

- **0 P0** — no hay bloqueante absoluto de Incheon *dentro del firmware CENTRAL*.
  (Los P0 del robot están en HW/otras placas: cámara recalibrada, el "solo gira M1",
  el freeze del BNO del TOP.)
- **11 P1** — impacto alto en partido.
- **35 P2** — deseables / capitalizables a 2027.

**Lo que los descartados nos tranquilizan:** el verificador refutó que el freno de
borde (`EMERGENCY_LINE`/`imminent_exit`) dependa de `cross_track` → **el fail-safe de
borde SÍ funciona en build de competencia** aunque `cross_track` llegue N/A. También
refutó el "runaway por BNO congelado" en sí (el offset de heading se cancela
algebraicamente en POSITION/APPROACH/CLEAR — el daño del BNO congelado es pérdida de
referencia absoluta, no spin descontrolado).

### Los 11 P1, agrupados por tema

1. **Integridad del lazo de heading (el cluster más peligroso):**
   - #9 **Overflow de int16 en `omega_centideg_s`** → con error de rumbo >~109° el PID
     satura, `omega*100` desborda int16 y el robot **gira al revés a casi máxima
     velocidad**. Sign-flip silencioso. *(fix trivial, alto impacto)*.
   - #8 **Convención de signo de omega sin validar en HW** → si el giro físico es CW
     y el PID asume CCW, todo estado con `heading_pid` entra en runaway (acotado por
     `output_clamp`). **Keystone: hay que medir el sentido físico en banco.**
   - #6 **Fuente de heading congelada**: el loop corre PIDs de heading sobre el BNO
     del TOP, que se congela; CENTRAL tiene el OTOS local vivo y no lo usa para esto.
   - #2 **Derivative kick**: la derivada del HeadingPID se toma sobre el error → jitter
     de visión inyecta espasmos de giro.
2. **Fail-safe / degradación segura:**
   - #3 **`LINE_AVOID` con línea stale**: si DOWN cae durante el avoid, el robot
     retrocede a ciegas según un `line_angle` viejo o se traba en el estado.
   - #7 **Sin watchdog independiente de pérdida de DOWN**: perder la línea es
     silencioso y deshabilita el único fail-safe de borde; el robot sigue a velocidad
     plena sin protección de borde.
   - #11 **`motors_brake` puede ser coast, no brake** (según el chip driver): en
     `EMERGENCY_LINE` el robot podría no frenar y cruzar el borde. *(confianza baja —
     hay que identificar el driver del Zircon)*.
3. **Validez de datos:** #5 el **fallback por profundidad** del PID lateral del arquero
   no chequea `data_valid` (solo el path cross_track lo hace) → arquero strafea con dato
   inválido. Es el camino más probable en competencia.
4. **Substrato de movimiento:** #10 **sin deadzone / PWM mínimo** → ruedas a baja
   velocidad quedan stalled. **Es el "solo gira M1" del banco 2026-06-03** (ligado a TASK-101).
5. **Robustez de contrato:** #1 **`WorldSnapshot` sin `schema_version`** → si se
   reordena el layout en una worktree y no en otra (TOP vs CENTRAL), CENTRAL
   reinterpreta basura sin detectarlo. `LineStatusV2` ya tiene esta protección; el snapshot no.
6. **Robustez de visión:** #4 **sin debounce de `ball_visible`** → flicker de la cámara
   (probable en Incheon con luz distinta) hace tartamudear la FSM SEARCH↔APPROACH /
   PATROL↔INTERCEPT y resetea PIDs cada tick.

## Recomendación para Etapa 2 (qué implementar y en qué orden)

> Separado por **lo que puedo cerrar yo host-testeable (sin tu placa)** vs **lo que
> requiere tu intervención en banco**. Recordá la regla 1: aunque yo lo programe y
> pase los tests host, **una TASK de hardware la cerrás vos en banco**, no yo.

### 🔑 Paso 0 — el test que destraba el cluster de heading (tu banco, ~1-2 h)
**#8 (signo de omega).** Es la *keystone*: comandar `omega>0` conocido en lazo abierto
y filmar desde arriba (¿CCW o CW?), después cerrar el lazo con setpoint +30° y
confirmar que **converge** (no diverge). Esto valida de una sola vez #8, #32 (CLEAR) y
da sentido al fix de #9. **Ya lo ibas a hacer mañana junto con el test de velocidad del
`diag_central_arbitro_strafe`** → aprovechá esa sesión.

### Tier 1 — puedo implementar + host-testear YO, sin tu placa (vos confirmás en banco después)
Bajo riesgo, alto valor, cierre por test host-native:
- **#9** clamp del omega antes del cast a int16 (1-2 h) — **el más urgente del tier**: sign-flip catastrófico, fix trivial, test host determinista (no necesita HW para probar la matemática).
- **#1** `schema_version` en `WorldSnapshot` (2-4 h) — fail-safe simétrico con la línea; patrón ya existente → buena tarea de aprendizaje.
- **#5** gate `data_valid` en el fallback por profundidad del GK (1 h).
- **#13** gate `data_valid` en `LINE_AVOID` (host) + **#3** gate de frescura al salir de `LINE_AVOID` (lógica host; banco para cerrar).
- **#29** anti-windup real (conditional integration) en la librería de PIDs (3-4 h, host).
- **#15 / #17** consolidar constantes muertas/duplicadas (timeout, baud, pines) (1-2 h, sin HW).
- **#21 / #23 / #25** robustez/observabilidad de comm (freshness propia de la vel OTOS, contadores de link, exponer `resync_events`) (host).

### Tier 2 — requiere TU intervención (medición/decisión en banco) — yo preparo, vos validás
- **#10** deadzone / PWM mínimo → necesitás medir el PWM de arranque de cada motor (ligado al test de 600 mm/s de TASK-101).
- **#6** fuente de heading (snapshot vs OTOS) → decisión + medir el drift del OTOS.
- **#7** modo degradado al perder DOWN → vos elegís cuánto capar la velocidad.
- **#11** brake vs coast → identificar el chip driver (Enzo) + medir tiempo de frenado.
- **#2** derivative kick → re-tunear `kd` con visión real en banco.
- **#4** debounce de `ball_visible` → elegir N de frames de gracia viendo el flicker real de la cámara.

### P2
Quedan 35 P2 en el backlog de abajo (deuda transversal: telemetría de partido, FSM
duplicada `strategy_transitions`, campos del contrato no consumidos, priorización de
omega en saturación, etc.). Capitalizables a 2027; ninguno bloquea Incheon.

---

## Backlog completo (46 confirmados) — datos del workflow

> Cada ítem: severidad (corregida por el verificador) · esfuerzo · ¿requiere humano? ·
> ¿requiere testeo HW? · qué observo · riesgo-no-fix · riesgo-fix · beneficio · cómo
> evaluar. Ordenado P1 → P2, luego por dimensión.

### 1. (P1 · comm · bug) WorldSnapshot no tiene schema_version: mismatch silencioso TOP/CENTRAL si cambia el layout sin cambiar el tamano
**Esfuerzo:** 2-4 h (agregar campo, ajustar TX TOP + decode CENTRAL + static_assert + 1 test host que mete schema viejo y verifica rechazo).  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** src/central/comm_top.cpp:21 ; src/shared/types.h:92 ; src/shared/types.h:126 ; src/shared/line_view.h:26  

- **Qué observo:** comm_top.cpp:21 acepta el WORLD_SNAPSHOT validando SOLO tipo (0x60) + payload_len==sizeof(WorldSnapshot)==27. No hay schema_version en el struct (types.h:92-126). En cambio LineStatusV2 (line_view.h:26-32) y el contrato explicitamente SI validan schema_version (LSV2_SCHEMA) justo para el caso 'mismo tamano, campos reordenados'. TOP y CENTRAL se flashean en worktrees/sesiones distintas (lo dice CLAUDE.md). Si alguien reordena campos del WorldSnapshot en el TOP manteniendo 27 bytes (ej. mover ball_vx/vy, reusar 'flags' reservados), CENTRAL reinterpreta basura como pose/pelota/referee_cmd SIN detectarlo: el robot 've' pelota fantasma o cree que el partido arranco. El propio pose_view.h:5 reconoce esta clase de riesgo para Pose2D/Velocity2D pero el snapshot quedo sin proteccion.
- **Riesgo si NO se hace:** Tras una edicion de WorldSnapshot en una worktree y no en la otra, el robot corre con world model corrupto sin ninguna senal de error. Sintoma en cancha: comportamiento erratico inexplicable que no se reproduce en banco si ahi se flashearon ambas placas del mismo commit. Dificil de diagnosticar bajo presion.
- **Riesgo del fix:** Agregar 1 byte schema_version al frente de WorldSnapshot rompe el tamano (27->28) y obliga a re-flashear TOP+CENTRAL juntos y actualizar el static_assert y el TX del TOP. Si se hace mal, el link deja de validar y nada pasa el filtro (snap_fresh=N permanente).
- **Beneficio:** Fail-safe simetrico con la linea: un layout incompatible se RECHAZA (cuenta como CRC/no-frame) en vez de envenenar la estrategia. Coherente con la disciplina multi-worktree del repo.
- **Cómo evaluar que funciona:** Test host-native: construir un frame WORLD_SNAPSHOT de 27 bytes con schema_version != actual y verificar que world_model NO se actualiza (snapshot_is_fresh sigue false). Comparar contra line_view que ya tiene su test de rechazo por schema.
- **Nota del verificador adversarial:** Severidad P1 bien calibrada, no la cambio. Matiz: el gatillo requiere una edicion de layout de WorldSnapshot que mantenga exactamente 27 bytes hecha en una sola worktree/branch sin propagar a la otra. types.h vive en src/shared/ (compartido por el mismo .git entre worktrees), y el protocolo de sesion (git fetch + git log origin/main antes de tocar shared, CLAUDE.md regla 5) reduce la probabilidad: ambas placas normalmente compilan el mismo types.h. La ventana de exposicion es transitoria (divergencia de branch antes de merge). El dano potencial es alto (world model corrupto silencioso, pelota fantasma, match_running falso, dificil de diagnosticar en cancha). Esfuerzo de fix bajo (~30-60 min): agregar `uint8_t schema_version` como primer campo de WorldSnapshot, una constante WS_SCHEMA, validar en comm_top.cpp:21, y ajustar el static_assert a 28 bytes (cabe en proto 32). El patron ya existe en el repo (LineStatusV2), asi que es replicar algo conocido — buen candidato como tarea de aprendizaje capitalizable a 2027.

### 2. (P1 · control · mejora) Derivative kick en HeadingPID: la derivada se calcula sobre el error, no sobre la medicion -> patada al cambiar setpoint cada tick
**Esfuerzo:** 2-3 h codigo (derivada sobre -measurement) + re-tuning de kd en banco. Recomendado hacerlo junto con el fix de fuente de heading.  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** src/shared/pids.cpp:59 ; src/central/strategy.cpp:372 ; src/central/strategy.cpp:373 ; src/central/strategy.cpp:310  

- **Qué observo:** derivative = (error - prev_error)/dt (pids.cpp:59), con error = setpoint - measurement. En APPROACH (strategy.cpp:372) el setpoint se RECOMPUTA cada tick desde ball_angle_abs = heading + atan2(bx,by); en POSITION (310) igual con heading+goal_angle. Como el heading feedback esta congelado (ver hallazgo P0), el error cambia tick-a-tick SOLO por el movimiento del setpoint (la pelota/arco en vision, que es ruidosa). Resultado: cada vez que la vision mueve el angulo de pelota, la derivada (kd=0.5) inyecta un pico de omega proporcional al salto del setpoint dividido dt (~0.01) -> kd*salto*100 puede ser enorme. Esto es el clasico 'derivative kick': la derivada deberia tomarse sobre -measurement, no sobre el error, justamente para no patear ante cambios de setpoint.
- **Riesgo si NO se hace:** Cada salto de la deteccion de pelota (jitter de OpenMV, pelota que parpadea) produce un espasmo de giro. Combinado con vision ruidosa = robot nervioso, micro-trompos al aproximar, peor punteria de pateo.
- **Riesgo del fix:** Cambiar a derivada-sobre-medicion altera la respuesta del PID; requiere re-tunear kd. Para el caso heading hay que derivar sobre el heading sensado (no sobre el error), lo cual con heading congelado da derivada 0 (consistente con el hallazgo P0: hay que arreglar la fuente de heading primero).
- **Beneficio:** Elimina espasmos por jitter de setpoint. Control de heading suave y predecible al perseguir pelota/arco. Mejora directa de punteria.
- **Cómo evaluar que funciona:** Host-native: alimentar heading_pid_tick con measurement constante y setpoint en escalon, comparar pico de output entre derivada-sobre-error (kick presente) vs derivada-sobre-measurement (sin kick). Banco para confirmar suavidad con vision real.
- **Nota del verificador adversarial:** Dos imprecisiones del hallazgo (no lo invalidan, lo recalibran): (1) El razonamiento "el error cambia SOLO por el setpoint PORQUE el heading esta congelado" es incorrecto: en APPROACH el heading se cancela algebraicamente (error=atan2(bx,by)), asi que el hallazgo P0 del heading congelado es IRRELEVANTE para este mecanismo. El pico viene del jitter de vision via el setpoint, este congelado o no el heading. (2) El remedio textual "tomar derivada sobre -measurement" es la receta de libro PERO esta mal mapeada aca: en esta arquitectura el ruido vive en el SETPOINT (vision), no en la measurement (heading, limpio). Derivative-on-measurement mataria el pico pero tambien anularia el termino D util. El fix correcto es: bajar kd, o filtrar/low-pass ball_angle_rel antes de alimentar el setpoint, o derivar sobre una version suavizada del error. Atenuantes que bajan el blast radius: output_clamp=360 acota el omega maximo; el efecto solo muerde si kd>0 Y la vision es ruidosa. Esfuerzo de fix: bajo (~1-2h: tunear kd a 0.1-0.2 + agregar EMA al angulo de pelota), pero requiere test en hardware con vision real para validar — no se puede cerrar host-native. P1 defendible; no es P0 (no desclasifica, no bloquea encendido).

### 3. (P1 · fsm · bug) LINE_AVOID retrocede con line_angle stale: sin gate de frescura al SALIR ni en el comando
**Esfuerzo:** 1-2 h (fix en ambos LINE_AVOID + test host del world_model fresheness)  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/strategy.cpp:250-262 ; software/teensy/Soccer 2026/src/central/strategy.cpp:432-443 ; software/teensy/Soccer 2026/src/central/world_model.cpp:73  

- **Qué observo:** Se ENTRA a LINE_AVOID con `imminent_exit() && line_data_fresh()` (strategy.cpp:198,419), pero una vez adentro el comando de retroceso usa `world_model_get_line_angle_deg()` y la salida `if(!world_model_imminent_exit())` SIN volver a chequear line_data_fresh(). `imminent_exit()` y `line_angle` se leen del ultimo frame g_line (world_model.cpp:71,73) que puede estar stale si el enlace Serial1 con DOWN se cae justo durante el avoid. Resultado: si DOWN deja de mandar mientras estamos en LINE_AVOID, el robot sigue retrocediendo ciegamente segun un line_angle congelado, y la condicion de salida depende de un imminent_exit stale (si quedo true, se queda atascado retrocediendo; si quedo false, sale aunque la linea siga). Es un caso limite del 'CENTRAL ignora data_valid' pero especifico de la maquina de estados de avoid.
- **Riesgo si NO se hace:** Caida del enlace DOWN durante un avoid -> robot retrocede sin control hacia la direccion vieja o se traba en LINE_AVOID; puede salirse de cancha (lo contrario de lo que el avoid intenta) o quedar inmovil retrocediendo contra una pared.
- **Riesgo del fix:** Agregar `|| !line_data_fresh()` a la condicion de salida y, si no fresco, mandar cmd cero (frenar) en vez de retroceder. Bajo riesgo; cambia comportamiento solo cuando el dato ya no es confiable.
- **Beneficio:** Avoid degradado a 'frenar y esperar' en vez de 'retroceder a ciegas'; mucho mas seguro ante perdida de DOWN.
- **Cómo evaluar que funciona:** Host: simular line_is_fresh()=false dentro de LINE_AVOID y verificar cmd=0 + salida del estado. En banco: entrar a LINE_AVOID, desconectar Serial1 de DOWN y confirmar que el robot frena en vez de seguir retrocediendo.
- **Nota del verificador adversarial:** P1 bien calibrada: impacto alto (robot retrocede ciegamente segun vector viejo y se traba en LINE_AVOID, justo lo contrario de evitar la salida -> puede irse de cancha o quedar contra una pared). Matiz sobre probabilidad: la ventana de exposicion es estrecha (LINE_AVOID es transitorio, dura poco) y requiere que DOWN se caiga justo dentro de esa ventana; un UART intermitente por cable/conector es un riesgo real de competencia pero no constante, asi que no llega a P0. El fix es barato (~0.5-1h): agregar `|| !line_data_fresh()` a la condicion de salida en strategy.cpp:258 y :439 (salir a SEARCH/PATROL si la linea dejo de ser fresca), y opcionalmente congelar el comando de retroceso o frenar si !line_data_fresh() en vez de usar el line_angle stale. Es el mismo gate `&& line_data_fresh()` que YA existe al ENTRAR (:198/:419), solo falta replicarlo adentro -> consistencia, no diseno nuevo. Test plan en hardware: con el robot en LINE_AVOID, desconectar Serial1 (DOWN) y verificar que sale del estado y no retrocede mas alla de ~500ms. Nota: es la misma clase de bug que el 'CENTRAL ignora data_valid/frescura' pero un sitio especifico distinto (la maquina de avoid), por lo que vale como item separado.

### 4. (P1 · fsm · mejora) Sin debounce de ball_visible: chatter SEARCH<->APPROACH y PATROL<->INTERCEPT resetea PIDs cada tick
**Esfuerzo:** 3-5 h (contador de gracia en ambos roles + tests host de la maquina con secuencias de visible/no-visible + ajuste de M en banco)  
**¿Requiere tu intervención?** Sí — Eleccion del numero de frames de gracia validable solo viendo el flicker real de la camara en cancha.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/strategy.cpp:167-181 ; software/teensy/Soccer 2026/src/central/strategy.cpp:264-285 ; software/teensy/Soccer 2026/src/central/strategy.cpp:445-490  

- **Qué observo:** Todas las transiciones de pelota usan world_model_ball_visible() crudo, sin histeresis temporal ni contador de frames. Cada transicion llama transition_atk/transition_gk que hace heading_pid_reset() (strategy.cpp:171,177). Si la vision parpadea ball_visible 1/0/1/0 (tipico con golf ball naranja bajo iluminacion de cancha distinta a la del lab, ver contexto openmv), la FSM oscila SEARCH<->APPROACH (ATK) o PATROL<->INTERCEPT (GK) a 100Hz, reseteando el HeadingPID en cada flanco -> el integral nunca acumula y la derivada se corrompe -> control de rumbo degradado justo cuando hay pelota. No hay 'ball_lost timeout' (N ticks sin ver antes de abandonar APPROACH/INTERCEPT).
- **Riesgo si NO se hace:** Comportamiento erratico cerca de la pelota cuando la deteccion parpadea (lo mas probable en Incheon con luz distinta). El robot 'tartamudea' y no converge a empujar/interceptar.
- **Riesgo del fix:** Agregar un contador de frames de gracia (ej. mantener APPROACH/INTERCEPT hasta M ticks sin ver, ~5-10 a 100Hz=50-100ms) y/o no resetear el HeadingPID en transiciones de ida-vuelta del mismo par. Riesgo bajo-medio; hay que elegir M sin introducir lag perceptible.
- **Beneficio:** FSM robusta a flicker de vision; PIDs mantienen estado y convergen. Capitaliza a 2027 (patron reusable).
- **Cómo evaluar que funciona:** Host: alimentar gk/atk_decide_transition con secuencia ball_visible=1,0,1,0,... y verificar que no transita en cada flanco (requiere agregar el contador a la replica pura). Banco: registrar el state_name a 100Hz con pelota intermitente y contar transiciones/seg.
- **Nota del verificador adversarial:** P1 bien calibrada: no desclasifica (no es P0) pero degrada el juego justo cerca de la pelota, que es donde se ganan puntos, y el escenario de flicker bajo iluminacion distinta a la del lab es exactamente lo que openmv-vision-tuning marca como riesgo para Incheon. DOS matices sobre el mecanismo de dano que el hallazgo describe imperfectamente: (1) el impacto del reset del INTEGRAL es menor de lo sugerido porque HeadingPID.ki=0.05 (pids.h:28) es muy bajo y el integral esta clampeado a 50; 'el integral nunca acumula' es literal pero su efecto de control es modesto. El dano real del reset esta en el termino derivativo (kd=0.5, se anula un tick por flanco) y, mas importante, NO mencionado por el hallazgo: el chatter de ESTADOS produce chatter de COMANDO DE MOTORES (SEARCH manda vy=200 + omega=60deg/s vs APPROACH va hacia la pelota; PATROL oscila vs INTERCEPT sigue la bola). El robot alterna entre 'buscar girando' e 'ir a la pelota' a cada flanco -> tartamudea en movimiento, no solo en rumbo. Esto refuerza el hallazgo aunque atribuye el dano principal al PID en vez del comando de movimiento. (2) Esfuerzo: el fix es barato y de bajo riesgo-fix. Basta un contador de frames lost/seen (ej. requerir N>=3 frames consistentes para flanquear, o un timeout de ~50-100ms de ball_lost antes de abandonar APPROACH/INTERCEPT), centralizado en una funcion debounced (ball_visible_stable()) reutilizada por ATK y GK. Estimacion honesta: 1-2h de firmware host-testeable + test en banco con la golf ball bajo luz variable. CLAUDE.md prohibe marcar TASK de hardware como done sin test real.

### 5. (P1 · fsm · bug) Fallback por profundidad del PID lateral GK no gatea data_valid (solo el path cross_track lo hace)
**Esfuerzo:** 1 h (fix + test host de gk_lateral_pid_output con data_valid mockeado)  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** sí  
**Archivos:** software/teensy/Soccer 2026/src/central/strategy.cpp:151-165 ; software/teensy/Soccer 2026/src/central/world_model.cpp:72,83-84  

- **Qué observo:** gk_lateral_pid_output() entra si `line_data_fresh()`. El branch Capa 3 usa world_model_cross_track_valid() que SI exige data_valid==1 (world_model.cpp:83). Pero el FALLBACK por profundidad (strategy.cpp:161-164) usa world_model_get_line_depth() = lsv2_penetration_u8(g_line) SIN chequear data_valid. O sea: frame fresco + data_valid==0 + cross_track N/A -> cae al fallback y alimenta el LateralPID con una profundidad potencialmente invalida. Como el contexto ya marca que en build de competencia el cross_track puede llegar N/A (cross_track_mm solo bajo DOWN_DEBUG_SERIAL), este fallback es el camino MAS probable en Incheon, y es justo el que no valida.
- **Riesgo si NO se hace:** El arquero strafea segun una profundidad basura -> deriva lateral espuria sobre el arco, posible salida de cancha o dejar hueco. Mas probable en el firmware de competencia (sin DOWN_DEBUG_SERIAL).
- **Riesgo del fix:** Agregar `&& world_model_line_data_valid()` en el branch fallback (o devolver 0 si !data_valid). Bajo riesgo; solo silencia el lateral cuando el dato no es confiable, igual que el path cross_track.
- **Beneficio:** Consistencia: ambos caminos del PID lateral respetan data_valid; arquero no se mueve con datos invalidos.
- **Cómo evaluar que funciona:** Host: mockear line_fresh=true, cross_track_valid=false, data_valid=false -> esperar salida 0 del helper (hoy devuelve PID sobre depth). Banco: con DOWN mandando data_valid=0, confirmar que el GK no strafea.
- **Nota del verificador adversarial:** Severidad P1 bien calibrada (no es P0: no bloquea competir ni desclasifica). El riesgo esta algo sobredimensionado: el output del fallback se pondera ×0.5 (PATROL, l.461) o ×0.3 (INTERCEPT, l.480) y se SUMA a vx_patrol/vx_intercept, no es comando directo de salida. La profundidad es magnitud no-firmada con setpoint GK_LATERAL_SETPOINT_DEPTH, asi que produce un SESGO lateral espurio (arquero descentrado, hueco en el arco) mas que una salida de cancha franca; 'posible salida de cancha' es el peor caso, no el tipico. Subcaso lifted: data_valid=0 por levantado, pero ahi los motores no tienen traccion util, daño marginal. Fix barato (1-2 h): que lsv2_penetration_u8 (o get_line_depth) retorne 0 si data_valid==0, alineandolo con el resto de helpers; o que el fallback en strategy.cpp chequee world_model_line_data_valid() antes de usar depth. Test plan en HW real obligatorio: forzar data_valid=0 (calib suspect / saturacion todo-blanco) y verificar que el arquero no derive lateralmente.

### 6. (P1 · mainloop · bug) snapshot_is_fresh con my_heading congelado: el robot corre PIDs de heading con un mundo válido pero un rumbo muerto
**Esfuerzo:** 0.5-1 día (decisión de fuente de heading + cableado + banco)  
**¿Requiere tu intervención?** Sí — Decisión de diseño cross-placa sobre la fuente de heading (snapshot congelado vs OTOS con drift). Requiere medir el drift del OTOS en banco para decidir el horizonte de confianza.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** media — **documentado:** sí  
**Archivos:** software/teensy/Soccer 2026/src/central/main_central.cpp:140 ; software/teensy/Soccer 2026/src/central/world_model.cpp:48-58  

- **Qué observo:** world_model_snapshot_is_fresh() sólo mira el timestamp del último frame, no la validez del CONTENIDO. Según el contexto conocido, el heading del BNO en TOP se CONGELA (contención BNO+ToF). El TOP puede seguir mandando snapshots frescos a 100 Hz con my_heading_centideg estancado -> snapshot_is_fresh()=true, el loop entra al path normal y strategy corre HeadingPID (POSITION/APPROACH/CLEAR) realimentando un heading muerto. El loop trata 'frame fresco' como 'dato útil', que no es lo mismo. CENTRAL tiene OTOS heading local vivo (world_model_get_otos_heading_deg) pero el loop/strategy de heading sigue usando get_my_heading_deg del snapshot.
- **Riesgo si NO se hace:** HeadingPID realimenta un valor constante -> o no corrige (si ya estaba en setpoint) o aplica omega constante intentando alcanzar un target inalcanzable -> giro/spin descontrolado. En cancha = robot girando en su eje sin sentido.
- **Riesgo del fix:** Migrar las realimentaciones de heading a get_otos_heading_deg (vivo) o validar staleness del heading dentro del snapshot. Riesgo: OTOS driftea; hay que decidir referencia y manejar el drift. Cambio toca strategy, no sólo el loop.
- **Beneficio:** El robot deja de realimentar un sensor muerto; usa el heading que realmente está vivo (OTOS local).
- **Cómo evaluar que funciona:** Banco: girar el robot a mano y verificar que la fuente de heading usada por strategy responde (no congelada). Host-native: validar la lógica de selección de fuente con stubs.
- **Nota del verificador adversarial:** El MECANISMO es real (gate de freshness por timestamp, no por validez; heading-hold atado al BNO/TOP que se congela en vez del OTOS local vivo) y P1 esta bien calibrado. PERO la descripcion del riesgo exagera el "spin descontrolado en su eje". Detalle de la matematica del PID (pids.cpp:38-68): en POSITION/APPROACH/CLEAR el setpoint es (heading_congelado + angulo_relativo) y el current es el MISMO heading_congelado, asi que error = wrap_diff(setpoint, current) = angulo_relativo, que SI es vivo (viene de la vision/ball_angle del snapshot fresco). El frozen se cancela -> el robot sigue girando hacia la pelota/arco, no entra en runaway. El dano real es mas sutil pero serio: (1) se pierde la referencia de heading ABSOLUTO -> el lazo queda open-loop en rumbo absoluto, la rotacion fisica que el PID comanda nunca se 've' (el heading no se mueve), (2) el termino D no puede amortiguar la rotacion real (deriva la diferencia de un valor congelado -> ~0) -> tendencia a overshoot/oscilacion, no spin puro, (3) drift de rumbo acumulado e invisible. KICKOFF no spina (fallback no-OTOS pone omega=0; path OTOS usa OTOS heading). Fix ya anotado en el journal: heading-hold del arquero con OTOS (vivo y local en CENTRAL, get_otos_heading_deg + otos_is_fresh ya existen). Esfuerzo: bajo-medio (cablear strategy a OTOS heading en vez de my_heading donde corresponda, mas decidir fallback cuando OTOS tampoco este fresco/patine -slip llego a 17 en banco-). Plan de prueba en banco obligatorio (regla 1): no se cierra por compilar.

### 7. (P1 · mainloop · bug) No hay watchdog independiente de pérdida de DOWN — perder la línea pasa silencioso y deshabilita el único fail-safe de borde
**Esfuerzo:** 2-4 h (modo degradado + LED + cap de velocidad) + banco  
**¿Requiere tu intervención?** Sí — Decisión de diseño: cuánto capar la velocidad al perder DOWN (trade-off jugar vs salir de cancha). Banco para validar el cap.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/main_central.cpp:127-149 ; software/teensy/Soccer 2026/src/central/world_model.cpp:52-54  

- **Qué observo:** El loop sólo reacciona a snapshot stale (TOP) con motors_stop. La frescura de DOWN (line_is_fresh) se usa únicamente como AND dentro de la guarda EMERGENCY (línea 127): si DOWN cae, world_model_line_is_fresh() pasa a false y la condición de brake nunca se cumple -> el robot SIGUE corriendo strategy normalmente, pero ya NO tiene detección de borde alguna. No hay degradación de modo ni reducción de velocidad ni alerta operativa al perder DOWN. El header del archivo (línea 13) dice 'Si ABAJO timeout 500 ms -> strategy ignora línea (modo ciego de borde)' pero ese 'modo ciego' no baja velocidad ni cambia comportamiento: simplemente corre a ciegas hacia la línea.
- **Riesgo si NO se hace:** Si el cable/placa DOWN falla en partido (lo más probable dado que es el bus de emergencia), el robot pierde toda protección de borde y sigue jugando a velocidad plena hasta salirse de cancha. Falla silenciosa = el equipo no se entera hasta ver el out.
- **Riesgo del fix:** Agregar un modo degradado al perder DOWN: capar velocidad (p.ej. saturar cmd a una fracción) y/o LED de alerta distinto. Riesgo: capar de más vuelve al robot inútil; hay que elegir umbral. No frenar del todo (perder DOWN no debería sacar al robot del juego).
- **Beneficio:** Falla de DOWN deja de ser silenciosa; el robot se vuelve conservador en vez de ciego-a-velocidad-plena. Capitalizable para 2027 como patrón de degradación graceful.
- **Cómo evaluar que funciona:** Banco: con TOP vivo, desconectar Serial1 (DOWN) y verificar (1) LED de alerta cambia, (2) cmd de strategy queda capado a la fracción elegida. Host-native: stub line_fresh=false, snapshot_fresh=true -> verificar que el cap se aplica al MotorCommand.
- **Nota del verificador adversarial:** El hallazgo es correcto en lo sustantivo pero exagera levemente en 'no hay alerta operativa': SI existe telemetria de debug por Serial cada 500ms (main_central.cpp:173-186: line_fresh=N, down[rx/crc/lost]). Es print de banco por USB, no una alerta en cancha — el LED de estado queda HIGH igual con DOWN vivo o caido (linea 147), asi que visualmente no se distingue. La objecion central se sostiene. Matiz de diseno: el AND con line_is_fresh es intencional (evita frenos falsos por frame stale); el defecto NO es ese AND sino la falta de un modo degradado cuando DOWN esta caido sostenido. Severidad P1 bien calibrada para el frame del repo (impacto alto en partido, no desclasifica de por si). Esfuerzo de fix razonable: agregar un watchdog de DOWN que, si line no esta fresh >500ms con match corriendo, reduzca velocidad maxima o pare motores + senal visual distinta — estimable en pocas horas de firmware mas test en hardware real (no cerrable por Claude). Verificacion limitada al firmware CENTRAL; no validado en hardware.

### 8. (P1 · motion · bug) Convencion de omega ambigua: header dice CCW, fisica puede dar CW -> riesgo de runaway
**Esfuerzo:** 0.5 dia banco (medir sentido fisico) + 1 h fijar signo + test.  
**¿Requiere tu intervención?** Sí — Banco: comandar +omega chico y confirmar si el chasis gira CCW o CW visto desde arriba. Decision: si es CW, negar el termino omega o el signo del PID. Depende de tener resuelta la inversion por-motor primero.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** media — **documentado:** sí  
**Archivos:** src/shared/kinematics.h:6 ; src/shared/kinematics.cpp:12 ; src/central/strategy.cpp:520 ; src/shared/pids.cpp:38  

- **Qué observo:** kinematics.h:6 documenta 'omega positivo = CCW visto desde arriba' y la formula suma +omega*R a las 3 ruedas (kinematics.cpp:14). Si el signo de giro fisico real (que depende del montaje de las ruedas + la inversion por-motor del hallazgo anterior) resulta ser HORARIO para +omega, entonces el HeadingPID (que asume que +omega corrige el error en una direccion, pids.cpp:38) realimenta con signo invertido -> el lazo es positivo en vez de negativo -> runaway: el robot gira cada vez mas rapido alejandose del setpoint. goalkeeper_tick CLEAR (strategy.cpp:520-523) usa heading_pid sin validacion en HW.
- **Riesgo si NO se hace:** Si el signo esta invertido, todo estado que use heading_pid (ATK POSITION/APPROACH/KICKOFF, GK CLEAR) entra en runaway de giro -> el robot trompa sin control. No se ve en banco de rotacion pura abierta (test_rotacion_pura) porque ahi no hay lazo cerrado.
- **Riesgo del fix:** Bajo si el fix es un solo signo global (negar omega en la formula o en la conversion). Riesgo: cambiar el signo sin medir y empeorarlo -> obligatorio banco.
- **Beneficio:** Lazo de heading estable y convergente. Sin esto el control de orientacion es una loteria.
- **Cómo evaluar que funciona:** Banco con lazo cerrado: setear target_heading = actual+30 grados y verificar que el robot CONVERGE (no diverge). Criterio medible: error de heading decrece monotono hasta < tolerancia. Host-native verifica la algebra de signos de la formula, no el sentido fisico real.
- **Nota del verificador adversarial:** P1 correcto pero recalibrar la narrativa antes de pasarlo al equipo: el hallazgo atribuye el riesgo a "la inversion por-motor del hallazgo anterior", y eso NO existe en el path de produccion (apply_pwm_to_motor usa polaridad uniforme). La causa verdadera del riesgo es doble y esta documentada en el journal: (a) la geometria de ruedas {60,-60,180} esta sin calibrar (Enzo, TASK-101), y (b) el signo del yaw que alimenta el HeadingPID viene del BNO del TOP y no esta definido como CCW+ en ningun lado de CENTRAL. Riesgo no exagerado: si el signo end-to-end esta invertido, TODO estado con heading_pid (ATK POSITION/APPROACH/KICKOFF + GK CLEAR) entra en runaway de giro. Mitigante real que reduce severidad de P0 a P1: output_clamp en pids.cpp:66 limita |omega| a output_clamp, asi que el "runaway" no es velocidad infinita sino giro sostenido al maximo permitido alejandose del setpoint (trompo a velocidad acotada, recuperable cortando match). Esfuerzo de validacion bajo (~1-2h banco, no rediseno): test plan = comandar omega>0 conocido en open-loop y filmar desde arriba para confirmar CCW; luego cerrar el lazo con setpoint a +30 grados y verificar que converge en vez de divergir. Es exactamente el tipo de test que el banco del 2026-06-03 todavia no hizo (solo probo lateral, no rotacion en lazo cerrado).

### 9. (P1 · motion · bug) Overflow de int16 en omega_centideg_s -> giro al reves a casi velocidad maxima
**Esfuerzo:** 1-2 h (clamp + test host que cubra omega saturado y verifique que no envuelve).  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** src/shared/pids.h:42 ; src/central/strategy.cpp:327 ; src/central/strategy.cpp:386 ; src/central/strategy.cpp:523 ; src/central/motors_zircon.cpp:115 ; src/shared/types.h:62  

- **Qué observo:** El HeadingPID tiene output_clamp=360.0 (grados/s, pids.h:42). En strategy la salida del PID se mete al comando con cmd.omega_centideg_s = static_cast<int16_t>(omega * 100.0f) (strategy.cpp:327/386/523). Pero omega_centideg_s es int16_t (types.h:62), rango +-32767. Cuando el error de heading es grande y el PID satura cerca de +360 deg/s, omega*100 = 36000, que NO entra en int16: 36000 se envuelve a 36000-65536 = -29536, o sea -295.36 deg/s. motors_zircon.cpp:115 lo reconvierte a rad/s y se lo pasa a la cinematica tal cual. Resultado: justo cuando el robot necesita girar fuerte para corregir rumbo (error >~ 92 grados con kp=3), gira para el LADO CONTRARIO a casi velocidad maxima. Es un sign-flip silencioso que solo aparece con errores grandes, no en banco de errores chicos.
- **Riesgo si NO se hace:** En cancha, cuando el robot pierde el rumbo (choque, perdida de pelota, reorientacion grande) el lazo de heading se invierte y el robot trompa girando al reves a maxima velocidad angular -> se va contra la pared o el rival, posible auto-gol o exclusion por juego peligroso. Intermitente y dificil de diagnosticar en vivo.
- **Riesgo del fix:** Bajisimo. Es clampear el omega en grados/s a +-327 antes de *100, o bajar output_clamp a 300, o castear con saturacion. No cambia el comportamiento en el rango normal.
- **Beneficio:** Elimina un sign-flip catastrofico del lazo de heading. Hace el PID seguro para cualquier error de rumbo.
- **Cómo evaluar que funciona:** Test host-native: forzar HeadingPID a saturar (error 180 grados, kp=3, dt) y verificar que la conversion a int16 nunca cambia de signo respecto del float (assert sign(int16)==sign(omega) y |int16/100| <= 327). Hoy ese caso NO esta cubierto en test_pids ni test_central_motion.
- **Nota del verificador adversarial:** Dos imprecisiones del hallazgo, no invalidantes: (1) El umbral real de DESBORDE no es error>~92 deg. El int16 recien desborda cuando omega>327.67 deg/s, o sea error>=~109 deg con kp=3 (la saturacion del PID arranca a error~120 deg con solo P; el integral clampeado a 50 aporta apenas 2.5). La banda que realmente desborda es omega en (327.67, 360], angosta pero alcanzable: el error de rumbo llega a 180 deg tras choque/reorientacion. (2) El mecanismo citado (36000 'envuelve' a 36000-65536=-29536) es en rigor COMPORTAMIENTO INDEFINIDO de C++ para float->int fuera de rango, no wrap modular definido. PERO en el target real (Teensy 4.x / GCC / ARM Cortex-M7) el camino tipico float->int32(36000)->truncar 16 bits bajos (0x8CA0) da exactamente -29536 (~-295 deg/s) con sign-flip, asi que el resultado concreto que cita es plausible/correcto en el hardware aunque la razon este floja. Severidad P1 bien calibrada: peligroso (giro invertido a alta velocidad en recuperacion de rumbo, riesgo de pared/rival/auto-gol) pero intermitente y solo en banda alta de saturacion; no bloquea competir (no P0) ni es mero deseable (no P2). Fix barato: clampear omega_deg_s a +/-327 antes del cast, o saturar tras *100 a INT16_MAX/MIN. Bajar output_clamp del HeadingPID a 320 (sigue siendo velocidad angular alta) lo resuelve de raiz para los 3 sitios. Plan de prueba HW: en banco, forzar setpoint con error >120 deg (girar el robot 150 deg respecto al target) y confirmar que gira hacia el lado corto y no se invierte.

### 10. (P1 · motion · bug) Sin deadzone / PWM minimo: ruedas a baja velocidad quedan stalled
**Esfuerzo:** 0.5 dia codigo + 0.5 dia banco para medir el PWM minimo de arranque por motor.  
**¿Requiere tu intervención?** Sí — Medir en banco el PWM minimo al que cada rueda arranca a girar (puede variar por motor/friccion). Sin ese numero el PWM_MIN es adivinanza.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** alta — **documentado:** sí  
**Archivos:** src/shared/kinematics.cpp:19 ; src/shared/kinematics.cpp:24 ; src/central/motors_zircon.cpp:124  

- **Qué observo:** wheel_speed_to_pwm hace pwm = (speed/max)*255 y trunca con static_cast<int> (kinematics.cpp:24, redondeo hacia cero). No hay compensacion de deadzone: cualquier velocidad de rueda por debajo de ~50-80 mm/s mapea a PWM <13-20, que en motores DC con friccion estatica NO arranca (zumba y no gira). Esto es exactamente el sintoma documentado 'solo gira el motor 1' en lateral: a vx=150 con MAX_SPEED=1000 el PWM de M2 queda ~13% y la rueda stalled. El truncado hacia cero ademas hace que speed que da pwm_f=0.99 -> 0.
- **Riesgo si NO se hace:** Maniobras finas (centrado de arquero, ajuste de posicion, orbit lento) no se ejecutan: el robot zumba pero no se mueve, o se mueve solo con las ruedas que casualmente quedaron arriba del umbral -> trayectoria distorsionada. En partido se traduce en posicionamiento impreciso y arquero que no llega a centrarse.
- **Riesgo del fix:** Medio: agregar un PWM_MIN de arranque (ej. mapear [eps..max] a [PWM_MIN..255] cuando speed!=0) puede generar saltos/jitter cerca de cero si PWM_MIN esta mal calibrado. Hay que medir el umbral real de arranque de CADA motor (pueden diferir).
- **Beneficio:** Movimiento parejo y predecible a baja velocidad; habilita control fino del arquero y del orbit del delantero. Capitaliza a 2027 como tabla de deadzone por motor.
- **Cómo evaluar que funciona:** Banco: comandar vx en rampa de 0 a 300 mm/s y registrar a que comando cada rueda EMPIEZA a girar (tacometro/observacion). Criterio: con el fix, las 3 ruedas arrancan por debajo de un vx objetivo (ej. 120 mm/s) en lugar de quedar stalled. Host-native solo verifica la formula del mapeo deadzone, no el arranque fisico.
- **Nota del verificador adversarial:** Dos correcciones de precision al hallazgo, sin invalidarlo: (1) Las citas de linea tienen un desfase menor: la multiplicacion (speed/max)*255 esta en kinematics.cpp:21, no en :24; la :24 es el return static_cast<int>(pwm_f) que efectivamente es el truncado hacia cero. Ambas lineas existen y dicen lo afirmado. (2) Atribucion causal: el propio journal de banco NO da por cerrada la causa raiz del 'solo gira M1' como puramente deadzone. Linea 60-62 deja explicito el experimento para discriminar: subir a 600 mm/s (~52% PWM); si M2 sigue muerto, la causa es polaridad INA/INB invertida de M2, no deadzone. O sea: la ausencia de deadzone es un defecto de codigo REAL e independiente, pero NO esta confirmado en hardware que sea LA causa del sintoma observado (podria coexistir con un problema electrico de M2). El fix de deadzone es barato (~2-4h: agregar MIN_PWM en config y un piso en wheel_speed_to_pwm tipo 'si 0<|pwm|<MIN_PWM -> sign*MIN_PWM'), bajo riesgo (risk-fix: un MIN_PWM mal calibrado mete jitter/zumbido cerca de v=0; mitigable con epsilon). Severidad P1 correcta aunque en el limite con P2: el robot SI compite y se mueve a velocidades de juego (vx>=600 da ~52% PWM), no es bloqueante para Incheon (no es P0); pero degrada posicionamiento fino del arquero, que es justamente el rol de la CENTRAL, asi que P1 es defendible. Plan de prueba en hardware obligatorio antes de cerrar: correr diag_central_arbitro_strafe a 150 y 600 mm/s con y sin MIN_PWM y medir cual rueda arranca.

### 11. (P1 · motion · bug) motors_brake asume freno activo con PWM=0; en drivers tipo VNH eso es coast, no brake
**Esfuerzo:** 0.5 dia: identificar el chip driver (datasheet) + ajustar la secuencia de brake + banco.  
**¿Requiere tu intervención?** Sí — Identificar el integrado driver del Zircon Rev v15 (Enzo/datasheet) y medir en banco el tiempo de frenado real con motors_brake vs el modo correcto del chip. Decision de diseno depende del HW.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** baja — **documentado:** NUEVO  
**Archivos:** src/central/motors_zircon.cpp:151 ; src/central/motors_zircon.cpp:157 ; src/central/motors_zircon.h:24 ; src/central/main_central.cpp:128  

- **Qué observo:** motors_brake (motors_zircon.cpp:151-160) pone INA=INB=HIGH y analogWrite(PWM, 0), comentado como 'corto interno en el H-bridge' = freno activo. Esto es valido SOLO en drivers donde PWM habilita el corto (ej. TB6612 con PWM=enable, INA=INB=1 -> short brake). En drivers tipo VNH2/VNH5/BTS donde el PWM es el enable del puente, PWM=0 DESHABILITA las dos ramas -> el motor queda en coast (alta impedancia), exactamente lo contrario de freno. El firmware llama 'H-bridge' generico y no fija el chip (config_central.h y comentarios solo dicen 'driver del motor'/'H-bridge'). main_central.cpp:128 usa esto en EMERGENCY_LINE esperando frenado <15ms.
- **Riesgo si NO se hace:** Si el driver real es VNH-style, en EMERGENCY_LINE (linea de borde detectada) el robot NO frena: coastea y cruza la linea de salida -> penalizacion/exclusion. El comportamiento creido ('freno rapido') no ocurre y nadie lo nota hasta que el robot se va de cancha.
- **Riesgo del fix:** Bajo en codigo (depende del chip: para freno real puede requerir PWM!=0 con ambos IN en el mismo nivel, o usar la modalidad de brake del chip). Requiere saber el driver exacto.
- **Beneficio:** Frenado de emergencia que realmente frena. Critico para la regla de no salirse de cancha.
- **Cómo evaluar que funciona:** Banco: motor girando, llamar motors_brake, medir tiempo hasta detencion (cronometro/encoder). Criterio: se detiene en <100ms (no coastea por segundos). Comparar contra motors_stop para confirmar que brake frena mas rapido. No es host-testeable (semantica del silicio).
- **Nota del verificador adversarial:** Recalibro P2 -> P1: EMERGENCY_LINE es la defensa anti-salida-de-cancha; coast en vez de brake = robot cruza la linea de borde = penalizacion en partido (impacto alto en partidos = P1 segun rubro CLAUDE.md). No es P0 (no impide competir ni desclasifica de entrada; es por evento, recuperable). Correccion al hallazgo: el ejemplo que da de 'brake valido' (TB6612 con INA=INB=1) en realidad necesita PWM ALTO para short-brake; con PWM=0 el TB6612 tambien coastea. O sea el problema es aun mas general de lo que dice. Esfuerzo de diagnostico BAJO: test de banco midiendo distancia/tiempo de parada brake vs stop con bateria (no USB). Riesgo no-fix: alto (silencioso, nadie lo nota hasta que el robot se va de cancha). El plan de prueba existente (journal 2026-05-15 'pisar linea a mano, ver brake') NO distingue brake de coast — solo verifica que la logica dispara. Para cerrar hace falta medir el frenado real, y antes identificar el chip del Zircon Rev v15 (Zircon.pdf, 2024-08-05).

### 12. (P2 · arch · mejora) Ausencia de telemetria de partido: el unico log es Serial USB cada 500 ms (inutil sin cable en cancha)
**Esfuerzo:** 1-2 dias (ring-buffer de eventos + dump por USB; SD opcional).  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** media — **documentado:** NUEVO  
**Archivos:** src/central/main_central.cpp:151-191  

- **Qué observo:** Toda la observabilidad de CENTRAL es Serial.print por USB cada 500 ms (loop, state, freshness, contadores de link). En partido el robot no tiene el USB conectado, asi que NO queda registro de que paso: por que entro a LINE_AVOID, cuantos frames se perdieron, si el snapshot quedo stale, etc. La estrategia multi-temporada del repo ('captura sistematica de aprendizajes' Incheon 2026) depende de poder reconstruir que paso, y hoy no hay buffer en RAM/SD ni telemetria por el link.
- **Riesgo si NO se hace:** Despues de cada partido en Incheon no hay datos para diagnosticar fallas (perdida de link, stale, transiciones raras). El aprendizaje queda en folklore, justo lo que el repo busca evitar. Costo de oportunidad alto dado el objetivo declarado 'inversion en aprendizaje'.
- **Riesgo del fix:** Medio: agregar un ring-buffer en RAM con eventos clave (transiciones FSM, freshness drops, frames_lost) volcable post-partido por USB, o a la SD del Teensy 4.1. No toca el camino de control si se hace no-bloqueante.
- **Beneficio:** Caja negra del partido = aprendizaje documentado real, reusable y diferenciador para jueces (rubrica) y para 2027.
- **Cómo evaluar que funciona:** Host-native: testear el ring-buffer (push/overflow/dump) sin placa. Banco: provocar perdida de link y un LINE_AVOID, luego volcar y verificar que los eventos quedaron registrados con timestamp.

### 13. (P2 · arch · bug) LINE_AVOID usa line_angle sin chequear data_valid -> retroceso hacia direccion arbitraria
**Esfuerzo:** 2-4 horas (guard + fallback + test host).  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** alta — **documentado:** sí  
**Archivos:** src/central/strategy.cpp:253-257 ; src/central/strategy.cpp:434-438 ; src/shared/line_view.h:53-56 ; src/central/world_model.cpp:71  

- **Qué observo:** En ambos LINE_AVOID se hace 'line_angle = world_model_get_line_angle_deg(); retreat = line_angle + 180'. Ese accessor delega en lsv2_line_angle_deg(), que SOLO chequea el centinela N/A (LSV2_NA_I16) pero NO la compuerta maestra data_valid. La entrada al estado si valida (imminent_exit honra data_valid+lifted), pero una vez DENTRO, si en un tick posterior data_valid cae a 0 con line_angle_centideg != N/A (basura residual), el robot retrocede hacia un angulo arbitrario en vez de alejarse de la linea. Confirmado en el contexto (auditoria DOWN P0.4) como patron, y aqui localizado a las dos lineas exactas.
- **Riesgo si NO se hace:** El robot puede 'huir' hacia afuera de la cancha justo cuando intentaba evitar la salida (salida de campo = penalizacion RCJ). Peor con frames degradados por ruido de motor cerca del borde.
- **Riesgo del fix:** Bajo: gatear las dos lecturas con world_model_line_data_valid() y, si invalido, usar un retroceso de fallback (p.ej. -Y robot) o congelar el ultimo angulo valido. Riesgo de elegir mal el fallback.
- **Beneficio:** LINE_AVOID robusto ante frames invalidos; respeta la 'regla maestra' del contrato que el resto del codigo ya honra.
- **Cómo evaluar que funciona:** Host-native: construir LineStatusV2 con data_valid=0 y line_angle_centideg=basura, llamar a la logica de LINE_AVOID y verificar que el vector de retroceso cae al fallback definido y no al angulo basura. Banco: forzar lifted/invalido en el borde y observar que no sale de cancha.
- **Nota del verificador adversarial:** Severidad bajada de P1 a P2. Razon: el bug se autolimita a un solo tick de control porque el check de salida del estado (if !imminent_exit -> transition) tambien gatea data_valid y dispara en el mismo tick que el angulo se vuelve no confiable. Un solo comando de velocidad espurio en ~5-10ms no saca al robot de la cancha. Sigue valiendo arreglarlo (consistencia con lsv2_cross_track_mm + defensa en profundidad): fix trivial de <0.5h -- gatear data_valid en lsv2_line_angle_deg devolviendo 0.0f, o guardar la computacion de retreat con world_model_line_data_valid() antes de usar el angulo. Matiz adicional no cubierto por el hallazgo: el camino de staleness real (DOWN deja de mandar pero ultimo frame tenia data_valid==1) NO esta protegido por line_data_fresh() DENTRO del estado -- el check in-state usa imminent_exit() que no chequea frescura -- ese seria un riesgo de retroceso sostenido pero el angulo ahi provino de un frame que SI fue valido, distinto al escenario de 'basura residual con data_valid==0' que plantea el hallazgo. Plan de prueba HW: en banco con DOWN, entrar a LINE_AVOID y forzar data_valid=0 manteniendo line_angle_centideg != N/A; loguear g_state_name y cmd.vx/vy por tick; confirmar que solo 1 tick lleva velocidad no-cero antes de pasar a SEARCH/PATROL.

### 14. (P2 · arch · bug) Firmware CENTRAL duplicado y desactualizado en central-board-pack/firmware
**Esfuerzo:** 2-3 horas (decidir politica, borrar o convertir en mirror generado, actualizar FUENTES-DE-VERDAD.md en el mismo commit).  
**¿Requiere tu intervención?** Sí — Decision: el pack autocontenido sigue siendo el metodo para programar la placa o se centraliza en src/? Lo define Gustavo/Enzo segun como flashean la CENTRAL.  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** hardware/electronics/central-board-pack/firmware/central/main_central.cpp ; hardware/electronics/central-board-pack/firmware/central/strategy.cpp ; software/teensy/Soccer 2026/src/central/strategy.cpp  

- **Qué observo:** Existe una copia completa del firmware de CENTRAL bajo hardware/electronics/central-board-pack/firmware/ ('pack autocontenido para programar la placa', ultimo commit 2026-05-24). El strategy.cpp del pack tiene 465 lineas vs 553 en src/central/ (~88 lineas / 9 dias de divergencia: le falta toda la logica WP-3-GK cross_track, drive_straight WP-2A, etc.). No hay symlink ni build compartido: son dos fuentes paralelas del MISMO firmware.
- **Riesgo si NO se hace:** Alguien (o un agente) flashea la placa CENTRAL desde el pack creyendo que es canonico y sube un binario viejo SIN el strafe del arquero ni el OTOS -> regresion silenciosa en cancha imposible de diagnosticar en vivo. Viola FUENTES-DE-VERDAD (dos canonicos para lo mismo).
- **Riesgo del fix:** Bajo: o se borra el pack y se documenta que el unico canonico es src/central/, o se reemplaza por un README + script que copie desde src/. Riesgo de romper el flujo de quien usa el pack para programar la placa standalone.
- **Beneficio:** Una sola fuente de verdad del firmware; elimina la clase de bug 'flasheamos la version vieja'.
- **Cómo evaluar que funciona:** Verificacion documental: confirmar en FUENTES-DE-VERDAD.md cual es canonico y que no quedan dos arboles editables. Si se hace mirror generado, diff -r pack vs src debe dar vacio tras el script.
- **Nota del verificador adversarial:** Bajar de P1 a P2. La duplicacion fisica y la divergencia de 9 dias son ciertas y verificables (465 vs 553 lineas, falta todo WP-2A/3-GK/OTOS). Pero el hallazgo subestima los controles existentes y exagera el riesgo: (1) NO viola FUENTES-DE-VERDAD — al reves, el doc canonico (linea 33) registra el pack y declara explicitamente src/central como fuente viva y el pack como snapshot subordinado; (2) el README del pack repite la "Regla de oro" + "NO son la fuente compilable" + manda compilar contra src/central; (3) no hay platformio.ini ni mecanismo de flasheo en el pack, asi que el escenario de regresion en cancha exige copiar archivos a mano ignorando 3 warnings. El riesgo real es de mantenimiento/staleness: un agente IA lee el snapshot del 2026-05-24 creyendolo actual, o el snapshot se pudre mas con cada feature nuevo de src/. Esto es deuda de documentacion/disciplina de pack (P2 capitalizable a 2027), no un hazard de firmware P1. Es un patron deliberado y replicado en TODOS los packs (DOWN, TOP, camaras). Fix barato (<1 h): agregar nota de "ultima sync" o un check que avise cuando el snapshot diverge N commits del vivo; o regenerar el snapshot. Decidir si los packs deben re-sincronizarse periodicamente o eliminar las copias .cpp y dejar solo los docs curados.

### 15. (P2 · arch · bug) COMMAND_TIMEOUT_MS=200 definido y muerto; watchdog real usa 500 hardcoded (drift doc/codigo)
**Esfuerzo:** 1-2 horas (consolidar las 3 constantes de timeout en config, corregir banner, recompilar).  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** src/central/config_central.h:97 ; src/central/world_model.cpp:19 ; src/central/main_central.cpp:12-13 ; src/central/main_central.cpp:141  

- **Qué observo:** config_central.h declara COMMAND_TIMEOUT_MS = 200 con comentario 'si no llega un MotorCommand del TOP en este tiempo los motores se detienen', pero (a) CENTRAL no recibe MotorCommand del TOP (recibe WorldSnapshot y genera el comando local; comentario heredado del rol viejo de motor-server), y (b) la constante no se usa en ningun lado: el watchdog real esta en world_model.cpp con SNAPSHOT_TIMEOUT_MS=500 hardcoded. Ademas el banner de main_central.cpp:12-13 dice '500 ms' y config dice 200 ms: drift entre comentario y constante, y constante muerta.
- **Riesgo si NO se hace:** Confusion: un alumno cambia COMMAND_TIMEOUT_MS=200 esperando endurecer el watchdog y no pasa nada (el real es 500 en otro archivo). El '200' tampoco corresponde al rol actual de CENTRAL. Bajo impacto funcional, alto costo de confusion para 2027/equipo nuevo.
- **Riesgo del fix:** Muy bajo: borrar COMMAND_TIMEOUT_MS o renombrarlo a SNAPSHOT_TIMEOUT_MS y centralizar el 500 en config para que world_model lo incluya. Riesgo nulo si solo se consolida la constante.
- **Beneficio:** Una sola constante nombrada para el watchdog del snapshot; comentarios coherentes; menos magic numbers (tres '500' separados: snapshot, line, otos).
- **Cómo evaluar que funciona:** Host-native: cambiar la constante unica y verificar por test que world_model_snapshot_is_fresh() respeta el nuevo umbral. Revision de codigo: grep que no quede ninguna constante de timeout muerta ni '500' hardcoded.

### 16. (P2 · arch · mejora) Campos del contrato producidos por DOWN/TOP y nunca consumidos en CENTRAL (ball_vx/vy, escape_angle, quality, sample_age_ms, slip, pose_confidence, min_obstacle, partner_*, in_penalty_area)
**Esfuerzo:** Documentar/anotar: 2-3 horas. Consumir min_obstacle en APPROACH/SEARCH: 0.5-1 dia con banco.  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** sí  
**Archivos:** src/shared/types.h:104-105 ; src/shared/types.h:133 ; src/shared/types.h:139-140 ; src/central/world_model.cpp:104-108 ; src/central/strategy.cpp  

- **Qué observo:** Verificado por grep en src/central/: NO se consumen en la FSM ni en world model de decision: ball_vx_mm_s/ball_vy_mm_s (sin accessor en world_model; solo en diag y ball_trajectory shared no cableado), escape_angle_centideg/quality/sample_age_ms del LineStatusV2 (sin accessor en CENTRAL), y accessors definidos pero sin uso en strategy: get_otos_x/y, otos_omega, otos_slip, otos_pose_confidence, my_x/y, goal_opp_distance, min_obstacle, in_own_penalty_area, partner_alive, partner_sees_ball, referee_cmd, line_detected. Es superficie de contrato no usada: la FSM Nivel 1/2 es mas pobre que lo que el snapshot ya trae (p.ej. no usa min_obstacle para evitar choques, ni slip para detectar patinazo al patear, ni partner para no chocar al companero).
- **Riesgo si NO se hace:** No es bug activo, pero es deuda: campos que parecen 'soportados' no hacen nada (trampa para el equipo nuevo) y se desperdicia informacion ya disponible que mejoraria el juego (evitar obstaculos, no pisar al companero). En el peor caso alguien asume que la evasion de obstaculos existe porque min_obstacle esta 'cableado'.
- **Riesgo del fix:** Variable: consumir min_obstacle/partner es trabajo de tactica (medio); marcar el resto como reservado/futuro es trivial. Riesgo de meter logica nueva sin banco.
- **Beneficio:** Claridad de que campos estan vivos vs futuros + oportunidad concreta de mejora tactica (evasion, juego en equipo) reusable a 2027.
- **Cómo evaluar que funciona:** Documental + host-native: marcar en world_model.h cuales accessors son 'usados por FSM' vs 'futuros'. Para cualquier campo que se empiece a consumir (ej. min_obstacle), test host con snapshots sinteticos y luego banco.

### 17. (P2 · arch · mejora) Constantes UART de pines (UART_TOP_RX/TX, UART_DOWN_RX/TX) en config no se usan -> doc-como-codigo propenso a drift
**Esfuerzo:** 1 hora.  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** src/central/config_central.h:86-90 ; src/central/comm_top.cpp:34-36 ; src/central/comm_down.cpp:50-52  

- **Qué observo:** config_central.h declara UART_TOP_RX=28/UART_TOP_TX=29 y UART_DOWN_RX=0/UART_DOWN_TX=1, pero comm_top.cpp y comm_down.cpp inicializan Serial7.begin()/Serial1.begin() sin pasar esos pines (en Teensy 4.1 Serial7/Serial1 tienen pines fijos). Las constantes son documentacion disfrazada de codigo: si alguien las edita pensando que reasigna el pin, no pasa nada y queda un mapa de pines mentiroso. Ademas comm_top/comm_down definen su propio 'constexpr long UART_BAUD = 230400' local en vez de usar UART_TOP_BAUD de config (DOWN si usa UART_TOP_BAUD; CENTRAL no) -> baud duplicado en 3 lugares.
- **Riesgo si NO se hace:** Bajo impacto funcional (los Serial fijos funcionan), pero alto riesgo de confusion: el dia que cambie el cableado, editar config no cambia nada y el debug del link se va por mal camino. Baud duplicado: si se cambia en un lado y no en otro, link muerto silencioso.
- **Riesgo del fix:** Muy bajo: o se borran las constantes de pin (y se deja solo un comentario), o se documenta explicitamente que son informativas. Centralizar el baud en una sola constante usada por ambos comm.
- **Beneficio:** Mapa de pines/baud que no miente; una sola fuente del baud inter-placa.
- **Cómo evaluar que funciona:** Revision de codigo: confirmar que no quedan constantes de pin sin uso y que ambos comm leen el mismo UART_BAUD de config. Recompilar central_robot1/robot2.

### 18. (P2 · arch · mejora) EMERGENCY_LINE en main bypassa el tick de 100 Hz pero retorna sin refrescar strategy ni LED de estado real (riesgo de starvation del watchdog de snapshot)
**Esfuerzo:** 0.5-1 dia (decidir politica unica + banco de borde).  
**¿Requiere tu intervención?** Sí — Decision de diseno de seguridad: freno duro indefinido vs freno corto + retroceso. Validar en banco que el robot no sale de cancha en ningun caso.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** media — **documentado:** NUEVO  
**Archivos:** src/central/main_central.cpp:127-134 ; src/central/strategy.cpp:14-15  

- **Qué observo:** Cuando world_model_imminent_exit() && line_fresh(), main hace motors_brake(); LED HIGH; return -- en cada iteracion del loop, salteando el bloque de strategy. Mientras la condicion persista, strategy_tick NUNCA corre, asi que g_atk_state/g_gk_state quedan congelados en el estado previo (no transicionan a LINE_AVOID hasta que la emergencia baje y vuelva el tick). Es coherente como freno duro, pero crea una divergencia: la FSM (que tambien tiene su propio LINE_AVOID gatillado por imminent_exit) y el bypass de main implementan dos politicas de evasion distintas para la MISMA condicion. Si la emergencia se sostiene (robot trabado sobre la linea), el robot queda en freno indefinido sin la maniobra de retroceso de LINE_AVOID.
- **Riesgo si NO se hace:** Robot que queda 'clavado' frenado sobre la linea en vez de retroceder: pierde la pelota y eventualmente lo levanta el arbitro. Doble politica de evasion (main brake vs FSM retreat) es confuso de mantener.
- **Riesgo del fix:** Medio: unificar (p.ej. main frena 1 tick y luego deja que la FSM haga el retreat, o el bypass hace el retreat). Tocar el camino de emergencia sin banco es delicado (es seguridad).
- **Beneficio:** Una sola politica de evasion de linea, sin estado de freno indefinido; mantenibilidad.
- **Cómo evaluar que funciona:** Banco: poner el robot pisando la linea de forma sostenida y verificar que ejecuta retroceso y se aleja (no queda frenado para siempre). Host-native: verificar la maquina de estados de emergencia con imminent_exit sostenido.

### 19. (P2 · arch · mejora) referee_cmd (RESET/HALFTIME) expuesto pero nunca consumido por la FSM
**Esfuerzo:** 0.5-1 dia (logica simple de stop en halftime; re-home en reset es mas).  
**¿Requiere tu intervención?** Sí — Decision tactica: que hace el robot ante RESET (volver a area propia? quedarse quieto?) dado que no hay homing confiable. Definir con el equipo antes de implementar.  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** src/central/world_model.cpp:95 ; src/central/world_model.h:79 ; src/central/strategy.cpp:191 ; src/shared/types.h:117  

- **Qué observo:** WorldSnapshot trae referee_cmd (0=stop,1=start,2=halftime,3=reset) y world_model lo expone via world_model_referee_cmd(), pero strategy NO lo lee en ningun lado (grep en strategy.cpp: 0 usos). La FSM solo mira match_running (flags bit 3). Tras un gol el arbitro manda RESET y el robot deberia volver a posicion inicial / detenerse; con halftime deberia parar. Hoy esos comandos no tienen efecto distinto de match_running.
- **Riesgo si NO se hace:** En partido real, tras gol/medio tiempo el robot sigue en el ultimo estado en vez de re-posicionarse o parar segun el arbitro. No desclasifica pero da comportamiento incoherente y pierde tiempo de juego; ademas es un campo del contrato que parece soportado y no lo esta (deuda oculta).
- **Riesgo del fix:** Bajo-medio: agregar manejo de RESET (transicion a WAIT_START / re-home) y HALFTIME (stop). Hay que definir el comportamiento de re-posicionamiento, que no es trivial sin pose absoluta confiable.
- **Beneficio:** Robot que respeta el protocolo de arbitro = mas partido jugado y mejor impresion de jueces (rubrica Incheon).
- **Cómo evaluar que funciona:** Host-native: inyectar snapshots con referee_cmd=2 y =3 y verificar las transiciones FSM esperadas (HALFTIME->stop, RESET->WAIT_START). Banco: emular comandos de arbitro por COMM y observar reaccion.
- **Nota del verificador adversarial:** Bajo de P1 a P2. El hallazgo EXAGERA el riesgo: afirma que "tras gol/medio tiempo el robot sigue en el ultimo estado en vez de pararse", pero eso es FALSO con el hardware actual — el arbitro RCJ baja el GPIO en STOP/halftime y match_running pasa a false, llevando ambas FSM a WAIT_START (parada). Verificado en strategy.cpp:196-197 (ATK) y 417-418 (GK). La unica conducta NO implementada es RESET=re-posicionamiento a posicion inicial, pero (a) el COMM de este equipo no emite RESET por el canal de 2 niveles GPIO (comm_arbiter.cpp:40), y (b) re-posicionarse requiere pose absoluta de cancha, que el robot aun no tiene (main_top.cpp:91 marca in_own_penalty_area como pendiente Nivel 2; heading sin posicion). El valor real del hallazgo es de DEUDA OCULTA / contrato enganoso: el campo referee_cmd + el accessor world_model_referee_cmd() aparentan soportar 4 estados pero son dead code (accessor con 0 callers). Fix recomendado de bajo costo (~30 min, P2): documentar en types.h que el contrato real es binario (solo START/STOP via GPIO) y marcar referee_cmd como reservado/legacy, o borrar el accessor muerto. risk-no-fix: confunde a futuras sesiones que crean tener soporte de halftime/reset. risk-fix: nulo (es solo doc + borrado de codigo muerto). NO es P0 ni P1: no afecta el comportamiento en cancha en Incheon.

### 20. (P2 · comm · mejora) comm_top_tick / comm_down_tick drenan el UART con while(available) sin tope: un burst puede robar el ciclo de 100 Hz y la latencia de EMERGENCY_LINE
**Esfuerzo:** 2-3 h (agregar tope configurable por tick + contador de 'backlog drenado' a la telemetria) + banco para elegir el numero.  
**¿Requiere tu intervención?** Sí — Medir en banco bytes/loop y backlog del FIFO bajo carga real (3 placas + motores andando) para fijar el tope sin introducir lag.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** media — **documentado:** NUEVO  
**Archivos:** src/central/comm_top.cpp:38-49 ; src/central/comm_down.cpp:54-64 ; src/central/main_central.cpp:99-101 ; src/central/main_central.cpp:127-134  

- **Qué observo:** Ambos tick() hacen 'while (SerialN.available() > 0) read()/feed()' sin limite de bytes por iteracion (comm_top.cpp:40, comm_down.cpp:56). El loop() de main_central.cpp llama los dos ticks al principio (99-101) y recien despues evalua EMERGENCY_LINE (127) y el tick de strategy a 100 Hz (137). Si una placa manda un burst (ej. DOWN a 200 Hz con 3 tipos de mensaje multiplexados, o ruido que llena el FIFO), el while drena TODO el backlog disponible en una sola pasada antes de dejar correr el resto del loop. A 230400 baud el riesgo es acotado, pero el bus DOWN es 'de emergencia' y justamente ahi la latencia de motors_brake() importa (objetivo documentado <15 ms). No hay backpressure de RX (descartar/limitar), solo se cuentan bytes.
- **Riesgo si NO se hace:** En el peor caso (FIFO saturado por ruido o cadencia alta sostenida) el freno por borde se retrasa unos ms extra porque el loop esta atascado parseando el otro enlace. Poco probable a 230400 pero no esta acotado por diseno; es el tipo de cosa que muerde solo en cancha con cables largos y ruido de motores.
- **Riesgo del fix:** Limitar bytes/tick (ej. max 64 por enlace por iteracion) podria, si se elige mal el tope, dejar backlog creciente y aumentar la latencia media de los frames. Hay que medir bytes/loop reales antes de tunear.
- **Beneficio:** Latencia de control acotada y deterministica; el bus de emergencia no puede ser monopolizado por el otro enlace.
- **Cómo evaluar que funciona:** Banco: instrumentar bytes drenados por tick y peor latencia de EMERGENCY_LINE con un generador que envie bursts; criterio: freno <15 ms incluso con el otro enlace saturado.

### 21. (P2 · comm · bug) La frescura de la velocidad OTOS se cuelga de la pose: vel puede quedar stale mientras otos_is_fresh() devuelve true
**Esfuerzo:** 2-3 h (timestamp propio para vel + accessor otos_vel_is_fresh + ajustar strategy a chequear ambos) + 1 test host de perdida selectiva.  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** src/central/world_model.cpp:97-99 ; src/central/world_model.cpp:104-106 ; src/down/comm_top.cpp:64-82 ; src/central/strategy.cpp:222-229  

- **Qué observo:** world_model_apply_otos_vel (world_model.cpp:98) NO actualiza g_otos_last_ms; el comentario dice 'freshness va con la pose (llegan juntas a 100 Hz)'. Es verdad HOY porque comm_top.cpp del DOWN (64-82) manda pose y vel back-to-back en la misma funcion. Pero CENTRAL solo verifica world_model_otos_is_fresh() (que mira la POSE) antes de usar la VEL en drive_straight: strategy.cpp:229 usa world_model_get_otos_vx_mm_s() para cancelar deriva lateral al patear. Si por corrupcion CRC se pierde selectivamente el frame de VEL (0x12) pero pasa el de POSE (0x11) — son frames distintos con CRC independiente — otos_is_fresh() sigue true y drive_straight cancela deriva con una vx vieja/de otro instante. El acople 'llegan juntas' es una suposicion de cadencia del emisor, no una garantia del receptor.
- **Riesgo si NO se hace:** Al patear/manejar derecho con OTOS, si se cae intermitentemente el frame de velocidad, el robot corrige deriva con un dato lateral viejo -> patea/avanza ligeramente torcido sin que ningun watchdog lo note. Efecto sutil, dificil de atribuir.
- **Riesgo del fix:** Darle freshness propia a la vel (g_otos_vel_last_ms) y gatearla aparte agrega estado; si strategy no se actualiza para chequear ambas, queda una freshness que nadie consulta (deuda muerta).
- **Beneficio:** Robustez del drive-straight ante perdida selectiva de frames; elimina una suposicion implicita de cadencia del emisor en el codigo del receptor.
- **Cómo evaluar que funciona:** Test host-native: aplicar pose a t y NO aplicar vel; avanzar millis() simulado; verificar que un nuevo otos_vel_is_fresh() pasa a false aunque otos_is_fresh()(pose) siga true.

### 22. (P2 · comm · bug) CENTRAL ingiere la pose OTOS aunque confidence==0 (ambos OTOS muertos) — el control derecho confia en odometria invalida
**Esfuerzo:** 2-3 h (gate por confidence en strategy + helper world_model_otos_usable() + test host) ; decision de umbral con el equipo.  
**¿Requiere tu intervención?** Sí — Decidir umbral de confidence aceptable (solo dos OTOS=100, o tambien aceptar uno=60). Decision de diseno de control que conviene validar en banco con un OTOS desconectado a proposito.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** src/central/world_model.cpp:97 ; src/central/world_model.cpp:108 ; src/down/comm_top.cpp:68-70 ; src/central/strategy.cpp:222-228  

- **Qué observo:** DOWN setea pose.confidence = 0 cuando NINGUN OTOS responde (down/comm_top.cpp:68-70: 100 con dos, 60 con uno, 0 con cero). CENTRAL guarda la pose con world_model_apply_otos_pose (world_model.cpp:97) sin mirar confidence, y otos_is_fresh() (linea 99) solo chequea timestamp. strategy.cpp:222 entra al refinamiento drive-straight con SOLO 'if (world_model_otos_is_fresh())' — usa heading y vx del OTOS para cancelar deriva. Si ambos OTOS estan caidos, DOWN igual difunde Pose2D/Velocity2D con ceros y confidence=0; CENTRAL los toma como frescos y maneja 'derecho' contra heading=0 y vx=0 falsos. world_model_otos_pose_confidence() existe (world_model.cpp:108) pero NADIE lo consulta en el camino de control (solo aparece en su definicion). Es el mismo patron P0.4 del audit DOWN (ignorar data_valid) trasladado al OTOS.
- **Riesgo si NO se hace:** Con los 2 OTOS desconectados/colgados (cable, I2C, power), el robot cree que tiene odometria valida (fresh + datos en cero) y ejecuta drive-straight/kickoff con referencia falsa: patea o avanza en direccion equivocada en vez de degradar al fallback sin-OTOS que ya existe (strategy.cpp:237). Es un fallo de cancha plausible: los OTOS son los sensores mas fragiles del stack.
- **Riesgo del fix:** Agregar el gate de confidence (ej. otos_is_fresh() && confidence>=umbral) puede dejar fuera el caso de un solo OTOS (confidence=60) si el umbral se pone mal; hay que decidir el umbral. Riesgo bajo y reversible.
- **Beneficio:** El drive-straight degrada limpio al fallback documentado cuando la odometria no es confiable, en vez de actuar con datos basura. Cierra el agujero gemelo del data_valid de linea pero del lado OTOS.
- **Cómo evaluar que funciona:** Test host-native: inyectar Pose2D fresca con confidence=0 y verificar que el helper world_model_otos_usable() devuelve false; en banco, desconectar ambos OTOS y confirmar que CENTRAL cae al fallback sin-OTOS y no al refinamiento.
- **Nota del verificador adversarial:** Calibracion de severidad: el hallazgo propone P1, lo bajo a P2 por dos razones de impacto real. (a) La correccion del drive-straight es ADITIVA y ACOTADA: en APPROACH (393-403) solo suma una correccion lateral en +X con DS_KP_LATERAL=0.5 sobre vx medido; el heading y el avance hacia la pelota los sigue manejando vision/HeadingPID, no el OTOS. No 'patea en direccion equivocada': el kick lo gatilla is_aligned_to_shoot() con datos de vision, no OTOS. (b) Solo en KICKOFF (222-236), durante ATK_KICKOFF_DURATION_MS=250 ms, el heading target se toma del OTOS -> ahi si una pose stale puede sesgar el boost inicial, pero es una ventana de 250 ms y vx/heading stale suelen ser chicos. El riesgo de 'avanza/patea totalmente equivocado' esta exagerado; el sintoma realista es una correccion lateral espuria de baja magnitud y un boost de kickoff levemente torcido. Fix correcto y barato (~1-2 h): gate por confidence. Opcion A: en world_model_apply_otos_pose, si pose.confidence==0 no actualizar g_otos_last_ms (la pose queda 'no fresca' -> fallback automatico). Opcion B: que otos_is_fresh() (o un nuevo otos_is_valid()) exija g_otos_pose.confidence > 0. Plan de test HW: desconectar ambos OTOS (I2C), verificar en KICKOFF/APPROACH que el robot cae al fallback sin-OTOS y no aplica correccion lateral. Correccion factual al texto del hallazgo: con cero OTOS la pose queda STALE, no en cero (otos.cpp:128 'pose no se actualiza').

### 23. (P2 · comm · mejora) g_frames_lost del enlace DOWN mezcla 3 tipos de mensaje y double-cuenta los frames con CRC malo como 'perdidos'
**Esfuerzo:** 2-3 h (documentar el solapamiento o, mejor, contar gaps por-tipo y exponer crc vs gap por separado) + test host.  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** src/central/comm_down.cpp:25-42 ; src/central/comm_down.cpp:30-34 ; src/down/down_tx.cpp:22-43  

- **Qué observo:** comm_down.cpp:30-34 mide huecos de SEQ sobre el stream que llega por Serial1, pero ese enlace multiplexa LINE_URGENT (0x10) + DOWN_OTOS_POSE (0x11) + DOWN_OTOS_VEL (0x12), todos con el MISMO contador de seq por-enlace de down_tx (down_tx.cpp:25, g_links[0].seq++). El conteo de gaps es correcto en magnitud (un solo seq monotono), PERO: (a) un frame que falla CRC nunca llega a handle_frame, asi que su seq nunca se ve y aparece como hueco en el proximo frame bueno -> ese frame ya se conto en crc_errors() Y se vuelve a contar en frames_lost (doble contabilidad); (b) frames_lost no distingue que TIPO se perdio, asi que la telemetria de 'aprendizaje Incheon' no puede decir si lo que se cae es la linea (critico) o la vel (menos critico). El comentario en :27-29 vende esto como 'magnitud del hueco' sin aclarar el solapamiento con CRC.
- **Riesgo si NO se hace:** Telemetria de salud de enlace enganosa: frames_lost infla el numero real de perdidas por gap puro (suma las corrupciones CRC) y no permite atribuir la perdida a un tipo. En el journal de Incheon se sacan conclusiones erradas sobre la calidad del bus DOWN.
- **Riesgo del fix:** Separar el conteo por tipo o restar las CRC requiere mas estado por enlace; bajo riesgo funcional (es solo telemetria, no afecta control).
- **Beneficio:** Diagnostico fiel del enlace de emergencia: cuanto se pierde por gap real vs corrupcion, y de que mensaje. Justo el dato que el frame 'aprendizaje, no podio' quiere capturar.
- **Cómo evaluar que funciona:** Test host-native: alimentar una secuencia con un frame CRC-malo intercalado y verificar que NO se cuente a la vez en crc_errors y en frames_lost; o que la doc deje explicito el solapamiento.

### 24. (P2 · comm · bug) El header de world_model promete 'expone confidence=0 si TOP cae' pero no hay accessor de confidence del snapshot ni gate por confidence en strategy
**Esfuerzo:** 2-4 h (accessor + decidir donde gatear en strategy + test host) ; la decision de umbral necesita datos del TOP.  
**¿Requiere tu intervención?** Sí — Definir umbral de my_pose_confidence usable y que hace CENTRAL por debajo (fallback OTOS-local vs detener). Requiere ver que valores reales reporta la fusion del TOP en banco.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** media — **documentado:** NUEVO  
**Archivos:** src/central/world_model.h:1-6 ; src/central/world_model.cpp:38-50 ; src/shared/types.h:97 ; src/central/strategy.cpp:188 ; src/central/strategy.cpp:519-521  

- **Qué observo:** world_model.h:4-6 afirma: 'Si CENTRAL deja de recibir snapshots ... expone confidence = 0 y is_fresh()=false'. La parte is_fresh() existe (world_model.cpp:48), pero NO hay ningun accessor world_model_get_my_pose_confidence() — el campo WorldSnapshot.my_pose_confidence (types.h:97) NO se expone ni se consulta. Ademas, al caer TOP el snapshot viejo simplemente se queda 'stale' (no se cerea), main_central solo frena por !snapshot_is_fresh (main_central.cpp:140). Pero dentro del tick de strategy, world_model_get_my_heading_deg() (strategy.cpp:188, 519) se usa SIN mirar my_pose_confidence: si el TOP esta vivo pero su fusion reporta confidence bajo (BNO congelado, que es justo el caso documentado del contexto), CENTRAL usa un heading basura como si fuera bueno. El contrato del header no se cumple del lado del consumidor.
- **Riesgo si NO se hace:** Con el BNO del TOP congelado (escenario YA conocido del contexto), el snapshot llega fresh con confidence posiblemente bajo y un heading viejo; CENTRAL no tiene forma de degradar porque ni siquiera lee la confidence. El delantero apunta el kickoff/heading con rumbo equivocado. Es la contracara CENTRAL del problema BNO ya conocido.
- **Riesgo del fix:** Agregar accessor + gate por confidence puede dejar al robot 'mas ciego' si el umbral es muy estricto (descarta poses buenas). Hay que elegir umbral con datos reales del TOP.
- **Beneficio:** Cierra el contrato que el propio header promete; permite degradar a OTOS-local o modo seguro cuando la pose del TOP no es confiable, alineado con el aprendizaje ya documentado de que CENTRAL no debe depender del heading del TOP.
- **Cómo evaluar que funciona:** Test host-native: aplicar snapshot fresco con my_pose_confidence bajo y verificar que el nuevo gate marca la pose como no-usable; banco con BNO del TOP congelado para confirmar que CENTRAL degrada en vez de usar heading viejo.

### 25. (P2 · comm · mejora) El decoder de proto cuenta resync_events pero CENTRAL no lo expone: ceguera a ruido/desfasaje de byte en el link TOP y DOWN
**Esfuerzo:** 1 h (getter + columna en el debug print de main_central:166-186 para ambos enlaces).  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** src/shared/proto.cpp:54-59 ; src/shared/proto.cpp:94-99 ; src/shared/proto.h:111 ; src/central/comm_top.cpp:51-53 ; src/central/comm_down.cpp:88-90  

- **Qué observo:** FrameDecoder lleva resync_events_ (proto.h:111), que se incrementa cuando LEN>MAX_PAYLOAD (proto.cpp:56) o el END byte no es 0x55 (proto.cpp:97) — sintomas de ruido/desincronizacion de bytes en el UART. CENTRAL expone frames_received y crc_errors (comm_top.cpp:51-53, comm_down.cpp:88-90) pero NO expone resync_events. La distincion importa: muchos resync con pocos CRC errors apunta a un problema fisico distinto (byte slip, baud, GND) que muchos CRC con sync OK (ruido en el medio del payload). El panel de diag (diag_central_rx_all) si mira seq_gaps pero el firmware de competencia no tiene visibilidad de resync en ningun enlace.
- **Riesgo si NO se hace:** En cancha, ante un link inestable, el equipo ve 'crc=0 pero fr no sube' y no tiene el contador que explica por que (resync). Diagnostico mas lento de un problema de cableado/GND durante una ventana de setup corta en Incheon.
- **Riesgo del fix:** Trivial; solo agrega un getter y una columna al debug print. Riesgo nulo.
- **Beneficio:** Observabilidad completa del enlace (sync vs payload corruption) sin reflashear con diag. Acelera el bring-up en cancha.
- **Cómo evaluar que funciona:** Test host-native: alimentar al decoder un LEN invalido y un END erroneo y verificar que resync_events sube y que el nuevo getter lo refleja; smoke en banco mirando la nueva columna.

### 26. (P2 · control · mejora) Doble corte de velocidad del HeadingPID: output_clamp (360 grados/s) + saturate_wheels distorsiona la mezcla traslacion/giro
**Esfuerzo:** 0.5-1 dia: implementar saturacion con prioridad a omega en kinematics + tests host-native + banco por estado.  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** media — **documentado:** NUEVO  
**Archivos:** src/shared/pids.h:42 ; src/shared/pids.cpp:66 ; src/central/motors_zircon.cpp:120 ; src/central/motors_zircon.cpp:121 ; src/shared/kinematics.cpp:27  

- **Qué observo:** El HeadingPID clampea su output a 360 grados/s (pids.h:42), y luego motors_zircon llama saturate_wheels (kinematics.cpp:27) que reescala las 3 ruedas proporcionalmente si alguna excede MAX_SPEED_MM_S. omega*R suma a la velocidad de cada rueda; si vx/vy ya estan cerca del max (p.ej. CLEAR a 500 mm/s + omega de giro), saturate_wheels escala TODO el vector incluyendo el omega -> el robot ni gira lo pedido ni traslada lo pedido, ambos quedan recortados por igual. No hay priorizacion (en MSL/top RCJ se suele priorizar el giro sobre la traslacion para no perder orientacion). Combinado con el lazo de heading abierto (P0), el robot puede quedar trasladando a la pelota sin completar el giro.
- **Riesgo si NO se hace:** En estados que combinan traslacion alta + giro (CLEAR del arquero, APPROACH con correccion lateral OTOS sumada en strategy.cpp:402), el robot pierde autoridad de giro justo cuando mas la necesita (pelota cerca). Despeje torcido / no se orienta al patear.
- **Riesgo del fix:** Implementar priorizacion (reservar headroom de PWM para omega antes de traslacion) cambia el comportamiento en todos los estados con giro -> re-tuning. Riesgo medio de regresion si no se testea cada estado.
- **Beneficio:** El robot mantiene autoridad de giro bajo saturacion -> mejor orientacion al patear/despejar. Tecnica de equipos top. Capitalizable a 2027.
- **Cómo evaluar que funciona:** Host-native: inyectar (vx,vy,omega) que saturen y verificar que el omega se preserva y la traslacion se recorta (no al reves). Banco: CLEAR contra pelota cerca, confirmar que el robot completa el giro hacia la pelota.

### 27. (P2 · control · mejora) El integral del HeadingPID acumula desde el primer tick antes de tener un dt real (usa fallback 0.01s) y nunca se congela si el robot no controla
**Esfuerzo:** 2-3 h: filtro low-pass en la derivada o dt fijo, + test host-native con dt jittery.  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** media — **documentado:** NUEVO  
**Archivos:** src/shared/pids.cpp:43 ; src/shared/pids.cpp:53 ; src/shared/pids.cpp:50  

- **Qué observo:** En heading_pid_tick, en el PRIMER tick tras un reset (primed=false) se usa dt=0.01 fallback (pids.cpp:43) y se integra error*dt ANTES de calcular la derivada (que en ese tick es 0 por el guard, correcto). El orden integra-luego-deriva esta bien, pero el integral se carga ya en el primer tick con un dt asumido que puede no corresponder a la cadencia real (si strategy corre a otra frecuencia que 100Hz por carga del loop, el dt real difiere). Como el loop principal gatilla strategy_tick cada 10ms via elapsedMillis (main_central.cpp:137) pero now_ms = millis() se lee dentro de cada *_tick, el dt medido (now_ms - last_tick_ms) puede ser 10ms o mas si el loop se atrasa por RX UART. El integral y la derivada usan ese dt variable; bajo jitter de loop la derivada (kd=0.5) amplifica el jitter de dt. No es critico pero suma ruido al omega.
- **Riesgo si NO se hace:** Ruido de bajo nivel en omega proporcional al jitter del loop. Poco visible solo, pero se suma al derivative kick y al lazo abierto de heading. Dificil de diagnosticar en cancha ('el robot tiembla un poco').
- **Riesgo del fix:** Bajo. Usar el dt nominal fijo (10ms) en vez del medido elimina el jitter de dt a cambio de error si el loop se atrasa mucho; o filtrar la derivada (low-pass). Decision de diseno menor.
- **Beneficio:** Omega mas limpio. Menos sensibilidad al jitter del scheduler cooperativo. Mejora de robustez reutilizable.
- **Cómo evaluar que funciona:** Host-native: alimentar ticks con dt variable (8-20ms) y measurement con ruido, comparar varianza del output con/sin filtro de derivada. Host-testeable completo.

### 28. (P2 · control · bug) PID lateral del arquero alimentado con cross_track (firmado) y depth (magnitud) usando los MISMOS clamps/ganancias y estado compartido
**Esfuerzo:** 0.5-1 dia: separar instancias de PID + ganancias por modo + reset al cambiar de modo + tests host-native. Tuning en banco aparte.  
**¿Requiere tu intervención?** Sí — Confirmar si DOWN va a emitir cross_track en build de competencia (sacar el #ifdef DOWN_DEBUG_SERIAL) o si CENTRAL debe asumir solo depth. Tuning de ganancias en banco con el arquero sobre la linea real.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** media — **documentado:** sí  
**Archivos:** src/central/strategy.cpp:151 ; src/central/strategy.cpp:155 ; src/central/strategy.cpp:159 ; src/central/strategy.cpp:162 ; src/central/strategy.cpp:164 ; src/shared/pids.h:69 ; src/shared/pids.h:76  

- **Qué observo:** gk_lateral_pid_output usa el MISMO g_lateral_pid_gk para dos modos con escalas/unidades distintas: modo Capa3 (cross_track en mm firmado, setpoint 0, error ~ +-cientos de mm) y modo fallback depth (magnitud no-firmada, setpoint=1.0, error ~ unidades) (strategy.cpp:155-164). Las ganancias (kp=50, ki=5, kd=10) y los clamps (integral_clamp=20, output_clamp=800) son los del struct (pids.h:65-77) y son los mismos para ambos modos. Con cross_track en mm, kp=50 * error de 100mm = 5000 mm/s -> clampea a 800 instantaneamente (siempre saturado salvo muy cerca del setpoint). Con depth (setpoint 1.0, error fraccionario), kp=50*0.x es razonable. Ademas el contexto marca que cross_track solo se calcula en DOWN bajo #ifdef DOWN_DEBUG_SERIAL -> en build de competencia world_model_cross_track_valid() es false -> SIEMPRE cae al fallback depth. Peor: si en una corrida alterna entre valido/N-A, el integral y prev_error del PID se arrastran entre dos modos de unidades incompatibles (solo se resetea al salir de PATROL/INTERCEPT, strategy.cpp:178-180).
- **Riesgo si NO se hace:** El strafe paralelo a la linea (Capa3) o satura siempre (cross_track grande) dando bang-bang lateral, o no existe (build competencia -> fallback depth). Al mezclar modos sin reset, el integral arrastrado de mm a 'depth' genera un transitorio grande. Arquero que no mantiene la linea de forma estable.
- **Riesgo del fix:** Separar en dos PID (uno para cross_track, otro para depth) con ganancias propias, o re-escalar la ganancia segun modo. Requiere re-tuning de banco de ambos. Bajo riesgo de codigo, costo de tuning.
- **Beneficio:** Strafe del arquero estable y tuneable en cada modo. Evita el transitorio al alternar. Cierra la divergencia entre lo que el codigo asume y lo que DOWN entrega en competencia.
- **Cómo evaluar que funciona:** Host-native: alimentar gk_lateral_pid_output alternando cross_track_valid true/false y verificar que el integral se resetea al cambiar de modo y que cada modo usa ganancias coherentes. Banco: arquero sobre linea, medir si mantiene el setpoint sin bang-bang.
- **Nota del verificador adversarial:** Bajo de P1 a P2 por dos razones. (a) La premisa de riesgo del auditor ('en competencia el strafe Capa3 no existe -> siempre fallback depth') es falsa — Capa3 es alcanzable en build de competencia, así que el escenario 'peor caso' que justifica P1 está mal fundado. (b) El output saturado se pondera ×0.5 en PATROL (strategy.cpp:461) y ×0.3 en INTERCEPT (:480): 800 mm/s -> 400/240 mm/s sumados, push lateral acotado, no catastrófico. Además el seguidor lateral del arquero está en etapa de banco (commits recientes), no probado en partido. El defecto es genuino y vale arreglarlo antes de confiar en el strafe Capa3, pero no es bloqueante de impacto-alto-en-partido. Esfuerzo de fix: bajo (~1-2 h). Opciones: (1) separar ganancias/clamps por modo (struct distinto o reescalar cross_track a la misma escala que depth), y (2) resetear el PID al cambiar de modo cross_track<->depth, no solo en transiciones de FSM. Plan de prueba HW obligatorio: con DOWN reportando cross_track real, medir vx lateral del arquero vs offset perpendicular y confirmar que no satura ni da bang-bang. Riesgo del fix: bajo (cambio localizado en gk_lateral_pid_output + pids).

### 29. (P2 · control · mejora) Anti-windup por clamp del integral no evita el windup real cuando el output satura (no hay back-calculation ni conditional integration)
**Esfuerzo:** 3-4 h: implementar conditional integration en heading_pid_tick y lateral_pid_tick + tests host-native de no-windup.  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** src/shared/pids.cpp:53 ; src/shared/pids.cpp:54 ; src/shared/pids.cpp:66 ; src/shared/pids.cpp:96 ; src/shared/pids.cpp:97 ; src/shared/pids.cpp:107  

- **Qué observo:** El 'anti-windup' es solo un clamp del termino integral a +-integral_clamp (pids.cpp:54 heading, :97 lateral). Eso NO es anti-windup real: el integral se sigue cargando hasta su clamp aunque el OUTPUT ya este saturado en output_clamp (:66/:107). El anti-windup correcto detiene la integracion cuando el output satura (conditional integration) o resta el exceso (back-calculation). Tal como esta, si el lazo satura output_clamp por un tiempo (p.ej. heading lejos del setpoint), el integral llega a +-50 (heading) o +-20 (lateral) y al cruzar el setpoint tarda en descargarse -> overshoot. Para el heading: ki=0.05 * integral_clamp=50 = 2.5 grados/s de aporte integral max, chico frente a kp*error, asi que el impacto es menor en heading. En el LATERAL del arquero ki=5 * integral_clamp=20 = 100 mm/s de aporte sostenido, nada despreciable contra GK_PATROL_SPEED=150.
- **Riesgo si NO se hace:** Overshoot/lag del arquero al volver al centro de la linea tras una excursion: el integral lateral cargado lo pasa de largo. Oscilacion lenta alrededor del setpoint de linea. En heading el efecto es chico.
- **Riesgo del fix:** Bajo y aislado a pids.cpp (funcion pura). Agregar conditional integration (no integrar si output ya saturado y el error empuja en el mismo sentido) es estandar y barato.
- **Beneficio:** Respuesta sin overshoot al volver al setpoint. Tuning del lateral mas predecible. Mejora reutilizable para 2027 (PID correcto en la libreria compartida).
- **Cómo evaluar que funciona:** Host-native: forzar saturacion de output sostenida, verificar que el integral NO crece mientras esta saturado y empujando en la direccion de saturacion; comparar overshoot al cruzar setpoint vs version actual. Totalmente host-testeable.

### 30. (P2 · fsm · mejora) Path OTOS de KICKOFF: omega siempre 0 (target==cur), el lazo de heading es codigo muerto
**Esfuerzo:** 1-2 h (decidir referencia + fix o limpieza + test host de drive_straight con target!=cur)  
**¿Requiere tu intervención?** Sí — Decision de diseno: que rumbo de referencia usar en el boost (y si vale la pena dado drift del OTOS).  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/strategy.cpp:222-242  

- **Qué observo:** En KICKOFF con OTOS fresco se setea ds_in.target_heading_deg = otos_heading y ds_in.cur_heading_deg = otos_heading (strategy.cpp:227-228), o sea el MISMO valor. drive_straight_compute hace omega = kp*(target-cur) = 0 siempre (drive_straight.cpp:23-25). El comentario dice 'mantener heading actual' pero el efecto es que el lazo de heading del KICKOFF nunca corrige nada: si el robot deriva angularmente durante el boost, no se endereza. La unica correccion activa es la lateral por otos_vy. El heading_pid_set_target(heading) de la linea 220 ademas no se usa en el path OTOS (se arma un DriveStraight aparte). Es codigo muerto / intencion no implementada.
- **Riesgo si NO se hace:** El boost de kickoff no mantiene rumbo angular; si arranca torcido o derrapa, no se autocorrige (solo cancela deriva lateral). En 250ms el impacto es chico pero el lazo da falsa sensacion de estabilizacion.
- **Riesgo del fix:** Definir un target_heading_deg fijo (rumbo de cancha al arco rival capturado al entrar a KICKOFF) y usar otos_heading como cur. Riesgo bajo, pero depende de tener un rumbo de referencia confiable (OTOS driftea).
- **Beneficio:** KICKOFF que de verdad mantiene rumbo; o, si se decide no hacerlo, borrar el lazo muerto para no confundir.
- **Cómo evaluar que funciona:** Host: drive_straight_compute con target!=cur ya cubierto; lo nuevo es el cableado en strategy. Banco: kickoff arrancando torcido, ver si endereza.

### 31. (P2 · fsm · mejora) POSITION puede declarar 'reached' prematuro cuando el target colapsa cerca del origen
**Esfuerzo:** 3-4 h (criterio + tests host de la geometria + validacion banco)  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** media — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/strategy.cpp:306-336 ; software/teensy/Soccer 2026/src/shared/behind_ball.cpp:20-39  

- **Qué observo:** El target behind-ball se computa como ball - gap*u (behind_ball.cpp:36-37) y tdist se mide desde el origen=robot (strategy.cpp:317). Cuando la pelota esta a ~gap mm justo en la direccion del arco (ej. bx=0, by~120 con goal_angle~0), el target colapsa a ~(0,0) y tdist<80 -> reached=true en el primer tick aunque el robot este DELANTE de la pelota, no detras. Como reached ademas exige aligned (ball_is_in_attack_line), en ese caso aligned tambien da true porque la pelota esta al frente; entonces pasa a APPROACH inmediatamente. En la geometria 'pelota entre robot y arco' eso es correcto, pero el criterio no distingue 'estoy bien detras' de 'el target degenero por proximidad' — no hay chequeo de que el robot este del lado correcto de la pelota.
- **Riesgo si NO se hace:** En ciertos angulos el robot saltea el posicionamiento y embiste de frente, perdiendo el beneficio del behind-the-ball (pelota sale desviada). Caso de borde, no constante.
- **Riesgo del fix:** Agregar al criterio de reached una verificacion de lado (ej. que el robot este por detras de la pelota respecto a la linea pelota-arco, no solo cerca del target). Riesgo bajo-medio; testeable host pero hay que evitar bloquear el caso legitimo.
- **Beneficio:** Posicionamiento mas confiable antes de empujar; menos tiros desviados.
- **Cómo evaluar que funciona:** Host: casos de compute_behind_ball_target + criterio reached con pelota delante/al costado y verificar que no declara reached cuando el robot esta del lado del arco. Banco: confirmar que rodea antes de empujar en angulos oblicuos.

### 32. (P2 · fsm · bug) CLEAR del arquero usa HeadingPID->omega sin validar sentido de giro en HW (riesgo runaway)
**Esfuerzo:** 0.5 dia de banco (medir sentido) + fix puntual si hace falta  
**¿Requiere tu intervención?** Sí — Medicion en HW del sentido fisico de giro ante omega>0; no se puede cerrar host-native.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** media — **documentado:** sí  
**Archivos:** software/teensy/Soccer 2026/src/central/strategy.cpp:516-523  

- **Qué observo:** GkState::CLEAR es el UNICO estado del arquero que comanda omega: heading_pid_tick() con setpoint = my_heading + ball_angle_rel y measurement = my_heading (strategy.cpp:518-523). El termino my_heading se cancela (error = ball_angle_rel, medicion viva de camara), asi que NO hay runaway por BNO congelado (el offset se anula). PERO sigue en pie la convencion de omega sin validar: si kinematics aplica +omega como horario fisico y el HeadingPID asume CCW, el omega de CLEAR gira en sentido contrario al esperado -> el frente se aleja de la pelota en vez de acercarse -> el lazo diverge (runaway por signo, no por BNO). Es el riesgo del contexto, localizado: CLEAR es donde primero se va a manifestar porque es el unico omega del GK.
- **Riesgo si NO se hace:** Si el signo de omega esta invertido respecto a lo que asume el HeadingPID, en el primer CLEAR real el arquero gira para el lado equivocado y se descontrola al borde del arco.
- **Riesgo del fix:** Validar el signo en banco (rueda libre) antes de habilitar CLEAR; si esta invertido, corregir el signo en un solo lugar (kinematics u output). Riesgo bajo de codigo, alto de no testear.
- **Beneficio:** Despeje con giro al lado correcto; cierra la incognita de convencion omega para todo el robot.
- **Cómo evaluar que funciona:** Banco: robot en soportes, forzar CLEAR con pelota a la derecha, observar si el frente gira HACIA la pelota (correcto) o se aleja (signo invertido). Medible a ojo + log de omega_centideg_s.
- **Nota del verificador adversarial:** Bajo P1 -> P2. Razon: el hallazgo es real como "convencion de signo de omega sin validar en HW", pero NO es un bug localizado en CLEAR ni una amenaza diferencial. El signo de omega afecta por igual a TODOS los estados rotantes (atacante POSITION/APPROACH/SEARCH/KICKOFF y GK CLEAR) porque comparten inverse_kinematics. Si el signo estuviera invertido, se manifestaria primero y mas frecuentemente en el atacante (que rota casi siempre), no en CLEAR (estado raro, solo cuando la pelota entra al area chica del arquero). Ademas el atacante ya tiene KICKOFF testeado en banco segun commits recientes, lo que actua como deteccion temprana del signo de omega comun. El verdadero entregable es un test de bench unico: comandar omega>0 constante y verificar que el robot gira en el sentido que el HeadingPID asume (acercar el frente al setpoint). Esa validacion (1-2h en banco, ya hay diags de strafe/giro) cubre CLEAR y todos los demas a la vez; no amerita una TASK P1 propia de "arreglar CLEAR". El framing del hallazgo como "CLEAR es donde primero se va a manifestar" es incorrecto: por frecuencia de uso, el atacante lo expone antes.

### 33. (P2 · fsm · mejora) Duplicacion de FSM no usada: strategy_transitions.cpp replica el arbol pero strategy.cpp no la llama
**Esfuerzo:** 1-2 dias (refactor cuidadoso + verificar paridad con la suite existente)  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** sí  
**Archivos:** software/teensy/Soccer 2026/src/shared/strategy_transitions.h:1-24 ; software/teensy/Soccer 2026/src/shared/strategy_transitions.cpp:27-197 ; software/teensy/Soccer 2026/src/central/strategy.cpp:185-529  

- **Qué observo:** strategy_transitions.cpp es una REPLICA FIEL a mano del arbol de transiciones de strategy.cpp (lo dice su propio banner, .h:1-24). El binario del robot NO la usa: attacker_tick/goalkeeper_tick tienen su propia copia del arbol. Es una red de caracterizacion util, pero crea deuda de sincronizacion: cualquier fix en strategy.cpp (ej. el reordenamiento INTERCEPT->PATROL de este informe) hay que portarlo a mano a la replica o los tests mienten. Ya hoy las dos copias estan sincronizadas pero el riesgo es estructural y va a morder cuando alguien arregle un bug en una sola.
- **Riesgo si NO se hace:** Divergencia silenciosa entre el codigo testeado (replica) y el codigo que corre (strategy.cpp); los tests verdes pueden dar falsa confianza sobre la FSM real. Mantenimiento doble.
- **Riesgo del fix:** Refactor para que strategy.cpp DELEGUE las transiciones en strategy_transitions (separar 'decidir transicion' de 'computar cmd'). Riesgo medio: tocar el cerebro que ya anda; el propio banner dice que se difirio por eso. Hacerlo con los tests de caracterizacion como red.
- **Beneficio:** Una sola fuente de verdad de la FSM, testeada host-native; menos deuda hacia 2027.
- **Cómo evaluar que funciona:** Host: tras el refactor, correr test_strategy_transitions y ademas verificar que attacker_tick/goalkeeper_tick producen las mismas fases que las funciones puras para una bateria de world views.

### 34. (P2 · fsm · mejora) Arquero nunca controla profundidad (eje +Y) en INTERCEPT: ciego a pelota frontal hasta 250mm
**Esfuerzo:** 0.5-1 dia (logica + tuning de cuanto achicar sin desproteger + banco)  
**¿Requiere tu intervención?** Sí — Decision tactica (cuanto sale el arquero) + tuning en banco/scrimmage.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** media — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/strategy.cpp:469-490  

- **Qué observo:** En INTERCEPT solo se comanda eje X (vx_intercept = bx*Kp + lateral_pid*0.3) y vy/omega quedan en 0. El arquero solo persigue el offset lateral de la pelota; no avanza ni retrocede para cortar una pelota que viene de frente (bx~0, by decreciente). Recien reacciona cuando dist<250mm dispara CLEAR. Para un tiro frontal directo al arco, el GK se queda quieto hasta el ultimo momento.
- **Riesgo si NO se hace:** Pelota que entra de frente y centrada no genera respuesta hasta 250mm; menos tiempo de reaccion -> gol. Es defensa puramente lateral.
- **Riesgo del fix:** Agregar una componente de profundidad acotada (ej. salir levemente a achicar cuando by < umbral y bx pequeno) o bajar GK_CLEAR_TRIGGER tuneado. Riesgo medio: salir del arco abre huecos; hay que limitar el avance.
- **Beneficio:** Arquero que achica frente a tiros centrados; mejor cobertura. Capitalizable.
- **Cómo evaluar que funciona:** Banco/scrimmage: tiros frontales centrados y medir si el GK reacciona antes de 250mm sin abandonar el arco. Logica de decision testeable host.

### 35. (P2 · fsm · mejora) PATROL: oscilacion con static direction/last_change no se reinicia al entrar al estado
**Esfuerzo:** 1 h  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/strategy.cpp:454-461  

- **Qué observo:** La oscilacion de patrulla usa variables function-static (`static int direction`, `static uint32_t last_change`) que persisten entre entradas a PATROL y entre partidos/resets. transition_gk no las toca y strategy_init tampoco. Tras volver de INTERCEPT/CLEAR la fase de oscilacion arranca donde quedo (last_change viejo) -> en el primer tick de regreso `now_ms - last_change` puede ser enorme y flipear direction inmediatamente, o quedar desfasada. Tambien rompe testeabilidad host (estado oculto no reseteable).
- **Riesgo si NO se hace:** Patrulla con fase impredecible al re-entrar a PATROL (arranca yendo para cualquier lado, posible flip inmediato). Efecto menor en juego pero ensucia el comportamiento y los tests.
- **Riesgo del fix:** Mover direction/last_change a estado del modulo y reinicializarlos en transition_gk(PATROL) y strategy_init. Riesgo bajo.
- **Beneficio:** Patrulla determinista y reseteable; mejor para tests host y para 2027.
- **Cómo evaluar que funciona:** Host: tras strategy_init, primer tick de PATROL debe arrancar con direction y fase conocidas. Verificable si se expone el estado o via observacion del signo de vx en los primeros ticks.

### 36. (P2 · fsm · bug) kicker_fire en APPROACH apunta a la pelota, no al arco: tiro a ciegas si robot no quedo bien orientado
**Esfuerzo:** 2-4 h (logica + test host de is_aligned_to_shoot extendido + tuning de banco)  
**¿Requiere tu intervención?** Sí — Decision de diseno (que tan estricto el gate) + tuning del umbral angular en banco con pelota real y arco.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** media — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/strategy.cpp:362-373 ; software/teensy/Soccer 2026/src/shared/behind_ball.cpp:41-50  

- **Qué observo:** En APPROACH el HeadingPID apunta el frente HACIA LA PELOTA (ball_angle_abs, strategy.cpp:370-372), pero el gate de disparo is_aligned_to_shoot() solo verifica que la pelota este cerca (<=80mm) y que |goal_angle| <= 12 (behind_ball.cpp:46-49). goal_angle es el angulo al arco respecto al FRENTE del robot. Como el frente apunta a la pelota, |goal_angle|<=12 implica que pelota y arco estan casi alineados respecto al robot — pero NO garantiza que el robot empuje la pelota hacia el arco: el robot embiste la pelota en la direccion robot->pelota, y si esa direccion difiere del robot->arco, la pelota sale desviada. POSITION mitiga esto colocandose detras, pero APPROACH re-apunta a la pelota y puede disparar en una geometria donde el empuje no va al arco. El umbral de 12 sobre goal_angle (no sobre la diferencia empuje-vs-arco) es la causa.
- **Riesgo si NO se hace:** Disparos/empujes que no van al arco; en cancha se traduce en posesiones perdidas. El robot 'patea' pero la pelota se va de lado.
- **Riesgo del fix:** Cambiar el criterio de kick para exigir que el VECTOR robot->pelota este alineado con robot->arco (usar ball_is_in_attack_line con tolerancia chica) ademas de la distancia. Riesgo medio: cambia cuando dispara; hay que re-tunear en banco para no volverse tan conservador que nunca patee.
- **Beneficio:** El kicker solo dispara cuando el empuje realmente va al arco -> mejor conversion.
- **Cómo evaluar que funciona:** Host: extender is_aligned_to_shoot/ball_is_in_attack_line con casos donde frente apunta a pelota pero arco a 11 grados -> hoy dispara, deberia no disparar. Banco: medir % de disparos que entran al arco desde distintos angulos de aproximacion.
- **Nota del verificador adversarial:** El hallazgo es real pero la propuesta sobreestima el riesgo al afirmar que el gate 'NO garantiza' empuje al arco. Hay mitigaciones reales en el codigo que el hallazgo subestima: (1) POSITION solo entra a APPROACH cuando ball_is_in_attack_line con tol 30deg (strategy.cpp:332-334), o sea el robot ya se rodeo a la pelota colocandola en la linea pelota-arco; (2) APPROACH vuelve a POSITION con histeresis 40deg si se desalinea (strategy.cpp:355-359); (3) cuando la pelota esta pegada (<=80mm) y centrada por el HeadingPID, robot->pelota approx frente, asi que goal_angle approx angulo empuje-vs-arco y el umbral de 12deg lo acota a ~12deg mas el error de centrado lateral; (4) el refinamiento WP-2A (strategy.cpp:393-403) cancela deriva lateral con OTOS para que el empuje salga derecho. El error residual real es el centrado lateral de la pelota (bx) no incluido en el gate. Baja severidad a P2 (mejora deseable de precision de tiro, no impacto sistematico): no es un disparo 'a ciegas', es un disparo con tolerancia angular acotada pero no optima. Esfuerzo de fix: bajo (1-2h) -- agregar al gate un chequeo de que |atan2(bx,by) - goal_angle| sea chico, es decir que la pelota este alineada entre frente-de-empuje y arco, no solo que goal_angle respecto al frente sea chico. Requiere test en hardware real con pelota descentrada para validar.

### 37. (P2 · mainloop · mejora) Sin gate de match_running antes de aplicar comandos de motor en el path normal
**Esfuerzo:** 30 min - 1 h + banco  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/main_central.cpp:144-148 ; software/teensy/Soccer 2026/src/central/strategy.cpp:196-211  

- **Qué observo:** El loop llama strategy_tick() + motors_apply_command(cmd) siempre que el snapshot esté fresco, sin chequear match_running en el loop. La seguridad pre-match depende ENTERAMENTE de que strategy devuelva cmd=0 en WAIT_START. Hoy funciona (attacker_tick/goalkeeper_tick transicionan a WAIT_START si !match_running y devuelven MotorCommand{}), pero es una garantía frágil: cualquier bug futuro en una transición de strategy (p.ej. un estado que no chequea match) movería motores con el árbitro en STOP. No hay defensa en profundidad en el loop. Además, motors_apply_command corre la cinemática inversa cada tick aun en WAIT_START (cmd 0 -> PWM 0, inocuo pero gasto).
- **Riesgo si NO se hace:** Si una futura edición de la FSM rompe el gate de match, el robot se mueve durante STOP -> descalificación. La seguridad de 'no moverse sin árbitro' no está enforced en el nivel del loop, sólo en strategy.
- **Riesgo del fix:** Agregar en el loop: si !match_running -> motors_stop() y saltear strategy (o sólo correr strategy para el state_name de debug pero forzar cmd 0). Riesgo: hay que no romper el flanco STOP->RUN del KICKOFF, que se detecta dentro de strategy; si el loop saltea strategy en STOP, el flanco se sigue detectando porque strategy igual corre en el primer tick con RUN.
- **Beneficio:** Defensa en profundidad: 'sin árbitro = sin motores' enforced a nivel loop, independiente de bugs futuros en la FSM.
- **Cómo evaluar que funciona:** Host-native: stub match_running=false, snapshot_fresh=true -> verificar que se llama motors_stop() y NO motors_apply_command con cmd no-cero. Banco: confirmar inmovilidad total con árbitro en STOP.

### 38. (P2 · mainloop · mejora) g_loop_count uint32_t sin manejo de wrap en debug — cosmético
**Esfuerzo:** 10 min  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/main_central.cpp:36 ; software/teensy/Soccer 2026/src/central/main_central.cpp:97  

- **Qué observo:** g_loop_count++ es uint32_t; a la frecuencia del loop (libre, miles de Hz) puede dar wrap en horas de banco. Sólo se usa para debug print. No es un bug funcional (el wrap es benigno) pero el contador de loop puede confundir al leer telemetría de sesiones largas. Lo señalo por completitud del review del loop; no tiene impacto en cancha (partidos de minutos).
- **Riesgo si NO se hace:** Ninguno funcional; sólo lectura confusa de telemetría en banco de varias horas.
- **Riesgo del fix:** Ninguno relevante; opcionalmente loggear loops/seg en vez del contador absoluto.
- **Beneficio:** Telemetría de banco más legible.
- **Cómo evaluar que funciona:** Revisión de código; sin HW.

### 39. (P2 · mainloop · bug) EMERGENCY_LINE frena aunque el match NO esté corriendo / pre-start
**Esfuerzo:** 15 min  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/main_central.cpp:127-134  

- **Qué observo:** El bypass EMERGENCY_LINE en loop() chequea world_model_imminent_exit() && world_model_line_is_fresh() PERO no consulta world_model_match_running(). Antes del START del árbitro (o tras un STOP), si DOWN ya está reportando imminent_exit (el robot apoyado sobre/cerca de la línea en la colocación inicial), la CENTRAL ejecuta motors_brake() (INA=INB=HIGH, corto activo en los 3 H-bridge) en cada iteración del loop. El resto del loop (tick de strategy) sí está gateado por snapshot_fresh, pero el brake no está gateado por match. Resultado: H-bridges en freno activo consumiendo corriente de corto mientras el robot está quieto en la cancha esperando el silbato.
- **Riesgo si NO se hace:** Estrés térmico/eléctrico de los 3 drivers durante la espera de inicio (puede ser minutos en una colocación de árbitro). Con el robot ya apoyado en la línea de área, el brake activo se sostiene indefinidamente. No es un movimiento peligroso, pero castiga el hardware justo en el momento de menor necesidad.
- **Riesgo del fix:** Trivial: agregar && world_model_match_running() a la guarda. Riesgo casi nulo; el único cuidado es que durante el match el brake debe seguir activo (match_running ya será true).
- **Beneficio:** Evita corriente de freno activo innecesaria pre-start; alinea el bypass con el resto del loop que ya respeta el estado de match.
- **Cómo evaluar que funciona:** Test host-native: stub world_model con imminent_exit=true, line_fresh=true, match_running=false -> verificar que loop() NO llama motors_brake() sino que cae al path de strategy (que con WAIT_START devuelve cmd 0). Confirmar en banco midiendo corriente de los drivers con robot sobre la línea y match en STOP.

### 40. (P2 · mainloop · mejora) Watchdog de snapshot stale usa motors_stop() (libre) en vez de motors_brake() — el robot se desliza tras perder TOP
**Esfuerzo:** 1-2 h (brake-then-free con timer) + banco  
**¿Requiere tu intervención?** Sí — Banco: medir distancia de deslizamiento con motors_stop vs motors_brake desde 500-600 mm/s, y evaluar estrés térmico del brake sostenido si TOP no vuelve.  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/main_central.cpp:140-143 ; software/teensy/Soccer 2026/src/central/motors_zircon.cpp:140-149  

- **Qué observo:** Cuando TOP cae >500 ms (snapshot stale), loop() llama motors_stop(), que pone INA=INB=0 y PWM=0 -> ruedas LIBRES (free-wheel), no freno. Si el robot venía a ATK_APPROACH_MAX_SPEED=600 mm/s o GK_CLEAR_SPEED=500 mm/s, sigue rodando por inercia hasta detenerse por fricción, potencialmente cruzando la línea o chocando, durante el segundo+ que tarda en frenar. El nombre y doc de motors_stop() lo dejan claro: 'frena por fricción'. Para un robot que perdió su world model y por ende su capacidad de evitar la línea, free-wheel es la peor opción.
- **Riesgo si NO se hace:** Tras perder TOP a alta velocidad, el robot se desliza sin control y puede salir de cancha o chocar -> penalización u out. Justo en el modo más ciego (sin mundo) el frenado es el más débil.
- **Riesgo del fix:** Cambiar a motors_brake() en el path stale. Riesgo: brake activo sostenido estresa drivers si TOP queda caído mucho tiempo; mitigable con brake breve seguido de stop (brake N ms, luego free).
- **Beneficio:** Frenado real al perder el cerebro, reduciendo deslizamiento descontrolado. Comportamiento fail-safe más conservador.
- **Cómo evaluar que funciona:** Banco: robot a 500 mm/s, cortar el cable de Serial7 (TOP), medir distancia recorrida hasta detenerse con stop vs brake. Criterio: brake reduce la distancia de deslizamiento de forma medible (>30%).
- **Nota del verificador adversarial:** El hallazgo es factualmente correcto en todos sus detalles. Bajo la severidad de P1 a P2 por contexto, no porque sea falso: (1) Existe una red de protección ANTES de este path. El bloque EMERGENCY_LINE (main_central.cpp:127-134) corre en CADA iteración del loop (no espera al tick de 100Hz) y SÍ usa motors_brake() cuando DOWN reporta línea inminente con dato fresco. DOWN y TOP son enlaces independientes (Serial1 vs Serial7), así que perder TOP no implica perder la detección de línea de DOWN — el escenario de "cruza la línea sin frenar" requiere que ADEMÁS DOWN falle o no alcance a detectar a tiempo. El watchdog de TOP es la segunda capa, no la única. (2) El gap real es claramente un bug de robustez de un caracter de path (motors_stop→motors_brake en la rama stale), fix de minutos, riesgo-fix bajo: motors_brake ya está implementado y probado en el path de emergencia. (3) Caveat de hardware a verificar en banco: el freno activo (INA=INB=1) genera corriente de freno alta y, si el watchdog dispara repetidamente (TOP intermitente), podría estresar el driver/batería — por eso conviene frenar una vez y luego soltar, no mantener brake indefinido. Plan de test obligatorio (regla del repo): medir en banco la distancia de deslizamiento del robot a 600 mm/s al simular caída de TOP, comparando motors_stop() vs motors_brake(), y confirmar que el driver tolera el brake sostenido. Esfuerzo del fix subestimado solo si se quiere el patrón "brake-then-coast" en vez de brake crudo.

### 41. (P2 · mainloop · bug) Doc/header de motors_brake contradice la implementación (free vs brake activo)
**Esfuerzo:** 10 min  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/motors_zircon.h:19-20 ; software/teensy/Soccer 2026/src/central/motors_zircon.cpp:151-160  

- **Qué observo:** El header motors_zircon.h:19-20 documenta motors_brake() como 'Frena los 3 motores en modo libre (PWM=0, INA/INB=0). Motor queda libre — frena por fricción'. Pero la implementación (motors_zircon.cpp:151-160) hace INA=INB=HIGH con PWM=0 = freno ACTIVO (corto en H-bridge), que es lo correcto para EMERGENCY_LINE. El comentario del header describe en realidad motors_stop(). La implementación es correcta; la doc del header está cruzada con la de motors_stop(). Un alumno que lea sólo el header creerá que la emergencia usa free-wheel cuando en realidad usa corto activo (corriente alta).
- **Riesgo si NO se hace:** Confusión de mantenimiento: alguien podría 'arreglar' motors_brake para que coincida con el header roto, eliminando el freno activo real de emergencia. Riesgo de regresión inducida por doc errónea.
- **Riesgo del fix:** Corregir el comentario del header para que describa el corto activo (INA=INB=HIGH). Riesgo nulo, sólo doc.
- **Beneficio:** Doc coherente con el código; evita una regresión futura en el camino de seguridad más crítico.
- **Cómo evaluar que funciona:** Revisión de código: el comentario del header coincide con motors_zircon.cpp:155-159. No requiere HW.

### 42. (P2 · mainloop · mejora) Drenado de USB Serial en arranque manual puede atragantarse y robar tiempo de loop
**Esfuerzo:** 15 min  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** media — **documentado:** NUEVO  
**Archivos:** software/teensy/Soccer 2026/src/central/main_central.cpp:113  

- **Qué observo:** En el bloque CENTRAL_ENABLE_MANUAL_START, `while (Serial.available()) { if (Serial.read() == '\n') ser = true; }` drena TODO el buffer USB en una iteración. Si una herramienta de banco/monitor manda un chorro de bytes, esto se ejecuta antes de la guarda EMERGENCY (que está más abajo, línea 127). Aunque la USB CDC del Teensy es rápida, el orden es subóptimo: en una iteración con muchos bytes pendientes, el freno de emergencia se posterga hasta que termine de drenar el USB. Está gateado tras CENTRAL_ENABLE_MANUAL_START (sólo banco), por eso es P2, pero es exactamente el escenario de banco donde se prueban líneas.
- **Riesgo si NO se hace:** En banco con monitor verboso, latencia extra antes del brake de emergencia. No afecta el build de competencia (flag OFF).
- **Riesgo del fix:** Mover el bloque de arranque manual DESPUÉS de la guarda EMERGENCY, o limitar el drenado a N bytes por iteración. Riesgo nulo.
- **Beneficio:** La emergencia mantiene prioridad sobre la lógica de banco; orden de loop más limpio.
- **Cómo evaluar que funciona:** Host-native: no aplica directo (toca Serial HW). Revisión de código + banco: inyectar burst por USB y medir que el brake no se atrasa. Criterio: latencia de brake <15 ms aun con USB cargado.

### 43. (P2 · motion · bug) motors_zircon no aplica inversion por-motor; M2 esta invertido por HW
**Esfuerzo:** 1-2 h codigo + banco para confirmar que motor(es) invertir.  
**¿Requiere tu intervención?** Sí — Confirmar en banco con motors_set_one(idx, +pwm) que motor(es) giran al reves del sentido esperado, y fijar el array de inversion. Decision de diseno: invertir por SW (este fix) vs re-soldar INA/INB (Enzo).  
**¿Requiere testeo en HW para cerrar?** Sí — **confianza:** alta — **documentado:** sí  
**Archivos:** src/central/motors_zircon.cpp:81 ; src/central/motors_zircon.cpp:84 ; src/central/motors_zircon.cpp:113  

- **Qué observo:** apply_pwm_to_motor (motors_zircon.cpp:81-97) escribe INA/INB segun el signo del PWM, identico para los 3 motores. No existe ningun array de inversion por-motor (algo como MOTOR_INVERT[3]). Segun el banco 2026-06-01 el motor 2 tiene INA/INB invertidos por hardware, asi que con la misma logica de signo M2 gira al reves del esperado por la cinematica. La cinematica calcula bien las 3 velocidades, pero la capa de aplicacion no compensa la inversion fisica.
- **Riesgo si NO se hace:** BLOQUEANTE: con M2 invertido cualquier comando que use M2 (todo lo que no sea avance/retroceso puro alineado al eje muerto) produce una trayectoria incorrecta -> el robot no va donde la FSM cree. Esto invalida toda prueba de movimiento real.
- **Riesgo del fix:** Bajo: agregar constexpr bool MOTOR_INVERT[3] en config y multiplicar pwm_signed por el signo en apply_pwm_to_motor. Riesgo de equivocar cual motor invertir -> se valida en banco con motors_set_one.
- **Beneficio:** La cinematica vuelve a corresponder con el movimiento fisico real. Es prerequisito para validar TODO lo demas (deadzone, omega, strafe).
- **Cómo evaluar que funciona:** Banco con diag que llame motors_set_one: comandar +PWM a cada motor y verificar sentido de giro 'adelante' consistente entre los 3. Con el fix, los 3 responden igual al signo. No es testeable host (toca pines).
- **Nota del verificador adversarial:** Severidad propuesta P0 sobre-calibrada. Bajar a P2: es un gap estructural latente (la capa de aplicacion de produccion carece del signo por-motor que el propio diag_central_motors dice que hay que portar), pero NO es bloqueante hoy porque: (1) el robot que anduvo en banco usa control directo, no este path; (2) el banco 2026-06-03 atribuye el 'solo gira M1' a deadzone de PWM, no a inversion de M2, y dejo la confirmacion de M2 abierta pendiente de retest; (3) la cinematica generica {60,-60,180} esta sin calibrar (Enzo) por lo que ese path entero esta en suspenso. El hallazgo invierte causa-efecto: toma una observacion estructural real + un quirk HW real y los suelda en un blocker causal que la evidencia de banco no respalda. Esfuerzo de fix real cuando se decida usar el path de produccion: bajo (~1-2 h: agregar un MOTOR_INVERT[3]/MOTOR_DIR[3] en config_central.h y multiplicarlo en apply_pwm_to_motor), PERO requiere el retest de banco a PWM alto para SABER que signos van — sin ese dato el fix es especulativo. Riesgo si no se hace: nulo mientras se use control directo; relevante solo si/ cuando se adopte motors_apply_command como path de conduccion. Recomendacion: dejar como tema-a-analizar ligado a TASK-101 (reconciliar substrato cinematica vs control directo), no como P0.

### 44. (P2 · motion · mejora) Sin watchdog visible dentro de la capa de motores: depende 100% del caller
**Esfuerzo:** 2-3 h (timestamp interno opcional gateado por flag + test).  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** media — **documentado:** sí  
**Archivos:** src/central/motors_zircon.cpp:113 ; src/central/config_central.h:97 ; src/central/main_central.cpp:146  

- **Qué observo:** motors_apply_command (motors_zircon.cpp:113) aplica el ultimo comando recibido y lo MANTIENE indefinidamente: el PWM queda fijo hasta el proximo motors_apply_command/motors_stop. El watchdog (COMMAND_TIMEOUT_MS=200, config_central.h:97) vive afuera, en main_central (que llama motors_stop si no llega comando). La capa de motores en si no tiene noción de 'comando viejo'. Si un diag o un refactor llama motors_apply_command una sola vez y se cuelga el loop, el robot queda a velocidad fija. Es una eleccion de diseno valida (watchdog en el caller) pero fragil: cualquier path de codigo que use motors_apply_command sin el watchdog del main hereda un robot que no se detiene solo.
- **Riesgo si NO se hace:** Un diag o estado de la FSM que aplique un comando y no vuelva a tickear deja el robot corriendo descontrolado. Ya hay varios diags que llaman motors_apply_command directo (diag_central_strafe, drive_straight, arbitro_strafe) sin el watchdog del main.
- **Riesgo del fix:** Medio: agregar un timestamp interno + auto-stop si no se refresca en N ms cambiaria la semantica para los diags que esperan que el comando persista. Requiere coordinacion.
- **Beneficio:** Defensa en profundidad: el robot se frena solo aunque el caller falle. Util para la moratoria de hardware-up donde los diags se corren sueltos.
- **Cómo evaluar que funciona:** Host-native: simular millis() avanzando sin llamar motors_apply_command y verificar que un tick de watchdog interno fuerza PWM=0 tras el timeout. Banco: aplicar comando y cortar el loop, verificar que el robot se detiene solo (no requiere obligatoriamente HW si se testea la logica host).

### 45. (P2 · motion · mejora) dribbler_pwm del MotorCommand se ignora por completo en la capa de motores
**Esfuerzo:** 1 h (banner + TODO) o 0.5 dia si se implementa el PWM real (requiere pin del dribbler de Enzo).  
**¿Requiere tu intervención?** Sí — Decidir si hay dribbler en el robot 2026 y, de haberlo, que GPIO/MOSFET lo maneja (Enzo). Si no hay, solo documentar.  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** alta — **documentado:** NUEVO  
**Archivos:** src/shared/types.h:64 ; src/central/motors_zircon.cpp:113 ; src/diag/diag_central_arbitro_strafe.cpp:147  

- **Qué observo:** MotorCommand tiene el campo uint8_t dribbler_pwm (types.h:64) y hasta el diag lo setea (diag_central_arbitro_strafe.cpp:147 pone cmd.dribbler_pwm=0). Pero motors_apply_command (motors_zircon.cpp:113-138) nunca lo lee ni hay pin/PWM de dribbler en config_central.h. El campo es dead data en la capa de movimiento: si maniana se monta un dribbler y strategy lo comanda, no pasa nada y nadie ve el bug porque compila.
- **Riesgo si NO se hace:** Si se agrega dribbler para Incheon/2027 y strategy lo comanda, el firmware lo descarta silenciosamente -> horas perdidas buscando por que el dribbler no responde. Tambien es un campo del struct que confunde a quien lee el contrato.
- **Riesgo del fix:** Bajo: o se implementa (pin + analogWrite gateado por ROBOT2) o se documenta explicito 'no implementado' con un TODO/banner. Hoy no hay ni una cosa ni la otra.
- **Beneficio:** Contrato honesto: el lector sabe que dribbler_pwm no hace nada hoy. Deja el gancho listo y documentado para 2027.
- **Cómo evaluar que funciona:** Host: no aplica (no hace nada). Si se implementa: banco midiendo PWM en el pin del dribbler con osciloscopio al comandar dribbler_pwm. Minimo aceptable hoy: un banner en el .cpp/.h aclarando que el campo se ignora.

### 46. (P2 · motion · mejora) Doble saturacion (saturate_wheels + clamp en wheel_speed_to_pwm) — la segunda es dead y puede enmascarar bugs
**Esfuerzo:** 1 h (comentario + test que documente el invariante) o 2 h si se refactoriza.  
**¿Requiere tu intervención?** No  
**¿Requiere testeo en HW para cerrar?** No — **confianza:** media — **documentado:** NUEVO  
**Archivos:** src/central/motors_zircon.cpp:121 ; src/central/motors_zircon.cpp:124 ; src/shared/kinematics.cpp:22 ; src/shared/kinematics.cpp:34  

- **Qué observo:** motors_apply_command llama saturate_wheels(ws, MAX_SPEED_MM_S) (motors_zircon.cpp:121), que escala las 3 ruedas proporcionalmente si alguna excede MAX_SPEED -> preserva la trayectoria. Despues wheel_speed_to_pwm vuelve a clampear por-rueda contra el MISMO MAX_SPEED (kinematics.cpp:22-23). Como saturate_wheels ya garantizo que ninguna rueda supera MAX_SPEED, el clamp por-rueda de wheel_speed_to_pwm NUNCA actua: es codigo muerto en este pipeline. El problema sutil: ese clamp por-rueda esta ahi como 'red de seguridad' pero si alguien quita saturate_wheels o cambia el max, el clamp por-rueda distorsionaria la trayectoria (recorta solo la rueda que excede, no las 3) sin avisar. Es una redundancia que da falsa sensacion de seguridad y oculta de quien depende realmente la preservacion de trayectoria.
- **Riesgo si NO se hace:** Bajo hoy (funciona). Riesgo a futuro: un cambio que toque saturate_wheels cae en el clamp por-rueda que rompe la direccion del vector de movimiento de forma silenciosa. Deuda de claridad arquitectonica.
- **Riesgo del fix:** Muy bajo: documentar que wheel_speed_to_pwm asume entrada ya saturada, o quitar el clamp por-rueda y dejar solo saturate_wheels como unico punto de saturacion (single source of truth).
- **Beneficio:** Una sola responsabilidad de saturacion, mas facil de razonar y testear. Capitaliza a 2027 (menos trampas para alumnos nuevos).
- **Cómo evaluar que funciona:** Host-native: test que pase a wheel_speed_to_pwm un speed > max_speed y verifique el clamp, MAS un test de integracion que muestre que en motors_apply_command saturate_wheels ya dejo todo dentro de rango (el clamp nunca dispara). test_kinematics ya cubre las piezas por separado; falta el test del pipeline integrado.

---

## Descartados por el verificador adversarial (6)

Hallazgos que un revisor propuso pero el verificador refutó leyendo el código (buena señal: el sistema NO los tiene):

- **(fsm) INTERCEPT: pelota stale puede disparar CLEAR en vez de PATROL** — El orden de chequeo citado es real (strategy.cpp:484 `if (dist < GK_CLEAR_TRIGGER_MM) -> CLEAR` antes de 486 `else if (!world_model_ball_visible()) -> PATROL`, GK_CLEAR_TRIGGER_MM=250 en strategy.cpp:122) y los getters world_model_get_ball_x/y_mm() (world_model.cpp:61-62) devuelven crudo sin gate de visibilidad. PERO la premisa central del hallazgo es falsa: NO existen coords stale. El productor de la pelota, fuse_ball_dual (src/shared/cameras_fusion.cpp:64-68), en la rama no-visible setea EXPLICITAMENTE out.x_mm=0, out.y_mm=0, out.visible=false; y recompute_fused() recalcula g_ball desde cero en cada tick (cameras_runtime.cpp:126) sobre un BallFused out{} default-zero. No hay retencion de ultima posicion en ningun punto del pipeline. La unica via viable es la que el propio hallazgo menciona como secundaria: coords en (0,0) -> dist=0 -> 0<250 TRUE -> entra a CLEAR. Pero la consecuencia afirmada (el arquero sale del arco a despejar) NO ocurre: (1) en el tick que transiciona a CLEAR, strategy.cpp:489 `return cmd` devuelve el comando de INTERCEPT cuyo vx_intercept = bx*KP = 0*KP = 0 (solo queda el PID lateral), no un comando de carga; (2) en el tick siguiente, lo PRIMERO de CLEAR es strategy.cpp:498 `if (!world_model_ball_visible()) { transition_gk(PATROL); return cmd; }` con cmd en cero — la logica de carga 'ir DERECHO a la pelota a velocidad alta' (lineas 512-515) NUNCA se ejecuta porque el guard de visibilidad la corta. Neto real: un rebote espurio de un tick INTERCEPT->CLEAR->PATROL emitiendo comando ~cero, luego vuelve a PATROL. El arquero no abandona el arco. La replica pura strategy_transitions.cpp:176-183 reproduce el mismo orden pero igualmente no implica el gol regalado.
- **(control) HeadingPID realimenta sobre un heading CONGELADO (BNO del TOP) -> lazo de orientacion abierto** — Refutado. La afirmacion central ("error = setpoint - heading no decrece aunque el robot gire") es algebraicamente falsa en los tres sitios citados, porque el setpoint se construye SUMANDO heading a una cantidad relativa de vision, y el mismo heading se usa como feedback dentro del mismo tick, de modo que se cancela. En attacker_tick, heading se lee UNA sola vez (strategy.cpp:188) y se reutiliza. POSITION (310-311): setpoint = heading + goal_angle, feedback = heading => error = goal_angle. APPROACH (371-373): setpoint = ball_angle_abs = heading + ball_angle_rel, feedback = heading => error = ball_angle_rel. CLEAR (518-522): setpoint = world_model_get_my_heading_deg() + ball_angle_rel y feedback = world_model_get_my_heading_deg(), ambas llamadas consecutivas leen el mismo g_snap.my_heading_centideg => error = ball_angle_rel. En heading_pid_tick (pids.cpp:40) error = wrap_diff_deg(setpoint, current), y current==el heading usado en el setpoint, asi que el termino heading desaparece. El error efectivo es SIEMPRE el angulo RELATIVO de la pelota/arco medido por la camara del TOP (atan2(bx,by) / goal_opp_angle), que SI cambia cuando el robot gira fisicamente porque la vision re-mide la posicion relativa cada frame. El lazo de orientacion-hacia-la-pelota se cierra por VISION, no por el heading absoluto del BNO. Un BNO congelado solo aporta un offset constante que se cancela; no abre este lazo. El integral (ki=0.05) carga contra ball_angle_rel decreciente, no contra un error constante. Por lo tanto no hay spin descontrolado ni lazo abierto por esta via.
- **(control) Convencion de signo de omega del HeadingPID no validada contra kinematics (riesgo de runaway por inversion CCW/CW)** — La premisa central del hallazgo es falsa segun el codigo. El hallazgo afirma "el contexto YA marca que +omega gira las 3 ruedas = HORARIO fisico mientras el PID asume CCW". Pero la cadena de control esta documentada como CCW+ de punta a punta: kinematics.h:6,33 ("omega positivo = CCW visto desde arriba"), inverse_kinematics suma +omega*R (kinematics.cpp:14), y drive_straight.cpp:21-22 razona el MISMO patron que el HeadingPID ("error>0 target adelante en CCW -> omega>0 -> gira CCW -> reduce el error"). Critico: el heading sensado YA viene en CCW+. localization.cpp:29-32 y sensors_imu.cpp:14-15 documentan que el BNO055 da CW+ pero se aplica HEADING_SIGN=-1 en la FUENTE, asi que el heading que llega al PID (strategy.cpp:311,373) es CCW+. Entonces heading CCW+ -> error=wrap_diff(setpoint-heading) -> output=kp*error -> omega CCW+ -> kinematics CCW+ -> heading sube -> error baja = lazo NEGATIVO estable, no positivo. El unico "+omega=horario" del repo es diag_central_motors.cpp:103 (DIRECTION_SIGN=+1, default arbitrario de un test de motores standalone), que NO es la cadena inverse_kinematics; el hallazgo confunde el default de un diagnostico con la convencion cinematica real.
- **(control) HeadingPID no se tickea en SEARCH/PATROL/INTERCEPT/KICKOFF -> dt y prev_error stale, primer tick del proximo estado distorsionado** — Las observaciones de codigo son exactas pero el hallazgo NO describe un bug presente, por su propia admision. Confirmado: heading_pid_tick solo se llama en POSITION (311), APPROACH (373) y CLEAR (520); SEARCH (268), PATROL, INTERCEPT, KICKOFF (omega via drive_straight/set_target sin tick), LINE_AVOID y WAIT_START setean omega por otra via. Pero los 3 estados que tickean SOLO se alcanzan via transition_atk/transition_gk, que SIEMPRE llaman heading_pid_reset (167-172, 174-181). heading_pid_reset (28-32) pone primed=false, y heading_pid_tick con primed==false usa dt fallback=0.01 (43-44) y derivada=0 (57-58), ignorando last_tick_ms viejo. Dentro de un estado que tickea, los ticks son consecutivos (sin gap), asi que last_tick_ms siempre es reciente. NO existe hoy ningun camino que tickee con last_tick_ms stale. El propio hallazgo dice 'Hoy se salva por el reset en transition' y habla de 'riesgo en alguna refactor futura' y 'deuda latente' -> es una hipotesis sobre codigo futuro, no un defecto del codigo actual. Ademas, incluso en el escenario hipotetico, dt esta clampeado a 0.1s (48) y output a output_clamp (66), acotando el supuesto 'salto'. Un verificador adversarial descarta hallazgos cuyo propio texto admite que el sintoma no ocurre hoy.
- **(mainloop) EMERGENCY_LINE depende de imminent_exit que se calcula con cross_track sólo en build de banco** — La premisa central del hallazgo es falsa. EV_IMMINENT_EXIT NO depende de cross_track_mm. En down_model.cpp:213 el flag se setea con `if(g.line_present && g.sensors_on_line>=cfg.imminent_depth) ev|=EV_IMMINENT_EXIT;` — depende del conteo de sensores en blanco (sensors_on_line vs imminent_depth=6, comm_central.cpp:25), no del cross_track. Además cross_track_mm SÍ se computa en el build de competencia: dm_line_metrics() (down_model.cpp:71-93, llamado en línea 205-207 dentro de dm_update) lo deriva de la geometría real cuando n==32, fuera de cualquier ifdef. El bloque #ifdef DOWN_DEBUG_SERIAL (comm_central.cpp:123-147) sólo SOBREESCRIBE cross_track con un centroide simple para banco; no es la única fuente. Y dm_update es el path productivo: main_down.cpp:124 llama comm_central_send_line_urgent() a 200 Hz sin gate, que invoca dm_update (comm_central.cpp:113) y down_tx_broadcast_line (línea 121) fuera de todo ifdef. Por lo tanto imminent_exit se computa normalmente en competencia y world_model_imminent_exit()→lsv2_imminent_exit() (line_view.h:41) opera sobre data_valid + EV_IMMINENT_EXIT + !lifted. El freno de emergencia en main_central.cpp:127 funciona en competencia.
- **(arch) La FSM realimenta el HeadingPID con el heading CONGELADO del TOP (control de rumbo ciego)** — La lectura literal del codigo es correcta (heading = world_model_get_my_heading_deg() = g_snap.my_heading_centideg, que es el BNO del TOP, world_model.cpp:58 + main_top.cpp:64; y se usa como setpoint y como medicion en strategy.cpp:310-311, 372-373, 518-522). PERO la tesis central del hallazgo es matematicamente FALSA. El error del HeadingPID es wrap_diff(setpoint, measurement) (pids.cpp:40). En los tres estados: APPROACH -> wrap_diff(heading+ball_angle_rel, heading) = ball_angle_rel; POSITION -> wrap_diff(heading+goal_angle, heading) = goal_angle; CLEAR -> wrap_diff(my_heading+ball_angle_rel, my_heading) = ball_angle_rel. El termino 'heading' se CANCELA exacto. El error queda manejado puramente por el angulo RELATIVO de la pelota/arco (ball_angle_rel = atan2(bx,by), goal_angle), que viene de las CAMARAS del TOP (live), no del BNO. omega se aplica en marco-cuerpo (kinematics.cpp:14, omega*R) sobre la misma referencia de cuerpo que atan2(bx,by). Conclusion: un BNO congelado NO afecta este lazo porque se cancela; el lazo nunca dependio del BNO. La afirmacion 'si el robot rota fisicamente el PID no lo ve' es al reves: si el robot rota, la camara ve la pelota en otro angulo relativo, ball_angle_rel cambia, y el PID SI corrige. Tampoco hay 'sesgo constante' en la integral: integra ball_angle_rel, independiente del offset congelado. El unico estado que usaria heading absoluto (KICKOFF) usa OTOS (otos_heading) para target y medicion (strategy.cpp:227-228), y tambien se cancela. No existe ningun heading-hold absoluto contra el BNO en la FSM.

## Atribución

Workflow + análisis + redacción: Claude Opus 4.8 (Anthropic), sesión 2026-06-03 (6 agentes revisores + verificación adversarial). Pedido + scope: Gustavo Viollaz (@gviollaz). Etapa 1 (análisis); las TASKs se crean en Etapa 2 al decidir qué implementar.
