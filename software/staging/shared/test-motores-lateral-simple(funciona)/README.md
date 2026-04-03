---
title: "Test: Movimiento lateral omnidireccional con giroscopio, PID y compensación de frenado"
date: 2026-04-02
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: true
ai-tool: "Claude (Anthropic)"
requested-by: "María (IITA)"
status: final
tags: [movimiento, lateral, omnidireccional, pid, giroscopo, bno055, test, frenado, auto-calibracion]
robot: delantero
---

# Test: Movimiento lateral omnidireccional con giroscopio y PID

## Descripción general

Programa de prueba para el movimiento lateral (tipo "cangrejo") del Robot 2 (delantero) usando cinemática omnidireccional de 3 ruedas, con corrección de heading mediante giroscopio BNO055 y control PID. Incluye un sistema de compensación dinámica de frenado para la rueda trasera (M3) y auto-calibración basada en el giroscopio.

**Archivo**: `test-motores-lateral-simple.ino`  
**Versión actual**: v4

## ¿Para qué sirve?

Este test valida el movimiento lateral puro, que es esencial para las maniobras de posicionamiento del delantero (orbitar la pelota, esquivar oponentes, realinearse). El robot ejecuta un loop: se mueve a la derecha 3 segundos, pausa, se mueve a la izquierda 3 segundos, pausa, y repite.

A diferencia del movimiento adelante/atrás (que solo usa M1 y M2), el movimiento lateral requiere la participación de los **3 motores** con velocidades y direcciones específicas derivadas de la cinemática omnidireccional.

## Hardware requerido

- Robot 2 (delantero) con placa Zircon
- Teensy con BNO055 en dirección I2C `0x28`
- 3 motores conectados: M1 (frontal izq), M2 (frontal der), M3 (trasero)

## Cinemática del movimiento lateral

Para desplazar el robot lateralmente sin girar:

| Motor | Rol | Velocidad base | Dirección (derecha) |
|-------|-----|---------------|---------------------|
| M1 (frontal) | Empuja en diagonal | 55 | -1 |
| M2 (frontal) | Empuja en diagonal (opuesto) | 55 | +1 |
| M3 (trasero) | Empuje lateral principal | 100 | +1 |

M3 va a casi el doble de velocidad que M1/M2 porque es la rueda que más contribuye al desplazamiento lateral en la configuración omnidireccional de 3 ruedas a 120°.

Para ir a la **izquierda**, se invierten todas las direcciones.

## Estructura del programa

### Velocidad de cada motor

Cada motor recibe la suma de dos componentes:

```
velocidad_total = componente_lateral + componente_rotación_PID
```

- **Componente lateral**: el movimiento deseado (calibrado manualmente).
- **Componente rotación**: la corrección del PID para mantener el heading en 0°. Los 3 motores participan en la corrección.

### Saturación proporcional

Si algún motor supera 255, se escalan **todos** proporcionalmente (no se clampea individualmente). Esto mantiene la dirección correcta del movimiento.

### Compensación de frenado M3 (v3-v4)

**Problema**: M3 va más rápido (100 vs 55) y tiene más inercia, así que al frenar tarda más en detenerse que M1/M2, causando una desviación al final de cada movimiento.

**Solución**: M3 frena `anticipacionReal` milisegundos antes que M1/M2.

La anticipación se calcula dinámicamente:

```
anticipacionReal = BASE_ANTICIPACION_MS × VEL_M3 / 100
```

Si se cambia `VEL_M3`, la anticipación se ajusta automáticamente sin necesidad de recalibrar.

### Auto-calibración con giroscopio (v4)

Después de cada frenado, el sistema:

1. Espera 100ms para que se disipe la inercia residual.
2. Mide el heading final y lo compara con el heading al momento de frenar.
3. Si la diferencia (drift) supera `UMBRAL_DRIFT` (1.5°), ajusta `BASE_ANTICIPACION_MS` en ±5ms.
4. Si M3 frenó tarde (drift en dirección del movimiento) → sube la anticipación.
5. Si M3 frenó de más (drift contrario al movimiento) → baja la anticipación.

Esto hace que el robot se auto-ajuste en las primeras pasadas y luego se estabilice.

## Comandos Serial en tiempo real (v4)

El programa acepta comandos por Serial Monitor mientras corre:

| Comando | Alternativa | Acción |
|---------|-------------|--------|
| `+` | `a` | Sube anticipación base +10ms |
| `-` | `z` | Baja anticipación base -10ms |
| `c` | — | Activa/desactiva auto-calibración |
| `v` | — | Muestra todos los valores actuales |

Esto permite calibrar en la cancha sin recompilar.

## Parámetros ajustables

### Velocidades y direcciones

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| `VEL_M1` | 55 | Velocidad M1 frontal |
| `VEL_M2` | 55 | Velocidad M2 frontal |
| `VEL_M3` | 100 | Velocidad M3 trasero (principal) |
| `DIR_M1` | -1 | Dirección M1 para ir a la derecha |
| `DIR_M2` | +1 | Dirección M2 para ir a la derecha |
| `DIR_M3` | +1 | Dirección M3 para ir a la derecha |

### PID

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| `Kp` | 3.0 | Ganancia proporcional |
| `Ki` | 0.05 | Ganancia integral |
| `Kd` | 0.5 | Ganancia derivativa |
| `FACTOR_ROTACION` | 0.5 | Peso de la corrección PID vs movimiento lateral |
| `MAX_CORRECCION` | 50 | Límite máximo de corrección PID |

### Compensación de frenado

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| `BASE_ANTICIPACION_MS` | 60.0 | Anticipación base en ms (para VEL_M3=100) |
| `UMBRAL_DRIFT` | 1.5° | Umbral de desviación para auto-calibrar |
| `PASO_AUTOCAL` | 5.0 ms | Paso de ajuste en cada auto-calibración |
| `MAX_ANTICIPACION` | 300 ms | Límite máximo de seguridad |

### Tiempos

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| `TIEMPO_MOVIMIENTO` | 3000 ms | Duración de cada tramo lateral |
| `TIEMPO_PAUSA` | 500 ms | Pausa entre cambio de dirección |

## Instrucciones de uso

1. Subir al Teensy del Robot 2
2. Abrir Serial Monitor a **19200 baud**
3. **NO MOVER** el robot durante la calibración del giroscopio (~5 seg)
4. El movimiento arranca automáticamente 2 segundos después de calibrar
5. Observar el Serial para ver heading, corrección PID, velocidades de cada motor, y estado del frenado

## Qué observar durante el test

- **Movimiento lateral puro**: el robot debería desplazarse de costado sin girar.
- **Heading cerca de 0°**: los valores `H:` en el Serial deberían mantenerse bajos.
- **Tag `[FRENO]`**: aparece cuando M3 entra en fase de pre-frenado.
- **Mensajes `[AUTOCAL]`**: muestran el drift medido y el ajuste aplicado. Después de varias pasadas deberían decir "OK (dentro de umbral)".
- **Al final del movimiento**: no debería haber desviación rotacional (ese era el problema original que este programa soluciona).

## Historial de versiones

| Versión | Fecha | Autor | Cambios principales |
|---------|-------|-------|---------------------|
| v1 | 2026-03-27 | María | Versión inicial con velocidades calibradas |
| v2 | 2026-03-27 | Claude (supervisión Gustavo) | Fix PID doble, saturación proporcional, 3 motores en corrección |
| v3 | 2026-03-27 | Claude (supervisión María) | Pre-frenado M3 con anticipación fija |
| v4 | 2026-03-27 | Claude (supervisión María) | Anticipación dinámica, auto-calibración con giroscopio, comandos Serial |

## Convención importante

Este programa **no usa zirconLib** — los pines de motores están definidos directamente con `#define`. Los valores de pines corresponden al Robot 2 (delantero).

## Relación con otros archivos

- `test-gyro-movimiento-basico/` — Test complementario para movimiento adelante/atrás
- `docs/internal/lecciones-pid-movimiento-lateral.md` — Lecciones aprendidas del PID en movimiento lateral (bugs v1→v2)
- `docs/internal/cinematica-omnidireccional-movimientos.md` — Modelo cinemático de 3 ruedas
- `docs/internal/giroscopo-bno055-analisis-tecnico.md` — Análisis técnico del BNO055
