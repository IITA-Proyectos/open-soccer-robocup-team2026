---
title: "02 — Pipeline de detección de punta a punta"
date: 2026-06-03
---

# 02 — Cómo detecta (cámara → TOP → WorldSnapshot)

```
[sensor N6 RGB565 QVGA]
        │  (exposición/WB: BRING_UP=True autos ON para ver; False = fija para partido)
        ▼
[threshold LAB por color]  NARANJA(pelota) / AMARILLO(arco) / AZUL(arco)
        │
        ▼
[find_blobs + filtro pixels_min]  → blob más grande por color
        │
        ▼
[homografía H_MATRIX]  pixel → (x, y) en cm ; Y ≈ distancia
        │
        ▼
[packet 9 bytes]  header 201=pelota / 202=amarillo / 203=azul ; sentinel si no hay blob
        │  UART 19200 8N1  (pyb.UART)
        ▼
[TOP cameras.cpp]  parser robusto de los 9 bytes (frontal=Serial3, trasera=Serial5)
        │
        ▼
[cameras_fusion.cpp]  fusiona front+back (rota 180° la trasera), watchdog
        │
        ▼
[main_top::build_snapshot]  ball_x/y + ball_velocity (EMA) + arcos → WorldSnapshot
        │
        ▼
   → CENTRAL (strategy) decide
```

## Parámetros clave por etapa

| Etapa | Parámetro | Efecto |
|---|---|---|
| Sensor | `EXPOSURE_US`, auto-WB/gain | Estabilidad del color con la luz. Para partido: fijos. |
| LAB | `*_THRESHOLD` (L,A,B min/max) | Qué color agarra cada detección. **Lo más sensible a la iluminación.** |
| Blobs | `*_PIXELS_MIN` | Filtra ruido (pelota ≥20, amarillo ≥600, azul ≥300). |
| Geometría | `H_MATRIX`, `CAM_HEIGHT_CM` | Convierte pixel→cm; calidad de la distancia (Y). |
| Montaje | `HMIRROR`/`VFLIP` | Orientación de la imagen (la pelota arriba debe leerse arriba). |
| Protocolo | header 201/202/203 + sentinel | Contrato con el TOP (no romper). |

## Convención de ejes de la cámara

`+x` de la cámara debe ser la **derecha del robot** (ver
`docs/CONVENCION-EJES-ROBOT.md`). ⚠️ El **signo real está sin verificar en
hardware** (TASK-202): si con la pelota a la derecha `ball_x` sale negativo, hay
que negar el signo (en el parser TOP o en el script). La cámara **trasera** NO
rota sus coordenadas internamente — la rotación 180° la aplica el TOP en
`cameras_fusion`.

## Dónde puede fallar (mirar acá cuando "no ve")

1. **No transmite** → UART_PORT mal (probar 1/2/3) o `machine.UART`/`csi` (usar `sensor`+`pyb.UART`).
2. **Transmite pero no detecta** → LAB sin calibrar (TASK-022) o exposición que cambió el color.
3. **Detecta ruido** → `pixels_min` bajo / threshold demasiado ancho.
4. **Distancia/posición rara** → homografía placeholder o `CAMERA_UNIT_TO_MM` del TOP sin calibrar.
