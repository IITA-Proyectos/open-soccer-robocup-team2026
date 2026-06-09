---
id: TASK-015
title: "CRC + fin de trama en el enlace de cámara OpenMV→TOP (con tests del parser)"
date_created: 2026-05-18
assigned: [mariaviollaz]
priority: P0
status: crc-END-implementado-2026-06-08-falta-tests-parser-y-validacion-banco
estimated_hours: 12
blocks: [confiabilidad de visión en partido]
tags: [firmware, vision, comunicacion, camara, openmv]
depends_on: []
---

# TASK-015 — CRC + fin de trama en el enlace de cámara

## Por qué importa (P0)

El enlace OpenMV→TOP es el **único sin checksum ni fin de trama** (framing legacy
9 B, 19200). Un byte de ruido eléctrico = coordenadas de pelota falsas
**indetectables** → el robot persigue una pelota fantasma en cancha. La
verificación (V-A4) además advierte: implementar CRC16 bit-a-bit en MicroPython
puede matar el framerate, y **no existen tests del parser de cámara**.

## Pasos concretos

1. **Primero: escribir tests host-native del parser actual** (`cameras.cpp`):
   basura antes de header, paquete partido, byte de dato == 201/202/203, resync.
   Sin esta red, no tocar el parser.
2. Agregar al paquete: byte(s) de **fin de trama** + **checksum**. Decisión de
   diseño: alinear con el sistema definitivo
   (`docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md`) —
   idealmente migrar a `proto.h`, mínimo CRC8/XOR + END.
3. En la OpenMV (`software/vision/enviar coordenadas 2 arcos y pelota`):
   implementar el checksum por **tabla precomputada** (NO bit-a-bit) para no
   perder fps. Medir fps antes/después.
4. Mantener la heurística de visibilidad o reemplazarla por un flag explícito
   (hoy `(0,0)`→"no visible" es frágil y acoplada al encoding legacy).
5. Adaptar el parser del Teensy + sus tests.

## Criterio de cierre

- [ ] Tests host-native del parser de cámara existen y pasan (antes y después).
- [ ] Paquete con END + checksum en ambos extremos.
- [ ] fps de la OpenMV medido: caída < 10 % respecto del actual.
- [ ] Fusión 2 cámaras sigue funcionando (test de regresión).

## Plan de prueba en hardware real

1. **Setup:** robot con cámara apuntando a pelota fija sobre cancha.
2. **Inyección de ruido:** cable de cámara cerca de los motores acelerando/
   frenando (EMI real), o cable parcialmente suelto.
3. **Criterio medible:** 0 coordenadas falsas aceptadas; los frames corruptos se
   cuentan y descartan; el robot no reacciona al ruido; latencia pelota→reacción
   no empeora > 5 ms.
4. **Regresión:** fusión de las 2 cámaras correcta; detección de "no visible"
   sigue andando con la pelota en el centro exacto.

## Notas / decisiones

_(completar al ejecutar — registrar fps medido y decisión proto.h vs CRC8)_

## Cambios de estado

- 2026-05-18: creada por Claude tras la verificación independiente, a pedido de
  Gustavo Viollaz.
- 2026-06-08: ✅ **CRC8 + END implementados en ambos extremos.** Cámara: el script de
  producción `hardware/electronics/camaras-openmv/main.py` arma el contrato v2 (11 bytes:
  3 headers + 6 coords codificadas [0,200] + **CRC8 = XOR de los 9 bytes de datos** + **END=254**;
  "no detectado" = sentinel 255,255). TOP: parser con `cam_crc8()` en `cameras.h`. El framing
  legacy 9 B sin checksum queda en `main-comunicacion-vieja.py` (referencia, NO se flashea).
  **NO cerrada** — falta de banco: (1) **tests host-native del parser** de cámara (paso 1 del
  criterio: basura/paquete partido/resync — todavía NO escritos); (2) **fps medido** antes/después
  (<10% caída); (3) deploy coordinado cámaras+TOP + **inyección de ruido** (EMI de motores) → 0
  coords falsas aceptadas. Esos los cierra el equipo en banco. Nota por Claude (Opus 4.8) a pedido de Gustavo.
