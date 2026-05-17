---
id: TASK-013
title: "Recuperar BOM + Pick&Place + serigrafía de la placa TOP (Roboliga2026 TOP)"
date_created: 2026-05-17
assigned: [enzzo195]
priority: P1
status: pending
estimated_hours: 2
blocks: [trazabilidad placa TOP, repuestos Incheon, integración TOP-COMM]
tags: [electronica, top-board, bom, documentacion, gap]
depends_on: []
---

# TASK-013 — Recuperar documentación de la placa TOP

## Por qué importa

El paquete de fabricación de la placa **TOP** entregado el 2026-04-20
(`Tope-20260517T175152Z-3-001.zip`) **solo contiene gerbers**. No tiene BOM ni
Pick&Place. A diferencia de las placas Base y Comm (ambas con BOM + P&P +
netlist), la TOP es hoy una **caja negra documental**: no se puede saber qué IC
es cada componente, qué matean los conectores, ni los ratings de potencia. La
TOP es el **master del robot** (Teensy 4.0 según schematic) — sin trazabilidad
no hay forma de pedir repuestos ni reparar en Incheon.

Detalle técnico: `hardware/electronics/2026-05-17-placa-top-analisis-gerbers.md`.

## Pasos concretos

1. Abrir el proyecto EasyEDA fuente `Roboliga2026_TOP` (Enzo lo tiene — los
   gerbers dicen EasyEDA v6.5.40, generados 2026-04-15).
2. Exportar **BOM** (`.csv`/`.xlsx`) y **Pick&Place** (`.csv`).
3. Renderizar `Gerber_TopSilkscreenLayer.GTO` y `.GBO` en un visor (KiCad
   GerbView o gerbv) y exportar un **PNG de la serigrafía** (con designadores
   y rótulos de conectores legibles).
4. Commitear los 3 archivos a `hardware/electronics/` (BOM, P&P, PNG).
5. Cruzar el BOM real contra `hardware/electronics/mapa-pines-placas-nuevas.md`
   (schematic decodificado: debe aparecer Teensy 4.0 U14, 2× BNO055, 4× ToF,
   HC-SR04, etc.). Anotar discrepancias en "Notas".
6. Registrar también el stack de fabricación pedido (espesor / cobre / acabado)
   — `How-to-order-PCB.txt` no lo documenta.

## Criterio de cierre

- [ ] BOM de `Roboliga2026_TOP` commiteado en `hardware/electronics/`.
- [ ] Pick&Place commiteado.
- [ ] PNG de serigrafía (top + bottom) commiteado.
- [ ] BOM real cruzado contra `mapa-pines-placas-nuevas.md`, discrepancias
      anotadas en "Notas".
- [ ] Stack de fab registrado.

## Notas / decisiones

_(vacío — completar al ejecutar)_

## Cambios de estado

- 2026-05-17: creada por Claude durante el análisis de las 3 placas
  (journal `2026-05-17-analisis-3-placas-y-correccion-firmware-c6.md`),
  bajo requerimiento de Gustavo Viollaz.
