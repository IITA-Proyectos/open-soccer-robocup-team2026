---
id: TASK-014
title: "Loop de TOP no-bloqueante + medir período real del loop en hardware"
date_created: 2026-05-18
assigned: [mariaviollaz, elias]
priority: P2
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

- **2026-05-29 — lado firmware del HC-SR04 RESUELTO** (no cierra la TASK).
  La auditoría del TOP gateó el HC-SR04 tras `#ifdef TOP_ENABLE_HCSR04`
  (OFF por default) en `sensors_tof.cpp`: sin el flag, no se llama a
  `pulseIn(PIN_HCSR04_ECHO=7, …, 25000UL)` ni se tocan los pines 6/7, así que
  el stall de 25 ms del paso 1 **ya no ocurre** en el build de competencia. El
  ToF frontal U2 cubre la distancia frontal de forma redundante.
  **Lo que SIGUE pendiente de esta TASK** (sigue siendo P0, sigue abierta):
  - El paso 2 (acotar el I²C bloqueante del BNO055) **no** se tocó.
  - Los pasos 3-5 (instrumentar + **medir el período real con osciloscopio**
    en TOP/CENTRAL/DOWN) **siguen siendo del equipo** — son la base empírica
    de las ventanas de frescura (TASK-017).
  - Decisión de hardware: si alguna vez se quiere reactivar el HC-SR04, primero
    hay que **mover el ECHO del pin 7** (Serial2 RX2) a un pin libre.
  Ver `journal/2026-05-29-auditoria-top-pre-incheon-top.md` (Tema B).

- **2026-06-10 — NÚCLEO DE LA TASK RESUELTO EN BANCO (Gustavo, robot2).** El período
  real del loop del TOP se midió SIN osciloscopio, con el contador `loop=` del panel
  (Δloop entre líneas de 500 ms — instrumento equivalente al paso 3):
  - **Antes: ~6 Hz** (Δloop=+3/línea) — ¡166 ms por vuelta! El bloqueante real NO era
    el HC-SR04 ni el BNO que predecía esta TASK: eran los **4 `getRangingData()` del
    VL53L7CX por pasada**, cada uno trayendo el bloque COMPLETO de resultados por
    `Wire` a 100 kHz (~60 ms por sensor).
  - **Fix 1 (`a6c0366`):** round-robin — UN ToF por tick → ~16 Hz.
  - **Fix 2 (payload):** `-DVL53L7CX_DISABLE_*` de los bloques no usados (ambient,
    señal/SPAD, sigma, reflectancia, motion) → transferencia ~5× más chica.
  - **Después: ~190.000 vueltas/s** (Δloop≈+95.000/línea) — ~5 µs nominal, peor caso
    ~10-15 ms (una lectura ToF recortada). El snapshot a CENTRAL vuelve a los 100 Hz
    de diseño. Validado en banco: hdg trackea giro a mano (0→18→−23→+15), ToF
    responden, `resync=0`, cámaras vivas.
  Queda de la TASK (por eso baja P0→P2): medir período de CENTRAL (el panel ya
  imprime `loop_us max/avg`) y DOWN, y acotar formalmente el I²C del BNO (hoy ya
  no es crítico: 20 Hz + deconflict en R1, bus propio Wire2 en R2).

## Cambios de estado

- 2026-05-18: creada por Claude tras la verificación independiente del protocolo
  de comms, a pedido de Gustavo Viollaz.
- 2026-05-29: lado firmware del stall HC-SR04 resuelto (gateado por flag); resta
  acotar I²C BNO055 + medir período de loop en hardware. TASK sigue abierta P0.
- 2026-06-10: período del TOP medido + causa raíz (ToF) corregida + re-medido en
  banco por Gustavo (ver nota arriba). Baja a P2 (resta CENTRAL/DOWN + formalizar
  presupuesto I²C del BNO).
