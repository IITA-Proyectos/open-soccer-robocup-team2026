---
title: "Verificación pre-banco de la mitad inferior (CENTRAL+motores+DOWN) + review del GK"
date: 2026-06-03
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M)"
status: final
tags: [central, banco, arquero, strafe, comm-down, cinematica, motores, review, coach]
robot: arquero (ROBOT1) — aplica a ambos en la capa de motores
area: control
tipo: analisis
---

# Verificación pre-banco de la mitad inferior + review del GK

## Contexto

Sesión de coach en el worktree `agente/central`. Encargo: preparar/verificar las
pruebas de banco de la **mitad inferior** (motores + CENTRAL + DOWN) — correr
`diag_central_strafe` (FASE B: medir deriva), validar el link DOWN→CENTRAL con
`diag_central_comm_down`, y **revisar el GK existente antes de duplicar conducta**.
Claude NO ejecuta hardware (regla 1 CLAUDE.md) — esta sesión es verificación de
firmware/árbol + análisis, no cierre de TASK de banco.

## Qué se hizo

1. **Protocolo de sesión.** `git fetch`; la rama `agente/central` ya contenía todo
   `origin/main` (`HEAD..origin/main` vacío → merge no-op). Leídos
   `ESTADO-ACTUAL.md` + `FUENTES-DE-VERDAD.md`.
2. **Compilación (árbol sano).** `central_robot1` **SUCCESS** (2.98 s). Los 3
   diag de banco que el equipo va a flashear: `diag_central_strafe_robot1`,
   `diag_central_comm_down`, `diag_central_rx_all` → **SUCCESS** los 3.
3. **Verificación de Serial en CÓDIGO** (no sólo docs). Confirmado que coinciden
   con el mapa UART vigente (banner ESTADO-ACTUAL 2026-06-02):
   - `diag_central_comm_down.cpp:48` → `#define DOWN_UART Serial1` ✓
   - `diag_central_rx_all.cpp:38-39` → DOWN=Serial1 (pin 0), TOP=Serial7 (pin 28) ✓
   - `comm_down.cpp:51` (producción) → `Serial1.begin(230400)` ✓
   - `diag_central_strafe.cpp` → sin UART (open-loop sobre `motors_zircon`) ✓
   Toda la cadena diag↔producción es consistente: **DOWN=Serial1, TOP=Serial7**.
4. **Review del GK** (`strategy.cpp::goalkeeper_tick`, paso 4).
5. **Fixes de seguridad de docs/comentarios** (ver "Archivos tocados").

## Qué se observó (sin hardware — verificación de código + cruce con journals)

### Hallazgo 1 (el importante) — `diag_central_strafe` corre sobre cinemática NO calibrada
`diag_central_strafe.cpp:144` → `motors_apply_command()` →
`motors_zircon.cpp:120` → `inverse_kinematics(WHEEL_ANGLES_DEG)` con
`WHEEL_ANGLES_DEG={60,-60,180}`, marcado **"TENTATIVO — confirmar/medir con Enzo"**
(`config_central.h:65-72`). El journal del **2026-06-01** (María, banco) ya
documentó que **la cinemática genérica da CÍRCULOS** y que el arquero que SÍ
anduvo usa **control directo** de motores (`diag_central_line_sweep`), NO esta
cinemática. ⇒ La "deriva" de FASE B puede estar **dominada por la cinemática**, no
por la falta de heading-hold. Riesgo de **falso positivo** ("hace falta v2").

### Hallazgo 2 — contradicción en la calibración de M2 entre dos sesiones de banco
- 2026-05-29 (`DIAG-CENTRAL-MOTORS.md`): `MOTOR_DIR={+1,+1,+1}`, los 3 consistentes,
  **sin inversión**.
- 2026-06-01 (journal): **"M2 tiene polaridad INA/INB invertida por hardware"**
  (`ROT_M2=-1` en `diag_central_line_sweep.cpp:84`).
`motors_zircon.cpp` (producción) **no** tiene inversión por motor (mezcla uniforme
`PWM>0→INA=HIGH`). Si M2 está invertido en HW, producción lo maneja al revés.
**Hay que reconciliar cuál es la verdad para ESTE robot antes de confiar en el strafe.**

### Hallazgo 3 — dos "arqueros" en paralelo (riesgo de duplicación de conducta)
- `strategy.cpp::goalkeeper_tick` (producción): PATROL/INTERCEPT/CLEAR/LINE_AVOID,
  sigue la línea por `cross_track` (Capa 3, fallback exacto a profundidad), emite
  `MotorCommand`→`motors_zircon`→cinemática.
- `diag_central_line_sweep` (banco, validado 2026-06-01): mismo "seguir línea por
  cross_track" pero con **control directo** + PID de heading sobre el **BNO local
  de CENTRAL** (`imu_get_heading()`), que **no está conectado** (heading viene de
  TOP) → ese PID está muerto. Es el blocker del semicírculo del 2026-06-01.

### Hallazgo 4 — en PATROL/INTERCEPT el GK de producción NO sostiene heading (omega=0)
`goalkeeper_tick` sólo usa `heading_pid` en **CLEAR**. En PATROL e INTERCEPT no
setea `omega` → omega=0, **misma limitación open-loop que `diag_central_strafe`**.
⇒ la deriva que mide FASE B es directamente la deriva que tendrá el GK real
patrullando. El test es un proxy fiel del GK (no un juguete) — pero arrastra el
Hallazgo 1.

### Hallazgo 5 — riesgo de signo de omega (runaway) — latente, ya en producción
`DIAG-CENTRAL-MOTORS.md` advierte que en `kinematics.cpp` `+omega` = horario físico
(convención estándar = antihorario). CLEAR ya usa `heading_pid` con esa cinemática
**sin validar en HW**. Cualquier heading-hold (CLEAR hoy, o v2) puede **corregir
para el lado contrario** si el signo está mal. Validar con giro chico ANTES de v2.

## Conclusión

- **Árbol y firmware listos para banco**: compila todo, Serial verificado en código
  (DOWN=Serial1 pin 0, TOP=Serial7 pin 28; diag y producción coinciden).
- **Pero el plan "correr strafe y medir deriva" tiene un confound**: la cinemática
  no calibrada (+ posible M2 invertido) puede ensuciar FASE B. La deriva grande YA
  se observó el 2026-06-01 (semicírculo) → la pregunta real no es "¿hay deriva?"
  sino "¿de qué fuente?" (cinemática vs heading) y "¿qué heading uso para el hold?".
- **v2 (heading-hold con OTOS) es el unblock correcto del semicírculo del 2026-06-01**:
  el OTOS llega LOCAL a CENTRAL (`world_model_get_otos_heading_deg` / `_otos_is_fresh`,
  broadcast 2026-06-01), reemplazando el BNO de CENTRAL ausente. NO requiere TOP ni
  mensaje nuevo. **Gating antes de escribirlo**: (a) reconciliar substrato de
  movimiento (cinemática vs directo) y M2; (b) confirmar signo de omega (no runaway).

## Próximos pasos

1. **Banco (equipo)** — secuencia recomendada en el reporte coach del día:
   (i) `diag_central_motors` → reconciliar M2 (¿invertido o no en ESTE robot?);
   (ii) link DOWN→CENTRAL con `diag_central_comm_down` (cable al **pin 0**, NO pin 7);
   (iii) `diag_central_strafe` FASE A (¿lateral limpio o círculos?) — si círculos,
   el dato útil no es la deriva sino "la cinemática no sirve aún".
2. **Decisión de Gustavo** — substrato de movimiento del arquero: ¿converger en
   `goalkeeper_tick` (calibrar `WHEEL_ANGLES_DEG` con Enzo + heading-hold OTOS) o
   evolucionar `diag_central_line_sweep` (control directo + swap BNO→OTOS)?
3. **v2 heading-hold OTOS** — implementar recién con (1)+(2) resueltos. ~1-2 h.
4. **Reconciliar deuda de docs**: la duplicación arquero (strategy vs line_sweep)
   merece una fila en `FUENTES-DE-VERDAD.md` post-banco.

## Archivos tocados (docs/comentarios — NO firmware funcional)

- `src/diag/diag_central_strafe.cpp` — comentario de cabecera: corregido OTOS stale
  (ya llega a CENTRAL; v2 local) + **agregado caveat de cinemática** (círculos / M2).
- `docs/firmware/DIAG-CENTRAL-MOTORS.md` — corregidas 2 refs **stale Serial2/pin 7**
  (la caja + fila #1 de "Pendientes") que contradecían el banner del propio doc y
  podían mandar a cablear un pin de motor. Ahora: link = Serial1, 7/8 = motor 2.
- `docs/firmware/DIAG-CENTRAL-STRAFE.md` — agregado caveat de cinemática en "Plan de
  prueba en banco" (apunta al journal 2026-06-01).

> Sin medición de hardware en esta sesión (coach no ejecuta banco). Los compiles
> son host/build, no validan comportamiento físico.
