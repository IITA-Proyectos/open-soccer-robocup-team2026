> ⚠️ **STAGING CONGELADO (2026-06-03) — NO subir más material a `software/staging/`.**
> Antes de tocar o agregar algo, leé **`software/staging/up_board/00-LEER-PRIMERO-recomendaciones-reuso.md`**.
> Este scratch repite bugs ya resueltos. Usá el stack de PRODUCCIÓN testeado en
> `software/teensy/Soccer 2026/src/` (ver el "mapa de reúso" del documento).

---
title: "Test: Dibujar un círculo sin girar (traslación circular con heading fijo)"
date: 2026-04-02
author: "Claude (Anthropic - Claude Opus 4.6)"
requested-by: "María Virginia Viollaz (@mariaviollaz)"
ai-assisted: true
ai-tool: "Claude (Anthropic - Claude Opus 4.6)"
status: staging
tags: [test, movimiento, circulo, omnidireccional, pid, giroscopo, bno055]
robot: robot-2-delantero
area: movilidad
tipo: protocolo
---

# Test: Dibujar un círculo sin girar

## Descripción general

El robot traza un círculo sobre el piso **sin rotar** — siempre mirando al frente, como un compás que mantiene su orientación. Combina continuamente el movimiento adelante/atrás con el lateral usando funciones seno y coseno para que la dirección de traslación recorra 360° suavemente.

**Archivo**: `test-circulo.ino`

## Cómo funciona

Un ángulo va de 0° a 360° a lo largo del tiempo. En cada instante:

```
factor_derecha  = cos(ángulo)   → componente lateral
factor_adelante = sin(ángulo)   → componente adelante/atrás
```

Esto hace que el robot se mueva así:

| Ángulo | Dirección | Motores principales |
|--------|-----------|-------------------|
| 0° | Puro derecha | M1=-55, M2=+55, M3=+100 |
| 90° | Puro adelante | M1=+85, M2=+85, M3=0 |
| 180° | Puro izquierda | M1=+55, M2=-55, M3=-100 |
| 270° | Puro atrás | M1=-85, M2=-85, M3=0 |

Entre estos puntos, los valores se mezclan suavemente. El PID del giroscopio mantiene el heading en 0° todo el tiempo.

## Hardware requerido

- Teensy Robot 2 (delantero) con placa Zircon
- Giroscopio BNO055 en I2C `0x28`
- 3 motores con drivers H-bridge
- Botón en pin 9
- Espacio libre de al menos 1.5m x 1.5m

## Instrucciones de uso

1. Subir al Teensy, Serial Monitor a 19200 baud
2. No mover durante calibración (~5 seg)
3. Botón → test heading (verificar girando manualmente)
4. Botón → iniciar círculo
5. Observar la trayectoria y ajustar con comandos Serial

## Comandos Serial

| Comando | Acción |
|---------|--------|
| `+` | Círculo más rápido (tiempo -1s, círculo más chico) |
| `-` | Círculo más lento (tiempo +1s, círculo más grande) |
| `d` | Cambiar sentido (horario / antihorario) |
| `p` | Pausar / reanudar |
| `v` | Ver valores actuales |

## Parámetros ajustables

| Parámetro | Valor default | Efecto |
|-----------|--------------|--------|
| `TIEMPO_CIRCULO` | 10000ms (10s) | Tiempo para una vuelta completa. Más largo = círculo más grande |
| `VELOCIDAD_CIRCULO` | 85 | Velocidad componente adelante/atrás. Subir si el círculo sale achatado en esa dirección |
| `VEL_M1/M2` | 55 | Velocidad lateral frontales. Del test lateral calibrado |
| `VEL_M3` | 100 | Velocidad lateral trasero. Del test lateral calibrado |
| `SENTIDO_CIRCULO` | 1 | 1=antihorario, -1=horario |

## Qué observar

- **Forma del círculo**: si sale ovalado, ajustar VELOCIDAD_CIRCULO. Si se estira hacia los costados, bajar. Si se estira adelante/atrás, subir.
- **Heading**: debería mantenerse cerca de 0° todo el tiempo. Si se desvía mucho, el PID necesita ajuste.
- **Serial**: muestra el ángulo actual, heading, y velocidades de cada motor en tiempo real.

## Origen del código

Usa las mismas funciones de motor, PID y giroscopio de los tests que ya funcionan, sin cambiar valores calibrados.

| Componente | Origen |
|------------|--------|
| motor1/2/3(), parar() | test-motores-lateral-simple v4 |
| Velocidades laterales | test-motores-lateral-simple (calibrado María) |
| PID (Kp=3, Ki=0.05, Kd=0.5) | test-motores-lateral-simple |
| inicializarGyro(), leerHeading() | test-gyro-movimiento-basico |
| Botón con edge detection | test-gyro-movimiento-basico |

## Documentación relacionada

- `test-4-movimientos/` — Test de los 4 movimientos por separado
- `test-gyro-movimiento-basico/` — Test adelante/atrás
- `test-motores-lateral-simple/` — Test lateral v4
- `docs/internal/cinematica-omnidireccional-movimientos.md` — Modelo cinemático
