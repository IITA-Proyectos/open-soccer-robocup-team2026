> ⚠️ **STAGING CONGELADO (2026-06-03) — NO subir más material a `software/staging/`.**
> Antes de tocar o agregar algo, leé **`software/staging/up_board/00-LEER-PRIMERO-recomendaciones-reuso.md`**.
> Este scratch repite bugs ya resueltos. Usá el stack de PRODUCCIÓN testeado en
> `software/teensy/Soccer 2026/src/` (ver el "mapa de reúso" del documento).

---
title: "Test: Giroscopio y movimiento básico adelante/atrás con PID"
date: 2026-03-27
author: "Claude (Anthropic - Claude Opus 4.6) bajo supervisión de María"
ai-assisted: true
ai-tool: "Claude (Anthropic)"
status: final
tags: [test, movimiento, adelante, atras, pid, giroscopo, bno055, motores, basico]
robot: robot-2-delantero
---

# Test: Giroscopio y movimiento básico adelante/atrás con PID

Programa de prueba para verificar la **inicialización del giroscopio BNO055** y validar el **movimiento rectilíneo adelante/atrás** del Robot 2 (delantero), con corrección PID para mantener la trayectoria en línea recta.

**Archivo:** `software/staging/shared/test-gyro-movimiento-basico/test-gyro-movimiento-basico.ino`

---

## Qué hace este programa

El programa tiene dos fases controladas con un botón físico (pin 9):

1. **Test de heading:** el robot está detenido y muestra en Serial el heading del giroscopio en tiempo real. Sirve para verificar que el BNO055 está funcionando correctamente girando el robot manualmente y observando los valores.

2. **Loop infinito adelante/atrás:** el robot se mueve hacia adelante durante 5 segundos, hace una pausa de 0.5 segundos, retrocede 5 segundos, pausa, y repite indefinidamente. Durante el movimiento, el PID corrige la orientación usando los motores frontales (M1 y M2) para que el robot vaya derecho.

El programa incluye un contador de ciclos visible en Serial para facilitar el seguimiento de las pruebas.

---

## Diferencia con el test lateral

| Aspecto | Este test (básico) | Test lateral |
|---------|-------------------|--------------|
| **Movimiento** | Adelante/atrás (rectilíneo) | Derecha/izquierda (lateral) |
| **Motores activos** | M1 y M2 (frontales), M3 apagado | Los 3 motores |
| **Corrección PID** | Diferencial entre M1 y M2 | Los 3 motores participan |
| **Cinemática** | Simple (diferencial) | Omnidireccional completa |
| **Compensación inercia** | No necesaria | Sí (M3 pre-frenado dinámico) |
| **Comandos Serial** | No | Sí (+, -, c, v) |
| **Inicio** | Por botón (pin 9) | Automático (2 seg delay) |

---

## Hardware requerido

- Teensy (Robot 2 - delantero) con placa Zircon
- Giroscopio BNO055 conectado por I2C (dirección 0x28)
- 3 motores con drivers H-bridge (pines definidos en el código)
- Botón conectado al pin 9 (activo HIGH)
- Monitor Serial a 19200 baud

---

## Flujo del programa

```
[Encendido]
    ↓
[Inicialización BNO055] — 5 seg máximo, no mover el robot
    ↓
[BOTÓN] → Test Heading (girar robot manualmente, ver valores)
    ↓
[BOTÓN] → Esperando inicio del loop
    ↓
[BOTÓN] → Loop infinito:
              ADELANTE 5s → Pausa 0.5s → ATRÁS 5s → Pausa 0.5s → repetir
```

Si el BNO055 falla en la inicialización, el programa se detiene con parpadeo rápido del LED.

---

## Parámetros ajustables

| Parámetro | Valor actual | Descripción |
|-----------|-------------|-------------|
| `VELOCIDAD_BASE` | 100 | Velocidad de los motores (0-255) |
| `TIEMPO_MOVIMIENTO` | 5000 | Duración de cada movimiento en ms |
| `PAUSA_ENTRE_MOVIMIENTOS` | 500 | Pausa entre adelante y atrás en ms |
| `Kp` | 3.0 | Ganancia proporcional del PID |
| `Ki` | 0.05 | Ganancia integral del PID |
| `Kd` | 0.5 | Ganancia derivativa del PID |

---

## Corrección PID en movimiento rectilíneo

El PID calcula una corrección basada en el error de heading (diferencia entre la orientación actual y la orientación cero). Esta corrección se aplica de forma diferencial a M1 y M2:

- **Adelante:** M1 recibe `velocidad + corrección`, M2 recibe `velocidad - corrección`
- **Atrás:** la corrección se **invierte** (`M1 = velocidad - corrección`, `M2 = velocidad + corrección`) porque el robot se mueve en dirección opuesta

Esta inversión es importante: si no se invierte, el PID corrige para el lado equivocado al ir marcha atrás.

Motor 3 permanece apagado durante todo el test, ya que no contribuye al movimiento adelante/atrás en esta configuración omnidireccional.

---

## Indicadores LED

| Estado del LED | Significado |
|----------------|-------------|
| Fijo encendido | Moviéndose adelante |
| Apagado | Moviéndose atrás |
| Parpadeo rápido | En pausa entre movimientos |
| Parpadeo durante heading test | Heading desviado más de 30° |

---

## Salida Serial de ejemplo

```
****************************************************
*  TEST: GIROSCOPO + MOVIMIENTO CON PID (LOOP)     *
*  Robot: ROBOT 2 (delantero)                      *
****************************************************

=== INICIALIZANDO GIROSCOPO BNO055 ===
1. Detectando BNO055... OK!
2. Configurando cristal externo... OK!
3. Esperando estabilización (1 segundo)... OK!
4. Calibrando giróscopo (NO MOVER!)...
   Calibración: SYS=0 GYR=3 ACC=0 MAG=0
   -> Giróscopo calibrado en 250ms!
5. Estableciendo posición cero...
   -> Offset establecido: 127.3

>>> CICLO 1: ADELANTE (5 seg) <<<
  [ADELANTE] H:+0.2° | t=0.2s
  [ADELANTE] H:+0.1° | t=0.4s
  ...
  -> Pausa...
>>> CICLO 1: ATRAS (5 seg) <<<
  [ATRAS]    H:-0.3° | t=0.2s
```

---

## Máquina de estados

El programa utiliza una máquina de estados (`enum EstadoTest`) con las siguientes transiciones:

| Estado | Descripción | Transición |
|--------|-------------|------------|
| `ESPERANDO_INICIO` | Esperando botón para comenzar | Botón → `TEST_HEADING` |
| `TEST_HEADING` | Muestra heading en tiempo real | Botón → `ESPERANDO_LOOP` |
| `ESPERANDO_LOOP` | Esperando botón para iniciar loop | Botón → `LOOP_ADELANTE` |
| `LOOP_ADELANTE` | Moviéndose adelante con PID | Timeout → `LOOP_PAUSA_1` |
| `LOOP_PAUSA_1` | Pausa entre adelante y atrás | Timeout → `LOOP_ATRAS` |
| `LOOP_ATRAS` | Moviéndose atrás con PID | Timeout → `LOOP_PAUSA_2` |
| `LOOP_PAUSA_2` | Pausa entre atrás y adelante | Timeout → `LOOP_ADELANTE` |

El manejo del botón usa detección de flanco de subida con `esperarSoltarBoton()` para evitar lecturas múltiples.

---

## Notas importantes

- **No mover el robot** durante la inicialización del BNO055 (primeros ~5 segundos). El giroscopio necesita estar quieto para calibrar correctamente.
- Si el BNO055 no se detecta en 3 segundos, el programa se **detiene completamente** (a diferencia del test lateral que continúa sin corrección).
- El programa **no usa zirconLib**, los pines están definidos directamente para simplificar.
- Se usa `bno.begin()` sin parámetros por compatibilidad con el hardware del equipo.

---

## Documentación relacionada

- `docs/internal/lecciones-pid-movimiento.md` — Lecciones sobre PID en movimiento rectilíneo
- `docs/internal/giroscopo-bno055-analisis-tecnico.md` — Análisis técnico del BNO055
- `software/staging/shared/test-motores-lateral-simple/` — Test complementario de movimiento lateral
