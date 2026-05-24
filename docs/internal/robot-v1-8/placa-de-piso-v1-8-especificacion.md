---
title: "Placa de piso V1.8 — Especificación de diseño"
date: 2026-03-29
author: "Gustavo Viollaz + ChatGPT (OpenAI)"
ai-assisted: true
ai-tool: "ChatGPT (OpenAI)"
status: review
tags: [v1-8, placa-piso, linea, optical-flow, sensores, pcb, especificacion]
---
> [!WARNING]
> **Doc historico** -- analisis previo a la arquitectura 3-placas, hecho con ChatGPT el 29-mar-2026.
> Conservado por valor de referencia del proceso de diseno. **Para temas vigentes ver:**
> - Arquitectura actual: `docs/ARQUITECTURA-3-PLACAS-2026.md` + `docs/firmware/*`
> - Para programar un subsistema: el pack correspondiente en `hardware/electronics/*-pack/` (ver `hardware/electronics/PACKS-INDEX.md`)
> - Fuentes canonicas: `docs/FUENTES-DE-VERDAD.md`
>
> El contenido de este doc puede tener piezas todavia validas (especialmente sensores de piso) pero NO es la fuente actual.

# Placa de piso V1.8 — Especificación de diseño

## Propósito

Definir concretamente qué debe tener la **placa de piso** para comenzar el diseño.

Esta placa no debe resolver estrategia ni percepción global.

Debe convertirse en un:

> **sensor inteligente del contacto del robot con la cancha**

---

## 1. Función principal

Responder con alta frecuencia a estas preguntas:
- ¿dónde está tocando la línea?
- ¿con qué intensidad y geometría?
- ¿cómo se está moviendo realmente el robot sobre el piso?
- ¿hay patinamiento?
- ¿qué vector de escape conviene sugerir?

---

## 2. Responsabilidades

La placa de piso será responsable de:
- lectura del anillo de sensores de línea
- lectura de 2 sensores optical flow al piso
- preprocesamiento de datos
- cálculo de variables útiles
- envío de información a la placa superior
- autodiagnóstico básico

---

## 3. Sensores obligatorios

## 3.1 Anillo de línea
### Recomendación
- **20 a 24 sensores**
- distribución perimetral
- algo más de densidad en frente y laterales delanteros
- footprint preparado para ampliación futura

### Variables a calcular
- `line_angle`
- `line_strength`
- `line_width`
- `line_sensor_count`
- `escape_vector`
- `confidence`

## 3.2 Optical flow al piso
### Recomendación
- **2 sensores**
- separados geométricamente
- montaje rígido
- altura al piso cuidadosamente controlada

### Objetivo
Obtener:
- `floor_vx`
- `floor_vy`
- `floor_omega_est`
- `slip_flag`

---

## 4. Microcontrolador

## Recomendación
- **Teensy 4.0**

### Motivos
- buen margen de muestreo
- baja latencia
- capacidad de filtrado
- ecosistema ya conocido por el equipo

---

## 5. Salida funcional esperada

La placa no debería transmitir solo datos crudos.

Debe enviar un paquete compacto con, al menos:
- `line_angle`
- `line_strength`
- `line_width`
- `line_sensor_count`
- `escape_vector_x`
- `escape_vector_y`
- `floor_vx`
- `floor_vy`
- `floor_omega_est`
- `slip_flag`
- `confidence`
- `timestamp`
- `alive/fault`

---

## 6. Entradas necesarias

La placa debe poder recibir:
- alimentación
- sincronización / heartbeat
- parámetros de calibración
- comando de modo (normal, calibración, debug)

---

## 7. Interfaz recomendada

La placa debe comunicarse principalmente con la **placa superior**.

Criterios:
- baja latencia
- paquete compacto
- validación básica
- timestamps
- estado de vida del módulo

---

## 8. Consideraciones mecánicas

La placa debe diseñarse para:
- ir lo más baja posible
- mantener altura consistente al piso
- evitar flexión
- permitir recambio simple
- respetar geometría del chasis

La calidad mecánica del montaje impacta directamente en:
- lectura de línea
- lectura de optical flow
- estabilidad de odometría
- repetibilidad

---

## 9. Requisitos eléctricos

Debe contemplar:
- regulación estable
- desacople local
- conectores robustos
- ruteo claro hacia sensores
- diagnóstico básico de alimentación

---

## 10. Modos de operación

## Modo normal
- entrega variables resumidas al sistema

## Modo calibración línea
- ajuste de thresholds
- verificación de uniformidad

## Modo calibración floor flow
- validación de altura
- validación de señal
- medición de ruido

## Modo debug
- acceso temporal a información más detallada

---

## 11. Plan de diseño recomendado

## Etapa 1
- congelar cantidad de sensores
- congelar ubicación
- congelar altura
- congelar interfaz de salida

## Etapa 2
- validación mecánica rápida
- pruebas de optical flow
- pruebas de línea

## Etapa 3
- esquemático
- conectores
- alimentación
- MCU

## Etapa 4
- PCB
- ruteo
- mounting

## Etapa 5
- firmware
- filtrado
- paquetes
- heartbeat

## Etapa 6
- calibración
- slip
- latencia
- validación en cancha

---

## 12. Criterio de éxito

La placa de piso estará lista cuando:
- detecte línea desde múltiples ángulos de manera repetible
- entregue un vector de escape útil
- mida movimiento local con estabilidad razonable
- detecte slip de forma útil
- entregue datos compactos y confiables a la placa superior

---

## 13. Definición final

> **La placa de piso V1.8 será una placa dedicada a línea y odometría local al piso, basada en 20–24 sensores de línea, 2 sensores optical flow y un microcontrolador capaz de convertir esas lecturas en un paquete inteligente y compacto para la placa superior.**
