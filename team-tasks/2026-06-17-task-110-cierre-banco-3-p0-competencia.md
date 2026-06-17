---
task: TASK-110
titulo: "Cerrar en banco los 3 P0 de competencia (FLOOR_SCALE arquero R1 · watchdog CENTRAL · pose XY)"
fecha-creada: 2026-06-17
creada-por: "Claude (sesión coach — Opus 4.8 1M)"
asignado: "equipo (Gustavo / Virginia / Elías) — requiere banco con robot"
prioridad: P0
estado: ABIERTA (firmware listo; banco pendiente — Claude NO puede cerrar)
placas: CENTRAL + TOP
---

# TASK-110 — Cierre de banco de los 3 P0 de competencia

Del estado de situación de las 3 placas (2026-06-17, validado contra código), salieron 3
puntos P0 que afectan el binario de competencia. El firmware quedó LISTO (compila); el
cierre es banco — **esta regla NO la puede cumplir Claude** (regla no-negociable #1).

---

## P0.2 — FLOOR_SCALE en el arquero R1 (YA aplicado al env de competencia)

**Qué se hizo:** se agregó `-DCENTRAL_FLOOR_SCALE` a `[env:central_robot1]` (el env de
competencia del arquero R1). Antes corría SIN el flag → el clamp por-rueda se come las
correcciones de gyro y el arquero "pierde el frente" (síntoma diagnosticado 2026-06-09).
El arquero v3.3 se VALIDÓ con FLOOR_SCALE (en el cuerpo de R2). Es una **corrección hacia
lo validado**, no una feature nueva.

**Por qué es banco igual:** el arquero con FLOOR_SCALE se validó en R2, NO en el cuerpo de
R1. Los pisos `{70,70,107}` de R1 están marcados "a verificar" en `config_central.h`.

**Checklist de banco (R1 como arquero):**
- [ ] Flashear `central_robot1` (ya trae FLOOR_SCALE).
- [ ] Confirmar que en strafe lateral el arquero R1 **mantiene el frente** (no se va de
      rumbo). Si rota/pierde el frente → revisar pisos de R1 + ganancia PFM.
- [ ] Confirmar que la patrulla v3.3 (mover-parar-pulso) se ve fluida, sin giros violentos.
- [ ] Si los pisos de R1 dan rotación parásita → titrar `MOTOR_MIN_PWM[3]` de R1.

---

## P0.1 — Watchdog de hardware en la CENTRAL (env candidato listo, NO en default)

**Qué se hizo:** el watchdog ya existía gateado (`-DCENTRAL_ENABLE_WDT`) con el env
`central_robot1_wdt`. Se creó `central_robot2_wdt` para **paridad** (antes solo R1). El
flag **NO se metió al binario de competencia** (`central_robot1`/`central_robot2`): un
reset espurio en pleno partido es catastrófico → se valida primero.

**Por qué es seguro pero requiere banco:** el WDOG1 (1 s) se alimenta cada vuelta del loop;
el loop normal corre muy rápido (>>1 Hz), así que el riesgo de reset espurio es bajo. Pero
"bajo" no es "cero".

**Checklist de banco (ambos robots):**
- [ ] Flashear `central_robot1_wdt` (R1) y `central_robot2_wdt` (R2).
- [ ] **30 min de marcha continua → 0 resets espurios.** (Al boot, mirar el motivo de
      reset; no debe ser WDT salvo en el hang-test.)
- [ ] Hang test: flashear `central_robot1_wdt_hangtest`, dispararlo, confirmar que el WDOG1
      **resetea en ~1 s** (prueba que el watchdog realmente protege).
- [ ] **Si pasa:** mover `-DCENTRAL_ENABLE_WDT` a `central_robot1` y `central_robot2`
      (binarios de competencia). Recién ahí el robot de competencia queda protegido.

---

## P0.3 — Pose XY (pose_fusion) — env candidato limpio para ambos robots, NO en default

**Qué se hizo:** ya existían `top_robot2_pri_posefusion` (R2) y `top_robot1_pri_xval` (R1,
con extras). Se creó `top_robot1_pri_posefusion` (R1 LIMPIO: solo POSE_FUSION +
FREEZE_DETECT, sin xval/sentinel) para **paridad** y para aislar la variable. El flag
**NO se metió al binario de competencia**: una pose XY mal anclada en partido es PEOR que
no tener pose (el robot navegaría hacia una posición fantasma).

**Por qué es banco (TASK-210/211):**
- El SIGNO/eje del delta OTOS vs el marco de cancha nunca se validó: si está invertido, la
  pose se va al girar.
- El ruido de los 4 ToF no se midió (afecta `seed_tol_mm`).
- El freeze-detector puede dar falso-CONGELADO con el robot quieto.

**Checklist de banco (R1 con OTOS; R2 no tiene OTOS → degrada a localization sola):**
- [ ] Flashear `top_robot1_pri_posefusion` (R1) — la CENTRAL NO cambia.
- [ ] Con el monitor (`python -m monitor_base`, panel "Cancha CENTRAL"): mover el robot a
      mano y confirmar que la pose dibujada SIGUE al robot (no se va en dirección opuesta).
- [ ] Hacer girar el robot 360° y confirmar que la pose NO deriva (valida el signo del
      delta OTOS / la des-rotación rot_lut).
- [ ] Robot quieto 3-5 min → confirmar que el freeze-detector no apaga `heading_valid`.
- [ ] **Si pasa:** decidir si se promueve a competencia (R1). En R2 (sin OTOS) la pose
      depende solo de la trilateración ToF — validar por separado que los 4 ToF anclan.

---

## Criterio de cierre de la TASK

Esta TASK se cierra cuando los 3 checklists estén ✅ en banco. Mientras tanto:
- **P0.2 (FLOOR_SCALE)** ya está en el binario de competencia de R1 — falta solo confirmarlo.
- **P0.1 (WDT)** y **P0.3 (pose XY)** quedan en envs candidatos — el equipo decide
  promoverlos al default tras validar.

Journal: `journal/2026-06-17-resolucion-3-p0-competencia.md`.
