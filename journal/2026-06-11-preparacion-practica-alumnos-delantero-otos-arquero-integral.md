---
title: "Preparación de la práctica 2026-06-12 — delantero sin gyro por OTOS (Elías/R1) + arquero integral con caja negra (Virginia/R2)"
date: 2026-06-11
author: "Claude (sesión coach, post-demo) — diseño y firmware; el equipo humano valida en cancha"
status: firmware listo (gate verde + 8 envs pio SUCCESS) — TODO el hardware pendiente de banco/cancha
---

# Preparación práctica con alumnos 2026-06-12

## Pedido de Gustavo (post-demo 2026-06-11)

1. Mejorar el **sistema de registración de datos** y pulir con **robot 2** (más
   sensores sanos). Virginia → programa integral de ARQUERO: patrulla lateral
   cubriendo el arco, despeja con fuerza cuando ve la pelota CERCA, vuelve a su
   puesto.
2. **Robot 1** → probar "patear" con **odometría de piso** (sin gyro: los BNO
   quedaron desconectados). Elías → DELANTERO: busca girando, se aproxima,
   orbita hasta alinear pelota-arco, y empuja con fuerza EN RECTA controlado
   por OTOS.
3. Árbitro start/stop + no salirse de la cancha (línea) en ambos; freno
   anti-choque por ultrasonido como **segunda instancia**.
4. Docs UNO POR ALUMNO, claros, con secuencia progresiva que quepa en las
   **2 horas** de clase, y captura de datos para análisis posterior del coach.

## Qué se construyó (todo ADITIVO y gateado; sin flag = binario idéntico)

### Firmware

- **`src/shared/atk_nogyro.h`** (PURO, host-testeado, 19 tests nuevos):
  - `nogyro_attack_axis` — cascada cámara > BNO > **OTOS** (nuevo 3er fallback).
  - `nogyro_yaw_hold_omega_degps` — P sobre el yaw OTOS con clamp ±40°/s y
    **bail-out a 45°** (signo invertido / yaw basura → mejor no corregir).
  - `nogyro_push_dir` — dirección de empuje CONGELADA (vector unitario a la
    pelota al comprometerse; el omni empuja en diagonal sin rotar).
  - `nogyro_obstacle_gate_vy` — freno anti-choque (corta solo vy>0; 65535 y
    snapshot stale NO frenan).
- **`strategy.cpp`** — 4 zonas `#ifdef`:
  - `ATK_SEARCH_SPIN_ONLY`: buscar girando EN EL LUGAR (sin avance a ciegas).
  - `ATK_OTOS_NOGYRO`: eje por OTOS + **gatillo de PUSH geométrico** (pelota
    sobre el eje a <150 mm — el clásico "|ax.rel|≤12°" exige rotar a apuntar,
    cosa que sin lazo de rumbo nunca se cumple y el PUSH jamás dispararía) +
    PUSH/PUSH_BACK por la dirección congelada con yaw-hold OTOS.
  - `ATK_OBSTACLE_STOP_MM=<mm>`: gate anti-choque en `strategy_tick`, SOLO rol
    delantero (el arquero patrulla con la pared del arco atrás → el min sin
    dirección lo dejaría mudo).
- **`main_central.cpp`**: `CENTRAL_FORCE_ROLE_ATTACKER` (espejo del override
  GK; delantero sobre hardware R1 cuyo default es arquero) + campo **`otos=`**
  en el panel (chequeo de signo del yaw a mano — paso 2 de Elías).
- **Caja negra v1.2**: columnas `otos_fresh,otos_x,otos_y,otos_hdg` (en R1 sin
  BNO el OTOS ES el rumbo → sin esto el "empuje recto" no se audita). Rec pasa
  de 36 a 43 B → ~190 KB DMAMEM (RAM2 con 313 KB libres, verificado en build).
- **Envs nuevos** (platformio.ini, sección "PRÁCTICA 2026-06-12"):
  `central_robot2_arquero_patrol_bb`, `central_robot2_arquero_bb` (Virginia),
  `central_robot1_delantero_practica`, `..._practica_bb`, `..._practica_obst_bb`
  (Elías; el `_obst` lleva `ATK_OBSTACLE_STOP_MM=250`).

### Herramientas de análisis

- **`tools/blackbox/analizar_corrida.py`**: parsea v1.2 (tolerante a CSVs
  viejos), detector **EMPUJE TORCIDO** (deriva de yaw OTOS >10° dentro de cada
  tramo `ATK_PUSH`), detector **OTOS INTERMITENTE** (pose fresca <50% del
  tiempo de ataque), y curva `otos_hdg` en el panel 4 del PNG. Smoke-test con
  CSV sintético: detecta un empuje torcido de 19.2° plantado a propósito. ✅

### Documentos para los alumnos (los entregables pedidos)

- `docs/pruebas-banco/PRACTICA-2026-06-12-VIRGINIA-ARQUERO-R2.md` — 5 pasos +
  cierre en 2 h: preparación → calibración línea → panel → patrulla sin pelota
  (×3 CSV) → INTERCEPT/CLEAR con pelota (×5 CSV) → reporte. Criterios medibles
  por paso, tabla de problemas, qué NO tocar, mini-glosario de estados.
- `docs/pruebas-banco/PRACTICA-2026-06-12-ELIAS-DELANTERO-R1.md` — 8 pasos +
  cierre: preparación → línea → **verificación de signo del OTOS a mano**
  (gate del día) → FSM en el aire → buscar/llegar → órbita → **empuje recto
  MEDIDO CON CINTA (<15 cm de desvío lateral a 1 m, 4/5)** → gol completo →
  (2ª instancia) freno anti-choque con caja. Ídem criterios/tabla/glosario.

## Decisiones de diseño que conviene recordar

1. **Reuso máximo**: el arquero de Virginia es la FSM v3.3 EXISTENTE — la
   práctica solo le agrega caja negra y el debut de INTERCEPT/CLEAR con pelota
   en R2. Cero código de conducta nuevo en el camino del arquero.
2. **Gatillo de empuje geométrico (no angular) sin gyro** — el cambio
   conceptual del delantero R1. Empujar EN LA DIRECCIÓN DE LA PELOTA con el
   omni evita necesitar lazo fino de rumbo; la condición "pelota sobre el eje"
   garantiza que esa dirección ≈ dirección al arco.
3. **Bail-out por diseño**: el signo CCW+ del yaw del OTOS NO está validado en
   banco (el precedente de KICKOFF/WP-2A lo asume, nunca se corrió con dato
   real). Por eso (a) el paso 2 de Elías lo verifica A MANO antes de mover
   nada, y (b) el yaw-hold se rinde a 45° de error en vez de perseguir un
   signo invertido. Lección heredada de la J/U del retroceso del arquero.
4. **El freno anti-choque NO va en el arquero** y solo corta el componente de
   avance: `min_obstacle` no trae dirección (min de 4 ToF + HC-SR04) y cerca
   de paredes daría falsos frenos permanentes.
5. **Panel `otos=` siempre visible** (no gateado): es diagnóstico puro y evita
   el clásico "¿por qué no orbita?" → `otos=N` responde en 2 segundos.

## Verificación (lo que Claude SÍ puede cerrar)

- Gate host: **58 suites PASS / 0 FAIL (798 tests)**, incluye `test_atk_nogyro`.
- `pio run`: **8 envs SUCCESS** — los 4 nuevos + regresión de
  `central_robot2`, `central_robot2_demo_bb`, `central_robot1_arquero_demo_bb`,
  `central_robot1_delantero_practica`. RAM/flash holgados.
- Analizador: py_compile + smoke-test sintético v1.2. ✅

## Lo que SOLO el banco/cancha puede cerrar (la práctica es ese test)

| # | Riesgo | Mitigación ya puesta |
|---|---|---|
| 1 | **Signo del yaw OTOS invertido** | Paso 2 de Elías (chequeo a mano) + bail-out 45° + 1 línea de fix si falla |
| 2 | Frescura real del OTOS en la CENTRAL (`down[rx]` con `lost` alto ya visto en banco) | Panel `otos=` + detector OTOS INTERMITENTE |
| 3 | Órbita sin rotación puede ser lenta/fea con pisos {70,70,107} | Velocidades de POSITION ya serenas (400); CSV para tunear |
| 4 | INTERCEPT/CLEAR de R2 debutan con pelota real (falsos naranjas) | Fallback `_patrol_bb` sin culpa + caja negra del desastre |
| 5 | Umbral 250 mm del anti-choque vs distancia de frenado real | Env `_obst` separado (2ª instancia), umbral por flag |

## Temas-a-analizar que siguen abiertos (sin cambios hoy)

Los de la auditoría 2026-06-11: freno de emergencia vs anti-flapping del GK ·
statics de GK_PATROL sin reset · trilateración conf=70 con heading muerto ·
soft-resync del imu_fusion elige al BNO congelado · re-apretar pulsos GK
(35→20°) · TASK-041 (deadline form TDP, P0) · TASK-022 (visión LAB en venue).
La práctica NO los toca; los CSVs de mañana pueden alimentar varios.
