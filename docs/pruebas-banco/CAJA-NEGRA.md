# CAJA NEGRA de la CENTRAL — log completo de corridas para análisis y calibración

> **Origen (2026-06-11, Gustavo):** "el delantero se mueve para todos lados, muy
> rápido, no lo puedo probar a ojo y no siempre tengo cancha. Quiero un log de
> corrida COMPLETO que pueda bajar y pasarte para que analices qué está pasando."
> Diseñado como experimento de ingeniería: capturar TODO lo que el robot vio,
> decidió y aplicó, con timestamps, para reconstruir la corrida offline.

## Decisión de diseño: dónde se guarda y por qué

| Opción | Veredicto |
|---|---|
| **RAM + volcado USB post-corrida (v1, IMPLEMENTADO)** | ✅ Cero hardware nuevo, cero riesgo al loop de 100 Hz (escribir en RAM cuesta ~ns), 90 s de historia circular. El USB del Teensy es ~1 MB/s: volcar 90 s tarda <1 s. El "cable lento" no aplica: el cable se conecta DESPUÉS de la corrida (o queda puesto en pruebas de escritorio). |
| microSD en la CENTRAL (fase 2) | La CENTRAL es **Teensy 4.1 con ranura microSD integrada** → sesiones de horas y partidos completos en Incheon. Requiere SdFat + buffer (las escrituras SD tienen picos de 10-50 ms que NO pueden tocar el loop). Se hace post-demo si la v1 queda corta. |
| Streaming USB en vivo | Ya existe para sensores del TOP (`TOP_DEBUG_TELEMETRY` + app `tools/monitor-base`). Sirve en escritorio; en cancha el cable arrastrado molesta. La caja negra lo complementa con la mitad que faltaba: **las decisiones de la CENTRAL**. |
| Radio (ESP32, telemetría F1) | Roadmap 2027 (ya en TDP §4.6) — no para esta semana. |

## Qué graba (cada 20 ms = 50 Hz, los últimos ~90 s)

Una fila por muestra con TODO lo que la CENTRAL ve y hace:

| Columna | Qué es |
|---|---|
| `t_ms` | reloj del robot (millis) |
| `state` | estado FSM exacto (`ATK_SEARCH`, `ATK_POSITION`, `ATK_PUSH`, `GK_PATROL_MOVE`...) |
| `match,snap,ball_vis,goal_vis,hdg_valid,line,imminent` | flags 0/1 (juez, snapshot fresco, pelota/arco vistos, heading válido, línea pisada, borde inminente) |
| `hdg_deg` | rumbo del BNO (vía TOP) |
| `ball_x,ball_y` | pelota en mm, frame robot (lo que las CÁMARAS dijeron) |
| `ball_vx,ball_vy` | velocidad estimada de la pelota (mm/s) |
| `goal_deg` | ángulo al arco rival (cámara) |
| `cmd_vx,cmd_vy,cmd_w_dps` | lo que la FSM ORDENÓ (mm/s y °/s) |
| `pwm1,pwm2,pwm3` | lo que REALMENTE llegó a cada motor (signed, post-pisos/impulso) |
| `line_deg` | ángulo de la línea (DOWN) |
| `min_obst` | obstáculo más cercano (ToF del TOP, mm) |

Con eso se reconstruye la cadena completa **sensado → decisión → actuación** de
cada instante: si el robot "se fue para cualquier lado", el CSV dice si fue la
cámara (pelota falsa), la FSM (estado equivocado), la cinemática (cmd raro) o
los motores (PWM no corresponde al cmd).

**Qué NO graba la v1 (a propósito):** acelerómetro crudo, ToF individuales y
blobs crudos de cámara — viven en el TOP y NO entran en las decisiones de la
CENTRAL. Si un análisis los pide, se usa la telemetría USB del TOP
(`TOP_DEBUG_TELEMETRY` + `tools/monitor-base`) en paralelo, o se agregan al
snapshot (fase 2).

## Cómo se usa (procedimiento del experimento)

1. **Flashear el env `_bb`** del rol a estudiar (USB a la CENTRAL):
   ```
   pio run -e central_robot2_demo_bb -t upload          (delantero)
   pio run -e central_robot1_arquero_demo_bb -t upload  (arquero)
   ```
2. **Corrida**: robot a la cancha/piso, GO con la app o `g` por el monitor.
   El cable USB puede ir desconectado durante la corrida: la caja graba sola.
3. **Fin de corrida**: STOP (app o `s`). En ese instante la CENTRAL **vuelca el
   CSV automáticamente** por USB. Si el cable no estaba puesto: conectarlo y
   mandar `d` (la historia queda en RAM mientras no se corte la alimentación).
   `x` = borrar historia antes de una corrida nueva.
4. **Capturar en la PC** (cualquiera de las dos):
   - `python tools/blackbox/leer_caja_negra.py COM15` → guarda `corrida_*.csv`.
   - `pio device monitor -f log2file` (guarda TODO el monitor a un `.log`;
     sirve igual, se recorta después).
5. **Análisis**: pasarle el CSV a Claude. Pedidos típicos: "reconstruí la
   corrida", "por qué giró acá", "validá el perfil de velocidad del APPROACH",
   "calibrá el umbral de PUSH".

⚠️ La historia vive en RAM: **se pierde al apagar la batería**. Volcar antes de
apagar. (Persistencia entre apagones = la fase microSD.)

## Experimentos estándar que habilita (con criterio de aceptación)

- **E1 — Perfil de acercamiento**: pelota quieta a 1 m, robot alineado → GO.
  Aceptación: `cmd_vy` decrece suave con la distancia (perfil 400→200), estado
  pasa SEARCH→APPROACH→PUSH→PUSH_BACK sin rebotes de estado.
- **E2 — Geometría del orbit**: pelota a 1 m DESALINEADA 90° del eje → GO.
  Aceptación: POSITION mantiene ~120 mm de gap (de `ball_x/y`), `hdg` apunta al
  eje todo el tramo, sale a APPROACH en <4 s.
- **E3 — Disparo del PUSH**: aceptación: la fila donde `state` pasa a `ATK_PUSH`
  tiene `dist(ball) < 80 mm` y `|eje| ≤ 12°`; el PUSH dura 500 ms con `cmd_vy=700`
  y los `pwm*` correspondientes; PUSH_BACK retrocede.
- **E4 — Lazo de rumbo**: en cualquier tramo recto, `hdg_deg` vs `cmd_w_dps`
  deben moverse en oposición (el PID corrige); si `hdg` diverge con `cmd_w`
  saturado → revisar signos/ganancias.
- **E5 — Cazas falsas**: corrida con el entorno real de la demo; cada entrada a
  APPROACH/POSITION con `ball_vis=1` y la pelota NO en cancha = falso positivo
  de color (recalibrar LAB con el kit).

## Implementación (para el equipo)

- `src/central/blackbox.{h,cpp}` — gateado `-DCENTRAL_BLACKBOX` (sin el flag:
  binario idéntico, cero costo). Buffer `DMAMEM` (RAM2) de 4500 × 36 B ≈ 158 KB.
- Hook: `blackbox_tick(cmd)` al final del lazo de control de `main_central.cpp`.
- PWM real: `motors_get_applied_pwm()` (nuevo getter en motors_zircon, costo 3
  stores por apply).
- Comandos serie `d`/`x` integrados al lector del juez-PC (mismo gate de banco).
