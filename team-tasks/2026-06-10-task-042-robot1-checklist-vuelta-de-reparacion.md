---
id: TASK-042
title: "ROBOT1 vuelta de reparación: checklist de re-validación (motion A-VERIFICAR + FLOOR_SCALE + fixes del TOP heredados)"
date_created: 2026-06-10
assigned: [gustavo, mariaviollaz, elias]
priority: P1
status: pending
estimated_hours: 3
blocks: [usar robot1 en Incheon (2º cuerpo para 2v2)]
tags: [robot1, banco, motores, top, floor-scale]
depends_on: [reparación física de robot1 (sin TASK — trackear acá)]
---

# TASK-042 — Robot1 al volver de reparación: qué re-validar ANTES de confiar en él

> 🔄 **ACTUALIZACIÓN 2026-06-17 (Gustavo):** BNO de R1 reconectados y funcionando. Confirmar en banco
> que los 4 ToF de R1 enumeran (había nota vieja de "ToF derecho no enumera") y que el motion con gyro
> anda. Sigue **ABIERTA** para esos checks.

## Por qué importa (P1)

Robot1 es el **segundo cuerpo** para jugar 2v2 en Incheon y está en reparación
sin tracking. Mientras tanto, TODO el desarrollo 2026-06-09/10 se hizo sobre
robot2, y robot1 **hereda por código** varios cambios que en su hardware están
**A VERIFICAR**:

1. **Motion nuevo** (config_central.h rama ROBOT1): `MOTOR_MIN_PWM={70,70,107}`,
   `MOTOR_EFF_X100={100,100,131}`, kickstart + freno anticipado, `OMEGA_SIGN=-1`
   — todos validados en ROBOT2, copiados a R1 con leyenda A VERIFICAR.
2. **Fixes del TOP** (heredados por código común): ToF round-robin (`a6c0366`) +
   payload VL53L7CX recortado (`bf8ddd4`). Esperado: loop del TOP ~190k/s como R2.
3. **⚠️ GAP detectado en la revisión 2026-06-10:** el env de competencia
   `central_robot1` NO lleva `-DCENTRAL_FLOOR_SCALE` — TODO el tuning del arquero
   v3.2/v3.3 (strafe fiel, pulsos a 40°/s) se desarrolló CON ese flag. Si robot1
   se flashea con el env tal cual, el arquero corre con el clamp viejo por-rueda
   y ω máx 10°/s → repite los síntomas del 2026-06-09 (pierde el frente, no
   endereza) que ya están diagnosticados y resueltos.

## Pasos concretos (en orden, ~1 banco)

1. `diag_central_motors` en robot1: sentido de las 3 ruedas. ⚠️ SUPERADO
   2026-06-11: en la reparación el M2 quedó RECABLEADO DERECHO →
   `MOTOR_INVERT={+1,+1,+1}` (validado en piso). NO volver a `{+1,-1,+1}`
   salvo recableado físico.
2. `diag_central_strafe`: validar pisos {70,70,107} + impulso en ESE chasis
   (fricción distinta a R2 → puede necesitar otro valor de trasera).
3. Flashear `top_robot2_pri` (TOP de R1 recableada a arq. R2; envs
   `top_robot1*` = cableado viejo) y verificar en el panel `[TOP]`: Δ`loop=` ≥ +10.000 por
   línea de 500 ms, `hdg` trackea giro a mano, `min_obst` con números, resync=0.
4. **Decisión de env**: agregar `-DCENTRAL_FLOOR_SCALE` a `[env:central_robot1]`
   (o crear `central_robot1_arquero`) SOLO después de que (2) valide los pisos.
5. Correr el checklist de patrulla (ARQUERO-EN-ROBOT2-PLAN.md FASE 4) en robot1.

## Criterio de cierre

- [ ] Los 5 pasos con resultado anotado en journal (valores reales medidos).
- [ ] `central_robot1` definido (con FLOOR_SCALE o justificación de por qué no).
- [ ] Las leyendas "A VERIFICAR" de config_central.h rama ROBOT1 actualizadas.

## Cambios de estado

- 2026-06-10: creada por Claude (revisión integral: hallazgos P1 "env central_robot1
  sin FLOOR_SCALE" + "reparación de robot1 sin trackear").

## Estado 2026-06-11 (banco nocturno)

- Pasos 1-2 del checklist ✅ HECHOS: motores y strafe validados en piso —
  M2 recableado (quedó DERECHO) detectado y corregido
  (`MOTOR_INVERT={+1,+1,+1}`), pisos `{70,70,107}` confirmados.
- Pendientes NUEVOS:
  - [ ] ToF derecho (LP pin 11) no enumera — revisar cable LP.
  - [ ] BNOs desconectados — re-testear cada módulo en bus propio
        (posible repuesto).
  - [ ] Alimentación de la Teensy TOP nueva: soldar VIN desde el rail +
        cortar el puente VUSB-VIN antes de convivir batería+USB.
  - [ ] Test del giro con `top_robot2_pri` al reconectar BNO.
