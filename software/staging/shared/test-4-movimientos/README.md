---
title: "Test: 4 movimientos omnidireccionales (adelante/atrás/derecha/izquierda)"
date: 2026-04-02
author: "Claude (Anthropic - Claude Opus 4.6)"
requested-by: "María Virginia Viollaz (@mariaviollaz)"
ai-assisted: true
ai-tool: "Claude (Anthropic - Claude Opus 4.6)"
status: staging
tags: [test, movimiento, adelante, atras, lateral, omnidireccional, pid, giroscopo, bno055]
robot: robot-2-delantero
area: movilidad
tipo: protocolo
---

# Test: 4 movimientos omnidireccionales con PID

## Descripción general

Programa que combina los 4 movimientos básicos del robot en un solo test: adelante, atrás, derecha e izquierda. Ejecuta un loop infinito de 3 segundos por dirección con pausas de 0.5 segundos entre cada cambio.

**Archivo**: `test-4-movimientos.ino`

## Origen del código

Este programa es la unión directa de dos tests existentes sin modificar valores:

| Movimiento | Código fuente | PID usado |
|------------|--------------|-----------|
| Adelante / Atrás | `test-gyro-movimiento-basico.ino` | Kp=3.0, Ki=0.08, Kd=0.8, MAX_CORR=80 |
| Derecha / Izquierda | `test-motores-lateral-simple.ino` v4 | Kp=3.0, Ki=0.05, Kd=0.5, MAX_CORR=50 |

El programa cambia automáticamente entre los dos juegos de parámetros PID al transicionar entre movimiento rectilíneo y lateral.

## Hardware requerido

- Teensy Robot 2 (delantero) con placa Zircon
- Giroscopio BNO055 conectado por I2C (dirección 0x28)
- 3 motores con drivers H-bridge
- Botón conectado al pin 9 (activo HIGH)
- Monitor Serial a 19200 baud

## Secuencia del loop

```
[Encendido] → Inicialización BNO055 (~5 seg, no mover)
    ↓
[BOTÓN] → Test Heading (girar robot manualmente, verificar valores)
    ↓
[BOTÓN] → Loop infinito:
    ADELANTE 3s (PID básico, M1+M2)
        → Pausa 0.5s
    ATRÁS 3s (PID básico invertido, M1+M2)
        → Pausa 0.5s
    DERECHA 3s (PID lateral, M1+M2+M3, con anticipación frenado M3)
        → Pausa 0.5s + auto-calibración frenado
    IZQUIERDA 3s (PID lateral, M1+M2+M3, con anticipación frenado M3)
        → Pausa 0.5s + auto-calibración frenado
    → repetir
```

## Parámetros (sin cambios respecto a los programas originales)

### Movimiento adelante/atrás

| Parámetro | Valor | Origen |
|-----------|-------|--------|
| VELOCIDAD_BASE | 150 | test-gyro-movimiento-basico |
| Kp | 3.0 | test-gyro-movimiento-basico |
| Ki | 0.08 | test-gyro-movimiento-basico |
| Kd | 0.8 | test-gyro-movimiento-basico |
| MAX_CORRECCION | 80 | test-gyro-movimiento-basico |

### Movimiento lateral

| Parámetro | Valor | Origen |
|-----------|-------|--------|
| VEL_M1 / VEL_M2 | 55 / 55 | test-motores-lateral-simple |
| VEL_M3 | 100 | test-motores-lateral-simple |
| DIR_M1 / DIR_M2 / DIR_M3 | -1 / +1 / +1 | Calibrado por María |
| Kp | 3.0 | test-motores-lateral-simple |
| Ki | 0.05 | test-motores-lateral-simple |
| Kd | 0.5 | test-motores-lateral-simple |
| MAX_CORRECCION | 50 | test-motores-lateral-simple |
| FACTOR_ROTACION | 0.5 | test-motores-lateral-simple |
| BASE_ANTICIPACION_MS | 60 | test-motores-lateral-simple v4 |

## Comandos Serial (en tiempo real)

| Comando | Acción |
|---------|--------|
| `+` o `a` | Sube anticipación frenado M3 +10ms |
| `-` o `z` | Baja anticipación frenado M3 -10ms |
| `c` | Activa/desactiva auto-calibración de frenado |
| `v` | Muestra todos los valores actuales |

## Qué observar durante el test

- **Adelante/Atrás**: el robot debería ir en línea recta, corrigiendo con M1 y M2. M3 queda apagado.
- **Derecha/Izquierda**: los 3 motores participan. M3 es el motor principal del movimiento lateral.
- **Heading**: en las 4 direcciones debería mantenerse cerca de 0°.
- **Transiciones**: al cambiar de modo (rectilíneo → lateral), el PID se resetea y cambian los parámetros automáticamente.
- **Tags `[FRENO]`**: aparecen cuando M3 entra en fase de pre-frenado antes de parar.
- **Mensajes `[AUTOCAL]`**: muestran el drift medido después del frenado lateral.

## Notas importantes

- **No usa zirconLib**, los pines están definidos directamente.
- Usa `bno.begin()` sin parámetros (modo NDOF por defecto, por compatibilidad).
- Los valores de PID y velocidades son exactamente los de los programas originales.
- Lo único cambiado respecto a los originales es el tiempo de movimiento (3s para todos, el básico tenía 5s).

## Documentación relacionada

- `test-gyro-movimiento-basico/` — Test original de adelante/atrás
- `test-motores-lateral-simple/` — Test original de movimiento lateral v4
- `docs/internal/lecciones-pid-movimiento.md` — Lecciones PID rectilíneo
- `docs/internal/lecciones-pid-movimiento-lateral.md` — Lecciones PID lateral
- `docs/internal/cinematica-omnidireccional-movimientos.md` — Modelo cinemático
