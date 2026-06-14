---
title: "Catálogo de diagramas del robot + figuras D5 (eléctrico) y D10 (layout/cobertura)"
date: 2026-06-13
author: "Claude Opus 4.8 (Anthropic), vía Claude Code"
requested-by: "Gustavo Viollaz (@gviollaz)"
area: [electronica, mecanica, vision]
tipo: asset-figura
robot: ambos
prioridad: alta
status: vivo
---

# Catálogo de diagramas + primeras dos figuras de arquitectura

## Qué se hizo

1. **Relevamiento + catálogo de 21 diagramas** del robot, vía workflow multi-agente
   (7 lectores de dominio → 3 lentes arquitecto/pedagógico/marketing → síntesis →
   crítica de completitud). Cada contenido verificado contra el **código vivo**, no
   contra los docs (que en varios puntos están desactualizados). El catálogo cubre:
   topología 3 placas (D1), 2 módulos por contrato (D2, draft existente), capas de
   abstracción (D3, draft), mapeo sensor→campo del snapshot (D4), eléctrico (D5),
   presupuesto de potencia (D6), dataflow Fig.2 (D7, existe), presupuesto temporal +
   freno <15 ms (D8), frame UART + corrección de errores (D9), layout/cobertura (D10),
   pila de placas (D11), FSM dual (D12, Fig.4 stale → regenerar), localización (D13),
   pipeline visión (D14), árbol de fallos (D15), R1 vs R2 (D16), Fig.8/9 (D17/18, existen),
   madurez/timeline/roadmap (D19–21, drafts).

2. **D5 — `assets/drafts/fig_electrico_distribucion.svg` (+ .png)**: árbol de distribución
   de potencia. Batería LiPo 2S 7.4 V / 6800 mAh con Deans XP1 por placa (sin bus
   central) → por placa Schottky B5819W → 2× MP1584 → rieles 5 V/3.3 V → cargas; CENTRAL
   vía Zircon → 3 H-bridges → 3 motores; COMM con LDO UA78M33. Callout rojo del brownout
   de OTOS. Panel de honestidad: sin switch maestro documentado, `battery_mv=0` (sin
   monitoreo), set-points de los MP1584 sin medir, voltaje de H-bridges `?`.

3. **D10 — `assets/drafts/fig_layout_cobertura.svg` (+ .png)**: vista superior a escala
   (1.6 px/mm), eje +Y=frente/+X=der. Cobertura de cámaras (frente+atrás ~140°, con
   zonas ciegas laterales), 4 ToF (±45°, cierran 360°), HC-SR04 frontal; ruedas omni
   en posición física, anillo de 32 sensores con hueco atrás, 2 OTOS. FOV de cámaras y
   haz del HC-SR04 marcados como **asumidos** (docs era H7) → medir en banco.

## Proceso (skill rcj-diagramas-poster)

Pregunta única por figura, ≤7 unidades, matemática de impresión ANTES de dibujar
(viewBox 1440 px → 360 mm → 0.25 mm/px, cuerpo ≥24 pt), y **render con Edge headless +
mirar** con iteración (D5 necesitó 2 pasadas por corte de texto y solape de etiquetas).
Paleta firma: azul=TOP, naranja=CENTRAL, verde=DOWN, rojo=emergencia.

## Estado / pendientes

- Son **drafts** (la skill deja `drafts/` como zona de borrador; el equipo decide qué
  sube al A1 y con qué número Fig.N). Al maquetar, mantener ancho impreso ≈360 mm para
  no bajar del mínimo de legibilidad de la rúbrica.
- Cifras con `?`/`*` requieren **banco** para cerrarse (set-points MP1584 con multímetro;
  FOV real N6 + alcance HC-SR04). Claude no cierra eso (regla de hardware).
- Siguen en el catálogo: **D1** (topología 3 placas) y **D4** (mapeo sensor→campo del
  snapshot) — próximos a producir.
- Alertas para futuras figuras: Fig.4 FSM (D12) está desactualizada (regenerar contra
  `strategy.cpp`); watchdog de motores real = `SNAPSHOT_TIMEOUT_MS=500` (no 200 ms);
  arcos amarillo/azul (no cyan/magenta); WorldSnapshot = 31 B v3 (un doc viejo dice 27 B);
  contradicción BNO de R1 sin resolver en `pinout_common.h`.
