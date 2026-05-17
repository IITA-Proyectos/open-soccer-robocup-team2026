---
id: TASK-012
title: "Activar libs reales de OTOS (DOWN) y ToF (TOP) — salir de modo stub"
date_created: 2026-05-15
assigned: [enzzo195, elias]
priority: P0
status: pending
estimated_hours: 6
blocks: [pose absoluta / EKF Nivel 3, deteccion de obstaculos real]
blocked_by: [decision modelo ToF (Q4), red para bajar libs PlatformIO]
tags: [firmware, down, top, otos, tof, hardware, stub]
---

# TASK-012 — Activar libs OTOS + ToF (salir de stub)

## Resumen

`src/down/otos.cpp` y `src/top/sensors_tof.cpp` corren en **modo stub**: la
lógica de fusión ya está implementada y activa, pero las llamadas a las
librerías de hardware están comentadas entre marcas `TODO_OTOS_LIB` /
`TODO_TOF_LIB`. Sin esto: odometría = 0 (sin pose), y solo HC-SR04 da
proximidad (sin los 4 ToF). Es **P0** porque bloquea pose absoluta y la
detección de obstáculos real.

## Contexto

Esto NO es código a medias — los stubs están bien diseñados a propósito,
esperando 2 decisiones/recursos que no son de software:

1. **OTOS**: falta confirmar el nombre exacto del `lib_dep` en
   `platformio.ini` + tener red para bajarla + hardware para validar la API.
2. **ToF**: falta **decidir el modelo físico** (Q4 del coach): VL53L7CX
   (disponible) vs VL53L5CX (por llegar) vs VL53L1X (Pololu, más simple, sin
   array). Cada uno tiene una librería y API distintas — no se puede
   descomentar sin saber cuál es.

⚠️ Bug latente YA corregido (2026-05-15, commit de esta sesión): en
`otos.cpp`, las velocidades `g_vx/g_vy/g_omega` no se asignaban ni siquiera
con la lib activa. Se agregó el cálculo en la fusión + las asignaciones en el
bloque comentado. Quedó listo para cuando se descomente. No re-introducir el
bug al editar.

## Pasos concretos

### Parte A — OTOS (placa DOWN)

1. Confirmar el nombre exacto de la librería en
   [registry.platformio.org](https://registry.platformio.org). Candidato:
   `sparkfun/SparkFun Qwiic OTOS Arduino Library`. El header es
   `SparkFun_Qwiic_OTOS_Arduino_Library.h` (ya referenciado en el código).
2. En `platformio.ini`, env `[env:down]`, descomentar/agregar en `lib_deps`:
   ```
   lib_deps =
       sparkfun/SparkFun Qwiic OTOS Arduino Library
   ```
3. En `src/down/otos.cpp`, descomentar los 4 bloques entre
   `// TODO_OTOS_LIB_BEGIN` y `// TODO_OTOS_LIB_END` (init, tick incl.
   velocity, reset, y el `#include` + objetos globales).
4. Verificar la API real contra la doc de la lib: nombres
   `getPose`/`getVelocity`, tipos `sfe_otos_pose2d_t`/`sfe_otos_velocity2d_t`,
   método `calibrateImu`/`resetTracking`/`isConnected`. Ajustar si la lib usa
   otros nombres (el stub usó los esperados, confirmar).
5. Confirmar el factor de unidades: el stub asume pulgadas→mm (`* 25.4f`).
   Verificar contra la config de unidades de la lib (puede setearse a mm
   directamente con `setLinearUnit`).
6. Compilar: `pio run -e down`. Resolver errores de API.

### Parte B — ToF (placa TOP)

7. **DECISIÓN DE HARDWARE (coach + Enzo)**: ¿qué modelo de ToF se usa?
   Cerrar Q4. Sin esto, no se puede avanzar el código.
8. Según el modelo, agregar en `[env:top]` `lib_deps` la lib correspondiente:
   - VL53L7CX → `stm32duino/STM32duino VL53L7CX`
   - VL53L5CX → `stm32duino/STM32duino VL53L5CX`
   - VL53L1X → `pololu/VL53L1X`
9. En `src/top/sensors_tof.cpp`, elegir el `#include` correcto y descomentar
   los bloques `TODO_TOF_LIB` (enumeración I2C por XSHUT + lectura en tick).
10. Implementar la enumeración I2C: encender ToF uno por uno por XSHUT,
    asignar direcciones 0x52/0x54/0x56/0x58.
11. Compilar: `pio run -e top`.

## Criterio de cierre

- [ ] `pio run -e down` compila con la lib OTOS real.
- [ ] `pio run -e top` compila con la lib ToF real (modelo decidido).
- [ ] **Plan de prueba en hardware real (OTOS)**: robot sobre cancha, mover
      50 cm en X medido con regla → `otos_get_x_mm()` reporta 500 ±25 mm.
      Rotar 90° → `otos_get_heading_deg()` ±5°. Velocidad: empujar a velocidad
      conocida → `otos_get_vx_mm_s()` no-cero y con signo correcto.
- [ ] **Plan de prueba en hardware real (ToF)**: objeto a 10/30/50/100 cm
      frente a cada ToF → `sensors_tof_get_distance_mm(i)` ±10% del valor real.
- [ ] Test de regresión: el bus DOWN→CENTRAL (LINE_URGENT) sigue a 100 Hz sin
      degradar por la carga I2C nueva (medir loop time DOWN antes/después).
- [ ] Journal con resultados de las mediciones.

## Notas / decisiones

_(actualizar cuando se ejecute)_

## Cambios de estado

- 2026-05-15: creado por Claude tras análisis integral del firmware. El bug
  latente de velocity en otos.cpp se arregló en el mismo commit (código
  comentado, sin cambiar el binario actual).
