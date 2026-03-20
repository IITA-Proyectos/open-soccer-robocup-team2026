---
title: "Investigar: Sensores TOF para posicionamiento en cancha"
date: 2026-03-20
author: "Gustavo Viollaz"
status: backlog
priority: media
tags: [tof, lidar, posicionamiento, hardware]
roadmap_id: HW-013
---

# Investigar: Sensores TOF para posicionamiento

## Pregunta

¿Cómo implementar un sistema de posicionamiento en cancha usando sensores TOF y cuál es la precisión esperable?

## Qué investigar

1. **Sensores**: VL53L0X vs VL53L1X vs VL53L4CX. Alcance, precisión, velocidad, costo.
2. **Cantidad**: 8 vs 12 vs 16 sensores. Trade-off precisión vs complejidad.
3. **Multiplexor I2C**: TCA9548A permite 8 dispositivos I2C con misma dirección.
4. **Algoritmo de triangulación**: Cómo calcular (x, y) a partir de distancias a paredes.
5. **Procesador local**: ¿Teensy 4.0 o ESP32 dedicado? Ventajas de cada uno.
6. **Interferencia**: ¿Los TOF del oponente interfieren con los nuestros? (usan IR).
7. **Cancha**: Dimensiones oficiales 2026, material de paredes/rejas.
8. **Precisión reportada**: Buscar en TDPs de equipos que usen TOF.

## Referencia

- Equipos como EME (Eindhoven) y otros de Soccer Open usan arrays TOF
- VL53L0X datasheet: https://www.st.com/resource/en/datasheet/vl53l0x.pdf
