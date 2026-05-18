---
title: "Verificación independiente del protocolo de comunicaciones propuesto — fallas, deadlocks, saturación"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [comunicacion, electronica, decision, verificacion, ambos]
robot: ambos
area: comunicacion
tipo: analisis
related: [docs/decisions/2026-05-18-protocolo-comunicaciones-entre-placas.md]
---

# Verificación independiente — comunicaciones entre placas

> Trabajo de validación **adversarial e independiente** (2 revisores sin sesgo con
> la propuesta) sobre `docs/decisions/2026-05-18-protocolo-comunicaciones-entre-placas.md`.
> Objetivo: romper el diseño, encontrar deadlocks, saturación, puntos de falla.
> **Resultado: la propuesta NO debe ejecutarse tal como está.** Diagnóstico correcto,
> pero el plan de acción tiene fallas críticas — varias mitigaciones no cubren lo
> que dicen cubrir y al menos una (`Serial.clear()`) **agrega** un fallo nuevo.

## 1. Hallazgos críticos (los 2 revisores convergieron)

### V-C1 — Bajar el fail-safe a 150 ms ES PELIGROSO con el loop bloqueante actual
El loop de TOP se bloquea hasta **25 ms** en `pulseIn` del HC-SR04
(`sensors_tof.cpp:31`, timeout 25000 µs, caso común: sin eco en cancha abierta) +
I2C bloqueante del BNO055 (`sensors_imu.cpp:53-57`). Con buffer RX por defecto del
Teensy (~64 B), el RX **desborda a los ~23 ms** y corrompe odometría **en silencio**
(no dispara LOST, hay tráfico, solo corrupto). Las ventanas de la tabla §3.3
(T_OK=50 ms) son **inalcanzables** con este loop, y bajar el timeout de motores de
500→150 ms (§3.4/§5) **introduce paradas de motor espurias en pleno partido**.
El criterio del doc "no bloquear más que T_LOST" es el límite **equivocado**: el
límite real es el **tiempo de overflow del buffer (~23 ms)**, no T_LOST (150 ms).
→ **Orden correcto invertido:** PRIMERO sacar `pulseIn`/I2C bloqueante del loop y
**medir el período real del loop con timestamps en hardware**; recién después
fijar ventanas. La tabla §3.3 no tiene base empírica hoy.

### V-C2 — `Serial.clear()` en recuperación (§3.5/decisión 6): eliminarlo
Descarta frames válidos a medio llegar en cada reconexión y **contradice** lo que
el propio doc admite en §3.6 ("el resync del `FrameDecoder` alcanza"). Hoy **no
hay ningún `Serial.clear()`** en el código (verificado): esto es daño que el
diseño *agrega*, no que corrige. **Quitar de la propuesta.**

### V-C3 — Flapping STALE/LOST↔OK sin histéresis → robot tartamudea
Los motores del Zircon están en la **misma placa que CENTRAL** → su ruido EMI
corta el enlace que controla esos mismos motores (lazo de realimentación de
fallo). Sin histéresis (N frames consecutivos para declarar OK, definición
operativa de STALE en strategy), una microcaída cerca del umbral hace que el
robot oscile entre frenar y jugar. **La propuesta no menciona histéresis.**

### V-C4 — "Quedarse con el último" pierde emergencias y comandos one-shot
- `imminent_exit` es un **flag de evento** dentro de `LineStatus` (`types.h:45`);
  el patrón "quedarse con el último" lo **pisa** si tras un stall llegan varios
  `LINE_URGENT` (el pulso de borde queda enterrado). Necesita **OR-latch** sobre
  el drenado, no sobreescritura.
- Comandos one-shot (`CENTRAL_RESET_OTOS`, `CENTRAL_CALIB_LINE`) requieren
  procesar **todos** los frames, no coalesce. El doc eleva "quedarse con el
  último" a principio rector homogéneo → **rompe el path de comandos** que hoy
  funciona (`down/comm_central.cpp:20-35` los procesa todos, bien).
- **Conclusión:** el protocolo debe distinguir **stream** (coalesce al último) de
  **evento/comando** (latch / procesar todos). La propuesta los colapsa en uno.

### V-C5 — Verificar SEQ genera falsos packet-loss tras resync legítimo
Un glitch de CRC aislado hace saltar el SEQ sin pérdida real. Implementado como
está escrito (§4.4), dispara "modo borde conservador" sin causa. Hay que contar
gap de SEQ **solo** si no hubo `crc_error`/`resync` entre medio, y tolerar gaps
1–2 sin acción. La propuesta lo ignora.

### V-C6 — Sin `static_assert(sizeof)`; comentario de tamaño errado
`WorldSnapshot` real = **23 B**, no 24 (comentario `types.h:90` y el doc están
mal por 1 byte). El (de)serializado es `memcpy` crudo sin guarda de compile-time.
El refactor a "módulo `Link` único" (§3.7) es el **peor momento** para un
desalineo silencioso de structs entre placas si no se agregan
`static_assert(sizeof(T)==N)` como red.

## 2. Hallazgos altos

| # | Falla | ¿La propuesta la cubre? |
|---|-------|-------------------------|
| V-A1 | LOST simultáneo mundo+línea (stall de CENTRAL): `motors_stop` gana, el "modo borde conservador" del P0 4.3 **nunca corre** → el P0 estrella es inalcanzable en el fallo más común | NO — no define precedencia |
| V-A2 | `comm_down_tick` **sin cota** de bytes/tick (cámaras sí la tiene): post-stall infla la latencia de emergencia >15 ms | NO |
| V-A3 | `CENTRAL_CALIB_LINE` recibido en runtime bloquea DOWN ~320 ms (`line_ring.cpp:147-165`) → cascada LOST de 2 enlaces, autoinfligida | NO — no contempla handlers bloqueantes-largos |
| V-A4 | Migrar cámara a `proto.h`: CRC16 en MicroPython a 30 fps subestimado; **no existen tests del parser de cámara**; riesgo de regresión de fusión | SUBESTIMA |
| V-A5 | Bug de frescura al boot: son **3 sitios** en TOP (`top/comm_down.cpp:110/113/116`), no 1; no resuelto sin histéresis | Parcial |
| V-A6 | Los 5 `comm_*.cpp` **no son copia exacta**: 3 patrones de receptor distintos + funciones emisoras duplicadas con colisión de nombres + emisor multi-frame. Refactor "2-3 días" es optimista | SUBESTIMA |

## 3. Lo que la propuesta SÍ acertó (crédito)

- Elegir **`proto.h`** como base (length-prefixed + CRC16 + resync byte-a-byte +
  START≠END; tests reales y buenos). Correcto.
- **Cámara sin CRC = P0** y **fail-safe de borde se pierde en silencio = P0**:
  diagnóstico correcto y bien priorizado.
- No agregar ACK/retransmisión para stream a 100 Hz: correcto.
- Diagnóstico del heartbeat ("vivo == hubo datos" no distingue colgado de
  link-down) y del bug de boot (`last_ms>0`): correctos.
- CRC-16 false-accept para un partido de 10 min: despreciable (bien tolerado).

## 4. Orden de ejecución CORREGIDO (reemplaza §5 de la propuesta)

**P0 reales (antes que cualquier otra cosa):**
1. **Sacar `pulseIn` y I2C bloqueante del loop de TOP** (HC-SR04 no bloqueante /
   por interrupción; BNO055 con lectura acotada) y **medir el período real del
   loop con timestamps en hardware** (osciloscopio/pin toggle). Sin esto, nada de
   las ventanas de frescura tiene base. (V-C1)
2. **Eliminar `Serial.clear()` de la propuesta.** (V-C2)
3. **CRC + fin de trama en el enlace de cámara** (P0 original 4.1, sigue válido)
   — pero con CRC por **tabla** en MicroPython y tests del parser escritos primero. (V-A4)
4. **Fail-safe de borde**: tratar `imminent_exit` como **latch OR** sobre el
   drenado y definir precedencia explícita cuando mundo y línea caen juntos
   (motors_stop vs modo conservador). (V-C4, V-A1)

**P1 (después de medir el loop real):**
5. Heartbeat explícito **con histéresis** (N frames para OK, STALE definido en
   strategy) y SEQ contado **solo** sin resync de por medio. (V-C3, V-C5)
6. Cotar `comm_*_tick` con `MAX_BYTES_PER_TICK` como ya hace cámaras. (V-A2)
7. Guard `last_ms>0` en los **3** sitios de TOP. (V-A5)
8. Gate del debug `Serial.print` por flag de competición. 

**P2 (capitalizable 2027):**
9. Refactor a módulo `Link` único **con `static_assert(sizeof)`** y respetando
   los 3 patrones de receptor (stream/evento/comando) — no es mecánico. (V-C6, V-A6)
10. Corregir config/docs obsoletas (C1 motor-server, comentario 23≠24 B).

## 5. Plan de prueba — corregido

El plan original (desconectar cables) **no detecta V-C1/V-C3/V-C4**: esos fallan
con el cable conectado, por stall de loop y ruido EMI de motores. Los tests en
hardware real **deben** incluir: (a) inyección de stall de loop (forzar
`pulseIn` sin eco), (b) ruido EMI con motores acelerando/frenando, (c) ráfaga de
`LINE_URGENT` con `imminent_exit` intermitente tras stall, (d) medición del
período real del loop de cada placa con pin-toggle + osciloscopio.

## 6. Decisión de la verificación

La propuesta queda como **base conceptual válida pero NO ejecutable como está**.
Antes de abrir TASKs: aplicar el orden corregido de §4. El cambio más importante
y contraintuitivo: **NO bajar el timeout de motores a 150 ms hasta que el loop de
TOP sea no-bloqueante y se haya medido su período real** — hacerlo antes
introduce paradas espurias en partido (peor que el problema original).

- **Verificación por:** 2 revisores independientes (Claude, Anthropic Opus 4.7 1M)
  a pedido de Gustavo Viollaz (@gviollaz), 2026-05-18.
- **Acción:** cada P0/P1 corregido se abre como TASK con el plan de prueba de §5.

## 7. Fuentes

- Propuesta auditada: `docs/decisions/2026-05-18-protocolo-comunicaciones-entre-placas.md`.
- Código: `sensors_tof.cpp:31`, `sensors_imu.cpp:53-57`, `main_*.cpp`,
  `proto.cpp`, `comm_*.cpp`, `world_model.cpp:39-45`, `types.h:90-122`,
  `line_ring.cpp:147-165`, `cameras_runtime.cpp`, `test/test_proto/`.
- Journal: `journal/2026-05-18-analisis-comunicaciones-entre-placas.md`.
