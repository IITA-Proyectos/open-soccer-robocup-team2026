---
title: "Investigar: Motores brushless con encoders para Roboliga 2026"
date: 2026-03-20
author: "Gustavo Viollaz"
status: backlog
priority: media
tags: [motores, brushless, encoders, roboliga]
roadmap_id: HW-010
---

# Investigar: Motores brushless con encoders

> **⚡ ACTUALIZACIÓN 2026-07-25 — este research está EN EJECUCIÓN.** Post-Incheon, el equipo está
> definiendo la compra para **Roboliga 2026** (el robot que iría a RoboCup 2027 Alemania).
> Candidato concreto: **Nanotec DF45** (BLDC plano 24 V), el que usan equipos de RoboCup
> Small Size League. **Las decisiones abiertas y su estado viven ahora en
> [`team-tasks/2026-07-25-task-043-definir-motor-upgrade-drivetrain.md`](../../team-tasks/2026-07-25-task-043-definir-motor-upgrade-drivetrain.md)**
> (modelo · con o sin caja · acople de eje · con o sin encoder), y las mediciones que las bloquean
> en [`TASK-044`](../../team-tasks/2026-07-25-task-044-medir-robot-y-reglamento-roboliga.md).
> Respuestas parciales ya obtenidas a las preguntas de abajo:
> **(2)** DF45M = 24 V, rated 5.260 rpm, par rated 0,084 N·m / pico 0,25 (⚠️ los distribuidores
> publican el pico como si fuera el nominal); DF45L = 0,13 / 0,39 N·m, 65 W.
> **(2-bis) ¿Reductor? Recomendación: NO** — direct drive; con reducción la inercia reflejada crece
> con i² y el par supera lo que el piso puede transmitir. El planetario GP42 además no entra en 18 cm.
> **(6)** el motor es de 24 V: **no** va sobre la LiPo 2S actual, requiere bus nuevo.
> Cuando se cierren las 4 decisiones, mover este archivo a `research/completed/`.

## Pregunta

¿Qué motores brushless con encoders integrados son adecuados para un robot de RoboCup Junior Soccer Open? ¿Necesitan reductores externos?

## Qué investigar

1. Modelos disponibles (tamaño compatible con chasis actual)
2. Especificaciones: voltaje, RPM, torque, con/sin reductor
3. Drivers ESC compatibles con Teensy
4. Precio y disponibilidad en Argentina / importación
5. Cómo leen otros equipos de RCJ los encoders (interrupt, I2C, SPI)
6. Consumo vs batería actual (LiPo 2S 7.4V)

## Referencia

Ver TDPs de equipos top Soccer Open 2024-2025 en `research/backlog/investigar-equipos-top.md`
