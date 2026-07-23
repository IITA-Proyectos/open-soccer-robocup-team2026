---
title: "OpenMV N6 / PAG7936 — límites físicos del sensor y migración de API a firmware 5.0.0"
date: 2026-07-23
author: "Claude (Anthropic — Claude Opus 4.8)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: vivo
tipo: referencia
area: vision
---

# PAG7936 / OpenMV N6 — lo que el sensor puede y lo que no

> **Por qué existe este doc.** Tres de los valores de "calibración" que los 4 scripts
> de cámara tienen hardcodeados **nunca se aplicaron**. El firmware los recorta o los
> ignora **en silencio**, sin excepción ni warning. Este doc deja escritos los límites
> reales, medidos en la placa y verificados contra el código fuente, para que nadie
> vuelva a calibrar contra números imposibles.
>
> Medido el 2026-07-23 en la cámara **trasera del ROBOT1** (OpenMV N6, sensor PAG7936,
> firmware **5.0.0**, MicroPython v1.28.0-49, STM32N657X0).

## 1. Los tres techos

| Parámetro | Techo real | Qué pasa si pedís más |
|---|---|---|
| **Ganancia** | **24,08 dB** (= 16× lineal) | Se clampea al techo, **sin error** |
| **Exposición** | **8.248 µs** a 120 fps | Se clampea, **sin error** |
| **Balance de blancos** | *no existe* | El valor se **ignora por completo** |

### Ganancia — 3,15 a 24,08 dB

```c
// drivers/sensors/pag7936.c:152-157
#define PAG7936_MIN_AGAIN (1472)    // 1.4375x  ->  3.15 dB
#define PAG7936_MAX_AGAIN (16384)   //    16x   -> 24.08 dB
// :755
gain = IM_CLAMP(gain, PAG7936_MIN_AGAIN_REG, PAG7936_MAX_AGAIN_REG);
```

Convención: **dB = 20·log₁₀(lineal)** (`pag7936.c:769` get, `:754` set).

| Lineal | dB | |
|---:|---:|---|
| 1,4375× | 3,15 | **piso** |
| 2× | 6,02 | |
| 4× | **12,04** | ← el valor de los scripts de producción |
| 10× | 20,00 | |
| 16× | **24,08** | **techo** |

**Regla:** ×2 = **+6 dB**. ×10 = **+20 dB**. Los dB **no se multiplican** — el ejemplo
oficial *Sensor Manual Gain Control* de OpenMV hace `gain_db × GAIN_SCALE`, que sobre
este sensor no hace nada útil (o clampea al techo).

> ⚠️ **Los 4 scripts de producción corren a 12,04 dB = 4×, un cuarto del máximo.**
> Subirlos a 24 dB da **+12 dB (4× más señal) sin costar un solo fps ni blur.**
> Único costo: más ruido → subir `pixels_threshold` de `find_blobs` si aparecen
> falsos positivos chicos. Es la mejora de visión más barata identificada.

### Exposición — el techo es el tiempo de trama, no el silicio

Cadena completa, verificada en `drivers/sensors/pag7936.c` (tag v5.0.0):

| Paso | Código | Valor |
|---|---|---|
| `reset()` fija el framerate | `:541` → `HD_FPS_MAX` (`:182`) | **120 fps** |
| tiempo de trama | `:645` → `FT_CLK / framerate` = `1000000/120` | 8.333 µs |
| clamp + división entera | `:786` → `(8333 − 80) / 8` | 1.031 líneas |
| readback × 8 | `:801` | **8.248 µs** |

La cuenta **reproduce el valor medido exactamente**. Y `get_exposure_us()` no devuelve
lo que le escribimos: lee `AE_EXP_LINE_NUM`, la exposición que el sensor **realmente
aplicó** — por eso sirve como evidencia.

**Para subir el techo: bajar el framerate.**

```python
sensor.set_framerate(60)     # pag7936.c:919 -> py_csi.c:1346
```

| fps | techo exposición | × luz | costo |
|---:|---:|---:|---|
| **120** (default) | 8.248 µs | 1,00× | — |
| 60 | 16.584 µs | 2,01× | blur ~18 px @1 m/s |
| 30 | 33.248 µs | 4,03× | blur ~35 px → la pelota se vuelve una raya |
| 15 | 66.584 µs | 8,07× | inutilizable para jugar |

> Es una palanca de **banco**, no de partido. Un blob de pelota mide ~26 px; a 30 fps
> el arrastre lo supera y se rompen el área y el centroide de `find_blobs`.

**Tres trampas de `set_framerate()`** — si no se respetan, parece que no funciona:

1. **Orden.** `configure()` (`:665-675`) **no** re-aplica la exposición pedida: re-clampea
   la *actual* contra el `frame_time` nuevo. Hay que fijar el framerate **antes** y volver
   a llamar `set_auto_exposure()` **después**.
2. **`sensor.reset()` lo borra** (`omv_csi.c:304` → 0; `pag7936.c:541` → 120).
3. **`sensor.get_framerate()` lanza excepción**, no sirve de readback. El único readback
   válido es **`get_exposure_us()`**.
4. **Nunca `set_framerate(0)`** — `omv_csi.c:645` divide por él.

### Balance de blancos — no existe en este sensor

```c
// drivers/sensors/pag7936.c:806-809
static int set_auto_whitebal(omv_csi_t *csi, int enable, float r, float g, float b) {
    csi->stats_enabled = enable;
    return 0;                      // <- los tres gains se DESCARTAN
}
```

`sensor.set_auto_whitebal(False, rgb_gain_db=(...))` **no escribe ningún registro**.
Los seis decimales de los scripts de producción nunca hicieron nada en esta cámara.

**Consecuencia:** si el canal **A** (rojo/verde) del LAB viene corrido, el balance de
blancos **no es la palanca** para corregirlo — hay que recalibrar los thresholds.

## 2. Migración de API a firmware 5.0.0

El cambio que rompe más código: **los blobs y `get_statistics()` pasaron de clases a
attrtuples**. Sus campos son **atributos**, no métodos.

| | fw 4.8.1 | fw 5.0.0 |
|---|---|---|
| blob | `b.cx()` `b.rect()` `b.pixels()` | `b.cx` `b.rect` `b.pixels` |
| estadísticas | `st.l_mean()` | `st.l_mean` |

Con la forma vieja en 5.0.0: `TypeError: 'int' object isn't callable`.

**Shim compatible con las dos versiones** (usado en `main-r1-trasera-fw5-bringup.py`):

```python
def _bv(v):
    return v() if callable(v) else v
# uso:  _bv(blob.cx)   _bv(img.get_statistics().l_mean)
```

> ⚠️ **Nunca indexar el blob por número.** El índice 4 es `pixels` en 4.8.1 y `cx` en
> 5.0.0 → devuelve el número equivocado **sin tirar error**. Siempre por nombre + `_bv()`.

**Funciones de dibujo:** en 5.0.0 exigen la geometría en **una tupla**.

```python
img.draw_line((x0, y0, x1, y1), color=c)    # 4 enteros sueltos -> TypeError en 5.0.0
img.draw_cross((x, y), color=c)             # idem
img.draw_rectangle(_bv(blob.rect), color=c) # rect() ya es tupla: sirve en ambas
```
La forma-tupla funciona en **las dos** versiones.

**`find_blobs`:** en 5.0.0 todo salvo `thresholds` es **keyword-only**. Los scripts del
equipo ya los pasan por nombre → compatibles. **Regla: nunca pasarlos posicionalmente.**

**Lo que NO cambió:** `pyb.LED`, `pyb.UART`, `sensor.reset/set_pixformat/set_framesize/
set_hmirror/set_vflip/snapshot` y las tres `set_auto_*` corren igual en 5.0.0.
El módulo legacy `sensor` **sigue funcionando** — `csi` no es obligatorio.

> **`csi` no resuelve nada de lo de arriba.** Los techos son del sensor y su framerate,
> no de la API. Además el repo registra que `csi` dio **preview negro** en esta placa en
> 4.8.1 (`cameraFront-pack/firmware/openmv/cam-frontal-n6.py:17-19`); en 5.0.0 no se
> probó. Migrar es tarea aislada para cuando las 4 cámaras estén en la misma versión.

## 3. La regla que evita que esto vuelva a pasar

**Un `set_auto_*` sin su `get_*` impreso al lado no se commitea.**

```python
sensor.set_auto_gain(False, gain_db=X)
sensor.set_auto_exposure(False, exposure_us=Y)
print("CAL REAL -> gain %.2f dB  exp %d us"
      % (sensor.get_gain_db(), sensor.get_exposure_us()))
```

Todos los clamps de este sensor son **silenciosos**: `IM_CLAMP` no devuelve error y
`set_auto_gain` solo hace un `printf` que se pierde (`py_csi_ng.c:544-549`). Sin el
readback al lado, una calibración imposible sobrevive meses sin que nadie lo note —
que es exactamente lo que pasó con `exposure_us=100328`.

Mejor todavía: **dejar los autos correr, LEER lo que encontraron y congelar ESO**
(patrón implementado en `AUTO_CALIB` de `main-r1-trasera-fw5-bringup.py`). Así los
valores congelados son alcanzables **por construcción**.

## 4. Medición de referencia (banco 2026-07-23, escritorio de noche)

Con ganancia y exposición **ambas en el techo físico**:

```
ganancia : pedido 24.08 dB -> real 24.08 dB  OK
exposicion: pedido 8248 us -> real 8328 us  OK
L escena ≈ 19-20   (máx 95)   ·   loop a ~7-8 fps
```

El **lazo automático, decidiendo solo, también eligió el techo en ambas** — firma de
falta de luz, no de configuración.

> ⚠️ **Error de método a no repetir:** comparar el **L medio de la escena** contra el
> threshold de la **pelota** es inválido. El threshold `naranja_threshold =
> (30, 61, 39, 70, 20, 50)` se evalúa sobre los píxeles **de la pelota**, y lo que la
> separa del verde de cancha **no es L: es el canal A** (39-70 = fuertemente rojo;
> el verde tiene A negativo). Usar la sonda LAB del centro
> (`PROBE_LAB` en el script de bring-up), no la media global.

## 5. Pendiente de medir

| # | Qué | Cómo | Estado |
|---|---|---|---|
| 1 | LAB real de la pelota naranja con luz de laboratorio | sonda `PROBE_LAB` con la pelota en el cuadrito | **pendiente** (2026-07-24) |
| 2 | Lux del laboratorio y de una cancha | app de celular, anotar | pendiente |
| 3 | Si el silicio integra más de 1.030 líneas | `set_framerate(60)` + `get_exposure_us()` | **NO CONFIRMADO** — datasheet no público |
| 4 | Sensor/firmware de las **otras 3 cámaras** | `sensor.get_id()` + banner del IDE | pendiente — **el repo no lo registra en ningún lado** |

⚠️ **No tocar las otras 3 cámaras hasta cerrar el punto 4.** Si alguna tiene otro sensor,
`exposure_us=100328` puede ser legítimo ahí y cambiarlo a ciegas rompe lo que anda.

## Referencias

- Fuente: [`openmv/openmv` tag v5.0.0](https://github.com/openmv/openmv/tree/v5.0.0) — `drivers/sensors/pag7936.c`, `modules/py_csi*.c`, `common/omv_csi.c`
- Script de bring-up: `hardware/electronics/camaras-openmv/main-r1-trasera-fw5-bringup.py`
- Journal: `journal/2026-07-23-camara-trasera-r1-bringup-fw5.md`
