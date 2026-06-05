---
title: "YOLO/FOMO en la N6 vs detección por color (find_blobs) para pelota + arcos"
date: 2026-06-04
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M, Anthropic)"
status: backlog
tags: [vision, camaras, openmv, n6, machine-learning, yolo, fomo, npu, P2, 2027]
robot: ambos
area: vision
tipo: analisis
---

# YOLO/FOMO en la N6 vs detección por color — análisis comparativo

> **Estado: backlog (investigación a futuro).** Análisis pedido por Gustavo el
> 2026-06-04. NO es una decisión ni un plan de ejecución — es el material para
> decidir, capitalizable a 2027. No toca el path de competencia.

## Pregunta concreta a responder

¿Conviene reemplazar la detección por color (`image.find_blobs` + thresholds LAB)
por un modelo de detección aprendido (YOLOv8n / FOMO) corriendo en el **NPU
Neural-ART** de la cámara OpenMV N6, para detectar pelota naranja y arcos
amarillo/azul? ¿Con qué esfuerzo, cuántos datos, y cómo se etiquetan?

## Por qué importa

- **Dolor #1 actual:** los thresholds LAB se rompen al cambiar de iluminación →
  hay que recalibrar en cada cancha (TASK-022). Un modelo entrenado con luz variada
  generaliza mucho mejor → ataca ese dolor de raíz.
- **Hardware ocioso:** la N6 ya comprada tiene un NPU de 600 GOPS diseñado para esto
  (corre YOLOv8n a >30 FPS @256² con <0.75 W). Hoy ese NPU está **apagado**.
- **Estrategia multi-temporada (CLAUDE.md):** Incheon 2026 = aprendizaje; 2027 = la
  apuesta real. Esto es exactamente una inversión 2027 que debe sobrevivir al equipo.

## Contexto RoboCup (modera el entusiasmo)

En las ligas **mayores** de RoboCup el salto a redes lo forzó la **pelota blanca**
(desde 2015). En **RCJ Soccer Open 2026 la pelota es naranja** → el color sigue
siendo señal válida. O sea: la presión "obligatoria" de las mayores **no aplica del
todo** acá. El beneficio para nosotros NO es "sin esto no veo la pelota", sino
**robustez a iluminación + menos falsos positivos**. Patrón común en embebido:
**híbrido** (ROI por color + red sobre el recorte), ~30 FPS.

## Aclaración de términos

- Lo de hoy **no es OpenCV**: es `find_blobs` de OpenMV (segmentación por color LAB).
- "YOLO en la N6" **no es** el YOLOv8 de GPU: es un modelo chico **cuantizado int8**
  en el NPU. Dos familias realistas:
  - **FOMO** (Edge Impulse): da *centroides* por clase, ultra liviano (>200 FPS).
    Mapea **exacto** al contrato actual (x/y por objeto).
  - **YOLOv8n / tiny-YOLO**: da *cajas* (centro de la caja = x/y). Más pesado, la N6
    lo banca a >30 FPS.

## Comparación

| Dimensión | Color-blobs (hoy) | YOLO/FOMO en N6 |
|---|---|---|
| Robustez a iluminación | Frágil: recalibrar LAB por cancha | Fuerte si se entrena con luz variada |
| Falsos positivos | Remera/zapatilla naranja = "pelota" | Distingue por forma+textura |
| Arcos ocluidos | Por área; sufre | Mejor con datos de oclusión |
| Confianza por detección | Fija (placeholder 80 en `cameras_fusion.cpp:11`) | Real (score del modelo) → mejora fusión TOP |
| FPS | ~25–30 (QVGA) | YOLOv8n >30 @256² / FOMO >200; medir en MicroPython real |
| Tuning en cancha | Bajo por sesión, repetitivo | Casi nulo si generaliza |
| Setup inicial | Ya hecho | Semanas (datos+label+train+integración) |
| Mantenibilidad alumnos | Cambiar 3 tuples (trivial) | Reentrenar (pipeline; documentar para 2027) |
| Uso del hardware | NPU ocioso | Aprovecha lo que la N6 fue diseñada para hacer |
| Riesgo firmware | Conocido, andando | **Incógnita**: ¿el runtime ML corre con `sensor` o exige `csi` (preview negro en esta placa)? |

## Envergadura: qué cambia

**Buena noticia arquitectónica:** el modelo reemplaza **solo el front-end de detección**
dentro del script de la cámara. El **contrato cámara→TOP puede quedar igual** (centroide
x/y + visible por clase). El parser/fusión del TOP **no se tocan**. Swap limpio en un
borde bien definido.

1. **Firmware cámara** (`cam-*-n6.py`): reemplazar `find_blobs` por inferencia
   (`ml`/NPU), extraer centroide por clase, empaquetar igual. **Dejar color como
   fallback con flag** (de-risking).
2. **Modelo** .tflite int8 en la N6 + confirmar soporte NPU del firmware.
3. **Pipeline de datos/entrenamiento** (fuera del robot): captura → label → train →
   export N6.
4. **(Opcional, mejora)** agregar `confidence` real al contrato → la fusión del TOP
   deja de usar el 80 fijo. Eso SÍ toca el contrato (coordinar con TOP) → v3, no
   necesario para arrancar.

## ¿Cuántas fotos?

- **Mínimo viable (FOMO):** ~50–150; modelos decentes con ~136.
- **Objetivo robusto (4 cámaras + 2 venues):** ~500–1500 imágenes **diversas**
  (iluminación lab/venue/sombra; distancia 20/50/100/150 cm; ángulo; oclusión;
  fondos con público/robots/líneas; las 4 cámaras/montajes).
- **Diversidad > número crudo.** Augmentation (brillo/blur/flip/recorte) multiplica
  ×5–10. Imagen sweet-spot ~180×180 / 256×256.

## Cómo etiquetar / auto-etiquetar (esto abarata todo)

1. **Tu propio detector de color como auto-etiquetador (weak labeling) — la más astuta.**
   Correr `find_blobs` offline sobre las fotos para generar cajas; el humano solo
   **corrige**. Etiquetar = revisar, no dibujar. Casi gratis y a medida.
2. **Zero-shot Grounding DINO + SAM (Autodistill / Roboflow Auto-Label):** prompt de
   texto ("orange ball", "blue goal", "yellow goal") → cajas sin entrenar. Bueno para
   clases comunes; revisar igual.
3. **Manual asistido:** Roboflow Annotate (SAM 1-click), CVAT, Label Studio.

OpenMV ya está integrado con **Roboflow** y **Edge Impulse** para capturar→entrenar→
desplegar a la N6 → no hay que armar el toolchain de cero.

## Tiempo de codificación (honesto, por fases)

| Fase | Qué | Tiempo |
|---|---|---|
| 0. Spike factibilidad ⚠️ | Confirmar que la N6 corre un modelo tiny con el path de cámara que SÍ anda (`sensor` vs `csi`). **Gatea todo.** | 0.5–1 día |
| 1. Captura de datos | 500–1500 fotos, 4 cámaras, luz/distancia/oclusión variadas (ideal: incluir Incheon) | horas/sesión, repartido |
| 2. Etiquetado | Auto-label (blob o Grounding-DINO) + verificación humana | 1–2 días |
| 3. Entrenamiento + eval | Edge Impulse/Roboflow, iterar F1 | 1–3 días (mucho es esperar) |
| 4. Integración on-device | Inferencia + centroides + empaquetado al contrato + fallback flag | 1–3 días (si Fase 0 verde) |
| 5. Validación banco | FPS, falsos positivos, distancia, consistencia entre cámaras | 2–4 días |
| 6. Doc pipeline 2027 | Para que el equipo nuevo reentrene | 1 día |

**Total realista:** ~2–4 semanas part-time. **Código puro ≈ 1 semana**; domina la
iteración de datos, no la programación.

## Riesgos (formato coach)

**Risk-no-fix:** seguís atado a recalibrar LAB por cancha; NPU ocioso; no capitalizás 2027.
**Risk-fix (apurado):** romper el path que hoy anda; `csi`/firmware no soporta ML con
tu setup → spike fallido; modelo solo-Salta que no generaliza a Incheon; pipeline
opaco que el equipo 2027 no sabe reentrenar.
**Bloqueante #1 a verificar primero:** ¿el runtime ML de la N6 corre con `sensor` (el
que anda) o exige `csi` (preview negro en esta placa)? **No verificable sin banco.**

## Plan de prueba en hardware real

1. **Setup (Fase 0):** N6 montada, firmware al día, correr el ejemplo oficial de
   detección N6 (FOMO/YOLOv8n) tal cual; confirmar preview NO negro + inferencia viva.
   (Robot no necesario aún.)
2. **Criterio de aceptación:** inferencia ≥15 FPS sobre 1 clase con el path de cámara
   funcional; si exige `csi`, documentar si actualizar firmware lo arregla.
3. **Regresión:** el script color-blobs sigue corriendo idéntico (fallback intacto);
   el contrato no cambia; el TOP sigue parseando.

## Recomendación (multi-temporada)

1. **Incheon 2026:** competir con color-blobs + el filtro de forma ya agregado.
   **Track paralelo P2** que NO toca firmware de competencia: hacer la **Fase 0
   (spike)** y **capturar dataset del venue real**.
2. **Post-Incheon → Nacional Nov / 2027:** entrenar FOMO/YOLOv8n con datos de
   Incheon + lab, integrar con fallback, validar en banco, documentar el pipeline.
3. **Híbrido** (robustez antes de la red completa): color para el ROI + red sobre el
   recorte (patrón embebido de RoboCup).

## Próximos pasos sugeridos

- [ ] Correr la **Fase 0 (spike)** en banco (humano) — único dato que destraba la
      factibilidad real.
- [ ] Definir si Incheon se usa como sesión de **captura de dataset** (planificarlo).
- [ ] Si Fase 0 verde → mover este doc a `research/in-progress/` y arrancar el pipeline.

## Recursos / fuentes

- OpenMV N6 / STM32N6 NPU (YOLOv8n >30 FPS @256², 600 GOPS): https://blog.st.com/openmv-n6/ ·
  https://www.cnx-software.com/2025/03/17/micropython-openmv-n6-and-ae3-ai-cameras-run-on-battery-for-years/
- FOMO / dataset size (Edge Impulse): https://docs.edgeimpulse.com/docs/edge-impulse-studio/learning-blocks/object-detection/fomo-object-detection-for-constrained-devices
- RoboCup CNN ball detection: https://link.springer.com/chapter/10.1007/978-3-319-68792-6_2 ·
  https://arxiv.org/pdf/1909.02406 ·
  https://www.researchgate.net/publication/376243587_Ball_Detection_and_Tracking_with_Different_Embedded_Systems_in_the_RoboCup_Soccer_context
- Auto-label (Grounding DINO + SAM / Autodistill): https://blog.roboflow.com/enhance-image-annotation-with-grounding-dino-and-sam/ ·
  https://blog.roboflow.com/label-train-deploy-autodistill/
- Código relacionado en el repo: `hardware/electronics/cameraFront-pack|cameraBack-pack/firmware/openmv/cam-*-n6.py` ·
  `software/teensy/Soccer 2026/src/top/cameras.cpp` · `src/shared/cameras_fusion.cpp` ·
  `docs/firmware/CONTRATO-DATOS-CAMARAS.md` · `research/in-progress/2026-06-03-eje-x-codificacion-asimetrica-vision.md`
