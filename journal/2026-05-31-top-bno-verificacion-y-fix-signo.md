---
title: "Verificación del BNO055 en banco + fix de signo de heading (CW→CCW)"
date: 2026-05-31
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8, Anthropic)"
status: final
tags: [top-board, bno055, imu, heading, sentido-giro, hardware-test, fix]
robot: robot1
area: firmware
tipo: verificacion + fix
---

# Verificación del BNO055 en banco + fix de signo de heading

> **TL;DR.** Probamos el BNO055 en hardware con un diag dedicado
> (`diag_top_bno`). **Tres resultados:** (1) el init actual de `sensors_imu.cpp`
> es CORRECTO — no se toca. (2) Solo hay **UN BNO conectado** (el LEFT, en
> `Wire`); el RIGHT (Wire1/24-25) NO está cableado. (3) El BNO mide
> **CW-positivo** (girar a la DERECHA sube el heading: +90), pero el firmware
> asume **CCW-positivo** (izquierda sube). Era un bug de signo latente que el
> propio `localization.cpp` ya advertía. **Fix aplicado** en la FUENTE
> (`sensors_imu.cpp`, `HEADING_SIGN = -1`). Compila limpio. Falta una
> re-validación corta en banco de que el heading ya sale CCW.

## Qué se hizo

Gustavo pidió verificar que el BNO funciona (orientación + sentido de giro) y
analizar a fondo cómo inicializarlo. Para no inventar, primero leí lo que ya
existía en el repo.

### Hallazgo 1 — La inicialización ya era correcta (no se reescribe)

Existe un análisis técnico completo: `docs/internal/giroscopo-bno055-analisis-tecnico.md`
(del problema histórico "el BNO no funcionaba en 2025"). El módulo vivo
`src/top/sensors_imu.cpp` **ya implementa las 6 recomendaciones**:

| Recomendación del análisis | En `sensors_imu.cpp` |
|---|---|
| Modo IMUPLUS (sin magnetómetro → inmune a motores DC) | ✅ `begin(OPERATION_MODE_IMUPLUS)` |
| Cristal externo | ✅ `setExtCrystalUse(true)` |
| Estabilización ≥1000 ms post-init | ✅ `STABILIZE_MS = 1000` |
| Espera calibración del gyro (con timeout) | ✅ hasta 2000 ms, corta si `gyro≥3` |
| Heading inicial = promedio de 10 lecturas | ✅ `capture_offset()` |
| Degradación elegante (sin `while(1)`) | ✅ sigue con el otro BNO si uno falla |

**Conclusión: el init no se toca.** Lo que faltaba era *verificarlo en
hardware* y *medir el signo de giro* — eso es lo que aporta el diag nuevo.

### Hallazgo 2 — Solo un BNO conectado

`diag_top_bno` prueba los dos buses. Resultado de banco (Gustavo, 2026-05-31):
- **LEFT** (U10, `Wire`, pines 18/19, 0x28): **OK**, inicializa y mide.
- **RIGHT** (U11, `Wire1`, remap 24/25, 0x28): **no conectado** físicamente.

El firmware ya degrada bien (usa el LEFT, no se cuelga). Pero el init igual
intenta el RIGHT y gasta ~3 s de timeout al boot, y toca Wire1 (24/25) — los
pines que el recableado de los ToF quería liberar para la placa DOWN.
**Se deja como tema-a-analizar** (abajo), sin tocar el init dual-IMU ahora
(decisión de Gustavo: respetar la moratoria de cambios de diseño).

### Hallazgo 3 — Signo de giro invertido respecto al firmware (FIX)

**Medición de banco (`diag_top_bno`, sensor LEFT):**
- Cero en cero. ✅
- Girar **90° a la DERECHA** (horario / CW) → heading **+90**.
- Girar **90° a la IZQUIERDA** (antihorario / CCW) → heading **−90**.

O sea el BNO entrega **CW-positivo**.

**Pero la convención canónica del firmware es CCW-positiva:**
- `pinout_common.h` (`TOF_MOUNT_ANGLE_DEG`): "+90° = izquierda del robot".
- `localization.cpp::classify_wall` (comentario línea 27-29 ORIGINAL):
  *"asume heading positivo = giro CCW… si el BNO da CW positivo, hay que
  invertir el signo"*. El código YA AVISABA del bug; nadie lo había medido.

Con el signo crudo, **localización cruzaría las paredes E/W** y el heading que
llega al CENTRAL (navegación, HeadingPID) tendría el giro al revés.

**Fix aplicado — en la FUENTE, un solo punto:**
`src/top/sensors_imu.cpp`: nueva constante `HEADING_SIGN = -1.0f`, aplicada en
`sensors_imu_tick()` al yaw crudo de ambos sensores. Desde ahí, TODO el
firmware (snapshot a CENTRAL, localización, world_model) recibe heading
CCW-positivo y consistente.

**Verificado que NO hay doble inversión** rastreando toda la cadena:
- `localization_runtime`: offset y heading se leen ambos sign-corregidos → la
  resta `heading - offset` es consistente. ✅
- `main_top.cpp:64` → `world_model.cpp:49` (CENTRAL solo divide /100, sin tocar
  signo) → recibe CCW. ✅
- Actualicé el comentario de `localization.cpp::classify_wall` para que diga
  que la inversión YA se hace en `sensors_imu.cpp` → **no volver a invertir acá**.

**Estado:** compila limpio en `top_robot1` (FLASH code 30040, 0 warnings
nuevos). `test_localization` host sigue verde. **Pendiente:** re-correr
`diag_top_bno` o el debug de `main_top` y confirmar que ahora **izquierda sube
el heading** (validación de 2 minutos en banco).

## Tema-a-analizar — 2º BNO (RIGHT) no conectado

**Categoría:** electrónica / firmware · **Robot afectado:** robot1 (TOP) · **Prioridad: P2**

**Qué observo.** Solo el BNO LEFT está cableado. El init
(`sensors_imu_init`) intenta igual el RIGHT en `Wire1` (24/25), gastando
~3 s de timeout al boot y ocupando esos pines, que el recableado de ToF quería
liberar para la placa DOWN.

**Risk-no-fix.** (a) ~3 s extra de boot. (b) Sin redundancia de IMU: si el LEFT
falla en partido, no hay respaldo (el diseño dual existía justo para eso).
(c) Posible contención futura en Wire1 cuando se conecte DOWN ahí.
**Risk-fix.** Bajo. Un flag `ROBOT_HAS_IMU_RIGHT=0` en `pinout_robot1.h` saca el
intento de init del RIGHT (saca el delay y no toca Wire1). Reversible. Riesgo:
perder el segundo IMU si después SÍ se conecta y nadie revierte el flag.
**Tiempo estimado.** 15 min (flag + guard en `sensors_imu_init`) + recompilar.

**Plan de prueba en hardware.**
1. Con flag OFF: confirmar boot sin los ~3 s de espera del RIGHT y que el LEFT
   sigue midiendo.
2. Decidir a futuro: ¿se conecta el 2º BNO (redundancia) o se libera Wire1
   definitivamente para DOWN? Es una decisión de arquitectura del TOP.

## Cómo re-validar el fix de signo (pendiente humano, 2 min)

1. `pio run -e diag_top_bno -t upload` (este diag lee el yaw CRUDO, así que
   sigue mostrando CW+; sirve para confirmar el hardware, no el fix).
2. Para validar el FIX: `pio run -e top_robot1 -t upload`, abrir monitor 115200,
   mirar la línea de debug `hdg=`. Girar a la **IZQUIERDA** → `hdg` debe **subir**
   (ahora CCW-positivo). Si sube a la derecha, el signo quedó al revés.

## Archivos tocados

- `src/top/diag_top_bno.cpp` — **nuevo** (commit `c61aeb8`), diag de verificación.
- `src/top/sensors_imu.cpp` — `HEADING_SIGN = -1` (fix de signo CW→CCW).
- `src/shared/localization.cpp` — comentario de `classify_wall` actualizado
  (la inversión ya se hace en la fuente; no doble-invertir).
- `platformio.ini` — env `diag_top_bno` (offline, lib vendoreada) + quité una
  opción inválida (`build_lib_ldf_mode`).
