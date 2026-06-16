# 2026-06-16 — F3 cableado: detección temprana de línea por VECINO FÍSICO (gateado)

## Contexto

Continuación de la reingeniería RT de DOWN (ver `2026-06-16-reingenieria-rt-down-validada-y-programada.md`).
Esa sesión dejó F0/F1/F2/F4/F5 cableados y F3/F6 como **módulos puros listos pero sin cablear al
loop vivo** (decisión: post-Incheon). Gustavo pidió "seguir avanzando en la codificación de lo que
falte". El ítem de mayor valor + más barato + host-testeable era **F3** (la palanca "temprano" del
diseño, §4 de `ARQUITECTURA-LAZO-DOWN-RT.md`), así que se cableó.

> ⚠️ **El push pedido ("push de todo el análisis + merge a main") ya estaba hecho**: el commit
> `9d6acc7` (reingeniería RT DOWN) ya estaba en `main` y en GitHub (`origin/main`) al empezar la
> sesión. No había nada que pushear de esa tanda. Esta entrada documenta SOLO el trabajo nuevo (F3).

## Qué se hizo

**Glue gateado `-DDOWN_EARLY_EVIDENCE` en `dm_update` (`src/shared/down_model.cpp`):**

- Builder cacheado `dm_phys_neighbors()` que arma UNA vez la LUT de vecino FÍSICO (`line_neighbors.h`)
  desde `SENSOR_POS` (mismo patrón que `dm_outer_radius_mm`). Sólo para el anillo completo (n==32).
- Justo tras el `lf_spatial_filter` (vecino de ÍNDICE), se **UNEN a `validated[]`** los blancos que
  tienen un PAR físicamente adyacente también blanco (`ln_adjacent`, simétrico → ambos del par entran,
  nunca un punto suelto). Reusa los helpers ya host-tested; no reimplementa geometría.

**Por qué importa (el bug que ataca):** el filtro espacial histórico usa el vecino de ÍNDICE (i±1),
pero en esta placa hay pares de índice contiguos físicamente LEJOS (idx 7↔8 = 141 mm; 23↔24 = 116 mm;
31↔0 = 102 mm). Un blanco REAL cuyo único vecino-de-índice quedó lejos cae como "aislado" y se
descarta — el modo de falla del arquero sobre la línea de fondo. Con la unión física, el aviso
(`line_present` / `sensors_on_line` / `escape_angle`) sale ANTES, sin esperar los 6 sensores del
`imminent`, y SIN tocar el wire-contract (CENTRAL lee los mismos campos que ya recibe; §4.2).

**Por qué es seguro:**
- **ESTRICTAMENTE ADITIVO**: sólo prende `validated[i]`, jamás lo apaga → `sensors_on_line` nunca
  BAJA respecto de hoy → el freno duro (`EV_IMMINENT_EXIT`, `sensors_on_line>=imminent_depth`) nunca
  se dispara MÁS TARDE. No degrada el fail-safe probado.
- Las exclusiones de salud / débil / saturación de abajo se aplican igual a los agregados.
- Todo bajo `#ifdef DOWN_EARLY_EVIDENCE` → con el flag OFF (competencia `[env:down]`) el binario es
  **byte-idéntico**.

**Envs nuevos (`platformio.ini`):** `down_earlyev` (banco, extiende `down_loopmon`); `test_native_earlyev`
(host-test del path gateado). F3 agregado a `down_rt_all` (ahora F0+F1+F2+F3+F4+F5).

**Test nuevo `test/test_down_early_evidence/`:** el MISMO archivo verifica AMBOS lados con un `#ifdef`:
caso `white={5,7}` (físicamente adyacentes ~18 mm, pero aislados por índice — idx 6 en el medio lee
carpet) → con el flag `line_present=1, sensors_on_line=2, escape válido`; sin el flag `line_present=0`
(byte-identidad). Controles: un blanco aislado puro NO dispara (ni con el flag); un cluster contiguo
normal se detecta igual (la unión no rompe lo histórico).

## Verificación (host + compilación; NADA en banco — regla #1)

- **Gate ON** (`-DDOWN_EARLY_EVIDENCE`, g++ Webots): `test_down_early_evidence` 3/3 PASS.
- **Gate OFF** (suite completa `scripts/run-host-tests.sh`, g++ Webots): **88 suites / 1186 tests /
  0 fallos (exit 0)** — el test nuevo corre en modo gate-OFF y confirma `line_present=0` (byte-identidad)
  + cero regresión en el resto.
- **Compila** `pio run`: `down` (competencia, SUCCESS), `down_earlyev` (SUCCESS), `down_rt_all` (SUCCESS).

## Lo que NO se hizo (honestidad, regla #1)

- **NADA probado en banco.** Lo cierra el equipo → **TASK-309 (fila F3)**: robot a
  velocidad conocida cruzando la línea → medir **penetración_mm al aviso temprano vs inminente** +
  **0 falsos positivos** en marcha normal sobre carpet. Re-confirmar que `imminent_depth=6` mantiene
  el mismo timing de freno duro.
- **Consumo en CENTRAL** (`world_model_get_escape_angle()` + uso del vector temprano en la fase ESCAPE
  del arquero, §4.4) sigue pendiente — es la otra mitad de F3, en la placa CENTRAL, post-Incheon. F3
  acá deja la DATA disponible más temprano; CENTRAL todavía no la consume distinto.
- **F6** (pizarra seqlock / IntervalTimer de línea) sigue siendo módulo puro, sin cablear.

## Atribución
Glue F3 + envs + test + docs + esta entrada: Claude Opus 4.8 (Anthropic), 2026-06-16
(requested-by Gustavo Viollaz). Sobre el diseño y los módulos puros de la misma sesión-IA (2026-06-15/16).
