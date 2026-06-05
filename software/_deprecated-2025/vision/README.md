# Visión - OpenMV

> ⚠️ **DESACTUALIZADO / 2025.** El hardware VIVO son **2× OpenMV N6** (no H7 / H7 Plus), y el
> código de cámara que se flashea a competencia son las cámaras vivas:
>   - `hardware/electronics/cameraFront-pack/firmware/openmv/cam-frontal-n6.py`
>   - `hardware/electronics/cameraBack-pack/firmware/openmv/cam-trasera-n6.py`
> Esas usan el contrato cámara→TOP **v2 (11 bytes, CRC8+END=254)**. El script v1 de este
> directorio (`enviar coordenadas 2 arcos y pelota`) es 9 B sin CRC/END y NO debe flashearse.

Código histórico (2025) de la cámara OpenMV H7 / H7 Plus para detección de pelota y arcos.

## Punto de partida

Para el código vivo, ver las cámaras N6 de los packs (`cam-frontal-n6.py` / `cam-trasera-n6.py`,
rutas arriba). El script 2025 de este directorio (`enviar coordenadas 2 arcos y pelota`) queda
solo como referencia histórica.

## Funcionalidades

- Detección de pelota por color (thresholds LAB)
- Detección de arcos (1 o 2 arcos)
- Cálculo de coordenadas
- Envío de datos por UART al Teensy
- Calibración de thresholds
