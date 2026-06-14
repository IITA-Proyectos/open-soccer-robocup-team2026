---
title: "Re-auditoría + expansión del TDP y POSTER (ES) + 5 figuras — sesión deliverables 2026-06-14"
date: 2026-06-14
author: "Claude (Anthropic — Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: completado
tipo: journal
scope: docs/competencia/ (entregables de competencia, versiones ES)
---

# Re-auditoría + expansión de los entregables de competencia (TDP + POSTER, ES)

## Qué pidió el equipo
Analizar el repo y la documentación existente del **TDP y el POSTER (versiones en español)**, y
**completarlos con las últimas actualizaciones, diagramas y todo lo necesario** para confeccionar el repo.
Decisiones de alcance acordadas con Gustavo: **re-auditoría completa + expansión** (no solo deltas) y
**crear/renderizar los diagramas que falten**.

## Cómo se hizo (4 workflows en secuencia + QA en el loop)
1. **Análisis paralelo (read-only):** minería de avances 2026-06-14, auditoría del TDP y del POSTER contra la
   rúbrica oficial 2026, inventario de diagramas y chequeo de consistencia.
2. **Producción de texto:** TDP, POSTER y propagación de consistencia (ONE-PAGER / RUBRICA-COBERTURA / BOM),
   cada uno con manifiesto aterrizado + skills de voz/honestidad, prohibición dura de inventar.
3. **Diagramas:** regenerar Fig.8/Fig.4/Fig.2 + crear 2 figuras nuevas, con render matplotlib y auto-QA.
4. **Verificación adversarial:** jurado de rúbrica + verificador anti-invención + integridad/consistencia.

## Cifra de tests — ground truth de hoy
**858 tests / 61 suites / 0 fallos**, medido el **2026-06-14** con `scripts/run-host-tests.sh` usando el
**g++ de Webots** (no hay g++/pio en PATH — ver `memory/host-build-toolchain`). Subió de 834/60 (2026-06-13):
+24 tests / +1 suite, principalmente `test_top_config` (12, config EEPROM A2.1) + cambios en `test_telemetry_top`
y `test_cameras_fusion`. **Re-confirmado de forma independiente** por un agente de la fase de verificación
(re-corrió el runner: `Tests:858 Failures:0`). El pytest del monitor de banco (`tools/monitor-base`) es un
**ecosistema Python aparte = 116 tests** — NO se suma a los 858 C++.
Cadena de crecimiento canónica (= `TEST_MILESTONES` de `gen_figuras.py` + hoy):
`246 → 262 → 324 → 354 → 403 → 470 → 545 → 658 → 834 → 858`.

## Qué se incorporó (avances 2026-06-14 que faltaban en los entregables)
- **TOP→CENTRAL validado en banco** (`diag_central_rx_all`: SNAPSHOT 66 Hz, crc=0, seqGap=0, HEADING_VALID=1) →
  §1.7, §3.7, §4.4 + Fig.2 anotada. Honestidad: **100 Hz de diseño / 66 Hz medido**.
- **Zonas crudas 4×4 de cada ToF en telemetría** (campo `z` aditivo, schema sigue 2) → §1.3 + Fig. nueva.
- **Monitor TOP de salud** (`--top-salud`, validado en placa real) → §3.10 + Fig. nueva del árbol de salud.
- **Config persistente EEPROM (A2.1)**, **botón físico deshabilitado por default (fail-safe)**,
  **telemetría TOP v2 per-cámara**, **delantero R1 (giro 60→30, salida de línea)**, **arquero strafe con
  retroceso-al-arco** (FSM corrió en banco; conducta abierta, revisión 2026-06-15), **hallazgo OTOS R2 = binario
  equivocado** → §3.4/§3.5/§3.7/§3.10/§4.2 y POSTER Zona G (tabla de iteraciones, ahora con columna de madurez).
- **Boot del TOP de ~40 s → ~9,6 s** (carga del firmware-blob de los 4 ToF promovida a **1 MHz**, validada en banco
  2026-06-14 con >15 power-cycles sin *fallback*, **TASK-211**) → §1.3 + §4.4. Llegó al repo por un merge de la
  sesión paralela DURANTE esta corrida (HEAD avanzó 81af8cc → 9eef2d2; también se commitearon Fig.8/858 y el doc
  `TESTS-HOST-NATIVE-EXPLICADOS.md`).

## Drift corregido (claims viejos vs código vivo)
- Envs PlatformIO **83 → 82** (verificado `grep -cE '^\[env:'`).
- `MOTOR_INVERT` **{+1,−1,+1} → {+1,+1,+1}** en ambos robots (config_central.h:47/:97; recableado 2026-06-11) —
  el viejo pasó a historia.
- `MOTOR_MIN_PWM` **{70,70,42} → {70,70,107}** (valor final; {70,70,42} = paso intermedio 2026-06-08).
- ToF: producción lee **4×4 (16 zonas)**, el 8×8 quedó diferido.
- POSTER: tamaño **B1 (70.7×100) → A1 (84.1×59.4)** corregido en todo el doc (era riesgo de requisito DURO).
- FSM del arquero: agregado el estado **GOTO_LINE** (ASCII §3.4 + Fig.4 + caption del POSTER).
- BNO unificado a **"2 montados, 1 sano"**; cifra 858/61/0 + cadena canónica propagadas a los deliverables ES
  (TDP, POSTER, ONE-PAGER, RUBRICA-COBERTURA, BOM, ENTREVISTA-PREP, MEJORAS-PENDIENTES, USO-DE-IA).

## Expansiones de alto ROI (aterrizadas en el repo, no inventadas)
Batería 6800 mAh ≈ 50 Wh (§1.2) · subtotales de costo por subsistema (§1.8, percepción = 41%) · iteración del
loop TOP 6 Hz→~190k/s (§1.3) · level shifter TXS0102 + LIS3DHTR (§1.6) · 3 técnicas de motion lateral (§2.4) ·
dimensiones de cancha ↔ trilateración-vs-MCL (§2.5↔§3.5) · hitos fechados (§4.1) · 5ª lección "medí el Hz de
tu loop" (§4.2) · paths verificables de PCB/gerbers (§5.1) · packs + extract_pinout (§5.2).

## Figuras (5, con QA visual)
- **Fig.8** regenerada con el punto 858. **Fig.4** con GOTO_LINE. **Fig.2** anotada con el link validado.
- **NUEVAS:** `assets/drafts/fig_zonas_tof_4x4.png` (+ `gen_zonas_tof.py`) y `assets/drafts/fig_arbol_salud.png`
  (+ `gen_arbol_salud.py`). Valores de zonas rotulados "ILUSTRATIVOS, no captura de banco"; árbol de salud
  derivado de `health.py` real. Las 5 pasaron QA visual (legibles, sin texto encimado, honestas).

## Verificación adversarial — veredicto
- **Cero evidencia inventada** en TDP/POSTER (≈20 claims muestreados, todos verificados contra journal/código/BOM;
  `maturityMislabels: []` — nada de host disfrazado de banco).
- **Niveles de rúbrica (hoy, contenido):** TDP Software/Presentation = Excellent, Electrical = Excellent-borderline
  (ahora con "uso de recursos" explícito), Mechanical = Proficient (techo por specs de motor/rueda/CAD ausentes);
  bonus software = +1, CAD/PCB = parcial (falta STL del chasis 2026). POSTER: Data = el más fuerte (15 iteraciones
  con madurez), el cuello sigue siendo **EJECUCIÓN** (fotos, maquetar A1, traducir EN), no contenido.

## Pendiente del equipo (NO lo cierra Claude)
- **CRÍTICO — versiones EN desactualizadas:** `en/TDP.md` está en 717 y el resto del set EN en 658. Regenerar
  el set EN (pipeline de traducción) a **858/61/0** + cadena canónica, y correr corrector ortográfico EN. Existe
  `docs/competencia/assets/actualizar-cifra.py` (requiere g++ de Webots en PATH).
- **Re-correr el gate el día previo a entregar/grabar** y re-propagar la cifra del día (es "cifra viva").
- **Fotos** (8 faltan): robot armado, equipo Nacional 2025, banco decodificando WorldSnapshot, bodge de los 4 ToF,
  pila/standoffs, anillo de 32 sensores, antes/después.
- **Maquetar el A1 real** (Figma/Inkscape → PDF 300 dpi) y verificar legibilidad a 1.5 m.
- **Datos de banco/medición** que siguen como GAP honesto: set-points de los 6 MP1584, C-rating/peso/autonomía de
  la batería, specs del motor 2026, WHEEL_RADIUS, diámetro/peso del robot, CAD/STL del chasis 2026, espaciado de
  la pila. Validaciones de banco abiertas: visión LAB+homografía (TASK-022, bloqueante #1), trilateración
  (TASK-035), arquero strafe con retroceso (revisión 2026-06-15), config EEPROM, failover BNO.
- Docs de soporte que aún pueden tener cadenas/cifras viejas y conviene barrer con `actualizar-cifra.py`:
  `README.md`, `IA-VIBE-ENGINEERING-EVIDENCIA.md`, `CUESTIONARIO-DATOS-EQUIPO.md`, `VIDEO-GUION.md`.

> **Regla 1 (CLAUDE.md):** ninguna TASK de hardware se marca `done` en esta sesión. Lo "validado en banco" se
> tomó de los journals del equipo (Gustavo/María/Elías), no de una validación de Claude.
