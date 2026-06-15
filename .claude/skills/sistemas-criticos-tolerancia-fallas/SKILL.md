---
name: sistemas-criticos-tolerancia-fallas
description: Use when the deadline IS safety — designing or reviewing fault-tolerant, fail-safe embedded control where a miss can hurt someone or destroy hardware. Covers fail-safe state design, watchdogs (windowed), redundancy & voting (TMR/dual-dual), FDIR, graceful degradation, defensive coding, and the safety standards/cultures (DO-178C aerospace, ISO 26262 automotive, MISRA C, IEC 61508). Includes deep worked case studies of the three domains the user asked about - electronic fuel injection (ECU), electronically-actuated gearbox (shift/clutch-by-wire), and aerospace flight control. Triggers - "fail-safe / estado seguro", "watchdog", "redundancia / votación / TMR", "FDIR / detección de fallas", "degradación con gracia / limp mode", "DO-178C / ISO 26262 / MISRA / ASIL / DAL / IEC 61508", "sistema crítico / safety-critical", "inyección electrónica / ECU", "caja de cambios electrónica / shift-by-wire", "control aeroespacial / fly-by-wire / FADEC". NOT for general timing (tiempo-real-determinismo), RTOS scheduling (rtos-scheduling-embebido), or control-loop math (control-embebido-tiempo-real) — this is the RELIABILITY layer on top of those.
---

# Sistemas críticos y tolerancia a fallas — cuando el deadline ES la seguridad

## Principio central

**En un sistema crítico, "anda casi siempre" es un fracaso.** Lo que se diseña no es
el camino feliz — es **qué pasa cuando algo falla**: un sensor miente, un bus se corta,
una tarea se cuelga, un bit se voltea. La pregunta rectora no es "¿funciona?" sino
**"¿cómo falla, y falla SEGURO?"**. Un controlador de inyección, una caja por cable o
un control de vuelo se juzgan por su peor minuto, no por su mejor hora.

> Esta es la **capa de confiabilidad** que va ARRIBA de las otras tres skills de
> tiempo real. Asume que ya sabés timing (`tiempo-real-determinismo`), scheduling
> (`rtos-scheduling-embebido`) y la realización del lazo (`control-embebido-tiempo-real`).

## La idea madre: el estado seguro (fail-safe state)

Todo sistema crítico tiene un **estado seguro** definido: el lugar donde caer cuando
no podés garantizar operación correcta. Diseñarlo es la primera decisión, no la última.

| Sistema | Estado seguro |
|---|---|
| Robot de fútbol | **motores parados** (el árbitro manda STOP; si se corta el COMM → STOP) |
| Inyección (ECU) | **limp mode** (potencia limitada, RPM capadas) o motor apagado |
| Caja por cable | **mantener marcha actual / ir a neutral**, nunca un cambio no comandado |
| Control de vuelo | **degradar de ley de control** (normal → directa) manteniendo control manual |

**Regla:** la pérdida de energía o de señal debe llevar al estado seguro por
*defecto físico*, no por software. En el robot: el árbitro entra como **nivel GPIO
con `INPUT_PULLDOWN`** → si se desconecta el cable, los pines leen 0 → STOP, sin
que ningún código tenga que "decidirlo". Eso es fail-safe de verdad: la falla
*por construcción* cae a seguro.

## Las herramientas de la tolerancia a fallas

### 1. Watchdog (el seguro de vida)

Un timer hardware que resetea el MCU si el software no lo "patea" a tiempo. El
**windowed watchdog** es mejor: hay que patearlo dentro de una ventana (ni muy
tarde NI muy temprano) → atrapa tanto el cuelgue como el lazo desbocado. Patrón:
una tarea supervisora patea el watchdog solo si TODAS las tareas críticas saludaron
(ver `rtos-scheduling-embebido` → watchdog task). El robot tiene el primo lógico: si
`heading_valid=0`, `central_gate_heading_omega` pone ω=0 (no actúa con dato falso).

### 2. Redundancia y votación

- **TMR (Triple Modular Redundancy):** 3 canales calculan lo mismo; un **votador** toma
  la mayoría (2 de 3). Tolera 1 falla. La base del control de vuelo.
- **Dual-dual / self-checking pairs:** pares que se auto-chequean; si el par discrepa,
  se aísla y otro toma el control.
- **Diversidad:** los canales redundantes con HW/SW DISTINTO (distinto compilador,
  distinto equipo) para no repetir el mismo bug en los 3 (un bug de software es idéntico
  en 3 copias idénticas → la redundancia no lo cubre).

### 3. FDIR — Fault Detection, Isolation and Recovery

El ciclo: **detectar** la falla (rango plausible, comparación entre redundantes,
watchdog, CRC), **aislar** la fuente (cuál sensor/canal miente), **recuperar**
(cambiar a redundante, degradar, reiniciar el módulo). El robot lo hace en chiquito:
`imu_freeze` detecta el BNO congelado, el outlier-rejection del ToF aísla el sensor
tapado por un rival, y el arquero "degrada con gracia" navegando sin BNO.

### 4. Degradación con gracia (graceful degradation)

Mejor un sistema que pierde funciones de a una que uno que cae entero. Niveles
explícitos de degradación: plena → reducida → mínima segura. En autos es el "limp
mode"; en aviones, las "leyes de control" (normal/alternate/direct law del A320). En
el robot: con heading válido juega posicional; sin heading, degrada a navegar por
línea + cámara + OTOS, sin orientar con un rumbo falso.

### 5. Codificación defensiva

- **Rango/plausibilidad** en cada entrada (un sensor fuera de rango físico se rechaza).
- **CRC/checksum** en todo mensaje que cruza un enlace (el robot ya lo hace: CRC16 en el
  protocolo entre placas, CRC8+END en el de cámaras).
- **Sin asignación dinámica** en runtime (MISRA, DO-178C): todo estático.
- **Máquinas de estado con default seguro** (un `switch` con `default:` que cae a seguro,
  no que ignora).
- **Inicialización conocida** y secuencias de arranque verificadas.

## Las culturas/normas (qué exige cada dominio)

| Norma | Dominio | Idea clave |
|---|---|---|
| **DO-178C** | aeroespacial (software) | niveles **DAL A–E** según consecuencia de falla; A (catastrófico) exige trazabilidad total requisito→código→test + MC/DC coverage |
| **ISO 26262** | automotriz | niveles **ASIL A–D**; análisis de peligros, metas de seguridad, redundancia proporcional al riesgo |
| **IEC 61508** | industrial (genérica) | **SIL 1–4**; la madre de las normas de seguridad funcional |
| **MISRA C** | auto/embebido | subconjunto seguro de C: sin `malloc`, sin recursión no acotada, tipos explícitos, sin comportamiento indefinido |

No necesitás certificar el robot — pero **la disciplina** (estado seguro, watchdog,
rango, CRC, sin dinámico, degradar con gracia) es exactamente lo que hace que un
robot juegue 8 minutos sin colgarse, y lo que un jurado de RoboCup reconoce como
ingeniería seria.

## Los tres dominios que pediste — casos de estudio profundos

Cada uno aplica TODO lo de arriba en un producto real. Referencia detallada (decodificación
de cigüeñal, scheduling motor-síncrono, votación de vuelo, particionado ARINC 653,
limp modes): **`references/casos-inyeccion-caja-aeroespacial.md`**.

- **Inyección electrónica (ECU):** control *motor-síncrono* (el tiempo lo marca el
  cigüeñal, no un reloj fijo) — el ejemplo más puro de "el instante ES la corrección".
- **Caja de cambios por cable (shift/clutch-by-wire):** control de actuadores
  electromecánicos en posición+fuerza con una FSM de cambio y fail-safe a marcha segura.
- **Control aeroespacial (fly-by-wire / FADEC):** redundancia, votación, FDIR y
  particionado como norma, no como extra.

## Errores comunes

- Diseñar el camino feliz y "agregar manejo de errores después" (en crítico, el manejo
  de fallas ES el diseño).
- Redundancia con copias IDÉNTICAS → un bug de software está en las 3 (hace falta diversidad).
- Watchdog que se patea desde un timer fijo aunque la lógica esté colgada (patealo solo
  si las tareas saludaron — si no, no protege nada).
- Estado seguro que depende de que el software "decida" en vez de caer ahí por física
  (pull-down, default-off, energía cortada → seguro).
- Confiar en un sensor sin chequeo de rango/plausibilidad → el control actúa sobre una
  mentira con total confianza.

## Skills relacionadas

- **Timing/WCET/jitter (la base):** `tiempo-real-determinismo`.
- **Watchdog task, particionado, prioridades:** `rtos-scheduling-embebido`.
- **El lazo de control que estos sistemas ejecutan:** `control-embebido-tiempo-real`.
- **Fail-safe del robot en hardware real (test):** `hardware-test-protocol`.
- **Visión top-down de cómo el fail-safe cruza dominios:** `arquitectura-robotica-topdown`.
