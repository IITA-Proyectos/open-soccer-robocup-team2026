---
title: "Mejoras al protocolo UART OpenMV-Teensy"
date: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
status: backlog
priority: alta
tags: [comunicacion, uart, vision, software]
---

# Mejoras al protocolo UART OpenMV-Teensy

## Problema actual

El protocolo 2025 usa 6 bytes fijos `[201][Xp][Yp][202][Xa][Ya]`. Si se pierde un byte, el parser se desincroniza y lee datos incorrectos hasta que casualmente se alinea de nuevo.

## Qué investigar

- Implementar checksum o CRC para validar paquetes
- Agregar byte de inicio de frame (SOF) y largo de paquete
- Evaluar si el baud rate 19200 es suficiente o conviene subir
- Considerar protocolo con ACK para confirmar recepción
- Evaluar agregar más datos: ángulo, distancia estimada, confianza de detección
- Medir latencia actual del pipeline visión → UART → decisión

## Referencia

Ver protocolo actual en `legacy/2025-season/code/delantero/perseguir-pelota.ino`
