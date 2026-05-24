---
id: TASK-027
title: "Validar SENSOR_POS[] de la placa DOWN — eje Y + orientación física del PCB"
date_created: 2026-05-24
assigned: [gviollaz, virginia-viollaz, elias]
priority: P2
status: pending
estimated_hours: 0.5
blocks: []
tags: [hardware, down-board, sensor-pos, validacion, geometria]
---

# TASK-027 — Validar eje Y de SENSOR_POS[] + orientación física del PCB

## Resumen

El array `SENSOR_POS[32]` del firmware (definido en
`hardware/electronics/down-board-pack/01-pinout-y-posiciones.md` §5b)
contiene las coordenadas (x, y) en mm de los 32 sensores físicos del
anillo. **Está validado parcialmente.** Sesión 2026-05-24:

- ✅ **Eje X:** evidencia parcial positiva. Un barrido lateral con hoja
  blanca sobre fondo negro produjo orden temporal coincidente con orden
  por coordenada X (los sensores X+ se taparon primero, los X- al final).
- ❌ **Eje Y:** no validado. El barrido que iba a probar Y resultó ser
  lateral por error de orientación de la hoja o por rotación del robot.
- ❌ **Orientación física del PCB:** asumida +Y = adelante del robot,
  +X = derecha — pendiente confirmar visualmente.

## Cómo terminar la validación

Usar el script ya programado `software/teensy/Soccer 2026/scripts/diag_position_sweep.py`.

### Paso 1 — Confirmar orientación física del PCB

Mirar el robot desde arriba e identificar el lado donde están **más
densos** los sensores: un arco de 8 sensores (F1-F8) en línea + 4 más
hacia adentro (F25-F28), separados de los laterales por un espacio.

Ese lado es el **+Y del PCB**. Confirmar que coincide con la convención
"adelante del robot" usada por el resto del firmware (cinemática de las
ruedas en CENTRAL, fusión de cámaras en TOP).

Si NO coincide (PCB rotado o invertido al montar), anotar la rotación
exacta (90°, 180°, 270°) y aplicar la matriz de rotación trivial al
SENSOR_POS[] del firmware.

### Paso 2 — Barrido Y real

Setup: placa DOWN encendida con batería, fondo negro debajo, hoja
blanca rígida con borde recto. Orientar el robot con el lado +Y
(frente, lado del arco denso) apuntando "hacia adelante" del operador.

```powershell
cd "C:\Users\violl\iitasoccer\open-soccer-robocup-team2026\software\teensy\Soccer 2026\scripts"
python diag_position_sweep.py --port COM10 --axis Y --duration 10
```

Cuando el script imprima `>>> EMPEZÁ A DESLIZAR LA HOJA AHORA <<<`,
deslizar la hoja desde el FRENTE del robot hacia ATRÁS (o al revés,
da igual — el script auto-detecta dirección). El borde de la hoja
debe ir **paralelo a un eje lateral** (perpendicular a la dirección
del movimiento).

### Paso 3 — Barrido X (re-confirmar)

```powershell
python diag_position_sweep.py --port COM10 --axis X --duration 10
```

Idem, pero hoja deslizándose lateralmente (izquierda↔derecha), borde
paralelo al eje frente-atrás.

### Paso 4 — Verdict

```powershell
python diag_position_sweep.py --verdict
```

Esperado:
- Score Y ≥ 70% → eje Y de SENSOR_POS validado.
- Score X ≥ 70% → eje X re-confirmado.

Si alguno da < 70%: ver inversiones reportadas, identificar si hay
sensores específicos desplazados o si todo el array está sistemáticamente
rotado/espejado.

## Criterio de cierre

- [ ] Orientación física del PCB confirmada (foto + anotación de
      rotación si aplica).
- [ ] Score Y del verdict ≥ 70%.
- [ ] Score X del verdict ≥ 70%.
- [ ] Si hay rotación del PCB respecto al doc, aplicar al SENSOR_POS[]
      del firmware + actualizar §5b del doc canónico.

## Notas / decisiones

- 2026-05-24: TASK creada al cierre de la sesión donde se programó
  `diag_position_sweep.py` pero solo se logró un barrido (X, sin querer).
  Ver journal `2026-05-24-hardware-up-down-anillo-linea.md` (puede
  ampliarse con sección de validación parcial de SENSOR_POS).

## Cambios de estado

- 2026-05-24: creada. Script de validación listo y testeado parcialmente.
