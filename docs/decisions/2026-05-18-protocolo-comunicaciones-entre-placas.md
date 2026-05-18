---
title: "Protocolo de comunicaciones entre placas — análisis profundo y diseño confiable, simple y homogéneo"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [comunicacion, electronica, decision, protocolo, ambos]
robot: ambos
area: comunicacion
tipo: decision
related-tasks: [TASK-003, TASK-006, TASK-008]
related: [docs/ARQUITECTURA-3-PLACAS-2026.md, software/teensy/Soccer 2026/src/shared/proto.h, journal/2026-05-18-analisis-comunicaciones-entre-placas.md]
---

# Comunicaciones entre placas — análisis profundo + diseño objetivo

> Análisis anclado en el **código real** (`software/teensy/Soccer 2026/src/`), no en
> los docs de spec. Donde el código y la spec se contradicen, manda el código y se
> marca la contradicción. Requisitos del coach: **100% confiable, heartbeat,
> recuperación ante fallas, limpieza de buffer (datos siempre frescos),
> SIMPLE y HOMOGÉNEO** en todos los enlaces.

## 1. Mapa real de enlaces (lo que el código hace hoy)

Roles: **TOP** = cerebro sensorial (Teensy 4.0). **CENTRAL** = master/decisor + motores
(Teensy 4.1 sobre Zircon; aplica PWM **localmente**). **DOWN** = sensor de piso +
odometría. Cámaras OpenMV ×2 → TOP. COMM (ESP32-C6) → TOP (árbitro, ya resuelto).

| Enlace | Dato | UART (origen→destino) | Baud | Framing | CRC | Heartbeat | Recuperación |
|---|---|---|---|---|---|---|---|
| DOWN→TOP | odometría OTOS (`DOWN_OTOS_POSE/VEL`) | DOWN Serial5 → TOP Serial1 | 230400 | `proto.h` | ✅ CRC16 | ❌ implícito | ⚠️ parcial |
| TOP→CENTRAL | mundo (`WORLD_SNAPSHOT`) | TOP Serial2 → CEN Serial1 | 230400 | `proto.h` | ✅ CRC16 | ❌ implícito | ✅ fail-safe motores |
| DOWN→CENTRAL | línea (`LINE_URGENT`, bus emergencia) | DOWN Serial1 → CEN Serial2 | 230400 | `proto.h` | ✅ CRC16 | ❌ implícito | ⚠️ se pierde silenciosa |
| cam1→TOP | blobs pelota/arcos | OpenMV → TOP Serial3 | **19200** | **legacy 9 B** | ❌ **sin CRC** | ❌ | ⚠️ solo timeout 1 s |
| cam2→TOP | blobs pelota/arcos | OpenMV → TOP Serial5 | **19200** | **legacy 9 B** | ❌ **sin CRC** | ❌ | ⚠️ solo timeout 1 s |

Tabla de enlaces física/pines: fuente única `docs/ARQUITECTURA-3-PLACAS-2026.md:401-409`
(no se reduplica acá). Pines TOP↔CENTRAL aún sin confirmar físicamente → **TASK-008**.

## 2. Lo que SÍ está bien (no tocar, es la base)

- **`proto.h` es sólido y testeado.** Frame `0xAA | LEN | TYPE | SEQ | PAYLOAD | CRC16-BE | 0x55`,
  CRC-16/CCITT-FALSE sobre LEN+TYPE+SEQ+PAYLOAD, decoder byte-a-byte que
  resincroniza solo. `test/test_proto/` cubre basura, CRC corrupto, END corrupto,
  back-to-back, oversized. **Este es el protocolo homogéneo objetivo: todo debe usarlo.**
- Baud **230400** consistente en los 3 enlaces inter-placa, con ~12 % de uso →
  sobra ancho de banda; la latencia dominante es el período de loop, no el UART.
- **Fail-safe de motores** ante caída de TOP→CENTRAL: `motors_stop()` a los 500 ms
  (`main_central.cpp:108-111`). Correcto en concepto.
- Bus de emergencia DOWN→CENTRAL chequeado **cada iteración del loop** (no por
  tick), latencia de freno ~2 ms. Bien resuelto.
- Parser de cámara con máquina de estados que descarta basura hasta header y
  valida 3 headers (corrige los bugs de 2025).

## 3. Diseño objetivo — confiable, simple, homogéneo

Principio rector: **un solo mecanismo para todos los enlaces.** Mismo framing
(`proto.h`), misma política de heartbeat, misma política de frescura, misma
máquina de estados de enlace, misma limpieza de buffer. Nada de 5 implementaciones
distintas.

### 3.1 Framing único
`proto.h` en **todos** los enlaces, **incluidas las cámaras** (hoy son el único
enlace sin CRC ni fin de trama → es el más frágil). El payload de cámara entra
holgado en `PROTO_MAX_PAYLOAD=32`.

### 3.2 Heartbeat explícito (homogéneo)
Hoy el "heartbeat" es implícito (si hay datos, está vivo). Funciona mientras haya
tráfico, pero **no distingue "enlace muerto" de "emisor colgado sin datos"**, y
es la queja directa del coach. Diseño objetivo:

- Cada emisor manda su frame de datos a su frecuencia normal. **Si en `HB_TX_MS`
  (p.ej. 50 ms) no tuvo nada que mandar, manda un frame `LINK_HEARTBEAT` vacío**
  (nuevo `MsgType`, payload 0). Costo: ~12 B cada 50 ms = nada.
- Cada receptor mantiene `last_rx_ms` por enlace. Estado del enlace:
  - **OK**: `now - last_rx_ms < T_OK`
  - **STALE**: `T_OK ≤ now - last_rx_ms < T_LOST` → usar último dato pero marcarlo viejo
  - **LOST**: `≥ T_LOST` → acción de seguridad (ver 3.4)
- El emisor incrementa un contador en cada heartbeat → el receptor detecta
  "emisor vivo pero sin datos nuevos" vs "enlace muerto".

### 3.3 Política de frescura (reemplaza el 500 ms único)
500 ms es 50× el período de emisión (10 ms). Para la pelota/obstáculos es ceguera
estratégica. Tabla objetivo por enlace (período de emisión → T_OK → T_LOST):

| Enlace | Período | T_OK | T_LOST | Acción en LOST |
|---|---|---|---|---|
| TOP→CENTRAL (mundo) | 10 ms | 50 ms | 150 ms | `motors_stop()` + LED falla |
| DOWN→CENTRAL (línea) | 5 ms | 30 ms | 100 ms | **modo borde conservador** (no "ciego") |
| DOWN→TOP (odom) | 10 ms | 50 ms | 200 ms | fusión sin odometría, marcar pose inválida |
| cam→TOP | ~33 ms | 100 ms | 300 ms | `ball.visible=false`, flag cámara caída |

### 3.4 Recuperación y fail-safe homogéneos
Máquina de 3 estados por enlace (OK/STALE/LOST), idéntica en todos. Reglas:
- **LOST de TOP→CENTRAL** → motores STOP (ya existe; bajar timeout a 150 ms).
- **LOST de DOWN→CENTRAL** → **NO seguir jugando ciego**: pasar a modo borde
  conservador (limitar velocidad / no avanzar hacia afuera) y señalizar. Hoy se
  pierde la protección de borde **en silencio** (`main_central.cpp:13,95`) → riesgo
  de salir de cancha (penalización RCJ).
- Reconexión: automática al volver frames (ya pasa); además **resetear el
  `FrameDecoder` y vaciar el RX al detectar LOST→OK** para no arrastrar basura.
- Sin ACK ni retransmisión (fire-and-forget está bien a 100 Hz): la pérdida de un
  frame se cubre con el siguiente; lo que falta es **detectar** la pérdida (SEQ).

### 3.5 Limpieza de buffer / datos siempre frescos
- Mantener el patrón "drenar todo el RX cada loop y quedarse con el último frame
  válido" (ya se hace en los 5 `comm_*_tick`), pero:
  - Añadir `Serial.clear()` en cada `*_init()` y al recuperar de LOST (las cámaras
    hoy no lo hacen → arrastran basura post-reset).
  - Garantizar que el loop nunca se bloquee más que `T_LOST` (auditar I2C
    bloqueante del BNO055/ToF en TOP) — si el loop se cuelga, el RX se acumula y
    se procesa con retraso = datos viejos.
- Verificar **SEQ** en el receptor: contar saltos → métrica de packet-loss real
  (hoy SEQ se transmite pero **nadie lo chequea**).

### 3.6 Arranque / reset
- Guard `last_ms > 0` en **todas** las funciones `is_fresh()` (CENTRAL lo tiene,
  TOP **no** → bug: reporta datos "frescos" sin haber recibido nada los primeros
  500 ms post-boot).
- Estado inicial seguro: cada enlace arranca en **LOST** hasta el primer frame
  válido (motores quietos hasta que el mundo llegue — ya pasa por el guard de
  CENTRAL; replicar el patrón).
- No hace falta handshake complejo; el resync del `FrameDecoder` alcanza si el
  estado inicial es seguro y hay heartbeat.

### 3.7 Homogeneización del código
Las 5 implementaciones `comm_*.cpp` son copy-paste casi idéntico (decoder +
`while(available)` + `g_send_seq` + stats), con el baud **hardcodeado 3 veces**.
Objetivo: **una sola clase/módulo `Link`** parametrizada (UART, baud desde
`config_*.h`, tipo). Corregir el protocolo debe ser tocar **un** archivo, no seis.

## 4. Temas a analizar (formato coach)

### 4.1 Cámara sin CRC ni fin de trama

**Categoría:** comunicación / visión · **Robot:** ambos · **Prioridad:** P0

**Qué observo.** El enlace OpenMV→TOP usa un framing legacy de 9 bytes sin
checksum ni byte de fin (`cameras.cpp`; emisor `software/vision/enviar coordenadas
2 arcos y pelota:149-155`). Un byte de ruido eléctrico produce coordenadas de
pelota falsas **indetectables**. Es el único enlace del robot sin CRC.

**Risk-no-fix.** El robot persigue una pelota fantasma en cancha; imposible de
diagnosticar. **Risk-fix.** Toca el firmware OpenMV (estaba fuera de scope de
Hito 1) + el parser; riesgo de regresión de sincronización si se hace sin tests.
**Tiempo estimado.** 1–2 días (migrar el paquete de cámara a `proto.h`, o mínimo
agregar END + CRC8, en ambos extremos + tests host-native).

**Plan de prueba en hardware real.**
1. Robot sobre cancha, cámara apuntando a pelota fija; inyectar ruido (cable
   suelto / fuente conmutada cerca).
2. Criterio: 0 coordenadas falsas aceptadas; los frames corruptos se cuentan y
   descartan; el robot no reacciona al ruido.
3. Regresión: verificar que la latencia pelota→reacción no empeora (>5 ms) y que
   la fusión 2 cámaras sigue OK.

### 4.2 Sin heartbeat explícito / "vivo" = "hubo datos"

**Categoría:** comunicación · **Robot:** ambos · **Prioridad:** P1

**Qué observo.** No existe frame de keepalive (`DEBUG_PING=0xFF` definido en
`proto.h:68` pero nunca se envía). Si un emisor se cuelga sin producir datos pero
el UART sigue eléctrico, el receptor no distingue causa y solo reacciona a los
500 ms. Decisión previa de los docs (`FIRMWARE-PLACA-ABAJO.md:451-459`) fue
"heartbeat implícito"; el coach pide explícito.

**Risk-no-fix.** CENTRAL no sabe con certeza si TOP/DOWN están vivos; diagnóstico
ciego en cancha. **Risk-fix.** Bajo: agregar 1 `MsgType` y un timer de TX; sin
romper nada. **Tiempo estimado.** 0.5 día + test.

**Plan de prueba.** 1. Desconectar a propósito el cable de cada enlace con el
robot encendido. 2. Criterio: el receptor pasa a STALE→LOST dentro de la ventana
de la tabla 3.3 y ejecuta la acción de seguridad; al reconectar, vuelve a OK
limpio (sin basura). 3. Regresión: juego normal sin falsos LOST en 5 min.

### 4.3 Fail-safe de borde se pierde en silencio

**Categoría:** comunicación / control · **Robot:** ambos · **Prioridad:** P0

**Qué observo.** `EMERGENCY_LINE` exige `world_model_line_is_fresh()`
(`main_central.cpp:95`); si DOWN→CENTRAL muere, el robot sigue jugando **sin
protección de borde** ("modo ciego de borde", `main_central.cpp:13`).

**Risk-no-fix.** El robot sale de cancha sin freno → penalización/expulsión del
juego en Incheon. **Risk-fix.** Bajo: cambiar la política a "si línea LOST →
modo conservador" en vez de "ignorar línea". **Tiempo estimado.** 0.5 día.

**Plan de prueba.** 1. En cancha, con el robot jugando, desconectar el bus
DOWN→CENTRAL. 2. Criterio: el robot **no** cruza la línea blanca; entra en modo
conservador y lo señaliza. 3. Regresión: con el bus sano, el comportamiento de
borde no cambia.

### 4.4 SEQ no verificado / sin observabilidad en cancha

**Categoría:** comunicación · **Robot:** ambos · **Prioridad:** P1

**Qué observo.** Los 5 emisores incrementan `SEQ` pero ningún receptor lo
verifica → no se detecta pérdida. `crc_errors/resync/frames` existen pero no se
imprimen para los enlaces inter-placa (sí para cámaras).

**Risk-no-fix.** Un enlace degradándose (cable flojo, ruido) es invisible hasta
que falla en partido. **Risk-fix.** Bajo. **Tiempo estimado.** 0.5 día.

**Plan de prueba.** 1. Inyectar pérdida (bajar calidad de cable). 2. Criterio: el
contador de SEQ-perdidos y CRC-errors sube y es visible (LED/serial/telemetría).
3. Regresión: enlace sano → contadores en 0.

### 4.5 Bug de frescura al arranque del TOP

**Categoría:** comunicación · **Robot:** ambos · **Prioridad:** P1

**Qué observo.** `top/comm_down.cpp:25-27` `fresh()` sin guard `last_ms>0`:
los primeros 500 ms post-boot reporta datos como frescos sin haber recibido
nada. CENTRAL sí tiene el guard (`world_model.cpp:40`).

**Risk-no-fix.** TOP actúa sobre odometría inexistente al arranque. **Risk-fix.**
Trivial (una condición). **Tiempo estimado.** 1 hora + test de arranque.

**Plan de prueba.** Bootear TOP sin DOWN conectado; criterio: `is_fresh()`
devuelve false hasta el primer frame real.

### 4.6 Diseño no homogéneo (5 copias) + config contradictoria

**Categoría:** comunicación / docs · **Robot:** ambos · **Prioridad:** P2

**Qué observo.** 5 `comm_*.cpp` casi idénticos; baud hardcodeado 3 veces fuera de
`config_*.h`; `config_central.h:1-15` aún describe el modelo viejo "motor server
TOP-master" que el código ya **no** usa (CENTRAL es master y maneja motores
local). Contradicciones C1–C6 listadas en el journal asociado.

**Risk-no-fix.** Cada corrección del protocolo = tocar 6 archivos; alguien
cablea/configura según docs equivocadas; deuda que el equipo 2027 hereda.
**Risk-fix.** Medio (refactor a una clase `Link`); hacer con tests del protocolo
ya existentes como red. **Tiempo estimado.** 2–3 días.

**Plan de prueba.** Refactor cubierto por `test_proto` + test de banco de los 3
enlaces a 230400 durante 10 min sin pérdida ni desincronización.

## 5. Decisión

1. **Estandarizar en `proto.h` TODOS los enlaces**, incluido cámaras (P0 cámara,
   P2 la homogeneización completa).
2. **Heartbeat explícito** (`LINK_HEARTBEAT`) homogéneo + máquina OK/STALE/LOST
   por enlace, con las ventanas de la tabla 3.3 (P1).
3. **Acciones de seguridad por LOST** explícitas y conservadoras; eliminar el
   "modo ciego de borde" (P0).
4. **Verificar SEQ + exponer contadores** de salud en cancha (P1).
5. **Guard `last_ms>0`** en todas las `is_fresh()` (P1).
6. **`Serial.clear()`** en init y en recuperación LOST→OK; mantener "drenar y
   quedarse con el último" (P1/P2).
7. **Refactor a un único módulo `Link`** parametrizado por `config_*.h`; corregir
   la config obsoleta de CENTRAL (P2, capitalizable 2027).

**Orden para Incheon (prioridad honesta):** P0 4.1 (CRC cámara) y 4.3 (borde) →
P1 4.2/4.4/4.5 → P2 4.6. Sin los P0, el robot puede perder partidos por pelota
fantasma o salir de cancha. Los P2 son inversión 2027 (robot honesto primero).

## 6. Consecuencias

- **Ganamos:** enlaces verificables, fallas detectables y seguras, datos siempre
  frescos con ventanas realistas, un solo protocolo que el equipo 2027 entiende.
- **Sacrificamos:** tiempo de firmware ahora (los P0/P1 son ~3–4 días sumados);
  tocar el firmware OpenMV (estaba fuera de scope de Hito 1).
- **No se rompe** lo que funciona: `proto.h` y sus tests son la red de seguridad
  del refactor; el fail-safe de motores ya existente se conserva (solo se afina
  el timeout).

## 7. Quién decidió y cuándo

- **Análisis y propuesta:** Claude (Anthropic, Opus 4.7 1M) a pedido de
  Gustavo Viollaz (@gviollaz), 2026-05-18.
- **Decisión de priorización (P0→P2) y ejecución:** pendiente de validación del
  coach con el equipo. Este documento es la base técnica; cada tema se ejecuta
  como TASK con su plan de prueba en hardware real (no se cierra sin test).

## 8. Fuentes

- Código: `software/teensy/Soccer 2026/src/{shared,top,down,central}/*` —
  `proto.h`, `crc16.*`, `comm_*.cpp/h`, `cameras*.cpp/h`, `config_*.h`,
  `main_*.cpp`, `test/test_proto/`.
- Emisor cámara: `software/vision/enviar coordenadas 2 arcos y pelota`.
- Arquitectura: `docs/ARQUITECTURA-3-PLACAS-2026.md`,
  `docs/firmware/FIRMWARE-PLACA-{ABAJO,ARRIBA,CENTRAL}.md`.
- Análisis previos: `research/in-progress/2026-05-10-diseno-firmware-3-placas.md`,
  `research/in-progress/2026-05-11-analisis-arquitectura-3-placas-distribuida.md`,
  `software/staging/shared/cambios-uart-sincronizacion.md`.
- Journal de esta sesión: `journal/2026-05-18-analisis-comunicaciones-entre-placas.md`.
