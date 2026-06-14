---
id: TASK-308
title: "DOWN: OTOS sin odometría — pose conf=0 (todo ceros) y OTOS vel no se difunde"
date_created: 2026-06-14
assigned: [virginia-viollaz, elias, gviollaz]
priority: P2
status: causa-identificada-binario-equivocado (flashear down_robot2)
blocks: [cierre completo de TASK-031]
tags: [hardware, down-board, otos, comunicaciones, bateria]
---

# TASK-308 — DOWN: OTOS sin odometría (pose conf=0, vel no difunde)

## Qué se observó (banco 2026-06-14, `diag_central_rx_all` en la CENTRAL)

El enlace DOWN→CENTRAL **transporta bien** (crc=0), pero el dato OTOS está mal:

```
DOWN (Serial1): enlace [REVISAR]  (crc=0  seqGap=1400 y subiendo)
  [OK]    LINEA      200 Hz
  [OK]    OTOS pose  100 Hz   → x=0 y=0 hdg=0 conf=0   (todo ceros / inválido)
  [FALTA] OTOS vel   #0  nunca
```

- La **pose** llega a 100 Hz pero con **`conf=0` y todo en cero** = el OTOS no entrega
  pose válida.
- La **velocidad NUNCA se difunde** (#0). Eso además **explica el `seqGap`**: el `seqGap`
  (≈1400) coincide con el conteo de pose (≈1400) = es exactamente el stream de vel a
  100 Hz que falta. **No es pérdida real de tramas** (`crc=0`), es el slot de vel que no
  sale. O sea: `[FALTA] vel` y `seqGap` son **el mismo problema**.

## ✅ CAUSA REAL (corregida 2026-06-14 con dato de María: **R2 NO tiene OTOS**)

Mi primer diagnóstico ("casi seguro batería") **estaba MAL.** Con el dato de que **el
robot 2 no tiene OTOS hasta nuevo aviso**, la explicación correcta es:

- El DOWN estaba corriendo el binario **`down`** (que asume **`DOWN_NUM_OTOS_CONNECTED=2`**)
  en un robot que **físicamente no tiene OTOS** → el read de OTOS no devuelve nada → la
  pose sale con `conf=0` (basura) y la vel no se difunde. El `seqGap` es ese stream de vel
  inexistente. **No es batería ni hardware roto: es el binario equivocado.**
- El binario correcto para R2 es **`down_robot2`** (extends `down` + `-DDOWN_NUM_OTOS_CONNECTED=0`,
  `platformio.ini:1340`). Con OTOS=0 el DOWN **no intenta leer OTOS** y los consumidores
  (drive_straight, GK paralelo, cross_track) caen al **fallback exacto sin OTOS**.

## Criterio de cierre

- [ ] **Flashear `down_robot2`** en la DOWN del robot 2:
      `pio run -e down_robot2 -t upload`.
- [ ] Re-correr `diag_central_rx_all`: el enlace DOWN debe quedar **limpio** — la LÍNEA
      sigue `[OK]` a 200 Hz, y el OTOS ya **no aparece como pose-inválida/`conf=0`** ni
      genera `seqGap` espurio (OTOS ausente = esperado, R2 no los tiene).
- [ ] (Cuando lleguen los OTOS a R2) volver a `down` (OTOS=2) y reabrir la validación real
      de odometría.

## Notas

- **No bloquea** el monitor TOP ni el enlace TOP→CENTRAL (ambos OK, banco 2026-06-14).
- **Sí importa para el ARQUERO/DELANTERO** que navegan con OTOS (ver
  `journal/2026-06-12-*` y la deriva de rumbo del arquero strafe).
- Relacionado: TASK-029 (validar OTOS superficie), TASK-031 (UART 3 placas).
