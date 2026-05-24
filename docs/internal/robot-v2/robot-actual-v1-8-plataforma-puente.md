---
title: "Robot actual V1.8 — Plataforma puente a V2"
date: 2026-03-29
author: "Gustavo Viollaz + ChatGPT (OpenAI)"
ai-assisted: true
ai-tool: "ChatGPT (OpenAI)"
status: review
tags: [robot-actual, v1-8, transicion, roadmap, prioridades, arquitectura]
---
> [!WARNING]
> **Doc historico** -- analisis previo a la arquitectura 3-placas, hecho con ChatGPT el 29-mar-2026.
> Conservado por valor de referencia del proceso de diseno. **Para temas vigentes ver:**
> - Arquitectura actual: `docs/ARQUITECTURA-3-PLACAS-2026.md` + `docs/firmware/*`
> - Para programar un subsistema: el pack correspondiente en `hardware/electronics/*-pack/` (ver `hardware/electronics/PACKS-INDEX.md`)
> - Fuentes canonicas: `docs/FUENTES-DE-VERDAD.md`
>
> El contenido de este doc puede tener piezas todavia validas (especialmente sensores de piso) pero NO es la fuente actual.

# Robot actual V1.8 — Plataforma puente a V2

## Propósito

Este documento define una evolución **confiable y sustancial** del robot actual para esta temporada.

La V1.8 tiene dos objetivos simultáneos:

1. mejorar el rendimiento competitivo real este año
2. generar experiencia, software y módulos reutilizables para la V2

No se trata de construir la V2 ya mismo.
Se trata de construir una:

> **plataforma puente ordenada, medible y útil**

---

## 1. Decisión central

Se mantiene la placa actual y se agregan **2 placas nuevas** con responsabilidades muy claras:

### Placa actual
- movimiento
- actuadores
- seguridad
- ejecución

### Placa de piso
- sensado del suelo
- línea
- odometría local
- slip

### Placa superior
- percepción del entorno
- IMU
- ToF
- cámaras
- comunicación
- fusión sensorial
- estado local del robot

---

## 2. Filosofía correcta

La V1.8 no debe intentar resolver todo.

Debe atacar lo que hoy más limita el robot actual:

- poca información de línea
- poca información real del movimiento sobre el piso
- poca percepción de entorno cercano
- dependencia excesiva de pocos sensores
- pobre separación entre percepción y locomoción

---

## 3. Qué mejora real busca

La V1.8 debería mejorar de forma visible:

- recuperación desde borde
- lectura del área por el arquero
- coherencia entre “mandé mover” y “realmente me moví”
- evasión de rivales
- heading más confiable
- mejor coordinación entre robots
- menor dependencia de una sola fuente sensorial

---

## 4. Arquitectura recomendada

## 4.1 Placa actual
Debe quedar como **controlador de movimiento**.

Responsable de:
- control de motores
- primitivas de movimiento
- seguridad
- dribbler / kicker si aplica

## 4.2 Placa de piso
Debe convertirse en el **sensor inteligente del contacto robot–cancha**.

Responsable de:
- anillo de línea
- optical flow
- vector de escape
- slip
- odometría local corta

## 4.3 Placa superior
Debe convertirse en la **capa de percepción y estimación de estado**.

Responsable de:
- IMU
- ToF
- cámaras (recibiendo detecciones)
- comunicación
- world model local
- comandos de alto nivel hacia la placa actual

---

## 5. Alternativas evaluadas

## Alternativa A — Conservadora
- placa de piso completa
- placa superior simple
- 1 IMU
- 4 ToF
- 2 cámaras
- sin ultrasónicos

### Ventajas
- alta confiabilidad
- integración más rápida

## Alternativa B — Balanceada (recomendada)
- placa de piso con línea + 2 optical flow + Teensy 4.0
- placa superior con 6 ToF
- conectores para 4 cámaras, usando 2
- 1 IMU principal + 1 secundaria opcional
- ESP32 + Teensy de fusión
- placa actual como motion controller

### Ventajas
- mucha mejora real
- modularidad fuerte
- muy buen puente a V2

## Alternativa C — Ambiciosa
- placa de piso muy densa
- placa superior con 8 ToF, 2 ultrasónicos, 4 cámaras, 2 IMUs
- mucha lógica de estimación

### Riesgo
- demasiadas novedades simultáneas
- integración más difícil
- mayor riesgo competitivo este año

---

## 6. Alternativa recomendada

Se adopta como base de trabajo la:
> **Alternativa B — Balanceada**

---

## 7. Prioridades de trabajo

## Prioridad 1 — Placa de piso
Es la mejora con mejor relación:
- impacto
- confiabilidad
- costo de integración
- valor de aprendizaje

## Prioridad 2 — Placa superior mínima útil
Debe arrancar con:
- 1 IMU principal
- 4 o 6 ToF
- ESP32
- 2 cámaras activas
- Teensy de fusión

## Prioridad 3 — Integración con la placa actual
La lógica debe quedar así:
- placa superior manda objetivos
- placa actual ejecuta movimiento
- placa de piso corrige con información del suelo

---

## 8. Plan de trabajo recomendado

## Fase 1 — Definición
- congelar arquitectura de 3 placas
- definir buses y mensajes
- cerrar lista mínima de sensores

## Fase 2 — Placa de piso
- esquemático
- PCB
- firmware
- calibración de línea
- validación de optical flow

## Fase 3 — Placa superior mínima útil
- IMU principal
- 4 o 6 ToF
- ESP32
- 2 cámaras
- Teensy de fusión

## Fase 4 — Integración
- protocolo entre placas
- control por objetivos
- fallbacks
- logging

## Fase 5 — Afinado competitivo
- tuning
- pruebas en cancha
- robustez
- tiempos de respuesta

---

## 9. Riesgos principales

- querer poner demasiados sensores desde el inicio
- hacer una placa superior “todopoderosa”
- no validar primero la placa de piso
- acoplar demasiado los tres niveles
- no medir latencias ni calidad de dato

---

## 10. Criterio de éxito

La V1.8 será exitosa si logra:

- mejora visible este año
- arquitectura más ordenada
- módulos reutilizables para la V2
- experiencia real de integración
- menor incertidumbre para el robot del próximo año

---

## 11. Definición final

> **La V1.8 del robot actual será una plataforma puente a V2 basada en mantener la placa actual como control de movimiento, agregar una placa de piso como sensor inteligente del suelo y una placa superior como capa de percepción, comunicaciones y estimación de estado.**
