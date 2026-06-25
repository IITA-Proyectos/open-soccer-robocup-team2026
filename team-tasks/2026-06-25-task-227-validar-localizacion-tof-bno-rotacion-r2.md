# TASK-227 — Validar en banco la localización ToF con BNO (rotación-aware) en ROBOT2

- **Placa:** TOP (ROBOT2) — luego CENTRAL para el lazo cerrado
- **Asignado:** equipo (banco) — Gustavo / Enzo / Virginia
- **Prioridad:** P2 (mejora del arquero; capitaliza 2027 — localización por landmarks)
- **Estado:** abierta
- **Depende de:** lecturas hechas (informe `research/in-progress/2026-06-25-tof-localizacion-arquero-informe.md`),
  módulo `src/shared/keeper_xy_walls.h` (heading-free, ya en repo), env `top_robot2_pri_tofmaxrange`.
- **Relacionada:** TASK-221 (keeper_xy_walls heading-free), TASK-225/226 (ToF montaje/8×8).
- **Firmware LISTO (compila + host-test 16/16, competencia byte-idéntica):** env **`top_robot2_pri_xypose`**
  = max-range + `keeper_xy_from_walls_heading` (cos(θ) a la distancia + gating |θ|>30°). Perillas FIJAS v1:
  wall_reach 1300, gate 3000 cdeg, trim 35. **NO validado en HW — esta TASK es esa validación.**
  - Flashear: `pio run -e top_robot2_pri_xypose -t upload`
  - Ver pose XY: `python -m monitor_base --field --port COMxx` (dibuja snap.my_x/y/heading; x=… y=… hdg=… conf=…)
  - Anclar cero: con el robot mirando al arco, comando `IMU ZERO` (o arrancar mirando al arco).

## Contexto

Las lecturas de banco (2026-06-25, 5 posiciones) confirman que **se puede localizar al arquero con los 4
ToF**: X sólido, alcance con negro ~1,4–1,5 m, Y bueno cerca del fondo. El BNO primario anda bien (estable,
signo/magnitud correctos) PERO **su cero es relativo al encendido** y **el heading del snapshot no aplica
offset de cancha** (auditoría de código 2026-06-25). Falta cerrar dos cosas en hardware: **anclar el cero
del heading al arco** y **validar la selección de zonas perpendiculares + la pose bajo rotación**.

## Qué validar / cerrar

### A) Anclar y caracterizar el CERO del heading (precondición de todo lo demás)
1. **Reproducir el "47°":** con el firmware actual (`top_robot2_pri_tofmaxrange`, trae USB monitor), hacer
   **5–8 boots seguidos QUIETO apuntando al arco rival** (marcar la pose con cinta). Anotar
   `snap.my_heading_deg` de cada boot. Criterio de reproducción: ≥1 boot da |heading| > ~5°.
2. **Correlacionar con el sellado:** en cada boot anotar (print temporal en el getter
   `sensors_imu_get_heading_centideg()` — común a `build_snapshot` y `snapshot_emitter`) el `gyro_calib` y
   el std de las HEADING_SAMPLES en el instante de `capture_offset`. ¿Los boots malos coinciden con
   `gyro_calib<3` / std alto?
3. **FIX C (red operativa, sin reflashear):** con el robot mirando al arco, disparar **`IMU ZERO`** desde el
   monitor (`sensors_imu_recalibrate_zero()`). Confirmar que `snap.my_heading_deg` queda ~0 y que el comando
   está cableado. Este es el procedimiento de campo recomendado.
4. **(Opcional) FIX B:** si se decide robustecer el boot, gatear la captura en `gyro_calib≥3` + retry +
   **fallback por timeout** (que NO cuelgue el boot si el gyro nunca calibra). Gateado `#ifdef` → competencia
   byte-idéntica. **Gatillo B→A:** si con el robot BIEN al arco y quieto el |heading| sigue dando un sesgo
   sistemático >3° → el problema es de FRAME, evaluar FIX A (marco-cancha) auditando ANTES cómo CENTRAL
   consume el heading (riesgo de doble des-rotación).

### B) Validar la selección de zonas perpendiculares + la pose bajo rotación
5. **Estático con rotación conocida:** robot en posiciones marcadas (centro de fondo, corrido, corner),
   rotado a 0°, ±15°, ±30°, ±45°. Para cada (posición, rotación), leer zonas + heading
   (`tof_zonas_promedio.py` / `probe_heading_diag.py`) y aplicar la **fórmula del informe §5.2**:
   `D_perp = d_zona·cos(ε)·cos(β)`, `β = (θ+φ_montaje+α) − ψ_pared`. Verificar que `D_perp` de la pared da la
   distancia real (cinta/metro) **a pesar de la rotación**, dentro de **±5 cm con la pared cercana visible**.
6. **Confirmar los signos de α/ε por sensor** (mapeo crudo→físico: rotaciones FRONT/BACK 180°, RIGHT 90°,
   LEFT 270°). Es el dato que la fórmula asume y hay que fijar en banco.
7. **Pose:** con el cero anclado (paso 3), comparar `x, y, θ` calculados vs metro en cada punto.
8. **Compuerta:** verificar que cuando una pared cae fuera del FoV (|β| grande), ese eje se marca inválido
   (no inventa pose).

## Criterio de cierre

- El cero del heading queda **anclado al arco** (FIX C confirmado; o FIX B si se implementa) y |heading| ≤ 2–3°
  mirando al arco, estable.
- La fórmula con BNO da la distancia de pared correcta (±5 cm con pared cercana) en al menos 0°, ±30°, ±45°.
- La pose `x,y,θ` coincide con el metro dentro de tolerancia en las posiciones de fondo.
- `top_robot2_pri` (competencia) sigue **byte-idéntico** si se agregó algún flag (verificar hash del `.hex`).

## Escape / rollback

Reflashear `top_robot2_pri` (competencia). Todos los cambios de firmware van **gateados** → el binario de
competencia no se toca. La selección de zonas / pose con BNO vive en su propio env de banco.
