---
title: "2026-05-18 — Verificación independiente del protocolo de comunicaciones propuesto"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [comunicacion, electronica, verificacion, analisis, ambos]
robot: ambos
area: comunicacion
tipo: analisis
related: [docs/decisions/2026-05-18-verificacion-protocolo-comunicaciones.md, docs/decisions/2026-05-18-protocolo-comunicaciones-entre-placas.md]
---

# Verificación independiente del protocolo propuesto

## Contexto

Gustavo pidió validar de forma independiente la propuesta de comunicaciones
(`docs/decisions/2026-05-18-protocolo-comunicaciones-entre-placas.md`) buscando
deadlocks, saturación y puntos de falla.

## Qué se hizo

2 revisores adversariales independientes (sin sesgo con la propuesta) leyendo el
código real + la propuesta, con misiones distintas (timing/saturación/deadlock y
integridad/riesgo de cambio). Síntesis en
`docs/decisions/2026-05-18-verificacion-protocolo-comunicaciones.md`.

## Qué se observó (los 2 convergieron)

- **El plan de acción de la propuesta NO es ejecutable como está** (el
  diagnóstico sí es correcto).
- **Crítico V-C1:** bajar el fail-safe de motores a 150 ms es peligroso con el
  loop bloqueante actual (`pulseIn` 25 ms HC-SR04 + I2C BNO055) → RX desborda a
  ~23 ms, corrompe odometría en silencio, y mete paradas de motor espurias. El
  límite real es el overflow del buffer, no T_LOST.
- **V-C2:** `Serial.clear()` propuesto agrega un fallo que hoy no existe; quitarlo.
- **V-C3:** falta histéresis → flapping frenar/jugar (motores del Zircon =
  fuente de ruido en la misma placa que CENTRAL).
- **V-C4:** "quedarse con el último" pierde el flag de emergencia `imminent_exit`
  y comandos one-shot; hay que distinguir stream vs evento/comando.
- **V-C5:** verificar SEQ da falsos packet-loss tras resync legítimo.
- **V-C6:** sin `static_assert(sizeof)`; comentario de tamaño errado (23≠24 B).
- Altos: cascada LOST mundo+línea deja el P0 de borde inalcanzable; `CALIB_LINE`
  en runtime stallea DOWN 320 ms; migración de cámara subestimada (CRC en
  MicroPython, sin tests del parser); los 5 `comm_*.cpp` no son copia exacta.
- **Acertado:** elegir `proto.h`; P0 cámara-sin-CRC; P0 borde-en-silencio;
  diagnóstico de heartbeat y boot.

## Conclusión

Orden de ejecución corregido: PRIMERO hacer el loop de TOP no-bloqueante y medir
su período real en hardware; recién después tocar ventanas/timeout. Eliminar
`Serial.clear()`. Agregar histéresis, OR-latch de emergencia, distinguir
stream/comando, `static_assert`. La propuesta queda con banner de advertencia
apuntando a la verificación.

## Próximos pasos

- Abrir TASKs por cada P0/P1 del orden corregido, con el plan de prueba de §5
  de la verificación (inyección de stall + ruido EMI + medición de loop, no solo
  desconexión de cables).
- No ejecutar el §5 de la propuesta original.
