---
title: "Auditoría exhaustiva, crítica y objetiva de la placa DOWN — pre-Incheon"
date: 2026-05-29
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: in-progress
tipo: auditoria-riesgo
robot: ambos
area: integral-placa-down
horizonte: "Incheon 2026 (T-25 días)"
related-files:
  - software/teensy/Soccer 2026/src/down/*
  - software/teensy/Soccer 2026/src/shared/{line_*,sensor_*,calib_storage,down_*,surface_monitor}.{h,cpp}
  - hardware/electronics/down-board-pack/
related-docs:
  - research/in-progress/2026-05-24-deteccion-linea-down-investigacion-y-diseno.md
  - journal/2026-05-24-down-board-passing-tests-cierre.md
related-tasks: [TASK-001, TASK-009, TASK-029, TASK-031]
tags: [audit, riesgo, fmea, placa-down, pre-incheon, fail-safe, robustez]
---

# Auditoría exhaustiva de la placa DOWN — pre-Incheon 2026

> **Frame del documento.** Este es un audit **CRÍTICO y OBJETIVO**, no
> una validación complaciente. El objetivo es detectar TODO lo que puede
> fallar en cancha real (Incheon 2026, ~25 días), no celebrar lo que ya
> está hecho. Está basado en:
>
> - Revisión exhaustiva del código del firmware (`src/down/*` + módulos
>   `shared/` consumidos por DOWN).
> - Revisión del schematic + PCB físico (placa 04-12).
> - Cross-check con TDPs de equipos top (LNX, Bohlebots, Omicron).
> - Test results en hardware (los que existen) y huecos de testing.
> - Audit independiente con agente Explore (prompt no-sesgado).
>
> Sale con priorización **P0 / P1 / P2** + plan de pruebas en hardware
> real. No es decisión final; es input para decisión del coach.

---

## 1. Resumen ejecutivo

**Nivel de riesgo global para Incheon: MEDIO-ALTO.**

El subsistema DOWN está **operacional a nivel banco** (hardware
validado 2026-05-24, 246 tests host-native pasan, geometría real
implementada 2026-05-29). Pero **tiene 3 bloqueadores sistémicos y
~10 modos de fallo plausibles de impacto medio-alto** que pueden
activarse en condiciones reales de competencia. Los 3 críticos son:

1. **UART real DOWN↔TOP/CENTRAL NUNCA validada en hardware**
   (TASK-031 postergada). El firmware está listo pero ni un solo byte
   ha viajado por Serial5/Serial1 a otra placa real. Si hay un problema
   de cable, baud rate o timing, el robot **no siente la línea en cancha**.
2. **`calib_storage` está implementado pero NO INTEGRADO** en
   `main_down.cpp` setup/loop. Cada power cycle = calibración perdida.
   En un torneo con N matches y power cycles entre matches, esto se
   traduce en N momentos de "robot arranca con datos inválidos".
3. **CENTRAL consume solo 2 de los 7 flags que DOWN emite hoy**
   (`imminent_exit`, `line_angle`). Los otros 5 (`mux_dead`,
   `sensor_noisy`, `calib_suspect`, `degraded_geometry`, `corner`,
   `line_end`) son emitidos pero ignorados → **degradación silenciosa**:
   el robot puede seguir jugando con datos parcialmente corruptos sin
   que CENTRAL lo sepa.

El resto del documento detalla 10 modos de fallo concretos, 6 riesgos
sistémicos y un roadmap de 6 horas para mitigar los 3 críticos antes
de Incheon.

---

## 2. Estado del subsistema DOWN al 2026-05-29

### 2.1 Hardware físico (PCB 04-12 fabricada abril 2026)

| Componente | Estado | Verificado por |
|---|---|---|
| **Pinout Teensy↔Mux** (12 SEL + 4 ADC) | ✅ Correcto | Extracción schematic + lectura empírica 2026-05-24 (32 sensores responden) |
| **32 sensores ALS-PT19 + LED** | ✅ 0 muertos | `diag_capture.py verdict`; lámina protectora removida 2026-05-24 |
| **2 OTOS U5 + U6** | ✅ Operacional | Ambos responden I²C 0x17; láminas removidas; error 6.5% sobre cartón corrugado |
| **Alimentación (2× MP1584 buck)** | ❓ **NUNCA MEDIDO** | Trimpot factory; nadie midió 5V/3.3V con multímetro |
| **Capacitores desacople + Schottky** | ✅ BOM correcto | Montados según schematic; comportamiento bajo carga del solenoide no testado |
| **Conectores U10 (→TOP) y U11 (→CENTRAL)** | ⚠️ Físicos OK, **cable NUNCA usado en operación real** | Cableado existe pero el firmware nunca emitió por estos puertos en condiciones reales |
| **Carrier mecánico + montaje en chasis** | ❓ Pendiente | Orientación física del PCB respecto al robot (+Y = adelante?) sin validar (TASK-027) |

**Hallazgos hardware no resueltos:**

- **TASK-001** (10 nets PCB DOWN no ruteadas) — Estado real? Si la placa 04-12
  llegó con bugs físicos, ¿cómo es que los 32 sensores responden? Necesita verificación
  en próxima sesión.
- **Lámina protectora OTOS:** la removimos pero queda sin tapa de protección
  (TASK-030 closed). El lente queda expuesto a polvo y golpes — riesgo en transporte
  internacional a Incheon.

### 2.2 Firmware DOWN — pipeline completo al 2026-05-29

```
┌─────────────────────────────────────────────────────────────────┐
│  Tick @ 1 kHz (líneas) + 100 Hz (OTOS)                          │
└─────────────────────────────────────────────────────────────────┘

[ADC raw 32 sensores, 10-bit (0-1023)]
        │
        ▼ moving average de 4 muestras  (lf_temporal_update)
[filtered]
        │
        ▼ hysteresis ±20 counts          (lf_hysteresis_on_white)
[is_white_raw]
        │
        ▼ filtro espacial (vecino requerido) (lf_spatial_filter)
[is_white_validated_pre]
        │
        ├──► MuxWatchdog   [NUEVO 2026-05-29]
        │    ¿8 sensores de un mux pegados a valor extremo
        │     por >100ms? → EV_MUX_DEAD + dead[mux_idx]
        │
        ├──► SensorHealth  [NUEVO 2026-05-29]
        │    ¿sensor con >20 transiciones/seg o stuck >5s?
        │    → unhealthy[i] = true
        │
        ▼ exclusión: validated[i] = false si !sh_is_healthy(i)
[is_white_validated_final]
        │
        ▼ centroide:
        │   - si n==32: lg_compute_xy(SENSOR_POS) [geometría real]
        │   - si n<32:  lg_compute(angles) [fallback uniforme]
        │   + corner detection siempre via lg_compute(angles)
[GeomResult: line_angle, escape_angle, sensors_on_line, corner]
        │
        ├──► SurfaceMonitor / LiftedDetector
        │    ¿28/32 sensores < carpet-50 por >100ms? → EV_LIFTED
        │
        ├──► LineCalib::lc_is_suspect
        │    ¿algún sensor con |white-carpet|<120? → EV_CALIB_SUSPECT
        │
        ├──► LineTracker::lt_update
        │    ¿línea presente ≥200ms y desapareció? → EV_LINE_END
        │
        ▼ down_encode → LineStatusV2 (16 bytes)
        │   data_valid = !lifted && !suspect && !any_mux_dead
        │   event_flags = OR de EV_* arriba
        │
        ▼ comm_central (Serial1, 200 Hz, conector U11) → CENTRAL [⚠️ NUNCA PROBADO REAL]
        ▼ comm_top     (Serial5, 100 Hz, conector U10) → TOP     [⚠️ NUNCA PROBADO REAL]
```

### 2.3 Módulos vivos (al 2026-05-29)

| Módulo | Estado | Cobertura tests | Probado HW |
|---|---|---|---|
| `line_ring` | ✅ Vivo | 17 tests | ✅ banco |
| `line_filters` (temporal, hysteresis, spatial, lifted) | ✅ Vivo | 22 tests | ✅ banco |
| `MuxWatchdog` 🆕 | ✅ Activado en dm_update | 11 tests (incl. wrap-safe) | ❌ |
| `SensorHealth` 🆕 | ✅ Activado en dm_update | 12 tests (incl. wrap-safe) | ❌ |
| `SurfaceMonitor` | ✅ Vivo | parcial | ✅ banco |
| `LineCalib` | ✅ Vivo | parcial | ✅ banco |
| `LineTracker` | ✅ Vivo | parcial | ❌ no probado fin línea real |
| `line_geometry` (`lg_compute`) | ✅ Vivo | 6 tests | ✅ banco |
| `line_geometry` (`lg_compute_xy`) 🆕 | ✅ Activado en dm_update si n=32 | 4 tests | ❌ |
| `sensor_geometry` (`SENSOR_POS[32]`) 🆕 | ✅ LUT cargada | 6 tests | ⚠️ eje X validado, eje Y pendiente (TASK-027) |
| `calib_storage` 🆕 | ⚠️ **CÓDIGO LISTO, NO INTEGRADO** | 19 tests | ❌ |
| `eeprom_calib` 🆕 | ⚠️ **GLUE LISTO, NO LLAMADO** | n/a (Arduino-only) | ❌ |
| `otos` | ✅ Vivo, lib SparkFun activada | parcial | ✅ banco (cartón corrugado, 6.5% error) |
| `comm_top` | ⚠️ Código listo | parcial | ❌ **0 bytes reales emitidos** |
| `comm_central` | ⚠️ Código listo | parcial | ❌ **0 bytes reales emitidos** |
| `down_model` | ✅ Orquestador (integra todo) | 30 tests | ✅ banco indirecto |

### 2.4 Lo que CENTRAL realmente consume

Lectura de `src/central/strategy.cpp` (audit anterior):

**CENTRAL lee de `LineStatusV2`:**
- ✅ `imminent_exit_flag` → dispara `LINE_AVOID` en arquero y delantero.
- ✅ `line_angle_centideg` → calcula `retreat_angle = angle + 180°`.
- ✅ watchdog 500 ms → si DOWN no envía, falla segura.

**CENTRAL IGNORA:**
- ❌ `data_valid` — **CRÍTICO**: si CENTRAL no chequea esto antes de usar
  line_angle, puede usar un ángulo sesgado cuando hay mux muerto, robot
  levantado o calibración sospechosa.
- ❌ `event_flags`: `EV_CORNER`, `EV_LINE_END`, `EV_LIFTED`,
  `EV_CALIB_SUSPECT`, `EV_MUX_DEAD`, `EV_DEGRADED_GEOMETRY`,
  `EV_SENSOR_NOISY`.
- ❌ `escape_angle_centideg` (declarado, nunca consumido).
- ❌ `cross_track_mm` (nunca computado por DOWN).
- ❌ `quality` (setea 85/95, CENTRAL ignora).
- ❌ `sample_age_ms` (no usado para detectar staleness).

**Implicación:** DOWN está **sobre-produciendo** señales que CENTRAL no usa.
Esto es deuda de integración, no bug en DOWN. Pero en práctica significa
que el sistema **no se entera** cuando DOWN reporta degradación.

---

## 3. Modos de fallo plausibles — FMEA top 10

Cada modo: causa → detección actual → mitigación → probabilidad Incheon →
impacto. Ordenados por **riesgo neto** (probabilidad × impacto).

### Modo 1: UART DOWN→CENTRAL no funciona en cancha (TASK-031 sin cerrar)

- **Causa raíz:** Cable mal confeccionado / pin swapped / baud rate
  desajustado / interferencia EMI / firmware nunca emitió bytes reales.
- **Detección actual:** ✅ CENTRAL tiene watchdog 500 ms (`line_is_fresh()`).
- **Mitigación actual:** Si DOWN cuelga, CENTRAL después de 500 ms deja de
  confiar en `line_angle`. Pero **el robot pierde sentido sensorial de línea
  durante el match**: el fail-safe de borde queda inoperativo → posible
  expulsión por cruzar borde.
- **Probabilidad Incheon:** **ALTA** (50%) si no se prueba antes.
- **Impacto:** **GOL EN CONTRA o EXPULSIÓN.**
- **Recomendación:** P0.1 — validar en banco antes de viajar.

### Modo 2: Calibración perdida entre matches (calib_storage no integrado)

- **Causa raíz:** `main_down.cpp` no llama a `ec_load_calibration()` en
  setup. Cada power cycle arranca con threshold default (500) sin carpet
  conocido → `EV_CALIB_SUSPECT` → `data_valid=0`.
- **Detección actual:** ✅ LineCalib emite `EV_CALIB_SUSPECT`.
- **Mitigación actual:** Hasta que TOP envíe comando de calibración, DOWN
  reporta `data_valid=0` → CENTRAL ignora línea → fail-safe roto.
- **Probabilidad Incheon:** **ALTA** (90%, si hay power cycle entre matches
  es seguro).
- **Impacto:** Match arranca degradado; necesita 30 seg de calibración
  manual antes de cada partido + olvidos humanos.
- **Recomendación:** P0.2 — 1 hora de trabajo.

### Modo 3: CENTRAL ignora `data_valid` / `EV_MUX_DEAD`

- **Causa raíz:** `strategy.cpp` lee `line_angle` sin chequear primero
  `data_valid==1`. Si hay mux muerto, DOWN sigue emitiendo un ángulo
  (calculado con 24 sensores), pero está sesgado sistemáticamente.
- **Detección actual:** ✅ DOWN reporta `data_valid=0` cuando mux_dead
  (fix audit 2026-05-29).
- **Mitigación actual:** **Inefectiva** si CENTRAL no usa la guard.
- **Probabilidad Incheon:** BAJA por causa (mux muere), pero CONDITIONAL si
  pasa, va a ser silencioso.
- **Impacto:** Sesgo angular constante → robot gira mal en LINE_AVOID.
- **Recomendación:** P0.4 — code review strategy.cpp y agregar guard.

### Modo 4: Brown-out del Teensy al patear

- **Causa raíz:** Spike de corriente al activar solenoide del kicker (delantero)
  induce caída momentánea de Vcc → Teensy resetea.
- **Detección actual:** ❌ Sin monitoreo de Vcc (no se lee el ADC de referencia
  interna).
- **Mitigación actual:** Capacitores de desacople 100 nF montados, diodos
  Schottky D1/D2 — pero **dimensionamiento NO verificado contra peak del
  solenoide**.
- **Probabilidad Incheon:** MEDIA (común en robots Junior). Mitigable con
  softstart del solenoide o capacitor electrolítico más grande en Vcc.
- **Impacto:** Reset de Teensy (~2 seg de bootup) durante match = pérdida
  parcial de set → gol probable.
- **Recomendación:** P1.2 — softstart firmware solenoide.

### Modo 5: Cambio brusco de luz ambiente (flash cámara TV, sol directo)

- **Causa raíz:** ALS-PT19 es sensor ambiental (no IR-filtrado); luz visible
  intensa lo satura.
- **Detección actual:** ⚠️ SurfaceMonitor detecta "todos por debajo de
  carpet-50" (caso "robot levantado"), pero NO detecta "todos por encima
  de carpet+X" (caso "luz extrema").
- **Mitigación actual:** Filtro temporal (moving avg 4 muestras) + hysteresis
  ±20 absorben flicker corto (1-2 frames). Adaptación carpet (α=0.02) se
  ajusta a cambios graduales.
- **Hueco:** Caso "todos a blanco súbito por flash" puede pasar el filtro
  espacial (cluster grande contiguo = línea válida) y disparar
  EV_IMMINENT_EXIT falso.
- **Probabilidad Incheon:** MEDIA (RCJ con cámara TV).
- **Impacto:** Robot dispara LINE_AVOID innecesario → pierde 1-2 seg de
  juego.
- **Recomendación:** P1.5 (nueva) — implementar all-white rejection
  estilo Omicron (sub-tema del research doc, 30 min).

### Modo 6: Overflow del buffer Serial1 si CENTRAL no drena

- **Causa raíz:** DOWN escribe a 200 Hz por Serial1; CENTRAL debería leer
  a igual o mayor velocidad. Si el firmware CENTRAL se cuelga o lentea, el
  buffer hardware (64 bytes en Teensy 4.x) se llena.
- **Detección actual:** ❌ `comm_central.cpp` no chequea `Serial1.availableForWrite()`
  antes de TX.
- **Mitigación actual:** Si el buffer está lleno, `Serial1.write()` BLOQUEA
  hasta que se libere espacio → el loop principal de DOWN se demora →
  muestreo 1 kHz se rompe.
- **Probabilidad Incheon:** BAJA (firmware CENTRAL probado en banco), pero
  CRÍTICA si pasa.
- **Impacto:** Línea con muestreo degradado → ángulo ruidoso.
- **Recomendación:** P1.6 — agregar `availableForWrite()` check con skip
  si buffer lleno. 30 min.

### Modo 7: OTOS pierde tracking momentáneo durante movimiento rápido

- **Causa raíz:** Sensor óptico tipo mouse depende de features del piso. Al
  acelerar > velocidad límite, puede perder y reconstruir mal.
- **Detección actual:** ❌ Sin chequeo de "magnitud de delta_pose vs delta
  esperado".
- **Mitigación actual:** Ninguna. La pose acumulada queda con un salto.
- **Probabilidad Incheon:** BAJA-MEDIA (alfombra RCJ tiene buena textura,
  típicamente).
- **Impacto:** Pose con offset persistente → CENTRAL tiene posición errónea
  hasta que se recalibre por cámara (si se hace).
- **Recomendación:** P2.3 — agregar `slip_detection` en `otos_tick` con
  threshold de velocidad máxima. Diferido a 2027.

### Modo 8: Sensor individual ruidoso / stuck (soldadura, vibración)

- **Causa raíz:** Soldadura intermitente del fototransistor / LED
  desencajado del lente por vibración / traza partida.
- **Detección actual:** ✅ **SensorHealth** detecta >20 transiciones/seg
  o >5s stuck → excluye del centroide.
- **Mitigación actual:** ✅ **Efectiva.** Sensor problemático no contamina
  el ángulo. CENTRAL recibe `EV_SENSOR_NOISY` (pero lo ignora — ver Modo 3).
- **Probabilidad Incheon:** BAJA-MEDIA (vibración mecánica acumulada).
- **Impacto:** Pérdida de 1-3 sensores de los 32 → degradación mínima del
  ángulo (centroide sigue siendo correcto con 29 de 32).
- **Recomendación:** Mitigación actual es OK. Nada urgente.

### Modo 9: Mux entero CD4051 muere (chip quemado por ESD, glitch)

- **Causa raíz:** ESD en pines SEL, glitch de voltaje al power-on, defecto
  de fabricación. CD4051 queda con COM pegado a Vcc o GND.
- **Detección actual:** ✅ **MuxWatchdog** detecta los 8 sensores del mux
  en valor extremo por >100ms → `EV_MUX_DEAD`, `data_valid=0`.
- **Mitigación actual:** ✅ **Efectiva** si CENTRAL chequea data_valid.
  Inefectiva si CENTRAL ignora (ver Modo 3).
- **Probabilidad Incheon:** BAJA (~5%).
- **Impacto:** Pérdida de 1/4 del anillo → CENTRAL debería dejar de confiar
  en line_angle. Con el fix de hoy (`data_valid` incluye `!any_mux_dead`),
  el contrato es honesto.
- **Recomendación:** **Validar P0.4** (que CENTRAL respete data_valid).

### Modo 10: Lámina protectora OTOS no removida completamente

- **Causa raíz:** Lámina removida 2026-05-24, pero si por sello se re-pega
  (transporte internacional, calor), OTOS no enfoca.
- **Detección actual:** ❌ Sin chequeo automático. OTOS reporta pose=(0,0,0)
  pero el firmware no distingue "robot quieto" de "OTOS no ve nada".
- **Mitigación actual:** Visual inspection manual antes de match.
- **Probabilidad Incheon:** BAJA (lámina removida, robot guardado en caja).
- **Impacto:** OTOS reporta 0 movimiento → localization roto.
- **Recomendación:** P1.7 — checklist pre-match: "verificar lente OTOS limpio".

---

## 4. Riesgos sistémicos (más allá de modos puntuales)

### Riesgo A — Deuda viva `line_ring` + `DownModel` en paralelo

Hoy `main_down.cpp` corre `line_ring` (cadena vieja, lectura cruda 1 kHz)
**y** `comm_central.cpp` invoca `DownModel` (cadena nueva con todos los
detectores) en paralelo. Ambas leen los mismos sensores 2 veces.

- **Por qué está así:** decisión consciente del coach ("no archivar antes
  de Incheon", regla CLAUDE.md).
- **Riesgo:** Si alguien toca el filtro temporal sin entender que sensor_health
  necesita `raw` sin filtrar, la detección de stuck falla silenciosamente.
- **Mitigación:** Después de Incheon, decisión binaria — archivar `line_ring`
  o documentar formalmente la separación.

### Riesgo B — Tests host-native NO simulan condiciones de cancha real

246 tests pasan, todos matemáticos/unitarios. Pero:

- No hay test de luz ambiente variando.
- No hay test de patada de solenoide durante UART TX.
- No hay test de vibración prolongada.
- No hay test de timing real (latencia UART medida con osciloscopio).

**Conclusión:** Los tests validan **lógica**, no **comportamiento real**.

### Riesgo C — Sin instrumentación de telemetría en vuelo

DOWN reporta a CENTRAL por UART, pero no hay un canal de "debug logs"
visible durante el match. Si algo falla, los humanos no saben qué pasó
hasta el post-match.

- **Mitigación parcial:** El protocolo proto.h soporta tramas tipo
  `DEBUG_LOG`, pero ninguna sesión la ha implementado.

### Riesgo D — Trade-off de ALS-PT19 vs LED IR + fotodiodo

Investigación previa (research doc 2026-05-24):
- LNX (campeón Hannover 2024): LED IR + fotodiodo discreto (OSRAM SFH-4656).
- ALS-PT19 (lo nuestro): sensor ambiental fototransistor + LED visible.

Trade-off:
- ALS-PT19 es **simple** (un chip vs 2 componentes), económico (~$0.50 vs $1.50),
  BOM compacta.
- ALS-PT19 es **menos robusto** ante luz ambiente intensa.

Para Incheon 2026 esto es decisión arquitectónica que no se cambia. Para
Mundial 2027 vale evaluar migración.

### Riesgo E — Voltajes MP1584 nunca medidos

Los 2 reguladores buck en la placa DOWN (U8, U9) tienen **trimpot
físico** para ajustar voltaje de salida (5V y 3.3V típicos). Nadie midió
con multímetro. Si el trimpot vino mal de fábrica o se movió en transporte,
podrían estar fuera de spec.

- **Plan de prueba:** 30 minutos con multímetro antes de power-on completo
  del robot.

### Riesgo F — Orientación física del PCB DOWN respecto al chasis sin validar

`SENSOR_POS[32]` asume:
- +Y del PCB = adelante del robot.
- +X del PCB = derecha del robot.

Validación parcial 2026-05-24 (TASK-027): solo el eje X confirmado por
sweep lateral. **Eje Y no fue validado.**

- **Riesgo:** Si el PCB se montó rotado 90° respecto a la asunción, todos
  los ángulos están off por 90°. El robot huiría "atrás" cuando debería
  huir "izquierda".
- **Plan de prueba:** TASK-027 paso 1 + paso 2. ~10 minutos.

---

## 5. Mejoras propuestas — formato coach (P0/P1/P2)

### Tema P0.1 — Validar UART real DOWN↔TOP/CENTRAL

**Categoría:** comunicaciones / integración
**Robot afectado:** ambos
**Prioridad:** **P0 (BLOQUEANTE INCHEON)**

**Qué observo.** El firmware `comm_central.cpp` y `comm_top.cpp` está listo,
con CRC-16 y framing por proto.h. Pero **nunca un byte salió por Serial1 o
Serial5 a otra placa real**. TASK-031 sigue marcada como postponed.

**Risk-no-fix.** Probabilidad ALTA de que el primer match en Incheon
descubra un problema de cable, baud rate o timing → robot juega sin línea
→ sale del borde → expulsión.

**Risk-fix.** Requiere que TOP o CENTRAL esté flasheada con firmware
compatible. Si flasheamos TOP con `top` env y CENTRAL con `central_robot1/2`,
deberíamos poder pinchar cables UART. Riesgo de descubrir incompatibilidades
nuevas en el protocolo.

**Tiempo estimado.** 3 horas.

**Plan de prueba en hardware real.**
1. Flashear DOWN con `pio run -e down -t upload` (firmware competencia, no diag).
2. Flashear al menos UNA de TOP o CENTRAL con su firmware respectivo.
3. Conectar cable UART entre conector U10 de DOWN y RX/TX correspondiente de TOP
   (o U11 ↔ CENTRAL Serial2).
4. Capturar 60 seg de tramas con un logic analyzer / osciloscopio
   conectado en el cable. Confirmar:
   - Tramas LineStatusV2 llegan a 200 Hz (período 5 ms).
   - CRC válido en todas (o casi todas — tolerancia <1%).
   - Latencia desde "cambio de blanco en DOWN" hasta "byte recibido en CENTRAL"
     < 15 ms.
5. Tests de regresión: el muestreo 1 kHz de DOWN no se degrada por TX UART.

### Tema P0.2 — Integrar `calib_storage` en `main_down.cpp`

**Categoría:** firmware / operación de torneo
**Robot afectado:** ambos
**Prioridad:** **P0 (BLOQUEANTE OPERACIÓN MULTI-MATCH)**

**Qué observo.** `calib_storage.cpp` + `eeprom_calib.cpp` están implementados
y testeados (19 tests host-native), pero `main_down.cpp` NO los invoca.
Cada power cycle = calibración por defecto = `EV_CALIB_SUSPECT` permanente
hasta recalibrar.

**Risk-no-fix.** En torneo con N matches, hay N power cycles. Cada match
arranca con 30 segundos de "robot inválido" + necesidad de recalibrar
desde TOP. Olvido humano = match arranca con datos malos.

**Risk-fix.** Bajo. Cambios localizados en setup y en handler de comando
"calibrar". Riesgo: si EEPROM se corrompe (firmware bug o ESD), la
calibración cargada puede ser basura — pero `cs_deserialize` valida CRC,
así que rechaza buffer corrupto y vuelve a default.

**Tiempo estimado.** 1 hora (impl + test power-cycle en banco).

**Plan de prueba en hardware real.**
1. En `main_down.cpp::setup()`, después de `line_ring_init`, agregar:
   ```cpp
   if (ec_load_calibration(g_model.calib, NUM_LINE_SENSORS)) {
       Serial.println("[DOWN] calibracion cargada de EEPROM");
   } else {
       Serial.println("[DOWN] EEPROM vacia/invalida - calibrar manual");
       line_ring_calibrate_carpet();  // fallback al comportamiento actual
   }
   ```
2. En el handler RX de `comm_central` para comando "save calib", llamar
   `ec_save_calibration(g_model.calib, NUM_LINE_SENSORS)`.
3. Test power-cycle: calibrar manual; verificar `data_valid=1`;
   desconectar batería + USB 10 seg; reconectar; confirmar que arranca con
   `data_valid=1` (calibración sobrevivió).

### Tema P0.3 — Medir voltajes MP1584 con multímetro

**Categoría:** electrónica / hardware
**Robot afectado:** ambos
**Prioridad:** P0 (rápido y barato)

**Qué observo.** Los 2 reguladores buck U8/U9 tienen trimpot. Nadie midió.
Si están fuera de ±5% del nominal (5.00 V y 3.30 V), riesgo de
inestabilidad ADC o brown-out.

**Risk-no-fix.** Posible inestabilidad latente que se manifiesta en
condiciones de calor/carga.

**Risk-fix.** Cero (solo medir). Si están mal, ajustar trimpot.

**Tiempo estimado.** 30 minutos.

**Plan de prueba en hardware real.**
1. Robot apagado, multímetro en modo voltaje DC.
2. Punta + en pad de salida de U8 (5V), punta − en GND. Leer.
3. Idem para U9 (3V3).
4. Si fuera de ±5%, ajustar trimpot con destornillador (revisar datasheet
   MP1584 para sentido de giro).

### Tema P0.4 — Validar que CENTRAL respeta `data_valid` antes de usar `line_angle`

**Categoría:** firmware / integración
**Robot afectado:** ambos
**Prioridad:** **P0 (CRÍTICO)**

**Qué observo.** Code review parcial de `strategy.cpp`: lee `line_angle`
sin chequear primero `LineStatusV2.data_valid`. Si DOWN señala
`data_valid=0` (porque hay mux muerto o robot levantado), CENTRAL puede
estar usando un ángulo basura.

**Risk-no-fix.** **Degradación silenciosa** en presencia de fallos
parciales. El robot se ve "funcional" pero está tomando decisiones con
datos corruptos.

**Risk-fix.** Bajo. Agregar guard `if (!ls.data_valid) return;` antes de
los puntos de uso.

**Tiempo estimado.** 2 horas (review + impl + tests strategy_transitions).

**Plan de prueba en hardware real.**
1. Code review exhaustivo de strategy.cpp buscando todos los accesos a
   `world_model_get_line_angle_deg()`.
2. Agregar guard `data_valid` antes de cada uso.
3. Simular `data_valid=0` durante 1 seg (forzar `EV_LIFTED` levantando robot)
   y confirmar que strategy NO usa el ángulo durante ese período.

### Tema P1.5 — All-white rejection (sub-tema research doc)

**Categoría:** firmware / robustez
**Prioridad:** P1
**Tiempo:** 30 min

Implementar `lf_all_white_rejection()` en line_filters: si todos los
sensores marcan blanco simultáneamente, forzar todos a no-blanco. Patrón
Omicron. Mitigación para Modo 5 (luz extrema).

### Tema P1.6 — `availableForWrite()` check en `comm_central_send`

**Categoría:** firmware / robustez UART
**Prioridad:** P1
**Tiempo:** 30 min

Antes de TX, chequear `Serial1.availableForWrite() >= frame_size`. Si no,
skipear ese frame (mejor perder 1 muestra que demorar el loop principal).

### Tema P1.7 — Checklist pre-match

**Categoría:** operación de torneo
**Prioridad:** P1
**Tiempo:** 1 hora (documentar + practicar)

Documentar en `docs/operacion/checklist-pre-match.md`:
- Lente OTOS limpio (visual).
- Batería ≥ 7.4 V (multímetro).
- Calibración cargada (verificar serial print al boot).
- Sensores de línea sanos (correr `diag_capture.py` 5 seg).

### Tema P2.x — Diferidos a 2027

- Migrar ALS-PT19 a LED IR + fotodiodo discreto (rediseño PCB).
- Implementar `cross_track_mm` real con geometría.
- Modos tácticos `FOLLOW_PARALLEL_*` (research doc Tema 6).
- Shock detection con IMU del TOP (research doc Tema 5).
- Multi-cluster semantic line detection (área chica, lateral vs fondo).
- Test E2E hardware-in-the-loop.

---

## 6. Roadmap recomendado para Incheon (T-25 días)

### Semana 1 (T-25 a T-19): cerrar bloqueantes

- **P0.1** (3h) — validar UART real.
- **P0.2** (1h) — integrar calib_storage.
- **P0.3** (0.5h) — medir voltajes MP1584.
- **P0.4** (2h) — guard `data_valid` en strategy.cpp.

**Total: 6.5 horas. Riesgo residual baja de MEDIO-ALTO a BAJO-MEDIO.**

### Semana 2 (T-18 a T-12): robustez

- **P1.5** (0.5h) — all-white rejection.
- **P1.6** (0.5h) — UART backpressure check.
- **P1.7** (1h) — checklist pre-match.
- Plan de prueba hardware completo de TASK-001 (10 nets PCB), TASK-027
  (orientación PCB), TASK-029 (validación cuantitativa OTOS).

### Semana 3 (T-11 a T-5): testing E2E

- Simulacro de torneo: 4 matches consecutivos con power cycle entre, sobre
  cancha real (o aproximación).
- Documentar bugs en journal, ajustar.

### Semana 4 (T-4 a T-0): pulido + viaje

- Solo bugs blocker.
- No más features.

---

## 7. Lo que NO se pudo auditar bien (honestidad sobre límites)

1. **Comportamiento eléctrico real durante patada del solenoide.** Sin
   osciloscopio + carga del kicker, los cálculos son teóricos. Se asume
   que los capacitores de desacople son suficientes — *no verificado*.

2. **OTOS sobre alfombra RCJ verde real.** Sólo testamos sobre cartón
   corrugado. Alfombra verde RCJ tiene textura/reflectancia distinta;
   error real puede ser distinto al 6.5% medido.

3. **Latencia real DOWN→CENTRAL.** Sin logic analyzer o osciloscopio
   en el cable UART, la latencia <15 ms es teórica.

4. **Comportamiento bajo vibración prolongada** (15 min de juego). Sin
   mesa vibrante, imposible saber si soldaduras se aflojan, sensores se
   desencajan, OTOS pierde tracking por aceleración.

5. **Interacciones entre los nuevos detectores (Mux + SensorHealth +
   Lifted + Suspect) en escenarios combinados.** Los tests son unit,
   no combinatorios. Por ejemplo: ¿qué pasa si MuxWatchdog detecta dead
   AL MISMO TIEMPO que LiftedDetector se activa? Combinaciones no
   exhaustivamente testadas.

6. **Comportamiento del firmware bajo brown-out parcial** (Vcc bajo 4.5 V).
   Teensy puede ejecutar código con valores ADC corruptos antes de
   resetear. No simulado.

---

## 8. Veredicto del auditor (objetivo)

El subsistema DOWN está **muy bien diseñado** desde lo arquitectónico:
- Pipeline modular, separación host-testeable / Arduino glue.
- 246 tests unitarios pasan.
- Detección multimodal de fallos (Mux + Sensor + Lifted + Calib).
- Geometría real validada.
- Audit independiente realizado y respondido.

Pero la **integración con el resto del robot es incompleta**:
- UART nunca probada en hardware real.
- EEPROM persistence dormida.
- CENTRAL no consume las señales nuevas.

Esto es **deuda de integración**, no falla de diseño. La diferencia importa:
- Diseño = horas-hombre en research + implementación.
- Integración = horas en hardware con cables.

**El equipo tiene 25 días. Los 4 temas P0 caben en 6.5 horas.** Si se
priorizan ahora, el robot llega a Incheon con un fail-safe DOWN robusto
y un sistema de calibración operativo. Si no, el riesgo de
"funcionalidad parcial silenciosamente degradada" en cancha real es alto.

**Recomendación final:** ejecutar los 4 temas P0 esta semana antes de
cualquier otro desarrollo. El research y los temas P1/P2 pueden seguir
en paralelo, pero los P0 no se postergan.

---

## 9. Referencias

- Research doc anterior: `research/in-progress/2026-05-24-deteccion-linea-down-investigacion-y-diseno.md`
- Journal cierre DOWN: `journal/2026-05-24-down-board-passing-tests-cierre.md`
- Pack pinout: `hardware/electronics/down-board-pack/01-pinout-y-posiciones.md`
- Pack funcionalidad: `hardware/electronics/down-board-pack/02-funcionalidad.md`
- TASKs: 001 (10 nets), 027 (orientación PCB), 029 (OTOS cuantitativo),
  030 (lámina OTOS — cerrada), 031 (UART real — pendiente).
- Commits del 2026-05-29 (P0+P1 del research):
  - `c0d8061` MuxWatchdog (P0)
  - `8a3e39b` sensor_health (P1)
  - `6d00fe8` EEPROM persistence (P1, código listo, no integrado)
  - `954a6a8` migración lg_compute_xy (P1)
  - `703871b` fix audit (wrap-safety + data_valid + OOB)

## 10. Atribución

- Pedido + dirección de scope: Gustavo Viollaz (@gviollaz), coach IITA.
- Audit, gap analysis, FMEA, redacción: Claude Opus 4.7 (Anthropic),
  sesión 2026-05-29 (asistido por audit independiente con sub-agente
  Explore para revisión cruzada).
- Material base: equipo IITA Salta + sesiones Claude anteriores (mayo 2026).

> **Status: in-progress.** Este audit es input para decisión del coach.
> Las TASKs derivadas de los temas P0/P1/P2 deben crearse después de
> feedback de Gustavo + equipo. Regla 6 CLAUDE.md sigue vigente: nada
> de producción se cambia sin aprobación explícita.
