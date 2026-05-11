---
id: TASK-010
title: "Verificar si el firmware oficial RCJ Communication Module es compatible con ESP32-C6 (vs ESP32 clásico)"
date_created: 2026-05-10
assigned: [virginia, elias]
priority: P0
status: pending
estimated_hours: 4
blocks: [TASK-006 (cargar firmware RCJ), homologación Incheon]
tags: [firmware, esp32, comm-board, rcj-official]
depends_on: [TASK-006]
---

# TASK-010 — Compatibilidad firmware RCJ con ESP32-C6

## Por qué importa

El BOM de la placa COMM (revelado hoy 2026-05-10 en el commit `343c420` de Enzo) confirma que el MCU es **ESP32-C6-MINI-1-N4** (Espressif), no el ESP32 clásico.

Diferencias críticas:

| Característica | ESP32 clásico | ESP32-C6 |
|----------------|---------------|----------|
| CPU | Xtensa LX6 (dual core) | **RISC-V** (single core) |
| Arduino SDK target | `esp32` (default) | `esp32c6` (más nuevo, soporte parcial) |
| WiFi | 4 (802.11n) | **6 (802.11ax)** |
| BLE | 4.2 | **5.0 LE** |
| GPIO | 34 (clásico) | 22 (C6) — **menos pines** |
| Thread / Zigbee | No | Sí |

**El firmware oficial RCJ** (https://github.com/robocup-junior/soccer-communication-module) fue diseñado y testeado en **ESP32 clásico**. Compilarlo para ESP32-C6 requiere:

1. Cambio de target SDK (`espressif32` → `espressif32` con board `esp32-c6-devkitm-1` o similar).
2. Revisar GPIO mapping — el ESP32-C6 tiene menos pines y otras numeraciones.
3. Revisar APIs usadas — algunas librerías clásicas no compilan en RISC-V sin updates.
4. Testear conexión a árbitros oficial (¿el protocolo se mantiene?).

Si la homologación de Incheon exige el firmware oficial **exactamente como está publicado**, hay un problema serio.

## Pasos concretos

### Fase 1 — Investigación (2 horas)

1. Clonar el repo del firmware oficial:
   ```
   git clone https://github.com/robocup-junior/soccer-communication-module
   ```
2. Leer `README.md` del firmware oficial. ¿Mencionan ESP32-C6, o solo ESP32 clásico?
3. Revisar `platformio.ini` o `.ino` files: identificar `board = ...` o `#include` de librerías específicas (`<WiFi.h>` vs `<WiFi_C6.h>`).
4. Buscar issues / PRs del repo oficial: alguien más puede haber portado ya al C6 (esto es lo más probable porque el módulo oficial RCJ pudo haber migrado a C6 también).
5. Verificar en RCJ Forum / Discord si **hay equipos usando C6** ya en competencia 2026 o si el módulo oficial siguió en clásico.

### Fase 2 — Compilación experimental (1 hora)

1. Instalar PlatformIO si no está.
2. Intentar compilar el firmware oficial con `board = esp32-c6-devkitm-1`.
3. Listar errores de compilación. Si son pocos, hay esperanza. Si son muchos (cambios profundos de API), considerar Plan B.

### Fase 3 — Decisión (1 hora)

Según hallazgo, decidir:

- **(a) Plan A**: el firmware oficial soporta C6 → cargarlo y testear (TASK-006).
- **(b) Plan B**: hay port no-oficial documentado → usar ese (verificar que pasa homologación).
- **(c) Plan C**: portar nosotros el firmware oficial al C6 (~2-3 días de trabajo).
- **(d) Plan D**: implementar firmware propio que respete el **protocolo RCJ** del módulo. Más rápido pero hay que documentar bien para homologación.

## Criterio de cierre

- [ ] Investigación documentada en `research/in-progress/2026-05-XX-firmware-rcj-c6-compat.md`.
- [ ] Compilación experimental ejecutada, errores documentados.
- [ ] Decisión tomada (Plan A/B/C/D) con justificación en journal.
- [ ] Si Plan C o D: estimación de tiempo realista y nuevo task creado para implementación.

## Notas / decisiones

_(actualizar cuando se ejecute)_

## Cambios de estado

- 2026-05-10: creado por Claude tras descubrir ESP32-C6 en el BOM Comm 04-20.
