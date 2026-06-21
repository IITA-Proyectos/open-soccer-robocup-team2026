# 2026-06-21 — R1 TOP: firmware de prueba "todo menos ToF" (sin rangeo) + corrección de DIAG_NO_TOF

Continuación de [2026-06-20 (causa raíz BNO freeze = rangeo ToF)](2026-06-20-r1-top-bno-freeze-causa-raiz-tof-ranging.md).

## Qué se hizo
Pedido de Gustavo: dejar en la TOP de R1 el RT completo (primario BNO @100 Hz solo en Wire2 +
secundario centinela @1 Hz) pero **con los ToF SIN rangear**, para probar "todo menos los ToF"
(el rangeo es la causa del freeze del BNO). Nuevo env: **`top_robot1_pri_rt_notof`**.

## Hallazgo importante: `-DDIAG_NO_TOF` CUELGA el firmware completo
Primero usé `-DDIAG_NO_TOF` (saltea `sensors_tof_init()` + el tick). **El firmware NO arranca**
(COM11 enumera pero no sale telemetría; 0 bytes en 20 s). Aislado con un control: el mismo
estado del robot con `top_robot1_pri_rt` (init normal) **sí imprime** → el cuelgue es por
`DIAG_NO_TOF`, no por el robot/serial. **Causa:** el `startRanging()` se hace DENTRO de
`sensors_tof_init()` (sensors_tof.cpp:363), y saltear TODO el init deja al firmware completo
(snapshot timer ISR / scheduler / globals) en un estado que cuelga el boot. (No se diagnosticó
el sitio exacto del cuelgue; se evitó el camino.)
→ **`DIAG_NO_TOF` queda como NO usable en el firmware completo** (servía solo en diags chicos).
La nota de TASK-223 / journal 2026-06-20 que lo listaba como herramienta queda corregida acá.

## La solución: `-DTOP_TOF_NO_RANGE` (enumerar pero NO rangear)
Flag nuevo (default OFF = competencia byte-idéntica):
- `sensors_tof.cpp`: se gatea SOLO el `startRanging()` (multi-ToF). El init corre completo
  (enumera, configura, setea scheduler/clock, `g_ready`) → el firmware arranca bien.
- `main_top.cpp`: el `sensors_tof_tick()` (lectura en el loop) también se saltea con el flag.
- Resultado: ToF **enumerados pero SIN ranging** → **sin pulsos de VCSEL → no congela el BNO**
  (causa raíz TASK-223), y el boot NO se cuelga.

## Verificación de banco (2026-06-21)
`pio run -e top_robot1_pri_rt_notof -t upload` → flashea OK. Captura serial:
```
[sensors_tof] multi-ToF: 4 de 4 (enumerados)   [boot] setup_total=10767 ms
[TOP] hdg=0.0 imu_L=Y  min_obst=65535 (NO_READING -> sin rangeo)  loop corriendo
```
- ✅ Bootea y corre (hang resuelto).
- ✅ `min_obst=65535` = los ToF NO rangean (objetivo cumplido).
- ✅ `imu_L=Y` (primario listo); cámaras Y/Y; centinela/snapshot corriendo.
- ⏳ **Heading-vivo NO confirmado en banco hoy** (a robot quieto `hdg=0.0` no distingue
  vivo de congelado). Falta girarlo → lo prueba Gustavo mañana. Por toda la evidencia previa
  (sin rangeo el BNO anda), se espera que el heading siga el giro.

## Nota cosmética
Bajo `TOP_TOF_NO_RANGE` el log dice "4 de 4 midiendo" (cuenta `g_ready`), pero NO miden:
`min_obst=65535` lo confirma. No afecta la prueba.

## Comando
```
pio run -e top_robot1_pri_rt_notof -t upload     # (dir: software/teensy/Soccer 2026)
pio device monitor -b 115200                      # ver [TOP] ... hdg=
```
Verificación mañana: girar ~90° ida y vuelta → si `hdg` sigue el giro = el resto del robot
anda sin los ToF (y queda confirmada la causa raíz). Si igual queda en 0.0 girando, la
enumeración (no solo el rangeo) estaría implicada → avisar.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
