---
id: TASK-003
title: "Confirmar Wire1 remap a pines 24/25 en placa TOP"
date_created: 2026-05-10
assigned: [enzzo195]
priority: P0
status: pending
estimated_hours: 1
blocks: [sensors_imu.h firmware TOP]
tags: [hardware, pcb, top-board, validacion, i2c]
---

# TASK-003 — Confirmar Wire1 remap en TOP

## Resumen

Confirmar físicamente que en la placa TOP fabricada, los nets `SCL1` y `SDA1` están conectados a los **pines 24 y 25 lógicos** del Teensy 4.0 (no a los pines 16/17 default).

## Contexto

Análisis del PCB JSON (`research/in-progress/2026-05-10-diseno-firmware-3-placas.md` sección 5b) infirió que Wire1 está remapeado a pines 24/25 porque el PCB tiene tracks ruteados separados para los 4 nets `SCL1, SDA1, RX4, TX4` — solo posible si están en pines distintos.

Esto NO está confirmado físicamente. El firmware TOP necesita saber con certeza para llamar `Wire1.setSCL(24); Wire1.setSDA(25);` antes de `Wire1.begin()`.

## Pasos concretos

### Opción A — Verificar en EasyEDA (más rápido)

1. Abrir proyecto TOP en EasyEDA: `hardware/electronics/pcb_design/top_board/`.
2. En el PCB editor, hacer click sobre el track de `SCL1` (color celeste o el que tenga asignado).
3. Seguir el track hasta el pad del componente U14 (Teensy 4.0).
4. Identificar **qué número de pad** del footprint tiene el track conectado.
5. Mapear pad físico → pin lógico Teensy 4.0:
   - **Pad 18 = pin 16 lógico** (SCL1 default).
   - **Pad 26 = pin 24 lógico** (SCL1 remap).
6. Repetir para `SDA1`:
   - **Pad 19 = pin 17 lógico** (SDA1 default).
   - **Pad 27 = pin 25 lógico** (SDA1 remap).
7. Repetir para `RX4` y `TX4`:
   - Si SCL1/SDA1 están en pads 26/27 → RX4/TX4 deberían estar en pads 18/19 (pines 16/17 default).
   - Confirmar esto también.

### Opción B — Verificar con multímetro

1. Identificar los pines físicos 24 y 25 del módulo Teensy 4.0 (cara superior del módulo, pads etiquetados).
2. Medir continuidad entre pin 24 del Teensy y cualquier pad I2C del componente U10 (BNO055) — debería dar continuidad si SCL1 está en pin 24.
3. Repetir para pin 25 (SDA1) y BNO055 U10.
4. Repetir para pines 16 y 17 — deberían NO tener continuidad con BNO055 (porque ahí está RX4/TX4 hacia placa COMM).

## Criterio de cierre

- [ ] Confirmado a qué pad físico del Teensy van `SCL1` y `SDA1`.
- [ ] Confirmado a qué pad físico van `RX4` y `TX4`.
- [ ] Resultado documentado en `hardware/electronics/mapa-pines-placas-nuevas.md` sección Q3 (reemplazar "no 100% confirmado" por confirmación).
- [ ] Si la inferencia se confirma (Wire1 remap a 24/25): nada más que hacer, el firmware ya tiene la nota correcta en `src/shared/types.h`.
- [ ] Si la inferencia NO se confirma (Wire1 está en 16/17 default): notificar urgente al coach + Virginia. Significa que hay un bug PCB similar al de DOWN. Probablemente RX4/TX4 están en otros pines remapeados (improbable porque Serial4 no tiene remap en Teensy 4.0) o hay nets duplicados/conflictivos.

## Notas / decisiones

_(actualizar cuando se ejecute)_

## Cambios de estado

- 2026-05-10: creado por Claude bajo requerimiento de Gustavo Viollaz.
