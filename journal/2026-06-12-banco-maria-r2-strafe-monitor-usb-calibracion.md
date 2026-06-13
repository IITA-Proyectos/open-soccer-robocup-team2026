---
title: "Banco práctica 2026-06-12 — María/R2: arquero strafe (v1→v6 PFM), monitor USB en competencia, fix sensores débiles, calibración línea"
date: 2026-06-12
author: "María Virginia Viollaz (banco) + Claude (firmware/coach)"
status: sesión EN CURSO al momento del corte (cambio de chat por contexto) — v6 compilada SIN flashear/probar
---

# Banco 2026-06-12 — María con ROBOT 2 (día completo)

## Validado en hardware HOY ✅

1. **Botón fantasma del Zircon (pin 9)**: el pulsador onboard quedó clavado → GO
   permanente (el robot patrullaba al prender; el STOP se re-disparaba solo).
   Mitigado por software: flag `CENTRAL_MANUAL_START_NO_BUTTON` + envs `*_nobtn`.
   Pendiente hardware: revisar/cambiar el pulsador.
2. **Monitor USB EN el binario de competencia de DOWN** (pedido María, TASK-306):
   `-DDOWN_USB_MONITOR` en envs `down`/`down_robot2`. Validado en banco 3/3:
   dormido al boot ✅ · streamea 20 Hz con la app (STREAM ON + PING keepalive) ✅
   · **se apaga solo a los ~3 s sin host** (sacar cable = modo partido) ✅.
   + Fixes TASK-306: CAL_* re-deriva el DownModel EN VIVO (antes: hasta reboot),
   CAL_SAVE con ACK/NAK, CAL_AUTO_OFF con sanity-check, getters por-OTOS gateados
   también para el monitor.
3. **App monitor-base**: keepalive PING 1 s + STREAM ON al conectar; título con
   fuente; banner rojo SIMULADOR; guarda anti-comandos-en-sim; panel CALIBRAR
   guiado (1·Verde 2·Blanco 3·Guardar) + grilla semáforo 32 sensores + veredicto.
   82/82 pytest. La app del juez de línea para Corea quedó utilizable sin IA.
4. **Fix sensores débiles** (raíz del "no detecta línea"): S01/S08 de R2 son
   físicamente flojos (~70 counts de rango vs ~280) y UN débil invalidaba TODA la
   calib (`lc_is_suspect`) → robot ciego de línea. Ahora `lc_count_weak` +
   exclusión por-sensor del centroide (patrón EV_SENSOR_NOISY); data_valid=1
   hasta `max_weak_sensors=4` débiles (EV_CALIB_SUSPECT avisa); 5+ invalida como
   siempre. Default 0 = semántica histórica (tests viejos intactos). +9 tests.
5. **Calibración de línea de R2 guardada** (EEPROM + respaldo en
   `docs/pruebas-banco/datos-banco-2026-06-12/calib-linea-r2-2026-06-12.txt`):
   29/32 sensores con margen ≥40 (S01/S08 débiles físicos, S19 al límite).
   Lección de método: el calibrador captura el blanco INSTANTÁNEO → hoja blanca
   grande cubriendo TODO el anillo, sin sombras (26 malos → 1 con la hoja bien).
6. **Arquero "strafe simple"** (`central_robot2_arquero_strafe_bb`, gateado
   `GK_SIMPLE_STRAFE`): v3/v4 con pausas VALIDADA en banco por María — strafe
   lateral, rebote por línea con **ESCAPE de ~12 cm SIN leer sensores antes de
   decidir** (fix diagnosticado POR MARÍA: parado sobre la línea hacía "cosas
   raras"), frente al arco rival por pulsos en pausas.

## La saga del control de rumbo (planta IDENTIFICADA con datos)

- ω continuo capado 40°/s durante strafe 200 mm/s → **runaway** (pierde contra
  la deriva parásita ~80°/s). ω continuo kp=3 capado 120°/s → **oscilación
  violenta ±140° + trompo** (actuador cuantizado por pisos: giro todo-o-nada).
- Conclusión de ingeniería: a 200 mm/s el robot está en régimen CUANTIZADO; el
  control fino continuo clásico es inviable. **Skills nuevas** (en
  `.claude/skills/`, también para el Claude de Elías):
  `control-pid-zona-muerta` (PFM/duty-cycling, deadband, PI-feedforward,
  titración) y `dinamica-omni-3-ruedas` (la planta medida: pisos, regímenes,
  parásita, mínimos físicos, hechos validados de banco).
- **v6 implementada** según las skills: `src/shared/pfm_heading.h` (puro, 8
  tests) — PI + PFM: corrección entregada en pulsos de magnitud fija (100°/s)
  durante fracción de ventanas de 160 ms; deadband 5°; integrador anti-windup
  que APRENDE la deriva (auto-calibración); red de seguridad re-escuadre a 45°.
  **COMPILADA, NO FLASHEADA NI PROBADA — primer paso de la próxima sesión.**

## Hallazgos de hardware del día (anotar/reparar)

- ⚠️ **Cable USB del banco: 5 cortes en la sesión**, 2 veces dejó un STOP sin
  enviar (robot corriendo sin control del juez-PC). CAMBIARLO antes de seguir.
- ⚠️ **El BNO de R2 se congeló 2 veces en frío** (hdg bit-clavado, girándolo a
  mano; revive con power-cycle de 10 s). Distinto del caso R1 (que era el bus).
  Refuerza F4 del backlog (detector de muerte del BNO). **Regla de banco: antes
  de CADA prueba de movimiento, girar el robot a mano y verificar que hdg
  trackea.**
- ⚠️ **Cámara frontal de R2 muda** (pkts_F=0; la trasera anda). Pendiente.
- El robot calienta con batería conectada en reposo (revisar con Enzo).
- Conector USB de la DOWN también flojo (2 cortes).

## Continuación tarde (sesión nueva de Claude) — flasheo v6 + bloqueo USB

- **v6 FLASHEADA a la CENTRAL** (`central_robot2_arquero_strafe_bb`). Crónica:
  - 1er intento: pio **SUCCESS mintiendo de nuevo** — el uploader decía "No
    Teensy boards were found" (cero puertos COM en Windows). La regla del día
    atajó el falso positivo en el acto.
  - María cambió el cable USB → `COM17` apareció → 2º intento OK y **reboot
    REAL confirmado** (contador de loop arrancando de ~0 en el panel).
  - Marcador `state=GK_SIMPLE_WAIT` **AÚN NO VERIFICADO**: con batería OFF la
    TOP no manda snapshot y la estrategia no corre (`state=INIT` en cualquier
    binario → no discrimina). El veredicto necesita batería ON.
- ⚠️ **BLOQUEO: el USB murió otra vez al prender la batería / acomodar el
  robot, ya con el cable NUEVO, y no volvió en ~20 min de vigía.** Ya no es
  (solo) el cable: sospechosos = conector USB de la CENTRAL (tensión mecánica)
  o conflicto eléctrico batería+USB (cf. "misterio batería-mata-todo" de R1 y
  el "calienta en reposo" de R2 anotado a la mañana).
- **Experimento discriminador pendiente** (1 min, María): batería OFF + USB →
  ¿aparece el puerto? → prender batería SIN tocar nada → ¿se cae el puerto en
  ese instante? Se cae = eléctrico (frenar y hablar con Enzo). No se cae =
  mecánico (conector/tensión del cable; alivio de tensión con cinta + revisar
  soldadura del conector con multímetro).
- Estado al cierre de este bloque: v6 a bordo (sin verificar marcador), corrida
  de banco NO realizada, tune pendiente. Próximo paso al volver el USB:
  marcador → gyro a mano (`hdg` trackea) → `g` corrida corta → titración según
  skill `control-pid-zona-muerta`.

## Skill nueva: `rcj-deliverables-judge` (mientras el USB estaba caído)

- **Pedido María:** skill de coach/jurado experto para EVALUAR TDP, póster de
  ingeniería y video según rúbricas RCJ Soccer 2026 + criterios generales
  RoboCup/RCJ. Complementa a `rcj-judging-package` (productor) — esta es el
  lado EVALUADOR.
- **Método: TDD de skills** (superpowers/writing-skills). Línea base SIN skill
  (subagente jurado sobre `en/TDP.md`): acertó la escala pero **heredó el
  framing del propio TDP**, contaminó criterios (pidió replicabilidad en
  Mechanical cuando es puerta de Electrical), inventó metodología ("promedio
  honesto: 3"), cero citas verbatim, aritmética difusa. CON skill: puertas
  aplicadas (Mechanical 1/5 defendido con descriptor citado), VERIFICADO vs
  DECLARADO, 8 hallazgos adversariales nuevos y reales sobre el TDP (párrafos
  duplicados §1.1/§1.2, §6 contradice §1.8, branches citadas inexistentes,
  rutas rotas, LICENSE inconsistente, 57-vs-59-vs-83 envs).
- **Grounding:** rúbrica oficial capturada VERBATIM 2026-06-12 de
  robocup-junior.github.io/soccer-rules (scoring + reglas 2026 + política de
  mentores/students-do-the-work) → `references/rubrica-oficial-2026.md`.
- **Bonus del test GREEN:** la evaluación del subagente quedó como pre-review
  real del TDP — los 8 hallazgos adversariales son accionables para los P0 de
  entregables (alimentan MEJORAS-PENDIENTES).
- CLAUDE.md actualizado: lista de skills 8→11 (sumadas también las 2 de control
  del día que habían quedado sin registrar).

## Skills nuevas (2): `rcj-doc-voz-estudiante` + `rcj-diagramas-poster`

- **Pedido María:** lado PRODUCTOR de máximo puntaje — documentar programas,
  sintetizar la esencia de sistemas complejos, diagramas modulares simples y
  póster, con cerebro de ingeniero senior y VOZ de estudiante de 18.
- **Método: TDD de skills, una por vez** (RED→GREEN cada una):
  - **Voz/redacción** — baseline: contenido bueno pero voz de profesor/IA,
    jerga desnuda (FSM, PID, histéresis sin explicar), cero anécdota, sin
    esencia-primero. Con skill: "tres computadoras que se pasan notas… 31
    bytes, 100 veces por segundo", toda la jerga explicada, anécdota REAL del
    repo (pines 28/29 inexistentes en el borde del 4.0), datos fechados,
    cierre honesto (TASK-022).
  - **Diagramas/póster** — baseline: figura sofisticada PERO texto de 13 px
    (≈10 pt impreso, viola el ≥24 pt de POSTER.md), 12+ unidades duplicando
    zonas, SVG entregado a ciegas, cifra de tests stale, archivo dropeado en
    `assets/` sin avisar. Con skill: matemática de impresión ANTES de dibujar
    (393 mm de la grilla real → cuerpo 40 px), exactamente 7 unidades, **el
    render obligatorio (Edge headless, probado en esta máquina) cazó una
    colisión título/leyenda real** que se corrigió antes de entregar, cifra
    viva fechada (827/60/0 del gate de hoy), drafts declarados en
    `assets/drafts/`.
- **Subproducto útil:** `docs/competencia/assets/drafts/fig2_sistema_3_placas.svg`
  (+ render PNG) — candidata a Fig.2 del póster (alimenta P0-4). En ES; falta
  traducir textos al EN al maquetar. El draft RED (`fig2_system_architecture.svg`,
  con violaciones de tamaño y cifras viejas) se borró — superado.
- CLAUDE.md: registro de skills 11→13.

## Skill nueva: `arquitectura-robotica-topdown` (lente de arquitecto senior)

- **Pedido María:** comportarse como arquitecto senior de sistemas — top-down,
  capas de abstracción, integración multi-dominio (mecánica/eléctrica/potencia/
  control/visión/localización/SLAM/comms/fail-safe), con foco en SÍNTESIS y
  simplificación. Es el escalón UPSTREAM de las skills de doc/diagrama: produce
  el MODELO que ellas expresan.
- **TDD de skills — y el RED fue revelador:** la línea base (con opus-4-8[1m],
  modelo fuerte) ya producía un análisis bueno, pero cometió los 5 errores que
  separan a un generalista de un arquitecto: (1) mezcló capas de abstracción con
  concerns transversales (puso potencia/comms como "capas"); (2) listó riesgos
  pero NO caminó las costuras entre dominios; (3) no nombró la restricción
  dominante; (4) síntesis al final, no al principio; (5) lente de control
  superficial. La skill targetea exactamente esos 5.
- **GREEN:** con la skill, separó capas (vertical, con test de ocultamiento) de
  concerns (horizontal, con el por-qué-no-son-capas), **caminó 6 costuras reales
  ancladas a incidentes del repo** (brownout disfrazado de SW, BNO que MIENTE vs
  muere, contrato de línea roto en silencio, TOP 6 Hz, pelota fantasma, régimen
  cuantizado de rueda), nombró el cuello (percepción calibrada = TASK-022),
  esencia-primero, lentes por dominio aplicadas.
- **Subproducto:** la vista de arquitectura del GREEN es publicable como doc
  canónico si el equipo lo quiere (hoy NO se versionó; sería `docs/` + update de
  FUENTES-DE-VERDAD/ESTADO-ACTUAL en el mismo commit, por protocolo).
- CLAUDE.md: skills 13→14. **Cierra la familia de 4 skills de competencia:**
  arquitectura (modelo) → diagrama (`rcj-diagramas-poster`) → prosa
  (`rcj-doc-voz-estudiante`) → juez (`rcj-deliverables-judge`).

## Skills nuevas (2): `ia-educacion-no-trampa` + `ensenar-con-analogias-y-motivar`

- **Pedido María/Gustavo:** ingeniero senior dedicado a la EDUCACIÓN — enfoque
  pedagógico, enseñar lo complejo con analogías simples, motivar jóvenes,
  fanático de la fast/experimental engineering (vibe coding/3D/PCB, impresión
  3D, CNC, simuladores), y **defender a muerte la IA como base educativa** (no
  trampa = EDUCACIÓN para el futuro), plasmado en skills para que la
  documentación MUESTRE y DEFIENDA el uso de IA en vez de esconderlo.
- **Decisión: 2 skills (triggers distintos), TDD cada una:**
  - **`ia-educacion-no-trampa`** (postura/argumentos) — RED: defensa competente
    pero apologética, rodea la acusación de "trampa", sin argumento
    fáctico/laboral, sin pasar a la ofensiva. GREEN: confronta la frase del
    profesor de frente, reframe CÓMO-no-QUÉ, "prohibir es la falla pedagógica",
    ancla en evidencia real (I²C, +650 tests, journal), guardarraíl de
    honestidad (declara límites), cierra con tesis de futuro. Referencia:
    arsenal de argumentos (histórico/calculadora, fáctico/laboral, esfuerzo,
    transparencia, rebatir objeciones). Generaliza `USO-DE-IA.md` a CUALQUIER
    doc.
  - **`ensenar-con-analogias-y-motivar`** (método pedagógico) — RED: cálido pero
    con 3 analogías decorativas diluidas, "vos podés" afirmado, todo contar sin
    invitar a pensar, fast-engineering como folleto. GREEN: UNA analogía
    (jugadora de fútbol) sostenida + mapeo uno-a-uno + flag de dónde se rompe
    (números no palabras) + predicción ganable (tapar los ojos) + primera
    victoria diseñada (cambiar un número y verlo en el simulador esta semana).
    Referencia: cómo hacer contagiosa la ingeniería rápida.
- **Anclaje:** ambas consistentes con `USO-DE-IA.md` (postura institucional ya
  fijada) y `AI-INSTRUCTIONS.md` (atribución). NO contradicen; generalizan.
- CLAUDE.md: skills 14→16, nueva categoría "Pedagogía y postura educativa".
- **Familia de skills del día cerrada (8 nuevas):** arquitectura (modelo) →
  diagrama → prosa → juez (entregables) + postura-IA + pedagogía (educación) +
  las 2 de control (movimiento). Todas TDD (RED documentado + GREEN verificado),
  todas aditivas (cero cambio de firmware), gate host verde.

## Proceso

- **Regla nueva aprendida (memoria + aplicada todo el día): el SUCCESS de pio
  upload NO garantiza que el binario llegó** — verificar SIEMPRE el panel serial
  con un marcador único del binario antes de juzgar conducta (nos comió ~40 min
  con la patrulla vieja corriendo disfrazada de strafe).
- Gates: host 59 suites / 819+8 tests verdes (down_calib 17, down_model 25,
  pfm_heading 8); pio: 5 envs DOWN + strafe_bb + regresiones SUCCESS; app 82/82.
- TASK-306 creada (app confiable para Incheon). PR #18 mergeado a la mañana.
- Commits del día a nombre de María (git config local del repo).
