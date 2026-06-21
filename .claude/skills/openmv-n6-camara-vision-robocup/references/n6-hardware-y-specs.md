# OpenMV N6 — specs de hardware y comparación con el H7

> Tabla de consulta. Fuentes primarias: openmv.io/products/openmv-n6, blog.st.com/openmv-n6,
> docs.openmv.io/openmvcam/quickref/openmv-n6.html. Confianza ALTA salvo donde se marca.
> La verdad que MANDA para el robot es el código (`cameras.h:1` "cámaras OpenMV N6", `main.py`,
> `cam-frontal-n6.py:35` "sensor PAG7936 != H7"), NO la skill `openmv-vision-tuning` (que dice "H7").

## Specs de la OpenMV N6

| Bloque | Dato |
|---|---|
| **SoC** | STMicroelectronics **STM32N657** (variante N657L0) |
| **CPU** | Arm **Cortex-M55 @ 800 MHz** (1280 DMIPS), FPU doble precisión, **Helium/MVE** (SIMD 128-bit) |
| **NPU** | ST **Neural-ART Accelerator @ 1 GHz, ~600 GOPS INT8** (acelerador de redes neuronales, no de propósito general) |
| **Sensor** | **PAG7936**: 1 MP, color, **GLOBAL SHUTTER** |
| Resoluciones/fps nativos del sensor | 1280×800 @120 fps · 640×400 @240 fps · 320×200 @480 fps |
| **Memoria** | 64 MB SDRAM externa + 4,2 MB SRAM interna + 32 MB FLASH |
| Consumo | ~150-180 mA @5 V (~0,75 W) con NPU activa; ~1,6 mA @3,7 V deep sleep (conector BAT) |
| Módulo MicroPython de cámara | **`sensor`** (en esta placa `csi` daba preview **NEGRO** — lección del repo) |
| Extras | ISP, H.264/JPEG por HW, GPU de escalado, WiFi a/b/g/n + BT 5.1, Ethernet Gigabit, 2 buses AXI 64-bit para la NPU |
| Rendimiento IA | YOLOv8n >30 fps @256×256 (<0,75 W); FOMO (Edge Impulse) >120 fps con conversión NPU |

⚠️ **Inconsistencia de prensa (no citar como dato firme):** la NPU es **600 GOPS** (operaciones
ENTERAS INT8). Algunos artículos (Hackster, Edge-AI Vision) la llaman "600 GFLOPS" (punto flotante)
— es un error de redacción; la Neural-ART es un acelerador entero INT8. Unidad correcta = GOPS.

## N6 vs H7 / H7 Plus — qué cambió

| | **N6** (en uso) | H7 | H7 Plus |
|---|---|---|---|
| SoC | STM32N657, Cortex-**M55** @800 MHz | STM32H743, Cortex-M7 @480 MHz | STM32H743, Cortex-M7 @480 MHz |
| NPU | **Neural-ART 600 GOPS** | **ninguna** | **ninguna** |
| Sensor (default) | **PAG7936 1 MP** | OV7725 (0,3 MP) | OV5640 (5 MP) |
| Shutter | **GLOBAL** | ROLLING | ROLLING |
| RAM | 64 MB SDRAM + 4,2 MB SRAM | 1 MB interna (+ shield) | 1 MB + 32 MB SDRAM |
| ML on-camera | YOLO/FOMO acelerado por NPU | TFLite en CPU (lento) | TFLite en CPU (lento) |

**Consecuencias operativas para el robot:**
1. **Thresholds LAB del H7 NO se transportan al N6** — sensor distinto (PAG7936 vs OV*). Recalibrar.
2. **GLOBAL shutter** elimina motion-blur/skew de la pelota rápida → exposiciones cortas sin
   distorsión geométrica. El H7 (rolling) no lo tiene.
3. La N6 tiene NPU pero **el robot HOY no la usa** (pipeline `find_blobs` LAB en CPU) — capacidad
   ociosa, capitalizable a 2027 (ver `calibracion-y-cuando-ml.md`).
4. Módulo `sensor` (no `csi`): en esta placa `csi` daba preview negro. Usar `import sensor`.

## FPS — el publicitado NO es el del pipeline (confianza media, foro)

El máximo del sensor (480 fps @320×200, 120 fps @1280×800) **NO es el del pipeline útil**:
- QVGA crudo ~470 fps → con `find_blobs` + UART cae a **~110 fps** (el procesamiento + UART es el
  cuello de botella, ~1/4 del máximo).
- A VGA (640×480, lo que corre el robot por la homografía) cae más.
- ⚠️ **El FPS real del robot NO está registrado en el repo** — medirlo en banco con `clock.fps()`.
  Apuntar a un piso (≥25-30 Hz) y verificarlo. Fuente: foros.openmv.io (usuario, no benchmark oficial).
