---
id: TASK-014
title: "Loop de TOP no-bloqueante + medir período real del loop en hardware"
date_created: 2026-05-18
assigned: [mariaviollaz, elias]
priority: P0
status: pending
estimated_hours: 8
blocks: [TASK-017, rediseño de ventanas de frescura, fail-safe de motores]
tags: [firmware, comunicacion, top-board, sensores, timing]
depends_on: []
---

# TASK-014 — Loop de TOP no-bloqueante + medir período real

## Por qué importa (P0, fundacional)

La verificación independiente
(`docs/decisions/2026-05-18-verificacion-protocolo-comunicaciones.md`, V-C1)
encontró que el loop de TOP se bloquea hasta **25 ms** en
`pulseIn(PIN_HCSR04_ECHO, HIGH, 25000UL)` (`sensors_tof.cpp:31`, caso común: sin
eco) + I2C bloqueante del BNO055 (`sensors_imu.cpp:53-57`). Con el buffer RX por
defecto del Teensy (~64 B) el RX **desborda a ~23 ms** y corrompe odometría **en
silencio** (no dispara LOST: hay tráfico, solo corrupto).

**Consecuencia:** ninguna ventana de frescura ni timeout de motores puede
fijarse con base real hasta que esto se arregle. Bajar el fail-safe a 150 ms con
este loop **mete paradas de motor espurias en partido** (peor que el problema
original). Esta tarea **bloquea** todo el rediseño de comms.

## Pasos concretos

1. Reemplazar `pulseIn` bloqueante del HC-SR04 por lectura **no bloqueante**
   (interrupción en el pin ECHO + máquina de estados con timeout suave, o
   directamente eliminar el HC-SR04 del loop si no es crítico para Incheon).
2. Acotar el I2C del BNO055: lectura no bloqueante o con presupuesto de tiempo;
   medir cuánto tarda `bno.getEvent()` real.
3. Instrumentar el loop: toggle de un pin GPIO libre al inicio/fin del loop de
   cada placa (TOP, CENTRAL, DOWN) + contador de período min/avg/max impreso
   (gated por flag debug).
4. **Medir con osciloscopio / analizador lógico** el período real del loop de
   cada placa: caso nominal y peor caso (HC-SR04 sin eco, ráfaga de cámara,
   emergencia activa).
5. Documentar los números medidos en el journal — son la **base empírica** de
   las ventanas de frescura de TASK-017.

## Criterio de cierre

- [ ] Ningún `pulseIn`/I2C bloqueante en el loop de TOP (verificado por código).
- [ ] Período de loop de TOP medido en hardware: nominal y peor caso, < el
      tiempo de overflow del buffer RX (calcular: bytes/s del enlace más rápido
      que entra a TOP vs tamaño real del buffer del core Teensy instalado).
- [ ] Períodos de CENTRAL y DOWN también medidos y documentados.
- [ ] Números crudos en `journal/YYYY-MM-DD-medicion-periodo-loop.md`.

## Plan de prueba en hardware real

1. **Setup:** robot completo encendido, TOP con el firmware modificado, pin de
   instrumentación a osciloscopio.
2. **Inyección de stall:** apuntar el HC-SR04 al vacío (sin eco) → forzar el peor
   caso; verificar que el loop NO se cuelga 25 ms.
3. **Criterio medible:** período de loop de TOP < tiempo de overflow del buffer
   RX en TODOS los casos, incluido HC-SR04 sin eco + ráfaga de cámara simultánea.
4. **Regresión:** la odometría DOWN→TOP no presenta saltos/corrupción durante 5
   min con el HC-SR04 sin eco continuo (contador de CRC-errors del enlace = 0).

## Notas / decisiones

_(completar al ejecutar — registrar los números medidos acá y en el journal)_

## Cambios de estado

- 2026-05-18: creada por Claude tras la verificación independiente del protocolo
  de comms, a pedido de Gustavo Viollaz.
