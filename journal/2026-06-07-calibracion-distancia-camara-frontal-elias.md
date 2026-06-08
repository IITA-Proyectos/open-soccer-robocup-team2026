# 2026-06-07 — Calibración de distancia de la cámara frontal (ROBOT1) + integración a v2

**Quién:** Elías (calibración de banco) · Gustavo (decisiones) · Claude Opus 4.8 (integración + docs).
**Qué:** se calibró en banco la **homografía de distancias** de la cámara **frontal** del
ROBOT1 (lente **ultra-wide**, resolución **VGA**), y se integró la calibración a los
scripts de producción (contrato v2). Esto cierra el grueso del bloqueante #1 de Incheon
(visión sin calibrar / TASK-022).

## Lo que hizo Elías (banco)
- Calibró la **homografía** (matriz `H`) que mapea píxel → cm en el piso, con el lente
  **ultra-wide** puesto y a resolución **VGA (640×480)**.
- Entregó el archivo `vision-frontal-calibrada.py` (cámara frontal, YA calibrada).
- Distancias **tomadas desde el CENTRO DEL LENTE de la cámara, NO desde el centro del robot.**

## Valores de calibración extraídos (fuente de verdad)
- **Homografía (frontal, ultra-wide, VGA):**
  ```
  H = [[ 7.54504107e-01,  1.54808424e-02, -1.96304100e+02],
       [-1.40623499e-01, -2.05684020e-01,  2.30315983e+02],
       [-7.07447958e-03,  8.46088118e-02,  1.00000000e+00]]
  ```
- **Altura cámara** `h = 18.7 cm` · **radio pelota** `r = 13.5/(2π) ≈ 2.15 cm`.
- **Corrección de perspectiva:** `X = x·(h−r)/h`, `Y = y·(h−r)/h` (la pelota tiene radio; su
  centro está a altura r, no en el piso).
- **Resolución: VGA** (la "intermedia" — ni la más baja ni la más alta). ⚠️ **La H está
  ATADA a VGA**: a otra resolución los píxeles cambian de escala y la H deja de valer.
- **Orientación:** `hmirror=True`, `vflip=True` (montaje 180°, conector arriba).
- **LAB (sin cambios — ya coincidían con producción):** naranja `(21,67,18,79,-32,127)`,
  amarillo `(17,70,-27,14,38,111)`, azul `(4,36,-13,57,-64,-4)`.

## Decisión (Gustavo): misma calibración para las 4 cámaras (provisorio)
Hasta tener otra calibración, **las 4 cámaras** (frontal + trasera de ROBOT1 y de ROBOT2)
usan **esta misma `H`**. Cuando haya más calibraciones → **una independiente por cámara**.
Aplicado en `cam-frontal-n6.py` y `cam-trasera-n6.py` (misma H).

## Integración — lo que hizo Claude (y por qué)
El archivo de Elías estaba calibrado pero con el **protocolo cámara→TOP VIEJO (v1)**:
9 bytes `[201,X,Y+100, 202,…, 203,…]`, **sin CRC/END/sentinel** y con **X crudo** (no
simétrico). El firmware del TOP ya está en **contrato v2** (11 bytes, `X+100` simétrico,
CRC8 + END=254, sentinel 255). Flashear el v1 tal cual **rompe la cadena** (el TOP no lo
parsea). Por eso:
1. **`vision-frontal-calibrada.py`** se subió tal cual como **artefacto de banco** (prueba
   de la calibración), con encabezado que avisa que NO se flashea directo.
2. La **`H` calibrada se portó a los scripts de producción v2** (`cam-frontal-n6.py` y
   `cam-trasera-n6.py`), que conservan CRC/sentinel y matchean el parser del TOP.
3. Se cambió el framesize de producción **QVGA → VGA** (donde se calibró la H).
4. `py_compile` OK en los 3 scripts.

> La distancia física (X,Y en cm) es **idéntica** en v1 y v2 — sólo cambia el encoding del
> wire. Portar la H a v2 da las mismas distancias de Elías, pero parseables por el TOP.

## ⚠️ Caveat crítico: distancias desde el LENTE, no desde el centro del robot
La FSM/estrategia razona con la pelota **relativa al robot**. La cámara reporta cm **desde
el centro del lente**, que está montado **adelante y arriba** del centro del robot. Hay un
**offset fijo lente→centro** que hay que **medir y restar** aguas abajo (en el TOP al armar
el WorldSnapshot, o en la CENTRAL). Sin ese offset, el arquero/atacante calculan mal la
distancia a la pelota por unos cm. → **tema-a-analizar abierto** (ver TASK nueva).

## Qué queda para cerrar del todo (banco, lo hace el equipo)
1. **Deploy COORDINADO:** re-flashear las **2 cámaras (cam-*-n6.py v2 @VGA con la H nueva)
   + el TOP** juntos (piecemeal = cadena muerta), y verificar que el TOP parsea (CRC OK,
   sin sentinel fantasma).
2. **Medir el offset lente→centro del robot** y aplicarlo (TASK nueva).
3. **fps a VGA**: medir; VGA es más lento que QVGA. Si no alcanza, evaluar recalibrar a QVGA.
4. **Lock de exposición/WB/gain** (la calib se hizo con autos ON) + estabilidad 10 min +
   prueba bajo luz tipo Incheon (criterios restantes de TASK-022).
5. **Validar distancias contra regla** (30/50/80/100 cm) con la H portada a v2.

## Archivos tocados
- `hardware/electronics/cameraFront-pack/firmware/openmv/vision-frontal-calibrada.py` (NUEVO, artefacto)
- `hardware/electronics/cameraFront-pack/firmware/openmv/cam-frontal-n6.py` (H + VGA)
- `hardware/electronics/cameraBack-pack/firmware/openmv/cam-trasera-n6.py` (H compartida + VGA)
- `docs/firmware/CALIBRACION-HOMOGRAFIA-XY-N6.md` (resultado registrado)
- `team-tasks/2026-05-18-task-022-camara-operativa.md` (avance + estado)
- `docs/ESTADO-ACTUAL.md`, `docs/FUENTES-DE-VERDAD.md` (bloqueante + canónico)
