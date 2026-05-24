---
id: TASK-029
title: "Validar precisión cuantitativa de OTOS sobre superficie texturada (cancha o alfombra)"
date_created: 2026-05-24
assigned: [gviollaz, virginia-viollaz, elias]
priority: P1
status: pending
estimated_hours: 0.5
blocks: [pose absoluta confiable en partido, fusion sensorial CENTRAL con datos OTOS reales]
tags: [hardware, down-board, otos, validacion, calibracion]
---

# TASK-029 — Validar precisión cuantitativa OTOS sobre superficie texturada

## Resumen

La lib SparkFun Qwiic OTOS ya está activada en el firmware (TASK-012
cerrada parcialmente 2026-05-24). Los 2 chips U5 + U6 responden I²C y
reportan pose cambiante con movimiento real. **Lo que falta:** validar
que la pose reportada coincide cuantitativamente con el desplazamiento
físico real, sobre una superficie adecuada.

El test del 2026-05-24 sobre **hoja A4** dio tracking errático
(desplazamiento neto 28.6 mm cuando el movimiento real fue ~300 mm).
La causa probable es que la hoja A4 es demasiado uniforme: el OTOS es un
sensor óptico tipo mouse y requiere **textura microscópica del piso**.

## Lo que hay que hacer

### Setup del test

1. Robot apoyado sobre superficie con textura microscópica visible:
   - **Cancha verde RoboCup** (ideal).
   - **Alfombra densa** (segundo mejor).
   - **Madera con veta visible** o baldosa con grano (alternativa).
   - ⚠️ NO usar: vidrio, plástico brillante, hoja A4 blanca, mesa lisa.
2. Batería + USB conectados (orden correcto, ver TASK-028).
3. Firmware `diag_down` flasheado con lib OTOS activa (commit del
   2026-05-24, `software/teensy/Soccer 2026/src/down/otos.cpp`).
4. Robot en posición de inicio marcada con cinta o lápiz.

### Test 1 — Desplazamiento lineal

1. Lanzar captura serial 15s:
   ```powershell
   cd "C:\Users\violl\iitasoccer\open-soccer-robocup-team2026\software\teensy\Soccer 2026\scripts"
   python -c "import serial,time,re; s=serial.Serial('COM10',115200,timeout=0.3); start=time.time(); samples=[]; reg=re.compile(r'OTOS: x=(-?[0-9.]+) y=(-?[0-9.]+) hdg=(-?[0-9.]+)'); 
   while time.time()-start<15:
       l=s.readline().decode('utf-8','replace').strip(); m=reg.match(l)
       if m: samples.append((time.time()-start,float(m.group(1)),float(m.group(2)),float(m.group(3))))
   s.close(); print(f'{len(samples)} muestras'); 
   if samples: t0,x0,y0,h0=samples[0]; tf,xf,yf,hf=samples[-1]; dx,dy=xf-x0,yf-y0; print(f'dx={dx:+.1f} dy={dy:+.1f} dist={(dx*dx+dy*dy)**0.5:.1f}mm dhdg={hf-h0:+.1f}deg')"
   ```
2. Esperar 2-3 segundos para baseline quieto.
3. Mover el robot **300 mm exactos** (medidos con regla) en línea recta,
   sin rotar, deslizando (no levantar).
4. Esperar al final 2-3 segundos.

**Criterio de éxito:** `dist` reportada ≈ 300 mm ±25 mm (8% tolerancia).
Si da más de 50 mm de error, hay calibración mal seteada (linear scalar)
o la superficie sigue siendo subóptima.

### Test 2 — Rotación

1. Repetir el setup.
2. Rotar el robot **90° exactos** sin trasladarlo, alrededor de su centro.
3. Verificar `dhdg ≈ 90° ±5°`.

Si error > 10°, hay desalineación física entre los 2 OTOS (afecta el
heading calculado por diferencial), o el `OTOS_SEPARATION_MM = 200` del
`config_down.h` no coincide con la separación física real.

### Test 3 — Round trip

1. Mover 300 mm adelante.
2. Mover 300 mm atrás (de regreso al origen).
3. Verificar que `xf ≈ x0` y `yf ≈ y0` (±25 mm).

Mide drift acumulado. Si hay drift significativo (> 50 mm en 600 mm
totales), el OTOS está perdiendo tracking momentáneamente — probable
problema de superficie o velocidad de movimiento.

## Criterio de cierre

- [ ] Test 1 (lineal): error < 25 mm sobre 300 mm.
- [ ] Test 2 (rotación): error < 5° sobre 90°.
- [ ] Test 3 (round trip): drift acumulado < 50 mm.
- [ ] Resultado documentado en journal nuevo.
- [ ] Si algún test falla, identificar causa (superficie, calibración,
      separación física) y actualizar `OTOS_SEPARATION_MM` o el
      `setLinearScalar`/`setAngularScalar` correspondiente.

## Cambios de estado

- 2026-05-24: creada al cierre de la sesión de activación de la lib OTOS
  (TASK-012). Validación cuantitativa pendiente por superficie A4
  inadecuada.
