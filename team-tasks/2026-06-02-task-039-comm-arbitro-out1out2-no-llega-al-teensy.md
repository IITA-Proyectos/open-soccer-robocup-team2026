---
id: TASK-039
title: "COMM árbitro: el START/STOP (OUT1/OUT2) no llega al Teensy del TOP"
date_created: 2026-06-02
date_updated: 2026-06-02
assigned: [enzo, gustavo]
priority: P0
status: resolved
estimated_hours: 2
blocks: [homologación Incheon — el robot no recibe START/STOP del árbitro]
relacionado: [TASK-006]
tags: [comm-board, arbitros, hardware, top-board, multimetro, homologacion]
---

# TASK-039 — El START/STOP del árbitro (OUT1/OUT2) no llega al Teensy

> **✅ RESUELTO 2026-06-02 (Gustavo, banco).** El árbitro **SÍ llega como NIVEL GPIO** a los
> pines **5 (OUT1) y 6 (OUT2, espejo)** del Teensy del TOP: **0 = parado, 1 = jugando** (3.3 V) —
> probado en banco, togglea con PLAY/STOP. **Firmware actualizado:** `comm_arbiter.cpp`
> (`read_referee_gpio()` lee pines 5/6 con `INPUT_PULLDOWN`; `match_running = pin5 Y pin6`,
> fail-safe a STOP si se desconecta) + probe temporal removido de `main_top.cpp` (queda
> `arb[... p5/6=..]` en el debug). El UART (Serial2, 7/8) queda solo para partner ESP-NOW.
> **Cierra el E2E del árbitro que TASK-006 había dejado pendiente.** El detalle de abajo es el
> diagnóstico que llevó hasta acá.
>
> *(Para Enzo — diagnóstico original, ya superado:)* Necesitaba multímetro + app RefMate.

## Síntoma

El módulo COMM está flasheado (TASK-006) y **responde a la app** (el OLED muestra
que prende/apaga). Pero el Teensy del TOP **no ve el cambio**: en el monitor serie
del TOP, el árbitro queda congelado y `rx=0` por UART.

Monitor del TOP (firmware `top_robot1` con probe temporal de pines, ver §4):
```
[TOP] ... arb[ref=255 match=N age=... rx=0] refprobe[5=0 6=1 7=0 8=0 26=0 27=0] ...
```
- `rx=0` → por UART (Serial4) no llega nada. **Esperado** (ver §2).
- `refprobe`: **pin 6 = 1 fijo, pin 5 = 0 fijo, el resto 0 — NO cambian** al dar
  on/off en la app. Ese es el problema.

## 2. Causa raíz de diseño (ya entendida)

El árbitro oficial RCJ **NO se comunica por UART**: señaliza con un **NIVEL en
OUT1/OUT2** (3.3 V = PLAY/GO, 0 V = STOP). Lo dice el doc del propio módulo:
`hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md:217,220`.

Pero el firmware del TOP (`src/top/comm_arbiter.cpp`) **escucha Serial4 (UART)** y
espera frames `COMM_REFEREE_CMD` → con el firmware OFICIAL del COMM eso **nunca llega**
(por eso `rx=0`). Esto es un mismatch que ni el contrato ni la auditoría habían
detectado. **El fix de firmware es leer el árbitro como PIN DIGITAL** (lo hace el
coach, ver §5) — pero primero hay que confirmar que la señal llega eléctricamente al
Teensy, que es **lo que NO está pasando** y el objeto de esta tarea.

## 3. Ground-truth del ruteo (del netlist del PCB)

De `hardware/electronics/pcb_design/top_board/PCB_PCB_Roboliga2026_TOP_2026-04-12.json`:

| Señal COMM | Pad del Teensy U14 | GPIO del Teensy | Estado medido por firmware |
|---|---|---|---|
| **OUT1** (PLAY/STOP) | pad **27** | **GPIO 5** (`IN2`) | `0` fijo |
| **OUT2** (espejo de OUT1) | pad **26** | **GPIO 6** (`OUT1D`) | `1` fijo |

> Las señales del módulo entran por el conector **U1 (2541WV-06P, "PINES MODULO")**:
> OUT1, OUT2, RX_OUT, TX_OUT, USB_D±. El nivel lógico/ alimentación va por **U7
> (2541WV-04P)**: LOGV + 3.3V. (El GPIO→nombre del Teensy: confirmar contra el
> esquemático `Schematic_Roboliga2026_TOP_2026-04-12.pdf`; hay ±1 de ambigüedad en
> cómo el símbolo numera, por eso el firmware probó 5,6,7,8,26,27 — todos cubiertos,
> ninguno togglea.)

## 4. Cómo reproducir / observar

Firmware con **probe temporal** ya commiteado (lee y muestra los pines candidatos):
```
cd "software/teensy/Soccer 2026"
pio run -e top_robot1 -t upload
pio device monitor -b 115200      # esperar ~40 s de boot (carga ToF)
```
Mirar el campo `refprobe[5=.. 6=.. 7=.. 8=.. 26=.. 27=..]` mientras se da
**START/STOP del partido** en la app. **Si la señal llegara**, 1–2 de esos pines
pasarían de `0`↔`1`. Hoy NO cambian. (El probe vive en `src/top/main_top.cpp`,
bloque de debug, marcado `TEMP probe (bring-up COMM)`.)

## 5. Qué hacer (en orden)

**Paso A — ¿qué se aprieta en la app? (descartar operación, 5 min)**
`OUT1/OUT2` siguen el **START/STOP del PARTIDO** (kickoff/stop del árbitro), **no** el
conectar/desconectar BLE ni un "encender el robot". Confirmar que en la **RefMate** se
está dando el **START de partido** real. Si con el START real tampoco togglea → Paso B.

**Paso B — multímetro en el MÓDULO (punto inequívoco)**
Medir en las salidas del módulo COMM **OUT1/OUT2 (= ESP32-C6 GPIO20 / GPIO19)** al dar
START y STOP:
- **¿Togglean 3.3 V ↔ 0 V?**
  - **SÍ** → el módulo genera bien la señal. El problema es el **camino al Teensy**:
    - Continuidad **módulo → conector U1 → Teensy pin 5 (OUT1) y pin 6 (OUT2)**.
    - Que el módulo esté **bien encastrado** en U1.
    - Nivel **LOGV** del conector U7 = **3.3 V** (si LOGV está mal/flotante, el módulo
      no traslada el nivel correcto al Teensy).
  - **NO togglean en el módulo** → es **firmware/config del módulo COMM**, no el
    cableado del TOP. Revisar que el firmware oficial esté en modo partido y que las
    salidas OUT1/OUT2 estén habilitadas (ver `comm-board/2026-05-17-procedimiento-flash-firmware-c6.md`
    test 4: medir U3_1/U3_2 = OUT1/OUT2 con PLAY/STOP).

**Paso C — reportar el resultado** (¿toggla en el módulo? ¿hay continuidad al pin 5/6
del Teensy? ¿LOGV OK?). Con eso el coach cierra el lado firmware (§6).

## 6. Follow-up de firmware (coach, cuando Enzo confirme la HW)

Una vez que la señal **llegue y togglee** en el pin del Teensy (5 = OUT1):
- Cambiar `comm_arbiter`/`main_top` para derivar `match_running` de
  `digitalRead(PIN_REFEREE)` (HIGH = PLAY) en vez de (o además de) Serial4.
- Definir el pin en `pinout_common.h` (`PIN_REFEREE`), con la polaridad confirmada.
- Sacar el probe temporal de `main_top.cpp`.
- Esto cierra el E2E que TASK-006 dejó pendiente (homologación: el robot arranca/para
  con el árbitro).

## Criterio de cierre

- [ ] Confirmado qué comando de la app mueve OUT1/OUT2 (START/STOP de partido).
- [ ] Medido en el módulo: OUT1/OUT2 togglean 3.3 V↔0 V con PLAY/STOP.
- [ ] Continuidad módulo→Teensy (pin 5/6) verificada (o reparada).
- [ ] En el monitor del TOP, `refprobe` (o el read final) togglea con PLAY/STOP.
- [ ] Coach: firmware leyendo el pin → `match_running` togglea; probe removido.

## Referencias

- Firmware lado TOP: `src/top/comm_arbiter.cpp` (hoy UART, hay que pasar a GPIO),
  `src/top/main_top.cpp` (probe temporal + `referee_cmd`/`match_running` al snapshot).
- Doc del módulo: `hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md`
  (§ OUT1/OUT2 = nivel, no UART) y `...-procedimiento-flash-firmware-c6.md` (test OUT1/OUT2).
- Netlist/esquemático TOP: `hardware/electronics/pcb_design/top_board/` (PCB/SCH json + PDF).
- App de árbitro: `hardware/electronics/comm-board/RECURSOS-Y-ENLACES.md` (RefMate Android).
- Antecedente: TASK-006 (COMM flasheada; el E2E quedó pendiente — es esta tarea).

---

### Nota relacionada (secundaria, NO bloquea esta tarea)
Revisando el esquemático apareció una posible **discrepancia en los pines del HC-SR04**:
el firmware usa `TRIG=4 / ECHO=3` (`pinout_common.h`) pero el ruteo del PCB podría ser
otro. Como el ultrasónico **anda**, no es urgente — pero conviene confirmarlo con el
mismo método (probe + multímetro) cuando se cierre el COMM. (Gustavo/Enzo.)
