---
id: TASK-204
title: "Multímetro: pines OUT1/OUT2 de la COMM (árbitro START/STOP) → pin Teensy del TOP"
date_created: 2026-06-02
date_due: 2026-06-07
assigned: [Enzo]
priority: P0
status: pending
estimated_hours: 0.5
blocks: [integrar-arbitro-en-firmware-top]
blocked_by: []
tags: [hardware, top-board, comm, arbitro, referee, multimetro, homologacion]
---

# TASK-204 — Confirmar a qué pines del Teensy llegan OUT1/OUT2 de la COMM

## Por qué importa (P0 — homologación)

Verificamos en código que **el START/STOP del árbitro NO le llega hoy al
Teensy del TOP**. Hay un desajuste de protocolo entre las dos placas:

- **La COMM (firmware oficial RCJ) entrega el árbitro como NIVEL de tensión**,
  no por UART. Confirmado en el firmware oficial (`state_machine.cpp`:
  `digitalWrite(OUTPUT1/2, HIGH/LOW)`) y en la doc de la placa
  (`hardware/electronics/comm-board/2026-05-17-...md`, sección 5):
  **OUT_1 / OUT_2 = 3.3 V → GO (PLAY), 0 V → STOP**. OUT_2 es espejo de OUT_1.
- **El firmware del TOP escucha el árbitro por UART** (`comm_arbiter.cpp`,
  Serial4) esperando un frame binario `COMM_REFEREE_CMD` con CRC que **la COMM
  oficial nunca emite**.

Resultado: aunque la COMM esté flasheada y reciba bien el árbitro por
Bluetooth, **el robot no se entera de cuándo arranca o para el partido**. Sin
esto el robot no compite. Por eso es P0.

La solución de firmware (Opción A, alineada con el diseño oficial RCJ) es que
el TOP **lea OUT1/OUT2 como 2 entradas digitales** en vez de esperar el UART.
Pero para programarlo necesito saber **a qué 2 pines del Teensy llegan
físicamente** esos niveles — y la documentación los marca como TENTATIVOS.

## Qué dice la documentación (sin verificar físicamente)

El conector de la COMM (`U3`, header 6P) mate-ea con el conector `U1`
("PINES MODULO") del TOP. El `mapa-pines-placas-nuevas.md` sugiere:

| Señal COMM | Pin Teensy (TENTATIVO, con "?") | Silk |
|---|---|---|
| OUT_1 (U3_1) | **23** | "OUT1C" |
| OUT_2 (U3_2) | **26** | "OUT1D" |
| RX_OUT (U3_3) | 25 (RX2) | — |
| TX_OUT (U3_4) | 24 (TX2) | — |

⚠️ Esos pines están marcados con `"¿PWM motor?"` y signos de interrogación en
la propia doc — **no están verificados**. Además el pin 23 lo usamos como
candidato en los barridos de ToF, así que hay que confirmarlo sí o sí antes de
que el firmware lo lea.

## Qué necesito (medir con multímetro, placa alimentada)

Para cada salida del header de la COMM, continuidad hasta el pin del Teensy:

| Señal | Pad en header COMM (U3) | ¿A qué pin del Teensy llega? |
|---|---|---|
| OUT_1 | U3_1 |  |
| OUT_2 | U3_2 |  |

### Cómo medir
1. Continuidad (pitido) desde **U3_1 (OUT_1)** de la COMM, recorriendo los
   pines del Teensy del TOP hasta que pite. Anotar el número.
2. Igual con **U3_2 (OUT_2)**.
3. **Verificación funcional (opcional, muy útil)**: con la app del árbitro
   mandando PLAY, medir tensión en U3_1/U3_2 → debe dar **~3.3 V**; con STOP →
   **0 V**. Confirma que la COMM realmente cambia el nivel.

## Criterio de cierre
- Los 2 números de pin (OUT_1 → pin __, OUT_2 → pin __).
- (Opcional) confirmado que cambian 3.3V/0V con PLAY/STOP de la app.
- Pasarme los pines → escribo la adaptación de `comm_arbiter.cpp` para leer
  START/STOP como nivel digital (con OUT_2 como verificación redundante) + un
  diag `diag_top_arbitro` que imprima PLAY/STOP en vivo.
- Journal entry con los números.

## Nota de diseño (para cuando programe)
- Leer 2 pines como `INPUT`. PLAY = ambos HIGH, STOP = ambos LOW. Si difieren
  (uno HIGH, otro LOW) → glitch/cable: mantener el último estado estable +
  marcar flag de error. Debounce ~20-50 ms.
- El nivel de OUT1/OUT2 ya viene en dominio 3.3 V (level shifter TXS0102 en la
  COMM), compatible con el Teensy. **Confirmar que NO son 5 V** antes de
  conectar (el Teensy 4.0 NO tolera 5 V en sus GPIO).

## Cambios de estado
- 2026-06-02: creada por Claude (Opus 4.8) tras verificar en código el
  desajuste de protocolo COMM(niveles) vs TOP(UART) del árbitro, a pedido de
  Gustavo Viollaz. Ver journal `2026-06-02-arbitro-gap-y-ultrasonido-top.md`.
