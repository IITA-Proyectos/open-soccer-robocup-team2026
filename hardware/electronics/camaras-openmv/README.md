# Cámaras OpenMV — scripts de cancha (Soccer 2026)

Scripts de las cámaras OpenMV que detectan **pelota naranja (201) / arco amarillo
(202) / arco azul (203)** y los mandan al Teensy **TOP** por UART.

## Archivos

| Archivo | Qué es | ¿Va en la cámara? |
|---|---|---|
| **`main.py`** | Detección que ANDA + **comunicación SEGURA** (contrato v2, 11 bytes, X/Y codificados, CRC8 + END). **Es el de producción.** | ✅ **SÍ** — copialo como `main.py` a la cámara. |
| `main-comunicacion-vieja.py` | El mismo script de detección **pero con la comunicación VIEJA (insegura)**: 9 bytes, X sin codificar, sin CRC. Se guarda **solo para pruebas / referencia** de la detección. | ❌ No (el TOP no la parsea bien). |

> **Por ahora va el MISMO `main.py` en las dos cámaras** (frontal y trasera). El
> TOP distingue cuál es cuál por el puerto UART y rota 180° la trasera de su lado.

## Diferencia entre las dos (lo único que cambia es la comunicación)

| | Vieja (`main-comunicacion-vieja.py`) | Nueva (`main.py`) |
|---|---|---|
| Bytes por trama | 9 | **11** |
| X | crudo (puede ser negativo → rompe `bytearray`) | **codificado** `X+100` ∈ [0,200] |
| Y | `Y+100` | `Y+100` ∈ [0,200] |
| "No detectado" | `0,0` (choca con el borde real) | **`255,255`** (inalcanzable desde una detección) |
| Integridad | — | **CRC8** (XOR de los 9 bytes) + **END=254** |

Todo lo demás (umbrales LAB / calibración, LEDs como indicador, recorte inferior
ROI, homografía de Elías 2026-06-07 atada a VGA, `pyb.UART(3, 19200)`) es **idéntico**.

## Cómo cargarla a la cámara

1. OpenMV IDE → abrir `main.py`.
2. Conectar la cámara → guardar el script en la placa **como `main.py`** (así corre
   sola al encender, sin la PC).
3. Verificar en banco con `diag_top_cameras` en el TOP (debe dar **FORMATO OK**).

## Origen

Derivado del script de banco que funciona (`mainopenmvcomvieja.py`, María/Elías).
El cambio de comunicación se alinea al contrato que parsea el TOP
(`src/top/cameras*.cpp`, `cam_crc8()` en `cameras.h`). Banco 2026-06-08.
