---
id: TASK-040
title: "BNO/ToF en TOP: bus I²C marginal + 2º BNO (0x29) no inicia"
date_created: 2026-06-02
date_updated: 2026-06-15
assigned: [enzo, gustavo]
priority: P1
status: SUPERADA-2026-06-15  # la arquitectura cambió: ya NO hay 2º BNO en 0x29 (ambos 0x28, buses separados). Ver banner.
estimated_hours: 3
blocks: [heading robusto (2 BNO) + localización por trilateración (4 ToF)]
relacionado: [TASK-038, TASK-039]
tags: [top-board, bno055, vl53l7cx, i2c, hardware, multimetro, bring-up]
---

# TASK-040 — Bus I²C del TOP marginal: 2º BNO (0x29) no arranca + ToF 0/4

> ✅ **SUPERADA (2026-06-15).** Esta TASK debuggeaba un 2º BNO en **0x29** (ADR puenteado a 3V3,
> en el bus `Wire`) que no arrancaba. **Esa arquitectura fue un ERROR, ya corregido en hardware:**
> ambos robots tienen sus 2 BNO en **0x28**, en **buses separados** (primario en `Wire2` 24/25 sin
> ToF, secundario en `Wire` 18/19 con los ToF). Ya **no hay ningún BNO en 0x29**, así que el "2º BNO
> 0x29 no inicia" dejó de existir. El firmware se unificó a esa realidad el 2026-06-15
> (ver `journal/2026-06-16-correccion-bno-0x28-unificado.md` + TASK-216). El texto de abajo queda
> como **registro histórico** del debug del bus marginal.

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
5. **Lo más robusto (si nada alcanza):** **separar los 4 ToF a `Wire2` (24/25)** y dejar
   los 2 BNO solos en `Wire` (18/19). Menos carga por bus + elimina el choque en 0x29.
   Requiere bodge del I²C de los ToF + cambio chico de firmware (ToF → `Wire2`).
   > **Corrección 2026-06-09:** el bus de los pines 24/25 es **`Wire2`** (LPI2C4), no `Wire1`
   > (i²c scan corregido, commit 9da8e9e). Decisión actual (TASK-207): el 2º BNO va a `Wire2`
   > (solo, como PRIMARIO), no los ToF — el de `Wire2` sin ToF es el más confiable.

## Estado del firmware (commiteado, WIP bring-up)

En `src/top/` quedó (mejoras que conviene mantener):
- **Orden correcto de init** (`main_top.cpp`): `sensors_tof_predim_lp()` (duerme ToF) →
  `sensors_imu_init()` → `sensors_tof_init()` (enumera). Receta de `diag_pose_live`.
- **Guard del 2º BNO** (`sensors_imu.cpp`): solo hace `begin()` del RIGHT si 0x29 es un
  BNO real (chip-id 0xA0); si no, lo saltea para **no colgar el bus** → el robot degrada a
  **1 BNO + ToF** (jugable: el heading sale del LEFT).
- **Diagnósticos temporales** (sacar cuando se resuelva): `sensors_tof_scan_wire()` (escáner
  I²C al boot), `refprobe[...]` (pines del COMM, TASK-039) en el print del TOP.
- **▶ SUPERADO 2026-06-22:** el primario BNO se movió a `Wire2` (sin ToF) → el bus de ToF ya sube a
  **400 kHz durante el `getRangingData()`** (validado R1+R2; `-DTOP_TOF_FAST_BUS`), y el boot bajó a
  ~9,6 s (carga ToF a 1 MHz, TA-2). El "NO volver a 400" de abajo era para el primario en el bus
  COMPARTIDO; ya no aplica. Registro histórico del ítem original:
  - ✅ I²C QUEDA en 100 kHz (`Wire.setClock(100000)` + ToF `begin(...,100000)`): NO volver a
    400 kHz — el BNO055 y los VL53L7CX no coexisten a 400 kHz (con los ToF rangeando, el read
    multi-byte del BNO se congela; bisect `diag_bno_tof` 2026-06-02). A 100 kHz andan los dos.
    Costo: boot ~40 s. Está en `sensors_imu.cpp` + `sensors_tof.cpp`.

## Criterio de cierre
- [ ] Leído el `chip_id` de 0x29 → causa identificada (BNO fallado vs ToF/pin-10).
- [ ] Bus I²C estable: `[IMU] LEFT OK` + (`RIGHT OK` si se arregla el 2º BNO) **consistente**
      en varios power-cycles.
- [ ] `[sensors_tof] 4 de 4 midiendo` + `hdg` se mueve al girar + `min_obst` baja con la mano.
- [x] Coach: ~~I²C queda en **100 kHz** (NO 400: BNO+ToF no coexisten)~~ → **SUPERADO 2026-06-22:** bus de ToF a 400 kHz validado (primario en `Wire2`). Falta sacar diagnósticos temporales (scan_wire) + dejar firmware limpio.

## Referencias
- Firmware: `src/top/sensors_imu.cpp`, `src/top/sensors_tof.{h,cpp}`, `src/top/main_top.cpp`.
- Diag que funciona (solo LEFT + ToF): `src/diag/diag_pose_live.cpp` (`pio run -e diag_pose_live`).
- Diag de direcciones: `src/diag/diag_bno_addr_check.cpp`, `diag_top_i2c_scan.cpp`.
- Pinout: `src/top/pinout_common.h` (I²C 18/19, **Wire2 24/25**) + `pinout_robot1.h` (LP {9,10,11,12}, pin-10 conflicto). (El bus de 24/25 es `Wire2`/LPI2C4, no `Wire1` — corregido 2026-06-09.)
- Relacionado: TASK-038 (XSHUT/bodge), TASK-039 (COMM árbitro).
