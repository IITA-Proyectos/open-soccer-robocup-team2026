# 2026-07-23 — Bring-up de la cámara trasera del ROBOT1 en firmware 5.0.0

**Autor:** Claude (Anthropic — Claude Opus 4.8), sesión de banco con Gustavo Viollaz (@gviollaz).
**Hardware:** OpenMV N6 · sensor PAG7936 · firmware **5.0.0** · COM9 · unidad `F:`

## Punto de partida

Gustavo reflasheó la cámara trasera del R1 el 2026-07-22 → el filesystem quedó
formateado y sin script de visión (solo el blinky de fábrica). Objetivo de la sesión:
volver a dejarla corriendo y entender por qué nunca se pudo calibrar.

## Estado al cierre

✅ **El script corre limpio en firmware 5.0.0.** Sin crashes.
❌ **No detecta todavía** — falta luz. Se retoma mañana con luz de día en el laboratorio,
con la pelota, usando la sonda LAB.

Script vivo: `hardware/electronics/camaras-openmv/main-r1-trasera-fw5-bringup.py`
(idéntico al que quedó en la cámara). El `main-r1-trasera.py` original **no se tocó**.

## Hallazgo principal: tres de las "calibraciones" nunca se aplicaron

Los 4 scripts de cámara tienen tres líneas de calibración. Verificado contra el fuente
del firmware y **medido en la placa**, dos no hacen lo que dicen y una se ignora entera.

| Línea | Qué dice | Qué pasa de verdad |
|---|---|---|
| `set_auto_gain(gain_db=12.041200)` | ganancia 4× | ✅ **es la única que funciona** |
| `set_auto_exposure(exposure_us=100328)` | 100 ms | ❌ el sensor aplica **8.248 µs** (clamp silencioso) |
| `set_auto_whitebal(rgb_gain_db=(...))` | balance de blancos | ❌ **se descarta entero** (`pag7936.c:806-809`) |

Detalle completo, con los techos y la cadena de código que los produce, en
[`docs/firmware/PAG7936-LIMITES-Y-API-FW5.md`](../docs/firmware/PAG7936-LIMITES-Y-API-FW5.md).

**Sobre el `100328`: no fue invento de nadie.** Salió de un `get_exposure_us()` real —
el bloque comentado de las líneas 214-221 de los scripts es ese patrón de copiar-pegar.
Pero `100328/8 = 12541` líneas exactas requieren ~9,96 fps, que no es un framerate
entero: **este driver no pudo generarlo**. Vino de otra cámara, anterior a la N6, y se
rompió en silencio al migrar. **No es negligencia del equipo: es un clamp que no avisa.**

## Cronología de lo que se arregló

1. **Crash en `draw_line`** — fw 5.0.0 exige la geometría en una tupla.
2. **Crash en los accesores del blob** — en 5.0.0 son atributos, no métodos
   (`b.cx` y no `b.cx()`). Resuelto con el shim `_bv()`, compatible con 4.8.1 y 5.0.0.
3. **Crash en `get_statistics().l_mean()`** — mismo cambio de attrtuple. **Bug
   introducido por Claude** al agregar el print de diagnóstico, y no detectado antes
   porque el crash anterior tapaba el camino.
4. **Gamma invertido** — el realce de pantalla con `gamma=0.6` **oscurecía**
   (`L_ver=14` < `L_esc=19`). La convención es **gamma > 1 aclara**. Medido, no supuesto.

## Medición de banco (escritorio de noche)

```
ganancia : pedido 24.08 dB -> real 24.08 dB  OK
exposicion: pedido 8248 us -> real 8328 us  OK
L escena ≈ 19-20 (máx 95) · loop ~7-8 fps · cámara 73-86 fps
```

**El lazo automático, decidiendo solo, eligió el techo en las dos.** Cuando el AE y el
AGC se pegan al máximo simultáneamente, el sensor está diciendo que no hay luz. Es la
confirmación más independiente que se puede pedir.

## Error de método de Claude — corregido

Comparé el **L medio de la escena** (20) contra el threshold de la **pelota** (L 30-61)
y concluí que no iba a detectar. **Ese razonamiento es inválido:** la media de un
escritorio oscuro no dice nada de los píxeles de la pelota, y el propio histograma
mostraba máx = 95.

Peor: lo que separa el naranja del verde de cancha **no es L, es el canal A**
(`naranja_threshold` pide A entre 39 y 70 = fuertemente rojo; el verde tiene A negativo).

Corrección implementada: **sonda LAB del centro** (`PROBE_LAB`). Dibuja un cuadrito
amarillo en el medio; se pone la pelota adentro y el print da su L/A/B real con
`[OK]`/`[FUERA]` por canal contra el threshold. **Esa es la medición que vale.**

## Qué se instrumentó en el script

| Flag | Default | Para qué |
|---|---|---|
| `AUTO_CALIB` | `True` | autos → **leer** → congelar eso + imprimir las 3 líneas para pegar |
| `PROBE_LAB` | `True` | sonda LAB del centro (L/A/B de la pelota vs threshold) |
| `DISPLAY_BOOST` | `True` | aclara **solo la pantalla**, después de `find_blobs` |
| `BOOST_AFECTA_DETECCION` | `False` | experimental: realzar antes de detectar (exige recalibrar) |
| `SET_FRAMERATE` | `0` | bajar fps para subir el techo de exposición (banco, no partido) |
| `DEBUG_PRINT` | `True` | `L_esc` / `L_ver` / fps / coordenadas |

`AUTO_CALIB` es el arreglo estructural: al congelar lo que el sensor **ya logró**, un
valor imposible como el `100328` se vuelve imposible por construcción.

## Para mañana — laboratorio, luz de día, con la pelota

1. Poner la pelota naranja adentro del cuadrito amarillo y anotar la línea `SONDA`.
2. Interpretación:
   - **A entra (39-70) y L entra** → el threshold sirve, era luz. Listo.
   - **A entra, L corto** → solo falta luz. Lámpara.
   - **A queda bajo con la pelota adentro** → el naranja no llega como naranja.
     Problema de color, no de brillo. **Sería el hallazgo grande** — explicaría por qué
     las traseras nunca calibraron. Sospechoso #1: el WB que nunca estuvo bajo control.
3. Medir los **lux** del laboratorio con el celular y anotarlos.
4. Anotar con qué `L` empieza a detectar de forma estable: ese número **es el margen de
   luz que le faltó al robot en Incheon**.

## Deuda abierta

- ⚠️ **La homografía de esta cámara sigue siendo la de la FRONTAL del R1** — las 4
  cámaras comparten la misma matriz, con el comentario `# Camara delantera robot 1` en
  las cuatro. Los centímetros que reporta están sesgados, sobre todo en X. Hay que
  medirla con la lona de grilla. Es independiente del problema de luz.
- ⚠️ **No se sabe qué sensor ni qué firmware tienen las otras 3 cámaras.** El repo no lo
  registra. **No tocarlas** hasta identificarlas (`sensor.get_id()` + banner del IDE).
- El loop corre a ~7-8 fps mientras la cámara entrega 73-86. El cuello es el
  procesamiento, probablemente el enmascarado de esquinas (100 `draw_line` por frame).
  Se puede cambiar por 2 `draw_rectangle` o recorte de ROI. **P2, después de la detección.**

## Respaldos

- `backup-camara.ps1` — script para respaldar una cámara **antes** de reflashearla.
  Detecta la unidad sola, copia todo y deja manifiesto con MD5.
- `backups/2026-07-23-r1-trasera-post-reflash/` — estado de la cámara antes de escribirle.

⚠️ **Las otras 3 cámaras todavía no están respaldadas.** Los scripts del repo son del
21 de junio y el torneo fue del 30/6 al 6/7 — si alguien las editó desde el IDE en esos
días y no commiteó, esa versión vive **solo** adentro de cada cámara. Respaldar antes
de tocarlas. **P0: se destruye información de forma irreversible.**
