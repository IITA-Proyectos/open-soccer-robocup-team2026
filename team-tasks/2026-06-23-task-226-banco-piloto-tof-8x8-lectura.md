---
task: TASK-226
fecha: 2026-06-23
asignado: equipo (banco con R2 + TOP)
prioridad: P2
placa: TOP (ROBOT2)
estado: abierta
env: top_robot2_pri_tof8x8
solicitado-por: Virginia
bloquea: la fase de localización por ToF en 8×8 (selección 2-por-columna + pose en TOP)
---

# TASK-226 — Validar en banco que los 4 ToF LEEN en 8×8 (piloto) y medir el impacto

## Qué es

Piloto gateado (`-DTOP_TOF_8X8`, env `top_robot2_pri_tof8x8`) que pone los 4 ToF en 8×8 (64 zonas) y
emite las 64 zonas crudas + el `dt_us` por sensor por un mensaje debug `ZN8`. Visualizador nuevo
`python -m monitor_base --tof8x8` (4 grillas 8×8). Competencia byte-idéntica. Detalle:
`journal/2026-06-23-piloto-tof-8x8-visualizador.md`.

**Claude NO puede cerrar esto** (regla #1): `pio SUCCESS` + host-tests no prueban que el 8×8 lee bien
ni que el loop/BNO aguanten. Lo cierra el equipo en banco.

## Criterios de cierre (los 4 — si fallan 3 o 5, el piloto NO justifica la fase completa)

1. **Lee 8×8:** salen líneas `ZN8` con 64 valores, `idx` rota 0..3, las 4 grillas se pueblan con
   distancias coherentes vs un objeto a distancia conocida.
2. **Resolución vertical real:** objeto tapando la mitad inferior del FOV → la **fila activa de las 8
   se desplaza al subir/bajar el objeto** y el mm sigue la distancia (300→600→900). Esto es lo que
   4×4 NO da (en 4×4 el objeto cae en 1-2 filas gruesas). **Compará explícitamente contra 4×4.**
3. **Loop tolerable (PEOR caso):** mirar `dt_us`/fps por sensor con los 4 a 8×8. ¿El tick del ToF se
   estira tanto que el loop "late"? Anotar el `dt_us` máximo. Si es intolerable → bajar a menos
   sensores en 8×8 o bajar `TOF_RANGING_FREQ_HZ`.
4. **BNO NO se congela (BLOQUEANTE):** girar el robot ≥60 s con los ToF a 8×8 y confirmar que el yaw
   del heading se mueve sin saltos/congelamientos. Si se congela, el piloto **falla** aunque las
   grillas se vean lindas.

## Cómo

1. `pio run -e top_robot2_pri_tof8x8 -t upload` (TOP de R2).
2. `python -m monitor_base --tof8x8` (desde `tools/monitor-base/`) — o `pio device monitor -b 115200`
   para ver el texto crudo `ZN8`.
3. Correr los 4 criterios. Anotar el `dt_us` máximo y el resultado del giro (BNO).
4. Volver a competencia: `pio run -e top_robot2_pri -t upload`.

## Resultado → decide la fase siguiente

- **Pasa los 4** → seguir con la **selección 2-por-columna + pose por paredes en TOP** (idea de
  Virginia: mantiene el contrato `z` en 16, no rompe el monitor 4×4, CENTRAL no cambia).
- **Falla 3 (loop) o 4 (BNO)** → 8×8 con los 4 no es viable; evaluar menos sensores o bajar la
  frecuencia. **No avanzar a la fase completa sin esto.**

## Relacionada
TASK-225 (los ToF leen el piso — altura de montaje). Si además se baja el montaje (≤ altura de pared),
mejora la geometría independientemente de 4×4 vs 8×8.
