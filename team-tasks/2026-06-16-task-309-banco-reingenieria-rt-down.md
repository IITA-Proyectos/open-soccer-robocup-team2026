---
id: TASK-309
title: "Banco: validar la reingeniería RT de la placa DOWN (F0–F5 gateadas) — latencia + fail-safe"
date_created: 2026-06-16
assigned: [equipo (firmware DOWN)]
priority: P2  # post-Incheon (capitaliza 2027). Sube a P1 lo que toque el freno de borde si se decide usar en Incheon.
pedido-por: Gustavo Viollaz (2026-06-15/16: "optimizar DOWN con rutinas en paralelo + SUPER confiable fail-safe")
status: firmware-PROGRAMADO-2026-06-16  # glue gateado + módulos puros ESCRITOS + COMPILAN + host-tested. Falta banco. Claude NO cierra HW (regla 1).
relacionada: TASK-207 (BNO bus aparte), TASK-002 (DRC), diseño docs/firmware/ARQUITECTURA-LAZO-DOWN-RT.md
tags: [firmware, down, tiempo-real, latencia, fail-safe, adc, otos, rx, banco, hardware-real]
depends_on: []
---

# TASK-309 — Banco de la reingeniería RT de la placa DOWN

## Qué se programó (todo GATEADO off-by-default → competencia `[env:down]` byte-idéntica)

Diseño: [`docs/firmware/ARQUITECTURA-LAZO-DOWN-RT.md`](../docs/firmware/ARQUITECTURA-LAZO-DOWN-RT.md)
(validado adversarialmente + banner de estado). Todo host-tested + compila; **NADA probado en
banco** (regla #1: solo el equipo con la placa cierra). Módulos puros nuevos: `adc_scan_plan.h`,
`line_neighbors.h`, `line_reliable_gate.h`, `rx_calib_defer.h`, `rx_byte_budget.h`,
`down_blackboard.h` + `line_early_escape.h` refinado — **79 tests host, full gate 1162/0**.

| Fase | Gate / env de banco | Qué mide/valida en banco |
|---|---|---|
| **F0** | `-DDOWN_LOOP_MONITOR` / `down_loopmon` | El "ANTES": `loop_us(max/avg)` (vuelta COMPLETA, incluye spike OTOS) + `scan_us` (barrido) impresos a 4 Hz. |
| **F1a** | `-DDOWN_ADC_FAST` / `down_adcfast` | averaging=1: barrido ~717µs → ~205µs. **Que el bit white NO flickee** sobre el path `dm_update`. |
| **F1b** | `-DDOWN_ADC_DUAL` / `down_adcdual` | dual-ADC: ~205µs → ~126µs. **Comparar el delta crudo carpet/blanco del MISMO mux por ADC1 vs ADC2** (offset por-ADC; la calib individual lo absorbe porque el umbral se mide con el MISMO ADC del runtime). |
| **F2** | `-DDOWN_OTOS_FAST_I2C` / `down_otosfast` | OTOS: `loop_us(max)` con/sin el flag (esperado spike ~3-4 ms → <0.6 ms). 30 min de marcha: **0 fallos I²C espurios a 400 kHz** (si hay, bajar reloj). |
| **F4** | `-DDOWN_RELIABLE_GATE` / `down_reliable` | Titular `reliable_min_healthy` (≥24/32) y `reliable_min_sensors_for_vector` (3) con el robot quieto contando `healthy_count`. Robot levantado / sensor tapado → `data_valid=0` + geometría SELLADA a N/A. |
| **F5** | `-DDOWN_RX_HARDEN` / `down_rxharden` | Inyectar 0x21 en vivo → la cadencia de LINE_URGENT a CENTRAL **NO se interrumpe** (calib diferido). Saturar RX con basura → 0 comandos perdidos (CRC resincroniza). |
| todas | `down_rt_all` | F0+F1+F2+F4+F5 juntas (verifica que coexisten; medir el conjunto). |

## ⚠️ Notas de la revisión adversarial del glue (leer antes del banco)

1. **F4 — el env `down_reliable` con sus DEFAULTS ya SELLA** (§6.4-A: con `enabled=false`/pisos 0,
   `seal_geometry = !data_valid`). O sea: ya cambia la geometría del wire vs `[env:down]` ANTES de
   titular ningún piso. Es la semántica buscada (la fuente deja de mentir), pero **NO atribuir el
   "antes no sellaba, ahora sí" a los pisos** — lo causa el sellado de §6.4-A, activo con el gate.
   El único binario behavior-idéntico a competencia es `[env:down]` (sin el flag).
2. **F5 — GAP-5 (deuda conocida, NO bloqueante):** la 2ª boca de calib (`down_telemetry_serial.cpp`,
   comandos `CAL_*` por USB) **NO se difirió** — sigue bloqueando ~320 ms en `down_telemetry_tick()`.
   **Decisión de scope:** el monitor USB solo corre con host conectado = admin/banco/robot QUIETO,
   NUNCA en partido (sin USB nunca despierta) → el bloqueo ahí es aceptable. Si se quiere cerrar el
   §8.1 al 100%, diferir también esa boca (compartir el slot `rx_calib_defer`). Opcional, post-banco.
3. **F1 — el path `solo_mux`** (dual-ADC con placa de 1/3 muxes) es código muerto en producción
   (4 muxes → 2 pares, sin solo). Solo se ejerce si se prueba explícitamente la placa degradada.

## Criterio de cierre (lo cierra el EQUIPO, no Claude — regla #1)

Por fase: los números del cuadro de arriba medidos en banco + **0 escapes de borde falsos** en
marcha normal sobre carpet (el contrato de seguridad). Resultado en journal. Decidir para cada
fase: ¿se promueve a `[env:down]`/competencia o queda de banco/2027?

## Riesgos (formato coach)

- `risk-no-fix`: el lazo DOWN sigue con el barrido lento (~717µs) + el OTOS bloqueante robando
  3-4 ticks de línea → ventana ciega de borde a velocidad alta. Las mejoras lo atacan, pero sin
  banco no se promueven.
- `risk-fix`: cada `-D` cambia el binario → validar en banco antes de competir. El más sensible es
  F1b (dual-ADC: offset por-ADC) y F2 (400 kHz: errores I²C). F4 es fail-safe (sella de más, nunca
  de menos). Competencia `[env:down]` byte-idéntica mientras no se promueva nada.
- `tiempo`: ~1 sesión de banco por fase (F0 mide; F1/F2 son cortas; F4 titula umbrales; F5 stress RX).

## Atribución

Validación de diseño + módulos puros + glue gateado + esta TASK: Claude Opus 4.8 (Anthropic),
2026-06-16 (requested-by Gustavo Viollaz). Construye sobre el diseño previo de la misma sesión-IA.
Validación en banco = equipo humano.
