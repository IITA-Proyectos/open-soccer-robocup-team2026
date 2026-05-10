---
id: TASK-006
title: "Cargar firmware oficial RCJ en la placa COMM"
date_created: 2026-05-10
assigned: [mariaviollaz, elias]
priority: P1
status: pending
estimated_hours: 3
blocks: [Hito 5 — integración COMM]
tags: [firmware, comm-board, rcj, arbitros, esp32]
---

# TASK-006 — Cargar firmware oficial RCJ en placa COMM

## Resumen

La placa COMM (copia 100% del módulo oficial RCJ con ESP32 + OLED + acelerómetro + 2 botones) necesita firmware. Bajar el firmware oficial de RoboCupJunior, compilarlo y cargarlo. Verificar funcionamiento básico (display QR, botones, conexión WiFi a árbitros).

## Contexto

El módulo de comunicación con árbitros es **bloqueante para Incheon** (regla L1 documentada en `docs/internal/limitaciones-robot-marzo-2026.md`). Sin él, el robot no es homologable.

Como la placa fabricada es copia 100% del módulo oficial (dimensiones, hardware), el firmware oficial debería funcionar **tal cual** sin modificaciones. Confirmar esto primero. Después se evalúa si agregamos ESP-NOW para inter-robot (objetivo Hito 5 / post-mundial).

## Pasos concretos

### 1. Conseguir el firmware oficial

1. Ir a https://github.com/robocup-junior/soccer-communication-module
2. Clonar o descargar el repo.
3. Leer el README — confirmar versión, dependencias, instrucciones de carga.
4. Identificar si el firmware se distribuye como `.bin` precompilado o como código fuente que hay que compilar.

### 2. Setup del entorno

5. Instalar la toolchain según indica el README (Arduino IDE con ESP32 support, o PlatformIO con ESP32 platform).
6. Instalar dependencias listadas (probablemente: librería OLED, librería acelerómetro específico, librería WiFi).
7. Conectar la placa COMM al PC por USB.

### 3. Cargar y probar

8. Compilar y subir el firmware a la ESP32 de la placa COMM.
9. Abrir Serial Monitor y verificar logs de inicio.
10. **Test 1 — Display OLED:** confirmar que aparece el código QR.
11. **Test 2 — Botones:** presionar cada botón, verificar que se registra en serial.
12. **Test 3 — Acelerómetro:** mover la placa, verificar lectura.
13. **Test 4 — WiFi:** confirmar que conecta a la red del árbitro (puede requerir un AP de prueba).

### 4. Documentar

14. Crear `journal/2026-MM-DD-firmware-comm-cargado.md` con resultados de los 4 tests.
15. Si algo no funciona, crear nueva task para investigar (puede ser bug de hardware o de firmware adaptable al hardware copiado).

### 5. Próximos pasos (futuro, NO en esta tarea)

- Evaluar si agregar ESP-NOW para inter-robot. Posibilidades:
  - (a) Modificar firmware oficial (riesgo: romper la certificación de homologación).
  - (b) App secundaria en el mismo ESP32 que se activa después del start/stop.
  - (c) Otro ESP32 separado solo para ESP-NOW.
- Esta decisión queda para Hito 5 / post-mundial.

## Criterio de cierre

- [ ] Firmware oficial RCJ cargado en placa COMM.
- [ ] Display OLED muestra QR.
- [ ] Botones responden.
- [ ] Acelerómetro lee.
- [ ] WiFi conecta a un AP de prueba (puede ser el celular).
- [ ] Journal entry creado con resultados.

## Notas / decisiones

_(actualizar cuando se ejecute)_

## Cambios de estado

- 2026-05-10: creado por Claude bajo requerimiento de Gustavo Viollaz.
