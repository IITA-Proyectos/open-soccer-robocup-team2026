# 2026-06-21 — centralmix (delantero "mix") ahora puede usar el BNO del TOP (snapshot)

## Qué se pidió
Elías: tomar la última versión del **delantero "mix"** (el port 2025 `src/centralmix/`) y
**actualizarlo para que trabaje con el BNO encendido**, ahora que el BNO de R1 quedó andando.
Con condición de PARAR si no se encontraba: (1) el programa mix que andaba, (2) el trabajo de
ayer del BNO andando, o (3) la confirmación de que el BNO anda.

## Las 3 cosas se encontraron (no se paró)
1. **Programa "mix" que andaba** → `src/centralmix/` + env `central_robot1_mix`. Port FIEL del
   delantero 2025 (FSM 24 estados + motores directos) alimentado por TOP/DOWN vía `g_io`. Elías
   lo corrió en R1 el **2026-06-19** y lo dejó andando ("la estrategia funciona", commit `e83d43c`,
   journal `2026-06-19-centralmix-port-delantero-2025.md`). ⚠️ Honestidad: TASK-113 sigue ABIERTA
   y ese "anda" es banco informal; ese día el BNO del TOP estaba en 0.0 → el control de rumbo casi
   no se ejerció (el `error` quedaba ~0 y la conducta era por visión).
2. **Trabajo de ayer del BNO andando** → journal `2026-06-21-bno-heading-fix-config-flag-no-era-tof.md`
   (commit `abb917c`). La causa del "hdg clavado en 0.0" **NO era el chip ni los ToF**: era un
   **flag stale en EEPROM `bno_left_en=0`** que dejaba al BNO primario FUERA de la fusión
   (`fused_heading=0.0` siempre). Fix: forzar el primario habilitado en `sensors_imu.cpp:279`.
3. **BNO confirmado** → validado en banco 2026-06-21 con `top_robot1_pri_rt` (firmware real de
   competencia, ToF rangeando): reposo `hdg=-15.6`, girado ~90° `hdg=101.4` (Δ≈117° → sigue el giro).

> Nota de fecha: el usuario lo llamó "ayer"; el fix quedó journaleado **hoy 2026-06-21** (la
> investigación venía del 2026-06-20, `2026-06-20-r1-top-bno-freeze-causa-raiz-tof-ranging.md`).

## El hallazgo clave (por qué no alcanzaba con "sacar un flag")
El selector de heading del centralmix tenía DOS modos: BNO (default) y OTOS (`-DMIX_HEADING_OTOS`).
Pero el modo "BNO" del código inicializa y lee un **BNO LOCAL en la CENTRAL (`Wire@0x28`)** — y
**R1 NO tiene BNO local**: los 2 BNO viven en el TOP (fuente de verdad: ESTADO-ACTUAL). Con la
CENTRAL sin BNO, `read_bno_heading()` corría al final de cada tick y dejaba `heading_valid=false`
(+ 1,5 s de delay al boot intentando un chip que no existe). El BNO que SÍ anda (el del TOP) ya
llega por el WorldSnapshot (Serial7), pero no había un modo limpio que lo usara sin tocar el BNO
local. Esto era exactamente el **pendiente #5** del journal del 2026-06-19.

## Qué se hizo (aditivo, detrás de flag — nada existente cambia de binario)
Tercer modo de heading: **`-DMIX_HEADING_SNAPSHOT`** = heading del **BNO del TOP por snapshot**,
SIN tocar ningún BNO local.
- **`mix_comm.cpp`**: macro interno `MIX_HEADING_LOCAL_BNO` (definido solo si NO hay OTOS ni
  SNAPSHOT). Todo el código del BNO local (include, instancia, init, `read_bno_heading()` y su
  llamada en el tick) quedó detrás de `#ifdef MIX_HEADING_LOCAL_BNO`. El bloque que toma el
  heading del snapshot (`apply_top_snapshot`, `#ifndef MIX_HEADING_OTOS`) queda activo en modo
  SNAPSHOT y es la ÚNICA fuente; `heading_inicial` se sella con el primer heading válido del
  snapshot (igual patrón que el modo OTOS). `#error` si se definen SNAPSHOT y OTOS juntos.
- **`mix_config.h`**: selector a 3 modos + `MIX_HEADING_SOURCE_IS_SNAPSHOT` (diagnóstico) + doc.
- **`platformio.ini`**: env nuevo **`[env:central_robot1_mix_bno]`** = `central_robot1_mix` +
  `-DMIX_HEADING_SNAPSHOT`. Corregido el comentario que decía "BNO (snapshot) por default" (el
  default real es BNO LOCAL).
- **`README.md` de centralmix**: documentado el modo + env; pendiente #5 marcado RESUELTO.

La FSM (`mix_fsm.cpp`) NO se tocó: consume `g_io.heading_error_deg` igual que antes (no mira
`heading_valid`). El cambio es 100% de la capa de comm/fuente de heading.

## ⚠️ Lo que NO pude verificar (límites de esta sesión)
- **NO compilé.** La shell de esta máquina está rota (`TEMP` secuestrado por KMSpico →
  `EPERM mkdir`); no pude correr `pio run -e central_robot1_mix_bno`. El cambio es de
  preprocesador + un env nuevo (bajo riesgo), pero **el equipo debe compilarlo** antes de flashear.
- **NO validado en banco** (regla #1: lo cierra el humano con la placa). Plan → **TASK-115**.

## Cableado para que R1 juegue de delantero CON gyro
- **TOP de R1**: `top_robot1_pri_rt` (trae el fix del BNO). **CENTRAL de R1**: `central_robot1_mix_bno`.
- DOWN de R1: el de siempre (`down_robot1*`) para línea + OTOS.
- Escape sin gyro: `central_robot1_mix` con `-DMIX_HEADING_OTOS` (usa OTOS, como el failover viejo).

Ver [[project-iita-soccer-2026-strategy]]. Pendiente #5 de `2026-06-19-centralmix-port-delantero-2025.md`
cerrado en firmware; falta el banco.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
