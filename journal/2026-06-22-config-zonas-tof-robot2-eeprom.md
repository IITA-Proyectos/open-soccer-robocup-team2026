---
title: "Banco — config de zonas/orientación ToF de robot2 escrita a EEPROM (desde la GUI)"
date: 2026-06-22
author: "Claude Opus 4.8 (Anthropic) + Gustavo Viollaz (placa)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
tipo: banco-config
toca-competencia: SÍ (escribe g_top_cfg.tof[].zone_mask en la EEPROM de robot2 → cambia la distancia ToF que usa el robot)
status: aplicado + verificado EN VIVO (robot2, COM22). Persistencia confirmada por firmware ("config guardada en EEPROM"); falta el power-cycle de confirmación (lo cierra el equipo).
---

# Config de zonas ToF de robot2 → EEPROM

Gustavo configuró la orientación + veto de zonas de los 4 ToF en la GUI del monitor (vista **Config
ToF**) y pasó las capturas (`OneDrive/configuracionTOF.docx`). **Captura 1 = robot2, captura 2 = robot1**
(los nombres figuran en las capturas).

Para **robot2** (conectado por USB, COM22) apliqué la **captura 1** por serie + `CFG SAVE`:

| ToF | pos | rotación (GUI) | veto | ZONEMASK CRUDA enviada |
|---|---|---|---|---|
| 0 | FRENTE    | **180°** | 2ª fila desde abajo | `0x00F0` (raw zonas 4–7) |
| 1 | ATRÁS     | **180°** | idem | `0x00F0` |
| 2 | DERECHA   | **90°**  | idem | `0x4444` (raw columna 2) |
| 3 | IZQUIERDA | **270°** | idem | `0x2222` (raw columna 1) |

## Cómo se aplicó

- **Posiciones** ya estaban correctas en EEPROM (`tof[0:@0 1:@180 2:@270 3:@90]`, verificado antes) → no se reenviaron.
- **Rotación NO se manda como `ROT`** (el build `top_robot2_pri` no tiene `-DTOP_ENABLE_TOF_ROT`): se
  **pliega en la ZONEMASK cruda** (modo default de la app). Las máscaras se calcularon con
  `monitor_base.tof_layout.raw_zone_mask` (módulo testeado, el mismo que usa la GUI) — veto = "solo 2ª
  fila desde abajo" (`apply_default_veto`).
- Comandos enviados: `TOF n ZONEMASK <hex>` ×4 + `CFG SAVE`. Firmware: cada máscara → **"efecto
  inmediato; CFG SAVE para persistir"**; `CFG SAVE` → **"config guardada en EEPROM"**.

## Verificación EN VIVO (no solo el ack)

Leído el stream: para los 4 sensores, **`dist` (tof_mm, lo que usa el robot) == promedio de SOLO las
zonas activas de su máscara** (±1 mm, 3 frames). O sea el veto **SÍ afecta** la distancia.
→ El tooltip "Hoy es SOLO visual: el robot no aplica el veto" de la app **está DESACTUALIZADO**: el
firmware (A2.2) aplica `g_top_cfg.tof[i].zone_mask` a las zonas crudas. Conviene corregir ese texto en
el monitor.

## ⚠️ Importante / pendientes

- **La rotación NO es legible de la EEPROM** (va plegada en la máscara). Este journal + la captura son el
  único registro de que robot2 usa rot 180/180/90/270. Si se reconfigura, partir de acá.
- **Power-cycle de confirmación:** el firmware dijo "guardada en EEPROM" (CRC + dual-bank); falta apagar/
  prender y re-leer para cerrar la persistencia. Lo hace el equipo.
- **robot1 (captura 2):** rot ~90/90/… (parte tapada por un tooltip en la captura). Cuando se conecte
  robot1, releo la captura 2 con precisión y aplico el mismo procedimiento.
- Herramienta usada: `tools/monitor-base/probe_top_serial.py` + scripts ad-hoc (en %TEMP%).
