---
title: "Roadmap de Mejoras — Temporada 2026"
date: 2026-03-20
author: "Gustavo Viollaz + Claude (Anthropic)"
ai-assisted: true
status: final
tags: [roadmap, mejoras, hardware, software, prioridades]
---

# Roadmap de Mejoras — Temporada 2026

## Visión general

Este documento organiza TODAS las mejoras identificadas en 4 horizontes temporales, con prioridad, estado y dependencias. Cada item tiene un ID único para seguimiento.

---

## Horizonte 1: CRÍTICO para Mundial Incheon 2026 (abril-junio)

Estas tareas deben completarse ANTES de viajar al mundial.

### HW-001: Módulo de comunicación con árbitros
- **Tipo**: Hardware + Software
- **Prioridad**: BLOQUEANTE
- **Descripción**: Implementar el módulo oficial ESP32 + pantalla QR para start/stop por árbitros.
- **Hardware**: ESP32, pantalla OLED/TFT, conector al Teensy
- **Software**: Firmware ESP32 (protocolo oficial), integración serial con programa principal
- **Dependencias**: Obtener especificación oficial del protocolo
- **Estado**: No iniciado
- **Responsable**: Por asignar

### SW-001: Giróscopo IMUPLUS + init mejorado
- **Tipo**: Software
- **Prioridad**: Alta
- **Descripción**: Cambiar BNO055 de NDOF a IMUPLUS, init con calibración, promedio de lecturas
- **Estado**: En staging, pendiente prueba viernes 28/3
- **Referencia**: `docs/internal/giroscopo-bno055-analisis-tecnico.md`

### SW-002: Fix UART sincronización
- **Tipo**: Software
- **Prioridad**: Alta
- **Descripción**: Lectura UART robusta con while-peek, validación de 3 headers
- **Estado**: En staging, pendiente prueba viernes 28/3
- **Referencia**: `staging/shared/cambios-uart-sincronizacion.md`

### SW-003: Fix rampa de pateo
- **Tipo**: Software
- **Prioridad**: Alta
- **Descripción**: Reset velocidadActualPateo en transiciones de pateo
- **Estado**: En staging, pendiente prueba viernes 28/3

### SW-004: Fix currentYaw en arquero
- **Tipo**: Software
- **Prioridad**: Alta
- **Descripción**: Reemplazar comparaciones con currentYaw raw por error normalizado
- **Estado**: En staging, pendiente prueba viernes 28/3

### SW-005: Primitivas de movimiento omnidireccional
- **Tipo**: Software
- **Prioridad**: Alta
- **Descripción**: Función moverRobot(velocidad, dirección, headingObjetivo) con PD
- **Estado**: En staging, pendiente prueba viernes 28/3
- **Referencia**: `docs/internal/cinematica-omnidireccional-movimientos.md`

### SW-006: Refactoring modular del código
- **Tipo**: Software
- **Prioridad**: Media
- **Descripción**: Separar código en módulos: config.h, motores.h, sensores.h, comunicacion.h
- **Dependencias**: SW-001 a SW-005 probados primero
- **Estado**: Propuesto, no iniciado

---

## Horizonte 2: DESEABLE para Mundial (si hay tiempo)

### HW-002: Sensores TOF frontales para detección de oponentes
- **Tipo**: Hardware + Software
- **Prioridad**: Alta
- **Descripción**: 2-3 sensores VL53L0X/VL53L1X en la parte frontal para detectar oponentes
- **Hardware**: Sensores TOF (I2C, ~$3 c/u), soportes impresos 3D
- **Software**: Lectura periódica, lógica de evasión en máquina de estados
- **Dependencias**: Verificar pines I2C disponibles (multiplexor si es necesario)
- **Estado**: No iniciado

### HW-003: Lente wide-angle para OpenMV
- **Tipo**: Hardware
- **Prioridad**: Media
- **Descripción**: Reemplazar lente estándar (~70°) por wide-angle (~140°)
- **Impacto**: Duplica el campo de visión sin cambiar hardware ni software (solo recalibrar thresholds)
- **Estado**: Pendiente compra

### SW-007: Estimación de posición por cámara
- **Tipo**: Software
- **Prioridad**: Media
- **Descripción**: Usar tamaño y posición de arcos detectados para estimar distancia y lateralidad
- **Referencia**: Si veo el arco amarillo grande = estoy cerca. Si lo veo a la izquierda del frame = estoy desplazado a la derecha.
- **Dependencias**: Necesita calibración específica por lente
- **Estado**: No iniciado

### SW-008: Recalibración de heading por visión
- **Tipo**: Software
- **Prioridad**: Media
- **Descripción**: Cuando el robot ve un arco centrado, recalibrar el offset del giróscopo para compensar drift
- **Referencia**: `docs/internal/giroscopo-bno055-analisis-tecnico.md`, sección 4.2
- **Estado**: No iniciado

### HW-004: Giróscopo dual (redundancia)
- **Tipo**: Hardware + Software
- **Prioridad**: Baja
- **Descripción**: Colocar un segundo BNO055 en la capa superior del robot (lejos de motores). Comparar lecturas de ambos y usar el promedio o descartar el que presente saltos por impacto.
- **Análisis**: Otros equipos top de Soccer Open usan esta técnica. El segundo sensor podría ir en NDOF (lejos de motores, menor interferencia) mientras el principal va en IMUPLUS. Si el de NDOF calibra bien, puede servir como referencia absoluta periódica.
- **Dependencias**: Segundo sensor BNO055, dirección I2C alternativa (0x29 con pin ADR en alto)
- **Estado**: Idea, no investigado

### HW-005: Encoders magnéticos en motores actuales
- **Tipo**: Hardware
- **Prioridad**: Baja (para mundial)
- **Descripción**: Evaluar colocación de encoders magnéticos tipo AS5600 en los motores TT actuales.
- **Desafíos**: Espacio físico en el eje del motor, pines disponibles en Teensy (3 encoders = 6 pines de interrupción o 3 I2C adicionales), interferencia magnética con BNO055.
- **Estado**: Idea, requiere análisis mecánico

---

## Horizonte 3: OBJETIVO Roboliga 2026 (post-mundial)

Estas mejoras requieren rediseño significativo. Se planifican para después del mundial.

### HW-010: Motores brushless con encoders
- **Tipo**: Hardware
- **Prioridad**: Alta (para Roboliga)
- **Descripción**: Reemplazar motores TT 5V por motores brushless con encoders integrados y reductores.
- **Ventajas**: Más potencia, más durabilidad, control preciso de velocidad por motor
- **Desafíos**: Drivers ESC, mayor consumo, posible rediseño mecánico, verificar si requieren reductores externos
- **Estado**: Investigación pendiente

### HW-011: Sistema de pateo completo (kicker + dribbler)
- **Tipo**: Hardware + Software
- **Prioridad**: Alta (para Roboliga)
- **Componentes**:
  - Kicker: solenoide/electroímán + circuito capacitor (boost converter 7.4V→30-50V) + MOSFET + servo para dirección
  - Dribbler: motor pequeño con rodillo en la parte frontal inferior
- **Desafíos**: Espacio mecánico, peso, alimentación del capacitor, seguridad eléctrica
- **Estado**: Concepto, requiere diseño mecánico y eléctrico

### HW-012: Placa de sensores de línea en anillo
- **Tipo**: Hardware
- **Prioridad**: Media
- **Descripción**: PCB propietaria con 16-48 sensores de luz distribuidos en anillo en la base. Procesador local (Teensy 4.0 o ESP32) que calcula posición angular de la línea y envía dato procesado al controlador principal.
- **Ventajas**: Detección de línea desde cualquier ángulo, seguimiento de línea para arquero, prescindencia de línea individual
- **Estado**: Concepto, requiere diseño de PCB

### HW-013: Placa TOF / LIDAR para posicionamiento
- **Tipo**: Hardware + Software
- **Prioridad**: Media
- **Descripción**: Placa superior con 8-16 sensores TOF (VL53L0X/VL53L1X) apuntando radialmente. Con las distancias a las paredes/rejas de la cancha, se puede triangular la posición (x, y) del robot.
- **Hardware**: Sensores TOF, multiplexor I2C (TCA9548A), Teensy 4.0 o ESP32 como procesador local
- **Software**: Algoritmo de triangulación, filtro de mediciones, comunicación con controlador principal
- **Análisis**: Equipos top reportan precisión de ~5-10cm con 8 TOF. Algunos prescinden completamente de sensores de línea con esta información. La combinación TOF + línea + cámara daría posicionamiento muy robusto.
- **Estado**: Concepto, requiere investigación y prototipo

### HW-014: Multi-cámara (3x OpenMV con wide-angle)
- **Tipo**: Hardware + Software
- **Prioridad**: Media
- **Descripción**: 3 cámaras OpenMV con lente wide-angle (140°) para cobertura 360°
- **Alternativa**: Cámara omnidireccional con espejo cónico (una sola cámara mirando arriba)
- **Desafíos**: Costo (3x OpenMV ~$210), procesamiento, ancho de banda UART (3 canales), sincronización
- **Estado**: Concepto

---

## Horizonte 4: VISIÓN LARGO PLAZO (2027+)

### HW-020: Procesador de alto rendimiento para visión
- **Tipo**: Hardware + Software
- **Prioridad**: Baja (futuro)
- **Descripción**: Evaluar si se necesita un controlador más potente (Raspberry Pi, Jetson Nano, etc.) para procesamiento de imágenes avanzado, filtro de Kalman, y fusión sensorial.
- **Contexto**: Los equipos de ligas mayores (MSL, SSL) usan procesadores potentes con OpenCV, redes neuronales, y planificación de trayectoria en tiempo real. En Junior Soccer Open, la mayoría usa microcontroladores + cámaras inteligentes (OpenMV, Pixy). Un Raspberry Pi 4/5 podría ser un punto medio.
- **Desafíos**: Consumo, tamaño, complejidad de software, confiabilidad (Linux vs bare-metal)
- **Estado**: Idea conceptual

### COM-010: Comunicación entre robots
- **Tipo**: Hardware + Software
- **Prioridad**: Media (futuro)
- **Descripción**: Implementar comunicación inalámbrica entre los 2 robots del equipo para compartir información de posición, estado, y coordinar estrategia.
- **Hardware**: Módulos de radio (ESP-NOW, nRF24L01, o el mismo ESP32 del módulo de árbitros)
- **Software**: Protocolo de comunicación, estructura de paquetes, manejo de latencia
- **Referencia**: Ver documento `sistema-posicionamiento-y-comunicacion.md`
- **Estado**: Diseño conceptual

---

## Tabla resumen de tracking

| ID | Descripción | Horizonte | Prioridad | Estado | Bloqueado por |
|----|------------|-----------|-----------|--------|---------------|
| HW-001 | Módulo árbitros | H1 Mundial | BLOQUEANTE | No iniciado | Especificación oficial |
| SW-001 | Giróscopo IMUPLUS | H1 Mundial | Alta | En staging | Prueba 28/3 |
| SW-002 | Fix UART | H1 Mundial | Alta | En staging | Prueba 28/3 |
| SW-003 | Fix rampa pateo | H1 Mundial | Alta | En staging | Prueba 28/3 |
| SW-004 | Fix currentYaw | H1 Mundial | Alta | En staging | Prueba 28/3 |
| SW-005 | Primitivas movimiento | H1 Mundial | Alta | En staging | Prueba 28/3 |
| SW-006 | Refactoring modular | H1 Mundial | Media | Propuesto | SW-001..005 |
| HW-002 | TOF frontales | H2 Deseable | Alta | No iniciado | Hardware |
| HW-003 | Lente wide-angle | H2 Deseable | Media | Pendiente compra | |
| SW-007 | Posición por cámara | H2 Deseable | Media | No iniciado | |
| SW-008 | Recalib heading visión | H2 Deseable | Media | No iniciado | SW-001 |
| HW-004 | Giróscopo dual | H2 Deseable | Baja | Idea | Hardware |
| HW-005 | Encoders magnéticos | H2 Deseable | Baja | Idea | Análisis mecánico |
| HW-010 | Motores brushless | H3 Roboliga | Alta | No iniciado | Investigación |
| HW-011 | Kicker + dribbler | H3 Roboliga | Alta | Concepto | Diseño mecánico |
| HW-012 | Placa sensores anillo | H3 Roboliga | Media | Concepto | Diseño PCB |
| HW-013 | Placa TOF posición | H3 Roboliga | Media | Concepto | Investigación |
| HW-014 | Multi-cámara 360° | H3 Roboliga | Media | Concepto | Costo |
| HW-020 | Procesador potente | H4 Futuro | Baja | Idea | |
| COM-010 | Comunicación robots | H4 Futuro | Media | Concepto | HW-001 (ESP32) |
