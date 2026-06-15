---
title: "Integración (cableado) gateada del análisis RT: quick-wins CENTRAL + pose_fusion TOP + motor_slew"
date: 2026-06-15
author: "Claude (Anthropic - Claude Opus 4.8 1M) — coach"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M)"
status: final
tags: [control, sensores, comunicacion, firmware, ambos, gateado]
robot: ambos
area: control
tipo: implementacion
---

# Integración RT gateada — quick-wins + pose_fusion + motor_slew

## Contexto

Continuación del [análisis RT de las 3 placas](2026-06-15-arquitectura-rt-3-placas.md)
(diseño + 5 módulos puros, ya en `main`). Esta sesión **cablea** parte de ese diseño
al firmware vivo siguiendo el orden del [handoff](../docs/firmware/HANDOFF-INTEGRACION-RT.md),
con la regla dura: **TODO gateado off-by-default → binario de competencia byte-idéntico
hasta validar en banco**. Claude NO puede testear en hardware ni en compilador Teensy
(no hay `pio`/g++-arm en PATH); programa host-testeable + glue Arduino que el equipo
compila y valida en banco.

## Qué se hizo (todo gateado, default OFF)

### A) Quick-wins del lazo CENTRAL

1. **A1 — `CENTRAL_DEBUG_SERIAL`**: el bloque de ~30 `Serial.print` cada 500 ms en el
   `loop()` de la CENTRAL (`main_central.cpp`) corría SIEMPRE → pico de jitter periódico
   dentro del lazo de motores si el buffer USB CDC se llena. Ahora está gateado.
   **Byte-idéntico HOY**: el flag se DEFINE en las bases `central_robot1`/`central_robot2`
   (se propaga por herencia a TODAS las variantes) → el binario no cambia. La quita real
   del jitter es un **flip de banco**: borrar el flag del env de competencia tras validar
   que la telemetría USB no hace falta en cancha. (Mismo patrón que `CENTRAL_BLACKBOX`.)
   - **Por qué NO lo saqué directo**: sacarlo cambiaría el binario de competencia sin
     validación de banco, contra la regla dura. La infraestructura queda lista; el lever
     es del equipo.

2. **A2 — `CENTRAL_TOP_RX_BIGBUF`**: el RX de `Serial7` (link TOP→CENTRAL, `comm_top.cpp`)
   tiene solo 64 B de buffer interno vs los 512 B del link de DOWN. Un WorldSnapshot
   (~37 B) a 100 Hz: si una vuelta del loop se alarga, 64 B se llenan en <1 frame y los
   bytes nuevos se DESCARTAN en silencio → frame corrupto → resync → snapshot perdido sin
   aviso. `addMemoryForRead(buf,512)` da ~13 frames de colchón. **Default OFF = 64 B = el
   binario de hoy** (acá NO toqué los envs de competencia: el default ya es byte-idéntico).
   El equipo agrega `-DCENTRAL_TOP_RX_BIGBUF` en banco (chequear que `resync` baja bajo carga).

### B) TOP estimación

3. **`ball_sticky` e `imu_freeze`: VERIFICADO que YA estaban cableados** (sesiones previas).
   - `ball_sticky` → `cameras_runtime.cpp` tras `-DTOP_CAM_STICKY` (env `top_robot2_pri_sticky`).
   - `imu_freeze` → `sensors_imu.cpp` tras `-DTOP_ENABLE_BNO_FREEZE_DETECT` (sin env que lo
     prenda aún = infraestructura default-OFF).
   - El handoff los listaba como "a cablear" sin saber que ya estaban. No re-hice nada.

4. **`pose_fusion` + `pose_filter`: CABLEADOS** en `main_top.cpp::build_snapshot` tras
   `-DTOP_ENABLE_POSE_FUSION` (env nuevo `top_robot2_pri_posefusion`). Complementario
   ToF(absoluto)+OTOS(odometría) + suavizado/gate de salto. Heading NO se fusiona (sigue
   del BNO). Entradas ya disponibles en el TOP: `localization_runtime_get_pose()` (ToF),
   `comm_down_get_pose()`/`is_pose_fresh()` (OTOS de DOWN por broadcast), `sensors_imu_get_heading_centideg()`.
   - **INTERLOCK DURO (pedido explícito de Gustavo): `#error` si `TOP_ENABLE_POSE_FUSION`
     sin `TOP_ENABLE_BNO_FREEZE_DETECT`.** El heading es la RAÍZ: un rumbo congelado rota
     TODO el mapa y la corrección ToF ancla en el lugar equivocado. No compila sin el
     freeze-detector activo.
   - **SEGURIDAD POR DISEÑO**: mientras la fusión NO ancle (`pfo.valid=false`) —el caso de
     HOY, porque el ToF casi nunca da `valid` (sólo hay ToF en el eje Y)— `build_snapshot`
     **cae al comportamiento EXACTO de localization** (byte-idéntico aun con el flag ON).
     La fusión recién cambia x/y cuando el ToF ancla de verdad, que es cuando aporta.

### C) Ladrillos puros → lazo vivo

5. **`motor_slew` (Capa 2 del lazo CENTRAL): CABLEADO** tras `-DCENTRAL_MOTOR_SLEW`
   (env nuevo `central_robot2_strafe_slew_bb`). Rampa el comando `{vx,vy,omega}` en el
   espacio de ejes (antes de la cinemática) para que un salto crudo no patine las ruedas
   (arranque parejo, dirección limpia). **Los REFLEJOS bypasean la rampa** (regla dura del
   handoff): el freno de borde (`motors_brake`) y el `SAFE_NO_TOP`/STOP (`motors_stop`)
   se aplican YA y llaman `slew_reset()` para que la rampa no tironee desde un valor viejo
   el tick siguiente. El override de giro crudo (`spin_pwm != 0`) usa `slew_force()` (no
   rampa, sincroniza estado). Pendientes por eje TUNEABLES (`-DCENTRAL_SLEW_DVX/DVY/DW`,
   defaults conservadores poco-limitantes).

## Qué NO se cableó — y por qué (decisión de coach, honesta)

Los otros ladrillos del handoff C **no se fuerzan** en esta sesión. No es pereza: cablearlos
hoy violaría una regla dura o metería glue de alto riesgo que NO puedo compilar-verificar.

- **`state_timer` (CENTRAL) y `sensor_slot`/`snapshot_assembler` (CENTRAL/TOP)**: son
  ladrillos para la **FSM/loop NUEVOS** de `ARQUITECTURA-LAZO-CENTRAL-RT.md` /
  `ARQUITECTURA-SENSORIAL-TOP-NO-BLOQUEANTE.md`. Cablearlos al loop vivo = **reescribir
  `strategy.cpp` / `main_*.cpp`**, prohibido por el handoff. La pizarra (seqlock) además
  tiene las sutilezas de concurrencia que el review adversarial marcó (barrera `__DMB()`,
  preempción anidada del NVIC) → glue de máximo riesgo, no compilable acá. **Pertenecen al
  rewrite del loop post-Incheon**, no a un flag suelto.
- **`line_early_escape` (DOWN)**: cableable, pero cambia la **señal de borde
  safety-crítica** que CENTRAL usa para frenar, y su trigger temprano DEBE titularse contra
  el anillo real (un umbral mal puesto = escapes falsos). Sin banco, forzar eso al path de
  seguridad es irresponsable. **Queda como tarea de banco bien definida** (ya tiene su
  módulo puro + tests + el plan F0-F7 en `ARQUITECTURA-LAZO-DOWN-RT.md`).

> Principio del repo: *mejora corta y bien documentada > ambiciosa y opaca.* 4 integraciones
> seguras y verdes > 6 con glue no verificable en el path crítico.

## Verificación

- **Gate host: 937 tests / 67 envs / 0 fallos** antes y después (no se tocó ningún archivo
  host-compilado: los cambios son glue Arduino en `main_central.cpp`/`comm_top.cpp`/
  `main_top.cpp` + `platformio.ini`; los módulos `pose_fusion`/`pose_filter`/`motor_slew`/
  `ball_sticky`/`imu_freeze` ya tenían sus tests puros y NO se modificaron).
- **NO compilé los envs Teensy** (sin toolchain en PATH). El glue es compile-only para el
  equipo. Los envs nuevos (`top_robot2_pri_posefusion`, `central_robot2_strafe_slew_bb`)
  son flasheables tras `pio run`.

## Pendiente de banco (Claude NO cierra TASKs de HW)

1. **A1**: tras validar, borrar `-DCENTRAL_DEBUG_SERIAL` de `central_robot1/2` → saca el jitter.
2. **A2**: agregar `-DCENTRAL_TOP_RX_BIGBUF` a competencia → chequear que `resync` del link TOP baja.
3. **B4 (pose_fusion)**: medir **ruido de sensores + signo/eje del DELTA OTOS vs marco de
   cancha** ANTES de confiar en x/y. La fusión es inerte hasta que el ToF ancle.
4. **C5 (motor_slew)**: titular `DVX/DVY/DW` en el strafe con caja negra (`central_robot2_strafe_slew_bb`);
   observar la **interacción con el kickstart** (impulso inicial 130 PWM × 40 ms): el
   kickstart rompe fricción estática, el slew evita el tirón posterior — verificar que se
   complementan y no se pelean.

## Próximos pasos

- Cablear `line_early_escape` (DOWN) cuando haya banco para titular el trigger temprano.
- El rewrite del loop/FSM (state_timer + pizarra + assembler) es trabajo de arquitectura
  post-Incheon (sus diseños ya están en `docs/firmware/`).
