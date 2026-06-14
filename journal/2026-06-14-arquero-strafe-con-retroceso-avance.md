# 2026-06-14 — Arquero strafe: sumar retroceso-al-arco + avance antes del barrido lateral

**Pedido (Gustavo):** la patrulla sin pelota que estaba usando (`_patrol_bb`, v3.3) hacía
adelante/atrás (retrocede a la línea, avanza un poco, vuelve atrás) por su lógica de REACQ.
Quería traer lo que ya teníamos: **(1)** después de retroceder y tocar la línea de fondo,
**avanzar un poco más** para despegarse; **(2)** moverse **lateralmente** (derecha hasta la
línea lateral → alejarse → izquierda hasta el otro lateral), con **control de rumbo para
mirar siempre al frente**.

## Diagnóstico

Las dos piezas YA existían, en envs distintos, sin combinar:
- **#1 (retroceso + avance):** estado `GOTO_LINE` en 2 fases (banco 2026-06-09 v2) — lo tenía `_patrol_bb`.
- **#2 (strafe lateral con frente):** `GK_SIMPLE_STRAFE` (env `_strafe_bb`, banco María 2026-06-12 v6):
  strafe der↔izq a 200 mm/s, **rebote por línea lateral** (ve línea a la derecha → va izquierda;
  ignora la de atrás ±135°), **escape ~12 cm** al tocar el lateral, **frente al arco con PI+PFM**.

El `_patrol_bb` tenía #1 pero su barrido v3.3 deriva adelante/atrás (REACQ); el `_strafe_bb`
tenía #2 limpio pero **se salteaba el retroceso** (arrancaba el strafe desde donde lo dejaban).

> Aclaración técnica: Gustavo pidió "control PID" para el frente. El código ya lo tiene como
> **PI + PFM** (corrección por pulsos), NO PID continuo clásico — porque el clásico se probó en
> banco (06-12) y **descontroló** el robot (0→−150° en 3 s; con los pisos {70,70,107} cualquier
> ω continuo desborda las ruedas chicas del strafe). Se mantuvo el PFM (es "el PID que anda acá").

## Cambio (enfoque A, elegido por Gustavo: sumar GOTO_LINE al strafe)

En `src/central/strategy.cpp`, **dentro del `#ifdef GK_SIMPLE_STRAFE`** (solo afecta `_strafe_bb`):
se agregaron 2 fases iniciales a la máquina de estados del strafe, que **reusan la lógica de
GOTO_LINE** (signos `-Y`/`+Y`, `gk_gyro_hold_omega`, detección + margen idénticos):
- **Fase GOTO_BACK (5):** retrocede recto (`-GK_GOTO_LINE_VY_BACK`) con gyro-hold hasta que la
  DOWN ve la línea (de fondo) o timeout → ADVANCE.
- **Fase ADVANCE (6):** avanza (`+GK_ADVANCE_SPEED_MM_S`) hasta que la línea deja de verse
  `GK_ADVANCE_MS` seguidos (o timeout) → MOVE (el strafe lateral de siempre).
- El arranque (en GO) ahora entra por GOTO_BACK; el resto (MOVE/ESCAPE/RESQUARE/SETTLE) intacto.

**Conducta resultante de `central_robot2_arquero_strafe_bb`:** GO → retrocede al arco → avanza
~10 cm → strafe der hasta lateral → escapa → strafe izq hasta el otro lateral → … siempre de
frente al arco. Los demás envs de arquero quedan **byte-idénticos** (el cambio está gateado).

## Verificación

- Host/compilación: `pio run -e central_robot2_arquero_strafe_bb` y `-e central_robot2_arquero_bb`
  → **SUCCESS**. El cambio está 100% dentro del `#ifdef GK_SIMPLE_STRAFE` (los host-tests no lo
  compilan; el path no-gateado se confirmó compilando `_arquero_bb`).
- **Lo cierra el equipo en banco (regla hardware):** flashear `central_robot2_arquero_strafe_bb`,
  dar GO (`g`/ENTER), y confirmar:
  1. Retrocede DERECHO al arco (−Y) — no hacia adelante; toca la línea de fondo.
  2. Avanza ~10 cm y se despega (deja de ver la línea).
  3. Strafe a la DERECHA hasta tocar el lateral → escapa ~12 cm → invierte a la IZQUIERDA hasta el otro lateral.
  4. Mantiene el frente al arco todo el tiempo (PI+PFM); rebota en los LATERALES, ignora la de fondo.
  Caja negra (`_bb`) graba todo; con el CSV se afina (deadband/ki/kp del PFM, velocidades).

## Resultado de banco 2026-06-14 (probado por Gustavo)

- ✅ La secuencia FSM andó: `GOTO_BACK → ADVANCE → MOVE ↔ ESCAPE` (mi cambio funciona).
- ⚠️ 1ª corrida: `hdg=0.0` SIEMPRE → strafe DIAGONAL, sin gyro. Causa: el **BNO de la TOP
  estaba muerto** (heading 0/invalid); R2 no tiene OTOS → sin ese heading no hay control de rumbo.
- ✅ Re-flasheada la TOP (`top_robot2_pri`, era flasheo viejo de la demo) → **el heading arrancó**
  (`hdg` cambia al girar).
- ⚠️ 2ª corrida: "anduvo un poco mejor pero a veces queda parado, hay que empujarlo". El heading
  NO se mantiene (oscila ±37° → diagonal + rebota contra su propio arco + `RESQUARE` se traba).
  Raíz: el PFM de rumbo no sostiene el frente (acople strafe↔giro conocido; nunca había corrido).
- **PENDIENTE (revisión 2026-06-15 9:00 con Virginia + Elías):** sacar el CSV de la caja negra
  (per-motor PWM + hdg + estado), chequear batería, y tunear el PFM / la lógica de rebote con
  datos. Todo documentado en [`docs/pruebas-banco/PRACTICA-2026-06-15-ARQUERO-STRAFE-REVISION.md`](../docs/pruebas-banco/PRACTICA-2026-06-15-ARQUERO-STRAFE-REVISION.md).
