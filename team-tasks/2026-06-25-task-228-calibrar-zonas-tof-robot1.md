# TASK-228 — Calibrar las zonas ToF del OTRO robot (ROBOT1) para la misma localización

- **Placa:** TOP (ROBOT1)
- **Asignado:** equipo (banco) — Gustavo / Enzo
- **Prioridad:** P2 (paridad R1↔R2; la localización por paredes sirve a los dos robots)
- **Estado:** abierta
- **Depende de:** que en R2 la fórmula con BNO esté validada (TASK-227) para replicar el procedimiento.
- **Relacionada:** TASK-216/217/222/223 (R1: BNO unificado, firmware RT, acople ToF), TASK-227 (R2).

## Contexto

Las lecturas y la fórmula de localización por paredes (informe
`research/in-progress/2026-06-25-tof-localizacion-arquero-informe.md`) se hicieron en **ROBOT2**. La misma
fórmula vale para R1, **pero cada robot tiene su propio montaje de ToF**: altura, inclinación y, sobre todo,
el **mapeo crudo→físico de zonas por sensor** (qué columna/fila es azimut/elevación, las rotaciones
FRONT/BACK/RIGHT/LEFT) pueden diferir. Hay que **caracterizar las zonas de R1** para que la fórmula
`D_perp = d_zona·cos(ε)·cos(β)` use los signos/índices correctos.

## Qué hacer

1. **Flashear R1** con el equivalente max-range (env análogo a `top_robot2_pri_tofmaxrange` para R1; si no
   existe, crearlo gateado siguiendo el patrón — `-DTOP_TOF_MAXRANGE -DTOP_ENABLE_TOF_CONTINUOUS
   -DTOP_BNO_SETTLE_MS=3000`). **Confirmar competencia (`top_robot1_pri`) byte-idéntica.**
2. **Mapeo crudo→físico por sensor:** con `tof_zonas_promedio.py` y el robot a distancia conocida de una
   pared (clara y negra), identificar para cada ToF **qué fila es elevación y qué columna es azimut**, y los
   **signos** (las rotaciones FRONT/BACK 180°, RIGHT 90°, LEFT 270° pueden no ser iguales que en R2).
3. **Alcance con negro en R1:** alejar el robot de la pared negra y anotar hasta qué distancia la franja
   perpendicular sigue viéndola (esperado ~1,4–1,5 m, pero medir).
4. **Validar X** con el método de R2 (suma `LEFT+RIGHT+robot ≈ ancho`) en 2–3 posiciones.
5. **Anclar el cero del heading** de R1 igual que R2 (FIX C `IMU ZERO` mirando al arco; ver TASK-227).
6. **Documentar las diferencias R1 vs R2** (mapeo, alcance, tof_offset) para que la config de la fórmula sea
   por-robot (identidad de robot ya existe en EEPROM, TASK-215).

## Criterio de cierre

- Mapeo crudo→físico de las 16 zonas documentado para los 4 ToF de R1.
- Alcance con negro medido en R1.
- X validado (±5 cm con pared cercana) en 2–3 posiciones.
- Diferencias R1↔R2 anotadas (para parametrizar la fórmula por robot).
- `top_robot1_pri` (competencia) byte-idéntica.

## Escape / rollback

Reflashear `top_robot1_pri`. Todo gateado.
