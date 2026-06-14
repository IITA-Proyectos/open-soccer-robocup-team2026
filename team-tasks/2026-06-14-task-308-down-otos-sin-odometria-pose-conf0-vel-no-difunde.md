---
id: TASK-308
title: "DOWN: OTOS sin odometría — pose conf=0 (todo ceros) y OTOS vel no se difunde"
date_created: 2026-06-14
assigned: [virginia-viollaz, elias, gviollaz]
priority: P1
status: pending
blocks: [arquero/delantero que navegan con OTOS, cierre completo de TASK-031]
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

## Causa más probable (a confirmar)

**Alimentación de los OTOS.** Los OTOS se alimentan del **3.3 V del MP1584, que viene de
la BATERÍA — el USB NO los alimenta** (gotcha conocido del repo; ver
`docs/ESTADO-ACTUAL.md` "CÓMO ENCENDER LOS OTOS" y journals 2026-05-24/29). Si la DOWN
está por USB solo, o la batería no entrega corriente de verdad, los OTOS dan `conf=0` /
sin vel. El propio diag lo apunta: *"revisar broadcast+batería en DOWN, no la CENTRAL"*.

## Criterio de cierre

- [ ] **Power-cycle completo de la DOWN con batería cargada y entregando corriente**
      (switch ON, Dean XP1 bien puesto), esperar 10 s, reconectar. Re-correr
      `diag_central_rx_all` (o el monitor de base).
- [ ] La **OTOS pose** pasa a `conf>0` con x/y reales al mover el robot.
- [ ] La **OTOS vel** aparece (`[OK]` a ~100 Hz) y el `seqGap` del enlace DOWN se va a ~0.
- [ ] Si tras el power-cycle la pose sigue en `conf=0`: NO es batería → escanear I²C al
      boot (ambos OTOS deben dar `0x17`; una dirección rara tipo `0x64` = brownout) y, si
      el I²C está sano pero la vel no sale, **revisar en el firmware del DOWN si la
      difusión de `Velocity2D` (0x12) se gatea por validez** (`down_tx` / `main_down`).

## Notas

- **No bloquea** el monitor TOP ni el enlace TOP→CENTRAL (ambos OK, banco 2026-06-14).
- **Sí importa para el ARQUERO/DELANTERO** que navegan con OTOS (ver
  `journal/2026-06-12-*` y la deriva de rumbo del arquero strafe).
- Relacionado: TASK-029 (validar OTOS superficie), TASK-031 (UART 3 placas).
