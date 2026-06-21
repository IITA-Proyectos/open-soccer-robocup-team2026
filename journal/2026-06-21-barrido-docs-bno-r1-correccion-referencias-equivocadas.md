# 2026-06-21 — Barrido del repo: corrección de referencias equivocadas al BNO de R1

## Qué se hizo
Tras el fix del heading de R1 (ver [journal del fix](2026-06-21-bno-heading-fix-config-flag-no-era-tof.md)),
se barrió TODO el repo con un workflow paralelo (5 agentes) buscando referencias FALSAS-HOY al
BNO de R1 (no funciona / desconectado / chip fallado / "0x29 muerto" / "los ToF lo congelan" /
"cristal dañado" / "R1 sin gyro") y se corrigieron en **`soccer-main`** (worktree canónico, rama
`main`), sin reescribir historia.

## La verdad reconciliada (anclada al código, no a una sola narrativa)
Había DOS narrativas en el repo y los propios agentes se dividieron entre ellas. La reconciliación
correcta es **dos BNO, dos historias** — no una:
- **Ambos BNO055 de R1 están SANOS**, @0x28, en buses SEPARADOS. **NO hay ningún BNO en 0x29**
  (0x29 es solo la dir de fábrica de los ToF VL53L7CX).
- **PRIMARIO** (`Wire2` 24/25, solo, sin ToF) = fuente de rumbo. Su "heading=0.0" reciente NO era
  chip/ToF/cristal: era el flag `bno_left_en=0` en EEPROM (de un `BNO_L_OFF` viejo) → la fusión lo
  marcaba DEAD → `fused=0.0`. Fix: `sensors_imu.cpp:284` fuerza `enabled=true`. **Confirmado en
  banco 2026-06-21** (`top_robot1_pri_rt`, ToF rangeando): `hdg` sigue el giro (−15.6 → 101.4).
- **SECUNDARIO** (`Wire` 18/19, con los ToF) = chip SANO pero en bus contendido → **centinela @1 Hz,
  fuera de la fusión** (`TOP_BNO_PRIMARY_ONLY`). El freeze histórico (journals 2026-06-02/03/08) SÍ
  era real, pero de ESTE (un BNO sano trasplantado congelaba idéntico → es el bus). La solución
  dual-bus (primario a `Wire2`) es la que lo blindó.
- La hipótesis "el rangeo de los ToF congela el BNO" (**TASK-223**) era **PISTA FALSA para el
  primario** (vive aparte; los ToF no lo tocan). Reconectar el HW (06-17) era necesario pero **no
  suficiente**: el flag stale mantenía el 0.0 hasta el fix de firmware (06-21).

## Inconsistencias surfaceadas (no homogeneizadas)
- **El workflow barrió `soccer-rt-top`, no `soccer-main`.** En esa rama NO existen el fix, los
  journals 06-20/06-21 ni `top_robot1_pri_rt` → el agente de síntesis (correctamente, para ESA
  rama) dijo "no hay forzado de enabled / no existe el env". Verificado contra el código de
  `soccer-main`: ahí el fix SÍ está (`sensors_imu.cpp:284`) y el env existe (`platformio.ini:639`).
  ⚠️ **`soccer-rt-top` está atrasada** (código sin fix + docs viejas) — pendiente que el equipo
  decida si se rebasa/mergea o se descarta esa worktree.
- Los 5 agentes se dividieron: 2 adoptaron "contención I²C, resuelta por Wire2"; 2 adoptaron "flag
  EEPROM, ToF pista falsa". Ambas tienen razón EN PARTE (secundario vs primario). No se eligió una
  en silencio: se documentó la reconciliación de las dos.

## Archivos corregidos en `soccer-main`
Código/firmware (comentarios; sin cambios de lógica):
- `src/top/sensors_imu.cpp` — comentario del cristal (hipótesis refutada) + nota del primario-solo.
- `platformio.ini` — `top_robot1_oscint`, `top_robot1_pri` (boot-check ✅), `top_robot1_pri_rt_notof`
  (TASK-223 = pista falsa), y comentarios de diags `diag_top_bno` / `diag_bno_left` / `diag_bno_tof`
  / `diag_bno_dual_live` / `diag_bno_addr_check` (este último marcado OBSOLETO: arquitectura 0x29).

Docs canónicos / vivos:
- `docs/ESTADO-ACTUAL.md` — banner nuevo 2026-06-21 (la verdad del heading) arriba del de 06-17.
- `docs/FUENTES-DE-VERDAD.md` — nota 06-21 en la entrada BNO canónica.
- `docs/robot-variants/REFERENCIAS-POR-ROBOT.md` — "ambos desconectados/1 sano" → ambos sanos.
- `docs/competencia/POSTER.md` — fila "heading se congela (problema abierto)" → RESUELTO + causa real.
- `docs/competencia/BOM-COSTOS-TEMPLATE.md` — "1 (0x29) fallada, corre con 1 sano" → 2 sanos.
- `docs/competencia/assets/diagramas.md` — "1 BNO, 0x29 muerto" → 2 BNO, primario aparte.
- `docs/BACKLOG-INCHEON.md` — F5 matizada (riesgo no activo en competencia hoy).
- `docs/competencia/assets/drafts/fig_localizacion_fusion.svg` y `fig_r1_vs_r2.svg` — labels
  "BNO DESCONECTADOS / 0x29 muerto / sin gyro" → estado real.
- `software/teensy/Soccer 2026/_deprecated/README.md` — fila `diag_top_bno` (arquitectura superada).

## Historia NO reescrita (deliberado)
Los banners dated viejos de `ESTADO-ACTUAL` (06-10/06-12) y los journals 06-03/06-08 quedan como
registro; el banner nuevo arriba los supera. El journal 06-08 (contención del secundario) NO se
marca refutado: su diagnóstico era correcto para el secundario.

## Pendientes (los cierra el equipo)
- ⚠️ **SVGs `drafts/` necesitan re-render + revisión visual** antes de cualquier uso (regla
  `rcj-diagramas-poster`); solo se corrigió el TEXTO de los labels falsos.
- 🟡 **Posible error de roles en `fig_r1_vs_r2.svg`**: dice "R1: arquero / R2: delantero", al revés
  de `ESTADO-ACTUAL` (Elías+R1=delantero, Virginia+R2=arquero). NO se tocó (fuera del scope BNO) —
  revisar.
- `soccer-rt-top` atrasada (sin el fix ni estas correcciones) — decidir rebase/merge/descarte.
- Cierre formal de TASK-216 / TASK-223 (banco, equipo humano).

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
