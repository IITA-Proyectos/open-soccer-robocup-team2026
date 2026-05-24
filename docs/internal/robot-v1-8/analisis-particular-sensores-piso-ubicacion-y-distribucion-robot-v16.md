---
title: "Análisis particular de sensores de piso, ubicación y distribución para robot v16"
date: 2026-03-29
author: "Gustavo Viollaz + ChatGPT (OpenAI)"
ai-assisted: true
ai-tool: "ChatGPT (OpenAI)"
status: review
tags: [robot-v16, placa-piso, linea, distribucion, ubicacion, sensores, arquitectura]
---
> [!WARNING]
> **Doc historico** -- analisis previo a la arquitectura 3-placas, hecho con ChatGPT el 29-mar-2026.
> Conservado por valor de referencia del proceso de diseno. **Para temas vigentes ver:**
> - Arquitectura actual: `docs/ARQUITECTURA-3-PLACAS-2026.md` + `docs/firmware/*`
> - Para programar un subsistema: el pack correspondiente en `hardware/electronics/*-pack/` (ver `hardware/electronics/PACKS-INDEX.md`)
> - Fuentes canonicas: `docs/FUENTES-DE-VERDAD.md`
>
> El contenido de este doc puede tener piezas todavia validas (especialmente sensores de piso) pero NO es la fuente actual.

# Análisis particular de sensores de piso, ubicación y distribución para robot v16

## Propósito

Este documento aterriza la selección de sensores de piso al **robot actual/v16** y propone una arquitectura concreta de:
- tipo de sensor
- cantidad
- ubicación
- distribución física
- función de cada subconjunto de sensores
- lógica de lectura recomendada

No es un documento general de componentes.
Es una propuesta **particular para el robot actual de IITA**.

---

## 1. Objetivo funcional en el robot v16

La placa y distribución de sensores de piso debe resolver, de forma competitiva, estas funciones:

- detectar línea antes de salir
- saber desde qué ángulo llega la línea
- permitir calcular un vector de escape útil
- mejorar muchísimo el comportamiento del arquero cerca del área
- detectar cuando el robot fue empujado o patinó
- dar información local de movimiento al resto del sistema

---

## 2. Sensor recomendado para este robot

## Recomendación principal
Para el robot v16 recomiendo:

> **arquitectura de reflectancia IR distribuida basada en fototransistor + LED IR discreto**

### Motivos
- permite adaptar la geometría al chasis real
- es mejor para un anillo perimetral propio
- se integra mejor con una PCB inferior dedicada
- permite lectura diferencial muy robusta
- escala bien a 20–24 canales

## Recomendación secundaria para prototipo rápido
Si se busca cerrar la primera iteración muy rápido:
- TCRT5000 o equivalente

Pero lo tomaría como:
- versión rápida de validación
- no necesariamente como decisión final de arquitectura

---

## 3. Cantidad recomendada

## Primera versión seria
- **20 a 24 sensores de línea**

### Por qué no menos
Con muy pocos sensores:
- se pierde resolución angular
- aumentan casos raros
- cuesta reconstruir el vector correcto de salida

### Por qué no más de entrada
Con demasiados sensores al inicio:
- aumenta el costo de integración
- aumenta tiempo de calibración
- aumenta complejidad de PCB
- no garantiza una mejora proporcional inmediata

---

## 4. Distribución recomendada

## 4.1 Forma general
Recomiendo una distribución tipo:

- **anillo perimetral**
- con algo más de densidad en frente y laterales delanteros
- manteniendo cobertura continua alrededor del robot

## 4.2 Justificación
En Open, la línea no solo se “pisará” de frente.
También aparece:
- en giros
- en empujes laterales
- en retrocesos
- al arquero sobre la línea de fondo
- en salidas de área y bordes extraños

Por eso el anillo es muy superior a:
- 1 sensor central
- 3 sensores mínimos
- una cruz demasiado pobre

---

## 5. Distribución concreta sugerida

## Opción recomendada de 24 sensores
### Reparto sugerido
- frente: 6
- frente-laterales: 4
- laterales: 4
- trasera: 4
- transición trasera-lateral: 6 repartidos

Otra forma de verlo:
- más resolución donde más se necesita reaccionar rápido:
  - frente
  - diagonales delanteras
  - laterales delanteros

## Opción recomendada de 20 sensores
### Reparto sugerido
- frente: 5
- frente-laterales: 4
- laterales: 4
- trasera: 3
- transición restante: 4

### Criterio
Si el espacio y ruteo aprietan, prefiero un anillo de 20 **bien ubicado** antes que uno de 24 mal resuelto.

---

## 6. Altura al piso

La altura al piso es crítica.

## Recomendación
Los sensores deben ir:
- muy cerca del piso
- con altura homogénea
- con montaje rígido
- protegidos frente a flexión de chasis

### Regla práctica
La repetibilidad entre sensores importa más que una altura “mágica” única.

Lo clave es:
- que todos midan parecido
- que la placa no flexe
- que no cambie la separación al acelerar o chocar

---

## 7. Optical flow / tracking tipo mouse

Además del anillo de línea, recomiendo incorporar:

- **2 sensores optical flow**

## Ubicación recomendada
- separados geométricamente
- idealmente en diagonal o en dos puntos que permitan estimar traslación y un componente de giro

## Función
No reemplazan la línea.
Complementan la placa de piso con:
- `floor_vx`
- `floor_vy`
- estimación corta de giro
- detección de slip

---

## 8. Estrategia de lectura

## Recomendación
La lectura correcta del anillo no debería ser solo:
- “blanco sí / blanco no”

Debería producir:
- `line_angle`
- `line_strength`
- `line_width`
- `line_sensor_count`
- `escape_vector_x`
- `escape_vector_y`
- `confidence`

Y, combinada con optical flow:
- `floor_vx`
- `floor_vy`
- `floor_omega_est`
- `slip_flag`

---

## 9. Subdivisión por bancos

Para el robot v16 recomiendo dividir el anillo en:
- **4 bancos**

### Motivos
- reducir corriente instantánea
- mejorar inmunidad a luz ambiente
- facilitar lectura diferencial
- mejorar escalabilidad del firmware

### Esquema conceptual
- banco frontal
- banco lateral izquierdo
- banco lateral derecho
- banco trasero

No tienen que ser obligatoriamente esos cuadrantes exactos, pero la idea de lectura por bancos sí es muy recomendable.

---

## 10. Función competitiva por zona del robot

## Frente
Debe tener mayor densidad porque es donde:
- entra la pelota
- más importa no perder el borde
- el robot ataca
- el robot hace correcciones finas

## Laterales delanteros
Muy importantes para:
- detectar línea en giros
- reacción a empujes
- salidas diagonales

## Laterales traseros y trasera
Importantes para:
- recuperación en reversa
- defensa
- arquero
- estados anómalos de empuje

---

## 11. Recomendación específica para arquero y delantero

## Arquero
El arquero se beneficia muchísimo de:
- buena cobertura trasera
- buena cobertura lateral
- lectura clara de línea de fondo
- reconstrucción del vector de borde

## Delantero
El delantero se beneficia más de:
- mejor resolución frontal
- diagonales delanteras
- lectura rápida al entrar y salir de zona de pateo

### Conclusión
La distribución propuesta debe servir a ambos, pero si hay que priorizar:
- reforzar frente y diagonales
- sin descuidar la trasera del arquero

---

## 12. Selección final recomendada para el robot v16

### Sensor recomendado
- reflectancia IR discreta (fototransistor + LED IR)

### Cantidad recomendada
- 20 a 24 sensores

### Distribución recomendada
- anillo perimetral
- densidad algo mayor en frente y diagonales delanteras

### Complemento recomendado
- 2 sensores optical flow

### Procesamiento recomendado
- Teensy 4.0 en placa inferior dedicada

---

## 13. Qué no recomiendo para este robot

No recomiendo para la placa de piso del robot v16:
- sensor RGB visible como base principal
- muy pocos sensores
- una distribución solo frontal
- una cruz demasiado pobre
- un único sensor central
- placa pasiva sin procesamiento local

---

## 14. Ruta recomendada para este documento

Este archivo tiene sentido en el repo del robot actual/v16, como documento específico de diseño.

### Ruta sugerida
`docs/internal/robot-v16/analisis-particular-sensores-piso-ubicacion-y-distribucion-robot-v16.md`

Si prefieren mantener todo en la misma línea documental de V1.8/V2, también podría ir en:
`docs/internal/robot-v2/analisis-particular-sensores-piso-ubicacion-y-distribucion-robot-v16.md`

---

## 15. Definición final

> **Para el robot v16 de IITA, la mejor arquitectura de sensores de piso es un anillo perimetral de 20–24 sensores de reflectancia IR discretos, con mayor densidad en frente y diagonales, complementado por 2 sensores optical flow y procesado por una placa de piso dedicada con Teensy 4.0.**
