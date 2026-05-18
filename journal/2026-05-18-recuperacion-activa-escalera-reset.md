---
title: "2026-05-18 — Recuperación activa: escalera de reset (WDT + peer-reset + reset HW)"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [comunicacion, seguridad, watchdog, recuperacion, ambos]
robot: ambos
area: comunicacion
tipo: decision
related: [docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md, team-tasks/2026-05-18-task-021-recuperacion-activa-wdt-reset.md]
---

# Recuperación activa — escalera de reset

## Contexto

Gustavo propuso: como los enlaces son unidireccionales con heartbeat, cuando el
receptor no recibe heartbeat por mucho tiempo, que mande un comando en sentido
inverso pidiendo al emisor que se RESETEE.

## Qué se observó (verificado en código)

- Canales inversos: TOP↔DOWN y CENTRAL↔DOWN **ya existen** (comandos
  RESET_OTOS/CALIB_LINE). TOP↔CENTRAL inverso anticipado pero NO implementado
  (`CENTRAL_RESET_TOP=0x61` sin uso). Cámara OpenMV→TOP es unidireccional real.
- **No hay watchdog de hardware en ninguna placa** (solo timeout SW de cámara).

## Conclusión

La idea es correcta como **refuerzo**, pero un reset por comando **falla en el
peor caso**: emisor colgado no lee su RX → el comando no se procesa. Y nadie
puede resetear a CENTRAL por comando (es el master). Se elevó a una **escalera
de recuperación de 3 niveles** (principio: el mecanismo de recuperación no puede
depender de que la placa caída esté sana):

1. **WDT de hardware por MCU (PRIMARIO)** — autocuración, cubre el cuelgue duro,
   única forma de recuperar a CENTRAL. Es el mayor faltante.
2. **Reset por comando del peer (la idea, SECUNDARIO)** — solo recupera al
   emisor vivo-pero-trabado-lógicamente.
3. **Línea de reset por hardware (opcional, el más fuerte)** — GPIO al pin RESET,
   funciona aunque el MCU esté muerto; para cámaras es el único camino.
4. **Convergencia anti-tormenta** — backoff + N máx + estado RESETTING que
   espera el boot del emisor + escalada a estado seguro.

## Qué se hizo

- Diseño definitivo: agregada **Capa 6 — Recuperación activa (§5.bis)** y fila
  en la tabla de capas; §7 y §8 actualizados.
- **TASK-021** (P1) creada con la escalera y plan de prueba en hardware
  (cuelgue duro, trabado lógico, CENTRAL, anti-tormenta, reset espurio).
- Índice `team-tasks/README.md` actualizado.

## Próximos pasos

- En TASK-021: hacer Nivel 1 (WDT) primero (mayor valor, protege a CENTRAL).
  `WDT_MS` se fija con el período medido en TASK-014.
- Nivel 3 (reset HW) requiere coordinar cableado/GPIO con Enzo.
