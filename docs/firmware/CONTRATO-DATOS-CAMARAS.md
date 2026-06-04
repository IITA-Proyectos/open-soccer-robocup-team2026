---
title: "Contrato de datos de las cámaras OpenMV — protocolo, diseño de programas y gaps (v2)"
date: 2026-06-03
author: "Claude (Anthropic - claude-sonnet-4-6 v1; Claude Opus 4.8 v2)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M, Anthropic)"
status: draft
tags: [comunicacion, firmware, protocolo, contrato, camaras, openmv, top, vision]
robot: ambos
area: vision
tipo: protocolo
contract-schema: 2
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

## ⚠️ CHANGELOG v2 (contract-schema: 2) — 2026-06-03 — TOCA EL WIRE

> **El packet pasó de 9 a 11 bytes y cambió la semántica de X y del sentinel.**
> **Re-flashear AMBAS cámaras (`cam-frontal-n6.py` + `cam-trasera-n6.py`) Y el TOP
> en el MISMO deploy, y validar en banco.** NO es regresión-segura por separado:
> una punta v1 contra la otra v2 NO decodifica (longitud, offset y sentinel
> distintos). Resuelve la decisión A/B de
> `research/in-progress/2026-06-03-eje-x-codificacion-asimetrica-vision.md` → **Opción A**.

Tres cambios, en una sola revisión coherente del packet:

1. **Eje X simétrico a Y (Opción A).** Antes X se mandaba sin offset (0..200) e Y
   con offset (+100); la pelota a la **izquierda** (X<0) se clampeaba a 0 y el TOP
   la leía "al frente" — la mitad izquierda del FOV se perdía. Ahora **X se codifica
   igual que Y**: `X_coded = X + 100` (X ∈ [-100,100] → 0..200) y el TOP hace
   `ball_x = byte - 100`. La izquierda ya es representable.

2. **Sentinel inequívoco.** Antes "no detectado" era `(X=0, Y_coded=0)` → frágil:
   colisionaba con un objeto real en `(X=0, Y=-100)` (borde del FOV) y `X=0` es una
   columna válida. Ahora el sentinel es **un byte coded == 255** (`0xFF`) en X y/o Y.
   Como las coords reales se clampean a **[0,200]**, el valor 255 es **inalcanzable
   desde una detección** → cero colisión. Un objeto se marca **no visible** si
   **cualquiera** de sus dos bytes coded es 255.

3. **Integridad (CRC + fin de trama).** Se agregan 2 bytes al final: **CRC8** (XOR
   de los 9 bytes de datos, bytes 0..8) y **END = 254** (`0xFE`). El parser valida
   los 3 headers en posición fija, el END en posición fija y el CRC; si algo falla,
   **descarta el frame** y cuenta `crc_errors` / `resync_events`. Detecta bit-flips
   del enlace y datos que coinciden con un header (bug R6).

**Por qué no hay colisión de bytes:** coords coded ∈ [0,200]; headers = 201/202/203;
END = 254; SENTINEL = 255. Ningún dato de coordenada real puede valer 201/202/203/254/255.
El CRC es el único byte libre [0,255]; por eso se valida **antes** de aceptar el packet.

**Fuente del v2:** `src/top/cameras.h` (constantes públicas + `cam_crc8`),
`src/top/cameras.cpp` (state machine 11 bytes), ambos `*-n6.py` (armado + `crc8`),
`test/test_cameras_parser/test_main.cpp` (17 tests host: izq/centro/der, sentinel,
CRC malo, END malo, dato==header).

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
| Puerto Teensy | Serial3 | **Serial5** (soldada ahí, banco 2026-05-31) |
| Pines Teensy 4.0 | RX=15, TX=14 | **RX=21, TX=20** |
| Conector en PCB TOP | U8 "UART-CAMERA1" | (pin 21 — confirmar conector con Enzo) |
| Baud rate | 19200 8N1 | 19200 8N1 |
| Constante firmware | `UART_CAMERA1_BAUD = 19200` | `UART_CAMERA2_BAUD = 19200` |

Fuente: `src/top/cameras_runtime.cpp` (código vivo).

> **✅ Actualizado 2026-05-31 (TASK-204):** la cámara trasera quedó **soldada en
> Serial5 (RX pin 21)** — confirmado en banco (`diag_top_cameras`, FORMATO OK).
> `cameras_runtime.cpp` lee la trasera en `Serial5`.
>
> **🔧 Fix 2026-06-02 (vale sobre lo de arriba):** el UART **TOP→CENTRAL** NO va por
> Serial7. El Teensy 4.0 NO expone Serial7 (28/29) en el borde (son pads SMD traseros);
> ponerlo ahí dejaba al TOP sin llegar a la CENTRAL. El enlace real es **Serial4 (16/17)**:
> `src/top/comm_central.cpp` usa **`Serial4`** (cable TOP pin17/TX4 → CENTRAL pin28/RX7,
> que en el 4.1 sí es Serial7). En el TOP, Serial2 (7/8) = módulo COMM (árbitro).

### 1.2 Protocolo VIGENTE v2 — 11 bytes con CRC + END (contract-schema 2)

Cada cámara manda un paquete de **11 bytes** por iteración del loop:

```
byte  0:  201        (HEADER1 — sync pelota)
byte  1:  Xp_coded   (X pelota + 100, uint8, datos ∈ [0,200] | 255 = SENTINEL)
byte  2:  Yp_coded   (Y pelota + 100, uint8, datos ∈ [0,200] | 255 = SENTINEL)
byte  3:  202        (HEADER2 — sync arco amarillo)
byte  4:  Xam_coded  (X arco amarillo + 100)
byte  5:  Yam_coded  (Y arco amarillo + 100)
byte  6:  203        (HEADER3 — sync arco azul)
byte  7:  Xaz_coded  (X arco azul + 100)
byte  8:  Yaz_coded  (Y arco azul + 100)
byte  9:  CRC8       (XOR de los bytes 0..8 inclusive)
byte 10:  254        (END — fin de trama, 0xFE)
```

Fuente: `cameras.h` (layout + `CAM_*` constantes + `cam_crc8`), `cameras.cpp`
(state machine), `cam-frontal-n6.py` / `cam-trasera-n6.py` (`crc8` + armado).

- **X e Y se codifican SIMÉTRICO:** `coded = valor + 100`, con `valor ∈ [-100,100]`.
  El TOP recupera `valor = byte - 100` en **ambos** ejes. `+x = derecha`, `-x = izquierda`.
- **SENTINEL "no detectado" = byte coded == 255.** Inalcanzable desde datos reales
  (clamp a [0,200]). Un objeto es "no visible" si X_coded **o** Y_coded es 255.
- **CRC8 = XOR de los 9 bytes de datos.** El parser descarta el frame si no matchea
  (cuenta `crc_errors()`). No es CRC16 a propósito: 1 byte mantiene el packet corto
  y el parser trivial de auditar.
- **END = 254** valida el fin de trama en posición fija (segundo guard contra
  desalineación además de los 3 headers).

**Por qué los headers como datos ya no rompen (bug R6 cerrado):** coords coded ∈
[0,200], así que **ningún dato real puede valer 201/202/203/254/255**. Aunque por un
bit-flip un dato quede igual a un header, el CRC del frame no matchea → se descarta.

#### 1.2-bis Protocolo LEGACY v1 — 9 bytes (histórico, ya NO se usa)

> ⚠️ **Obsoleto desde 2026-06-03.** Se deja como referencia de migración. Una cámara
> o un TOP que sigan en v1 NO interoperan con v2.

```
[201, Xp, Yp_coded, 202, Xam, Yam_coded, 203, Xaz, Yaz_coded]   (9 bytes)
```

Defectos de v1 que v2 corrige: (1) **X sin offset** → la izquierda (X<0) colapsaba a
0 = "al frente"; (2) **sentinel `(X=0,Y_coded=0)`** colisionaba con el borde del FOV
y `X=0` era columna válida; (3) **sin CRC ni byte de fin** → un dato == header o un
bit-flip desincronizaba (bug R6, `cameras.h` viejo).

---

## 2. Sentinel "no detectado" — el gap P0 central

> ⚠️ **SECCIÓN HISTÓRICA (formato v1, contract-schema 1).** Lo descrito en §2.1 es el
> sentinel **OBSOLETO** del packet de 9 bytes (X asimétrico [0..200], sentinel
> posicional `(X=0, Y_coded=0)`). **El formato VIGENTE es v2** (11 bytes: 9 datos +
> CRC8 + END=254, X e Y simétricos con offset +100, sentinel = byte coded **255**) —
> ver §1.2 y §2.2 y, como fuente de verdad, `src/top/cameras.h` (`CAM_SENTINEL=255`,
> `CAM_END_BYTE=254`, `CAM_PACKET_LEN=11`). Se conserva §2.1 para documentar el bug
> P0 del fantasma que motivó el rediseño; NO implementarlo.

### 2.1 Estado actual (inconsistencia) — HISTÓRICO v1

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

### 2.2 Sentinel VIGENTE v2 — byte coded == 255 (inequívoco)

El v2 deja de inferir "no detectado" de la posición y usa un **byte reservado**:

| Caso | X_coded enviado | Y_coded enviado | Decodifica parser | Marca parser |
|------|-----------------|-----------------|-------------------|--------------|
| Detectado, X=+30 Y=+50 | 130 | 150 | (+30, +50) | **visible** |
| Detectado al centro, X=0 Y=0 | 100 | 100 | (0, 0) | **visible** |
| Detectado esquina, X=-100 Y=-100 | 0 | 0 | (-100, -100) | **visible** ✓ |
| **No detectado (SENTINEL)** | **255** | **255** | — | **no visible** |
| No detectado (basta uno) | 255 | cualquiera | — | **no visible** |

> **Clave del v2:** como las coords reales se clampean a **[0,200]**, el valor 255
> es **inalcanzable desde una detección**. Por eso la esquina extrema `(X=-100, Y=-100)`
> —que en v1 colisionaba con el sentinel— en v2 es una detección **válida y visible**.

**Regla del sentinel v2:**
- No detectado: la cámara envía `X_coded=255, Y_coded=255` para ese objeto
  (constante `SENTINEL_CODED` en ambos `.py`; `CAM_SENTINEL` en `cameras.h`).
- El parser marca el objeto **no visible** si X_coded **o** Y_coded es 255.
- Ya **no** se usa la heurística `is_visible(x==0 && y==-100)` de v1 (era frágil).

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
                 +90° = DERECHA
                 -90° = izquierda
```

Fuente: `cameras_fusion.cpp:97-100` (+x = DERECHA).

> **✅ Signo RESUELTO (2026-05-31):** `+x = DERECHA`, `atan2(x, y)` con +y=frente ⇒
> **+90° = arco/pelota a la DERECHA del robot**. Coincide con `cameras_fusion.cpp:97-99`
> ("=> +90° = arco a la derecha") y con `docs/CONVENCION-EJES-ROBOT.md`. La nota vieja
> ("+90°=izquierda, verificar") quedó SUPERADA — el código ya está corregido.

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

## 6. Ejemplos byte-a-byte (protocolo VIGENTE v2, 11 bytes)

Cada packet termina en `[…, CRC8, 254]`. CRC8 = XOR de los 9 bytes de datos (0..8).
Los CRC de abajo están calculados; un test host (`test_crc8_known_vector`) los fija.

**Ejemplo A — Pelota a la DERECHA (X=+80, Y=+30), arcos no visibles**
```
Xp_coded  = 80+100 = 180   Yp_coded = 30+100 = 130
arcos: SENTINEL (255,255)
data    = [201,180,130, 202,255,255, 203,255,255]
CRC8    = 254 ; END = 254
Paquete = [201,180,130, 202,255,255, 203,255,255, 254, 254]   (11 B)
```
Parser: ball x=180-100=+80 (derecha), y=130-100=+30, visible ✓; arcos no visibles ✓.

**Ejemplo A2 — Pelota a la IZQUIERDA (X=-80, Y=+30) — el fix central**
```
Xp_coded = -80+100 = 20   Yp_coded = 130
data    = [201,20,130, 202,255,255, 203,255,255]   CRC8 = 94 ; END = 254
Paquete = [201,20,130, 202,255,255, 203,255,255, 94, 254]
```
Parser: ball x=20-100=**-80 (IZQUIERDA, representable)**, y=+30, visible ✓.
> En v1 este caso colapsaba a X=0 → "al frente". Ése era el bug que v2 cierra.

**Ejemplo B — Nada visible (cámara tapada o campo vacío)**
```
todo SENTINEL: data = [201,255,255, 202,255,255, 203,255,255]
CRC8 = 200 ; END = 254
Paquete = [201,255,255, 202,255,255, 203,255,255, 200, 254]
```
Parser: ball/yellow/blue todos no visibles ✓ (ningún fantasma).

**Ejemplo C — Arco amarillo visible (X=+20, Y=-20), pelota y azul no**
```
Xam_coded = 20+100 = 120   Yam_coded = -20+100 = 80
data    = [201,255,255, 202,120,80, 203,255,255]   CRC8 = 224 ; END = 254
Paquete = [201,255,255, 202,120,80, 203,255,255, 224, 254]
```
Parser: ball no visible; yellow x=+20, y=-20, visible ✓; blue no visible.

**Ejemplo D — Pelota AL CENTRO (X=0, Y=0) — visible, NO sentinel**
```
Xp_coded = 100   Yp_coded = 100
data    = [201,100,100, 202,255,255, 203,255,255]   CRC8 = 200 ; END = 254
Paquete = [201,100,100, 202,255,255, 203,255,255, 200, 254]
```
Parser: ball x=0, y=0, **visible** ✓ (en v2 el centro ya no se confunde con
"no detectado": el sentinel es 255, no 0).

**Ejemplo E — Frame CORRUPTO (bit-flip) → descartado**
```
Tomar el Ejemplo A y voltear 1 bit del byte Xp (180 → 181):
Paquete = [201,181,130, 202,255,255, 203,255,255, 254, 254]
El CRC recibido (254) ya no coincide con XOR(data') → el parser DESCARTA el
frame, NO publica, e incrementa crc_errors(). El último packet bueno queda intacto.
```

---

## 7. Reglas de interpretación obligatorias para el TOP

### 7.1 Detección de "no visible" sin ambigüedad

El TOP implementa estas reglas en `cameras.cpp` (parser v2):

1. Un objeto es **no visible** si y solo si su byte X_coded **o** su byte Y_coded
   recibido valía `255` (`CAM_SENTINEL`). Se evalúa sobre el **byte crudo**, antes
   de restar el offset. Ver `cameras.cpp` (`decode_coord` + flags `*_vis_`).
2. El TOP **no debe** interpretar `(x=0, y=0)` como "no visible" — el centro exacto
   del frame es una detección real (X_coded=100, Y_coded=100, lejos de 255).
3. El TOP **no debe** interpretar `x=0` solo como no visible — es una columna válida.
4. El TOP **solo publica** un packet cuando los 3 headers, el byte END (254) y el
   CRC8 chequearon. Un frame con CRC/END/header malo se **descarta** (cuenta
   `crc_errors()` / `resync_events()`) y NO pisa el último packet bueno.

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

> ⚠️ **SECCIÓN HISTÓRICA (formato v1, contract-schema 1).** El esqueleto Python de
> §8.1 codifica el packet **OBSOLETO de 9 bytes**: arma `bytearray([201, Xp, Ypc,
> 202, ...])` SIN CRC ni END, con X asimétrico y sentinel posicional `SENTINEL_X=0` /
> `SENTINEL_Y_CODED=0`. **El formato VIGENTE es v2** (11 bytes con CRC8 + END=254, X
> simétrico `X_coded=X+100`, sentinel = byte coded **255**). Para el layout y el
> sentinel reales ver §1.2, §6 y los scripts vivos `cam-frontal-n6.py` /
> `cam-trasera-n6.py` + `src/top/cameras.h`. El ejemplo de abajo sirve para la
> estructura del programa (init del sensor, homografía, clamp anti-crash), **NO**
> para el encoding del packet — ése es v1 y quedó superado.

### 8.1 Estructura común (base compartida entre frontal y trasera) — encoding HISTÓRICO v1

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

### Gap 8 (P2): Sin byte de fin / checksum en protocolo — ✅ RESUELTO en v2 (2026-06-03)

- **Evidencia (v1):** el protocolo de 9 bytes no tenía CRC ni byte de fin.
- **Fix aplicado (v2, contract-schema 2):** se agregó **CRC8** (XOR de los 9 bytes
  de datos) + **END=254** al packet (ahora 11 bytes). El parser descarta frames con
  CRC/END/header malo y cuenta `crc_errors()`/`resync_events()`. Se eligió CRC8 (no
  CRC16 de `proto.h`) por simplicidad: 1 byte, parser trivial, suficiente para
  bit-flips de enlace corto a 19200 baud. Ver §1.2 y `cameras.h`/`cameras.cpp`.
- **Riesgo cerrado:** datos == header (201/202/203) ya no pueden ocurrir (coords ∈
  [0,200]) y, si un bit-flip los provocara, el CRC del frame no matchea → se descarta.
- **TOCA EL WIRE:** re-flashear cámara + TOP juntos + validar en banco (ver §0 changelog).

### Gap 9 (P2): Confianza fija en la fusión (no proporcional al área del blob)

- **Evidencia:** `cameras_fusion.cpp:11-12`, `CONF_SINGLE_CAMERA = 80.0f` fijo.
- **Fix:** agregar `pixels_count` al paquete de la cámara; usar área del blob como peso.
- **Riesgo sin fix:** una detección de 5 px y una de 500 px tienen el mismo peso en la fusión.
- **Esfuerzo:** requiere extender el protocolo (breaking change).
- **Recomendación:** posponer a post-Incheon junto con Gap 8.

---

## 10. Checklist de criterio de cierre (TASK-022)

Estos son los criterios de cierre de TASK-022 en términos de este contrato:

- [x] `cam-frontal-n6.py`: sentinel `(255,255)` cuando no hay blob → TOP marca `ball_visible=false`. **(v2)**
- [x] `cam-trasera-n6.py`: ídem. **(v2)**
- [x] X codificado simétrico a Y (`X_coded=X+100`) → pelota a la izquierda representable. **(v2)**
- [x] CRC8 + END en el packet; parser descarta frames corruptos. **(v2)**
- [ ] Ninguna cámara crashea con bytearray (test de inyección de coordenadas extremas).
- [ ] `set_auto_whitebal(False)`, `set_auto_gain(False)`, `set_auto_exposure(False, exposure_us=VALOR_MEDIDO)` en ambos scripts.
- [ ] `VALOR_MEDIDO` documentado en el journal con foto del setup de medición.
- [ ] Homografía calibrada para `cam_frontal.py` en posición de montaje real.
- [ ] Homografía calibrada para `cam_trasera.py` en posición de montaje real.
- [ ] `CAMERA_UNIT_TO_MM` calibrado (pelota a 30/50/80/100 cm, error < 10%).
- [x] Tests host-native del parser: izq/centro/der, sentinel, CRC malo, END malo, resync. **(v2, 17 tests)**
- [ ] `pixels_threshold` de pelota ≥ 20 (no ruido).

---

## 11. Versionado del contrato

- `contract-schema: 2` (VIGENTE, 2026-06-03) — packet de **11 bytes**: X simétrico
  a Y (`X_coded=X+100`), sentinel `255`, CRC8 + END=254. **Breaking vs v1:** cambia
  longitud, offset de X y semántica del sentinel → re-flashear cámara + TOP juntos
  y validar en banco (§0 changelog). Resuelve la Opción A de
  `research/in-progress/2026-06-03-eje-x-codificacion-asimetrica-vision.md` y el Gap 8.
- `contract-schema: 1` (HISTÓRICO) — packet de 9 bytes, X sin offset, sentinel
  `(0,0)`, sin CRC. Defectos en §1.2-bis.
- **Regla:** cualquier cambio de layout (longitud, offsets, sentinel, CRC)
  **incrementa** `contract-schema` y actualiza el frontmatter, y exige re-flashear
  ambas puntas en el mismo deploy.

## 12. Fuentes

| Archivo | Líneas relevantes |
|---------|------------------|
| `hardware/electronics/cameraFront-pack/firmware/openmv/cam-frontal-n6.py` | `crc8`, `transformar` (X_coded=X+100, clamp [0,200]), `SENTINEL_CODED=255`, armado packet 11 B |
| `hardware/electronics/cameraBack-pack/firmware/openmv/cam-trasera-n6.py` | ídem frontal (la trasera NO rota coords; lo hace el TOP) |
| `software/teensy/Soccer 2026/src/top/cameras.cpp` (v2) | `decode_coord`, state machine 11 bytes con `READ_CRC`/`WAIT_END`, validación CRC8 + descarte de frame corrupto |
| `software/teensy/Soccer 2026/src/top/cameras.h` (v2) | constantes `CAM_*`, `cam_crc8`, layout 11 bytes, `CameraPacket`, `crc_errors()` |
| `software/teensy/Soccer 2026/test/test_cameras_parser/test_main.cpp` (v2) | 17 tests host: izq/centro/der, sentinel, CRC malo, END malo, dato==header |
| `software/teensy/Soccer 2026/src/top/cameras_runtime.cpp` | :16 (CAMERA_TIMEOUT_MS), :21-25 (CAMERA_UNIT_TO_MM placeholder), :32-33 (g_parser_front/back), :59-77 (cam_obs_to_robot_frame calls) |
| `software/teensy/Soccer 2026/src/top/cameras_runtime.h` | :15-16 (convención +y=frente) |
| `software/teensy/Soccer 2026/src/top/config_top.h` | :43-55 (UART asignaciones y bauds) |
| `software/teensy/Soccer 2026/src/shared/cameras_fusion.cpp` | :11-12 (CONF_SINGLE/CONSENSUS), :15-32 (cam_obs_to_robot_frame + rotación 180°), :34-71 (fuse_ball_dual), :73-107 (fuse_goal_dual + atan2) |
| `software/teensy/Soccer 2026/src/shared/cameras_fusion.h` | :38-46 (GoalFused, convención ángulo) |
| `team-tasks/2026-05-18-task-022-camara-operativa.md` | completo |
| `research/completed/2026-05-18-estado-firmware-robot-evaluacion-critica.md` | :49-51 (tabla cámaras), :62-65 (P0 sentinel/crash/auto-WB) |
| `docs/firmware/CONTRATO-DATOS-DOWN.md` | plantilla de estilo y nivel de precisión esperado |
