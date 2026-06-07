---
title: "Diseño del Robot Definition único — IITA Soccer 2026"
date: 2026-06-05
status: diseño + scaffold (perfil ROBOT2 creado; rewiring pendiente con PlatformIO)
relacionados: ["REFERENCIAS-POR-ROBOT.md (auditoría)", "../ESTADO-MADUREZ-FEATURES.md", "src/shared/robot_config/robot2.h (seed)"]
---

# Robot Definition único — diseño y plan de migración

## 1. Objetivo
Que para **un robot nuevo** (hoy ROBOT2, mañana ROBOT3) **el ÚNICO archivo a editar/confirmar** sea su **perfil de robot** (`src/shared/robot_config/robotN.h`), con **TODA** la configuración que varía por robot. Hoy esa config está **esparcida y hardcodeada** en varios `config_*.h`, `pinout_*.h` y hasta en los `.py` de las cámaras (ver auditoría `REFERENCIAS-POR-ROBOT.md`). La meta: **diseños limpios, sin hardcode disperso**, y "subir y que funcione directo".

## 2. Mecanismo de selección por robot (hoy → propuesto)
- **Hoy:** mezcla de (a) envs de PlatformIO `*_robot1` / `*_robot2` que definen `-DROBOT2`, (b) bloques `#if defined(ROBOT2) ... #else ...` dentro de `config_central.h` (ej. `MOTOR_INVERT`), (c) headers `pinout_robot2.h`, y (d) constantes hardcodeadas COMUNES en `pinout_common.h` que en realidad **deberían** ser per-robot (ej. `TOF_MOUNT_ANGLE_DEG`). Inconsistente.
- **Propuesto:** un header por robot bajo `src/shared/robot_config/` (`robot1.h`, `robot2.h`, …) que **define TODO lo per-robot** como `constexpr`/`#define`. Un selector único:
  ```cpp
  // robot_config/active_robot.h
  #if   defined(ROBOT_ID) && ROBOT_ID == 2
  #  include "robot_config/robot2.h"
  #elif ... == 3
  #  include "robot_config/robot3.h"
  #else
  #  include "robot_config/robot1.h"   // default
  #endif
  ```
  Los `config_*.h` de cada placa **tiran de `active_robot.h`** en vez de hardcodear. `-DROBOT_ID=N` (en el env de platformio) elige el perfil. **El perfil es la fuente de verdad.**

## 3. Estado de esta entrega (qué se hizo / qué falta)
- ✅ **Auditoría** completa de referencias por-robot: `REFERENCIAS-POR-ROBOT.md`.
- ✅ **Seed del perfil ROBOT2**: `src/shared/robot_config/robot2.h` — captura TODOS los deltas y placeholders con marcas `[R1=…]`, `TODO-BANCO`, `DELTA-R2`. **Aditivo: NO lo incluye ningún build → ROBOT1 byte-idéntico, gate intacto.**
- ⏳ **Falta (sesión de migración con PlatformIO, NO se puede compilar Teensy acá):** crear `robot1.h` (espejando los valores reales actuales, byte-idéntico), `active_robot.h`, y reapuntar los `config_*.h` al perfil; + los cambios de `.cpp` que habilitan los deltas de ROBOT2 (ver §5).

## 4. Plan de migración BYTE-IDÉNTICO para ROBOT1 (orden seguro, con `pio`)
1. Crear `robot1.h` copiando **textual** los valores actuales de `config_central.h`/`config_down.h`/`pinout_*.h` (mismos números). Crear `active_robot.h` (default → robot1).
2. En un `config_*.h`, reemplazar **un grupo** de constantes por las del perfil e **incluir** `active_robot.h`. `pio run -e central_robot1` y **comparar el binario** (o al menos confirmar build + gate). Repetir grupo por grupo (motores → IMU → ToF → …). Cada paso byte-idéntico antes de seguir.
3. Recién cuando ROBOT1 queda 100% tirando del perfil y byte-idéntico, se "activa" `robot2.h` con `-DROBOT_ID=2` y se trabajan los deltas en banco.
> Regla: **ningún paso entra a main sin que ROBOT1 compile igual.** Acá no hay toolchain Teensy → esta fase la hace el equipo con `pio`.

## 5. Deltas de ROBOT2 que requieren CAMBIO DE CÓDIGO (no solo config)
El perfil describe el HW; estos `.cpp` hoy asumen ROBOT1 y hay que parametrizarlos:

| Subsistema | Hoy (hardcode) | Cambio para ROBOT2 |
|---|---|---|
| **IMU — 2 BNO en 2 buses** | `sensors_imu.cpp` instancia ambos BNO en `&Wire` y detecta el 2º por chip-id en `0x29`. | Leer la tabla `IMU_BNO_BUS[]`/`IMU_BNO_ADDR[]` del perfil y crear cada `Adafruit_BNO055` en su bus (`Wire`/`Wire1`), ambos en `0x28`. Wire1 requiere `Wire1.begin()` (cables soldados abajo del Teensy 4.0 → pines 24/25). |
| **ToF — modelo + FOV + rotación** | `sensors_tof.cpp` instancia `Adafruit_VL53L7CX` FIJO; `TOF_MOUNT_ANGLE_DEG` vive COMÚN en `pinout_common.h`; **no existe** constante de FOV. | Parametrizar el modelo por slot (`TOF_MODEL[]` → enum VL53L7CX/L5CX/L8CX; libs ya en `lib/`); mover `TOF_MOUNT_ANGLE_DEG` al perfil (per-robot, R2 rotado ~90°); agregar `TOF_FOV_DEG[]` (R2 tiene uno a ~40°) donde la lógica de obstáculo lo necesite. |
| **OTOS — ninguno en R2** | un solo `[env:down]` con `-DDOWN_NUM_OTOS_CONNECTED=2`. | Crear `[env:down_robot2]` con `=0`. `otos.cpp` ya guarda `NUM_OTOS>=1/>=2` → odometría/pose caen al **fallback exacto** (drive_straight, GK paralelo, cross_track) sin OTOS. Cero código nuevo, solo el env + el perfil. |
| **Motores ROBOT2 — pines/inversión dudosos** | `config_central.h` `#if ROBOT2` con `MOTOR_INVERT` copiado de R1. | El perfil lleva `PIN_INA/INB/PWM` por motor + `MOTOR_INVERT[]`. **Validar con `diag_central_motors` en R2** y fijar los reales (el usuario advierte posible error de armado: pines o dirección). (ROBOT1 ya está validado en banco: M1=U5 normal, M2=U17 invertido HW, M3=U7 normal → MOTOR_INVERT={+1,-1,+1}, re-confirmado 2026-06-06). El 'dudoso' aplica solo al delantero/ROBOT2. |

## 6. Cámara: calibración de DISTANCIA vs COLOR (decisión de diseño)
- **Distancia / homografía (H_MATRIX, unit→mm):** es **per-robot-y-per-cámara** (depende del montaje físico). **VA en el robot-def** (`CAM_FRONT_H_MATRIX`, `CAM_BACK_H_MATRIX`, alturas). Los `.py` de las cámaras hoy tienen la homografía hardcodeada; el robot-def debe ser la fuente y un **generador** produce los `.py` desde el perfil (o el `.py` lee un bloque versionado). Así "un solo archivo" incluye la calibración de distancias, como pediste.
- **Color / LAB:** es **más per-VENUE/iluminación que per-robot** (cambia con la luz de la sede; por eso TASK-022 es el bloqueante #1). **Recomendación:** el LAB **NO** es parte "dura" del robot-def; se guarda un **baseline por robot/cámara** (para no arrancar de cero en Incheon) que **se RE-CALIBRA en sede**. El perfil incluye el baseline (`CAM_*_THRESHOLD`) marcado "recalibrar en sede". Esto responde tu duda: **distancia sí en el robot-def; color = baseline en el robot-def pero se recalibra en cancha.**

## 7. Qué NO es per-robot (queda común, no se duplica)
Puertos de comunicación entre placas y con las cámaras (UART Serial1/2/3/4/5 + baudios), árbitro por GPIO nivel (pin5/6 OR), geometría del anillo de 32 sensores (LUT del PCB DOWN compartido), dimensiones de cancha. Confirmado por la auditoría (sin `#if ROBOT`). Se listan en el perfil **solo como informativos** para que sea autocontenido.

## 8. Próximos pasos
1. (Banco/`pio`) Migración byte-idéntica §4 + crear `robot1.h`/`active_robot.h`.
2. (Banco) Confirmar y fijar en `robot2.h` todos los `TODO-BANCO` cuando ROBOT2 esté armado: pines/inversión de motor, mount angles + qué ToF es el de 40°, modelo de ToF, addresses/bus de los 2 BNO, `TOF_OFFSET_MM`, homografías de cámara.
3. (Código) Los 4 cambios de `.cpp` de §5 (IMU 2-buses, ToF modelo/FOV/ángulos, env down_robot2, motores).
4. (Cámara) Generador `.py` ← robot-def para la homografía; recalibrar LAB en sede.

> El seed `robot2.h` ya deja TODO esto en UN archivo con placeholders — cuando el robot esté armado, se confirman los `TODO-BANCO` ahí y se sube.
