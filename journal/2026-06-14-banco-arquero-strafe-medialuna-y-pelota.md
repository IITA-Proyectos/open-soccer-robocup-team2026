# 2026-06-14 — Banco arquero (María): destrabar línea, medialuna, y centrado de pelota

**Contexto:** sesión de banco con María sobre el arquero R2 (env `central_robot2_arquero_strafe_cam_*`,
strafe lateral + cámara), con una **pelota real en la cancha**. Todo el diagnóstico se hizo con la
**caja negra** (CSV del CENTRAL), no a ojo. Cinco temas, cada uno con su causa de raíz probada.

## 1. Se quedaba TRABADO sobre la línea izquierda (no era el motor)

**Síntoma:** escapaba bien de la línea derecha, pero en la izquierda quedaba clavado 5+ s.
**Causa (CSV):** el **freno de borde** (`EMERGENCY_LINE` en `main_central.cpp`) tiene prioridad sobre
la FSM: si `world_model_imminent_exit() && line_fresh` → `motors_brake()` + `return`, **salteando
`strategy_tick()`**. En la izquierda `imminent_exit` quedaba en 1 → frenaba y salteaba la huida en
cada vuelta → el robot no se movía → la línea seguía inminente → **deadlock** (`emerg=1` todo el
tramo del log). En la derecha `imminent` no disparaba (geometría) y por eso ahí sí escapaba.
**Fix (`7f95291`):** anti-latch. Frena el momento `GK_EDGE_BRAKE_MAX_MS=350 ms` y luego **suelta**,
devolviendo el control a la FSM (cuyo ESCAPE despega hacia adentro). Sigue fail-safe ante un borde
real; se elimina solo el congelamiento permanente. Afecta a todos los roles (es el loop central).

## 2. La medialuna (un error mío que corregí)

**Síntoma:** el strafe arqueaba como medialuna; "estaba peor".
**Causa:** en una sesión previa **yo subí el piso de la trasera 107→120** (mala interpretación de
"120" = distancia de escape, no piso de motor). El modelo MEDIDO (skill `dinamica-omni-3-ruedas`)
dice que **`{70,70,107}` da strafe derecho a 200 mm/s con ω=0** (validado banco R2 2026-06-09). Con
`FLOOR_SCALE`, subir el piso de la rueda **dominante** del strafe sobre-escala el vector de forma
**asimétrica** → yaw parásito direccional = medialuna PEOR. La caja negra lo mostró: con piso 120 la
trasera daba **140 a un lado y 120 (=piso) al otro**; con 107 quedó **simétrica (±108)** y la deriva
a ω=0 bajó de **~18 a ~8 °/s**.
**Fix (`9aa5af1`):** revertir a `{70,70,107}` (valor validado). La deriva base residual la cancela
el integrador del PFM en competencia. **No es band-aid: es volver al número medido en banco.**

## 3. Se salía para los DOS lados (cámara, no robot) — y una corrección mía

**Síntoma:** con la pelota en cancha, se iba afuera a izquierda y derecha.
**Primer diagnóstico (ERRADO):** lo llamé "pelota fantasma". María aclaró que **había una pelota
real** → mi conclusión fue equivocada (la leí constante en (1000,1000) al boot, pero una pelota real
quieta da eso). **Corregido** en código y commit (`130b244`).
**Causa REAL (CSV):** la posición reportada **salta de +1000 a −1000 en 20 ms** (t=15443→15463),
imposible para una pelota real → la **fusión de las 2 cámaras de la TOP** es lo no-confiable (la
pelota cruza de cámara delantera a trasera y el promedio brinca). El centrado, correcto, perseguía
ese dato saltarín → afuera a los dos lados.
**Fix CENTRAL (`b1b17d0`, mitigación):** gate `GK_BALL_MAX_ABS_X_MM=900` → ignora lecturas no
confiables (al borde/atrás) y sigue patrullando. Doble función: un arquero no persigue pelotas anchas.
**Fix de fondo (PENDIENTE, TOP):** firmware de cámara pegajosa `top_robot2_pri_sticky` (arregla la
fusión `fuse_ball_dual`). Hasta flashear la TOP, el centrado va a ser ruidoso aunque la pelota sea real.

## 4. Pelota: por ahora SOLO centrarse y quedar de frente (pedido María)

Se retiró el despeje cerca→empuje (PUSH) y el split cerca/lejos. Ahora: ve la pelota → strafe lateral
para llevar `bx→0` (queda enfrentada), `vy=0` (no se va de la línea), frente con PFM. (Incluido en `9aa5af1`.)

## 5. Veía la pelota pero se ALEJABA (signo invertido)

**Síntoma:** corregía al lado contrario — veía la pelota y se alejaba en vez de enfrentarla.
**Causa:** `GK_BALL_TRACK_SIGN` estaba en `-1` (un fix viejo que quedó al revés).
**Fix (`91424ac`):** `+1` → va HACIA la pelota (pelota a la derecha → strafe derecha → `bx→0`).
**Resultado en banco (María): "queda buscándola a la pelota muy bien"** ✅.

## Pendientes para el equipo (banco)

- **TOP:** flashear `top_robot2_pri_sticky` para que la cámara deje de saltar (causa de raíz del #3).
- **Centrado fino:** probar con rumbo ON (env `_bb`), no en `noheading` (sin hold de rumbo el
  seguimiento es inestable de por sí).
- **Tune PFM** (la oscilación ±37° de sesiones previas) queda para una corrida con CSV dedicada.

## Lección

El piso 107 ya estaba **medido y validado**; subirlo "para compensar" lo rompió. Cuando hay modelo
de banco, se respeta el dato, no la intuición. Y cuando me equivoqué con la "pelota fantasma", se
corrige claro (era pelota real; el problema es la fusión de cámaras).

— Trabajo asistido por Claude (Opus 4.8), banco con María. Testing en hardware real: equipo.
