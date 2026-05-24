---
title: "Diseño definitivo de comunicaciones entre placas — robusto, confiable y a prueba de fallas"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [comunicacion, electronica, decision, protocolo, seguridad, ambos]
robot: ambos
area: comunicacion
tipo: decision
related-tasks: [TASK-014, TASK-015, TASK-016, TASK-017, TASK-018, TASK-019, TASK-020, TASK-021]
related: [docs/decisions/2026-05-18-protocolo-comunicaciones-entre-placas.md, docs/decisions/2026-05-18-verificacion-protocolo-comunicaciones.md]
---

# Diseño definitivo de comunicaciones — a prueba de fallas

> Este documento es la **fuente única de verdad** del diseño de comunicaciones
> entre placas. Integra el diagnóstico de la propuesta original y **todas las
> correcciones** de la verificación independiente. Reemplaza el plan de acción
> (§3/§5) de `2026-05-18-protocolo-comunicaciones-entre-placas.md` (que queda
> solo como base conceptual con su banner).

## 0. Principio rector — "fail-safe by default"

Toda falla de comunicación debe degradar el robot a un **estado SEGURO,
OBSERVABLE y ESTABLE**, nunca a uno peligroso ni oscilante.

- **Seguro:** ante duda, el robot **frena** o entra en modo conservador; nunca
  "sigue jugando ciego". El estado seguro es alcanzable desde cualquier fallo.
- **Observable:** toda degradación se cuenta y se señaliza (LED/telemetría
  gateada). Una falla silenciosa es una falla inaceptable.
- **Estable:** con histéresis. Nada de oscilar entre frenar y jugar.
- **Medido, no inventado:** ningún umbral de tiempo se fija sin haber **medido
  el período real del loop en hardware** (TASK-014). Esta es la base de todo.

## 1. Arquitectura en capas (defensa en profundidad)

| Capa | Responsabilidad | Mecanismo | TASK |
|------|-----------------|-----------|------|
| **0. Loop** | El loop nunca se bloquea más que el tiempo de overflow del buffer RX | Sin `pulseIn`/I2C bloqueante; período medido en HW | TASK-014 |
| **1. Física** | Cableado robusto, separación de EMI de motores | Trenzado/blindaje, GND común, ruteo lejos de PWM | (hardware) |
| **2. Framing** | Integridad de cada frame | `proto.h`: `0xAA\|LEN\|TYPE\|SEQ\|PAYLOAD\|CRC16\|0x55`, resync byte-a-byte. **En TODOS los enlaces, incl. cámara** | TASK-015 |
| **3. Enlace** | Saber si el peer está vivo y reaccionar estable | Heartbeat explícito + FSM OK/STALE/LOST **con histéresis** | TASK-017 |
| **4. Aplicación** | Tratar cada mensaje según su naturaleza | **Taxonomía STREAM/EVENTO/COMANDO** | TASK-016, TASK-018 |
| **5. Sistema** | Estado seguro global, arranque, observabilidad | Estado inicial LOST, guard `last_ms>0`, contadores | TASK-019 |
| **6. Recuperación activa** | Recuperar una placa caída/colgada | Escalera WDT → reset por comando → reset por HW | TASK-021 |

Cada capa asume que la de abajo puede fallar y degrada seguro. Ninguna capa
confía ciegamente en la anterior.

## 2. Capa 2 — Framing único `proto.h`

`proto.h` es sólido y testeado (`test/test_proto/`): length-prefixed (un dato
0xAA/0x55 en el payload NO confunde), CRC-16/CCITT sobre LEN+TYPE+SEQ+PAYLOAD,
resync automático. **Se estandariza en todos los enlaces, incluido el de
cámara** (hoy único sin CRC ni fin de trama — el más frágil). En la OpenMV el
CRC se implementa **por tabla**, no bit-a-bit (no matar fps). Agregar
`is_valid_msgtype()` (hoy el TYPE se castea ciego). **No** se usa ACK/
retransmisión: a 100 Hz la pérdida de un frame la cubre el siguiente — lo que
falta es *detectarla* (capa 3), no recuperarla.

## 3. Capa 4 — Taxonomía de mensajes (el corazón del diseño)

El error central de la propuesta original era homogeneizar TODO bajo "quedarse
con el último". Eso es correcto para datos de stream y **catastrófico** para
eventos y comandos. El diseño correcto clasifica **cada** `MsgType`:

| Clase | Política de recepción | Ejemplos | Regla |
|-------|----------------------|----------|-------|
| **STREAM** | Coalesce: drenar todo y quedarse con el **último** válido | `WORLD_SNAPSHOT`, `DOWN_OTOS_POSE/VEL` | Idempotente; el dato más nuevo invalida al viejo |
| **EVENTO** | **Latch OR** sobre todos los frames del drenado; se limpia al consumir la acción | `imminent_exit` (borde) | Un pulso NUNCA se pierde aunque venga seguido de frames "sin evento" |
| **COMANDO** | Procesar **todos**; idempotente o con confirmación | `CENTRAL_RESET_OTOS`, `CENTRAL_CALIB_LINE` | Un comando one-shot no se coalesce; handlers bloqueantes-largos se rechazan/difieren en match |

"Homogéneo" = **un solo módulo `Link`** que implementa estas 3 políticas de
forma uniforme y parametrizable (TASK-020), NO una sola política para todo.

## 4. Capa 3 — Enlace: heartbeat + FSM con histéresis

- **Heartbeat explícito** (`LINK_HEARTBEAT`, payload 0 + contador): cada emisor
  lo manda si en `HB_TX_MS` no tuvo dato real. Distingue "emisor vivo sin datos"
  de "enlace muerto".
- **FSM por enlace, con histéresis (anti-flapping):**

```
        ┌────────── N frames buenos consecutivos ──────────┐
        ▼                                                   │
   [ LOST ] ──primer frame──► [ STALE ] ──N consec.──► [ OK ]
        ▲                        │  ▲                     │
        └── now-last > T_LOST ───┘  └── now-last > T_OK ───┘
```

- `OK` requiere **N frames consecutivos** (no 1) → no se confía en un frame
  espurio de arranque o post-ruido.
- Umbrales de entrada y salida **separados** (histéresis) → no oscila.
- **`STALE` tiene comportamiento definido por consumidor** (no "usar el último"
  a secas): strategy degrada (baja velocidad / no compromete), fail-safe se
  prepara. `LOST` ejecuta la acción de seguridad.
- `T_OK`/`T_LOST` por enlace = **f(período de loop medido en TASK-014)**, y el
  techo duro es el **tiempo de overflow del buffer RX**, no un número inventado.
  La tabla de valores se llena **después** de TASK-014 (hoy sería ficción).
- **Recuperación sin `Serial.clear()`**: se apoya en el resync del
  `FrameDecoder` (ya existe y testeado). Drenado **acotado** por
  `MAX_BYTES_PER_TICK` (TASK-018).
- **SEQ = métrica de salud, no gatillo de seguridad.** Se cuenta gap solo si NO
  hubo `crc_error`/`resync` entre medio (si no, da falsos packet-loss). Nunca
  dispara LOST por sí solo.

## 5. Capa 5 — Estado seguro, arranque y precedencia de fail-safe

### 5.1 Retícula de seguridad (precedencia explícita)
Cuando varios enlaces caen a la vez, **gana la acción más conservadora** y el
modo conservador de borde es **siempre alcanzable**:

| Situación | Acción (la más conservadora aplicable) |
|-----------|----------------------------------------|
| Mundo (TOP→CENTRAL) LOST | `motors_stop()` + LED falla |
| Línea (DOWN→CENTRAL) LOST | Modo borde conservador: velocidad limitada + vector prohibido hacia afuera + señalización. **Nunca** "seguir jugando ciego" |
| **Ambos LOST** | `motors_stop()` (cubre "no salir"); al recuperar mundo, si línea sigue LOST → permanecer en modo conservador, NO volver a juego pleno |
| `imminent_exit` (EVENTO, latch) | `motors_brake()` inmediato — tiene prioridad sobre cualquier comando de strategy |

Clave: la protección de borde **no depende de que mundo esté fresco**. Se elimina
el "modo ciego de borde" actual.

### 5.2 Arranque seguro
- Todo enlace arranca en **LOST**; el robot no mueve motores hasta el primer
  `WORLD_SNAPSHOT` válido (patrón ya correcto en CENTRAL; replicar en TOP).
- Guard `last_ms > 0` en **todas** las `is_fresh()` (3 sitios faltantes en TOP).
- Calibraciones bloqueantes (~320 ms) **solo** en pre-partido, nunca con
  `match_running`.

### 5.3 Observabilidad
Contadores `crc_errors/resync/frames/seq_gap` por enlace, expuestos por
LED/telemetría **gateada por flag de competición** (sin stalls de `Serial.print`
en partido).

## 5.bis Capa 6 — Recuperación activa (escalera de reset)

> El heartbeat (capa 3) **detecta** que un emisor cayó; esta capa lo **recupera**.
> Principio: *el mecanismo que recupera una placa colgada NO puede depender de
> que esa placa esté sana.* Por eso es una **escalera** de 3 niveles, no un solo
> mecanismo. La idea del peer-reset por comando es la Capa 2 (no la primaria).

**Estado físico de los canales inversos (verificado en código):** TOP↔DOWN y
CENTRAL↔DOWN ya tienen canal inverso (comandos `RESET_OTOS`/`CALIB_LINE`).
TOP↔CENTRAL inverso está anticipado pero NO implementado (`CENTRAL_RESET_TOP=0x61`
en `proto.h:51`, sin uso). Cámara OpenMV→TOP es realmente unidireccional. **Hoy
no hay watchdog de hardware en ninguna placa** (solo timeout software de cámara).

### Nivel 1 — Autocuración: watchdog de hardware (WDT) por MCU — **PRIMARIO**
Cada MCU (TOP, CENTRAL, DOWN; y el WDT propio de la OpenMV) habilita su WDT y lo
"patea" en cada loop sano. Si el loop se cuelga > `WDT_MS`, la placa **se
auto-resetea sola**, sin depender de ningún peer. Cubre el **peor caso** (cuelgue
/ crash) y es **lo único que puede recuperar a CENTRAL** (nadie está por encima
del master). Es el faltante de mayor valor. `WDT_MS` > período de loop peor caso
medido en TASK-014.

### Nivel 2 — Reset por comando del peer — **SECUNDARIO, best-effort**
El receptor, tras `T_LOST` + ventana de gracia, envía un `*_RESET_*` (COMANDO,
ver taxonomía §3) por el canal inverso. Implementar `CENTRAL_RESET_TOP=0x61`.
**Solo recupera al emisor vivo-pero-trabado-lógicamente** (loop corre, dejó de
producir datos); NO recupera un cuelgue duro (ahí el emisor tampoco lee su RX).
Nunca puede ser el único mecanismo.

### Nivel 3 — Línea de reset por hardware — **el más fuerte peer-driven (opcional)**
GPIO del supervisor al pin RESET del emisor (open-drain, con debounce/latch).
Funciona aunque el MCU esté **totalmente muerto**. Recomendado para CENTRAL→TOP,
CENTRAL→DOWN y **cámaras** (TOP→RST de la OpenMV, único camino ya que no hay
canal de datos inverso). Cuesta 1 wire + GPIO; riesgo: reset espurio → exige
debounce/latch.

### Convergencia (anti-tormenta de resets) — obligatorio en todos los niveles
- Backoff + **máximo N intentos**.
- Tras pedir/ejecutar un reset, el receptor entra en estado `RESETTING` y
  **espera el tiempo de boot conocido del emisor** (ej. DOWN calibra ~320 ms +
  init OTOS) antes de reevaluar. No reintenta durante el boot legítimo.
- Si tras N resets no se recupera → **estado seguro global** (motores stop +
  señalización) y se deja de martillar. Nunca un loop de reset.
- Quién supervisa a quién: CENTRAL supervisa TOP y DOWN; TOP supervisa cámaras;
  **CENTRAL solo se recupera por su propio WDT** (Nivel 1, no negociable).
- Todo contado y observable (resets pedidos/ejecutados, gateado).

## 6. Especificación por enlace (valores tras TASK-014)

| Enlace | Clase de dato | Heartbeat | T_OK / T_LOST | Acción LOST |
|--------|---------------|-----------|---------------|-------------|
| TOP→CENTRAL | STREAM (snapshot) | sí | f(loop medido) | `motors_stop` |
| DOWN→CENTRAL | EVENTO (`imminent_exit`) + STREAM (línea) | sí | f(loop medido), techo = overflow buffer | modo borde conservador |
| DOWN→TOP | STREAM (odometría) | sí | f(loop medido) | fusión sin odometría, pose inválida |
| cam→TOP | STREAM (blobs) | (por ausencia) | f(fps medido) | `ball.visible=false` + flag cámara caída |
| CENTRAL↔DOWN cmds | COMANDO | n/a | n/a | reintento idempotente |

> Los números concretos de `T_OK`/`T_LOST` se completan en TASK-017 **con la
> medición de TASK-014**. Fijarlos antes sería repetir el error que la
> verificación detectó.

## 7. Mapa de ejecución (orden obligatorio)

```
TASK-014 (P0, fundacional: loop no-bloqueante + medición)
   │  └─ habilita ventanas reales
   ├─► TASK-015 (P0: CRC cámara)            [independiente]
   ├─► TASK-016 (P0: fail-safe borde)       [independiente]
   ├─► TASK-017 (P1: heartbeat+histéresis+SEQ)   ← depende de 014
   ├─► TASK-018 (P1: cota drain, sin Serial.clear)
   ├─► TASK-019 (P1: robustez arranque/debug/handlers)
   ├─► TASK-021 (P1: WDT por placa + reset por comando + reset HW)  ← WDT primero
   └─► TASK-020 (P2: refactor Link + static_assert + config)  ← después de P0/P1
```

> **TASK-021 — orden interno:** Nivel 1 (WDT por placa) es lo primero y de mayor
> valor (única forma de recuperar a CENTRAL). Nivel 2 (reset por comando) y
> Nivel 3 (reset por HW) son refuerzos posteriores. NO depende de TASK-014 para
> el WDT, pero `WDT_MS` se fija con el período medido en TASK-014.

**Regla dura:** NO bajar el timeout de motores ni fijar ventanas hasta cerrar
TASK-014. Hacerlo antes mete paradas espurias en partido (peor que el problema).

## 8. Decisión

Se adopta este diseño en capas, fail-safe-by-default, con taxonomía de mensajes
y FSM con histéresis, como **arquitectura objetivo de comunicaciones del robot**.
Se ejecuta vía TASK-014..021 en el orden de §7, cada una con su plan de prueba
en hardware real (inyección de stall + ruido EMI de motores + medición de loop,
NO solo desconexión de cables).

## 9. Consecuencias

- **Ganamos:** un sistema donde toda falla degrada a seguro, observable y
  estable; un solo protocolo con 3 políticas claras; base empírica para los
  umbrales; herencia limpia para 2027.
- **Sacrificamos:** ~4–5 días de firmware (P0/P1) antes de tener el sistema
  completo; tocar el firmware OpenMV (estaba fuera de scope de Hito 1).
- **No se rompe lo que anda:** `proto.h` + `test_proto` son la red; el fail-safe
  de motores existente se conserva (solo se afina tras medir); los cambios van
  de a una TASK con prueba en hardware.
- **Para Incheon:** con los 3 P0 cerrados (014/015/016) el robot es
  competitivamente seguro (no pelota fantasma, no sale de cancha, loop sano).
  Los P1 endurecen; el P2 es inversión 2027.

## 10. Quién decidió y cuándo

- **Análisis, verificación independiente y diseño:** Claude (Anthropic, Opus
  4.7 1M) a pedido de Gustavo Viollaz (@gviollaz), 2026-05-18.
- **Validación de priorización y ejecución:** el coach con el equipo; cada TASK
  se cierra solo con su prueba en hardware real (sin test, no se cierra).

## 11. Fuentes

- Propuesta (concepto): `docs/decisions/2026-05-18-protocolo-comunicaciones-entre-placas.md`
- Verificación (fundamento de las correcciones): `docs/decisions/2026-05-18-verificacion-protocolo-comunicaciones.md`
- Tareas: `team-tasks/2026-05-18-task-014..020-*.md`
- Código: `software/teensy/Soccer 2026/src/{shared,top,down,central}/*`, `software/vision/`
- Journal: `journal/2026-05-18-diseno-comunicaciones-definitivo.md`
