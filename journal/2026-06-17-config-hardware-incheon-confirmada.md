---
title: "Configuración de hardware para Incheon 2026 confirmada — R1 con BNO reconectados (juega con gyro)"
date: 2026-06-17
author: "Claude (sesión coach — Opus 4.8 1M) + 3 subagentes de auditoría"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: docs actualizados; cambio de comportamiento de R1 (con gyro) pendiente de banco
tipo: journal
---

# Resumen

Gustavo confirmó la configuración FÍSICA final de los 2 robots que van a Incheon. El delta
principal contra lo que el repo documentaba: **R1 ahora tiene los 2 BNO conectados y
funcionando** (antes "desconectados / R1 sin gyro"). Se hizo una auditoría READ-ONLY de
todo el repo (3 subagentes paralelos: firmware / docs / tasks) y se actualizó la
documentación al estado real, SIN tocar binarios de comportamiento (eso lo valida el banco).

# Configuración confirmada (fuente de verdad)

**AMBOS robots (R1 y R2), todo funcionando:**
- 2× BNO055: principal en dirección/bus I²C propio a alta velocidad + centinela en el bus de
  los ToF a 1 Hz.
- 4× ToF (frente/atrás/izquierda/derecha → ejes X e Y).
- HC-SR04 ultrasonido, módulo de juez, 2× cámaras OpenMV, sensores de luz de la placa DOWN.

**Solo R1:** odometría OTOS. **R2 NO tiene OTOS.**

# Decisiones de Gustavo (2026-06-17)

1. **R1 juega CON gyro** (BNO), igual que R2. El flag `ATK_OTOS_NOGYRO` (env
   `central_robot1_delantero_practica`) queda como FALLBACK histórico, NO competencia.
   ⚠️ El cambio de comportamiento lo valida el equipo en banco — NO se reasignó `default_envs`
   ni se borró el flag (queda como red de seguridad por si el BNO falla).
2. **TASKs 207/216/217/042**: se ACTUALIZAN a "hardware reconectado 2026-06-17, falta
   confirmar juego con gyro en banco". NO se cierran (las cierra el banco).

# Hallazgos de la auditoría (3 agentes)

- **R2 ya estaba bien documentado**: el repo refleja correctamente 2 BNO + 4 ToF + US + juez
  + 2 cámaras + luz, sin OTOS. (Un agente reportó por error "R2 tiene OTOS" — FALSO,
  descartado: R2 no tiene OTOS y el repo coincide.)
- **R1 = el delta real**: docs canónicos (ESTADO-ACTUAL, ARQUITECTURA-3-PLACAS, BACKLOG) +
  código (env `central_robot1_delantero_practica` con `ATK_OTOS_NOGYRO`, `atk_nogyro.h`,
  comentarios de pinout) asumían "R1 sin BNO". Eso quedó superado.
- La **arquitectura del firmware YA soporta** la config target (2 BNO en buses separados
  unificados 2026-06-15; 4 ToF por bodge de Enzo; US/juez/cámaras/luz sin gate por robot).
  No falta código nuevo — el delta es de ESTADO (hardware reconectado) + qué env se flashea.

# Qué se cambió (DOCS, no binarios)

- `docs/ESTADO-ACTUAL.md`: banner maestro "CONFIGURACIÓN DE HARDWARE PARA INCHEON 2026"
  (este journal lo ancla). Es la fuente de verdad; supera los banners "R1 sin gyro".
- `docs/ARQUITECTURA-3-PLACAS-2026.md`: aclarado que la redundancia 2 BNO aplica a AMBOS
  robots, y que OTOS es solo R1.
- `docs/BACKLOG-INCHEON.md`: ítems que dependían de "R1 sin gyro" (F3/F8) anotados.
- `docs/pruebas-banco/QUE-FLASHEO-HOY.md`: env de R1 con gyro para competencia.
- Packs de hardware + comentarios de código (`atk_nogyro.h`, pinout, platformio): banners de
  "R1 ahora con BNO; nogyro = fallback".
- `team-tasks/` 207/216/217/042: actualizadas (no cerradas).

# Qué NO se cambió (requiere banco)

- El flag `ATK_OTOS_NOGYRO` y el env `central_robot1_delantero_practica`: se preservan como
  fallback. NO se borran.
- `default_envs` y los binarios de competencia: intactos. El equipo decide y valida en banco
  qué env de R1 se flashea para competir con gyro.
- R2 / OTOS: nada (ya estaba correcto).

# Pendiente equipo (banco)

- Confirmar en banco que R1 con gyro (BNO) ataca/arquea bien (TASK-216/217 lo cubren).
- Validar que los 4 ToF de R1 enumeran (había una nota vieja "ToF derecho de R1 no enumera"
  en TASK-042 — confirmar resuelto).
- Decidir el env de competencia de R1 (con gyro) y flashearlo.
