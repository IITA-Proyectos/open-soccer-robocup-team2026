---
title: "Análisis de selección de cámaras para RoboCupJunior Soccer Open 2026"
date: 2026-04-19
author: "ChatGPT (OpenAI)"
ai-assisted: true
ai-tool: "GPT-5.4 Thinking (OpenAI)"
status: final
priority: alta
tags: [vision, sensores, hardware, openmv, camaras, comparacion, analisis]
---

# Análisis de selección de cámaras para RoboCupJunior Soccer Open 2026

## Propósito del documento

Este documento reúne información técnica para **análisis y selección de componentes de visión** del equipo IITA Soccer Open 2026.

El objetivo es separar dos horizontes de decisión:

1. **Compra inmediata para RoboCup 2026 en Incheon**: minimizar riesgo de integración y llegar a una solución funcional en el corto plazo.
2. **Exploración para un robot más competitivo hacia el nacional de noviembre**: evaluar si conviene evolucionar a una arquitectura de visión distinta, con mayor techo de rendimiento.

> Este documento **no** define una arquitectura obligatoria del robot. Su función es dejar asentados criterios objetivos, contexto del repositorio y comparación técnica entre opciones de cámara.

---

## 1. Contexto técnico del repositorio actual

### 1.1 Stack base heredado de 2025

Del análisis del repositorio y del legado 2025 se desprende que el stack base con el que el equipo ganó el nacional 2025 estuvo centrado en:

- **Teensy 4.1** como controlador principal.
- **OpenMV H7 / H7 Plus** como cámara principal.
- **UART a 19200 baud** entre OpenMV y Teensy.
- Detección de pelota y arcos con **thresholds LAB**, QVGA y pipeline clásico de blobs.
- PCB propia **Zircon** y librería `zirconLib`.

Esto aparece de forma explícita en:

- `legacy/2025-season/README.md`
- `research/completed/2026-02-21-arquitectura-sistema-2025.md`
- `software/vision/README.md`

### 1.2 Qué significa eso para una compra inmediata

Desde el punto de vista de integración, el repositorio actual está orientado a una familia de trabajo muy concreta:

- Teensy + OpenMV.
- Comunicación serial simple.
- Código de visión pensado para OpenMV H7 / H7 Plus.
- Flujo de calibración y pruebas ya conocido por el equipo.

Por lo tanto, cualquier compra inmediata debe evaluarse no solo por especificaciones absolutas, sino por **costo de integración real** sobre el stack existente.

### 1.3 Estado del proyecto 2026

El cronograma del repositorio indica que:

- la evaluación de hardware todavía está en curso,
- la definición de mejoras todavía estaba pendiente,
- y la integración de visión, control y comunicación aún requería trabajo antes de la competencia internacional.

En consecuencia, para el robot de Incheon conviene priorizar componentes que reduzcan incertidumbre técnica.

---

## 2. Preguntas que este análisis intenta responder

1. ¿Conviene comprar ahora una **OpenMV H7 Plus** o una **OpenMV N6**?
2. ¿Qué opción sirve mejor para el robot de **Incheon** con el menor riesgo?
3. ¿Qué opción deja mejor parado al equipo para una **arquitectura más competitiva en noviembre**?
4. ¿Qué parámetros hay que mirar para no decidir solo por marketing o por especificaciones aisladas?

---

## 3. Parámetros objetivos de decisión

Para comparar cámaras en RoboCupJunior Soccer Open, los parámetros más relevantes no son solo megapíxeles o frecuencia de CPU. Los factores de decisión deberían ser los siguientes:

### 3.1 Riesgo de integración inmediata

- Compatibilidad con el código actual.
- Facilidad de hablar por UART con Teensy.
- Curva de depuración en semanas, no en meses.
- Disponibilidad de ejemplos y herramientas que el equipo ya conoce.

### 3.2 Rendimiento útil de visión

- FPS útil en condiciones reales.
- Latencia extremo a extremo (detección + envío + reacción).
- Tipo de sensor: **rolling shutter** vs **global shutter**.
- Comportamiento con movimiento rápido, giros bruscos y pelota en desplazamiento.

### 3.3 Óptica y campo visual

- Tipo de montura (M12, M8, etc.).
- Facilidad para montar lentes gran angulares.
- Posibilidad de experimentar con ópticas diferentes sin rediseñar toda la mecánica.

### 3.4 Tamaño, peso y empaquetado

- Espacio físico disponible dentro del robot.
- Altura del conjunto cámara+lente.
- Masa total y distribución de peso.

### 3.5 Interfaz y escalabilidad futura

- UART, SPI, CAN, USB, Ethernet.
- Posibilidad de migrar a arquitecturas con más cómputo central.
- Adecuación para una futura solución multicámara o distribuida.

### 3.6 Continuidad con el repositorio actual

- Aprovechamiento del trabajo ya hecho.
- Reutilización del conocimiento del equipo.
- Posibilidad de probar rápido y llegar a una versión estable.

---

## 4. Hallazgos externos relevantes para el problema

### 4.1 Tendencias observadas en equipos competitivos

A partir de TDPs y documentación pública reciente de RoboCupJunior Soccer Open / Soccer Vision, se observan dos direcciones técnicas competitivas:

#### A. Visión distribuida con varias cámaras compactas

Hay equipos fuertes que usan **múltiples cámaras pequeñas** para cubrir 360° en lugar de una sola cámara frontal. En esa línea, aparecen ejemplos de:

- sistemas de **4 cámaras con solape entre vistas**,
- cámaras compactas con lentes gran angulares,
- nodos de visión distribuidos que envían datos a controladores principales.

Esta línea es especialmente interesante para futuro, porque encaja con la idea de cubrir visión 360° en un robot pequeño.

#### B. Cómputo central más fuerte + sensores/cámaras desacoplados

También aparecen equipos competitivos con arquitecturas del tipo:

- **Raspberry Pi 5 + acelerador AI**,
- **Jetson Orin Nano**,
- microcontroladores separados para control de motores y tiempo real,
- procesamiento de visión en una capa superior distinta de la capa de control.

Esta línea ofrece mayor techo competitivo, pero también aumenta complejidad, consumo, térmica y tiempo de integración.

### 4.2 Qué enseña eso para IITA

- Para **competir rápido**, una smart camera integrada sigue siendo una decisión válida.
- Para **subir de nivel hacia noviembre**, la arquitectura probablemente deba evolucionar a visión distribuida o a una capa central de mayor cómputo.
- La compra de hoy debería evaluarse también por su valor como **plataforma de aprendizaje** para ese salto.

---

## 5. Comparación técnica: OpenMV H7 Plus vs OpenMV N6

## 5.1 OpenMV H7 Plus

### Fortalezas

- Es la opción con **mayor continuidad** con el repositorio actual.
- El stack existente del equipo ya está pensado para **OpenMV H7 / H7 Plus**.
- Usa **montura M12**, lo que facilita experimentar con lentes gran angulares.
- Es una plataforma muy conocida dentro del ecosistema OpenMV y muy razonable para detección clásica por color.
- Desde el punto de vista del equipo, es la opción de **menor riesgo cognitivo**: menos cosas nuevas al mismo tiempo.

### Debilidades

- Es una plataforma más vieja frente a la N6.
- Su ventaja está más en continuidad que en techo tecnológico.
- No ofrece Ethernet onboard como la N6.
- Su sensor y pipeline quedan más cerca del enfoque OpenMV clásico de blobs/color que de una cámara orientada a visión rápida y moderna.

### Lectura práctica

La H7 Plus es una muy buena compra si el objetivo es:

- poner una cámara en el robot **ya**,
- reusar el flujo Teensy + OpenMV + UART,
- y llegar a Incheon con la menor probabilidad de sorpresas de integración.

---

## 5.2 OpenMV N6

### Fortalezas

- Es la plataforma OpenMV más potente y moderna de la línea.
- Tiene **sensor global shutter**, una ventaja muy relevante para robots que giran rápido y para visión de pelota en movimiento.
- Tiene **mucho más techo de procesamiento**.
- Incorpora **Ethernet**, además de conectividad más rica.
- Mantiene **montura M12**, por lo que no penaliza la exploración con ópticas gran angulares.
- Queda mucho mejor posicionada como punto de partida para un sistema más ambicioso hacia noviembre.

### Debilidades

- Implica más novedad tecnológica sobre el stack actual.
- A corto plazo puede introducir más incertidumbre que una H7 Plus si el objetivo principal es cerrar una solución estable en poco tiempo.
- Aunque sigue siendo OpenMV, ya no es simplemente “la misma cámara, pero mejor”: empuja a pensar una integración más moderna.

### Lectura práctica

La N6 es una muy buena compra si el objetivo es:

- empezar a aprender desde ahora una plataforma con más futuro,
- explorar visión rápida y más robusta al movimiento,
- y usar esta compra como base de evaluación para una arquitectura más competitiva hacia el segundo semestre.

---

## 6. Comparación resumida por criterios

| Criterio | H7 Plus | N6 | Comentario práctico |
|---|---:|---:|---|
| Continuidad con el repo actual | 5/5 | 3/5 | El repositorio y el legado están muy alineados con H7/H7 Plus |
| Riesgo de integración inmediata | 5/5 | 3/5 | H7 Plus favorece llegar antes a una versión funcional |
| Techo de procesamiento | 2/5 | 5/5 | N6 es claramente superior como plataforma de cómputo |
| Robustez de visión rápida | 2/5 | 5/5 | El global shutter de N6 es un diferencial importante |
| Potencial para evolución 2026-2027 | 2/5 | 5/5 | N6 abre más puertas para noviembre y para un robot futuro |
| Compatibilidad con lentes M12 | 5/5 | 5/5 | Ambas son buenas en este punto |
| Conectividad avanzada | 2/5 | 5/5 | Ethernet en N6 es una ventaja real para exploración futura |
| Aprendizaje inmediato para el equipo | 5/5 | 4/5 | Ambas sirven, pero H7 Plus se integra más directo con el conocimiento actual |

> Escala cualitativa 1–5 construida para comparación interna del equipo. No representa benchmark de laboratorio.

---

## 7. Matriz de decisión para Incheon 2026

Para la compra inmediata, conviene ponderar fuerte la probabilidad de integración exitosa dentro del tiempo disponible.

### Ponderación sugerida para Incheon

| Criterio | Peso |
|---|---:|
| Compatibilidad con repo y código actual | 35% |
| Riesgo de integración / debugging | 25% |
| Facilidad de uso con Teensy/UART | 15% |
| Óptica y posibilidad de gran angular | 10% |
| Mejora técnica inmediata en visión | 10% |
| Valor de aprendizaje futuro | 5% |

### Resultado cualitativo

| Cámara | Resultado estimado |
|---|---:|
| H7 Plus | **Más favorable para Incheon** |
| N6 | Favorable, pero con más riesgo de integración |

### Interpretación

Si la prioridad número uno es **tener el robot listo para competir en pocos meses**, la **H7 Plus** aparece como la opción más segura.

No porque sea objetivamente la más potente, sino porque:

- el equipo ya viene de una arquitectura ganadora basada en esa familia,
- el repositorio está organizado alrededor de OpenMV H7/H7 Plus,
- y la transición desde el legado es más directa.

---

## 8. Matriz de decisión para noviembre 2026

Para el nacional de noviembre, el criterio cambia: ya no domina tanto la continuidad, sino el **techo competitivo**.

### Ponderación sugerida para noviembre

| Criterio | Peso |
|---|---:|
| Techo de procesamiento | 25% |
| Robustez para visión rápida / global shutter | 20% |
| Escalabilidad a arquitectura nueva | 20% |
| Integración con cómputo superior o red | 15% |
| Óptica / gran angular | 10% |
| Continuidad con código actual | 10% |

### Resultado cualitativo

| Cámara | Resultado estimado |
|---|---:|
| H7 Plus | Válida, pero con techo más limitado |
| N6 | **Más favorable para noviembre y evolución futura** |

### Interpretación

Si el objetivo es preparar desde ahora el camino a un robot significativamente más competitivo, la **N6** es la plataforma OpenMV que mejor conversa con esa ambición.

---

## 9. Conclusiones para compra

## 9.1 Si la prioridad es Incheon (bajo riesgo)

La compra con menor riesgo técnico es:

### **OpenMV H7 Plus**

Motivos:

- máxima continuidad con el stack actual del equipo,
- menor fricción con el código ya existente,
- menor costo de aprendizaje e integración en el corto plazo,
- posibilidad de usar lentes M12 gran angulares sin complicar la mecánica.

## 9.2 Si la prioridad es empezar a construir el salto de noviembre

La compra con mejor valor estratégico es:

### **OpenMV N6**

Motivos:

- mayor techo de procesamiento,
- sensor global shutter,
- Ethernet y mejor conectividad,
- mejor base para explorar una evolución de arquitectura.

## 9.3 Si el presupuesto permite comprar más de una cámara

La combinación más inteligente no es necesariamente elegir una sola “ganadora”, sino cubrir ambos horizontes:

### Opción recomendada de compra dual

- **1 x H7 Plus** para continuidad, pruebas rápidas y cierre de la solución inmediata.
- **1 x N6** para exploración, benchmarking y aprendizaje hacia noviembre.

Esta combinación permitiría:

- no comprometer el calendario de Incheon,
- mientras el equipo empieza a conocer la plataforma con más futuro.

---

## 10. Implicancias para una futura arquitectura multicámara

Aunque este documento no define la arquitectura futura, sí deja asentadas algunas observaciones importantes para el segundo semestre:

1. **La visión 360° con múltiples cámaras es una dirección técnicamente válida y competitiva.**
2. **Las cámaras pequeñas tienen valor estratégico real** si el robot debe cubrir varias direcciones desde un volumen reducido.
3. **Ethernet es interesante**, pero dentro de un robot pequeño no siempre es la única ni la mejor solución física; el costo en cableado y empaquetado debe evaluarse con prototipos reales.
4. Para una arquitectura multicámara, la decisión correcta ya no será solo “qué cámara comprar”, sino:
   - dónde se procesa,
   - cómo se sincronizan datos,
   - cómo se fusionan detecciones,
   - y cómo se separa la capa de control de la capa de percepción.

---

## 11. Recomendación final sintetizada

### Recomendación para el robot de Incheon

**Prioridad práctica:** H7 Plus.

### Recomendación para exploración de futuro

**Prioridad estratégica:** N6.

### Recomendación si solo se compra una cámara hoy

- **Elegir H7 Plus** si el objetivo dominante es llegar con menos riesgo a Corea.
- **Elegir N6** si el objetivo dominante es empezar cuanto antes a aprender la plataforma con la que más probablemente convenga evolucionar el robot de noviembre.

---

## 12. Ubicación elegida para este documento

Este archivo se ubica en:

`research/completed/`

Se eligió esta ubicación porque:

- es un **análisis terminado**, no una tarea pendiente;
- su naturaleza es de **investigación y comparación de componentes**;
- y no corresponde a documentación oficial de competencia ni a una instrucción de diseño cerrada.

---

## 13. Fuentes consultadas

### Internas del repositorio

- `README.md`
- `AI-INSTRUCTIONS.md`
- `research/README.md`
- `competition/timeline.md`
- `software/vision/README.md`
- `legacy/2025-season/README.md`
- `research/completed/2026-02-21-arquitectura-sistema-2025.md`
- `docs/internal/analisis-arquero-legacy.md`
- `research/references/README.md`

### Externas públicas

- Documentación oficial de OpenMV (H7 Plus y N6)
- Reglas RoboCupJunior Soccer Vision 2026
- TDPs 2025 de RoboCupJunior Soccer Open / Soccer Vision
- Documentación oficial de Raspberry Pi 5, cámara Global Shutter y Jetson Orin Nano

---

## 14. Próximo uso sugerido de este documento

Este análisis puede usarse como base para tres trabajos posteriores:

1. una tabla comparativa más detallada con proveedores y tiempos de entrega,
2. un protocolo de pruebas A/B entre cámaras,
3. y una evaluación separada de arquitectura multicámara para el robot de noviembre.

---

*Documento generado por ChatGPT (OpenAI), solicitado por Gustavo Viollaz (@gviollaz), para análisis de selección de componentes de visión del equipo IITA Soccer Open 2026.*
