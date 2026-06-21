# Protocolo v2 cámara OpenMV → TOP — byte a byte

> FUENTE DE VERDAD = `src/top/cameras.h:33-57` (contract-schema 2). Toca el WIRE: cambiarlo exige
> re-flashear AMBas cámaras + el TOP en el MISMO deploy + validar en banco. La verdad MANDA = el
> código, no este resumen.

## Layout (11 bytes por packet)

| byte | contenido | rango |
|---|---|---|
| 0 | `201` HEADER1 (sync pelota) | fijo |
| 1 | Xp_coded (X pelota + 100) | [0,200] · 255 = sentinel |
| 2 | Yp_coded (Y pelota + 100) | [0,200] · 255 = sentinel |
| 3 | `202` HEADER2 (sync arco amarillo) | fijo |
| 4 | Xam_coded | [0,200] · 255 |
| 5 | Yam_coded | [0,200] · 255 |
| 6 | `203` HEADER3 (sync arco azul) | fijo |
| 7 | Xaz_coded | [0,200] · 255 |
| 8 | Yaz_coded | [0,200] · 255 |
| 9 | **CRC8** = XOR de los bytes 0..8 | [0,255] |
| 10 | `254` END (fin de trama) | fijo |

Constantes (`cameras.h:65-71`): `CAM_HEADER1/2/3 = 201/202/203`, `CAM_END_BYTE = 254`,
`CAM_SENTINEL = 255`, `CAM_COORD_OFFSET = 100`, `CAM_PACKET_LEN = 11`.

## Codificación / decodificación

- **Coords X e Y SIMÉTRICAS:** `coded = valor + 100`, valor ∈ [-100,100] → byte ∈ [0,200]. El TOP
  decodifica `valor = byte - 100` (`cameras.cpp:8-11`, `decode_coord`).
- **Sentinel "no detectado" = byte coded `255`** — INALCANZABLE desde una detección real (las coords
  se clampean a [0,200] en la cámara, `main.py:84-85`). Objeto no-visible ⇔ su X_coded **O** Y_coded
  fue 255 (`cameras.cpp:55,63`).
- **Por qué no colisionan los bytes:** coords ∈ [0,200], headers = 201/202/203, END = 254,
  sentinel = 255 → ningún dato de coordenada real puede valer 201-203/254/255. El CRC es el único
  byte que puede tomar cualquier valor → se valida ANTES de aceptar el packet, no por posición.

## CRC8 (las dos implementaciones DEBEN coincidir)

XOR simple de los 9 bytes de datos (bytes 0..8), NO polinómico:
```c
// lado TOP — cameras.h:77-81
uint8_t cam_crc8(const uint8_t* data9){ uint8_t c=0; for(int i=0;i<9;++i) c^=data9[i]; return c; }
```
```python
# lado cámara — main.py:64-68
def crc8(data):
    c = 0
    for b in data: c ^= b
    return c & 0xFF
```
Si difieren, el parser descarta TODO frame (cuenta `crc_errors_`). Simple y determinista a propósito;
detecta cualquier bit-flip de 1 bit y la mayoría de los múltiples. Subir a CRC16 = subir el schema.

## Parser lado-TOP (`cameras.cpp:41-139`)

State machine de 11 estados (`WAIT_HEADER1 → READ_BALL_X → … → READ_CRC → WAIT_END`). En `WAIT_END`:
1. Si el byte ≠ 254 → `on_resync` (frame corrupto, no publica).
2. Si END OK pero `cam_crc8(buf) != rx_crc` → `crc_errors_++`, descarta (NO publica).
3. Si END + CRC OK → publica el packet (atómico: recién acá toca `packet_`), `packets_decoded_++`.

Estadísticas: `packets_decoded()`, `resync_events()` (framing perdido: header/END fuera de lugar),
`crc_errors()` (bit-flip en el enlace). `resync` se cuenta solo cuando un header esperado no aparece;
basura pre-trama se descarta en silencio. Estos contadores son la métrica de salud del enlace
cámara→TOP (TASK-015): si crecen en banco, hay ruido/bit-flips en el cable.

## Transporte (físico)

| | Frontal | Trasera |
|---|---|---|
| Serial del TOP | **Serial3** (RX pin **15**, conector U8) | **Serial5** (RX pin **21**, SOLDADA — SWAP TASK-204 2026-05-31) |
| `cam_id` (fusión) | 0 (coords tal cual) | 1 (el TOP rota 180°: invierte x e y) |
| Baud | **19200 8N1** (`UART_CAMERA1_BAUD`, `pinout_common.h:61`) | **19200 8N1** (`UART_CAMERA2_BAUD`, `:63`) |
| Lado cámara | `UART(3, 19200)` (`main.py:6`) | `UART(3, 19200)` |

- **fps ~30** (un packet cada ~33 ms; lo asume `CAMERA_TIMEOUT_MS=1000`, `cameras_runtime.cpp:31`).
- **Colchón RX 256 B** Arduino-only (`cameras_runtime.cpp:166-169`): el ring por defecto es 64 B; si
  el loop se bloquea, el packet de 11 B puede desbordarlo en silencio. 256 B = ~23 packets en vuelo.
- **Drenado:** `cameras_tick` lee hasta `MAX_BYTES_PER_TICK=64` bytes/tick por UART (margen 3× a
  19200), alimenta el parser, `recompute_fused()` (`cameras_runtime.cpp:187-224`).

## v1 (legacy INSEGURO) — por qué v2

v1 (contract-schema 1, 9 bytes `[201,Xp,Ypc,202,Xam,Yamc,203,Xaz,Yazc]`): X SIN offset (la mitad
IZQUIERDA del FOV se perdía por clamp), sentinel frágil `(X=0,Y=-100)`, SIN CRC ni END → un bit-flip
o un dato == header desincronizaba (bug R6). `main-comunicacion-vieja.py` es esa referencia v1 —
**NO flashear** (`cameras.h:9-31`). v2 lo resuelve de una: X simétrica + sentinel 255 inalcanzable +
CRC8 + END.
