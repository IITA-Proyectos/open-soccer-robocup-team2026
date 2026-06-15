---
title: "Documentar strategy.cpp en español: guía de alto nivel + diccionario en el código"
date: 2026-06-14
author: "Claude (Anthropic - Claude Opus 4.8 1M)"
requested-by: "Virginia Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M)"
status: final
tags: [control, analisis, ambos, media]
robot: ambos
area: control
tipo: analisis
---

# Documentar `strategy.cpp` en español — guía + comentarios en el código

## Contexto

Virginia está tratando de entender `strategy.cpp` (el "cerebro" de la CENTRAL):
1759 líneas, el archivo más largo y enredado del robot. Pidió un documento **muy
de alto nivel** que explique la lógica del programa y cómo funciona su máquina de
estados. Después pidió que estuviera lo más posible en **español simple y
directo**, y por último que esa traducción quedara también como **comentarios
dentro de `strategy.cpp`**.

## Qué se hizo

1. **Guía de lectura nueva**: [`docs/firmware/STRATEGY-CPP-COMO-FUNCIONA.md`](../docs/firmware/STRATEGY-CPP-COMO-FUNCIONA.md).
   Mapa mental para entender el archivo antes de abrirlo:
   - La idea en una frase (mirar → elegir UNA conducta → dar una orden de
     movimiento, 100 veces por segundo).
   - El modelo entra → decide → sale.
   - Los dos cerebros en un archivo (delantero/arquero), elegidos al compilar.
   - Qué es una máquina de estados, con la comparación "conductas de un jugador".
   - Tabla de estados de cada FSM (nombre en código → palabra en español → qué hace).
   - Las reglas con prioridad (árbitro, línea, saque) y el freno de borde que
     vive en `main_central.cpp`, no acá.
   - Por qué el archivo es largo (historia de banco + interruptores `#ifdef`) y
     un orden de lectura recomendado.
   - Verbosidad pensada para voz de estudiante (jerga explicada en su 1er uso).
2. **Diagrama** de las dos máquinas de estados (entregado en la sesión, en
   español: ESPERAR/SAQUE/BUSCAR/RODEAR/ACERCARSE/EMPUJAR/RETROCEDER y
   ESPERAR/IR A LA LÍNEA/PATRULLAR/INTERCEPTAR/DESPEJAR, con el nombre del código
   en chico debajo).
3. **Comentarios en español dentro de `src/central/strategy.cpp`**:
   - Un **diccionario** arriba del `enum` de estados que mapea los 12 estados de
     las dos FSM a su palabra en castellano + una línea de qué hace, y apunta a
     la guía.
   - Un **comentario al lado de cada `case`** del `switch` (14 en total) con la
     traducción (ej. `case AtkState::SEARCH: {   // BUSCAR — gira y avanza ...`).
4. **Índice canónico**: fila nueva en [`FUENTES-DE-VERDAD.md`](../docs/FUENTES-DE-VERDAD.md)
   ("FSM táctica CENTRAL (guía de lectura de alto nivel)") apuntando a la guía,
   aclarando que NO reemplaza al §8 (spec detallado) ni al código vivo.

## Qué se midió/observó

- **No se midió nada en hardware.** No corresponde: el cambio en `strategy.cpp`
  es **solo comentarios** (comentarios de línea `//` después de código completo)
  → no cambia el comportamiento del robot ni el binario.
- **No se compiló el firmware** de la CENTRAL en esta sesión: la cadena de
  Teensy/PlatformIO no está disponible en esta máquina (la `host-build-toolchain`
  solo cubre los módulos puros de `src/shared`, no `strategy.cpp` con includes de
  Arduino). Riesgo de compilación: nulo para un cambio solo-comentarios bien
  formado; el equipo lo verá compilar limpio en el próximo build/flasheo.
- Lectura verificada del archivo completo (1759 líneas), `strategy.h`, y el
  punto donde `main_central.cpp` llama a `strategy_tick()` y aplica el freno de
  borde `EMERGENCY_LINE` con prioridad sobre la FSM.

## Conclusión

`strategy.cpp` quedó **autodocumentado en español** (diccionario + comentario por
estado) y con una guía de alto nivel que sirve de rampa de entrada antes de leer
el código. Esto baja el riesgo de la deuda conocida #2 de `FUENTES-DE-VERDAD.md`
("conectar `strategy.cpp` → `strategy_transitions`, riesgo: tocar el cerebro"):
quien tenga que tocarlo ahora arranca con un mapa.

## Próximos pasos

- **Equipo**: confirmar que `central_robot1`/`central_robot2` compilan limpio en
  el próximo build (formalidad — es solo-comentarios).
- **Opcional**: si la guía resulta útil, replicar el mismo patrón
  (diccionario + comentarios) en otros archivos largos del cerebro
  (`world_model`, `motors_zircon`).
- **Sin tarea de hardware nueva.** Claude no cierra TASKs de hardware (regla 1
  de CLAUDE.md); acá no hay ninguna que abrir.
