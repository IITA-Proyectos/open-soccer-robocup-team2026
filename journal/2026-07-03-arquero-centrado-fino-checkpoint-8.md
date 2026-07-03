---
title: "Arquero: CENTRADO FINO (8º checkpoint, gateado) — banda muerta angular 8°→5°"
date: 2026-07-03
participantes: "María (pedido + banco), Claude (análisis + implementación)"
tipo: sesion-firmware
---

# 2026-07-03 — Arquero: centrado fino (checkpoint #8, `_centrado_fino`)

## Contexto

María reportó del banco con `_evita_lejos` (#6): el arquero a veces queda "centrado" **lejos** de la
pelota y **no de frente**. Análisis contra el código (`amix_fsm.cpp` / `amix_config.h`):

1. **"Lejos"**: el seguimiento lateral usa una banda muerta POR ÁNGULO (`AMIX_TOL_CENTRADO_DEG=8°`,
   fix 2026-06-21 que reemplazó los umbrales en mm sin calibrar). El corrimiento lateral aceptado crece
   con la distancia: ≈ distancia × tan(8°) ≈ 14% → pelota a 1,5 m = hasta ~21 cm corrido y el FSM lo da
   por alineado. No es bug: es la geometría de la tolerancia.
2. **"No de frente"**: en `esperar_quieto` NADA re-orienta al robot (el strafe corrige rumbo solo grueso,
   3 bandas; alineado = `parar()` a secas). La orientación solo se corrige en la secuencia de despeje y
   en el homing/re-homing. Robot golpeado/derivado queda torcido, y su cono de ±8° se mide desde el
   frente torcido → ambos síntomas se refuerzan.

## Qué se hizo (aditivo, gateado — regla de oro respetada)

- **`amix_config.h`**: `AMIX_TOL_CENTRADO_DEG` gateado — `-DARQMIX_CENTRADO_FINO` → **5°** (era 8°, que
  queda como default sin el flag). Comentario con la matemática y la titración (oscila → 6; corrido → 4).
- **`platformio.ini`**: env nuevo **`central_robot2_arqueromix_centrado_fino`** = copia del #7
  (`_evita_lejos_rehome`) + el flag. Hereda el re-homing, que además mitiga el "no de frente" (cada
  re-homing endereza). El "no de frente" con pelota a la vista queda como tema aparte (posible #9,
  requiere diseño: el cero del heading del TOP deriva).
- **TASK-122** (`team-tasks/2026-07-03-task-122-validar-arquero-centrado-fino-banco.md`): plan de banco
  con el riesgo principal (banda angosta + piso de PWM = micro-strafes que no terminan) y criterio de cierre.

## Verificación (host — el banco lo cierra el equipo)

- Los **8 envs compilan SUCCESS**; los **7 checkpoints anteriores byte-idénticos** (md5 del `firmware.hex`
  verificados contra la tabla, incluido el base intocable `_quieto` `8D0168CF81F7CDAF347B0AB89E030E59`).
- md5 del #8: ver tabla en `docs/pruebas-banco/CONTEXTO-ARQUERO-CHECKPOINTS-2026-07-03.md` (actualizada).
- Además se re-verificó la byte-identidad de los 7 tras el merge de main de hoy (`19b13ce`, cambios de
  centralmix, carpetas disjuntas): los 7 md5 OK.

## Pendientes

- Banco TASK-122 (este checkpoint) — SIN validar en hardware.
- Sigue BLOQUEANTE el chequeo pelota vs anti-choque (TASK-121) para #4/#6/#7/#8.
- `git fetch` falló al final de la sesión (SEC_E_UNTRUSTED_ROOT, certificado — problema de red local);
  el pull del inicio de sesión anduvo. Re-intentar antes del próximo cambio.
