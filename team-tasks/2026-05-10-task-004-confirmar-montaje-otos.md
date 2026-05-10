---
id: TASK-004
title: "Confirmar montaje físico de los 2 SparkFun OTOS"
date_created: 2026-05-10
assigned: [elias, enzzo195]
priority: P1
status: pending
estimated_hours: 1
blocks: [otos.h firmware DOWN, análisis diferencial]
tags: [hardware, mecanica, otos, montaje]
---

# TASK-004 — Montaje físico de los 2 OTOS

## Resumen

Confirmar la posición y orientación física de los 2 SparkFun OTOS en la base del robot. El coach (Q5) los quiere uno a cada costado para análisis diferencial. Esta tarea documenta cómo quedan instalados.

## Contexto

Los SparkFun OTOS (Optical Tracking Odometry Sensor) miden movimiento óptico contra el piso. Para análisis diferencial — detectar si el robot rota inadvertidamente al patear — necesitan estar **separados lateralmente** (uno a cada costado del centro del robot).

El firmware DOWN (`otos.h`) necesita saber **exactamente la posición de cada OTOS** respecto al centro del chasis para fusionar correctamente las dos lecturas.

> **Nota:** la placa DOWN tiene un bug PCB (TASK-001) que desconecta uno de los 2 OTOS. Hasta que TASK-001 se resuelva, solo uno funciona. Esta tarea sigue siendo válida porque define el montaje físico para cuando TASK-001 se cierre.

## Pasos concretos

1. Decidir con Gustavo y Elías la posición exacta de cada OTOS:
   - **Distancia desde el centro del robot al centro de cada OTOS** (en mm).
   - **Orientación**: ¿la cara del OTOS apunta exactamente hacia abajo? ¿alguna rotación?
   - **Altura desde el piso**: el OTOS necesita ~5-10mm de "stand-off" del piso para enfocar.

2. Tomar 3 fotos de la base del robot mostrando el montaje:
   - Vista desde abajo (con los OTOS visibles).
   - Vista lateral (mostrando el stand-off del piso).
   - Vista detalle de cada OTOS con cable de conexión.

3. Guardar fotos en `hardware/mechanical/photos/2026-05-10-montaje-otos/`.

4. Crear documento `hardware/mechanical/montaje-otos.md` con:
   - Diagrama (puede ser sketch a mano + foto) mostrando posición.
   - Tabla con coordenadas:
     | OTOS | X (mm desde centro) | Y (mm desde centro) | Orientación | Altura piso (mm) |
     | U5 (izq) | ? | ? | ? | ? |
     | U6 (der) | ? | ? | ? | ? |
   - Bus I2C de cada uno (U5 = Wire1 SCL1/SDA1, U6 = Wire2 SCL2/SDA2).

5. Comunicar a Virginia las coordenadas exactas — entran al `config_down.h` como constantes:
   ```cpp
   constexpr float OTOS_LEFT_X_MM = ...;
   constexpr float OTOS_LEFT_Y_MM = ...;
   constexpr float OTOS_RIGHT_X_MM = ...;
   constexpr float OTOS_RIGHT_Y_MM = ...;
   ```

## Criterio de cierre

- [ ] Coordenadas (X, Y, orientación, altura) de cada OTOS medidas y registradas.
- [ ] Fotos archivadas en `hardware/mechanical/photos/2026-05-10-montaje-otos/`.
- [ ] Documento `hardware/mechanical/montaje-otos.md` creado.
- [ ] Coordenadas comunicadas al equipo de firmware (entran a `config_down.h`).

## Notas / decisiones

_(actualizar cuando se ejecute)_

## Cambios de estado

- 2026-05-10: creado por Claude bajo requerimiento de Gustavo Viollaz.
