---
title: "Banco 2026-06-12 — Práctica Elías: delantero R1 sin gyro por OTOS"
date: 2026-06-12
autor: "Elías Cordero (@3liasCo) en el banco + Claude (asistencia)"
tipo: banco
robot: R1
rol: delantero (ATTACKER forzado, sin gyro, rumbo por OTOS)
---

# Banco 2026-06-12 — Delantero R1 por OTOS (práctica de Elías)

Sesión de práctica del guion `docs/pruebas-banco/PRACTICA-2026-06-12-ELIAS-DELANTERO-R1.md`.
Robot 1, delantero, **sin giroscopio** (BNOs desconectados): el rumbo lo da el OTOS del piso.

## Qué se hizo / qué se encontró

### 1. Calibración de línea (DOWN) — HECHA y guardada
Se calibró la línea en la cancha y se **guardó en EEPROM** (persistente). Se usó el
flujo de la app visual / el diag de texto. Confirmado que el reflasheo NO borra la
calib (EEPROM sobrevive; solo `x`/CAL ERASE la borra).

### 2. `diag_central_blink` — firmware nuevo (CENTRAL inofensiva)
Para calibrar/probar **con la batería puesta sin que el robot se mueva**: la CENTRAL
solo parpadea el LED y **fuerza los 3 drivers a OFF** (INA=INB=LOW, PWM=0) — así las
entradas del H-bridge no quedan flotando al boot. Archivo
`src/diag/diag_central_blink.cpp` + env `diag_central_blink`. NO es firmware de
competencia. Pedido por Elías porque al intentar calibrar, los motores se movían.

### 3. PASO 2 — Signo del OTOS VALIDADO ✅ (cierra el riesgo #1 del guion)
Girando el robot a mano (panel `otos=`):
- **Antihorario → SUBE** hacia +180. **Horario → BAJA** hacia −180. Estable al deslizar.
- El "salto/cambio de signo" que notó Elías al llegar a ±180 es el **wraparound normal**
  del ángulo (+180° y −180° = misma dirección). NO es bug; el empuje usa diferencia de
  ángulo que cuenta el wrap. **No se tocó el signo.** El riesgo #1 del guion (signo del
  yaw OTOS sin validar) queda CERRADO.

### 4. Fix pin 9 — la CENTRAL arrancaba SOLA tras un rato encendida
Síntoma: el robot empezaba a moverse solo después de un rato en `WAIT_START`. Mismo
síntoma que ya había tenido R2 (resuelto deshabilitando el pin 9). Causa: el pin 9
(`PIN_MANUAL_START_BUTTON`, INPUT_PULLUP) es un botón **ASUMIDO/sin confirmar** que
está cableado; flotando/con ruido se lee LOW → GO espurio (el propio comentario del
firmware lo admitía: "puede dar GO espurio").
Fix: flag **`CENTRAL_DISABLE_PIN9_BUTTON`** que apaga SOLO la lectura del pin 9
(`#ifndef` alrededor del `pinMode` y del `digitalRead` en `main_central.cpp`),
activado solo en `central_robot1_delantero_practica*`. **Intactos:** el juez por
teclado (`g`/`s`) y la app del árbitro (camino real `world_model_match_running()` por
GPIO 5/6 del TOP → snapshot; es función distinta del override manual). R2 y competencia
byte-idénticos (flag exclusivo de los envs de práctica R1). Gate: `central_robot1_delantero_practica_bb` compila SUCCESS.

### 5. Corrida de cancha — la máquina de estados COMPLETA funcionó
Primera vez que el delantero R1 recorre toda la FSM en cancha:
`WAIT_START → KICKOFF → SEARCH → POSITION → APPROACH → PUSH → PUSH_BACK`.
Arrancó solo con el GO (no se disparó en los ~31 s previos → fix del pin 9 aguanta).
Cámara vio la pelota (`ball_vis=1`).

**Pero (a analizar):**
- **`INIT` de ~31 s:** el TOP no mandó snapshots (`snap_fresh=N`, `top[fr=0]`) hasta el
  segundo ~31 (boot BNO+ToF ~40 s, documentado). La CENTRAL no sale de INIT hasta el
  primer snapshot. **Operativa: esperar a salir de INIT antes del GO.** En partido es
  tema serio (P1).
- **Se queda ORBITANDO en `POSITION`:** ~235 s casi siempre en POSITION, tocó `PUSH`
  solo 4 veces. `goal_vis=0` SIEMPRE — **nunca vio el arco** → el eje de ataque depende
  100% del OTOS capturado **al encender**. Hipótesis #1: el robot no se encendió mirando
  al arco rival → la órbita no cierra. (A reconfirmar orientando el robot al arco al boot.)
- **DOWN pierde tramas con motores andando:** `lost` congelado en 19 con motores quietos;
  apenas se mueve, sube parejo (~50/s, ~25%). Ruido de motor sobre el cable DOWN→CENTRAL.
  `valid=Y` sigue (degradado, no muerto). A vigilar (P2) — bajo carga podría afectar el
  frenado de borde.

### 6. TASK-103 abierta (P2)
App PC visual para la CENTRAL (ver FSM/decisiones en vivo), fase 3 del sistema de
monitoreo (DOWN=TASK-304, TOP=TASK-205). Hoy la CENTRAL solo se ve por panel de texto.
Alternativa barata anotada: graficar la FSM desde el CSV con `analizar_corrida.py`.

### 7. Próximo en este banco
Test visual de cámaras + ToF del TOP: `top_robot2_pri_debug_telemetry` (USB en TOP) +
`python -m monitor_base --top --port COMxx`, con la CENTRAL en `diag_central_blink`
(quieta). Objetivo: confirmar si detecta pelota (2 cámaras), **arco** (clave del
`goal_vis=0`) y los 4 ToF.

## Pendientes / a verificar en HARDWARE (NO cerrados — requieren al equipo)
- [ ] Reconfirmar que con el robot **encendido mirando al arco rival** la órbita cierra y
      patea seguido (PASO 5/6).
- [ ] Test visual TOP: ¿detecta arco? ¿pelota en ambas cámaras? ¿4 ToF?
- [ ] `valid=N` de línea sobre verde: confirmar que pasa a `Y` cruzando blanco (frenado de borde).
- [ ] Ruido motor → pérdida de tramas DOWN: cuantificar / ver si afecta partido.
- [ ] INIT de 31 s (boot TOP): evaluar mitigación para competencia (P1).

## Archivos tocados
- `src/diag/diag_central_blink.cpp` (nuevo) + env en `platformio.ini`.
- `src/central/main_central.cpp` + `platformio.ini`: flag `CENTRAL_DISABLE_PIN9_BUTTON`.
- `team-tasks/2026-06-12-task-103-*` + índice en `team-tasks/README.md`.
