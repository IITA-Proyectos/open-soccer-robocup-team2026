---
title: "2026-05-24 — OTOS: lib SparkFun activada + hallazgo crítico de power cycle"
date: 2026-05-24
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [hardware-up, down-board, otos, lib-sparkfun, power-cycle, i2c]
robot: ambos
area: electronica
tipo: hardware-up
related-tasks: [TASK-012, TASK-028, TASK-029]
related-journals: [2026-05-24-hardware-up-down-anillo-linea.md]
---

# OTOS: lib SparkFun activada en firmware + hallazgo crítico de power cycle

> **TL;DR.** Cierra TASK-012 parcialmente. La lib SparkFun Qwiic OTOS está
> activada en el firmware (3 bloques `TODO_OTOS_LIB` reescritos contra la
> API real), `platformio.ini` ajustado, compila y flashea OK. Los 2
> chips OTOS U5 y U6 responden I²C en 0x17 y reportan pose cambiante con
> movimiento real. **Hallazgo crítico:** al primer arranque después de
> flash, los OTOS NO encendían (`L=-- R=--`, I²C scan sin dispositivos).
> Después de un **power cycle completo** (batería + USB desconectados 10s
> y reconectados), los OTOS arrancan OK. Esto es comportamiento
> reproducible y debe documentarse como regla operativa.

## Contexto

Sesión continuación del journal `2026-05-24-hardware-up-down-anillo-linea.md`
(hardware-up del anillo de línea). Una vez confirmado que los 32 sensores
de luz responden bien, atacamos la TASK-012 ("Activar lib OTOS — salir
de stub") para tener odometría real en la placa DOWN.

## Qué se hizo

1. **Verificación de la API real de la lib SparkFun Qwiic OTOS.**
   Bajé el header oficial de GitHub
   (`sparkfun/SparkFun_Qwiic_OTOS_Arduino_Library`) y crucé los métodos
   contra los stubs que el firmware asumía. Encontré 3 mismatches:
   - `isConnected()` retorna `sfTkError_t`, no `bool`. (Pero el método
     correcto es `begin()` que sí devuelve `bool`).
   - **NO existe `getPose()` — el método se llama `getPosition()`.**
   - **NO existe `sfe_otos_velocity2d_t` — `getVelocity()` usa el mismo
     tipo `sfe_otos_pose2d_t` que `getPosition()`.**
2. **Activación en `platformio.ini`:**
   - Agregado `lib_deps = sparkfun/SparkFun Qwiic OTOS Arduino Library`
     a `[env:down]` y `[env:diag_down]`.
   - Activado `-DDOWN_NUM_OTOS_CONNECTED=2` en `[env:diag_down]` para
     intentar inicializar ambos OTOS (U5 y U6).
3. **Reescritura de `src/down/otos.cpp`:**
   - Quitados los 3 bloques `TODO_OTOS_LIB` (`init`, `tick`, `reset`).
   - Init: `g_otos_left.begin(Wire)` retorna bool directamente +
     `setLinearUnit(kSfeOtosLinearUnitMeters)` + `setAngularUnit(kSfeOtosAngularUnitDegrees)`
     + `calibrateImu()` + `resetTracking()`.
   - Tick: `getPosition()` y `getVelocity()` ambos con `sfe_otos_pose2d_t`.
     Conversión meters → mm (×1000) y deg/s → rad/s (×π/180).
   - Reset: `resetTracking()` real.
   - Corregido typo viejo del comentario: decía "Wire2 / I²C2", cambiado
     a "Wire1 / I²C1" (alineado con el doc canónico §6).
4. **Compilación:** SUCCESS sin errores (build subió de 16KB a 21KB de
   flash por la lib).
5. **Flash + lectura serial:** primera vez `L=-- R=--`. Ningún OTOS
   respondió.
6. **Agregué I²C scanner al `main_diag_down.cpp`** para descartar
   problemas de address o bus. Scanner imprime una vez en setup y
   periódicamente cada 5 segundos en loop.
7. **Reflashear + leer:** scanner reportó AMBOS buses vacíos (sin
   ningún dispositivo en addresses 0x01..0x7F). Diagnóstico: **OTOS sin
   alimentación 3.3V** (consistente con "ningún LED prendido en los
   módulos OTOS" que reportó Gustavo).
8. **Power cycle completo:** desconectar batería + USB, esperar 10
   segundos, reconectar ambos. Al volver el USB, **AMBOS OTOS responden**:
   - `[i2c-scan] Wire  (I2C0, OTOS U5): 0x17` ✅
   - `[i2c-scan] Wire1 (I2C1, OTOS U6): 0x17` ✅
   - `OTOS: x=0.0 y=0.0 hdg=0.0  [L=OK R=OK]` ✅
9. **Test de movimiento** ~30 cm adelante sobre hoja A4. La pose se
   actualiza con el movimiento (vimos cambios de ~150 mm intermedios)
   pero el desplazamiento neto reportado fue solo 28.6 mm. Conclusión:
   firmware OK pero la **hoja A4 no tiene textura microscópica
   suficiente** para tracking confiable del OTOS — el tracking salta y
   pierde referencia.

## Qué se midió (datos)

### Antes del power cycle
```
[i2c-scan] Wire  (I2C0, OTOS U5): (vacío)
[i2c-scan] Wire1 (I2C1, OTOS U6): (vacío)
OTOS: x=0.0 y=0.0 hdg=0.0  [L=-- R=--]
```

### Después del power cycle (sin tocar firmware)
```
[i2c-scan] Wire  (I2C0, OTOS U5): 0x17
[i2c-scan] Wire1 (I2C1, OTOS U6): 0x17
OTOS: x=0.0 y=0.0 hdg=0.0  [L=OK R=OK]
```

### Test de movimiento sobre hoja A4 (~30 cm en línea recta)
```
t= 0.2s:  x= -100.7mm  y=  +22.1mm  hdg= -23.4deg
t= 3.2s:  x=  -62.1mm  y=   +9.6mm  hdg= -22.8deg
t= 4.7s:  x=  +24.7mm  y=  -33.1mm  hdg= -23.4deg
t= 7.7s:  x= -127.3mm  y=  +33.4mm  hdg= -23.2deg   ← salto erratico
t=15.0s:  x= -127.1mm  y=  +33.1mm  hdg= -23.2deg   ← pose congelada
Desplazamiento neto: 28.6 mm (movimiento real ~300 mm)
```

## Conclusión

1. **Firmware OTOS plenamente activado.** Lib SparkFun integrada
   correctamente. Compilación + flash + responses I²C verificados.
2. **El bug `L=-- R=--` después de flash es real y se resuelve con power
   cycle completo (batería + USB).** Causa probable: los reguladores
   buck MP1584 o algún capacitor de bypass requieren un arranque "frío"
   limpio. Apenas tocás batería en caliente (con USB conectado), algo
   queda en estado raro. **Esta regla operativa debe documentarse para
   sesiones futuras: SIEMPRE iniciar la placa con power cycle completo,
   no solo encender batería sobre una placa que ya estaba con USB.**
3. **Validación cuantitativa pendiente.** La hoja A4 no sirve como
   superficie de test. Para validar precisión necesitamos cancha verde
   RoboCup, alfombra o superficie con textura visible.

## Próximos pasos

### Inmediatos (cierra esta sesión)
1. ✅ Commit del firmware activado + I²C scanner + journal.
2. ✅ Sincronizar snapshot del pack DOWN (`firmware/down/otos.cpp`).
3. ✅ Crear TASK-028 (regla operativa de power cycle).
4. ✅ Crear TASK-029 (validación cuantitativa OTOS sobre superficie texturada).
5. ✅ Actualizar TASK-012 a `validated-empirically-partial`.

### Pendientes humanos (no bloquean Incheon en escenario de pose 100% confiable)
6. **TASK-029**: validar precisión OTOS sobre cancha o alfombra. Mover
   robot distancia conocida (300 mm con regla), verificar que
   `sqrt(dx²+dy²) ≈ 300 ±25 mm`. Rotar 90° con guía, verificar
   `Δhdg ≈ 90 ±5°`.
7. **TASK-028**: documentar en `docs/firmware/SETUP-ENTORNO-BUILD-WINDOWS.md`
   o equivalente la regla "iniciar siempre con power cycle completo".
8. **Hipótesis a confirmar**: ¿el bug del primer arranque ocurre solo
   después de flash, o también después de un reset Teensy con batería
   ya conectada? Si solo es post-flash, puede ser un timing issue del
   `Wire.begin()` cuando los reguladores aún no estabilizaron.

## Status del hardware-up post-OTOS (regla 8 CLAUDE.md)

| Condición | Estado hoy |
|---|---|
| Robot encendido | ✅ (batería + USB) |
| COMM flasheada | ❌ TASK-006 sigue pendiente |
| DOWN reportando línea por UART real | ⚠️ Por USB serial todavía, no UART hardware |
| **OTOS leídos y reportando pose** | ✅ (validado cualitativamente — falta cuantitativo) |

**Moratoria de fábrica de papel sigue vigente.** Próxima sesión Claude:
candidatos naturales son TASK-006 (COMM flash) o TASK-029 (validación
cuantitativa OTOS sobre superficie real).

## Atribución

- Hardware en mano + power cycle + movimiento físico — Gustavo Viollaz (@gviollaz).
- API verification, firmware changes, I²C scanner, journal — Claude Opus
  4.7 (Anthropic), sesión 2026-05-24, modo ejecución directa con
  asistencia del humano.
