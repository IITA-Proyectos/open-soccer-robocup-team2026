# 2026-07-23 — Cierre de Incheon y cambio de frame: el repo pasa a MODO APRENDIZAJE

**Autor:** Claude (Anthropic — Claude Opus 4.8), sesión con Gustavo Viollaz (@gviollaz).
**Pedido de:** Gustavo, 2026-07-23.

## Qué pasó

**La competencia de Incheon terminó** (se jugó del 30 de junio al 6 de julio de 2026).
Gustavo lo informa hoy y pide tres cosas concretas:

1. Que quede **registrado en el repo** que Incheon terminó.
2. **Cambiar el modo de trabajo** a modo aprendizaje: investigar qué falló, corregirlo,
   entenderlo. Y que **ahora trabaja él**, investigando y aprendiendo — ya no los alumnos.
3. **Limpiar las inconsistencias de documentación** para que el repo quede sano a futuro.

Más las **cosas a hacer** que reportó (ver abajo).

## Hueco en el registro — hay que saldarlo

El journal cortaba el **`2026-07-04`**
(`2026-07-04-arqueromix-pateo-recto-y-corte-retro-checkpoint-9.md`). Entre esa fecha y hoy
**no hay nada**: los días de torneo y el post-mortem no están en el repo.

⚠️ **Resultados, partidos y crónica de Incheon: PENDIENTES de cargar por Gustavo.**
Esta entrada NO los inventa. Todo lo técnico de abajo viene del **reporte verbal** del
2026-07-23 y está marcado como tal.

## Lo que no funcionó (reporte verbal de Gustavo — root-cause en curso)

### (A) Cámaras traseras — no las supieron calibrar
Sospecha inicial: **el firmware de las cámaras no se actualizó**.
Plan acordado: actualizar el firmware de **la trasera del ROBOT1 primero**; si anda bien,
recién ahí las otras tres.

### (B) Sensores de línea / odometría de piso (placa DOWN)
Síntoma textual: **"pasaba de no detectar a detectar falsas líneas"**. No encontraron un
umbral intermedio que funcionara. Hipótesis de Gustavo: *"no sé si será que el cambio se
hacía por números enteros"*.
Pedido explícito: **un programa de calibración más simple**.

### (C) Sensores ToF
Tampoco los pudieron hacer andar bien. Hay trabajo previo documentado
(`docs/firmware/TOF-ZONAS-TOP-MONITOR-ANALISIS.md`, piloto 8×8, TASK-225 a TASK-228) sin
conclusión de que sirvan hoy.

## Qué se hizo en esta sesión

### Cambio de frame (commiteado)

- **NUEVO** `docs/MODO-APRENDIZAJE.md` — doc ancla del modo. Define qué cambia (quién
  implementa, riesgo aceptable, criterio de éxito), la escala P0/P1/P2 nueva, el rango
  `TASK-400+`, las 3 líneas abiertas y las 7 reglas que **no** cambian.
- `CLAUDE.md` — banner de modo aprendizaje arriba de todo; frame del coach actualizado
  (ahora Claude **sí** implementa, en par con Gustavo); estrategia multi-temporada con
  Incheon tachada; escala P0/P1/P2 redefinida; protocolo de sesión con `MODO-APRENDIZAJE.md`
  como lectura obligatoria; regla no-negociable #1 con el matiz del lazo corto.
- `docs/ESTADO-ACTUAL.md` — banner arriba: todo el contenido de abajo es **snapshot
  pre-torneo**, historia técnica válida pero **instrucción vencida**. Las moratorias y los
  "no tocar" de competencia quedan **levantados**.
- `docs/FUENTES-DE-VERDAD.md` — fila nueva para el modo de trabajo.

### Escala P0/P1/P2 — la vieja estaba anclada al torneo

| | Antes | Ahora |
|---|---|---|
| **P0** | Bloqueante para Incheon (no compite / desclasifica) | **Bloquea el aprendizaje** — no da datos, no se puede medir, o se pierde info irrecuperable |
| **P1** | Impacto alto en partidos | **Deuda que va a volver a morder** — anda a medias y nadie sabe por qué |
| **P2** | Capitalizable a 2027 | Igual |

### Inconsistencia de documentación corregida

**`hardware/electronics/camaras-openmv/main.py` NO EXISTE.** `FUENTES-DE-VERDAD.md` lo
declaraba canónico. Los archivos reales son **cuatro**, uno por cámara:
`main-r1-frontal.py`, `main-r1-trasera.py`, `main-r2-frontal.py`, `main-r2-trasera.py`
(rename de Elías 2026-06-21, commits `c4510b9`/`a888217`/`2f19b2c`). Corregido.

Se detectó tirando de un `Read` que falló durante la sesión de depuración de ganancia de
las cámaras OpenMV — o sea, **la inconsistencia era real y estaba activa**: cualquier
sesión que siguiera el doc canónico no encontraba el archivo.

Hay una auditoría más amplia en curso (barrido de referencias a Incheon como futuro,
rutas rotas en `FUENTES-DE-VERDAD`/`ESTADO-ACTUAL`/skills, y delta entre los thresholds
LAB documentados y los que corren). Sus resultados se cargan en una entrada aparte.

## Contexto lateral útil (misma sesión, tema cámaras)

Depurando el ejemplo *Sensor Manual Gain Control* de OpenMV se verificó contra el código
fuente del firmware (`drivers/sensors/pag7936.c`, tags v4.8.1 y v5.0.0):

- **El PAG7936 de la N6 tiene un rango de ganancia de 1.4375× a 16×**, o sea
  **3.15 dB a 24.08 dB**, y **clampea en silencio** (`pag7936.c:152-157`, `:755`).
  Una H7 con OV5640 llega a 36 dB — por eso el ejemplo oficial, escrito para esos sensores,
  no sirve tal cual en la N6.
- **Convención dB = 20·log10(lineal)** (`pag7936.c:769` GET, `:754` SET). El
  `gain_db=12.041200` de los 4 scripts de producción es exactamente **20·log10(4)** = 4×.
  ×2 = +6 dB · ×10 = **+20 dB**.
- El ejemplo oficial multiplica **decibeles** por `GAIN_SCALE` → pedir 10× sobre 12 dB da
  120 dB ≈ un millón de veces, que clampea a 16×. Por eso cambiar el número no hacía nada.
- **`csi` ya existe en v4.8.1** — que `import csi` funcione **no** indica que el firmware
  sea 5.x. (Corrige una afirmación previa de esta misma sesión.)
- El firmware que trae cacheado el IDE 5.0.0 de esta máquina es la **4.7.0**
  (`%APPDATA%/OpenMV/openmvide/firmware/settings.json:2`) — **más viejo** que el 4.8.1 que
  el repo dice que tienen las cámaras. **Flashear con el default sería un downgrade.**

Esto es directamente relevante para la línea (A).

## Estado

- Cambio de frame: **hecho y commiteado**.
- Auditoría de inconsistencias + root-cause de (A)/(B)/(C): **en curso**.
- TASKs `TASK-400+`: se cargan cuando cierre la auditoría.
- **Nada de esto está probado en hardware.** Es cambio de documentación y de frame.

## Para quien retome

- Frame vigente: [`docs/MODO-APRENDIZAJE.md`](../docs/MODO-APRENDIZAJE.md).
- **No leas `docs/BACKLOG-INCHEON.md` como lista vigente** — es del torneo. Sus ítems hay
  que reevaluarlos bajo la escala nueva.
- Las TASKs de competencia que quedaron abiertas **no se cierran de oficio**: se revisan una
  por una y se marcan como *vigente en modo aprendizaje*, *histórica* o *muerta*.
