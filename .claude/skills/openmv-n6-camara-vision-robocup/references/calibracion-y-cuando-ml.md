# Cámara N6 — calibración (LAB / exposición / homografía) y veredicto ML/NPU

> Procedimiento operativo de banco/Incheon, anclado a `docs/firmware/CALIBRACION-VISION-N6.md`,
> `CALIBRACION-HOMOGRAFIA-XY-N6.md` y al script de producción `camaras-openmv/main.py`. La verdad
> que MANDA = el script flasheado + el banco. Lo que está sin reconciliar está marcado ⚠️.

## 1. COLOR (LAB) — depende de la LUZ → rehacer en Incheon

- **Qué:** umbrales LAB `(L_min,L_max,A_min,A_max,B_min,B_max)` por color. LAB > RGB porque separa
  luminosidad (L) de cromaticidad (A=verde-rojo, B=azul-amarillo) → más robusto a la luz.
- **Cómo:** Threshold Editor del OpenMV IDE, o `get_histogram()`/`get_statistics()` sobre un ROI con
  el objeto, **bajo la luz REAL de la cancha** (no la del lab). ~15 min. Kit: `calib-lab-n6.py`.
- **Targets del robot (RoboCup Soccer Open):** pelota **naranja** (por color, circunferencia 13,5 cm
  → `r=13.5/(2π)` en `main.py:45`), arcos **amarillo** (header 202) y **azul** (header 203).
  ⚠️ NO son "cyan/magenta" ni "golf ball IR pasiva" (eso dice la skill vieja `openmv-vision-tuning`,
  desactualizada).
- ⚠️ **INCONSISTENCIA de thresholds (3 sets divergentes — marcar, cerrar en banco):**
  - `main.py:47-49` (producción): naranja `(30,61,39,70,20,50)`, amarillo `(40,65,0,20,10,30)`,
    azul `(10,30,0,30,-35,-10)`.
  - `CALIBRACION-VISION-N6.md:17-24` ("final" 2026-06-09): naranja `(21,67,18,79,-32,127)`,
    amarillo `(17,70,-27,14,38,111)`, azul `(4,38,-13,57,-64,-4)`.
  - `main-comunicacion-vieja.py:75-77`: un tercer set.
  Los tres difieren. Cuál está REALMENTE flasheado es estado de hardware → **cerrar en banco**, dejar
  UN valor en `main.py` y registrarlo. `main.py` es la fuente única por FUENTES-DE-VERDAD.

## 2. EXPOSICIÓN / GANANCIA / WB — bloquear SIEMPRE antes de competir

Con autos ON la luz cambiante hace derivar A/B del LAB; el auto-exposure de OpenMV es conservador
(ajusta poco la exposición y compensa subiendo GANANCIA → ruido). Patrón:
```python
sensor.set_auto_whitebal(True); sensor.set_auto_gain(True)
sensor.skip_frames(time=2000)               # dejar aprender
# leer get_gain_db()/get_rgb_gain_db()/get_exposure_us() y FIJAR:
sensor.set_auto_gain(False, gain_db=...)
sensor.set_auto_whitebal(False, rgb_gain_db=(...))
sensor.set_auto_exposure(False, exposure_us=...)
```
⚠️ Firmas con confianza media-alta (verificadas contra el book OpenMV + foro + `main.py`; la página
oficial `omv.sensor.html` dio 404 al fetch — no verbatim de la doc).

- **En `main.py`:** `:29-40` deja correr los autos (`skip_frames(2000)`) y luego fija WB/gain/exposure
  con valores hardcoded (gain 12,04 dB, exposure 100328 µs). El bloque que IMPRIME los 3 valores para
  pegarlos está COMENTADO (`:149-156`) → descomentarlo es el flujo de re-calibración.
- ⚠️ **El flag `BRING_UP` NO está en `main.py`** (vivía en los `cam-*-n6.py` DEPRECADOS). Es buena
  práctica reusable (True=autos ON para leer / False=fijos para competir) → mejora P2 capitalizable,
  **NO un hecho actual**.
- **Explotar el global shutter:** exposiciones cortas para pelota rápida, sin blur ni skew.
- **Incheon:** re-bloquear bajo la luz del venue 15 min antes. Tener 2-3 perfiles guardados
  (luz alta/media/baja) + pendrive con scripts offline.

## 3. HOMOGRAFÍA (XY) — depende del MONTAJE, NO de la luz

- **Qué:** convierte (u,v) píxeles → (X,Y) cm relativos al robot. `transformarcoordenadas()` en
  `main.py:71-86`, con corrección de perspectiva `(h-r)/h`. H actual = la de Elías 2026-06-07
  (ultra-wide, VGA).
- **Cómo:** lona con grilla → `solve_homografia.py` (mínimos cuadrados, `--csv`/`--validate`,
  2026-06-15, `--selftest` error ~1e-11). Imprime el bloque H **apuntando a `main.py`** (HI-6).
- ⚠️ **ATADA a VGA:** cambiar el framesize (VGA→QVGA para subir fps) **INVALIDA la homografía** y
  hay que revisar `pixels_threshold` de área. No cambiar framesize sin recalibrar.
- ⚠️ **INCONSISTENCIA de altura (load-bearing, no resuelta):** `main.py:44` usa `h=95.0`; los docs y
  `main-comunicacion-vieja.py:65` usan `CAM_HEIGHT_CM=18.7`. 95,0 vs 18,7 NO reconciliado — afecta la
  corrección de perspectiva. Cuál corresponde al montaje real = banco.
- ⚠️ **Docs con la cita vieja `cam-*-n6.py`** (DEPRECADOS) a propagar HI-6: `CONTRATO-DATOS-CAMARAS.md`,
  cuerpo de `CALIBRACION-VISION-N6.md` y `CALIBRACION-HOMOGRAFIA-XY-N6.md` Paso 4. El destino canónico
  es `camaras-openmv/main.py`.
- ⚠️ **`CAMERA_UNIT_TO_MM=10.0`** (`cameras_runtime.cpp:33-41`) es PLACEHOLDER (correcto solo si la H
  reporta cm). Validar con pelota a 30/50/80/100 cm (<10% error) = criterio de cierre **TASK-022**.

**Qué se rehace cuándo:** COLOR (LAB) → SÍ en Incheon (luz). HOMOGRAFÍA → la de Salta sirve si el
robot no se desarmó (montaje).

## 4. Cuándo (y cuándo NO) usar ML/NPU — honesto: HOY el robot NO la usa

- **Color por LAB en CPU es el método GANADOR hoy** para pelota naranja + arcos azul/amarillo:
  corre en CPU, sin entrenar, se calibra en minutos. **No migrar a ML "porque la N6 puede".**
- **ML (YOLO/FOMO) conviene SOLO cuando el color no resuelve:** distinguir por forma/textura,
  detectar robots adversarios, ambientes con muchos falsos positivos de color. Costo real:
  recolectar+etiquetar dataset + entrenar (Edge Impulse/Roboflow) — días.
- **Pipeline si se adopta:** entrenar → exportar **TFLite INT8** → **Convert Model for NPU** (OpenMV
  IDE → Tools → Machine Vision) → escribir a ROM → `ml.Model('/rom/modelo.tflite')`. La NPU se usa
  automáticamente para modelos compatibles. Rendimiento: YOLOv8n >30 fps @256×256; FOMO 120+ fps con
  conversión.
- **Gotchas (confianza media, foro openmv):** cargar el `.tflite` CRUDO desconecta/reconecta el
  dispositivo; portar un FOMO de una OpenMV vieja (RT1062) NO es plug-and-play (requiere conversión).
- **Veredicto:** la integración `ml.Model` en la fw de las N6 del repo NO está probada (el repo no
  usa la NPU hoy). **Capitalizable a 2027, NO operativo para Incheon.** Si se vuelve producción
  (dataset etiquetado + modelo convertido corriendo en partido), AHÍ conviene escindir una sub-skill
  `openmv-n6-ml-npu`; hoy sería prematuro.
