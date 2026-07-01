# 2026-07-01 — MARCADOR: el BNO falló en un momento (arquero R2) — sin acción por ahora

**Autor:** nota pedida por María (arquero R2 / placa CENTRAL), sesión 2026-07-01 (Incheon).

## Qué se observó
Durante el trabajo de banco del arquero (R2), **el BNO falló en un momento**. Observación cruda de María;
la causa NO está diagnosticada y NO se hace ningún cambio relacionado ahora.

## Decisión (explícita de María)
- **Por ahora NO se toca nada** por este tema. El arquero siguió trabajándose (retroceso post-pateo, TASK-120)
  sin cambiar nada del BNO/heading.
- **Este es el marcador temporal:** si en el FUTURO el BNO vuelve a dar problemas o empeora, la investigación
  arranca **desde este momento (2026-07-01)** como punto de referencia. Antes de esto no había síntoma nuevo.

## Contexto para quien retome (NO re-diagnosticar sin leer)
- El arquero (`arqueromix`) usa el **heading del SNAPSHOT del TOP** (no un BNO local en la CENTRAL) — o sea el
  BNO que "falló" es el de la placa **TOP** de R2. Si el heading se ensucia, afecta `orientar_frente` /
  `acomodar_orientar` del arquero (que igual tienen fallback a cámara/timeout).
- Historia de BNO ya documentada (leer antes de tocar): `docs/ESTADO-ACTUAL.md` (banners BNO 2026-06-17/21:
  2× BNO @0x28 en buses separados, primario en Wire2, centinela @1 Hz; el freeze histórico era del secundario
  en el bus compartido con los ToF), módulos `imu_freeze` / `imu_cross_validate` (gateados, sin cablear),
  skill `.claude/skills/bno055-imu-heading-robocup/`.
- Posibles líneas SI recurre (no ahora): confirmar cuál BNO falló (primario Wire2 vs centinela), si coincide
  con los ToF rangeando (contención de bus), temperatura/alimentación, o cable. Cablear `imu_freeze` (ya existe,
  host-tested) para degradar con gracia.
