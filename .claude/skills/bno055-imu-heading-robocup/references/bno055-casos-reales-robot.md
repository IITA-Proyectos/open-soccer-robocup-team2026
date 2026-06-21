# BNO055 — casos reales del robot IITA (evidencia + lecciones)

> Anclaje del caso vivido para dar credibilidad SIN sobre-ajustar la skill. Lee esto para
> entender por qué el árbol de diagnóstico DERIVA la causa en vez de asumirla.

## Caso A — heading=0.0 SIEMPRE por un flag de config (CONFIRMADO, 2026-06-21)

**Síntoma:** el firmware de competencia daba `hdg=0.0` clavado SIEMPRE (girando o no, con ToF
o sin ToF, USB o batería), pero los programas diag leían el heading y seguían el giro.

**Causa raíz:** el EEPROM del Teensy tenía **`bno_left_en = 0`** — el BNO PRIMARIO deshabilitado
en la config. Mecanismo exacto (`src/shared/imu_fusion.cpp:102`):
`if (!scfg.enabled){ s.health = ImuHealth::DEAD; continue; }` → con el primario marcado DEAD y
el secundario ausente (primary-only), `n_use=0` → la fusión conserva su valor inicial
`fused_heading_deg = 0.0` para siempre. **El chip estaba perfecto; el firmware lo ignoraba.**

Probablemente alguien corrió `BNO_L_OFF` (telemetría, `top_telemetry_serial.cpp:290`) en una
sesión vieja creyendo que el chip estaba fallado, y quedó persistido en EEPROM.

**Cómo se encontró (el método que GANA):** instrumentar el camino de lectura y comparar el
diag-que-anda vs el firmware-que-no. La traza decisiva (`TOP_DBG_BNO`, `sensors_imu.cpp`):
```
RAW_eul = 298 → 285   (euler crudo del registro: SE MUEVE al girar → chip SANO)
in0     = 27 → 40     (heading por-sensor tras offset/signo: SE MUEVE → lectura OK)
fused   = 0.0         (la fusión lo tira a 0 → el bug está en la fusión/config)
```
`RAW_eul` y `in0` seguían el giro, `fused` quedaba en 0.0 → la fusión excluía al primario →
se rastreó a `g_scfg[0].enabled = bno_left_en = 0`.

**Fix (`src/top/sensors_imu.cpp:279`):** forzar `g_scfg[0].enabled = true` — el primario es la
ÚNICA fuente de rumbo en primary-only, NUNCA debe quedar deshabilitado por config.

**Lección:** un sensor "apagado en una sesión vieja" es indistinguible de uno "fallado" si no
se loguea el motivo. **Al boot, logueá el valor REAL de todos los flags de habilitación y de
qué sensores usa la fusión.** `bno_left_en` es firmware-specific, NO está en el datasheet Bosch.

## Caso B — la "contención eléctrica de los ToF" (PROBABLE PISTA FALSA del Caso A)

**Lo que se concluyó el 2026-06-20** (journal `2026-06-20-r1-top-bno-freeze-causa-raiz-tof-ranging.md`,
TASK-223): que el RANGEO de cualquier ToF (pulsos del VCSEL) congelaba la fusión del BNO por
acople eléctrico LOCAL en la placa — descartando con datos cristal, bus, batería, frecuencia y
clock I2C (la tabla de 12 pruebas de abajo).

⚠️ **Corrección honesta (2026-06-21):** ese diagnóstico es **DUDOSO y probablemente equivocado.**
El `bno_left_en = 0` (Caso A) ya estaba en EEPROM en esa fecha → **el firmware mostraba 0.0
SIN IMPORTAR los ToF** (porque ignoraba el primario), y el diag andaba **porque no usa
config/fusión**. Es decir: el contraste "diag anda / firmware no" del 2026-06-20 se explica por
el flag de config, NO por los ToF. Casi toda la tabla de 12 pruebas (todas con el firmware
dando 0.0) es consistente con el flag, no con un acople eléctrico.

**Lo único NO explicado por el flag:** el `diag_bno_freeze_probe` (lectura de registros CRUDOS,
sin config ni fusión) mostró el euler congelado en UNA captura durante rotación — pero fue
**intermitente** (otra captura, "capture 7", trackeó perfecto) y con la ambigüedad de timing de
"¿estaba girando justo en esa ventana?". No se reprodujo de forma confiable. → **insuficiente
para afirmar un freeze real de hardware.**

**Estado:** la causa "ToF/eléctrico" **NO está cerrada y probablemente no exista.** El test
DECISIVO (pendiente de banco): con el `bno_left_en` arreglado, flashear el firmware real **con
los ToF PRENDIDOS** y girar — si el heading sigue el giro, **los ToF nunca fueron el problema**
y TASK-223 se puede cerrar como pista falsa. (Al cierre de esta skill ese giro de confirmación
estaba pendiente del equipo.)

### Tabla de 12 pruebas del 2026-06-20 (re-interpretada)
Todas daban `hdg=0.0` EXCEPTO el diag. Bajo la lente del Caso A, el patrón es "el firmware
ignora el primario (flag), el diag no" — no "los ToF congelan el chip":

| Config probada | Resultado | Re-lectura 2026-06-21 |
|---|---|---|
| diag (lectura directa, sin fusión) | **anda** | no usa config → no sufre el flag |
| firmware, ToF rangeando | 0.0 | flag (no ToF) |
| firmware, ToF enumerados sin rangear | 0.0 | flag (no "el rangeo") |
| firmware 100 Hz / 20 Hz | 0.0 | flag (no el rate) |
| con/sin features RT | 0.0 | flag (no la ISR) |
| USB-only / batería | 0.0 | flag (no la alimentación) |
| oscilador interno (sin cristal ext) | 0.0 | flag (no el cristal) |
| probe raw @100 kHz (sin ToF) | anda | no usa config |
| probe raw, multi-rotación | congelado (1 captura, intermitente) | ⚠️ único dato no-flag; no reproducido |

## Pistas falsas que costaron días (anti-trampas, para no repetir)
- **Culpar al cristal** → cambiar a oscilador interno no cambió nada. (El cristal solo importa
  para el reset por línea RST, otra cosa.)
- **Culpar al rangeo de los ToF / acople eléctrico** → el primario está en su PROPIO bus (Wire2,
  sin ToF) y aun así daba 0.0: los ToF NO podían ser la causa de ESE sensor. Aislá la hipótesis
  por bus antes de gastar tiempo.
- **Culpar a la alimentación USB-only** → un brownout tira TODO, incluido el diag; el diag
  andaba con la misma fuente.
- **Culpar a la frecuencia de lectura** → 20 Hz y 100 Hz ambos clavados; ninguna frecuencia
  produce 0.0 EXACTO y persistente.
- **La trampa madre:** asumir que el diagnóstico de hardware estaba cerrado sin haber descartado
  primero el software/config con el árbol. Un `0.0` PERFECTO grita "lógica de software / sensor
  descartado", no "ruido eléctrico" (eso daría lecturas erráticas o NAKs, no un cero limpio).

## Archivos reales (punteros)
- `src/top/sensors_imu.cpp` — dual-BNO, fusión, EEPROM calib, soft-resync, `TOP_DBG_BNO`,
  el fix del flag (~línea 279).
- `src/shared/imu_fusion.cpp` / `.h` — fusión circular pesada, salud (DEAD/DEGRADED/OK), glitch,
  drift, `enabled→DEAD` (línea 102).
- `src/shared/imu_freeze.h` — detector de freeze bit-exacto + guarda de gyro.
- `src/diag/diag_bno_dual_live.cpp`, `src/diag/diag_bno_freeze_probe.cpp` — los oráculos de
  lectura directa.
- `src/shared/top_config.cpp` — serialización del flag `bno_left_en` a EEPROM.
- `journal/2026-06-20-r1-top-bno-freeze-causa-raiz-tof-ranging.md`, `team-tasks/...task-223...` —
  el caso B (leer con la corrección de arriba en mente).
