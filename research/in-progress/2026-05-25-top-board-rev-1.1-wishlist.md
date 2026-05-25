---
title: "TOP board rev 1.1 — Wishlist de mejoras para respin post-Incheon"
date-start: 2026-05-25
date-target-close: 2026-08-15  # 6 semanas post-Incheon (margen para workshop + diseño)
status: in-progress
owner: "Equipo IITA + Enzo (PCB) + Gustavo (decision)"
last-updated: 2026-05-25
priority: P1  # importante pero no bloquea Incheon
related:
  - journal/2026-05-25-top-xshut-no-routed-hallazgo-forense.md
  - hardware/electronics/top-board-pack/
  - team-tasks/2026-05-25-task-033-decidir-cuantos-tofs-incheon.md
---

# TOP board rev 1.1 — Wishlist de mejoras para respin post-Incheon

## Pregunta de investigación

¿Qué cambios concretos incorporar al diseño del PCB TOP rev 1.1 para
2027, partiendo de la experiencia operando con rev 1.0 + lo que aprendamos
en Incheon?

## Por qué importa

TOP rev 1.0 tiene limitaciones que dificultaron la integración para
Incheon:

- 4 ToFs no aprovechables sin rework (XSHUT no ruteados — ver journal
  forense 2026-05-25).
- Cámaras montadas "a mano" sin mounting hardware definido — frágil
  para transporte aéreo + cancha.
- Cero diagnóstico visual a bordo: para saber qué subsistema está vivo
  hay que abrir el monitor serial o medir con multímetro.
- Reguladores MP1584 pegados al borde del PCB — chocan con el chasis al
  intentar atornillar la placa.
- COMM ESP32-C6 vive en placa hija aparte → más cables, más conectores
  que se pueden zafar, más espacio.

Capturar el wishlist completo **ahora, mientras está fresco** evita que
estas observaciones se diluyan en el ruido post-Incheon. El respin para
2027 arranca con un brief sólido en lugar de "qué nos había molestado de
la TOP, alguien se acuerda?".

## Restricciones del proceso

- **Este doc NO es decisión todavía.** Es captura de input + propuesta
  de prioridades. La decisión definitiva (qué entra a rev 1.1, qué
  queda fuera) se cocina post-Incheon con el equipo completo,
  incorporando aprendizajes de la cancha.
- **Las prioridades P0/P1/P2 de abajo son sugeridas por el coach** —
  confirmar en workshop post-Incheon.
- **No empezar diseño de rev 1.1 hasta que esté cerrado este doc.**
  Mover a `research/completed/` cuando arranque el respin.

## Items del wishlist

Los 7 items vienen de input directo de Gustavo (2026-05-25). Orden = el
en el que los mencionó, no el de prioridad.

---

### W1 — Rutear XSHUT/LPn de los 4 slots ToF (P0 sugerida)

**Qué cambiar.** Agregar 4 nets `XSHUT_U2`, `XSHUT_U3`, `XSHUT_U5`,
`XSHUT_U17` que vayan desde los pads Xshut de cada slot hasta 4 GPIOs
libres del Teensy 4.0. Quitar los flags "No Connect" actuales del SCH.

**Por qué (problema observado en rev 1.0).** Los 4 XSHUT están
intencionalmente sin conectar (verificado forensicamente 2026-05-25;
NC flags explícitos en SCH, sin tracks en PCB, sin nets en netlist).
Sin XSHUT individual no se pueden enumerar 2 ToFs en el mismo bus I²C
(ambos arrancan en 0x29). Máximo soportado en rev 1.0: 2 ToFs (1 por
bus). Para Incheon esto nos limita el sensing perimetral (era una
ventaja prometida por el diseño de 4 ToFs).

**Recursos / referencias.**
- Journal forense: `journal/2026-05-25-top-xshut-no-routed-hallazgo-forense.md`.
- Datasheet VL53L7CX: el pin LPn (= XSHUT) acepta GPIO 3.3 V directo,
  no requiere driver intermedio. Pull-up interno 10 kΩ.
- En firmware ya hay implementación de enumeración estándar (commented
  out en `sensors_tof.cpp`), reusar.

**Estimación de esfuerzo.** Chico (≤2 horas de diseño + verificar que
los 4 GPIOs elegidos no chocan con otros usos del Teensy).

**Prioridad sugerida.** **P0** — el item ancla del respin. Sin esto la
ventaja arquitectural de 4 ToFs sigue muerta.

---

### W2 — Agujeros mecánicos para montar las cámaras (P0 sugerida)

**Qué cambiar.** Agregar 2 sets de mounting holes (3 mm M3 estándar) en
posiciones que coincidan con la base del módulo OpenMV H7 / H7 Plus. Un
set por cámara (frontal + trasera).

**Por qué (problema observado en rev 1.0).** Actualmente las cámaras se
montan "a mano" con soportes ad-hoc (cinta, cable, prints sueltos). Esto
es frágil para:
- Transporte aéreo a Incheon (impactos en caja).
- Vibración durante partido (el robot recibe choques de la pelota y de
  otros robots).
- Recalibración de FOV — cada vez que se desmonta una cámara hay que
  recalibrar el LAB y la homografía (`skill openmv-vision-tuning`).

Un montaje robusto evita el ciclo "se zafó la cámara → recalibrar → se
zafó otra vez".

**Recursos / referencias.**
- Datasheet mecánica OpenMV H7: holes a 3.0 mm de diámetro, separación
  X mm × Y mm (medir con el módulo en mano antes de diseñar).
- Verificar si el chasis actual ya tiene anclajes M3 para cámaras o si
  hay que coordinar con la skill `vibe-mechanical-design` para spec
  mecánica consistente entre chasis y PCB.

**Estimación de esfuerzo.** Chico (4 agujeros M3 con anillo de soldadura
metalizada para GND opcional).

**Prioridad sugerida.** **P0** — sin esto el resto del wishlist queda
en una placa que se sigue cayendo a pedazos en torneo.

---

### W3 — Conectores keyed/locked para cámaras (P1 sugerida)

**Qué cambiar.** Reemplazar los headers tipo Dupont 2.54 mm actuales por
conectores con retención mecánica:
- Opción A: Molex PicoBlade (1.25 mm) — chico, retención por clip.
- Opción B: JST PH (2.0 mm) — más robusto, retención por clip + key.
- Opción C: Molex KK (2.54 mm, mismo pitch actual) — drop-in fácil
  desde el footprint Dupont.

**Por qué (problema observado en rev 1.0).** Los headers Dupont se zafan
con vibración. Durante un partido el robot recibe golpes; si un cable de
cámara se desconecta a mitad de partido, perdemos vision hasta el próximo
power cycle (que no podemos hacer en cancha — solo el referee puede). La
keying además evita conexión invertida (mal cableado durante setup
nocturno antes de partido = riesgo real).

**Recursos / referencias.**
- Coordinar pinout con los conectores que use el módulo OpenMV (puede
  que requiera adaptador cable hembra a hembra si OpenMV viene con
  header sin keying).
- Validar con Enzo si tiene preferencia / stock de un connector family.

**Estimación de esfuerzo.** Medio (cambio de footprint en SCH + PCB
+ compra de stock de cables prearmados).

**Prioridad sugerida.** **P1** — alto impacto en robustez, no bloquea
funcionalidad.

---

### W4 — Voltímetro on-board con indicador visual (P2 sugerida)

**Qué cambiar.** Agregar circuitería para monitorear los rails 3.3 V y
5 V on-board, con visualización inmediata sin necesidad de multímetro:
- Opción A (mínima): 2 LEDs verde "OK" con divisor + comparador (LM393)
  que se apagan si el rail cae fuera de tolerancia.
- Opción B (rica): bar graph LED LM3914 (10 LEDs) por rail, mostrando
  voltaje en escala visual.
- Opción C (gourmet): display OLED 0.96" I²C con voltaje numérico de
  ambos rails + status de subsistemas.

**Por qué (problema observado en rev 1.0).** Hoy para verificar que los
rails están en spec hay que: (a) abrir el robot, (b) buscar el
multímetro, (c) puntear sobre TPs (que no están serigrafiados). En
Incheon eso pasa entre partidos con cronómetro corriendo → no se hace,
se asume "andaba antes".

**Recursos / referencias.**
- Si se elige Opción A: LM393 + 2 LEDs + resistores = footprint
  pequeño, ~$0.50.
- Si se elige Opción C: INA219 + OLED I²C = más complejo pero
  capitalizable para todo el wishlist de diagnóstico (W5).

**Estimación de esfuerzo.** Chico (A) / Medio (B) / Grande (C, requiere
firmware extra para manejar el OLED).

**Prioridad sugerida.** **P2** — calidad de vida importante para el
equipo en torneo, pero el robot funciona sin esto.

---

### W5 — LEDs OK multi-punto por subsistema (P1 sugerida)

**Qué cambiar.** 1 LED de status por subsistema. Cada LED se enciende
cuando el subsistema responde correctamente al boot del Teensy.
Layout serigrafiado claro: `CAM_F`, `CAM_T`, `IMU_L`, `IMU_R`, `TOF_1`,
`TOF_2`, `TOF_3`, `TOF_4`, `DOWN`, `OTOS`, `COMM`. Total ~11 LEDs.

Implementación recomendada: shift register 74HC595 + 16 LEDs → el firmware
solo necesita 3 GPIOs (DATA/CLK/LATCH). Alternativa: TLC59283 (LED
driver con corriente constante, mejor para uniformidad visual).

**Por qué (problema observado en rev 1.0).** Hoy el debug de "qué
subsistema está vivo en el boot" requiere abrir el Serial Monitor +
parsear texto. Eso:
- No funciona si el robot está alimentado por batería y no hay laptop
  conectada.
- Lleva 30-60 s por intento (boot + abrir monitor + leer).
- En la previa de un partido (5 minutos entre llamado y referee start)
  no hay tiempo para esto.

Con LEDs visuales el equipo verifica "todo verde en el boot" de un
vistazo. Si falta el LED de `TOF_3` → sabemos exactamente qué arreglar.

**Recursos / referencias.**
- Patrones similares: PCB del robot Italo Roboteam (Open 2024, equipo
  campeón) usa 8 LEDs de status. Confirmar referencias.
- Documentar en el firmware qué condición específica enciende cada LED
  (no es "el módulo respondió alguna vez" sino "respondió en la última
  verificación periódica < N segundos").

**Estimación de esfuerzo.** Medio (footprint de shift register + 11
LEDs + serigrafía + firmware para manejarlos).

**Prioridad sugerida.** **P1** — junto con W2 son los items que cambian
la operación en torneo. P1 (no P0) porque requiere firmware nuevo y eso
agrega scope al respin.

---

### W6 — Integrar STM32 para comms en la misma placa (P2 sugerida, posiblemente diferir)

**Qué cambiar.** Reemplazar la placa hija COMM ESP32-C6 (BLE para
arbitraje RoboCup) por un MCU integrado en TOP rev 1.1. Candidatos:
- STM32WB55 (Cortex-M4 + radio BLE 5.0 integrada) — sí cumple el
  protocolo RCJ ref hub.
- ESP32-C6 SoC directo en la placa (sin la placa hija) — más simple,
  pero ya tenemos firmware probado para C6.

**Por qué (problema observado en rev 1.0).** La placa hija COMM
ESP32-C6:
- Requiere conector + cables UART entre TOP y COMM (4 cables: VCC, GND,
  RX, TX) — cosa más que se puede zafar.
- Ocupa espacio físico en el chasis.
- Tiene su propio procedimiento de flasheo (USB-C separado, modo boot,
  ver `hardware/electronics/comm-board/2026-05-17-procedimiento-flash-firmware-c6.md`)
  → más superficie de error.
- Setup time mayor (2 placas separadas para configurar).

**Recursos / referencias.**
- Firmware RCJ COMM actual: branch `esp32-c6` ya validado. Si
  migramos a STM32WB55 hay que portear → trabajo no trivial.
- Si nos quedamos con ESP32-C6 SoC directo en TOP, el firmware actual
  se reutiliza con cambio mínimo de pinout.

**Estimación de esfuerzo.** Grande (cambio arquitectural que afecta
firmware + diseño RF + certificación del módulo BLE si es chip
discreto). **Este item duplica la complejidad del diseño de rev 1.1.**

**Prioridad sugerida.** **P2** — alto impacto si se hace, pero el costo
es alto y la placa hija COMM actual funciona. **Posible decisión post-
Incheon: diferir a rev 1.2 / 2028**, dejando rev 1.1 más conservadora
(solo items P0/P1).

---

### W7 — Separar reguladores de tensión del borde del PCB (P0 sugerida)

**Qué cambiar.** Mover los 2 (o 3) reguladores MP1584 que actualmente
están pegados al borde del PCB hacia adentro, dejando ~5-10 mm de
separación con el borde. Reorganizar el ruteo del area de power
accordingly.

**Por qué (problema observado en rev 1.0).** Los MP1584 están justo en
el borde → al atornillar el PCB al chasis chocan con:
- Los tornillos M3 de anclaje al chasis (cabezas + arandelas).
- Los cables de power 7.4 V que entran desde la batería (que tienden a
  rutearse por el perímetro del chasis).
- El chasis mismo (si se monta volado sobre standoff, el regulador queda
  expuesto al exterior — vulnerable a impactos).

El espacio "ganado" al ponerlos en el borde es ilusorio: cualquier
montaje práctico requiere recortarlos o tortuosamente rerutear los
cables. Mejor moverlos adentro y aceptar 5 mm más de PCB total.

**Recursos / referencias.**
- Footprint MP1584: ~17 mm × 11 mm cada uno. Moverlos 5 mm hacia
  adentro requiere reubicar también sus capacitors de entrada/salida
  (no se pueden separar mucho por integridad de la regulación).
- Coordinar con W2 (mounting holes cámaras) — el perímetro libre se
  puede usar para los nuevos agujeros mecánicos.

**Estimación de esfuerzo.** Chico (reorganización local del PCB, no
afecta SCH ni firmware).

**Prioridad sugerida.** **P0** — es un bug de diseño mecánico actual
que afecta TODO ensamblaje del robot.

---

## Resumen de prioridades sugeridas (a confirmar post-Incheon)

| Item | Título | Prio sugerida | Esfuerzo |
|------|--------|---------------|----------|
| W1 | Rutear XSHUT/LPn 4 ToFs | **P0** | Chico |
| W2 | Agujeros mecánicos cámaras | **P0** | Chico |
| W7 | Reguladores fuera del borde | **P0** | Chico |
| W3 | Conectores keyed cámaras | P1 | Medio |
| W5 | LEDs OK multi-punto | P1 | Medio |
| W4 | Voltímetro on-board | P2 | Chico-Grande |
| W6 | STM32 integrado para comms | P2 (diferir?) | Grande |

**Coach take.** Rev 1.1 debería entregar los 3 P0 + los 2 P1 (W1, W2,
W7, W3, W5). Los P2 (W4, W6) se evalúan en el workshop con presupuesto
de tiempo real. **W6 es la decisión más jugada** — alta complejidad,
podría justificar diferir a rev 1.2.

## Próximos pasos

### Para cerrar este research (mover a `completed/`)

1. **Post-Incheon: workshop con el equipo** (Virginia + Elías + Enzo +
   Gustavo) para:
   - Confirmar las prioridades sugeridas o reordenarlas según lo
     aprendido en torneo.
   - Sumar items nuevos detectados en cancha (probable: 1-3 items más).
   - Decidir scope final de rev 1.1 (¿qué entra, qué difiere?).
2. **Buscar mentor de PCB** (sponsor / contacto industria) para revisar
   el diseño antes de fabricar. Aprendizaje del bug DOWN 04-12 (10 nets
   sin rutear): conviene tener segundo par de ojos antes de mandar a
   JLCPCB.
3. **Diseño en KiCad o EasyEDA** — Enzo, con apoyo del mentor si
   conseguimos. Plazo objetivo: agosto 2026 (6 semanas post-Incheon).
4. **Fabricar 2 unidades** en JLCPCB para tener spare. Costo
   aprox: USD 30-50 para 5 unidades + envío.
5. **DRC + ERC obligatorios antes de mandar a fabricar** (regla 7 de
   CLAUDE.md, no negociable).

### Decisiones que NO se toman hoy

- Qué subset exacto de los 7 items entra a rev 1.1 → workshop
  post-Incheon.
- Si W6 (STM32 integrado) se difiere a rev 1.2 o se hace ahora → workshop.
- Connector family para W3 → decisión técnica de Enzo cuando arranque
  el diseño.

## Referencias

- Journal del hallazgo XSHUT que originó este wishlist:
  `journal/2026-05-25-top-xshut-no-routed-hallazgo-forense.md`
- Pack TOP rev 1.0: `hardware/electronics/top-board-pack/`
- Skills relacionadas: `.claude/skills/vibe-pcb-design/SKILL.md` (será la
  guía del proceso de diseño rev 1.1).
- CLAUDE.md regla 7: DRC + ERC obligatorios antes de fabricar.

## Atribución

- **Wishlist completo (los 7 items)** — Gustavo Viollaz (@gviollaz),
  input verbal del 2026-05-25.
- **Estructura research note + estimaciones + prioridades sugeridas
  + redacción** — Claude Opus 4.7 (Anthropic), session 2026-05-25.
- **Validación / decisión final post-Incheon** — Equipo completo (a
  ejecutar en workshop).
