---
title: "Matriz comparativa de sensores de piso candidatos para IITA"
date: 2026-03-29
author: "Gustavo Viollaz + ChatGPT (OpenAI)"
ai-assisted: true
ai-tool: "ChatGPT (OpenAI)"
status: review
tags: [sensores, piso, linea, reflectancia, componentes, seleccion, comparativa]
---
> [!WARNING]
> **Doc historico** -- analisis previo a la arquitectura 3-placas, hecho con ChatGPT el 29-mar-2026.
> Conservado por valor de referencia del proceso de diseno. **Para temas vigentes ver:**
> - Arquitectura actual: `docs/ARQUITECTURA-3-PLACAS-2026.md` + `docs/firmware/*`
> - Para programar un subsistema: el pack correspondiente en `hardware/electronics/*-pack/` (ver `hardware/electronics/PACKS-INDEX.md`)
> - Fuentes canonicas: `docs/FUENTES-DE-VERDAD.md`
>
> El contenido de este doc puede tener piezas todavia validas (especialmente sensores de piso) pero NO es la fuente actual.

# Matriz comparativa de sensores de piso candidatos para IITA

## Propósito

Este documento reúne, en formato de selección de componentes, las principales familias de sensores de piso que tienen sentido para robots de fútbol tipo RoboCupJunior Soccer y proyectos afines.

Su objetivo es servir como documento **más general**, reusable en un repositorio transversal de robótica, visión y electrónica, y no solo para un robot específico.

---

## 1. Criterio general de selección

Para sensores de piso orientados a línea/borde y navegación local, los criterios más importantes son:

- contraste útil entre línea blanca y piso
- inmunidad a luz ambiente
- velocidad de lectura
- estabilidad con pequeñas variaciones de altura
- facilidad de integración en PCB propia
- repetibilidad entre unidades
- costo
- disponibilidad
- escalabilidad a arreglos de muchos sensores

---

## 2. Familias candidatas

## A. Fototransistor + LED IR discreto
### Ejemplos de familia
- fototransistores tipo SFH309 o equivalentes
- LED IR discretos emparejados

### Características
- solución analógica custom
- máxima libertad geométrica
- excelente para anillos o arreglos perimetrales propios
- requiere más diseño electrónico y calibración

## B. TCRT5000 / similares
### Características
- sensores reflectivos integrados muy comunes
- fáciles de conseguir
- muy buenos para prototipos rápidos
- muy usados en robótica educativa y experimental

## C. QTR / QTRX tipo Pololu
### Características
- arreglos de reflectancia muy documentados
- electrónica bastante resuelta
- buena referencia de benchmarking
- menos libertad geométrica que una PCB totalmente custom

## D. CNY70 / QRD1114 / familias clásicas
### Características
- sensores históricos de reflectancia
- válidos para soluciones simples o compactas
- no suelen ser mi primera recomendación para una arquitectura “top”

## E. Sensores RGB / color visibles
### Características
- distinguen color visible y no solo reflectancia IR
- útiles en otras aplicaciones
- no recomendados como sensor principal de borde de cancha

---

## 3. Matriz comparativa principal

| Familia | Tipo | Precisión útil de línea | Tiempo de lectura | Altura de trabajo típica | Inmunidad a luz ambiente | Facilidad de integración | Flexibilidad geométrica | Costo relativo | Veredicto |
|---|---|---|---|---|---|---|---|---|---|
| Fototransistor + LED IR discreto | Reflectancia IR analógica custom | Alta | Muy rápida | Muy baja, pocos mm | Alta si se usa LED OFF/ON | Media | Muy alta | Bajo/medio | **Mejor opción para PCB propia competitiva** |
| TCRT5000 / similares | Reflectancia IR integrada | Media/alta | Rápida | Baja | Media/alta con lectura diferencial | Alta | Media | Bajo | **Excelente para prototipo serio** |
| QTR / QTRX | Reflectancia IR en arrays | Alta | Rápida | Baja | Alta en implementación cuidada | Alta | Media/baja | Medio | **Muy buena referencia funcional** |
| CNY70 / QRD1114 | Reflectancia IR integrada clásica | Media | Rápida | Muy baja | Media | Media | Media | Bajo | **Aceptable, no favorita para top performance** |
| Sensores RGB visibles | Color visible / reflectancia visible | Baja/media para línea blanca competitiva | Media/lenta | Variable | Baja/media | Media | Baja | Medio/alto | **No recomendados como sensor principal** |

---

## 4. Comparativa por criterio

## 4.1 Precisión útil sobre borde
La mejor precisión práctica no viene solo del sensor, sino de:
- óptica
- altura
- rigidez mecánica
- estrategia de lectura
- cantidad y distribución de canales

### Ranking práctico
1. Fototransistor + IR discreto bien diseñado
2. QTR/QTRX
3. TCRT5000 / similares
4. CNY70 / QRD1114
5. RGB visibles

---

## 4.2 Tiempo de lectura
Para detección de línea en robots rápidos:

### Más convenientes
- reflectancia IR analógica
- lectura multiplexada o por bancos
- sensores con salida simple y rápida

### Menos convenientes
- sensores RGB con procesamiento más pesado o integración más lenta

---

## 4.3 Altura de trabajo
En casi todas las familias útiles, la altura de trabajo real es **muy baja** y el montaje mecánico es decisivo.

### Regla clave
El mejor sensor mal montado rinde peor que un sensor solo correcto bien montado.

---

## 4.4 Inmunidad a color y luz ambiente
El factor decisivo no es solo el componente, sino la técnica de medición.

### Técnica recomendada
- medir ambiente con LED OFF
- medir reflectancia con LED ON
- restar ambas lecturas

Eso favorece mucho a las arquitecturas IR dedicadas.

---

## 4.5 Escalabilidad a muchos canales
Si el objetivo es un anillo de 20–24 sensores:

### Mejor arquitectura
- fototransistor + LED discreto
- multiplexado analógico o bancos
- microcontrolador dedicado

### Arquitecturas menos cómodas
- arrays rígidos si la geometría final no acompaña
- sensores RGB individuales

---

## 5. Selección recomendada por escenario

## Escenario 1 — Prototipo rápido
### Recomendación
- TCRT5000 o equivalente
- o módulos / arreglos tipo QTR como benchmark

### Objetivo
- validar idea
- medir contraste
- ganar velocidad de implementación

## Escenario 2 — PCB propia seria
### Recomendación
- fototransistor + LED IR discreto
- topología analógica propia
- multiplexado y lectura diferencial

### Objetivo
- máxima libertad geométrica
- mejor integración en anillo
- mejor camino a arquitectura competitiva

## Escenario 3 — Comparativa de laboratorio
### Recomendación
Probar en paralelo:
- 1 canal discreto
- 1 canal TCRT5000
- 1 referencia tipo QTR/QTRX

### Objetivo
- medir contraste
- sensibilidad a altura
- ruido
- inmunidad a iluminación

---

## 6. Decisión recomendada para IITA

Si el objetivo es diseñar una placa de piso seria para robots competitivos:

> **La familia más recomendable es una arquitectura de reflectancia IR analógica propia, basada en fototransistor + LED IR discreto.**

Si el objetivo inmediato es reducir tiempo de prototipo:

> **TCRT5000 o una referencia tipo QTR/QTRX son excelentes puntos de comparación y validación.**

---

## 7. Qué no elegiría como sensor principal

No elegiría como base del sistema de línea:
- sensores RGB visibles
- una solución de muy pocos puntos
- sensores sin lectura diferencial
- un único sensor central

---

## 8. Recomendación final

### Recomendación general de compra y evaluación
1. Armar un banco comparativo con:
   - canal discreto IR
   - TCRT5000
   - referencia tipo QTR/QTRX
2. Medir:
   - contraste blanco/fondo
   - sensibilidad a altura
   - velocidad útil
   - ruido
   - inmunidad a luz ambiente
3. Elegir la arquitectura final en base a datos y no solo a disponibilidad

---

## 9. Uso recomendado de este documento

Este archivo está pensado para ir a un **repo más general**, por ejemplo uno orientado a:
- recursos de robótica
- machine vision
- componentes
- electrónica aplicada

## Ruta sugerida
Si se lo quiere guardar en un repo general, una ruta razonable sería algo del estilo:

`docs/components/sensors/floor-line-sensors/matriz-comparativa-de-sensores-de-piso-candidatos-para-iita.md`

o equivalente según la estructura del repositorio destino.
