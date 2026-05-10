---
id: TASK-001
title: "PCB DOWN — decidir y aplicar fix para 10 nets sin rutear"
date_created: 2026-05-10
assigned: [enzzo195]
priority: P0
status: pending
estimated_hours: 4
blocks: [hito-2-firmware-down, montaje-final-robot]
tags: [hardware, pcb, down-board, urgente]
---

# TASK-001 — PCB DOWN: 10 nets sin rutear

## Resumen

La placa DOWN fabricada tiene **10 conexiones del schematic que NO se rutearon en el PCB**. Sin fix, solo funciona el 25% de los sensores de línea (8 de 32) y solo 1 de los 2 OTOS.

Decidir entre opciones A (puentes manuales), B (re-fabricar) o C (aceptar parcial). Aplicar la decisión.

## Contexto

Análisis completo en `research/in-progress/2026-05-10-auditoria-pcb-down-unrouted-nets.md`.

**Nets faltantes en el PCB:**
- `O1`, `O2`, `O3` — salidas de muxes U1, U2, U3 hacia Teensy (24 sensores desconectados).
- `SDA1`, `SCL1` — bus I2C 1 del OTOS U5 (uno de los 2 OTOS desconectado).
- `E1`, `E6`, `E7`, `E11`, `LED32_2` — señales de control y un LED.

**Impacto si no se fixea:**
- Anillo de 32 sensores no funciona (solo 8).
- Análisis diferencial OTOS (Q5 del coach) imposible.
- Posicionamiento por OTOS degradado.

## Pasos concretos

### Antes de elegir opción

1. **Verificar físicamente la placa DOWN** que ya tenés en mano. ¿Hay cables/jumpers soldados a mano? ¿Algún componente extra que no esté en el schematic? Documentar lo que ves (foto en `hardware/electronics/down-board-fab-as-received.md`).

2. **Confirmar el bug con multímetro**: medir continuidad entre el pin 3 (COMOUT/IN) del mux U1 y cualquier entrada analógica del Teensy U7. Si NO hay continuidad → bug confirmado.

3. **Reportar resultado** al coach.

### Opción A — Puentes manuales (recomendada por velocidad)

Si se elige esta opción:

1. Identificar los 5 wires necesarios:
   - U1 pin 3 (COMOUT) → Teensy U7 pin analógico libre (A6 sugerido, verificar disponible).
   - U2 pin 3 (COMOUT) → Teensy U7 pin analógico libre (A7).
   - U3 pin 3 (COMOUT) → Teensy U7 pin analógico libre (A8).
   - OTOS U5 pin 3 (SDA) → Teensy U7 pin SDA1 (pin lógico 17, pad físico 19).
   - OTOS U5 pin 4 (SCL) → Teensy U7 pin SCL1 (pin lógico 16, pad físico 18).
2. Soldar con wire fino (AWG 30 wire-wrap o equivalente).
3. Asegurar con epoxi o silicona caliente para resistir choques.
4. Verificar cada conexión con multímetro (continuidad + sin shorts a tierra).
5. Documentar las modificaciones en `hardware/electronics/down-board-mods-fab1.md` con foto + diagrama.
6. Actualizar `hardware/electronics/mapa-pines-placas-nuevas.md` con los pines reales usados (pueden diferir del schematic si se eligieron pines disponibles distintos).
7. Coordinar con Virginia (firmware TOP) el handshake: cuando esta placa se conecta vía UART, qué pin del Teensy U7 corresponde a cada mux output. Esto entra al `config_down.h`.

### Opción B — Re-fabricar

Si se elige esta opción:

1. Abrir proyecto en EasyEDA.
2. Correr DRC: identificar exactamente los 10 unrouted (debería coincidir con la auditoría).
3. Rutear manualmente los 10 nets faltantes.
4. Re-correr DRC: 0 warnings.
5. Correr ERC: 0 errors.
6. Exportar Gerbers a `hardware/electronics/pcb_design/down_board/gerber-v1.1/`.
7. Pedir cotización JLCPCB con opción "fast" (3-5 días producción).
8. Mientras llega la placa nueva, **arrancar Opción A en paralelo** sobre la placa actual para no perder tiempo de testing.

### Opción C — Aceptar parcial

Si se elige esta opción:

1. Actualizar `2026-05-10-diseno-firmware-3-placas.md` para reflejar la realidad: 8 sensores en lugar de 32, 1 OTOS en lugar de 2.
2. Actualizar `hardware/electronics/mapa-pines-placas-nuevas.md` con los pines que SÍ funcionan.
3. Aceptar que análisis diferencial OTOS no es posible y comunicar al coach que ese decision Q5 no se puede ejecutar en Incheon.

## Criterio de cierre

- [ ] Estado de placa documentado en `hardware/electronics/down-board-fab-as-received.md` (con foto).
- [ ] Bug confirmado con multímetro (o invalidado si hay cables/jumpers ocultos).
- [ ] Decisión registrada en el archivo `2026-05-10-auditoria-pcb-down-unrouted-nets.md` (sección "Decisión final").
- [ ] Si Opción A: 5 wires soldados, verificados, documentados con foto.
- [ ] Si Opción B: Gerbers v1.1 exportados, JLCPCB pedido, fecha estimada de arribo registrada.
- [ ] Si Opción C: docs actualizados con la realidad parcial.
- [ ] `team-tasks/2026-05-10-task-002-drc-erc-pcb-completo.md` arrancada (TASK-002 confirma que no hay otros bugs).

## Notas / decisiones

_(actualizar cuando se ejecute)_

## Cambios de estado

- 2026-05-10: creado por Claude bajo requerimiento de Gustavo Viollaz.
