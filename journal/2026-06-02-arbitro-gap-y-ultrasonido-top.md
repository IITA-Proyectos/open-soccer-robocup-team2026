---
title: "TOP — Árbitro: el START/STOP no llega al Teensy (gap de protocolo) + ultrasonido andando"
date: 2026-06-02
author: "Claude Opus 4.8 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8, Anthropic)"
status: final
tags: [top-board, arbitro, referee, comm, ultrasonido, hcsr04, hardware-up]
robot: ambos
area: firmware
tipo: hallazgo
---

# TOP — Árbitro no llega al Teensy + ultrasonido andando

> **TL;DR.** Dos cosas esta sesión. **(1) Hallazgo P0 del árbitro:** la placa
> COMM (firmware oficial RCJ) entrega el START/STOP como **nivel de tensión**
> en OUT_1/OUT_2 (3.3V=GO, 0V=STOP), pero el firmware del TOP lo escucha por
> **UART** esperando un frame binario que la COMM nunca emite. Resultado: con
> el firmware actual, **aunque la COMM esté flasheada, el robot no se entera de
> cuándo arranca/para el partido**. → TASK-204 (Enzo confirma pines) + adaptar
> `comm_arbiter.cpp` a leer niveles. **(2) Ultrasonido HC-SR04 ANDANDO** en
> banco, cableado real **TRIG=pin4, ECHO=pin3** (no 6/7 del PCB), sin divisor
> de tensión en el ECHO.

## 1. Árbitro — el START/STOP no llega al Teensy del TOP (P0)

### Contexto
Gustavo conectó los cables de comunicación entre placas (TOP↔DOWN, TOP↔CENTRAL)
y avisó que la COMM "ya está flasheada hace rato". Pregunta concreta:
**¿le llega al Teensy del TOP el comando de empezar/parar partido?**

### Qué verifiqué (en código + docs, no de memoria)

**La COMM oficial entrega el árbitro como NIVEL, no por UART.**
- Firmware oficial (`_official_fw/firmware/RCj_comm_module/state_machine.cpp`):
  en PLAY → `digitalWrite(OUTPUT1_GPIO, HIGH); digitalWrite(OUTPUT2_GPIO, HIGH)`;
  en STOP → ambos LOW. `OUTPUT1_GPIO=20`, `OUTPUT2_GPIO=19`. El UART solo lo usa
  para imprimir "PLAY"/"STOP" de debug por el USB, no por el header al robot.
- Doc de la placa COMM (`hardware/electronics/comm-board/2026-05-17-...md` §5):
  textual, *"El robot lee START/STOP como NIVEL en OUT_1/OUT_2 (no por serial).
  El arranque/parada del árbitro NO es un mensaje UART... El UART RX_OUT/TX_OUT
  es hardware pasivo: el firmware oficial v0.91 NO lo usa."*
  `OUT_1` (U3_1) = 3.3V GO / 0V STOP. `OUT_2` (U3_2) = espejo redundante.

**El TOP escucha el árbitro por UART, esperando un frame binario.**
- `src/top/comm_arbiter.cpp`: drena Serial4, decodifica frames con CRC y solo
  reacciona a `MsgType::COMM_REFEREE_CMD`. Ese frame **la COMM oficial nunca lo
  emite**. El `comm_arbiter_is_match_running()` que consume `main_top.cpp` para
  el flag del WorldSnapshot, por lo tanto, **nunca cambia a true en cancha**.

**El cable físico va al UART, no a los niveles.**
- El conector `U1` del TOP ("PINES MODULO") mate-ea con `U3` de la COMM y trae
  los 6 pines (OUT1, OUT2, RX_OUT, TX_OUT, USB_D+, USB_D-). El firmware del TOP
  solo mira RX_OUT/TX_OUT (Serial4) — justo los 2 que la COMM deja pasivos. Los
  OUT1/OUT2, que son los que cambian con PLAY/STOP, el firmware no los lee.

### Conclusión
**Desajuste de protocolo COMM(niveles) ↔ TOP(UART). Nunca se probó y, tal como
está, no puede funcionar.** No es un cable suelto: es integración faltante.

### Tema-a-analizar (formato coach)

**Categoría:** comunicación · **Robot:** ambos · **Prioridad: P0** (homologación)

**Qué observo.** `comm_arbiter.cpp` espera `COMM_REFEREE_CMD` por Serial4; la
COMM oficial entrega niveles en OUT1/OUT2 (ver arriba). El robot no recibe el
START del árbitro → no arranca en cancha.

**Risk-no-fix.** No compite: sin START válido el robot queda quieto en el
partido (o peor, no homologa).
**Risk-fix.** Bajo. Leer 2 GPIO es trivial y no toca el firmware certificado de
la COMM (que NO se debe modificar). Único cuidado: confirmar que OUT1/OUT2 son
3.3V (lo son, level shifter TXS0102 en la COMM) — el Teensy 4.0 no tolera 5V.
**Tiempo.** ~1 h una vez confirmados los pines (TASK-204).

**Plan.**
1. **TASK-204 (Enzo):** multímetro, continuidad OUT_1/OUT_2 (U3_1/U3_2 de la
   COMM) → pin del Teensy. La doc sugiere 23 y 26 pero están SIN verificar.
2. Adaptar `comm_arbiter.cpp`: leer 2 entradas digitales (PLAY=ambos HIGH,
   STOP=ambos LOW, OUT_2 como verificación redundante, debounce ~20-50 ms).
   Mantener el path UART para el partner/ESP-NOW si en el futuro se usa.
3. Diag `diag_top_arbitro`: imprime PLAY/STOP en vivo para confirmar con la app
   del árbitro mandando start/stop.

> **Pendiente de doc cruzada:** TASK-006 ("cargar firmware COMM") sigue marcada
> `pending` en `ESTADO-ACTUAL.md` aunque la COMM ya está flasheada. Actualizar
> ese estado cuando el equipo confirme + cierre TASK-006 (Claude no cierra
> TASKs de hardware).

## 2. Ultrasonido HC-SR04 — ANDANDO en banco (TRIG=4, ECHO=3)

### Resultado
Con el diag de ultrasonido, lecturas válidas y estables en banco:
```
T4/E3: 738us=12.7cm   ...   3597us=62.0cm   (ecos coherentes con la distancia real)
T3/E4: sin eco
```
O sea: el sensor responde en el par **TRIG=pin 4, ECHO=pin 3**. El par invertido
(T3/E4) da "sin eco" → descarta esa asignación.

### Lo importante (3 puntos)

1. **El cableado real NO es el del PCB (6/7).** `pinout_common.h` declara
   `PIN_HCSR04_TRIG=6, PIN_HCSR04_ECHO=7`. El cableado físico de banco es
   **TRIG=4, ECHO=3**. Hay que actualizar el pinout cuando se integre al
   firmware vivo (hoy el HC-SR04 está gateado OFF por `#ifdef TOP_ENABLE_HCSR04`).

2. **Esto ESQUIVA el conflicto histórico del pin 7.** El ECHO en 6/7 chocaba
   con Serial2 RX2 (uplink a CENTRAL) — era la causa del stall de 25 ms (TASK-014).
   Con ECHO en pin 3, **ese conflicto desaparece**: pin 3 está libre. Es una
   buena noticia, pero hay que verificar que pin 3 y pin 4 no choquen con otro
   uso (revisar contra el resto del pinout antes de habilitar).

3. **El ECHO va directo, SIN divisor de tensión.** El HC-SR04 es un sensor de
   5 V y su pin ECHO entrega **5 V**; el Teensy 4.0 **NO tolera 5 V en sus
   GPIO** (máx 3.3 V). Hoy "anda" porque el pin aguanta, pero es **operar fuera
   de spec** → riesgo de degradar/quemar el pin del Teensy con el tiempo.

### Tema-a-analizar (formato coach)

**Categoría:** electrónica · **Robot:** ambos · **Prioridad: P1**

**Qué observo.** ECHO del HC-SR04 (5 V) conectado directo al pin 3 del Teensy
4.0 (tolerancia máx 3.3 V), sin divisor resistivo.

**Risk-no-fix.** El pin 3 del Teensy puede degradarse o quemarse por
sobretensión → se pierde el ultrasonido y, si el daño escala, el MCU. Latente:
puede andar semanas y fallar en cancha.
**Risk-fix.** Mínimo: divisor con 2 resistencias (p.ej. 1kΩ + 2kΩ) en el ECHO,
o un módulo level-shifter. 10 min de soldadura.
**Tiempo.** ~15 min.

**Plan de prueba.**
1. Agregar divisor en ECHO (1k a la señal, 2k a GND, toma en el medio → ~3.3 V).
2. Re-verificar que el diag siga leyendo distancias coherentes (12-13 cm a 60 cm).
3. Confirmar con multímetro que el pin medio del divisor da ≤3.3 V con ECHO en HIGH.

> **Nota:** el HC-SR04 es **redundante** con el ToF frontal (que ya da distancia
> frontal). No es bloqueante para Incheon, pero si se va a usar, el divisor es
> obligatorio. Decidir si vale la pena el pin extra o si el ToF frontal alcanza.

## Archivos / acciones de esta sesión
- `team-tasks/2026-06-02-task-204-*.md` — **nueva**, Enzo confirma pines OUT1/OUT2.
- Este journal.
- (Pendientes para cuando Enzo confirme pines): adaptar `comm_arbiter.cpp` a
  leer niveles + diag `diag_top_arbitro`; actualizar `PIN_HCSR04_*` a 4/3 +
  divisor antes de habilitar `TOP_ENABLE_HCSR04`.
