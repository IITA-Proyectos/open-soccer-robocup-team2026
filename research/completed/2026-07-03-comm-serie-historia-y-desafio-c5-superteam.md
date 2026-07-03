---
title: "Comunicación serie TOP↔COMM: la historia real + rehabilitación para el desafío SuperTeam C5"
date: 2026-07-03
author: "Claude (Fable 5) — workflow paralelo 7 agentes, verificación adversarial"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
tipo: research-completed
status: investigación completa · citas archivo:línea verificadas por el main loop · mediciones de hardware PENDIENTES (§4)
ancla: "desafío SuperTeam Incheon: Robot-to-Robot Kick-off & Pass con módulos C5 (UART bridge 115200)"
---

# La serie TOP↔COMM: por qué "se deshabilitó" y cómo rehabilitarla para el C5

> Pregunta de Gustavo: "habíamos deshabilitado las comunicaciones serie y no recuerdo por qué; me
> parece que la placa TOP tenía mal cableadas las pistas y se anularon cables". Barrido COMPLETO del
> repo (journal, team-tasks, hardware/electronics, docs/decisions, código TOP/CENTRAL) + historia git.

## 1) LA HISTORIA — la serie NUNCA se deshabilitó ni se anularon cables

- **2026-05-17** — Forense de la placa COMM: el firmware oficial RCJ v0.91 señaliza START/STOP como
  **NIVEL en OUT1/OUT2, no por UART**; el UART RX_OUT/TX_OUT es "hardware pasivo: el firmware oficial
  no lo usa" — `hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md:220-224`.
  Ese día se firmó la **Opción A: NO inter-robot para Incheon** —
  `docs/decisions/2026-05-17-comunicacion-inter-robot-superteam.md:55` (status final).
- **2026-06-02** — Banco: el TOP escuchaba al árbitro por UART esperando un frame que el firmware
  oficial jamás emite. "Desajuste de protocolo COMM(niveles) ↔ TOP(UART). No es un cable suelto: es
  integración faltante" — `journal/2026-06-02-arbitro-gap-y-ultrasonido-top.md:69-70`. Resolución
  (TASK-039, commits `72a6376`/`723103d`/`29e24d5`): **árbitro migrado a GPIO 5/6** (match = pin5 OR
  pin6) + remapeo COMM=Serial2 (7/8), CENTRAL=Serial4 (16/17) — el Teensy 4.0 no expone Serial7 en el
  borde. Cierre textual: "El UART (Serial2, 7/8) queda solo para partner ESP-NOW" → **reservado, no anulado**.
- **2026-06-03** — Hito: COMM→GPIO→TOP→CENTRAL mueve el robot. Desde ahí, silencio sobre el partner UART.

**El recuerdo de "pistas mal cableadas → cables anulados" NO tiene respaldo escrito para el UART del
COMM.** Candidatos probables de la confusión (estos SÍ pasaron):
1. **HC-SR04**: el PCB ruteaba ECHO en pin 7 (= Serial2 RX2); se ANULÓ ese cableado y se recableó a
   TRIG=4/ECHO=3 — eso justamente LIBERÓ el Serial2 para el COMM (`journal/2026-06-02-...md:117-126`).
2. **Serial7 fantasma** (TASK-204): el uplink a CENTRAL apuntaba a un serial inexistente en el borde.
3. **XSHUT de ToF no ruteados** (bodges a 9-12) y 10 nets sin rutear de la DOWN — otros subsistemas.
4. Botón pin 9 de la CENTRAL deshabilitado por GO espurio (`config_central.h:245-249`) — otra cosa.

## 2) ESTADO HOY

- **Físico:** el camino UART existe por **pista nativa del PCB** (netlist ground-truth
  `hardware/electronics/top-board-pack/ground-truth/PCB_PCB_Roboliga2026_TOP_2026-04-12.json`):
  U1 pads 4/3 (RX_OUT/TX_OUT) → GPIO 8/7 = **Serial2**. Sin conflicto con el BNO (una afirmación previa
  que los ponía en Wire2 era un error de pads-vs-GPIO).
- **Firmware TOP:** VIVO, sin ifdefs — `Serial2.begin(115200)` incondicional (`comm_arbiter.cpp:83`;
  baud en `pinout_common.h:62` — **ya coincide con el C5**), drena RX cada tick, acepta
  `COMM_PARTNER_DATA` (0x40) → `g_partner` (freshness 500 ms). Las funciones TX existen pero **cero
  callers** — el TOP jamás transmitió un byte.
- **Por qué `rx=0`:** esperado por diseño — el firmware oficial de la COMM no transmite nada por UART.
  No hay transmisor del otro lado. **No es evidencia de cable roto** (TASK-039:41 "rx=0 → Esperado").
- **Consumidores:** nadie decide hoy con datos de partner (`partner_sees_ball` hardcodeado false en
  `snapshot_emitter.cpp:150`; strategy/centralmix/arqueromix lo ignoran) → rehabilitar no rompe nada.

## 3) REHABILITAR PARA EL DESAFÍO C5 — temas a analizar

### P0-A — Validación física del tramo U1↔Serial2 (NUNCA probado end-to-end)
- risk-no-fix: si TX/RX está invertido o falta continuidad, todo el software del desafío es inútil.
- risk-fix: ninguno (es medición). — tiempo: **30-45 min** (multímetro + loopback).
- Plan: (a) continuidad U1 pad4/pad3 ↔ pines 7/8; (b) LOGV de U7 a 3.3 V (sin eso el TXS0102 no
  traduce); (c) con el C5 conectado, mandar bytes y ver subir `arb[rx]` en el panel (contador ya existe).
- Nota: el doc COMM se auto-contradice sobre el sentido de RX_OUT (`:110-111` vs `:164-173`) y TASK-008
  (rewiring UART) sigue pending desde mayo.

### P0-B — Firmware: 3 fixes chicos (2-4 h + 1 h banco)
1. **Nadie llama TX** → llamada periódica (~10-20 Hz) a `comm_arbiter_send_partner()` desde el loop.
2. **Bridge transparente:** el C5 repite bytes crudos → el frame llega como `TOP_PARTNER_DATA` (0x41) y
   `handle_frame` lo descarta (`comm_arbiter.cpp:75-76`). Fix de 1 línea (aceptar 0x41). El framing
   propio (0xAA/LEN/TYPE/SEQ/CRC16/0x55) sobrevive el bridge entre dos robots IITA.
3. **Consumidor:** subir "partner pateó" a la decisión (bit libre del snapshot) — Robot B lo espera.
- No toca el path GPIO del árbitro (sigue autoritativo).

### P1 — Actualizar la decisión formal
La Opción A ("NO inter-robot para Incheon", canónica en `FUENTES-DE-VERDAD.md:69`) queda invalidada por
el pedido del comité (C5 prestado). **Superarla en el mismo commit que rehabilite la serie** (regla 4).

### Bloques del desafío que YA existen
- **Robot A (patea a B):** `KICKOFF_SEEK` de centralmix = patada recta con heading-hold OTOS
  (`mix_fsm.cpp:334-360`). Falta portar el botón físico (`CENTRAL_ENABLE_PHYSICAL_BUTTON`,
  `main_central.cpp:342-351`) a centralmix — con **botón externo** (el onboard pin 9 dio GO espurio).
- **Robot B (recibe en 35 cm y patea al arco):** arqueromix casi as-is (ve pelota → alinea al arco rival
  → `avanzar_patear` → frena, `amix_fsm.cpp:328-391`). Ajuste: variante "quedarse en el lugar".
- **Estimación total del desafío: 1-2 días**, la mayor parte banco/cancha.

## 4) LO QUE NO ESTÁ ESCRITO — confirmar con el hardware en mano

1. Continuidad y **sentido real de RX_OUT/TX_OUT** hasta 7/8 (netlist lo sugiere; jamás se midió).
2. ¿**Cable o pista**? `top-board-pack/01-pinout-y-hardware.md:135` dice "(cable a pines 7/8)"; el
   netlist muestra pista nativa. Mirar la placa.
3. Si alguien cortó/anuló cables, **no quedó registrado** — si al abrir el robot aparece uno, journal ese día.
4. **Cómo se conecta el C5 prestado**: ¿reemplaza a la COMM en U1/U7 (se pierde el árbitro GPIO durante
   el desafío — probablemente OK, arranca por botón) o va en paralelo? No hay spec en el repo.
5. **Formato de datos con el equipo partner** (si no es IITA, `proto.h` no aplica) — definir ANTES de viajar.

**Bottom line: no hay nada roto que arreglar — hay algo nunca-usado que estrenar.** El camino físico
casi seguro existe, el firmware está a ~3 fixes chicos, y los bloques de juego ya están escritos.
Primero el multímetro, después el código.
