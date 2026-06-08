---
title: "Calibración de la homografía (posición XY de pelota/arcos) — OpenMV N6"
date: 2026-06-07
status: vivo
audiencia: "Virginia / Elías — operativa en el banco e Incheon"
canonico-de: "procedimiento de calibración de homografía / coordenadas XY de las cámaras N6"
author: "Claude Opus 4.8 (Anthropic), vía Claude Code"
requested-by: "Gustavo Viollaz (@gviollaz)"
supersede: "el 'Paso 4 — Homografía' (stub) de docs/firmware/CALIBRACION-VISION-N6.md y el §3 de los 04-calibracion-lab-y-homografia.md de los packs"
firmware-source: >
  hardware/electronics/cameraFront-pack/firmware/openmv/calib-homografia-n6.py
  hardware/electronics/cameraBack-pack/firmware/openmv/calib-homografia-n6.py
  hardware/electronics/vision-optimization-pack/tools/solve_homografia.py
---

# Calibrar el XY de las cámaras N6 — procedimiento SIMPLE y CONFIABLE

> **Qué resuelve esto.** Que cuando la cámara ve la pelota (o un arco), reporte
> **bien dónde está** en cm relativos al robot (X = izquierda/derecha, Y =
> distancia adelante). Eso es la **homografía** (`H_MATRIX` en `cam-*-n6.py`).
> Hoy esa matriz es un **placeholder** → las distancias/ángulos están mal hasta
> calibrar. Esto es parte de **TASK-022**.

> Esto NO es la calibración de COLOR (LAB). Eso va en
> [`CALIBRACION-VISION-N6.md`](CALIBRACION-VISION-N6.md) (Pasos 1–3). Hacé el color
> PRIMERO (sin ver la pelota no podés calibrar su posición).

---

## ✅ RESULTADO DE CALIBRACIÓN — frontal ROBOT1 (Elías, 2026-06-07)

**Primera calibración real de distancias.** Cámara **frontal** del ROBOT1, lente
**ultra-wide**, resolución **VGA (640×480)**. Fuente: `vision-frontal-calibrada.py`
(artefacto de banco) → portada a `cam-frontal-n6.py` (producción v2).

```python
H_MATRIX = [
    [ 7.54504107e-01,  1.54808424e-02, -1.96304100e+02],
    [-1.40623499e-01, -2.05684020e-01,  2.30315983e+02],
    [-7.07447958e-03,  8.46088118e-02,  1.00000000e+00],
]
CAM_HEIGHT_CM  = 18.7                      # altura cámara sobre el suelo
BALL_RADIUS_CM = 13.5 / (2 * math.pi)      # ≈ 2.15 cm
# X,Y físicos = H·(u,v) corregidos por perspectiva ×(h−r)/h
```

| Parámetro | Valor | Nota |
|---|---|---|
| Resolución | **VGA 640×480** | "intermedia" (ni la más baja ni la más alta) — **la H está ATADA a esta resolución** |
| Lente | **ultra-wide** | la H absorbe la distorsión de barril para ESTE lente/ángulo |
| Altura cámara `h` | 18.7 cm | — |
| Orientación | `hmirror=True`, `vflip=True` | montaje 180° |
| Origen de las distancias | **CENTRO DEL LENTE** | ⚠️ **NO** el centro del robot (ver abajo) |

> ### ⚠️ Las distancias son DESDE EL LENTE, no desde el centro del robot
> La FSM razona con la pelota relativa al **robot**. La cámara reporta cm desde el
> **centro del lente** (montado adelante/arriba). **Falta medir y restar el offset
> lente→centro** aguas abajo (TOP o CENTRAL). Sin eso, la distancia a la pelota queda
> corrida unos cm. → tema-a-analizar abierto (TASK de banco).

> ### Decisión provisoria (Gustavo): MISMA H para las 4 cámaras
> Hasta tener otra calibración, frontal+trasera de ROBOT1 y de ROBOT2 usan esta misma
> `H`. Aplicado en `cam-frontal-n6.py` y `cam-trasera-n6.py`. Después → una por cámara.

> ### Protocolo: el artefacto es v1; producción es v2
> `vision-frontal-calibrada.py` usa el protocolo **v1** (9 bytes, sin CRC, X crudo).
> Producción (`cam-*-n6.py`) + el parser del TOP están en **v2** (11 bytes, X+100
> simétrico, CRC8 + END). **La `H` se portó a v2** (misma distancia física, encoding
> correcto). NO flashear el v1. **Deploy coordinado:** re-flashear las 2 cámaras + TOP
> juntos. Detalle en `journal/2026-06-07-calibracion-distancia-camara-frontal-elias.md`.

---

## TL;DR — la decisión: lona con grilla, NO pelota en puntos sueltos

**Recomendado: una LONA/HOJA impresa con una grilla de puntos negros**, que la
cámara detecta sola. La "pelota en puntos definidos" queda solo como **validación**
final. Por qué:

| | Pelota en puntos sueltos | **Lona con grilla (recomendado)** |
|---|---|---|
| Velocidad | Lenta: 1 punto por vez, mover/medir/leer | **1 sola foto** captura todos los puntos |
| Error humano | Alto: ubicar la pelota + leer píxeles a ojo | **Bajo**: la cámara lee los píxeles |
| Robustez de la H | 4 puntos = mínimo, 1 error la arruina | **9–20 puntos → mínimos cuadrados** (un punto malo no la arruina) |
| Repetible en Incheon | Re-medir todo cada vez | **Desenrollás la misma lona** → 4 cámaras en minutos |
| Fabricación | Nada (solo la pelota) | Imprimir una lona (barato, 1 vez) |

La grilla gana en lo que pediste: **rápido, simple y confiable**. El único "costo"
es imprimir la lona una vez.

---

## Qué necesitás (la "lona" de calibración)

**Ya está el PDF listo para imprimir:**
`hardware/electronics/vision-optimization-pack/tools/lona-calibracion-homografia-A1.pdf`
(se regenera/modifica con `gen_lona_calibracion.py`).

- **Imprimir:** A1 (594×841 mm), **al 100% / "tamaño real"** (NO "ajustar a la
  página"). Para banco, una hoja A1 plastificada; para Incheon, **lona/vinilo
  enrollable** (no se arruga —la planitud importa— y viaja en un tubo).
- **Control de escala (CRÍTICO):** el PDF trae una **regla de 200 mm**. Tras
  imprimir, medila: si NO mide 200 mm exactos, reimprimí. La escala mal arruina
  TODA la calibración.
- **Qué trae:** grilla **5×5 = 25 puntos** negros sobre blanco; columnas
  X = **−24, −12, 0, +12, +24 cm**; filas y = **0, 15, 30, 45, 60 cm** medidas
  desde el **borde cercano**. Flecha **+Y/ADELANTE**, eje **X=0** punteado y borde
  cercano marcado. Negro sobre blanco = máximo contraste → `find_blobs` con
  threshold oscuro lo agarra bajo cualquier luz.
- **Colocación:** lona plana; **alineá la columna X=0 con el eje delantero del
  robot** y **medí la distancia del robot al borde cercano** (= `NEAR_EDGE_Y_CM`
  en el script). Así el X de cada punto = el X impreso, y el Y = (y impreso +
  `NEAR_EDGE_Y_CM`).
- **Por qué grilla impresa y no marcar puntos a mano:** las distancias quedan
  exactas por el PDF 1:1 (no dependés de medir bien cada punto en el piso).

> **Detalle técnico (por qué la grilla va en el SUELO):** la `H` mapea pixel →
> punto en el **plano del piso**. La pelota tiene radio (su centro está elevado),
> y de eso se encarga la corrección `(CAM_HEIGHT_CM − BALL_RADIUS_CM)/CAM_HEIGHT_CM`
> que YA hace `cam-*-n6.py`. Por eso calibrar contra puntos en el piso es lo
> correcto: la H es del piso, y la altura de la pelota se corrige aparte.

---

## Las 3 piezas de software

1. **`calib-homografia-n6.py`** (corre EN la cámara, en ambos packs): detecta los
   puntos de la grilla, los ordena, los dibuja **numerados**, e imprime las
   **correspondencias** (píxel → cm) listas para copiar.
2. **`solve_homografia.py`** (corre en la **PC**, con numpy): toma esas
   correspondencias y calcula la `H_MATRIX` por mínimos cuadrados + te dice el
   **error de reproyección** (si quedó bien o no). El álgebra pesada va en la PC,
   no en la N6 (más confiable). ✅ **Algoritmo validado** (`--selftest` recupera
   una H conocida con error ~1e-11).
3. **`cam-frontal-n6.py` / `cam-trasera-n6.py`**: ahí pegás la `H_MATRIX` final.

---

## Procedimiento paso a paso (~10 min por cámara)

### Paso 0 — Antes de empezar
- Hacé primero la **calibración de color** (Pasos 1–3 de `CALIBRACION-VISION-N6.md`).
- **Montá la cámara en su posición DEFINITIVA** en el robot (si la movés después,
  la H deja de servir).
- Confirmá `HMIRROR`/`VFLIP` del `cam-*-n6.py` de esa cámara. El kit de homografía
  **usa los mismos flips** (la H se calibra en el mismo frame que usa producción).

### Paso 1 — Colocar la lona
- Poné la lona **plana** en el piso, en el campo de visión de la cámara.
- Alineala: la flecha **+Y apuntando adelante** (lejos del robot para la frontal;
  detrás del robot para la trasera), y el **origen** centrado respecto al robot.
- Medí (o confirmá) las coordenadas X de cada columna y Y de cada fila, en cm,
  respecto al robot.

### Paso 2 — Capturar los puntos (en la cámara)
1. Abrí `calib-homografia-n6.py` en el OpenMV IDE → **▶ Run** (corre de RAM; NO lo
   guardes como main.py).
2. En el framebuffer, cada punto detectado sale con un **recuadro verde**; si la
   cuenta coincide con la grilla, además sale un **número** (0,1,2…) en orden.
3. Arriba dice `blobs=N/9 OK` (o `AJUSTAR`). Si no detecta los 9:
   - Subí/bajá `DOT_PIXELS_MIN` (ruido vs puntos chicos).
   - Ajustá `DARK_THRESHOLD` si la luz es rara.
   - Mejorá la planitud/encuadre de la lona.
4. **Confirmá la grilla** en el script (los defaults ya casan con la lona A1):
   - `COL_X_CM` = X impreso de cada columna (ya viene −24…+24).
   - `Y_REL_CM` = filas del PDF (0/15/30/45/60).
   - **`NEAR_EDGE_Y_CM`** = la distancia que mediste del robot al borde cercano.
     ← normalmente es **lo ÚNICO** que tocás.
   - Si los números dibujados muestran la fila lejana ABAJO (no arriba), poné
     `IMAGE_TOP_IS_FAR = False`.
5. Cuando los números cuadran, la consola imprime un bloque
   `CORRESPONDENCIAS = [ (u,v,X,Y), ... ]`. **Copialo.**

### Paso 3 — Calcular la H (en la PC)
1. Abrí `hardware/electronics/vision-optimization-pack/tools/solve_homografia.py`.
2. Pegá el bloque `CORRESPONDENCIAS` que copiaste.
3. Corré: `python solve_homografia.py`
4. Lee el **error de reproyección**:
   - **medio < 1–2 cm y max < 3 cm → BIEN.** Imprime el bloque `H_MATRIX`.
   - error alto → casi siempre es una fila/columna mal ordenada o una coord mal
     medida. Revisá `COL_X_CM`/`ROW_Y_CM` y la lona, recapturá.
5. Copiá el bloque `H_MATRIX` que imprime.

### Paso 4 — Pegar la H y medir la altura
1. Pegá `H_MATRIX` en `cam-frontal-n6.py` (o trasera) reemplazando el placeholder.
2. Medí la **altura real de la cámara sobre el piso** → `CAM_HEIGHT_CM`.
3. (`BALL_RADIUS_CM` ya está derivado de la circunferencia; no lo toques salvo que
   cambie la pelota.)

### Paso 5 — Validar con la pelota (acá SÍ se usa la pelota)
Poné la pelota a distancias **conocidas** y comprobá lo que reporta:

| Pelota en… | (X,Y) esperado (cm) | (X,Y) reportado | Error |
|---|---|---|---|
| 30 cm al frente | (0, 30) | ? | ? |
| 50 cm al frente | (0, 50) | ? | ? |
| 80 cm al frente | (0, 80) | ? | ? |
| 100 cm al frente | (0, 100) | ? | ? |
| 50 cm frente, 30 der | (+30, 50) | ? | ? |
| 50 cm frente, 30 izq | (−30, 50) | ? | ? |

**Criterio de aceptación (TASK-022): error < 10%.** Si no llega, recalibrá la H
(volvé al Paso 2) o revisá `CAM_HEIGHT_CM`.

> Para LEER lo que reporta la cámara podés usar el TOP por USB (los getters de
> `cameras_runtime`) o, más simple, agregar temporalmente un `print(X,Y)` en el
> `cam-*-n6.py`. La pelota a la **izquierda debe dar X negativo** (contrato v2,
> eje-X simétrico) — si da positivo, revisá `HMIRROR` (TASK-202).

### Paso 6 — Ajustar `CAMERA_UNIT_TO_MM` en el TOP
La cámara reporta en **cm**; el TOP convierte a mm con
`CAMERA_UNIT_TO_MM` (`src/top/cameras_runtime.cpp`, hoy = 10.0). Como la H reporta
cm → **10.0 es correcto** (1 cm = 10 mm). Solo cambialo si tu H reportara otra
unidad. Confirmalo con la validación del Paso 5.

### Paso 7 — Guardar
- `Tools → Save open script to OpenMV Cam (as main.py)` → corre solo al energizar.
- Power-cycle, confirmá que arranca y transmite sin la IDE.

---

## Recalibración rápida en Incheon

La homografía depende del **montaje** (altura/ángulo), no de la luz → si el robot
no se desarmó, **la H de Salta sigue sirviendo en Incheon**. Lo que SÍ hay que
rehacer en Incheon es el **color** (LAB). Igual, si querés revalidar:
desenrollás la lona, corrés el Paso 2–3 por cámara (minutos) y listo.

---

## Mejora futura (más automática): AprilTags

Si en banco se confirma que `image.find_apriltags()` corre en la N6 con el módulo
`sensor`, se puede reemplazar los puntos negros por una grilla de **AprilTags**:
cada tag tiene un **ID único** y centro **sub-pixel**, así las correspondencias se
identifican solas (sin depender del orden) y con más precisión. **No verificado en
esta N6** (misma cautela que `csi`/`machine.UART`) → queda como P2. Los puntos
negros + `find_blobs` son el método **confiable que sabemos que anda**.

---

## Criterios de aceptación (resumen)

- [ ] `calib-homografia-n6.py` detecta los 9 (o N) puntos de la grilla, numerados OK.
- [ ] `solve_homografia.py` da error de reproyección **medio < 2 cm, max < 3 cm**.
- [ ] `H_MATRIX` pegada en `cam-*-n6.py` de cada cámara (frontal y trasera tienen H distintas).
- [ ] `CAM_HEIGHT_CM` medido real.
- [ ] Validación con pelota a 30/50/80/100 cm: **error < 10%**.
- [ ] Pelota a la izquierda → X negativo (eje-X v2 / TASK-202).
- [ ] `CAMERA_UNIT_TO_MM` del TOP coherente (cm → 10.0).

> Claude **no cierra TASK-022 ni TASK-202** (son de banco/humano). Esto prepara y
> documenta; el banco confirma.

## Estado de las herramientas

- `solve_homografia.py` — **algoritmo validado** host-side (`--selftest` OK, error ~1e-11). Listo.
- `calib-homografia-n6.py` — sintaxis OK; usa API estándar de OpenMV (`find_blobs`,
  `draw_*`) con el módulo `sensor`. **No se pudo probar en la N6 real** → confirmar
  en banco (fail-safe igual que el resto del kit).

## Archivos / referencias

- **Lona para imprimir (A1, 1:1):** `hardware/electronics/vision-optimization-pack/tools/lona-calibracion-homografia-A1.pdf`
- Generador del PDF (parametrico): `hardware/electronics/vision-optimization-pack/tools/gen_lona_calibracion.py`
- Captura en cámara: `cameraFront-pack|cameraBack-pack/firmware/openmv/calib-homografia-n6.py`
- Solver PC: `hardware/electronics/vision-optimization-pack/tools/solve_homografia.py`
- Producción (pegar H): `.../cam-frontal-n6.py`, `.../cam-trasera-n6.py`
- Color (hacer primero): [`CALIBRACION-VISION-N6.md`](CALIBRACION-VISION-N6.md)
- Contrato cámara↔TOP (coords/unidades): [`CONTRATO-DATOS-CAMARAS.md`](CONTRATO-DATOS-CAMARAS.md)
- Eje-X (contexto del signo): `research/in-progress/2026-06-03-eje-x-codificacion-asimetrica-vision.md`
