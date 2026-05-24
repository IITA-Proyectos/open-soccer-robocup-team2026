---
title: "Arquitectura entre placas y flujo de datos — Robot actual V1.8 / camino a V2"
date: 2026-03-29
author: "Gustavo Viollaz + ChatGPT (OpenAI)"
ai-assisted: true
ai-tool: "ChatGPT (OpenAI)"
status: review
tags: [arquitectura, placas, flujo-de-datos, v1-8, robot-v2, comunicaciones, integracion]
---
> [!WARNING]
> **Doc historico** -- analisis previo a la arquitectura 3-placas, hecho con ChatGPT el 29-mar-2026.
> Conservado por valor de referencia del proceso de diseno. **Para temas vigentes ver:**
> - Arquitectura actual: `docs/ARQUITECTURA-3-PLACAS-2026.md` + `docs/firmware/*`
> - Para programar un subsistema: el pack correspondiente en `hardware/electronics/*-pack/` (ver `hardware/electronics/PACKS-INDEX.md`)
> - Fuentes canonicas: `docs/FUENTES-DE-VERDAD.md`
>
> El contenido de este doc puede tener piezas todavia validas (especialmente sensores de piso) pero NO es la fuente actual.

# Arquitectura entre placas y flujo de datos — Robot actual V1.8 / camino a V2

## Propósito

Este documento define cómo deben relacionarse entre sí las tres capas principales del robot actual mejorado:

- **placa actual**
- **placa de piso**
- **placa superior**

El objetivo es dejar explícito:

- qué información produce cada placa
- qué información consume cada placa
- qué decisiones toma cada una
- qué flujo de datos debe implementarse
- cómo evitar acoplamientos incorrectos

---

## 1. Principio rector

La arquitectura correcta no es “todas las placas hablan con todo”.

La arquitectura correcta es:

> **cada placa produce el dato que mejor sabe producir, y consume solo lo que necesita para cumplir su rol**

---

## 2. Roles de las tres placas

## 2.1 Placa actual

### Rol
**Control de movimiento y actuadores**

### Responsabilidades
- control de motores
- primitivas de movimiento
- ejecución de consignas
- actuadores
- seguridad inmediata del robot
- watchdog de locomoción

### Qué no debe hacer en la nueva arquitectura
- fusionar demasiados sensores
- estimar estado global
- recibir cámaras directamente
- decidir localización del robot

---

## 2.2 Placa de piso

### Rol
**Sensor inteligente del contacto robot–cancha**

### Responsabilidades
- leer anillo de línea
- leer 2 sensores optical flow
- calcular vector de línea
- estimar movimiento local corto
- detectar slip
- generar un paquete compacto de estado del suelo

### Qué no debe hacer
- mover motores
- interpretar estrategia
- comunicarse con el otro robot
- procesar visión

---

## 2.3 Placa superior

### Rol
**Percepción, fusión sensorial, comunicaciones y estimación de estado**

### Responsabilidades
- leer IMU(s)
- leer ToF
- integrar datos de cámara ya procesados
- recibir paquete de placa de piso
- comunicarse con compañero y/o módulo oficial
- construir una estimación local del estado del robot
- emitir objetivos de alto nivel a la placa actual

### Qué no debe hacer
- generar PWM de motores
- cerrar lazo rápido de ruedas
- procesar video crudo pesado
- reemplazar el motion controller

---

## 3. Arquitectura conceptual

```text
                           +----------------------+
                           |  CAMARAS (2 ahora,   |
                           |  4 preparadas)       |
                           |  entregan detecciones |
                           +----------+-----------+
                                      |
                                      v
+------------------+        +----------------------+        +----------------------+
|  PLACA DE PISO   |------->|   PLACA SUPERIOR     |------->|    PLACA ACTUAL      |
|                  |        |                      |        |                      |
| linea            |        | IMU principal        |        | control de motores   |
| optical flow     |        | IMU secundaria opc.  |        | primitivas           |
| vector escape    |        | ToF                  |        | actuadores           |
| slip             |        | ESP32 / radio        |        | seguridad            |
| odometria local  |        | fusión sensorial     |        | watchdog locomocion  |
+------------------+        +----------------------+        +----------------------+
                                      |
                                      v
                           +----------------------+
                           |  COMPAÑERO / MODULO  |
                           |  OFICIAL / ESP32     |
                           +----------------------+
```

---

## 4. Flujo de datos recomendado

## 4.1 Flujo desde la placa de piso hacia la superior

La placa de piso debe enviar un paquete compacto, por ejemplo:

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

### Interpretación
La placa superior usa esto para:
- entender relación con el borde
- mejorar estimación local
- detectar deslizamiento
- corregir decisiones de movimiento y evasión

---

## 4.2 Flujo desde cámaras hacia la placa superior

Las cámaras no deberían enviar imagen cruda.

Deben enviar datos procesados, como por ejemplo:

- `ball_angle`
- `ball_distance_est`
- `ball_confidence`
- `own_goal_angle`
- `opp_goal_angle`
- `goal_size_est`
- `vision_timestamp`

### Interpretación
La placa superior usa esto para:
- ubicar pelota
- inferir orientación útil
- estimar relación con arcos
- construir world model local

---

## 4.3 Flujo desde ToF / IMU a la placa superior

Estos sensores están directamente asociados a la percepción del entorno y a la estimación de estado.

### ToF
Aportan:
- rival cercano
- pared
- borde próximo
- eventos útiles para evasión

### IMU
Aporta:
- heading
- cambio angular
- validación de estabilidad
- redundancia si hay segunda IMU

---

## 4.4 Flujo desde la placa superior hacia la placa actual

La placa superior no debe mandar órdenes de bajo nivel a motores.

Debe mandar objetivos de alto nivel, por ejemplo:

- `desired_direction`
- `desired_speed`
- `desired_heading`
- `avoidance_priority`
- `behavior_mode`
- `state_confidence`
- `timestamp`

### Interpretación
La placa actual transforma eso en:
- consignas por motor
- correcciones de orientación
- ejecución segura
- comportamiento locomotor real

---

## 5. Quién decide qué

## Placa de piso decide:
- vector de línea
- lectura útil del suelo
- slip
- odometría local corta

## Placa superior decide:
- heading estimado
- hipótesis de estado local
- si hay rival / pared / borde
- prioridad de evasión
- objetivo de movimiento
- intención táctica local

## Placa actual decide:
- cómo mover el robot realmente
- cómo repartir a motores
- cómo proteger locomoción y actuadores
- cómo reaccionar ante fallas inmediatas

---

## 6. Frecuencias relativas recomendadas

No hace falta fijar números finales exactos ahora, pero sí respetar este orden:

## Muy rápido
- control de motores
- seguridad locomotora

## Rápido
- línea
- optical flow
- IMU

## Medio
- ToF
- integración de estado

## Más lento
- estrategia local
- comunicación entre robots
- decisiones de comportamiento

### Regla de oro
> **La percepción lenta nunca debe bloquear el control rápido**

---

## 7. Manejo de fallas y degradación elegante

La arquitectura debe seguir funcionando aunque un subsistema degrade.

## Casos ejemplo

### Si falla la placa de piso
- el robot pierde línea rica y odometría al piso
- pero sigue moviéndose con la placa actual
- la placa superior degrada confianza

### Si falla una cámara
- la placa superior sigue usando IMU, ToF y piso
- baja la confianza en pelota o arcos
- no debe colgar todo el sistema

### Si falla un ToF
- debe marcarse como sensor degradado
- no debe contaminar todas las decisiones

### Si falla la comunicación con el compañero
- el robot sigue jugando en modo autónomo local

### Si falla la placa superior
- idealmente la placa actual debería poder entrar en un modo de juego degradado y seguro

---

## 8. Señales mínimas de salud por módulo

Cada placa debería poder publicar, al menos:

- `alive`
- `fault`
- `confidence`
- `timestamp`
- `mode`

Esto evita cajas negras y ayuda muchísimo a depurar.

---

## 9. Buses e interconexión

Este documento no congela aún el bus definitivo, pero sí congela el principio:

- la placa de piso debe tener un enlace simple y robusto hacia la placa superior
- la placa superior debe ser el nodo central de percepción
- la placa actual debe recibir comandos compactos y claros

### Filosofía
No hacer que:
- la placa actual lea todo
- la placa de piso hable con cámaras
- la placa superior genere PWM de motores

---

## 10. Secuencia de integración recomendada

## Paso 1
Integrar placa de piso sola con telemetría.

## Paso 2
Integrar placa superior sola con IMU + ToF + cámara(s).

## Paso 3
Integrar placa de piso y superior entre sí.

## Paso 4
Integrar salida de objetivos de la placa superior hacia la placa actual.

## Paso 5
Agregar comunicación entre robots y/o módulo oficial.

---

## 11. Criterio correcto de arquitectura

La forma más sana de pensar esta integración es:

> **la placa de piso mide el suelo, la placa superior interpreta el estado, la placa actual ejecuta movimiento**

Si esa división se respeta, la V1.8 va a ser:
- más ordenada
- más confiable
- más fácil de probar
- y mucho más útil como escalón hacia la V2

---

## 12. Definición final

> **La arquitectura entre placas del robot actual V1.8 deberá implementarse como un sistema de tres niveles, en el que la placa de piso produce información inteligente del contacto con la cancha, la placa superior fusiona sensores y estima el estado local del robot, y la placa actual ejecuta el movimiento y protege la locomoción.**
