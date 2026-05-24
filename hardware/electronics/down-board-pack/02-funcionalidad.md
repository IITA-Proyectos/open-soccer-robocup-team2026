---
title: "Placa DOWN — Especificación funcional (qué hace y cómo)"
date: 2026-05-24
status: vigente
parte-de: down-board-pack
basado-en: docs/firmware/FIRMWARE-PLACA-ABAJO.md (curado, sin pinout obsoleto)
---

# Placa DOWN — Especificación funcional

> Este doc describe **qué hace** el firmware de la placa DOWN. Para los pines
> físicos (Teensy ↔ CD4051 ↔ sensores) ver [`01-pinout-y-posiciones.md`](01-pinout-y-posiciones.md).
> Para el contrato byte-a-byte de las tramas ver [`03-contrato-datos.md`](03-contrato-datos.md).
> Para el diseño general de las comunicaciones de las 3 placas ver
> [`04-protocolo-comunicaciones.md`](04-protocolo-comunicaciones.md).

## 1. Qué es la placa DOWN

Es un **sensor inteligente puro**. No toma decisiones tácticas, no controla
motores, no corre lazos de control. Solo lee sensores del piso (32 fotodiodos
de línea + 2 OTOS de odometría óptica) y los reporta procesados a las otras
dos placas.

**Doble stream de salida**:

- **Hacia CENTRAL** (Serial1, conector U11, bus de emergencia): `LINE_URGENT`
  cada 5 ms (200 Hz). Ángulo de línea + profundidad + flag de salida inminente.
  Es el camino más corto para que el CENTRAL frene si el robot está por
  salirse de la cancha.
- **Hacia TOP** (Serial5, conector U10): `DOWN_OTOS_POSE` + `DOWN_OTOS_VEL`
  cada 10 ms (100 Hz). Pose odométrica y velocidades, que TOP usa en su
  fusión sensorial.

**Doble stream de entrada**: comandos administrativos desde CENTRAL (Serial1)
y desde TOP (Serial5) — reset OTOS, recalibrar umbrales de línea.

La placa DOWN **no necesita conocer el rol del robot** (arquero/delantero)
ni el estado del partido (running/stop). Siempre reporta lo mismo. CENTRAL
decide qué hacer con el dato según el contexto.

## 2. Hardware (resumen)

| Componente | Cantidad | Nota |
|---|---|---|
| MCU Teensy 4.0 (U7) | 1 | Cortex-M7 a 600 MHz, 1 MB RAM, 2 MB flash |
| Sensores ALS-PT19 (F1–F32) + LED activo | 32 | Reflectivo activo |
| Multiplexores CD4051BM (U1–U4) | 4 | 8 canales cada uno = 32 sensores |
| SparkFun OTOS (U5, U6) | 2 | Buses I²C separados — comparten dirección 0x17 |
| Conector UART hacia TOP (U10) | 1 | Serial5 (pines 21/20) |
| Conector UART hacia CENTRAL (U11) | 1 | Serial1 (pines 0/1) |
| Reguladores MP1584-EN (U8, U9) | 2 | 7.4 V → 5 V + 7.4 V → 3.3 V |
| LED de estado | 1 | LED_BUILTIN (pin Arduino 13) |

**Pinout completo + posiciones físicas (x, y mm) de cada sensor**:
ver [`01-pinout-y-posiciones.md`](01-pinout-y-posiciones.md).

## 3. Responsabilidades funcionales

| # | Responsabilidad | Detalle |
|---|---|---|
| R1 | Muestrear los 32 sensores de luz a 1 kHz | 4 muxes barrer 8 canales cada uno |
| R2 | Calcular ángulo de la línea detectada | Centroide angular ponderado |
| R3 | Calcular profundidad (depth) de la línea | Cuántos sensores adyacentes ven blanco |
| R4 | Disparar flag `imminent_exit` | Cuando ≥ N sensores ven blanco simultáneamente |
| R5 | Calibrar umbrales de línea por sensor | Blanco (línea) y carpet (verde) |
| R6 | Detectar "robot levantado" | Si todos los sensores reportan baja luz uniforme |
| R7 | Leer los 2 OTOS a 100 Hz | I²C dual (`Wire` + `Wire1`) |
| R8 | Fusionar OTOS en pose central | Promedio + heading inferido del diferencial |
| R9 | Calcular slip estimate | Diferencia anómala entre OTOS izquierdo y derecho |
| R10 | Enviar `LINE_URGENT` a CENTRAL a 200 Hz | Bus de emergencia, latencia < 15 ms |
| R11 | Enviar `DOWN_OTOS_POSE/VEL` a TOP a 100 Hz | Para fusión sensorial |
| R12 | Recibir comandos administrativos | Reset OTOS, calibrar línea |
| R13 | Watchdog y recovery elegante | UART caída, OTOS desconectado, mux malo |
| R14 | LED de estado | Comunicar al humano el estado del firmware |

## 4. Modos de operación

Un único **modo táctico** (siempre reporta lo mismo) + modos administrativos.

| Modo | Cuándo se activa | Comportamiento |
|---|---|---|
| `NORMAL` (default) | Al boot, después de modos admin | Lectura continua + streams a 200/100 Hz |
| `CALIBRATING_CARPET` | Comando `CENTRAL_CALIB_LINE` flag=0 | 32 muestras × 10 ms por sensor → promedio carpet. Bloquea streams ~320 ms |
| `CALIBRATING_WHITE` | Comando `CENTRAL_CALIB_LINE` flag=1 | Ídem sobre línea blanca |
| `OTOS_RESET` | Comando `CENTRAL_RESET_OTOS` | Pose (x, y, heading) en 0 en ambos OTOS. Instantáneo |
| `LIFTED` (auto) | Detectado por R6 | Sigue enviando streams con flag `lifted=1`. CENTRAL ignora datos de línea |

**Transiciones automáticas**:
- `CALIBRATING_*` → `NORMAL` al terminar la captura.
- `LIFTED` → `NORMAL` cuando los sensores reportan valores normales por > 1 segundo (anti-glitch).

## 5. Procesamiento del anillo de 32 sensores

### 5.1 Barrido del anillo

**Hardware**: cada uno de los 4 muxes CD4051 tiene SUS PROPIOS 3 selectores
A/B/C (NO compartidos — ver §4 de `01-pinout`). Los 4 muxes pueden barrerse
**en paralelo**: al fijar el canal `i` en los 12 selectores (3 por mux),
los 4 muxes presentan sus respectivos canales `i` en las 4 salidas analógicas
simultáneamente.

**Pseudocódigo del barrido** (ver implementación real en
[`firmware/down/line_ring.cpp`](firmware/down/line_ring.cpp)):

```cpp
for (int i = 0; i < 8; ++i) {
    uint8_t ch = MUX_CH_FOR_SENSOR[i];   // canal real (scrambling del PCB)
    for (int m = 0; m < 4; ++m) {
        digitalWrite(PIN_MUX_A[m], (ch & 0x01) ? HIGH : LOW);
        digitalWrite(PIN_MUX_B[m], (ch & 0x02) ? HIGH : LOW);
        digitalWrite(PIN_MUX_C[m], (ch & 0x04) ? HIGH : LOW);
    }
    delayMicroseconds(5);                 // settle time CD4051
    for (int m = 0; m < 4; ++m) {
        raw[m * 8 + i] = analogRead(PIN_MUX_OUT[m]);
    }
}
```

**Timing estimado**:
- Settle time mux: ~5 µs.
- `analogRead` Teensy 4.0 a 10 bits: ~2 µs.
- Por canal: 5 + 4 × 2 = 13 µs.
- Por barrido completo (8 canales): ~110 µs.
- **Frecuencia máxima de lectura: > 8 kHz** (margen de 8× sobre el objetivo de 1 kHz).

### 5.2 Calibración

Cada sensor tiene umbrales propios porque los ALS-PT19 tienen variabilidad
de fabricación + diferencias de iluminación local.

**Almacenado por sensor**:
- `carpet_avg[i]` — valor promedio sobre carpet verde.
- `white_avg[i]` — valor promedio sobre línea blanca.
- `threshold[i] = (carpet_avg[i] + white_avg[i]) / 2`.

**Procedimiento**:
1. CENTRAL envía `CENTRAL_CALIB_LINE` con flag 0 (carpet).
2. DOWN entra en `CALIBRATING_CARPET`.
3. Durante 320 ms (32 muestras × 10 ms): lee los 32 sensores y promedia.
4. Vuelve a `NORMAL`.
5. Operario mueve el robot a línea blanca.
6. CENTRAL envía `CENTRAL_CALIB_LINE` con flag 1. Ídem.
7. DOWN recalcula `threshold[i]` y queda listo.

**Persistencia**: la calibración vive en RAM. Al apagar se pierde. Mejora
futura: guardar en EEPROM del Teensy.

**Detección de calibración inválida**: si `white_avg[i] - carpet_avg[i] < 100`
(separación insuficiente), marcar sensor `i` como "no confiable" y excluirlo
del cálculo de ángulo.

### 5.3 Detección de línea (algoritmo)

Cada sensor tiene una **posición física (x, y)** en milímetros desde el
centro del robot — disponible en la LUT `SENSOR_POS[32]` (ver §5b de
`01-pinout`). Esto permite calcular el ángulo del centroide **ponderado por
posición real**, no por asumir distribución uniforme.

```cpp
float sum_x = 0, sum_y = 0;
uint8_t depth = 0;

for (int i = 0; i < 32; ++i) {
    if (raw[i] >= threshold[i]) {
        depth++;
        float weight = (raw[i] - threshold[i]) /
                       float(white_avg[i] - threshold[i]);
        weight = clamp(weight, 0.0f, 1.0f);
        sum_x += weight * SENSOR_POS[i].x_mm;
        sum_y += weight * SENSOR_POS[i].y_mm;
    }
}

float angle_rad = atan2(sum_y, sum_x);
float angle_deg = angle_rad * (180.0f / M_PI);  // 0° = +X = derecha del robot
```

**Convención angular**: 0° = +X (derecha del robot), +90° = +Y (adelante),
±180° = atrás. Es la misma convención del sistema de referencia documentado
en `01-pinout-y-posiciones.md` §5b.

**Profundidad (`depth`)**: cantidad de sensores que ven blanco simultáneamente.
Proxy de qué tan adentro de la línea está el robot:
- `depth = 0` → fuera de línea.
- `depth = 1-2` → tocando borde.
- `depth = 3-5` → pisando línea (típico).
- `depth ≥ 6` → muy adentro, saliéndose.

**Flag `imminent_exit`**: se dispara cuando `depth ≥ IMMINENT_EXIT_DEPTH`
(configurable, default 6). Señal urgente para que CENTRAL frene.

### 5.4 Filtrado y redundancia

Tres niveles de filtrado para reducir falsos positivos:

**Nivel 1 — Filtro temporal (promedio móvil)**:
- Mantiene `raw_buf[i][4]` — últimas 4 lecturas por sensor.
- Cada tick, `raw_filtered[i] = mean(raw_buf[i])`.
- Costo: 32 × 4 = 128 bytes de RAM. Reduce ruido de alta frecuencia (vibración, EMI motores).

**Nivel 2 — Hysteresis del umbral**:
- En vez de `if (raw >= threshold)`, dos umbrales:
  - `threshold_high[i] = threshold[i] + 20` (para pasar de "carpet" a "blanco").
  - `threshold_low[i] = threshold[i] - 20` (para pasar de "blanco" a "carpet").
- Evita flickeo cuando el sensor está cerca del umbral.

**Nivel 3 — Filtro espacial (consistencia entre vecinos)**:
- Si un sensor aislado dice "blanco" pero sus vecinos directos dicen
  "carpet", probablemente es ruido.
- Regla: para que un sensor cuente, al menos uno de sus vecinos directos
  también debe estar sobre el threshold.
- **Vecindario**: definido por proximidad espacial en la LUT `SENSOR_POS[]`,
  no por índice consecutivo (porque la numeración S1–S32 no es geométrica;
  ver `01-pinout` §5b).

Implementación: ver [`firmware/shared/line_filters.{h,cpp}`](firmware/shared/line_filters.h)
(con 22 tests host-native en `tests/test_line_filters.cpp`).

### 5.5 Detección de "robot levantado" (lifted)

Cuando el árbitro levanta el robot (final del partido, reposicionar,
accidental), los sensores ven aire en vez de carpet:

- **Carpet verde**: ~200-400 counts (10-bit ADC).
- **Aire**: uniformemente baja, ~50-150 counts.
- **Línea blanca**: ~600-900 counts.

**Algoritmo**:

```cpp
bool is_lifted() {
    int low_count = 0;
    for (int i = 0; i < 32; ++i) {
        if (raw_filtered[i] < (carpet_avg[i] - 50)) low_count++;
    }
    if (low_count >= 28) return true;  // 28/32 = 87%

    // Cross-check con OTOS surface quality (cuando esté activa la lib)
    if (otos_left.surface_quality < 30 &&
        otos_right.surface_quality < 30) return true;

    return false;
}
```

**Comportamiento bajo LIFTED**:
- Flag `lifted = 1` en cada `LINE_URGENT` enviado.
- CENTRAL ignora medidas de línea con este flag.
- DOWN sigue muestreando; cuando vuelve al piso, < 1 s para desactivar el flag.

**Anti-glitch**: el flag se activa solo después de **≥ 100 ms continuos** de
criterio cumplido (evita falsos positivos por bumps).

Implementación: [`firmware/shared/surface_monitor.{h,cpp}`](firmware/shared/surface_monitor.h)
(con tests en `tests/test_down_surface.cpp`).

## 6. Procesamiento de odometría (OTOS dual)

### 6.1 Lectura I²C

Los 2 SparkFun OTOS están en buses I²C separados (necesario porque ambos
comparten dirección 0x17 de fábrica):
- **OTOS U5** → `Wire` (I²C0) — SDA=18, SCL=19.
- **OTOS U6** → `Wire1` (I²C1) — SDA=17, SCL=16.

(Detalles completos en `01-pinout-y-posiciones.md` §6.)

**Frecuencia máxima de lectura**: el SparkFun OTOS actualiza internamente
hasta ~50 Hz. Leerlo a 100 Hz desde el Teensy es seguro (devuelve el último
valor disponible).

**Latencia I²C**: a 400 kHz (Fast Mode), una transacción de OTOS
(pose + velocity, ~24 bytes) toma ~700 µs. Dos OTOS en buses distintos: también
~700 µs (no se serializan).

### 6.2 Fusión central + análisis diferencial

Los 2 OTOS están montados **a los costados del robot** (separación
configurable en `OTOS_SEPARATION_MM` — default 200 mm, a confirmar con
montaje real).

**Datos por OTOS**:
- `pose.x`, `pose.y` (mm desde último reset).
- `pose.heading` (grados desde último reset).
- `vel.vx`, `vel.vy` (mm/s).
- `vel.omega` (rad/s).
- `surface_quality` (0-255, calidad óptica).

**Fusión del centro del robot**:

```cpp
// Pose central = promedio simple
center.x = (left.pose.x + right.pose.x) / 2.0f;
center.y = (left.pose.y + right.pose.y) / 2.0f;

// Heading: dos formas, elegir según confiabilidad
float heading_avg = (left.pose.heading + right.pose.heading) / 2.0f;
float heading_diff = atan2(right.pose.y - left.pose.y, OTOS_SEPARATION_MM)
                     * (180.0f / M_PI);

if (abs(heading_avg - heading_diff) < 5.0f) {
    center.heading = heading_avg;
} else {
    // discrepancia → confiar en el de mejor surface_quality
    center.heading = (left.surface_quality > right.surface_quality)
                     ? left.pose.heading : right.pose.heading;
}
```

### 6.3 Slip estimate

Detección de cuándo una rueda patina (al patear, chocar, cancha sucia):

```cpp
float expected_diff = omega_rad_s * OTOS_SEPARATION_MM;
float observed_diff = right.vel.vx - left.vel.vx;
float slip = abs(observed_diff - expected_diff);
```

Si `slip > 50 mm/s`, anomalía. Se reporta como `slip_estimate` (0-255
saturado) en `DOWN_OTOS_VEL`. CENTRAL puede usarlo para:
- Confiar menos en la pose mientras dura el slip.
- PID más agresivo de corrección de heading.
- Loguear el evento para análisis post-partido.

### 6.4 Reset y referenciación

Los OTOS acumulan pose desde el último reset. Eventos típicos de reset:

- **Al boot**: reset automático en `setup()` → ambos OTOS en (0, 0, 0).
- **Comando `CENTRAL_RESET_OTOS`** desde CENTRAL:
  - Al inicio de un nuevo partido (después de SETUP_GAME del árbitro).
  - Cuando TOP reposiciona el robot por triangulación de cámaras y necesita
    resetear el offset.
  - Cuando se detecta drift acumulado fuerte.

**El reset es instantáneo** — la librería SparkFun lo expone como una
llamada I²C única.

## 7. Comunicaciones (resumen)

Detalles byte-a-byte: ver [`03-contrato-datos.md`](03-contrato-datos.md).
Diseño general de comunicaciones de las 3 placas: ver [`04-protocolo-comunicaciones.md`](04-protocolo-comunicaciones.md).
Aquí solo el resumen aplicable a DOWN:

### 7.1 Stream DOWN → CENTRAL (`LINE_URGENT`, 200 Hz, Serial1)

- **Frecuencia**: 5 ms (200 Hz). Máxima razonable para línea — más alto es overkill.
- **Baud**: 230400. Frame de ~12-14 bytes tarda ~700 µs en transmitirse.
- **Payload**: `LineStatus` (5 bytes) + 7 bytes overhead = 12 bytes/frame.

### 7.2 Stream DOWN → TOP (`DOWN_OTOS_POSE` + `DOWN_OTOS_VEL`, 100 Hz, Serial5)

- **Frecuencia**: 10 ms (100 Hz) para ambos mensajes.
- **Payload**: `Pose2D` (7 bytes) + `Velocity2D` (7 bytes).
- **Carga total**: ~2800 bytes/s = 1.2% del baud. Holgado.

### 7.3 Recepción de comandos administrativos

**Desde CENTRAL (Serial1)**:

| Comando | Acción |
|---|---|
| `CENTRAL_RESET_OTOS` | Reset (x, y, heading) en ambos OTOS |
| `CENTRAL_CALIB_LINE` flag=0 | Calibración carpet (320 ms) |
| `CENTRAL_CALIB_LINE` flag=1 | Calibración blanca (320 ms) |

**Desde TOP (Serial5)**: no se esperan comandos relevantes en la arquitectura
actual; se decodifican por completitud del protocolo pero se ignoran.

## 8. Heartbeat, detección de fallos, recovery

**El envío continuo de los streams ES el heartbeat implícito**. No hay
ping/keepalive porque:
- CENTRAL espera `LINE_URGENT` cada 5 ms. Sin frames por 100 ms (20 frames
  perdidos) → marca `comm_down_is_line_fresh() = false` y degrada estrategia.
- TOP espera `DOWN_OTOS_POSE/VEL` cada 10 ms. Sin frames por 500 ms → marca
  pose como stale y degrada fusión sensorial.

| Mecanismo | Qué detecta | Tiempo de detección |
|---|---|---|
| SEQ del protocolo | Packets perdidos (no fatal, solo diagnóstico) | Inmediato |
| CRC-16/CCITT | Corrupción de frame | Inmediato |
| Timeout en CENTRAL | UART hacia CENTRAL caída | 100 ms |
| Timeout en TOP | UART hacia TOP caída | 500 ms |
| `is_lifted()` | Robot levantado del piso | 100 ms |
| Mux muerto | Mux roto o desconectado | 100 ms |
| OTOS muerto | Sensor roto, cable suelto | Inmediato |

**Recovery elegante**:
- **Mux muerto**: lecturas analógicas erráticas (flotantes). Si las 8
  lecturas asociadas a un mux son uniformemente bajas O altas por > 100 ms,
  marcar `mux_X_dead = true` y excluir sus 8 sensores del cálculo. El anillo
  sigue operativo con resolución degradada.
- **Un OTOS muerto**: usar solo el funcional. `slip_estimate = 0` (no se
  puede calcular sin diferencial). Flag `otos_quality_degraded = true`.
- **Ambos OTOS muertos**: `Pose2D` con `confidence = 0`. TOP detecta y
  degrada su fusión a solo IMU + cámaras.
- **UART caída**: DOWN sigue muestreando normalmente — no hay nada que pueda
  hacer. Cuando vuelve la comunicación, primer frame en < 5 ms reactiva todo.

## 9. Timing y latencias

### 9.1 Loop principal

```
loop():
    comm_central_tick()       # drena RX desde CENTRAL (~10 µs)
    comm_top_tick()           # drena RX desde TOP (~10 µs)

    if since_line_tick >= 1 ms:
        line_ring_tick()      # ~110 µs barrido + filtros
        check_lifted()        # ~20 µs

    if since_otos_tick >= 10 ms:
        otos_tick()           # ~700 µs I²C dual + fusión

    if since_line_send >= 5 ms:
        comm_central_send_line_urgent()  # ~50 µs + ~700 µs UART async

    if since_otos_send >= 10 ms:
        comm_top_send_status()  # ~100 µs + ~1500 µs UART async
```

- **Loop típico**: < 100 µs (la mayoría es polling de UART vacío).
- **Loop en tick de OTOS**: ~800 µs.
- **Peor caso (todos los ticks coinciden)**: ~1 ms.
- **Frecuencia efectiva del loop**: > 10 kHz.

### 9.2 Latencia sensor → CENTRAL (camino crítico)

| Paso | Tiempo |
|---|---|
| Sensor detecta blanco | 0 ms |
| Próximo tick de `line_ring_tick()` (peor caso) | < 1 ms |
| Próximo tick de `comm_central_send_line_urgent()` (peor caso) | < 5 ms |
| Encode + TX UART (~12 bytes a 230400 baud) | ~700 µs |
| Decode en CENTRAL + tick FSM + `motors_stop()` | < 5 ms |
| **Total** | **~10-12 ms** ✅ < 15 ms objetivo |

### 9.3 Latencia odometría → TOP (no crítico)

| Paso | Tiempo |
|---|---|
| OTOS internal update | hasta 20 ms |
| Próximo tick de `otos_tick()` | < 10 ms |
| Próximo tick de `comm_top_send_status()` | < 10 ms |
| Encode + TX UART | ~1500 µs |
| **Total** | **~25-40 ms** (aceptable para fusión sensorial) |

## 10. Estructuras de datos enviadas

Definiciones binarias canónicas en [`firmware/shared/types.h`](firmware/shared/types.h).
Contrato byte-a-byte en [`03-contrato-datos.md`](03-contrato-datos.md).

### 10.1 `LineStatus` (LINE_URGENT, DOWN → CENTRAL)

```cpp
struct LineStatus {
    int16_t angle_centideg;       // ángulo línea: 0 = +X derecha, ±18000 = ±180°
    uint8_t depth_mm;             // cantidad de sensores en blanco (0-32)
    uint8_t imminent_exit_flag;   // 0/1
    uint8_t flags;                // bit 0 = lifted, bit 1 = mux_dead,
                                  // bit 2 = calibration_invalid
} __attribute__((packed));        // 5 bytes payload
```

A 200 Hz: 12 × 200 = 2400 bytes/s. 1% del baud 230400.

### 10.2 `Pose2D` (DOWN_OTOS_POSE, DOWN → TOP)

```cpp
struct Pose2D {
    int16_t x_mm;
    int16_t y_mm;
    int16_t heading_centideg;
    uint8_t confidence;           // 0-100. 0 si ambos OTOS muertos
} __attribute__((packed));        // 7 bytes payload
```

### 10.3 `Velocity2D` (DOWN_OTOS_VEL, DOWN → TOP)

```cpp
struct Velocity2D {
    int16_t vx_mm_s;
    int16_t vy_mm_s;
    int16_t omega_centideg_s;
    uint8_t slip_estimate;        // 0-255 saturado
} __attribute__((packed));        // 7 bytes payload
```

A 100 Hz ambos: 28 bytes × 100 = 2800 bytes/s. 1.2% del baud.

## 11. Diagnóstico y debug

### 11.1 LED de estado (pin Arduino 13 = LED_BUILTIN)

| Patrón | Significado |
|---|---|
| Apagado | Firmware no inició o en `setup()` |
| Encendido fijo | NORMAL, todo OK |
| Parpadeo lento (1 Hz) | LIFTED detectado |
| Parpadeo rápido (5 Hz) | Algún mux o OTOS no responde |
| 3 parpadeos + pausa | Calibrando |
| Apagado tras estar prendido | UART hacia CENTRAL cayó hace > 1 s |

### 11.2 USB Serial (debug humano)

A 115200 baud por el puerto USB del Teensy 4.0, el firmware imprime:

- En `setup()`: estado de cada subsistema (sensores OK, OTOS encontrados, UARTs abiertos).
- Cada 1 segundo en NORMAL: contadores (frames TX, frames RX, errores CRC, sensores ON, OTOS quality).
- En cada cambio de modo: la transición.
- Si se detecta fallo (mux/OTOS muerto, robot levantado): print del evento.

Permite a Virginia/Elías conectar USB durante development sin osciloscopio.

### 11.3 Programa de diagnóstico standalone

Hay un binario separado [`diag/main_diag_down.cpp`](diag/main_diag_down.cpp)
(env PlatformIO: `diag_down`) que reemplaza el firmware NORMAL por una rutina
que:
- Toggle de cada selector de mux por separado.
- Dump de cada sensor S1–S32 con valor crudo.
- Comparación con un script Python ([`diag/diag_capture.py`](diag/diag_capture.py))
  que captura por COM y emite veredicto `OK / sospechoso / muerto` después
  de tapar los 32 sensores con papel blanco vs negro.

Útil para validar el anillo después de soldar / antes de un torneo.

## 12. Tabla resumen

| Aspecto | Valor |
|---|---|
| MCU | Teensy 4.0 @ 600 MHz |
| Sensores de línea | 32 × ALS-PT19 con LED activo (reflectivo activo) |
| Multiplexación | 4 × CD4051BM (12 selectores INDEPENDIENTES, 4 salidas analógicas) |
| Frecuencia muestreo línea | 1 kHz |
| Algoritmo de ángulo | Centroide ponderado por intensidad **y posición física (x,y mm)** |
| Filtros aplicados | Temporal (mov avg 4) + hysteresis + espacial (vecinos físicos) |
| Sensores odometría | 2 × SparkFun OTOS en I²C dual (Wire + Wire1) |
| Frecuencia muestreo OTOS | 100 Hz |
| Fusión OTOS | Promedio central + heading diferencial + slip estimate |
| UART hacia CENTRAL | Serial1 / U11 / 230400 baud / 200 Hz envío |
| UART hacia TOP | Serial5 / U10 / 230400 baud / 100 Hz envío |
| Latencia sensor → CENTRAL | ~10-12 ms ✅ < 15 ms objetivo |
| Heartbeat explícito | No (stream continuo cumple esa función) |
| Modos | 1 táctico (NORMAL) + 4 administrativos (calib×2, reset, lifted) |
| Carga CPU estimada | ~22% (mucho margen) |
| Detección robot levantado | Umbral uniforme bajo + cross-check con OTOS surface quality |
| Recovery | Auto: mux/OTOS muerto → degradación elegante, sigue operativo |

## 13. Lo que NO hace (límite de scope)

La placa DOWN es un **sensor inteligente puro**. NO hace:

- **Control de motores**: vive en CENTRAL.
- **Cinemática inversa**: vive en CENTRAL.
- **PID lateral del arquero**: vive en CENTRAL. DOWN solo entrega measurement (profundidad signed).
- **PID de heading**: vive en CENTRAL.
- **Detección de "área chica"**: requiere pose absoluta (cámaras) — vive en TOP.
- **Detección de pelota**: visión, vive en TOP.
- **Estrategia táctica (FSM)**: vive en CENTRAL.
- **Comunicación con árbitro**: bridge en TOP vía placa COMM.
- **Coordinación con partner robot**: ESP-NOW manejado por TOP.
- **Almacenamiento en SD**: el Teensy 4.0 no tiene slot. Si se necesita, en otra placa.

---

## 14. Referencias dentro del pack

- Pinout completo + posiciones físicas: [`01-pinout-y-posiciones.md`](01-pinout-y-posiciones.md)
- Contrato de datos (binario byte-a-byte): [`03-contrato-datos.md`](03-contrato-datos.md)
- Diseño general de comunicaciones: [`04-protocolo-comunicaciones.md`](04-protocolo-comunicaciones.md)
- Firmware vivo: [`firmware/down/`](firmware/down/) + [`firmware/shared/`](firmware/shared/)
- Tests host-native: [`tests/`](tests/)
- Diagnóstico de hardware + scripts: [`diag/`](diag/)
- Ground-truth (schematic + PCB + BOM): [`ground-truth/`](ground-truth/)

## 15. Referencias externas

- SparkFun OTOS: <https://www.sparkfun.com/products/24904>
- ALS-PT19 datasheet: <https://lcsc.com/datasheet/lcsc_datasheet_2102090218_EVERLIGHT-ALS-PT19-315C-L177-TR8_C146233.pdf>
- CD4051BM datasheet: <https://www.ti.com/product/CD4051B>
- Teensy 4.0 pinout PJRC: <https://www.pjrc.com/teensy/pinout.html>
