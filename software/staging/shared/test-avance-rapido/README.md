---
title: "Test: Avance rápido en línea recta (2 seg, rampa + PID)"
date: 2026-04-10
author: "Claude (Anthropic - Claude Opus 4.6)"
requested-by: "María Virginia Viollaz (@mariaviollaz)"
ai-assisted: true
ai-tool: "Claude (Anthropic - Claude Opus 4.6)"
status: staging
tags: [test, movimiento, avance, rapido, rampa, pid, giroscopo, bno055]
robot: robot-1-arquero
area: movilidad
tipo: protocolo
---

# Test: Avance rápido en línea recta (2 seg, rampa + PID)

## Descripción general

Programa que hace avanzar al robot lo más rápido posible durante 2 segundos manteniendo la línea recta con corrección PID de heading. Usa una rampa de aceleración para evitar que las ruedas patinen al arrancar, y un PID con corrección limitada para no desestabilizar el robot a alta velocidad.

**Archivo**: `test-avance-rapido.ino`

## Estrategia de velocidad

```
t=0ms      → Arranca a 60 PWM (mínimo para mover sin saltar)
t=0-400ms  → Rampa lineal: 60 → 230 PWM
t=400-2000ms → Crucero a 230 PWM constante
t=2000ms   → Freno total
```

La velocidad máxima es 230 PWM. Con corrección PID de ±50, los motores pueden llegar a 240 como máximo, lejos de la zona peligrosa de 250.

## PID para alta velocidad

A alta velocidad, la corrección PID tiene que ser más chica que en tests a baja velocidad, porque una diferencia grande entre motores hace que el robot oscile.

| Parámetro | Valor | Nota |
|-----------|-------|------|
| Kp | 4.5 | Más alto que el test básico (3.0) porque a alta vel se necesita reacción fuerte |
| Ki | 0.05 | Bajo para no acumular error |
| Kd | 0.8 | Alto para frenar la corrección antes de pasarse |
| MAX_CORRECCION | 50 | A vel 230, motores van entre 180-240 |

### Historial de ajustes

| Iteración | Kp | MAX_CORR | Resultado |
|-----------|----|----------|-----------|
| 1 | 2.5 | 25 | Corregía lento, tardaba en volver al centro |
| 2 | 3.5 | 35 | Robot curvaba (arco abierto), corrección insuficiente |
| 3 (actual) | 4.5 | 50 | Funciona bien con batería cargada |

## Lección aprendida: efecto de la batería

Durante las pruebas el robot curvaba consistentemente. La causa no era el PID sino la **batería baja**. Con batería descargada (~7V), los motores no entregan la misma potencia y uno queda más débil que el otro. Con batería cargada (~8V+), el robot fue recto.

**Regla para competencia**: siempre cargar las baterías antes de cada partido.

## Hardware

- Probado en Robot 1 (arquero) con pines ROBOT1
- Giroscopio BNO055 en I2C (0x28)
- Botón en pin 9
- Monitor Serial a 19200 baud

## Pines de motores (Robot 1 — arquero)

| Motor | INA | INB | PWM |
|-------|-----|-----|-----|
| M1 | 2 | 5 | 3 |
| M2 | 8 | 7 | 6 |
| M3 | 11 | 12 | 4 |

Para usar con Robot 2 (delantero), cambiar a: M1=8/7/6, M2=11/12/4, M3=2/5/3.

## Flujo del programa

```
[Encendido] → Inicialización BNO055 (~5 seg, no mover)
    ↓
[BOTÓN] → Test Heading (girar robot manualmente, verificar valores)
    ↓
[BOTÓN] → Preparar avance (posicionar robot)
    ↓
[BOTÓN] → AVANCE RÁPIDO 2 segundos
    ↓
[BOTÓN] → Repetir avance
```

## Qué observar

- **Fase RAMPA**: el robot arranca suave y acelera (primeros 400ms)
- **Fase CRUCE**: velocidad constante a 230 PWM (~1.6 seg)
- **Heading**: debería mantenerse cerca de 0° todo el tiempo
- **Heading final**: se muestra al terminar, idealmente < ±5°

## Documentación relacionada

- `test-gyro-movimiento-basico/` — Test a velocidad moderada (150 PWM)
- `docs/internal/lecciones-pid-movimiento.md` — Lecciones PID rectilíneo
- `docs/internal/cinematica-omnidireccional-movimientos.md` — Modelo cinemático
