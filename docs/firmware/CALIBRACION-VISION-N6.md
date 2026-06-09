---
title: "Calibración de visión OpenMV N6 — procedimiento de banco (TASK-022)"
date: 2026-06-03
status: vivo
audiencia: "Virginia — operativa en el banco / Incheon"
firmware-source: >
  PRODUCCIÓN (se copia como main.py en las 2 cámaras): hardware/electronics/camaras-openmv/main.py
  Kit de calibración: hardware/electronics/camera{Front,Back}-pack/firmware/openmv/calib-lab-n6.py
  (los cam-*-n6.py de los packs quedaron DEPRECADOS — ver banner ⛔ en sus headers)
environment: "OpenMV IDE (versión que liste 'OpenMV N6') → USB-C → ▶ Run"
author: "Claude Opus 4.8 (Anthropic), vía Claude Code"
requested-by: "Gustavo Viollaz (@gviollaz)"
---

# Calibrar las cámaras N6 para que el robot VEA — TASK-022

> **✅ CALIBRACIÓN LAB HECHA — banco 2026-06-09 (Gustavo).** Los 3 thresholds quedaron
> calibrados para la N6 (sensor PAG7936); detecta pelota + arcos. **Valores finales:**
> - **naranja** (pelota → 201): `(21, 67, 18, 79, -32, 127)`
> - **amarillo** (arco → 202): `(17, 70, -27, 14, 38, 111)`
> - **azul** (arco → 203): `(4, 38, -13, 57, -64, -4)`  *(azul ajustado 36→38 el 2026-06-09)*
>
> **Fuente única (producción, va en las 2 cámaras): `hardware/electronics/camaras-openmv/main.py`.**
> Si tuneás más, cambiá ahí y propagá al resto.
>
> ⚠️ **Igual conviene REHACERLO en Incheon** bajo la luz del venue (los LAB dependen de la
> iluminación). El procedimiento de abajo es para eso (~15 min repetibles, kit `calib-lab-n6.py`).

> ⚠️ **Hay que rehacer esto en Incheon** bajo la luz del venue (los thresholds LAB
> dependen de la iluminación). Por eso el objetivo es un proceso de ~15 min
> repetible, no una odisea. El kit `calib-lab-n6.py` está para eso.

## Antes de empezar (1 vez por cámara)

- OpenMV IDE actualizado (que liste "OpenMV N6") + firmware oficial N6 por DFU/USB-C.
- Cámara alimentada y con preview a color (init del módulo `sensor`, ya verificado).
- Hacé la calibración **con la cámara montada en el robot** (mismo ángulo/altura/luz
  que en partido). Calibrar en la mano y después montar = thresholds que no sirven.
- **Cada cámara se calibra por separado** (sensores e iluminación distintos).

---

## Paso 1 — Confirmar el UART (¿la N6 le llega al Teensy?)

Objetivo: saber qué `UART_PORT` de la N6 sale al `Serial` correcto del Teensy.

- Frontal → debe entrar al **Serial3** del TOP (conector U8).
- Trasera → debe entrar al **Serial5** del TOP (conector U9, pin 21).

1. Abrí el script de producción de esa cámara (`cam-frontal-n6.py` / `cam-trasera-n6.py`),
   `BRING_UP=True`, ▶ Run. En la consola del IDE tienen que salir paquetes
   `[201, X, Ypc, 202, ...]`.
2. Con el TOP flasheado (`diag_top_all` o `top_robot1`) mirando por USB, confirmá
   que el TOP cuenta packets de esa cámara (`cameras_packets_front/back`).
3. Si el TOP NO recibe: probá `UART_PORT = 1`, luego `2`, luego `3` (re-Run cada vez)
   hasta que el contador del TOP suba. **Anotá el valor que funcionó** en el script.
   - La trasera viene anotada como UART3→Serial5 ✅; confirmá igual.

> Si la imagen sale espejada/al revés, ajustá `HMIRROR` / `VFLIP` hasta que en el
> preview la pelota arriba esté arriba. (El flip puede diferir entre las 2 cámaras.)

---

## Paso 2 — Calibrar los 3 thresholds LAB (el corazón de TASK-022)

Herramienta: **`calib-lab-n6.py`** (NO transmite, no toca el script de competencia).

1. Abrí `calib-lab-n6.py` → ▶ Run (corre de RAM; **no** lo guardes como main.py).
2. En el framebuffer aparece un **cuadro blanco central** (la "sonda").
3. Por cada color (`TARGET = "naranja"`, luego `"amarillo"`, luego `"azul"` — re-Run):
   a. Poné el objeto (pelota / arco) **llenando el cuadro central**.
   b. Leé en la consola el `TUPLE sugerido` (es el LAB real del objeto + margen).
   c. Pegá ese tuple en `THRESHOLDS[TARGET]` (en el mismo `calib-lab-n6.py`), re-Run.
   d. Mirá el framebuffer: el **recuadro verde** tiene que rodear **SÓLO el objeto**
      y nada del fondo. Si agarra fondo → subí el threshold (achicá rangos / subí
      `MARGEN` al revés); si NO agarra el objeto → aflojá (ampliá rangos).
   e. Confirmá en consola: "threshold actual agarra: 1 blobs, mayor = N px" con N
      cómodamente por encima de `PIXELS_MIN` (pelota ≥20, amarillo ≥600, azul ≥300).
4. Cuando los 3 quedan limpios, **copiá los 3 tuples finales** a las constantes
   `NARANJA_THRESHOLD` / `AMARILLO_THRESHOLD` / `AZUL_THRESHOLD` del script de
   **producción** de esa cámara.

> Alternativa equivalente: el **Threshold Editor** del IDE (Tools → Machine Vision →
> Threshold Editor) sobre un frame en vivo. El kit es más rápido porque te da el
> tuple ya calculado y el feedback de blobs en el mismo loop.

---

## Paso 3 — Fijar exposición para competencia

Con autos ON la luz cambia y rompe los LAB. Para partido va todo fijo.

1. En el script de **producción**, pasá `BRING_UP = False`.
2. Ajustá `EXPOSURE_US` hasta que la imagen quede igual de buena que con autos
   (empezá en ~37000 y subí/bajá). Re-Run y verificá que los 3 colores SIGUEN
   detectándose con los thresholds del Paso 2.
3. Si al fijar exposición cambió el color → re-tocá los thresholds (Paso 2) **con
   `BRING_UP=False`**, así quedan calibrados para la condición real de partido.

---

## Paso 4 — Homografía (posición XY ≈ distancia)  [si hay tiempo]

Esto convierte pixeles a cm y hace que `(X, Y)` sea la posición real de la pelota
(X = izq/der, Y = distancia adelante).

> **➡️ Procedimiento completo y CONFIABLE (lona con grilla + tool de captura +
> solver de PC):** [`CALIBRACION-HOMOGRAFIA-XY-N6.md`](CALIBRACION-HOMOGRAFIA-XY-N6.md).
> Ese doc reemplaza el método viejo de "4 puntos a ojo": usa una lona con grilla
> de puntos que la cámara detecta sola (`calib-homografia-n6.py`) y un solver
> numpy en la PC (`solve_homografia.py`, validado). Más rápido, simple y repetible
> en Incheon.

Resumen: lona con grilla → `calib-homografia-n6.py` imprime correspondencias →
`solve_homografia.py` calcula `H_MATRIX` → pegar en `cam-*-n6.py` → medir
`CAM_HEIGHT_CM` → validar con pelota a 30/50/80/100 cm (<10% error) → ajustar
`CAMERA_UNIT_TO_MM` del TOP (`src/top/cameras_runtime.cpp`, hoy = 10.0; cm → 10.0).

> El Paso 4 es "calidad de distancia". Para que el robot **vea y persiga** la
> pelota alcanza con los Pasos 1–3; priorizá esos si el tiempo aprieta.

---

## Paso 5 — Guardar en la cámara

Cuando el script de producción detecta bien con `BRING_UP=False`:

- `Tools → Save open script to OpenMV Cam (as main.py)` → corre solo al energizar.
- Power-cycle y confirmá que arranca y transmite sin la IDE conectada.

---

## Qué es "calibrado OK" (criterios de aceptación)

- [ ] El TOP cuenta packets de **ambas** cámaras (Paso 1).
- [ ] Con `BRING_UP=False` (exposición fija), las 3 detecciones (pelota/amarillo/azul)
      salen estables al mover el objeto por el campo de visión.
- [ ] El recuadro verde rodea sólo el objeto correcto, sin falsos del fondo.
- [ ] (Opcional) Y ≈ distancia con <10% error a 30/50/80/100 cm.
- [ ] `main.py` guardado en ambas N6; arrancan y transmiten tras power-cycle.

> Claude **no cierra TASK-022** (es hardware/banco). Cuando se cumplan estos
> criterios, lo cierra el equipo y se actualiza `docs/ESTADO-ACTUAL.md`.

## Archivos

- Kit de calibración: `cameraFront-pack|cameraBack-pack/firmware/openmv/calib-lab-n6.py`
- Producción: `.../cam-frontal-n6.py`, `.../cam-trasera-n6.py`
- Contrato del protocolo cámara↔Teensy: [`CONTRATO-DATOS-CAMARAS.md`](CONTRATO-DATOS-CAMARAS.md)
- Análisis de fondo: `journal/2026-05-31-analisis-vision-n6-deteccion-protocolo.md`
- Memoria del bring-up N6: ver `vision-openmv-n6` (reglas `sensor`/`pyb.UART`).
