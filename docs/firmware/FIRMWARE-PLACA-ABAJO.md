---
title: "Firmware Placa ABAJO — Especificación funcional completa"
date: 2026-05-11
status: especificación
audience: equipo IITA Soccer Open
tags: [firmware, placa-abajo, down-board, sensor-piso, otos, line-ring, especificacion]
---

# Firmware Placa ABAJO — Especificación funcional completa

> **⚠️ PARCIALMENTE SUPERADO (2026-05-19, ampliado 2026-05-24).** Este doc
> describe el algoritmo `line_ring` (mayo 2026-05-11) que sigue VIVO en
> `src/down/main_down.cpp` para la lectura cruda a 1 kHz. **PERO**:
>
> 1. Desde 2026-05-18 existe en paralelo otra cadena (`down_model +
>    line_geometry + line_tracker + line_calib + surface_monitor +
>    down_encode`) que la usa `src/down/comm_central.cpp` para armar
>    `LineStatusV2`. Para el contrato real de datos al CENTRAL ver
>    **`docs/firmware/CONTRATO-DATOS-DOWN.md`** y el índice
>    **`docs/FUENTES-DE-VERDAD.md`**.
>
> 2. **🚨 Las menciones de pinout de muxes en este doc fueron ACTUALIZADAS
>    el 2026-05-24** con los valores reales validados empíricamente. La
>    arquitectura correcta es **A/B/C propios por mux (12 pines SEL en
>    total)**, NO compartidos. INH atados a GND fijo en el PCB. Fuente
>    canónica de pinout: `hardware/electronics/down-board-pack/01-pinout-y-posiciones.md`.
>    Validación: `journal/2026-05-24-hardware-up-down-anillo-linea.md`.

> Documento de referencia del firmware que corre en la placa base (Teensy 4.0) del
> robot 2026. Define qué hace el programa, cómo lo hace, qué envía, cada cuánto,
> cómo detecta errores y cómo se comporta ante fallas.

---

## Tabla de contenidos

1. [Resumen](#1-resumen)
2. [Hardware sobre el que corre](#2-hardware-sobre-el-que-corre)
3. [Responsabilidades funcionales](#3-responsabilidades-funcionales)
4. [Modos de operación](#4-modos-de-operación)
5. [Procesamiento del anillo de 32 sensores de luz](#5-procesamiento-del-anillo-de-32-sensores-de-luz)
6. [Procesamiento de odometría (OTOS dual)](#6-procesamiento-de-odometría-otos-dual)
7. [Comunicaciones (2 streams + 2 recepciones)](#7-comunicaciones)
8. [Detección de fallos, heartbeat y recovery](#8-detección-de-fallos-heartbeat-y-recovery)
9. [Timing y latencias](#9-timing-y-latencias)
10. [Estructuras de datos enviadas](#10-estructuras-de-datos-enviadas)
11. [Diagnóstico y debug](#11-diagnóstico-y-debug)
12. [Tabla resumen](#12-tabla-resumen)
13. [Lo que NO hace (límite de scope)](#13-lo-que-no-hace-límite-de-scope)
14. [Referencias](#14-referencias)

---

## 1. Resumen

La placa ABAJO es un **sensor inteligente puro**. No toma decisiones tácticas, no controla motores, no corre lazos de control (PIDs). Solo lee sensores del suelo (32 fotodiodos de línea + 2 sensores de odometría óptica) y los reporta procesados a las otras dos placas.

**Doble stream de salida:**
- **Hacia CENTRAL** (Serial1, bus de emergencia): `LINE_URGENT` cada 5 ms (200 Hz). Contiene ángulo de línea, profundidad y flag de salida inminente. Es el camino más corto para que CENTRAL frene si el robot está por salirse de la cancha.
- **Hacia ARRIBA** (Serial5): `DOWN_OTOS_POSE` y `DOWN_OTOS_VEL` cada 10 ms (100 Hz). Contiene pose odométrica del robot y velocidades. ARRIBA usa esto para su fusión sensorial completa.

**Doble stream de entrada:**
- Comandos administrativos desde CENTRAL (Serial1): reset OTOS, recalibrar umbrales de línea.
- Comandos administrativos desde ARRIBA (Serial5): legacy, baja prioridad.

La placa ABAJO no necesita conocer el rol del robot (arquero/delantero) ni el estado del partido (running/stop). Siempre reporta lo mismo. CENTRAL decide qué hacer con el dato según el contexto.

---

## 2. Hardware sobre el que corre

| Componente | Cantidad | Conexión | Nota |
|-----------|----------|----------|------|
| MCU Teensy 4.0 | 1 | — | Cortex-M7 a 600 MHz, 1 MB RAM, 2 MB flash |
| Sensores ALS-PT19 (fotodiodo + LED activo) | 32 | Vía 4 muxes CD4051 | Reflectivo activo, no pasivo |
| Multiplexores CD4051BM (8:1 analógico) | 4 | A/B/C propios por mux (12 pines SEL totales), INH a GND físico, salidas O1..O4 | Settle time ~3 µs típico |
| Resistencias de pull-up de sensor | 32 × 10 kΩ | Por sensor | |
| Resistencias limitadoras de LED | 32 × 330 Ω | Por sensor | |
| SparkFun OTOS | 2 | I2C dual (Wire bus 0 + Wire1 bus 1) | Montados a izquierda y derecha del centro |
| Conector UART hacia CENTRAL | 1 | Serial1 (pines 0/1), conector U11 | Verificado en PCB JSON 04-12 |
| Conector UART hacia ARRIBA | 1 | Serial5 (pines 21/20), conector U10 | Verificado en PCB JSON 04-12 |
| Conector Dean-T-F batería | 1 | 7.4 V LiPo | Comparte con CENTRAL y ARRIBA |
| Reguladores MP1584-EN | 2 | 7.4V → 5V y 7.4V → 3.3V | |
| LED de estado | 1 | LED_BUILTIN (pin 13) | Para indicar estado al humano |

**Pinout en `src/down/config_down.h`** (extraído del schematic 04-12 y validado empíricamente 2026-05-24):

- `PIN_MUX_A[4] = { 13, 4, 7, 10 }` — Selector A de U1, U2, U3, U4 (un pin por mux, NO compartido).
- `PIN_MUX_B[4] = {  2, 5, 8, 11 }` — Selector B de U1, U2, U3, U4.
- `PIN_MUX_C[4] = {  3, 6, 9, 12 }` — Selector C de U1, U2, U3, U4.
- `PIN_MUX_OUT[4] = { A0, A1, A8, A9 }` — Entradas analógicas COM de cada mux. **Atención**: A8/A9 son los pines del Teensy 4.0 que llegan a U3/U4; NO usar A2/A3 (esos van al I²C2 del OTOS U6 y producen ADC=1023 sólido).
- `MUX_CH_FOR_SENSOR[8] = { 3, 0, 1, 2, 5, 7, 6, 4 }` — Mapeo canal del mux → sensor lógico (scrambling del PCB).
- Los pines INH (Enable) de los 4 muxes están atados a GND físico en el PCB — el firmware NO los controla. No existe `PIN_MUX_INH[]`.
- I²C OTOS: `Wire` (SDA=18, SCL=19) → U5; `Wire1` (SDA=17, SCL=16) → U6. Ambos a dirección 0x17 default SparkFun. NO se usa Wire2.

---

## 3. Responsabilidades funcionales

| # | Responsabilidad | Detalle |
|---|-----------------|---------|
| R1 | Muestrear los 32 sensores de luz a 1 kHz | 4 muxes en paralelo, barrido 8 canales |
| R2 | Calcular ángulo de la línea detectada | Centroide angular ponderado |
| R3 | Calcular profundidad (depth) de la línea | Cuántos sensores adyacentes ven blanco |
| R4 | Disparar flag `imminent_exit` | Cuando ≥ N sensores ven blanco simultáneamente |
| R5 | Calibrar umbrales de línea | Por sensor: blanco (línea) y carpet (verde) |
| R6 | Detectar "robot levantado" | Si todos los sensores reportan baja luz uniforme |
| R7 | Leer los 2 OTOS a 100 Hz | I2C dual (Wire + Wire1) |
| R8 | Fusionar OTOS en pose central | Promedio + heading inferido del diferencial |
| R9 | Calcular slip estimate | Diferencia anómala entre OTOS izq y der |
| R10 | Enviar `LINE_URGENT` a CENTRAL a 200 Hz | Bus de emergencia, latencia < 15 ms |
| R11 | Enviar `DOWN_OTOS_POSE/VEL` a ARRIBA a 100 Hz | Para fusión sensorial completa |
| R12 | Recibir comandos administrativos | Reset OTOS, calibrar línea, futuro: setpoint del arquero |
| R13 | Watchdog y recovery | Detectar UART caída, OTOS desconectado, mux malo |
| R14 | LED de estado | Comunicar al humano el estado del firmware |

---

## 4. Modos de operación

El firmware tiene **un único modo de trabajo táctico** (siempre reporta lo mismo). Pero tiene **modos administrativos** que pueden activarse por comando de CENTRAL:

| Modo | Cuándo se activa | Comportamiento |
|------|------------------|----------------|
| `NORMAL` (default) | Al boot, después de cualquier modo admin | Lectura continua + envío de streams a 200/100 Hz |
| `CALIBRATING_CARPET` | Comando `CENTRAL_CALIB_LINE` con flag=0 | Toma 32 muestras × 10 ms por sensor, calcula promedio carpet, actualiza umbrales. Bloquea streams ~320 ms |
| `CALIBRATING_WHITE` | Comando `CENTRAL_CALIB_LINE` con flag=1 | Idéntico pero sobre línea blanca. Bloquea streams ~320 ms |
| `OTOS_RESET` | Comando `CENTRAL_RESET_OTOS` | Pone (x, y, heading) en 0 en ambos OTOS. Instantáneo |
| `LIFTED` (auto) | Detectado por R6 | Sigue enviando streams pero con flag `lifted=1` en `LineStatus.flags`. CENTRAL ignora datos de línea cuando ve este flag |

**No hay modo "arquero" vs "delantero" en ABAJO.** La placa ABAJO no necesita saber el rol del robot. CENTRAL recibe `LINE_URGENT` siempre y decide qué hacer con el dato según `strategy_get_role()`.

**Transiciones automáticas:**
- `CALIBRATING_*` vuelve a `NORMAL` automáticamente al terminar la captura de muestras.
- `LIFTED` se desactiva automáticamente cuando los sensores reportan valores normales en > 1 segundo (anti-glitch).

---

## 5. Procesamiento del anillo de 32 sensores de luz

### 5.1 Multiplexación y barrido

Los 32 sensores están conectados a 4 muxes CD4051 de 8 canales. **Cada mux tiene sus 3 propias líneas de selección A/B/C** (12 pines digitales en total, NO compartidos — descubrimiento del schematic 2026-05-19 validado empíricamente 2026-05-24). Por cada iteración del barrido, el firmware setea las 12 líneas simultáneamente al mismo canal i (aprovechando el patrón de scrambling idéntico de los 4 muxes), espera el settle del CD4051, y lee las 4 salidas analógicas (O1, O2, O3, O4) **en paralelo** vía A0/A1/A8/A9 del Teensy 4.0.

**Algoritmo del barrido**:

```
para cada canal i = 0 a 7:
    digitalWrite(SEL_A, i & 0x01)
    digitalWrite(SEL_B, i & 0x02)
    digitalWrite(SEL_C, i & 0x04)
    delayMicroseconds(3)  # settle time del CD4051

    raw[0*8 + i] = analogRead(PIN_MUX_OUT[0])  # sensor 0..7
    raw[1*8 + i] = analogRead(PIN_MUX_OUT[1])  # sensor 8..15
    raw[2*8 + i] = analogRead(PIN_MUX_OUT[2])  # sensor 16..23
    raw[3*8 + i] = analogRead(PIN_MUX_OUT[3])  # sensor 24..31
```

**Timing estimado**:
- Settle time mux: 3 µs.
- `analogRead` Teensy 4.0 a 10 bits: ~2 µs por lectura.
- Por canal: 3 + 4 × 2 = 11 µs.
- Por barrido completo (8 canales): ~88 µs.
- **Frecuencia máxima de lectura del anillo: > 10 kHz** (margen de 10x sobre el objetivo de 1 kHz).

Esto permite **promediar lecturas o aplicar filtros temporales sin perder responsividad**.

### 5.2 Calibración

Cada sensor tiene umbrales propios porque los componentes ALS-PT19 tienen variabilidad de fabricación + diferencias de iluminación local.

**Almacenado por sensor**:
- `carpet_avg[i]` — valor promedio sobre carpet verde.
- `white_avg[i]` — valor promedio sobre línea blanca.
- `threshold[i] = (carpet_avg[i] + white_avg[i]) / 2` — umbral binario.

**Procedimiento de calibración (real, 2026-06)**:

> ⚠️ El gatillo REAL de calibración en cancha es la herramienta de banco
> `diag_down_calibracion` por USB (comandos `c`/`b`/`s`), NO un comando UART desde
> CENTRAL. El handler `CENTRAL_CALIB_LINE` existe en el firmware de DOWN, pero
> **ninguna placa lo emite hoy** (`comm_down_send_calib_line()` no tiene caller en
> `src/central` ni `src/top`). El flujo descrito abajo (CENTRAL ordena) es el
> diseño original que el código NO implementa — se conserva sólo como referencia.

1. Operario carga `pio run -e diag_down_calibracion -t upload` y abre el monitor.
2. Robot sobre carpet verde → comando `c`: lee los 32 sensores y promedia `carpet_avg[i]`.
3. Operario mueve el robot a la línea blanca → comando `b`: ídem `white_avg[i]`.
4. Comando `m` para revisar sensores sospechosos; comando `s` para GUARDAR en EEPROM.
5. Recalcula `threshold[i] = (carpet_avg[i] + white_avg[i]) / 2`.

**Persistencia**: ✅ la calibración se guarda en la EEPROM emulada del Teensy
(P0.2, integrado 2026-05-29) y el firmware de competencia la **carga al boot**
(`comm_central_load_persisted_calib()` en `main_down.cpp`), quedando `data_valid=1`
sin recalibrar. Hay que correr la calibración de campo UNA vez por cancha/iluminación.

**Detección de calibración inválida**: si `white_avg[i] - carpet_avg[i] < 100` (separación insuficiente), marcar sensor i como "no confiable" y excluirlo del cálculo de ángulo.

### 5.3 Detección de línea — algoritmo

Cada sensor `i` está físicamente en un ángulo `θ_i = i × 360° / 32 = i × 11.25°` del centro del robot (asumiendo distribución uniforme).

**Algoritmo del ángulo (centroide angular ponderado)**:

```cpp
float sum_x = 0.0f, sum_y = 0.0f;
uint8_t depth = 0;

for (int i = 0; i < 32; ++i) {
    if (raw[i] >= threshold[i]) {
        depth++;
        float theta = i * (2 * M_PI / 32);  // radianes
        // Ponderación por intensidad relativa (cuán "blanco" es)
        float weight = (raw[i] - threshold[i]) / float(white_avg[i] - threshold[i]);
        weight = clamp(weight, 0.0f, 1.0f);
        sum_x += weight * cos(theta);
        sum_y += weight * sin(theta);
    }
}

float angle_rad = atan2(sum_y, sum_x);
float angle_deg = angle_rad * (180.0f / M_PI);  // -180 a +180, 0 = frente
```

**Profundidad (`depth`)**: cantidad de sensores que ven blanco simultáneamente. Es un proxy de qué tan adentro de la línea está el robot:
- `depth = 0` → fuera de línea.
- `depth = 1-2` → tocando borde.
- `depth = 3-5` → pisando línea (típico).
- `depth >= 6` → muy adentro, saliéndose.

**Flag `imminent_exit`**: se dispara cuando `depth >= IMMINENT_EXIT_DEPTH` (configurable, default = 6). Es la señal urgente para que CENTRAL frene.

### 5.4 Filtrado y redundancia

El firmware aplica tres niveles de filtrado para reducir falsos positivos y ruido:

**Nivel 1: filtro temporal (promedio móvil)**:
- Mantiene `raw_buf[i][4]` — últimas 4 lecturas por sensor.
- Cada tick, `raw_filtered[i] = mean(raw_buf[i])`.
- Costo: 32 × 4 = 128 bytes de RAM. Trivial.
- Reduce ruido de alta frecuencia (vibración, EMI de motores).

**Nivel 2: hysteresis del umbral**:
- En vez de `if (raw >= threshold)`, usa dos umbrales:
  - `threshold_high[i] = threshold[i] + 20` (para pasar de "carpet" a "blanco").
  - `threshold_low[i] = threshold[i] - 20` (para pasar de "blanco" a "carpet").
- Evita flickeo cuando el sensor está cerca del umbral.

**Nivel 3: filtro espacial (consistencia entre vecinos)**:
- Si solo un sensor aislado dice "blanco" y sus dos vecinos dicen "carpet", lo más probable es ruido.
- Regla: para que un sensor cuente en el cálculo de ángulo, al menos uno de sus vecinos directos también debe estar sobre el threshold.
- Sensores 0 y 31 son vecinos (anillo cerrado).

**Resultado combinado**: el algoritmo final del ángulo solo considera sensores que pasan los 3 filtros.

### 5.5 Detección de "robot levantado" (falsas lecturas)

Cuando el árbitro levanta el robot del piso (al final de un partido, para reposicionar, o accidentalmente), los sensores ven aire en vez de carpet. Las lecturas en aire son distintas a carpet:

- **En carpet verde**: lectura típica 200-400 counts (10-bit ADC).
- **En aire**: lectura uniformemente baja, ~50-150 counts (depende de luz ambiente).
- **En línea blanca**: lectura típica 600-900 counts.

**Algoritmo de detección de "levantado"**:

```cpp
bool is_lifted() {
    // Criterio 1: todos los sensores reportan menos que el umbral mínimo de carpet
    int low_count = 0;
    for (int i = 0; i < 32; ++i) {
        if (raw_filtered[i] < (carpet_avg[i] - 50)) {
            low_count++;
        }
    }
    if (low_count >= 28) return true;  // 28/32 = 87%

    // Criterio 2: surface quality de los OTOS baja
    if (otos_left.surface_quality < 30 && otos_right.surface_quality < 30) return true;

    return false;
}
```

**Comportamiento cuando se detecta `LIFTED`**:
- Flag `lifted = 1` en cada `LINE_URGENT` enviado a CENTRAL.
- CENTRAL ignora medidas de línea cuando ve este flag (no decide en base a datos basura).
- ABAJO sigue muestreando — cuando el robot vuelva al piso, en < 1 segundo detecta lecturas normales y desactiva el flag.

**Anti-glitch**: el flag `lifted` solo se activa después de **≥ 100 ms continuos** de criterio cumplido (evita falsos positivos por bumps).

---

## 6. Procesamiento de odometría (OTOS dual)

### 6.1 Lectura I2C

Los 2 SparkFun OTOS (U5 y U6 en el schematic) están en buses I2C separados:
- OTOS izquierdo → `Wire` (I2C bus 0, pines 18/19 del Teensy 4.0).
- OTOS derecho → `Wire1` (I2C bus 1, pines remapeados 24/25).

Esto es necesario porque ambos OTOS comparten dirección I2C por default (0x17) y no pueden coexistir en el mismo bus.

**Frecuencia máxima de lectura**: el SparkFun OTOS soporta hasta ~50 Hz de actualización interna. Leerlo a 100 Hz desde el Teensy es seguro (devuelve el último valor disponible cuando se le pide).

**Latencia I2C**: a 400 kHz (Fast Mode), una transacción típica de OTOS (pose + velocity, ~24 bytes) toma ~700 µs. Dos OTOS leídos en paralelo en buses distintos: también ~700 µs (no se serializan). Costo aceptable.

### 6.2 Fusión central + análisis diferencial

Los 2 OTOS están montados **a los costados del robot** (verificación pendiente con Elías — TASK-004), aprox a 100 mm del centro a cada lado.

**Datos por OTOS**:
- `pose.x`, `pose.y` (mm, desde último reset).
- `pose.heading` (grados, desde último reset).
- `vel.vx`, `vel.vy` (mm/s).
- `vel.omega` (rad/s).
- `surface_quality` (0-255, calidad de imagen óptica).

**Fusión del centro del robot**:

```cpp
// Pose central = promedio simple de ambos OTOS
center.x = (left.pose.x + right.pose.x) / 2.0f;
center.y = (left.pose.y + right.pose.y) / 2.0f;

// Heading robot: 2 formas, se elige según confiabilidad
//   (a) Promedio del heading reportado por cada OTOS
float heading_avg = (left.pose.heading + right.pose.heading) / 2.0f;

//   (b) Inferido por diferencial Y entre los 2 OTOS (más estable en rotación rápida)
float heading_diff = atan2(right.pose.y - left.pose.y, OTOS_SEPARATION_MM)
                     * (180.0f / M_PI);

// Si la diferencia entre ambos es coherente (< 5°), promediarlos.
// Si difieren mucho, usar el que tenga mejor surface_quality.
if (abs(heading_avg - heading_diff) < 5.0f) {
    center.heading = heading_avg;
} else {
    center.heading = (left.surface_quality > right.surface_quality)
                     ? left.pose.heading : right.pose.heading;
}
```

### 6.3 Slip estimate

El análisis diferencial detecta cuando una rueda patina (típicamente al patear, al chocar, o sobre cancha sucia):

```cpp
// Diferencia esperada entre OTOS por rotación pura
float expected_diff = omega_rad_s * OTOS_SEPARATION_MM;

// Diferencia observada
float observed_diff = right.vel.vx - left.vel.vx;

// Slip = exceso sobre lo esperado
float slip = abs(observed_diff - expected_diff);
```

Si `slip > 50 mm/s`, hay anomalía. Se reporta como `slip_estimate` (0-255 saturado) en `DOWN_OTOS_VEL`. CENTRAL puede usar este dato para:
- Confiar menos en la pose mientras dura el slip.
- Disparar PID más agresivo de corrección de heading.
- Loguear el evento para análisis post-partido.

### 6.4 Reset y referenciación

Los OTOS acumulan pose desde el último reset. Eventos típicos para resetear:

- **Al boot**: reset automático en `setup()` para que ambos OTOS arranquen en (0, 0, 0).
- **Al comando `CENTRAL_RESET_OTOS`**: cuando CENTRAL decide. Casos:
  - Al inicio de un nuevo partido (después de SETUP_GAME del árbitro).
  - Cuando ARRIBA reposiciona el robot por triangulación de cámaras y necesita resetear el offset.
  - Cuando se detecta drift acumulado fuerte.

**El reset es instantáneo** — la librería SparkFun de OTOS lo expone como una llamada I2C única.

---

## 7. Comunicaciones

### 7.1 Stream 1: `LINE_URGENT` → CENTRAL (200 Hz, Serial1)

**Frecuencia**: cada 5 ms (200 Hz). Esta tasa es la máxima razonable para línea — más alto es overkill, más bajo aumenta riesgo de no frenar a tiempo.

**Baud rate**: 230400. Un frame completo (16-17 bytes con overhead) tarda ~700 µs en transmitirse. A 200 Hz, el ciclo permite ~5000 µs entre frames, así que hay margen de 7x.

**Frame payload**: `LineStatus` (struct definido en `src/shared/types.h`).

```cpp
struct LineStatus {
    int16_t angle_centideg;       // ángulo línea (0 = frente, ±18000)
    uint8_t depth_mm;             // profundidad (cantidad de sensores en blanco)
    uint8_t imminent_exit_flag;   // 0/1 — ≥ N sensores en blanco
    uint8_t flags;                // bit 0 = lifted, bits 1-7 reservados
};  // 5 bytes payload + 7 bytes overhead = 12 bytes/frame
```

### 7.2 Stream 2: `DOWN_OTOS_POSE` + `DOWN_OTOS_VEL` → ARRIBA (100 Hz, Serial5)

**Frecuencia**: cada 10 ms (100 Hz) para ambos mensajes alternados o juntos.

**Frame payloads**:

```cpp
struct Pose2D {
    int16_t x_mm;
    int16_t y_mm;
    int16_t heading_centideg;
    uint8_t confidence;
};  // 7 bytes payload

struct Velocity2D {
    int16_t vx_mm_s;
    int16_t vy_mm_s;
    int16_t omega_centideg_s;
    uint8_t slip_estimate;
};  // 7 bytes payload
```

**Total por ciclo**: 2 frames × ~14 bytes = 28 bytes cada 10 ms = 2800 bytes/s. A 230400 baud = ~10% de utilización del UART. Holgado.

### 7.3 Recepción: comandos administrativos

**Desde CENTRAL (Serial1)**:

| Comando | Acción |
|---------|--------|
| `CENTRAL_RESET_OTOS` | Pone (x, y, heading) en 0 en ambos OTOS |
| `CENTRAL_CALIB_LINE` con flag=0 | Inicia calibración carpet (320 ms) |
| `CENTRAL_CALIB_LINE` con flag=1 | Inicia calibración blanca (320 ms) |

**Desde ARRIBA (Serial5)**: en la arquitectura actual no se esperan comandos relevantes desde ARRIBA. Se procesa el frame por completitud del protocolo pero se ignora.

### 7.4 Heartbeat / detección de fallo

**El envío continuo de los streams ES el heartbeat implícito.** No hace falta un mensaje específico de "estoy vivo" porque:
- CENTRAL espera `LINE_URGENT` cada 5 ms. Si no llega en 100 ms (20 frames perdidos), marca `comm_down_is_line_fresh() = false` y degrada estrategia.
- ARRIBA espera `DOWN_OTOS_POSE/VEL` cada 10 ms. Si no llega en 500 ms, marca pose como stale y degrada fusión sensorial.

**Mecanismos adicionales que aseguran calidad de comunicación:**

1. **Número de secuencia (SEQ)** en cada frame del protocolo. Receptor detecta packets perdidos comparando SEQ_actual con SEQ_esperado = SEQ_anterior + 1. Útil para diagnóstico, no para acción inmediata.
2. **CRC-16/CCITT** en cada frame. Receptor descarta frames con CRC inválido. Cuenta `crc_errors` para diagnóstico.
3. **START byte 0xAA y END byte 0x55** distintos para resincronización rápida ante bytes basura.

**Lo que pasa si el UART hacia CENTRAL se cae**:
- ABAJO sigue muestreando línea y OTOS normalmente — no le importa la caída.
- CENTRAL detecta el timeout en < 100 ms y entra en "modo ciego de línea" (no toma decisiones que dependan de línea).
- Cuando vuelve la comunicación, los frames frescos vuelven a `is_fresh() = true` automáticamente. Sin handshake necesario.

### 7.5 Recovery

**Si un mux no responde** (uno de los 4 CD4051 está roto o desconectado):
- Las lecturas analógicas en `PIN_MUX_OUT[X]` quedan flotantes → valores erráticos.
- Detección: si las lecturas de los 8 sensores asociados al mux X son uniformemente bajas O uniformemente altas durante > 100 ms, marcar `mux_X_dead = true`.
- Acción: excluir esos 8 sensores del cálculo de ángulo y profundidad. El anillo queda con resolución degradada pero sigue operativo.

**Si un OTOS no responde** (cable I2C suelto, sensor roto):
- Detección: lecturas I2C que retornan timeout o error.
- Acción: usar solo el OTOS funcional. `slip_estimate = 0` (no se puede calcular sin diferencial). Marcar `otos_quality_degraded = true` en flags.

**Si ambos OTOS fallan**:
- Pose pierde resolución completamente. ABAJO envía `Pose2D` con `confidence = 0`. ARRIBA detecta el flag y degrada su fusión a solo IMU + cámaras.

---

## 8. Detección de fallos, heartbeat y recovery

### 8.1 Resumen de mecanismos

| Mecanismo | Qué detecta | Tiempo de detección |
|-----------|-------------|---------------------|
| SEQ del protocolo | Packets perdidos (cuántos, no fatal) | Inmediato (1 frame) |
| CRC-16 | Corrupción del frame | Inmediato |
| Timeout en CENTRAL | UART hacia CENTRAL caído | 100 ms |
| Timeout en ARRIBA | UART hacia ARRIBA caído | 500 ms |
| `is_lifted()` | Robot levantado del piso | 100 ms |
| Detección mux muerto | Mux roto o desconectado | 100 ms |
| Detección OTOS muerto | Sensor roto, cable suelto | Inmediato (siguiente lectura) |

### 8.2 ¿Hace falta un heartbeat explícito?

**No**. Tres razones:

1. **El stream de datos ya es el heartbeat**. Si no llega data, el receptor sabe que hay problema. Un "ping" adicional sería redundante.
2. **Frecuencias altas hacen la detección rápida**. A 200 Hz, un timeout de 100 ms = 20 frames perdidos en fila. Es muy raro perder 20 seguidos sin razón. Si pasa, hay problema real.
3. **El protocolo de 12-14 bytes por frame es liviano**. No estamos saturando UART. Agregar pings sería gastar ancho de banda innecesario.

Decisión: **no implementar heartbeat explícito**. Mantener stream continuo.

### 8.3 ¿Qué hace ABAJO si CENTRAL desaparece?

**Nada especial**. ABAJO sigue muestreando y enviando streams. No hay motores que pueda apagar, no hay decisión que pueda tomar. Si el robot está en cancha y CENTRAL cayó, los motores ya no responden (CENTRAL es el master) — ABAJO solo sigue su rutina.

Cuando CENTRAL vuelve, el primer frame que reciba (en < 5 ms) ya tiene datos frescos. No hace falta handshake.

---

## 9. Timing y latencias

### 9.1 Loop principal del firmware

```
loop():
    comm_central_tick()       # drena UART desde CENTRAL (~10 µs)
    comm_top_tick()           # drena UART desde ARRIBA (~10 µs)

    if since_line_tick >= 1 ms:
        line_ring_tick()      # ~88 µs por barrido completo + filtros
        check_lifted()        # ~20 µs

    if since_otos_tick >= 10 ms:
        otos_tick()           # ~700 µs I2C dual + fusión

    if since_line_send >= 5 ms:
        comm_central_send_line_urgent()  # ~50 µs + 700 µs UART async

    if since_otos_send >= 10 ms:
        comm_top_send_status()  # ~100 µs + 1500 µs UART async
```

**Loop iteration típico**: < 100 µs (la mayoría es polling de UART vacío).
**Loop iteration en tick de OTOS**: ~800 µs (con I2C dual).
**Margen de cabeza**: el peor caso (todos los ticks coinciden) es ~1 ms. El loop corre fácil a > 10 kHz.

### 9.2 Latencia sensor → CENTRAL

Camino crítico para "el robot está saliendo de la cancha":

| Paso | Tiempo |
|------|--------|
| Sensor detecta blanco | 0 ms |
| Próximo tick de `line_ring_tick()` (peor caso) | < 1 ms |
| Próximo tick de `comm_central_send_line_urgent()` (peor caso) | < 5 ms |
| Encode frame + TX UART (16 bytes a 230400 baud) | ~700 µs |
| Decode en CENTRAL + watchdog tick + motors_stop() | < 5 ms |
| **Total** | **~10-12 ms** ✓ < 15 ms objetivo |

### 9.3 Latencia odometría → ARRIBA

No es crítico (la odometría se usa para fusión, no para reacción urgente):

| Paso | Tiempo |
|------|--------|
| OTOS internal update | hasta 20 ms |
| Próximo tick de `otos_tick()` | < 10 ms |
| Próximo tick de `comm_top_send_status()` | < 10 ms |
| Encode + TX UART | ~1500 µs |
| **Total** | **~25-40 ms** (aceptable para fusión sensorial) |

---

## 10. Estructuras de datos enviadas

### 10.1 `LineStatus` (LINE_URGENT, ABAJO → CENTRAL)

```cpp
struct LineStatus {
    int16_t angle_centideg;       // ángulo línea: 0 = frente, ±18000 = ±180°
    uint8_t depth_mm;             // cantidad de sensores en blanco (0-32)
    uint8_t imminent_exit_flag;   // 0/1
    uint8_t flags;                // bit 0 = lifted (robot en aire)
                                  // bit 1 = mux_X_dead (algún mux roto)
                                  // bit 2 = calibration_invalid
                                  // bits 3-7 = reservados
} __attribute__((packed));
```

5 bytes payload + 7 bytes overhead protocolo = 12 bytes/frame.

A 200 Hz: 12 × 200 = 2400 bytes/s. 1% del baud 230400. Holgadísimo.

### 10.2 `Pose2D` (DOWN_OTOS_POSE, ABAJO → ARRIBA)

```cpp
struct Pose2D {
    int16_t x_mm;                 // pose acumulada desde último reset
    int16_t y_mm;
    int16_t heading_centideg;
    uint8_t confidence;           // 0-100. 0 si ambos OTOS muertos.
} __attribute__((packed));
```

7 bytes payload.

### 10.3 `Velocity2D` (DOWN_OTOS_VEL, ABAJO → ARRIBA)

```cpp
struct Velocity2D {
    int16_t vx_mm_s;
    int16_t vy_mm_s;
    int16_t omega_centideg_s;
    uint8_t slip_estimate;        // 0-255, saturado
} __attribute__((packed));
```

7 bytes payload.

A 100 Hz ambos juntos: 28 bytes × 100 = 2800 bytes/s. 1.2% del baud. OK.

---

## 11. Diagnóstico y debug

### 11.1 LED de estado (pin 13)

| Patrón | Significado |
|--------|-------------|
| Apagado | Firmware no inició o en `setup()` |
| Encendido fijo | NORMAL, todo OK |
| Parpadeo lento (1 Hz) | LIFTED detectado |
| Parpadeo rápido (5 Hz) | Algún mux o OTOS no responde |
| 3 parpadeos + pausa | Calibrando |
| Apagado tras estar prendido | UART hacia CENTRAL cayó hace > 1 s |

### 11.2 USB Serial (debug)

A 115200 baud por el puerto USB del Teensy 4.0, el firmware imprime al humano:

- En `setup()`: estado de cada subsistema (sensores OK, OTOS encontrados, UARTs abiertos).
- Cada 1 segundo en NORMAL: contadores (frames TX, frames RX, errores CRC, sensores ON, OTOS quality).
- En cada cambio de modo: la transición.
- Si se detecta fallo (mux muerto, OTOS muerto, robot levantado): print del evento.

Esto permite que Virginia o Elías conecten un cable USB durante development y vean qué está pasando sin necesidad de osciloscopio.

### 11.3 Comandos USB de debug (si se implementa)

Vía USB Serial el operario puede mandar texto para probar:
- `cal_carpet` — fuerza calibración carpet.
- `cal_white` — fuerza calibración blanca.
- `reset_otos` — reset.
- `dump_raw` — imprime las 32 lecturas crudas.
- `dump_pose` — imprime pose de cada OTOS.
- `stats` — imprime contadores.

Útil para troubleshooting sin tener que pasar por CENTRAL.

---

## 12. Tabla resumen

| Aspecto | Valor |
|---------|-------|
| MCU | Teensy 4.0 a 600 MHz |
| Sensores de línea | 32 ALS-PT19 con LED activo (reflectivo activo) |
| Multiplexación | 4 × CD4051BM (3 bits selección propios por mux = 12 pines SEL, 4 salidas analógicas a A0/A1/A8/A9, INH a GND) |
| Frecuencia muestreo línea | 1 kHz |
| Algoritmo de ángulo | Centroide ponderado por intensidad |
| Filtros aplicados | Temporal (mov avg 4), hysteresis, espacial (vecinos) |
| Sensores odometría | 2 × SparkFun OTOS en I2C dual |
| Frecuencia muestreo OTOS | 100 Hz |
| Fusión OTOS | Promedio central + heading diferencial + slip estimate |
| UART hacia CENTRAL | Serial1, 230400 baud, 200 Hz envío |
| UART hacia ARRIBA | Serial5, 230400 baud, 100 Hz envío |
| Latencia sensor → motor frenado | **~10-12 ms** (< 15 ms objetivo) |
| Heartbeat explícito | No (stream continuo cumple esa función) |
| Modos | 1 táctico (NORMAL) + 4 administrativos (calib, reset, lifted) |
| Carga CPU estimada | ~22% (mucho margen) |
| Detección robot levantado | Por umbral uniforme bajo + OTOS surface quality |
| Recovery | Auto: mux/OTOS muerto → degradación elegante, sigue operativo |

---

## 13. Lo que NO hace (límite de scope)

Lo siguiente NO es responsabilidad del firmware de la placa ABAJO. Si el equipo lo necesita, vive en otra placa o en otro componente:

- **Control de motores**: vive en CENTRAL.
- **Cinemática inversa**: vive en CENTRAL.
- **PID lateral del arquero**: vive en CENTRAL. ABAJO solo entrega measurement (profundidad signed).
- **PID de heading**: vive en CENTRAL.
- **Detección de "área chica"**: requiere pose absoluta (cámaras) — vive en ARRIBA.
- **Detección de pelota**: visión, vive en ARRIBA.
- **Estrategia táctica**: FSM principal en CENTRAL.
- **Comunicación con árbitros**: bridge en ARRIBA via placa COMM.
- **Coordinación con partner**: ESP-NOW manejado por ARRIBA.
- **Almacenamiento de datos en SD**: el Teensy 4.0 no tiene slot (solo 4.1). Si hace falta, agregar en otra placa.

ABAJO es un **sensor inteligente puro**. Nada más, nada menos.

---

## 14. Referencias

- Arquitectura general: [`docs/ARQUITECTURA-3-PLACAS-2026.md`](../ARQUITECTURA-3-PLACAS-2026.md)
- Pinout de la placa ABAJO: [`hardware/electronics/mapa-pines-placas-nuevas.md`](../../hardware/electronics/mapa-pines-placas-nuevas.md)
- Schematic de la placa ABAJO: [`hardware/electronics/pcb_design/down_board/Schematic_Roboliga_2026_Futbol_2026-04-12.pdf`](../../hardware/electronics/pcb_design/down_board/Schematic_Roboliga_2026_Futbol_2026-04-12.pdf)
- Protocolo UART: `src/shared/proto.h`
- Tipos compartidos: `src/shared/types.h`
- Implementación actual del firmware: `src/down/`
- Análisis de unrouted nets del PCB 04-12: [`research/in-progress/2026-05-10-auditoria-pcb-down-unrouted-nets.md`](../../research/in-progress/2026-05-10-auditoria-pcb-down-unrouted-nets.md)
- SparkFun OTOS datasheet y librería oficial: https://www.sparkfun.com/products/24904
- ALS-PT19 datasheet: https://lcsc.com/datasheet/lcsc_datasheet_2102090218_EVERLIGHT-ALS-PT19-315C-L177-TR8_C146233.pdf
- CD4051BM datasheet: https://www.ti.com/product/CD4051B

---

*Documento mantenido por IITA — Instituto de Innovación y Tecnología Aplicada, Salta, Argentina.*
