# 2026-06-14 — Banco: monitor TOP validado en placa + TOP→CENTRAL OK + hallazgo OTOS DOWN

> Sesión de banco (Gustavo, placa real). Claude asiste/registra; la **validación
> de hardware la cerró Gustavo** (regla 1 CLAUDE.md). Continúa el journal de
> implementación `2026-06-14-monitor-top-salud-y-zonas-telemetria.md`.

## Qué se probó y resultado

### 1. Monitor `--top-salud` en la placa TOP real → ✅ ANDA
- Se flasheó `top_robot2_pri` (trae el monitor dormido + el campo `z` de zonas) y la
  app `python -m monitor_base --top-salud` **conectó y mostró dato real** (sensores,
  zonas). El handshake STREAM ON/PING funciona en placa.
- **Bug encontrado y arreglado en el momento: PARPADEO.** La ventana se redimensionaba
  cada frame (medido: reqwidth saltaba 834↔1016). Causa: el frame de cada tile tenía
  `grid_propagate(False)` pero sus hijos van con `pack` → no los clampeaba y el tile
  se agrandaba con el texto. Fix: `pack_propagate(False)` + ancho fijo del header.
  Verificado estable (1016×596 constante) + test de regresión. Commit `b42e220`.

### 2. TOP → CENTRAL (WorldSnapshot) → ✅ ANDA (lo que faltaba probar)
Con `diag_central_rx_all` (receptor de banco en la CENTRAL):
```
TOP (Serial7): enlace [OK]  (crc=0  seqGap=0)
  [OK]  SNAPSHOT  66-68 Hz  age 3-15ms
  pose x≈1260 y≈520  conf=70 · flags=0x10 (HEADING_VALID=1) · ball vis · goal_opp vis
```
Enlace **limpio** (0 CRC, 0 seqGap, 66 Hz), snapshot decodificado entero. El `hdg=0.0`
es real (robot quieto en orientación de boot). Las cámaras + trilateración del TOP
están andando (detecta pelota y arco). **El cambio del campo `z` NO afectó el snapshot
a CENTRAL** (otro contrato, intacto).

### 3. Hallazgo aparte (DOWN, NO bloquea lo anterior): OTOS sin odometría
El mismo diag mostró el lado DOWN:
```
DOWN (Serial1): enlace [REVISAR]  (crc=0  seqGap=1400↑)
  [OK]    LINEA      200 Hz
  [OK]    OTOS pose  100 Hz   → pero x=0 y=0 hdg=0 conf=0  (todo ceros)
  [FALTA] OTOS vel   #0 nunca
```
Diagnóstico: el **OTOS no produce dato válido** (`conf=0`). La **velocidad nunca se
difunde** (#0) — y eso **explica el `seqGap`**: `seqGap`≈conteo de pose = el stream de
vel a 100 Hz que falta (no es pérdida real, `crc=0`). Causa casi segura: **los OTOS se
alimentan del 3.3 V de la batería (MP1584), NO del USB** → si la DOWN está por USB solo
o la batería no entrega, OTOS = `conf=0` / sin vel. El propio diag lo apunta. → **TASK-308**.

> **Corrección (mismo día, dato de María: el robot 2 NO tiene OTOS hasta nuevo aviso):**
> la causa **NO era batería**. El DOWN estaba con el binario **`down`** (asume 2 OTOS) en
> un robot **sin OTOS** → pose basura (`conf=0`) + vel inexistente + `seqGap`. **Fix:
> flashear `down_robot2`** (`OTOS=0`); los consumidores caen al fallback sin OTOS.
> TASK-308 corregida (causa = binario equivocado, no hardware; P1→P2).

## Cierres / estado de tareas
- **TASK-209** (validar banco monitor TOP) → **CERRADA**: monitor en placa ✓ + TOP→CENTRAL ✓.
  Sub-checks finos (tapar sensor, botones config, pelota fantasma, cable-pull) no se
  reportaron uno por uno — quedan como verificación opcional, no bloquean.
- **TASK-031** (verificar UART 3 placas) → **transporte CONFIRMADO** (DOWN→CENTRAL LINEA
  200 Hz crc=0; TOP→CENTRAL SNAPSHOT 66 Hz crc=0). Resta DOWN→TOP directo, latencia con
  osciloscopio y comandos RX → se deja anotado, transporte ya probado.
- **TASK-208** (GUI monitor TOP) → el objetivo operativo (ver per-cámara/OTOS/escape/zonas
  + config) se cumplió con la **vista nueva `--top-salud`** (no la extensión del radar
  `gui_top.py`). Cerrada como cumplida por vista alternativa; el radar queda opcional.
- **TASK-308** (NUEVA) → OTOS de la DOWN sin odometría (pose conf=0, vel no difunde).
