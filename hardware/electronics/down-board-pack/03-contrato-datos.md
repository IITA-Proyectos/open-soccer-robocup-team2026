---
title: "Contrato de datos de la placa DOWN — qué envía, con precisión y ejemplos (v2)"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [comunicacion, firmware, protocolo, contrato, down-board, ambos]
robot: ambos
area: comunicacion
tipo: protocolo
contract-schema: 2
related: [software/teensy/Soccer 2026/src/shared/proto.h, software/teensy/Soccer 2026/src/shared/types.h, docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md]
---

# Contrato de datos DOWN — referencia única para programar DOWN, CENTRAL y TOP

> **Propósito.** Definir SIN AMBIGÜEDAD qué datos emite la placa DOWN, con qué
> formato, unidades, rangos, convenciones y ejemplos byte-a-byte (CRC real
> calculado). Quien programe DOWN implementa exactamente esto; quien programe
> CENTRAL o TOP interpreta exactamente esto. Si el código y este documento
> difieren, **se corrige el que esté mal y se versiona el contrato**
> (`contract-schema`).

## 0. Frontera de responsabilidad (leer primero)

DOWN **no tiene IMU ni heading** (el heading vive en TOP→CENTRAL). Por lo tanto:

- **DOWN entrega primitivas geométricas en MARCO DEL ROBOT** (ángulo de la
  línea, penetración, vector de escape robot-frame) + **eventos locales**
  (corner, fin-de-línea, levantado, calib dudosa) con **precisión apta para un
  PID en CENTRAL**.
- **CENTRAL** (que tiene heading vía `WORLD_SNAPSHOT`) **clasifica** si la
  línea es lateral / fondo / frente y decide el escape en marco-cancha. El
  `escape_angle` robot-frame de DOWN es el fallback que no necesita heading.

DOWN **no** dice "esta es la línea de fondo": dice "hay línea a +45.00°,
penetración 15 mm, escape a -135.00°, evento CORNER". La semántica de cancha la
pone CENTRAL.

## 1. Capa de transporte (proto.h)

Frame idéntico para todos los enlaces (`src/shared/proto.h`):

```
┌──────┬─────┬──────┬─────┬────────────┬──────────┬──────┐
│ 0xAA │ LEN │ TYPE │ SEQ │  PAYLOAD   │ CRC16 BE │ 0x55 │
│  1B  │ 1B  │  1B  │ 1B  │  LEN bytes │   2B     │  1B  │
└──────┴─────┴──────┴─────┴────────────┴──────────┴──────┘
```

| Elemento | Valor / regla |
|---|---|
| START | `0xAA` |
| LEN | longitud del PAYLOAD en bytes (= `sizeof(struct)`) |
| TYPE | `MsgType` (tabla §2) |
| SEQ | contador 0–255 que **envuelve**; lo incrementa el emisor por frame |
| CRC16 | CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`, sin reflexión, xorout `0x0000`) sobre **LEN+TYPE+SEQ+PAYLOAD** (NO incluye START ni END). Se transmite **big-endian** (primero byte alto, luego bajo). |
| END | `0x55` |
| Overhead | 7 bytes; payload máximo **32 bytes** (`PROTO_MAX_PAYLOAD`) |

**Endianness del payload (CRÍTICO):** los structs se serializan con `memcpy`
crudo desde un Teensy (ARM, little-endian). Por lo tanto **todo entero
multibyte del payload es little-endian** (byte menos significativo primero).
El CRC, en cambio, va big-endian. No confundir.

**Enlace físico:** UART **230400 baud, 8N1**, sin control de flujo.
- DOWN → CENTRAL: `Serial1` de DOWN → (conector U11) → `Serial?` de CENTRAL.
  **El mapeo físico de UART está pendiente de verificación (TASK-008/014):
  medir con osciloscopio antes de confiar.**
- DOWN → TOP: `Serial5` de DOWN → `Serial1` de TOP.

## 2. Catálogo de mensajes que DOWN emite y recibe

| Dir | TYPE | Nombre | Payload | Frecuencia | Propósito |
|---|---|---|---|---|---|
| DOWN→CENTRAL | `0x10` | `LINE_URGENT` | `LineStatusV2` (16 B) | 200 Hz | Bus de emergencia: línea + eventos (§3) |
| DOWN→TOP | `0x11` | `DOWN_OTOS_POSE` | `Pose2D` (7 B) | 100 Hz | Odometría OTOS (§4) |
| DOWN→TOP | `0x12` | `DOWN_OTOS_VEL` | `Velocity2D` (7 B) | 100 Hz | Velocidades + slip (§4) |
| CENTRAL→DOWN | `0x20` | `CENTRAL_RESET_OTOS` | `uint8` | evento | Reset odometría |
| CENTRAL→DOWN | `0x21` | `CENTRAL_CALIB_LINE` | `uint8` (0=carpet,1=white,2=auto) | evento | Disparar calibración |

> El contrato cubre lo que DOWN **emite** (foco del pedido) y los comandos que
> **recibe** (para que CENTRAL sepa cómo pedir calibración/reset).

## 3. `LineStatusV2` — DOWN → CENTRAL (TYPE 0x10), 16 bytes

**Reemplaza el `LineStatus` v1 de 5 bytes** (insuficiente: no llevaba escape,
corner, fin-de-línea, cross-track ni penetración en mm reales). 16 B ≪ 32 B.

### 3.1 Layout exacto (offsets, little-endian)

| Off | Campo | Tipo | Unidad | Rango / Sentinela | Significado |
|----:|-------|------|--------|-------------------|-------------|
| 0 | `schema_version` | u8 | — | `2` | Versión de este contrato. CENTRAL **descarta** si ≠ esperado. |
| 1 | `data_valid` | u8 | — | 0 / 1 | **Compuerta maestra.** 0 ⇒ CENTRAL NO debe usar la geometría (levantado / calib dudosa). |
| 2 | `line_angle_centideg` | i16 | centideg | −18000..+18000; **N/A = −32768** | Dirección del centroide de la línea, **marco robot** (§3.3). |
| 4 | `escape_angle_centideg` | i16 | centideg | −18000..+18000; **N/A = −32768** | Hacia dónde moverse para alejarse de la línea, marco robot. |
| 6 | `penetration_mm` | u16 | mm | 0..65534; **N/A = 65535** | Cuánto cruzó el punto de referencia hacia el blanco (0 = recién tocando). Calibrado a mm reales (NO “# sensores”). |
| 8 | `cross_track_mm` | i16 | mm | −5000..+5000; **N/A = −32768** | **Error lateral con signo** de la línea respecto del borde de referencia del robot. Señal fina para el **PID lateral del arquero** (§3.4). |
| 10 | `line_present` | u8 | — | 0 / 1 | Hay línea bajo el robot (con histéresis, anti-parpadeo). |
| 11 | `sensors_on_line` | u8 | conteo | 0..32 | Sensores del anillo viendo blanco (diagnóstico + base de CORNER). |
| 12 | `event_flags` | u8 | bitfield | ver §3.2 | Eventos locales. |
| 13 | `quality` | u8 | — | 0..100 | Confianza en la geometría reportada (CENTRAL pondera el PID). |
| 14 | `sample_age_ms` | u8 | ms | 0..255 | Antigüedad de la medición física (los sensores se muestrean a 1 kHz, se envía a 200 Hz). |
| 15 | `reserved` | u8 | — | 0 | Forward-compat. Emisor escribe 0; receptor ignora. |

`sizeof(LineStatusV2) == 16` (`__attribute__((packed))`, sin padding). El
firmware DEBE incluir `static_assert(sizeof(LineStatusV2)==16)`.

### 3.2 `event_flags` (bitfield)

| Bit | Máscara | Nombre | Significado |
|----:|---------|--------|-------------|
| 0 | `0x01` | `IMMINENT_EXIT` | Penetración profunda: el robot está por cruzar del todo. CENTRAL debe frenar/escapar ya. |
| 1 | `0x02` | `CORNER` | Blanco en dos sectores ~perpendiculares simultáneos ⇒ esquina de cancha. |
| 2 | `0x04` | `LINE_END` | Venía siguiendo una línea continua y se terminó (p.ej. fin de la línea del área chica). |
| 3 | `0x08` | `LIFTED` | Robot separado del piso ⇒ `data_valid` será 0; ignorar geometría. |
| 4 | `0x10` | `CALIB_SUSPECT` | Calibración ambigua (no separa piso/blanco). Geometría degradada o `data_valid`=0. |
| 5 | `0x20` | `MUX_DEAD` | Un multiplexor no responde ⇒ cobertura del anillo reducida. |
| 6 | `0x40` | `DEGRADED_GEOMETRY` | Corriendo con anillo parcial (p.ej. 1 mux/8 sensores): ángulo de menor exactitud. |
| 7 | `0x80` | reservado | — |

`event_flags` puede traer varios bits a la vez (ej. `IMMINENT_EXIT|CORNER` =
`0x03`). `LIFTED`/`CALIB_SUSPECT` implican normalmente `data_valid=0`.

### 3.3 Convención de ángulos (sin ambigüedad)

```
            +Y  (FRENTE del robot)
             │   line_angle = 0
             │
   -90.00° ──┼── +90.00°      (vista desde ARRIBA)
  (izq, -9000)│ (der, +9000)
             │
            -Y  (ATRÁS)  = ±180.00° (±18000)
```

- Origen: centro geométrico del robot. Eje 0° = **frente** (+Y del robot).
- Signo: **positivo = sentido horario visto desde arriba** (hacia la derecha
  del robot). Rango `(-18000, +18000]` centidegrees (= grados ×100).
- `line_angle` = dirección del **centroide** de los sensores en blanco.
- `escape_angle` = dirección recomendada de huida (≈ bisectriz del sector SIN
  blanco; típicamente cerca de `line_angle ± 18000`, pero se computa del patrón
  real de sensores, no por fórmula).
- **N/A**: cuando no hay línea (`line_present=0`) los ángulos valen `-32768`
  (INT16_MIN) y CENTRAL los trata como inválidos, NO como 0°.

### 3.4 Uso por CENTRAL (PID arquero pisando la línea de fondo del área)

`cross_track_mm` es el error con signo entre la línea y el borde de referencia
del robot (configurable: típicamente el borde trasero). El PID lateral de
CENTRAL hace `setpoint = 0` sobre `cross_track_mm`: el arquero se desplaza
lateralmente manteniendo la parte de atrás sobre la línea de fondo del área.
`LINE_END` avisa que llegó al extremo de esa línea; `CORNER` avisa esquina.
CENTRAL pondera la acción por `quality` y la **anula si `data_valid=0`**.

### 3.5 Reglas de interpretación obligatorias (CENTRAL/TOP)

1. Si `schema_version` ≠ 2 → descartar frame y contar error de contrato.
2. Si `data_valid == 0` → **no usar** `line_angle/escape/penetration/cross_track`
   para control; solo leer `event_flags` (LIFTED/CALIB_SUSPECT) y reaccionar
   conservador.
3. Sentinelas (`-32768` / `65535`) **no** son medidas: significan N/A.
4. `line_present` ya trae histéresis: CENTRAL **no** debe re-filtrar con su
   propio umbral (evita doble histéresis).
5. `sample_age_ms` alto + sin frames nuevos ⇒ tratar como enlace degradado
   (ver máquina OK/STALE/LOST del diseño de comunicaciones).

### 3.6 Ejemplos byte-a-byte (CRC real, FRAME completo, hex)

SEQ de ejemplo entre paréntesis. Bytes en MAYÚSCULA = frame completo listo para
poner en el cable. Verificá tu encoder/decoder contra estos valores exactos.

**A — Sin línea, sobre carpet, todo OK** (SEQ=0x00)
`data_valid=1, line/escape/pen/xt=N/A, present=0, nsens=0, ev=0x00, q=95, age=2`
```
FRAME = AA 10 10 00 02 01 00 80 00 80 FF FF 00 80 00 00 00 5F 02 00 FD 01 55
CRC16 = 0xFD01
```

**B — Línea a +45.00°, poco profunda (arquero siguiéndola)** (SEQ=0x01)
`valid=1, line=+4500, escape=-13500, pen=15mm, xt=-8mm, present=1, nsens=4, ev=0, q=88, age=1`
```
FRAME = AA 10 10 01 02 01 94 11 44 CB 0F 00 F8 FF 01 04 00 58 01 00 DF BF 55
CRC16 = 0xDFBF
```

**C — Salida inminente + esquina** (SEQ=0x2A)
`valid=1, line=+90.00°, escape=-90.00°, pen=60mm, xt=+30mm, present=1, nsens=11, ev=IMMINENT_EXIT|CORNER (0x03), q=70, age=1`
```
FRAME = AA 10 10 2A 02 01 28 23 D8 DC 3C 00 1E 00 01 0B 03 46 01 00 E6 84 55
CRC16 = 0xE684
```

**D — Robot levantado (datos inválidos)** (SEQ=0x2B)
`valid=0, geometría N/A, present=0, ev=LIFTED (0x08), q=0, age=0`
```
FRAME = AA 10 10 2B 02 00 00 80 00 80 FF FF 00 80 00 00 08 00 00 00 48 97 55
CRC16 = 0x4897
```

**E — Fin de línea del área (LINE_END)** (SEQ=0x2C)
`valid=1, geometría N/A (línea desapareció), present=0, ev=LINE_END (0x04), q=60, age=1`
```
FRAME = AA 10 10 2C 02 01 00 80 00 80 FF FF 00 80 00 00 04 3C 01 00 58 02 55
CRC16 = 0x5802
```

Decodificación de B, paso a paso (para validar un parser):
`AA`(start) `10`(len=16) `10`(type=LINE_URGENT) `01`(seq) →
payload `02`(schema=2) `01`(valid) `94 11`(LE → 0x1194 = 4500 = +45.00°)
`44 CB`(LE → 0xCB44 = -13500 = -135.00°) `0F 00`(LE → 15 mm)
`F8 FF`(LE → 0xFFF8 = -8 mm) `01`(present) `04`(4 sensores) `00`(sin eventos)
`58`(q=88) `01`(age=1 ms) `00`(reserved) → `DF BF`(CRC16=0xDFBF) `55`(end).

## 4. `Pose2D` / `Velocity2D` — DOWN → TOP (TYPE 0x11 / 0x12)

OTOS es **opcional**. Sin OTOS, DOWN sigue 100% funcional para línea; igual
emite estos mensajes para que TOP sepa que DOWN está vivo, pero con
**disponibilidad honesta**.

`Pose2D` (7 B, LE): `int16 x_mm`, `int16 y_mm`, `int16 heading_centideg`,
`uint8 confidence`.
`Velocity2D` (7 B, LE): `int16 vx_mm_s`, `int16 vy_mm_s`,
`int16 omega_centideg_s`, `uint8 slip_estimate`.

**Regla de disponibilidad (corrige el bug de la auditoría):**
- `confidence == 0` ⇒ **pose INVÁLIDA** (no hay OTOS o no listo). TOP **no
  debe** fusionarla ni mostrar (0,0) como posición real.
- `1..100` ⇒ calidad de la estimación (100 = 2 OTOS sanos; ~85 = 1 OTOS).
- `slip_estimate`: 0 = sin patinaje; >50 = anomalía (patada/choque).
- Si no hay OTOS, DOWN envía `Pose2D` con `confidence=0` a baja tasa (≥1 Hz)
  como latido; NO miente disponibilidad.

Ejemplos (CRC real):

**Sin OTOS (confidence=0 ⇒ inválida)** (SEQ=0x00)
```
FRAME = AA 07 11 00 00 00 00 00 00 00 00 45 1E 55     CRC16=0x451E
```
**OTOS OK: x=1234 mm, y=-560 mm, heading=+90.00°, conf=85** (SEQ=0x01)
```
FRAME = AA 07 11 01 D2 04 D0 FD 28 23 55 C2 4A 55      CRC16=0xC24A
```
(`D2 04`=LE 1234; `D0 FD`=LE −560; `28 23`=LE 9000=+90.00°; `55`=85).

## 5. Versionado del contrato

- `schema_version` viaja en el byte 0 de `LineStatusV2`. Cualquier cambio de
  layout **incrementa** `schema_version` y actualiza el frontmatter
  `contract-schema` de este documento.
- Migración v1→v2: el firmware DOWN actual usa el `LineStatus` de 5 B (v1, sin
  `schema_version`). Este documento define **v2** como el contrato objetivo;
  la migración se ejecuta en la implementación del programa DOWN (spec
  separada). Hasta migrar, CENTRAL/TOP deben saber qué versión esperan.

## 6. Checklist para quien programe cada placa

**DOWN (emisor):** implementar `LineStatusV2` exacto (orden, unidades,
sentinelas, histéresis en `line_present`, `data_valid` como compuerta), CRC y
frame de `proto.h`, `static_assert(sizeof==16)`, y el latido de `Pose2D`
conf=0 si no hay OTOS.

**CENTRAL (receptor):** validar `schema_version`, respetar `data_valid`,
tratar sentinelas como N/A, NO re-filtrar `line_present`, alimentar el PID
lateral con `cross_track_mm` ponderado por `quality`, clasificar lateral/
fondo/frente con su heading, reaccionar a `event_flags`.

**TOP (receptor de OTOS):** `confidence==0` ⇒ pose inválida (no fusionar, no
(0,0)); usar `confidence` como peso.

## 7. Fuentes

- `software/teensy/Soccer 2026/src/shared/proto.h`, `crc16.h`, `types.h`
  (definiciones de transporte y structs v1).
- `software/teensy/Soccer 2026/src/down/comm_central.cpp`, `comm_top.cpp`
  (encoders actuales — v1).
- CRCs y frames de §3.6 / §4 calculados con CRC-16/CCITT-FALSE sobre
  LEN+TYPE+SEQ+PAYLOAD (verificable; algoritmo en `crc16.h`).
- Diseño marco: `docs/decisions/2026-05-18-diseno-comunicaciones-robusto-definitivo.md`.
