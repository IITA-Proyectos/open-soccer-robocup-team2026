---
id: TASK-021
title: "Recuperación activa: watchdog HW por placa + reset por comando del peer + línea de reset HW"
date_created: 2026-05-18
assigned: [mariaviollaz, elias]
priority: P1
status: pending
estimated_hours: 16
blocks: [recuperación ante placa colgada en partido]
tags: [firmware, comunicacion, seguridad, watchdog, recuperacion]
depends_on: [TASK-017]
---

# TASK-021 — Recuperación activa (escalera de reset)

## Por qué importa (P1)

El heartbeat **detecta** que un emisor cayó pero **no lo recupera**. Idea del
equipo: que el receptor mande un comando de reset en sentido inverso. Correcta
como refuerzo, **pero falla en el peor caso**: si el emisor está colgado
(stuck en `pulseIn`, crash, loop trabado) tampoco lee su RX → el comando nunca
se procesa. Además **nadie puede resetear a CENTRAL** por comando (es el master).
Hoy **no hay watchdog de hardware en ninguna placa**.

Principio: *el mecanismo que recupera una placa colgada NO puede depender de que
esa placa esté sana.* Por eso es una **escalera de 3 niveles**.
Diseño completo: `docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md` §5.bis.

## Pasos concretos

### Nivel 1 — Watchdog HW por MCU (PRIMARIO, hacer primero)
1. Habilitar el WDT del Teensy 4.x en TOP, CENTRAL y DOWN; "patearlo" solo en
   un loop sano. Si el loop se cuelga > `WDT_MS` → auto-reset.
2. `WDT_MS` > período de loop peor caso medido en TASK-014 (con margen).
3. Habilitar/verificar el watchdog propio de la OpenMV.

### Nivel 2 — Reset por comando del peer (best-effort)
4. Implementar `CENTRAL_RESET_TOP=0x61` (ya en `proto.h:51`, sin uso) y usar los
   canales inversos existentes TOP↔DOWN / CENTRAL↔DOWN. Clase **COMANDO**
   (procesar todos, idempotente).
5. El receptor lo envía tras `T_LOST` + ventana de gracia, NO antes.

### Nivel 3 — Línea de reset por HW (opcional, el más fuerte)
6. GPIO open-drain del supervisor al pin RESET del emisor, con debounce/latch:
   CENTRAL→TOP, CENTRAL→DOWN y TOP→RST de cada OpenMV (único camino: la cámara
   no tiene canal de datos inverso). Requiere 1 wire + GPIO → coordinar con Enzo.

### Convergencia (anti-tormenta, obligatorio)
7. Backoff + máximo N intentos. Tras un reset, el receptor entra en `RESETTING`
   y espera el **tiempo de boot conocido** del emisor (DOWN calibra ~320 ms +
   init OTOS) antes de reevaluar. Si tras N resets no recupera → **estado seguro
   global** (motores stop + señalización), dejar de martillar.
8. Contadores observables (resets pedidos/ejecutados), gateados.

## Criterio de cierre

- [ ] WDT habilitado y verificado en TOP, CENTRAL, DOWN (+ OpenMV).
- [ ] `CENTRAL_RESET_TOP` implementado; reset por comando en TOP↔DOWN/CENTRAL↔DOWN.
- [ ] (Si se hace Nivel 3) línea de reset HW con debounce, sin resets espurios.
- [ ] Anti-tormenta: estado `RESETTING` + backoff + N máx + escalada a seguro.

## Plan de prueba en hardware real

1. **Cuelgue duro:** forzar `while(1)` / `pulseIn` infinito en un emisor → el
   **WDT lo auto-resetea** y el enlace se recupera SIN intervención del peer.
2. **Trabado lógico:** emisor con loop vivo pero sin mandar datos → el peer
   manda reset y se recupera.
3. **CENTRAL colgado:** verificar que su propio WDT lo recupera (nadie lo
   resetea por comando).
4. **Anti-tormenta:** durante el boot del emisor el receptor NO dispara más
   resets (estado de gracia); N resets sin éxito → estado seguro, sin loop.
5. **Reset espurio:** ruido en la línea de reset HW NO reinicia la placa.

## Notas / decisiones

_(completar al ejecutar — registrar WDT_MS elegido y su base de medición)_

## Cambios de estado

- 2026-05-18: creada por Claude a partir de la propuesta de recuperación activa
  de Gustavo Viollaz (reset en sentido inverso), elevada a escalera de 3 niveles
  con WDT como primario.
