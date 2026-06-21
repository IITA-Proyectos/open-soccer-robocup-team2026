# 2026-06-21 — R1 TOP: el heading del BNO "no andaba" por un FLAG DE CONFIG (no el chip, no los ToF)

## Resumen ejecutivo
El heading de R1 (BNO primario, TOP) que venía "clavado en 0.0 / no anda" desde hace días
**quedó RESUELTO**, y la causa raíz NO era ninguna de las que veníamos persiguiendo. Era un
**flag de configuración en la EEPROM del Teensy: `bno_left_en = 0`** (el BNO primario
deshabilitado por config, seguro apagado con `BNO_L_OFF` en una sesión vieja creyendo que el
chip estaba fallado). Con el primario marcado disabled, la fusión lo trata como `DEAD`
(`imu_fusion.cpp:102`) → `fused_heading = 0.0` SIEMPRE, aunque el chip lea perfecto.

**El chip nunca estuvo fallado ni congelado.** Confirmado en banco con el firmware REAL de
competencia (ToF prendidos):
- reposo: `hdg = -15.6`
- girado ~90° y mantenido: `hdg = 101.4` (Δ≈117° → **sigue el giro**)

## Cómo se encontró (el método que ganó)
Comparar el **diag que ANDA vs el firmware que NO**, e **instrumentar el camino de lectura**
(`TOP_DBG_BNO` en `sensors_imu.cpp`):
```
RAW_eul = 298→285  (euler crudo: SE MUEVE → chip SANO)
in0     = 27→40    (heading por-sensor: SE MUEVE → lectura OK)
fused   = 0.0      (la fusión lo tira a 0 → el bug está en fusión/config)
```
`RAW_eul` e `in0` seguían el giro pero `fused=0.0` → la fusión excluía al primario → se rastreó
a `g_scfg[0].enabled = bno_left_en = 0`.

## El fix
`src/top/sensors_imu.cpp:279`: se fuerza `g_scfg[0].enabled = true` (era `g_top_cfg.bno_left_en`).
El primario es la ÚNICA fuente de rumbo en primary-only → NUNCA debe quedar deshabilitado por un
flag de EEPROM heredado de una sesión vieja. (El valor stale en EEPROM queda, pero el código ya
lo ignora para el primario.)

## ⚠️ Corrección de conclusiones previas (honestidad)
- **El caso de "contención eléctrica de los ToF" ([journal 2026-06-20](2026-06-20-r1-top-bno-freeze-causa-raiz-tof-ranging.md) / TASK-223) era PROBABLE PISTA FALSA del mismo flag.**
  En esa fecha `bno_left_en=0` ya estaba en EEPROM → el firmware daba 0.0 sin importar los ToF, y
  el diag andaba porque no usa config/fusión. Ahora el BNO da heading vivo **con los ToF
  rangeando** → los ToF no eran el problema. TASK-223 se marca como pista falsa.
- También quedan refutadas (todas eran el flag): cristal, alimentación USB-only, frecuencia de
  lectura, clock I2C, features RT.
- Único dato no explicado por el flag: una captura del `diag_bno_freeze_probe` (registros crudos)
  mostró el euler congelado en UNA rotación, intermitente y no reproducido → insuficiente para
  afirmar un freeze real de hardware.

## Entregables de esta sesión
- **Fix** en `sensors_imu.cpp` (primario forzado habilitado). TOP flasheada con `top_robot1_pri_rt`
  (firmware real de competencia, ToF on) + el fix → R1 va con BNO andando.
- **Skill nueva** `.claude/skills/bno055-imu-heading-robocup/` (experta en BNO055/IMU:
  diagnóstico, calibración, recuperación, prácticas; con la trampa del flag documentada) —
  construida con un workflow de investigación en paralelo. Commit cc80db0.
- Herramientas de diagnóstico (gateadas, default OFF = competencia byte-idéntica): `TOP_DBG_BNO`
  (traza del read path), `diag_bno_freeze_probe` (scanner I2C dual-bus), `TOP_TOF_NO_RANGE` /
  `DIAG_NO_TOF` (ToF sin rangear / apagados), envs `top_robot1_pri_rt_notof`.

## Lección (la grande)
Un sensor "apagado en una sesión vieja" por un flag persistente es indistinguible de uno
"fallado" si no se loguea el motivo. **Al boot hay que LOGUEAR el valor real de los flags de
habilitación y de qué sensores usa la fusión.** Y SIEMPRE correr el diag de lectura directa
ANTES de culpar al hardware: si el diag anda, el bug es de software/config, no del chip.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
