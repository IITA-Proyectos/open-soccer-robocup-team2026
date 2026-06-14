---
title: "TOP A2.1: config persistente de sensores en EEPROM — deshabilitar cámara/BNO/ToF/ultrasonido + ubicación ToF, con comandos y carga al boot"
date: 2026-06-14
author: "Claude (Anthropic, Opus 4.8) — superpowers (Brainstorm→Plan→Execute→Review) + workflow multi-agente"
requested-by: "Gustavo Viollaz (@gviollaz)"
area: comunicacion
tipo: resultado
robot: ambos
status: code-complete + gate host verde + pio SUCCESS — PENDIENTE BANCO (hardware lo cierra el equipo)
relacionada: TASK-206, research/in-progress/2026-06-13-diseno-monitor-general-top-config-persistente.md
---

# A2.1 — Config persistente de sensores de la TOP (fail-safe)

**Sesión:** Claude + Gustavo. Método: **superpowers** + **workflow** (6 agentes de análisis).
Segundo paso del "Monitor del sistema de posicionamiento" (A1 = telemetría v2; A2 = config).

## Qué es A2.1

El fail-safe que paga en cancha: **apagar un sensor que manda basura** (la pelota fantasma de
la cámara dual vista el 2026-06-13) y que la decisión **persista en la EEPROM de la TOP**.

- **Deshabilitar** cámara F/B, BNO L/R, ultrasonido, ToF entero (booleanos, efecto inmediato).
- **Ubicación/bearing** por ToF (reemplaza el mapeo hardcodeado `TOF_MOUNT_ANGLE_DEG`).
- **Persistir** todo en EEPROM (offset 368, magic+version+CRC16) + carga ungated al boot.
- **Comandos** `CAM/BNO/US/TOF ON|OFF`, `TOF n POS`, `CFG SAVE|LOAD|RESET` + línea `CFG` en el
  texto humano (ENTER).

**A2.2 queda para después** (zonas/rotación/flip del ToF): requiere subir la lectura de 16→64
zonas, lo que sube el I²C ×3-5 y arriesga el loop a 100 Hz. El struct YA reserva esos campos
(mismo formato/versión EEPROM) → A2.2 no será wire-breaking respecto a A2.1.

## Proceso (superpowers + workflow)

- **Análisis (workflow, 6 agentes):** EEPROM/persistencia · sensors_tof (el más riesgoso) ·
  apply BNO/cámaras/US · comandos+cfg · boot/glue + síntesis. Hallazgos que guiaron el split:
  hoy el ToF lee **16 zonas (4×4)**, no 64 → anular zonas es A2.2; los apply de enables YA tienen
  rieles (`front_alive`/`back_alive`, `scfg.enabled`, `TOF_NO_READING`) → A2.1 es solo pre-ANDear.
- El repo ya tenía el patrón de 3 capas probado en DOWN (`calib_storage` puro + `eeprom_calib`
  glue + carga al boot) → se espejó como `top_config` + `top_eeprom_config`.

## Cambios

- **`src/shared/top_config.{h,cpp}` (NUEVO, PURO):** struct `TopConfig` (enables + por-ToF
  enabled/bearing + campos A2.2 reservados) + serialize/deserialize byte-a-byte LE + CRC16-CCITT
  + defaults no-op. **12 tests host** (`test_top_config`: round-trip, CRC check 0x29B1, byte-layout,
  rechazo de magic/version/CRC/EEPROM-en-blanco).
- **`src/top/top_eeprom_config.{h,cpp}` (NUEVO, glue Arduino):** offset 368, `EEPROM.update` byte
  a byte; `g_top_cfg` global; `top_config_load` deja defaults si la EEPROM es inválida.
- **`src/top/main_top.cpp`:** carga `top_config_load(&g_top_cfg)` al boot (ungated, antes de los
  inits de sensores).
- **Apply-points (mínimos, sin tocar el orden crítico de init):**
  - `cameras_runtime.cpp`: `front_alive &= cam_front_en` (y back) → cámara off no entra a la fusión.
  - `sensors_imu.cpp`: `g_scfg[0/1].enabled = bno_left/right_en` → BNO off excluido de la fusión.
  - `sensors_tof.cpp`: ultrasonido off = NO_READING (sin pulseIn); ToF off = NO_READING en el getter.
  - `localization_runtime.cpp`: bearing de cada ToF desde la config (default = mapeo de hoy).
- **Comandos:** `telemetry_top.{h,cpp}` enum `TtCmd` + `tt_parse_command` (PURO, host-tested) +
  `TT_TOK_MAX` 3→4; `top_telemetry_serial.cpp` dispatch (muta `g_top_cfg` + CFG SAVE/LOAD/RESET +
  ACKs) + línea `CFG` en `emit_human`.
- **Docs:** `docs/firmware/EEPROM-MAP.md` (NUEVO, mapa por placa) + `TELEMETRIA-TOP.md` §2 comandos.

## Decisión de scope (sin segundo wire-break)

El bloque `cfg` JSON (schema 2→3) + su parser en la app quedaron **diferidos**: la GUI todavía no
tiene ni los paneles v2 (carril del otro agente), así que el bloque JSON no tiene consumidor. La
visibilidad de config en banco se cubre con la **línea `CFG` del texto humano** (ENTER), que NO
versiona ni toca la app. El bloque JSON se hará en una pasada coordinada con la GUI (o junto a A2.2).

## Verificación (lo que Claude cierra)

- **Host:** `test_top_config` **12/0** (NUEVO); `test_telemetry_top` 19→**20** (comando config);
  gate completo VERDE (ver corrida).
- **Firmware:** `pio run -e top_robot2_pri` **SUCCESS** (FLASH code 74344).

## NO validado (hardware lo cierra el equipo)

- ⚠️ El binario de competencia cambió; sigue **byte-idéntico en conducta con EEPROM en blanco**
  (defaults no-op) → **regresión A/B en banco**: flashear, EEPROM blanca → 30 min, diff de snapshots = 0.
- Banco: `CAM B OFF` con la trasera mintiendo → la fusión la ignora; `CFG SAVE` → power-cycle →
  sigue off (persistencia). `BNO R OFF` → heading al otro. `TOF 0 OFF` → `min_obst` lo ignora.
  Confirmar que la calib del BNO ([320,367]) sobrevive un `CFG SAVE` (offset 368 no la pisa).

## Próximo (A2.2)

Zonas/rotación/flip del ToF: subir a 64 zonas + `top_config_apply_zone_mask` + medir el loop a
100 Hz en banco (riesgo aislado). Y el bloque `cfg` JSON + GUI (coordinado con el otro agente).
