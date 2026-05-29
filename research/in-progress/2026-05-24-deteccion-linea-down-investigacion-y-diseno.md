---
title: "Detección de línea en placa DOWN — investigación de equipos top + diseño de arquitectura robusta"
date: 2026-05-24
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: in-progress
tipo: investigacion-diseno
robot: ambos
area: firmware
related-files:
  - software/teensy/Soccer 2026/src/shared/line_filters.{h,cpp}
  - software/teensy/Soccer 2026/src/shared/line_geometry.{h,cpp}
  - software/teensy/Soccer 2026/src/shared/line_tracker.{h,cpp}
  - software/teensy/Soccer 2026/src/shared/line_calib.{h,cpp}
  - software/teensy/Soccer 2026/src/shared/surface_monitor.{h,cpp}
  - software/teensy/Soccer 2026/src/shared/down_model.{h,cpp}
  - software/teensy/Soccer 2026/src/shared/sensor_geometry.{h,cpp}
  - software/teensy/Soccer 2026/src/central/strategy.cpp
related-docs:
  - docs/firmware/FIRMWARE-PLACA-ABAJO.md
  - docs/firmware/CONTRATO-DATOS-DOWN.md
  - hardware/electronics/down-board-pack/02-funcionalidad.md
tags: [research, down, line-detection, fault-tolerance, fail-safe, sensor-fusion, robocup-soccer-open]
---

# Detección de línea robusta — investigación + diseño

> **Frame.** Este documento NO es una decisión ni una spec final. Es un
> doc de **investigación + diseño en discusión** (research/in-progress).
> El alcance: profundizar en cómo los equipos top de RoboCup Soccer
> resuelven la detección de línea, identificar los gaps del firmware
> actual del IITA Soccer 2026, y proponer una arquitectura concreta que
> cubre los 6 casos de uso planteados por el coach (Gustavo) el
> 2026-05-24:
>
> 1. Validación temporal + exclusión de sensor "ruidoso".
> 2. Detección de "robot levantado por choque" (todos los sensores
>    cambian a la vez).
> 3. Arquero patrullando paralelo a la línea de fondo.
> 4. Defensor avanzando paralelo a una lateral sin sacar el 50% del robot.
> 5. Detección del fin del área chica (línea curva).
> 6. Detección de esquina (2 patrones de línea simultáneos).
>
> Las recomendaciones finales van con priorización **P0 / P1 / P2** + plan
> de prueba en hardware real (frame `rcj-soccer-coach`).
>
> **Salida esperada de este doc**: feedback del equipo + decisión binaria
> sobre cuáles módulos se implementan para Incheon y cuáles quedan para
> Nacional 2026 / Mundial 2027.

---

## 1. Resumen ejecutivo

El firmware DOWN ya tiene un **pipeline funcional** de detección de línea
(ver §2). Sin embargo, comparado con los equipos campeones de Hannover 2024
(LNX Robots, Bohlebots) y la práctica histórica de Team Omicron (top
australiano), tiene **5 gaps importantes** (ver §4):

| # | Gap | Estado actual | Estado top teams |
|---|---|---|---|
| 1 | Exclusión dinámica de sensor roto | Solo rechaza aislados (1-vecino) | Omicron lo hace + `EV_MUX_DEAD` flag |
| 2 | Validación temporal anti-anomalía global | Solo "lifted" (28/32 < carpet-50) | Omicron rechaza "todos a blanco" + delta súbito |
| 3 | Penetración en mm reales | Proxy: # de sensores | Calibrable por geometría (sg_radius_mm ya existe hoy) |
| 4 | Cross-track lateral (para PID arquero) | Declarado, nunca computado | Requerido para arquero paralelo |
| 5 | Detección semántica (área chica, esquina, lateral vs fondo) | Solo `EV_CORNER` genérico | Algunos equipos detectan multi-cluster (Omicron hasta 3) |

La buena noticia: **la geometría real de los 32 sensores ya está
disponible** desde hoy 2026-05-24 (commit `34e3025`, `sensor_geometry.h`
con `SENSOR_POS[32]` validado contra schematic). Eso desbloquea
algoritmos que antes eran imposibles sin esa LUT: penetración en mm
reales, cross-track, vector de evitación cartesiano.

La mala noticia: **el cerebro CENTRAL hoy ignora la mayoría de los flags
que DOWN emite** (solo consume `imminent_exit` y `line_angle`).
Cualquier nueva feature requiere también cambios en `central/strategy.cpp`.

**Propuesta núcleo** (detallada en §5): mantener la cadena existente,
agregar **3 módulos nuevos** (`sensor_health`, `line_pattern_classifier`,
`tactical_line_modes`) sin tocar la cadena vieja `line_ring`, y **migrar
DownModel gradualmente** a usar geometría cartesiana real.

---

## 2. Estado del arte EN EL REPO (qué ya está)

Mapeo basado en lectura completa de los módulos `shared/line_*`,
`shared/surface_monitor.*`, `shared/down_model.*`, y de `central/strategy.cpp`.

### 2.1 Pipeline actual completo (1 kHz)

```
[ADC raw (32 valores, 10-bit)]
        │
        ▼ moving average 4 samples (FilterBuffer)
[filtered]
        │
        ▼ hysteresis ±20 counts sobre threshold[i]
[is_white_raw[]]
        │
        ▼ filtro espacial: requiere vecino blanco contiguo en el anillo
[is_white_validated[]]
        │
        ▼ centroide angular ponderado (lg_compute, ángulos uniformes)
[GeomResult: line_angle, escape_angle, sensors_on_line, corner]
        │
        ├── LiftedDetector + SurfaceMonitor: ≥28/32 sensores < carpet-50 → lifted=true
        ├── LineTracker: línea presente ≥200ms y desaparece → EV_LINE_END
        ├── LineCalib: si |white-carpet|<120 → EV_CALIB_SUSPECT (data_valid=0)
        └── adaptación carpet (α=0.02) si sensor NO está sobre línea
        │
        ▼ down_encode → LineStatusV2 (16 bytes)
        │
        ▼ comm_central (200 Hz, Serial1) → CENTRAL
```

### 2.2 Frecuencias

- **Muestreo raw**: 1 kHz (32 sensores en ~110 µs).
- **Pipeline completo**: tick a 1 kHz; salida UART a 200 Hz (cada 5 ms).
- **Comparación con top teams**: LNX 2023 (campeón Hannover 2024) reporta
  **~4 kHz por sensor**. Bohlebots usa 4 ADC en paralelo. Estamos en el
  rango aceptable pero hay margen.

### 2.3 Eventos efectivamente emitidos hoy

Mirando `down_model.cpp` (no solo `types.h`), DOWN setea **6 de los 7
EV_ flags declarados**:

| Flag | Emitido? | Condición |
|---|---|---|
| `EV_IMMINENT_EXIT` | ✅ | `sensors_on_line ≥ 6` |
| `EV_CORNER` | ✅ | 2+ clusters separados 55°–125° |
| `EV_LINE_END` | ✅ | Línea presente ≥200ms, luego desaparece (one-shot) |
| `EV_LIFTED` | ✅ | ≥28/32 sensores < carpet-50 durante ≥100ms |
| `EV_CALIB_SUSPECT` | ✅ | Algún sensor con \|white-carpet\| < 120 |
| `EV_DEGRADED_GEOMETRY` | ✅ | `n < 32` (heurística burda) |
| **`EV_MUX_DEAD`** | ❌ | **Declarado en `types.h`, NUNCA seteado en código** |

### 2.4 Lo que CENTRAL realmente consume

`strategy.cpp` solo lee:
- `world_model_imminent_exit()` → flag de salida inminente.
- `world_model_line_is_fresh()` → watchdog 500ms.
- `world_model_get_line_angle_deg()` → si `data_valid==1`.

**Lo que CENTRAL IGNORA** del LineStatusV2:
- `escape_angle_centideg` (declarado, jamás leído).
- `cross_track_mm` (declarado, nunca computado por DOWN).
- `penetration_mm` (setea como proxy "# sensores", no mm reales).
- `quality` (setea 85 si línea, 95 si no; CENTRAL ignora).
- `event_flags` (`EV_CORNER`, `EV_LINE_END`, `EV_CALIB_SUSPECT`...). **CENTRAL no las consume.**

Esto es un gap arquitectónico: DOWN está sobre-produciendo y CENTRAL
sub-consumiendo. **No es bug** — es deuda de integración.

---

## 3. Investigación: estrategias de los top teams

### 3.1 Equipos y fuentes consultadas

| Equipo | Liga | Posición reciente | Fuente |
|---|---|---|---|
| **LNX Robots** (Eslovaquia) | RCJ Junior Open | 🥇 **Campeón Hannover 2024** | [TDP 2023 PDF](https://lnxrobots.github.io/docs/2023/LNX_Robots_documentation.pdf) (gen3 v2 → 404) |
| **Bohlebots** (Alemania) | RCJ Junior Open | 🥈 2do Hannover 2024 | [github.com/stiebel/bohlebots](https://github.com/stiebel/bohlebots) |
| **Team Omicron** (Australia) | RCJ Junior Open | Histórico fuerte 2019-2020 | [github.com/mattyoung101/omicron](https://github.com/mattyoung101/omicron) |
| **Cardano Robotics** (Italia) | RCJ Junior Open | Activo desde 2019 | [github.com/gianlucafarinaccio/cardano-robotics](https://github.com/gianlucafarinaccio/cardano-robotics) |
| **Tech United Eindhoven** | Middle Size League | 🥇 Campeón 2024 | [Springer paper 2024](https://link.springer.com/chapter/10.1007/978-3-031-85859-8_40) |
| **CAMBADA** (Portugal) | Middle Size League | Histórico | [paper ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/S0957415810000863) |

**Vacíos honestos** — equipos que mencionamos pero NO encontramos TDP
público accesible (probable acceso restringido a proceedings RoboCup
Symposium / Springer):

- HBR (Hartmann-Bayer-Robotics, Alemania)
- Skuba (Tailandia/Japón)
- Boxiang Robot (China)
- GoalEE / EsCo (Italia)
- PE-COJI / RFC Vienna

### 3.2 Calibración por sensor (no global)

**Patrón consistente entre los top teams Junior**: cada sensor tiene su
**propio umbral**, no uno global. Razón: tolerancias ópticas entre LED
+ fototransistor + posición sobre PCB hacen que dos sensores idénticos
den lecturas distintas para el mismo blanco.

- **Omicron** (`LightSensorController.cpp`): barre 200 muestras por sensor
  durante calibración, calcula `(max + min) / 2` por sensor, guarda en
  EEPROM. Cada `lsRing[i].isOnWhite()` compara contra su umbral propio.
- **LNX 2023** (TDP § "Line calibration"): calibración asistida por
  herramienta de graficación que exporta a Excel. Por sensor.
- **Bohlebots**: umbral único global (`schwellwert=100`) — la excepción
  entre los top. Compensa con más muestras.

**Estado en el repo**: ✅ ya tenemos calibración per-sensor en
`line_calib.cpp` (`carpet[i]`, `white[i]`, `threshold[i]` arrays).
Pero **NO se persiste** entre power cycles (sin EEPROM). Cada arranque
queda en `Suspect` hasta que se calibre.

### 3.3 Filtro espacial 1-vecino (isolated rejection + gap fill)

**Patrón Omicron** (`read()` en `LightSensorController.cpp`):
- Si sensor `i` activo pero `i-1` e `i+1` apagados → fuerza apagar `i`
  (rechazo de outlier ruidoso).
- Si sensor `i` apagado pero `i-1` e `i+1` activos → fuerza encender
  `i` (gap fill, asume sensor saturado/limpio).

**Estado en el repo**: ✅ implementado parcialmente en
`lf_spatial_filter()` (rechazo de aislado). **Gap fill NO implementado**.

### 3.4 "All-on rejection" = heurística anti-anomalía global

**Patrón Omicron**: si los 32 sensores marcan blanco simultáneamente,
**fuerza todos a 0**. Interpretación: "esto no puede pasar físicamente
con una línea normal — debe ser anomalía (robot levantado, luz directa,
flash de cámara de partido)".

**Estado en el repo**: ✅ implementado **invertido** en `surface_monitor`
+ `LiftedDetector`: detecta el caso opuesto (≥28/32 sensores **debajo
del carpet** → robot levantado, ven techo lejano). **NO está la versión
"todos a blanco"** que también es señal de anomalía (ej. línea súbitamente
masiva por reflejo de cámara de árbitro).

### 3.5 Detección de robot levantado por choque

**Patrón implícito en los TDPs**: detectar levantado mirando los sensores
de línea SIN IMU explícito. Asume que un robot apoyado sobre cancha verde
da ~carpet baseline en TODOS los sensores; si esa baseline se ROMPE
súbitamente a todos a la vez (ya sea hacia blanco o hacia "muy lejano"),
es anomalía.

- **Omicron**: usa el "all-on rejection" (todos a blanco).
- **DOWN actual**: usa el "all-below" (todos < carpet-50, sensores ven
  techo lejano).
- **Tech United (MSL)** y similares: usan particle filter; cuando
  confianza cae bajo umbral (sucede al levantar), reinjecta partículas
  globalmente. No transferible a Junior sin pose absoluta.

**Gap detectado para el caso "choque entre robots"**:
El choque puede generar **delta súbito en TODOS los sensores
simultáneamente** sin necesariamente caer "muy debajo de carpet" o "todo
a blanco". Por ejemplo: si el robot rota súbitamente 30° por el choque,
todos los sensores ven valores DISTINTOS al frame anterior pero coherentes
con la nueva pose. Ningún equipo top público documenta cómo distinguir
"rotación rápida normal" de "anomalía por choque". Sin IMU + OTOS
correlacionados con la lectura de línea, es difícil.

**Idea propia (no encontrada en TDP)**: combinar **delta L2 entre frame
N-1 y N** con **omega del IMU**. Si Δlectura es grande y omega es chico
(< 50°/s) → anomalía (robot golpeado, no rotando). Esto requiere
integración DOWN ↔ TOP (IMU está en TOP).

### 3.6 Detección de esquina y patrones complejos

- **Omicron** (`calculateClusters()`): identifica hasta **3 clusters**
  simultáneos. Calcula el "biggest angle" entre clusters → infiere ángulo
  de huida (`midAngleBetween`). Esto cubre **esquina** (2 clusters
  perpendiculares) y **3-way** (esquina + línea cercana, ej. al final del
  área chica).
- **DOWN actual**: detecta hasta **2 clusters** (boolean `EV_CORNER`).
  Tolerancia 55°–125° (90° ± 35°).
- **Diferenciar línea lateral vs línea de fondo**: NO encontrado en
  ningún TDP Junior público. Equipos MSL lo hacen con cámara + mapa.
- **Detectar fin del área chica (línea curva)**: NO encontrado. Tendencia
  general: tratan toda línea blanca igual, no como elemento de pose.

### 3.7 Hardware típico

| Aspecto | Lo que muestra la evidencia |
|---|---|
| **Cantidad sensores** | 8 (Cardano) → 16 (LNX 2023 **campeón**) → 32 (Omicron, Bohlebots, **nosotros**) |
| **Tipo sensor** | LED IR + fototransistor discreto (LNX, Omicron). **ALS-PT19 (lo nuestro) NO aparece en TDPs top**. Nosotros estamos en territorio menos explorado. |
| **Frecuencia** | LNX 4 kHz/sensor. Nosotros 1 kHz. **Hay margen para subir**. |
| **Calibración** | Por sensor + EEPROM (Omicron, LNX). Nosotros: por sensor, sin EEPROM. |
| **Algoritmo de ángulo** | Centroide angular (Omicron, nosotros) **o** trigonometría con 2 sensores marginales + ley del coseno (LNX). LNX afirma que es más limpio matemáticamente. |

### 3.8 Vector de evitación reactivo (estado del arte Junior)

**Patrón Omicron** (`lsavoid.c`):
- Convierte detección de línea en `avoidVect` (magnitud + ángulo).
- Si movimiento deseado está dentro de ±90° del vector de evitación
  → recalcula movimiento = proyección perpendicular + empuje opuesto
  proporcional a `(1 - mag) * 120`.
- Es **campo potencial reactivo**, sin pose absoluta. **Suficiente para
  un mundial Junior**.

**Estado en el repo**: `strategy.cpp` hace algo equivalente — calcula
`retreat_angle = line_angle + 180°` y retrocede a 400 mm/s en
`ATK_LINE_AVOID` / `GK_LINE_AVOID`. **Es más simple que Omicron**, pero
funciona para el caso "huir del borde". No cubre los casos
"avanzar paralelo a la línea" que el coach pidió.

---

## 4. Gaps identificados (gap analysis)

### Gap 1 — Exclusión dinámica de sensor ruidoso/roto

**Hoy**: rechaza aislados (1-vecino). Si un sensor da blanco constante
falso (ej. soldadura fría, LED quemado), queda dentro de un cluster
contiguo válido y contamina el ángulo.

**Top teams**: Omicron + filtro 1-vecino + sin exclusión de "ruidoso
crónico". Bohlebots = solo umbral. **Nadie hace exclusión adaptativa
sofisticada**.

**Idea propia**: contar transiciones por sensor (cuántas veces pasa de
blanco → no-blanco en una ventana). Sensor con tasa anormalmente alta
de transiciones = ruidoso → excluir hasta recalibración manual. Emitir
`EV_SENSOR_NOISY` (extender bitfield).

### Gap 2 — Validación temporal anti-anomalía global por choque

**Hoy**: solo `EV_LIFTED` (sensores van debajo del carpet).

**Top teams**: solo "all-on rejection" (Omicron). Nada robusto contra
choque sin IMU.

**Idea propia**: ya descrita en §3.5 — combinar Δlectura con omega del
IMU. Emitir `EV_SHOCK_DETECTED` (extender bitfield). Requiere TOP enviar
IMU a DOWN (Serial5 ya está armado para comandos, agregar canal de IMU
sería P2).

### Gap 3 — Penetración y cross-track en mm reales (no proxy)

**Hoy**: `penetration_mm` = # de sensores blancos (no mm). `cross_track_mm`
nunca se computa.

**Recurso disponible (NUEVO 2026-05-24)**: `sensor_geometry.h` con
`SENSOR_POS[32]` y `sg_radius_mm(i)` validados contra schematic.

**Diseño propuesto**:
- **Penetración real** = `max( SENSOR_RADIUS_MAX - sg_radius_mm(i) )` para
  i blanco. Es decir: cuánto más adentro del borde está el sensor blanco
  más cercano al centro. Si todos los blancos son del anillo externo,
  penetración chica. Si hay blancos en anillo interno (R≈54mm), penetración
  grande.
- **Cross-track** = proyección del centroide cartesiano (de
  `lg_compute_xy`, ya implementado hoy) sobre la dirección perpendicular
  al heading del robot. Concretamente: `cross_track = -centroide_x_mm`
  (con el robot mirando +Y, el cross-track lateral es −X del centroide
  observado).

### Gap 4 — Modos tácticos de seguimiento de línea (paralelo)

Los casos 3 y 4 del coach ("arquero patrullando línea de fondo",
"defensor paralelo a lateral sin salir") **no están en el firmware**
ni en ningún TDP público.

**Diseño propuesto** (§5.4): modo `LINE_FOLLOW_PARALLEL` activable por
comando desde CENTRAL. DOWN reporta línea + dirección preferida del
arquero/defensor (RX de comando). El control fino lo hace CENTRAL con
un PID que usa `cross_track_mm` como measurement.

### Gap 5 — Detección semántica (área chica, lateral vs fondo)

Sin pose absoluta es **muy difícil** distinguir geométricamente lateral
vs fondo. Pero hay **señales útiles**:

- **Línea curva del área chica**: el centroide cartesiano se MUEVE
  monotónicamente al avanzar (no es estático como en una línea recta).
  Si DOWN detecta que el ángulo de línea cambia ~constante mientras el
  robot avanza recto → línea curva. Emitir `EV_CURVED_LINE`.
- **Esquina**: ya cubierto por `EV_CORNER`.
- **Lateral vs fondo**: requiere pose. NO se resuelve en este ciclo.

### Gap 6 — Persistencia de calibración (EEPROM)

**Hoy**: calibración en RAM, se pierde al apagar. Cada arranque queda en
`Suspect`. En cancha real esto frena 30 segundos antes de poder jugar.

**Patrón Omicron**: EEPROM, persiste entre partidos.

**Diseño**: Teensy 4.0 tiene 1080 bytes de EEPROM emulado. Calibración
ocupa `32 sensores × 4 bytes (carpet+white as uint16) = 128 bytes`. Sobra
espacio.

---

## 5. Diseño propuesto

### 5.1 Principio de diseño

> "Mínima invasión, máxima reutilización del código que ya funciona."

NO reescribir la cadena vieja `line_ring`. NO romper la integración con
`strategy.cpp`. Agregar **módulos nuevos paralelos** que el `DownModel`
opcionalmente consume. Migración incremental, validada con tests
host-native antes de cada paso.

### 5.2 Nuevos módulos propuestos (3)

```
src/shared/
├── sensor_geometry.h+cpp           [✅ YA EXISTE desde 2026-05-24]
│   SENSOR_POS[32], sg_angle_deg, sg_radius_mm, sg_fill_angles_deg
│
├── sensor_health.h+cpp             [🆕 PROPUESTO P1]
│   Tracking per-sensor de transiciones, ruido y "stuck values".
│   Emite EV_SENSOR_NOISY si un sensor cambia >N veces/seg.
│   Provee bool sh_is_healthy(int i) que line_geometry filtra.
│
├── line_pattern_classifier.h+cpp   [🆕 PROPUESTO P2]
│   Más allá de "corner": detecta secuencia temporal del centroide
│   para EV_CURVED_LINE (área chica), confirma EV_CORNER con
│   tolerancia ampliada [45°, 135°].
│
└── tactical_line_modes.h+cpp       [🆕 PROPUESTO P2]
    Modos: NORMAL_AVOID, FOLLOW_PARALLEL_BACKWARD (arquero patrulla
    línea fondo), FOLLOW_PARALLEL_FORWARD (defensor avanza paralelo
    lateral). Activables por comando de CENTRAL. Cada modo computa
    una targeting line (línea de referencia + dirección de avance)
    que DOWN reporta como part of LineStatusV3 (o un campo nuevo).
```

### 5.3 Cambios menores en módulos existentes

```
shared/line_calib.h+cpp
  + lc_save_to_eeprom() / lc_load_from_eeprom() [P1]
  
shared/line_filters.h+cpp
  + lf_spatial_gap_fill() [P2] — completa gaps aislados (patrón Omicron)
  + lf_all_white_rejection() [P1] — anomalía simétrica a all_lifted
  
shared/down_model.h+cpp
  + opción usar lg_compute_xy() en lugar de lg_compute() [P1]
  + computar penetration_mm real con sg_radius_mm [P1]
  + computar cross_track_mm real con centroide cartesiano [P1]
  + consumir sensor_health para filtrar sensores ruidosos [P1]
  
shared/types.h
  + EV_SENSOR_NOISY, EV_SHOCK_DETECTED, EV_CURVED_LINE, EV_MUX_DEAD
  + nueva versión LineStatusV3 (24 bytes) con campos extra:
      - tactical_mode (1 byte): NORMAL_AVOID|FOLLOW_BACK|FOLLOW_LATERAL
      - target_line_angle (int16, centideg): línea de referencia
      - target_offset_mm (int16): cross-track deseado
      - healthy_sensor_count (1 byte): cuántos sensores activos
      - rejected_sensor_bitmap (4 bytes): bitmap de cuáles excluidos
```

### 5.4 Pipeline propuesto (post-cambios)

```
                                                  ┌─ sensor_health.tick (1 kHz)
[ADC raw 32 valores]                              │   tracking de transiciones
        │                                         │
        ▼ moving avg(4)                           ▼
[filtered]            ────────────────► reject_noisy[]  (bitmap)
        │                                         │
        ▼ hysteresis ±20                          │
[is_white_raw]                                    │
        │                                         │
        ▼ spatial: isolated rejection + gap fill  │
        │ (NUEVO: gap fill estilo Omicron)        │
[is_white_validated_v1]                           │
        │                                         │
        ├─ ALL-on rejection: si los 32 blancos    │ <- NUEVO P1
        │  → anomalía global, marcar lifted
        │
        ▼ excluir sensors marcados unhealthy ◄───┘
[is_white_validated_final]
        │
        ▼ lg_compute_xy(SENSOR_POS, ...) [NUEVO P1, geometría real]
[GeomResult: line_angle, escape_angle, sensors_on_line, corner]
        │
        ▼ line_pattern_classifier [NUEVO P2]
        │   - confirma corner con tolerancia ampliada
        │   - detecta línea curva (delta angular monotónico)
        │   - emite EV_CURVED_LINE, EV_CORNER (refined)
        │
        ▼ tactical_line_modes [NUEVO P2]
        │   - aplica el modo activo solicitado por CENTRAL
        │   - calcula target_line_angle, target_offset_mm para PID
        │
        ▼ LiftedDetector + SurfaceMonitor
        ▼ shock_detector [NUEVO P1, requiere IMU de TOP]
        │   - delta L2 grande + omega chico → EV_SHOCK_DETECTED
        ▼ LineCalib + adaptación carpet
        │
        ▼ down_encode → LineStatusV3 (24 bytes)
        │
        ▼ comm_central (200 Hz) → CENTRAL
        ▼ comm_top (100 Hz) → TOP
```

### 5.5 Casos de uso del coach — diseño concreto

#### Caso 1: Validación temporal + exclusión de sensor ruidoso

**Algoritmo `sensor_health`**:

```cpp
struct SensorHealth {
    uint16_t transition_count_1s[32];   // transiciones blanco↔no-blanco en último seg
    uint16_t stuck_count[32];           // ticks consecutivos con el mismo valor exacto
    bool     unhealthy[32];             // resultado
    uint32_t window_start_ms;
};

void sh_update(SensorHealth& s, const uint16_t* raw, const bool* is_white,
               uint32_t now_ms) {
    static bool was_white[32] = {false};
    for (int i = 0; i < 32; ++i) {
        if (is_white[i] != was_white[i]) ++s.transition_count_1s[i];
        was_white[i] = is_white[i];
    }
    if (now_ms - s.window_start_ms >= 1000) {
        // Cada 1 segundo, evaluar:
        for (int i = 0; i < 32; ++i) {
            // Más de 20 transiciones/segundo es ruido extremo (>10Hz de toggle)
            // — normalmente una línea pasa máximo 5-10 veces por segundo.
            if (s.transition_count_1s[i] > 20) s.unhealthy[i] = true;
            // Stuck: si lleva > 5000 ticks (5s) con el mismo raw exacto → muerto
            if (s.stuck_count[i] > 5000) s.unhealthy[i] = true;
            s.transition_count_1s[i] = 0;
        }
        s.window_start_ms = now_ms;
    }
}
```

**Integración**: `line_geometry` ignora i si `sh_is_healthy(i) == false`.
Reporta `healthy_sensor_count` y `rejected_sensor_bitmap` en LineStatusV3.

#### Caso 2: Detección de robot levantado por choque

**Algoritmo combinado** (DOWN + IMU del TOP):

```cpp
// En DOWN:
struct ShockDetector {
    uint16_t prev_filtered[32];
    uint32_t last_delta_ts_ms;
    float    last_omega_deg_s;  // RX de TOP cada 50ms
};

bool sd_detect_shock(ShockDetector& s, const uint16_t* filtered, uint32_t now_ms) {
    // Delta L2 (norma Euclidiana de la diferencia entre frame N y N-1)
    float delta_sq = 0.0f;
    for (int i = 0; i < 32; ++i) {
        float d = filtered[i] - s.prev_filtered[i];
        delta_sq += d * d;
        s.prev_filtered[i] = filtered[i];
    }
    float delta_norm = sqrtf(delta_sq);
    
    // Heurística:
    //   - delta_norm > 200 (cambio rms de ~35 por sensor a través de los 32)
    //   - AND |omega| < 50 deg/s (robot NO está rotando rápido)
    //   → es shock, no movimiento normal
    return delta_norm > 200.0f && fabsf(s.last_omega_deg_s) < 50.0f;
}
```

**Costo de integración**: TOP debe enviar `omega` a DOWN cada 50ms por
Serial5 RX. Comando nuevo en el protocolo. **Sin esto, no hay diferencia
robusta entre "rotación rápida normal" y "choque sin movimiento intencional"**.

#### Caso 3: Arquero patrullando paralelo a la línea de fondo

**Modo táctico `FOLLOW_PARALLEL_BACKWARD`**:

- CENTRAL detecta que el arquero está cerca de la línea de fondo (por
  `EV_IMMINENT_EXIT` por algunos frames). Envía comando a DOWN:
  `SET_TACTICAL_MODE = FOLLOW_PARALLEL_BACKWARD`.
- DOWN entra en modo: **fija el ángulo de la línea detectada como
  referencia** (`target_line_angle = line_angle observado`).
- Reporta cross-track con respecto a esa línea (`target_offset_mm`).
- CENTRAL corre un **PID de mantenimiento de cross-track** que controla
  la velocidad lateral del arquero para mantenerlo a distancia constante
  del borde. La velocidad longitudinal va para donde está la pelota.
- DOWN sale del modo cuando: `EV_LINE_END` → reporta automáticamente
  fin de la línea de fondo (típicamente esquina).

**Ventaja con SENSOR_POS real**: el cross-track se computa directamente
de las coords reales (no es proxy). Bias de ±2-3 mm.

#### Caso 4: Defensor avanzando paralelo a lateral sin salir

**Modo táctico `FOLLOW_PARALLEL_FORWARD`**:

- Similar al modo 3 pero la dirección dominante (forward) está
  90° al ángulo de la línea (perpendicular = avance, paralelo = línea de
  referencia).
- DOWN mantiene `target_line_angle` y reporta cross-track lateral.
- CENTRAL hace PID lateral + velocidad longitudinal libre.

#### Caso 5: Fin del área chica (línea curva)

**Algoritmo `line_pattern_classifier`**:

```cpp
// Ventana deslizante de los últimos 10 frames de line_angle.
// Si el delta entre frames es monotónico (todos positivos o todos
// negativos) y la magnitud acumulada supera 30° en 100 ms (10 frames
// a 100 Hz de output), es CURVA.

struct PatternClassifier {
    float angle_history[10];
    int   write_idx;
};

bool pc_is_curved(const PatternClassifier& p) {
    int monotonic_pos = 0, monotonic_neg = 0;
    float total_delta = 0;
    for (int i = 1; i < 10; ++i) {
        float d = p.angle_history[i] - p.angle_history[i-1];
        if (d > 1.0f)      ++monotonic_pos;
        else if (d < -1.0f) ++monotonic_neg;
        total_delta += fabsf(d);
    }
    return (monotonic_pos >= 7 || monotonic_neg >= 7) && total_delta > 30.0f;
}
```

**Emisión**: `EV_CURVED_LINE` durante la curva. CENTRAL interpreta esto
como "estoy en el área chica, no es una línea recta de fondo o lateral".

#### Caso 6: Esquina (detección refinada)

**Mejora sobre `EV_CORNER` actual**:
- Ampliar tolerancia angular de [55°, 125°] a [45°, 135°] (recomendación
  ya marcada en `line_geometry.cpp` línea 49 como "ampliar si hay falsos
  negativos").
- Reportar **el número de clusters detectados** (uint8 en LineStatusV3)
  para que CENTRAL distinga "esquina simple (2 clusters)" de "esquina
  triple = área chica intersectando lateral (3 clusters)".

---

## 6. Recomendaciones priorizadas (formato coach)

### Tema 1 — Activar EV_MUX_DEAD + watchdog por mux

**Categoría:** electrónica / control
**Robot afectado:** ambos
**Prioridad:** P0

**Qué observo.** En `down_model.cpp` y `types.h`, el flag
`EV_MUX_DEAD = 0x20` está **declarado** pero NUNCA seteado por el
firmware. Si en Incheon un mux se cuelga (líneas SEL flotantes, ESD)
el robot **no se entera** y juega con datos corruptos.

**Risk-no-fix.** Lectura de un mux entero queda fija o errática → ángulo
de línea calculado mal → fail-safe de borde dispara mal → gol en contra
o expulsión.

**Risk-fix.** Trivial. Agregar en `down_model.cpp` un check: si los 8
sensores de un mux dan el mismo valor exacto por >100 ms consecutivos,
setear `EV_MUX_DEAD`. Riesgo de false positive en superficie totalmente
uniforme (parche en cancha real con valores idénticos por azar). Mitigar
exigiendo además que el valor sea ≤50 o ≥970 (extremos del ADC).

**Tiempo estimado.** 2 horas (impl + test host-native).

**Plan de prueba en hardware real.**
1. Setup: placa DOWN + diag_down, batería ON, COM10.
2. Test positivo: cortocircuitar manualmente un mux a GND con cable
   (desenergizado → energizar) → confirmar que dentro de 200 ms el
   serial reporta `EV_MUX_DEAD` con el bitmap correcto del mux culpable.
3. Test negativo (regresión): pasar el robot por cancha con todos los
   muxes sanos → confirmar que NUNCA se setea `EV_MUX_DEAD`.

### Tema 2 — Persistir calibración en EEPROM

**Categoría:** firmware / operación
**Robot afectado:** ambos
**Prioridad:** P1

**Qué observo.** `line_calib.cpp` mantiene `carpet[i]`/`white[i]` en
RAM. Cada power cycle se pierde. En cancha real (Incheon), recalibrar
toma ~30 seg antes de cada partido — tiempo que podría usarse para
otra cosa.

**Risk-no-fix.** Operación más lenta entre partidos. Si por algún motivo
no se calibra, el firmware queda en `EV_CALIB_SUSPECT` → `data_valid=0`
→ CENTRAL ignora línea → fail-safe roto.

**Risk-fix.** Bajo. Teensy 4.0 EEPROM emulada hasta 1080 bytes; calibración
requiere ~128 bytes. Riesgo: si una sesión calibra con cancha de
practice (verde claro) y después juega en Incheon (verde más oscuro
quizás), los umbrales viejos pueden estar mal. Mitigación: comando "force
recalibrate" siempre disponible desde CENTRAL.

**Tiempo estimado.** 3 horas (impl + test + integración con
`comm_central`).

**Plan de prueba en hardware real.**
1. Calibrar con superficie clara (cartulina blanca + cartón negro).
2. Verificar valores en `EV_CALIB_SUSPECT` flag = false.
3. Power cycle completo (USB + batería desconectados 10s).
4. Reconectar → verificar que `EV_CALIB_SUSPECT` = false (calibración
   sobrevivió).
5. Forzar recalibración via comando → confirmar update.

### Tema 3 — Migrar `down_model` a `lg_compute_xy()` con geometría real

**Categoría:** firmware / control
**Robot afectado:** ambos
**Prioridad:** P1

**Qué observo.** `lg_compute_xy()` está implementado y testeado (20/20
tests pass, commit `34e3025`), pero `down_model::dm_update()` sigue
usando `lg_compute()` con ángulos uniformes aproximados. La asimetría
de los 3 anillos del PCB DOWN (R≈37, 54, 80-87 mm) hace que el ángulo
calculado se desvíe respecto al real cuando solo unos pocos sensores ven
blanco.

**Risk-no-fix.** `line_angle` puede diferir ±10° del real cuando el
patrón de sensores blancos es asimétrico (típico al cruzar línea oblicua).
PID lateral del arquero (cuando se implemente) puede oscilar.

**Risk-fix.** Medio. Cambia comportamiento del `line_angle` que va al
CENTRAL → fail-safe de borde puede tener reacciones distintas en cancha.
Riesgo de introducir bug que solo se ve corriendo en cancha real.

**Tiempo estimado.** 1 día (impl + verificación con datos reales).

**Plan de prueba en hardware real.**
1. Compilar y flashear `diag_down` con la nueva ruta.
2. Apoyar robot sobre cancha verde con línea blanca atravesando el
   anillo en distintos ángulos conocidos (0°, 45°, 90°).
3. Confirmar que `line_angle` reportado coincide con el real ±2°.
4. Test de regresión: ejecutar `pio test -e test_native -f test_down`
   completo. Si tests caracterización pasan, el comportamiento global no
   cambió crítico.
5. **Validación en cancha real con el robot moviéndose** — no firmar
   esta TASK como done sin esto.

### Tema 4 — Implementar `sensor_health` (exclusión dinámica de ruidoso)

**Categoría:** firmware / fault-tolerance
**Robot afectado:** ambos
**Prioridad:** P1

**Qué observo.** No hay defensa contra un sensor que de repente empieza
a oscilar (toque mecánico, soldadura intermitente). Hoy contamina el
ángulo silenciosamente.

**Risk-no-fix.** Un sensor roto en Incheon no se detecta hasta que el
arbitraje retire al robot. Pérdida de tiempo + frustración en torneo.

**Risk-fix.** Bajo. Módulo nuevo aislado, no toca cadena existente.
Riesgo de false positive en sensor que ve "línea + cancha alternando
correctamente al cruzar borde rápido" — mitigado por usar ventana de 1
segundo y umbral conservador (20 transiciones/s = 10 Hz, mucho más que
normal).

**Tiempo estimado.** 1 día (impl + tests host-native + integración).

**Plan de prueba en hardware real.**
1. Robot sobre cancha quieto → verificar que `healthy_sensor_count = 32`.
2. Tocar manualmente un sensor con dedo + soltar repetidamente >15 veces
   en 1 segundo → confirmar `EV_SENSOR_NOISY` + ese sensor en el bitmap.
3. Mover robot normal por cancha → cuando el sensor NO ruidoso pasa por
   línea, verificar que NO se marca como noisy.

### Tema 5 — Detección de shock con IMU (Δlectura + omega)

**Categoría:** firmware / sensor-fusion / robustez
**Robot afectado:** ambos
**Prioridad:** P2 (depende de integración DOWN ↔ TOP)

**Qué observo.** El `EV_LIFTED` actual solo cubre el caso "robot al aire".
Un choque que rota el robot 30-60° en 100ms no es "lifted" pero sí debería
invalidar la lectura por unos frames hasta que se estabilice.

**Risk-no-fix.** En un choque entre robots (común en RCJ), la lectura de
línea de los siguientes 100-300 ms puede ser basura. Si CENTRAL la usa,
puede tomar decisiones tácticas mal informadas.

**Risk-fix.** Alto. Requiere extender el protocolo Serial5 para que TOP
envíe omega del IMU a DOWN cada 50 ms. Cambio en `comm_top.cpp` (DOWN) +
`comm_down.cpp` (TOP). Riesgo de race condition si la trama de IMU se
desincroniza.

**Tiempo estimado.** 2-3 días (protocolo + integración + tests).

**Plan de prueba en hardware real.**
1. Pre-requisito: TASK-031 cerrada (UART real DOWN↔TOP funcionando).
2. Robot sobre cancha quieto → empujar con la mano 30° en <100ms →
   verificar `EV_SHOCK_DETECTED` por 1-2 frames.
3. Robot girando suavemente 30°/seg → verificar que NO se marca shock
   (`omega > threshold` lo descarta).

### Tema 6 — Modos tácticos `FOLLOW_PARALLEL_*` (arquero + defensor)

**Categoría:** firmware / estrategia
**Robot afectado:** ambos
**Prioridad:** P2 (potencial impacto alto pero requiere integración con strategy)

**Qué observo.** Casos 3 y 4 del coach no están implementados. Hoy el
arquero patrulla "ciego" lateralmente; cuando llega cerca de la línea de
fondo, el único feedback es `EV_IMMINENT_EXIT` que dispara retreat.

**Risk-no-fix.** El arquero NO puede usar la línea de fondo como
referencia para patrullar con precisión. En cancha grande de Incheon, eso
puede traducirse en goles evitables.

**Risk-fix.** Alto. Toca múltiples módulos: DOWN (`tactical_line_modes`),
protocolo (comando nuevo + LineStatusV3), CENTRAL (estado FSM nuevo +
PID lateral). Riesgo de regresión en el arquero que ya funciona.

**Tiempo estimado.** 4-5 días.

**Plan de prueba en hardware real.**
1. Pre-requisitos: Temas 1, 2, 3 cerrados (calibración robusta + geometría
   real).
2. Robot sobre cancha grande con línea de fondo marcada.
3. CENTRAL setea modo `FOLLOW_PARALLEL_BACKWARD`.
4. Mover robot perpendicular a línea de fondo → verificar que `cross_track_mm`
   sigue siendo medible mientras se ve la línea, y que CENTRAL puede
   corregir lateralmente.
5. Test de salida: el robot NO debe sacar más del 25% (regla conservadora)
   en ningún momento.

---

## 7. Recomendación de roadmap para Incheon

**Lo que cabe en el tiempo restante (~25 días hasta Incheon)**:
- ✅ Temas 1, 2, 3, 4 (4-5 días estimados, sin paralelizar).
- ❌ Temas 5 y 6 quedan **para Nacional 2026 / Mundial 2027**.

**Justificación coach**: el roadmap del repo dice "inversión en
aprendizaje, no en podio". Mejor llegar a Incheon con un fail-safe
robusto (Temas 1-4) que con todo el catálogo táctico (Temas 5-6)
medio implementado y sin probar.

**Filtro Omicron de "all-on rejection"** (sub-tema de Tema 1 si se
quiere): trivial de agregar, capítulo `lf_all_white_rejection()` ya
mencionado. Tiempo: 30 min.

---

## 8. Referencias

### Equipos top consultados

- LNX Robots (Hannover 2024 champion): <https://lnxrobots.github.io/docs/2023/LNX_Robots_documentation.pdf>
- Team Omicron (Junior, repo): <https://github.com/mattyoung101/omicron>
- Bohlebots (Hannover 2024 runner-up, repo): <https://github.com/stiebel/bohlebots>
- Cardano Robotics (repo): <https://github.com/gianlucafarinaccio/cardano-robotics>
- Tech United Eindhoven (MSL 2024 champion): <https://link.springer.com/chapter/10.1007/978-3-031-85859-8_40>
- CAMBADA (MSL, omnidirectional vision): <https://www.sciencedirect.com/science/article/abs/pii/S0957415810000863>
- Reliability-based particle filter (SPL): <https://pmc.ncbi.nlm.nih.gov/articles/PMC3871090/>
- Awesome RCJ Soccer index: <https://github.com/robocup-junior/awesome-rcj-soccer>
- LNX vs Bohlebots final Hannover 2024 (YouTube): <https://www.youtube.com/watch?v=SR-EyCEltZw>

### Docs internos del repo

- `docs/firmware/FIRMWARE-PLACA-ABAJO.md`
- `docs/firmware/CONTRATO-DATOS-DOWN.md`
- `hardware/electronics/down-board-pack/02-funcionalidad.md`
- `hardware/electronics/down-board-pack/01-pinout-y-posiciones.md` §5b
- `journal/2026-05-24-sensor-geometry-real-firmware-down.md`

### Código del repo citado

- `src/shared/line_filters.cpp` (filtro temporal + spatial + hysteresis + lifted)
- `src/shared/line_geometry.cpp` (centroide + corner detection)
- `src/shared/line_tracker.cpp` (LINE_END one-shot)
- `src/shared/line_calib.cpp` (calibración per-sensor + suspect detection)
- `src/shared/surface_monitor.cpp` (lifted con SurfaceMonitor wrapper)
- `src/shared/down_model.cpp` (orquestador del pipeline)
- `src/shared/sensor_geometry.cpp` (NUEVO 2026-05-24: SENSOR_POS[32])
- `src/central/strategy.cpp` (consumidores de LineStatusV2)

## 9. Vacíos que este doc NO resuelve

Sé honesto: **muchos equipos top de RCJ Soccer Open NO publican TDPs
con el detalle que necesitaría este diseño** (HBR, Skuba, Boxiang, GoalEE,
EsCo, PE-COJI, RFC Vienna). La síntesis acá está limitada a lo que es
público y accesible. Para 2027 valdría la pena:

1. **Asistir a Mundial 2026 con micrófono abierto** — observar in situ
   cómo se comportan los robots top frente a esquinas, choques, línea
   curva. Filmar (con permiso) para análisis post.
2. **Contactar directamente equipos open-source** — mattyoung101
   (Omicron) y stiebel (Bohlebots) tienen Discord/email públicos.
3. **Considerar comprar libros de proceedings RoboCup Symposium 2023/2024**
   (Springer) si HBR/Skuba publicaron ahí.

## 10. Atribución

- Investigación externa + diseño + redacción: Claude Opus 4.7 (Anthropic),
  sesión 2026-05-24.
- Pedido y dirección de scope: Gustavo Viollaz (@gviollaz), coach IITA.
- Material previo del repo: equipo IITA Salta + sesiones Claude
  anteriores (ver journals 2026-05-*).

> **Frame check.** Este doc es **propuesta para discusión**, no
> implementación. La regla 6 de CLAUDE.md exige que cambios de
> producción sean aprobados explícitamente por el coach. Las TASKs
> derivadas de los Temas 1-6 deben crearse después de feedback de
> Gustavo + equipo.
