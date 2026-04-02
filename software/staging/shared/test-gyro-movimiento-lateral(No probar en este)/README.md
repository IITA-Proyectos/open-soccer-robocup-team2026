---
title: "Test: Movimiento lateral con data logging CSV"
date: 2026-04-02
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: true
ai-tool: "Claude (Anthropic)"
requested-by: "María (IITA)"
status: final
tags: [test, movimiento, lateral, data-logging, csv, giroscopo, bno055, acelerometro]
robot: delantero
---

# Test: Movimiento lateral con data logging CSV

## Descripción general

Programa de prueba para movimiento lateral con **registro de datos** (data logging). Mientras el robot se mueve lateralmente, guarda muestras de heading, aceleración 3D y velocidades/direcciones de los 3 motores cada 50ms. Al presionar el botón durante una pausa, vuelca todos los datos por Serial en formato CSV para copiar y analizar en una planilla o script.

**Archivo**: `test-gyro-movimiento-lateral.ino`

## Diferencia con test-motores-lateral-simple

| Aspecto | Este test (data logging) | test-motores-lateral-simple |
|---------|-------------------------|----------------------------|
| **Propósito** | Capturar datos para análisis | Calibrar movimiento en vivo |
| **Data logging** | Sí (CSV por Serial) | No |
| **Acelerómetro** | Sí (registra accel X/Y/Z) | No |
| **Cinemática** | Factores configurables M1/M2/M3 | Velocidades absolutas calibradas |
| **Compensación frenado M3** | No | Sí (dinámica con auto-calibración) |
| **Comandos Serial** | No | Sí (+, -, c, v) |
| **Inicio** | Por botón (pin 9), con test heading previo | Automático |
| **Capacidad logging** | 600 muestras (~30 seg a 50ms) | — |

## Hardware requerido

- Teensy Robot 2 (delantero) con BNO055 en I2C `0x28`
- 3 motores con drivers H-bridge
- Botón en pin 9

## Flujo del programa

```
[Encendido] → Inicialización BNO055
    ↓
[BOTÓN] → Test Heading (ver heading + aceleración en vivo)
    ↓
[BOTÓN] → Inicia loop lateral + logging:
              DERECHA 3s → Pausa → IZQUIERDA 3s → Pausa → repetir
              (en cualquier pausa: BOTÓN = volcar datos CSV)
    ↓
[BOTÓN durante pausa] → Volcado CSV por Serial
    ↓
[BOTÓN] → Reiniciar test
```

## Formato de datos CSV

Al presionar el botón durante una pausa, el programa vuelca por Serial:

```csv
timestamp_ms,heading_deg,accel_x,accel_y,accel_z,vel_m1,vel_m2,vel_m3,dir_m1,dir_m2,dir_m3,estado
0,0.12,0.05,-0.10,9.81,50,50,100,-1,-1,1,D
50,0.15,0.08,-0.12,9.80,52,48,100,-1,-1,1,D
...
```

Donde `estado` es: `D` = derecha, `I` = izquierda, `P` = pausa.

## Parámetros ajustables

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| `VELOCIDAD_LATERAL` | 100 | Velocidad base (0-255) |
| `FACTOR_M1_LATERAL` | 0.5 | Factor velocidad M1 |
| `FACTOR_M2_LATERAL` | 0.5 | Factor velocidad M2 |
| `FACTOR_M3_LATERAL` | 1.0 | Factor velocidad M3 (rueda principal) |
| `TIEMPO_MOVIMIENTO` | 3000 ms | Duración cada dirección |
| `Kp` | 4.0 | PID proporcional |
| `Ki` | 0.1 | PID integral |
| `Kd` | 0.8 | PID derivativo |
| `MAX_SAMPLES` | 600 | Máximo de muestras (~30 seg) |
| `SAMPLE_INTERVAL` | 50 ms | Intervalo de muestreo |

## Notas importantes

- Los datos se guardan en un array en RAM (`DataSample dataLog[600]`), así que hay un límite de ~30 segundos de registro.
- Copiar los datos CSV del Serial Monitor rápidamente, ya que se pierden al apagar.
- Si el BNO055 falla, el programa se detiene (a diferencia de test-motores-lateral-simple que continúa sin corrección).

## Documentación relacionada

- `test-motores-lateral-simple/` — Test lateral definitivo con calibración dinámica (sin data logging)
- `docs/internal/cinematica-omnidireccional-movimientos.md` — Modelo cinemático
- `docs/internal/lecciones-pid-movimiento-lateral.md` — Lecciones PID lateral
