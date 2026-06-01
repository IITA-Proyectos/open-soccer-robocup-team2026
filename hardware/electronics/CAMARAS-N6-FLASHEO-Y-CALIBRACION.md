---
title: "Cargar y calibrar las cámaras OpenMV N6 (frontal + trasera) — Soccer 2026"
date: 2026-05-31
status: vigente
area: vision
---

# Cargar programas a las cámaras OpenMV N6

> Procedimiento para flashear las **2 cámaras N6** (migración desde H7 Plus) y
> dejar corriendo la detección de pelota naranja + arco amarillo + arco azul.
> Scripts listos (lógica + protocolo ya probados en H7, migrados a la API `csi`):
> - **Frontal:** [`cameraFront-pack/firmware/openmv/cam-frontal-n6.py`](cameraFront-pack/firmware/openmv/cam-frontal-n6.py)
> - **Trasera:** [`cameraBack-pack/firmware/openmv/cam-trasera-n6.py`](cameraBack-pack/firmware/openmv/cam-trasera-n6.py)
>
> Análisis completo del subsistema: `journal/2026-05-31-analisis-vision-n6-deteccion-protocolo.md`.

## A. Flashear el firmware de la N6 (una vez por cámara, ×2)

1. **Actualizar OpenMV IDE** a la versión que lista **"OpenMV N6"** en el selector de placas
   (la IDE/firmware de la H7 **NO** sirve — cada chip tiene su binario).
2. Conectar la N6 por **USB-C** al PC.
3. Clic en **Connect** (abajo a la izquierda). En la primera conexión la IDE detecta la N6 y
   **ofrece instalar el firmware oficial** → aceptar.
   - Si no entra en modo carga: **bootloader DFU** → doble-pulsar **RESET** y reintentar Connect.
4. Ante **"Erase internal file system?"** → **No** (salvo querer limpiar todo).
5. Listo: queda **Connected** y lista para correr scripts.

## B. Cargar el script de detección (una vez por cámara)

1. Abrir en la IDE el script según la cámara: `cam-frontal-n6.py` o `cam-trasera-n6.py`.
2. **Play (▶)** para probarlo en vivo (mientras está conectada).
3. Para que **arranque solo al encender** (sin la IDE, que es lo que pasa en cancha):
   **Tools → Save open script to OpenMV Cam (as main.py)**. Eso lo deja persistente en la N6.
4. Repetir con la otra cámara y su script.

## C. Calibrar EN BANCO antes de confiar (cada cámara) — lo marcado ⚠️ en el script

> El sensor de la N6 (PAG7936, global shutter) es **distinto** al de la H7 → la
> calibración vieja NO sirve. Hay que rehacer esto **por cámara**:

1. **`UART_PORT`** — la N6 tiene 3 UART. Confirmar cuál mapea al pin cableado al
   **Serial3** del Teensy (frontal, conector U8) / **Serial5** (trasera, RX pin 21).
   Probar 1/2/3 y ver en el TOP cuál recibe paquetes (`diag`/contadores de `cameras_runtime`).
2. **Thresholds LAB** (`NARANJA/AMARILLO/AZUL_THRESHOLD`) — **Tools → Machine Vision →
   Threshold Editor** sobre la pelota / cada arco, con la **luz real**. Sin esto no detecta bien.
3. **Exposición** (`EXPOSURE_US`) — poner `auto_exposure(True)` un momento, leer el valor con
   buena luz, y fijarlo. Global shutter → suele querer exposición más baja.
4. **Homografía** (`H_MATRIX`) — calibrar con 4 puntos conocidos en el suelo
   (la **trasera con los suyos, DETRÁS** del robot). Procedimiento en
   `cameraFront-pack/04-calibracion-lab-y-homografia.md`. Validar error <10% a 30/50/80/100 cm.
5. **`HMIRROR`/`VFLIP`** — las cámaras están **montadas rotadas 180°** (conector arriba):
   `HMIRROR=True + VFLIP=True` (juntos = giro de 180° de la imagen) compensa ese montaje.
   Verificar con el **preview** del IDE que la imagen quede **derecha**; si queda espejada/al
   revés, ajustar el par. **OJO — son DOS rotaciones distintas, no confundir:**
   - El giro de la **imagen** por el montaje 180° → acá, en el script (HMIRROR/VFLIP).
   - El giro de **coordenadas** de la trasera por mirar hacia atrás → lo hace el **TOP**
     (`cameras_fusion.cpp:25-30`, cam_id=1). Las dos hacen falta; no quitar ninguna.

## D. Validar la cadena cámara → TOP

- En el TOP, los paquetes llegan como 9 bytes `[201,X,Y+100, 202,X,Y+100, 203,X,Y+100]` a 19200
  (frontal=Serial3, trasera=Serial5). El parser es `src/top/cameras.cpp`.
- Chequeos: pelota tapada → **no** debe reportar pelota (sentinel ok, no "pelota fantasma");
  mover un objeto X muy a un costado → la cámara **no se cuelga** (clamp ok); cada color
  prende su LED de diagnóstico.

## E. Qué NO cambió respecto a la H7 (para tranquilidad)
Toda la lógica de detección (`find_blobs` + LAB), la homografía, el empaquetado UART y el
protocolo de 9 bytes son **idénticos** al template H7. Lo único migrado es la capa de cámara
(`sensor` → `csi`). Si algún `csi.*` da error, ver `docs.openmv.io/library/omv.csi.html`
(los nombres pueden variar por versión de firmware).
