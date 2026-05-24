---
title: "Placa superior V1.8 — Especificación de diseño"
date: 2026-03-29
author: "Gustavo Viollaz + ChatGPT (OpenAI)"
ai-assisted: true
ai-tool: "ChatGPT (OpenAI)"
status: review
tags: [v1-8, placa-superior, tof, imu, camaras, esp32, pcb, especificacion]
---
> [!WARNING]
> **Doc historico** -- analisis previo a la arquitectura 3-placas, hecho con ChatGPT el 29-mar-2026.
> Conservado por valor de referencia del proceso de diseno. **Para temas vigentes ver:**
> - Arquitectura actual: `docs/ARQUITECTURA-3-PLACAS-2026.md` + `docs/firmware/*`
> - Para programar un subsistema: el pack correspondiente en `hardware/electronics/*-pack/` (ver `hardware/electronics/PACKS-INDEX.md`)
> - Fuentes canonicas: `docs/FUENTES-DE-VERDAD.md`
>
> El contenido de este doc puede tener piezas todavia validas (especialmente sensores de piso) pero NO es la fuente actual.

# Placa superior V1.8 — Especificación de diseño

## Propósito

Definir concretamente qué debe tener la **placa superior** para iniciar su diseño como capa de:
- percepción
- comunicaciones
- fusión sensorial
- estimación de estado local del robot

Esta placa no debe mover motores.
Debe dedicarse a **entender mejor el entorno y el estado del robot**.

---

## 1. Función principal

Responder a estas preguntas:
- ¿hacia dónde está orientado el robot?
- ¿qué tan confiable es ese heading?
- ¿qué tengo cerca: pared, rival, línea, borde?
- ¿qué dicen las cámaras sobre pelota y arcos?
- ¿cómo fusiono eso con la placa de piso?
- ¿qué objetivo conviene mandarle a la placa actual?

---

## 2. Responsabilidades

La placa superior será responsable de:
- leer IMU(s)
- leer ToF
- integrar datos de cámaras ya procesados
- recibir datos de la placa de piso
- gestionar comunicación con compañero
- integrar módulo oficial / ESP32
- estimar estado local del robot
- emitir objetivos de alto nivel a la placa actual

---

## 3. Sensores obligatorios / recomendados

## 3.1 IMU
### Recomendación base
- **1 IMU principal obligatoria**
- **1 segunda IMU opcional** para validación / redundancia

### Regla
La segunda IMU no debe existir para promediar sin criterio.
Debe existir para:
- detectar divergencia
- validar heading
- servir de fallback

## 3.2 ToF
### Recomendación base
- **4 a 6 ToF** en la primera iteración útil
- arquitectura preparada para crecer a 8

### Distribución sugerida
- frontal izquierdo
- frontal centro
- frontal derecho
- lateral izquierdo
- lateral derecho
- trasero

### Objetivos
- detección de rival cercano
- pared
- entorno próximo
- eventos útiles para evasión y estimación de estado

## 3.3 Ultrasonidos
### Recomendación
- opcionales
- no obligatorios en la primera iteración

## 3.4 Cámaras
### Recomendación base
- **4 conectores físicos**
- **2 cámaras activas** en la primera etapa

### Regla crítica
La placa superior no debe procesar imagen cruda pesada.

Debe recibir de cada cámara:
- pelota
- arco propio
- arco rival
- confianza
- timestamp
- otras detecciones resumidas si aplica

---

## 4. Microcontrolador / procesamiento

## Recomendación base
- **Teensy 4.x** como controlador de fusión
- **ESP32** para comunicaciones y módulo oficial

### Rol del Teensy
- fusionar sensores
- administrar estado local
- producir objetivos para la placa actual

### Rol del ESP32
- comunicación con compañero
- soporte módulo oficial
- conectividad específica

---

## 5. Entradas esperadas

La placa superior debe recibir:
- paquete de placa de piso
- IMU(s)
- ToF
- datos procesados de cámaras
- estado del compañero
- estado del módulo oficial
- heartbeat del sistema

---

## 6. Salidas esperadas

La placa superior debe producir, como mínimo:
- `estimated_heading`
- `heading_confidence`
- `obstacle_map_simple`
- `wall_proximity`
- `ball_seen`
- `goal_seen`
- `rival_seen`
- `state_confidence`
- `motion_goal`
- `priority_flags`
- `fault/status`
- `timestamp`

### Hacia la placa actual
Debe mandar:
- dirección deseada
- velocidad deseada
- heading objetivo
- prioridad de evasión / seguridad
- modo o intención general

No PWM ni control de rueda directo.

---

## 7. Interfaz con cámaras

Diseñar la placa con:
- 4 conectores de cámara
- alimentación adecuada
- líneas de comunicación claras
- capacidad de usar solo 2 este año

La placa debe permitir crecer sin rehacer todo.

---

## 8. Interfaz con placa de piso

La placa superior es el principal consumidor de la salida de la placa de piso.

Debe poder:
- recibir paquetes compactos
- timestamping
- integrar confianza
- detectar pérdida o degradación del módulo

---

## 9. Interfaz con la placa actual

La relación correcta es:
- placa superior = percepción y decisión de alto nivel
- placa actual = ejecución de movimiento

La placa superior debe emitir comandos compactos y claros.

---

## 10. Consideraciones mecánicas

La placa superior debería:
- ubicarse por encima de la main board actual
- dejar paso para cableado
- no interferir con dribbler / kicker
- tener mounting robusto
- permitir montar ToF periféricos y conectores de cámara

---

## 11. Requisitos eléctricos

Debe contemplar:
- alimentación bien desacoplada
- separación razonable del ruido de potencia
- conectores robustos
- distribución clara de buses
- soporte para crecimiento
- monitoreo básico de estado

---

## 12. Modos de operación

## Modo normal
- fusión de sensores
- estado local
- envío de motion goals

## Modo calibración IMU
- validación y comparación entre IMUs
- chequeo de offsets

## Modo calibración ToF
- verificación de sensores
- consistencia geométrica

## Modo cámaras
- validación de paquetes y timing

## Modo debug
- logging extendido
- métricas internas
- paquetes enriquecidos

---

## 13. Plan de diseño recomendado

## Etapa 1
- congelar arquitectura mínima
- 1 IMU principal
- 4–6 ToF
- ESP32
- Teensy 4.x
- 4 conectores de cámara
- interfaz con placa de piso
- interfaz con placa actual

## Etapa 2
- prototipo mínimo funcional
- IMU
- pocos ToF
- ESP32
- 1 o 2 cámaras

## Etapa 3
- versión balanceada
- 2 cámaras activas
- 6 ToF
- fusión con placa de piso
- motion goals estables

## Etapa 4
- preparación para crecimiento
- segunda IMU opcional
- footprint final para 8 ToF
- conectores listos para 4 cámaras

---

## 14. Criterio de éxito

La placa superior estará lista cuando:
- produzca heading estable y confiable
- integre de forma útil la placa de piso
- entregue información robusta del entorno próximo
- reciba correctamente detecciones de cámaras
- se comunique con el otro robot
- emita objetivos claros a la placa actual

---

## 15. Definición final

> **La placa superior V1.8 será una capa de percepción y estimación de estado, basada en Teensy 4.x + ESP32, con IMU, ToF, conectores para 4 cámaras y comunicación con la placa de piso y la placa actual, entregando objetivos de alto nivel al sistema de movimiento.**
