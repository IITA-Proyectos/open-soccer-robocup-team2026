---
title: "Root-cause de los 3 subsistemas que fallaron en Incheon: línea/piso, ToF y cámaras"
date: 2026-07-23
author: "Claude (Anthropic — Claude Opus 4.8)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: completado
tipo: analisis
area: sensores
---

# Por qué fallaron los 3 subsistemas — análisis verificado contra el código

> **Origen.** El 2026-07-23 Gustavo reportó verbalmente las tres cosas que no
> funcionaron en Incheon. Este doc es el root-cause de cada una, verificado contra el
> código vivo con `archivo:línea`. Auditoría de 21 agentes: 98 hallazgos, 12 confirmados
> adversarialmente. Los tres hallazgos principales fueron **re-verificados a mano**
> antes de escribirlos acá.
>
> **Nada de esto está cerrado en hardware.** Son causas identificadas con plan de prueba;
> los cierra el equipo con la placa.

---

## (B) Sensores de línea / piso — *"pasaba de no detectar a detectar falsas líneas"*

**Es el hallazgo más importante de los tres.**

### La hipótesis de Gustavo era correcta, pero el entero está en otro lado

Gustavo sospechó *"que el cambio se hacía por números enteros"*. **Acertó** — pero no en
el umbral (esa aritmética está bien y es de resolución fina). Está en el **lazo
adaptativo**.

Y se descarta la otra hipótesis obvia: **histéresis SÍ hay**, ±20 counts
([`line_filters.cpp:32-42`](../../software/teensy/Soccer%202026/src/shared/line_filters.cpp:32)),
viva en el lazo real. El flapping **no** viene de que falte histéresis.

### El mecanismo

[`line_calib.cpp:41-42`](../../software/teensy/Soccer%202026/src/shared/line_calib.cpp:41):

```c
float nc = (1.0f-alpha)*(float)c.carpet + alpha*(float)filtered;
c.carpet = (uint16_t)lroundf(nc);      // <-- el entero
c.threshold = mid(c.carpet, c.white);
```

Un promedio exponencial calculado en float pero **guardado en `uint16` con redondeo**.
Con `alpha = 0.02` ([`comm_central.cpp:59`](../../software/teensy/Soccer%202026/src/down/comm_central.cpp:59))
el paso es `0.02 × error`. Para redondear a ≥1 count hace falta un error de **≥25 counts**.

**No hay régimen intermedio — es bang-bang:**

| Error | Comportamiento |
|---|---|
| < 25 counts | el paso redondea a **0** → adaptación **congelada** |
| ≥ 25 counts | **1 count por tick** |

Y corre a **200 Hz** (`dm_update` desde `comm_central_send_line_urgent`, cada 5 ms) →
**200 counts/s** en el carpet, **100 counts/s** en el umbral (`th = mid(carpet, white)`).

Con los márgenes reales medidos en banco (**82 a 339 counts**, semi-margen 41-170):
**el umbral barre el margen entero del sensor en 0,4 a 1,7 segundos.**

### Por qué produce exactamente ese síntoma

El gate es `if(on_line) return;` — solo adapta cuando **no** ve blanco. Realimentación
**positiva en las dos direcciones**:

- **Sensor sobre blanco que no cruzó `umbral+20`** → sigue en `white=false` → adapta su
  carpet *hacia el blanco* a 200 counts/s → en ~1 s queda **ciego permanente**.
  → **NO DETECTA**
- **Sensor que sí declaró blanco** → deja de adaptar → baseline congelado en el aire.
  Cuando baja la luz (batería, sombra) su umbral queda **debajo del verde** → dispara
  siempre. → **FALSAS LÍNEAS**

Dos **estados absorbentes** distintos por sensor. El anillo se parte en sensores ciegos
y sensores que gritan. **No había ventana de calibración porque el sistema no tiene
punto de equilibrio.** No era falta de pericia: no existía el punto a encontrar.

### Ya se había visto y se tapó

[`comm_central.cpp:60-69`](../../software/teensy/Soccer%202026/src/down/comm_central.cpp:60), textual del repo:

> *"con 60 arrancaba valid=1 pero a ~1-2 s la auto-adaptación del carpet erosiona el
> margen del sensor más flaco (S09) por debajo de 60 → data_valid volvía a 0 (CALIB?).
> Bajado a 40..."*

Banco del 2026-06-06: vieron el síntoma y bajaron el umbral de aceptación 120→60→40 en
vez de apagar la causa.

### Primer experimento — una línea, 5 minutos

```c
/* adapt_alpha */ 0.0f,     // era 0.02f  — comm_central.cpp:59
```

`lc_adapt_carpet` ya retorna sin hacer nada con `alpha<=0` (`line_calib.cpp:39`) →
quirúrgico y reversible.

```bash
cd "software/teensy/Soccer 2026" && pio run -e down -t upload
```

- **Hipótesis a falsar:** con la adaptación apagada aparece la ventana de calibración.
- **Verificación:** calibrar, anotar los 32 umbrales, dejar el robot **quieto sobre verde
  3 minutos** y releer. Con `alpha=0` deben ser **idénticos**; hoy se mueven.
- **Qué se pierde:** la compensación automática ante cambios lentos de luz — que hoy
  tampoco funciona (por la cuantización, un drift <25 counts nunca se corrige).

### El agujero de fondo: nunca se midió el ruido

Buscando en `docs/`, `journal/`, `research/`, `team-tasks/` **no aparece ninguna
medición de σ** de los 32 sensores. Todas las constantes (±20 de histéresis, 40 de
margen mínimo, 4 taps) se eligieron por tanteo — el 40 está documentado como
*"2× la banda de histéresis"*, o sea 2× otra constante arbitraria.

**Sin σ no se puede saber si un margen de 82 counts es cómodo o es ruido puro.** Con
σ=15 son 5,5σ (usable). Con σ=30 son 2,7σ: **ninguna sintonía lo salva** y estuvieron
peleando contra la física.

**Paso 0 de cualquier recalibración:** robot quieto sobre verde, 1000 muestras por
sensor, imprimir media/σ/pico-a-pico. Repetir sobre blanco. **2 horas.**

### Causas contribuyentes (confirmadas, secundarias)

| # | Qué | Dónde |
|---|---|---|
| 1 | La pantalla de calibración **no muestra la función que decide**: hay 3 definiciones distintas de "ve blanco" (`line_ring`, `DownModel`, la app) y el frame de telemetría **mezcla las tres** | `down_telemetry_serial.cpp:71-99`, `panel_base.py:321` |
| 2 | La auto-calib usa **MIN/MAX crudos**, sin percentiles ni descarte de outliers. Y los 32 sensores viven en 3 anillos de radios distintos → los internos pueden no ver blanco nunca → umbral ≈ verde → disparan siempre | `down_telemetry_serial.cpp:196-214` |
| 3 | Calibración en **counts absolutos** con los LEDs colgados de la batería → se vence sola dentro del partido, y correlaciona con el movimiento | `config_down.h:144-148`, journal 2026-06-06 |
| 4 | Banda de histéresis **fija** (±20): se come el 49 % del margen en el sensor flaco y el 12 % en el bueno | `line_filters.h:24` |
| 5 | Muestreo a 1 kHz **decimado 5:1 sin anti-alias** — se tira el 80 % de las muestras y el ruido se pliega | `main_down.cpp:196-209` |
| 6 | Si la EEPROM no carga, los 32 quedan con `white=800` por defecto → robot **ciego con toda la telemetría en verde** | `line_ring.cpp:118-122` |

---

## (C) ToF — problema de alcance **y** de anulación

Reporte de Gustavo: *"no llegan a medir hasta el final de la cancha"* + *"problema de
anular y calibrar los que funcionan"*. **Las dos cosas confirmadas, con causas distintas.**

### Alcance: es física

Alcance medido contra pared negra (banco 2026-06-25): **~1,4-1,5 m**. La cancha mide
**2.430 mm**. Las paredes son de madera pintada de **negro** por reglamento, y el negro
absorbe el IR del VCSEL → `target_status` inválido → la zona se descarta.

**Ningún ajuste de firmware cubre esa diferencia.** "Medir hasta el fondo de la cancha"
no va a pasar con este sensor.

### Pero jugaron con la configuración equivocada

El binario de competencia es `top_robot2_pri` (`platformio.ini:25`, `default_envs`) y
**no trae** `-DTOP_TOF_MAXRANGE` ni `-DTOP_ENABLE_TOF_CONTINUOUS`. El propio archivo lo
dice en la línea 464: *"Competencia (sin los flags)"*.

Sin esos flags → **4×4 @ 15 Hz autónomo**, que es exactamente la config que el banco del
2026-06-23 documentó como **incapaz de ver las paredes negras**. El modo que sí las ve
(`top_robot2_pri_tofmaxrange`) quedó en un env de banco y **nunca llegó al robot que jugó**.

Costo del modo bueno: el ranging baja a 2 Hz → **~1,7-2 Hz efectivos por cara**, latencia
hasta 620 ms.

### Anulación: bug confirmado

Hay **dos getters** y solo uno respeta `enabled`:

- [`sensors_tof.cpp:678`](../../software/teensy/Soccer%202026/src/top/sensors_tof.cpp:678) — individual, **correcto**:
  `if (idx < TOP_CFG_NUM_TOF && !g_top_cfg.tof[idx].enabled) return TOF_NO_READING;`
- [`sensors_tof.cpp:697-714`](../../software/teensy/Soccer%202026/src/top/sensors_tof.cpp:697) — `sensors_tof_get_min_distance_mm()`,
  **el que arma `min_obstacle_mm`**: itera `g_distances_mm[]` **sin chequear `enabled`**.

Las FSM que jugaron (`arqueromix`, `centralmix`) **no leen la pose de los ToF** — su
único consumo es `min_obstacle_mm`, cableado a un guard de frenado a 150 mm. Y el escape
documentado en el propio arquero (*"deshabilitar los ToF frontales en la config del
TOP"*) **no funciona**. Se apagaba el ToF desde la EEPROM y seguía frenando al robot.

**Fix de 3 líneas.** Es la victoria más barata del subsistema.

### La causa raíz física que nadie midió

Dos journals consecutivos **se contradicen**:

- `2026-06-22`: *"ToF ~170 mm · pared ~140 mm"* → el ToF está **arriba** de la pared
- `2026-06-23`: *"datos corregidos: ToF 160 mm < pared 220-260 mm"* → está **abajo**

**Nadie lo midió con regla.** TASK-225 sigue abierta con ese criterio sin marcar. Y el
fix se invierte según cuál sea: si está arriba hay que bajarlo; si está abajo, bajarlo
**empeora**. Los 4 ToF leyendo ~360-475 mm sin importar la posición del robot es firma
inequívoca de **piso**.

### Orden de bring-up

| # | Qué | Tiempo |
|---|---|---|
| 0 | Fix del `enabled` en `min_obstacle` | 30 min |
| 1 | **Medir con regla** altura de los 4 ToF y de la pared | 15 min |
| 2 | Confirmar que los 4 enumeran (tapar uno por uno con la mano) | 30 min |
| 3 | Repetir el protocolo de 4 posiciones con **max-range** y montaje corregido | 2 h |
| 4 | Franja perpendicular, máscara, pose | días |

El 8×8 va **último, o no va**. El paso 1 no necesita computadora y decide todo lo demás.

**Veredicto honesto:** con el montaje corregido y max-range, lo realista es **X sólido en
toda la cancha, Y solo cerca de los fondos, a ~2 Hz**. Alcanza para corregir a un arquero
lento. **No alcanza para el lazo cerrado de un delantero.**

---

## (A) Cámaras traseras

Dos causas independientes. La de firmware/sensor está en
[`docs/firmware/PAG7936-LIMITES-Y-API-FW5.md`](../../docs/firmware/PAG7936-LIMITES-Y-API-FW5.md).
La geométrica es esta:

### Las 4 cámaras comparten la MISMA homografía

Verificado con `grep` sobre los 4 scripts: la matriz `H` es **byte-idéntica** en
`main-r1-frontal.py:110`, `main-r1-trasera.py:103`, `main-r2-frontal.py:102` y
`main-r2-trasera.py:103` — y el comentario dice, en las cuatro:

```python
    # Camara delantera robot 1
```

La homografía es **la** transformación píxel→cm del piso: depende de la altura, la
inclinación y el ángulo de montaje de **esa** cámara. Y no están montadas igual — la
propia r1-frontal declara `h = 95.0` y las otras tres `h = 82.5`.

Lo único genuinamente calibrado por cámara es el **ajuste cúbico del eje Y**
(`CORR_Y_A/B/C`), que en la trasera de R1 sí se midió en vivo de 5 a 90 cm con RMSE
1,12 cm.

**Ahí está el problema:** estaban ajustando un polinomio de **una** variable sobre Y para
compensar una transformación proyectiva de **dos** variables que es de otra cámara. Cada
punto que corregían movía otro. El eje X no tiene corrección ninguna —
`main-r1-trasera.py:115` dice *"X queda intacta: ese eje ya anda bien"*, que solo puede
ser cierto por casualidad si la H es ajena.

**Fix:** medir la homografía de cada cámara con la lona de grilla y
`vision-optimization-pack/tools/solve_homografia.py`. No hay atajo.

---

## Inconsistencias de documentación encontradas de paso

| Qué | Estado |
|---|---|
| `camaras-openmv/main.py` **no existe** (son 4 archivos desde el rename del 2026-06-21) — **22 referencias rotas** en el repo | FUENTES-DE-VERDAD corregido 2026-07-23; el resto pendiente |
| Thresholds LAB "finales" de `CALIBRACION-VISION-N6.md` **no son los que corren** | pendiente |
| `FIRMWARE-PLACA-ABAJO.md:198` dice margen mínimo 100; el código usa **40** | pendiente |
| Dos procedimientos de calibración de línea **distintos**, ambos documentados como "el" procedimiento | pendiente |
| **85+ TASKs abiertas** de 105 al terminar Incheon | revisar bajo la escala nueva |
| El repo **no registra** qué sensor/firmware tiene cada una de las 4 cámaras | **P0 antes de tocarlas** |

## Método

Auditoría con 21 agentes en paralelo (6 líneas de investigación + 14 verificadores
adversariales + síntesis). Los verificadores recibieron la instrucción de **refutar**
cada hallazgo leyendo el código, no de confirmarlo. Los tres hallazgos principales de
este doc fueron además re-verificados a mano leyendo el fuente antes de escribirlos.

⚠️ El agente de síntesis final murió por corte de conexión; los 98 hallazgos se
recuperaron del `journal.jsonl` del workflow y se sintetizaron a mano.
