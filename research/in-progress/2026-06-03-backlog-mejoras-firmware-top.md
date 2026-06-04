---
title: "Backlog priorizado de mejoras del firmware TOP (análisis multi-agente 2026-06-03)"
date: 2026-06-03
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8, Anthropic) — workflow 16 agentes"
status: in-progress
tags: [top-board, firmware, backlog, auditoria, bugs, mejoras, incheon]
robot: ambos
area: firmware
tipo: analisis
---

# Backlog priorizado de mejoras — firmware TOP

> **Qué es esto.** Análisis profundo del firmware TOP por un workflow de 16
> agentes (auditoría en 6 dimensiones + verificación adversarial contra el
> código vivo). 76 hallazgos crudos → 24 ítems priorizados. **Esto es la ETAPA 1
> (identificar + evaluar).** La ETAPA 2 (decidir qué implementar) la hacemos con
> Gustavo sobre esta lista.
>
> **Cada ítem trae:** esfuerzo · riesgo-no-fix · riesgo-fix · beneficio · si
> requiere intervención humana · cómo se evalúa · si requiere testeo humano.
>
> ⚠️ **Moratoria (CLAUDE.md):** esto es backlog priorizado, NO mandato de
> ejecución masiva. La mayor relación valor/esfuerzo HOY es **desbloquear las
> TASK P0 abiertas del equipo (022/014/015)**, no abrir frentes nuevos.

## Diagnóstico central

El firmware TOP **percibe bien** pero tiene **tres familias de deuda** que pueden
costar partidos en Incheon:

1. **Timing / robustez del uplink.** El `pulseIn` del HC-SR04 (12 ms bloqueante)
   se come el presupuesto de 10 ms del snapshot a 100 Hz y desborda el RX de
   DOWN/CENTRAL en silencio (ring 64 B se llena en ~2.8 ms @230400) corrompiendo
   odometría. Sin watchdog de hardware ni instrumentación del período del loop.
2. **Riesgos directos de partido.** Polaridad de arco hardcodeada (autogol
   sistemático ~50% de asignaciones de lado), rol por dipswitch nunca leído,
   cámara sin CRC, calibración cámara y `TOF_OFFSET_MM` sin medir.
3. **Fusión desconectada.** `pose_fusion`/`pose_filter`/OTOS están construidos y
   testeados pero **huérfanos** — ningún `src/` los invoca; CENTRAL navega sin
   posición teniendo la OTOS disponible.

## Resumen por prioridad

| Prio | # | Tema |
|------|---|------|
| **P0** | 5 | arco+rol, cámara operativa (TASK-022), CRC cámara (TASK-015), loop HC-SR04 (TASK-014), calibración placeholders |
| **P1** | 8 | fusión OTOS, debounce árbitro, watchdog, heading-init, ToF stale, SEQ link-health, IMU heartbeat, doc-contrato |
| **P2** | 11 | slip, admin CENTRAL, sentinel cámara, clamp pose, ball-vel boot, timeouts, ACK comandos, doc-varios, dead-code, HW-validations |

**Auto-ejecutable por Claude (cero hardware):** solo los 2 ítems de docs
(`P1-DOC-CONTRATO` + `P2-DOC-VARIOS`). El resto toca el path vivo o depende de
medición/decisión física → requiere humano. Nada que cablee la fusión, toque
`main_top` o dependa de medición es auto-ejecutable.

---

## P0 — bloqueantes / riesgo de partido perdido

### P0-ARCO-ROL · Polaridad de arco hardcodeada + rol por dipswitch nunca leído
- **Tipo:** bug. **Qué:** `build_snapshot` fija `goal_opp=yellow/goal_own=blue`
  sin señal de play-side; `PIN_ROLE_DIPSWITCH` nunca se lee con `digitalRead`.
- **Esfuerzo:** M (6-10 h firmware + cableado + banco).
- **Riesgo-no-fix:** si toca el lado invertido, el robot **ataca su propio arco →
  autogol en ~1 de cada 2 partidos**. Sin rol leído, CENTRAL no diferencia
  arquero/delantero al boot. Cuesta partidos enteros.
- **Riesgo-fix:** medio en integración: hay que decidir el mecanismo (dipswitch
  vs referee play-side vs botón); tocar la FSM de arranque sin probar ambos lados
  puede dejar un estado inicial mal definido.
- **Beneficio:** elimina la causa #1 de partidos perdidos por geometría invertida.
- **🔴 Requiere intervención:** DECISIÓN de Enzo/equipo sobre el mecanismo +
  cableado del dipswitch + **testeo humano** en banco con ambos lados.
- **Evaluación:** banco — setear lado A, confirmar que `goal_opp` apunta al arco
  rival correcto; repetir lado B; verificar que strategy diferencia rol.

### P0-CAM-OPERATIVA · Cámara OpenMV no endurecida (TASK-022) — 🟡 ~70% (avance 2026-06-03)
- **Tipo:** mejora. **Qué:** percepción depende de scripts OpenMV sin sentinel,
  sin lock de exposición, sin calibración mm/LAB. **Esfuerzo:** L (~24 h).
- **✅ Avance 2026-06-03 (banco):** detecta pelota + arcos; colores calibrados
  (azul/amarillo/naranja); **lente ultra-wide** colocado, "anduvo muy bien".
- **🔴 FALTA para cerrar:** (1) verificar **estabilidad** (10 min sin perderla,
  sin falsos positivos) + **sentinel** (cámara tapada → no fantasma); (2) **lock
  de exposición/WB/gain**; (3) calibración de distancia (→ P0-CALIB, ahora más
  urgente por el ultra-wide); (4) idealmente, luz tipo Incheon.
- **Riesgo-no-fix:** sin esto el robot **no ve la pelota** confiable bajo la luz
  de Incheon → no compite. Es el bloqueante más cercano a "no juega".
- **Riesgo-fix:** medio — sobre-ajustar a una luz y fallar en otra; mitigable con
  `openmv-vision-tuning` + datasets en varias luces.
- **🔴 Requiere intervención:** equipo con cámaras montadas + pelota + cancha con
  iluminación representativa. **Testeo humano** obligatorio. **La cierra el equipo.**
- **Evaluación:** pelota a 30/50/80/100 cm, detección estable + distancia mm en
  tolerancia; lock de exposición en 2-3 luces; 0 crashes en 10 min.

### P0-CAM-CRC · Enlace cámara→TOP sin CRC ni fin de trama (TASK-015)
- **Tipo:** mejora. **Qué:** parser valida solo headers 201/202/203; los 6 bytes
  de datos sin checksum; rango de datos (0..200) solapa con HEADER1=201.
  **Esfuerzo:** M (8-12 h, ambos extremos + tests host).
- **Riesgo-no-fix:** un bit-flip = pelota fantasma indetectable; único enlace del
  robot sin integridad de datos.
- **Riesgo-fix:** bajo (aditivo, host-testeable); el cuidado es flashear ambos
  extremos en el mismo deploy para no romper el framing.
- **🔴 Requiere intervención:** flasheo simultáneo OpenMV + TOP; **testeo humano**
  en banco. **Parte se puede adelantar host:** el parser nuevo con tests de
  frames corruptos inyectados SÍ es host-testeable (Claude puede prepararlo).
- **Evaluación:** host — frames corruptos deben rechazarse; banco — contar
  `crc_errors`/`resyncs` en 10 min.

### P0-LOOP-HCSR04 · pulseIn bloquea 12 ms y desborda el RX en silencio (TASK-014)
- **Tipo:** bug. **Qué:** con `-DTOP_ENABLE_HCSR04` ACTIVO en `top_robot1/2`,
  `pulseIn` bloquea hasta 12 ms cada ~90 ms; el snapshot apunta a 10 ms; el ring
  RX 64 B @230400 se llena en ~2.8 ms → se pierde odometría OTOS **sin disparar
  LOST**. ⚠️ Contradicción doc-vs-código: los docs dicen "HC-SR04 gateado OFF"
  pero el flag se reactivó el 2026-06-02. **Esfuerzo:** M-L (8-16 h).
- **Riesgo-no-fix:** jitter de ~12 ms en ~1 de cada 9 ciclos del uplink +
  corrupción silenciosa de odometría. Bloquea el dimensionamiento de las ventanas
  de frescura (TASK-017).
- **Riesgo-fix:** medio — pasar `pulseIn` a interrupción toca `sensors_tof` del
  path vivo. Gatear OFF es trivial pero pierde el sensor frontal. **Aumentar el
  ring RX (`addMemoryForRead`) es aditivo y de bajo riesgo — hacer en paralelo.**
- **🔴 Requiere intervención:** osciloscopio para medir el período real (pasos
  3-5 de TASK-014) + **testeo humano**. Decisión de scope: no-bloqueante vs OFF.
- **Evaluación:** banco — toggle GPIO + osciloscopio, período min/avg/max con y
  sin eco; contar bytes perdidos del RX; confirmar período <10 ms p99.

### P0-CALIB-PLACEHOLDERS · Factor cámara 10.0 y TOF_OFFSET_MM 95 sin calibrar — ⬆️ MÁS URGENTE (lente ultra-wide 2026-06-03)
- **Tipo:** mejora. **Qué:** `CAMERA_UNIT_TO_MM=10.0` y `TOF_OFFSET_MM=95mm` son
  placeholders. **Esfuerzo:** S-M (4-6 h, el trabajo es físico).
- **⚠️ 2026-06-03:** se colocó **lente ultra-wide** → distorsión de barril. El
  factor `unit→mm` y el mapeo de ángulos viejos son de OTRO lente y **ya no
  valen**. La recalibración con el ultra-wide puesto pasa de "deseable" a
  **obligatoria** antes de confiar en distancia/ángulo de pelota y arcos
  (verificar el ángulo en varios puntos del FOV, no solo al centro).
- **Riesgo-no-fix:** distancias de approach mal → el robot frena/acelera mal
  cerca de la pelota; la pose ToF arranca con offset fijo erróneo sesgando toda
  la trilateración. Calibrar localización (TASK-035) sin esto es construir sobre
  arena.
- **Riesgo-fix:** muy bajo (son constantes); el riesgo es medir mal.
- **🔴 Requiere intervención:** **MEDICIÓN FÍSICA con cinta** (pelota a varias
  distancias; radio plano-sensor→centro). Claude no puede medir. **Testeo humano.**
- **Evaluación:** pelota a distancias conocidas, ajustar factor hasta error <10%;
  medir radio y validar pose contra posición conocida.

---

## P1 — robustez del uplink y fusión

### P1-FUSION-OTOS · OTOS + pose_fusion/pose_filter construidos pero NUNCA cableados
- **Tipo:** mejora. **Qué:** `comm_down` cachea la OTOS pero `build_snapshot` solo
  usa ToF+IMU; `pose_fusion`/`pose_filter` existen testeados pero ningún `src/`
  los invoca. **Esfuerzo:** L (12-20 h).
- **Riesgo-no-fix:** sin posición confiable continua; la odometría suave de
  100 Hz que mata el ruido del ToF se descarta. Mejora de mayor impacto
  estructural pendiente.
- **Riesgo-fix:** **alto** — toca la lógica de fusión de `main_top`; un filtro mal
  ajustado da pose suave-pero-derivada que confunde más. Requiere datos de cancha
  para sintonizar. **Por eso explícitamente NO auto-ejecutable.**
- **🔴 Requiere intervención:** **datos de cancha** para sintonizar + decisión
  TASK-033 (cuántos ToF) + **testeo humano** de que la pose fusionada sigue la
  real. *(Este es el que planeamos hacer DESPUÉS de tu captura de cancha.)*
- **Evaluación:** host — tests `pose_fusion` ya verdes; cancha — trayectoria
  conocida, pose fusionada vs ground-truth, anclaje ToF corrige deriva OTOS.

### P1-ARB-DEBOUNCE · Árbitro GPIO sin debounce ni latch
- **Tipo:** mejora. **Qué:** `read_referee_gpio` setea `match_running` con el OR
  instantáneo de pines 5/6, sin debounce. **Esfuerzo:** S (2-4 h).
- **Riesgo-no-fix:** un glitch de EMI de motores → START espurio en STOP
  (**moverse en STOP es penalizable**) o STOP a mitad de jugada.
- **Riesgo-fix:** bajo (aditivo, host-testeable); +20-50 ms de latencia
  despreciable. Toca `comm_arbiter` del path vivo → validar en banco.
- **🟡 Parcial:** la lógica del debounce es **host-testeable** (Claude la prepara)
  pero el estado de juego es crítico → **testeo humano** en banco con señal real.
- **Evaluación:** host — glitch de 1 tick ignorado; banco — inyectar
  transitorios con motores corriendo, `match_running` no cambia espurio.

### P1-WDT · Sin watchdog de hardware
- **Tipo:** bug. **Qué:** `setup`/`loop` no instalan WDT_T4 ni `Wire.setWireTimeout`;
  un cuelgue de I2C deja el loop mudo sin auto-reset. **Esfuerzo:** S (2-4 h).
- **Riesgo-no-fix:** un cuelgue de I2C en partido deja al robot mudo el resto del
  match (CENTRAL frena por fail-safe pero el robot queda parado).
- **Riesgo-fix:** bajo-medio — timeout muy corto resetea en stalls legítimos
  (HC-SR04/BNO 12 ms); debe ser holgado (500 ms-1 s). Toca setup/loop.
- **🔴 Requiere intervención:** **testeo humano** — confirmar 0 resets espurios en
  30 min + forzar cuelgue I2C (desconectar sensor en caliente) y ver auto-reset.

### P1-HEADING-INIT · Heading fusionado arranca en 0 y se envía como real
- **Tipo:** bug. **Qué:** `build_snapshot` envía siempre el heading sin gatear por
  validez; al boot (o si ambos BNO fallan el init) CENTRAL recibe heading=0 como
  medición. El `WorldSnapshot` no tiene flag `heading_valid`. **Esfuerzo:** S-M.
- **Riesgo-no-fix:** al arranque el robot orienta ~50 ms (o indefinido) con
  heading falso de 0.
- **Riesgo-fix:** medio — agregar flag toca el **contrato** (`shared/types.h` +
  CENTRAL). Coordinación cross-placa. NO auto-ejecutable.
- **🔴 Requiere intervención:** decisión de contrato + coordinación con agente
  CENTRAL + **testeo humano** del arranque.

### P1-TOF-STALE · ToF sin marca de frescura
- **Tipo:** mejora. **Qué:** `sensors_tof_tick` mantiene el último valor si
  `getRangingData` falla, **sin timeout**; un ToF colgado en 80 mm se propaga
  para siempre como `min_obstacle_mm`. **Esfuerzo:** S-M (3-5 h).
- **Riesgo-no-fix:** CENTRAL frena/evade contra un fantasma toda la partida. Modo
  de falla silencioso y persistente.
- **Riesgo-fix:** bajo (aditivo, host-testeable); elegir timeout holgado vs 15 Hz
  (~200-300 ms). Toca `sensors_tof` del path vivo.
- **🟡 Parcial:** lógica **host-testeable** (Claude la prepara); **testeo humano**
  para forzar un ToF colgado y ver que expira a `TOF_NO_READING`.

### P1-SEQ-LINK-HEALTH · SEQ se transmite pero nunca se consume en RX
- **Tipo:** mejora. **Qué:** `comm_down/arbiter/central` escriben `g_send_seq++`
  pero **jamás leen `f.seq`** en RX; no hay `g_frames_lost`. **Esfuerzo:** S-M.
- **Riesgo-no-fix:** un enlace que pierde frames (cable flojo, vibración) pasa
  silencioso mientras lleguen dentro de los 500 ms. No hay diagnóstico en mesa.
- **Riesgo-fix:** **bajo** — aditivo, solo agrega contadores, no cambia el path
  vivo. Casi auto-ejecutable; conviene validar contra DOWN/CENTRAL reales.
- **🟡 Parcial:** **host-testeable** (Claude lo prepara); **testeo humano** para
  confirmar `frames_lost=0` en enlace sano y que sube al degradarlo.

### P1-IMU-HEARTBEAT · sensors_imu asume present sin re-chequear ACK
- **Tipo:** mejora. **Qué:** `present=g_ready[i]` sin sondeo I2C; un BNO que muere
  en caliente sigue "presente" → heading congelado sin failover. **Esfuerzo:** M.
- **Riesgo-no-fix:** failover roto — el robot orienta con un heading muerto.
- **Riesgo-fix:** medio — transacciones I2C periódicas reintroducen parte del
  costo que el band-aid evitó; medir impacto en el período del loop.
- **🔴 Requiere intervención:** **testeo humano** — desconectar un BNO en caliente
  y ver failover; medir período del loop.

### P1-DOC-CONTRATO · Contratos de datos TOP describen estado anterior — ✅ HECHO (parcial)
- **Tipo:** doc-desactualizado. **Estado:** ✅ **corregido hoy** en
  `docs/firmware/CONTRATO-DATOS-TOP.md` (pose real, conf 70, ToF/ball_vx real,
  árbitro GPIO) + banner de "stale → ver canónica" en la copia del board-pack.
- **Esfuerzo:** S (hecho). **Auto-ejecutable: SÍ** (era doc verificable).
- **🟢 NO requiere intervención ni testeo humano.**

### P1-GOALOWN-VISUAL · goal_own solo envía visible + min_obstacle sin dirección
- **Tipo:** mejora. **Qué:** de `goal_own` solo se manda `visible` (se descarta
  ángulo/distancia que cameras_runtime SÍ expone); `min_obstacle_mm` colapsa 4
  ToF a un escalar sin bearing. **Esfuerzo:** M (6-10 h).
- **Riesgo-no-fix:** se pierde un 2º punto de referencia visual + el robot no
  esquiva direccionalmente (solo frena).
- **Riesgo-fix:** medio — amplía el contrato (`shared/types.h` + CENTRAL).
- **🔴 Requiere intervención:** coordinación con CENTRAL + **testeo humano**.

---

## P2 — mejoras y limpieza (resumen)

| ID | Tema | Esf. | Auto-ejec. | Requiere humano |
|----|------|------|-----------|-----------------|
| P2-SLIP-PROPAGATE | propagar `slip_estimate` OTOS al snapshot (detección de choque) | S-M | no | coord. CENTRAL + banco |
| P2-CENTRAL-ADMIN | `comm_central` descarta `CENTRAL_RESET_TOP/CMD` (canal control muerto, latente) | S | no | decisión + banco |
| P2-CAM-VISIBLE-SENTINEL | `(0,-100)` real colisiona con sentinel "no visible" (falso-neg en captura) | S | no | se cierra con P0-CAM-CRC |
| P2-LOCALIZ-CLAMP | pose sin clamp a cancha → puede emitir pose fuera de cancha con conf=70 | S | no* | latente hasta 4 ToF valid |
| P2-BALLVEL-BOOT | `sample_ms=0` al boot puede sembrar el estimador (guard lo salva, frágil) | S | no* | banco mínimo |
| P2-TIMEOUT-UNIFY | timeouts inconsistentes (500/1000 ms) sin criterio; depende de TASK-014 | S | no | medición período loop |
| P2-PROTO-ACK-CMD | comandos de evento único sin ACK ni reintento (reset OTOS/calib) | M | no | coord. DOWN + banco |
| **P2-DOC-VARIOS** | **comentarios stale + TASK-204 colisión + código muerto** | **S** | **SÍ** | **no** |
| P2-DEADCODE-CLEANUP | motion_target sin callers, flags bit0/2, LineStatusV2.quality placeholder | var. | no | coord. CENTRAL |
| P2-HW-VALIDATIONS | cluster TASKs solo-humano: signos cámara/BNO (202), zonas ToF (203), XSHUT (038), cuántos ToF (033), trilateración (035) | var. | no | **MEDICIÓN/DECISIÓN humana** |

> *no\* = host-testeable de bajo riesgo, pero toca path vivo → validación de banco
> recomendada antes de confiar.

**Nota P2-HW-VALIDATIONS:** el más peligroso es **TASK-202 (signos izq/der cámara
y sentido BNO)** — si están invertidos el robot se mueve al lado contrario (gol en
contra) y **compila igual**. Y **TASK-033 (cuántos ToF)** bloquea el scope de todo
el firmware ToF y ya venció su fecha. Prioridad de ejecución humana sugerida:
**033 → 202 → 038/035**.

---

## Lo único auto-ejecutable por Claude (sin hardware)

1. **`P1-DOC-CONTRATO`** — ✅ hecho hoy (contrato corregido + banner en board-pack).
2. **`P2-DOC-VARIOS`** — pendiente: corregir comentarios stale (`sensors_tof.cpp`
   cabecera "1 ToF", `sensors_imu.cpp` "~2s"→~10s, comentario del loop), cerrar/
   renumerar la colisión TASK-204, marcar código muerto. Bajo riesgo, alto valor
   de mantenimiento. **Lo puedo hacer cuando digas.**

Todo lo demás toca el path vivo, el contrato cross-placa, o depende de medición/
decisión física → **requiere tu intervención** (decisión y/o testeo en hardware).

## Para la ETAPA 2 (decidir qué implementar) — mi recomendación honesta

Dado el calendario (~27 días a Incheon) y la moratoria, el orden de mayor
valor/esfuerzo:

1. **Desbloquear las P0 del equipo** (TASK-022 cámara, TASK-014 loop, TASK-015
   CRC) — son las que están entre "compite" y "no compite". Requieren tu hardware.
2. **P0-ARCO-ROL** — barato en concepto, evita autogoles. Necesita tu decisión de
   mecanismo.
3. **P0-CALIB-PLACEHOLDERS** — 1 hora de cinta métrica que desbloquea toda la
   calibración de localización.
4. **P1-FUSION-OTOS** — el de mayor impacto estructural, pero **después** de tu
   captura de cancha (lo planeamos así).
5. **Los host-testeables de bajo riesgo** (debounce árbitro, ToF stale, SEQ
   health, WDT) — los puedo preparar yo con tests; vos validás en banco.

## Cambios de estado
- 2026-06-03: creado por Claude (workflow 16 agentes) a pedido de Gustavo. Etapa 1
  (identificar + evaluar) completa. Etapa 2 (decidir) pendiente con el coach.
