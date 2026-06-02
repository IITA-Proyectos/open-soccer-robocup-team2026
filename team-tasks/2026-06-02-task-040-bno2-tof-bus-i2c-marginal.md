---
id: TASK-040
title: "BNO/ToF en TOP: bus I²C marginal + 2º BNO (0x29) no inicia"
date_created: 2026-06-02
date_updated: 2026-06-02
assigned: [enzo, gustavo]
priority: P1
status: pending
estimated_hours: 3
blocks: [heading robusto (2 BNO) + localización por trilateración (4 ToF)]
relacionado: [TASK-038, TASK-039]
tags: [top-board, bno055, vl53l7cx, i2c, hardware, multimetro, bring-up]
---

# TASK-040 — Bus I²C del TOP marginal: 2º BNO (0x29) no arranca + ToF 0/4

> **Para Enzo (banco).** Resumen de una sesión larga de debug (Gustavo + coach,
> 2026-06-02). Conclusión: **es un problema de HARDWARE en el bus I²C `Wire` (18/19)**
> del TOP, no de firmware. Necesita multímetro + revisión de soldaduras/pull-ups.

## Síntoma

En el bus `Wire` (18/19) del TOP cuelgan **6 dispositivos**: 2× BNO055 (0x28 + 0x29) +
4× VL53L7CX ToF (default 0x29, se enumeran a 0x2A–0x2D). Al correr el firmware del TOP:
- El **2º BNO (RIGHT, 0x29)** `begin()` **FALLA**, y ese fallo **cuelga todo el bus I²C**
  → después el LEFT deja de leerse (`hdg` clavado en 0.0) y los **ToF dan 0 de 4**.
- **Los resultados CAMBIAN entre booteos** (a veces LEFT OK, a veces FAIL; 0x29 a veces
  ACK, a veces no). Esa **intermitencia = bus marginal** (firma clásica de pull-ups
  débiles / soldaduras frías / cables largos de bodge / demasiada carga en el bus).

## Pruebas realizadas (firmware, todas en `top_robot1`)

| # | Cambio probado | Resultado |
|---|----------------|-----------|
| 1 | Orden original (IMU init → ToF init) | LEFT OK, RIGHT FAIL, ToF 0/4 |
| 2 | Reordenar ToF primero (enumerar antes del BNO) | ToF **andan**, pero **ambos BNO fallan** (los ToF rangeando dejan el bus mal para el BNO) |
| 3 | Receta `diag_pose_live`: **dormir ToF (LP low) → init BNO → enumerar ToF** | LEFT recupera; RIGHT sigue FAIL; ToF 0/4 (el RIGHT en 0x29 estorba la enumeración) |
| 4 | Guard: saltear RIGHT si 0x29 no ACKea | No alcanzó: **0x29 ACKea (SI)** → el begin igual corre y cuelga |
| 5 | Bajar I²C a **100 kHz** (por si era velocidad/marginal) | **Igual** → no es la velocidad del bus |
| 6 | Guard por **chip-id** (solo init RIGHT si 0x29 = 0xA0) + `read_reg` | **← último flasheado. Falta leer el `chip_id` de 0x29 (ver abajo).** |

**Hallazgo clave que explica todo:** `diag_pose_live` SÍ mostraba ToF + heading porque
**usa solo el LEFT BNO** (nunca toca 0x29). El intento de iniciar el RIGHT (0x29) es lo
que envenena el bus. (`imu_L=Y` en el monitor es solo el flag de init, NO "leyendo ahora":
con el bus colgado, `hdg` queda en 0.0.)

## El diagnóstico que falta (1 línea del monitor)

El último firmware imprime, apenas arranca:
```
[IMU] Sondeo 0x29 -> ACK=SI chip_id=0xXX
```
**Leer ese `chip_id`** define la causa exacta:

| `chip_id` | Qué es 0x29 | Acción |
|---|---|---|
| **`0xA0`** | Es un **BNO real** (el puente ADR→3V3 funcionó) pero su `begin()` falla | 2º BNO **fallado o mal config**: revisar **PS0/PS1 a GND** (modo I²C), alimentación (Vin/3V3+GND), o **chip dañado** (probar reemplazo). El bus marginal puede agravarlo. |
| **otra cosa** (no 0xA0) | Es un **ToF que NO se durmió**, pegado en 0x29 | **Conflicto del pin 10**: el LP del ToF[1] (ATRÁS) está en pin 10, y ahí también está el **dipswitch de rol** (`pinout_robot1.h:97`). Si hay un dipswitch físico en pin 10, no deja dormir al ToF[1]. **Sacar el dipswitch del pin 10** (el rol va por `#define`, no se lee dipswitch). |

## Acciones de hardware (Enzo) — en orden de probabilidad

1. **Pull-ups del I²C (SDA=18 / SCL=19):** con 6 dispositivos + cables de bodge, la
   capacitancia es alta. Si los pull-ups son 10k, subir a **2.2–3.3 kΩ a 3V3**. Es lo más
   probable que estabilice el bus.
2. **Revisar el pin 10** (dipswitch vs LP del ToF[1]) — ver tabla arriba.
3. **Resoldar:** SDA/SCL, las patas de cada BNO y ToF, los cables del bodge, y el **puente
   ADR→3V3 del 2º BNO** (intermitente: a veces ACKea, a veces no).
4. **2º BNO:** confirmar PS0/PS1 a GND (modo I²C) y alimentación; si sigue, probar reemplazo.
5. **Lo más robusto (si nada alcanza):** **separar los 4 ToF a `Wire1` (24/25)** y dejar
   los 2 BNO solos en `Wire` (18/19). Menos carga por bus + elimina el choque en 0x29.
   Requiere bodge del I²C de los ToF + cambio chico de firmware (ToF → `Wire1`).

## Estado del firmware (commiteado, WIP bring-up)

En `src/top/` quedó (mejoras que conviene mantener):
- **Orden correcto de init** (`main_top.cpp`): `sensors_tof_predim_lp()` (duerme ToF) →
  `sensors_imu_init()` → `sensors_tof_init()` (enumera). Receta de `diag_pose_live`.
- **Guard del 2º BNO** (`sensors_imu.cpp`): solo hace `begin()` del RIGHT si 0x29 es un
  BNO real (chip-id 0xA0); si no, lo saltea para **no colgar el bus** → el robot degrada a
  **1 BNO + ToF** (jugable: el heading sale del LEFT).
- **Diagnósticos temporales** (sacar cuando se resuelva): `sensors_tof_scan_wire()` (escáner
  I²C al boot), `refprobe[...]` (pines del COMM, TASK-039) en el print del TOP.
- ⚠️ **I²C a 100 kHz** (`Wire.setClock(100000)` + ToF `begin(...,100000)`): fue un
  experimento que NO ayudó. **Volver a 400 kHz** cuando se arregle el bus (el boot a 100 kHz
  es ~2× más lento). Está en `sensors_imu.cpp` + `sensors_tof.cpp`.

## Criterio de cierre
- [ ] Leído el `chip_id` de 0x29 → causa identificada (BNO fallado vs ToF/pin-10).
- [ ] Bus I²C estable: `[IMU] LEFT OK` + (`RIGHT OK` si se arregla el 2º BNO) **consistente**
      en varios power-cycles.
- [ ] `[sensors_tof] 4 de 4 midiendo` + `hdg` se mueve al girar + `min_obst` baja con la mano.
- [ ] Coach: revertir I²C a 400 kHz, sacar diagnósticos temporales, dejar firmware limpio.

## Referencias
- Firmware: `src/top/sensors_imu.cpp`, `src/top/sensors_tof.{h,cpp}`, `src/top/main_top.cpp`.
- Diag que funciona (solo LEFT + ToF): `src/diag/diag_pose_live.cpp` (`pio run -e diag_pose_live`).
- Diag de direcciones: `src/diag/diag_bno_addr_check.cpp`, `diag_top_i2c_scan.cpp`.
- Pinout: `src/top/pinout_common.h` (I²C 18/19, Wire1 24/25) + `pinout_robot1.h` (LP {9,10,11,12}, pin-10 conflicto).
- Relacionado: TASK-038 (XSHUT/bodge), TASK-039 (COMM árbitro).
