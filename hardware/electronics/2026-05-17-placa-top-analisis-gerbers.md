---
title: "Placa TOP (Roboliga2026 TOP) — Análisis de gerbers y GAP de documentación"
date: 2026-05-17
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [electronica, top-board, teensy, gerbers, gap-documentacion, referencia]
robot: ambos
area: electronica
tipo: analisis
---

# Placa TOP (Roboliga2026 TOP) — Análisis de gerbers + GAP

> **Fuente:** `Tope-20260517T175152Z-3-001.zip` (2026-04-20). **Solo trae
> gerbers.** NO trae BOM ni Pick&Place. EasyEDA v6.5.40, gerber 2026-04-15.

## 1. Hallazgo principal: GAP de documentación (P1)

A diferencia de Base y Comm, el paquete de la placa **TOP no incluye BOM ni
Pick&Place**. Las identidades de los componentes **no se pueden determinar
desde los gerbers** (la serigrafía es trazo vectorial, no texto). Esta placa es
hoy una **caja negra documental** — exactamente lo que la disciplina del repo
(CLAUDE.md reglas 5/6) existe para evitar.

> Lo que sí sabemos de la TOP viene de `hardware/electronics/mapa-pines-placas-nuevas.md`
> (decodificado del schematic 2026-04-08): MCU **Teensy 4.0** (U14), 2× BNO055
> (IMU dual), 4× ToF, HC-SR04, 2 UART cámaras OpenMV, UART a COMM, conector 6P
> "PINES MODULO" (= mate del U3 de la placa COMM), conector 4P
> (LOGV/3.3V/GND/+5V), 2× MP1584, Dean, 2× Schottky. **Eso es schematic, no la
> fabricación entregada** — hay que cerrar el gap con el BOM/P&P reales.

## 2. Lo que SÍ se extrae de los gerbers

| Métrica | Valor | Fuente |
|---------|-------|--------|
| Nombre CAD | `Roboliga2026_TOP` | header `G04` |
| Bounding box (vértices) | 224.0 × 97.5 mm | `.GKO` |
| **Envelope real (arcos trazados)** | **≈ 229.7 × 103.7 mm** | arcos `G03` del `.GKO` |
| Forma | Contorno irregular curvo (6 segmentos + 4 arcos) — deck superior contorneado | `.GKO` |
| Capas | 2 (cobre top + bottom presentes) | `.GTL` + `.GBL` |
| Footprints (clusters) | ~19 multi-pad + ~10-12 pads aislados | clustering D03 |
| Cluster grande | 35 pads en (169.5, 79.9) mm = IC/módulo/conector grande | `.GTL`/`.GBL` |
| Headers | ~6 clusters de 6 pads = headers 2×3 / 2.54 mm | clustering |
| Drills PTH | 140 (incl. 54 de 1.0 mm = pines de header 2.54) | `Drill_PTH_*` |
| Drills NPTH | 13 = **8× M2 + 4× M3 + 1× 5 mm** | `Drill_NPTH_Through.DRL` |

13 agujeros mecánicos + outline grande curvo + muchos headers 2.54 =
**placa de distribución / deck superior** atornillada al chasis en muchos
puntos, que rutea power y señales entre subsistemas del nivel superior (MCU
master ↔ cámaras / sensores / módulo COMM).

`How-to-order-PCB.txt` solo trae un link genérico de EasyEDA — **el stack de
fabricación (espesor / cobre / acabado) no está documentado**. Inferido: 2
capas FR4 estándar, pero es asunción, no dato.

## 3. Temas a analizar (frame coach)

### TOP board sin BOM ni Pick&Place — recuperar antes de confiar

**Categoría:** electrónica / docs · **Robot:** ambos · **Prioridad:** P1

**Qué observo.** El paquete de fab de la TOP solo trae gerbers. No se puede
saber qué IC es el cluster de 35 pads, qué matean los conectores, ni ratings
de potencia. No se puede pedir repuestos ni reparar a ciegas.

**Risk-no-fix.** Placa crítica (es el master del robot por schematic) sin
trazabilidad de componentes → si se quema algo en Incheon no hay con qué
reponer; integración a ciegas.
**Risk-fix.** Nulo — es exportar del proyecto EasyEDA existente.
**Tiempo estimado.** 1–2 h.

**Plan de prueba / cierre.**
1. Enzo exporta BOM + Pick&Place del proyecto EasyEDA `Roboliga2026_TOP`.
2. Renderiza `Gerber_TopSilkscreenLayer.GTO`/`.GBO` en visor (KiCad GerbView /
   gerbv) y archiva un PNG en `hardware/electronics/`.
3. Se cruza con `mapa-pines-placas-nuevas.md`: el BOM real debe coincidir con
   el schematic decodificado (Teensy 4.0 U14, 2× BNO055, etc.).
4. Criterio de cierre: BOM + P&P + PNG de serigrafía commiteados al repo.

→ Se crea `team-tasks/2026-05-17-task-013-recuperar-bom-placa-top.md`.

## 4. Fuentes

`C:\Users\violl\iitasoccer\placaspedidas\Tope-20260517T175152Z-3-001.zip` →
`Gerber_BoardOutlineLayer.GKO`, `Gerber_TopLayer.GTL`, `Gerber_BottomLayer.GBL`,
`Drill_NPTH_Through.DRL`, `How-to-order-PCB.txt`.
Cruzado con `hardware/electronics/mapa-pines-placas-nuevas.md`.
