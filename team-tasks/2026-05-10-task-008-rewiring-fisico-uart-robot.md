---
id: TASK-008
title: "Rewiring físico: OpenMV ahora va al TOP, no al Zircon. Zircon recibe del TOP."
date_created: 2026-05-10
assigned: [enzzo195, elias]
priority: P1
status: pending
estimated_hours: 2
blocks: [primer test integrado del firmware nuevo]
tags: [hardware, rewiring, uart, integracion]
---

# TASK-008 — Rewiring UART del robot 2026

## Resumen

La arquitectura nueva (3 placas + Zircon como motor server) requiere recablear los UART del robot. En el robot del nacional 2025, las OpenMV iban directo al Zircon (Teensy 4.1). En el robot 2026, las OpenMV van al **TOP** y el Zircon recibe comandos solamente del TOP.

## Contexto

**Cableado del nacional 2025 (a desarmar):**

```
OpenMV → Serial1 del Zircon (Teensy 4.1, pines 0/1)
```

**Cableado nuevo del robot 2026:**

```
OpenMV cámara 1 → conector U8 del TOP    (UART_CAMERA1, Serial3)
OpenMV cámara 2 → conector U9 del TOP    (UART_CAMERA2, Serial5)
DOWN board      → conector U16 del TOP   (UART_COMM_IN, Serial1)
COMM board      → conector U15 del TOP   (UART_COMM_OUT, Serial4)
TOP conector U1 → Zircon Serial1 (pines 0/1 del Teensy 4.1)
```

El TOP es el hub central. El Zircon ya no habla con la cámara directo — solo recibe comandos pre-procesados del TOP por su Serial1.

## Pasos concretos

1. **Desarmar el cableado viejo** del robot del nacional 2025:
   - Desconectar el cable OpenMV ↔ Zircon (pines 0/1 del Teensy 4.1).
2. **Identificar los conectores del TOP**:
   - U8 "UART_CAMERA1" (pines 5V/GND/TX/RX hembra 4-pin).
   - U9 "UART_CAMERA2".
   - U15 "UART_COMM_OUT" (a COMM).
   - U16 "UART_COMM_IN" (desde DOWN).
   - U1 "PINES MODULO" (a Zircon — 6 pines incluyen OUT1/OUT2/RX_OUT/TX_OUT/USB_D±).
3. **Confirmar pinout de cada conector** comparando con el schematic TOP (`Schematic_Roboliga2026_TOP_2026-04-12.pdf`).
4. **Construir 5 cables** (o reusar los del 2025 con cambios):
   - Cable OpenMV1 → U8.
   - Cable OpenMV2 → U9.
   - Cable DOWN → U16.
   - Cable COMM → U15.
   - Cable TOP_U1 → Zircon pines 0/1 + alimentación.
5. **Verificar continuidad y polaridad** de cada cable con multímetro antes de enchufar (evitar swap 5V ↔ GND).
6. **Enchufar progresivamente y testear**:
   - Solo alimentación primero. Verificar tensiones 5V y 3.3V en TOP, DOWN, Zircon.
   - Después UART TOP↔Zircon: cargar firmware del Zircon, mandar bytes random desde TOP via serial debug, ver si Zircon resyncroniza.
   - Después agregar UART hacia DOWN, COMM, cámaras una por una.
7. **Documentar el cableado final** con foto en `hardware/electrical/photos/2026-05-10-cableado-uart/` y diagrama en `hardware/electrical/cableado-uart-robot-2026.md`.

## Criterio de cierre

- [ ] Cable OpenMV1 ↔ TOP U8 funcional (la cámara comunica al TOP).
- [ ] Cable OpenMV2 ↔ TOP U9 funcional (si hay 2da cámara).
- [ ] Cable DOWN ↔ TOP U16 funcional (TOP recibe frames del protocolo nuevo desde DOWN).
- [ ] Cable COMM ↔ TOP U15 funcional (TOP comunica con módulo árbitros).
- [ ] Cable TOP_U1 ↔ Zircon Serial1 funcional (Zircon recibe MotorCommand desde TOP).
- [ ] Foto del cableado final en `hardware/electrical/photos/`.
- [ ] Documento `hardware/electrical/cableado-uart-robot-2026.md` con diagrama del cableado.

## Notas / decisiones

_(actualizar cuando se ejecute)_

## Cambios de estado

- 2026-05-10: creado por Claude bajo requerimiento de Gustavo Viollaz.
