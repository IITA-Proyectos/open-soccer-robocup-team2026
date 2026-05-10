---
title: "Auditoría PCB DOWN — 10 nets sin rutear en la placa fabricada"
date: 2026-05-10
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: draft
tags: [pcb, down-board, bug-critico, hardware, validacion, urgente]
robot: ambos
area: electronica
tipo: analisis
---

# Auditoría PCB DOWN — 10 nets sin rutear

## 🚨 Resumen ejecutivo

La placa DOWN fabricada **NO funciona como anillo de 32 sensores ni como OTOS dual.** El análisis del archivo `PCB_PCB_Roboliga_2026_Futbol_2026-04-12.json` muestra **10 nets que están en la netlist (`routerRule.nets`, definidos por el schematic) pero NO tienen tracks en el PCB ruteado.**

**Funcionalidad real esperable de la placa fabricada (sin modificación):**
- ✅ 8 sensores de línea (los conectados al mux U4 vía `O4`).
- ❌ Los otros 24 sensores (muxes U1, U2, U3) — desconectados.
- ✅ 1 OTOS (U6, en bus I2C 2).
- ❌ 1 OTOS (U5, en bus I2C 1) — desconectado.
- ❌ Análisis diferencial OTOS (Q5 del coach) — imposible sin 2 OTOS funcionales.

## Análisis técnico

### Comparación netlist (schematic) vs tracks (PCB)

Comparé `routerRule.nets` (94 nets definidos por el schematic) contra los nets que tienen tracks en el array `shape` del PCB. **10 nets faltantes en el PCB:**

| Net | Origen | Función | Severidad |
|-----|--------|---------|-----------|
| `O1` | Mux U1 → Teensy | Salida agregada de 8 sensores de línea (S1-S8) | **P0 — 8 sensores muertos** |
| `O2` | Mux U2 → Teensy | Salida agregada de 8 sensores de línea (S9-S16) | **P0 — 8 sensores muertos** |
| `O3` | Mux U3 → Teensy | Salida agregada de 8 sensores de línea (S17-S24) | **P0 — 8 sensores muertos** |
| `SDA1` | OTOS U5 → Teensy | I2C bus 1 data | **P0 — OTOS U5 muerto** |
| `SCL1` | OTOS U5 → Teensy | I2C bus 1 clock | **P0 — OTOS U5 muerto** |
| `E1` | ? → ? | Señal de control / I/O | P1 — verificar uso |
| `E6` | Mux INH? | Habilitación de mux (probable) | P1 — relacionado con O1-O3 |
| `E7` | Mux INH? | Habilitación de mux (probable) | P1 — relacionado con O1-O3 |
| `E11` | Mux INH? | Habilitación de mux (probable) | P1 — relacionado con O1-O3 |
| `LED32_2` | LED 32 cathode | Un LED del par activo S32 | P2 — sensor F32 puede igual andar si pull-up alcanza |

> **Comprobación adicional:** los nets `O1, O2, O3, SDA1, SCL1` aparecen en `routerRule.nets` (definidos), aparecen en strings del JSON (referenciados en componentes/pads), pero **ningún `TRACK~` los menciona en su net field**. Conclusión: existen como conexión "lógica" en el schematic pero no como conexión "física" en el PCB.

### Hipótesis sobre cómo pasó

Lo más probable: **el equipo mandó a fabricar la placa sin correr DRC (Design Rule Check) + ERC (Electrical Rule Check) en EasyEDA**, que habrían detectado los unrouted nets como warning. Esta es la clase de bug que el DRC/ERC atrapa rutinariamente.

### Pruebas que descartan otras hipótesis

- **¿Será que los nets están en otra capa interna no detectada?** No — la placa es 2-capa (top + bottom), no hay capas internas según el JSON.
- **¿Será un error de mi parser?** No — los nets aparecen en `routerRule.nets` (lista definida por el schematic) y NO aparecen como atributo `net` de ningún `TRACK~`. Verificado con doble check.
- **¿Será que `O1/O2/O3` se mapean a otro nombre en el PCB (alias)?** Improbable — `O4` sí está ruteado con ese nombre exacto, y `O1/O2/O3` siguen el mismo patrón de nomenclatura.

## Opciones (decisión del coach)

### Opción A — Soldar puentes manualmente sobre la placa fabricada

**Esfuerzo:** ~2-4 horas de trabajo de Enzo.

**Pasos:**
1. Identificar pads de salida de cada mux U1, U2, U3 (pin 3 = COMOUT/IN).
2. Identificar 4 entradas analógicas libres del Teensy U7 que NO estén usadas.
3. Soldar 3 wires (O1, O2, O3) entre cada mux y un pin analógico del Teensy.
4. Identificar pads de SDA1/SCL1 cerca del OTOS U5 (pin 3 SDA, pin 4 SCL).
5. Soldar 2 wires entre OTOS U5 y los pads I2C correspondientes del Teensy (pines 16/17 si Wire1 está en default, o 24/25 si remap).
6. Verificar continuidad con multímetro.
7. Documentar las modificaciones en `hardware/electronics/down-board-mods-fab1.md` (foto + diagrama de wires).

**Pros:** rápido, ya tenemos la placa, sin esperar fabricación.

**Contras:** placa con cables sueltos (frágil para competencia, choques pueden cortar wires), poco profesional para foto del poster A1, riesgo de errores en cableado.

### Opción B — Re-fabricar la placa con el ruteo corregido

**Esfuerzo:** ~1 semana (China) + costo de fabricación + tiempo del equipo para correr DRC + revisar.

**Pros:** placa limpia, sin cables externos, robusta para competencia, foto profesional.

**Contras:** ~1 semana mínimo (JLCPCB tiene "fast" en 3 días + envío). Costo extra de PCB + envío internacional.

**Calendario:** si se decide hoy 2026-05-10 y JLCPCB tarda 3 días + envío 7 días = 10 días. Llegaría ~2026-05-20. Eso deja 5-6 semanas para Incheon — todavía viable pero ajustado.

### Opción C — Aceptar funcionamiento parcial (8 sensores + 1 OTOS)

**Pros:** cero trabajo adicional, arrancamos firmware ya.

**Contras:** **anula la razón de ser de la placa DOWN.** Con 8 sensores en lugar de 32 perdemos la cobertura angular del anillo. Con 1 OTOS perdemos el análisis diferencial (Q5 explícita del coach).

**Mi recomendación coach:** **Opción A (puentes manuales)** para no perder tiempo + **Opción B en paralelo** para tener placa limpia rumbo a Nacional Nov 2026.

## Implicancias para el firmware DOWN

El diseño en `2026-05-10-diseno-firmware-3-placas.md` (módulo `line_ring.h` para 32 sensores + `otos.h` para 2 OTOS) **NO se puede implementar tal cual** hasta resolver la modificación física de la placa. Mientras tanto, **firmware DOWN debe escribirse con `#ifdef DOWN_BOARD_HAS_MODS`** para soportar ambos casos:

- Sin mods: solo 8 sensores (mux U4 + O4) + 1 OTOS (U6).
- Con mods: 32 sensores + 2 OTOS según diseño original.

Esto permite arrancar Hito 2 ahora con la versión parcial y migrar cuando los mods estén hechos.

## Acciones inmediatas (asignadas en `team-tasks/`)

1. **`team-tasks/2026-05-10-task-001-pcb-down-unrouted-nets-fix.md`** — Enzo decide entre A/B/C. P0.
2. **`team-tasks/2026-05-10-task-002-drc-erc-pcb-completo.md`** — Enzo corre DRC+ERC en EasyEDA sobre **ambas placas** para detectar más unrouted nets antes de Hito 2 / Hito 3. P0.

## Lección para vibe-pcb-design skill

Esta es exactamente la clase de bug que la skill `vibe-pcb-design` que diseñamos debe atrapar en sus verification gates. Actualizar la skill local con un hard-rule explícito:

> **No mandar a producir sin DRC clean + ERC clean documentados. Capturar screenshot del reporte como evidencia y archivarlo en `hardware/electronics/pcb_design/<board>/drc-erc-reports/`.**

## Referencias técnicas

- Archivo analizado: `hardware/electronics/pcb_design/down_board/PCB_PCB_Roboliga_2026_Futbol_2026-04-12.json`
- Total nets en `routerRule.nets`: **94**
- Total nets con tracks: **84**
- Diferencia: **10 nets sin rutear**.
- Datos crudos: ver tabla en sección "Análisis técnico" arriba.
