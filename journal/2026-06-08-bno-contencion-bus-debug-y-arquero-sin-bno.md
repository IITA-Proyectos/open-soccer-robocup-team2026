# 2026-06-08 — BNO del TOP: contención de bus (software agotado) + el arquero degrada sin BNO

**Quién:** Gustavo (banco) · Claude Opus 4.8 (diagnóstico/firmware).
**Contexto:** chequeo post-caída de ROBOT1 (se rompió la manija y se cayó). Salió casi todo OK;
el único problema que NO se pudo cerrar es el **heading del BNO055 en producción**.

## Qué pasó (resumen del debugging post-caída)
1. **Caída** → al principio: BNO congelado + a veces no levantaban los ToF (boot intermitente).
2. **Causa de la intermitencia de boot = bodge XSHUT/LP flojo** (pines 9-12). El bus I²C crudo
   estaba sano (`diag_top_i2c_scan` estable: 0x28 + ToF). **Re-asentar los bodges lo arregló** →
   los 4 ToF + BNO levantan bien en `diag_top_all`. ✅
3. **Pero el BNO sigue sin andar EN PRODUCCIÓN** (`main_top`): `hdg=0.0`, `flags=0x0`
   (heading_valid=0, la fusión lo excluye). **En `diag_top_all` SÍ trackea.** Misma placa, mismo
   `sensors_imu`, mismo init.

## Diagnóstico (firme): contención del BNO055 en el bus `Wire` compartido
- El BNO055 es un **mal ciudadano I²C documentado** ("violates the I2C protocol", falla en buses
  compartidos/cargados — foros Bosch/Adafruit/ESP32).
- En el firmware **AMBOS BNO están en `&Wire` (18/19), compartido con los 4 ToF** (`sensors_imu.cpp:43`).
- **Anda en el diag (loop liviano) y NO en `main_top`** porque producción carga mucho más el sistema
  (UART de DOWN ~300 Hz en Serial1, snapshot 100 Hz a CENTRAL en Serial4, localización 30 Hz) → el
  read multi-byte del BNO se corrompe → la fusión lo descarta.
- El propio código ya lo decía: *"Fix de fondo: BNO a Wire1, bus aparte"* (`sensors_imu.cpp:262`).

## Software AGOTADO (4 band-aids, los 4 fallaron)
| Intento | Resultado |
|---|---|
| I²C a 100 kHz (vs 400) | ya estaba; no alcanza |
| BNO leído a 20 Hz (no 100) | ya estaba; no alcanza |
| `TOP_BNO_TOF_DECONFLICT` (saltear read del BNO en pasada con ToF) | no destrabó |
| `TOP_BNO_READ_NOINTERRUPTS` (noInterrupts en el read) | **no destrabó → NO eran las interrupciones** |

→ **Conclusión: el software no resuelve la contención. El fix es de hardware: BNO a un bus I²C
APARTE (Wire2, pines 24/25), exactamente como ROBOT2.** Ver **TASK-207**.

## Lo bueno: el ARQUERO degrada con gracia SIN el BNO (verificado en código)
- **`central_gate_heading_omega(heading_valid, ω)` → `heading_valid=0` ⇒ ω=0**: la estrategia NO
  intenta orientar con un rumbo falso. Gateado en strategy.cpp (245, 514, 582, 812).
- **"Avanzar derecho" usa el heading del OTOS de la base** (`world_model_get_otos_heading_deg`,
  strategy.cpp:421), NO el BNO. El OTOS anda.
- **GOTO_LINE** (reposición) y **PATROL** (cross_track) van por la **línea** (DOWN); **CLEAR/INTERCEPT**
  por la **pelota** (cámara). Nada de eso necesita el BNO.
- **Detección de lado por color de arco**: `goal_polarity` (cámara, sin heading) — el snapshot ya trae
  `goal_opp`/`goal_own`. Funciona sin el BNO.
- **Lo único que pierde sin BNO:** la orientación fina por giroscopio (cae al fallback sola).

**Decisión Gustavo 2026-06-08:** dejar producción flasheada (CENTRAL + TOP), probar el arquero
(línea + cámara + OTOS) con el BNO marcado como problema abierto, y resolver el bus aparte después.

## Otros hallazgos del día (cerrados)
- **OTOS-vel del DOWN STALE** intermitente → batería floja (se recupera con power cycle / carga).
- **Detector de freeze (`TOP_ENABLE_BNO_FREEZE_DETECT`) PROBADO Y QUITADO**: con el robot quieto
  daba FALSO-DEAD y latcheaba (el BNO055 fusión quieto reporta el mismo centideg). Re-activarlo
  requiere tunear N/T + que no latchee.

## Archivos / commits
- Firmware: `82e7fbd` (revert noInterrupts), `3101f02` (quitar freeze-detect), `15443b7` (deconflict).
- Pendiente: **TASK-207** (BNO→Wire2 24/25, hardware + 1 línea de firmware).
