---
title: "Investigar: Módulo de comunicación con árbitros RCJ 2026"
date: 2026-03-20
author: "Gustavo Viollaz"
status: backlog
priority: BLOQUEANTE
tags: [arbitros, esp32, mundial, homologacion]
roadmap_id: HW-001
---

# Investigar: Módulo de comunicación con árbitros RCJ 2026

## Pregunta de investigación

¿Cuál es la especificación oficial del módulo de comunicación con árbitros para RoboCup Junior 2026? ¿Qué hardware se necesita y cómo se integra?

## Por qué es crítico

Sin este módulo, el robot NO puede participar en el mundial de Incheon 2026. Es requisito de homologación.

## Qué investigar

1. Especificación oficial del protocolo (buscar en reglas 2026 y foro RCJ)
2. Hardware requerido (ESP32, tipo de pantalla, conector)
3. Firmware disponible (si hay implementaciones de referencia open source)
4. Cómo se integra con el controlador principal (serial, GPIO)
5. Requisitos de QR (qué info codifica, formato)
6. Si el ESP32 puede servir también para comunicación entre robots (ESP-NOW)

## Fuentes a consultar

- https://robocup-junior.github.io/soccer-rules/2026-soccer-draft-rules/rules.html
- https://junior.forum.robocup.org/
- Repos de equipos que ya lo implementaron
- Repositorios oficiales de RCJ en GitHub

## Entregable

Documento en `research/completed/` con: especificación, BOM, esquema de conexión, plan de implementación.
