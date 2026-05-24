---
title: "Robot V2 — Documento Maestro de Arquitectura"
date: 2026-03-29
author: "Gustavo Viollaz + ChatGPT (OpenAI)"
ai-assisted: true
ai-tool: "ChatGPT (OpenAI)"
status: review
tags: [robot-v2, arquitectura, hardware, mecanica, control, sensores, roadmap]
---
> [!WARNING]
> **Doc historico** -- analisis previo a la arquitectura 3-placas, hecho con ChatGPT el 29-mar-2026.
> Conservado por valor de referencia del proceso de diseno. **Para temas vigentes ver:**
> - Arquitectura actual: `docs/ARQUITECTURA-3-PLACAS-2026.md` + `docs/firmware/*`
> - Para programar un subsistema: el pack correspondiente en `hardware/electronics/*-pack/` (ver `hardware/electronics/PACKS-INDEX.md`)
> - Fuentes canonicas: `docs/FUENTES-DE-VERDAD.md`
>
> El contenido de este doc puede tener piezas todavia validas (especialmente sensores de piso) pero NO es la fuente actual.

# Robot V2 — Documento Maestro de Arquitectura

## Propósito

Este documento consolida la visión técnica de la futura **V2 del robot** del equipo IITA para RoboCupJunior Soccer Open.

Integra y organiza en un solo lugar:
- visión general del robot futuro
- decisiones de arquitectura
- tren motriz
- sensores
- capas electrónicas
- prioridades de diseño
- riesgos y criterios de congelamiento

No busca ser solo una lista de ideas. Busca ser una **definición coherente de plataforma**.

---

## 1. Visión general

La V2 ya no debe pensarse como una suma de mejoras sueltas sobre el robot actual.

Debe pensarse como una **plataforma nueva**, diseñada desde el inicio para:

- mejor control de movimiento
- mejor percepción
- mejor packaging interno
- mejor frente de ataque
- mejor mantenibilidad
- mejor integración entre sensores y estrategia

La V2 debe resolver mejor que el robot actual:
- control por rueda
- recuperación desde borde
- heading confiable
- percepción del entorno cercano
- integración de visión
- comunicación con compañero
- dribbler + kicker con frente realmente utilizable

---

## 2. Filosofía correcta de diseño

La V2 no debe organizarse alrededor de “qué sensores le ponemos”.

Debe organizarse alrededor de estas preguntas:

1. ¿Cómo dejamos libre el centro del robot para dribbler y kicker?
2. ¿Cómo medimos mejor lo que el robot realmente hace sobre la cancha?
3. ¿Cómo separamos control rápido de percepción pesada?
4. ¿Cómo hacemos cada módulo reemplazable y testeable?
5. ¿Cómo logramos rendimiento sin volver el robot inmantenible?

---

## 3. Arquitectura mecánica recomendada

## 3.1 Configuración base

La dirección recomendada para la V2 es:

- **4 ruedas omnidireccionales**
- **geometría asimétrica**
- **ruedas delanteras más separadas**
- **ruedas traseras algo más cerradas**
- **frente ancho para dribbler + kicker**
- **motores compactos en esquinas**

## 3.2 Por qué 4 omni asimétrica

Esta arquitectura tiene ventajas importantes cuando la prioridad pasa a ser liberar el centro:

- mejor estabilidad de base
- mejor reparto de masa
- mejor canal central para kicker
- mejor frente útil para dribbler
- más libertad para ubicar electrónica y batería
- mejor compatibilidad con un robot más cargado de sensado

## 3.3 Condición importante

Una 4-omni asimétrica no debe modelarse como una 4-omni ideal simétrica.

La cinemática y el control deberán construirse sobre:
- geometría real
- radios efectivos de rueda
- ubicación real de contacto
- matrices ajustadas al robot de verdad

---

## 4. Tren motriz futuro

## 4.1 Dirección recomendada

La V2 debería tender a:

- **motores brushless compactos / flat**
- **transmisión indirecta**
- **eje de rueda soportado por rodamientos**
- **encoders lo más cerca posible de la rueda**

## 4.2 Motivo principal de usar brushless compactos

La motivación principal no es solo potencia.

La motivación principal es **packaging**:
- dejar el centro libre
- abrir el frente
- facilitar dribbler + kicker
- sacar volumen axial del interior del robot

## 4.3 Transmisión recomendada

La arquitectura más coherente es:

**motor en esquina  
→ reducción corta  
→ eje de rueda soportado  
→ rueda omni con hub serio**

Como primera línea:
- **reducción por correa dentada corta**

Como opciones posteriores:
- reducción por engranajes
- gearhead coaxial muy bien resuelto

## 4.4 Qué no conviene

No se recomienda:
- rueda montada directamente sobre el eje del motor
- módulos con mucho juego
- acoples por prisionero simple como solución final

---

## 5. Sensores objetivo de V2

La V2 debería integrar sensado complementario y redundante.

## 5.1 Sensado del suelo
- anillo de sensores de línea
- optical flow al piso
- detección de slip
- odometría local

## 5.2 Sensado de entorno cercano
- ToF distribuidos
- ultrasónicos solo si aportan valor real
- eventos de pared / rival / borde

## 5.3 Orientación
- IMU principal confiable
- IMU secundaria opcional para validación

## 5.4 Visión
- arquitectura preparada para 4 cámaras
- 2 cámaras como primer escalón realista
- percepción separada del control de locomoción

---

## 6. Arquitectura electrónica objetivo

La V2 debería ser modular.

## 6.1 Motion controller
Responsable de:
- control de ruedas
- primitivas de movimiento
- actuadores
- seguridad

## 6.2 Sensor / state estimation layer
Responsable de:
- fusión sensorial
- heading
- estimación de estado
- relación con entorno
- world model local

## 6.3 Comunicación
Responsable de:
- compañero
- módulo oficial
- intercambio de intención y estado

---

## 7. Prioridades de diseño de V2

## Prioridad 1
**Módulo de rueda**
- motor
- transmisión
- encoder
- rueda
- hub
- rodamientos
- soporte

## Prioridad 2
**Frente del robot**
- dribbler
- kicker
- canal de pelota
- ancho útil del frente

## Prioridad 3
**Arquitectura electrónica**
- reparto entre placas
- buses
- señales
- fallbacks

## Prioridad 4
**Sensado del suelo**
- línea
- optical flow
- slip

## Prioridad 5
**Percepción**
- ToF
- IMU
- cámaras
- comunicación

---

## 8. Qué congelar ya

Conviene congelar cuanto antes:

- futura plataforma **4 omni**
- frente ancho
- centro libre para kicker
- módulos de rueda independientes
- arquitectura modular por placas
- sensado del suelo como parte estructural del sistema

---

## 9. Qué dejar abierto

No conviene congelar todavía:

- marca definitiva de motor
- relación final de reducción
- gearbox definitivo
- número final de cámaras activas
- número final de ToF poblados
- segunda IMU como obligatoria o no

Eso debe salir de prototipos y mediciones.

---

## 10. Relación con el robot actual

La V2 debe construirse a partir de una plataforma intermedia que deje:
- software reutilizable
- módulos electrónicos reutilizables
- experiencia real de integración
- menos incertidumbre técnica

Por eso se define también la:
- **V1.8 del robot actual / plataforma puente a V2**

---

## 11. Riesgos principales

Los riesgos más fuertes no son falta de ideas.

Son:
- meter demasiadas novedades juntas
- no cerrar bien el drivetrain
- sobrecargar la percepción
- subestimar el frente de juego
- diseñar un robot difícil de reparar

---

## 12. Definición final

> **La V2 objetivo del equipo IITA será una plataforma 4-omni asimétrica, con centro libre para dribbler y kicker, drivetrain modular, sensado del suelo robusto, percepción distribuida y arquitectura electrónica preparada para crecer de forma ordenada.**

---

## 13. Documentos relacionados

Leer junto con:
- `robot-actual-v1-8-plataforma-puente.md`
- `placa-de-piso-v1-8-especificacion.md`
- `placa-superior-v1-8-especificacion.md`
