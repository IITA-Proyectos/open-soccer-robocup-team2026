> ⚠️ **STAGING CONGELADO (2026-06-03) — NO subir más material a `software/staging/`.**
> Antes de tocar o agregar algo, leé **`software/staging/up_board/00-LEER-PRIMERO-recomendaciones-reuso.md`**.
> Este scratch repite bugs ya resueltos. Usá el stack de PRODUCCIÓN testeado en
> `software/teensy/Soccer 2026/src/` (ver el "mapa de reúso" del documento).

---
title: "Test: BNO055 en modo IMUPLUS (standalone)"
date: 2026-04-02
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: true
ai-tool: "Claude (Anthropic)"
requested-by: "María (IITA)"
status: final
tags: [test, giroscopo, bno055, imuplus, sensor, standalone]
robot: ambos
---

# Test: BNO055 en modo IMUPLUS (standalone)

## Descripción general

Programa de verificación standalone del giroscopio BNO055 en modo IMUPLUS (sin magnetómetro). No mueve motores — solo lee y muestra el heading por Serial. Es el primer test que se debe correr antes de cualquier otro test de movimiento.

**Archivo**: `test-bno055-imuplus.ino`

## ¿Para qué sirve?

Valida que el BNO055 funciona correctamente antes de integrarlo al programa completo del robot. Específicamente verifica que el heading arranca en 0° (no en un valor aleatorio), que responde coherentemente al girar el robot, y permite medir el drift dejándolo quieto varios minutos.

## Hardware requerido

- Teensy (cualquiera de los dos robots) con BNO055 en I2C `0x28`
- No requiere motores ni botón

## Qué hace

1. Inicializa el BNO055 en modo `OPERATION_MODE_IMUPLUS` (diferencia clave con otros tests que usan `bno.begin()` sin parámetros)
2. Calibra el giroscopio (~3 seg, no mover el robot)
3. Captura el heading inicial como offset (promedio de 10 lecturas)
4. Entra en loop infinito mostrando heading a 10 Hz

## Qué observar

- El heading debería arrancar en ~0° (no en 35° como ocurría antes del fix)
- Al girar 90° a la derecha → debería mostrar ~-90°
- Al girar 90° a la izquierda → debería mostrar ~+90°
- **Test de drift**: dejar 5 minutos quieto y anotar cuánta desviación acumula

## Indicadores LED

| Estado LED | Significado |
|------------|-------------|
| Parpadeo rápido al inicio | Inicializando |
| Parpadeo muy rápido sin parar | ERROR: BNO055 no detectado |
| Fijo encendido | Funcionando, heading < 30° |
| Parpadeo lento | Heading desviado > 30° |

## Parámetros

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| Baud rate | 19200 | Velocidad Serial |
| Modo BNO055 | IMUPLUS | Sin magnetómetro |
| Frecuencia lectura | 10 Hz | 100ms entre lecturas |

## Notas importantes

- Si el BNO055 no se detecta en 3 segundos, el programa se **detiene permanentemente** con parpadeo rápido del LED.
- Usa `bno.begin(OPERATION_MODE_IMUPLUS)` en vez de `bno.begin()` sin parámetros.
- Compatible con ambos robots (no depende de pines de motores).

## Documentación relacionada

- `docs/internal/giroscopo-bno055-analisis-tecnico.md` — Análisis técnico del BNO055
- `shared/evaluar-bohlebots-bno055.md` — Evaluación de librería alternativa
- `shared/cambios-bno055-init.md` — Parches de inicialización
