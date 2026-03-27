---
title: "Protocolo: Test de giróscopo BNO055 y movimiento con PID"
date: 2026-03-27
author: "Claude (Anthropic)"
ai-assisted: false
status: final
tags: [giroscopo, movilidad, sensores, bno055, pid]
robot: delantero
---

# Protocolo: Test de giróscopo BNO055 y movimiento con PID

## Objetivo

Verificar que:
1. El giróscopo BNO055 se inicializa correctamente
2. El heading se establece en 0° al encender
3. El control PID mantiene al robot en línea recta
4. La corrección funciona tanto adelante como atrás

## Materiales necesarios

- Robot delantero (ROBOT2) con Teensy
- Cable USB para programar/monitorear
- Arduino IDE con Teensyduino instalado
- Librería Adafruit BNO055 instalada
- Superficie lisa para pruebas de movimiento
- Espacio libre de al menos 2m x 2m

## Archivo de test

`software/staging/shared/test-gyro-movimiento-basico.ino`

## Lecciones aprendidas relacionadas

Ver `docs/internal/lecciones-pid-movimiento.md` para detalles sobre:
- Por qué se invierte la corrección al ir atrás
- Cómo manejar el debounce del botón
- Parámetros PID recomendados

## Procedimiento

### Preparación

1. Conectar el robot al computador por USB
2. Abrir Arduino IDE
3. Seleccionar placa: **Teensy 4.0**
4. Abrir el archivo `test-gyro-movimiento-basico.ino`
5. Compilar y subir al Teensy

### Ejecución

#### Fase 1: Inicialización del giróscopo

1. Abrir Serial Monitor a **19200 baud**
2. Colocar el robot en una superficie estable
3. **NO MOVER** el robot durante los primeros 5 segundos
4. Observar los mensajes de calibración
5. Esperar hasta que el LED quede **fijo**

**Verificar:**
- [ ] El BNO055 se detecta correctamente
- [ ] La calibración del giróscopo alcanza nivel 3
- [ ] El heading offset se establece
- [ ] El LED queda fijo

#### Fase 2: Test de heading (TEST 1)

1. Presionar **BOTÓN** (pin 9)
2. Girar el robot manualmente 90° a la derecha → debería mostrar ~-90°
3. Girar 90° a la izquierda → debería mostrar ~+90°
4. Volver a posición inicial → debería mostrar ~0°

**Verificar:**
- [ ] El heading cambia correctamente
- [ ] El sentido de giro es correcto
- [ ] Vuelve a ~0° al volver a posición inicial

#### Fase 3: Loop de movimiento

1. Presionar **BOTÓN** para iniciar el loop
2. El robot hará ciclos infinitos de:
   - ADELANTE 5 segundos
   - Pausa 0.5 segundos
   - ATRÁS 5 segundos
   - Pausa 0.5 segundos
   - (repetir)

**Verificar:**
- [ ] El robot avanza recto hacia adelante
- [ ] El robot retrocede recto hacia atrás
- [ ] El heading se mantiene cerca de 0° durante el movimiento
- [ ] No da vuelta en U al ir hacia atrás

## Indicadores LED

| Estado LED | Significado |
|------------|-------------|
| Fijo | Yendo adelante / Giróscopo listo |
| Apagado | Yendo atrás |
| Parpadeo rápido | Pausa entre movimientos |
| Parpadeo lento | Test completado |

## Criterios de éxito

| Criterio | Pasa | Falla |
|----------|------|-------|
| BNO055 detectado | Se muestra "OK!" | Error o timeout |
| Heading correcto | ±10° del valor esperado | Desviación >30° |
| Movimiento adelante | Línea recta | Curva o giro |
| Movimiento atrás | Línea recta | Vuelta en U |
| Heading durante movimiento | Se mantiene ±5° | Deriva >15° |

## Ajuste de parámetros PID

Si el robot no mantiene la línea recta, ajustar en el código:

```cpp
float Kp = 3.0;    // Subir si no corrige suficiente
float Ki = 0.05;   // Subir si hay error constante
float Kd = 0.5;    // Subir si oscila
```

## Notas

- El test funciona en **loop infinito** - no necesita reiniciar para repetir
- La corrección PID está **invertida** para el movimiento hacia atrás
- Este test **NO usa zirconLib** - los pines están definidos directamente
