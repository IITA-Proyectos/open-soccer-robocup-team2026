---
title: "Contrato de datos de las cámaras OpenMV — protocolo, diseño de programas y gaps (v1)"
date: 2026-05-18
author: "Claude (Anthropic - claude-sonnet-4-6)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (claude-sonnet-4-6, Anthropic)"
status: draft
tags: [comunicacion, firmware, protocolo, contrato, camaras, openmv, top, vision]
robot: ambos
area: vision
tipo: protocolo
contract-schema: 1
related:
  - software/vision/enviar coordenadas 2 arcos y pelota
  - software/teensy/Soccer 2026/src/top/cameras.cpp
  - software/teensy/Soccer 2026/src/top/cameras.h
  - software/teensy/Soccer 2026/src/top/cameras_runtime.cpp
  - software/teensy/Soccer 2026/src/top/config_top.h
  - software/teensy/Soccer 2026/src/shared/cameras_fusion.h
  - software/teensy/Soccer 2026/src/shared/cameras_fusion.cpp
  - team-tasks/2026-05-18-task-022-camara-operativa.md
  - research/completed/2026-05-18-estado-firmware-robot-evaluacion-critica.md
---

# Contrato de datos — Cámaras OpenMV → placa TOP

> **Propósito.** Definir SIN AMBIGÜEDAD qué envía cada cámara OpenMV al TOP,
> en qué formato, con qué unidades, sentinelas, convenciones de coordenadas y
> ejemplos byte-a-byte. Quien programe la OpenMV implementa exactamente esto;
> quien programe el TOP (parser `cameras.cpp`) interpreta exactamente esto.
> Si el código y este documento difieren, **se corrige el que esté mal y se
> versiona el contrato** (`contract-schema`).
>
> Este documento se organiza en dos capas:
> - **Protocolo actual** — lo que hoy existe en el código real.
> - **Protocolo objetivo** — lo que debe existir para que las cámaras sean
>   operativas (no demo). Las diferencias entre ambas capas son los gaps P0.

---

## 0. Frontera de responsabilidad (leer primero)

Las cámaras OpenMV son los **únicos sensores de pelota y arcos** del robot. Sin
ellas el robot no puede jugar. Su responsabilidad se limita a:

- **Detectar** pelota naranja y arcos (amarillo / azul) por color en el frame.
- **Calcular** coordenadas 2D relativas a la cámara (espacio imagen o
  transformado por homografía).
- **Enviar** esas coordenadas al TOP vía UART en el protocolo acordado.
- **Informar "no detectado"** de forma inequívoca (sentinel definido).

La cámara **NO** decide táctica, **NO** conoce la pose global, **NO** sabe si
es delantera o trasera en términos de juego. Eso lo hace el TOP.

**Separación frontal / trasera:** cada robot tiene 2 cámaras OpenMV montadas
en lados opuestos. La cámara frontal apunta al frente del robot; la trasera,
al atrás. El TOP recibe ambas streams en UARTs distintos y las fusiona.
**Hoy hay 1 solo script genérico** (`software/vision/enviar coordenadas 2 arcos
y pelota`). Para que funcionen bien, hacen falta **2 programas separados**, uno
por cámara, porque la homografía, el mirror y el FOV son distintos.

---

## 1. Capa de transporte (estado actual)

### 1.1 UART físico

| Parámetro | Cámara frontal (cam 1) | Cámara trasera (cam 2) |
|-----------|------------------------|------------------------|
| Puerto Teensy | Serial3 | **Serial7** (⚠️ movida de Serial5 el 2026-05-29) |
| Pines Teensy 4.0 | RX=15, TX=14 | **RX=28, TX=29** |
| Conector en PCB TOP | U8 "UART-CAMERA1" | U9 "UART-CAMERA2" |
| Baud rate | 19200 8N1 | 19200 8N1 |
| Constante firmware | `UART_CAMERA1_BAUD = 19200` | `UART_CAMERA2_BAUD = 19200` |

Fuente: `src/top/cameras_runtime.cpp` (código vivo).

> **⚠️ Cambio 2026-05-29:** la cámara trasera se movió de **Serial5 → Serial7
> (pines 28/29)** porque **Serial5 (20/21) pasó a ser el UART TOP→CENTRAL**
> (`WORLD_SNAPSHOT`). Los pines físicos del conector U9 quedan pendientes de
> confirmar con Enzo (la placa TOP todavía no está armada). Firmware ya
> corregido en `cameras_runtime.cpp`.

### 1.2 Protocolo actual — 9 bytes raw

El script actual (`software/vision/enviar coordenadas 2 arcos y pelota:148-155`)
manda un paquete de 9 bytes en cada iteración del loop:

```
byte 0:  201        (HEADER1 — sync pelota)
byte 1:  Xp         (coord X pelota, uint8, rango nominal 0..200)
byte 2:  Yp_coded   (Y pelota + 100, uint8, rango nominal 0..200)
byte 3:  202        (HEADER2 — sync arco amarillo)
byte 4:  Xam        (coord X arco amarillo, uint8, 0..200)
byte 5:  Yam_coded  (Y arco amarillo + 100, uint8, 0..200)
byte 6:  203        (HEADER3 — sync arco azul)
byte 7:  Xaz        (coord X arco azul, uint8, 0..200)
byte 8:  Yaz_coded  (Y arco azul + 100, uint8, 0..200)
```

Fuente: `cameras.h:20-30` (comentario de layout), `cameras.cpp:6-9` (constantes).

**Sin byte de fin. Sin checksum. Sin longitud explícita.** La sincronización
depende enteramente de que los headers (201/202/203) no aparezcan como datos.

**Problema con los headers como datos:** si X o Y_coded de cualquier objeto
vale 201, 202 o 203, el parser del TOP se desincroniza. El rango X ∈ [0,200]
y Y_coded ∈ [0,200] hacen que **201 y 202 sean inalcanzables en datos pero 203
no** (Y_coded puede ser 203 si Y = 103, que sería X=103 en el espacio de
coordenadas de salida). La función `procesar_blob` clampea Y a [-100,100], por
lo tanto Y_coded ∈ [0,200] — 201 y 202 son inalcanzables. **X, en cambio, no
se clampea en el camino al `bytearray`:** si X > 200 se clampea a 200 por la
línea `if X>200: X=200` (`enviar...:100`), pero el cast `int(X)` antes de
poner en el array puede dar 201/202/203 en casos de desbordamiento numérico
de la homografía. Este es el bug R6 documentado en `cameras.h:8`.

---

## 2. Sentinel "no detectado" — el gap P0 central

### 2.1 Estado actual (inconsistencia)

Cuando no hay blobs, `procesar_blob` retorna `(0, 0)` — ver
`enviar...:81-82`:

```python
def procesar_blob(blobs, dibujar_color):
    if not blobs:
        return 0, 0  # Si no hay detección, se manda 0
```

Esto produce en el paquete UART: `Xp=0`, `Yp_coded = 0 + 100 = 100`.

El parser del TOP (`cameras.cpp:14-16`) define "no visible" como:

```cpp
inline bool is_visible(int16_t x, int16_t y) {
    return !(x == 0 && y == -Y_OFFSET);   // Y_OFFSET = 100
}
```

Es decir, el parser espera `(X=0, Y_coded=0)` → `Y = 0 - 100 = -100` para
marcar "no visible". Pero el script manda `Y_coded=100` cuando no detecta
(porque hace `Yp_coded = 0 + 100`). El parser recibe `Y = 100 - 100 = 0`,
que NO es -100, y por lo tanto **marca la pelota como visible en (0, 0)**.

**Consecuencia:** el robot persigue una pelota fantasma permanente en el origen
cada vez que no hay pelota en el frame. **Es el bug P0 #1.**

El comentario en `cameras.h:12-16` documenta el sentinel esperado como
"firmware viejo envía (X=0, Y_coded=0)" — pero el script actual manda
`Y_coded=100`, no `Y_coded=0`. **El script no concuerda con el contrato que
el parser ya implementa.**

### 2.2 Protocolo objetivo — sentinel corregido

El protocolo objetivo alinea script y parser usando `Y_coded=0` como sentinel:

| Caso | X enviado | Y_coded enviado | Y que ve el parser | Marca parser |
|------|-----------|----------------|--------------------|--------------|
| Objeto detectado, Y = +50 | valor real | 150 | +50 | visible |
| Objeto detectado, Y = 0 | valor real ≠ 0 | 100 | 0 | visible |
| Objeto detectado, Y = -100 | valor real ≠ 0 | 0 | -100 | ver nota* |
| **No detectado (sentinel)** | **0** | **0** | **-100** | **no visible** |

> *Nota: si el objeto está en Y = -100 (coordenada real extrema) y X ≠ 0, el
> parser lo marca como visible. Si X también es 0, colisiona con el sentinel.
> Para el espacio de coordenadas actual (Y ∈ [-100, 100]) el valor Y=-100 es
> el límite del clamp. **La solución correcta a largo plazo es migrar al
> protocolo con byte de fin/checksum (ver §8).**

**Regla del sentinel v1 (protocolo actual corregido):**
- No detectado: OpenMV envía `X=0, Y_coded=0` para ese objeto.
- El parser lo decodifica como `Y = 0 - 100 = -100` y `is_visible(0, -100)` = false.
- Esta convención ya está implementada en el parser (`cameras.cpp:14-16`).
  **Solo hay que corregir el script OpenMV.**

---

## 3. Crash `bytearray` — gap P0 #2

El script actual arma el paquete con:

```python
packet = [
    201, int(Xp), int(codedYp),
    202, int(Xam), int(codedYam),
    203, int(Xaz), int(codedYaz)
]
uart.write(bytearray(packet))  # enviar...:155
```

`bytearray()` en MicroPython requiere que todos los elementos sean enteros en
`[0, 255]`. Si cualquier valor cae fuera de ese rango, **lanza `ValueError` y
la cámara se detiene** (no hay `try/except` en el script).

¿Cuándo puede pasar? La homografía `transformarcoordenadas` produce `x, y` en
coordenadas físicas (cm). La corrección de perspectiva `X = x*(h-r)/h` puede
dar valores negativos o mayores a 255 si la pelota está en un ángulo extremo
o si la homografía no está bien calibrada. El clamp actual en `enviar...:99-106`
solo cubre X>200 y Y∈[-100,100]; **no protege contra X<0 ni contra
desbordamientos de la homografía antes del clamp.**

**Solución objetivo:** clampear TODOS los valores a [0,255] con `max(0, min(255, int(v)))`
inmediatamente antes de construir el `bytearray`, sin depender de que la
homografía sea correcta.

---

## 4. Convención de coordenadas y unidades

### 4.1 Sistema de coordenadas actual (script)

El script calcula coordenadas en **centímetros, en el plano del suelo,
relativas a la cámara**. La homografía `transformarcoordenadas` proyecta del
pixel (u, v) al punto físico (x, y) en cm. Luego aplica corrección de
perspectiva para obtener (X, Y) = (x, y) * (h - r) / h donde:

- `h = 18.7 cm` (altura de la cámara sobre el suelo) — `enviar...:48`
- `r = 13.5/(2*pi) cm` (radio de la pelota, derivado de circunferencia 13.5 cm) — `enviar...:49`

El eje de coordenadas del script NO está documentado. Por inspección del código,
el eje +Y apunta aproximadamente al frente de la cámara y +X al lateral. El
origen es la proyección de la cámara en el suelo.

### 4.2 Conversión a mm en el TOP

El TOP convierte unidades "del protocolo viejo" a mm con:

```cpp
constexpr float CAMERA_UNIT_TO_MM = 10.0f;   // cameras_runtime.cpp:25
```

**Este valor es un placeholder.** El comentario en `cameras_runtime.cpp:21-24`
lo dice explícitamente: "Sin calibración fina, asumimos '1 unidad ≈ 1 cm' —
alcanza para que la FSM produzca ángulos correctos y distancias razonables".

**No hay calibración real:** nadie midió pelota a 30/50/80/100 cm desde el
robot y verificó qué valor de X, Y produce el script. La escala 10.0 (1 unidad
= 10 mm = 1 cm) es una suposición razonable dado que el script calcula en cm,
pero no está verificada. Los TASK-022 y TASK-023 exigen esta calibración como
criterio de cierre.

### 4.3 Sistema de coordenadas del robot (como lo ve el TOP)

Una vez convertido a mm, el TOP usa la convención:

```
+Y = frente del robot
+X = lateral derecho del robot
origen = centro del robot
```

Fuente: `cameras_runtime.h:15-16`. La fusión en `cameras_fusion.cpp:25-29`
aplica rotación 180° para la cámara trasera (`cam_id == 1`): invierte signo de
x e y.

### 4.4 Convención de ángulos para arcos

Los arcos se reportan como ángulo + distancia (no como x, y):

```
angle_centideg = atan2(x_mm, y_mm) × (18000/π)
                 cero = frente del robot
                 +90° = izquierda
                 -90° = derecha
```

Fuente: `cameras_fusion.cpp:99-100`.

> **Inconsistencia de signo lateral**: la convención en `cameras_fusion.h:40`
> dice "+x lateral", y `atan2(x, y)` con +y=frente da:
> - +x a la derecha → atan2(+x, +y) > 0 para objeto a la derecha.
> Pero el comentario dice "+90° = izquierda". Verificar con test en hardware.
> NO implementado el test de este signo.

---

## 5. Diferencia frontal vs trasera — por qué hacen falta 2 programas

El script actual es **1 programa genérico sin identificación de cámara**. Las
diferencias que requieren programas distintos o parámetros distintos son:

| Aspecto | Cámara frontal | Cámara trasera |
|---------|---------------|----------------|
| **Montaje físico** | Apunta al frente del robot | Apunta al atrás del robot |
| **Corrección de imagen** | `set_hmirror(True), set_vflip(True)` (actual) | Puede requerir valores distintos según cómo esté montada |
| **Homografía** | Calibrada para la posición y ángulo del montaje frontal | Calibrada para la posición y ángulo del montaje trasero (valores de H distintos) |
| **FOV útil** | El frente del robot; pelota más frecuente aquí | El atrás; útil para defender / ver pelota detrás |
| **Rotación para fusión** | Coords se usan directo (`cam_id=0`) | Coords se rotan 180° en el TOP (`cam_id=1`, `cameras_fusion.cpp:25-29`) |
| **Thresholds LAB** | En principio iguales (mismo tipo de sensor) | En principio iguales |
| **Exposición** | Puede diferir si hay diferencias de iluminación por montaje | Puede diferir |

**El problema de usar 1 script genérico:** la homografía actual
(`enviar...:67-75`) es una sola matriz H, calculada (presumiblemente) para UNA
cámara. Si se carga el mismo script en ambas, la segunda cámara usa la
homografía equivocada y las coordenadas son incorrectas. Cada cámara necesita
su propia matriz H calibrada en posición.

**Estado actual de la homografía:** el comentario en la función
`transformarcoordenadas` dice "(ajustar)" — `enviar...:67`. La homografía
actual es un placeholder de desarrollo, no está calibrada para el robot de
competencia. La calibración de la homografía es parte de TASK-022.

---

## 6. Ejemplos byte-a-byte (protocolo actual v1, NO objetivo)

Los siguientes ejemplos corresponden al protocolo actual de 9 bytes, con el
sentinel **corregido** (objetivo: usar `Y_coded=0` para "no detectado").
Hex decimal; se muestra el array de bytes que llega al Teensy.

**Ejemplo A — Pelota visible en (X=80, Y=+30), arcos no visibles**
```
Script calcula: Xp=80, Yp=30 → codedYp = 30+100 = 130
Sin arco amarillo: Xam=0, Yam=0 → codedYam=0  (SENTINEL)
Sin arco azul:     Xaz=0, Yaz=0 → codedYaz=0  (SENTINEL)

Paquete decimal: [201, 80, 130, 202, 0, 0, 203, 0, 0]
Paquete hex:     C9  50  82   CA  00 00  CB  00 00
                 ^^  ^^  ^^   ^^            ^^
                 H1  Xp  Ypc  H2  Xam Yamc H3  Xaz Yazc
```

Parser TOP decodifica:
- ball: x=80, y=130-100=30 → is_visible(80, 30) = true ✓
- goal_yellow: x=0, y=0-100=-100 → is_visible(0, -100) = false ✓
- goal_blue:   x=0, y=0-100=-100 → is_visible(0, -100) = false ✓

**Ejemplo B — Nada visible (cámara tapada o campo vacío)**
```
Xp=0, codedYp=0 (SENTINEL)
Xam=0, codedYam=0 (SENTINEL)
Xaz=0, codedYaz=0 (SENTINEL)

Paquete: [201, 0, 0, 202, 0, 0, 203, 0, 0]
Hex:      C9  00 00  CA  00 00  CB  00 00
```

Parser TOP: todos is_visible = false → ball_visible=false, goal_*_visible=false ✓

**Ejemplo C — Arco amarillo visible, pelota y azul no visibles**
```
Xp=0, codedYp=0 (SENTINEL pelota)
Xam=120, Yam=-20 → codedYam = -20+100 = 80
Xaz=0, codedYaz=0 (SENTINEL arco azul)

Paquete: [201, 0, 0, 202, 120, 80, 203, 0, 0]
Hex:      C9  00 00  CA  78   50  CB  00 00
```

Parser TOP:
- ball: is_visible(0, -100) = false ✓
- goal_yellow: x=120, y=80-100=-20 → is_visible(120, -20) = true ✓
- goal_blue: is_visible(0, -100) = false ✓

**Ejemplo D — BUG ACTUAL (antes de la corrección)**
Con el script sin corregir:
```
Sin pelota: procesar_blob retorna (0, 0) → codedYp = 0+100 = 100
Paquete: [201, 0, 100, ...]
Parser: x=0, y=100-100=0 → is_visible(0, 0) = true → ¡FANTASMA!
```
Esto es el bug P0 documentado en TASK-022 y research/completed/2026-05-18-*:63.

---

## 7. Reglas de interpretación obligatorias para el TOP

### 7.1 Detección de "no visible" sin ambigüedad

El TOP ya implementa correctamente estas reglas en `cameras.cpp` asumiendo que
el script envía el sentinel correcto:

1. `is_visible(x, y)` retorna `false` si y solo si `x == 0 && y == -100`
   (después de restar el offset). Ver `cameras.cpp:14-16`.
2. El TOP **no debe** interpretar `(x=0, y=0)` como "no visible" — esa posición
   puede ser una detección real cerca del centro del frame.
3. El TOP **no debe** interpretar `x=0` solo como no visible — es una columna
   válida del frame.

### 7.2 Qué es "stale" (dato viejo)

```cpp
constexpr uint32_t CAMERA_TIMEOUT_MS = 1000;   // cameras_runtime.cpp:16
```

Si una cámara no manda ningún packet en 1000 ms, `camera_alive()` retorna
`false`. La función `recompute_fused` recibe el flag `front_alive`/`back_alive`
y si está caída, sus datos se ignoran aunque el último packet decía visible=true.
El dato puede ser stale si la cámara crasheó (bug bytearray) o si la UART se
desconectó. A 30 Hz, 1 segundo = ~30 packets perdidos — margen holgado.

Los callers del TOP que usan `cameras_ball_visible()` deben saber que esta
función ya incorpora el timeout: no necesitan agregar su propio watchdog.

### 7.3 Cómo se fusiona

La fusión es responsabilidad de `cameras_fusion.cpp`. Reglas vigentes:

- Ambas cámaras ven objeto → promedio ponderado con confianza fija
  `CONF_SINGLE_CAMERA = 80.0` cada una → `confidence = 95` (consenso).
- Solo una ve → esa, `confidence = 80`.
- Ninguna ve → `visible = false`, `confidence = 0`.
- Cámara marcada como caída (watchdog) → sus datos se ignoran aunque el último
  packet dijera visible.

**Limitación conocida (NO implementado):** la fusión usa confianza fija (80)
para ambas cámaras porque el protocolo actual no envía área del blob. El
protocolo objetivo incluye `pixels_count` para que la confianza sea
proporcional al área del blob. Hasta entonces la fusión trata ambas cámaras
como iguales.

### 7.4 Escala y unidades de salida

| Getter TOP | Unidad de salida | Factor de conversión |
|-----------|-----------------|---------------------|
| `cameras_get_ball_x_mm()` | mm | `X_script × CAMERA_UNIT_TO_MM` |
| `cameras_get_ball_y_mm()` | mm | `Y_script × CAMERA_UNIT_TO_MM` |
| `cameras_get_goal_yellow_angle_centideg()` | centideg (°×100) | calculado por `atan2` |
| `cameras_get_goal_yellow_distance_mm()` | mm | módulo del vector en mm |

`CAMERA_UNIT_TO_MM = 10.0` es placeholder. Ver §4.2.

---

## 8. Diseño de los 2 programas OpenMV (objetivo)

### 8.1 Estructura común (base compartida entre frontal y trasera)

Ambos programas comparten la misma estructura. Las diferencias entre ellos son
exactamente los parámetros marcados como [FRONTAL] vs [TRASERA].

```python
# cam_frontal.py / cam_trasera.py
# ============================================================
# PARÁMETROS DE ESTE SCRIPT (los únicos que difieren entre cámaras)
# ============================================================
CAM_ID = 0          # 0 = frontal, 1 = trasera   [FRONTAL: 0 / TRASERA: 1]
UART_PORT = 3       # [FRONTAL: UART3 / TRASERA: UART3 también — OpenMV
                    # solo tiene UART3 disponible en H7; el multiplexado
                    # físico determina a qué Teensy Serial va]
UART_BAUD = 19200

# Exposición fija (medir en cancha antes de Incheon):
EXPOSURE_US = 37000     # [CALIBRAR: valor de ejemplo, ajustar en cancha]

# Corrección geométrica:
HMIRROR = True      # [FRONTAL: True / TRASERA: True o False según montaje]
VFLIP   = True      # [FRONTAL: True / TRASERA: True o False según montaje]

# Homografía (calibrar para CADA cámara en su posición real):
H_MATRIX = [        # [DISTINTO POR CÁMARA — calibrar con herramienta]
    [ 4.49341044e-02, -9.48228474e-01,  7.78932109e+02],
    [-2.39913185e+00, -5.65934886e-02,  3.91128921e+02],
    [-1.81344856e-03,  1.15408531e-01,  1.00000000e+00]
]
# La homografía actual es placeholder de desarrollo. Calibrar con
# tablero de ajedrez o puntos conocidos en la cancha.

# Dimensiones físicas:
CAM_HEIGHT_CM = 18.7       # altura cámara sobre el suelo [MEDIR para cada robot]
BALL_RADIUS_CM = 13.5 / (2 * 3.14159)  # radio pelota de la circunferencia

# Thresholds LAB (calibrar en la cancha de competencia con la iluminación real):
NARANJA_THRESHOLD = (21, 67, 18, 79, -32, 127)   # pelota naranja [CALIBRAR]
AMARILLO_THRESHOLD = (17, 70, -27, 14, 38, 111)  # arco amarillo  [CALIBRAR]
AZUL_THRESHOLD = (4, 36, -13, 57, -64, -4)       # arco azul      [CALIBRAR]

# Filtros de área mínima (subir para reducir ruido):
NARANJA_PIXELS_MIN = 20     # [actual: 7 — muy bajo, detecta ruido]
AMARILLO_PIXELS_MIN = 300
AZUL_PIXELS_MIN = 300

# ============================================================
# INICIALIZACIÓN (sin auto-WB, sin auto-gain — CRÍTICO)
# ============================================================
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)

sensor.set_auto_whitebal(False)   # DEBE ser False — auto-WB invalida thresholds LAB
sensor.set_auto_gain(False)       # DEBE ser False — auto-gain invalida thresholds LAB
sensor.set_auto_exposure(False, exposure_us=EXPOSURE_US)  # fija exposición

sensor.set_hmirror(HMIRROR)
sensor.set_vflip(VFLIP)

sensor.skip_frames(time=500)      # dar tiempo al sensor para estabilizarse

uart = UART(UART_PORT, UART_BAUD)

# ============================================================
# SENTINEL — "no detectado"
# ============================================================
# Regla: si no hay blob, enviar X=0, Y_coded=0.
# El parser del TOP (cameras.cpp:14-16) interpreta (X=0, Y=Y_coded-100=-100)
# como "no visible".
# NUNCA enviar X=0, Y_coded=100 (que es lo que hace el script actual —
# ese es el bug P0 del fantasma).
SENTINEL_X = 0
SENTINEL_Y_CODED = 0

# ============================================================
# FUNCIÓN DE TRANSFORMACIÓN (con clamp anti-crash)
# ============================================================
def transformar(u, v):
    H = H_MATRIX
    denom = H[2][0]*u + H[2][1]*v + H[2][2]
    if abs(denom) < 1e-6:
        return SENTINEL_X, SENTINEL_Y_CODED  # degenerate — reportar no-detectado
    x = (H[0][0]*u + H[0][1]*v + H[0][2]) / denom
    y = (H[1][0]*u + H[1][1]*v + H[1][2]) / denom
    # Corrección de perspectiva
    X = x * (CAM_HEIGHT_CM - BALL_RADIUS_CM) / CAM_HEIGHT_CM
    Y = y * (CAM_HEIGHT_CM - BALL_RADIUS_CM) / CAM_HEIGHT_CM
    # Clamp al rango físico plausible
    X = max(-127, min(200, X))
    Y = max(-100, min(100, Y))
    Y_coded = int(Y) + 100   # Y_coded ∈ [0, 200]
    X_int   = int(X)
    # Si X=0 e Y_coded=0, no es sentinel — ajustar X a 1 para no confundir
    # (solo ocurre si la pelota está exactamente en X=0, Y=-100, que es el
    # límite del clamp; rarísimo en práctica)
    if X_int == 0 and Y_coded == 0:
        X_int = 1
    # CLAMP final a uint8 — anti-crash en bytearray
    X_int   = max(0, min(255, X_int))
    Y_coded = max(0, min(255, Y_coded))
    return X_int, Y_coded

def procesar_blob(blobs):
    if not blobs:
        return SENTINEL_X, SENTINEL_Y_CODED
    blob = max(blobs, key=lambda b: b.pixels())
    return transformar(blob.cx(), blob.cy())

# ============================================================
# LOOP PRINCIPAL
# ============================================================
while True:
    img = sensor.snapshot()

    naranja_blobs  = img.find_blobs([NARANJA_THRESHOLD],
                                     pixels_threshold=NARANJA_PIXELS_MIN,
                                     area_threshold=NARANJA_PIXELS_MIN, merge=True)
    amarillo_blobs = img.find_blobs([AMARILLO_THRESHOLD],
                                     pixels_threshold=AMARILLO_PIXELS_MIN,
                                     area_threshold=AMARILLO_PIXELS_MIN, merge=True)
    azul_blobs     = img.find_blobs([AZUL_THRESHOLD],
                                     pixels_threshold=AZUL_PIXELS_MIN,
                                     area_threshold=AZUL_PIXELS_MIN, merge=True)

    Xp,  Ypc  = procesar_blob(naranja_blobs)
    Xam, Yamc = procesar_blob(amarillo_blobs)
    Xaz, Yazc = procesar_blob(azul_blobs)

    # Todos los valores ya son uint8 por el clamp en transformar/procesar_blob
    packet = bytearray([201, Xp, Ypc, 202, Xam, Yamc, 203, Xaz, Yazc])
    uart.write(packet)
    # NO print() en producción — consume ~3ms y reduce fps
```

### 8.2 Diferencias concretas entre cam_frontal.py y cam_trasera.py

| Parámetro | cam_frontal.py | cam_trasera.py | Estado |
|-----------|---------------|----------------|--------|
| `CAM_ID` | `0` | `1` | Informativo (el TOP distingue por puerto UART, no por este campo) |
| `H_MATRIX` | Calibrada montada en posición frontal | Calibrada montada en posición trasera | **NO implementado — calibrar** |
| `HMIRROR` | `True` (actual) | **Verificar según montaje físico** | Pendiente verificación |
| `VFLIP` | `True` (actual) | **Verificar según montaje físico** | Pendiente verificación |
| `CAM_HEIGHT_CM` | Medir en el robot real | Medir en el robot real | **NO medido — placeholder 18.7** |
| `EXPOSURE_US` | Medir en cancha | Medir en cancha | **NO medido — comentado en script** |

La inversión de coordenadas para la cámara trasera (rotación 180°) la hace
el TOP en `cameras_fusion.cpp:25-29`, NO la cámara. La cámara trasera NO debe
invertir sus propias coordenadas.

### 8.3 Diseño del parser TOP — qué es lógica pura testeable host-native

El parser `cameras.cpp` / `CameraParser::feed()` ya está bien diseñado como
lógica pura (sin dependencia de Arduino). Es completamente testeable
host-native. Lo que falta:

1. **Test del sentinel:** inyectar bytes `[201, 0, 0, 202, 0, 0, 203, 0, 0]`
   → verificar `ball_visible == false`. **NO implementado** (TASK-023).
2. **Test del fantasma:** inyectar bytes del script SIN CORREGIR
   `[201, 0, 100, ...]` → verificar que `ball_visible == true` (documentar el
   bug). **NO implementado**.
3. **Test de resync:** inyectar bytes con header faltante → verificar que
   `resync_events` incrementa y que el parser recupera el siguiente packet
   bien. **Probablemente NO implementado** (no aparece en test/).
4. **Test de datos=201/202/203:** inyectar un packet donde `Xp=201` →
   verificar comportamiento (actualmente causaría desincronización).

`cameras_fusion.cpp` ya tiene tests (`pio test -e test_native -f test_cameras_fusion`)
per la evaluación crítica dice que existen 16 tests. Los tests de detección de
no-visible del parser no están cubiertos.

---

## 9. Gaps citados — qué falta para "2 cámaras operativas, no demo"

### Gap 1 (P0): Sentinel roto → fantasma permanente

- **Evidencia:** `enviar...:81-82` retorna `(0,0)` → `Y_coded=100` ≠ sentinel esperado por `cameras.cpp:14-16`.
- **Fix:** en `procesar_blob`, cuando no hay blobs retornar `(SENTINEL_X=0, SENTINEL_Y_CODED=0)` directamente.
- **Riesgo sin fix:** robot persigue pelota fantasma en origen permanentemente. Inutilizable.
- **Esfuerzo:** 5 minutos de código, 15 minutos de test.

### Gap 2 (P0): Crash `bytearray` con coordenadas negativas

- **Evidencia:** `enviar...:155` sin clamp previo. La homografía puede dar X<0.
- **Fix:** `max(0, min(255, int(v)))` en TODOS los campos antes de `bytearray`.
- **Riesgo sin fix:** cámara se detiene en partido cuando la pelota está en posición extrema.
- **Esfuerzo:** 5 minutos de código.

### Gap 3 (P0): Auto-WB y auto-gain encendidos

- **Evidencia:** `enviar...:31-32` tiene `set_auto_whitebal(True)` y `set_auto_gain(True)`. La línea de `set_auto_exposure` está **comentada** (`enviar...:39`).
- **Fix:** apagar los 3 autos y fijar `exposure_us` medido en la cancha de Incheon.
- **Riesgo sin fix:** los thresholds LAB calibrados en Salta no funcionan en Incheon (iluminación distinta). La pelota "desaparece" o hay falsos positivos.
- **Esfuerzo:** apagar los autos = 2 líneas; medir el exposure = 30 min en cancha.

### Gap 4 (P0): 1 solo script genérico, sin homografía calibrada

- **Evidencia:** `enviar...:67` tiene `# (ajustar)` en la homografía. Un único script para ambas cámaras.
- **Fix:** calibrar homografía para cada cámara en su posición de montaje. Crear `cam_frontal.py` y `cam_trasera.py`.
- **Riesgo sin fix:** coordenadas incorrectas → la cámara trasera ve el mundo con la homografía de la frontal → distancias y ángulos erróneos → fusión da resultado incorrecto.
- **Esfuerzo:** calibración de homografía = 2–4 horas por cámara con procedimiento correcto. Crear los 2 scripts = 1 hora.

### Gap 5 (P1): `CAMERA_UNIT_TO_MM = 10.0` placeholder

- **Evidencia:** `cameras_runtime.cpp:25`, comentario "TODO: calibrar".
- **Fix:** medir pelota a 30/50/80/100 cm, registrar X reportado, calcular factor.
- **Riesgo sin fix:** distancias en mm incorrectas → la FSM de CENTRAL usa umbrales de approach/kick calibrados en mm que no corresponden a la realidad.
- **Esfuerzo:** 30 min de medición + 10 min de ajuste.

### Gap 6 (P1): `pixels_threshold=7` para pelota (ruido)

- **Evidencia:** `enviar...:120`.
- **Fix:** subir a 20–50 según pruebas. Valor 7 detecta ruido como pelota.
- **Riesgo sin fix:** falsos positivos → el robot ve una pelota que no existe.
- **Esfuerzo:** 10 min de ajuste + 15 min de test.

### Gap 7 (P1): Sin tests del parser de cámara

- **Evidencia:** evaluación crítica:102 "El bug P0 del sentinel de cámara está fuera de los tests".
- **Fix:** agregar tests host-native para el sentinel, el fantasma, el resync y el crash (ver §8.3).
- **Riesgo sin fix:** el bug puede reaparecer sin que los tests lo detecten.
- **Esfuerzo:** 2–3 horas de tests en C++.

### Gap 8 (P2): Sin byte de fin / checksum en protocolo

- **Evidencia:** `cameras.h:20-30` documenta el protocolo de 9 bytes sin CRC.
- **Fix a largo plazo:** migrar al protocolo de `proto.h` (START + LEN + TYPE + SEQ + PAYLOAD + CRC16 + END). Requiere actualizar el parser del TOP y los scripts OpenMV.
- **Riesgo sin fix:** datos que coinciden con headers (201/202/203) pueden causar desincronización esporádica (bug R6 en `cameras.h:8`).
- **Esfuerzo:** ~8 horas (script + parser + tests). Es un cambio breaking del protocolo.
- **Recomendación para Incheon:** posponer a post-Incheon. Resolver gaps 1–5 primero.

### Gap 9 (P2): Confianza fija en la fusión (no proporcional al área del blob)

- **Evidencia:** `cameras_fusion.cpp:11-12`, `CONF_SINGLE_CAMERA = 80.0f` fijo.
- **Fix:** agregar `pixels_count` al paquete de la cámara; usar área del blob como peso.
- **Riesgo sin fix:** una detección de 5 px y una de 500 px tienen el mismo peso en la fusión.
- **Esfuerzo:** requiere extender el protocolo (breaking change).
- **Recomendación:** posponer a post-Incheon junto con Gap 8.

---

## 10. Checklist de criterio de cierre (TASK-022)

Estos son los criterios de cierre de TASK-022 en términos de este contrato:

- [ ] `cam_frontal.py`: sentinel `(0, SENTINEL_Y_CODED=0)` cuando no hay blob → TOP marca `ball_visible=false`.
- [ ] `cam_trasera.py`: ídem.
- [ ] Ninguna cámara crashea con bytearray (test de inyección de coordenadas extremas).
- [ ] `set_auto_whitebal(False)`, `set_auto_gain(False)`, `set_auto_exposure(False, exposure_us=VALOR_MEDIDO)` en ambos scripts.
- [ ] `VALOR_MEDIDO` documentado en el journal con foto del setup de medición.
- [ ] Homografía calibrada para `cam_frontal.py` en posición de montaje real.
- [ ] Homografía calibrada para `cam_trasera.py` en posición de montaje real.
- [ ] `CAMERA_UNIT_TO_MM` calibrado (pelota a 30/50/80/100 cm, error < 10%).
- [ ] Tests host-native del parser: sentinel OK, no-fantasma, resync.
- [ ] `pixels_threshold` de pelota ≥ 20 (no ruido).

---

## 11. Versionado del contrato

- `contract-schema: 1` — este documento define la versión 1 del contrato de
  cámaras. Cualquier cambio de layout del paquete (agregar campos, cambiar
  offsets) **incrementa** `contract-schema` y actualiza el frontmatter.
- El protocolo actual de 9 bytes es el **"protocolo viejo"** — funciona sin
  cambios en el paquete para los gaps 1–7. Solo los gaps 8–9 requieren
  cambiar el protocolo y hacer upgrade de `contract-schema`.

## 12. Fuentes

| Archivo | Líneas relevantes |
|---------|------------------|
| `software/vision/enviar coordenadas 2 arcos y pelota` | :31-32 (auto-WB/gain), :67-75 (homografía), :80-107 (procesar_blob, sentinel), :148-155 (bytearray/packet) |
| `software/teensy/Soccer 2026/src/top/cameras.cpp` | :6-9 (constantes headers/offset), :14-16 (is_visible/sentinel), :30-94 (state machine parser) |
| `software/teensy/Soccer 2026/src/top/cameras.h` | :1-16 (decisión protocolo viejo), :20-30 (layout 9 bytes), :36-50 (CameraPacket struct) |
| `software/teensy/Soccer 2026/src/top/cameras_runtime.cpp` | :16 (CAMERA_TIMEOUT_MS), :21-25 (CAMERA_UNIT_TO_MM placeholder), :32-33 (g_parser_front/back), :59-77 (cam_obs_to_robot_frame calls) |
| `software/teensy/Soccer 2026/src/top/cameras_runtime.h` | :15-16 (convención +y=frente) |
| `software/teensy/Soccer 2026/src/top/config_top.h` | :43-55 (UART asignaciones y bauds) |
| `software/teensy/Soccer 2026/src/shared/cameras_fusion.cpp` | :11-12 (CONF_SINGLE/CONSENSUS), :15-32 (cam_obs_to_robot_frame + rotación 180°), :34-71 (fuse_ball_dual), :73-107 (fuse_goal_dual + atan2) |
| `software/teensy/Soccer 2026/src/shared/cameras_fusion.h` | :38-46 (GoalFused, convención ángulo) |
| `team-tasks/2026-05-18-task-022-camara-operativa.md` | completo |
| `research/completed/2026-05-18-estado-firmware-robot-evaluacion-critica.md` | :49-51 (tabla cámaras), :62-65 (P0 sentinel/crash/auto-WB) |
| `docs/firmware/CONTRATO-DATOS-DOWN.md` | plantilla de estilo y nivel de precisión esperado |
